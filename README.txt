ED: ENCRYPTED TEXT EDITOR FOR THE 6502

INTRODUCTION

"ED" is a simple text editor for 6502 CPU-based computers from the 80s. It has
advanced cryptographic capabilities with an assembly-optimized chacha20
implementation running at nearly 1 kBytes/s. It was designed to provide a
reliable means of storing sensitive information, using a modern and efficient
encryption algorithm along with old and thus safe electronic components,
eliminating any risk of pernicious hardware backdoor implantation from the
factory.

THANKS

The development of "ED" would not have been possible without the fantastic tools
provided by the community, and especially cc65, a pretty reliable C compiler for
6502 platforms and the various emulators available like Clock Signal or
Oricutron.

ORIC IMPLEMENTATION

"ED" works on the Oric Atmos platform with the Sedoric 3.0 operating system.
Since only 32kB of RAM is available, only 250 lines of 40 columns are available.
