/* ================================================================== *
 * ed: Some global definition of the main file                        *
 * ================================================================== */

#ifndef __ED_H__
#define __ED_H__

// Master debug switch: toggle on to activate sanity checks
#define ED_DEBUG
//#define ED_VERBOSE							// Toggle verbose outputs

// Usefull macros
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// Global defines
/* Reading the LOW counter of a 6522 timer acknowledges that timer's
** interrupt: the flag is cleared as a side effect of the read. The ROM
** drives its own interrupt from timer 1, and that interrupt is what
** scans the keyboard and advances the timer below, so reading $0304 or
** $0308 steals an interrupt from the ROM whenever the read falls between
** the timer expiring and the handler acknowledging it. The high counters
** and the timer the ROM keeps in memory carry the same unpredictability
** and have no side effect at all, so those are the ones sampled. */
#define ED_ORIC_VIA_T1_HIGH		0x305		//	Timer 1 counter of the VIA, high byte
#define ED_ORIC_VIA_T2_HIGH		0x309		//	Timer 2 counter of the VIA, high byte
#define ED_ORIC_ULA_TIM			0x276		//	Three byte timer advanced by the ROM
#define ED_ORIC_ULA_TIM_SZ		3			//	Bytes of that timer

/* Pieces of the messages printed on the console, now that they are put *
 * together by hand rather than by the printf family                    */
#define ED_PANIC_PREFIX			"PANIC: "			// Header of a fatal error message
#define ED_PANIC_SEPARATOR		":"					// Between the file and the line number
#define ED_PANIC_NUMBER_WIDTH	5					// Digits of a source line number
#define ED_VERSION_PREFIX		"Version "			// Header of the version line
#define ED_FN_PREFIX_LEN		9
#define ED_FN_SUFFIX			".ted"
#define ED_FN_SUFFIX_LEN		4
#define ED_FN_MAX_LENGTH		(ED_FN_PREFIX_LEN+ED_FN_SUFFIX_LEN)
#define ED_PW_MAX_LENGTH		CHACHA_KEY_SZ
#define ED_FATAL_ERROR			-1
#define ED_NO_ERROR				0

// Primitives of public functions
#ifdef ED_VERBOSE
void ed_fatal_error	( char*, uint32_t );
#else
void ed_fatal_error	( char* );
#endif

#endif /* __ED_H__ */