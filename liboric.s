; ======================================================================
; liboric assembly code
; Based on the original idea from: 
; https://github.com/iss000/oricOpenLibrary/tree/main/lib-basic
; ======================================================================

	.setcpu		"6502"
	.smart		on
	.autoimport	on
	.case		on
	.debuginfo	on
	.macpack	longbranch
	.export		_basic_asm
	.export		_basic_s_asm

; ----------------------------------------------------------------------
; Defines
; ----------------------------------------------------------------------
.define	MAX_CMD_SIZE	40			; Max length of a BASIC command (with NULL terminating)

; ----------------------------------------------------------------------
; Constants
; ----------------------------------------------------------------------
atmos_exec    	=   $c4bd
oric1_exec    	=   $c4cd
errvec			=	$c01d			; Sedoric error vector 
basic_ret		=	$001b			; Display "Ready" vector
inst_buf		=	$0035			; Basic instruction buffer
overlay_sw		=	$0477			; Switch overlay RAM
oric1_detect	=	$59
atmos_detect	=	$cc
address_detect	=	$c001

; ----------------------------------------------------------------------
; Variables
; ----------------------------------------------------------------------
basic_ret_bak: 	.byt   0,0			; Backup of "Ready" vector
errvec_bak:		.byt   0,0			; Backup of Sedoric error vector
_basic_s_asm:	.byt   0,0			; Basic command parameter

; ----------------------------------------------------------------------
; Command execution code
; ----------------------------------------------------------------------
_basic_asm:	lda   basic_ret			; Backup of the "Ready" vector
            sta   basic_ret_bak		;
            lda   basic_ret+1		;
            sta   basic_ret_bak+1	;

            lda   #<nbasic_ret		; Replacement of the "Ready" vector
            sta   basic_ret			;
            lda   #>nbasic_ret		;
            sta   basic_ret+1		;

			jsr   overlay_sw		; Switch to overlay RAM

			lda   errvec			; Backup of the Sedoric error vector
            sta   errvec_bak		;
            lda   errvec+1			;
            sta   errvec_bak+1		;

            lda   #<nseder_ret		; Replacement of the Sedoric error vector
            sta   errvec			;
            lda   #>nseder_ret		;
            sta   errvec+1			;

			jsr   overlay_sw		; Switch back to ROM

            lda   _basic_s_asm		; Copy the command string address to zero page
            sta   $00				;
            lda   _basic_s_asm+1	;
            sta   $00+1				;

            ldy   #$00				; Copy the command string to instruction buffer
loop:       lda   ($00),y			;
            sta   inst_buf,y		;
            beq   endloop			;
            iny						;
            cpy   #MAX_CMD_SIZE		;
            bcc   loop				;
endloop:	ldx   #<(inst_buf-1)	; x and y registers define pointer to next command - 1
            ldy   #>(inst_buf-1)

            lda   address_detect	; Check if ROM 1.0 or ROM 1.1
			cmp   #oric1_detect		;
			bne	  is_atmos			;
			jmp   oric1_exec		; Execute command with BASIC 1.0 interpreter
is_atmos:	jmp   atmos_exec		; Execute command with BASIC 1.1 interpreter

; ----------------------------------------------------------------------
; Return form BASIC code
; ----------------------------------------------------------------------
nbasic_ret: lda   basic_ret_bak		; Restore the "Ready" vector
            sta   basic_ret			;
            lda   basic_ret_bak+1	;
            sta   basic_ret+1		;

			jsr   overlay_sw		; Switch to overlay RAM

			lda   errvec_bak		; Restore the Sedoric error vector
            sta   errvec			;
            lda   errvec_bak+1		;
            sta   errvec+1			;

			jsr   overlay_sw		; Switch back to ROM

            pla						; Synchronize the stack
            pla						;

            rts						; Return to C

; ----------------------------------------------------------------------
; Return form SEDORIC error code
; ----------------------------------------------------------------------
nseder_ret: rts						; Return without processing error
