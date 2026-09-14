# ARC-Insight 0

## What it is:

ARC-Insight 0 is a pair of AI glasses. The frame carries a custom PCB with an onboard camera, mics, bone-conduction drivers behind the ears, and a heart-rate sensor. The glasses are a wearable "thin client": they capture audio, video, and sensors and stream them over Wi-Fi to Jarvis (my desktop AI assistant), who does the actual thinking and streams replies back through the bone conduction.

The goal is a personal assistant that lives on my face — face recognition, questions, and conversation, with the emotion of my heartbeat to tell whether I'm stressed or excited.

## How to use it:

First a quick primer, because this whole thing is two halves that have to work together, you have the glasses, which are the dumb half on purpose, they just record (microphone, camera, and your heart rate) and stream it all over Wi-Fi, and then you have Jarvis, which is the smart half, a desktop AI assistant I've been building for a while now, he's a custom language model with tools for things like object recognition, memory, web search, and much much more (I'm still deciding whether to ever make him open source, but if I do I'll drop a link here), and he does all the actual thinking and streams his reply back to the glasses so you hear it in your ears. Neither half is useful alone, you need both.

Setup happens in two places, and you only really do it once, the glasses and the PC.

The glasses half, the firmware lives in this repo at Code/firmware, you open it in PlatformIO (a coding tool for microcontrollers, sort of like a much more powerful Arduino IDE, it handles the build and the libraries for you), and before you build it the first time you need to put your Wi-Fi name and password, plus the address of the PC that runs Jarvis, into the little config file at Code/firmware/arcinsight_config.h (there are blanks right where they belong, you can't miss them), and then you plug the glasses in over USB, hit build and upload, and that's the only time you'll ever need a cable. After that the glasses just need power and to be on the same Wi-Fi as the PC, and they take care of themselves.

The PC half, you need Jarvis running (same desk assistant I mentioned, the glasses really are just a remote control for him), and you drop this arc_insight package into the same folder as the rest of his code, where it plugs in with a few lines and doesn't touch anything else (that was the whole design goal, he behaves exactly the same until the glasses show up), then you just start him up like normal, and when the glasses connect you'll see `ARCINSIGHT: glasses connected` in the console, and you're off.

Once they're connected it's all voice, you say "jarvis" (the same wake word you might already use at the desk, it becomes a habit fast) followed by whatever you need, like "what do you see", "who is this", "take a picture", or "how am I feeling", the last one is the heart rate sensor earning its keep, Jarvis reads your heartbeat, mixes it with the tone of what you said, and guesses if you're stressed or excited or just having a chill day, which is honestly the feature I'm most excited to see working. You can jump between the glasses and the desk whenever you want too, just say "jarvis, switch to glasses" or "jarvis, switch to laptop", or press Ctrl+Alt+G (the hotkey, it's easy to remap in the config), and Jarvis moves his ears, his mouth, and his eyes to wherever you are.

And when a newer firmware comes out, the cable stays in the drawer, you build the new image and run one little command, `python arc_insight\ota.py --bin ..\arc-insight\Code\.pio\build\arcinsight\firmware.bin`, and it streams the update over Wi-Fi on the same connection, the glasses verify it and reboot into the new version (if it fails the check they just stay on the old one, which is a nice safety net).

## Why I am building this:

- To learn real firmware development, power management, and how to package electronics into a wearable.
- To take Jarvis with me instead of keeping him chained to the desk.
- The heartbeat + speech sentiment combo lets Jarvis react to *how* I say things, not just *what* I say.

## Notes:

1. This is the first version of this project, so some things will change in future versions, like for instance the code may be optimized to run everything locally on the device, but that is not yet(look at note 3 for more information).
2. The code will not be all just written from memory for me for it all, I really am diving deep into this, but rest assured it will have some copy and paste from Google search, reddit snippets, or anything I need to make it work that I don't know.
3. Version 0 of this project is designed to be mostly a peripheral to my AI assistant J.A.R.V.I.S., who is a custom made Generative Prompt Transformer(GPT) based off of NanoGPT by Andrej Karpathy, with added tools for things like image and object recognition, memory, web search, and much much more. For more detail on that I will soon be making a repo to document that journey and how the AI runs now.
4. This is my first time designing a custom PCB, so forgive me if it is messy and not optimized, but I am quite proud of it and I think it will function how needed for this version.
