/*	$NetBSD: interrupt.c,v 1.4 1995/11/23 02:34:08 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Keith Bostic, Chris G. Demetriou
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>

#include <machine/autoconf.h>
#include <machine/reg.h>

#ifdef EVCNT_COUNTERS
#include <sys/device.h>
#else
#include <machine/intrcnt.h>
#endif

struct logout {
#define	LOGOUT_RETRY	0x1000000000000000	/* Retry bit. */
#define	LOGOUT_LENGTH	0xffff			/* Length mask. */
	u_int64_t q1;				/* Retry and length */
	/* Unspecified. */
};

void		machine_check __P((struct trapframe *, struct logout *,
		    u_int64_t));
static void	nullintr __P((void *, int));

static void	(*iointr) __P((void *, int)) = nullintr;
static void	(*clockintr) __P((void *, int)) = nullintr;
static int	mc_expected, mc_received;

#ifdef EVCNT_COUNTERS
struct evcnt	clock_intr_evcnt;	/* event counter for clock intrs. */
#endif

void
interrupt(framep, type, vec, logoutp)
	struct trapframe *framep;
	u_int64_t type, vec;
	struct logout *logoutp;
{

	if (type == 1)			/* clock interrupt */
		(*clockintr)(framep, vec);
	else if (type == 3)		/* I/O device interrupt */
		(*iointr)(framep, vec);
	else if (type == 2)
		machine_check(framep, logoutp, vec);
	else
		panic("unexpected interrupt: type %ld, vec %ld\n",
		    (long)type, (long)vec);
}

static void
nullintr(framep, vec)
	void *framep;
	int vec;
{
}

static void
real_clockintr(framep, vec)
	void *framep;
	int vec;
{

#ifdef EVCNT_COUNTERS
	clock_intr_evcnt.ev_count++;
#else
	intrcnt[INTRCNT_CLOCK]++;
#endif
	hardclock(framep);
}

void
set_clockintr()
{

	if (clockintr != nullintr)
		panic("set clockintr twice");

	clockintr = real_clockintr;
}

void
set_iointr(niointr)
	void (*niointr) __P((void *, int));
{

	if (iointr != nullintr)
		panic("set iointr twice");

	iointr = niointr;
}

void
machine_check(framep, logoutp, vec)
	struct trapframe *framep;
	struct logout *logoutp;
	u_int64_t vec;
{

	if (!mc_expected)
		panic("machine check: vec %lx, pc = 0x%lx, ra = 0x%lx",
		    vec, framep->tf_pc, framep->tf_regs[FRAME_RA]);

	mc_expected = 0;
	mc_received = 1;

	logoutp->q1 &= ~LOGOUT_RETRY;		/* XXX: Necessary? */
	pal_mtpr_mces(0x19);			/* XXX: VMS PAL! */
}

int
badaddr(addr, size)
	void *addr;
	u_int64_t size;
{
	int rv;
	volatile long rcpt;

	/* Tell the trap code to expect a machine check. */
	mc_received = 0;
	mc_expected = 1;

	/* Read from the test address, and make sure the read happens. */
	wbflush();
	switch (size) {
	case sizeof (u_int8_t):
		rcpt = *(u_int8_t *)addr;
		break;

	case sizeof (u_int16_t):
		rcpt = *(u_int16_t *)addr;
		break;

	case sizeof (u_int32_t):
		rcpt = *(u_int32_t *)addr;
		break;

	case sizeof (u_int64_t):
		rcpt = *(u_int64_t *)addr;
		break;

	default:
		panic("badaddr: invalid size (%ld)\n", size);
	}
	wbflush();
	pal_draina();

	/* disallow further machine checks */
	mc_expected = 0;

	/* Return non-zero (i.e. true) if it's a bad address. */
	return (mc_received);
}
