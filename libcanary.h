/* ================================================================== *
 * libcanary: watch the memory nobody is supposed to write to         *
 *                                                                    *
 * Built only when TED_CANARY is defined, and meant for hunting a     *
 * fault that shows up rarely and never in the same place twice.      *
 *                                                                    *
 * Two regions are painted with a pattern at start-up and looked at   *
 * again on every screen refresh:                                     *
 *                                                                    *
 *   - the part of the alternate character set the C stack does not   *
 *     reach. The stack lives there and was measured to use about a   *
 *     hundred bytes, so anything below the margin is either the      *
 *     stack going far deeper than it ever has, or somebody else      *
 *     writing where the stack lives. Both are worth knowing.         *
 *                                                                    *
 *   - the bottom of page one, which the processor stack grows down   *
 *     into. The editor was measured to use about thirty bytes of it, *
 *     but the ROM and Sedoric share the same page.                   *
 *                                                                    *
 * A breach is reported as the address of the first byte that         *
 * changed, which usually names the culprit on its own.               *
 * ================================================================== */

#ifndef __LIBCANARY_H__
#define __LIBCANARY_H__

#include <stdint.h>

/* The C stack starts at the top of the alternate character set and    *
 * grows downwards. The margin is what it is allowed to use without    *
 * being reported; the rest of the region is watched.                  */
#define LIBCANARY_STACK_TOP			0xBB80	// Where the C stack starts
#define LIBCANARY_STACK_BASE		0xB800	// Bottom of the alternate character set
#define LIBCANARY_STACK_MARGIN		0x0100	// Room the C stack may use unreported

/* The processor stack grows down through page one. The editor uses    *
 * the top of it, so the bottom is watched.                            */
#define LIBCANARY_PAGE1_BASE		0x0100	// Bottom of the processor stack page
#define LIBCANARY_PAGE1_SIZE		0x0080	// How much of it is watched

#define LIBCANARY_PATTERN			0xA5	// Neither zero nor a plausible address

#define LIBCANARY_INTACT			0		// Returned while nothing has changed

void		libcanary_init	( void );
uint16_t	libcanary_check	( void );

#endif /* __LIBCANARY_H__ */
