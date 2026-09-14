import argparse
import asyncio
import json
import sys

import websockets
from websockets.exceptions import ConnectionClosed

from arc_insight import protocol

# OTA app0/app1 partition size in the arc-insight layout (partitions.csv).
OTA_PARTITION_SIZE = 0x3F0000
ESP_IMAGE_MAGIC = b"\xe9"  # first byte of a valid ESP32 app image header


class OtaError(Exception):
    pass


class _Session:
    def __init__(self, ws, timeout):
        self.ws = ws
        self.timeout = timeout
        self.received = 0
        self.restarted = False

    async def _recv(self):
        while True:
            raw = await asyncio.wait_for(self.ws.recv(), timeout=self.timeout)
            if not isinstance(raw, str):
                continue
            try:
                message = json.loads(raw)
            except json.JSONDecodeError:
                continue
            mtype = message.get("type")
            if mtype == "ota_ack":
                self.received = message.get("received", self.received)
                return message
            if mtype == "ota_event" and message.get("event") == "device_restarted":
                self.restarted = True
                return None
            if mtype == "ota_event" and message.get("event") == "device_error":
                raise OtaError(f"device reported error: {message.get('detail', '')}")

    async def send(self, data):
        await self.ws.send(data)

    async def expect(self, description):
        message = await self._recv()
        if message is None:
            return None
        return message


async def run_upload(
    uri,
    firmware,
    chunk_bytes=4096,
    timeout=30.0,
    ack_timeout=10.0,
    wait_restart=True,
    connect_timeout=15.0,
):
    if not firmware:
        raise OtaError("firmware is empty")
    if len(firmware) > OTA_PARTITION_SIZE:
        raise OtaError(
            f"firmware is {len(firmware)} bytes but the OTA partition holds "
            f"{OTA_PARTITION_SIZE} bytes")

    if firmware[:1] != ESP_IMAGE_MAGIC:
        print("warning: image does not start with the ESP32 app magic byte; "
              "the device will reject it during apply", file=sys.stderr)

    async with websockets.connect(uri, max_size=None) as ws:
        await asyncio.wait_for(
            ws.send(json.dumps({"type": "hello", "name": "ota"})),
            timeout=connect_timeout)

        session = _Session(ws, ack_timeout)
        size = len(firmware)

        await session.send(json.dumps({"type": "ota_begin", "size": size}))
        ack = await session.expect("ota_begin acknowledgement")
        if ack is None:
            raise OtaError("the glasses restarted before the transfer began")
        if ack.get("stage") != "begin" or ack.get("size") != size:
            raise OtaError(
                f"server acknowledged begin size {ack.get('size')}, "
                f"expected {size}")

        sent = 0
        while sent < size:
            chunk = firmware[sent:sent + chunk_bytes]
            await session.send(protocol.frame(protocol.BIN_FW_CHUNK, chunk))
            ack = await session.expect("fw chunk acknowledgement")
            if ack is None:
                break  # device restarted mid-transfer
            if session.received < sent + len(chunk):
                raise OtaError("server reported fewer bytes than sent")
            sent += len(chunk)
            print(f"\r  ota {(session.received * 100) // size:3d}%  "
                  f"{session.received}/{size} bytes", end="", flush=True)

        restarted = session.restarted
        if not restarted:
            await session.send(json.dumps({"type": "ota_end", "written": sent}))
            await session.expect("ota_end acknowledgement")
            restarted = session.restarted

        if not wait_restart:
            return {"ok": True, "restarted": False, "bytes": sent,
                    "received": session.received, "aborted": False}

        if not restarted:
            session.timeout = timeout
            try:
                message = await session.expect("device restart")
                restarted = message is None
            except asyncio.TimeoutError:
                restarted = False

    return {"ok": True, "restarted": restarted, "bytes": sent,
            "received": session.received, "aborted": False}


def _main():
    parser = argparse.ArgumentParser(
        description="OTA-upload a firmware image to arc-insight glasses "
                    "through the arc_insight websocket server.")
    parser.add_argument("--bin", required=True, help="path to firmware.bin")
    parser.add_argument("--uri", default="ws://localhost:8765",
                        help="arc_insight server URI (default: %(default)s)")
    parser.add_argument("--chunk", type=int, default=4096,
                        help="payload bytes per websocket frame (default: %(default)s)")
    parser.add_argument("--timeout", type=float, default=90.0,
                        help="seconds to wait for the device restart (default: %(default)s)")
    parser.add_argument("--no-wait-restart", action="store_true",
                        help="finish after the transfer; do not wait for reboot")
    args = parser.parse_args()

    try:
        with open(args.bin, "rb") as fh:
            firmware = fh.read()
    except OSError as exc:
        print(f"error: cannot read {args.bin}: {exc}", file=sys.stderr)
        return 1

    try:
        report = asyncio.run(
            run_upload(args.uri, firmware, chunk_bytes=args.chunk,
                       wait_restart=not args.no_wait_restart,
                       timeout=args.timeout))
    except OtaError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except (ConnectionClosed, OSError) as exc:
        print(f"error: connection failed: {exc}", file=sys.stderr)
        return 1
    except asyncio.TimeoutError:
        print("error: timed out waiting for the glasses to restart", file=sys.stderr)
        return 2

    print()
    print(f"transferred {report['bytes']} bytes")
    if args.no_wait_restart:
        print("transfer complete; not waiting for reboot")
        return 0
    if report["restarted"]:
        print("glasses accepted the image and rebooted")
        return 0
    print("transfer completed but no restart was observed — the image may "
          "have failed verification; inspect the glasses log", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(_main())