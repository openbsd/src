;	$OpenBSD: milli.S,v 1.1 2005/04/01 10:45:29 mickey Exp $
;
;  (c) Copyright 1986 HEWLETT-PACKARD COMPANY
;
;  To anyone who acknowledges that this file is provided "AS IS"
;  without any express or implied warranty:
;      permission to use, copy, modify, and distribute this file
;  for any purpose is hereby granted without fee, provided that
;  the above copyright notice and this notice appears in all
;  copies, and that the name of Hewlett-Packard Company not be
;  used in advertising or publicity pertaining to distribution
;  of the software without specific, written prior permission.
;  Hewlett-Packard Company makes no representations about the
;  suitability of this software for any purpose.
;

	.text
	.EXPORT $$remI,millicode
$$remI:
	.PROC
	.CALLINFO NO_CALLS
	.ENTRY
	addit,= 0,%arg1,%r0
	add,>= %r0,%arg0,%ret1
	sub %r0,%ret1,%ret1
	sub %r0,%arg1,%r1
	ds %r0,%r1,%r0
	or %r0,%r0,%r1
	add %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	movb,>=,n %r1,%ret1,remI300
	add,< %arg1,%r0,%r0
	add,tr %r1,%arg1,%ret1
	sub %r1,%arg1,%ret1
remI300: add,>= %arg0,%r0,%r0

	bv %r0(%rp)
	sub %r0,%ret1,%ret1
	.EXIT
	.PROCEND

	.export $$divU,millicode
	.import $$divU_3,millicode
	.import $$divU_5,millicode
	.import $$divU_6,millicode
	.import $$divU_7,millicode
	.import $$divU_9,millicode
	.import $$divU_10,millicode
	.import $$divU_12,millicode
	.import $$divU_14,millicode
	.import $$divU_15,millicode
$$divU:
	.proc
	.callinfo NO_CALLS
; The subtract is not nullified since it does no harm and can be used
; by the two cases that branch back to "normal".
	comib,>=  15,%arg1,special_divisor
	sub	%r0,%arg1,%r1		; clear carry, negate the divisor
	ds	%r0,%r1,%r0		; set V-bit to 1
normal:
	add	%arg0,%arg0,%ret1	; shift msb bit into carry
	ds	%r0,%arg1,%r1		; 1st divide step, if no carry
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 2nd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 3rd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 4th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 5th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 6th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 7th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 8th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 9th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 10th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 11th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 12th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 13th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 14th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 15th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 16th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 17th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 18th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 19th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 20th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 21st divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 22nd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 23rd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 24th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 25th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 26th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 27th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 28th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 29th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 30th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 31st divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 32nd divide step,
	bv	0(%rp)
	addc	%ret1,%ret1,%ret1	; shift last %ret1 bit into %ret1
;_____________________________________________________________________________
; handle the cases where divisor is a small constant or has high bit on
special_divisor:
	depd	%r0,31,32,%arg1

	comib,>,n  0,%arg1,big_divisor	; nullify previous instruction
	nop
	blr	%arg1,%r0
	nop
zero_divisor: ; this label is here to provide external visibility

	addit,=	0,%arg1,0		; trap for zero dvr
	nop
	bv	0(%rp)			; divisor == 1
	copy	%arg0,%ret1
	bv	0(%rp)			; divisor == 2
	extru	%arg0,30,31,%ret1
	 b,n	$$divU_3		; divisor == 3
	nop
	bv	0(%rp)			; divisor == 4
	extru	%arg0,29,30,%ret1
	 b,n	$$divU_5		; divisor == 5
	nop
	 b,n	$$divU_6		; divisor == 6
	nop
	 b,n	$$divU_7		; divisor == 7
	nop
	bv	0(%rp)			; divisor == 8
	extru	%arg0,28,29,%ret1
	 b,n	$$divU_9		; divisor == 9
	nop
	 b,n	$$divU_10		; divisor == 10
	nop
	b	normal			; divisor == 11
	ds	%r0,%r1,%r0		; set V-bit to 1
	 b,n	$$divU_12		; divisor == 12
	nop
	b	normal			; divisor == 13
	ds	%r0,%r1,%r0		; set V-bit to 1
	 b,n	$$divU_14		; divisor == 14
	nop
	 b,n	$$divU_15		; divisor == 15
	nop
;_____________________________________________________________________________
; Handle the case where the high bit is on in the divisor.
; Compute:	if( dividend>=divisor) quotient=1; else quotient=0;
; Note:		dividend>==divisor iff dividend-divisor does not borrow
; and		not borrow iff carry
big_divisor:
	sub	%arg0,%arg1,%r0
	bv	0(%rp)
	addc	%r0,%r0,%ret1
	.procend
	.end

;_____________________________________________________________________________

$$divide_by_constant:
	.PROC
	.CALLINFO NO_CALLS
	.export $$divide_by_constant,millicode
; Provides a "nice" label for the code covered by the unwind descriptor
; for things like gprof.

$$divI_2:
	.EXPORT	$$divI_2,MILLICODE
	COMCLR,>=	%arg0,0,0
	ADDI		1,%arg0,%arg0
	bv		0(%rp)
	EXTRS		%arg0,30,31,%ret1

$$divI_4:
	.EXPORT		$$divI_4,MILLICODE
	COMCLR,>=	%arg0,0,0
	ADDI		3,%arg0,%arg0
	bv		0(%rp)
	EXTRS		%arg0,29,30,%ret1

$$divI_8:
	.EXPORT		$$divI_8,MILLICODE
	COMCLR,>=	%arg0,0,0
	ADDI		7,%arg0,%arg0
	bv		0(%rp)
	EXTRS		%arg0,28,29,%ret1

$$divI_16:
	.EXPORT		$$divI_16,MILLICODE
	COMCLR,>=	%arg0,0,0
	ADDI		15,%arg0,%arg0
	bv		0(%rp)
	EXTRS		%arg0,27,28,%ret1

$$divI_3:
	.EXPORT		$$divI_3,MILLICODE
	COMB,<,N	%arg0,0,$neg3

	ADDI		1,%arg0,%arg0
	EXTRU		%arg0,1,2,%ret1
	SH2ADD		%arg0,%arg0,%arg0
	B		$pos
	ADDC		%ret1,0,%ret1

$neg3:
	SUBI		1,%arg0,%arg0
	EXTRU		%arg0,1,2,%ret1
	SH2ADD		%arg0,%arg0,%arg0
	B		$neg
	ADDC		%ret1,0,%ret1

$$divU_3:
	.EXPORT		$$divU_3,MILLICODE
	ADDI		1,%arg0,%arg0
	ADDC		0,0,%ret1
	SHD		%ret1,%arg0,30,%arg1
	SH2ADD		%arg0,%arg0,%arg0
	B		$pos
	ADDC		%ret1,%arg1,%ret1

$$divI_5:
	.EXPORT		$$divI_5,MILLICODE
	COMB,<,N	%arg0,0,$neg5
	ADDI		3,%arg0,%arg1
	SH1ADD		%arg0,%arg1,%arg0
	B		$pos
	ADDC		0,0,%ret1

$neg5:
	SUB		0,%arg0,%arg0
	ADDI		1,%arg0,%arg0
	SHD		0,%arg0,31,%ret1
	SH1ADD		%arg0,%arg0,%arg0
	B		$neg
	ADDC		%ret1,0,%ret1

$$divU_5:
	.EXPORT		$$divU_5,MILLICODE
	ADDI		1,%arg0,%arg0
	ADDC		0,0,%ret1
	SHD		%ret1,%arg0,31,%arg1
	SH1ADD		%arg0,%arg0,%arg0
	B		$pos
	ADDC		%arg1,%ret1,%ret1

$$divI_6:
	.EXPORT		$$divI_6,MILLICODE
	COMB,<,N	%arg0,0,$neg6
	EXTRU		%arg0,30,31,%arg0
	ADDI		5,%arg0,%arg1
	SH2ADD		%arg0,%arg1,%arg0
	B		$pos
	ADDC		0,0,%ret1

$neg6:
	SUBI		2,%arg0,%arg0
	EXTRU		%arg0,30,31,%arg0
	SHD		0,%arg0,30,%ret1
	SH2ADD		%arg0,%arg0,%arg0
	B		$neg
	ADDC		%ret1,0,%ret1

$$divU_6:
	.EXPORT		$$divU_6,MILLICODE
	EXTRU		%arg0,30,31,%arg0
	ADDI		1,%arg0,%arg0
	SHD		0,%arg0,30,%ret1
	SH2ADD		%arg0,%arg0,%arg0
	B		$pos
	ADDC		%ret1,0,%ret1

$$divU_10:
	.EXPORT		$$divU_10,MILLICODE
	EXTRU		%arg0,30,31,%arg0
	ADDI		3,%arg0,%arg1
	SH1ADD		%arg0,%arg1,%arg0
	ADDC		0,0,%ret1
$pos:
	SHD		%ret1,%arg0,28,%arg1
	SHD		%arg0,0,28,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%ret1
$pos_for_17:
	SHD		%ret1,%arg0,24,%arg1
	SHD		%arg0,0,24,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%ret1

	SHD		%ret1,%arg0,16,%arg1
	SHD		%arg0,0,16,%r1
	ADD		%arg0,%r1,%arg0
	bv		0(%rp)
	ADDC		%ret1,%arg1,%ret1

$$divI_10:
	.EXPORT		$$divI_10,MILLICODE
	COMB,<		%arg0,0,$neg10
	COPY		0,%ret1
	EXTRU		%arg0,30,31,%arg0
	ADDIB,TR	1,%arg0,$pos
	SH1ADD		%arg0,%arg0,%arg0

$neg10:
	SUBI		2,%arg0,%arg0
	EXTRU		%arg0,30,31,%arg0
	SH1ADD		%arg0,%arg0,%arg0
$neg:
	SHD		%ret1,%arg0,28,%arg1
	SHD		%arg0,0,28,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%ret1
$neg_for_17:
	SHD		%ret1,%arg0,24,%arg1
	SHD		%arg0,0,24,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%ret1

	SHD		%ret1,%arg0,16,%arg1
	SHD		%arg0,0,16,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%ret1
	bv		0(%rp)
	SUB		0,%ret1,%ret1

$$divI_12:
	.EXPORT		$$divI_12,MILLICODE
	COMB,<		%arg0,0,$neg12
	COPY		0,%ret1
	EXTRU		%arg0,29,30,%arg0
	ADDIB,TR	1,%arg0,$pos
	SH2ADD		%arg0,%arg0,%arg0

$neg12:
	SUBI		4,%arg0,%arg0
	EXTRU		%arg0,29,30,%arg0
	B		$neg
	SH2ADD		%arg0,%arg0,%arg0

$$divU_12:
	.EXPORT		$$divU_12,MILLICODE
	EXTRU		%arg0,29,30,%arg0
	ADDI		5,%arg0,%arg1
	SH2ADD		%arg0,%arg1,%arg0
	B		$pos
	ADDC		0,0,%ret1

$$divI_15:
	.EXPORT		$$divI_15,MILLICODE
	COMB,<		%arg0,0,$neg15
	COPY		0,%ret1
	ADDIB,TR	1,%arg0,$pos+4
	SHD		%ret1,%arg0,28,%arg1

$neg15:
	B		$neg
	SUBI		1,%arg0,%arg0

$$divU_15:
	.EXPORT		$$divU_15,MILLICODE
	ADDI		1,%arg0,%arg0
	B		$pos
	ADDC		0,0,%ret1

$$divI_17:
	.EXPORT		$$divI_17,MILLICODE
	COMB,<,N	%arg0,0,$neg17
	ADDI		1,%arg0,%arg0
	SHD		0,%arg0,28,%arg1
	SHD		%arg0,0,28,%r1
	SUB		%r1,%arg0,%arg0
	B		$pos_for_17
	SUBB		%arg1,0,%ret1

$neg17:
	SUBI		1,%arg0,%arg0
	SHD		0,%arg0,28,%arg1
	SHD		%arg0,0,28,%r1
	SUB		%r1,%arg0,%arg0
	B		$neg_for_17
	SUBB		%arg1,0,%ret1

$$divU_17:
	.EXPORT		$$divU_17,MILLICODE
	ADDI		1,%arg0,%arg0
	ADDC		0,0,%ret1
	SHD		%ret1,%arg0,28,%arg1
$u17:
	SHD		%arg0,0,28,%r1
	SUB		%r1,%arg0,%arg0
	B		$pos_for_17
	SUBB		%arg1,%ret1,%ret1

$$divI_7:
	.EXPORT		$$divI_7,MILLICODE
	COMB,<,N	%arg0,0,$neg7
$7:
	ADDI		1,%arg0,%arg0
	SHD		0,%arg0,29,%ret1
	SH3ADD		%arg0,%arg0,%arg0
	ADDC		%ret1,0,%ret1
$pos7:
	SHD		%ret1,%arg0,26,%arg1
	SHD		%arg0,0,26,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%ret1

	SHD		%ret1,%arg0,20,%arg1
	SHD		%arg0,0,20,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%arg1

	COPY		0,%ret1
	SHD,=		%arg1,%arg0,24,%arg1
$1:
	ADDB,TR		%arg1,%ret1,$2
	EXTRU		%arg0,31,24,%arg0

	bv,n  0(%rp)

$2:
	ADDB,TR		%arg1,%arg0,$1
	EXTRU,=		%arg0,7,8,%arg1

$neg7:
	SUBI		1,%arg0,%arg0
$8:
	SHD		0,%arg0,29,%ret1
	SH3ADD		%arg0,%arg0,%arg0
	ADDC		%ret1,0,%ret1

$neg7_shift:
	SHD		%ret1,%arg0,26,%arg1
	SHD		%arg0,0,26,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%ret1

	SHD		%ret1,%arg0,20,%arg1
	SHD		%arg0,0,20,%r1
	ADD		%arg0,%r1,%arg0
	ADDC		%ret1,%arg1,%arg1

	COPY		0,%ret1
	SHD,=		%arg1,%arg0,24,%arg1
$3:
	ADDB,TR		%arg1,%ret1,$4
	EXTRU		%arg0,31,24,%arg0

	bv		0(%rp)
	SUB		0,%ret1,%ret1

$4:
	ADDB,TR		%arg1,%arg0,$3
	EXTRU,=		%arg0,7,8,%arg1

$$divU_7:
	.EXPORT		$$divU_7,MILLICODE
	ADDI		1,%arg0,%arg0
	ADDC		0,0,%ret1
	SHD		%ret1,%arg0,29,%arg1
	SH3ADD		%arg0,%arg0,%arg0
	B		$pos7
	ADDC		%arg1,%ret1,%ret1

$$divI_9:
	.EXPORT		$$divI_9,MILLICODE
	COMB,<,N	%arg0,0,$neg9
	ADDI		1,%arg0,%arg0
	SHD		0,%arg0,29,%arg1
	SHD		%arg0,0,29,%r1
	SUB		%r1,%arg0,%arg0
	B		$pos7
	SUBB		%arg1,0,%ret1

$neg9:
	SUBI		1,%arg0,%arg0
	SHD		0,%arg0,29,%arg1
	SHD		%arg0,0,29,%r1
	SUB		%r1,%arg0,%arg0
	B		$neg7_shift
	SUBB		%arg1,0,%ret1

$$divU_9:
	.EXPORT		$$divU_9,MILLICODE
	ADDI		1,%arg0,%arg0
	ADDC		0,0,%ret1
	SHD		%ret1,%arg0,29,%arg1
	SHD		%arg0,0,29,%r1
	SUB		%r1,%arg0,%arg0
	B		$pos7
	SUBB		%arg1,%ret1,%ret1

$$divI_14:
	.EXPORT		$$divI_14,MILLICODE
	COMB,<,N	%arg0,0,$neg14
$$divU_14:
	.EXPORT		$$divU_14,MILLICODE
	B		$7
	EXTRU		%arg0,30,31,%arg0

$neg14:
	SUBI		2,%arg0,%arg0
	B		$8
	EXTRU		%arg0,30,31,%arg0

	.PROCEND
	.END

	.export $$remU,millicode
$$remU:
	.proc
	.callinfo NO_CALLS
	.entry

	comib,>=,n  0,%arg1,special_case
	sub	%r0,%arg1,%ret1		; clear carry, negate the divisor
	ds	%r0,%ret1,%r0		; set V-bit to 1
	add	%arg0,%arg0,%r1		; shift msb bit into carry
	ds	%r0,%arg1,%ret1		; 1st divide step, if no carry
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 2nd divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 3rd divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 4th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 5th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 6th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 7th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 8th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 9th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 10th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 11th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 12th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 13th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 14th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 15th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 16th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 17th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 18th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 19th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 20th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 21st divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 22nd divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 23rd divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 24th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 25th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 26th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 27th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 28th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 29th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 30th divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 31st divide step
	addc	%r1,%r1,%r1		; shift %r1 with/into carry
	ds	%ret1,%arg1,%ret1		; 32nd divide step,
	comiclr,<= 0,%ret1,%r0
	  add	%ret1,%arg1,%ret1	; correction
;	.exit
	bv,n  0(%rp)
	nop
; Putting >= on the last DS and deleting COMICLR does not work!
;_____________________________________________________________________________
special_case:
	addit,=	0,%arg1,%r0		; trap on div by zero
	sub,>>=	%arg0,%arg1,%ret1
	  copy	%arg0,%ret1
	.exit
	bv,n  0(%rp)
	nop
	.procend
	.end

	.align 16
$$mulI:

	.proc
	.callinfo NO_CALLS
	.export $$mulI, millicode
	combt,<<=	%arg1,%arg0,l4	; swap args if unsigned %arg1>%arg0
	copy		0,%ret1		; zero out the result
	xor		%arg0,%arg1,%arg0	; swap %arg0 & %arg1 using the
	xor		%arg0,%arg1,%arg1	;  old xor trick
	xor		%arg0,%arg1,%arg0
l4: combt,<=	0,%arg0,l3		; if %arg0>=0 then proceed like unsigned

	zdep		%arg1,30,8,%r1	; %r1 = (%arg1&0xff)<<1 *********
	sub,>		0,%arg1,%r1		; otherwise negate both and
	combt,<=,n	%arg0,%r1,l2	;  swap back if |%arg0|<|%arg1|
	sub		0,%arg0,%arg1
	movb,tr,n	%r1,%arg0,l2	; 10th inst.

l0:	add	%ret1,%r1,%ret1				; add in this partial product

l1: zdep	%arg0,23,24,%arg0			; %arg0 <<= 8 ******************

l2: zdep		%arg1,30,8,%r1	; %r1 = (%arg1&0xff)<<1 *********

l3: blr		%r1,0		; case on these 8 bits ******

	extru		%arg1,23,24,%arg1	; %arg1 >>= 8 ******************

;16 insts before this.
;			  %arg0 <<= 8 **************************
x0: comb,<>	%arg1,0,l2	! zdep	%arg0,23,24,%arg0	! bv,n  0(%rp)	! nop

x1: comb,<>	%arg1,0,l1	!	add	%ret1,%arg0,%ret1	! bv,n  0(%rp)	! nop

x2: comb,<>	%arg1,0,l1	! sh1add	%arg0,%ret1,%ret1	! bv,n  0(%rp)	! nop

x3: comb,<>	%arg1,0,l0	!	sh1add	%arg0,%arg0,%r1	! bv	0(%rp)	!	add	%ret1,%r1,%ret1

x4: comb,<>	%arg1,0,l1	! sh2add	%arg0,%ret1,%ret1	! bv,n  0(%rp)	! nop

x5: comb,<>	%arg1,0,l0	!	sh2add	%arg0,%arg0,%r1	! bv	0(%rp)	!	add	%ret1,%r1,%ret1

x6:	sh1add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh1add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x7:	sh1add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh2add	%arg0,%ret1,%ret1	! b,n	ret_t0

x8: comb,<>	%arg1,0,l1	! sh3add	%arg0,%ret1,%ret1	! bv,n  0(%rp)	! nop

x9: comb,<>	%arg1,0,l0	!	sh3add	%arg0,%arg0,%r1	! bv	0(%rp)	!	add	%ret1,%r1,%ret1

x10:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh1add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x11:	sh1add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh3add	%arg0,%ret1,%ret1	! b,n	ret_t0

x12:	sh1add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh2add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x13:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh3add	%arg0,%ret1,%ret1	! b,n	ret_t0

x14:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x15:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	!	sh1add	%r1,%r1,%r1	! b,n	ret_t0

x16: zdep	%arg0,27,28,%r1	! comb,<>	%arg1,0,l1	!	add	%ret1,%r1,%ret1	! bv,n  0(%rp)

x17:	sh3add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh3add	%arg0,%r1,%r1	! b,n	ret_t0

x18:	sh3add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh1add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x19:	sh3add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh1add	%r1,%arg0,%r1	! b,n	ret_t0

x20:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh2add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x21:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh2add	%r1,%arg0,%r1	! b,n	ret_t0

x22:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x23:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x24:	sh1add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh3add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x25:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	!	sh2add	%r1,%r1,%r1	! b,n	ret_t0

x26:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x27:	sh1add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	!	sh3add	%r1,%r1,%r1	! b,n	ret_t0

x28:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x29:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x30:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x31: zdep	%arg0,26,27,%r1	! comb,<>	%arg1,0,l0	! sub	%r1,%arg0,%r1	! b,n	ret_t0

x32: zdep	%arg0,26,27,%r1	! comb,<>	%arg1,0,l1	!	add	%ret1,%r1,%ret1	! bv,n  0(%rp)

x33:	sh3add	%arg0,0,%r1		! comb,<>	%arg1,0,l0	! sh2add	%r1,%arg0,%r1	! b,n	ret_t0

x34: zdep	%arg0,27,28,%r1	! add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x35:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh3add	%arg0,%r1,%r1

x36:	sh3add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh2add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x37:	sh3add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh2add	%r1,%arg0,%r1	! b,n	ret_t0

x38:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x39:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x40:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh3add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x41:	sh2add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! sh3add	%r1,%arg0,%r1	! b,n	ret_t0

x42:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x43:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x44:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x45:	sh3add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	!	sh2add	%r1,%r1,%r1	! b,n	ret_t0

x46:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! add	%r1,%arg0,%r1

x47:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sh1add	%arg0,%r1,%r1

x48:	sh1add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l0	! zdep	%r1,27,28,%r1	! b,n	ret_t0

x49:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sh2add	%arg0,%r1,%r1

x50:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x51:	sh3add	%arg0,%arg0,%r1		! sh3add	%arg0,%r1,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x52:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x53:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x54:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x55:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x56:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x57:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x58:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x59:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_t02a0	!	sh1add	%r1,%r1,%r1

x60:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x61:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x62: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x63: zdep	%arg0,25,26,%r1	! comb,<>	%arg1,0,l0	! sub	%r1,%arg0,%r1	! b,n	ret_t0

x64: zdep	%arg0,25,26,%r1	! comb,<>	%arg1,0,l1	!	add	%ret1,%r1,%ret1	! bv,n  0(%rp)

x65:	sh3add	%arg0,0,%r1		! comb,<>	%arg1,0,l0	! sh3add	%r1,%arg0,%r1	! b,n	ret_t0

x66: zdep	%arg0,26,27,%r1	! add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x67:	sh3add	%arg0,0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x68:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x69:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x70: zdep	%arg0,25,26,%r1	! sh2add	%arg0,%r1,%r1	!	b	e_t0	! sh1add	%arg0,%r1,%r1

x71:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,0,%r1	!	b	e_t0	! sub	%r1,%arg0,%r1

x72:	sh3add	%arg0,%arg0,%r1		! comb,<>	%arg1,0,l1	! sh3add	%r1,%ret1,%ret1	! bv,n  0(%rp)

x73:	sh3add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	! b	e_shift	!	add	%ret1,%r1,%ret1

x74:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x75:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x76:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x77:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x78:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_2t0	! sh1add	%r1,%arg0,%r1

x79: zdep	%arg0,27,28,%r1	!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sub	%r1,%arg0,%r1

x80: zdep	%arg0,27,28,%r1	!	sh2add	%r1,%r1,%r1	! b	e_shift	!	add	%ret1,%r1,%ret1

x81:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	! b	e_shift	!	add	%ret1,%r1,%ret1

x82:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x83:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x84:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x85:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x86:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_2t0	! sh1add	%r1,%arg0,%r1

x87:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	! b	e_t02a0	! sh2add	%arg0,%r1,%r1

x88:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x89:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x90:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x91:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x92:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_4t0	! sh1add	%r1,%arg0,%r1

x93: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x94:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_2t0	! sh1add	%arg0,%r1,%r1

x95:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x96:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x97:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x98: zdep	%arg0,26,27,%r1	!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh1add	%arg0,%r1,%r1

x99:	sh3add	%arg0,0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x100:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x101:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x102: zdep	%arg0,26,27,%r1	! sh1add	%arg0,%r1,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x103:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_t02a0	! sh2add	%r1,%arg0,%r1

x104:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x105:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x106:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x107:	sh3add	%arg0,%arg0,%r1		! sh2add	%arg0,%r1,%r1	! b	e_t02a0	! sh3add	%r1,%arg0,%r1

x108:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x109:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x110:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_2t0	! sh1add	%r1,%arg0,%r1

x111:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x112:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! zdep	%r1,27,28,%r1

x113:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_t02a0	!	sh1add	%r1,%r1,%r1

x114:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_2t0	!	sh1add	%r1,%r1,%r1

x115:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_2t0a0	!	sh1add	%r1,%r1,%r1

x116:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_4t0	! sh2add	%r1,%arg0,%r1

x117:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	!	sh3add	%r1,%r1,%r1

x118:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_t0a0	!	sh3add	%r1,%r1,%r1

x119:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_t02a0	!	sh3add	%r1,%r1,%r1

x120:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x121:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x122:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x123:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x124: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x125:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x126: zdep	%arg0,25,26,%r1	! sub	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x127: zdep	%arg0,24,25,%r1	! comb,<>	%arg1,0,l0	! sub	%r1,%arg0,%r1	! b,n	ret_t0

x128: zdep	%arg0,24,25,%r1	! comb,<>	%arg1,0,l1	!	add	%ret1,%r1,%ret1	! bv,n  0(%rp)

x129: zdep	%arg0,24,25,%r1	! comb,<>	%arg1,0,l0	! add	%r1,%arg0,%r1	! b,n	ret_t0

x130: zdep	%arg0,25,26,%r1	! add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x131:	sh3add	%arg0,0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x132:	sh3add	%arg0,0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x133:	sh3add	%arg0,0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x134:	sh3add	%arg0,0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_2t0	! sh1add	%r1,%arg0,%r1

x135:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x136:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x137:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x138:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x139:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_2t0a0	! sh2add	%r1,%arg0,%r1

x140:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_4t0	!	sh2add	%r1,%r1,%r1

x141:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_4t0a0	! sh1add	%r1,%arg0,%r1

x142:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,0,%r1	!	b	e_2t0	! sub	%r1,%arg0,%r1

x143: zdep	%arg0,27,28,%r1	!	sh3add	%r1,%r1,%r1	!	b	e_t0	! sub	%r1,%arg0,%r1

x144:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x145:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x146:	sh3add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x147:	sh3add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x148:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x149:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x150:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_2t0	! sh1add	%r1,%arg0,%r1

x151:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_2t0a0	! sh1add	%r1,%arg0,%r1

x152:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x153:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x154:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x155: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x156:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_4t0	! sh1add	%r1,%arg0,%r1

x157: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	! b	e_t02a0	!	sh2add	%r1,%r1,%r1

x158: zdep	%arg0,27,28,%r1	!	sh2add	%r1,%r1,%r1	!	b	e_2t0	! sub	%r1,%arg0,%r1

x159: zdep	%arg0,26,27,%r1	!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sub	%r1,%arg0,%r1

x160:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x161:	sh3add	%arg0,0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x162:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x163:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	!	b	e_t0	! sh1add	%r1,%arg0,%r1

x164:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x165:	sh3add	%arg0,0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x166:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_2t0	! sh1add	%r1,%arg0,%r1

x167:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	! b	e_2t0a0	! sh1add	%r1,%arg0,%r1

x168:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x169:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x170: zdep	%arg0,26,27,%r1	! sh1add	%arg0,%r1,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x171:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_t0	!	sh3add	%r1,%r1,%r1

x172:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_4t0	! sh1add	%r1,%arg0,%r1

x173:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_t02a0	!	sh3add	%r1,%r1,%r1

x174: zdep	%arg0,26,27,%r1	! sh1add	%arg0,%r1,%r1	! b	e_t04a0	!	sh2add	%r1,%r1,%r1

x175:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_5t0	! sh1add	%r1,%arg0,%r1

x176:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_8t0	! add	%r1,%arg0,%r1

x177:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_8t0a0	! add	%r1,%arg0,%r1

x178:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_2t0	! sh3add	%r1,%arg0,%r1

x179:	sh2add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_2t0a0	! sh3add	%r1,%arg0,%r1

x180:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x181:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x182:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_2t0	! sh1add	%r1,%arg0,%r1

x183:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_2t0a0	! sh1add	%r1,%arg0,%r1

x184:	sh2add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	!	b	e_4t0	! add	%r1,%arg0,%r1

x185:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x186: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	!	b	e_2t0	!	sh1add	%r1,%r1,%r1

x187:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_t02a0	!	sh2add	%r1,%r1,%r1

x188:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_4t0	! sh1add	%arg0,%r1,%r1

x189:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_t0	!	sh3add	%r1,%r1,%r1

x190:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_2t0	!	sh2add	%r1,%r1,%r1

x191: zdep	%arg0,25,26,%r1	!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sub	%r1,%arg0,%r1

x192:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x193:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x194:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x195:	sh3add	%arg0,0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x196:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_4t0	! sh1add	%r1,%arg0,%r1

x197:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_4t0a0	! sh1add	%r1,%arg0,%r1

x198: zdep	%arg0,25,26,%r1	! sh1add	%arg0,%r1,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x199:	sh3add	%arg0,0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_2t0a0	!	sh1add	%r1,%r1,%r1

x200:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x201:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x202:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x203:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_2t0a0	! sh2add	%r1,%arg0,%r1

x204:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_4t0	!	sh1add	%r1,%r1,%r1

x205:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x206: zdep	%arg0,25,26,%r1	! sh2add	%arg0,%r1,%r1	! b	e_t02a0	!	sh1add	%r1,%r1,%r1

x207:	sh3add	%arg0,0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_3t0	! sh2add	%r1,%arg0,%r1

x208:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_8t0	! add	%r1,%arg0,%r1

x209:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_8t0a0	! add	%r1,%arg0,%r1

x210:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_2t0	!	sh2add	%r1,%r1,%r1

x211:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_2t0a0	!	sh2add	%r1,%r1,%r1

x212:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_4t0	! sh2add	%r1,%arg0,%r1

x213:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_4t0a0	! sh2add	%r1,%arg0,%r1

x214:	sh3add	%arg0,%arg0,%r1		! sh2add	%arg0,%r1,%r1	! b	e2t04a0	! sh3add	%r1,%arg0,%r1

x215:	sh2add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_5t0	! sh1add	%r1,%arg0,%r1

x216:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x217:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x218:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_2t0	! sh2add	%r1,%arg0,%r1

x219:	sh3add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x220:	sh1add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	!	b	e_4t0	! sh1add	%r1,%arg0,%r1

x221:	sh1add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	! b	e_4t0a0	! sh1add	%r1,%arg0,%r1

x222:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_2t0	!	sh1add	%r1,%r1,%r1

x223:	sh3add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_2t0a0	!	sh1add	%r1,%r1,%r1

x224:	sh3add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_8t0	! add	%r1,%arg0,%r1

x225:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_t0	!	sh2add	%r1,%r1,%r1

x226:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_t02a0	! zdep	%r1,26,27,%r1

x227:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_t02a0	!	sh2add	%r1,%r1,%r1

x228:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_4t0	!	sh1add	%r1,%r1,%r1

x229:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_4t0a0	!	sh1add	%r1,%r1,%r1

x230:	sh3add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_5t0	! add	%r1,%arg0,%r1

x231:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_3t0	! sh2add	%r1,%arg0,%r1

x232:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	!	b	e_8t0	! sh2add	%r1,%arg0,%r1

x233:	sh1add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e_8t0a0	! sh2add	%r1,%arg0,%r1

x234:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	!	b	e_2t0	!	sh3add	%r1,%r1,%r1

x235:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e_2t0a0	!	sh3add	%r1,%r1,%r1

x236:	sh3add	%arg0,%arg0,%r1		! sh1add	%r1,%arg0,%r1	! b	e4t08a0	!	sh1add	%r1,%r1,%r1

x237: zdep	%arg0,27,28,%r1	!	sh2add	%r1,%r1,%r1	!	b	e_3t0	! sub	%r1,%arg0,%r1

x238:	sh1add	%arg0,%arg0,%r1		! sh2add	%r1,%arg0,%r1	! b	e2t04a0	!	sh3add	%r1,%r1,%r1

x239: zdep	%arg0,27,28,%r1	!	sh2add	%r1,%r1,%r1	! b	e_t0ma0	!	sh1add	%r1,%r1,%r1

x240:	sh3add	%arg0,%arg0,%r1		! add	%r1,%arg0,%r1	!	b	e_8t0	!	sh1add	%r1,%r1,%r1

x241:	sh3add	%arg0,%arg0,%r1		! add	%r1,%arg0,%r1	! b	e_8t0a0	!	sh1add	%r1,%r1,%r1

x242:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_2t0	! sh3add	%r1,%arg0,%r1

x243:	sh3add	%arg0,%arg0,%r1		!	sh3add	%r1,%r1,%r1	!	b	e_t0	!	sh1add	%r1,%r1,%r1

x244:	sh2add	%arg0,%arg0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_4t0	! sh2add	%r1,%arg0,%r1

x245:	sh3add	%arg0,0,%r1		!	sh1add	%r1,%r1,%r1	!	b	e_5t0	! sh1add	%r1,%arg0,%r1

x246:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	!	b	e_2t0	!	sh1add	%r1,%r1,%r1

x247:	sh2add	%arg0,%arg0,%r1		! sh3add	%r1,%arg0,%r1	! b	e_2t0a0	!	sh1add	%r1,%r1,%r1

x248: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	! b	e_shift	! sh3add	%r1,%ret1,%ret1

x249: zdep	%arg0,26,27,%r1	! sub	%r1,%arg0,%r1	!	b	e_t0	! sh3add	%r1,%arg0,%r1

x250:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	!	b	e_2t0	!	sh2add	%r1,%r1,%r1

x251:	sh2add	%arg0,%arg0,%r1		!	sh2add	%r1,%r1,%r1	! b	e_2t0a0	!	sh2add	%r1,%r1,%r1

x252: zdep	%arg0,25,26,%r1	! sub	%r1,%arg0,%r1	! b	e_shift	! sh2add	%r1,%ret1,%ret1

x253: zdep	%arg0,25,26,%r1	! sub	%r1,%arg0,%r1	!	b	e_t0	! sh2add	%r1,%arg0,%r1

x254: zdep	%arg0,24,25,%r1	! sub	%r1,%arg0,%r1	! b	e_shift	! sh1add	%r1,%ret1,%ret1

x255: zdep	%arg0,23,24,%r1	! comb,<>	%arg1,0,l0	! sub	%r1,%arg0,%r1	! b,n	ret_t0

;1040 insts before this.
ret_t0: bv	0(%rp)

e_t0:	add	%ret1,%r1,%ret1

e_shift: comb,<>	%arg1,0,l2

	zdep	%arg0,23,24,%arg0	; %arg0 <<= 8 ***********
	bv,n  0(%rp)
e_t0ma0: comb,<>	%arg1,0,l0

	sub	%r1,%arg0,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e_t0a0: comb,<>	%arg1,0,l0

	add	%r1,%arg0,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e_t02a0: comb,<>	%arg1,0,l0

	sh1add	%arg0,%r1,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e_t04a0: comb,<>	%arg1,0,l0

	sh2add	%arg0,%r1,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e_2t0: comb,<>	%arg1,0,l1

	sh1add	%r1,%ret1,%ret1
	bv,n  0(%rp)
e_2t0a0: comb,<>	%arg1,0,l0

	sh1add	%r1,%arg0,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e2t04a0: sh1add	%arg0,%r1,%r1

	comb,<>	%arg1,0,l1
	sh1add	%r1,%ret1,%ret1
	bv,n  0(%rp)
e_3t0: comb,<>	%arg1,0,l0

	sh1add	%r1,%r1,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e_4t0: comb,<>	%arg1,0,l1

	sh2add	%r1,%ret1,%ret1
	bv,n  0(%rp)
e_4t0a0: comb,<>	%arg1,0,l0

	sh2add	%r1,%arg0,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e4t08a0: sh1add	%arg0,%r1,%r1

	comb,<>	%arg1,0,l1
	sh2add	%r1,%ret1,%ret1
	bv,n  0(%rp)
e_5t0: comb,<>	%arg1,0,l0

	sh2add	%r1,%r1,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1
e_8t0: comb,<>	%arg1,0,l1

	sh3add	%r1,%ret1,%ret1
	bv,n  0(%rp)
e_8t0a0: comb,<>	%arg1,0,l0

	sh3add	%r1,%arg0,%r1
	bv	0(%rp)
	add	%ret1,%r1,%ret1

	.procend
	.end

	.import $$divI_2,millicode
	.import $$divI_3,millicode
	.import $$divI_4,millicode
	.import $$divI_5,millicode
	.import $$divI_6,millicode
	.import $$divI_7,millicode
	.import $$divI_8,millicode
	.import $$divI_9,millicode
	.import $$divI_10,millicode
	.import $$divI_12,millicode
	.import $$divI_14,millicode
	.import $$divI_15,millicode
	.export $$divI,millicode
	.export	$$divoI,millicode
$$divoI:
	.proc
	.callinfo	NO_CALLS
	comib,=,n  -1,%arg1,negative1	; when divisor == -1
$$divI:
	comib,>>=,n 15,%arg1,small_divisor
	add,>=	0,%arg0,%ret1		; move dividend, if %ret1 < 0,
normal1:
	  sub	0,%ret1,%ret1		;   make it positive
	sub	0,%arg1,%r1		; clear carry,
					;   negate the divisor
	ds	0,%r1,0		; set V-bit to the comple-
					;   ment of the divisor sign
	add	%ret1,%ret1,%ret1	; shift msb bit into carry
	ds	%r0,%arg1,%r1		; 1st divide step, if no carry
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 2nd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 3rd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 4th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 5th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 6th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 7th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 8th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 9th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 10th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 11th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 12th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 13th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 14th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 15th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 16th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 17th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 18th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 19th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 20th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 21st divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 22nd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 23rd divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 24th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 25th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 26th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 27th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 28th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 29th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 30th divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 31st divide step
	addc	%ret1,%ret1,%ret1	; shift %ret1 with/into carry
	ds	%r1,%arg1,%r1		; 32nd divide step,
	addc	%ret1,%ret1,%ret1	; shift last %ret1 bit into %ret1
	xor,>=	%arg0,%arg1,0		; get correct sign of quotient
	  sub	0,%ret1,%ret1		;   based on operand signs
	bv,n  0(%rp)
	nop
;______________________________________________________________________
small_divisor:
	depd	%r0,31,32,%arg1
	blr,n	%arg1,%r0
	nop
; table for divisor == 0,1, ... ,15
	addit,=	0,%arg1,%r0	; trap if divisor == 0
	nop
	bv	%r0(%rp)	; divisor == 1
	copy	%arg0,%ret1
	 b,n	$$divI_2	; divisor == 2
	nop
	 b,n	$$divI_3	; divisor == 3
	nop
	 b,n	$$divI_4	; divisor == 4
	nop
	 b,n	$$divI_5	; divisor == 5
	nop
	 b,n	$$divI_6	; divisor == 6
	nop
	 b,n	$$divI_7	; divisor == 7
	nop
	 b,n	$$divI_8	; divisor == 8
	nop
	 b,n	$$divI_9	; divisor == 9
	nop
	 b,n	$$divI_10	; divisor == 10
	nop
	b	normal1		; divisor == 11
	add,>=	0,%arg0,%ret1
	 b,n	$$divI_12	; divisor == 12
	nop
	b	normal1		; divisor == 13
	add,>=	0,%arg0,%ret1
	 b,n	$$divI_14	; divisor == 14
	nop
	 b,n	$$divI_15	; divisor == 15
	nop
;______________________________________________________________________
negative1:
	sub	%r0,%arg0,%ret1	; result is negation of dividend
	bv	0(%rp)
	addo	%arg0,%arg1,%r0	; trap iff dividend==0x80000000 && divisor==-1
	.procend
	.end
