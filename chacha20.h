#ifndef __CHACHA20_H__
#define __CHACHA20_H__

#define CHACHA_BLOCK_SZ	64
#define CHACHA_CST_SZ	16
#define CHACHA_KEY_SZ	32
#define CHACHA_CNT_SZ	4
#define CHACHA_NONCE_SZ	12

extern uint8_t* chacha_data_pointer;
extern uint16_t chacha_data_length;
extern uint8_t* chacha_key_pointer;
extern uint8_t* chacha_nonce_pointer;
extern uint16_t chacha_count;

extern void 	chacha_asm( void );	
void 			chacha_process( uint8_t*, uint16_t, uint8_t*, uint8_t*, uint16_t );

#endif