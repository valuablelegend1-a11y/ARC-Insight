# ARC-Insight 0

## What it is:

ARC-Insight 0 is a pair of AI glasses. The frame carries a custom PCB with an onboard camera, mics, bone-conduction drivers behind the ears, and a heart-rate sensor. The glasses are a wearable "thin client": they capture audio, video, and sensors and stream them over Wi-Fi to Jarvis (my desktop AI assistant), who does the actual thinking and streams replies back through the bone conduction.

The goal is a personal assistant that lives on my face — face recognition, questions, and conversation, with the emotion of my heartbeat to tell whether I'm stressed or excited.

## How to use it:

Now that both halves are actually built (the firmware on the glasses and the server side that sits right next to Jarvis) it's really not that hard to get running, you build the firmware in PlatformIO, flash it to the glasses over USB just the one time, and on the PC you drop the arc_insight package into the J.A.R.V.I.S. folder, and since main.py already calls into it (I wrote in the bridge hook myself, it was like three lines, and after that nothing else in Jarvis ever needed to change, which was the whole point of the design) you just run `python main.py` exactly like you always have. The second the glasses are on and on the same Wi-Fi the server sees them, Jarvis prints `ARCINSIGHT: glasses connected`, and from then on anything you say into the glasses (even a whisper, the mics are honestly really good) gets heard and understood and answered, and his reply comes right back into your ears through the bone conduction drivers, which feels weird for the first day, but you get used to it (I did, anyway).

You can hop between the glasses and your desk anytime you want, either by asking, "jarvis, switch to glasses" or "jarvis, switch to laptop", or by just hitting Ctrl+Alt+G, and Jarvis moves his ears, his mouth, and his eyes to wherever you are, same him, same memory, same everything, he just relocates. While you're wearing them you've basically got his eyes on your face, he can tell you "what do you see", "who is this", "take a picture", and my personal favorite, "how am I feeling", which is where the heartbeat sensor earns its place, because Jarvis takes how fast your heart is beating, combines it with the tone of the words you say, and decides if you're stressed or excited or just having a chill day (and honestly he's been right more often than not).

When there's new firmware to put on the glasses you don't need to crack them open again, you just build the new image in PlatformIO, and run `python arc_insight\ota.py --bin ..\arc-insight\Code\.pio\build\arcinsight\firmware.bin`, which sends it over Wi-Fi on the same websocket connection, the glasses double check the image, and then reboot into the new version, and if it doesn't pass the check they just stay on the old one and you'll see it in the logs (hasn't really happened to me, but it's a nice safety net).

## Why I am building this:

- To learn real firmware development, power management, and how to package electronics into a wearable.
- To take Jarvis with me instead of keeping him chained to the desk.
- The heartbeat + speech sentiment combo lets Jarvis react to *how* I say things, not just *what* I say.

## Notes:

1. This is the first version of this project, so some things will change in future versions, like for instance the code may be optimized to run everything locally on the device, but that is not yet(look at note 3 for more information).
2. The code will not be all just written from memory for me for it all, I really am diving deep into this, but rest assured it will have some copy and paste from Google search, reddit snippets, or anything I need to make it work that I don't know.
3. Version 0 of this project is designed to be mostly a peripheral to my AI assistant J.A.R.V.I.S., who is a custom made Generative Prompt Transformer(GPT) based off of NanoGPT by Andrej Karpathy, with added tools for things like image and object recognition, memory, web search, and much much more. For more detail on that I will soon be making a repo to document that journey and how the AI runs now.
4. This is my first time designing a custom PCB, so forgive me if it is messy and not optimized, but I am quite proud of it and I think it will function how needed for this version.
