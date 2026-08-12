/* ================================================================== *
 * strfmt: minimal text formatting                                    *
 * ================================================================== */

#include <stdint.h>
#include "strfmt.h"

// Write one character
// Returns the number of characters written, zero when there is no room
uint8_t strfmt_char( char *dst, char c, uint8_t max ) {

	if ( !max ) {
		return 0;
	}

	dst[0] = c;
	return 1;
}

// Copy a null terminated string, without its terminator
// Returns the number of characters written
uint8_t strfmt_copy( char *dst, const char *src, uint8_t max ) {
	uint8_t	i = 0;

	while ( ( i < max ) && ( src[i] != 0 ) ) {
		dst[i] = src[i];
		i++;
	}

	return i;
}

// Write an unsigned number, right aligned in a field of the given width
// and padded with zeros. A number too wide for the field is truncated on
// its left, exactly like the "%0Nu" conversion would do.
// A field which does not fit in the room left is not written at all,
// rather than written partly: the other two functions here stop at the
// room they are given, and this one would otherwise be the only way of
// walking past the end of a buffer
// Returns the number of characters written
uint8_t strfmt_number( char *dst, uint16_t value, uint8_t digits, uint8_t max ) {
	char	buffer[STRFMT_MAX_DIGITS];
	uint8_t	i = 0;
	uint8_t	j;

	if ( digits > max ) {
		return 0;
	}

	// Produce the digits, least significant one first
	do {
		buffer[i] = (char)( STRFMT_ZERO_DIGIT + ( value % STRFMT_NUMBER_BASE ) );
		value /= STRFMT_NUMBER_BASE;
		i++;
	} while ( ( value ) && ( i < STRFMT_MAX_DIGITS ) );

	// Pad the field with leading zeros
	for ( j = i; j < digits; j++ ) {
		dst[digits-1-j] = STRFMT_ZERO_DIGIT;
	}

	// Copy the digits back in the reading order
	for ( j = 0; ( j < i ) && ( j < digits ); j++ ) {
		dst[digits-1-j] = buffer[j];
	}

	return digits;
}

// Close a string built with the functions above
// The length is where the last of them stopped, which may be the whole
// size of the buffer: a text that filled it completely would otherwise
// be terminated one byte past its end
void strfmt_end( char *dst, uint8_t length, uint8_t size ) {

	if ( length >= size ) {
		length = size - 1;
	}

	dst[length] = 0;
}
