/* ================================================================== *
 * liboric code                                                       *
 * ================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "liboric.h"

char liboric_cmd[LIBORIC_MAX_CMD_SIZE];

char *sedoric_errors[SEDORIC_MAX_ERROR+1] = {
	"NO ERROR",
	"FILE NOT FOUND",
	"DRIVE NOT IN LINE",
	"INVALID FILE NAME",
	"DISK I/O",
	"WRITE PROTECTED",
	"WILDCARD(S) NOT ALLOWED",
	"FILE ALREADY EXISTS",
	"DISK FULL",
	"ILLEGAL QUANTITY",
	"SYNTAX",
	"UNKNOWN FORMAT",
	"TYPE MISMATCH",
	"FILE TYPE MISMATCH",
	"FILE NOT OPEN",
	"FILE ALREADY OPEN",
	"END OF FILE",
	"BAD RECORD NUMBER",
	"FIELD OVERFLOW",
	"STRING TOO LONG",
	"UNKNOW'N FIELD NAME"
};

void liboric_print_error( void ) {

	fprintf( stderr, "?%s ERROR\n", sedoric_errors[liboric_error_nd()] );
}

char* liboric_error_msg( void ) {
	unsigned char *pt;

	pt = (unsigned char*)(SEDORIC_LAST_ERROR_CODE);

	if ( *pt > SEDORIC_MAX_ERROR )
		return sedoric_errors[SEDORIC_NO_ERROR];
	
	return sedoric_errors[*pt];
}

unsigned char liboric_error_nd( void ) {
	unsigned char *pt;

	pt = (unsigned char*)(SEDORIC_LAST_ERROR_CODE);

	if ( *pt > SEDORIC_MAX_ERROR )
		return SEDORIC_NO_ERROR;
	
	return *pt;
}

void liboric_basic( char *str ) {

	if ( strlen( str ) > LIBORIC_MAX_CMD_SIZE ) {
		fprintf( stderr, "ERROR: LIBORIC COMMAND TOO LONG\n" );
		exit( -1 );
	}

	basic_s_asm = str;
	basic_asm( ); 
	}