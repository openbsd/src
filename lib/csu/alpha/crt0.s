/*	$NetBSD: crt0.s,v 1.4 1995/11/04 00:30:50 cgd Exp $	*/

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

#include <machine/asm.h>

#define	SETGP(pv)	ldgp	gp,0(pv)

#define	MF_FPCR(x)	mf_fpcr	x
#define	MT_FPCR(x)	mt_fpcr	x
#define	JMP(loc)	br	zero,loc
#define	CONST(c,reg)	ldiq	reg, c

/*
 * Set up the global variables provided by crt0:
 *	environ and __progname
 */
BSS(environ, 8)
	.data
	.align	3
EXPORT(__progname)
	.quad	$L1
$L1:
	.quad	0		/* Null string, plus padding. */
	.text

LEAF(__start, 0)		/* XXX */
	.set	noreorder
	br	pv, 1f
1:	SETGP(pv)

	ldq	s0, 0(sp)	/* get argc from stack */
	lda	s1, 8(sp)	/* get pointer to argv */
	s8addq	s0, s1, s2	/* add number of argv pointers */
	addq	s2, 8, s2	/* and skip the null pointer */
	stq	s2, environ	/* save the newly-found env pointer */

#ifdef MCRT0
eprol:
	lda	a0, eprol
	lda	a1, etext
	CALL(monstartup)	/* monstartup(eprol, etext); */
	lda	a0, _mcleanup
	CALL(atexit)		/* atext(_mcleanup); */
	stl	zero, errno
#endif

	ldq	a0, 0(s1)	/* a0 = argv[0]; */
	beq	a0, 2f		/* if it's null, then punt */
	CONST(0x2f, a1)		/* a1 = '/' */
	CALL(strrchr)
	addq	v0, 1, a0	/* move past the /, if there was one */
	bne	v0, 1f		/* if a / found */
	ldq	a0, 0(s1)	/* a0 = argv[0]; */
1:
	stq	a0, __progname	/* store the program name */
2:
	/* call main() */
__callmain:
	mov	s0, a0
	mov	s1, a1
	mov	s2, a2
	CALL(main)		/* v0 = main(argc, argv, env); */

	/* on return from main, call exit() with its return value. */
	mov	v0, a0
	CALL(exit)		/* exit(rv); */

	/* if that failed, bug out. */
	call_pal 0x81	/* XXX op_bugchk */
	.set	reorder
END(__start)

#ifndef MCRT0
LEAF(moncontrol, 0)
	RET
END(moncontrol)

LEAF(_mcount, 0)
        ret     zero, (at_reg), 1
END(_mcount)
#endif
