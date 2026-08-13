TED: ENCRYPTED TEXT EDITOR FOR THE 6502

INTRODUCTION

"TED" is a lightweight text editor designed for computers powered by the 6502 CPU. Despite its simplicity, it features advanced cryptographic capabilities, utilizing an assembly-optimized cipher capable of processing data at nearly 1 kByte per second.

ORIC IMPLEMENTATION

"TED" runs on an Oric Atmos fitted with a Microdisc and the Sedoric operating system. The 48kB of RAM allow for allocating 350 lines of 40 columns to user-entered text.

The screen shows 28 lines, one of them being the status line. A Commodore 64 port exists, "ted64", which shows 25 lines instead. Only how much text is visible at once changes: a file written on either machine is made of lines of 40 characters and is readable on the other one.

The software fully supports the Oric MCP-40 color printer. Text rendered in white, red, green, and blue on-screen, corresponds to print colors of black, red, green, and blue on the printer. Inverted fonts are highlighted through an overlay printing technique, providing enhanced emphasis for selected text.

STARTING THE EDITOR

The editor is started by typing TED followed by the file name at the BASIC prompt:

  TED'readme'

The name is entered in lowercase without extension, enclosed in single quotes. The closing quote is optional, and no space is required between TED and the opening quote.

Typing TED alone displays the usage screen.

FILE HANDLING

Text can be saved to disk either encrypted or in plain text, depending on whether a password is entered or not.

Each save operation generates a .BAK file, ensuring the previous version can be recovered in case of issues. The size of saved files is approximately proportional to the length of the text. Filenames can be up to 9 characters long, with the .TED extension automatically appended.

In the event of an error during saving, a message is displayed on the status line detailing the cause of the issue. The program then automatically returns to edit mode, allowing the user to address the problem and attempt saving again without losing any data.

ENCRYPTION

TED encrypts with a 256-bit key, so passwords may be up to 32 characters long. Without a password the text is saved in clear. Each save draws a fresh nonce, so the same text never encrypts twice to the same bytes.

The cipher is ChaCha20, written in assembly language, and is strictly the same routine as the one of the Commodore 64 version, so a text encrypted on one machine is decrypted on the other one.

The password is never used as it stands. Three keys are derived from it, one per use, so that knowing one of them says nothing about the others: one encrypts the text, one authenticates the file, and one draws the nonce.

AUTHENTICATION

Every file carries a 16 byte tag computed over everything it holds, with a key derived from the password. The tag is keyed BLAKE2s, truncated to 128 bits, and is written in assembly language like the cipher.

A text whose tag does not match has been damaged or opened with the wrong password. The editor says which it cannot tell, and asks whether to open it anyway. Answering no leaves the machine at the BASIC prompt with nothing altered.

The tag is checked before anything is decrypted, so a wrong password costs nothing: the text is left as it was and another password can be tried without reloading.

A text saved without a password is still given a tag, computed with a key of zeros. That detects a damaged file, and is not authentication, since anybody can compute it.

The nonce is drawn from a pool of entropy gathered at every keystroke of the session, mixed with the free running counters of the machine, the length of the file and the shape of the text. What is unpredictable there is not the value of a counter but the instant a key happened to be pressed, and a session offers hundreds of those.

Files written by earlier versions of TED are refused with a message rather than opened as noise: the format changed when the tag was added.

EDITION

Text is inserted at any position in the document and wraps automatically at 40 columns, so nothing has to be laid out by hand. A fast screen update keeps the scrolling smooth, and shortcuts jump to the beginning and to the end of the document.

Status line

The status line starts with the filename, followed by an asterisk when the file has been edited. Then comes the percentage of the memory the text uses; when no room is left for a new line, a ping warns the user. The character mode ends the line: STD is the normal one, INV means the inverse bit of the character is set, and [CTRL]-O switches between them. [CTRL]-G brings up a summary of all the shortcuts.

Navigation

The cursor represents the current insertion point and can be navigated using the arrow keys: one line up or down, and one character left or right. For faster navigation, an entire page can be scrolled up or down using [CTRL]-F and [CTRL]-B, respectively.

Shortcuts

Disk operation:
[CTRL]-S: Saves the file to disk. If a previous version of the file exists, it is renamed with a .BAK extension. For encrypted files, the process is as follows:
1. The text is encrypted in RAM.
2. The file is sealed with its tag.
3. The sealed text is saved to disk.
4. The text is decrypted in RAM.

Clipboard Management:
[CTRL]-C: Copies the current line to the clipboard.
[CTRL]-X: Cuts the current line to the clipboard.
[CTRL]-V: Pastes the clipboard content at the cursor position.

Editing aids:
[CTRL]-Z: Inserts a soft tabulation.
[CTRL]-N: Switches the screen saver on and off.

Color Attribute Handling:
On the Oric Atmos, attribute characters can be inserted to control video chip settings. These characters appear as blanks on the screen but act as control signals for color changes, valid for the current line only. The following shortcuts allow changes to ink (foreground) and paper (background) colors:
Ink Colors:
[CTRL]-Q: White ink
[CTRL]-W: Red ink
[CTRL]-E: Green ink
[CTRL]-R: Blue ink
[CTRL]-T: Black ink
Paper Colors:
[CTRL]-Y: Black paper
[CTRL]-U: Red paper
[CTRL]-A: Yellow paper
[CTRL]-D: Blue paper

PRINTING

[CTRL]-P: Opens the print dialog, prompting the user to specify whether an Oric MCP-40 color plotter is being used.
Abort Option: If you choose to abort, the printing process is canceled.
Generic printer: If you select No, generic ASCII codes are sent to the printer. Inverted characters are printed twice, creating a bold effect.
MCP-40 Plotter Printing: If you select Yes, the ink color attributes (red, blue, and green) are used to switch the corresponding pens on the plotter, enabling multicolor printing.
Long pressing any key aborts printing.

EXIT

[ESC]: Exits the program. If there are unsaved changes, the program prompts the user to save the file:
Choosing Yes saves the changes before exiting.
Choosing No discards all unsaved changes.

On the way out the text and the password are erased from the memory, and the screen is given back to BASIC.

THANKS

Special thanks to the fantastic Oric and Commodore communities for all their contributions, without which this project would never have come to life.
