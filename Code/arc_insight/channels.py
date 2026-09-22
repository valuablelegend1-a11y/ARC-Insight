import asyncio
import re
import time
from collections import deque


class IOManager:
    def __init__(self, cfg, emotion=None):
        self.cfg = cfg
        self.emotion = emotion
        self.channel = "laptop"
        self.loop = None
        self.server = None
        self.utterances = asyncio.Queue(maxsize=32)
        self.transcripts = deque(maxlen=40)
        self.latest_frame = None
        self.latest_frame_at = 0.0
        self.latest_hr = None
        self.latest_hr_at = 0.0
        self.hr_history = deque(maxlen=400)
        self.notifications = deque(maxlen=40)
        self.notification_handler = None
        self._seen_uids = deque(maxlen=256)
        self._client = False
        self._client_since = None

    @property
    def connected(self):
        return self._client

    def on_client_connected(self):
        if not self._client:
            self._client = True
            self._client_since = time.time()
            print("ARCINSIGHT: glasses connected", flush=True)

    def on_client_disconnected(self):
        if self._client:
            self._client = False
            print("ARCINSIGHT: glasses disconnected", flush=True)

    def switch(self, target):
        if target not in {"laptop", "glasses"}:
            return False
        if target == "glasses" and not self._client:
            return False
        self.channel = target
        print(f"ARCINSIGHT: channel switched to {target}", flush=True)
        return True

    def store_frame(self, jpeg):
        self.latest_frame = jpeg
        self.latest_frame_at = time.time()

    def fresh_frame(self, max_age=3.0):
        if self.latest_frame is None:
            return None
        if time.time() - self.latest_frame_at > max_age:
            return None
        return self.latest_frame

    def store_hr(self, sample):
        self.latest_hr = sample
        self.latest_hr_at = time.time()
        self.hr_history.append((sample.get("timestamp", time.time()), sample))
        if self.emotion:
            try:
                self.emotion.ingest_hr(sample)
            except Exception:
                pass

    def on_notification(self, sample):
        if not isinstance(sample, dict):
            return
        uid = sample.get("uid")
        if uid is not None:
            key = int(uid) & 0xFFFFFFFF
            if key in self._seen_uids:
                return
            self._seen_uids.append(key)
        entry = dict(sample)
        entry.setdefault("timestamp", time.time())
        self.notifications.append((entry["timestamp"], entry))
        handler = self.notification_handler
        if handler is None:
            return
        try:
            if self.loop is not None and self.loop.is_running():
                self.loop.create_task(self._run_handler(handler, entry))
            else:
                handler(entry)
        except Exception:
            pass

    async def _run_handler(self, handler, sample):
        try:
            await handler(sample)
        except Exception:
            pass

    async def on_transcript(self, text, ts=None):
        ts = ts or time.time()
        text = (text or "").strip()
        if not text:
            return
        self.transcripts.append((ts, text))
        if self.emotion:
            try:
                self.emotion.ingest_words(text, ts)
            except Exception:
                pass
        command = self._extract_command(text)
        if not command:
            return
        try:
            self.utterances.put_nowait(command.lower())
        except asyncio.QueueFull:
            pass

    def _extract_command(self, text):
        if not self.cfg["glasses"]["require_wake_word"]:
            return text.strip(" ,.!?") or None
        wake = self.cfg["glasses"]["wake_word"]
        match = re.match(rf"^\s*{re.escape(wake)}\b", text, flags=re.IGNORECASE)
        if not match:
            return None
        rest = text[match.end():].strip(" ,.!?")
        return rest or None