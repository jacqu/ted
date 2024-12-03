#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include "liboric.h"
#include "chacha20.h"
#include "libscreen.h"
#include "textstore.h"
#include "textedit.h"
#include "ed.h"

#define ED_ARG_ADDRESS			0x35
#define ED_ARG_SEPARATOR		'\''
#define ED_ARG_MAX_LENGTH		14
#define ED_PW_ASCII_MIN			33
#define ED_PW_ASCII_MAX			126
#define ED_PW_DELAY				40
#define ED_TIMER_ADDRESS		0x276
#define ED_TIMER_FREQ			100
#define ED_TIMER_MAX			0xFFFF
#define ED_CAPS_ADDRESS			0x26A
#define ED_CAPS_UPPER			0
#define ED_CAPS_LOWER			16
#define ED_PRINT_BLUE_PAPER		"PRINT CHR$(27);CHR$(84);"

uint16_t 	*ed_timer_a = (uint16_t*)ED_TIMER_ADDRESS;
uint16_t 	ed_timer = 0;

// Initialize timer
void ed_init_timer( void ) {

	ed_timer = *ed_timer_a;
}

// Get time in 1/100th seconds
// Note: timer counts downwards
uint16_t ed_get_timer( void ) {
	uint16_t current;

	// Get current time
	current = *ed_timer_a;

	if ( current <= ed_timer ) {
		return ed_timer - current;
	}
	else {
		// Here we suppose a wrapping of the timer
		return ED_TIMER_MAX - ( current - ed_timer );
	}
}

// Get filename from argument
char* ed_get_filename( void ) {
	char 		*arg = (char*)ED_ARG_ADDRESS;
	uint8_t 	i, j = 0;
	bool 		flag = false;
	static char filename[ED_FN_MAX_LENGTH+1];

	for ( i = 0; i < ED_ARG_MAX_LENGTH; i++ ) {
		// Find the first quote
		if ( ( arg[i] == ED_ARG_SEPARATOR ) && ( !flag ) ) {
			flag = true;
			continue;
		}
		// Find the second quote
		if ( ( arg[i] == ED_ARG_SEPARATOR ) && ( flag ) ) {
			flag = false;
			continue;
		}
		// If first quote found, store the filename
		if ( flag ) {
			// If zero char found in arg, exit from loop
			if ( !arg[i] ) {
				break;
			}
			// Store next character
			filename[j++] = arg[i];
			// If filename length is maxed out, exit from loop
			if ( j == ED_FN_PREFIX_LEN ) {
				break;
			}
		}
	}
	
	// Return pointer to filename if found
	if ( j ) {
		// Add suffix to filename including trailing null character
		strncpy( &filename[j], ED_FN_SUFFIX, ED_FN_SUFFIX_LEN + 1 );
		return filename;
	}
	else {
		return NULL;
	}
}

// Get password from user
char* ed_get_password( void ) {
	static char			password[ED_PW_MAX_LENGTH+1];
	register uint8_t 	i = 0, j;
	uint8_t				c;

	// Initialize password string
	password[0] = 0;

	// Display prompt
	printf( "pwd: " );

	// Get characters
	while ( 1 ) {
		
		c = cgetc( );

		// End password entry with RET
		if ( c == TEXTEDIT_KEY_RET ) {
			password[i] = 0;
			printf( "\n" );
			break;
		}

		// Handle delete key
		if ( c == TEXTEDIT_KEY_DEL ) {
			if ( i ) {
				i--;
				password[i] = 0;
				printf( "%c", c );
			}
			else {
				atmos_ping( );
			}
			continue;
		}

		// Limit password character range
		if ( ( c >= ED_PW_ASCII_MIN ) && ( c <= ED_PW_ASCII_MAX ) ) {
			if ( i < ED_PW_MAX_LENGTH ) {
				password[i++] = c;
				printf( "*" );
			}
			else {
				atmos_ping( );
			}
		}
	}

	// Return password if needed
	if ( i ) {
		// The password should be exactly 32 chars long, padding
		for ( j = i; j < ED_PW_MAX_LENGTH; j++ ) {
			password[j] = password[j-i];
		}
		password[j] = 0;
		return password;
	}
	else {
		return NULL;
	}
}

int main( void ) {
	char *filename;
	char *password;
	static char ed_blue_paper[] = ED_PRINT_BLUE_PAPER;

	// Get file name from argument
	filename = ed_get_filename( );

	// If no argument, display a little help
	
	if ( !filename ) {
		liboric_basic( ed_blue_paper );
		printf( "                < ED >\n\n" );
		liboric_basic( ed_blue_paper );
		printf( "usage: ed 'file'\n" );
		liboric_basic( ed_blue_paper );
		printf( "\n" );
		liboric_basic( ed_blue_paper );
		printf( " > file is lowercase w/o extension\n" );
		liboric_basic( ed_blue_paper );
		printf( " > blank password == no encryption\n" );
		return true;
	}

	// Get the password
	password = ed_get_password( );

	// Initialize textedit subsystem
	textedit_init( filename, password );

	// Main event loop
	while ( 1 ) {
		textedit_event( cgetc( ) );
	}

	return false;
}
