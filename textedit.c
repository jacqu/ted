/* ================================================================== *
 * textedit: higher-level API for managing text edition               *
 * ================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <atmos.h>
#include <unistd.h>
#include <conio.h>
#include <stdbool.h>
#include <stdint.h>
#include "libscreen.h"
#include "liboric.h"
#include "chacha20.h"
#include "textstore.h"
#include "textedit.h"
#include "ed.h"

#define 		TEXTEDIT_UNUSED(x) (void)(x)

uint8_t			textedit_cur_x = 0;
uint8_t			textedit_cur_y = TEXTEDIT_EDITORSCR_BASE;
uint16_t		textedit_lpntr = TEXTEDIT_TEXT_BASE;
uint16_t		textedit_spntr = TEXTEDIT_TEXT_BASE;
char			textedit_status[LIBSCREEN_NB_COLS+1];
uint8_t			textedit_copy_buf[TEXTSTORE_LINE_SIZE];
uint8_t			textedit_copy_buf_sz = 0;
bool			textedit_saved_flag = true;
bool			textedit_inverted_flag = false;
bool			textedit_ret_flag = false;
char*			textedit_filename = NULL;
char*			textedit_password = NULL;

// Check array equality
bool textedit_equal( uint8_t *first_block, uint8_t *second_block, uint8_t block_sizes ) {
	register uint8_t 	i;
	bool 				result = true;

  	for ( i = 0; i < block_sizes; i++ ) {
		if( first_block[i] != second_block[i] ) {
			result = false;
			break;
		}
  	}
  	return result;
}

void textedit_init( char* filename, char* password ) {

	// Sanity check
	#ifdef ED_DEBUG
	if ( !filename ) {
		fprintf( stderr, "ERROR: NO FILENAME PROVIDED\n" );
		exit( TEXTEDIT_FATAL_ERROR );
	}
	#endif
	textedit_filename = filename;
	textedit_password = password;

	// Initialize text and line APIs
	textstore_init( );

	// Load file
	snprintf( liboric_cmd, LIBORIC_MAX_CMD_SIZE, "LOAD\"%s\",A%u,N", textedit_filename, &textstore );
	liboric_basic( liboric_cmd );
	switch( liboric_error_nd( ) ) {
		case SEDORIC_NO_ERROR:
		printf( "FILE LOADED SUCCESSFULLY\n" );
		sleep( TEXTEDIT_UI_WAIT_TIME );

		// Fix pointers in case of incompatible file versions
		textstore_fix_pointers( );

		// Decrypting
		if ( textedit_password ) {
			printf( "DECRYPTING..." );
			chacha_process( (uint8_t*)&textstore.magic, 
							textstore_sizeof( ) - ( (uint16_t)&textstore.magic - (uint16_t)&textstore),
							(uint8_t*)textedit_password, 
							textstore.nonce, 
							0 );
		}

		// Check file validity
		if ( textstore.magic != TEXTSTORE_MAGIC ) {
			fprintf( stderr, "INVALID PASSWORD\n" );
			exit( TEXTEDIT_FATAL_ERROR );
		}
		break;
		
		case SEDORIC_FILE_NOT_FOUND_ERROR:
		printf( "NEW FILE CREATED\n" );
		sleep( TEXTEDIT_UI_WAIT_TIME );

		// Allocate first line of text
		if ( textstore_insert_line( TEXTEDIT_TEXT_BASE ) ) {
			fprintf( stderr, "ERROR: UNABLE TO ALLOCATE FIRST LINE\n" );
			exit( TEXTEDIT_FATAL_ERROR );
		}
		break;

		default:
		fprintf( stderr, "ERROR: LOADING FILE\n" );
		exit( TEXTEDIT_FATAL_ERROR );
	}

	// Clear screen
	libscreen_clear( TEXTSTORE_CHAR_SPACE );

	// Reset line position
	textedit_lpntr = TEXTEDIT_TEXT_BASE;

	// Reset screen scrolling offset
	textedit_spntr = TEXTEDIT_TEXT_BASE;

	// Initialize status line
	libscreen_clearline( TEXTEDIT_STATUSSCR_BASE, TEXTSTORE_CHAR_SPACE );
	memset( textedit_status, TEXTSTORE_CHAR_SPACE, LIBSCREEN_NB_COLS );

	// Reset the position of the cursor at the top-left corner of the editor window
	textedit_cur_x = 0;
	textedit_cur_y = TEXTEDIT_EDITORSCR_BASE;

	// Screen refresh
	textedit_screen_refresh( );
	textedit_status_refresh( 0 );
	textedit_cursor_refresh( );
}

// Print a message on the status line
void textedit_status_print( char *msg ) {

	// Display message
	memset( textedit_status, TEXTSTORE_CHAR_SPACE, LIBSCREEN_NB_COLS );
	snprintf( textedit_status, LIBSCREEN_NB_COLS, "%s", msg );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );
}

// Print a message on the status line and wait some time
void textedit_status_popup( char *msg ) {

	// Display message
	memset( textedit_status, TEXTSTORE_CHAR_SPACE, LIBSCREEN_NB_COLS );
	snprintf( textedit_status, LIBSCREEN_NB_COLS, "%s", msg );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );

	// Wait some time
	sleep( TEXTEDIT_UI_WAIT_TIME );
}

// Ask a question on the status line
uint8_t	textedit_status_YN( char *msg ) {

	// Print question
	memset( textedit_status, TEXTSTORE_CHAR_SPACE, LIBSCREEN_NB_COLS );
	snprintf( textedit_status, LIBSCREEN_NB_COLS, "%s (%c/%c/%c)", msg, TEXTEDIT_UI_YES_ANSWER, TEXTEDIT_UI_NO_ANSWER, TEXTEDIT_UI_CA_ANSWER );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );

	// Scan response
	while ( 1 ) {
		switch ( cgetc( ) ) {
			case TEXTEDIT_UI_YES_ANSWER:
			return true;

			case TEXTEDIT_UI_NO_ANSWER:
			return false;

			case TEXTEDIT_UI_CA_ANSWER:
			return TEXTEDIT_CANCEL;

			default:
			atmos_ping( );
		}
	}
}

void textedit_mem_full( void ) {
	
	// Produce a "ping" sound
	atmos_ping();

	// Display a temporary message in status line
	textedit_status_popup( "MEMORY FULL!" );
}

void textedit_update_nonce( void ) {
	uint8_t *via1 = (uint8_t*)0x304;
	uint8_t *via2 = (uint8_t*)0x308;
	uint8_t *tim = (uint8_t*)0x276;

	textstore.nonce[0] = via1[0];
	textstore.nonce[1] = via1[1];
	textstore.nonce[2] = via2[0];
	textstore.nonce[3] = via2[1];
	textstore.nonce[4] = tim[0];
	textstore.nonce[5] = tim[1];
}

// Event handler
void textedit_event( uint8_t c ) {
	register uint8_t 	i;

	switch ( c ) {
		// Toggle the inverted flag
		case TEXTEDIT_CTRL_O:
		textedit_inverted_flag = !textedit_inverted_flag;
		break;

		// Display help
		case TEXTEDIT_CTRL_G:
		libscreen_clear( LIBSCREEN_SPACE );
		libscreen_copyline_inv( 
							0, 	(uint8_t*)"          U S E R    G U I D E          " );

		libscreen_copyline( 4, 	(uint8_t*)"[CTRL]-S: SAVE      [CTRL]-C: COPY  LINE" );
		libscreen_copyline( 6, 	(uint8_t*)"[CTRL]-X: CUT LINE  [CTRL]-V: PASTE LINE" );

		libscreen_copyline( 10, (uint8_t*)"[CTRL]-Q: WHITE INK [CTRL]-W: RED   INK " );
		libscreen_copyline( 12, (uint8_t*)"[CTRL]-E: GREEN INK [CTRL]-R: BLUE  INK " );
		libscreen_copyline( 14, (uint8_t*)"[CTRL]-T: BLACK INK [CTRL]-Y: BLACK PAP " );
		libscreen_copyline( 16, (uint8_t*)"[CTRL]-U: RED   PAP [CTRL]-A: YELL  PAP " );
		libscreen_copyline( 18, (uint8_t*)"[CTRL]-D: BLUE  PAP [CTRL]-O: INVERT    " );

		libscreen_copyline( 22, (uint8_t*)"[CTRL]-G: PAGE UP   [CTRL]-B: PAGE DOWN " );
		libscreen_copyline( 24, (uint8_t*)"[CTRL]-P: PRINT     [ESC]:    QUIT      " );

		libscreen_copyline_inv( 
							27, (uint8_t*)"VERSION 0.23           (c) SYNTAXIC 1985" );
		cgetc( );
		break;

		case TEXTEDIT_CTRL_S:
		// Encrypting
		if ( textedit_password ) {
			textedit_status_print( "ENCRYPTING.." );
			textedit_update_nonce( );
			chacha_process( (uint8_t*)&textstore.magic, 
							textstore_sizeof( ) - ( (uint16_t)&textstore.magic - (uint16_t)&textstore ),
							(uint8_t*)textedit_password, 
							textstore.nonce, 
							0 );
		}
		// Display saving message
		textedit_status_print( "SAVING.." );
		// Save textstore
		snprintf( 	liboric_cmd, 
					LIBORIC_MAX_CMD_SIZE, 
					"SAVEU\"%s\",A%u,E%u", 
					textedit_filename, 
					(uint16_t)&textstore,
					(uint16_t)&textstore + textstore_sizeof( ) );
		liboric_basic( liboric_cmd );
		// Error handling
		switch( liboric_error_nd( ) ) {
			case SEDORIC_NO_ERROR:
			textedit_status_popup( "FILE SAVED SUCCESSFULLY" );
			// Update saved flag
			textedit_saved_flag = true;
			break;

			default:
			textedit_status_popup( liboric_error_msg( ) );
			break;
		}
		// Revert encrypting
		if ( textedit_password ) {
			textedit_status_print( "DECRYPTING.." );
			chacha_process( (uint8_t*)&textstore.magic, 
							textstore_sizeof( ) - ( (uint16_t)&textstore.magic - (uint16_t)&textstore ),
							(uint8_t*)textedit_password, 
							textstore.nonce, 
							0 );
		}
		break;

		case TEXTEDIT_CTRL_P:
		if ( textedit_status_YN( "SEND TEXT TO PRINTER?" ) == true ) {
			textedit_status_print( "PRINTING.. (PRESS ANY KEY TO ABORT)" );
			textstore_print( );
		}
		break;

		// Page up
		case TEXTEDIT_CTRL_F:
		// Check if text is shorter than the screen
		if ( textstore.nblines < TEXTEDIT_EDITORSCR_SZ ) {
			// Nothing to scroll
			break;
		}
		else {
			// Decrement scroll pointer by one page
			if ( textedit_spntr >= TEXTEDIT_EDITORSCR_SZ - 1 ) {
				textedit_spntr -= TEXTEDIT_EDITORSCR_SZ - 1;
			}
			else {
				// Saturate scroll pointer
				textedit_spntr = 0;
			}
		}
		// Set vertical cursor position at the first line
		textedit_cur_y = TEXTEDIT_EDITORSCR_BASE;
		textedit_lpntr = textedit_spntr;
		// Toggle the return flag
		textedit_ret_flag = false;
		// Update cursor horizontal position at the begin of the line
		textedit_cur_x = 0;
		break;

		// Page down
		case TEXTEDIT_CTRL_B:
		// Check if text is shorter than the screen
		if ( textstore.nblines < TEXTEDIT_EDITORSCR_SZ ) {
			// Nothing to scroll
			break;
		}
		else {
			// Increment scroll pointer by one page
			textedit_spntr += TEXTEDIT_EDITORSCR_SZ - 1;
			// Saturate scroll pointer if end of text is reached
			if ( ( textedit_spntr + TEXTEDIT_EDITORSCR_SZ ) > textstore.nblines ) {
				textedit_spntr = textstore.nblines - TEXTEDIT_EDITORSCR_SZ;
			}
		}
		// Set vertical cursor position at the last line
		textedit_cur_y = TEXTEDIT_EDITORSCR_LAST;
		textedit_lpntr = textedit_spntr + TEXTEDIT_EDITORSCR_SZ - 1;
		// Toggle the return flag
		textedit_ret_flag = false;
		// Update cursor horizontal position at the end of the line
		textedit_cur_x = textstore.lsize[textedit_lpntr];
		break;

		case TEXTEDIT_KEY_ESC:
		if ( !textedit_saved_flag ) {
			i = textedit_status_YN( "QUIT WITHOUT SAVING?" );
			if (  i == true ) {
				liboric_basic( "RESET" );
				exit ( TEXTEDIT_NO_ERROR );
			}
			else {
				if ( i == TEXTEDIT_CANCEL )
					break;
				textedit_event( TEXTEDIT_CTRL_S );
				if ( textedit_saved_flag ) {
					liboric_basic( "RESET" );
					exit ( TEXTEDIT_NO_ERROR );
				}
			}
		}
		else {
			liboric_basic( "RESET" );
			exit ( TEXTEDIT_NO_ERROR );
		}
		break;

		case TEXTEDIT_CTRL_C:
		// Copy current line to copy buffer
		memcpy( textedit_copy_buf, textstore.tlpt[textedit_lpntr], TEXTSTORE_LINE_SIZE );
		textedit_copy_buf_sz = textstore.lsize[textedit_lpntr];
		break;

		case TEXTEDIT_CTRL_X:
		// Cut current line to copy buffer
		memcpy( textedit_copy_buf, textstore.tlpt[textedit_lpntr], TEXTSTORE_LINE_SIZE );
		textedit_copy_buf_sz = textstore.lsize[textedit_lpntr];
		// If not on the last line, make the cut
		if ( textedit_lpntr < textstore.nblines - 1 ) {
			// Delete current line
			textstore_del_line( textedit_lpntr );
			// Update x cursor
			textedit_cur_x = textstore.lsize[textedit_lpntr];
			// Update saved flag
			textedit_saved_flag = false;
			// Toggle the return flag
			textedit_ret_flag = false;
		}
		// If on the last line, blank line
		else {
			if ( textstore.lsize[textedit_lpntr] ) {
				// Delete cars in line
				textstore_del_chars( 	textedit_lpntr, 
										0, 
										textstore.lsize[textedit_lpntr] );
				// Update x cursor
				textedit_cur_x = 0;
				// Update saved flag
				textedit_saved_flag = false;
				// Toggle the return flag
				textedit_ret_flag = false;
			}
		}
		break;

		case TEXTEDIT_CTRL_V:
		// Something in copy buffer ?
		if ( !textedit_copy_buf_sz ) {
			break;
		}
		// Line available ?
		if ( textstore.nblines >= TEXTSTORE_LINES_MAX ) {
			textedit_mem_full( );
			break;
		}
		// Insert a new line
		if ( textstore_insert_line( textedit_lpntr ) ) {
			fprintf( stderr, "ERROR: PASTING NEW LINE\n" );
			exit( TEXTEDIT_FATAL_ERROR );
		}
		// Paste buffer to the new line
		textstore_write_chars( textedit_lpntr, 0, textedit_copy_buf, textedit_copy_buf_sz );
		// Update saved flag
		textedit_saved_flag = false;
		// Toggle the return flag
		textedit_ret_flag = false;
		// Increment line counter
		textedit_lpntr++;
		// Update cursor vertical position
		if ( textedit_cur_y < TEXTEDIT_EDITORSCR_LAST ) {
			textedit_cur_y++;
		}
		else {
			textedit_spntr++;
		}
		// Update x cursor
		textedit_cur_x = textstore.lsize[textedit_lpntr];
		break;

		case TEXTEDIT_ARROW_RIGHT:
		if ( 	( textedit_cur_x < textstore.lsize[textedit_lpntr] ) &&
				( textedit_cur_x != TEXTSTORE_LINE_SIZE - 1 ) ) {
			// Update x cursor
			textedit_cur_x++;
			// Toggle the return flag
			textedit_ret_flag = false;
		}
		else {
			if ( textedit_lpntr < textstore.nblines - 1 ) {
				textedit_event( TEXTEDIT_ARROW_DOWN );
				// Update x cursor
				textedit_cur_x = 0;
				// Toggle the return flag
				textedit_ret_flag = false;
			}
		}
		// Refresh current line
		libscreen_copyline( textedit_cur_y, textstore.tlpt[textedit_lpntr] );
		goto textedit_skip_screen_refresh;
		break;

		case TEXTEDIT_ARROW_LEFT:
		if ( textedit_cur_x > 0 ) {
			// Update x cursor
			textedit_cur_x--;
			// Toggle the return flag
			textedit_ret_flag = false;
		}
		else {
			if ( textedit_lpntr > TEXTEDIT_TEXT_BASE ) {
				textedit_event( TEXTEDIT_ARROW_UP );
				textedit_cur_x = textstore.lsize[textedit_lpntr];
				if ( textedit_cur_x > TEXTSTORE_LINE_SIZE - 1 ) {
					textedit_cur_x = TEXTSTORE_LINE_SIZE - 1;
				}
				// Toggle the return flag
				textedit_ret_flag = false;
			}
		}
		// Refresh current line
		libscreen_copyline( textedit_cur_y, textstore.tlpt[textedit_lpntr] );
		goto textedit_skip_screen_refresh;
		break;

		case TEXTEDIT_ARROW_UP:
		// First line ?
		if ( textedit_lpntr == TEXTEDIT_TEXT_BASE ) {
			break;
		}
		// Decrement line counter
		textedit_lpntr--;
		// Update cursor vertical position
		if ( textedit_cur_y > TEXTEDIT_EDITORSCR_BASE  ) {
			textedit_cur_y--;
			// Toggle the return flag
			textedit_ret_flag = false;
		}
		else  {
			textedit_spntr--;
		}
		// If needed, update cursor horizontal position
		if ( textedit_cur_x > textstore.lsize[textedit_lpntr] ) {
			textedit_cur_x = textstore.lsize[textedit_lpntr];
		}
		break;

		case TEXTEDIT_ARROW_DOWN:
		// Last line ?
		if ( textedit_lpntr >= textstore.nblines - 1 ) {
			break;
		}
		// Increment line counter
		textedit_lpntr++;
		// Update cursor vertical position
		if ( textedit_cur_y < TEXTEDIT_EDITORSCR_LAST ) {
			textedit_cur_y++;
			// Toggle the return flag
			textedit_ret_flag = false;
		}
		else {
			textedit_spntr++;
		}
		// If needed, update cursor horizontal position
		if ( textedit_cur_x > textstore.lsize[textedit_lpntr] ) {
			textedit_cur_x = textstore.lsize[textedit_lpntr];
		}
		break;

		case TEXTEDIT_KEY_DEL:
		// Cursor is not at beginning of the line
		if ( textedit_cur_x ) {
			textstore_del_char( textedit_lpntr, --textedit_cur_x );
		}
		// Cursor is at beginning of the line
		else {
			// First line ?
			if ( textedit_lpntr == TEXTEDIT_TEXT_BASE ) {
				atmos_ping( );
				textedit_status_popup( "FIRST LINE!" );
				break;
			}
			// Toggle the return flag
			textedit_ret_flag = false;
			// Adjust i to move only whole words
			if ( textstore.lsize[textedit_lpntr] <= TEXTSTORE_LINE_SIZE - textstore.lsize[textedit_lpntr-1] ) {
				// The whole line #n fits into line #n-1
				i = textstore.lsize[textedit_lpntr];
			}
			else {
				// Find the next word boundary that fits into line #n-1
				for ( i = textstore.lsize[textedit_lpntr] - 1; i > 0; i-- ) {
					if ( textstore.tlpt[textedit_lpntr][i] == LIBSCREEN_SPACE ) {
						if ( i + 1 <= TEXTSTORE_LINE_SIZE - textstore.lsize[textedit_lpntr-1] ) {
							i++;
							break;
						}
					}
				}
			}
			// Check if moving characters is needed
			if ( i ) {
				// Copy current line at the end of previous line
				textstore_insert_chars( textedit_lpntr - 1, 
										textstore.lsize[textedit_lpntr-1], 
										textstore.tlpt[textedit_lpntr], 
										i );
				// Delete the characters that moved to the previous line
				textstore_del_chars( 	textedit_lpntr, 
										0, 
										i );
			}
			// If current line empty, delete line
			if ( textstore.lsize[textedit_lpntr] == 0 ) {
				textstore_del_line( textedit_lpntr );
			}
			// Decrement line pointer;
			textedit_lpntr--;
			// Delete last character from the previous line if it is full
			if ( ( textstore.lsize[textedit_lpntr] == TEXTSTORE_LINE_SIZE ) && ( !i ) ) {
				textstore_del_char( textedit_lpntr, textstore.lsize[textedit_lpntr] - 1 );
			}
			// Update saved flag
			textedit_saved_flag = false;
			// Update x cursor
			textedit_cur_x = textstore.lsize[textedit_lpntr] - i;
			// Update y cursor position
			if ( textedit_cur_y > TEXTEDIT_EDITORSCR_BASE  ) {
				textedit_cur_y--;
			}
			else  {
				textedit_spntr--;
			}
		}	
		break;

		case TEXTEDIT_KEY_RET:
		// Check if RET should be muted
		if ( 	( textedit_ret_flag ) && 
				( textedit_cur_x == 0 ) /* && 
				( textstore.lsize[textedit_lpntr] == 0 )*/ ) {
			textedit_ret_flag = false;
			goto textedit_skip_event;
		}
		// Line available ?
		if ( textstore.nblines >= TEXTSTORE_LINES_MAX ) {
			textedit_mem_full( );
			break;
		}
		// Insert a new line
		if ( textstore_insert_line( ++textedit_lpntr ) ) {
			fprintf( stderr, "ERROR: INSERTING NEW LINE AFTER RET\n" );
			exit( TEXTEDIT_FATAL_ERROR );
		}
		// Cursor before the end of the line ?
		if ( textedit_cur_x < textstore.lsize[textedit_lpntr-1] ) {
			// Copy remaining part of the line after the cursor in the new line
			textstore_insert_chars( textedit_lpntr, 
									0, 
									&textstore.tlpt[textedit_lpntr-1][textedit_cur_x], 
									textstore.lsize[textedit_lpntr-1] - textedit_cur_x );
			// Blank the remaining of the previous line after the cursor if required
			textstore_del_chars( 	textedit_lpntr - 1, 
									textedit_cur_x, 
									textstore.lsize[textedit_lpntr-1] - textedit_cur_x );
		}
		// Update saved flag
		textedit_saved_flag = false;
		// Update cursor position
		if ( textedit_cur_y < TEXTEDIT_EDITORSCR_LAST ) {
			textedit_cur_y++;
		}
		else {
			textedit_spntr++;
		}
		// Reset cursor position and char counter
		textedit_cur_x = 0;
		break;

		default:
		// Convert pound char into plain char
		if ( c == TEXTEDIT_KEY_POUND ) {
			c = LIBSCREEN_PLAIN_CHAR;
		}
		// Check if value is within the ASCII printable range
		if ( ( c < TEXTEDIT_ASCII_MIN ) || ( c > TEXTEDIT_ASCII_MAX ) ) {
			// Check if value is a special code
			switch ( c ) {
				// Replace code to switch ink color
				case TEXTEDIT_CTRL_Q:
				c = LIBSCREEN_WHITE_INK;
				break;

				case TEXTEDIT_CTRL_W:
				c = LIBSCREEN_RED_INK;
				break;

				case TEXTEDIT_CTRL_E:
				c = LIBSCREEN_GREEN_INK;
				break;

				case TEXTEDIT_CTRL_R:
				c = LIBSCREEN_BLUE_INK;
				break;

				case TEXTEDIT_CTRL_T:
				c = LIBSCREEN_BLACK_INK;
				break;

				// Replace code to switch ink color
				case TEXTEDIT_CTRL_Y:
				c = LIBSCREEN_BLACK_PAPER;
				break;

				case TEXTEDIT_CTRL_U:
				c = LIBSCREEN_RED_PAPER;
				break;

				case TEXTEDIT_CTRL_A:
				c = LIBSCREEN_YELLOW_PAPER;
				break;

				case TEXTEDIT_CTRL_D:
				c = LIBSCREEN_BLUE_PAPER;
				break;

				case LIBSCREEN_PLAIN_CHAR:
				break;

				// Char is non printable and has not been identified as a valide special code
				default:
				goto textedit_skip_event;
			}
		}

		// Check if word wrapping is needed
		if ( 	( textedit_ret_flag ) && 
				( textedit_cur_x == 0 ) /*&& 
				( textstore.lsize[textedit_lpntr] == 0 )*/ ) {
			// Toggle RET flag
			textedit_ret_flag = false;
			// Sanity check
			#ifdef ED_DEBUG
			if ( ( textedit_lpntr == 0 ) || ( textstore.lsize[textedit_lpntr-1] != TEXTSTORE_LINE_SIZE ) ) {
				fprintf( stderr, "ERROR: WHILE WARPING\n" );
				exit( TEXTEDIT_FATAL_ERROR );
			}
			#endif
			// Scan backward for a space character in the previous line
			for ( i = TEXTSTORE_LINE_SIZE - 1; i > 0; i-- ) {
				if ( textstore.tlpt[textedit_lpntr-1][i] == LIBSCREEN_SPACE ) {
					break;
				}
			}
			i++;
			if ( ( i < TEXTSTORE_LINE_SIZE ) && ( i > 1 ) ) {
				// Copy last word of last line to this line
				textstore_insert_chars( textedit_lpntr, 
										0, 
										&textstore.tlpt[textedit_lpntr-1][i], 
										TEXTSTORE_LINE_SIZE - i );
				// Blank the remaining of the previous line after the cutting point i
				textstore_del_chars( 	textedit_lpntr-1, 
										i, 
										TEXTSTORE_LINE_SIZE - i );
				// Update saved flag
				textedit_saved_flag = false;
				// Update horizontal cursor position
				textedit_cur_x = TEXTSTORE_LINE_SIZE - i;
				// Recall event handler with the desired character
				textedit_event( c );
				break;
			}
		}

		// Invert character if needed
		if ( textedit_inverted_flag ) {
			c |= LIBSCREEN_INVERT_BIT;
		}

		//
		// At least one character free in the line
		//
		if ( textstore.lsize[textedit_lpntr] < TEXTSTORE_LINE_SIZE ) {
			// Update saved flag
			textedit_saved_flag = false;
			// Insert character
			textstore_insert_char( textedit_lpntr, textedit_cur_x, c );
			// Refresh current line
			libscreen_copyline( textedit_cur_y, textstore.tlpt[textedit_lpntr] );
			// Update cursor position
			textedit_cur_x++;
			// Check if cursor reached end of line
			if ( textedit_cur_x >= TEXTSTORE_LINE_SIZE ) {
				// Line available ?
				if ( textstore.nblines >= TEXTSTORE_LINES_MAX ) {
					textedit_mem_full( );
					textedit_cur_x--;
					break;
				}
				// Insert a new line after the current line
				if ( textstore_insert_line( textedit_lpntr + 1 ) ) {
					fprintf( stderr, "ERROR: INSERTING NEW LINE AFTER EOL\n" );
					exit( TEXTEDIT_FATAL_ERROR );
				}
				// Increment line pointer
				textedit_lpntr++;
				// Update vertical cursor position
				if ( textedit_cur_y < TEXTEDIT_EDITORSCR_LAST ) {
					textedit_cur_y++;
				}
				else {
					textedit_spntr++;
				}
				// Carriage return of the cursor
				textedit_cur_x = 0;
				// Toggle the return flag
				textedit_ret_flag = true;
				// Since cursor changes line, refresh whole screen
				break;
			}
			goto textedit_skip_screen_refresh;
		}
		//
		// No character left in the line
		//
		else {
			// Scan backward for a space character in the current line
			i = TEXTSTORE_LINE_SIZE - 1;
			while ( i > 0 ) {
				i--;
				if ( textstore.tlpt[textedit_lpntr][i] == LIBSCREEN_SPACE ) {
					i++;
					break;
				}
			}
			// If no space found, separator is the cursor
			if ( i == 0 ) {
				i = textedit_cur_x; 
			}
			// If not on the last line
			// AND
			// If number of characters to move fits in next line, 
			// Skip new line insertion
			if ( textedit_lpntr < textstore.nblines - 1 ) {
				if ( 	( textstore.lsize[textedit_lpntr] - i ) < 
						( TEXTSTORE_LINE_SIZE - textstore.lsize[textedit_lpntr+1] ) ) {
					goto skip_line_insertion;
				}	
			}
			// Line available ?
			if ( textstore.nblines >= TEXTSTORE_LINES_MAX ) {
				textedit_mem_full( );
				break;
			}
			// Insert a new line after the current line
			if ( textstore_insert_line( textedit_lpntr + 1 ) ) {
				fprintf( stderr, "ERROR: INSERTING NEW LINE AFTER EOL\n" );
				exit( TEXTEDIT_FATAL_ERROR );
			}

			skip_line_insertion:

			// Update saved flag
			textedit_saved_flag = false;
			// Copy remaining part of the line after the cutting point i in the new line
			textstore_insert_chars( textedit_lpntr + 1, 
									0, 
									&textstore.tlpt[textedit_lpntr][i], 
									textstore.lsize[textedit_lpntr] - i );
			// Blank the remaining of the previous line after the cutting point i
			textstore_del_chars( 	textedit_lpntr, 
									i, 
									textstore.lsize[textedit_lpntr] - i );
			// Update cursor position
			if ( textedit_cur_x > textstore.lsize[textedit_lpntr] ) {
				// Cursor should be moved to next line
				textedit_cur_x = textedit_cur_x - textstore.lsize[textedit_lpntr];
				// Increment line pointer
				textedit_lpntr++;
				// Update vertical cursor position
				if ( textedit_cur_y < TEXTEDIT_EDITORSCR_LAST ) {
					textedit_cur_y++;
				}
				else {
					textedit_spntr++;
				}
			}
			// Insert character at the cursor position
			textstore_insert_char( textedit_lpntr, textedit_cur_x, c );
			// Increment cursor position
			textedit_cur_x++;
			// Check if cursor reached end of line
			if ( textedit_cur_x >= TEXTSTORE_LINE_SIZE ) {
				// Increment line pointer
				textedit_lpntr++;
				// Update vertical cursor position
				if ( textedit_cur_y < TEXTEDIT_EDITORSCR_LAST ) {
					textedit_cur_y++;
				}
				else {
					textedit_spntr++;
				}
				// Toggle the return flag
				textedit_ret_flag = true;
				// Carriage return of the cursor
				textedit_cur_x = 0;
			}
		}
		break;
	}

	// Refresh text portion of the screen
	textedit_screen_refresh( );

	textedit_skip_screen_refresh:

	// Refresh status line
	textedit_status_refresh( c );

	// Refresh cursor
	textedit_cursor_refresh( );

	textedit_skip_event:
	return;
}

// Text screen refresh
void textedit_screen_refresh( void ) {
	register uint8_t i;

	// Refresh text portion of the screen
	if ( textstore.nblines - textedit_spntr >= TEXTEDIT_EDITORSCR_SZ ) {
		libscreen_display( textedit_spntr, textstore.tlpt );
	}
	else {
		for ( i = 0; i < TEXTEDIT_EDITORSCR_SZ; i++ ) {
			if ( textedit_spntr + i < textstore.nblines ) {
				libscreen_copyline( TEXTEDIT_EDITORSCR_BASE + i, textstore.tlpt[textedit_spntr+i] );
			}
			else {
				libscreen_clearline( TEXTEDIT_EDITORSCR_BASE + i, TEXTSTORE_CHAR_SPACE );
			}

		}
	}
}

// Refresh status line
void textedit_status_refresh( uint8_t c ) {
	char saved, *state;
	static char inverse[] = "INVSE";
	static char normal[] = "NORML";

	TEXTEDIT_UNUSED( c );

	if ( textedit_filename == NULL ) {
		fprintf( stderr, "ERROR: EMPTY FILENAME\n" );
		exit( -1 );
	}

	if ( textedit_saved_flag ) {
		saved =' ';
	}
	else {
		saved ='*';
	}

	if ( textedit_inverted_flag ) {
		state = inverse;
	}
	else {
		state = normal;
	}

	snprintf( 	textedit_status, 
				LIBSCREEN_NB_COLS+1, 
				"%-13s%c [CTRL-G]:GUIDE %03d%% %s",  
				textedit_filename,
				saved,
				( textstore.nblines * 100 ) / TEXTSTORE_LINES_MAX,
				state );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );
}

// Refresh cursor
void textedit_cursor_refresh( void ) {
	libscreen_textbuf[textedit_cur_y*LIBSCREEN_NB_COLS+textedit_cur_x] ^= LIBSCREEN_INVERT_BIT;
}