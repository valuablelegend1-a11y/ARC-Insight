import time

from arc_insight.config import data_path
from arc_insight.emotion.fusion import FusionEngine
from arc_insight.emotion.hr_features import HRFeatureExtractor
from arc_insight.emotion.sentiment import SentimentAnalyzer
from arc_insight.emotion.store import EmotionStore

_BASELINE_REFRESH_SECONDS = 300.0
_STORE_EVERY_SECONDS = 10.0


class EmotionEngine:
    def __init__(self, cfg):
        self.cfg = cfg["emotion"]
        self.store = EmotionStore(
            data_path(cfg, "history_path"),
            data_path(cfg, "baseline_path"),
        )
        self.sentiment = SentimentAnalyzer()
        self.hrv = HRFeatureExtractor()
        self.fusion = FusionEngine()
        self.hr_baseline = self.store.baseline().get("hr")
        self._last_baseline_check = time.time()
        self._last_store = 0.0
        self._ingests = 0

    def ingest_words(self, text, ts=None):
        ts = ts or time.time()
        scored = self.sentiment.score(text)
        if scored["count"] == 0:
            return
        self.fusion.add_text(scored, ts)
        self._ingests += 1
        self._maybe_store({**scored, "type": "text", "text": text[:120]})

    def ingest_hr(self, sample, now=None):
        sample = dict(sample)
        now = now or sample.get("timestamp") or time.time()
        sample.setdefault("timestamp", now)
        self.hrv.add(sample)
        if self.hr_baseline is None:
            estimate = self.hrv.resting_estimate(now)
            if estimate:
                self.hr_baseline = estimate
                self.store.update_baseline(estimate)
        features = self.hrv.features(baseline=self.hr_baseline, now=now)
        if features:
            self.fusion.add_hr(features, now)
        self._ingests += 1
        self._maybe_store({"type": "hr", "bpm": features["current"] if features else None,
                           "deviation": features["deviation"] if features else None})
        self._maybe_refresh_baseline(now)

    def on_channel_switch(self, channel):
        self.fusion.on_break()

    def state(self):
        return self.fusion.current()

    def describe(self):
        return self.fusion.describe()

    def trend(self, days=1.0):
        return self.store.trend(days)

    def _maybe_store(self, event):
        now = time.time()
        if now - self._last_store >= _STORE_EVERY_SECONDS:
            current = self.fusion.current()
            event["state"] = current["state"]
            event["confidence"] = current["confidence"]
            event["valence"] = current["valence"]
            event["arousal"] = current["arousal"]
            event["drivers"] = current["drivers"]
            self.store.append_event(event)
            self._last_store = now

    def _maybe_refresh_baseline(self, now):
        if now - self._last_baseline_check < _BASELINE_REFRESH_SECONDS:
            return
        self._last_baseline_check = now
        if self.fusion.state not in {"stressed", "excited"}:
            estimate = self.hrv.resting_estimate(now)
            if estimate:
                self.hr_baseline = estimate
                self.store.update_baseline(estimate)