/*	$OpenBSD: copypage.s,v 1.5 2008/06/26 05:42:11 ray Exp $	*/
/*	$NetBSD: copypage.s,v 1.4 1997/05/30 01:34:49 jtc Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin <jtc@netbsd.org> and 
 * by Hiroshi Horitomo <horimoto@cs-aoi.cs.sist.ac.jp> 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Optimized functions for copying/clearing a whole page.
 */

#include <machine/asm.h>
#include "assym.h"

	.file	"copypage.s"
	.text

/*
 * copypage040(fromaddr, toaddr)
 *
 * Optimized version of bcopy for a single page-aligned NBPG byte copy,
 * using instructions only available on the mc68040 and later.
 */
#if defined(M68040) || defined(M68060)
ENTRY(copypage040)
	movl	sp@(4),a0		| source address
	movl	sp@(8),a1		| destination address
	movw	#NBPG/32-1,d0		| number of 32 byte chunks - 1
Lm16loop:
	.long	0xf6209000		| move16 a0@+,a1@+
	.long	0xf6209000		| move16 a0@+,a1@+
	dbf	d0,Lm16loop
	rts
#endif /* M68040 || M68060 */

/*
 * copypage(fromaddr, toaddr)
 *
 * Optimized version of bcopy for a single page-aligned NBPG byte copy.
 */
ENTRY(copypage)
	movl	sp@(4),a0		| source address
	movl	sp@(8),a1		| destination address
	movw	#NBPG/32-1,d0		| number of 32 byte chunks - 1
Lmlloop:
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	dbf	d0,Lmlloop
	rts

/*
 * zeropage(addr)
 *
 * Optimized version of bzero for a single page-aligned NBPG byte zero.
 */
ENTRY(zeropage)
	movl	sp@(4),a0		| dest address
	movql	#NBPG/256-1,d0		| number of 256 byte chunks - 1
	movml	d2-d7,sp@-
	movql	#0,d1
	movql	#0,d2
	movql	#0,d3
	movql	#0,d4
	movql	#0,d5
	movql	#0,d6
	movql	#0,d7
	movl	d1,a1
	lea	a0@(NBPG),a0
Lzloop:
	movml	d1-d7/a1,a0@-
	movml	d1-d7/a1,a0@-
	movml	d1-d7/a1,a0@-
	movml	d1-d7/a1,a0@-
	movml	d1-d7/a1,a0@-
	movml	d1-d7/a1,a0@-
	movml	d1-d7/a1,a0@-
	movml	d1-d7/a1,a0@-
	dbf	d0,Lzloop
	movml	sp@+,d2-d7
	rts
