/*	$OpenBSD: psl.h,v 1.11 2001/11/23 00:47:47 miod Exp $	*/
/*	$NetBSD: psl.h,v 1.14 1998/11/24 17:07:54 kleink Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef	_SUN3_PSL_H_
#define	_SUN3_PSL_H_
 
#include <m68k/psl.h>

/* Could define this in the common <m68k/psl.h> instead. */

#if defined(_KERNEL) && !defined(_LOCORE)

#ifndef __GNUC__
/* No inline, use the real functions in locore.s */
extern int _getsr __P((void));
extern int _spl __P((int new));
extern int _splraise __P((int new));
#else	/* GNUC */
/*
 * Define inline functions for PSL manipulation.
 * These are as close to macros as one can get.
 * When not optimizing gcc will call the locore.s
 * functions by the same names, so breakpoints on
 * these functions will work normally, etc.
 * (See the GCC extensions info document.)
 */

static __inline int _getsr __P((void));
static __inline int _spl __P((int));
static __inline int _splraise __P((int));

/* Get current sr value. */
static __inline int
_getsr(void)
{
	register int rv;

	__asm __volatile ("clrl %0; movew sr,%0" : "=&d" (rv));
	return (rv);
}

/* Set the current sr and return the old value. */
static __inline int
_spl(int new)
{
	register int old;

	__asm __volatile (
		"clrl %0; movew sr,%0; movew %1,sr" :
			"=&d" (old) : "di" (new));
	return (old);
}

/*
 * Like _spl() but can be used in places where the
 * interrupt priority may already have been raised,
 * without risk of enabling interrupts by accident.
 * The comparison includes the "S" bit (always on)
 * because that generates more efficient code.
 */
static __inline int
_splraise(int new)
{
	register int old;

	__asm __volatile ("clrl %0; movew sr,%0" : "=&d" (old));
	if ((old & PSL_HIGHIPL) < new) {
		__asm __volatile ("movew %0,sr;" : : "di" (new));
	}
	return (old);
}
#endif	/* GNUC */

/*
 * The rest of this is sun3 specific, because other ports may
 * need to do special things in spl0() (i.e. simulate SIR).
 * Suns have a REAL interrupt register, so spl0() and splx(s)
 * have no need to check for any simulated interrupts, etc.
 */

#define spl0()  _spl(PSL_S|PSL_IPL0)
#define spl1()  _spl(PSL_S|PSL_IPL1)
#define spl2()  _spl(PSL_S|PSL_IPL2)
#define spl3()  _spl(PSL_S|PSL_IPL3)
#define spl4()  _spl(PSL_S|PSL_IPL4)
#define spl5()  _spl(PSL_S|PSL_IPL5)
#define spl6()  _spl(PSL_S|PSL_IPL6)
#define spl7()  _spl(PSL_S|PSL_IPL7)
#define splx(x)	_spl(x)

/* IPL used by soft interrupts: netisr, softclock() */
#define spllowersoftclock()  spl1()
#define splsoftclock()  spl1()
#define splsoftnet()    spl1()

/* Highest block device (strategy) IPL. */
#define splbio()        spl2()

/* Highest network interface IPL. */
#define splnet()        spl3()

/* Highest tty device IPL. */
#define spltty()        spl4()

/*
 * Requirement: imp >= (highest network, tty, or disk IPL)
 * This is used mostly in the VM code. (Why not splvm?)
 * Note that the VM code runs at spl7 during kernel
 * initialization, and later at spl0, so we have to 
 * use splraise to avoid enabling interrupts early.
 */
#define splimp()        _splraise(PSL_S|PSL_IPL4)
#define splvm()         _splraise(PSL_S|PSL_IPL4)

/* Intersil clock hardware interrupts (hard-wired at 5) */
#define splclock()      spl5()
#define splstatclock()  splclock()

/* Zilog Serial hardware interrupts (hard-wired at 6) */
#define splzs()		spl6()

/* Block out all interrupts (except NMI of course). */
#define splhigh()       spl7()

#endif	/* KERNEL && !_LOCORE */
#endif	/* _SUN3_PSL_H_ */
