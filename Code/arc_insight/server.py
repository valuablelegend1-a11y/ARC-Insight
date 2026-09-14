import asyncio
import json
import time

import websockets
from websockets.exceptions import ConnectionClosed

from arc_insight import protocol
from arc_insight.stt import StreamSTT


class GlassesServer:
    def __init__(self, io, cfg, recognizer=None):
        self.io = io
        self.cfg = cfg
        self.server_cfg = cfg["server"]
        self.stt = StreamSTT(cfg, recognizer=recognizer)
        self.websocket = None  # device connection (compatibility alias)
        self.device = None  # the glasses connection
        self._ota = None  # OTA uploader connection
        self._ota_bytes = 0
        self._image_waiters = []
        self._loop = None

    async def run(self):
        self._loop = asyncio.get_running_loop()
        host = self.server_cfg["host"]
        port = int(self.server_cfg["port"])
        async with websockets.serve(self._accept, host, port, max_size=None, ping_interval=20, ping_timeout=20):
            print(f"ARCINSIGHT: glasses server listening on ws://{host}:{port}", flush=True)
            await asyncio.Future()

    async def _accept(self, ws):
        self._loop = asyncio.get_running_loop()
        try:
            async for raw in ws:
                await self._dispatch(ws, raw)
        except ConnectionClosed:
            pass
        except Exception:
            pass
        finally:
            if self.device is ws:
                self.device = None
                self.websocket = None
                self.io.on_client_disconnected()
                for future in self._image_waiters:
                    if not future.done():
                        future.cancel()
                self._image_waiters.clear()
                if self._ota is not None:
                    await self._send_to(
                        self._ota,
                        json.dumps({"type": "ota_event",
                                    "event": "device_restarted"}))
            if self._ota is ws:
                self._ota = None

    async def _dispatch(self, ws, raw):
        if isinstance(raw, bytes):
            if not raw:
                return
            opcode, payload = protocol.split_frame(raw)
            if opcode == protocol.BIN_FW_CHUNK:
                if ws is self._ota and self.device is not None:
                    self._ota_bytes += len(payload)
                    ok = await self._send_to(self.device, raw)
                    if ok:
                        await self._send_to(
                            ws,
                            json.dumps(
                                {"type": "ota_ack", "bytes": len(payload),
                                 "received": self._ota_bytes}),
                        )
                return
            if ws is not self.device:
                return
            if opcode == protocol.BIN_AUDIO_PCM:
                await self.stt.push_pcm(payload, self.io)
            elif opcode == protocol.BIN_IMAGE_JPEG:
                self.io.store_frame(payload)
                self._resolve_image_waiters(payload)
            return

        try:
            message = json.loads(raw)
        except json.JSONDecodeError:
            return

        message_type = message.get("type")
        if message_type == "hello":
            name = message.get("name")
            if name == "ota":
                self._ota = ws
                self._ota_bytes = 0
                print("ARCINSIGHT: ota uploader connected", flush=True)
                return
            if self.device is None:
                self.device = ws
                self.websocket = ws
                self.stt.reset()
                self.io.on_client_connected()
            print(f"ARCINSIGHT: hello from {name or 'glasses'}", flush=True)
            return

        if ws is self._ota:
            if message_type in ("ota_begin", "ota_end", "ota_abort"):
                if self.device is not None:
                    await self._send_to(self.device, raw)
                if message_type == "ota_begin":
                    size = message.get("size") or 0
                    await self._send_to(
                        ws,
                        json.dumps(
                            {"type": "ota_ack", "stage": "begin",
                             "size": int(size)}),
                    )
                elif message_type == "ota_end":
                    await self._send_to(
                        ws,
                        json.dumps(
                            {"type": "ota_ack", "stage": "end",
                             "received": self._ota_bytes}),
                    )
            return

        if ws is not self.device:
            return

        if message_type == "event":
            if message.get("event") == "switch":
                self.io.switch("laptop" if self.io.channel == "glasses" else "glasses")
        elif message_type == "ota_event":
            if self._ota is not None:
                await self._send_to(self._ota, raw)
        elif message_type == "hr":
            sample = message.get("sample") or {}
            if isinstance(sample, dict) and "bpm" in sample:
                sample = dict(sample)
                sample.setdefault("timestamp", time.time())
                self.io.store_hr(sample)
        elif message_type == "tts_done":
            pass

    async def _send_to(self, ws, data):
        try:
            await ws.send(data)
            return True
        except ConnectionClosed:
            return False
        except Exception:
            return False

    def _resolve_image_waiters(self, payload):
        waiters = self._image_waiters
        self._image_waiters = []
        for future in waiters:
            if not future.done():
                future.set_result(payload)

    async def send_text(self, message):
        if self.websocket is None:
            return False
        return await self._send_to(self.websocket, json.dumps(message))

    async def send_tts(self, mp3_bytes):
        if self.websocket is None or not mp3_bytes:
            return False
        return await self._send_to(self.websocket, protocol.frame(protocol.BIN_AUDIO_MP3, mp3_bytes))

    async def request_image(self, timeout=5.0):
        if self.websocket is None:
            return None
        loop = asyncio.get_running_loop()
        future = loop.create_future()
        self._image_waiters.append(future)
        ok = await self.send_text({"type": "capture"})
        if not ok:
            return None
        try:
            return await asyncio.wait_for(future, timeout=timeout)
        except (asyncio.TimeoutError, asyncio.CancelledError):
            return None

    async def send_capture(self):
        await self.send_text({"type": "capture"})

    async def frame_loop(self):
        refresh = float(self.cfg["glasses"]["frame_refresh_seconds"])
        while True:
            try:
                await asyncio.sleep(refresh)
                if self.io.channel == "glasses" and self.io.connected:
                    await self.request_image()
            except asyncio.CancelledError:
                return
            except Exception:
                continue