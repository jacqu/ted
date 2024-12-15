TED: ENCRYPTED TEXT EDITOR FOR THE 6502

INTRODUCTION

“TED” is a lightweight text editor designed for computers powered by the 6502 CPU. Despite its simplicity, it features advanced cryptographic capabilities, utilizing an assembly-optimized cipher capable of processing data at nearly 1 kByte per second.

ORIC IMPLEMENTATION

“TED” is compatible with the Oric Atmos platform equipped with a Microdisc running the Sedoric operating system. The Oric’s 48kB of RAM, allow for allocating 250 lines of 40 columns to user-entered text.

The software fully supports the Oric MCP-40 color printer. Text rendered in white, red, green, and blue on-screen, corresponds to print colors of black, red, green, and blue on the printer. Inverted fonts are highlighted through an overlay printing technique, providing enhanced emphasis for selected text.

FILE HANDLING

Text can be saved to disk either encrypted or in plain text, depending on whether a password is entered or not. If an incorrect password is provided, an error message is displayed, prompting the user to try again.

Each save operation generates a .BAK file, ensuring the previous version can be recovered in case of issues. The size of saved files is approximately proportional to the length of the text. Filenames can be up to 9 characters long, with the .CHA extension automatically appended.

When specifying filenames on the command line, they should be entered in lowercase and enclosed in single quotes. The closing quote is optional, and no space is required between "TED" and the opening quote.

In the event of an error during saving, a message is displayed on the status line detailing the cause of the issue. The program then automatically returns to edit mode, allowing the user to address the problem and attempt saving again without losing any data.

ENCRYPTION

TED implements an advanced encryption algorithm that utilizes a 256-bit key, allowing passwords to be up to 32 characters long. If no password is provided, the editor saves the content in plain text on the disk. For enhanced security, each save operation encrypts the data uniquely, using a randomly generated nonce to ensure variability. If a wrong password is given, the program prints an eror message and returns to the BASIC prompt. 