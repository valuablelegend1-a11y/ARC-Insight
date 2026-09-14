import asyncio
import time

import numpy as np
import speech_recognition as sr


class StreamSTT:
    def __init__(self, cfg, recognizer=None):
        self.cfg = cfg["glasses"]
        self.recognizer = recognizer or sr.Recognizer()
        self.rate = int(self.cfg["audio_sample_rate"])
        self.width = 2
        self.buffer = bytearray()
        self.speaking = False
        self.silence_started_at = None
        self.utterance_started_at = None
        self.noise_floor = None
        self.volume_history = []

    def reset(self):
        self.buffer.clear()
        self.speaking = False
        self.silence_started_at = None
        self.utterance_started_at = None
        self.noise_floor = None
        self.volume_history.clear()

    def _rms(self, data):
        samples = np.frombuffer(data, dtype=np.int16).astype(np.float32)
        if samples.size == 0:
            return 0.0
        return float(np.sqrt(np.mean(samples ** 2)))

    def _gate(self, rms):
        if self.noise_floor is None:
            self.noise_floor = rms
        elif rms < self.noise_floor:
            self.noise_floor = rms
        else:
            self.noise_floor += (rms - self.noise_floor) * 0.02
        return max(self.noise_floor * float(self.cfg["speech_gate_ratio"]), 80.0)

    async def push_pcm(self, data, io):
        now = time.monotonic()
        rms = self._rms(data)
        gate = self._gate(rms)
        block_seconds = len(data) / 2 / self.rate

        if not self.speaking:
            if rms <= gate:
                return
            self.speaking = True
            self.utterance_started_at = now
            self.buffer = bytearray()
        else:
            if rms <= gate:
                if self.silence_started_at is None:
                    self.silence_started_at = now
                elif now - self.silence_started_at >= float(self.cfg["silence_seconds_to_finalize"]):
                    await self.finalize(io)
                    return
            else:
                self.silence_started_at = None

        self.buffer.extend(data)

        if now - self.utterance_started_at >= float(self.cfg["max_utterance_seconds"]):
            await self.finalize(io)

    async def finalize(self, io):
        duration = len(self.buffer) / 2 / self.rate
        if self.buffer and duration >= float(self.cfg["min_utterance_seconds"]):
            text = await asyncio.to_thread(self._recognize, bytes(self.buffer))
            if text:
                await io.on_transcript(text.strip())
        self.reset()

    def _recognize(self, pcm):
        audio = sr.AudioData(pcm, self.rate, self.width)
        try:
            return self.recognizer.recognize_google(audio).lower()
        except sr.UnknownValueError:
            return ""
        except sr.RequestError:
            return ""
        except Exception:
            return ""