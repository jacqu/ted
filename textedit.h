/* ================================================================== *
 * textedit: higher-level API for managing text edition               *
 * ================================================================== */

#ifndef __TEXTEDIT_H__
#define __TEXTEDIT_H__

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
#define TEXTEDIT_SCREENSAVER_TO		300000			// Screensaver timeout (2500 = 1 second)
#define TEXTEDIT_UI_YES_ANSWER		'y'				// Character for the YES answer
#define TEXTEDIT_UI_NO_ANSWER		'n'				// Character for the NO answer
#define TEXTEDIT_UI_CA_ANSWER		'c'				// Character for the CANCEL answer

// Char redefinition
#define TEXTEDIT_ORIC_BASE_CHARSET	0xB400			// Char definition base address
#define TEXTEDIT_ORIC_CHARS_HEIGHT	8				// Char height in pixel
#define TEXTEDIT_RET_CHAR_ADDRESS	(TEXTEDIT_ORIC_BASE_CHARSET+TEXTSTORE_CHAR_RET*TEXTEDIT_ORIC_CHARS_HEIGHT)

// Misc
#define TEXTEDIT_CANCEL				2				// Cancel return code
#define TEXTEDIT_TABSZ				4				// Tab size
#define TEXTEDIT_INSBUFSCAN			3				// Number of lines to be scanned for one insertion

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
void 	textedit_ret_redef		( void );
void 	textedit_ret_restore	( void );
void 	textedit_exit			( void );
void 	textedit_init			( char*, char* );
void 	textedit_status_print	( char* );
void 	textedit_status_popup	( char* );
uint8_t	textedit_status_YN		( char* );
void 	textedit_mem_full		( void );
void 	textedit_update_nonce	( void );
void 	textedit_event			( uint8_t );
void 	textedit_screen_refresh	( void );
void	textedit_status_refresh	( void );
void	textedit_cursor_refresh	( void );
void 	textedit_screensaver	( void );
void	textedit_adjust_cursor	( void );
bool 	textedit_insert			( uint16_t, uint8_t, uint8_t );

#endif /* __TEXTEDIT_H__ */