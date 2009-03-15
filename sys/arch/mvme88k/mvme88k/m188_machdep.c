/*	$OpenBSD: m188_machdep.c,v 1.53 2009/03/15 20:39:53 miod Exp $	*/
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
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1995 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1995 Nivas Madhur
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	@(#)clock.c	8.1 (Berkeley) 6/11/93
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
#include <sys/timetc.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <machine/m88100.h>
#include <machine/mvme188.h>

#include <mvme88k/dev/sysconvar.h>
#include <dev/ic/z8536reg.h>
#include <mvme88k/mvme88k/clockvar.h>

#ifdef MULTIPROCESSOR
#include <machine/db_machdep.h>
#endif

void	m188_reset(void);
u_int	safe_level(u_int, u_int);

void	m188_bootstrap(void);
void	m188_clock_ipi_handler(struct trapframe *);
void	m188_ext_int(struct trapframe *);
u_int	m188_getipl(void);
void	m188_init_clocks(void);
void	m188_ipi_handler(struct trapframe *);
vaddr_t	m188_memsize(void);
u_int	m188_raiseipl(u_int);
void	m188_send_ipi(int, cpuid_t);
u_int	m188_setipl(u_int);
void	m188_startup(void);

/*
 * The MVME188 interrupt arbiter has 25 orthogonal interrupt sources.
 * We fold this model in the 8-level spl model this port uses, enforcing
 * priorities manually with the interrupt masks.
 */

/*
 * Copy of the interrupt enable register for each CPU.
 */
u_int32_t int_mask_reg[] = { 0, 0, 0, 0 };

u_int m188_curspl[] = { IPL_HIGH, IPL_HIGH, IPL_HIGH, IPL_HIGH };

/*
 * Interrupt masks, one per IPL level.
 */
u_int32_t int_mask_val[NIPLS];

#ifdef MULTIPROCESSOR
/*
 * Interrupts allowed on secondary processors.
 */
#define	SLAVE_MASK	0
#endif

/*
 * Figure out how much memory is available, by querying the MBus registers.
 *
 * For every 4MB segment, ask the MBus address decoder which device claimed
 * the range. Since memory is packed at low addresses, we will hit all memory
 * boards in order until reaching either a VME space or a non-claimed space.
 *
 * As a safety measure, we never check for more than 512MB - the 188 can
 * only have up to 4 memory boards, which theoretically can not be larger
 * than 128MB, and I am not aware of third-party larger memory boards.
 */
vaddr_t
m188_memsize()
{
	u_int pgnum;
	int32_t rmad;

#define	MVME188_MAX_MEMORY	((4 * 128) / 4)	/* 4 128MB boards */
	for (pgnum = 0; pgnum <	MVME188_MAX_MEMORY; pgnum++) {
		*(volatile int32_t *)MVME188_RMAD = (pgnum << 22);
		rmad = *(volatile int32_t *)MVME188_RMAD;

		if (rmad & 0x04)	/* not a memory board */
			break;
	}

	return (pgnum << 22);
}

void
m188_startup()
{
}

void
m188_bootstrap()
{
	extern struct cmmu_p cmmu8820x;

	cmmu = &cmmu8820x;
	md_interrupt_func_ptr = m188_ext_int;
	md_getipl = m188_getipl;
	md_setipl = m188_setipl;
	md_raiseipl = m188_raiseipl;
	md_init_clocks = m188_init_clocks;
#ifdef MULTIPROCESSOR
	md_send_ipi = m188_send_ipi;
#endif
	md_delay = m188_delay;
#ifdef MULTIPROCESSOR
	md_smp_setup = m88100_smp_setup;
#endif

	/* clear and disable all interrupts */
	*(volatile u_int32_t *)MVME188_IENALL = 0;
}

void
m188_reset()
{
	volatile int cnt;

	/* clear and disable all interrupts */
	*(volatile u_int32_t *)MVME188_IENALL = 0;

	if ((*(volatile u_int8_t *)MVME188_GLOBAL1) & M188_SYSCON) {
		/* Force a complete VMEbus reset */
		*(volatile u_int32_t *)MVME188_GLBRES = 1;
	} else {
		/* Force only a local reset */
		*(volatile u_int8_t *)MVME188_GLOBAL1 |= M188_LRST;
	}

	*(volatile u_int32_t *)MVME188_UCSR |= 0x2000;	/* clear SYSFAIL */
	for (cnt = 0; cnt < 5*1024*1024; cnt++)
		;
	*(volatile u_int32_t *)MVME188_UCSR |= 0x2000;	/* clear SYSFAIL */

	printf("reset failed\n");
}

/*
 * Return the next ipl >= ``curlevel'' at which we can reenable interrupts
 * while keeping ``mask'' masked.
 */
u_int
safe_level(u_int mask, u_int curlevel)
{
	int i;

#ifdef MULTIPROCESSOR
	if (mask & CLOCK_IPI_MASK)
		curlevel = max(IPL_CLOCK, curlevel);
	mask &= ~(IPI_MASK | CLOCK_IPI_MASK);
#endif
	for (i = curlevel; i < NIPLS; i++)
		if ((int_mask_val[i] & mask) == 0)
			return (i);

	return (NIPLS - 1);
}

u_int
m188_getipl(void)
{
	return m188_curspl[cpu_number()];
}

u_int
m188_setipl(u_int level)
{
	u_int curspl, mask, psr;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
	int cpu = ci->ci_cpuid;
#else
	int cpu = cpu_number();
#endif

	psr = get_psr();
	set_psr(psr | PSR_IND);

	curspl = m188_curspl[cpu];

	mask = int_mask_val[level];
#ifdef MULTIPROCESSOR
	if (cpu != master_cpu)
		mask &= SLAVE_MASK;
	mask |= SWI_IPI_MASK(cpu);
	if (level < IPL_CLOCK)
		mask |= SWI_CLOCK_IPI_MASK(cpu);
#endif

	m188_curspl[cpu] = level;
	*(u_int32_t *)MVME188_IEN(cpu) = int_mask_reg[cpu] = mask;
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);

	return curspl;
}

u_int
m188_raiseipl(u_int level)
{
	u_int mask, curspl, psr;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
	int cpu = ci->ci_cpuid;
#else
	int cpu = cpu_number();
#endif

	psr = get_psr();
	set_psr(psr | PSR_IND);

	curspl = m188_curspl[cpu];
	if (curspl < level) {
		mask = int_mask_val[level];
#ifdef MULTIPROCESSOR
		if (cpu != master_cpu)
			mask &= SLAVE_MASK;
		mask |= SWI_IPI_MASK(cpu);
		if (level < IPL_CLOCK)
			mask |= SWI_CLOCK_IPI_MASK(cpu);
#endif

		m188_curspl[cpu] = level;
		*(u_int32_t *)MVME188_IEN(cpu) = int_mask_reg[cpu] = mask;
	}
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);

	return curspl;
}

#ifdef MULTIPROCESSOR

void
m188_send_ipi(int ipi, cpuid_t cpu)
{
	struct cpu_info *ci = &m88k_cpus[cpu];
	u_int32_t bits = 0;

	if (ci->ci_ipi & ipi)
		return;

	atomic_setbits_int(&ci->ci_ipi, ipi);
	if (ipi & ~(CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK))
		bits |= SWI_IPI_BIT(cpu);
	if (ipi & (CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK))
		bits |= SWI_CLOCK_IPI_BIT(cpu);
	*(volatile u_int32_t *)MVME188_SETSWI = bits;
}

/*
 * Process inter-processor interrupts.
 */

/*
 * Unmaskable IPIs - those are processed with interrupts disabled,
 * and no lock held.
 */
void
m188_ipi_handler(struct trapframe *eframe)
{
	struct cpu_info *ci = curcpu();
	int ipi = ci->ci_ipi & (CI_IPI_DDB | CI_IPI_NOTIFY);

	*(volatile u_int32_t *)MVME188_CLRSWI = SWI_IPI_BIT(ci->ci_cpuid);
	atomic_clearbits_int(&ci->ci_ipi, ipi);

	if (ipi & CI_IPI_DDB) {
#ifdef DDB
		/*
		 * Another processor has entered DDB. Spin on the ddb lock
		 * until it is done.
		 */
		extern struct __mp_lock ddb_mp_lock;

		__mp_lock(&ddb_mp_lock);
		__mp_unlock(&ddb_mp_lock);

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
}

/*
 * Maskable IPIs
 */
void
m188_clock_ipi_handler(struct trapframe *eframe)
{
	struct cpu_info *ci = curcpu();
	int ipi = ci->ci_ipi & (CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK);

	/* clear clock ipi interrupt */
	*(volatile u_int32_t *)MVME188_CLRSWI = SWI_CLOCK_IPI_BIT(ci->ci_cpuid);
	atomic_clearbits_int(&ci->ci_ipi, ipi);

	if (ipi & CI_IPI_HARDCLOCK)
		hardclock((struct clockframe *)eframe);
	if (ipi & CI_IPI_STATCLOCK)
		statclock((struct clockframe *)eframe);
}

#endif

/*
 * Provide the interrupt source for a give interrupt status bit.
 */
const u_int obio_vec[32] = {
	0,		/* SWI0 */
	0,		/* SWI1 */
	0,		/* SWI2 */
	0,		/* SWI3 */
	INTSRC_VME,	/* VME1 */
	0,
	INTSRC_VME,	/* VME2 */
	0,		/* SIGLPI */
	0,		/* LMI */
	0,
	INTSRC_VME,	/* VME3 */
	0,
	INTSRC_VME,	/* VME4 */
	0,
	INTSRC_VME,	/* VME5 */
	0,
	0,		/* SIGHPI */
	INTSRC_DUART,	/* DI */
	0,
	INTSRC_VME,	/* VME6 */
	INTSRC_SYSFAIL,	/* SF */
	INTSRC_CIO,	/* CIOI */
	0,
	INTSRC_VME,	/* VME7 */
	0,		/* SWI4 */
	0,		/* SWI5 */
	0,		/* SWI6 */
	0,		/* SWI7 */
	INTSRC_DTIMER,	/* DTI */
	0,		/* ARBTO */
	INTSRC_ACFAIL,	/* ACF */
	INTSRC_ABORT	/* ABORT */
};

/*
 * Device interrupt handler for MVME188
 */

#define VME_VECTOR_MASK		0x1ff 	/* mask into VIACK register */
#define VME_BERR_MASK		0x100 	/* timeout during VME IACK cycle */

void
m188_ext_int(struct trapframe *eframe)
{
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
	u_int cpu = ci->ci_cpuid;
#else
	u_int cpu = cpu_number();
#endif
	u_int32_t cur_mask, ign_mask;
	u_int level, old_spl;
	struct intrhand *intr;
	intrhand_t *list;
	int ret, intbit;
	vaddr_t ivec;
	u_int intsrc, vec;
	int unmasked = 0;
	int warn;
#ifdef DIAGNOSTIC
	static int problems = 0;
#endif

	cur_mask = ISR_GET_CURRENT_MASK(cpu);
	ign_mask = 0;
	old_spl = eframe->tf_mask;

	if (cur_mask == 0) {
		/*
		 * Spurious interrupts - may be caused by debug output clearing
		 * DUART interrupts.
		 */
#ifdef MULTIPROCESSOR
		if (cpu != master_cpu) {
			if (++problems >= 10) {
				printf("cpu%d: interrupt pin won't clear, "
				    "disabling processor\n", cpu);
				cpu_emergency_disable();
				/* NOTREACHED */
			}
		}
#endif
		flush_pipeline();
		goto out;
	}

	uvmexp.intrs++;

#ifdef MULTIPROCESSOR
	/*
	 * Handle unmaskable IPIs immediately, so that we can reenable
	 * interrupts before further processing. We rely on the interrupt
	 * mask to make sure that if we get an IPI, it's really for us
	 * and no other processor.
	 */
	if (cur_mask & IPI_MASK) {
		m188_ipi_handler(eframe);
		cur_mask &= ~IPI_MASK;
		if (cur_mask == 0)
			goto out;
	}
#endif

#ifdef MULTIPROCESSOR
	if (old_spl < IPL_SCHED)
		__mp_lock(&kernel_lock);
#endif

	/*
	 * We want to service all interrupts marked in the IST register
	 * They are all valid because the mask would have prevented them
	 * from being generated otherwise.  We will service them in order of
	 * priority.
	 */
	do {
		level = safe_level(cur_mask, old_spl);
		m188_setipl(level);

		if (unmasked == 0) {
			set_psr(get_psr() & ~PSR_IND);
			unmasked = 1;
		}

#ifdef MULTIPROCESSOR
		/*
		 * Handle pending maskable IPIs first.
		 */
		if (cur_mask & CLOCK_IPI_MASK) {
			m188_clock_ipi_handler(eframe);
			cur_mask &= ~CLOCK_IPI_MASK;
			if (cur_mask == 0)
				break;
		}
#endif

		/* find the first bit set in the current mask */
		warn = 0;
		intbit = ff1(cur_mask);
		intsrc = obio_vec[intbit];

		if (intsrc == 0)
			panic("%s: unexpected interrupt source (bit %d), "
			    "level %d, mask 0x%b",
			    __func__, intbit, level, cur_mask, IST_STRING);

		if (intsrc == INTSRC_VME) {
			ivec = MVME188_VIRQLV + (level << 2);
			vec = *(volatile u_int32_t *)ivec & VME_VECTOR_MASK;
			if (vec & VME_BERR_MASK) {
				/*
				 * If only one VME interrupt is registered
				 * with this IPL, we can reasonably safely
				 * assume that this is our vector.
				 */
				vec = vmevec_hints[level];
				if (vec == (u_int)-1) {
					printf("%s: timeout getting VME "
					    "interrupt vector, "
					    "level %d, mask 0x%b\n",
					    __func__, level,
					   cur_mask, IST_STRING); 
					ign_mask |=  1 << intbit;
					continue;
				}
			}
			list = &intr_handlers[vec];
		} else {
			list = &sysconintr_handlers[intsrc];
		}

		if (SLIST_EMPTY(list)) {
			warn = 1;
		} else {
			/*
			 * Walk through all interrupt handlers in the chain
			 * for the given vector, calling each handler in turn,
			 * till some handler returns a value != 0.
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
			if (ret == 0)
				warn = 2;
		}

		if (warn != 0) {
			ign_mask |= 1 << intbit;

			if (intsrc == INTSRC_VME)
				printf("%s: %s VME interrupt, "
				    "level %d, vec 0x%x, mask 0x%b\n",
				    __func__,
				    warn == 1 ? "spurious" : "unclaimed",
				    level, vec, cur_mask, IST_STRING);
			else
				printf("%s: %s interrupt, "
				    "level %d, bit %d, mask 0x%b\n",
				    __func__,
				    warn == 1 ? "spurious" : "unclaimed",
				    level, intbit, cur_mask, IST_STRING);
		}
	} while (((cur_mask = ISR_GET_CURRENT_MASK(cpu)) & ~ign_mask &
	    ~IPI_MASK) != 0);

#ifdef DIAGNOSTIC
	if (ign_mask != 0) {
		if (++problems >= 10)
			panic("%s: broken interrupt behaviour", __func__);
	} else
		problems = 0;
#endif

#ifdef MULTIPROCESSOR
	if (old_spl < IPL_SCHED)
		__mp_unlock(&kernel_lock);
#endif

out:
	/*
	 * process any remaining data access exceptions before
	 * returning to assembler
	 */
	if (eframe->tf_dmt0 & DMT_VALID)
		m88100_trap(T_DATAFLT, eframe);

	/*
	 * Disable interrupts before returning to assembler, the spl will
	 * be restored later.
	 */
	set_psr(get_psr() | PSR_IND);
}

/*
 * Clock routines
 */

void	m188_cio_init(u_int);
u_int	read_cio(int);
void	write_cio(int, u_int);

int	m188_clockintr(void *);
int	m188_calibrateintr(void *);
int	m188_statintr(void *);
u_int	m188_cio_get_timecount(struct timecounter *);

volatile int	m188_calibrate_phase = 0;

/* multiplication factor for delay() */
u_int	m188_delay_const = 25;		/* no MVME188 is faster than 25MHz */

uint32_t	cio_step;
uint32_t	cio_refcnt;
uint32_t	cio_lastcnt;

struct mutex cio_mutex = MUTEX_INITIALIZER(IPL_CLOCK);

/*
 * Notes on the MVME188 clock usage:
 *
 * We have two sources for timers:
 * - two counter/timers in the DUART (MC68681/MC68692)
 * - three counter/timers in the Zilog Z8536
 *
 * However:
 * - Z8536 CT#3 is reserved as a watchdog device; and its input is
 *   user-controllable with jumpers on the SYSCON board, so we can't
 *   really use it.
 * - When using the Z8536 in timer mode, it _seems_ like it resets at
 *   0xffff instead of the initial count value...
 * - Despite having per-counter programmable interrupt vectors, the
 *   SYSCON logic forces fixed vectors for the DUART and the Z8536 timer
 *   interrupts.
 * - The DUART timers keep counting down from 0xffff even after
 *   interrupting, and need to be manually stopped, then restarted, to
 *   resume counting down the initial count value.
 *
 * Also, while the Z8536 has a very reliable 4MHz clock source, the
 * 3.6864MHz clock source of the DUART timers does not seem to be correct.
 *
 * As a result, clock is run on a Z8536 counter, kept in counter mode and
 * retriggered every interrupt, while statclock is run on a DUART counter,
 * but in practice runs at an average 96Hz instead of the expected 100Hz.
 *
 * It should be possible to run statclock on the Z8536 counter #2, but
 * this would make interrupt handling more tricky, in the case both
 * counters interrupt at the same time...
 */

#define	DART_ISR		0xfff82017	/* interrupt status */
#define	DART_IVR		0xfff82033	/* interrupt vector */
#define	DART_STARTC		0xfff8203b	/* start counter cmd */
#define	DART_STOPC		0xfff8203f	/* stop counter cmd */
#define	DART_ACR		0xfff82013	/* auxiliary control */
#define	DART_CTUR		0xfff8201b	/* counter/timer MSB */
#define	DART_CTLR		0xfff8201f	/* counter/timer LSB */
#define	DART_OPCR		0xfff82037	/* output port config*/

struct timecounter m188_cio_timecounter = {
	m188_cio_get_timecount,
	NULL,
	0xffffffff,
	0,
	"cio",
	0,
	NULL
};

void
m188_init_clocks(void)
{
	volatile u_int8_t imr;
	int statint, minint;
	u_int iter, divisor;
	u_int32_t psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);

#ifdef DIAGNOSTIC
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#endif
	tick = 1000000 / hz;

	m188_cio_init(tick);

	if (stathz == 0)
		stathz = hz;
#ifdef DIAGNOSTIC
	if (1000000 % stathz) {
		printf("cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}
#endif
	profhz = stathz;		/* always */

	/*
	 * The DUART runs at 3.6864 MHz, CT#1 will run in PCLK/16 mode.
	 */
	statint = (3686400 / 16) / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;
	statmin = statint - (statvar >> 1);

	/* clear the counter/timer output OP3 while we program the DART */
	*(volatile u_int8_t *)DART_OPCR = 0x00;
	/* do the stop counter/timer command */
	imr = *(volatile u_int8_t *)DART_STOPC;
	/* set counter/timer to counter mode, PCLK/16 */
	*(volatile u_int8_t *)DART_ACR = 0x30;
	*(volatile u_int8_t *)DART_CTUR = (statint >> 8);
	*(volatile u_int8_t *)DART_CTLR = (statint & 0xff);
	/* set the counter/timer output OP3 */
	*(volatile u_int8_t *)DART_OPCR = 0x04;
	/* give the start counter/timer command */
	imr = *(volatile u_int8_t *)DART_STARTC;

	statclock_ih.ih_fn = m188_statintr;
	statclock_ih.ih_arg = 0;
	statclock_ih.ih_wantframe = 1;
	statclock_ih.ih_ipl = IPL_STATCLOCK;
	sysconintr_establish(INTSRC_DTIMER, &statclock_ih, "stat");

	/*
	 * Calibrate delay const.
	 */
	clock_ih.ih_fn = m188_calibrateintr;
	clock_ih.ih_arg = 0;
	clock_ih.ih_wantframe = 1;
	clock_ih.ih_ipl = IPL_CLOCK;
	sysconintr_establish(INTSRC_CIO, &clock_ih, "clock");

	m188_delay_const = 1;
	set_psr(psr);
	while (m188_calibrate_phase == 0)
		;

	iter = 0;
	while (m188_calibrate_phase == 1) {
		delay(10000);
		iter++;
	}

	divisor = 1000000 / 10000;
	m188_delay_const = (iter * hz + divisor - 1) / divisor;

	set_psr(psr | PSR_IND);

	sysconintr_disestablish(INTSRC_CIO, &clock_ih);
	clock_ih.ih_fn = m188_clockintr;
	sysconintr_establish(INTSRC_CIO, &clock_ih, "clock");

	set_psr(psr);

	tc_init(&m188_cio_timecounter);
}

int
m188_calibrateintr(void *eframe)
{
	/* no need to grab the mutex, only one processor is running for now */
	/* ack the interrupt */
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_C_IP); 

	m188_calibrate_phase++;

	return (1);
}

int
m188_clockintr(void *eframe)
{
	mtx_enter(&cio_mutex);
	/* ack the interrupt */
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_C_IP); 
	cio_refcnt += cio_step;
	mtx_leave(&cio_mutex);

	hardclock(eframe);

#ifdef MULTIPROCESSOR
	/*
	 * Send an IPI to all other processors, so they can get their
	 * own ticks.
	 */
	m88k_broadcast_ipi(CI_IPI_HARDCLOCK);
#endif

	return (1);
}

int
m188_statintr(void *eframe)
{
	volatile u_int8_t tmp;
	u_long newint, r, var;

	/* stop counter and acknowledge interrupt */
	tmp = *(volatile u_int8_t *)DART_STOPC;
	tmp = *(volatile u_int8_t *)DART_ISR;

	/*
	 * Compute new randomized interval.  The intervals are
	 * uniformly distributed on
	 * [statint - statvar / 2, statint + statvar / 2],
	 * and therefore have mean statint, giving a stathz
	 * frequency clock.
	 */
	var = statvar;
	do {
		r = random() & (var - 1);
	} while (r == 0);
	newint = statmin + r;

	/* setup new value and restart counter */
	*(volatile u_int8_t *)DART_CTUR = (newint >> 8);
	*(volatile u_int8_t *)DART_CTLR = (newint & 0xff);
	tmp = *(volatile u_int8_t *)DART_STARTC;

	statclock((struct clockframe *)eframe);

#ifdef MULTIPROCESSOR
	/*
	 * Send an IPI to all other processors as well.
	 */
	m88k_broadcast_ipi(CI_IPI_STATCLOCK);
#endif

	return (1);
}

/* Write CIO register */
void
write_cio(int reg, u_int val)
{
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	i = *cio_ctrl;				/* goto state 1 */
	*cio_ctrl = 0;				/* take CIO out of RESET */
	i = *cio_ctrl;				/* reset CIO state machine */

	*cio_ctrl = (reg & 0xff);		/* select register */
	*cio_ctrl = (val & 0xff);		/* write the value */
}

/* Read CIO register */
u_int
read_cio(int reg)
{
	int c;
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	/* select register */
	*cio_ctrl = (reg & 0xff);
	/* delay for a short time to allow 8536 to settle */
	for (i = 0; i < 100; i++)
		;
	/* read the value */
	c = *cio_ctrl;
	return (c & 0xff);
}

/*
 * Initialize the CTC (8536)
 * Only the counter/timers are used - the IO ports are un-comitted.
 */
void
m188_cio_init(u_int period)
{
	volatile int i;

	/* Start by forcing chip into known state */
	read_cio(ZCIO_MIC);
	write_cio(ZCIO_MIC, ZCIO_MIC_RESET);	/* Reset the CTC */
	for (i = 0; i < 1000; i++)	 	/* Loop to delay */
		;

	/* Clear reset and start init seq. */
	write_cio(ZCIO_MIC, 0x00);

	/* Wait for chip to come ready */
	while ((read_cio(ZCIO_MIC) & ZCIO_MIC_RJA) == 0)
		;

	/* Initialize the 8536 for real */
	write_cio(ZCIO_MIC,
	    ZCIO_MIC_MIE /* | ZCIO_MIC_NV */ | ZCIO_MIC_RJA | ZCIO_MIC_DLC);
	write_cio(ZCIO_CT1MD, ZCIO_CTMD_CSC);	/* Continuous count */
	write_cio(ZCIO_PBDIR, 0xff);		/* set port B to input */

	period <<= 1;	/* CT#1 runs at PCLK/2, hence 2MHz */
	write_cio(ZCIO_CT1TCM, period >> 8);
	write_cio(ZCIO_CT1TCL, period);
	/* enable counter #1 */
	write_cio(ZCIO_MCC, ZCIO_MCC_CT1E | ZCIO_MCC_PBE);
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_TCB | ZCIO_CTCS_S_IE);

	cio_step = period;
	m188_cio_timecounter.tc_frequency = (u_int64_t)cio_step * hz;
}

u_int
m188_cio_get_timecount(struct timecounter *tc)
{
	u_int cmsb, clsb, counter, curcnt;

	/*
	 * The CIO counter is free running, but by setting the
	 * RCC bit in its control register, we can read a frozen
	 * value of the counter.
	 * The counter will automatically unfreeze after reading
	 * its LSB.
	 */

	mtx_enter(&cio_mutex);
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_RCC);
	cmsb = read_cio(ZCIO_CT1CCM);
	clsb = read_cio(ZCIO_CT1CCL);
	curcnt = cio_refcnt;

	counter = (cmsb << 8) | clsb;
#if 0	/* this will never happen unless the period itself is 65536 */
	if (counter == 0)
		counter = 65536;
#endif

	/*
	 * The counter counts down from its initialization value to 1.
	 */
	counter = cio_step - counter;

	curcnt += counter;
	if (curcnt < cio_lastcnt)
		curcnt += cio_step;

	cio_lastcnt = curcnt;
	mtx_leave(&cio_mutex);
	return curcnt;
}
