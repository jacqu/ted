; ======================================================================
; blake2s assembly code
;
; Compression function of BLAKE2s, RFC 7693.
;
; TED uses BLAKE2s as a message authentication code: a file carries a
; tag computed over everything it holds, with the password as the key,
; and a text that does not match its tag has either been damaged or
; opened with the wrong password.
;
; BLAKE2s was chosen because it is built out of exactly the same three
; operations as ChaCha20, which this program already carries in assembly:
; a 32 bit addition, a 32 bit exclusive or, and rotations by 16, 12, 8
; and 7 bits. Its G function is the quarter round of ChaCha20 with the
; rotations turned the other way and two message words folded in, so the
; macros below are the ones of chacha20.s with the rotations reversed.
; Nothing here needs a multiplication, which a 6502 does not have, and
; that is what makes it affordable on this machine where the usual
; companion of ChaCha20, Poly1305, would not be.
;
; Only the compression function lives here, the expensive part being the
; ten rounds it runs over every 64 byte block. The buffering around it,
; which is a matter of moving bytes rather than of mixing them, is in
; blake2s_c.c.
;
; The mixing function is written once and called eighty times per block,
; rather than written out eighty times. Writing it out would run about a
; third faster and would cost five kilobytes, which this program does not
; have: it has to leave room below the screen for a text of three hundred
; and fifty lines. What the single copy costs is the four words it has to
; fetch and put back on every call, since which four they are changes
; from one call to the next.
; ======================================================================

.setcpu		"6502"
.smart		on
.autoimport	on
.case		on
.debuginfo	on
.macpack	longbranch

.export		_blake2s_h
.export		_blake2s_m
.export		_blake2s_t
.export		_blake2s_f
.export		_blake2s_compress
.export		_blake2s_iv

; ----------------------------------------------------------------------
; Defines
; ----------------------------------------------------------------------
.define	BLAKE2S_HASH_SZ		32					; Size of the chaining state (bytes)
.define	BLAKE2S_BLOCK_SZ	64					; Size of a block (bytes)
.define	BLAKE2S_WORD_SZ		4					; Size of a word (bytes)
.define	BLAKE2S_NB_ROUNDS	10					; Rounds of the compression function
.define	BLAKE2S_SIGMA_SZ	16					; Message indices used by one round
.define	BLAKE2S_NB_MIXES	8					; Mixing functions run by one round
.define	BLAKE2S_QUAD_SZ		4					; Words each of them mixes

.define	BLAKE2S_ROTR12_STEPS	4				; Single bit rotations left after eight
.define	BLAKE2S_LAST_BIT	$80					; Highest bit of a byte
.define	BLAKE2S_FIRST_BIT	1					; Lowest bit of a byte

; Offsets of the sixteen working words inside the working state
.define	V0					blake2s_v+0*BLAKE2S_WORD_SZ
.define	V1					blake2s_v+1*BLAKE2S_WORD_SZ
.define	V2					blake2s_v+2*BLAKE2S_WORD_SZ
.define	V3					blake2s_v+3*BLAKE2S_WORD_SZ
.define	V4					blake2s_v+4*BLAKE2S_WORD_SZ
.define	V5					blake2s_v+5*BLAKE2S_WORD_SZ
.define	V6					blake2s_v+6*BLAKE2S_WORD_SZ
.define	V7					blake2s_v+7*BLAKE2S_WORD_SZ
.define	V8					blake2s_v+8*BLAKE2S_WORD_SZ
.define	V9					blake2s_v+9*BLAKE2S_WORD_SZ
.define	V10					blake2s_v+10*BLAKE2S_WORD_SZ
.define	V11					blake2s_v+11*BLAKE2S_WORD_SZ
.define	V12					blake2s_v+12*BLAKE2S_WORD_SZ
.define	V13					blake2s_v+13*BLAKE2S_WORD_SZ
.define	V14					blake2s_v+14*BLAKE2S_WORD_SZ
.define	V15					blake2s_v+15*BLAKE2S_WORD_SZ

; ----------------------------------------------------------------------
; Data shared with the C part
; ----------------------------------------------------------------------
.bss

_blake2s_h:		.res	BLAKE2S_HASH_SZ			; Chaining state, eight words
_blake2s_m:		.res	BLAKE2S_BLOCK_SZ		; Block being compressed, sixteen words
_blake2s_t:		.res	BLAKE2S_WORD_SZ			; Bytes fed in so far
_blake2s_f:		.res	1						; All bits set on the last block

blake2s_v:		.res	BLAKE2S_BLOCK_SZ		; Working state of one compression
blake2s_round:	.res	1						; Rounds left to run

; The four words the mixing function is working on. They are fetched out
; of the working state before it runs and put back afterwards, which is
; what lets one copy of it serve all eight mixes of a round
blake2s_ga:		.res	BLAKE2S_WORD_SZ			; First word being mixed
blake2s_gb:		.res	BLAKE2S_WORD_SZ			; Second word being mixed
blake2s_gc:		.res	BLAKE2S_WORD_SZ			; Third word being mixed
blake2s_gd:		.res	BLAKE2S_WORD_SZ			; Fourth word being mixed

blake2s_sigma_index:
				.res	1						; How far the permutation table is walked
blake2s_quad_index:
				.res	1						; Which of the eight mixes is running

; ----------------------------------------------------------------------
; Macros
; ----------------------------------------------------------------------

; BLAKE2S_SUM32: uint32 addition X=X+Y
; ADR_X: absolute address pointing to the first value
; ADR_Y: absolute address pointing to the second value
.macro			BLAKE2S_SUM32 ADR_X,ADR_Y
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

; BLAKE2S_SUM32_M: uint32 addition X=X+m[X register]
; The message word to fold in changes from one round to the next, so it
; is reached through the X register rather than named. X is left pointing
; at the last byte read
; ADR_X: absolute address pointing to the value to add to
.macro			BLAKE2S_SUM32_M ADR_X
				clc								; Clear carry
				lda ADR_X						; Load X low byte
				adc _blake2s_m,x				; A = X + m + C
				sta ADR_X						; Store the result
				inx								;
				lda ADR_X+1						;
				adc _blake2s_m,x				;
				sta ADR_X+1						;
				inx								;
				lda ADR_X+2						;
				adc _blake2s_m,x				;
				sta ADR_X+2						;
				inx								;
				lda ADR_X+3						; Load X high byte
				adc _blake2s_m,x				; A = X + m + C
				sta ADR_X+3						; Store the result
.endmacro

; BLAKE2S_XOR32: uint32 bitwise xor X=X^Y
; ADR_X: absolute address pointing to the first value
; ADR_Y: absolute address pointing to the second value
.macro			BLAKE2S_XOR32 ADR_X,ADR_Y
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

; BLAKE2S_ROTR16: 16 bits right rotate of a uint32
; Rotating a word by half its width is the same turn either way, so this
; is the very rotation chacha20.s does, under its own name
; ADR: absolute address of the word
.macro			BLAKE2S_ROTR16 ADR
				ldx ADR							; Swap low word and high word
				lda ADR+2						;
				sta ADR							;
				stx ADR+2						;
				ldx ADR+1						;
				lda ADR+3						;
				sta ADR+1						;
				stx ADR+3						;
.endmacro

; BLAKE2S_ROTR8: 8 bits right rotate of a uint32
; A rotation by a whole byte moves bytes and shifts nothing
; ADR: absolute address of the word
.macro			BLAKE2S_ROTR8 ADR
				ldx ADR							; Save low byte in x
				lda ADR+1						; Walk the other three down
				sta ADR							;
				lda ADR+2						;
				sta ADR+1						;
				lda ADR+3						;
				sta ADR+2						;
				stx ADR+3						; The low byte becomes the high one
.endmacro

; BLAKE2S_ROTR1: 1 bit right rotate of a uint32
; The bit leaving the bottom is caught in the carry before anything is
; stored, and is walked back in at the top
; ADR: absolute address of the word
.macro			BLAKE2S_ROTR1 ADR
				lda ADR							; Load the low byte
				lsr a							; Carry takes its lowest bit
				ror ADR+3						; Which comes back in at the top
				ror ADR+2						;
				ror ADR+1						;
				ror ADR							;
.endmacro

; BLAKE2S_ROTL1: 1 bit left rotate of a uint32
; ADR: absolute address of the word
.macro			BLAKE2S_ROTL1 ADR
.local			end_rot							;
				asl ADR							; The lowest bit is left empty
				rol ADR+1						;
				rol ADR+2						;
				rol ADR+3						;
				bcc end_rot						; Nothing left the top
				lda #BLAKE2S_FIRST_BIT			; The bit that did comes back in at
				ora ADR							; the bottom, which asl left clear
				sta ADR							;
end_rot:										;
.endmacro

; BLAKE2S_ROTR12: 12 bits right rotate of a uint32
; ADR: absolute address of the word
.macro			BLAKE2S_ROTR12 ADR
				BLAKE2S_ROTR8 ADR				; Eight of them are a byte move
				BLAKE2S_ROTR1 ADR				; The other four are done one by one
				BLAKE2S_ROTR1 ADR				;
				BLAKE2S_ROTR1 ADR				;
				BLAKE2S_ROTR1 ADR				;
.endmacro

; BLAKE2S_ROTR7: 7 bits right rotate of a uint32
; Turning eight to the right and one back to the left is shorter than
; turning seven to the right
; ADR: absolute address of the word
.macro			BLAKE2S_ROTR7 ADR
				BLAKE2S_ROTR8 ADR				; One byte to the right
				BLAKE2S_ROTL1 ADR				; One bit back to the left
.endmacro

; BLAKE2S_FETCH_WORD: copy one word of the working state into a scratch
; The word to copy is named by an entry of the quad table, which the Y
; register points at, and Y is left on the next entry
; ADR: absolute address of the scratch word
.macro			BLAKE2S_FETCH_WORD ADR
				ldx blake2s_quad,y				; Where that word sits in the state
				iny								;
				lda blake2s_v+0,x				;
				sta ADR+0						;
				lda blake2s_v+1,x				;
				sta ADR+1						;
				lda blake2s_v+2,x				;
				sta ADR+2						;
				lda blake2s_v+3,x				;
				sta ADR+3						;
.endmacro

; BLAKE2S_STORE_WORD: copy a scratch word back into the working state
; ADR: absolute address of the scratch word
.macro			BLAKE2S_STORE_WORD ADR
				ldx blake2s_quad,y				;
				iny								;
				lda ADR+0						;
				sta blake2s_v+0,x				;
				lda ADR+1						;
				sta blake2s_v+1,x				;
				lda ADR+2						;
				sta blake2s_v+2,x				;
				lda ADR+3						;
				sta blake2s_v+3,x				;
.endmacro

; BLAKE2S_G: mixing function of BLAKE2s
; The two message words it folds in are read from the permutation table
; through the Y register, which walks through the whole table as the ten
; rounds go by
; ADR_A, ADR_B, ADR_C, ADR_D: absolute addresses of the four words mixed
.macro			BLAKE2S_G ADR_A,ADR_B,ADR_C,ADR_D
				BLAKE2S_SUM32 ADR_A,ADR_B		; a = a + b
				ldx blake2s_sigma,y				; First message word of the pair
				iny								;
				BLAKE2S_SUM32_M ADR_A			; a = a + m[sigma]
				BLAKE2S_XOR32 ADR_D,ADR_A		; d = d ^ a
				BLAKE2S_ROTR16 ADR_D			; d = d >>> 16
				BLAKE2S_SUM32 ADR_C,ADR_D		; c = c + d
				BLAKE2S_XOR32 ADR_B,ADR_C		; b = b ^ c
				BLAKE2S_ROTR12 ADR_B			; b = b >>> 12
				BLAKE2S_SUM32 ADR_A,ADR_B		; a = a + b
				ldx blake2s_sigma,y				; Second message word of the pair
				iny								;
				BLAKE2S_SUM32_M ADR_A			; a = a + m[sigma]
				BLAKE2S_XOR32 ADR_D,ADR_A		; d = d ^ a
				BLAKE2S_ROTR8 ADR_D				; d = d >>> 8
				BLAKE2S_SUM32 ADR_C,ADR_D		; c = c + d
				BLAKE2S_XOR32 ADR_B,ADR_C		; b = b ^ c
				BLAKE2S_ROTR7 ADR_B				; b = b >>> 7
.endmacro

.code

; ----------------------------------------------------------------------
; Take the four words of the next mix out of the working state
; ----------------------------------------------------------------------
.proc blake2s_fetch

				ldy blake2s_quad_index			;
				BLAKE2S_FETCH_WORD blake2s_ga	;
				BLAKE2S_FETCH_WORD blake2s_gb	;
				BLAKE2S_FETCH_WORD blake2s_gc	;
				BLAKE2S_FETCH_WORD blake2s_gd	;
				rts								;

.endproc

; ----------------------------------------------------------------------
; Put them back where they came from
; ----------------------------------------------------------------------
.proc blake2s_store

				ldy blake2s_quad_index			;
				BLAKE2S_STORE_WORD blake2s_ga	;
				BLAKE2S_STORE_WORD blake2s_gb	;
				BLAKE2S_STORE_WORD blake2s_gc	;
				BLAKE2S_STORE_WORD blake2s_gd	;
				rts								;

.endproc

; ----------------------------------------------------------------------
; Mix the four words that have been fetched
;
; How far the permutation table has been walked is kept in memory rather
; than in the Y register, because the round loop needs Y for the quad
; table between two mixes.
; ----------------------------------------------------------------------
.proc blake2s_mix

				ldy blake2s_sigma_index			;
				BLAKE2S_G blake2s_ga, blake2s_gb, blake2s_gc, blake2s_gd
				sty blake2s_sigma_index			;
				rts								;

.endproc

; ----------------------------------------------------------------------
; Compress the block held in _blake2s_m into the state held in _blake2s_h
;
; The caller has already put the block together, counted the bytes fed in
; so far into _blake2s_t and said whether this is the last block by
; setting _blake2s_f to zero or to all ones.
; ----------------------------------------------------------------------
.proc _blake2s_compress

; The first half of the working state is the chaining state
				ldx #BLAKE2S_HASH_SZ			;
copy_state:		lda _blake2s_h-1,x				;
				sta blake2s_v-1,x				;
				dex								;
				bne copy_state					;

; The second half is the initialisation vector, which is that of SHA-256
				ldx #BLAKE2S_HASH_SZ			;
copy_iv:		lda _blake2s_iv-1,x				;
				sta blake2s_v+BLAKE2S_HASH_SZ-1,x
				dex								;
				bne copy_iv						;

; The thirteenth word carries the number of bytes fed in so far, which is
; what makes the same block compress differently at different places in
; the message. The fourteenth would carry the high half of that count,
; which no text of this editor is long enough to reach
				BLAKE2S_XOR32 V12,_blake2s_t	;

; The fifteenth word is turned inside out on the last block, which is
; what separates a message from any longer message beginning with it
				lda _blake2s_f					;
				eor V14							;
				sta V14							;
				lda _blake2s_f					;
				eor V14+1						;
				sta V14+1						;
				lda _blake2s_f					;
				eor V14+2						;
				sta V14+2						;
				lda _blake2s_f					;
				eor V14+3						;
				sta V14+3						;

; Ten rounds of eight mixes each. The permutation table is walked from
; beginning to end as they go by, two entries per mix, which is exactly
; its length: ten rounds of sixteen message words
				lda #BLAKE2S_NB_ROUNDS			;
				sta blake2s_round				;
				lda #0							;
				sta blake2s_sigma_index			;

round_loop:		lda #0							; Back to the first of the eight mixes
				sta blake2s_quad_index			;

mix_loop:		jsr blake2s_fetch				; Take the four words out
				jsr blake2s_mix					; Mix them
				jsr blake2s_store				; Put them back

				lda blake2s_quad_index			; On to the next mix
				clc								;
				adc #BLAKE2S_QUAD_SZ			;
				sta blake2s_quad_index			;
				cmp #BLAKE2S_NB_MIXES*BLAKE2S_QUAD_SZ
				bne mix_loop					;

				dec blake2s_round				;
				bne round_loop					;

; The two halves of the working state are folded back into the chaining
; state, so that nothing of the block survives except through them
				ldx #BLAKE2S_HASH_SZ			;
fold:			lda _blake2s_h-1,x				;
				eor blake2s_v-1,x				;
				eor blake2s_v+BLAKE2S_HASH_SZ-1,x
				sta _blake2s_h-1,x				;
				dex								;
				bne fold						;

				rts								;

.endproc

.rodata

; ----------------------------------------------------------------------
; Initialisation vector, the eight words of SHA-256, least significant
; byte first as the processor stores them
; ----------------------------------------------------------------------
_blake2s_iv:	.dword	$6A09E667, $BB67AE85, $3C6EF372, $A54FF53A
				.dword	$510E527F, $9B05688C, $1F83D9AB, $5BE0CD19

; ----------------------------------------------------------------------
; Permutation table
;
; Each round folds the sixteen message words in in a different order, and
; the ten orders are those of the specification. The values are held
; multiplied by the size of a word, so that they can be used as offsets
; into the block without anything having to be computed: a 6502 has no
; multiplication, and the four shifts it would take per word would be
; paid eighty times per block.
; ----------------------------------------------------------------------
.macro			BLAKE2S_SIGMA_ROW S0,S1,S2,S3,S4,S5,S6,S7,S8,S9,SA,SB,SC,SD,SE,SF
				.byt	S0*BLAKE2S_WORD_SZ, S1*BLAKE2S_WORD_SZ
				.byt	S2*BLAKE2S_WORD_SZ, S3*BLAKE2S_WORD_SZ
				.byt	S4*BLAKE2S_WORD_SZ, S5*BLAKE2S_WORD_SZ
				.byt	S6*BLAKE2S_WORD_SZ, S7*BLAKE2S_WORD_SZ
				.byt	S8*BLAKE2S_WORD_SZ, S9*BLAKE2S_WORD_SZ
				.byt	SA*BLAKE2S_WORD_SZ, SB*BLAKE2S_WORD_SZ
				.byt	SC*BLAKE2S_WORD_SZ, SD*BLAKE2S_WORD_SZ
				.byt	SE*BLAKE2S_WORD_SZ, SF*BLAKE2S_WORD_SZ
.endmacro

; ----------------------------------------------------------------------
; Which four words each of the eight mixes of a round works on
;
; The first four mixes take the columns of the four by four square the
; sixteen words are laid out as, the last four take its diagonals. That
; never changes from one round to the next, which is why these can be
; read from a table while the message words cannot.
;
; The values are offsets in bytes, for the same reason as in the
; permutation table.
; ----------------------------------------------------------------------
.macro			BLAKE2S_QUAD_ROW W0,W1,W2,W3
				.byt	W0*BLAKE2S_WORD_SZ, W1*BLAKE2S_WORD_SZ
				.byt	W2*BLAKE2S_WORD_SZ, W3*BLAKE2S_WORD_SZ
.endmacro

blake2s_quad:
	BLAKE2S_QUAD_ROW  0, 4,  8, 12					; The four columns
	BLAKE2S_QUAD_ROW  1, 5,  9, 13					;
	BLAKE2S_QUAD_ROW  2, 6, 10, 14					;
	BLAKE2S_QUAD_ROW  3, 7, 11, 15					;
	BLAKE2S_QUAD_ROW  0, 5, 10, 15					; The four diagonals
	BLAKE2S_QUAD_ROW  1, 6, 11, 12					;
	BLAKE2S_QUAD_ROW  2, 7,  8, 13					;
	BLAKE2S_QUAD_ROW  3, 4,  9, 14					;

.assert (* - blake2s_quad) = BLAKE2S_NB_MIXES * BLAKE2S_QUAD_SZ, error, "The quad table is the wrong size"

blake2s_sigma:
	BLAKE2S_SIGMA_ROW  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15
	BLAKE2S_SIGMA_ROW 14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3
	BLAKE2S_SIGMA_ROW 11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4
	BLAKE2S_SIGMA_ROW  7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8
	BLAKE2S_SIGMA_ROW  9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13
	BLAKE2S_SIGMA_ROW  2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9
	BLAKE2S_SIGMA_ROW 12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11
	BLAKE2S_SIGMA_ROW 13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10
	BLAKE2S_SIGMA_ROW  6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5
	BLAKE2S_SIGMA_ROW 10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0

; The Y register walks through the whole table without ever being reset
.assert (* - blake2s_sigma) = BLAKE2S_NB_ROUNDS * BLAKE2S_SIGMA_SZ, error, "The permutation table is the wrong size"
