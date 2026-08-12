/* ================================================================== *
 * blake2s: keyed hash used by TED as a message authentication code   *
 *                                                                    *
 * Everything expensive is in blake2s.s, which mixes one 64 byte block *
 * into the chaining state. What is left here is the bookkeeping       *
 * around it: gathering the input into blocks, counting the bytes fed  *
 * in, and telling the compression function which block is the last    *
 * one.                                                                *
 *                                                                    *
 * The last block is what makes the buffering slightly unusual. It is  *
 * compressed differently from the others, so a full block is never    *
 * compressed as soon as it is complete: it waits until either more    *
 * input arrives, which proves it was not the last, or the caller asks *
 * for the digest.                                                     *
 * ================================================================== */

#include <string.h>
#include <stdint.h>
#include "blake2s.h"

/* ------------------------------------------------------------------ *
 * State shared with the assembly part                                *
 * ------------------------------------------------------------------ */
extern uint8_t	blake2s_h[BLAKE2S_HASH_SZ];			// Chaining state
extern uint8_t	blake2s_m[BLAKE2S_BLOCK_SZ];		// Block being gathered
extern uint8_t	blake2s_t[sizeof(uint32_t)];		// Bytes fed in so far
extern uint8_t	blake2s_f;							// All bits set on the last block
extern uint8_t	blake2s_iv[BLAKE2S_HASH_SZ];		// Initialisation vector
extern void		blake2s_compress( void );			// Mix the block into the state

/* ------------------------------------------------------------------ *
 * Private variables                                                  *
 * ------------------------------------------------------------------ */
#define BLAKE2S_NOT_LAST		0x00				// Value of the flag on an ordinary block
#define BLAKE2S_LAST			0xFF				// Value of the flag on the last one
#define BLAKE2S_PARAM_OFFSET	0					// Word of the state the parameters land in
#define BLAKE2S_PARAM_KEY_SHIFT	8					// Where the key length sits in that word
#define BLAKE2S_PARAM_FAN_SHIFT	16					// Where the fanout sits
#define BLAKE2S_PARAM_DEP_SHIFT	24					// Where the depth sits

static uint16_t	blake2s_rest = 0;					// Bytes waiting in the block
static uint8_t	blake2s_out_size = BLAKE2S_HASH_SZ;	// Bytes of digest the caller asked for

/* ------------------------------------------------------------------ *
 * Private functions                                                  *
 * ------------------------------------------------------------------ */

// Add a number of bytes to the counter of the bytes fed in
// The counter is what makes the same block mix differently depending on
// where it sits in the message. It is kept on four bytes, which is far
// more than any text this editor can hold
static void blake2s_count( uint16_t bytes ) {
	uint32_t	*counter = (uint32_t*)blake2s_t;

	*counter += bytes;
}

// Mix the block that has been gathered into the chaining state
static void blake2s_block( uint8_t last ) {

	blake2s_f = last;
	blake2s_compress( );
}

/* ------------------------------------------------------------------ *
 * Public functions                                                   *
 * ------------------------------------------------------------------ */

// Start a new hash
// The key and the digest length are not fed in as data: they are folded
// into the very first word of the state, which is what makes two hashes
// of the same message with different keys or different lengths unrelated
// A key, when there is one, is then fed in as a whole block of its own
void blake2s_init( const uint8_t *key, uint8_t key_size, uint8_t out_size ) {
	uint32_t	*parameters = (uint32_t*)blake2s_h;
	uint32_t	*counter = (uint32_t*)blake2s_t;

	// The specification allows neither a longer key nor a longer digest,
	// and both are copied without being measured further down: a caller
	// asking for more would be writing past the end of a buffer
	if ( key_size > BLAKE2S_MAX_KEY_SZ ) {
		key_size = BLAKE2S_MAX_KEY_SZ;
	}
	if ( out_size > BLAKE2S_MAX_OUT_SZ ) {
		out_size = BLAKE2S_MAX_OUT_SZ;
	}

	memcpy( blake2s_h, blake2s_iv, BLAKE2S_HASH_SZ );

	*parameters ^=	(uint32_t)out_size						|
					( (uint32_t)key_size	<< BLAKE2S_PARAM_KEY_SHIFT ) |
					( (uint32_t)BLAKE2S_PARAM_FANOUT << BLAKE2S_PARAM_FAN_SHIFT ) |
					( (uint32_t)BLAKE2S_PARAM_DEPTH	<< BLAKE2S_PARAM_DEP_SHIFT );

	*counter = 0;
	blake2s_rest = 0;
	blake2s_out_size = out_size;

	if ( key_size ) {
		// The key occupies a whole block, padded with zeros, and is fed
		// in before anything else
		memset( blake2s_m, 0, BLAKE2S_BLOCK_SZ );
		memcpy( blake2s_m, key, key_size );
		blake2s_rest = BLAKE2S_BLOCK_SZ;
	}
}

// Feed data in
// A block is only mixed in once it is known not to be the last one,
// which is why a block that is merely full is left where it is
void blake2s_update( const uint8_t *data, uint16_t size ) {
	uint16_t	room;

	while ( size ) {

		if ( blake2s_rest == BLAKE2S_BLOCK_SZ ) {
			// The block was full and more is coming, so it was not the
			// last one after all
			blake2s_count( BLAKE2S_BLOCK_SZ );
			blake2s_block( BLAKE2S_NOT_LAST );
			blake2s_rest = 0;
		}

		room = BLAKE2S_BLOCK_SZ - blake2s_rest;
		if ( room > size ) {
			room = size;
		}

		memcpy( &blake2s_m[blake2s_rest], data, room );
		blake2s_rest += room;
		data += room;
		size -= room;
	}
}

// Close the hash and hand the digest over
void blake2s_final( uint8_t *out ) {

	blake2s_count( blake2s_rest );

	// The last block is padded with zeros, which is unambiguous because
	// the number of bytes fed in is mixed in as well
	memset( &blake2s_m[blake2s_rest], 0, BLAKE2S_BLOCK_SZ - blake2s_rest );
	blake2s_block( BLAKE2S_LAST );

	memcpy( out, blake2s_h, blake2s_out_size );
	blake2s_rest = 0;
}

// Tag of a single buffer, which is what TED asks for most of the time
void blake2s_mac( const uint8_t *key, const uint8_t *data, uint16_t size,
				  uint8_t *out, uint8_t out_size ) {

	blake2s_init( key, BLAKE2S_KEY_SZ, out_size );
	blake2s_update( data, size );
	blake2s_final( out );
}
