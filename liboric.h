/* ================================================================== *
 * liboric header                                                     *
 * ================================================================== */

#ifndef __LIBORIC_H__
#define __LIBORIC_H__

#define LIBORIC_MAX_CMD_SIZE					40

#define SEDORIC_LAST_ERROR_CODE					0x4FD

// Error codes
/*
	1.	FILE_NOT_FOUND - Address CDBF
	2.	DRIVE_NOT_IN_LINE - Address CDCD
	3.	INVALID_FILE_NAME - Address CDDE
	4.	DISK_I/O - Address CDEE
	5.	WRITE_PROTECTED - Address CDF7
	6.	WILDCARD(S)_NOT_ALLOWED - Address CE06
	7.	FILE_ALREADY_EXISTS - Address CE1D
	8.	DISK_FULL - Address CE30
	9.	ILLEGAL_QUANTITY - Address CE39
	10.	SYNTAX - Address CE49
	11.	UNKNOWN_FORMAT - Address CE4F
	12.	TYPE_MISMATCH - Address CE5E
	13.	FILE_TYPE_MISMATCH - Address CE6B
	14.	FILE_NOT_OPEN - Address CE7D
	15.	FILE_ALREADY_OPEN - Address CE8A
	16. END_OF_FILE - Address CE9B
	17. BAD_RECORD_NUMBER - Address CEA6
	18. FIELD_OVERFLOW - Address CEB7
	19. STRING_TOO_LONG - Address CEC5
	20. UNKNOW'N_FIELD_NAME - Address CED4
*/

#define SEDORIC_NO_ERROR						0
#define SEDORIC_FILE_NOT_FOUND_ERROR			1
#define SEDORIC_DRIVE_NOT_IN_LINE_ERROR			2
#define SEDORIC_INVALID_FILE_NAME_ERROR			3
#define SEDORIC_DISK_I_O_ERROR					4
#define SEDORIC_WRITE_PROTECTED_ERROR			5
#define SEDORIC_WILDCARD_NOT_ALLOWED_ERROR		6
#define SEDORIC_FILE_ALREADY_EXISTS_ERROR		7
#define SEDORIC_DISK_FULL_ERROR					8
#define SEDORIC_ILLEGAL_QUANTITY_ERROR			9
#define SEDORIC_SYNTAX_ERROR					10
#define SEDORIC_UNKNOWN_FORMAT_ERROR			11
#define SEDORIC_TYPE_MISMATCH_ERROR				12
#define SEDORIC_FILE_TYPE_MISMATCH_ERROR		13
#define SEDORIC_FILE_NOT_OPEN_ERROR				14
#define SEDORIC_FILE_ALREADY_OPEN_ERROR			15
#define SEDORIC_END_OF_FILE_ERROR				16
#define SEDORIC_BAD_RECORD_NUMBER_ERROR			17
#define SEDORIC_FIELD_OVERFLOW_ERROR			18
#define SEDORIC_STRING_TOO_LONG_ERROR			19
#define SEDORIC_UNKNOWN_FIELD_NAME_ERROR		20

#define SEDORIC_MAX_ERROR						20

extern char 	liboric_cmd[LIBORIC_MAX_CMD_SIZE];					
												// Commande buffer

extern char* 	basic_s_asm;					// Commande buffer of the assembly code
extern void 	basic_asm( void );				// Main function of the assembly code

void			liboric_print_error( void );	// Print last error in plain text
char*			liboric_error_msg( void );		// Returns pointer to last error message
unsigned char	liboric_error_nd( void );		// Returns last SEDORIC error		
void 			liboric_basic( char* );			// Execute command

#endif /* __LIBORIC_H__ */
