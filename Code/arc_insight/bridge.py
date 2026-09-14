import asyncio
import io as _io_module

import edge_tts

from arc_insight.channels import IOManager
from arc_insight.config import load_config
from arc_insight.glasses_tools import build_glasses_tools
from arc_insight.server import GlassesServer

_state = {}


def install(speaker, main_module, config=None):
    cfg = config or load_config()
    emotion = None
    if cfg["emotion"]["enabled"]:
        from arc_insight.emotion import EmotionEngine

        emotion = EmotionEngine(cfg)

    io = IOManager(cfg, emotion=emotion)
    server = GlassesServer(io, cfg)
    io.server = server

    _patch_speaker(speaker, io, cfg)
    _patch_registry(io, cfg, emotion)
    _start_hotkey(io, cfg)

    _state.update(
        cfg=cfg,
        io=io,
        server=server,
        speaker=speaker,
        main=main_module,
        emotion=emotion,
        installed=True,
    )
    print(f"ARCINSIGHT: installed (channel=laptop default, hotkey={cfg['handoff']['hotkey']})", flush=True)


def _patch_speaker(speaker, io, cfg):
    original = type(speaker).say

    async def say(words):
        if io.channel == "glasses" and io.connected:
            mp3 = await _tts_bytes(words, speaker.voice_name)
            if mp3 and await io.server.send_tts(mp3):
                from jarvis_core.ui_state import clear_ui_state, set_ui_state

                set_ui_state("speaking")
                clear_ui_state()
                return
        await original(speaker, words)

    speaker.say = say


async def _tts_bytes(words, voice_name):
    communicate = edge_tts.Communicate(words, voice_name)
    buffer = _io_module.BytesIO()
    async for chunk in communicate.stream():
        if chunk.get("type") == "audio":
            buffer.write(chunk["data"])
    data = buffer.getvalue()
    return data or None


def _patch_registry(io, cfg, emotion):
    import jarvis_core.command_tools as command_tools

    original = command_tools.get_registry

    def get_registry():
        registry = original()
        tools = build_glasses_tools(io, cfg, emotion)
        for tool in reversed(tools):
            registry.tools.insert(0, tool)
        return registry

    command_tools.get_registry = get_registry


def _start_hotkey(io, cfg):
    hotkey = cfg["handoff"]["hotkey"]
    if not hotkey:
        return

    def toggle():
        target = "laptop" if io.channel == "glasses" else "glasses"
        io.switch(target)

    try:
        import keyboard

        keyboard.add_hotkey(hotkey, toggle)
    except Exception as exc:
        print(f"ARCINSIGHT: hotkey unavailable ({exc})", flush=True)


def get_input():
    return _state.get("io")


def get_emotion():
    return _state.get("emotion")


def is_installed():
    return bool(_state.get("installed"))


async def next_input(recognizer, source):
    state = _state
    io = state["io"]
    main = state["main"]
    server = state["server"]
    ensure_started()

    if io.channel != "glasses" or not io.connected:
        return main.speech_input(recognizer, source)

    while True:
        ui_text = main.pop_ui_input()
        if ui_text:
            return ui_text, "ui"
        if main.typed_input_ready():
            text = main.read_typed_input()
            if text:
                return text, "typed"
        try:
            text = io.utterances.get_nowait()
        except asyncio.QueueEmpty:
            await asyncio.sleep(0.05)
            continue
        return text, "speech"


def ensure_started():
    server = _state.get("server")
    if server is None or server._loop is not None:
        return
    try:
        loop = asyncio.get_running_loop()
    except RuntimeError:
        return
    if server._loop is not None:
        return
    server._loop = loop
    io = _state["io"]
    io.loop = loop
    loop.create_task(server.run())
    loop.create_task(server.frame_loop())


async def start():
    ensure_started()