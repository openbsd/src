/*	$OpenBSD: ffs.S,v 1.1 1998/08/28 20:59:42 mickey Exp $	*/

/* 
 * Copyright (c) 1990, 1991, 1992, 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */

#include <machine/asm.h>

/*
 * ffs(bitmask)
 *
 * Return the position of the "most significant" bit in `bitmask'.
 * Since this is similar to the VAX ffs instruction, bits in a word
 * are numbered as "32, 31, ... 1", 0 is returned if no bits are set.
 */

ENTRY(ffs)
	comb,=	arg0,r0,ffsdone		; If arg0 is 0
	or	r0,r0,ret0		;   return 0
	ldi	32,ret0			; Set return to high bit
	extru,=	arg0,31,16,r0		; If low 16 bits are non-zero
	addi,tr	-16,ret0,ret0		;   subtract 16 from bitpos
	shd	r0,arg0,16,arg0		; else shift right 16 bits
	extru,=	arg0,31,8,r0		; If low 8 bits are non-zero
	addi,tr	-8,ret0,ret0		;   subtract 8 from bitpos
	shd	r0,arg0,8,arg0		; else shift right 8 bits
	extru,=	arg0,31,4,r0		; If low 4 bits are non-zero
	addi,tr	-4,ret0,ret0		;   subtract 4 from bitpos
	shd	r0,arg0,4,arg0		; else shift right 4 bits
	extru,=	arg0,31,2,r0		; If low 2 bits are non-zero
	addi,tr	-2,ret0,ret0		;   subtract 2 from bitpos
	shd	r0,arg0,2,arg0		; else shift right 2 bits
	extru,=	arg0,31,1,r0		; If low bit is non-zero
	addi	-1,ret0,ret0		;   subtract 1 from bitpos
ffsdone:
EXIT(ffs)
