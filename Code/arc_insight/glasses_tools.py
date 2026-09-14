from jarvis_core.tool_registry import Tool

from arc_insight import vision
from arc_insight.config import data_path

SEE_PHRASES = ("what do you see", "what can you see", "what am i looking at", "what am i seeing")
WHO_PHRASES = ("who is this", "who is that", "who am i looking at", "do you know who", "do you know this person")
PICTURE_PHRASES = ("take a picture", "take a photo", "snap a photo", "take another picture")
EMOTION_PHRASES = (
    "how am i feeling", "how do i feel", "check my stress", "am i stressed",
    "is my heart rate high", "how is my mood", "what is my mood", "what is my emotional state",
    "emotional state", "mood trend",
)

def build_glasses_tools(io, cfg, emotion):
    tools = []

    def in_glasses(io):
        return io.channel == "glasses" and io.connected

    def is_shielded(prompt):
        return "screen" in prompt or "on my pc" in prompt or "on my computer" in prompt

    tools.append(Tool(
        "glasses_see",
        "identify objects through the glasses camera when worn",
        lambda p: in_glasses(io) and any(phrase in p for phrase in SEE_PHRASES) and not is_shielded(p),
        lambda p: _see_handler(io),
    ))
    tools.append(Tool(
        "glasses_who",
        "recognize the person in front of the glasses camera",
        lambda p: in_glasses(io) and any(phrase in p for phrase in WHO_PHRASES),
        lambda p: _who_handler(io),
    ))
    tools.append(Tool(
        "glasses_picture",
        "take a picture with the glasses camera and save it",
        lambda p: in_glasses(io) and any(phrase in p for phrase in PICTURE_PHRASES),
        lambda p: _picture_handler(io, cfg),
    ))
    tools.append(Tool(
        "glasses_emotion",
        "report the current emotional state from heart rate and words",
        lambda p: any(phrase in p for phrase in EMOTION_PHRASES),
        lambda p: _emotion_handler(io, emotion),
    ))
    tools.append(Tool(
        "glasses_switch",
        "switch Jarvis between the glasses and the laptop",
        lambda p: _switch_target(p) is not None,
        lambda p: _switch_handler(io, cfg, p, emotion),
    ))
    return tools


def _switch_target(prompt):
    text = prompt.lower()
    laptop_phrases = ("switch to laptop", "switch to my laptop", "laptop mode", "use the laptop", "talk to me on the laptop")
    glasses_phrases = ("switch to glasses", "glasses mode", "use the glasses", "put it on the glasses", "talk to me through the glasses")
    to_laptop = any(phrase in text for phrase in laptop_phrases)
    to_glasses = any(phrase in text for phrase in glasses_phrases)
    if to_laptop:
        return "laptop"
    if to_glasses:
        return "glasses"
    return None


def _see_handler(io):
    frame = io.fresh_frame()
    if frame is None:
        return "The glasses camera is not available right now, Sir."
    detections = vision.detect_objects(frame)
    if detections is None:
        return "I could not process the glasses camera frame, Sir."
    message = vision.format_detections(detections)
    return message.replace("I did not", "I could not")


def _who_handler(io):
    frame = io.fresh_frame()
    if frame is None:
        return "The glasses camera is not available right now, Sir."
    identity = vision.recognize_face(frame)
    if identity is None:
        return "I could not process the glasses camera frame, Sir."
    if identity == "no_face":
        return "I cannot see a face through the glasses camera right now, Sir."
    if identity == "unknown":
        return "I can see a face, but I do not recognize who it is yet, Sir."
    return f"That is {identity}."


def _picture_handler(io, cfg):
    frame = io.fresh_frame()
    if frame is None:
        return "The glasses camera is not available right now, Sir."
    directory = data_path(cfg, "screenshot_dir")
    path = vision.save_jpeg(frame, directory)
    return "Picture saved, Sir."


def _emotion_handler(io, emotion):
    if emotion is None:
        return "The emotion system is not enabled, Sir."
    summary = emotion.describe()
    if not summary:
        return "I do not have enough signal to gauge your state yet, Sir."
    return summary


def _switch_handler(io, cfg, prompt, emotion):
    target = _switch_target(prompt)
    if target is None:
        return "Switching, Sir."
    if io.switch(target):
        if emotion is not None:
            try:
                emotion.on_channel_switch(target)
            except Exception:
                pass
        return f"Switching to {target}, Sir."
    return f"Cannot switch to {target}; the glasses are not connected, Sir."