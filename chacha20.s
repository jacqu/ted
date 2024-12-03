; Benchmark : 952 Bytes / s

.setcpu		"6502"
.smart		on
.autoimport	on
.case		on
.debuginfo	on
.macpack	longbranch
.export		_chacha_data_pointer
.export		_chacha_data_length
.export		_chacha_key_pointer
.export		_chacha_nonce_pointer
.export		_chacha_asm
.export		_chacha_count

; ----------------------------------------------
; Defines
; ----------------------------------------------
.define	CHACHA_BLOCK_SZ	64						; Size of a chacha20 block (bytes)
.define CHACHA_KEY_OF	16						; Offset of the key in block
.define CHACHA_KEY_SZ	32						; Size of a chacha20 key (bytes)
.define CHACHA_CNT_OF	48						; Offset of the counter in block
.define CHACHA_CNT_SZ	4						; Size of a chacha20 counter (bytes)
.define CHACHA_NONCE_OF	52						; Offset of the nonce in block
.define CHACHA_NONCE_SZ	12						; Size of a chacha20 nonce (bytes)
.define CHACHA_ROUNDS	10						; Number of chacha rounds

; ----------------------------------------------
; Global variables
; ----------------------------------------------
_chacha_data_pointer:	.byt   	0,0				; Pointer to the data buffer to encrypt
_chacha_data_length:	.byt	0,0				; Length of the data buffer
_chacha_key_pointer:	.byt	0,0				; Pointer to the encryption key
_chacha_nonce_pointer:	.byt	0,0				; Pointer to the encryption nonce
_chacha_count:			.byt	0,0				; Chacha20 block counter
blk:					.res	CHACHA_BLOCK_SZ	; Data of the chacha20 block (64 bytes)

; ----------------------------------------------
; Constants
; ----------------------------------------------
data_pointer 			=		$00
key_pointer				=		$02
nonce_pointer			=		$04

; ----------------------------------------------
; Macros
; ----------------------------------------------

; CHACHA_SUM32: uint32 addition X=X+Y
; ADR_X: absolute address pointing to first value
; ADR_Y: absolute address pointing to second value
.macro			CHACHA_SUM32 ADR_X,ADR_Y
				clc								; Clear carry
				lda ADR_X						; Load X low byte
				adc ADR_Y						; A = X + Y + C
				sta ADR_X						; Store the result
				lda ADR_X+1						; 
				adc ADR_Y+1						; 
				sta ADR_X+1						; 
				lda ADR_X+2						; 
				adc ADR_Y+2						; 
				sta ADR_X+2						; 
				lda ADR_X+3						; Load X high byte
				adc ADR_Y+3						; A = X + Y + C
				sta ADR_X+3						; Store the result
.endmacro

; CHACHA_XOR32: uint32 bitwise xor X=X^Y
; ADR_X: absolute address pointing to first value
; ADR_Y: absolute address pointing to second value
.macro			CHACHA_XOR32 ADR_X,ADR_Y
				lda ADR_X						; Load X low byte
				eor ADR_Y						; A = X ^ Y
				sta ADR_X						; Store the result
				lda ADR_X+1						; 
				eor ADR_Y+1						; 
				sta ADR_X+1						; 
				lda ADR_X+2						; 
				eor ADR_Y+2						; 
				sta ADR_X+2						; 
				lda ADR_X+3						; Load X high byte
				eor ADR_Y+3						; A = X ^ Y
				sta ADR_X+3						; Store the result
.endmacro

; CHACHA_BLK_ZERO: fill the chacha block with zeros
; ADR: absolute address of the chacha block
.macro			CHACHA_BLK_ZERO ADR				; Add the initial value to each byte in the block
.local			start_zero						; Make this label local to the macro
				ldx #CHACHA_BLOCK_SZ			; Initialize counter
				lda #$0							; Load 0 in accumulator
start_zero:		dex								; Decrement x
				sta ADR,x						; Store 0 at position x from block start address
				bne start_zero					; If index != 0, go to next iteration
.endmacro

; CHACHA_ADD_INIT: add the initial value to the chacha block
; ADR: absolute address of the chacha block
; KEY: indirect address of the key
; NCE: indirect address of the nonce
; CNT: absolute address of the counter
.macro			CHACHA_ADD_INIT ADR,KEY,NCE,CNT	; Initialize the block ar address ADR with zeros
.local			start_key						; Make these labels local to the macro
.local			start_nonce						;
; Initialize constant string
				clc								; Clear carry each 32 bits for uint32 additions
				lda #$65						;
				adc ADR							;
				sta ADR							;
				lda #$78						;
				adc ADR+1						;
				sta ADR+1						;
				lda #$70						;
				adc ADR+2						;
				sta ADR+2						;
				lda #$61						; 
				adc ADR+3						; 
				sta ADR+3						;
				clc								; Clear carry
				lda #$6e						;
				adc ADR+4						;
				sta ADR+4						;
				lda #$64						;
				adc ADR+5						;
				sta ADR+5						;
				lda #$20						;
				adc ADR+6						;
				sta ADR+6						;
				lda #$33						;
				adc ADR+7						;
				sta ADR+7						;
				clc								; Clear carry
				lda #$32						;
				adc ADR+8						;
				sta ADR+8						;
				lda #$2d						;
				adc ADR+9						;
				sta ADR+9						;
				lda #$62						;
				adc ADR+10						;
				sta ADR+10						;
				lda #$79						;
				adc ADR+11						;
				sta ADR+11						;
				clc								; Clear carry
				lda #$74						;
				adc ADR+12						;
				sta ADR+12						;
				lda #$65						;
				adc ADR+13						;
				sta ADR+13						;
				lda #$20						;
				adc ADR+14						;
				sta ADR+14						;
				lda #$6b						;
				adc ADR+15						;
				sta ADR+15						;
; Initialize key
				ldy #0							; Initialize key index
start_key:		clc								; Clear carry
				lda (KEY),y						; Load yth byte of the key
				adc ADR+CHACHA_KEY_OF,y			; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_KEY_OF,y			; Store the result in the chacha block
				iny								; Increment y
				lda (KEY),y						; Load yth byte of the key
				adc ADR+CHACHA_KEY_OF,y			; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_KEY_OF,y			; Store the result in the chacha block
				iny								; Increment y
				lda (KEY),y						; Load yth byte of the key
				adc ADR+CHACHA_KEY_OF,y			; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_KEY_OF,y			; Store the result in the chacha block
				iny								; Increment y
				lda (KEY),y						; Load yth byte of the key
				adc ADR+CHACHA_KEY_OF,y			; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_KEY_OF,y			; Store the result in the chacha block
				iny								; Increment y
				cpy #CHACHA_KEY_SZ				; Compare y with key size
				bne start_key					; If y is not equal to key size, go to next iteration
; Initialize counter
				clc								; Clear carry
				lda CNT							; Load LSB of the block counter
				adc ADR+CHACHA_CNT_OF			; Add this byte to the corresponding byte in the block
				sta ADR+CHACHA_CNT_OF			; Store it in the chacha block at the right position
				lda CNT+1						; Load MSB of the block counter
				adc ADR+CHACHA_CNT_OF+1			; Add this byte to the corresponding byte in the block
				sta ADR+CHACHA_CNT_OF+1			; Store it in the chacha block at the right position
				lda #0							; Load 0 in accumulator
				adc ADR+CHACHA_CNT_OF+2			; In case of carry=1 add it to next byte
				sta ADR+CHACHA_CNT_OF+2			; Store it in the chacha block at the right position
				lda #0							; Load 0 in accumulator
				adc ADR+CHACHA_CNT_OF+3			; In case of carry=1 add it to next byte
				sta ADR+CHACHA_CNT_OF+3			; Store it in the chacha block at the right position
; Initialize nonce
				ldy #0							; Initialize key index
start_nonce:	clc								; Clear carry
				lda (NCE),y						; Load yth byte of the nonce
				adc ADR+CHACHA_NONCE_OF,y		; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_NONCE_OF,y		; Store the result in the chacha block
				iny								; Increment y
				lda (NCE),y						; Load yth byte of the nonce
				adc ADR+CHACHA_NONCE_OF,y		; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_NONCE_OF,y		; Store the result in the chacha block
				iny								; Increment y
				lda (NCE),y						; Load yth byte of the nonce
				adc ADR+CHACHA_NONCE_OF,y		; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_NONCE_OF,y		; Store the result in the chacha block
				iny								; Increment y
				lda (NCE),y						; Load yth byte of the nonce
				adc ADR+CHACHA_NONCE_OF,y		; Add the current key bute to the corresponding block byte
				sta ADR+CHACHA_NONCE_OF,y		; Store the result in the chacha block
				iny								; Increment y
				cpy #CHACHA_NONCE_SZ			; Compare y with nonce size
				bne start_nonce					; If y is not equal to nonce size, go to next iteration
.endmacro

; CHACHA_ROTL: left rotate a uint32 inside a chacha block
; ADR: absolute address inside the chacha block
; NUM: number of rotations
.macro			CHACHA_ROTL ADR,NUM				; NUM left rotation(s) of a uint_32 located at address ADR
.local  		start_rot						; Make these labels local to the macro
.local  		end_rot							;
				ldx #NUM						; Iteration counter initialization						;
start_rot:		clc								; Clear carry
				lda ADR							; Load LSB of buffer in accumulator
				rol a							; Rotate left
				sta ADR							; Store the result of the left rotate operation
				lda ADR+1						; Load next byte of buffer in accumulator
				rol a							; Rotate left
				sta ADR+1						; Store the result of the left rotate operation
				lda ADR+2						; Load next byte of buffer in accumulator
				rol a							; Rotate left
				sta ADR+2						; Store the result of the left rotate operation
				lda ADR+3						; Load MSB of buffer in accumulator
				rol a							; Rotate left
				sta ADR+3						; Store the result of the left rotate operation
				bcc end_rot						; End if carry clear
				lda #1							; Least significant bit has to be set
				ora ADR							; Toggle bit
				sta ADR							; Store the result of the or operation
end_rot:		dex								; Decrement iteration counter
				bne start_rot					; Loop for next iteration if x is not equal to zero
.endmacro

; CHACHA_ROTL8: 8 bits left rotate a uint32 inside a chacha block
; ADR: absolute address inside the chacha block
.macro			CHACHA_ROTL8 ADR				; 8 bits rotation optimization
				ldx ADR+3						; Save high byte in x
				lda ADR+2						; Rotate bytes
				sta ADR+3						;
				lda ADR+1						;
				sta ADR+2						;
				lda ADR							;
				sta ADR+1						;
				stx ADR							;
.endmacro

; CHACHA_ROTL16: 16 bits left rotate a uint32 inside a chacha block
; ADR: absolute address inside the chacha block
.macro			CHACHA_ROTL16 ADR				; 16 bits rotation optimization
				ldx ADR							; Swap low word and high word
				lda ADR+2						; 
				sta ADR							;
				stx ADR+2						;
				ldx ADR+1						;
				lda ADR+3						;
				sta ADR+1						;
				stx ADR+3						;
.endmacro

; CHACHA_ROTL12: 12 bits left rotate a uint32 inside a chacha block
; ADR: absolute address inside the chacha block
.macro			CHACHA_ROTL12 ADR				; 12 bits rotation optimization
				CHACHA_ROTL8 ADR				; 8 bits rotation
				CHACHA_ROTL ADR,4				; 4 bits rotation
.endmacro

; CHACHA_ROTL7: 7 bits left rotate a uint32 inside a chacha block
; ADR: absolute address inside the chacha block
.macro			CHACHA_ROTL7 ADR				; 7 bits rotation optimization
.local  		end_rot							;
				CHACHA_ROTL8 ADR				; 8 bits left rotation
; 1 bit right rotation
				clc								; Clear carry
				lda ADR+3						; Load MSB of buffer in accumulator
				ror a							; Rotate right
				sta ADR+3						; Store the result of the right rotate operation
				lda ADR+2						; Load next byte of buffer in accumulator
				ror a							; Rotate left
				sta ADR+2						; Store the result of the right rotate operation
				lda ADR+1						; Load next byte of buffer in accumulator
				ror a							; Rotate right
				sta ADR+1						; Store the result of the right rotate operation
				lda ADR							; Load LSB of buffer in accumulator
				ror a							; Rotate right
				sta ADR							; Store the result of the right rotate operation
				bcc end_rot						; End if carry clear
				lda #$80						; Most significant bit has to be set
				ora ADR+3						; Toggle bit
				sta ADR+3						; Store the result of the or operation
end_rot:										; End
.endmacro

; CHACHA_QR: chacha quarter round
; ADR_A: absolute address of a
; ADR_B: absolute address of b
; ADR_C: absolute address of c
; ADR_D: absolute address of d
.macro			CHACHA_QR ADR_A,ADR_B,ADR_C,ADR_D
				CHACHA_SUM32 ADR_A,ADR_B		; a += b
				CHACHA_XOR32 ADR_D,ADR_A		; d ^= a
				CHACHA_ROTL16 ADR_D				; CHACHA20_ROTL(d, 16)
				CHACHA_SUM32 ADR_C,ADR_D		; c += d
				CHACHA_XOR32 ADR_B,ADR_C		; b ^= c
				CHACHA_ROTL12 ADR_B				; CHACHA20_ROTL(b, 12)
				CHACHA_SUM32 ADR_A,ADR_B		; a += b
				CHACHA_XOR32 ADR_D,ADR_A		; d ^= a
				CHACHA_ROTL8 ADR_D				; CHACHA20_ROTL(d, 8)
				CHACHA_SUM32 ADR_C,ADR_D		; c += d
				CHACHA_XOR32 ADR_B,ADR_C		; b ^= c
				CHACHA_ROTL7 ADR_B				; CHACHA20_ROTL(b, 7)
.endmacro


; Main code
_chacha_asm:	sei								; Disable interrupts
				lda _chacha_data_pointer		; Copy C argument _chacha_data_pointer in page 0
				sta data_pointer				;
				lda _chacha_data_pointer+1		;
				sta data_pointer+1				;
				lda _chacha_key_pointer			; Copy C argument _chacha_key_pointer in page 0
				sta key_pointer					;
				lda _chacha_key_pointer+1		;
				sta key_pointer+1				;
				lda _chacha_nonce_pointer		; Copy C argument _chacha_nonce_pointer in page 0
				sta nonce_pointer				;
				lda _chacha_nonce_pointer+1		;
				sta nonce_pointer+1				;
start_encrypt:	CHACHA_BLK_ZERO blk				; Fill chacha block with zeros
												; Add the initial value to the chacha block
				CHACHA_ADD_INIT blk,key_pointer,nonce_pointer,_chacha_count
												; Make 20 chacha rounds
				lda #CHACHA_ROUNDS				; Load round counter
start_round:	pha								; Push counter in stack
				CHACHA_QR blk+0*4,blk+4*4,blk+8*4,blk+12*4
				CHACHA_QR blk+1*4,blk+5*4,blk+9*4,blk+13*4
				CHACHA_QR blk+2*4,blk+6*4,blk+10*4,blk+14*4
				CHACHA_QR blk+3*4,blk+7*4,blk+11*4,blk+15*4
				CHACHA_QR blk+0*4,blk+5*4,blk+10*4,blk+15*4
				CHACHA_QR blk+1*4,blk+6*4,blk+11*4,blk+12*4
				CHACHA_QR blk+2*4,blk+7*4,blk+8*4,blk+13*4
				CHACHA_QR blk+3*4,blk+4*4,blk+9*4,blk+14*4
				pla								; Pull counter from stack
				tax								; Transfer a to x
				dex								; Decrement round counter
				beq stop_round					; If counter is equal to zero, finish
				txa								; Transfer x to a
				jmp start_round					; Else, go to next iteration
												; Add the initial value to the chacha block at the end
stop_round:		CHACHA_ADD_INIT blk,key_pointer,nonce_pointer,_chacha_count
; Increment chacha counter
				inc _chacha_count				; Increment block counter low byte
				bne	start_xor					; If not zero, continue
				inc _chacha_count+1				; Increment block counter high byte
; Xor up to 64 next bytes of text
start_xor:		ldy #0							; Initialize block counter
cont_xor2:		lda _chacha_data_length			; Load the low byte of the data length
				ora _chacha_data_length+1		; OR with data length high byte
				beq end_chacha					; If high OR low is zero, encryption is finished
; XOR loop for current 64 bytes block
cont_xor1:		lda blk,y						; Load the yth byte of the block
				eor (data_pointer),y			; XOR with input data
				sta (data_pointer),y			; Store result in input data buffer
; Decrement the 16 bits length counter
				lda _chacha_data_length			; Load length counter low byte into accumulator
				sec								; Set carry for substaction
				sbc #1							; Decrement accumulator
				sta _chacha_data_length			; Store the result in length counter low byte
				bcs cont_xor3					; If carry is set, no need to decrement high byte
				dec _chacha_data_length+1		; Otherwise decrement length counter high byte
; Increment the block index y
cont_xor3:		iny								; Increment y
				cpy #CHACHA_BLOCK_SZ			; Compare with block size
				bne cont_xor2					; If not equal, branch to cont_xor2
; Increment data pointer by 64 and jump to generate a new block
				clc								; Clear carry
				lda #CHACHA_BLOCK_SZ			; Load block size in accumulator
				adc data_pointer				; Increment data pointer low byte
				sta data_pointer				; Store the result
				lda #0							; Load block size high byte in accumulator
				adc data_pointer+1				; Increment data pointer high byte
				sta data_pointer+1				; Store the result
				jmp start_encrypt				; Generate next chacha block
end_chacha:		cli								; Enable interrupts
				rts								; Return to C
