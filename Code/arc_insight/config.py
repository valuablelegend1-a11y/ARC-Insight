import copy
import json
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = PACKAGE_ROOT.parent

DEFAULTS = {
    "server": {
        "host": "0.0.0.0",
        "port": 8765,
    },
    "glasses": {
        "name": "arc-insight",
        "require_wake_word": True,
        "wake_word": "jarvis",
        "audio_sample_rate": 16000,
        "frame_refresh_seconds": 1.0,
        "speech_gate_ratio": 3.0,
        "silence_seconds_to_finalize": 0.9,
        "max_utterance_seconds": 8.0,
        "min_utterance_seconds": 0.3,
    },
    "handoff": {
        "hotkey": "ctrl+alt+g",
        "to_laptop": ["switch to laptop", "switch to my laptop", "laptop mode", "use the laptop", "talk to me on the laptop"],
        "to_glasses": ["switch to glasses", "glasses mode", "use the glasses", "put it on the glasses", "talk to me through the glasses"],
    },
    "emotion": {
        "enabled": True,
        "history_path": "data/glasses/emotion_history.json",
        "baseline_path": "data/glasses/emotion_baseline.json",
    },
    "vision": {
        "screenshot_dir": "data/glasses",
        "face_distance_threshold": 0.5,
    },
}


def _deep_merge(base, override):
    merged = copy.deepcopy(base)
    for key, value in (override or {}).items():
        if isinstance(value, dict) and isinstance(merged.get(key), dict):
            merged[key] = _deep_merge(merged[key], value)
        else:
            merged[key] = copy.deepcopy(value)
    return merged


def load_config():
    config_path = PACKAGE_ROOT / "config.json"
    if config_path.exists():
        try:
            loaded = json.loads(config_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            loaded = {}
    else:
        loaded = {}
    return _deep_merge(DEFAULTS, loaded)


def data_path(cfg, key):
    rel = cfg["emotion"][key] if key in ("history_path", "baseline_path") else cfg["vision"].get(key)
    if key == "screenshot_dir":
        rel = cfg["vision"]["screenshot_dir"]
    path = Path(rel)
    if not path.is_absolute():
        path = PROJECT_ROOT / path
    return path