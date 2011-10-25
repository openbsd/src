/*	$OpenBSD: m197_machdep.c,v 1.46 2011/10/25 18:38:06 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 1998, 1999, 2000, 2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/bugio.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/m88410.h>
#include <machine/mvme197.h>

#include <mvme88k/dev/busswreg.h>
#include <mvme88k/mvme88k/clockvar.h>

#ifdef MULTIPROCESSOR
#include <machine/db_machdep.h>
#endif

void	m197_bootstrap(void);
void	m197_delay(int);
void	m197_ext_int(struct trapframe *);
u_int	m197_getipl(void);
int	m197_ipi_handler(struct trapframe *);
vaddr_t	m197_memsize(void);
uint32_t m197_mp_atomic_begin(__cpu_simple_lock_t *, uint *);
void	m197_mp_atomic_end(uint32_t, __cpu_simple_lock_t *, uint);
int	m197_nmi(struct trapframe *);
void	m197_nmi_wrapup(struct trapframe *);
u_int	m197_raiseipl(u_int);
u_int	m197_setipl(u_int);
void	m197_smp_setup(struct cpu_info *);
void	m197_soft_ipi(void);

/*
 * Figure out how much real memory is available.
 *
 * This relies on the fact that the BUG will configure the BusSwitch
 * system translation decoders to allow access to the whole memory
 * from address zero.
 *
 * If the BUG is not configured correctly wrt to the real amount of
 * memory in the system, this will return incorrect values, but we do
 * not care if you can't configure your system correctly.
 */
vaddr_t
m197_memsize()
{
	int i;
	u_int8_t sar;
	u_int16_t ssar, sear;
	struct mvmeprom_brdid brdid;

	/*
	 * MVME197LE 01-W3869B0[12][EF] boards shipped with a broken DCAM2
	 * chip, which can only address 32MB of memory. Unfortunately, 02[EF]
	 * were fitted with 64MB...
	 * Note that we can't decide on letter < F since this would match
	 * post-Z boards (AA, AB, etc).
	 *
	 * If the CNFG memory has been lost, you're on your own...
	 */
	bzero(&brdid, sizeof(brdid));
	bugbrdid(&brdid);
	if (bcmp(brdid.pwa, "01-W3869B02", 11) == 0) {
		if (brdid.pwa[11] == 'E' || brdid.pwa[11] == 'F')
			return (32 * 1024 * 1024);
	}

	for (i = 0; i < 4; i++) {
		sar = *(u_int8_t *)(BS_BASE + BS_SAR + i);
		if (!ISSET(sar, BS_SAR_DEN))
			continue;

		ssar = *(u_int16_t *)(BS_BASE + BS_SSAR1 + i * 4);
		sear = *(u_int16_t *)(BS_BASE + BS_SEAR1 + i * 4);

		if (ssar != 0)
			continue;

		return ((sear + 1) << 16);
	}

	/*
	 * If no decoder was enabled, how could we run so far?
	 * Return a ``safe'' 32MB.
	 */
	return (32 * 1024 * 1024);
}

/*
 * Device interrupt handler for MVME197
 */

void
m197_ext_int(struct trapframe *eframe)
{
	u_int32_t psr;
	int level;
	struct intrhand *intr;
	intrhand_t *list;
	int ret;
	vaddr_t ivec;
	u_int8_t vec;

#ifdef MULTIPROCESSOR
	if (eframe->tf_mask < IPL_SCHED)
		__mp_lock(&kernel_lock);
#endif

	uvmexp.intrs++;

	level = *(u_int8_t *)M197_ILEVEL & 0x07;
	/* generate IACK and get the vector */
	ivec = M197_IACK + (level << 2) + 0x03;
	vec = *(volatile u_int8_t *)ivec;

	/* block interrupts at level or lower */
	m197_setipl(level);
	psr = get_psr();
	set_psr(psr & ~PSR_IND);

	list = &intr_handlers[vec];
	if (SLIST_EMPTY(list))
		printf("Spurious interrupt (level %x and vec %x)\n",
		    level, vec);

	/*
	 * Walk through all interrupt handlers in the chain for the
	 * given vector, calling each handler in turn, till some handler
	 * returns a value != 0.
	 */

	ret = 0;
	SLIST_FOREACH(intr, list, ih_link) {
		if (intr->ih_wantframe != 0)
			ret = (*intr->ih_fn)((void *)eframe);
		else
			ret = (*intr->ih_fn)(intr->ih_arg);
		if (ret != 0) {
			intr->ih_count.ec_count++;
			break;
		}
	}

	if (ret == 0) {
		printf("Unclaimed interrupt (level %x and vec %x)\n",
		    level, vec);
	}

#if 0
	/*
	 * Disable interrupts before returning to assembler,
	 * the spl will be restored later.
	 */
	set_psr(psr | PSR_IND);
#endif

#ifdef MULTIPROCESSOR
	if (eframe->tf_mask < IPL_SCHED)
		__mp_unlock(&kernel_lock);
#endif
}

/*
 * NMI handler. Invoked with interrupts disabled.
 * Returns nonzero if NMI have been reenabled, and the exception handler
 * is allowed to run soft interrupts and AST; nonzero otherwise.
 */

int
m197_nmi(struct trapframe *eframe)
{
	u_int8_t abort;
	int rc;

	/*
	 * Non-maskable interrupts are either the abort switch (on
	 * cpu0 only) or IPIs (on any cpu). We check for IPI first.
	 */
#ifdef MULTIPROCESSOR
	if ((*(volatile u_int8_t *)(BS_BASE + BS_CPINT)) & BS_CPI_INT) {
		/* disable further NMI for now */
		*(volatile u_int8_t *)(BS_BASE + BS_CPINT) = 0;

		rc = m197_ipi_handler(eframe);

		/* acknowledge */
		*(volatile u_int8_t *)(BS_BASE + BS_CPINT) = BS_CPI_ICLR;

		if (rc != 0)
			m197_nmi_wrapup(eframe);
	} else
#endif
		rc = 1;

	if (CPU_IS_PRIMARY(curcpu())) {
		abort = *(u_int8_t *)(BS_BASE + BS_ABORT);
		if (abort & BS_ABORT_INT) {
			*(u_int8_t *)(BS_BASE + BS_ABORT) =
			    (abort & ~BS_ABORT_IEN) | BS_ABORT_ICLR;
			nmihand(eframe);
			*(u_int8_t *)(BS_BASE + BS_ABORT) |= BS_ABORT_IEN;
		}
	}

	return rc;
}

void
m197_nmi_wrapup(struct trapframe *eframe)
{
#ifdef MULTIPROCESSOR
	/* reenable IPIs */
	*(volatile u_int8_t *)(BS_BASE + BS_CPINT) = BS_CPI_IEN;
#endif
}

u_int
m197_getipl(void)
{
	return *(u_int8_t *)M197_IMASK & 0x07;
}

u_int
m197_setipl(u_int level)
{
	u_int curspl, psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = *(u_int8_t *)M197_IMASK & 0x07;
	*(u_int8_t *)M197_IMASK = level;
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);
	return curspl;
}

u_int
m197_raiseipl(u_int level)
{
	u_int curspl, psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = *(u_int8_t *)M197_IMASK & 0x07;
	if (curspl < level)
		*(u_int8_t *)M197_IMASK = level;
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);
	return curspl;
}

void
m197_bootstrap()
{
	extern struct cmmu_p cmmu88110;
	extern struct cmmu_p cmmu88410;
	extern int cpuspeed;
	u_int16_t cpu;
	u_int8_t version, btimer, pbt;

	if (mc88410_present()) {
		cmmu = &cmmu88410;	/* 197SP/197DP */

		/*
		 * Make sure all interrupts (levels 1 to 7) get routed
		 * to the boot cpu.
		 *
		 * We only need to write to one ISEL registers, this will
		 * set the correct value in the other one, since we set
		 * all the active bits.
		 */
		cpu = *(u_int16_t *)(BS_BASE + BS_GCSR) & BS_GCSR_CPUID;
		*(u_int8_t *)(BS_BASE + (cpu ? BS_ISEL1 : BS_ISEL0)) = 0xfe;
	} else
		cmmu = &cmmu88110;	/* 197LE */

	/*
	 * Find out the processor speed, from the BusSwitch prescaler
	 * adjust register.
	 */
	cpuspeed = 256 - *(volatile u_int8_t *)(BS_BASE + BS_PADJUST);

	/*
	 * Kernels running without snooping enabled (i.e. without
	 * CACHE_GLOBAL set in the apr in pmap.c) need increased processor
	 * bus timeout limits, or the instruction cache might not be able
	 * to fill or answer fast enough. It does not hurt to increase
	 * them unconditionnaly, though.
	 *
	 * Do this as soon as possible (i.e. now...), since this is
	 * especially critical on 40MHz boards, while some 50MHz boards can
	 * run without this timeout change... but better be safe than sorry.
	 *
	 * Boot blocks do this for us now, but again, better stay on the
	 * safe side. Be sure to update the boot blocks code if the logic
	 * below changes.
	 */
	version = *(volatile u_int8_t *)(BS_BASE + BS_CHIPREV);
	btimer = *(volatile u_int8_t *)(BS_BASE + BS_BTIMER);
	pbt = btimer & BS_BTIMER_PBT_MASK;
	btimer = (btimer & ~BS_BTIMER_PBT_MASK);
	
	/* XXX PBT256 might only be necessary for busswitch rev1? */
	if (cpuspeed < 50 || version <= 0x01) {
		if (pbt < BS_BTIMER_PBT256)
			pbt = BS_BTIMER_PBT256;
	} else {
		if (pbt < BS_BTIMER_PBT64)
			pbt = BS_BTIMER_PBT64;
	}

	*(volatile u_int8_t *)(BS_BASE + BS_BTIMER) = btimer | pbt;

	md_interrupt_func_ptr = m197_ext_int;
	md_nmi_func_ptr = m197_nmi;
	md_nmi_wrapup_func_ptr = m197_nmi_wrapup;
	md_getipl = m197_getipl;
	md_setipl = m197_setipl;
	md_raiseipl = m197_raiseipl;
	md_init_clocks = m1x7_init_clocks;
#ifdef MULTIPROCESSOR
	md_send_ipi = m197_send_ipi;
	md_delay = m197_delay;
	md_smp_setup = m197_smp_setup;
#else
	md_delay = m1x7_delay;
#endif
}

#ifdef MULTIPROCESSOR

/*
 * IPIs groups.
 *
 * There are three sorts of IPI on MVME197:
 *
 * - synchronous IPIs: TLB and cache operations
 *
 *	Those require immediate attention from the other processor, and the
 *	sender will wait for completion before resuming normal operations.
 *	This is done for so-called complex IPIs (those which take arguments),
 *	so that it isn't necessary to maintain a list of pending IPI work.
 *	However it is better to make tlb updates synchronous as well.
 *
 *	Handling of synchronous exceptions makes sure they can not be
 *	interrupted by another NMI; upon returning from the exception,
 *	the interrupted processor will not attempt to run soft interrupts
 *	and will not check for AST.
 *
 * - asynchronous fast IPIs: notify, ddb
 *
 *	Notify is just a trick to get the other processor to check for
 *	AST, it is processed almost immediately, but since it may cause
 *	preemption, the sender can not really wait for completion.
 *	As for DDB, waiting would interfere with ddb's logic.
 *
 * - asynchronous slow IPIs: clock
 *
 *	These may take a long time to execute. They cause the processor
 *	to self-inflict itself a soft interrupt, to make sure we won't
 *	run clock operations if it was running at splclock or higher when
 *	the IPI was received.
 */

#define	CI_IPI_CLOCK \
	(CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK)
#define	CI_IPI_SYNCHRONOUS \
	(CI_IPI_TLB_FLUSH_KERNEL | CI_IPI_TLB_FLUSH_USER | \
	 CI_IPI_CACHE_FLUSH | CI_IPI_ICACHE_FLUSH | CI_IPI_DMA_CACHECTL)

void
m197_send_ipi(int ipi, cpuid_t cpu)
{
	struct cpu_info *ci = &m88k_cpus[cpu];

	KASSERT((ipi & CI_IPI_SYNCHRONOUS) == 0);

	if ((ci->ci_flags & CIF_ALIVE) == 0)
		return;			/* XXX not ready yet */

	if (ci->ci_ddb_state == CI_DDB_PAUSE)
		return;			/* XXX skirting deadlock */

	atomic_setbits_int(&ci->ci_ipi, ipi);
	*(volatile u_int8_t *)(BS_BASE + BS_CPINT) |= BS_CPI_SCPI;
}

void
m197_send_complex_ipi(int ipi, cpuid_t cpu, u_int32_t arg1, u_int32_t arg2)
{
	struct cpu_info *ci = &m88k_cpus[cpu];
	uint32_t psr;
	int wait;

	if ((ci->ci_flags & CIF_ALIVE) == 0)
		return;				/* XXX not ready yet */

	if (ci->ci_ddb_state == CI_DDB_PAUSE)
		return;				/* XXX skirting deadlock */

	psr = get_psr();
	set_psr(psr | PSR_IND);

	/*
	 * Wait for the other processor to be ready to accept an IPI.
	 */
	for (wait = 1000000; wait != 0; wait--) {
		if (!ISSET(*(volatile u_int8_t *)(BS_BASE + BS_CPINT),
		    BS_CPI_STAT))
			break;
	}
	if (wait == 0)
		panic("couldn't send complex ipi %x to cpu %d: busy",
		    ipi, cpu);

#ifdef DEBUG
	if (ci->ci_ipi != 0)
		printf("%s: cpu %d ipi %x did not clear during wait\n",
		    ci->ci_cpuid, ci->ci_ipi);
#endif

	/*
	 * In addition to the ipi bit itself, we need to set up ipi arguments.
	 * Note that we do not need to protect against another processor
	 * trying to send another complex IPI, since we know there are only
	 * two processors on the board. This is also why we do not use atomic
	 * operations on ci_ipi there, since we know from the loop above that
	 * the other process is done doing any IPI work.
	 */
	ci->ci_ipi_arg1 = arg1;
	ci->ci_ipi_arg2 = arg2;
	ci->ci_ipi |= ipi;

	*(volatile u_int8_t *)(BS_BASE + BS_CPINT) |= BS_CPI_SCPI;

	/*
	 * Wait for the other processor to complete ipi processing.
	 */
	for (wait = 1000000; wait != 0; wait--) {
		if (!ISSET(*(volatile u_int8_t *)(BS_BASE + BS_CPINT),
		    BS_CPI_STAT))
			break;
	}
	if (wait == 0)
		panic("couldn't send complex ipi %x to cpu %d: no ack",
		    ipi, cpu);

#ifdef DEBUG
	/*
	 * If there are any simple IPIs pending, trigger them now.
	 * There really shouldn't any, since we have waited for all
	 * asynchronous ipi processing to complete before sending this
	 * one.
	 */
	if (ci->ci_ipi != 0) {
		printf("%s: cpu %d ipi %x did not clear after completion\n",
		    ci->ci_cpuid, ci->ci_ipi);
		*(volatile u_int8_t *)(BS_BASE + BS_CPINT) |= BS_CPI_SCPI;
	}
#endif

	set_psr(psr);
}
void
m197_broadcast_complex_ipi(int ipi, u_int32_t arg1, u_int32_t arg2)
{
	/*
	 * This relies upon the fact that we only have two processors,
	 * and their cpuid are 0 and 1.
	 */
	m197_send_complex_ipi(ipi, 1 - curcpu()->ci_cpuid, arg1, arg2);
}

int
m197_ipi_handler(struct trapframe *eframe)
{
	struct cpu_info *ci = curcpu();
	int ipi;
	u_int32_t arg1, arg2;

	if ((ipi = atomic_clear_int(&ci->ci_ipi)) == 0)
		return 1;

	/*
	 * Synchronous IPIs. There can only be one pending at the same time,
	 * sending processor will wait for us to have processed the current
	 * one before sending a new one.
	 * We process them ASAP, ignoring any other pending ipi - sender will
	 * take care of resending an ipi if necessary.
	 */
	if (ipi & CI_IPI_SYNCHRONOUS) {
		/* no need to use atomic ops, the other cpu waits */
		/* leave asynchronous ipi pending */
		ci->ci_ipi = ipi & ~CI_IPI_SYNCHRONOUS;

		arg1 = ci->ci_ipi_arg1;
		arg2 = ci->ci_ipi_arg2;

		if (ipi & CI_IPI_TLB_FLUSH_KERNEL) {
			cmmu_tlbis(ci->ci_cpuid, arg1, arg2);
		}
		else if (ipi & CI_IPI_TLB_FLUSH_USER) {
			cmmu_tlbiu(ci->ci_cpuid, arg1, arg2);
		}
		else if (ipi & CI_IPI_CACHE_FLUSH) {
			cmmu_cache_wbinv(ci->ci_cpuid, arg1, arg2);
		}
		else if (ipi & CI_IPI_ICACHE_FLUSH) {
			cmmu_icache_inv(ci->ci_cpuid, arg1, arg2);
		}
		else if (ipi & CI_IPI_DMA_CACHECTL) {
			dma_cachectl_local(arg1, arg2, DMA_CACHE_INV);
		}

		return 0;
	}

	/*
	 * Asynchronous IPIs. We can have as many bits set as possible.
	 */

	if (ipi & CI_IPI_CLOCK) {
		/*
		 * Even if the current spl level would allow it, we can
		 * not run the clock handlers from there because we would
		 * need to grab the kernel lock, which might already
		 * held by the other processor.
		 *
		 * Instead, schedule a soft interrupt. But remember the
		 * important fields from the exception frame first, so
		 * that a valid clockframe can be reconstructed from the
		 * soft interrupt handler (which can not get an exception
		 * frame).
		 */
		if (ipi & CI_IPI_HARDCLOCK) {
			ci->ci_h_sxip = eframe->tf_sxip;
			ci->ci_h_epsr = eframe->tf_epsr;
		}
		if (ipi & CI_IPI_STATCLOCK) {
			ci->ci_s_sxip = eframe->tf_sxip;
			ci->ci_s_epsr = eframe->tf_epsr;
		}

		/* inflict ourselves a soft ipi */
		ci->ci_softipi_cb = m197_soft_ipi;
	}

	if (ipi & CI_IPI_DDB) {
#ifdef DDB
		/*
		 * Another processor has entered DDB. Spin on the ddb lock
		 * until it is done.
		 */
		extern struct __mp_lock ddb_mp_lock;

		ci->ci_ddb_state = CI_DDB_PAUSE;

		__mp_lock(&ddb_mp_lock);
		__mp_unlock(&ddb_mp_lock);

		ci->ci_ddb_state = CI_DDB_RUNNING;

		/*
		 * If ddb is hoping to us, it's our turn to enter ddb now.
		 */
		if (ci->ci_cpuid == ddb_mp_nextcpu)
			Debugger();
#endif
	}
	if (ipi & CI_IPI_NOTIFY) {
		/* nothing to do! */
	}

	return 1;
}

/*
 * Maskable IPIs.
 *
 * These IPIs are received as non maskable, but are not processed in
 * the NMI handler; instead, they are processed from the soft interrupt
 * handler.
 *
 * XXX This is grossly suboptimal.
 */
void
m197_soft_ipi()
{
	struct cpu_info *ci = curcpu();
	struct trapframe faketf;
	int s;

	__mp_lock(&kernel_lock);
	s = splclock();

	if (ci->ci_h_sxip != 0) {
		faketf.tf_cpu = ci;
		faketf.tf_sxip = ci->ci_h_sxip;
		faketf.tf_epsr = ci->ci_h_epsr;
		ci->ci_h_sxip = 0;
		hardclock((struct clockframe *)&faketf);
	}

	if (ci->ci_s_sxip != 0) {
		faketf.tf_cpu = ci;
		faketf.tf_sxip = ci->ci_s_sxip;
		faketf.tf_epsr = ci->ci_s_epsr;
		ci->ci_s_sxip = 0;
		statclock((struct clockframe *)&faketf);
	}

	splx(s);
	__mp_unlock(&kernel_lock);
}

/*
 * Special version of delay() for MP kernels.
 * Processors need to use different timers, so we'll use the two
 * BusSwitch timers for this purpose.
 */
void
m197_delay(int us)
{
	if (CPU_IS_PRIMARY(curcpu())) {
		*(volatile u_int32_t *)(BS_BASE + BS_TCOMP1) = 0xffffffff;
		*(volatile u_int32_t *)(BS_BASE + BS_TCOUNT1) = 0;
		*(volatile u_int8_t *)(BS_BASE + BS_TCTRL1) |= BS_TCTRL_CEN;

		while ((*(volatile u_int32_t *)(BS_BASE + BS_TCOUNT1)) <
		    (u_int32_t)us)
			;
		*(volatile u_int8_t *)(BS_BASE + BS_TCTRL1) &= ~BS_TCTRL_CEN;
	} else {
		*(volatile u_int32_t *)(BS_BASE + BS_TCOMP2) = 0xffffffff;
		*(volatile u_int32_t *)(BS_BASE + BS_TCOUNT2) = 0;
		*(volatile u_int8_t *)(BS_BASE + BS_TCTRL2) |= BS_TCTRL_CEN;

		while ((*(volatile u_int32_t *)(BS_BASE + BS_TCOUNT2)) <
		    (u_int32_t)us)
			;
		*(volatile u_int8_t *)(BS_BASE + BS_TCTRL2) &= ~BS_TCTRL_CEN;
	}
}

void
m197_smp_setup(struct cpu_info *ci)
{
	/*
	 * Setup function pointers for mplock operation.
	 */

	ci->ci_mp_atomic_begin = m197_mp_atomic_begin;
	ci->ci_mp_atomic_end = m197_mp_atomic_end;
}

uint32_t
m197_mp_atomic_begin(__cpu_simple_lock_t *lock, uint *csr)
{
	uint32_t psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	*csr = *(volatile uint8_t *)(BS_BASE + BS_CPINT);
	*(volatile uint8_t *)(BS_BASE + BS_CPINT) = 0;

	__cpu_simple_lock(lock);

	return psr;
}

void
m197_mp_atomic_end(uint32_t psr, __cpu_simple_lock_t *lock, uint csr)
{
	__cpu_simple_unlock(lock);

	*(volatile uint8_t *)(BS_BASE + BS_CPINT) = csr & BS_CPI_IEN;

	set_psr(psr);
}
#endif	/* MULTIPROCESSOR */
