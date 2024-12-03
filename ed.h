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
#define ED_FN_PREFIX_LEN		9
#define ED_FN_SUFFIX			".cha"
#define ED_FN_SUFFIX_LEN		4
#define ED_FN_MAX_LENGTH		(ED_FN_PREFIX_LEN+ED_FN_SUFFIX_LEN)
#define ED_PW_MAX_LENGTH		CHACHA_KEY_SZ

#endif /* __ED_H__ */