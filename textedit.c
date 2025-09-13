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

uint8_t			*textedit_retc_a = (uint8_t*)TEXTEDIT_RET_CHAR_ADDRESS;
uint8_t			textedit_retc_i[TEXTEDIT_ORIC_CHARS_HEIGHT];
uint8_t			textedit_cur_x = 0;
uint8_t			textedit_cur_y = TEXTEDIT_EDITORSCR_BASE;
uint16_t		textedit_lpntr = TEXTEDIT_TEXT_BASE;
uint16_t		textedit_spntr = TEXTEDIT_TEXT_BASE;
char			textedit_status[LIBSCREEN_NB_COLS+1];
uint8_t			textedit_copy_buf[TEXTSTORE_LINE_SIZE];
uint8_t			textedit_copy_buf_sz = 0;
bool			textedit_saved_flag = true;
bool			textedit_inverted_flag = false;
char*			textedit_filename = NULL;
char*			textedit_password = NULL;
uint32_t 		textedit_sc_counter;
bool			textedit_sc_enable = true;

// Backup old RET shape
void textedit_ret_bak( void ) {
	uint8_t i;

	// Store initial char shape before modifying it
	for( i = 0; i < TEXTEDIT_ORIC_CHARS_HEIGHT; i++ ) {
		textedit_retc_i[i] = textedit_retc_a[i];
	}
}

// Redefine RET char
void textedit_ret_redef( void ) {
	const uint8_t newret[TEXTEDIT_ORIC_CHARS_HEIGHT] = { 0, 2, 2, 2, 10, 30, 8, 0 };
	uint8_t i;

	// Store initial char shape before modifying it
	for( i = 0; i < TEXTEDIT_ORIC_CHARS_HEIGHT; i++ ) {
		textedit_retc_a[i] = newret[i];
	}
}

// Blank RET char
void textedit_ret_blank( void ) {

	memset( textedit_retc_a, 0, TEXTEDIT_ORIC_CHARS_HEIGHT );
}

// Restore RET as original
void textedit_ret_restore( void ) {
	uint8_t i;

	// Restore initial shape
	for( i = 0; i < TEXTEDIT_ORIC_CHARS_HEIGHT; i++ ) {
		textedit_retc_a[i] = textedit_retc_i[i];
	}
}

// Exit cleanly
void textedit_exit( void ) {

	// Clear text in memory
	memset( &textstore, 0, textstore_sizeof( ) );

	// Clear password if needed
	if ( textedit_password ) {
		memset( textedit_password, 0, ED_PW_MAX_LENGTH );
	}
	
	// Restore RET shape
	textedit_ret_restore( );

	// Clear screen including the status bar
	libscreen_clear( LIBSCREEN_SPACE );

	// Reset cursor position
	liboric_basic( "CLS" );

	// Exit without error
	exit ( ED_NO_ERROR );
}

// Screen saver
void textedit_screensaver( void ) {
	static bool 	init_flag = false;
	static bool		leader_flag[LIBSCREEN_NB_COLS];
	static uint8_t	col_on[LIBSCREEN_NB_COLS];
	static uint8_t	col_off[LIBSCREEN_NB_COLS];
	uint16_t		i;

	if ( !textedit_sc_counter ) {

		// If needed, initialize screensaver
		if ( init_flag ) {

			// Clear screen
			libscreen_clear( LIBSCREEN_SPACE );

			// Initialize stripes counters
			for ( i = 1; i < LIBSCREEN_NB_COLS; i++ ) {
				col_on[i] = (uint8_t)( rand( ) % LIBSCREEN_NB_LINES );
				col_off[i] = (uint8_t)( rand( ) % LIBSCREEN_NB_LINES );
				leader_flag[i] = false;
			}

			init_flag = false;
		}

		// Refresh first line
		libscreen_textbuf[0] = LIBSCREEN_GREEN_INK;
		for ( i = 1; i < LIBSCREEN_NB_COLS; i++ ) {
			if ( col_on[i] ) {
				col_on[i]--;
				libscreen_textbuf[i] = LIBSCREEN_SPACE;
				if ( col_on[i] == 0 ) {
					leader_flag[i] = true;
				}
			}
			else {
				if ( col_off[i] ) {
					col_off[i]--;
					// The magic occurs here
					libscreen_textbuf[i] = '!' + ( rand() % 94 );
					if ( leader_flag[i] ) {
						leader_flag[i] = false;
						libscreen_textbuf[i] = LIBSCREEN_PLAIN;
					}
				}
				else {
					col_on[i] = (uint8_t)( rand( ) % LIBSCREEN_NB_LINES );
					col_off[i] = (uint8_t)( rand( ) % LIBSCREEN_NB_LINES );
				}
			}
		}

		// Scroll screen
		libscreen_scroll_down(  );

	}
	else {
		textedit_sc_counter--;
		if ( !textedit_sc_counter ) {
			init_flag = true;
		}
	}

}

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
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "E0" );
		#endif
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
			fprintf( stderr, "BAD MAGIC NUMBER\n" );
			exit( ED_FATAL_ERROR );
		}
		break;
		
		case SEDORIC_FILE_NOT_FOUND_ERROR:
		printf( "NEW FILE CREATED\n" );
		sleep( TEXTEDIT_UI_WAIT_TIME );

		// Allocate first line of text
		if ( textstore_insert_line( TEXTEDIT_TEXT_BASE ) ) {
			#ifdef ED_VERBOSE
			ed_fatal_error( __FILE__, __LINE__ );
			#else
			printf( "E1" );
			#endif
		}
		break;

		default:
		fprintf( stderr, "SEDORIC ERROR\n" );
		exit( ED_FATAL_ERROR );
	}

	// Redefine RET char
	textedit_ret_bak( );
	textedit_ret_redef( );

	// Clear screen
	libscreen_clear( LIBSCREEN_SPACE );

	// Reset line position
	textedit_lpntr = TEXTEDIT_TEXT_BASE;

	// Reset screen scrolling offset
	textedit_spntr = TEXTEDIT_TEXT_BASE;

	// Initialize status line
	libscreen_clearline( TEXTEDIT_STATUSSCR_BASE, LIBSCREEN_SPACE );
	memset( textedit_status, LIBSCREEN_SPACE, LIBSCREEN_NB_COLS );

	// Reset the position of the cursor at the top-left corner of the editor window
	textedit_cur_x = 0;
	textedit_cur_y = TEXTEDIT_EDITORSCR_BASE;

	// Screen refresh
	textedit_screen_refresh( );
	textedit_status_refresh( );
	textedit_cursor_refresh( );
}

// Print a message on the status line
void textedit_status_print( char *msg ) {

	// Display message
	memset( textedit_status, LIBSCREEN_SPACE, LIBSCREEN_NB_COLS );
	snprintf( textedit_status, LIBSCREEN_NB_COLS, "%s", msg );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );
}

// Print a message on the status line and wait some time
void textedit_status_popup( char *msg ) {

	// Display message
	memset( textedit_status, LIBSCREEN_SPACE, LIBSCREEN_NB_COLS );
	snprintf( textedit_status, LIBSCREEN_NB_COLS, "%s", msg );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );

	// Wait some time
	sleep( TEXTEDIT_UI_WAIT_TIME );
}

// Ask a question on the status line
uint8_t	textedit_status_YN( char *msg ) {

	// Print question
	memset( textedit_status, LIBSCREEN_SPACE, LIBSCREEN_NB_COLS );
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

	// Display a temporary message in status line
	textedit_status_popup( "MEMORY FULL!" );
}

void textedit_update_nonce( void ) {
	uint8_t *via1 = (uint8_t*)ED_ORIC_VIA_TIM1;
	uint8_t *via2 = (uint8_t*)ED_ORIC_VIA_TIM2;
	uint8_t *tim = (uint8_t*)ED_ORIC_ULA_TIM;

	textstore.nonce[0] = via1[0];
	textstore.nonce[1] = via1[1];
	textstore.nonce[2] = via2[0];
	textstore.nonce[3] = via2[1];
	textstore.nonce[4] = tim[0];
	textstore.nonce[5] = tim[1];
}

// Event handler
void textedit_event( uint8_t c ) {
	register int8_t i;

	switch ( c ) {
		// Insert soft TAB
		case TEXTEDIT_CTRL_Z:
			for ( i = TEXTEDIT_TABSZ - ( textedit_cur_x % TEXTEDIT_TABSZ ); i > 0; i-- ) {
				if ( textedit_insert( textedit_lpntr, textedit_cur_x, c ) == false ) {
					atmos_ping( );
					break;
				}
				else {
					// Update saved flag
					textedit_saved_flag = false;
				}
			}
		goto textedit_skip_screen_refresh;

		// Toggle the screensaver flag
		case TEXTEDIT_CTRL_N:
		textedit_sc_enable = !textedit_sc_enable;
		if ( textedit_sc_enable ) {
			textedit_status_popup( "SCREENSAVER ENABLED" );
			textedit_ret_redef( );
		}
		else {
			textedit_status_popup( "SCREENSAVER DISABLED" );
			textedit_ret_blank(  );
		}
		break;

		// Toggle the inverted flag
		case TEXTEDIT_CTRL_O:
		textedit_inverted_flag = !textedit_inverted_flag;
		break;

		// Display help
		case TEXTEDIT_CTRL_G:
		libscreen_clear( LIBSCREEN_SPACE );
		snprintf( 	textedit_status, 
					LIBSCREEN_NB_COLS+1,
					"%s", 				  "          U S E R    G U I D E          " );
		libscreen_copyline_inv( 0, 	(uint8_t*)textedit_status );

		libscreen_copyline( 3, 	(uint8_t*)"[CTRL]-S: SAVE      [CTRL]-C: COPY  LINE" );
		libscreen_copyline( 5, 	(uint8_t*)"[CTRL]-X: CUT LINE  [CTRL]-V: PASTE LINE" );

		libscreen_copyline( 9,  (uint8_t*)"[CTRL]-Q: WHITE INK [CTRL]-W: RED   INK " );
		libscreen_copyline( 11, (uint8_t*)"[CTRL]-E: GREEN INK [CTRL]-R: BLUE  INK " );
		libscreen_copyline( 13, (uint8_t*)"[CTRL]-T: BLACK INK [CTRL]-Y: BLACK PAP " );
		libscreen_copyline( 15, (uint8_t*)"[CTRL]-U: RED   PAP [CTRL]-A: YELL  PAP " );
		libscreen_copyline( 17, (uint8_t*)"[CTRL]-D: BLUE  PAP [CTRL]-O: INVERT    " );

		libscreen_copyline( 21, (uint8_t*)"[CTRL]-F: PAGE UP   [CTRL]-B: PAGE DOWN " );
		libscreen_copyline( 23, (uint8_t*)"[CTRL]-P: PRINT     [ESC]:    QUIT      " );
		libscreen_copyline( 25, (uint8_t*)"[CTRL]-Z: SOFT TAB  [CTRL]-N: SVR ON/OFF" );

		libscreen_copyline_inv( 
							27, (uint8_t*)"                       (c) SYNTAXIC 2025" );
		
		// Active wait and launch of a screensaver after a while
		if ( textedit_sc_enable ) {
			textedit_sc_counter = TEXTEDIT_SCREENSAVER_TO;
			while ( !kbhit() ) {
				textedit_screensaver( );
			}
		}

		// Keyboard buffer flush
		cgetc( );

		break;

		case TEXTEDIT_CTRL_S:
		// Checking if media is readable by issueing dummy command
		snprintf( 	liboric_cmd, 
					LIBORIC_MAX_CMD_SIZE, 
					"UNPROT\"%s\"", 
					textedit_filename );
		liboric_basic( liboric_cmd );
		if ( 	( liboric_error_nd( ) != SEDORIC_NO_ERROR ) &&
				( liboric_error_nd( ) != SEDORIC_FILE_NOT_FOUND_ERROR ) ) {
			// Error encountered, abort
			textedit_status_popup( "DISK ERROR!" );
			break;
		}
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
		i = textedit_status_YN( "ORIC MCP-40 PRINTER?" );
		if ( i == TEXTEDIT_CANCEL ) {
			break;
		}
		textedit_status_print( "PRINTING.. (PRESS ANY KEY TO ABORT)" );
		if ( i == true ) {
			textstore_print( TEXTSTORE_PRINTER_MCP40 );
		}
		else {
			textstore_print( TEXTSTORE_PRINTER_GENERIC );
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
		// Update cursor horizontal position at the end of the line
		textedit_cur_x = textstore.lsize[textedit_lpntr];
		break;

		case TEXTEDIT_KEY_ESC:
		if ( !textedit_saved_flag ) {
			i = textedit_status_YN( "QUIT WITHOUT SAVING?" );
			if (  i == true ) {
				textedit_exit( );
			}
			else {
				if ( i == TEXTEDIT_CANCEL )
					break;
				textedit_event( TEXTEDIT_CTRL_S );
				if ( textedit_saved_flag ) {
					textedit_exit( );
				}
			}
		}
		else {
			textedit_exit( );
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
			if ( textedit_cur_x == TEXTSTORE_LINE_SIZE ) {
				textedit_cur_x--;
			}
			// Update saved flag
			textedit_saved_flag = false;
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
			}
		}
		break;

		case TEXTEDIT_CTRL_V:
		// Something in copy buffer ?
		if ( !textedit_copy_buf_sz ) {
			break;
		}
		// Insert a new line
		if ( textstore_insert_line( textedit_lpntr ) ) {
			atmos_ping();
			textedit_mem_full( );
			break;
		}
		// Paste buffer to the new line
		textstore_write_chars( textedit_lpntr, 0, textedit_copy_buf, textedit_copy_buf_sz );
		// Update saved flag
		textedit_saved_flag = false;
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
				( textedit_cur_x != TEXTSTORE_LINE_SIZE - 1 ) &&
				( textstore.tlpt[textedit_lpntr][textedit_cur_x] != TEXTSTORE_CHAR_RET ) ) {
			// Update x cursor
			textedit_cur_x++;
		}
		else {
			if ( textedit_lpntr < textstore.nblines - 1 ) {
				textedit_event( TEXTEDIT_ARROW_DOWN );
				// Update x cursor
				textedit_cur_x = 0;
			}
		}
		// Adjust cursor position in case its at the right of a CRLF
		textedit_adjust_cursor( );
		// Refresh current line
		libscreen_copyline( textedit_cur_y, textstore.tlpt[textedit_lpntr] );
		break;

		case TEXTEDIT_ARROW_LEFT:
		if ( textedit_cur_x > 0 ) {
			// Update x cursor
			textedit_cur_x--;
		}
		else {
			if ( textedit_lpntr > TEXTEDIT_TEXT_BASE ) {
				textedit_event( TEXTEDIT_ARROW_UP );
				textedit_cur_x = textstore.lsize[textedit_lpntr];
				if ( textedit_cur_x > TEXTSTORE_LINE_SIZE - 1 ) {
					textedit_cur_x = TEXTSTORE_LINE_SIZE - 1;
				}
			}
		}
		// Adjust cursor position in case its at the right of a CRLF
		textedit_adjust_cursor( );
		// Refresh current line
		libscreen_copyline( textedit_cur_y, textstore.tlpt[textedit_lpntr] );
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
		}
		else  {
			textedit_spntr--;
		}
		// If needed, update cursor horizontal position
		if ( textedit_cur_x > textstore.lsize[textedit_lpntr] ) {
			textedit_cur_x = textstore.lsize[textedit_lpntr];
		}
		// Adjust cursor position in case its at the right of a CRLF
		textedit_adjust_cursor( );
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
		}
		else {
			textedit_spntr++;
		}
		// If needed, update cursor horizontal position
		if ( textedit_cur_x > textstore.lsize[textedit_lpntr] ) {
			textedit_cur_x = textstore.lsize[textedit_lpntr];
		}
		// Adjust cursor position in case its at the right of a CRLF
		textedit_adjust_cursor( );
		break;

		case TEXTEDIT_KEY_DEL:
		if ( textedit_insert( textedit_lpntr, textedit_cur_x, c ) == false ) {
			atmos_ping( );
		}
		else {
			// Update saved flag
			textedit_saved_flag = false;
			goto textedit_skip_screen_refresh;
		}
		break;

		case TEXTEDIT_KEY_RET:
		if ( textedit_insert( textedit_lpntr, textedit_cur_x, TEXTSTORE_CHAR_RET ) == false ) {
			atmos_ping( );
		}
		else {
			// Update saved flag
			textedit_saved_flag = false;
			goto textedit_skip_screen_refresh;
		}
		break;

		default:
		// Replace pound char with plain char
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

		// Invert character if needed
		if ( textedit_inverted_flag ) {
			c |= LIBSCREEN_INVERT_BIT;
		}

		// Insert char
		if ( textedit_insert( textedit_lpntr, textedit_cur_x, c ) == false ) {
			atmos_ping( );
		}
		else {
			// Update saved flag
			textedit_saved_flag = false;
			goto textedit_skip_screen_refresh;
		}
		break;
	}

	// Refresh text portion of the screen
	textedit_screen_refresh( );

	textedit_skip_screen_refresh:

	// Refresh status line
	textedit_status_refresh( );

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
				libscreen_clearline( TEXTEDIT_EDITORSCR_BASE + i, LIBSCREEN_SPACE );
			}

		}
	}
}

// Refresh status line
void textedit_status_refresh( void ) {
	char saved, *state;
	static char inverse[] = "INV";
	static char normal[] = "STD";

	if ( textedit_filename == NULL ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		printf( "E2" );
		#endif
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

	// Insert blue paper code
	textedit_status[0] = LIBSCREEN_BLUE_PAPER;

	snprintf( 	&textedit_status[1], 
				LIBSCREEN_NB_COLS, 
				"%-13s%c %03d%% %s [CTRL]-G>>HELP",  
				textedit_filename,
				saved,
				( textstore.nblines * 100 ) / TEXTSTORE_LINES_MAX,
				state );
	libscreen_copyline( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );
}

// Refresh cursor
void textedit_cursor_refresh( void ) {
	libscreen_textbuf[textedit_cur_y*LIBSCREEN_NB_COLS+textedit_cur_x] ^= LIBSCREEN_INVERT_BIT;
}

// Adjust cursor position
void textedit_adjust_cursor( void ) {
	// Check if cursor is at EOL and line is not empty
	if ( ( textstore.lsize[textedit_lpntr] ) && ( textedit_cur_x == textstore.lsize[textedit_lpntr] ) ) {
		// Check if character at the left is a CRLF
		if ( textstore.tlpt[textedit_lpntr][textedit_cur_x-1] == TEXTSTORE_CHAR_RET ) {
			// Place the cursor on the CRLF char
			textedit_cur_x--;
		}
	}
}

//
// Character insertion handling
// Returns false if insertion failed
// Returns true if insertion succeeded
// 
bool textedit_insert( uint16_t lpos, uint8_t cpos, uint8_t c ) {
															// Line buffer
	static uint8_t 	linebuf[TEXTEDIT_INSBUFSCAN*TEXTSTORE_LINE_SIZE+1];
	static uint8_t 	wordbuf[TEXTSTORE_LINE_SIZE];			// Word buffer
	uint16_t 		lidx;									// Line index
	uint16_t		lidxstart, lidxstop;					// Start and stop line numbers for scanning
	uint8_t			lbufsz = 0;								// Line buffer size
	uint8_t			lbufc = 0;								// Cursor position in line buffer
	uint8_t			cidx = 0;								// Char index in word buffer
	uint8_t			wbufsz = 0;								// Word buffer size
	uint8_t			wbufc = 0;								// Cursor position in word buffer
	bool			wbufcflag = false;						// Flag indicating cursor position has been set

	// If there is no more line left and the char is not DEL, refuse insertion
	if ( 	( textstore.nblines == TEXTSTORE_LINES_MAX ) && 
			( c != TEXTEDIT_KEY_DEL ) ) {
		textedit_mem_full( );
		return false;
	}

	// If we are appending and not at the boudndaries of the line, take a shortcut
	if ( 	( textstore.lsize[lpos] > 0 ) &&
			( textstore.lsize[lpos] < TEXTSTORE_LINE_SIZE - 1 ) &&
			( c != TEXTEDIT_KEY_DEL ) &&
			( c != TEXTSTORE_CHAR_RET ) &&
			( ( ( cpos > 0 ) && ( cpos < textstore.lsize[lpos] - 1 ) ) || 
			( ( cpos == textstore.lsize[lpos] ) && ( lpos == textstore.nblines - 1 ) ) ) ) {

		// Insert char
		textstore_insert_char( lpos, cpos, c );
		// Increment cursor horizontal position
		textedit_cur_x++;
		// Refresh only this line
		libscreen_copyline( textedit_cur_y, textstore.tlpt[lpos] );
		return true;
	}
	
	// Define scanning range
	if ( lpos == 0 ) {
		lidxstart = 0;
		lidxstop = TEXTEDIT_INSBUFSCAN - 1;
	}
	else {
		lidxstart = lpos - 1;
		lidxstop = lidxstart + TEXTEDIT_INSBUFSCAN;
	}
	if ( lidxstop > textstore.nblines ) {
		lidxstop = textstore.nblines;
	}

	// Copy scanned text portion into line buffer
	for ( lidx = lidxstart; lidx < lidxstop; lidx++ ) {

		// Sanity check
		#ifdef ED_DEBUG
		if ( lbufsz + textstore.lsize[lidx] > TEXTEDIT_INSBUFSCAN*TEXTSTORE_LINE_SIZE ) {
			#ifdef ED_VERBOSE
			ed_fatal_error( __FILE__, __LINE__ );
			#else
			printf( "E3" );
			#endif
		}
		#endif

		// Copy text into line buffer
		memcpy( &linebuf[lbufsz], textstore.tlpt[lidx], textstore.lsize[lidx] );

		// Update line buffer size
		lbufsz += textstore.lsize[lidx];	

		// Update cursor position within line buffer
		if ( lidx < lpos ) {
			lbufc += textstore.lsize[lidx];
		}
		if ( lidx == lpos ) {
			lbufc += cpos;
		}
	}

	// Limit the cursor position to the boundary
	if ( lbufc > lbufsz ) {
		lbufc = lbufsz;
	}

	// If typed char is DEL, remove a char
	if ( c == TEXTEDIT_KEY_DEL ) {
		if ( lbufc ) {
			// Move the chars starting from the cursor one step left
			// If cursor is at the end of the buffer, do nothing
			memmove( &linebuf[lbufc-1], &linebuf[lbufc], lbufsz - lbufc );

			// Decrement size of the buffer
			lbufsz--;

			// Decrement position of the cursor
			lbufc--;
		}
		// Try to DEL at the start on the line buffer
		else {
			return false;
		}
	}
	// Every other typed chars should be inserted
	else {

		// Move the chars starting form the cursor one step right
		// If cursor is at the end of the buffer, do nothing
		memmove( &linebuf[lbufc+1], &linebuf[lbufc], lbufsz - lbufc );

		// Insert char
		linebuf[lbufc] = c;

		// Increment size of the buffer
		lbufsz++;

		// Increment position of the cursor
		lbufc++;
	}

	// Scan the lines from the start
	lidx = lidxstart;

	// Clear current line
	textstore_clear_line( lidx );

	// Scan whole line buffer, look for for next word
	for ( cidx = 0; cidx < lbufsz; cidx++ ) {
		
		// If word buffer not full
		if ( wbufsz < TEXTSTORE_LINE_SIZE ) {

			// If needed, store cursor postion within word
			if ( cidx == lbufc ) {
				wbufc = wbufsz;
				wbufcflag = true;
			}

			// Add next char to word buffer
			wordbuf[wbufsz++] = linebuf[cidx];
		}

		// If current char is RET or SPACE, if wordbuf full or end of linebuf, process word
		if (	( linebuf[cidx] == TEXTSTORE_CHAR_SPACE ) ||
				( linebuf[cidx] == TEXTSTORE_CHAR_RET ) ||
				( wbufsz == TEXTSTORE_LINE_SIZE ) ||
				( cidx == lbufsz - 1 ) ) {

			// If current line + current word does not fit into current line
			if ( wbufsz + textstore.lsize[lidx] > TEXTSTORE_LINE_SIZE ) {
				
				// Increment current line 
				if ( ++lidx >= lidxstop ) {

					// Insert new line if we past the last scanned line
					if ( textstore_insert_line( lidx ) ) {
						textedit_mem_full( );
						return false;
					}
				}
				else {

					// Erase current line
					textstore_clear_line( lidx );
				}
			}

			// Update cursor position
			if ( wbufcflag ) {

				// Save horizontal position
				textedit_cur_x = textstore.lsize[lidx] + wbufc;

				// Save current line into vertical pointer
				textedit_lpntr = lidx;

				// Toggle cursor flag
				wbufcflag = false;
			}

			// Copy current word into current line
			textstore_insert_chars( lidx, textstore.lsize[lidx], wordbuf, wbufsz );

			// If current word is terminated by RET
			// NB: cursor position should not be updated since
			// the CRLF is after the cursor position which
			// is within the inserted word
			if ( linebuf[cidx] == TEXTSTORE_CHAR_RET ) {

				// Increment current line 
				if ( ++lidx >= lidxstop ) {

					// Insert new line if we past the last scanned line
					if ( textstore_insert_line( lidx ) ) {
						textedit_mem_full( );
						return false;
					}
				}
				else {

					// Erase current line
					textstore_clear_line( lidx );
				}
			}

			// Reset current word size
			wbufsz = 0;
		}
	}

	// Check if a line should be removed
	if ( lidx < lidxstop - 1 ) {
		textstore_del_line( lidx + 1 );
	}

	// Check if cursor should be appened after the text
	if ( lbufc == lbufsz ) {

		// Place the cursor on the current line
		textedit_lpntr = lidx;
		textedit_cur_x = textstore.lsize[lidx];

		// Check if cursor went over the limit of the line
		if ( ( textedit_cur_x >= TEXTSTORE_LINE_SIZE ) ) {

			// Insert new line
			if ( textstore_insert_line( lidx + 1 ) ) {
				textedit_mem_full( );
				return false;
			}

			// Update cursor position
			textedit_cur_x = 0;
			textedit_lpntr = lidx + 1;
		}
	}

	// Sanity check
	#ifdef ED_DEBUG
	if ( ( textedit_cur_x >= TEXTSTORE_LINE_SIZE ) || ( textedit_lpntr >= textstore.nblines ) ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "E4" );
		#endif
	}
	#endif

	// Update vertical screen pointers
	if ( textedit_lpntr < textedit_spntr ) {
		textedit_spntr = textedit_lpntr;
	}
	if ( textedit_lpntr >= textedit_spntr + TEXTEDIT_EDITORSCR_SZ ) {
		textedit_spntr = textedit_lpntr - TEXTEDIT_EDITORSCR_SZ + 1;
	}
	textedit_cur_y = textedit_lpntr - textedit_spntr + TEXTEDIT_EDITORSCR_BASE;

	// Reformat the remainder of the text
	if ( lidx < textstore.nblines - 1 ) {
		textstore_reformat( lidx + 1 );
	}

	// Refresh whole screen
	textedit_screen_refresh( );

	return true;
}
