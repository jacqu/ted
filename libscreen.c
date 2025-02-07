/* ================================================================== *
 * libscreen: API for printing text on screen                         *
 * ================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "libscreen.h"
#include "ed.h"

uint8_t *libscreen_textbuf = (uint8_t*)LIBSCREEN_BASE_ADDRESS;

// Clear screen
// Duration: 11ms
void libscreen_clear( uint8_t c ) {

	memset( libscreen_textbuf, c, LIBSCREEN_NB_CHARS );
}

// Copy buffer to screen
// Duration: 23ms
void libscreen_copy( uint8_t *b ) {

	memcpy( libscreen_textbuf, b, LIBSCREEN_NB_CHARS );
}

// Clear line
void libscreen_clearline( uint8_t line, uint8_t c ) {
	
	// Check if line is valid
	#ifdef ED_DEBUG
	if ( line >= LIBSCREEN_NB_LINES ) {
		ed_fatal_error( "INVALID LINE NUMBER IN CLEARLINE" );
	}
	#endif

	memset( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+line*LIBSCREEN_NB_COLS), c, LIBSCREEN_NB_COLS );

}

// Copy line to screen
// Lines are numbered from 0 to 27
// Duration: 16ms (18ms with check activated)
void libscreen_copyline( uint8_t line, uint8_t *b ) {

	// Check if line is valid
	#ifdef ED_DEBUG
	if ( line >= LIBSCREEN_NB_LINES ) {
		ed_fatal_error( "INVALID LINE NUMBER IN COPYLINE" );
	}
	#endif
	
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+line*LIBSCREEN_NB_COLS), b, LIBSCREEN_NB_COLS );
}

// Copy line to screen with flipped charcaters codes inverted bit
// Lines are numbered from 0 to 27
void libscreen_copyline_inv( uint8_t line, uint8_t *b ) {
	uint8_t i;

	// Flip inverted bit
	for ( i = 0; i < LIBSCREEN_NB_COLS; i++ ) {
		b[i] ^= LIBSCREEN_INVERT_BIT;
	}
	
	libscreen_copyline( line, b );
}

// Display current text buffer segment
// Variable offset is the first line number to be displayed
// The status line at the top is unaffected
// Duration : 35ms
void libscreen_display( uint16_t offset, uint8_t **b ) {

	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+LIBSCREEN_NB_COLS), b[offset], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+2*LIBSCREEN_NB_COLS), b[offset+1], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+3*LIBSCREEN_NB_COLS), b[offset+2], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+4*LIBSCREEN_NB_COLS), b[offset+3], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+5*LIBSCREEN_NB_COLS), b[offset+4], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+6*LIBSCREEN_NB_COLS), b[offset+5], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+7*LIBSCREEN_NB_COLS), b[offset+6], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+8*LIBSCREEN_NB_COLS), b[offset+7], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+9*LIBSCREEN_NB_COLS), b[offset+8], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+10*LIBSCREEN_NB_COLS), b[offset+9], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+11*LIBSCREEN_NB_COLS), b[offset+10], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+12*LIBSCREEN_NB_COLS), b[offset+11], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+13*LIBSCREEN_NB_COLS), b[offset+12], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+14*LIBSCREEN_NB_COLS), b[offset+13], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+15*LIBSCREEN_NB_COLS), b[offset+14], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+16*LIBSCREEN_NB_COLS), b[offset+15], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+17*LIBSCREEN_NB_COLS), b[offset+16], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+18*LIBSCREEN_NB_COLS), b[offset+17], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+19*LIBSCREEN_NB_COLS), b[offset+18], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+20*LIBSCREEN_NB_COLS), b[offset+19], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+21*LIBSCREEN_NB_COLS), b[offset+20], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+22*LIBSCREEN_NB_COLS), b[offset+21], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+23*LIBSCREEN_NB_COLS), b[offset+22], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+24*LIBSCREEN_NB_COLS), b[offset+23], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+25*LIBSCREEN_NB_COLS), b[offset+24], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+26*LIBSCREEN_NB_COLS), b[offset+25], LIBSCREEN_NB_COLS );
	memcpy( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+27*LIBSCREEN_NB_COLS), b[offset+26], LIBSCREEN_NB_COLS );
}

// Scroll screen one line downwards
void libscreen_scroll_down( void ) {
	memmove( (uint8_t*)(LIBSCREEN_BASE_ADDRESS+LIBSCREEN_NB_COLS), (uint8_t*)(LIBSCREEN_BASE_ADDRESS), LIBSCREEN_NB_CHARS - LIBSCREEN_NB_COLS );
}