# TED: A Frugal Text Editor for the 6502 with Modern Encryption

I recently got interested in frugal engineering in general and frugal computing in particular. TED is an attempt to develop a tiny text editor with all the basics features and an state of the art encryption algorithm (ChaCha20).

It runs smoothely on a 6502 platform clocked at a whoping 1MHz. The encryption achieves a throughput of 950 Bytes/s which is pretty solid for a CPU of this era.

I learned 3 lessons from this journey: memset, memcpy, memmove. If you want to extract all the juice of your CPU, you should heavily rely on these three routines in your C code. They improve a lot the performace since they inline some assembly-optimized code.

Of course, if you want to be really efficient, you have to write your code directly in assmebly language. I discovered that macros makes your life a lot simplier , almost as easy than C code when you are accustomed.

In this repo you will find a README.txt file explaining how TED works. You will need an emulator for the Oric (Clock Signal or Oricutron) or you can run the editor directly on the real hardware with a floppy disk.

Below you will find a little demo of TED.

Enjoy!
Jacques
