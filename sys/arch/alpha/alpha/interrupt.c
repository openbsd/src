/* $OpenBSD: interrupt.c,v 1.23 2007/05/29 18:10:41 miod Exp $ */
/* $NetBSD: interrupt.c,v 1.46 2000/06/03 20:47:36 thorpej Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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
/*
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center.
 * Redistribute and modify at will, leaving only this additional copyright
 * notice.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/sched.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/evcount.h>

#include <uvm/uvm_extern.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/rpb.h>
#include <machine/frame.h>
#include <machine/cpuconf.h>

#if defined(MULTIPROCESSOR)
#include <sys/device.h>
#endif

#include <net/netisr.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include "ppp.h"
#include "bridge.h"

#include "apecs.h"
#include "cia.h"
#include "lca.h"
#include "tcasic.h"

static u_int schedclk2;

extern struct evcount clk_count;

struct scbvec scb_iovectab[SCB_VECTOIDX(SCB_SIZE - SCB_IOVECBASE)];

void netintr(void);

void	scb_stray(void *, u_long);

void
scb_init(void)
{
	u_long i;

	for (i = 0; i < SCB_NIOVECS; i++) {
		scb_iovectab[i].scb_func = scb_stray;
		scb_iovectab[i].scb_arg = NULL;
	}
}

void
scb_stray(void *arg, u_long vec)
{

	printf("WARNING: stray interrupt, vector 0x%lx\n", vec);
}

void
scb_set(u_long vec, void (*func)(void *, u_long), void *arg)
{
	u_long idx;
	int s;

	s = splhigh();

	if (vec < SCB_IOVECBASE || vec >= SCB_SIZE ||
	    (vec & (SCB_VECSIZE - 1)) != 0)
		panic("scb_set: bad vector 0x%lx", vec);

	idx = SCB_VECTOIDX(vec - SCB_IOVECBASE);

	if (scb_iovectab[idx].scb_func != scb_stray)
		panic("scb_set: vector 0x%lx already occupied", vec);

	scb_iovectab[idx].scb_func = func;
	scb_iovectab[idx].scb_arg = arg;

	splx(s);
}

u_long
scb_alloc(void (*func)(void *, u_long), void *arg)
{
	u_long vec, idx;
	int s;

	s = splhigh();

	/*
	 * Allocate "downwards", to avoid bumping into
	 * interrupts which are likely to be at the lower
	 * vector numbers.
	 */
	for (vec = SCB_SIZE - SCB_VECSIZE;
	     vec >= SCB_IOVECBASE; vec -= SCB_VECSIZE) {
		idx = SCB_VECTOIDX(vec - SCB_IOVECBASE);
		if (scb_iovectab[idx].scb_func == scb_stray) {
			scb_iovectab[idx].scb_func = func;
			scb_iovectab[idx].scb_arg = arg;
			splx(s);
			return (vec);
		}
	}

	splx(s);

	return (SCB_ALLOC_FAILED);
}

void
scb_free(u_long vec)
{
	u_long idx;
	int s;

	s = splhigh();

	if (vec < SCB_IOVECBASE || vec >= SCB_SIZE ||
	    (vec & (SCB_VECSIZE - 1)) != 0)
		panic("scb_free: bad vector 0x%lx", vec);

	idx = SCB_VECTOIDX(vec - SCB_IOVECBASE); 

	if (scb_iovectab[idx].scb_func == scb_stray)
		panic("scb_free: vector 0x%lx is empty", vec);

	scb_iovectab[idx].scb_func = scb_stray;
	scb_iovectab[idx].scb_arg = (void *) vec;

	splx(s);
}

void
interrupt(unsigned long a0, unsigned long a1, unsigned long a2,
    struct trapframe *framep)
{
	struct proc *p;
	struct cpu_info *ci = curcpu();
	extern int schedhz;

	switch (a0) {
	case ALPHA_INTR_XPROC:	/* interprocessor interrupt */
#if defined(MULTIPROCESSOR)
	    {
		u_long pending_ipis, bit;

#if 0
		printf("CPU %lu got IPI\n", cpu_id);
#endif

#ifdef DIAGNOSTIC
		if (ci->ci_dev == NULL) {
			/* XXX panic? */
			printf("WARNING: no device for ID %lu\n", ci->ci_cpuid);
			return;
		}
#endif

		pending_ipis = atomic_loadlatch_ulong(&ci->ci_ipis, 0);
		for (bit = 0; bit < ALPHA_NIPIS; bit++)
			if (pending_ipis & (1UL << bit))
				(*ipifuncs[bit])();

		/*
		 * Handle inter-console messages if we're the primary
		 * CPU.
		 */
		if (ci->ci_cpuid == hwrpb->rpb_primary_cpu_id &&
		    hwrpb->rpb_txrdy != 0)
			cpu_iccb_receive();
	    }
#else
		printf("WARNING: received interprocessor interrupt!\n");
#endif /* MULTIPROCESSOR */
		break;
		
	case ALPHA_INTR_CLOCK:	/* clock interrupt */
#if defined(MULTIPROCESSOR)
		/* XXX XXX XXX */
		if (CPU_IS_PRIMARY(ci) == 0)
			return;
#endif
		uvmexp.intrs++;
		clk_count.ec_count++;
		if (platform.clockintr) {
			/*
			 * Call hardclock().  This will also call
			 * statclock(). On the primary CPU, it
			 * will also deal with time-of-day stuff.
			 */
			(*platform.clockintr)((struct clockframe *)framep);

			/*
			 * If it's time to call the scheduler clock,
			 * do so.
			 */
			if ((++schedclk2 & 0x3f) == 0 &&
			    (p = ci->ci_curproc) != NULL && schedhz != 0)
				schedclock(p);
		}
		break;

	case ALPHA_INTR_ERROR:	/* Machine Check or Correctable Error */
		a0 = alpha_pal_rdmces();
		if (platform.mcheck_handler)
			(*platform.mcheck_handler)(a0, framep, a1, a2);
		else
			machine_check(a0, framep, a1, a2);
		break;

	case ALPHA_INTR_DEVICE:	/* I/O device interrupt */
	    {
		struct scbvec *scb;

		KDASSERT(a1 >= SCB_IOVECBASE && a1 < SCB_SIZE);

#if defined(MULTIPROCESSOR)
		/* XXX XXX XXX */
		if (CPU_IS_PRIMARY(ci) == 0)
			return;
#endif
		uvmexp.intrs++;

		scb = &scb_iovectab[SCB_VECTOIDX(a1 - SCB_IOVECBASE)];
		(*scb->scb_func)(scb->scb_arg, a1);
		break;
	    }

	case ALPHA_INTR_PERF:	/* performance counter interrupt */
		printf("WARNING: received performance counter interrupt!\n");
		break;

	case ALPHA_INTR_PASSIVE:
#if 0
		printf("WARNING: received passive release interrupt vec "
		    "0x%lx\n", a1);
#endif
		break;

	default:
		printf("unexpected interrupt: type 0x%lx vec 0x%lx "
		    "a2 0x%lx"
#if defined(MULTIPROCESSOR)
		    " cpu %lu"
#endif
		    "\n", a0, a1, a2
#if defined(MULTIPROCESSOR)
		    , ci->ci_cpuid
#endif
		    );
		panic("interrupt");
		/* NOTREACHED */
	}
}

void
machine_check(unsigned long mces, struct trapframe *framep,
    unsigned long vector, unsigned long param)
{
	const char *type;
	struct mchkinfo *mcp;

	mcp = &curcpu()->ci_mcinfo;
	/* Make sure it's an error we know about. */
	if ((mces & (ALPHA_MCES_MIP|ALPHA_MCES_SCE|ALPHA_MCES_PCE)) == 0) {
		type = "fatal machine check or error (unknown type)";
		goto fatal;
	}

	/* Machine checks. */
	if (mces & ALPHA_MCES_MIP) {
		/* If we weren't expecting it, then we punt. */
		if (!mcp->mc_expected) {
			type = "unexpected machine check";
			goto fatal;
		}
		mcp->mc_expected = 0;
		mcp->mc_received = 1;
	}

	/* System correctable errors. */
	if (mces & ALPHA_MCES_SCE)
		printf("Warning: received system correctable error.\n");

	/* Processor correctable errors. */
	if (mces & ALPHA_MCES_PCE)
		printf("Warning: received processor correctable error.\n"); 

	/* Clear pending machine checks and correctable errors */
	alpha_pal_wrmces(mces);
	return;

fatal:
	/* Clear pending machine checks and correctable errors */
	alpha_pal_wrmces(mces);

	printf("\n");
	printf("%s:\n", type);
	printf("\n");
	printf("    mces    = 0x%lx\n", mces);
	printf("    vector  = 0x%lx\n", vector);
	printf("    param   = 0x%lx\n", param);
	printf("    pc      = 0x%lx\n", framep->tf_regs[FRAME_PC]);
	printf("    ra      = 0x%lx\n", framep->tf_regs[FRAME_RA]);
	printf("    curproc = %p\n", curproc);
	if (curproc != NULL)
		printf("        pid = %d, comm = %s\n", curproc->p_pid,
		    curproc->p_comm);
	printf("\n");
	panic("machine check");
}

#if NAPECS > 0 || NCIA > 0 || NLCA > 0 || NTCASIC > 0

int
badaddr(void *addr, size_t size)
{
	return(badaddr_read(addr, size, NULL));
}

int
badaddr_read(void *addr, size_t size, void *rptr)
{
	struct mchkinfo *mcp = &curcpu()->ci_mcinfo;
	long rcpt;
	int rv;

	/* Get rid of any stale machine checks that have been waiting.  */
	alpha_pal_draina();

	/* Tell the trap code to expect a machine check. */
	mcp->mc_received = 0;
	mcp->mc_expected = 1;

	/* Read from the test address, and make sure the read happens. */
	alpha_mb();
	switch (size) {
	case sizeof (u_int8_t):
		rcpt = *(volatile u_int8_t *)addr;
		break;

	case sizeof (u_int16_t):
		rcpt = *(volatile u_int16_t *)addr;
		break;

	case sizeof (u_int32_t):
		rcpt = *(volatile u_int32_t *)addr;
		break;

	case sizeof (u_int64_t):
		rcpt = *(volatile u_int64_t *)addr;
		break;

	default:
		panic("badaddr: invalid size (%ld)", size);
	}
	alpha_mb();
	alpha_mb();	/* MAGIC ON SOME SYSTEMS */

	/* Make sure we took the machine check, if we caused one. */
	alpha_pal_draina();

	/* disallow further machine checks */
	mcp->mc_expected = 0;

	rv = mcp->mc_received;
	mcp->mc_received = 0;

	/*
	 * And copy back read results (if no fault occurred).
	 */
	if (rptr && rv == 0) {
		switch (size) {
		case sizeof (u_int8_t):
			*(volatile u_int8_t *)rptr = rcpt;
			break;

		case sizeof (u_int16_t):
			*(volatile u_int16_t *)rptr = rcpt;
			break;

		case sizeof (u_int32_t):
			*(volatile u_int32_t *)rptr = rcpt;
			break;

		case sizeof (u_int64_t):
			*(volatile u_int64_t *)rptr = rcpt;
			break;
		}
	}
	/* Return non-zero (i.e. true) if it's a bad address. */
	return (rv);
}

#endif	/* NAPECS > 0 || NCIA > 0 || NLCA > 0 || NTCASIC > 0 */

int netisr;

void
netintr()
{
	int n;

	while ((n = netisr) != 0) {
		atomic_clearbits_int(&netisr, n);

#define	DONETISR(bit, fn)						\
		do {							\
			if (n & (1 << (bit)))				\
				fn();					\
		} while (0)

#include <net/netisr_dispatch.h>

#undef DONETISR
	}
}

struct alpha_soft_intr alpha_soft_intrs[SI_NSOFT];

/* XXX For legacy software interrupts. */
struct alpha_soft_intrhand *softnet_intrhand, *softclock_intrhand;

/*
 * softintr_init:
 *
 *	Initialize the software interrupt system.
 */
void
softintr_init()
{
	struct alpha_soft_intr *asi;
	int i;

	for (i = 0; i < SI_NSOFT; i++) {
		asi = &alpha_soft_intrs[i];
		TAILQ_INIT(&asi->softintr_q);
		simple_lock_init(&asi->softintr_slock);
		asi->softintr_siq = i;
	}

	/* XXX Establish legacy software interrupt handlers. */
	softnet_intrhand = softintr_establish(IPL_SOFTNET,
	    (void (*)(void *))netintr, NULL);
	softclock_intrhand = softintr_establish(IPL_SOFTCLOCK,
	    (void (*)(void *))softclock, NULL);
}

/*
 * softintr_dispatch:
 *
 *	Process pending software interrupts.
 */
void
softintr_dispatch()
{
	struct alpha_soft_intr *asi;
	struct alpha_soft_intrhand *sih;
	u_int64_t n, i;

	while ((n = atomic_loadlatch_ulong(&ssir, 0)) != 0) {
		for (i = 0; i < SI_NSOFT; i++) {
			if ((n & (1 << i)) == 0)
				continue;
	
			asi = &alpha_soft_intrs[i];

			for (;;) {
				(void) alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
				simple_lock(&asi->softintr_slock);

				sih = TAILQ_FIRST(&asi->softintr_q);
				if (sih != NULL) {
					TAILQ_REMOVE(&asi->softintr_q, sih,
					    sih_q);
					sih->sih_pending = 0;
				}

				simple_unlock(&asi->softintr_slock);
				(void) alpha_pal_swpipl(ALPHA_PSL_IPL_SOFT);

				if (sih == NULL)
					break;

				uvmexp.softs++;
				(*sih->sih_fn)(sih->sih_arg);
			}
		}
	}
}

static int
ipl2si(int ipl)
{
	int si;

	switch (ipl) {
	case IPL_TTY:			/* XXX */
	case IPL_SOFTSERIAL:
		si = SI_SOFTSERIAL;
		break;
	case IPL_SOFTNET:
		si = SI_SOFTNET;
		break;
	case IPL_SOFTCLOCK:
		si = SI_SOFTCLOCK;
		break;
	case IPL_SOFT:
		si = SI_SOFT;
		break;
	default:
		panic("ipl2si: %d", ipl);
	}
	return si;
}

/*
 * softintr_establish:		[interface]
 *
 *	Register a software interrupt handler.
 */
void *
softintr_establish(int ipl, void (*func)(void *), void *arg)
{
	struct alpha_soft_intr *asi;
	struct alpha_soft_intrhand *sih;
	int si;

	si = ipl2si(ipl);
	asi = &alpha_soft_intrs[si];

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT);
	if (__predict_true(sih != NULL)) {
		sih->sih_intrhead = asi;
		sih->sih_fn = func;
		sih->sih_arg = arg;
		sih->sih_pending = 0;
	}
	return (sih);
}

/*
 * softintr_disestablish:	[interface]
 *
 *	Unregister a software interrupt handler.
 */
void
softintr_disestablish(void *arg)
{
	struct alpha_soft_intrhand *sih = arg;
	struct alpha_soft_intr *asi = sih->sih_intrhead;
	int s;

	s = splhigh();
	simple_lock(&asi->softintr_slock);
	if (sih->sih_pending) {
		TAILQ_REMOVE(&asi->softintr_q, sih, sih_q);
		sih->sih_pending = 0;
	}
	simple_unlock(&asi->softintr_slock);
	splx(s);

	free(sih, M_DEVBUF);
}

int
_splraise(int s)
{
	int cur = alpha_pal_rdps() & ALPHA_PSL_IPL_MASK;
	return (s > cur ? alpha_pal_swpipl(s) : cur);
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int curipl = alpha_pal_rdps() & ALPHA_PSL_IPL_MASK;

	/*
	 * Tell soft interrupts apart from regular levels.
	 */
	if (wantipl < 0)
		wantipl = IPL_SOFTINT;

	if (curipl < wantipl) {
		splassert_fail(wantipl, curipl, func);
		/*
		 * If splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		alpha_pal_swpipl(wantipl);
	}
}
#endif
