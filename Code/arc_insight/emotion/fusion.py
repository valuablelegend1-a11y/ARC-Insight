import time

STATES = ("calm", "neutral", "stressed", "excited", "sad")

TEXT_HALF_LIFE = 240.0
HR_HALF_LIFE = 120.0
DWELL_SECONDS = 20.0


class FusionEngine:
    def __init__(self):
        self.text_scores = []
        self.hr_features = None
        self.hr_at = None
        self.state = "neutral"
        self.confidence = 0.0
        self.valence = 0.0
        self.arousal = 0.0
        self.drivers = []
        self._candidate = "neutral"
        self._candidate_since = time.time()
        self._started = time.time()
        self._break_since = None

    def on_break(self, now=None):
        self._break_since = now or time.time()
        self.text_scores.clear()
        self.hr_features = None
        self.hr_at = None
        self.drivers = []
        self._refresh(now or time.time())

    def add_text(self, scored, ts=None):
        ts = ts or time.time()
        if self._break_since and ts < self._break_since:
            ts = self._break_since
        self.text_scores.append((ts, scored["compound"]))
        if len(self.text_scores) > 120:
            self.text_scores = self.text_scores[-120:]
        self._refresh(ts)

    def add_hr(self, features, ts=None):
        ts = ts or time.time()
        self.hr_features = features
        self.hr_at = ts
        self._refresh(ts)

    def _refresh(self, now):
        valence, text_weight = self._valence_signal(now)
        arousal, hr_weight = self._arousal_signal(now)

        state_probs = {s: 0.0 for s in STATES}
        drivers = []
        if valence is not None and text_weight > 0.0:
            normalized = abs(valence)
            if valence > 0.25:
                state_probs["excited"] += text_weight * normalized
                state_probs["calm"] += text_weight * 0.2
                drivers.append("positive wording")
            elif valence < -0.25:
                state_probs["stressed"] += text_weight * normalized * 0.8
                state_probs["sad"] += text_weight * normalized * 0.5
                drivers.append("negative wording")
            else:
                state_probs["neutral"] += text_weight * 0.5
                state_probs["calm"] += text_weight * 0.2

        if arousal is not None and hr_weight > 0.0:
            if arousal > 0.6:
                if valence is None or valence >= 0.0:
                    state_probs["excited"] += hr_weight
                    drivers.append("raised heart rate")
                else:
                    state_probs["stressed"] += hr_weight
                    drivers.append("raised heart rate")
            elif arousal < -0.4:
                state_probs["calm"] += hr_weight * 0.8
                state_probs["neutral"] += hr_weight * 0.2
                if valence is not None and valence < -0.25:
                    state_probs["sad"] += hr_weight * 0.5
            else:
                state_probs["neutral"] += hr_weight * 0.6
                state_probs["calm"] += hr_weight * 0.3

        voices = sum(state_probs.values())
        if voices <= 0.0:
            return

        strongest, strongest_value = max(state_probs.items(), key=lambda item: item[1])
        confidence = min(1.0, strongest_value + 0.15 * min(text_weight + hr_weight, 1.0))
        confidence = max(0.0, min(1.0, confidence))

        if strongest != self._candidate:
            self._candidate = strongest
            self._candidate_since = now
        elif strongest == self._candidate:
            pass

        if strongest != self.state and now - self._candidate_since >= DWELL_SECONDS:
            self.state = strongest
            self.state_since = now

        self.confidence = confidence
        self.valence = valence if valence is not None else 0.0
        self.arousal = arousal if arousal is not None else 0.0
        self.drivers = drivers[:3]

    def _valence_signal(self, now):
        if not self.text_scores:
            return None, 0.0
        cutoff = now - 600.0
        recent = [(ts, s) for ts, s in self.text_scores if ts >= cutoff]
        if not recent:
            return None, 0.0
        total = 0.0
        weight = 0.0
        for ts, score in recent:
            age = max(0.0, now - ts)
            w = 0.5 ** (age / TEXT_HALF_LIFE)
            total += score * w
            weight += w
        return total / weight, min(1.0, weight / 3.0)

    def _arousal_signal(self, now):
        if self.hr_features is None or self.hr_at is None:
            return None, 0.0
        if now < self.hr_at:
            return None, 0.0
        age = now - self.hr_at
        if age > 60.0:
            return None, 0.0
        features = self.hr_features
        dev = abs(features["deviation"])
        slope = features["slope"]
        freshness = 0.5 ** (age / HR_HALF_LIFE)

        arousal = 0.0
        arousal += min(dev / 16.0, 1.2) * 0.55
        arousal += min(max(slope, -6.0), 6.0) / 6.0 * 0.25
        variation = features["std"] / max(features["mean"], 1.0)
        arousal += min(variation * 4.0, 1.0) * 0.2

        strength = min(1.0, freshness * (0.6 + min(abs(dev) / 25.0, 1.0)))
        return arousal, strength

    def current(self):
        return {
            "state": self.state,
            "confidence": round(self.confidence, 2),
            "valence": round(self.valence, 2),
            "arousal": round(self.arousal, 2),
            "drivers": list(self.drivers),
            "since": float(getattr(self, "state_since", self._started)),
        }

    def describe(self):
        state = self.state
        text = {
            "calm": "You seem calm",
            "neutral": "You seem steady",
            "stressed": "You seem stressed",
            "excited": "You seem excited",
            "sad": "You seem a little low",
        }[state]
        if self.confidence < 0.35:
            return f"{text}, though I am not fully sure yet."
        if self.drivers:
            return f"{text}. Signals: {'; '.join(self.drivers)}."
        return f"{text}, Sir."