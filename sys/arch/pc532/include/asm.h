/*	$NetBSD: asm.h,v 1.6 1996/01/26 08:10:10 phil Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND HELSINKI UNIVERSITY OF TECHNOLOGY ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND
 * HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

/*
 * 	File: asm.h
 *	Author: Johannes Helander, Tero Kivinen, Tatu Ylonen
 *	Modified by Phil Nelson for NetBSD.
 *	Modified by Matthias Pfaller for PIC.
 *	Helsinki University of Technology 1992.
 */

#ifndef _MACHINE_ASM_H_ 
#define _MACHINE_ASM_H_

#ifdef __STDC__
#define CAT(a, b)	a ## b
#define EX(x)		_ ## x
#define LEX(x)		_ ## x ## :
#else
#define CAT(a, b)	a/**/b
#define EX(x)		_/**/x
#define LEX(x)		_/**/x/**/:
#endif

#define FRAME	enter [],0
#define EMARF	exit []

#define S_ARG0	4(sp)
#define S_ARG1	8(sp)
#define S_ARG2	12(sp)
#define S_ARG3	16(sp)

#define B_ARG0	 8(fp)
#define B_ARG1	12(fp)
#define B_ARG2	16(fp)
#define B_ARG3	20(fp)

#define ALIGN 0

#ifdef PIC
#define PIC_PROLOGUE \
	sprd	sb,tos; \
	addr	__GLOBAL_OFFSET_TABLE_(pc),r1; \
	lprd	sb,r1
#define PIC_EPILOGUE \
	lprd	sb,tos
#define PIC_GOT(x)	0(x(sb))

#define PIC_S_ARG0	8(sp)
#define PIC_S_ARG1	12(sp)
#define PIC_S_ARG2	16(sp)
#define PIC_S_ARG3	20(sp)
#else
#define PIC_PROLOGUE
#define PIC_EPILOGUE
#define	PIC_GOT(x)	x(pc)

#define PIC_S_ARG0	4(sp)
#define PIC_S_ARG1	8(sp)
#define PIC_S_ARG2	12(sp)
#define PIC_S_ARG3	16(sp)
#endif

#ifdef PROF
#define	MC1	.data; 1:; .long 0; .text
#define MC2	addr 1b(pc),r0; bsr mcount
#else
#define MC1
#define MC2
#endif

#define	DECL(x)	MC1; .globl x; .type x,@function; .align ALIGN; CAT(x,:); MC2

#define	ENTRY(x)	DECL(EX(x))
#define	Entry(x)	DECL(EX(x))
#define ASENTRY(x)	DECL(x)
#define	ASMSTR		.asciz

#define	SVC svc

#endif
