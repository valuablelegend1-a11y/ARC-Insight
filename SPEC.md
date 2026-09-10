# ARC-Insight Spec Sheet (v0.1)

## Architecture

The glasses are a 'wearable thin client'. The Jarvis brain stays on the desktop PC:

- Glasses (ESP32-S3 XIAO Sense): capture mic audio, on-demand camera frames, on-demand heart rate; play TTS audio via bone conduction.
- PC (Jarvis): STT (Google), the trained nanoGPT model, tools (weather, search, face rec via YOLO, memory, etc.), TTS (edge-tts). Streams synthesized audio back to the glasses.

The ESP32-S3 (512 KB SRAM) physically cannot run the Jarvis transformer, so no attempt is made to run a model on the frame.

## Hardware

- **Board:** Seeed Studio XIAO ESP32-S3 Sense (ESP32-S3R8, 240 MHz dual-core, 8 MB PSRAM, 8 MB flash)
- **Camera:** OV2640 / OV3660 on the Sense flex connector (2 MP), PSRAM-backed framebuffer
- **Mics:** onboard PDM mic (primary); external INMP441 I2S as fallback if placement is bad
- **Output:** 2x bone-conduction transducers behind the ears, driven by MAX98357A I2S amps
- **Sensors:** MAX30102 PPG heart-rate (I2C), power-gated
- **Power:** 2x ultra-slim 1S LiPo (300-400 mAh) in parallel -> XIAO BAT+ / BAT- pads, charged by onboard 100 mA charger via USB-C
- **Wake:** external electret + envelope comparator -> GPIO wake from deep sleep

## Power model

| State | Draw | Notes |
|---|---|---|
| Deep sleep (idle) | ~3 mA | Floor with Sense expansion attached; ~6-7 days on 2x300 mAh |
| Wake + streaming mic over Wi-Fi | ~55-110 mA | Mic-only ~54 mA, Wi-Fi active ~110 mA |
| Speaking (amp on) | +20-50 mA | Gate amp off otherwise |
| Camera capture | ~150-300 mA | Bursts only, <1 s per frame |
| Heart-rate burst | +1 mA | ~15-30 s samples on demand |
| Charge | 100 mA | Onboard charger, ~6 h to full |

Expected: ~1-2 days per charge with a couple hours of conversation per day.


## Emotion mapping (Jarvis tool)

Read heart rate in short bursts and combine with the sentiment of the latest utterance:

- High HR + negative words -> stressed
- High HR + positive/energetic words -> excited
- Low HR -> calm

Implemented later as a `read_mood` tool in the Jarvis `route_tool` pipeline.

3. On-device wake word (ESP-SR WakeNet) to stop constant streaming.
4. Camera capture -> JPEG -> PC YOLO (reuse existing vision tools).
5. Heart-rate sensor + DSP + logging.
6. Battery mgmt, carrier PCB, mechanical integration into the frame.
