/* ================================================================== *
 * strfmt: minimal text formatting                                    *
 *                                                                    *
 * TED keeps its literals in plain ASCII, which is what the screen     *
 * driver and the .ted files expect. The printf family of cc65 cannot  *
 * be used in that setting: its format parser is assembled with the    *
 * character set of the target, so it looks for the conversion letters *
 * in PETSCII and silently drops every conversion of an ASCII format   *
 * string.                                                             *
 *                                                                    *
 * The few conversions TED needs are therefore done here. None of      *
 * these functions writes a terminating null character: they all       *
 * return the number of characters written, so that the caller can     *
 * chain them and, when a C string is needed, close it itself.         *
 * ================================================================== */

#ifndef __STRFMT_H__
#define __STRFMT_H__

#define STRFMT_NUMBER_BASE		10		// Numbers are printed in decimal
#define STRFMT_ZERO_DIGIT		'0'		// Character of the digit zero
#define STRFMT_MAX_DIGITS		5		// Digits of the widest 16 bit number

uint8_t	strfmt_char		( char*, char, uint8_t );
uint8_t	strfmt_copy		( char*, const char*, uint8_t );
uint8_t	strfmt_number	( char*, uint16_t, uint8_t, uint8_t );
void	strfmt_end		( char*, uint8_t, uint8_t );

#endif /* __STRFMT_H__ */
