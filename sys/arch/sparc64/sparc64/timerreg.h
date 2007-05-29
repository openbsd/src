/*	$OpenBSD: timerreg.h,v 1.5 2007/05/29 09:54:23 sobrado Exp $	*/
/*	$NetBSD: timerreg.h,v 1.3 1999/06/05 05:10:01 mrg Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)timerreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * These timers work in a rather peculiar fashion.  Most clock counters
 * run to 0 (as, e.g., on the VAX, where the ICR counts up to 0 from a
 * large unsigned number).  On the Sun-4c, it counts up to a limit.  But
 * for some reason, when it reaches the limit, it resets to 1, not 0.
 * Thus, if the limit is set to 4, the counter counts like this:
 *
 *	1, 2, 3, 1, 2, 3, ...
 *
 * and if we want to divide by N we must set the limit register to N+1.
 *
 * Sun-4u counters/timer are similar but:
 *
 *	- the registers have been shuffled around once again.  We need
 *	  to use offsets from the 3 addresses the ROM provides us.
 *	- The counters are 29 bits wide with 1us accuracy.
 *	- You can make them do funky things with the limit register
 *	- They have standard 64-bit SBus control registers.
 *
 * There is a problem on the Ultra5 and Ultra10.  As the PCI controller
 * doesn't include the timer, there are no `counter-timer' nodes here
 * and so we must use %tick.
 */
#ifndef _LOCORE
struct timer_4u {
	volatile int64_t t_count;		/* counter reg */
	volatile int64_t t_limit;		/* limit reg */

#define TMR_LIM_IEN		0x80000000	/* interrupt enable bit */
#define TMR_LIM_RELOAD		0x40000000	/* reload counter to 0 */
#define TMR_LIM_PERIODIC	0x20000000	/* reset at limit */
#define TMR_LIM_MASK		0x1fffffff
};

struct timerreg_4u {
	struct timer_4u		*t_timer;	/* There are two of them. */
	volatile int64_t	*t_clrintr;	/* There are two of these. */
	volatile int64_t	*t_mapintr;	/* Same here. */
};

#endif /* _LOCORE */

/* Compute a limit that causes the timer to fire every n microseconds. */
#define	tmr_ustolim(n)	(((n) - 1) & TMR_LIM_MASK)
