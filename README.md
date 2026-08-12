# TED: A Frugal Text Editor for the 6502 with Modern Encryption

I recently developed an interest in frugal engineering in general and frugal computing in particular. TED is my attempt to create a tiny yet functional text editor with all the essential features, coupled with a state-of-the-art encryption algorithm — **ChaCha20**.

TED runs smoothly on a 6502-based platform clocked at just **1 MHz**. Despite the limited processing power, it achieves an encryption throughput of **950 bytes per second**, which is quite impressive for a CPU of this era.

## Lessons Learned

Throughout this journey, I gained three key insights: **memset**, **memcpy**, and **memmove**. If you want to extract every ounce of performance from your CPU, these routines are indispensable in your C code. They significantly boost efficiency by inlining assembly-optimized operations.

However, for true performance optimization, writing directly in assembly language is the way to go. I also discovered that macros make assembly programming much simpler—almost as convenient as C, once you get accustomed to them.

## How to Run TED

This repository includes a **README.txt** explaining how TED works. To run it, you’ll need an Oric emulator such as **Clock Signal** or **Oricutron**. Alternatively, you can run TED on real **Oric** hardware using a floppy disk drive operated by **Sedoric**.

The disk image **ted_release.hfe** contains the readme.ted file that is readable within the emulator or on a real Oric.

The other disk images **ted.dsk** and **ted.hfe** contain only the editor program.

Below, you’ll find a short demo of TED in action.

Enjoy!

Jacques

[![TED DEMO](http://img.youtube.com/vi/e0JQoOaf2OE/0.jpg)](http://www.youtube.com/watch?v=e0JQoOaf2OE)

## Authentication

Every file carries a 16 byte tag: keyed **BLAKE2s truncated to 128 bits**,
computed over the whole file except the tag itself, with a key derived
from the password. A text that does not match its tag has been damaged or
opened with the wrong password, and the editor says so and asks before
opening it anyway.

BLAKE2s rather than Poly1305, the usual companion of ChaCha20, because
Poly1305 is a big-integer multiply and the 6502 has no multiplier, while
BLAKE2s is built from the same three operations as ChaCha20. Its `G`
function is the ChaCha quarter round with the rotations turned the other
way and two message words folded in, so `blake2s.s` is `chacha20.s` with
the rotations reversed. It is the same file on the Commodore 64, as is
the construction around it, which is what keeps the two versions able to
read each other's files.

The tag is computed over the ciphertext, so a wrong password is caught
before anything is decrypted: the text is left intact and another
password can be tried without reloading.

Three keys are derived from the password, one per use, so that knowing
one says nothing about the others. A text saved without a password is
still tagged, with a key of zeros: that detects damage, and is not
authentication, since anyone can compute it.

The nonce used to be six bytes read from the timers at the moment of the
save. It is now twelve bytes derived from a pool that is fed a sample of
the VIA and ULA counters at **every keystroke of the session**: what is
unpredictable is not the value of a counter but the instant a key was
pressed.

Files written by earlier versions are refused with a clear message
rather than opened as noise.

## Memory

The C stack lives in the alternate character set at `$B800`, the 896
bytes of it that TED never uses, instead of below the character sets.
That hands the kilobyte it used to occupy back to the text, which is
what makes room for the hash. The formatting of the printf family was
replaced by `strfmt`, as on the Commodore 64, for the same reason.

Data now ends at `$B322`, just under the standard character set at
`$B400`, one glyph of which TED redefines.
