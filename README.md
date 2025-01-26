# TED: A Frugal Text Editor for the 6502 with Modern Encryption

I recently developed an interest in frugal engineering in general and frugal computing in particular. TED is my attempt to create a tiny yet functional text editor with all the essential features, coupled with a state-of-the-art encryption algorithm—ChaCha20.

TED runs smoothly on a 6502-based platform clocked at just 1 MHz. Despite the limited processing power, it achieves an encryption throughput of 950 bytes per second, which is quite impressive for a CPU of this era.

## Lessons Learned

Throughout this journey, I gained three key insights: memset, memcpy, and memmove. If you want to extract every ounce of performance from your CPU, these routines are indispensable in your C code. They significantly boost efficiency by inlining assembly-optimized operations.

However, for true performance optimization, writing directly in assembly language is the way to go. I also discovered that macros make assembly programming much simpler—almost as convenient as C, once you get accustomed to them.

## How to Run TED

This repository includes a README.txt explaining how TED works. To run it, you’ll need an Oric emulator such as *Clock Signal* or *Oricutron*. Alternatively, you can run TED on real Oric hardware using a floppy disk drive operated by *Sedoric*.

Below, you’ll find a short demo of TED in action.

Enjoy!
Jacques
[![Watch the video]](https://youtu.be/e0JQoOaf2OE)