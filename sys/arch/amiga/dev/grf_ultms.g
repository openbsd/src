;
; ite support for A2410.

;
; Copyright (c) 1995 Ignatios Souvatzis.
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
; 1. Redistributions of source code must retain the above copyright
;    notice, this list of conditions and the following disclaimer.
; 2. Redistributions in binary form must reproduce the above copyright
;    notice, this list of conditions and the following disclaimer in the
;    documentation and/or other materials provided with the distribution.
; 3. All advertising materials mentioning features or use of this software
;    must display the following acknowledgement:
; 4. The name of the author may not be used to endorse or promote products
;    derived from this software withough specific prior written permission
;
; THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
; IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
; OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
; IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
; INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
; NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
; THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

; This file contains the source code for grf_ultmscode.h. It is
; assembler code for the TMS34010 CPU/graphics processor. 
;
; Use Paul Mackerras' gspa assembler to transfer it to hex format, then
; Ignatios Souvatzis' hex2c utility (available from the author) or a small 
; perl script to transform that into the form of grf_ultmscode.h. 
; 
; A modified gspa for this purpose will be released as soon as this
; procedure is cleaned up. 
;

; memory map:
; FF800000 .. FF9FFFFF	overlay planes
; FFA00000 .. FFA0FFFF	ite support code
; FFA10000 .. FFA1FFFF	ite support, input queue
; FFA20000 .. FFA2FEFF	variables
; FFA2FF00 .. FFA2FFFF	variables, X server
; FFA30000 .. FFA3FFFF	font data
; FFA40000 .. FFA4FFFF	font data, bold
; FFA50000 .. FFA5FFFF	X server, input queue
; FFA60000 .. FFFFC000	X server, onboard pixmaps

; Start of data area
	.org	$FFA20000
d:

;
; Ring buffer for getting stuff from host
; Data buffer:
inbuf	=	$FFA10000	; 64kbits here (8k bytes)
;
; Pointers: (these must be at address $FFA20000)
put:		.long	inbuf
get:		.long	inbuf

;
; Mode bits for communication between GSP and CPU
;
; GSP mode bits: set by CPU, control GSP operation
GSP_HOLD =	0
GSP_FLUSH =	1
GSP_ALT_SCRN =	2
GSP_DISP_CTRL =	3
GSP_NO_CURSOR =	4
GSP_CALL_X =	5
gsp_mode:	.word	0

;
; Pointer to X operation routine
xproc:		.long	0

; We leave the next few words for future communication requirements

		.org	d+0x100
;
; Other data:
magic:		.blkl	1		; set => screen already inited
MAGIC =		0xD0D0BEAC

screen_width:	.word	1024
screen_height:	.word	768
screen_origin:	.long	$FE000000	; just a placeholder
screen_pitch:	.word	8192		; 1024*8
pixel_size:	.word	8

		.org	d+0x200
font_adr:
;
; Font information is stored in the structure defined declared below.
;
bitmap_ptrs:	.long	$FFA30000	; points to first bitmap
font_size:	.long	$00080008	; Y:X bitmap size
under_row:	.word	6		; row # for underlines
under_ht:	.word	1		; thickness of underline
first_char:	.word	32		; first and last char in font
last_char:	.word	255		;
bold_smear:	.word	1		; for making bold fonts

bgcolor:	.long	0		; background color
fgcolor:	.long	$01010101	; foreground color
;precomputed out of what the host gave us:
font_area:	.word	64		; in pixels
font_pitch:	.word	8
font_lmo:	.word	28


; Control register addresses
hesync	=	$c0000000
dpyctl	=	$c0000080
control	=	$c00000b0
convsp	=	$c0000130
convdp	=	$c0000140
psize	=	$c0000150

;
; Bits in control register
T	=	$20		; enable transparency
W	=	$C0		; window options
PBH	=	$100		; pixblt horiz dirn
PBV	=	$200		; pixblt vertical dirn
PPOP	=	$7C00		; pixel processing options

;
; Bits in dpyctl register
SRT	=	$800		; do serial register transfers

free_memory:	.long	free_memory_start
free_memory_start:		; allocate dynamic arrays from here

;
; Program starts here.
	.org	$FFA00000
	.start	.

;
; initialization
;
	setf	16,0,0		; just in case
	setf	32,0,1
	move	$fffff000,sp

; Set up sync, blank parameters
; done by host through interface

; set up overlay clut:
	move	$0,a0
	move	a0,@$fe800000
	move	$fe800030,a1
	move	128,a0
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1
	move	0,a0
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1
	move	a0,*a1

; set up overlay planes:
	move	6,a0
	move	a0,@$fe800000
	move	$0b,a0
	move	a0,@$fe800020

; set up global registers
	move	@screen_pitch,b3,0
	move	@screen_origin,b4,1
	move	@bgcolor,b8,1
	lmo	b3,b0
	move	b0,@convdp,0
	move	@control,a0,0
	andn	$7FE0,a0		; clear PPOP, PBV, PBH, W, T fields
	move	a0,@control,0
	move	@pixel_size,a0,0
	move	a0,@psize,0
	move	@psize,a0,0


; clear the entire screen
	move	b4,b2
	move	0,b9
	move	@screen_width,b7,1
	fill	l

4:
; main stuff...
	move	@get,a0,1
	jruc	main_loop
loop_end:
	clr	a4
	move	a4,*a0,0
	addxy	a1,a0
	move	a0,@get,1
main_loop:
	move	@gsp_mode,a1,0
	btst	GSP_CALL_X,a1
	jreq	main_loop_1

	mmtm	sp,a0,a1,a2,a3
	move	@xproc,a4,1
	call	a4
	mmfm	sp,a0,a1,a2,a3

main_loop_1:
	move	@put,a3,1
	move	*a0,a1,0

	move	a1,a2
	andi	$FFF0,a1
	jrz	main_loop

	sub	a0,a3
	jreq	main_loop
continue:
	andi	$F,a2
	jrz	loop_end
	dec	a2
	jrnz	testfor2
; op 1 - char
	movk	6,b10
	move	b10,@$fe800000,0
	movk	1,b10
	move	b10,@$fe800020,0

	move	a0,b10
	move	*b10+,b12,0	; dummy move (faster than addk)
	move	*b10+,b12,0	; char code
	move	@first_char,b11,0
	sub	b11,b12		; minus first char in font
	move	@font_size,b7,1	;dydx - char size->pixel array dimensions
	move	@font_pitch,b1
	move	@font_lmo,b0
	move	b0,@convsp,0
	move	@font_area,b11
	
	mpyu	b12,b11		; times char offset
	move	@font_adr,b0,1	; font bitmaps base
	add	b11,b0		; character bitmap start addr. linear

	move	*b10+,b8,0	; fg
	move	*b10+,b9,0	; bg
	move	*b10+,b2,1	; y:x

	move	*b10+,b11,0	; flags
	move	b11,a4
	btst	0,a4
	jreq	noinv
	move	b8,b11
	move	b9,b8
	move	b11,b9
noinv:
	btst	2,a4
	jreq	nobold
	addi	$10000,b0
nobold:
	move	b2,a5
	pixblt	b,xy
	move	a5,b2

	btst	1,a4
	jreq	noul
	move	@under_row,b11,0
	sll	16,b11		; shift into Y half
	add	b11,b2
	move	@under_ht,b11,0
	sll	16,b11		; shift into Y half
	movy	b11,b7		; and move Y half only
	fill	xy
noul:
	jruc	loop_end
testfor2:
	dec	a2
	jrnz	testfor3
; op 2 - fill
	move	a0,b10
	move	*b10+,b9,0	; dummy move
	move	*b10+,b9,0	; color
	move	*b10+,b2,1	; XY start address
	move	*b10+,b7,1	; dydx

	move	@control,b0,0
	move	b0,*-sp
	move	*b10+,b0
	setf	5,0,0
	move	b0,@control+10
	setf	16,0,0
	move	@control,b0,0

	fill	xy

	move	*sp+,b0
	move	b0,@control,0
	jruc	loop_end,l

testfor3:
	dec	a2
	jrnz	testfor4
; op 3 - pixblt
	move	a0,b10
	move	@convdp,@convsp,0
	move	*b10+,b0,0	; dummy move
	move	*b10+,b0,1	; XY src
	move	*b10+,b7,1	; dxdy
	move	*b10+,b2,1	; XY dst
	move	b3,b1
	move	@control,b11,0
	andni	PBH|PBV,b11
	cmpxy	b0,b2
	jrc	yok
	ori	PBV,b11
yok:	jrv	xok
	ori	PBH,b11
xok:	move	b11,@control,0
	move	@control,b11,0
	
	pixblt	xy,xy
	jruc	loop_end,l

testfor4:
	dec	a2
	jrnz	testfor5

; op 4 - mirror the font and precompute some values.

	move	@font_size,a5,0
	movk	8,a6
	cmp	a6,a5
	jrle	t4b8
	movi	16, a6
t4b8:	move	a6,@font_pitch,0
	lmo	a5,a6
	move	a6,@font_lmo,0
	move	@font_size+$10,a6,0
	move	@font_pitch,a5,0
	mpyu	a6,a5
	move	a5,@font_area,0

	move	@last_char,a6,0
	move	@first_char,a5,0
	sub	a5,a6
	addk	1,a6
	move	@font_size+$10,a5,0
	mpyu	a6,a5
	move	@font_size,a7,0
	cmpi	8,a7
	move	$7f7f,a12	; mask for bold smearing
	jrgt	t4bf		; wider than 8 pixels?
	addk	1,a5		; yes, the words are only half the # of rows
	srl	1,a5
	move	$7fff,a12	; mask for bold smearing changes, too
t4bf:	move	@font_adr,a6,1
	move	a6,a9
	addi	$10000,a9 ; start address of bold font
	move	@bold_smear,a10

; fortunately, this loop fits into 3 of the 4 cache segments:
; execution time: about 32 periods per word of font.

mirlp:	move	*a6,a7
	clr	a8

	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8

	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8

	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8

	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8
	srl	1,a7
	addc	a8,a8

	move	a8,*a6+
	move	a8,a7
	move	a10,a11
smearlp:
	and	a12,a7
	sll	1,a7
	or	a7,a8
	dsj	a11,smearlp
	move	a8,*a9+

	dsj	a5,mirlp
;; support odd-sized fonts. pitch must still be 8 or 16
	move	@font_size,a5,0
	move	@font_pitch,a6,0
	sub	a5,a6
	move	@font_adr,a5,1
	add	a5,a6
	move	a6,@font_adr,1
;;
	jruc	loop_end,l

	
testfor5:
	dec	a2
	jrne	testfor6
; loadclut --- load clut entry.
;	1==overlay index red green blue
;	for speed reasons, the host will load the image clut directly rather
;	than through us, but its not that expensive to support both here 
;	just in case
	move	a0,a4
	addk	$10,a4
	move	$fe800030,a6
	move	*a4+,a5,0
	jrne	t5l1
	subk	$20,a6
t5l1:	move	*a4+,a5,0
	move	a5,@$fe800000,0
	move	*a4+,a5,0
	move	a5,*a6,0
	move	*a4+,a5,0
	move	a5,*a6,0
	move	*a4+,a5,0
	move	a5,*a6,0
	jruc	loop_end,l
	
testfor6:
	dec	a2
	jrne	testfor7

; op 6: load new framebuffer size and position for ite support.
	move	a0,b10
	addk	$10,b10
	move	*b10+,b7,1
	move	b7,@screen_width,1
	move	*b10+,b4,1
	move	b4,@screen_origin,1
	move	*b10+,b3,0
	move	b3,@screen_pitch,0
	lmo	b3,b0
	move	b0,@convdp,0
	move	*b10,b0,0
	move	b0,@psize,0
	move	b0,@pixel_size,0	; this syncs the psize write, too

	jruc	loop_end,l
	
testfor7:
	jruc	loop_end,l
;;;
