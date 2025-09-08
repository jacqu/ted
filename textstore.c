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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S0" );
		#endif
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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S1" );
		#endif
	}
	#endif

	// Calculate the index of line pointer that must be discarded
	i = (uint16_t)( textstore.tlpt[line_nb] - textstore.buf ) / TEXTSTORE_LINE_SIZE;

	// Sanity check
	#ifdef ED_DEBUG
	if ( textstore.ptflag[i] == TEXTSTORE_LINEPT_FREE ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S2" );
		#endif
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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S3" );
		#endif
	}
	if ( char_nb > textstore.lsize[line_nb] ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S4" );
		#endif
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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S5" );
		#endif
	}
	if ( char_nb >= textstore.lsize[line_nb] ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S6" );
		#endif
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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S7" );
		#endif
	}
	if ( char_nb > textstore.lsize[line_nb] ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S8" );
		#endif
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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "S9" );
		#endif
	}
	if ( char_nb >= textstore.lsize[line_nb] ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "SA" );
		#endif
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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "SB" );
		#endif
	}
	if ( char_nb >= TEXTSTORE_LINE_SIZE ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "SC" );
		#endif
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

// Replace existing line
void textstore_replace_line( uint16_t line_nb, uint8_t *chars, uint8_t chars_nb ) {

	// Sanity checks
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "SD" );
		#endif
	}
	if ( chars_nb >= TEXTSTORE_LINE_SIZE ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "SE" );
		#endif
	}
	#endif

	// Clear line
	memset( textstore.tlpt[line_nb], TEXTSTORE_CHAR_SPACE, TEXTSTORE_LINE_SIZE );

	// Replace line content
	memcpy( textstore.tlpt[line_nb], chars, chars_nb );

	// Update char counter
	textstore.lsize[line_nb] = chars_nb;
}

// Clear existing line
void textstore_clear_line( uint16_t line_nb ) {

	// Sanity checks
	#ifdef ED_DEBUG
	if ( line_nb >= textstore.nblines ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "SF" );
		#endif
	}
	#endif

	// Update line size
	textstore.lsize[line_nb] = 0;

	// Clear line
	memset( textstore.tlpt[line_nb], TEXTSTORE_CHAR_SPACE, TEXTSTORE_LINE_SIZE );
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

// Reformat the text starting from line "line_nb"
// Iterates until CRLF character or blank line or EOF is encountered
// Returns the number of characters moved in the current line
int8_t textstore_reformat( uint16_t line_nb ) {
	int8_t ret, i = 0;
	uint16_t j = line_nb;

	// Nothing to move upwards
	if ( 	( j == 0 ) || 
			( j >= textstore.nblines ) ) {
		return 0;
	}

	// Iterate
	do {
		ret = textstore_move_first_words_up( j );

		// If on the current line, store the nb of chars that have been moved
		if ( j == line_nb ) {
			i = ret;
		}

		// If no characters moved, stop
		if ( !ret ) {
			break;
		}

		// If CRLF at the end of the current line, end
		if ( textstore.tlpt[j][textstore.lsize[j]-1] == TEXTSTORE_CHAR_RET )	{
			break;
		}

	} while ( 	( ++j < textstore.nblines ) && 
				( textstore.lsize[j] != 0 ) );

	return i;
}

// Move as many words as possible from the beginning of line n to the end of line n-1
// If line n becomes empty, delete line n
// Returns the number of chars that have been moved
int8_t textstore_move_first_words_up( uint16_t line_nb ) {
	int8_t i;
	
	// Sanity checks
	#ifdef ED_DEBUG
	if ( ( line_nb >= textstore.nblines ) || ( line_nb == 0 ) ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "SG" );
		#endif
	}
	#endif

	// If last character of the previous line is a CRLF, do nothing
	if ( textstore.tlpt[line_nb-1][textstore.lsize[line_nb-1]-1] == TEXTSTORE_CHAR_RET ) {
		return 0;
	}

	// If previous line is empty, delete line
	if ( textstore.lsize[line_nb-1] == 0 ) {
		textstore_del_line( line_nb - 1 );
		return 0;
	}

	// Find the next word boundary of line n starting from the right
	for ( i = textstore.lsize[line_nb] - 1; i >= 0; i-- ) {
		// Check at each boundary if it fits
		if ( 	( i == textstore.lsize[line_nb] - 1 ) ||
				( textstore.tlpt[line_nb][i] == TEXTSTORE_CHAR_SPACE ) ) {
			// Check if this group of words fit at the end of previous line
			if ( i < TEXTSTORE_LINE_SIZE - textstore.lsize[line_nb-1] ) {
				// It fits, end loop
				break;
			}
		}
	}
	// Increment the boundary position to express the size of the block to move
	i++;

	// Check if moving characters is needed
	if ( i ) {
		// Copy current line at the end of previous line
		textstore_insert_chars( line_nb - 1, 
								textstore.lsize[line_nb-1], 
								textstore.tlpt[line_nb], 
								i );
		// Delete the characters that moved to the previous line
		textstore_del_chars( 	line_nb, 
								0, 
								i );
		// If current line empty, delete line
		if ( textstore.lsize[line_nb] == 0 ) {
			textstore_del_line( line_nb );
		}
	}

	return i;
}