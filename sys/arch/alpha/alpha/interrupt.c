/* $OpenBSD: interrupt.c,v 1.11 2001/01/20 20:29:53 art Exp $ */
/* $NetBSD: interrupt.c,v 1.44 2000/05/23 05:12:53 thorpej Exp $ */

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

#include <vm/vm.h>

#include <uvm/uvm_extern.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/rpb.h>
#include <machine/frame.h>
#include <machine/cpuconf.h>
#include <machine/intrcnt.h>

#if defined(MULTIPROCESSOR)
#include <sys/device.h>
#endif

static u_int schedclk2;

void
interrupt(a0, a1, a2, framep)
	unsigned long a0, a1, a2;
	struct trapframe *framep;
{
	struct proc *p;
#if defined(MULTIPROCESSOR)
	u_long cpu_id = alpha_pal_whami();
#endif
	extern int schedhz;

	switch (a0) {
	case ALPHA_INTR_XPROC:	/* interprocessor interrupt */
#if defined(MULTIPROCESSOR)
	    {
		struct cpu_info *ci = &cpu_info[cpu_id];
		u_long pending_ipis, bit;

#if 0
		printf("CPU %lu got IPI\n", cpu_id);
#endif

#ifdef DIAGNOSTIC
		if (ci->ci_dev == NULL) {
			/* XXX panic? */
			printf("WARNING: no device for ID %lu\n", cpu_id);
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
		if (cpu_id == hwrpb->rpb_primary_cpu_id &&
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
		if (cpu_id != hwrpb->rpb_primary_cpu_id)
			return;
#endif
		uvmexp.intrs++;
		intrcnt[INTRCNT_CLOCK]++;
		if (platform.clockintr) {
			(*platform.clockintr)((struct clockframe *)framep);
			if((++schedclk2 & 0x3f) == 0
			&& (p = curproc) != NULL
			&& schedhz)
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
#if defined(MULTIPROCESSOR)
		/* XXX XXX XXX */
		if (cpu_id != hwrpb->rpb_primary_cpu_id)
			return;
#endif
		uvmexp.intrs++;
		if (platform.iointr)
			(*platform.iointr)(framep, a1);
		break;

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
		    , cpu_id
#endif
		    );
		panic("interrupt");
		/* NOTREACHED */
	}
}

void
set_iointr(niointr)
	void (*niointr) __P((void *, unsigned long));
{
	if (platform.iointr)
		panic("set iointr twice");
	platform.iointr = niointr;
}


void
machine_check(mces, framep, vector, param)
	unsigned long mces;
	struct trapframe *framep;
	unsigned long vector, param;
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

int
badaddr(addr, size)
	void *addr;
	size_t size;
{
	return(badaddr_read(addr, size, NULL));
}

int
badaddr_read(addr, size, rptr)
	void *addr;
	size_t size;
	void *rptr;
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
		panic("badaddr: invalid size (%ld)\n", size);
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
