/*	$OpenBSD: timerreg.h,v 1.4 2003/06/02 23:27:55 millert Exp $	*/
/*	$NetBSD: timerreg.h,v 1.6 1996/10/28 00:20:32 abrown Exp $ */

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
 * Sun-4c counter/timer registers.  The timers are implemented within
 * the cache chip (!).  The counter and limit fields below could be
 * defined as:
 *
 *	struct {
 *		u_int	t_limit:1,	// limit reached
 *			t_usec:21,	// counter value in microseconds
 *			t_mbz:10;	// always zero
 *	};
 *
 * but this is more trouble than it is worth.
 *
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
 * Sun-4m counters/timer registers are similar, with these exceptions:
 *
 *	- the limit and counter registers have changed positions..
 *	- both limit and counter registers are 22 bits wide, but
 *	  they count in 500ns increments (bit 9 being the least
 *	  significant bit).
 *
 */
#ifndef _LOCORE
struct timer_4 {
	volatile int	t_counter;		/* counter reg */
	volatile int	t_limit;		/* limit reg */
};

struct timerreg_4 {
	struct	timer_4 t_c10;		/* counter that interrupts at ipl 10 */
	struct	timer_4 t_c14;		/* counter that interrupts at ipl 14 */
};

struct timer_4m {		/* counter that interrupts at ipl 10 */
	volatile int	t_limit;		/* limit register */
	volatile int	t_counter;		/* counter register */
	volatile int	t_limit_nr;		/* limit reg, non-resetting */
	volatile int	t_reserved;
	volatile int	t_cfg;			/* a configuration register */
/*
 * Note: The SparcClassic manual only defines this one bit
 * I suspect there are more in multi-processor machines.
 */
#define TMR_CFG_USER	1
};

struct counter_4m {		/* counter that interrupts at ipl 14 */
	volatile int	t_limit;		/* limit register */
	volatile int	t_counter;		/* counter register */
	volatile int	t_limit_nr;		/* limit reg, non-resetting */
	volatile int	t_ss;			/* Start/Stop register */
#define TMR_USER_RUN	1
};
#endif /* _LOCORE */

#define	TMR_LIMIT	0x80000000	/* counter reached its limit */
#define	TMR_SHIFT	10		/* shift to obtain microseconds */
#define	TMR_MASK	0x1fffff	/* 21 bits */

/* 
 * Compute a limit that causes the timer to fire every n microseconds.
 * The Sun4c requires that the timer register be initialized for n+1
 * microseconds, while the Sun4m requires it be initialized for n. Thus
 * the two versions of this function.
 *
 * Note that the manual for the chipset used in the Sun4m suggests that
 * the timer be set at n+0.5 microseconds; in practice, this produces
 * a 50 ppm clock skew, which means that the 0.5 should not be there... 
 */
#define	tmr_ustolim(n)	(((n) + 1) << TMR_SHIFT)

/*efine	TMR_SHIFT4M	9		-* shift to obtain microseconds */
/*efine tmr_ustolim4m(n)	(((2*(n)) + 1) << TMR_SHIFT4M)*/
#define tmr_ustolim4m(n)	((n) << TMR_SHIFT)
