/*	$OpenBSD: asm.h,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

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
 *	Utah $Hdr: asm.h 1.8 94/12/14$
 */

#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

/*
 *	hppa assembler definitions
 */

arg7	.reg	%r19
arg6	.reg	%r20
arg5	.reg	%r21
arg4	.reg	%r22
ap	.reg	%r29

#ifdef __STDC__
#define	__CONCAT(a,b)	a ## b
#else
#define	__CONCAT(a,b)	a/**/b
#endif

#ifdef PROF
#define	_PROF_PROLOGUE				\
	stw	%rp, HPPA_FRAME_CRP(%sr0,%sp)	!\
	ldil	L%_mcount,%r1			!\
	ble	R%_mcount(%sr0,%r1)		!\
	ldo	HPPA_FRAME_SIZE(%sp),%sp	!\
	ldw 	PPA_FRAME_CRP(%sr0,%sp),%rp
#else
#define	_PROF_PROLOGUE
#endif

#define	LEAF_ENTRY(x) ! .text ! .align	4	!\
	.export	x, entry ! .label x ! .proc	!\
	.callinfo frame=0,no_calls,save_rp	!\
	.entry ! _PROF_PROLOGUE

#define	ENTRY(x,n) ! .text ! .align 4			!\
	.export	x, entry ! .label x ! .proc		!\
	.callinfo frame=n,calls, save_rp, save_sp	!\
	.entry ! _PROF_PROLOGUE

#define ALTENTRY(x) ! .export x, entry ! .label  x
#define EXIT(x) ! .exit ! .procend ! .size   x, .-x

#define	BSS(n,s)	! .data ! .label n ! .comm s

#endif /* _MACHINE_ASM_H_ */
