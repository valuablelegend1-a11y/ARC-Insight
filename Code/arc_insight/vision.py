import json
import os
import time
from pathlib import Path

import cv2
import numpy as np

from arc_insight.config import PROJECT_ROOT

FACES_DIR = PROJECT_ROOT / "faces"
FACE_CACHE = PROJECT_ROOT / "face_cache.json"
OBJECT_MODEL_PATH = PROJECT_ROOT / "yolo11x.pt"

_face_encodings = None
_face_names = None
_object_model = None
_last_frame_decodes = None


def decode_jpeg(jpeg):
    arr = np.frombuffer(jpeg, dtype=np.uint8)
    return cv2.imdecode(arr, cv2.IMREAD_COLOR)


def detect_objects(jpeg):
    global _object_model
    frame = decode_jpeg(jpeg)
    if frame is None:
        return None
    if _object_model is None:
        from ultralytics import YOLO

        _object_model = YOLO(str(OBJECT_MODEL_PATH))

    results = _object_model(frame, stream=False, verbose=False)
    seen = {}
    ignored = {"person"}
    for result in results:
        for box in result.boxes:
            confidence = float(box.conf)
            if confidence < 0.3:
                continue
            label = _object_model.names[int(box.cls)]
            if label in ignored:
                continue
            seen[label] = seen.get(label, 0) + 1
    return seen


def format_detections(seen):
    if not seen:
        return "I did not identify any clear objects through the glasses camera."
    items = []
    for label, count in sorted(seen.items()):
        items.append(f"1 {label}" if count == 1 else f"{count} {label}s")
    return "Glasses camera shows: " + ", ".join(items) + "."


def _load_encodings():
    global _face_encodings, _face_names
    if _face_encodings is not None:
        return _face_encodings, _face_names

    known_encodings = []
    known_names = []
    files = sorted(p for p in os.listdir(FACES_DIR) if p.lower().endswith((".jpg", ".png", ".jpeg"))) if FACES_DIR.exists() else []
    current_files = [os.path.basename(p) for p in files]

    if FACE_CACHE.exists():
        try:
            cache = json.loads(FACE_CACHE.read_text(encoding="utf-8"))
            if cache.get("files") == current_files:
                _face_encodings = [np.array(e) for e in cache["encodings"]]
                _face_names = cache["names"]
                return _face_encodings, _face_names
        except (json.JSONDecodeError, KeyError):
            pass

    import face_recognition

    for file_name in files:
        image = face_recognition.load_image_file(FACES_DIR / file_name)
        encodings = face_recognition.face_encodings(image)
        if not encodings:
            height, width = image.shape[:2]
            forced_box = [(0, width, height, 0)]
            encodings = face_recognition.face_encodings(image, known_face_locations=forced_box)
        if encodings:
            label = os.path.splitext(file_name)[0]
            known_encodings.append(encodings[0])
            known_names.append(label)

    try:
        data = {
            "files": current_files,
            "encodings": [e.tolist() for e in known_encodings],
            "names": known_names,
        }
        FACE_CACHE.write_text(json.dumps(data), encoding="utf-8")
    except OSError:
        pass

    _face_encodings = known_encodings
    _face_names = known_names
    return _face_encodings, _face_names


def recognize_face(jpeg, threshold=0.5):
    import face_recognition

    frame = decode_jpeg(jpeg)
    if frame is None:
        return None
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    encodings, names = _load_encodings()
    live = face_recognition.face_encodings(rgb)
    if not live:
        return "no_face"
    distances = face_recognition.face_distance(encodings, live[0]) if encodings else np.array([])
    if distances.size == 0:
        return "no_face"
    best_index = int(distances.argmin())
    if distances[best_index] < threshold:
        return names[best_index]
    return "unknown"


def save_jpeg(jpeg, directory):
    Path(directory).mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    path = Path(directory) / f"glasses_{stamp}.jpg"
    path.write_bytes(jpeg)
    return path