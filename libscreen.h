/* ================================================================== *
 * libscreen: API for printing text on screen                         *
 * ================================================================== */

#ifndef __LIBSCREEN_H__
#define __LIBSCREEN_H__

#include <stdint.h>

#define LIBSCREEN_BASE_ADDRESS				0xBB80	// Base address of the screen in text mode
#define LIBSCREEN_NB_LINES					28		// Number of text lines
#define LIBSCREEN_NB_COLS					40		// Number of text columns
#define LIBSCREEN_NB_CHARS					1120	// Number of chars on the screen

#define LIBSCREEN_SPACE						32		// ASCII code for space char
#define LIBSCREEN_COPYRIGHT					96		// ASCII code for copyright
#define LIBSCREEN_GRAY						126		// ASCII code for gray pattern
#define LIBSCREEN_PLAIN						127		// ASCII code for plain pattern

#define LIBSCREEN_INVERT_BIT				0x80	// Bit controlling ink/paper color inversion

/* The console is the print routine of the ROM, reached through          *
 * libconsole.s, and not the one of cc65: the two do not wrap a line at   *
 * the same place, and the frame of the start-up screen is drawn by       *
 * letting its lines wrap on their own. The printf family used to reach   *
 * the very same routine of the ROM.                                      */
#define LIBSCREEN_NEWLINE					'\n'	// Asks for the next line
#define LIBSCREEN_RETURN					'\r'	// Asks for the first column

#define LIBSCREEN_BLACK_INK					0
#define LIBSCREEN_RED_INK					1
#define LIBSCREEN_GREEN_INK					2
#define LIBSCREEN_YELLOW_INK				3
#define LIBSCREEN_BLUE_INK					4
#define LIBSCREEN_MAGENTA_INK				5
#define LIBSCREEN_CYAN_INK					6
#define LIBSCREEN_WHITE_INK					7

#define LIBSCREEN_BLACK_PAPER				16
#define LIBSCREEN_RED_PAPER					17
#define LIBSCREEN_GREEN_PAPER				18
#define LIBSCREEN_YELLOW_PAPER				19
#define LIBSCREEN_BLUE_PAPER				20
#define LIBSCREEN_MAGENTA_PAPER				21
#define LIBSCREEN_CYAN_PAPER				22
#define LIBSCREEN_WHITE_PAPER				23

#define LIBSCREEN_PLAIN_CHAR				160

#define LIBSCREEN_CLS						12

extern uint8_t *libscreen_textbuf;

void libscreen_clear( uint8_t );
void	libscreen_console_putc	( char );
void	libscreen_console_puts	( const char* );
void libscreen_copy( uint8_t* );
void libscreen_clearline( uint8_t, uint8_t );
void libscreen_copyline( uint8_t, uint8_t* );
void libscreen_copyline_inv( uint8_t, const uint8_t* );
void libscreen_display( uint16_t, uint8_t** );
void libscreen_scroll_down( void );

#endif /* __LIBSCREEN_H__ */