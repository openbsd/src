/* 
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * HELSINKI UNIVERSITY OF TECHNOLOGY ALLOWS FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION. HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIMS ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * 29-Apr-92  Johannes Helander (jvh) at Helsinki University of Technology
 *	Created.
 *
 *	$Id: sigprocmask.S,v 1.1.1.1 1995/10/18 08:41:39 deraadt Exp $
 */

/* 
 * Note: Has never been tested. Debug before use.
 */


#include "SYS.h"

ENTRY(sigprocmask)
	movd	S_ARG1, r1		/* fetch new sigset pointer */
	cmpqd	0, r1			/* check new sigset pointer */
	bne	L1			/* if not null, indirect */
/*	movqd	0, S_ARG1		/* null mask pointer: block empty set */
	movqd	1, S_ARG0		/* SIG_BLOCK */
	br	L2
L1:	movd	0(r1), r1		/* fetch indirect  ... */
	movd	r1, S_ARG1		/* to new mask arg */
L2:	movd	SYS_sigprocmask, r0
	SVC
	bcs	cerror
	movd	S_ARG2, r1		/* fetch old mask requested */
	cmpqd	0, r1			/* test if old mask requested */
	beq	out
	movd	r0, 0(r1)		/* store old mask */
out:
	movqd	0, r0
	ret	0
