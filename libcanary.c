/* ================================================================== *
 * libcanary: watch the memory nobody is supposed to write to         *
 *                                                                    *
 * See libcanary.h for what is watched and why.                       *
 * ================================================================== */

#include <string.h>
#include <stdint.h>
#include "libcanary.h"

#ifdef TED_CANARY

/* ------------------------------------------------------------------ *
 * Private functions                                                  *
 * ------------------------------------------------------------------ */

// Look through one region and return the address of the first byte that
// is no longer the pattern, or nothing when the whole region is intact
static uint16_t libcanary_scan( uint16_t base, uint16_t size ) {
	uint8_t		*byte = (uint8_t*)base;
	uint16_t	i;

	for ( i = 0; i < size; i++ ) {
		if ( byte[i] != LIBCANARY_PATTERN ) {
			return (uint16_t)( base + i );
		}
	}

	return LIBCANARY_INTACT;
}

/* ------------------------------------------------------------------ *
 * Public functions                                                   *
 * ------------------------------------------------------------------ */

// Paint both regions
// Called once the machine is running and the C stack is in use, so the
// margin left at the top of the stack region has to be large enough for
// whatever is on the stack at this very moment
void libcanary_init( void ) {

	memset( (uint8_t*)LIBCANARY_STACK_BASE, LIBCANARY_PATTERN,
			LIBCANARY_STACK_TOP - LIBCANARY_STACK_MARGIN - LIBCANARY_STACK_BASE );
	memset( (uint8_t*)LIBCANARY_PAGE1_BASE, LIBCANARY_PATTERN,
			LIBCANARY_PAGE1_SIZE );
}

// Look at both regions
// Returns the address of the first byte that changed, or nothing while
// everything is still as it was painted
uint16_t libcanary_check( void ) {
	uint16_t	where;

	where = libcanary_scan( LIBCANARY_STACK_BASE,
							LIBCANARY_STACK_TOP - LIBCANARY_STACK_MARGIN -
							LIBCANARY_STACK_BASE );
	if ( where != LIBCANARY_INTACT ) {
		return where;
	}

	return libcanary_scan( LIBCANARY_PAGE1_BASE, LIBCANARY_PAGE1_SIZE );
}

#endif /* TED_CANARY */
