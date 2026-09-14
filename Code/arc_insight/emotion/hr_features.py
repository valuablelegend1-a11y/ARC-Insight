import time
from collections import deque


class HRFeatureExtractor:
    def __init__(self, max_samples=300):
        self.samples = deque(maxlen=max_samples)

    def clear(self):
        self.samples.clear()

    def add(self, sample, now=None):
        bpm = sample.get("bpm")
        confidence = sample.get("confidence", 1.0)
        if bpm is None or not confidence or confidence < 0.4:
            return
        bpm = float(bpm)
        if bpm < 30 or bpm > 220:
            return
        self.samples.append({
            "ts": float(sample.get("timestamp") or (now or time.time())),
            "bpm": bpm,
            "rr": sample.get("rr"),
        })

    def features(self, baseline=None, now=None, window=60.0):
        now = now or time.time()
        if not self.samples:
            return None
        if now - self.samples[-1]["ts"] > 15.0:
            return None

        cutoff = now - window
        windowed = [s for s in self.samples if s["ts"] >= cutoff]
        if len(windowed) < 10:
            windowed = [s for s in self.samples if s["ts"] >= now - 300.0]
        if len(windowed) < 5:
            return None

        bpms = [s["bpm"] for s in windowed]
        current = windowed[-1]["bpm"]
        mean = sum(bpms) / len(bpms)
        variance = sum((b - mean) ** 2 for b in bpms) / max(len(bpms) - 1, 1)
        std = variance ** 0.5
        low, high = min(bpms), max(bpms)

        recent = [s for s in windowed if s["ts"] >= now - 15.0]
        if len(recent) >= 2:
            recent_mean = sum(s["bpm"] for s in recent) / len(recent)
        else:
            recent_mean = current

        slope = 0.0
        if len(windowed) >= 6:
            recent_window = [s for s in windowed if s["ts"] >= now - 30.0]
            if len(recent_window) >= 3:
                slope = _linear_slope(recent_window)

        dev = (recent_mean - baseline) if baseline else 0.0

        rri = [s["rr"] for s in windowed if s.get("rr")]
        hrv_proxy = None
        if len(rri) >= 8:
            diffs = [abs(rri[i] - rri[i - 1]) for i in range(1, len(rri))]
            hrv_proxy = (sum(d ** 2 for d in diffs) / len(diffs)) ** 0.5
            hrv_proxy = max(hrv_proxy, 1e-6)

        return {
            "current": current,
            "mean": mean,
            "std": std,
            "low": low,
            "high": high,
            "recent_mean": recent_mean,
            "slope": slope,
            "deviation": dev,
            "hrv_proxy": hrv_proxy,
            "count": len(windowed),
        }

    def resting_estimate(self, now=None):
        now = now or time.time()
        if not self.samples:
            return None
        cutoff = now - 300.0
        bpms = [s["bpm"] for s in self.samples if s["ts"] >= cutoff]
        if len(bpms) < 20:
            return None
        ordered = sorted(bpms)
        trimmed = ordered[len(ordered) // 8: -len(ordered) // 8]
        if not trimmed:
            return None
        median = trimmed[len(trimmed) // 2]
        return round(median, 1)


def _linear_slope(windowed):
    n = len(windowed)
    sum_x = sum(s["ts"] for s in windowed)
    sum_y = sum(s["bpm"] for s in windowed)
    sum_xy = sum(s["ts"] * s["bpm"] for s in windowed)
    sum_xx = sum(s["ts"] * s["ts"] for s in windowed)
    denom = n * sum_xx - sum_x * sum_x
    if abs(denom) < 1e-9:
        return 0.0
    return (n * sum_xy - sum_x * sum_y) / denom * 60.0