C_SOURCES  = chacha20_c.c textstore.c textedit.c liboric_c.c libscreen.c ed.c
A_SOURCES  = liboric.s chacha20.s
PROGRAM    = ted
INIT       = 'PRINT CHR$$(20):TED'
START      = 1536
SYMBOLS    = sym

CC65_HOME  = /Users/jacques/Personnel/Retro/Oric/CC65/cc65
TOOLS_HOME = /Users/jacques/Personnel/Retro/Oric/osdk/osdk/main
HXC_HOME   = /Users/jacques/Personnel/Retro/Oric/Microdisc/HxCFloppyEmulator

CC         = $(CC65_HOME)/bin/cc65
CA         = $(CC65_HOME)/bin/ca65
LD         = $(CC65_HOME)/bin/ld65
CFLAGS     = -D__ATMOS__ --standard cc65 -DSTART_ADDRESS=$(START) -Oirs --codesize 500 -T -g -t atmos
CAFLAGS    = -g
LDFLAGS    = -C $(CC65_HOME)/cfg/atmosd.cfg -L$(CC65_HOME)/lib $(CC65_HOME)/lib/atmos.lib -D__START_ADDRESS__=$(START) -Ln $(SYMBOLS)
RM         = /bin/rm -f
HEADER     = $(TOOLS_HOME)/header/header
TAP2DSK    = $(TOOLS_HOME)/tap2dsk/tap2dsk
OLD2MFM    = $(TOOLS_HOME)/old2mfm/old2mfm
HXCFE      = $(HXC_HOME)/build/hxcfe
EMULATE    = /usr/bin/open -n /Applications/Clock\ Signal.app

########################################
.SUFFIXES:
.PHONY: all clean run
all: $(PROGRAM).hfe

%.i: %.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.i
	$(CA) $(CAFLAGS) $<

%.o: %.s
	$(CA) $(CAFLAGS) $<

$(PROGRAM): $(C_SOURCES:.c=.o) $(A_SOURCES:.s=.o)
	$(LD) -o $@ $^ $(LDFLAGS)

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