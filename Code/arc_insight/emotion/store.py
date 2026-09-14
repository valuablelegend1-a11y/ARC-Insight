import json
import time
from pathlib import Path


class EmotionStore:
    def __init__(self, history_path, baseline_path):
        self.history_path = Path(history_path)
        self.baseline_path = Path(baseline_path)
        self._baseline = self._load_baseline()

    def _load_baseline(self):
        if Path(self.baseline_path).exists():
            try:
                return json.loads(Path(self.baseline_path).read_text(encoding="utf-8"))
            except (json.JSONDecodeError, OSError):
                pass
        return {}

    def baseline(self):
        return self._baseline

    def update_baseline(self, hr_value):
        if not hr_value:
            return
        self._baseline["hr"] = hr_value
        self._baseline["updated"] = time.time()
        try:
            Path(self.baseline_path).parent.mkdir(parents=True, exist_ok=True)
            Path(self.baseline_path).write_text(
                json.dumps(self._baseline), encoding="utf-8"
            )
        except OSError:
            pass

    def append_event(self, event):
        event = dict(event)
        event.setdefault("ts", time.time())
        try:
            Path(self.history_path).parent.mkdir(parents=True, exist_ok=True)
            with Path(self.history_path).open("a", encoding="utf-8") as file:
                file.write(json.dumps(event) + "\n")
        except OSError:
            pass

    def trend(self, days=1.0):
        cutoff = time.time() - days * 86400.0
        counts = {}
        samples = 0
        if not Path(self.history_path).exists():
            return counts
        for line in Path(self.history_path).read_text(encoding="utf-8", errors="ignore").splitlines():
            if not line.strip():
                continue
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            if event.get("ts", 0) < cutoff:
                continue
            state = event.get("state")
            if not state:
                continue
            counts[state] = counts.get(state, 0) + 1
            samples += 1
        counts["samples"] = samples
        return counts

    def prune(self, max_events=8000):
        path = Path(self.history_path)
        if not path.exists():
            return
        lines = [line for line in path.read_text(encoding="utf-8", errors="ignore").splitlines() if line.strip()]
        if len(lines) <= max_events:
            return
        lines = lines[-max_events:]
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        except OSError:
            pass