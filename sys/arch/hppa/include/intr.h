/*	$OpenBSD: intr.h,v 1.4 1998/11/23 03:32:19 mickey Exp $	*/

/* 
 * Copyright (c) 1990,1991,1992,1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
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
 * 	Utah $Hdr: machspl.h 1.16 94/12/14$
 *	Author: Jeff Forys, Bob Wheeler, University of Utah CSL
 */

#ifndef	_MACHINE_INTR_H_
#define	_MACHINE_INTR_H_

#define	CPU_NINTS	32

/* hardwired clock int line */
#define	INT_ITMR	(0x80000000)

#define	IPL_NONE	0
#define	IPL_BIO		1
#define	IPL_NET		2
#define	IPL_TTY		3
#define	IPL_CLOCK	4
#define	IPL_HIGH	5

#define	IST_NONE	0
#define	IST_PULSE	1
#define	IST_EDGE	2
#define	IST_LEVEL	3


#if !defined(_LOCORE)
/*
 * Define the machine-independent SPL routines in terms of splx().
 * To prevent cluttering the global spl() namespace, routines that
 * need machine-dependent SPLs should roll their own.
 *
 * If compiling with GCC, it's easy to inline spl's with constant
 * arguments.  However, when the argument can be variable, there
 * is little or no win; as a result, splx() is not inline'd.
 */
#define __splhigh(splhval)	({				\
	register u_int _ctl_r;					\
	__asm __volatile("mfctl	%%cr15,%0" : "=r" (_ctl_r));	\
	__asm __volatile("mtctl	%0,%%cr15" :: "r" (splhval));	\
	__asm __volatile("rsm 1, %%r0"::);			\
	_ctl_r;							\
})

#define __spllow(spllval)	({				\
	register u_int _ctl_r;					\
	__asm __volatile("mfctl	%%cr15,%0" : "=r" (_ctl_r));	\
	__asm __volatile("mtctl	%0,%%cr15" :: "r" (spllval));	\
	__asm __volatile("ssm 1, %%r0"::);			\
	_ctl_r;							\
})

#define	spl0()		__spllow(0xffffffff)
#define	splsoft()	__spllow(INT_ITMR)
#define	splsoftnet()	splsoft()
#define	splsoftclock()	splsoft()
#define	splnet()	__spllow(INT_ITMR)
#define	splbio()	__spllow(INT_ITMR)
#define	splimp()	__spllow(INT_ITMR)
#define	spltty()	__spllow(INT_ITMR)
#define	splclock()	__spllow(0)
#define	splstatclock()	__spllow(0)
#define	splhigh()	__splhigh(0)
int	splx __P((int));

/* software interrupt register */
extern u_int32_t sir;

#define	SIR_CLOCK	0x01
#define	SIR_NET		0x02

#define	setsoftclock()		(sir |= SIR_CLOCK)
#define	setsoftnet()		(sir |= SIR_NET)

#endif	/* !_LOCORE */
#endif	/* _MACHINE_INTR_H_ */
