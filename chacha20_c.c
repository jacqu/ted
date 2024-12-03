#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "chacha20.h"

// Encryption and decryption algorithm
// Throughput: 952 bytes per second
// 		data: 	main data buffer of clear text of encrypted text 
// 		length: length of main data buffer
// 		key:	pointer to the 256 bits password
//		nonce:	pointer to the 96 bits nonce
//		count:	initial value of the block counter
void chacha_process( uint8_t* data, uint16_t length, uint8_t* key, uint8_t* nonce, uint16_t count ) {

	// Copy parameters to variables shared with assembly code
	chacha_data_pointer = data;
	chacha_data_length = length;
	chacha_key_pointer = key;
	chacha_nonce_pointer = nonce;
	chacha_count = count;

	// Call assembly code
	chacha_asm( );
}