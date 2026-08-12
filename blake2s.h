/* ================================================================== *
 * blake2s: keyed hash used by TED as a message authentication code   *
 *                                                                    *
 * BLAKE2s as specified by RFC 7693, with the key and the digest      *
 * length given at initialisation. TED uses it in two places:         *
 *                                                                    *
 *   - the tag of a file, sixteen bytes computed over everything the  *
 *     file holds with the password as the key, so that a text which  *
 *     does not match its tag has been damaged or opened with the     *
 *     wrong password                                                 *
 *                                                                    *
 *   - the nonce of the encryption, derived from the entropy the      *
 *     machine gathered and from the shape of the text, so that two   *
 *     saves never share one                                          *
 *                                                                    *
 * There is a single hash in flight at a time, exactly as there is a  *
 * single cipher in flight at a time in chacha20, so the state is     *
 * held by the module and not by the caller.                          *
 * ================================================================== */

#ifndef __BLAKE2S_H__
#define __BLAKE2S_H__

#include <stdint.h>

#define BLAKE2S_BLOCK_SZ		64		// Bytes the compression function eats at a time
#define BLAKE2S_HASH_SZ			32		// Bytes of the chaining state
#define BLAKE2S_MAX_KEY_SZ		32		// Longest key the specification allows
#define BLAKE2S_MAX_OUT_SZ		32		// Longest digest the specification allows

/* Sizes TED asks for. The tag is truncated to 128 bits: a forgery has
** to be found by trying, and one chance in 2^128 is out of reach of
** anything, while sixteen bytes in the header of every file is a cost
** that is paid on every save. */
#define BLAKE2S_TAG_SZ			16		// Bytes of the tag stored in a file
#define BLAKE2S_KEY_SZ			32		// Bytes of the key TED derives its keys as

/* Byte of the parameter block layout of the specification: the digest
** length, the key length, a fanout of one and a depth of one, which is
** the sequential mode. */
#define BLAKE2S_PARAM_FANOUT	0x01	// One leaf, the input is not a tree
#define BLAKE2S_PARAM_DEPTH		0x01	// The tree is one node deep

void	blake2s_init	( const uint8_t*, uint8_t, uint8_t );
void	blake2s_update	( const uint8_t*, uint16_t );
void	blake2s_final	( uint8_t* );
void	blake2s_mac		( const uint8_t*, const uint8_t*, uint16_t, uint8_t*, uint8_t );

#endif /* __BLAKE2S_H__ */
