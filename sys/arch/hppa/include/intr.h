/*	$OpenBSD: intr.h,v 1.8 1999/08/14 03:36:11 mickey Exp $	*/

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

#include <machine/psl.h>

#define	CPU_NINTS	32
#define	CPU_INTITMR	(0x80000000)

#define	IPL_NONE	(0xffffffff)
#define	IPL_BIO		(0x80000000)
#define	IPL_NET		(0x80000000)
#define	IPL_TTY		(0x80000000)
#define	IPL_IO		(IPL_BIO|IPL_NET|IPL_TTY)
#define	IPL_CLOCK	(0)
#define	IPL_HIGH	(0)

#define	IST_NONE	0
#define	IST_PULSE	1
#define	IST_EDGE	2
#define	IST_LEVEL	3

#if !defined(_LOCORE)
/*
 * Define the machine-independent SPL routines in terms of splx().
 */
#define __splhigh(splhval)	({					\
	register u_int _ctl_r;						\
	__asm __volatile("mfctl	%%cr15,%0\n\t"				\
			 "mtctl	%1,%%cr15"				\
			: "=&r" (_ctl_r): "r" (splhval));		\
	_ctl_r;								\
})

#define __spllow(spllval)	({					\
	register u_int _ctl_r;						\
	__asm __volatile("mfctl	%%cr15,%0\n\t"				\
			 "mtctl	%1,%%cr15"				\
			: "=&r" (_ctl_r): "r" (spllval));		\
	_ctl_r;								\
})

#define	splx(splval)		({					\
	register u_int _ctl_r;						\
	__asm __volatile("mfctl   %%cr15,%0\n\t"			\
			 "mtctl   %1,%%cr15\n\t"			\
			 : "=&r" (_ctl_r): "r" (splval));		\
	_ctl_r;								\
})

#define	spl0()		__spllow(IPL_NONE)
#define	splsoft()	__spllow(IPL_CLOCK)
#define	splsoftnet()	splsoft()
#define	splsoftclock()	splsoft()
#define	splbio()	__spllow(IPL_BIO)
#define	splnet()	__spllow(IPL_NET)
#define	spltty()	__spllow(IPL_TTY)
#define	splimp()	__spllow(IPL_CLOCK)
#define	splclock()	__spllow(IPL_CLOCK)
#define	splstatclock()	__spllow(IPL_CLOCK)
#define	splhigh()	__splhigh(IPL_HIGH)

/* software interrupt register */
extern u_int32_t sir;

#define	SIR_CLOCK	0x01
#define	SIR_NET		0x02

#define	setsoftclock()		(sir |= SIR_CLOCK)
#define	setsoftnet()		(sir |= SIR_NET)

#endif	/* !_LOCORE */
#endif	/* _MACHINE_INTR_H_ */
