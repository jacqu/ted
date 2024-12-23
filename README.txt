TED: ENCRYPTED TEXT EDITOR FOR THE 6502

INTRODUCTION

“TED” is a lightweight text editor designed for computers powered by the 6502 CPU. Despite its simplicity, it features advanced cryptographic capabilities, utilizing an assembly-optimized cipher capable of processing data at nearly 1 kByte per second.

ORIC IMPLEMENTATION

“TED” is compatible with the Oric Atmos platform equipped with a Microdisc running the Sedoric operating system. The Oric’s 48kB of RAM, allow for allocating 250 lines of 40 columns to user-entered text.

The software fully supports the Oric MCP-40 color printer. Text rendered in white, red, green, and blue on-screen, corresponds to print colors of black, red, green, and blue on the printer. Inverted fonts are highlighted through an overlay printing technique, providing enhanced emphasis for selected text.

FILE HANDLING

Text can be saved to disk either encrypted or in plain text, depending on whether a password is entered or not. If an incorrect password is provided, an error message is displayed, prompting the user to try again.

Each save operation generates a .BAK file, ensuring the previous version can be recovered in case of issues. The size of saved files is approximately proportional to the length of the text. Filenames can be up to 9 characters long, with the .TED extension automatically appended.

When specifying filenames on the command line, they should be entered in lowercase and enclosed in single quotes. The closing quote is optional, and no space is required between "TED" and the opening quote.

In the event of an error during saving, a message is displayed on the status line detailing the cause of the issue. The program then automatically returns to edit mode, allowing the user to address the problem and attempt saving again without losing any data.

ENCRYPTION

TED implements an advanced encryption algorithm that utilizes a 256-bit key, allowing passwords to be up to 32 characters long. If no password is provided, the editor saves the content in plain text on the disk. For enhanced security, each save operation encrypts the data uniquely, using a randomly generated nonce to ensure variability. If a wrong password is given, the program prints an eror message and returns to the BASIC prompt.

EDITION

TED offers an intuitive, distraction-free editing experience. It allows text insertion at any position within the document, with automatic text wrapping set to a width of 40 columns. This feature ensures smooth typing, enabling users to focus on the content rather than screen formatting. A fast screen update routine provides seamless text scrolling, while convenient shortcuts make it easy to quickly navigate to the beginning or end of the document.

Status line

The status line provides key information, starting with the filename. If the file has been edited, an asterisk (*) is appended to the filename as an indicator. A helpful reminder of the shortcut to display the help screen is also included: pressing [CTRL]-G brings up a summary of all available shortcuts. Next, the status line displays the percentage of memory used by the text. If there is no space left for a new line, a “ping” sound alerts the user. Finally, the current character mode is indicated at the end of the status line: “NORML” denotes normal color mode, while “INVSE” indicates that the inverse bit of the character is active. This mode can be toggle with [CTRL]-O.

Navigation

The cursor represents the current insertion point and can be navigated using the arrow keys: one line up or down, and one character left or right. For faster navigation, an entire page can be scrolled up or down using [CTRL]-F and [CTRL]-B, respectively.

Shortcuts

Disk operation:
[CTRL]-S: Saves the file to disk. If a previous version of the file exists, it is renamed with a .BAK extension. For encrypted files, the process is as follows:
1. The text is encrypted in RAM.
2. The encrypted text is saved to disk.
3. The text is decrypted in RAM.

Clipboard Management:
[CTRL]-C: Copies the current line to the clipboard.
[CTRL]-X: Cuts the current line to the clipboard.
[CTRL]-V: Pastes the clipboard content at the cursor position.

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
* Abort Option: If you choose to abort, the printing process is canceled.
* Generic printer: If you select “No,” generic ASCII codes are sent to the printer. Inverted characters are printed twice, creating a bold effect.
* MCP-40 Plotter Printing: If you select “Yes,” the ink color attributes (red, blue, and green) are used to switch the corresponding pens on the plotter, enabling multicolor printing.
Long pressing any key aborts printing.

EXIT

[ESC]: Exits the program. If there are unsaved changes, the program prompts the user to save the file:
* Choosing "Yes" saves the changes before exiting.
* Choosing "No" discards all unsaved changes.

After exiting, the program automatically reboots the disk operating system.

THANKS

