/* ================================================================== *
 * textstore: API for storing lines of text                           *
 * ================================================================== */

#ifndef __TEXTSTORE_H__
#define __TEXTSTORE_H__

#include "blake2s.h"

#define TEXTSTORE_LINES_MAX				350			// Maximum number of lines in the text
#define TEXTSTORE_LINE_SIZE				LIBSCREEN_NB_COLS			
													// Number of characters in a line
#define TEXTSTORE_NONCE_SZ				CHACHA_NONCE_SZ
													// Size of the nonce
#define TEXTSTORE_TAG_SZ				BLAKE2S_TAG_SZ
													// Size of the authentication tag

/* Version of the on disk format. A file carries it so that a version of *
 * TED which does not know how to read it says so instead of handing the *
 * user a text made of noise. Version 1 is the format that had neither a *
 * tag nor a version, and is not readable any more.                      */
#define TEXTSTORE_VERSION				2			// Format written by this version of TED

/* Everything in front of the text itself. A file shorter than that      *
 * cannot be one, whatever its header claims.                            */
#define TEXTSTORE_HEADER_SZ				( TEXTSTORE_TAG_SZ + TEXTSTORE_NONCE_SZ + 6 + \
										  4 * TEXTSTORE_LINES_MAX + 4 )

#define TEXTSTORE_MAGIC					0x94c910ff
													// Magic Number

#define TEXTSTORE_CHAR_SPACE			32			// Space character
#define TEXTSTORE_CHAR_RET				95			// CRLF character

#define TEXTSTORE_LINEPT_FREE			1			// Line pointer is available
#define TEXTSTORE_LINEPT_USED			0			// Line pointer is used

#define TEXTSTORE_ENO					0			// No error
#define TEXTSTORE_EMEM					1			// Out of memory error

#define TEXTSTORE_PRINTER_GENERIC		0			// Generic parallel printer
#define TEXTSTORE_PRINTER_MCP40			1			// Oric 4 colors plotter

#define TEXTSTORE_LPRINT				"OUT "
#define TEXTSTORE_LPRINT_DIGITS			3			// Digits of a character code
													// Printer instruction for one char
#define TEXTSTORE_LPRINT_LFCR			"OUT 10:OUT 13"
													// Printer LFCR
#define TEXTSTORE_LPRINT_BS				"OUT 8"
													// Printer BS
#define TEXTSTORE_MCP40_BLACK			0
#define TEXTSTORE_LPRINT_BLACK			"OUT 18:LPRINT\"C0\":OUT 17"
													// Printer black
#define TEXTSTORE_MCP40_BLUE			1													
#define TEXTSTORE_LPRINT_BLUE			"OUT 18:LPRINT\"C1\":OUT 17"
													// Printer blue
#define TEXTSTORE_MCP40_GREEN			2
#define TEXTSTORE_LPRINT_GREEN			"OUT 18:LPRINT\"C2\":OUT 17"
													// Printer green
#define TEXTSTORE_MCP40_RED				3
#define TEXTSTORE_LPRINT_RED			"OUT 18:LPRINT\"C3\":OUT 17"
													// Printer red
#define TEXTSTORE_LPRINT_WAIT			1			// Waiting period before changing color

#define TEXTSTORE_KBHIT_SLEEP			255			// Number of kbhit polling cycles

/* The tag comes first and the rest of the header follows it, so that    *
 * everything the tag authenticates is the one contiguous run of bytes    *
 * that begins right after it and ends where the file ends. The size of   *
 * the file is written in the header for the same reason: the reader has  *
 * to know how far the tag reaches before it can check anything.          */
struct textstore_struct {
	uint8_t		tag[TEXTSTORE_TAG_SZ];							// Tag of everything below
	uint8_t		nonce[TEXTSTORE_NONCE_SZ];							// Nonce
	uint16_t	version;											// Version of the format
	uint16_t	fsize;												// Bytes written to the file
	uint16_t	nblines;											// Total number of lines
	uint8_t*	tlpt[TEXTSTORE_LINES_MAX];							// Text line pointers array
	uint8_t		ptflag[TEXTSTORE_LINES_MAX];						// Flag for the line pointers
	uint8_t		lsize[TEXTSTORE_LINES_MAX];							// Number of characters in a line
	uint32_t	magic;												// Magic number
	uint8_t		buf[TEXTSTORE_LINES_MAX*TEXTSTORE_LINE_SIZE];		// Main text buffer
};

extern struct 	textstore_struct textstore;

void 		textstore_init					( void );
void 		textstore_fix_pointers			( void );
uint8_t 	textstore_insert_line			( uint16_t );
void 		textstore_del_line				( uint16_t );
void		textstore_insert_char			( uint16_t, uint8_t, uint8_t );
void 		textstore_del_char				( uint16_t, uint8_t );
void		textstore_insert_chars			( uint16_t, uint8_t, uint8_t*, uint8_t );
void 		textstore_del_chars				( uint16_t, uint8_t, uint8_t );
void 		textstore_write_chars			( uint16_t, uint8_t, uint8_t*, uint8_t );
void 		textstore_replace_line			( uint16_t, uint8_t*, uint8_t );
void 		textstore_clear_line			( uint16_t );
uint16_t	textstore_sizeof				( void );
void 		textstore_color_mcp40			( uint8_t, uint8_t );
void		textstore_print					( uint8_t );
int8_t 		textstore_reformat				( uint16_t );
int8_t 		textstore_move_first_words_up	( uint16_t );

#endif /* __TEXTSTORE_H__ */