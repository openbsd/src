/*	$NetBSD: bcopy.s,v 1.6 1996/01/26 08:11:46 phil Exp $	*/

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
 * 	File: ns532/bcopy.s
 *	Author: Tatu Ylonen, Jukka Virtanen
 *	Helsinki University of Technology 1992.
 */

	.text

/* ovbcopy (from, to, bcount)  -- does overlapping stuff */

ENTRY(bcopy)
ENTRY(ovbcopy)
	enter	[],0
	movd	B_ARG0,r1  /* from */
	movd	B_ARG1,r2  /* to */
	cmpd	r2, r1
	bls	common	   /* safe to do standard thing */
	movd	B_ARG2,r0  /* bcount */
	addd	r1, r0     /* add to start of from */
	cmpd	r2, r0
	bhs	common	   /* safe to do standard thing */

/* Must do a reverse copy  (and assume that the start is on a
	word boundry . . . so we move the "remaining" bytes first) */

	movd	B_ARG2, r0 /* bcount */
	addqd	-1, r0
	addd	r0, r1
	addd	r0, r2
	addqd	1, r0
	andd	3, r0
	movsb	b	   /* move bytes backwards */
	addqd	-3, r1	   /* change to double pointer */
	addqd	-3, r2	   /* ditto */
	movd	B_ARG2, r0
	lshd	-2,r0
	movsd	b	   /* move words backwards */
	exit	[]
	ret	0

/* bcopy(from, to, bcount) -- non overlapping */

/* ENTRY(bcopy)  -- same as ovbcopy until furthur notice.... */
	enter	[],0
	movd	B_ARG0,r1  /* from */
	movd	B_ARG1,r2  /* to */
common:	movd	B_ARG2,r0  /* bcount */
	lshd	-2,r0
	movsd		   /* move words */
	movd	B_ARG2,r0
	andd	3,r0
	movsb		   /* move bytes */
	exit	[]
	ret	0
