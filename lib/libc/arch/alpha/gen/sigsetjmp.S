/*	$NetBSD: sigsetjmp.S,v 1.1 1995/02/10 17:50:27 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "DEFS.h"

/*
 * C library -- sigsetjmp, siglongjmp
 *
 *	siglongjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	sigsetjmp(a, mask)
 * by restoring registers from the stack.
 * If `mask' is non-zero, the previous signal
 * state will be restored.
 */

	.set	noreorder

LEAF(sigsetjmp, 2)
	SETGP(pv)
	stq	a1, (81 * 8)(a0)		/* save the mask */
	bne	a1, Lsavesig			/* if !zero, save signals */
	JMP(_setjmp)				/* else don't. */
Lsavesig:
	JMP(setjmp)
END(sigsetjmp)

LEAF(siglongjmp, 2)
	SETGP(pv)
	ldq	t0, (81 * 8)(a0)		/* get the mask */
	bne	t0, Lrestoresig			/* if !zero, restore signals */
	JMP(_longjmp)
Lrestoresig:
	JMP(longjmp)
END(siglongjmp)
