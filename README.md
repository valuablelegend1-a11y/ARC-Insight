# ARC-Insight 0

## What it is:

ARC-Insight 0 is a pair of AI glasses. The frame carries a custom PCB with an onboard camera, mics, bone-conduction drivers behind the ears, and a heart-rate sensor. The glasses are a wearable "thin client": they capture audio, video, and sensors and stream them over Wi-Fi to Jarvis (my desktop AI assistant), who does the actual thinking and streams replies back through the bone conduction.

The goal is a personal assistant that lives on my face — face recognition, questions, and conversation, with the emotion of my heartbeat to tell whether I'm stressed or excited.

## How to use it:

*(WIP — firmware not written yet. Design phase.)*

## Why I am building this:

- To learn real firmware development, power management, and how to package electronics into a wearable.
- To take Jarvis with me instead of keeping him chained to the desk.
- The heartbeat + speech sentiment combo lets Jarvis react to *how* I say things, not just *what* I say.

## Notes:

1. Design is still in progress — see [SPEC.md](SPEC.md) for the full spec sheet.
2. Parts list is in [BOM.csv](BOM.csv). Nothing has been purchased yet.
3. The "brain" (the trained Jarvis model) stays on my PC. The ESP32-S3 physically cannot run it, and that's by design.
4. Build progress is logged in [JOURNAL.md](JOURNAL.md).
