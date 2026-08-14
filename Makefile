# Change these variables according to your own setup
# You can also directly redefine CC65, CA65 and LD65
# before calling make
CC65_HOME  ?= /Users/jacques/Personnel/Retro/Oric/CC65/cc65
EMULATE    ?= /usr/bin/open -n /Applications/Clock\ Signal.app
# End of user customizable section

C_SOURCES  = chacha20_c.c blake2s_c.c libcanary.c strfmt.c textstore.c textedit.c liboric_c.c libscreen.c ed.c
A_SOURCES  = liboric.s libconsole.s chacha20.s blake2s.s
PROGRAM    = ted
PYTHON     ?= python3

DOC_SOURCE = README.txt
DOC_TARGET = readme.ted
TEXTSTORE  = _textstore
INIT       = 'CLS:PRINT CHR$$(20):GRAB:TED'
START      = 1536
SYMBOLS    = sym

MAJOR_VER  = 3.0
VERSION    = "$(MAJOR_VER).$(shell date '+%y%m%d%H')"

CC65       ?= $(CC65_HOME)/bin/cc65
CA65       ?= $(CC65_HOME)/bin/ca65
LD65       ?= $(CC65_HOME)/bin/ld65
# The compiler is allowed to trade code size for speed only where speed
# is felt. The editor and the screen are on the path walked at every
# keystroke; everything else runs once per file or once per session, and
# is compiled for size instead
CODESIZE       = 100								# Cold modules, compiled small
# The canary build carries a few hundred bytes more, which the Atmos does
# not have to spare, so it gives up some of the speed instead
CODESIZE_FAST  = $(if $(CANARY),100,500)			# Modules on the keystroke path

# make CANARY=1 builds a version that paints the memory the stacks live
# in and stops with the address as soon as anything else writes there
CANARY     ?=
CANARY_FLAG = $(if $(CANARY),-DTED_CANARY,)

CFLAGS     = $(CANARY_FLAG) -DTED_VERSION=\"$(VERSION)\" -D__ATMOS__ --standard cc65 -DSTART_ADDRESS=$(START) -Oirs $(STATIC_LOCALS) --codesize $(CODESIZE) -T -g -t atmos
CAFLAGS    = -g
LDFLAGS    = -C ./atmos_ted.cfg -L$(CC65_HOME)/lib $(CC65_HOME)/lib/atmos.lib -D__START_ADDRESS__=$(START) -Ln $(SYMBOLS)
RM         = /bin/rm -f
SED_IMPORT = /usr/local/bin/sedoric-import

########################################
.SUFFIXES:
.PHONY: all clean run
all: $(PROGRAM).dsk

doc: $(DOC_TARGET)

# Every module is rebuilt when any header changes: a stale object file
# after a header has moved a field is a bug that takes a long time to
# recognize
HEADERS    = $(wildcard *.h)

# Local variables are given a fixed place instead of a place on the stack,
# which is both smaller and faster on a processor whose stack is one page.
# It is only safe where nothing calls itself, so textedit is left out: its
# event handler asks itself to save the text before leaving
STATIC_LOCALS = -Cl

textedit.i:  STATIC_LOCALS =
textedit.i:  CODESIZE = $(CODESIZE_FAST)
libscreen.i: CODESIZE = $(CODESIZE_FAST)

%.i: %.c $(HEADERS)
	$(CC65) $(CFLAGS) -o $@ $<

%.o: %.i
	$(CA65) $(CAFLAGS) $<

%.o: %.s
	$(CA65) $(CAFLAGS) $<

$(PROGRAM): $(C_SOURCES:.c=.o) $(A_SOURCES:.s=.o)
	$(LD65) -o $@ $^ $(LDFLAGS)

$(DOC_TARGET): $(PROGRAM) $(DOC_SOURCE)
	$(PYTHON) tools/tedtool.py --oric from-text $(DOC_SOURCE) $(DOC_TARGET) \
		--address `$(PYTHON) tools/symaddr.py $(SYMBOLS) $(TEXTSTORE)`

$(PROGRAM).hfe: $(PROGRAM) $(DOC_TARGET)
	$(SED_IMPORT) -n -f 80d -L $(PROGRAM) -N $(PROGRAM).COM -I $(INIT) -T binary -A $(START) -E $(START) $(PROGRAM).hfe $(PROGRAM)
	$(SED_IMPORT) -N $(DOC_TARGET) -T binary -A `$(PYTHON) tools/symaddr.py $(SYMBOLS) $(TEXTSTORE)` -E $(START) $(PROGRAM).hfe $(DOC_TARGET)

$(PROGRAM).dsk:	$(PROGRAM).hfe
	hfe2dsk $(PROGRAM).hfe $(PROGRAM).dsk

run: $(PROGRAM).hfe
	$(EMULATE) $(PROGRAM).hfe

clean:
	$(RM) $(C_SOURCES:.c=.i) $(C_SOURCES:.c=.o) $(A_SOURCES:.s=.o) $(PROGRAM) $(PROGRAM).hfe $(DOC_TARGET) $(SYMBOLS)

# Avoid removing .i files
.PRECIOUS: %.i