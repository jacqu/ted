/* ================================================================== *
 * textedit: higher-level API for managing text edition               *
 * ================================================================== */

#ifndef __TEXTEDIT_H__
#define __TEXTEDIT_H__

#include "blake2s.h"

// Special key codes
#define TEXTEDIT_ARROW_UP			11
#define TEXTEDIT_ARROW_DOWN			10
#define TEXTEDIT_ARROW_RIGHT		9
#define TEXTEDIT_ARROW_LEFT			8
#define TEXTEDIT_KEY_ESC			27
#define TEXTEDIT_KEY_DEL			127
#define TEXTEDIT_KEY_RET			13
#define TEXTEDIT_KEY_CIRCUMFLEX		94
#define TEXTEDIT_KEY_POUND			95
#define TEXTEDIT_CTRL_A				1		// YELLOW PAPER
#define TEXTEDIT_CTRL_B				2		// PAGE DOWN
#define TEXTEDIT_CTRL_C				3		// COPY
#define TEXTEDIT_CTRL_D				4		// BLUE PAPER
#define TEXTEDIT_CTRL_E				5		// GREEN INK
#define TEXTEDIT_CTRL_F				6		// PAGE UP
#define TEXTEDIT_CTRL_G				7		// GUIDE
#define TEXTEDIT_CTRL_H				8		// LEFT
#define TEXTEDIT_CTRL_I				9		// RIGHT
#define TEXTEDIT_CTRL_J				10		// DOWN
#define TEXTEDIT_CTRL_K				11		// UP
#define TEXTEDIT_CTRL_L				12
#define TEXTEDIT_CTRL_M				13		// RET
#define TEXTEDIT_CTRL_N				14		// SCREEN SAVER ON/OFF
#define TEXTEDIT_CTRL_O				15		// INVERT
#define TEXTEDIT_CTRL_P				16		// PRINT
#define TEXTEDIT_CTRL_Q				17		// WHITE INK
#define TEXTEDIT_CTRL_R				18		// BLUE INK
#define TEXTEDIT_CTRL_S				19		// SAVE
#define TEXTEDIT_CTRL_T				20		// BLACK INK
#define TEXTEDIT_CTRL_U				21		// RED PAPER
#define TEXTEDIT_CTRL_V				22		// PASTE
#define TEXTEDIT_CTRL_W				23		// RED INK
#define TEXTEDIT_CTRL_X				24		// CUT
#define TEXTEDIT_CTRL_Y				25		// BLACK PAPER
#define TEXTEDIT_CTRL_Z				26		// TAB

#define TEXTEDIT_ASCII_MIN			32
#define TEXTEDIT_ASCII_MAX			126

// File structure
// The main file is made of lines of 40 characters
#define TEXTEDIT_TEXT_BASE			0				// Index of the first line of the text
#define TEXTEDIT_LINES_MAX			TEXTSTORE_LINES_MAX
													// Max number of lines in the file

// Screen structure
#define TEXTEDIT_STATUSSCR_BASE		0				// Line of the status
#define TEXTEDIT_EDITORSCR_BASE		1				// First line of the editor in screen
#define TEXTEDIT_EDITORSCR_SZ		27				// Number of lines of the editor
#define TEXTEDIT_EDITORSCR_LAST		27				// Last line of the editor

// UI related defines
#define TEXTEDIT_UI_WAIT_TIME		1				// Wait duration for temporary UI events

/* Fields of the status line, laid out by hand now that the formatting  *
 * is done with strfmt instead of the printf family                     */
#define TEXTEDIT_STATUS_TEXT_BASE	1				// First column after the paper attribute
#define TEXTEDIT_STATUS_NAME_WIDTH	13				// Width of the file name field
#define TEXTEDIT_STATUS_PERCENT_DIGITS 3			// Width of the memory usage field
#define TEXTEDIT_STATUS_PERCENT_FULL 100			// Value standing for a full memory
#define TEXTEDIT_STATUS_PERCENT_SIGN '%'			// Character following the memory usage
#define TEXTEDIT_STATUS_SAVED_MARK	' '				// Mark of a file saved on disk
#define TEXTEDIT_STATUS_EDITED_MARK	'*'				// Mark of a file edited since last save
#define TEXTEDIT_STATUS_HELP_HINT	"[CTRL]-G>>HELP"
													// Reminder shown at the end of the line
#define TEXTEDIT_STATUS_GUIDE_TITLE	"          U S E R    G U I D E          "
													// Title of the help screen

/* Pieces the Sedoric commands are built out of */
#define TEXTEDIT_LOAD_COMMAND		"LOAD\""		// Read a file back at a given address
#define TEXTEDIT_LOAD_ADDRESS		"\",A"			// Where it has to land
#define TEXTEDIT_LOAD_SUFFIX		",N"			// Ignore the address written in the file
#define TEXTEDIT_SAVE_COMMAND		"SAVEU\""		// Write a memory area out
#define TEXTEDIT_SAVE_END			",E"			// Last address of the area
#define TEXTEDIT_UNPROT_COMMAND		"UNPROT\""		// Take the protection off a file
#define TEXTEDIT_COMMAND_QUOTE		'"'				// Closing quotation mark of a file name
#define TEXTEDIT_ADDRESS_DIGITS		5				// Digits of a sixteen bit address
#define TEXTEDIT_SCREENSAVER_TO		300000			// Screensaver timeout (2500 = 1 second)
#define TEXTEDIT_UI_YES_ANSWER		'y'				// Character for the YES answer
#define TEXTEDIT_UI_NO_ANSWER		'n'				// Character for the NO answer
#define TEXTEDIT_UI_CA_ANSWER		'c'				// Character for the CANCEL answer
#define TEXTEDIT_UI_ANSWER_PREFIX	" ("			// Opens the list of accepted answers
#define TEXTEDIT_UI_ANSWER_SEPARATOR '/'			// Between two of them
#define TEXTEDIT_UI_ANSWER_SUFFIX	")"				// Closes the list

// Char redefinition
#define TEXTEDIT_ORIC_BASE_CHARSET	0xB400			// Char definition base address
#define TEXTEDIT_ORIC_CHARS_HEIGHT	8				// Char height in pixel
#define TEXTEDIT_RET_CHAR_ADDRESS	(TEXTEDIT_ORIC_BASE_CHARSET+TEXTSTORE_CHAR_RET*TEXTEDIT_ORIC_CHARS_HEIGHT)

// Misc
#define TEXTEDIT_CANCEL				2				// Cancel return code
#define TEXTEDIT_TABSZ				4				// Tab size
#define TEXTEDIT_INSBUFSCAN			3				// Number of lines to be scanned for one insertion

// Screen saver defines
#define TEXTEDIT_SC_LEADER_BIT		0x80			// Head pixel of a stripe
#define TEXTEDIT_SC_COUNT_MASK		0x7F			// Bits holding the counter

// Globals
extern uint8_t 						textedit_cur_y;	// Vertical cursor position on the screen. First line is 0.
extern uint16_t						textedit_lpntr;	// Current line position in the scrolled text
extern uint8_t 						textedit_cur_x;	// Horizontal cursor position on the screen. First col is 0.
extern char							textedit_status[LIBSCREEN_NB_COLS+1];
													// Status line buffer
extern uint32_t 					textedit_sc_counter;
													// Screen saver counter
extern bool							textedit_sc_enable;
													// Screen saver flag

// Function prototypes
void	textedit_ret_bak		( void );
void 	textedit_ret_redef		( void );
void 	textedit_ret_restore	( void );
void 	textedit_ret_blank		( void );
void 	textedit_exit			( void );
void 	textedit_init			( char*, char* );
void 	textedit_status_print	( char* );
void 	textedit_status_popup	( char* );
uint8_t	textedit_status_YN		( char* );
void 	textedit_mem_full		( void );
/* ------------------------------------------------------------------ *
 * Entropy                                                            *
 *                                                                    *
 * The free running counters of the machine, read at the moment a     *
 * text is saved, carry far less unpredictability than their size     *
 * suggests: they are read at nearly the same point after a keystroke *
 * every time. A nonce drawn from them alone would repeat sooner than *
 * anyone would like, and a repeated nonce with a tag in the file is  *
 * much worse than a repeated nonce without one.                      *
 *                                                                    *
 * So the counters are not read once at the end but sampled at every  *
 * single keystroke of the session and piled into a pool. What is     *
 * unpredictable there is not the value of a counter but the instant  *
 * the user happened to press a key, and a session offers hundreds of *
 * those. The pool is never used as it stands: it is a heap of        *
 * samples, and the mixing is left to BLAKE2s at the moment the nonce *
 * is derived from it.                                                *
 * ------------------------------------------------------------------ */
#define TEXTEDIT_ENTROPY_SZ			6				// Bytes a single sample carries
#define TEXTEDIT_POOL_SZ			32				// Bytes of the pool the samples pile into
#define TEXTEDIT_POOL_MASK			31				// Wraps an index around the pool

/* Labels giving each use of the password its own key. The same password *
 * must never be handed to two different algorithms as it stands, so the *
 * cipher, the tag and the nonce each get a key of their own, derived    *
 * from it and from nothing else.                                        */
#define TEXTEDIT_KEY_SZ				BLAKE2S_KEY_SZ	// Bytes of a derived key
#define TEXTEDIT_CIPHER_LABEL		"ted cipher key 2"
													// Label of the key that encrypts
#define TEXTEDIT_TAG_LABEL			"ted tag key 2"	// Label of the key that authenticates
#define TEXTEDIT_NONCE_LABEL		"ted nonce key 2"
													// Label of the key that draws nonces

void 	textedit_update_nonce	( uint16_t );
void 	textedit_entropy_stir	( void );
void 	textedit_keys_derive	( void );
void 	textedit_event			( uint8_t );
void 	textedit_screen_refresh	( void );
void	textedit_status_refresh	( void );
void	textedit_cursor_refresh	( void );
void 	textedit_screensaver	( void );
void	textedit_adjust_cursor	( void );
bool 	textedit_insert			( uint16_t, uint8_t, uint8_t );

#endif /* __TEXTEDIT_H__ */