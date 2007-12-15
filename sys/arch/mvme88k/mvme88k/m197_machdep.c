/*	$OpenBSD: m197_machdep.c,v 1.22 2007/12/15 19:37:41 miod Exp $	*/
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
void	m197_clock_ipi_handler(struct trapframe *);
void	m197_ext_int(u_int, struct trapframe *);
u_int	m197_getipl(void);
void	m197_ipi_handler(struct trapframe *);
vaddr_t	m197_memsize(void);
u_int	m197_raiseipl(u_int);
u_int	m197_setipl(u_int);
void	m197_startup(void);

vaddr_t obiova;
vaddr_t flashva;

/*
 * Figure out how much real memory is available.
 * Start looking from the megabyte after the end of the kernel data,
 * until we find non-memory.
 */
vaddr_t
m197_memsize()
{
	unsigned int *volatile look;
	unsigned int *max;
	extern char *end;
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
	/*
	 * count it up.
	 */
#define	MAXPHYSMEM	0x30000000	/* 768MB */
	max = (void *)MAXPHYSMEM;
	for (look = (void *)Roundup(end, STRIDE); look < max;
	    look = (int *)((unsigned)look + STRIDE)) {
		unsigned save;

		/* if can't access, we've reached the end */
		if (badaddr((vaddr_t)look, 4)) {
#if defined(DEBUG)
			printf("%x\n", look);
#endif
			look = (int *)((int)look - STRIDE);
			break;
		}

		/*
		 * If we write a value, we expect to read the same value back.
		 * We'll do this twice, the 2nd time with the opposite bit
		 * pattern from the first, to make sure we check all bits.
		 */
		save = *look;
		if (*look = PATTERN, *look != PATTERN)
			break;
		if (*look = ~PATTERN, *look != ~PATTERN)
			break;
		*look = save;
	}

	return (trunc_page((vaddr_t)look));
}

void
m197_startup()
{
	/*
	 * Grab the FLASH space that we hardwired in pmap_bootstrap
	 */
	flashva = FLASH_START;
	uvm_map(kernel_map, (vaddr_t *)&flashva, FLASH_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (flashva != FLASH_START)
		panic("flashva %lx: FLASH not free", flashva);

	/*
	 * Grab the OBIO space that we hardwired in pmap_bootstrap
	 */
	obiova = OBIO197_START;
	uvm_map(kernel_map, (vaddr_t *)&obiova, OBIO197_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (obiova != OBIO197_START)
		panic("obiova %lx: OBIO not free", obiova);
}

/*
 * Device interrupt handler for MVME197
 */

void
m197_ext_int(u_int v, struct trapframe *eframe)
{
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
#endif
	u_int32_t psr;
	int level;
	struct intrhand *intr;
	intrhand_t *list;
	int ret;
	vaddr_t ivec;
	u_int8_t vec;
	u_int8_t abort;

#ifdef MULTIPROCESSOR
	if (eframe->tf_mask < IPL_SCHED)
		__mp_lock(&kernel_lock);
#endif

	uvmexp.intrs++;

	if (v == T_NON_MASK) {
		/*
		 * Non-maskable interrupts are either the abort switch (on
		 * cpu0 only) or IPIs (on any cpu). We check for IPI first.
		 */
#ifdef MULTIPROCESSOR
		if ((*(volatile u_int8_t *)(BS_BASE + BS_CPINT)) & BS_CPI_INT)
			m197_ipi_handler(eframe);
#endif

		abort = *(u_int8_t *)(BS_BASE + BS_ABORT);
		if (abort & BS_ABORT_INT) {
			*(u_int8_t *)(BS_BASE + BS_ABORT) =
			    abort | BS_ABORT_ICLR;
			nmihand(eframe);
		}

#ifdef MULTIPROCESSOR
		/*
		 * If we have pending hardware IPIs and the current
		 * level allows them to be processed, do them now.
		 */
		if (eframe->tf_mask < IPL_SCHED &&
		    ISSET(ci->ci_ipi,
		      CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK)) {
			psr = get_psr();
			set_psr(psr & ~PSR_IND);
			m197_clock_ipi_handler(eframe);
			set_psr(psr);
		}
#endif
	} else {
		level = *(u_int8_t *)M197_ILEVEL & 0x07;
		/* generate IACK and get the vector */
		ivec = M197_IACK + (level << 2) + 0x03;
		vec = *(volatile u_int8_t *)ivec;

		/* block interrupts at level or lower */
		m197_setipl(level);
		psr = get_psr();
		set_psr(psr & ~PSR_IND);

#ifdef MULTIPROCESSOR
		/*
		 * If we have pending hardware IPIs and the current
		 * level allows them to be processed, do them now.
		 */
		if (eframe->tf_mask < IPL_SCHED &&
		    ISSET(ci->ci_ipi, CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK))
			m197_clock_ipi_handler(eframe);
#endif

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

		/*
		 * Disable interrupts before returning to assembler,
		 * the spl will be restored later.
		 */
		set_psr(psr | PSR_IND);
	}

#ifdef MULTIPROCESSOR
	if (eframe->tf_mask < IPL_SCHED)
		__mp_unlock(&kernel_lock);
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

	md_interrupt_func_ptr = m197_ext_int;
	md_getipl = m197_getipl;
	md_setipl = m197_setipl;
	md_raiseipl = m197_raiseipl;
	md_init_clocks = m1x7_init_clocks;
#ifdef MULTIPROCESSOR
	md_send_ipi = m197_send_ipi;
#endif
}

#ifdef MULTIPROCESSOR

void
m197_send_ipi(int ipi, cpuid_t cpu)
{
	struct cpu_info *ci = &m88k_cpus[cpu];

	if (ci->ci_ipi & ipi)
		return;

	if (ci->ci_ddb_state == CI_DDB_PAUSE)
		return;				/* XXX skirting deadlock */

	atomic_setbits_int(&ci->ci_ipi, ipi);

	/*
	 * If the other processor doesn't have an IPI pending, send one,
	 * keeping IPIs enabled for us.
	 */
	if ((*(volatile u_int8_t *)(BS_BASE + BS_CPINT) & BS_CPI_STAT) == 0)
		*(volatile u_int8_t *)(BS_BASE + BS_CPINT) =
		    BS_CPI_SCPI | BS_CPI_IEN;
}

void
m197_send_complex_ipi(int ipi, cpuid_t cpu, u_int32_t arg1, u_int32_t arg2)
{
	struct cpu_info *ci = &m88k_cpus[cpu];
	int wait = 0;

	if ((ci->ci_flags & CIF_ALIVE) == 0)
		return;				/* XXX not ready yet */

	if (ci->ci_ddb_state == CI_DDB_PAUSE)
		return;				/* XXX skirting deadlock */

	/*
	 * Wait for the other processor to be ready to accept an IPI.
	 */
	for (wait = 10000000; wait != 0; wait--) {
		if (!ISSET(*(volatile u_int8_t *)(BS_BASE + BS_CPINT),
		    BS_CPI_STAT))
			break;
	}
	if (wait == 0)
		panic("couldn't send complex ipi %x to cpu %d", ipi, cpu);

	/*
	 * In addition to the ipi bit itself, we need to set up ipi arguments.
	 * Note that we do not need to protect against another processor
	 * trying to send another complex IPI, since we know there are only
	 * two processors on the board.
	 */
	ci->ci_ipi_arg1 = arg1;
	ci->ci_ipi_arg2 = arg2;
	atomic_setbits_int(&ci->ci_ipi, ipi);

	/*
	 * Send an IPI, keeping our IPIs enabled.
	 */
	*(volatile u_int8_t *)(BS_BASE + BS_CPINT) = BS_CPI_SCPI | BS_CPI_IEN;
}

void
m197_ipi_handler(struct trapframe *eframe)
{
	struct cpu_info *ci = curcpu();
	int ipi = ci->ci_ipi & ~(CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK);
	u_int32_t arg1, arg2;

	if (ipi != 0)
		atomic_clearbits_int(&ci->ci_ipi, ipi);

	/*
	 * Complex IPIs (with extra arguments). There can only be one
	 * pending at the same time, sending processor will wait for us
	 * to have processed the current one before sending a new one.
	 */
	if (ipi &
	    (CI_IPI_CACHE_FLUSH | CI_IPI_ICACHE_FLUSH)) {
		arg1 = ci->ci_ipi_arg1;
		arg2 = ci->ci_ipi_arg2;

		if (ipi & CI_IPI_CACHE_FLUSH) {
			cmmu_flush_cache(ci->ci_cpuid, arg1, arg2);
		}
		else if (ipi & CI_IPI_ICACHE_FLUSH) {
			cmmu_flush_inst_cache(ci->ci_cpuid, arg1, arg2);
		}
	}

	/*
	 * Regular, simple, IPIs. We can have as many bits set as possible.
	 */
	if (ipi & CI_IPI_TLB_FLUSH_KERNEL) {
		cmmu_flush_tlb(ci->ci_cpuid, 1, 0, 0);
	}
	if (ipi & CI_IPI_TLB_FLUSH_USER) {
		cmmu_flush_tlb(ci->ci_cpuid, 0, 0, 0);
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
		/* nothing to do */
	}

	/*
	 * Acknowledge IPIs.
	 */
	*(volatile u_int8_t *)(BS_BASE + BS_CPINT) = BS_CPI_ICLR | BS_CPI_IEN;
}

/*
 * Maskable IPIs.
 *
 * These IPIs are received as non maskable, but are only processed if
 * the current spl permits it; so they are checked again on return from
 * regular interrupts to process them as soon as possible.
 */
void
m197_clock_ipi_handler(struct trapframe *eframe)
{
	struct cpu_info *ci = curcpu();
	int ipi = ci->ci_ipi & (CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK);
	int s;

	atomic_clearbits_int(&ci->ci_ipi, ipi);

	s = splclock();
	if (ipi & CI_IPI_HARDCLOCK)
		hardclock((struct clockframe *)eframe);
	if (ipi & CI_IPI_STATCLOCK)
		statclock((struct clockframe *)eframe);
	splx(s);
}

#endif
