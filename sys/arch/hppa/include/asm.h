/*	$OpenBSD: asm.h,v 1.6 1999/01/16 07:58:32 mickey Exp $	*/

/* 
 * Copyright (c) 1990,1991,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: asm.h 1.8 94/12/14$
 */

#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

/*
 *	hppa assembler definitions
 */

/*
 * Hardware General Registers
 */
r0	.reg	%r0
r1	.reg	%r1
r2	.reg	%r2
r3	.reg	%r3
r4	.reg	%r4
r5	.reg	%r5
r6	.reg	%r6
r7	.reg	%r7
r8	.reg	%r8
r9	.reg	%r9
r10	.reg	%r10
r11	.reg	%r11
r12	.reg	%r12
r13	.reg	%r13
r14	.reg	%r14
r15	.reg	%r15
r16	.reg	%r16
r17	.reg	%r17
r18	.reg	%r18
r19	.reg	%r19
r20	.reg	%r20
r21	.reg	%r21
r22	.reg	%r22
r23	.reg	%r23
r24	.reg	%r24
r25	.reg	%r25
r26	.reg	%r26
r27	.reg	%r27
r28	.reg	%r28
r29	.reg	%r29
r30	.reg	%r30
r31	.reg	%r31

/*
 * Hardware Space Registers
 */
sr0	.reg	%sr0
sr1	.reg	%sr1
sr2	.reg	%sr2
sr3	.reg	%sr3
sr4	.reg	%sr4
sr5	.reg	%sr5
sr6	.reg	%sr6
sr7	.reg	%sr7

/*
 * Hardware Floating Point Registers
 */
fr0	.reg	%fr0
fr1	.reg	%fr1
fr2	.reg	%fr2
fr3	.reg	%fr3
fr4	.reg	%fr4
fr5	.reg	%fr5
fr6	.reg	%fr6
fr7	.reg	%fr7
fr8	.reg	%fr8
fr9	.reg	%fr9
fr10	.reg	%fr10
fr11	.reg	%fr11
fr12	.reg	%fr12
fr13	.reg	%fr13
fr14	.reg	%fr14
fr15	.reg	%fr15
fr16	.reg	%fr16
fr17	.reg	%fr17
fr18	.reg	%fr18
fr19	.reg	%fr19
fr20	.reg	%fr20
fr21	.reg	%fr21
fr22	.reg	%fr22
fr23	.reg	%fr23
fr24	.reg	%fr24
fr25	.reg	%fr25
fr26	.reg	%fr26
fr27	.reg	%fr27
fr28	.reg	%fr28
fr29	.reg	%fr29
fr30	.reg	%fr30
fr31	.reg	%fr31

/*
 * Hardware Control Registers
 */
cr0	.reg	%cr0
cr8	.reg	%cr8
cr9	.reg	%cr9
cr10	.reg	%cr10
cr11	.reg	%cr11
cr12	.reg	%cr12
cr13	.reg	%cr13
cr14	.reg	%cr14
cr15	.reg	%cr15
cr16	.reg	%cr16
cr17	.reg	%cr17
cr18	.reg	%cr18
cr19	.reg	%cr19
cr20	.reg	%cr20
cr21	.reg	%cr21
cr22	.reg	%cr22
cr23	.reg	%cr23
cr24	.reg	%cr24
cr25	.reg	%cr25
cr26	.reg	%cr26
cr27	.reg	%cr27
cr28	.reg	%cr28
cr29	.reg	%cr29
cr30	.reg	%cr30
cr31	.reg	%cr31

rctr	.reg	%cr0
pidr1	.reg	%cr8
pidr2	.reg	%cr9
ccr	.reg	%cr10
sar	.reg	%cr11
pidr3	.reg	%cr12
pidr4	.reg	%cr13
iva	.reg	%cr14
eiem	.reg	%cr15
itmr	.reg	%cr16
pcsq	.reg	%cr17
pcoq	.reg	%cr18
iir	.reg	%cr19
isr	.reg	%cr20
ior	.reg	%cr21
ipsw	.reg	%cr22
eirr	.reg	%cr23
hptmask	.reg	%cr24
tr0	.reg	%cr24
vtop	.reg	%cr25
tr1	.reg	%cr25
tr2	.reg	%cr26
tr3	.reg	%cr27
tr4	.reg	%cr28
tr5	.reg	%cr29
tr6	.reg	%cr30
tr7	.reg	%cr31

/*
 * CPU-secific additional control registers
 */
#define	dtlb_reg	8
#define	dtlb_size_even	26
#define	dtlb_size_odd	27
#define	itlb_reg	9
#define	itlb_size_even	24
#define	itlb_size_odd	25

/*
 * Calling Convention
 */
rp	.reg	%r2
arg3	.reg	%r23
arg2	.reg	%r24
arg1	.reg	%r25
arg0	.reg	%r26
dp	.reg	%r27
ret0	.reg	%r28
ret1	.reg	%r29
sl	.reg	%r29
sp	.reg	%r30

/*
 * Temporary registers
 */
t1	.reg	%r22
t2	.reg	%r21
t3	.reg	%r20
t4	.reg	%r19

/*
 * Temporary space registers
 */
ts1	.reg	%sr2

/*
 * Space Registers - SW Conventions
 */
sret	.reg	%sr1	; return value
sarg	.reg	%sr1	; argument

/*
 * Floating Point Registers - SW Conventions
 */
farg0	.reg	%fr5
farg1	.reg	%fr6
farg2	.reg	%fr7
farg3	.reg	%fr8
fret	.reg	%fr4

/*
 * Temporary floating point registers
 */
tf1	.reg	%fr11
tf2	.reg	%fr10
tf3	.reg	%fr9
tf4	.reg	%fr8

/*
 * Frame offsets to procedure status save and return areas
 */
#define FM_PSP  -4
#define FM_EP   -8
#define FM_CLUP	-12
#define FM_SL	-16
#define FM_CRP	-20
#define FM_ERP	-24
#define FM_ESR4	-28
#define FM_EDP	-32
#define FM_SIZE	32

/*
 * frame must be rounded to double word
 */
#define FR_ROUND 8

#define NARGS		12
#define ARG_SIZE	(NARGS*4)

/*
 * Offsets from sp/previous_sp for access to procedure arguments
 * A maximum of four args may be passed in registers.
 */
#define VA_ARG0	-FM_SIZE-4
#define VA_ARG1	-FM_SIZE-8
#define VA_ARG2	-FM_SIZE-12
#define VA_ARG3	-FM_SIZE-16

#define VA_ARG4		-FM_SIZE-20
#define VA_ARG5		-FM_SIZE-24
#define VA_ARG6		-FM_SIZE-28
#define VA_ARG7		-FM_SIZE-32
#define VA_ARG8		-FM_SIZE-36
#define VA_ARG9		-FM_SIZE-40
#define VA_ARG10	-FM_SIZE-44
#define VA_ARG11	-FM_SIZE-48
#define VA_ARG12	-FM_SIZE-52
#define VA_ARG13	-FM_SIZE-56

#ifdef __STDC__
#define	__CONCAT(a,b)	a ## b
#else
#define	__CONCAT(a,b)	a/**/b
#endif

/*
 * Standard space and subspace definitions.
 */
	.SPACE	$TEXT$,0
/*	.subspa $FIRST$,	QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=4 */
	.subspa $MILLICODE$,	QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=8
	.subspa $LIT$,		QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=16
	.subspa $CODE$,		QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=24,CODE_ONLY
/*	.subspa	$UNWIND$MILLICODE$,QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=64
	.subspa	$UNWIND$,	QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=72
	.subspa	$RECOVER$,	QUAD=0,ALIGN=4,ACCESS=0x2c,SORT=80
*/

/*
 * additional code subspaces should have ALIGN=8 for an interspace BV
 */
	.SPACE $PRIVATE$,1
/*	.subspa $GLOBAL$,	QUAD=1,ALIGN=8,ACCESS=0x1f,SORT=8 */
/*	.subspa $SHORTDATA$,	QUAD=1,ALIGN=8,ACCESS=0x1f,SORT=16 */
	.subspa $DATA$,		QUAD=1,ALIGN=8,ACCESS=0x1f,SORT=24
	.import $global$
/*	.subspa	$PFA_COUNTER$,	QUAD=1,ALIGN=4,ACCESS=0x1f,SORT=72 */
	.subspa $BSS$,		QUAD=1,ALIGN=8,ACCESS=0x1f,SORT=80,ZERO


#ifdef PROF
#define	_PROF_PROLOGUE !\
	stw rp, FM_CRP(sr0,sp)	!\
	ldil L%_mcount,r1	!\
	ble R%_mcount(sr0,r1)	!\
	ldo FM_SIZE(sp),sp	!\
	ldw FM_CRP(sr0,sp),rp
#else
#define	_PROF_PROLOGUE
#endif

#define	ENTRY(x)		!\
	.space	$text$		!\
	.subspa	$code$		!\
	.export	x,entry		!\
	.label	x               !\
	.proc			!\
	.callinfo calls		!\
	.entry			!\
	_PROF_PROLOGUE

#define ALTENTRY(x)		!\
	.export x,entry         !\
	.label  x

#define EXIT(x)			!\
	bv,n    r0(rp)          !\
	nop			!\
	.exit                   !\
	.procend

#endif /* _MACHINE_ASM_H_ */
