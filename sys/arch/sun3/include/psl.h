/*	$NetBSD: psl.h,v 1.9 1996/02/01 22:33:10 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	PSL_C
#include <m68k/psl.h>

/* Could define this in the common <m68k/psl.h> instead. */

#if defined(_KERNEL) && !defined(_LOCORE)

#ifndef __GNUC__
/* No inline, use real function in locore.s */
extern int _spl(int new);
#else	/* GNUC */
/*
 * Define an inline function for PSL manipulation.
 * This is as close to a macro as one can get.
 * If not optimizing, the one in locore.s is used.
 * (See the GCC extensions info document.)
 */
extern __inline__ int _spl(int new)
{
	register int old;

	__asm __volatile (
		"clrl %0; movew sr,%0; movew %1,sr" :
			"&=d" (old) : "di" (new));
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

/* IPL used by soft interrupts: netintr(), softclock() */
#define splsoftclock()  spl1()
#define splsoftnet()    spl1()

/* Highest block device (strategy) IPL. */
#define splbio()        spl2()

/* Highest network interface IPL. */
#define splnet()        spl3()

/* Highest tty device IPL. */
#define spltty()        spl4()

/* Requirement: imp >= (highest network, tty, or disk IPL) */
#define splimp()        spl4()

/* Intersil clock hardware interrupts (hard-wired at 5) */
#define splclock()      spl5()
#define splstatclock()  splclock()

/* Zilog Serial hardware interrupts (hard-wired at 6) */
#define splzs()         spl6()

/* Block out all interrupts (except NMI of course). */
#define splhigh()       spl7()
#define splsched()      spl7()

#endif	/* KERNEL && !_LOCORE */
#endif	/* PSL_C */
