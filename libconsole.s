; ======================================================================
; libconsole assembly code
;
; Prints one character on the console of the Oric.
;
; Everything the editor writes before it takes the screen over used to go
; through printf, which costs several kilobytes this program no longer
; has. What printf did on this machine, and what has to be done here in
; its place, is not merely to put characters somewhere: it handed every
; one of them to the print routine of the ROM.
;
; That matters more than it looks. The two routines do not lay a line out
; the same way, an Oric keeping the first columns of every line for the
; ink and paper attributes, and they therefore do not wrap at the same
; place. The frame of the start-up screen carries no new line of its own
; and is drawn by letting the lines wrap on their own, so it comes out in
; pieces as soon as they wrap anywhere else.
;
; The other thing the ROM does not do by itself is turn a line feed into
; a new line: it takes the line feed as an instruction to move one line
; down, and the carriage return as an instruction to go back to the first
; column. A new line needs both, which is exactly what the write routine
; of the C library does before handing anything over, and what is done
; here.
; ======================================================================

.setcpu		"6502"
.smart		on
.autoimport	on
.case		on
.debuginfo	on

.export		_libscreen_console_putc

.include	"atmos.inc"

; ----------------------------------------------------------------------
; Defines
; ----------------------------------------------------------------------
.define	CONSOLE_NEWLINE		$0A					; Asks for the next line
.define	CONSOLE_RETURN		$0D					; Asks for the first column

.code

; ----------------------------------------------------------------------
; Print one character
;
; The character arrives in the accumulator, the way cc65 hands over the
; single argument of a function, and the print routine of the ROM expects
; it in the X register.
; ----------------------------------------------------------------------
.proc _libscreen_console_putc

				tax								; Where the ROM looks for it
				cpx #CONSOLE_NEWLINE			; Is a new line being asked for ?
				bne output						;

				jsr PRINT						; It takes a line feed to go down
				ldx #CONSOLE_RETURN				; and a carriage return to go back to
												; the first column
output:			jmp PRINT						; The ROM returns to our caller

.endproc
