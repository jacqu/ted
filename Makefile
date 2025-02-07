# Change these variables according to your own setup
# You can also directly redefine CC65, CA65 and LD65
# before calling make
CC65_HOME  ?= /Users/jacques/Personnel/Retro/Oric/CC65/cc65
TOOLS_HOME ?= /Users/jacques/Personnel/Retro/Oric/osdk/osdk/main
HXC_HOME   ?= /Users/jacques/Personnel/Retro/Oric/Microdisc/HxCFloppyEmulator
EMULATE    ?= /usr/bin/open -n /Applications/Clock\ Signal.app
# End of user customizable section

C_SOURCES  = chacha20_c.c textstore.c textedit.c liboric_c.c libscreen.c ed.c
A_SOURCES  = liboric.s chacha20.s
PROGRAM    = ted
INIT       = 'CLS:PRINT CHR$$(20):TED'
START      = 1536
SYMBOLS    = sym

GIT_VER    = "$(shell date '+%y.%m.%d.%H')"

CC65       ?= $(CC65_HOME)/bin/cc65
CA65       ?= $(CC65_HOME)/bin/ca65
LD65       ?= $(CC65_HOME)/bin/ld65
CFLAGS     = -DTED_VERSION=\"$(GIT_VER)\" -D__ATMOS__ --standard cc65 -DSTART_ADDRESS=$(START) -Oirs --codesize 500 -T -g -t atmos
CAFLAGS    = -g
LDFLAGS    = -C ./atmos_ted.cfg -L$(CC65_HOME)/lib $(CC65_HOME)/lib/atmos.lib -D__START_ADDRESS__=$(START) -Ln $(SYMBOLS)
RM         = /bin/rm -f
HEADER     = $(TOOLS_HOME)/header/header
TAP2DSK    = $(TOOLS_HOME)/tap2dsk/tap2dsk
OLD2MFM    = $(TOOLS_HOME)/old2mfm/old2mfm
HXCFE      = $(HXC_HOME)/build/hxcfe

########################################
.SUFFIXES:
.PHONY: all clean run
all: $(PROGRAM).hfe

%.i: %.c
	$(CC65) $(CFLAGS) -o $@ $<

%.o: %.i
	$(CA65) $(CAFLAGS) $<

%.o: %.s
	$(CA65) $(CAFLAGS) $<

$(PROGRAM): $(C_SOURCES:.c=.o) $(A_SOURCES:.s=.o)
	$(LD65) -o $@ $^ $(LDFLAGS)

$(PROGRAM).tap: $(PROGRAM)
	$(HEADER) $(PROGRAM) $(PROGRAM).tap $(START)

$(PROGRAM).dsk: $(PROGRAM).tap
	$(TAP2DSK) -i$(INIT) -n$(PROGRAM) $(PROGRAM).tap $(PROGRAM).dsk
	$(OLD2MFM) $(PROGRAM).dsk

$(PROGRAM).hfe: $(PROGRAM).dsk
	$(HXCFE) -finput:$(PROGRAM).dsk -conv:HXC_HFE -foutput:$(PROGRAM).hfe

run: $(PROGRAM).hfe
	$(EMULATE) $(PROGRAM).hfe

clean:
	$(RM) $(C_SOURCES:.c=.i) $(C_SOURCES:.c=.o) $(A_SOURCES:.s=.o) $(PROGRAM) $(PROGRAM).tap $(PROGRAM).dsk $(PROGRAM).hfe $(SYMBOLS)

# Avoid removing .i files
.PRECIOUS: %.i