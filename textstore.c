/* ================================================================== *
 * textstore: API for storing lines of text                           *
 * ================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "chacha20.h"
#include "libscreen.h"
#include "liboric.h"
#include "textedit.h"
#include "textstore.h"
#include "ed.h"

// Main data structure
struct 	textstore_struct textstore;

// Initialize textstore subsystem
void textstore_init( void ) {

	// Mark each line pointer as free
	memset( textstore.ptflag, TEXTSTORE_LINEPT_FREE, TEXTSTORE_LINES_MAX * sizeof( uint8_t ) );
	
	// Initialize the char counters
	memset( textstore.lsize, 0, TEXTSTORE_LINES_MAX * sizeof( uint8_t ) );

	// Initialize line counter
	textstore.nblines = 0;

	// Initialize the magic number
	textstore.magic = TEXTSTORE_MAGIC;

	// Initializing nonce
	memset( textstore.nonce, 0, TEXTSTORE_NONCE_SZ * sizeof( uint8_t ) );
}

// Fix pointers in case of text files created with different versions
void textstore_fix_pointers( void ) {
	uint8_t		*address = (uint8_t*)0xffff;
	int32_t		offset;
	uint16_t	i;

	// Find the lower pointer in the pointer list
	for ( i = 0; i < textstore.nblines; i++ ) {
		if ( textstore.tlpt[i] < address ) {
			address = textstore.tlpt[i];
		}
	}

	// Find the lower index in the pointer flag array
	for ( i = 0; i < textstore.nblines; i++ ) {
		if ( textstore.ptflag[i] == TEXTSTORE_LINEPT_USED ) {
			break;
		}
	}

	// Calculate the offset
	offset = (int32_t)&textstore.buf[i*TEXTSTORE_LINE_SIZE] - (int32_t)address;

	// Fix the offset of the pointers list
	for ( i = 0; i < textstore.nblines; i++ ) {
		textstore.tlpt[i] += offset;
	}
}

// Insert new line
// line_nb: line number where the insertion occurs (starts from 0)
// Returns 0 when insertion is successfull
// Returns TEXTSTORE_EMEM when memory exhausted
// Duration: 25ms
uint8_t textstore_insert_line( uint16_t line_nb ) {
	uint16_t i;

	// Sanity check
	#ifdef ED_DEBUG
	if ( line_nb > textstore.nblines ) {
		ed_fatal_error( "INSERTION BEYOND END OF TEXT" );
	}
	#endif

	// Look for a free line in main buffer
	for ( i = 0; i < TEXTSTORE_LINES_MAX; i++ ) {
		if ( textstore.ptflag[i] == TEXTSTORE_LINEPT_FREE )
			break;
	}
	if ( i == TEXTSTORE_LINES_MAX )
		return TEXTSTORE_EMEM;

	// Shift one index increment upwards in array
	if ( line_nb < textstore.nblines ) {
		memmove( &textstore.tlpt[line_nb+1], &textstore.tlpt[line_nb], sizeof(uint8_t*) * ( textstore.nblines - line_nb ) );
		memmove( &textstore.lsize[line_nb+1], &textstore.lsize[line_nb], sizeof(uint8_t) * ( textstore.nblines - line_nb ) );
	}

	// Write pointer of the new line in the pointer array
	textstore.tlpt[line_nb] = &textstore.buf[i*TEXTSTORE_LINE_SIZE];
	textstore.ptflag[i] = TEXTSTORE_LINEPT_USED;

	// Blank line
	memset( textstore.tlpt[line_nb], TEXTSTORE_CHAR_SPACE, TEXTSTORE_LINE_SIZE );

	// Increment line counter
	textstore.nblines++;

	// Initialize char counter
	textstore.lsize[line_nb] = 0;

	return TEXTSTORE_ENO;
}

// Delete existing line
// Duration: 25ms
void textstore_del_line( uint16_t line_nb ) {
	uint16_t i;

	// Sanity check
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		ed_fatal_error( "DELETION BEYOND END OF TEXT" );
	}
	#endif

	// Calculate the index of line pointer that must be discarded
	i = (uint16_t)( textstore.tlpt[line_nb] - textstore.buf ) / TEXTSTORE_LINE_SIZE;

	// Sanity check
	#ifdef ED_DEBUG
	if ( textstore.ptflag[i] == TEXTSTORE_LINEPT_FREE ) {
		ed_fatal_error( "REMOVING FREE POINTER" );
	}
	#endif

	// Shift one index increment downwards in array
	if ( line_nb < textstore.nblines - 1 ) {
		memmove( &textstore.tlpt[line_nb], &textstore.tlpt[line_nb+1], sizeof(uint8_t*) * ( textstore.nblines - line_nb - 1 ) );
		memmove( &textstore.lsize[line_nb], &textstore.lsize[line_nb+1], sizeof(uint8_t) * ( textstore.nblines - line_nb - 1 ) );
	}
	
	// Mark deleted line pointer as free
	textstore.ptflag[i] = TEXTSTORE_LINEPT_FREE;

	// Decrement line counter
	textstore.nblines--;

}

// Insert char
void textstore_insert_char( uint16_t line_nb, uint8_t char_nb, uint8_t char_c ) {

	// Sanity checks
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		ed_fatal_error( "INSERT CHAR BEYOND END OF TEXT" );
	}
	if ( char_nb > textstore.lsize[line_nb] ) {
		ed_fatal_error( "INSERT CHAR BEYOND END OF LINE" );
	}
	#endif

	// Check if buffer is full
	if ( textstore.lsize[line_nb] == TEXTSTORE_LINE_SIZE )
		return;

	// If needed, right-shift the buffer to make room for the insertion 
	if ( char_nb < textstore.lsize[line_nb] )
		memmove( &textstore.tlpt[line_nb][char_nb+1], &textstore.tlpt[line_nb][char_nb], sizeof(uint8_t) * ( textstore.lsize[line_nb] - char_nb ) );

	// Increment char counter
	textstore.lsize[line_nb]++;

	// Write desired char in the newly inserted position
	textstore.tlpt[line_nb][char_nb] = char_c;
}

// Delete existing char
// Duration: 6ms
void textstore_del_char( uint16_t line_nb, uint8_t char_nb ) {

	// Sanity checks
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		ed_fatal_error( "DEL CHAR BEYOND END OF TEXT" );
	}
	if ( char_nb >= textstore.lsize[line_nb] ) {
		ed_fatal_error( "DEL CHAR BEYOND END OF LINE" );
	}
	#endif

	if ( textstore.lsize[line_nb] ) {

		// Left-shift char buffer if required
		if ( char_nb < textstore.lsize[line_nb] - 1 ) {
			memmove( &textstore.tlpt[line_nb][char_nb], &textstore.tlpt[line_nb][char_nb+1], sizeof(uint8_t) * ( textstore.lsize[line_nb] - char_nb - 1 ) );
		}

		// Decrement char counter
		textstore.lsize[line_nb]--;

		// Clear deleted character
		textstore.tlpt[line_nb][textstore.lsize[line_nb]] = TEXTSTORE_CHAR_SPACE;
	}
}

void textstore_insert_chars( uint16_t line_nb, uint8_t char_nb, uint8_t *chars, uint8_t chars_nb ) {

	// Sanity checks
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		ed_fatal_error( "INSERT CHARS BEYOND END OF TEXT" );
	}
	if ( char_nb > textstore.lsize[line_nb] ) {
		ed_fatal_error( "INSERT CHARS BEYOND END OF LINE" );
	}
	#endif

	// Check if buffer is full
	if ( textstore.lsize[line_nb] == TEXTSTORE_LINE_SIZE )
		return;

	// Limit the number of chars to insert to the max
	if (  chars_nb > TEXTSTORE_LINE_SIZE - textstore.lsize[line_nb] ) {
		chars_nb = TEXTSTORE_LINE_SIZE - textstore.lsize[line_nb];
	}

	// If needed, right-shift the buffer to make room for the insertion 
	if ( char_nb < textstore.lsize[line_nb] )
		memmove( &textstore.tlpt[line_nb][char_nb+chars_nb], &textstore.tlpt[line_nb][char_nb], sizeof(uint8_t) * ( textstore.lsize[line_nb] - char_nb ) );

	// Increment char counter
	textstore.lsize[line_nb] += chars_nb;

	// Write desired chars in the newly inserted position
	memcpy( &textstore.tlpt[line_nb][char_nb], chars, chars_nb );
}

// Delete existing chars
// Duration: 6ms
void textstore_del_chars( uint16_t line_nb, uint8_t char_nb, uint8_t chars_nb ) {

	// Sanity checks
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		ed_fatal_error( "DEL CHARS BEYOND END OF TEXT" );
	}
	if ( char_nb >= textstore.lsize[line_nb] ) {
		ed_fatal_error( "DEL CHARS BEYOND END OF LINE" );
	}
	#endif

	if ( chars_nb > textstore.lsize[line_nb] - char_nb ) {
		chars_nb = textstore.lsize[line_nb] - char_nb;
	}

	// Left-shift char buffer if required
	if ( char_nb < textstore.lsize[line_nb] - 1 ) {
		memmove( &textstore.tlpt[line_nb][char_nb], &textstore.tlpt[line_nb][char_nb+chars_nb], sizeof(uint8_t) * ( textstore.lsize[line_nb] - char_nb - 1 ) );
	}

	// Decrement char counter
	textstore.lsize[line_nb] -= chars_nb;

	// Clear deleted characters
	memset( &textstore.tlpt[line_nb][textstore.lsize[line_nb]], TEXTSTORE_CHAR_SPACE, chars_nb );
}

// Write chars at a specific position without right-shifting the text
void textstore_write_chars( uint16_t line_nb, uint8_t char_nb, uint8_t *chars, uint8_t chars_nb ) {

	// Sanity checks
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		ed_fatal_error( "WRITE CHARS BEYOND END OF TEXT" );
	}
	if ( char_nb >= TEXTSTORE_LINE_SIZE ) {
		ed_fatal_error( "WRITE CHARS BEYOND END OF LINE" );
	}
	#endif

	// Limit the number of chars to insert to the max
	if (  chars_nb > TEXTSTORE_LINE_SIZE - char_nb ) {
		chars_nb = TEXTSTORE_LINE_SIZE - char_nb;
	}

	// Update char counter
	textstore.lsize[line_nb] = MAX( char_nb+chars_nb, textstore.lsize[line_nb] );

	// Write desired chars at the desired position
	memcpy( &textstore.tlpt[line_nb][char_nb], chars, chars_nb );
}

// Compute actual size of the structure
uint16_t textstore_sizeof( void ) {
	uint16_t i, j = 0;

	// Scan all line pointers and keep the max
	for ( i = 0; i < textstore.nblines; i++ ) {
		if ( (uint16_t)textstore.tlpt[i] > j ) {
			j = (uint16_t)textstore.tlpt[i];
		}
	}
	// Make j point to the end of the line
	j += TEXTSTORE_LINE_SIZE;

	// Compute data structure size
	return ( j - (uint16_t)&textstore );
}

// Change color on MCP-40
void textstore_color_mcp40( uint8_t type, uint8_t color ) {
	
	if ( type == TEXTSTORE_PRINTER_MCP40 ) {
		// This sleep is needed. Firmware bug ?
		sleep ( TEXTSTORE_LPRINT_WAIT );

		switch( color )	{
			case TEXTSTORE_MCP40_BLACK:
			liboric_basic( TEXTSTORE_LPRINT_BLACK );
			break;
			case TEXTSTORE_MCP40_BLUE:
			liboric_basic( TEXTSTORE_LPRINT_BLUE );
			break;
			case TEXTSTORE_MCP40_GREEN:
			liboric_basic( TEXTSTORE_LPRINT_GREEN );
			break;
			case TEXTSTORE_MCP40_RED:
			liboric_basic( TEXTSTORE_LPRINT_RED );
			break;
			default:
			break;
		}
	}
}

// Output text on printer
void textstore_print ( uint8_t type ) {
	uint16_t 	i;
	uint8_t		j, k, c;

	// Initialize color and CRLF
	liboric_basic( TEXTSTORE_LPRINT_LFCR );
	textstore_color_mcp40( type, TEXTSTORE_MCP40_BLACK );
	
	// Print text
	for ( i = 0; i < textstore.nblines; i++ ) {
		for ( j = 0; j < textstore.lsize[i]; j++ ) {
			// Process current character
			c = textstore.tlpt[i][j];
			// Switch color and replace color code with a space
			switch( c ) {
				case LIBSCREEN_WHITE_INK:
				textstore_color_mcp40( type, TEXTSTORE_MCP40_BLACK );
				c = LIBSCREEN_SPACE;
				break;
				case LIBSCREEN_RED_INK:
				textstore_color_mcp40( type, TEXTSTORE_MCP40_RED );
				c = LIBSCREEN_SPACE;
				break;
				case LIBSCREEN_GREEN_INK:
				textstore_color_mcp40( type, TEXTSTORE_MCP40_GREEN );
				c = LIBSCREEN_SPACE;
				break;
				case LIBSCREEN_BLUE_INK:
				textstore_color_mcp40( type, TEXTSTORE_MCP40_BLUE );
				c = LIBSCREEN_SPACE;
				break;
				case LIBSCREEN_BLACK_INK:
				case LIBSCREEN_BLACK_PAPER:
				case LIBSCREEN_RED_PAPER:
				case LIBSCREEN_YELLOW_PAPER:
				case LIBSCREEN_BLUE_PAPER:
				c = LIBSCREEN_SPACE;
				break;
				default:
				// Check for invert bit
				if ( c & LIBSCREEN_INVERT_BIT ) {
					c ^= LIBSCREEN_INVERT_BIT;
					if ( 	( c >= TEXTEDIT_ASCII_MIN ) && 
							( c <= TEXTEDIT_ASCII_MAX ) ) {
						// Print inverted char
						snprintf( liboric_cmd, LIBORIC_MAX_CMD_SIZE, TEXTSTORE_LPRINT, c );
						liboric_basic( liboric_cmd );
						// Reverse one step to print it again on the same spot
						liboric_basic( TEXTSTORE_LPRINT_BS );
					}
				}
				// Check for unknown char: replace with space
				if ( 	( c < TEXTEDIT_ASCII_MIN ) || 
						( c > TEXTEDIT_ASCII_MAX ) ) {
					c = LIBSCREEN_SPACE;
				}
				break; 
			}
			if ( 	( c >= TEXTEDIT_ASCII_MIN ) && 
					( c <= TEXTEDIT_ASCII_MAX ) ) {
				snprintf( liboric_cmd, LIBORIC_MAX_CMD_SIZE, TEXTSTORE_LPRINT, c );
				liboric_basic( liboric_cmd );
			}
			// Check for user abort
			for ( k = 0; k < TEXTSTORE_KBHIT_SLEEP; k++ ) {
				if ( kbhit( ) ) {
					textedit_status_popup( "PRINTING ABORTED!" );
					goto end_print;
				}
			}
		}
		// Send RET at each end of line and revert to default color
		liboric_basic( TEXTSTORE_LPRINT_LFCR );
		textstore_color_mcp40( type, TEXTSTORE_MCP40_BLACK );
	}
	end_print:
	// Send RET at end of text and revert to default color
	liboric_basic( TEXTSTORE_LPRINT_LFCR );
	textstore_color_mcp40( type, TEXTSTORE_MCP40_BLACK );
	liboric_basic( TEXTSTORE_LPRINT_LFCR );
	liboric_basic( TEXTSTORE_LPRINT_LFCR );
}