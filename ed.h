/* ================================================================== *
 * ed: Some global definition of the main file                        *
 * ================================================================== */

#ifndef __ED_H__
#define __ED_H__

// Master debug switch: toggle on to activate sanity checks
#define ED_DEBUG

// Usefull macros
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// Global defines
#define ED_ORIC_VIA_TIM1		0x304		//  Timer 1 register address of VIA
#define ED_ORIC_VIA_TIM2		0x308		// 	Timer 2 register address of VIA
#define ED_ORIC_ULA_TIM			0x276		//	ULA system timer register
#define ED_FN_PREFIX_LEN		9
#define ED_FN_SUFFIX			".ted"
#define ED_FN_SUFFIX_LEN		4
#define ED_FN_MAX_LENGTH		(ED_FN_PREFIX_LEN+ED_FN_SUFFIX_LEN)
#define ED_PW_MAX_LENGTH		CHACHA_KEY_SZ
#define ED_FATAL_ERROR			-1
#define ED_NO_ERROR				0

// Primitives of public functions
void ed_fatal_error( char*, uint32_t );

#endif /* __ED_H__ */