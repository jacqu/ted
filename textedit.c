/* ================================================================== *
 * textedit: higher-level API for managing text edition               *
 * ================================================================== */

#include <stdlib.h>
#include <string.h>
#include <atmos.h>
#include <unistd.h>
#include <conio.h>
#include <stdbool.h>
#include <stdint.h>
#include "libscreen.h"
#include "liboric.h"
#include "chacha20.h"
#include "blake2s.h"
#include "textstore.h"
#include "textedit.h"
#include "strfmt.h"
#include "libcanary.h"
#include "ed.h"

#define 		TEXTEDIT_UNUSED(x) (void)(x)

/* ------------------------------------------------------------------ *
 * Cryptographic state of the session                                 *
 * ------------------------------------------------------------------ */
static uint8_t	textedit_pool[TEXTEDIT_POOL_SZ];		// Entropy gathered while typing
static uint8_t	textedit_pool_index = 0;				// Where the next sample lands
static uint8_t	textedit_cipher_key[TEXTEDIT_KEY_SZ];	// Key the text is encrypted with
static uint8_t	textedit_tag_key[TEXTEDIT_KEY_SZ];		// Key the tag is computed with
static uint8_t	textedit_nonce_key[TEXTEDIT_KEY_SZ];	// Key the nonce is drawn with
static bool		textedit_unverified = false;			// Set when a file failed its tag

/* These serve the loading of a file, which happens before they are   *
 * defined further down                                               */
static void		textedit_pool_add	( const uint8_t*, uint8_t );
static bool		textedit_tag_equal	( const uint8_t*, const uint8_t* );
static void		textedit_load_abort	( const char* );
static bool		textedit_console_YN	( const char* );

uint8_t			*textedit_retc_a = (uint8_t*)TEXTEDIT_RET_CHAR_ADDRESS;
uint8_t			textedit_retc_i[TEXTEDIT_ORIC_CHARS_HEIGHT];
uint8_t			textedit_cur_x = 0;
uint8_t			textedit_cur_y = TEXTEDIT_EDITORSCR_BASE;
uint16_t		textedit_lpntr = TEXTEDIT_TEXT_BASE;
uint16_t		textedit_spntr = TEXTEDIT_TEXT_BASE;
// The status line, one byte per column and one to spare
// This is an array of characters and not a string: the routines that
// fill it blank it to its full width and then write over the blanks,
// without ever laying down a terminator, and libscreen_copyline_inv
// takes a fixed number of columns rather than looking for one. Anything
// treating it as a string, strlen and the %s of printf being the two
// obvious ones, would read past the end of it. Use strfmt_end first
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
					col_off[i] |= TEXTEDIT_SC_LEADER_BIT;
				}
			}
			else {
				if ( col_off[i] & TEXTEDIT_SC_COUNT_MASK ) {
					col_off[i]--;
					// The magic occurs here
					libscreen_textbuf[i] = '!' + ( rand() % 94 );
					if ( col_off[i] & TEXTEDIT_SC_LEADER_BIT ) {
						col_off[i] &= TEXTEDIT_SC_COUNT_MASK;
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
	uint8_t		textedit_tag[TEXTSTORE_TAG_SZ];		// Tag recomputed over what arrived
	uint8_t		i;									// Index inside the command being built

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

	// Gather a first sample, so that the pool is never looked at empty

	textedit_entropy_stir( );


	// Give each use of the password its own key before anything is read

#ifdef TED_CANARY
	// Paint the memory that is going to be watched
	libcanary_init( );
#endif

	textedit_keys_derive( );


	// Initialize text and line APIs

	textstore_init( );

	// Load file
	i = strfmt_copy( liboric_cmd, TEXTEDIT_LOAD_COMMAND, LIBORIC_MAX_CMD_SIZE );
	i += strfmt_copy( &liboric_cmd[i], textedit_filename, LIBORIC_MAX_CMD_SIZE - i );
	i += strfmt_copy( &liboric_cmd[i], TEXTEDIT_LOAD_ADDRESS, LIBORIC_MAX_CMD_SIZE - i );
	i += strfmt_number( &liboric_cmd[i], (uint16_t)&textstore, TEXTEDIT_ADDRESS_DIGITS,
						LIBORIC_MAX_CMD_SIZE - i );
	i += strfmt_copy( &liboric_cmd[i], TEXTEDIT_LOAD_SUFFIX, LIBORIC_MAX_CMD_SIZE - i );
	strfmt_end( liboric_cmd, i, LIBORIC_MAX_CMD_SIZE );
	liboric_basic( liboric_cmd );
	switch( liboric_error_nd( ) ) {
		case SEDORIC_NO_ERROR:
		libscreen_console_puts( "FILE LOADED SUCCESSFULLY\n" );

		// Refuse a file this version does not know how to read, rather
		// than hand the user a text made of noise
		if ( textstore.version != TEXTSTORE_VERSION ) {
			textedit_load_abort( "UNKNOWN FILE FORMAT\n" );
		}

		// The tag is about to be checked over the number of bytes the
		// header claims, so that number has to name bytes that exist.
		// Sedoric does not say how many it read, so a file that was cut
		// short is caught by its tag rather than by its length
		if ( ( textstore.fsize <= TEXTSTORE_HEADER_SZ ) ||
			 ( textstore.fsize > sizeof( textstore ) ) ) {
			textedit_load_abort( "DAMAGED FILE\n" );
		}

		// The tag is checked before anything is decrypted. A wrong
		// password then costs nothing, and the text is still there to be
		// opened with another one instead of having been turned into
		// noise in place
		libscreen_console_puts( "CHECKING..." );
		blake2s_mac( textedit_tag_key,
					 textstore.nonce,
					 textstore.fsize - TEXTSTORE_TAG_SZ,
					 textedit_tag,
					 TEXTSTORE_TAG_SZ );

		if ( !textedit_tag_equal( textedit_tag, textstore.tag ) ) {
			libscreen_console_puts( "\nWRONG PASSWORD OR DAMAGED FILE\n" );
			if ( !textedit_console_YN( "OPEN IT ANYWAY (Y/N)? " ) ) {
				textedit_load_abort( "\n" );
			}
			// The text is opened on the user's word, and nothing that
			// comes out of it is to be trusted from here on
			textedit_unverified = true;
		}

		sleep( TEXTEDIT_UI_WAIT_TIME );

		// Fix pointers in case of incompatible file versions
		textstore_fix_pointers( );

		// Decrypting
		if ( textedit_password ) {
			libscreen_console_puts( "DECRYPTING..." );
			chacha_process( (uint8_t*)&textstore.magic, 
							textstore.fsize - ( (uint16_t)&textstore.magic - (uint16_t)&textstore),
							textedit_cipher_key, 
							textstore.nonce, 
							0 );
		}

		// Check file validity
		// A file whose tag matched cannot get here, so a magic number
		// that does not fit means the user asked for a file to be opened
		// against the advice given, and is simply told again
		if ( textstore.magic != TEXTSTORE_MAGIC ) {
			if ( !textedit_unverified ) {
				textedit_load_abort( "BAD MAGIC NUMBER\n" );
			}
			libscreen_console_puts( "THE TEXT DID NOT COME OUT RIGHT\n" );
			sleep( TEXTEDIT_UI_WAIT_TIME );
		}
		break;
		
		case SEDORIC_FILE_NOT_FOUND_ERROR:
		libscreen_console_puts( "NEW FILE CREATED\n" );
		sleep( TEXTEDIT_UI_WAIT_TIME );

		// Allocate first line of text
		if ( textstore_insert_line( TEXTEDIT_TEXT_BASE ) ) {
			#ifdef ED_VERBOSE
			ed_fatal_error( __FILE__, __LINE__ );
			#else
			ed_fatal_error( "E1" );
			#endif
		}
		break;

		default:
		libscreen_console_puts( "SEDORIC ERROR\n" );
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
	strfmt_copy( textedit_status, msg, LIBSCREEN_NB_COLS );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );
}

// Print a message on the status line and wait some time
void textedit_status_popup( char *msg ) {

	// Display message
	memset( textedit_status, LIBSCREEN_SPACE, LIBSCREEN_NB_COLS );
	strfmt_copy( textedit_status, msg, LIBSCREEN_NB_COLS );
	libscreen_copyline_inv( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );

	// Wait some time
	sleep( TEXTEDIT_UI_WAIT_TIME );
}

// Ask a question on the status line
uint8_t	textedit_status_YN( char *msg ) {
	uint8_t	i;										// Column the next field starts at

	// Print question
	memset( textedit_status, LIBSCREEN_SPACE, LIBSCREEN_NB_COLS );
	i = strfmt_copy( textedit_status, msg, LIBSCREEN_NB_COLS );
	i += strfmt_copy( &textedit_status[i], TEXTEDIT_UI_ANSWER_PREFIX, LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], TEXTEDIT_UI_YES_ANSWER, LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], TEXTEDIT_UI_ANSWER_SEPARATOR, LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], TEXTEDIT_UI_NO_ANSWER, LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], TEXTEDIT_UI_ANSWER_SEPARATOR, LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], TEXTEDIT_UI_CA_ANSWER, LIBSCREEN_NB_COLS - i );
	strfmt_copy( &textedit_status[i], TEXTEDIT_UI_ANSWER_SUFFIX, LIBSCREEN_NB_COLS - i );
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

// Leave the editor before it has started, saying why
static void textedit_load_abort( const char *message ) {

	libscreen_console_puts( message );
	sleep( TEXTEDIT_UI_WAIT_TIME );
	exit( ED_FATAL_ERROR );
}

// Ask a question on the console, before the status line exists
static bool textedit_console_YN( const char *question ) {
	uint8_t	c;

	libscreen_console_puts( question );

	for ( ;; ) {
		c = cgetc( );
		if ( c == TEXTEDIT_UI_YES_ANSWER ) {
			libscreen_console_puts( "\n" );
			return true;
		}
		if ( ( c == TEXTEDIT_UI_NO_ANSWER ) || ( c == TEXTEDIT_KEY_ESC ) ) {
			libscreen_console_puts( "\n" );
			return false;
		}
	}
}

// Read the free running counters of the machine
// The two timers of the VIA and the one of the ULA are the only things
// on an Atmos that are not the same from one run to the next
static void textedit_entropy( uint8_t *sample ) {
	uint8_t	*timer1 = (uint8_t*)ED_ORIC_VIA_T1_HIGH;
	uint8_t	*timer2 = (uint8_t*)ED_ORIC_VIA_T2_HIGH;
	uint8_t	*tim = (uint8_t*)ED_ORIC_ULA_TIM;
	uint8_t	i;

	sample[0] = *timer1;
	sample[1] = *timer2;
	for ( i = 0; i < ED_ORIC_ULA_TIM_SZ; i++ ) {
		sample[2+i] = tim[i];
	}
}

// Pile bytes into the entropy pool
// Nothing is mixed here on purpose: the pool is a heap of samples, and
// the mixing is the business of the hash that later draws a nonce out of
// it. Piling is an exclusive or, so a sample can only ever add to what
// is already there and never cancel it
static void textedit_pool_add( const uint8_t *data, uint8_t size ) {
	uint8_t	i;

	for ( i = 0; i < size; i++ ) {
		textedit_pool[textedit_pool_index] ^= data[i];
		textedit_pool_index = ( textedit_pool_index + 1 ) & TEXTEDIT_POOL_MASK;
	}
}

// Take one sample of the counters
// Called on every keystroke, so that what ends up in the pool is not the
// value of a counter, which is fairly predictable, but the instant the
// user happened to press a key, which is not
void textedit_entropy_stir( void ) {
	uint8_t	sample[TEXTEDIT_ENTROPY_SZ];

	textedit_entropy( sample );
	textedit_pool_add( sample, TEXTEDIT_ENTROPY_SZ );
}

// Give each use of the password a key of its own
// Handing the same password to the cipher, to the tag and to the nonce
// would tie the three together for no reason. Each gets a key derived
// from the password and from a label naming what it is for. A text saved
// without a password is still given a tag, computed with a key of zeros:
// it detects a damaged file, which is worth having, and it is not
// authentication, since anybody can compute it
void textedit_keys_derive( void ) {
	uint8_t	master[TEXTEDIT_KEY_SZ];

	if ( textedit_password ) {
		memcpy( master, textedit_password, TEXTEDIT_KEY_SZ );
	}
	else {
		memset( master, 0, TEXTEDIT_KEY_SZ );
	}

	blake2s_mac( master, (const uint8_t*)TEXTEDIT_CIPHER_LABEL,
				 sizeof( TEXTEDIT_CIPHER_LABEL ) - 1,
				 textedit_cipher_key, TEXTEDIT_KEY_SZ );
	blake2s_mac( master, (const uint8_t*)TEXTEDIT_TAG_LABEL,
				 sizeof( TEXTEDIT_TAG_LABEL ) - 1,
				 textedit_tag_key, TEXTEDIT_KEY_SZ );
	blake2s_mac( master, (const uint8_t*)TEXTEDIT_NONCE_LABEL,
				 sizeof( TEXTEDIT_NONCE_LABEL ) - 1,
				 textedit_nonce_key, TEXTEDIT_KEY_SZ );

	memset( master, 0, TEXTEDIT_KEY_SZ );
}

// Draw a fresh nonce for the text about to be saved
// Everything that could tell two saves apart goes in: the entropy piled
// up while the text was being typed, one last look at the counters, the
// length of the file and the shape of the text. A nonce which repeats
// would be serious now that files carry a tag, and it takes all of those
// agreeing at once for that to happen
void textedit_update_nonce( uint16_t fsize ) {
	uint8_t	sample[TEXTEDIT_ENTROPY_SZ];

	textedit_entropy( sample );

	blake2s_init( textedit_nonce_key, TEXTEDIT_KEY_SZ, TEXTSTORE_NONCE_SZ );
	blake2s_update( textedit_pool, TEXTEDIT_POOL_SZ );
	blake2s_update( sample, TEXTEDIT_ENTROPY_SZ );
	blake2s_update( (const uint8_t*)&fsize, sizeof( fsize ) );
	blake2s_update( textstore.lsize, textstore.nblines );
	blake2s_final( textstore.nonce );
}

// Compare two tags without letting the answer show in the time taken
// There is nobody to measure it on a file read from a disk, but a
// comparison that stops at the first difference is a habit worth not
// taking: every byte is looked at, and only the total is tested
static bool textedit_tag_equal( const uint8_t *left, const uint8_t *right ) {
	uint8_t	difference = 0;
	uint8_t	i;

	for ( i = 0; i < TEXTSTORE_TAG_SZ; i++ ) {
		difference |= left[i] ^ right[i];
	}

	return ( difference == 0 );
}

// Event handler
void textedit_event( uint8_t c ) {
	register int8_t i;
	uint16_t		textedit_fsize;					// Bytes the file is about to hold
	uint8_t			j;								// Index inside the command being built

	// Every keystroke tells the machine something it could not have
	// guessed: the instant it arrived
	textedit_entropy_stir( );

	switch ( c ) {
		// Insert soft TAB
		case TEXTEDIT_CTRL_Z:
			for ( i = TEXTEDIT_TABSZ - ( textedit_cur_x % TEXTEDIT_TABSZ ); i > 0; i-- ) {
				if ( textedit_insert( textedit_lpntr, textedit_cur_x, TEXTSTORE_CHAR_SPACE ) == false ) {
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
		strfmt_copy( textedit_status, TEXTEDIT_STATUS_GUIDE_TITLE, LIBSCREEN_NB_COLS );
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

		libscreen_clearline( 27, LIBSCREEN_SPACE ^ LIBSCREEN_INVERT_BIT );
		
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
		j = strfmt_copy( liboric_cmd, TEXTEDIT_UNPROT_COMMAND, LIBORIC_MAX_CMD_SIZE );
		j += strfmt_copy( &liboric_cmd[j], textedit_filename, LIBORIC_MAX_CMD_SIZE - j );
		j += strfmt_char( &liboric_cmd[j], TEXTEDIT_COMMAND_QUOTE, LIBORIC_MAX_CMD_SIZE - j );
		strfmt_end( liboric_cmd, j, LIBORIC_MAX_CMD_SIZE );
		liboric_basic( liboric_cmd );
		if ( 	( liboric_error_nd( ) != SEDORIC_NO_ERROR ) &&
				( liboric_error_nd( ) != SEDORIC_FILE_NOT_FOUND_ERROR ) ) {
			// Error encountered, abort
			textedit_status_popup( "DISK ERROR!" );
			break;
		}
		// Fill in the header before anything is computed over it
		textedit_fsize = textstore_sizeof( );
		textstore.version = TEXTSTORE_VERSION;
		textstore.fsize = textedit_fsize;
		textedit_update_nonce( textedit_fsize );

		// Encrypting
		if ( textedit_password ) {
			textedit_status_print( "ENCRYPTING.." );
			chacha_process( (uint8_t*)&textstore.magic, 
							textedit_fsize - ( (uint16_t)&textstore.magic - (uint16_t)&textstore ),
							textedit_cipher_key, 
							textstore.nonce, 
							0 );
		}

		// Sealing
		// The tag is computed over the file as it is about to be written,
		// encrypted header and all, so that everything a reader is going
		// to believe has been authenticated before it believes it
		textedit_status_print( "SEALING.." );
		blake2s_mac( textedit_tag_key,
					 textstore.nonce,
					 textedit_fsize - TEXTSTORE_TAG_SZ,
					 textstore.tag,
					 TEXTSTORE_TAG_SZ );

		// The tag is the most thoroughly mixed thing the machine owns, so
		// it goes back into the pool the next nonce will be drawn from
		textedit_pool_add( textstore.tag, TEXTSTORE_TAG_SZ );

		// Display saving message
		textedit_status_print( "SAVING.." );
		// Save textstore
		j = strfmt_copy( liboric_cmd, TEXTEDIT_SAVE_COMMAND, LIBORIC_MAX_CMD_SIZE );
		j += strfmt_copy( &liboric_cmd[j], textedit_filename, LIBORIC_MAX_CMD_SIZE - j );
		j += strfmt_copy( &liboric_cmd[j], TEXTEDIT_LOAD_ADDRESS, LIBORIC_MAX_CMD_SIZE - j );
		j += strfmt_number( &liboric_cmd[j], (uint16_t)&textstore, TEXTEDIT_ADDRESS_DIGITS,
							LIBORIC_MAX_CMD_SIZE - j );
		j += strfmt_copy( &liboric_cmd[j], TEXTEDIT_SAVE_END, LIBORIC_MAX_CMD_SIZE - j );
		j += strfmt_number( &liboric_cmd[j],
							(uint16_t)&textstore + textedit_fsize - 1,
							TEXTEDIT_ADDRESS_DIGITS,
							LIBORIC_MAX_CMD_SIZE - j );
		strfmt_end( liboric_cmd, j, LIBORIC_MAX_CMD_SIZE );
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
							textedit_fsize - ( (uint16_t)&textstore.magic - (uint16_t)&textstore ),
							textedit_cipher_key, 
							textstore.nonce, 
							0 );
		}
		break;

		case TEXTEDIT_CTRL_P:
		i = textedit_status_YN( "ORIC MCP-40 PRINTER?" );
		if ( i == TEXTEDIT_CANCEL ) {
			break;
		}
		textedit_status_print( "PRINTING.. (HOLD SPACE BAR TO PAUSE)" );
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
		if ( textedit_cur_x == TEXTSTORE_LINE_SIZE ) {
			textedit_cur_x--;
		}
		break;

		case TEXTEDIT_KEY_ESC:
		if ( !textedit_saved_flag ) {
			i = textedit_status_YN( "SAVE BEFORE EXITING?" );
			if (  i == true ) {
				textedit_event( TEXTEDIT_CTRL_S );
				if ( textedit_saved_flag ) {
					textedit_exit( );
				}
			}
			else {
				if ( i == TEXTEDIT_CANCEL ) {
					break;
				}
				textedit_exit( );
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
		// If the line is not full, add a CR at the end of the copy buffer
		if ( textstore.lsize[textedit_lpntr] < TEXTSTORE_LINE_SIZE ) {
			if
			( 
				( textedit_copy_buf[textstore.lsize[textedit_lpntr]-1] != TEXTSTORE_CHAR_RET ) &&
				( textedit_copy_buf[textstore.lsize[textedit_lpntr]-1] != TEXTSTORE_CHAR_SPACE )
			)
			{
				textedit_copy_buf[textstore.lsize[textedit_lpntr]] = TEXTSTORE_CHAR_RET;
				textedit_copy_buf_sz++;
			}
		}
		
		break;

		case TEXTEDIT_CTRL_X:
		if ( !textstore.nblines ) {
			break;
		}
		// Copy current line to copy buffer
		textedit_event( TEXTEDIT_CTRL_C );
		// Cut current line
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
		// Adjust cursor position in case its at the right of a CRLF
		textedit_adjust_cursor( );
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
		// Adjust cursor position in case its at the right of a CRLF
		textedit_adjust_cursor( );
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
			if ( textedit_cur_x == TEXTSTORE_LINE_SIZE ) {
				textedit_cur_x--;
			}
		}
		// Adjust cursor position in case its at the right of a CRLF
		textedit_adjust_cursor( );
		break;

		case TEXTEDIT_ARROW_DOWN:
		if ( !textstore.nblines ) {
			break;
		}
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
			if ( textedit_cur_x == TEXTSTORE_LINE_SIZE ) {
				textedit_cur_x--;
			}
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

				// Char is non printable and has not been identified as a valide special code
				default:
				goto textedit_skip_event;
			}
		}

		// Dead key + color code from 0 to 7
		if ( c == TEXTEDIT_KEY_POUND ) {
			// Waiting for color key
			c = cgetc( );
			if ( ( c >= '0' ) && ( c <= '7' ) ) {
				c = LIBSCREEN_BLACK_PAPER + ( c - '0' ); 
			}
			else {
				c = LIBSCREEN_PLAIN_CHAR;
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
// Bring the position back inside the text
// Nothing should ever put it outside, and everything below assumes it is
// inside: an index past the end makes the subtraction guarding the fast
// display path wrap round, the test then passes, and the display reads
// line pointers that do not exist. Whatever went wrong upstream, the
// damage stops here rather than turning into a screen full of whatever
// happened to be in memory
static void textedit_clamp( void ) {

	if ( !textstore.nblines ) {
		textedit_spntr = 0;
		textedit_lpntr = 0;
		textedit_cur_y = TEXTEDIT_EDITORSCR_BASE;
		return;
	}

	// The cursor cannot sit past the last line
	if ( textedit_lpntr >= textstore.nblines ) {
		textedit_lpntr = textstore.nblines - 1;
	}

	// The first line shown cannot be below the cursor, nor so far above
	// it that the cursor falls off the bottom of the screen
	if ( textedit_spntr > textedit_lpntr ) {
		textedit_spntr = textedit_lpntr;
	}
	if ( textedit_lpntr - textedit_spntr >= TEXTEDIT_EDITORSCR_SZ ) {
		textedit_spntr = textedit_lpntr - ( TEXTEDIT_EDITORSCR_SZ - 1 );
	}

	// The row of the cursor follows from the two, but it is only put back
	// when it says something impossible: the editor moves the three of
	// them in an order of its own, and a refresh may well happen while
	// they are on their way to agreeing again
	if ( ( textedit_cur_y < TEXTEDIT_EDITORSCR_BASE ) ||
		 ( textedit_cur_y >= TEXTEDIT_EDITORSCR_BASE + TEXTEDIT_EDITORSCR_SZ ) ) {
		textedit_cur_y = (uint8_t)( TEXTEDIT_EDITORSCR_BASE +
									( textedit_lpntr - textedit_spntr ) );
	}
}

#ifdef TED_CANARY
// Say where the memory was touched and go no further
// Carrying on would only pile more damage on top of the evidence, and
// the address of the first byte that changed is usually enough to name
// what wrote there
static void textedit_canary_stop( uint16_t where ) {
	char	message[LIBSCREEN_NB_COLS+1];
	uint8_t	i;

	i = strfmt_copy( message, TEXTEDIT_CANARY_MESSAGE, LIBSCREEN_NB_COLS );
	i += strfmt_number( &message[i], where, TEXTEDIT_CANARY_DIGITS,
						LIBSCREEN_NB_COLS - i );
	strfmt_end( message, i, sizeof( message ) );

	for ( ;; ) {
		textedit_status_print( message );
	}
}
#endif

void textedit_screen_refresh( void ) {
	register uint8_t i;
#ifdef TED_CANARY
	uint16_t		touched;
#endif

	// Refuse to draw from a position that does not exist
	textedit_clamp( );

#ifdef TED_CANARY
	// Has anybody written where nobody should ?
	touched = libcanary_check( );
	if ( touched != LIBCANARY_INTACT ) {
		textedit_canary_stop( touched );
	}
#endif

	// Refresh text portion of the screen
	if ( ( textedit_spntr <= textstore.nblines ) &&
		 ( textstore.nblines - textedit_spntr >= TEXTEDIT_EDITORSCR_SZ ) ) {
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
	uint8_t	i;										// Column the next field starts at
	char saved, *state;
	static char inverse[] = "INV";
	static char normal[] = "STD";

	if ( textedit_filename == NULL ) {
		#ifdef ED_VERBOSE
		ed_fatal_error( __FILE__, __LINE__ );
		#else
		ed_fatal_error( "E2" );
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

	// The whole line is blanked first, so that every field is padded.
	// The name used to be padded by the %-13s of a format string, and
	// nothing pads it now: without this, whatever the previous message
	// left in the columns the name does not fill stays on show
	memset( textedit_status, LIBSCREEN_SPACE, LIBSCREEN_NB_COLS );

	// Insert blue paper code
	textedit_status[0] = LIBSCREEN_BLUE_PAPER;

	// File name, truncated to the width of its field
	strfmt_copy( &textedit_status[TEXTEDIT_STATUS_TEXT_BASE],
				 textedit_filename,
				 TEXTEDIT_STATUS_NAME_WIDTH );
	i = TEXTEDIT_STATUS_TEXT_BASE + TEXTEDIT_STATUS_NAME_WIDTH;

	// Modification mark
	i += strfmt_char( &textedit_status[i], saved, LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], LIBSCREEN_SPACE, LIBSCREEN_NB_COLS - i );

	// Share of the text memory in use
	i += strfmt_number( &textedit_status[i],
						( textstore.nblines * TEXTEDIT_STATUS_PERCENT_FULL ) /
						TEXTSTORE_LINES_MAX,
						TEXTEDIT_STATUS_PERCENT_DIGITS,
						LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], TEXTEDIT_STATUS_PERCENT_SIGN,
					  LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], LIBSCREEN_SPACE, LIBSCREEN_NB_COLS - i );

	// Character mode
	i += strfmt_copy( &textedit_status[i], state, LIBSCREEN_NB_COLS - i );
	i += strfmt_char( &textedit_status[i], LIBSCREEN_SPACE, LIBSCREEN_NB_COLS - i );

	// Reminder of the shortcut opening the user guide
	strfmt_copy( &textedit_status[i], TEXTEDIT_STATUS_HELP_HINT, LIBSCREEN_NB_COLS - i );
	libscreen_copyline( TEXTEDIT_STATUSSCR_BASE, (uint8_t*)textedit_status );
}

// Refresh cursor
void textedit_cursor_refresh( void ) {

	// If the cursor is out of the screen, place it at the last column of the screen
	if ( textedit_cur_x >= LIBSCREEN_NB_COLS ) {
		textedit_cur_x = LIBSCREEN_NB_COLS - 1;
	}

	// Invert the character at the cursor position
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
	uint8_t			nlines;									// Lines needed by the reformatting
	uint8_t			fill;									// Simulated filling of the current line
	uint8_t			wsz;									// Simulated word buffer size

	// If there is no more line left and the char is not DEL, refuse insertion
	if ( 	( textstore.nblines == TEXTSTORE_LINES_MAX ) && 
			( c != TEXTEDIT_KEY_DEL ) ) {
		textedit_mem_full( );
		return false;
	}

	if ( 	
			// Typed char does not trigger any reflow
			// At least one character left in the line
			( c != TEXTEDIT_KEY_DEL ) && 
			( c != TEXTSTORE_CHAR_RET ) &&
			( c != TEXTSTORE_CHAR_SPACE ) && 
			( textstore.lsize[lpos] < TEXTSTORE_LINE_SIZE - 1 ) &&
			( // No word straddles the boundary with the line above
				( lpos == TEXTEDIT_TEXT_BASE ) ||
				( textstore.lsize[lpos-1] == 0 ) ||
				( textstore.tlpt[lpos-1][textstore.lsize[lpos-1]-1] == TEXTSTORE_CHAR_SPACE ) ||
				( textstore.tlpt[lpos-1][textstore.lsize[lpos-1]-1] == TEXTSTORE_CHAR_RET )
			) &&
			(
				( // At the end of the text, on a line not closed by a CRLF
					( lpos == textstore.nblines - 1 ) &&
					( cpos == textstore.lsize[lpos] ) &&
					(	( textstore.lsize[lpos] == 0 ) ||
						( textstore.tlpt[lpos][cpos-1] != TEXTSTORE_CHAR_RET )
					)
				) 
				||
				( // Not at the begining nor at the end of the current line
					( cpos > 0 ) && 
					( cpos < textstore.lsize[lpos] - 1 )
				)
			)
		) {
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
			ed_fatal_error( "E3" );
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

	// Count the lines the reformatting below will need. Nothing may be
	// erased before we know the text can be entirely rebuilt.
	// Do this only if the text is nearing its maximum length.
	if ( textstore.nblines >= TEXTSTORE_LINES_MAX - 2 * TEXTEDIT_INSBUFSCAN ) {
		nlines = 1;
		fill = 0;
		wsz = 0;
		for ( cidx = 0; cidx < lbufsz; cidx++ ) {

			// Accumulate the current word
			if ( wsz < TEXTSTORE_LINE_SIZE ) {
				wsz++;
			}

			// End of a word: place it exactly as the loop below would
			if (	( linebuf[cidx] == TEXTSTORE_CHAR_SPACE ) ||
					( linebuf[cidx] == TEXTSTORE_CHAR_RET ) ||
					( wsz == TEXTSTORE_LINE_SIZE ) ||
					( cidx == lbufsz - 1 ) ) {

				// Word does not fit in the current line: open a new one
				if ( wsz + fill > TEXTSTORE_LINE_SIZE ) {
					nlines++;
					fill = 0;
				}
				fill += wsz;

				// A CRLF closes the current line
				if ( linebuf[cidx] == TEXTSTORE_CHAR_RET ) {
					nlines++;
					fill = 0;
				}

				// Reset current word size
				wsz = 0;
			}
		}

		// Refuse the insertion if the lines to be created are not available.
		// Lines lidxstart..lidxstop-1 are reused, the others must be inserted.
		if ( nlines > lidxstop - lidxstart ) {
			if ( 	nlines - ( lidxstop - lidxstart ) > 
					TEXTSTORE_LINES_MAX - textstore.nblines ) {
				textedit_mem_full( );
				return false;
			}
		}
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

	// Remove every scanned line that has not been rewritten. Their content
	// has already been re-emitted above, keeping them would duplicate it.
	while ( lidx < lidxstop - 1 ) {
		textstore_del_line( lidx + 1 );
		lidxstop--;
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

	// If last line is full, insert new line to make room for cursor
	if ( textstore.lsize[textstore.nblines-1] == TEXTSTORE_LINE_SIZE ) {
		if ( textstore_insert_line( textstore.nblines ) ) {
			textedit_mem_full( );
		}
	}

	// Refresh whole screen
	textedit_screen_refresh( );

	return true;
}
