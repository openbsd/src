/*	$OpenBSD: bzero.s,v 1.3 2000/03/03 00:54:55 todd Exp $	*/
/*	$NetBSD: bzero.s,v 1.3 1996/01/26 08:11:47 phil Exp $	*/

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
 * 	File: ns532/bzero.s
 *	Author: Tero Kivinen, Helsinki University of Technology 1992.
 *
 */


/*
 * bzero(char * addr, unsigned int length)
 */

	.text
ENTRY(bzero)
	enter	[],0
	movd	B_ARG0,r1   /* addr */
	movd	B_ARG1,r2   /* length */
	movd	r1,r0       /* align addr */
	andd	3,r0
	cmpqd	0,r0
	beq	wstart      /* already aligned */
	negd	r0,r0
	addqd	4,r0
	cmpd	r0,r2
	bhi	bytes       /* not enough data to align */
b1loop:	movqb	0,0(r1)     /* zero bytes */
	addqd	1,r1
	addqd	-1,r2
	acbd	-1,r0,b1loop
wstart:	movd	r2,r0       /* length */
	lshd	-6,r0
	cmpqd	0,r0
	beq	phase2
w1loop:	movqd	0,0(r1)      /* zero words */
	movqd	0,4(r1)
	movqd	0,8(r1)
	movqd	0,12(r1)
	movqd	0,16(r1)
	movqd	0,20(r1)
	movqd	0,24(r1)
	movqd	0,28(r1)
	movqd	0,32(r1)
	movqd	0,36(r1)
	movqd	0,40(r1)
	movqd	0,44(r1)
	movqd	0,48(r1)
	movqd	0,52(r1)
	movqd	0,56(r1)
	movqd	0,60(r1)
	addd	64,r1
	acbd	-1,r0,w1loop
phase2:	movd	r2,r0       /* length */
	andd	63,r0
	lshd	-2,r0
	cmpqd	0,r0
	beq	bytes
w2loop:	movqd	0,0(r1)
	addqd	4,r1
	acbd	-1,r0,w2loop
bytes:	movd	r2,r0       /* length */
	andd	3,r0
	cmpqd	0,r0
	beq	done
bloop:	movqb	0,0(r1)      /* zero bytes */
	addqd	1,r1
	acbb	-1,r0,bloop
done:	exit	[]
	ret	0
