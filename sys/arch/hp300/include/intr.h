/*	$OpenBSD: intr.h,v 1.12 2004/09/29 07:35:54 miod Exp $	*/
/*	$NetBSD: intr.h,v 1.2 1997/07/24 05:43:08 scottr Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HP300_INTR_H_
#define	_HP300_INTR_H_

#include <machine/psl.h>
#include <sys/evcount.h>
#include <sys/queue.h>

struct isr {
	LIST_ENTRY(isr) isr_link;
	int		(*isr_func)(void *);
	void		*isr_arg;
	int		isr_ipl;
	int		isr_priority;
	struct evcount	isr_count;
};

#ifdef _KERNEL
/*
 * spl functions; all but spl0 are done in-line
 */

#define	_spl(s)								\
({									\
	register int _spl_r;						\
									\
	__asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" :		\
	    "=&d" (_spl_r) : "di" (s));					\
	_spl_r;								\
})

#define	_splraise(s)							\
({									\
	int _spl_r;							\
									\
	__asm __volatile ("						\
		clrl	d0					;	\
		movw	sr,d0					;	\
		movl	d0,%0					;	\
		andw	#0x700,d0				;	\
		movw	%1,d1					;	\
		andw	#0x700,d1				;	\
		cmpw	d0,d1					;	\
		jle	1f					;	\
		movw	%1,sr					;	\
	    1:"							:	\
		    "=&d" (_spl_r)				:	\
		    "di" (s)					:	\
		    "d0", "d1");					\
	_spl_r;								\
})

/* spl0 requires checking for software interrupts */
#define	spl1()	_spl(PSL_S|PSL_IPL1)
#define	spl2()	_spl(PSL_S|PSL_IPL2)
#define	spl3()	_spl(PSL_S|PSL_IPL3)
#define	spl4()	_spl(PSL_S|PSL_IPL4)
#define	spl5()	_spl(PSL_S|PSL_IPL5)
#define	spl6()	_spl(PSL_S|PSL_IPL6)
#define	spl7()	_spl(PSL_S|PSL_IPL7)

/*
 * These four globals contain the appropriate PSL_S|PSL_IPL? values
 * to raise interupt priority to the requested level.
 */
extern	unsigned short hp300_bioipl;
extern	unsigned short hp300_netipl;
extern	unsigned short hp300_ttyipl;
extern	unsigned short hp300_impipl;

/*
 * Interrupt "levels".  These are a more abstract representation
 * of interrupt levels, and do not have the same meaning as m68k
 * CPU interrupt levels.  They serve two purposes:
 *
 *	- properly order ISRs in the list for that CPU ipl
 *	- compute CPU PSL values for the spl*() calls.
 */
#define	IPL_NONE	0
#define	IPL_SOFTNET	1
#define	IPL_SOFTCLOCK	1
#define	IPL_BIO		1
#define	IPL_NET		2
#define	IPL_TTY		3
#define	IPL_TTYNOBUF	4 /* XXX */
#define	IPL_CLOCK	6
#define	IPL_STATCLOCK	6
#define	IPL_HIGH	7

/* These spl calls are _not_ to be used by machine-independent code. */
#define	splhil()	_splraise(PSL_S|PSL_IPL1)
#define	splkbd()	splhil()
#define	splsoft()	spl1()

/* These spl calls are used by machine-independent code. */
#define	spllowersoftclock()	splsoft()
#define	splsoftclock()		splsoft()
#define	splsoftnet()		splsoft()
#define	splbio()		_splraise(hp300_bioipl)
#define	splnet()		_splraise(hp300_netipl)
#define	spltty()		_splraise(hp300_ttyipl)
#define	splimp()		_splraise(hp300_impipl)
#define	splclock()		spl6()
#define	splstatclock()		spl6()
#define	splvm()			splimp()
#define	splhigh()		spl7()

/* watch out for side effects */
#define	splx(s)		((s) & PSL_IPL ? _spl((s)) : spl0())

/*
 * Simulated software interrupt register.
 */
extern volatile u_int8_t ssir;

#define	SIR_NET		0x01
#define	SIR_CLOCK	0x02

#define	siron(mask)	\
	__asm __volatile ( "orb %0,_ssir" : : "i" ((mask)))
#define	siroff(mask)	\
	__asm __volatile ( "andb %0,_ssir" : : "ir" (~(mask)));

#define	setsoftnet()	siron(SIR_NET)
#define	setsoftclock()	siron(SIR_CLOCK)

/* locore.s */
int	spl0(void);

/* intr.c */
void	intr_init(void);
void	intr_establish(struct isr *, const char *);
void	intr_disestablish(struct isr *);
void	intr_dispatch(int);
void	intr_printlevels(void);
#endif /* _KERNEL */

#endif /* _HP300_INTR_H_ */
