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

1. This is the first version of this project, so some things will change in future versions, like for instance the code may be optimized to run everything locally on the device, but that is not yet(look at note 3 for more information).
2. The code will not be all just written from memory for me for it all, I really am diving deep into this, but rest assured it will have some copy and paste from Google search, reddit snippets, or anything I need to make it work that I don't know.
3. Version 0 of this project is designed to be mostly a peripheral to my AI assistant J.A.R.V.I.S., who is a custom made Generative Prompt Transformer(GPT) based off of NanoGPT by Andrej Karpathy, with added tools for things like image and object recognition, memory, web search, and much much more. For more detail on that I will soon be making a repo to document that journey and how the AI runs now.
4. This is my first time designing a custom PCB, so forgive me if it is messy and not optimized, but I am quite proud of it and I think it will function how needed for this version.
