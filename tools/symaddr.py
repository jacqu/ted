#!/usr/bin/env python3
# ==================================================================== #
# symaddr: read the address of a symbol out of a ld65 label file        #
#                                                                       #
# ld65 writes its label file with the -Ln option, using the VICE format #
# in which every line looks like:                                       #
#                                                                       #
#   al 00C000 ._textstore                                               #
#                                                                       #
# The address field is hexadecimal and may or may not carry a bank      #
# prefix depending on the version, so both forms are accepted.          #
#                                                                       #
# Usage: symaddr.py label_file symbol_name                              #
# ==================================================================== #

import re
import sys

# --- Configuration --------------------------------------------------- #
LABEL_PATTERN = r"^\s*al\s+(?:[0-9a-zA-Z]+:)?([0-9a-fA-F]+)\s+\.(\S+)\s*$"
EXIT_FAILURE = 1                    # Status returned when the symbol is missing


def main():
    """Print the address of the requested symbol as a C style constant."""
    if len(sys.argv) != 3:
        sys.exit("usage: symaddr.py label_file symbol_name")

    label_file, symbol = sys.argv[1], sys.argv[2]
    matcher = re.compile(LABEL_PATTERN)

    with open(label_file, "r") as handle:
        for line in handle:
            found = matcher.match(line)
            if found and found.group(2) == symbol:
                print("0x%04X" % (int(found.group(1), 16) & 0xFFFF))
                return

    sys.exit("error: symbol %s not found in %s" % (symbol, label_file))


if __name__ == "__main__":
    main()
