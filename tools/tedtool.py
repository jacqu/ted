#!/usr/bin/env python3
# ==================================================================== #
# tedtool: handling of the .ted files of TED outside of a 6502          #
#                                                                       #
# A .ted file is the raw image of struct textstore_struct, whose layout #
# is identical on the Oric and on the Commodore 64:                     #
#                                                                       #
#   offset 0     nonce, 12 bytes                                        #
#   offset 12    number of lines, 16 bits little endian                 #
#   offset 14    line pointers, 350 addresses of 16 bits                #
#   offset 714   line pointer flags, 350 bytes                          #
#   offset 1064  line lengths, 350 bytes                                #
#   offset 1414  magic number, 32 bits little endian                    #
#   offset 1418  text buffer, 350 lines of 40 characters                #
#                                                                       #
# Only the magic number and the text buffer are encrypted, which is     #
# what lets ted64 rebase the line pointers of a file coming from        #
# another machine before decrypting it.                                 #
#                                                                       #
# A file saved by the Oric holds the payload alone, Sedoric keeping the #
# load address in the directory of the disk. A file saved by the        #
# Commodore 64 is an ordinary PRG, whose two first bytes hold the load  #
# address. That two byte header is the only difference between the two  #
# machines, and this tool adds or removes it.                           #
#                                                                       #
# Usage:                                                                #
#   tedtool.py to-c64   in.ted  out.prg --address 0xNNNN                #
#   tedtool.py to-oric  in.prg  out.ted                                 #
#   tedtool.py from-text in.txt out.prg --address 0xNNNN [--oric]       #
#   tedtool.py to-text  in.ted  out.txt [--oric]                        #
#   tedtool.py info     in.ted                                          #
# ==================================================================== #

import argparse
import hashlib
import sys

# --- Layout of struct textstore_struct ------------------------------- #
LINES_MAX = 350                     # TEXTSTORE_LINES_MAX
LINE_SIZE = 40                      # TEXTSTORE_LINE_SIZE
NONCE_SIZE = 12                     # TEXTSTORE_NONCE_SZ
TAG_SIZE = 16                       # TEXTSTORE_TAG_SZ
KEY_SIZE = 32                       # TEXTEDIT_KEY_SZ
VERSION = 2                         # TEXTSTORE_VERSION
MAGIC = 0x94C910FF                  # TEXTSTORE_MAGIC

# Labels TED derives a key per use from, and which have to be spelled
# here exactly as they are spelled in textedit.h
TAG_LABEL = b"ted tag key 2"        # TEXTEDIT_TAG_LABEL

OFFSET_TAG = 0
OFFSET_NONCE = OFFSET_TAG + TAG_SIZE
OFFSET_VERSION = OFFSET_NONCE + NONCE_SIZE
OFFSET_FSIZE = OFFSET_VERSION + 2
OFFSET_NBLINES = OFFSET_FSIZE + 2
OFFSET_POINTERS = OFFSET_NBLINES + 2
OFFSET_FLAGS = OFFSET_POINTERS + 2 * LINES_MAX
OFFSET_SIZES = OFFSET_FLAGS + LINES_MAX
OFFSET_MAGIC = OFFSET_SIZES + LINES_MAX
OFFSET_BUFFER = OFFSET_MAGIC + 4

# --- Character codes ------------------------------------------------- #
CHAR_SPACE = 32                     # TEXTSTORE_CHAR_SPACE
CHAR_RET = 95                       # TEXTSTORE_CHAR_RET
ASCII_MIN = 32                      # TEXTEDIT_ASCII_MIN
ASCII_MAX = 126                     # TEXTEDIT_ASCII_MAX

# --- Line pointer flags ---------------------------------------------- #
LINEPT_USED = 0                     # TEXTSTORE_LINEPT_USED
LINEPT_FREE = 1                     # TEXTSTORE_LINEPT_FREE

# --- Miscellaneous --------------------------------------------------- #
PRG_HEADER_SIZE = 2                 # Size of the load address of a PRG file
DEFAULT_TAB_SIZE = 4                # Width a tabulation is expanded to


def read_file(name):
    """Read a whole file and return its content as a bytearray."""
    with open(name, "rb") as handle:
        return bytearray(handle.read())


def write_file(name, data):
    """Write a bytearray to a file."""
    with open(name, "wb") as handle:
        handle.write(bytes(data))


def word(data, offset):
    """Read a 16 bit little endian value."""
    return data[offset] | (data[offset + 1] << 8)


def put_word(data, offset, value):
    """Write a 16 bit little endian value."""
    data[offset] = value & 0xFF
    data[offset + 1] = (value >> 8) & 0xFF


def put_long(data, offset, value):
    """Write a 32 bit little endian value."""
    for index in range(4):
        data[offset + index] = (value >> (8 * index)) & 0xFF


def check_payload(payload):
    """Complain when the payload cannot be a .ted file."""
    if len(payload) < OFFSET_BUFFER:
        sys.exit("error: file is too short to be a .ted file")
    nblines = word(payload, OFFSET_NBLINES)
    if nblines > LINES_MAX:
        sys.exit("error: line count of %d is out of range" % nblines)
    return nblines


def layout_text(text, tab_size):
    """Fold a plain text the exact way the editor itself would.

    textedit_insert accumulates a word until it meets a space, a carriage
    return or the width of a line, then places that word on the current
    line if it fits and opens a new one otherwise. Reproducing that very
    algorithm here guarantees that the file opens without being reflowed,
    and that every wrapped line still ends with the space separating it
    from the next word, which is what the editor expects.
    """
    stream = []

    if text.endswith("\n"):
        text = text[:-1]

    for paragraph in text.split("\n"):
        paragraph = paragraph.replace("\t", " " * tab_size)

        # A word longer than a line would lose its tail, so it is cut
        pieces = []
        for item in paragraph.split(" "):
            while len(item) >= LINE_SIZE:
                pieces.append(item[:LINE_SIZE-1])
                item = item[LINE_SIZE-1:]
            pieces.append(item)
        paragraph = " ".join(pieces)

        # Keep only the characters TED is able to display
        for character in paragraph:
            code = ord(character)
            stream.append(code if ASCII_MIN <= code <= ASCII_MAX else CHAR_SPACE)

        # A line break of the source closes the paragraph
        stream.append(CHAR_RET)

    lines = [[]]
    word = []

    for index, code in enumerate(stream):

        # Accumulate the current word
        if len(word) < LINE_SIZE:
            word.append(code)

        # End of a word: place it on the current line or on a new one
        if (code in (CHAR_SPACE, CHAR_RET) or len(word) == LINE_SIZE or
                index == len(stream) - 1):
            if len(word) + len(lines[-1]) > LINE_SIZE:
                lines.append([])
            lines[-1].extend(word)

            # A carriage return closes the current line
            if code == CHAR_RET:
                lines.append([])

            word = []

    if len(lines) > LINES_MAX:
        sys.exit("error: the text needs %d lines, the maximum is %d"
                 % (len(lines), LINES_MAX))

    return ["".join(chr(code) for code in line) for line in lines]


def tag_key(password):
    """Return the key TED authenticates a file with.

    Each use of the password gets a key of its own, derived from it and
    from a label naming what it is for. A file saved without a password
    is keyed with zeros, which detects damage but authenticates nothing,
    exactly as TED does it.
    """
    master = password if password else bytes(KEY_SIZE)
    return hashlib.blake2s(TAG_LABEL, key=master, digest_size=KEY_SIZE).digest()


def seal(payload, password=None):
    """Fill in the size and the tag of a payload about to be written."""
    put_word(payload, OFFSET_VERSION, VERSION)
    put_word(payload, OFFSET_FSIZE, len(payload))

    tag = hashlib.blake2s(bytes(payload[OFFSET_NONCE:]),
                          key=tag_key(password),
                          digest_size=TAG_SIZE).digest()
    payload[OFFSET_TAG:OFFSET_TAG + TAG_SIZE] = tag
    return payload


def build_payload(lines, base_address):
    """Build a .ted payload holding the given lines of text."""
    payload = bytearray(OFFSET_BUFFER + LINES_MAX * LINE_SIZE)

    # Plain text file: the nonce is left cleared
    for index in range(NONCE_SIZE):
        payload[OFFSET_NONCE + index] = 0

    put_word(payload, OFFSET_NBLINES, len(lines))
    put_long(payload, OFFSET_MAGIC, MAGIC)

    # Every line is free until it is given away below
    for index in range(LINES_MAX):
        payload[OFFSET_FLAGS + index] = LINEPT_FREE
        payload[OFFSET_SIZES + index] = 0

    # The whole text buffer is blank
    for index in range(LINES_MAX * LINE_SIZE):
        payload[OFFSET_BUFFER + index] = CHAR_SPACE

    for number, line in enumerate(lines):
        address = base_address + OFFSET_BUFFER + number * LINE_SIZE
        put_word(payload, OFFSET_POINTERS + 2 * number, address)
        payload[OFFSET_FLAGS + number] = LINEPT_USED
        payload[OFFSET_SIZES + number] = len(line)
        for column, character in enumerate(line):
            payload[OFFSET_BUFFER + number * LINE_SIZE + column] = ord(character)

    # Only the part of the buffer actually in use is saved by TED, and the
    # tag is computed over exactly what is going to be written
    used = OFFSET_BUFFER + len(lines) * LINE_SIZE
    return seal(payload[:used])


def extract_text(payload):
    """Turn a .ted payload back into plain text.

    The line pointers are absolute addresses of the machine that wrote the
    file, so they are turned into buffer indexes exactly the way
    textstore_fix_pointers does inside TED.
    """
    nblines = check_payload(payload)
    if not nblines:
        return ""

    # Lowest pointer in use and index of the first slot it stands for
    lowest = min(word(payload, OFFSET_POINTERS + 2 * n) for n in range(nblines))
    first_used = next(
        (n for n in range(LINES_MAX) if payload[OFFSET_FLAGS + n] == LINEPT_USED),
        0,
    )

    text = ""
    for number in range(nblines):
        pointer = word(payload, OFFSET_POINTERS + 2 * number)
        index = (pointer - lowest) // LINE_SIZE + first_used
        start = OFFSET_BUFFER + index * LINE_SIZE
        size = payload[OFFSET_SIZES + number]
        line = payload[start:start + size]
        for code in line:
            if code == CHAR_RET:
                text += "\n"
            elif ASCII_MIN <= code <= ASCII_MAX:
                text += chr(code)
            else:
                text += " "
    return text


def command_to_c64(args):
    """Add the PRG load address of the Commodore 64 to an Oric file."""
    payload = read_file(args.input)
    check_payload(payload)
    header = bytearray(PRG_HEADER_SIZE)
    put_word(header, 0, args.address)
    write_file(args.output, header + payload)


def command_to_oric(args):
    """Remove the PRG load address of a Commodore 64 file."""
    data = read_file(args.input)
    payload = data[PRG_HEADER_SIZE:]
    check_payload(payload)
    write_file(args.output, payload)


def command_from_text(args):
    """Build a .ted file out of a plain text file."""
    with open(args.input, "r", errors="replace") as handle:
        text = handle.read()

    # The line pointers are absolute addresses of the machine that will
    # load the file. On the Commodore 64 the two byte PRG header is not
    # part of the payload, so the base address is the same on both sides.
    payload = build_payload(layout_text(text, args.tab), args.address)

    if args.oric:
        write_file(args.output, payload)
    else:
        header = bytearray(PRG_HEADER_SIZE)
        put_word(header, 0, args.address)
        write_file(args.output, header + payload)


def command_to_text(args):
    """Extract the plain text out of a .ted file."""
    data = read_file(args.input)
    payload = data if args.oric else data[PRG_HEADER_SIZE:]
    with open(args.output, "w") as handle:
        handle.write(extract_text(payload))


def command_info(args):
    """Print what a .ted file holds."""
    data = read_file(args.input)
    payload = data if args.oric else data[PRG_HEADER_SIZE:]
    nblines = check_payload(payload)
    magic = (payload[OFFSET_MAGIC] | (payload[OFFSET_MAGIC + 1] << 8) |
             (payload[OFFSET_MAGIC + 2] << 16) | (payload[OFFSET_MAGIC + 3] << 24))
    print("payload size : %d bytes" % len(payload))
    print("lines        : %d" % nblines)
    print("first pointer: $%04X" % word(payload, OFFSET_POINTERS))
    print("magic number : $%08X (%s)"
          % (magic, "plain text" if magic == MAGIC else "encrypted"))


def main():
    """Parse the command line and run the requested command."""
    parser = argparse.ArgumentParser(description="TED file handling")
    parser.add_argument("--oric", action="store_true",
                        help="the file has no PRG load address")
    subparsers = parser.add_subparsers(dest="command", required=True)

    sub = subparsers.add_parser("to-c64", help="Oric file to Commodore 64 PRG")
    sub.add_argument("input")
    sub.add_argument("output")
    sub.add_argument("--address", type=lambda v: int(v, 0), required=True,
                     help="address of textstore in the ted64 binary")
    sub.set_defaults(function=command_to_c64)

    sub = subparsers.add_parser("to-oric", help="Commodore 64 PRG to Oric file")
    sub.add_argument("input")
    sub.add_argument("output")
    sub.set_defaults(function=command_to_oric)

    sub = subparsers.add_parser("from-text", help="plain text to .ted file")
    sub.add_argument("input")
    sub.add_argument("output")
    sub.add_argument("--address", type=lambda v: int(v, 0), required=True,
                     help="address of textstore on the target machine")
    sub.add_argument("--tab", type=int, default=DEFAULT_TAB_SIZE,
                     help="width a tabulation is expanded to")
    sub.set_defaults(function=command_from_text)

    sub = subparsers.add_parser("to-text", help=".ted file to plain text")
    sub.add_argument("input")
    sub.add_argument("output")
    sub.set_defaults(function=command_to_text)

    sub = subparsers.add_parser("info", help="describe a .ted file")
    sub.add_argument("input")
    sub.set_defaults(function=command_info)

    args = parser.parse_args()
    args.function(args)


if __name__ == "__main__":
    main()
