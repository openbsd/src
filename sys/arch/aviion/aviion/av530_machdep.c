/*	$OpenBSD: av530_machdep.c,v 1.6 2011/01/02 13:40:05 miod Exp $	*/
/*
 * Copyright (c) 2006, 2007, 2010 Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/board.h>
#include <machine/bus.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/pmap_table.h>
#include <machine/trap.h>

#include <machine/m88100.h>
#include <machine/m8820x.h>
#include <machine/avcommon.h>
#include <machine/av530.h>
#include <machine/prom.h>

#include <aviion/dev/sysconvar.h>
#include <aviion/dev/vmevar.h>

#ifdef MULTIPROCESSOR
#include <machine/db_machdep.h>
#endif

u_int	av530_safe_level(u_int, u_int, u_int);
void	av530_clock_ipi_handler(struct trapframe *);
void	av530_ipi_handler(struct trapframe *);

const struct pmap_table
av530_ptable[] = {
	{ AV530_PROM,	AV530_PROM_SIZE,	UVM_PROT_RW,	CACHE_INH },
#if 0	/* mapped by the hardcoded BATC entries */
	{ AV530_UTILITY,AV530_UTILITY_SIZE,	UVM_PROT_RW,	CACHE_INH },
#endif
	{ 0, (vsize_t)-1, 0, 0 }
};

const struct vme_range vme_av530[] = {
	{ VME_A16,
	  AV530_VME16_START,	AV530_VME16_END,	AV530_VME16_BASE },
	{ VME_A24,
	  AV530_VME24_START,	AV530_VME24_END,	AV530_VME24_BASE },
	{ VME_A32,
	  AV530_VME32_START1,	AV530_VME32_END1,	AV530_VME32_BASE },
	{ VME_A32,
	  AV530_VME32_START2,	AV530_VME32_END2,	AV530_VME32_BASE },
	{ 0 }
};

const struct board board_av530 = {
	av530_bootstrap,
	av530_memsize,
	av530_startup,
	av530_intr,
	rtc_init_clocks,
	av530_getipl,
	av530_setipl,
	av530_raiseipl,
#ifdef MULTIPROCESSOR
	av530_send_ipi,
	m88100_smp_setup,
#endif
	av530_intsrc,
	av530_get_vme_ranges,

	av530_ptable
};

/*
 * The MVME188 interrupt arbiter has 25 orthogonal interrupt sources.
 * On the AViiON 530 machines, there are even more interrupt sources in use,
 * requiring the use of two arbiters.
 * We fold this model in the 8-level spl model this port uses, enforcing
 * priorities manually with the interrupt masks.
 */

/*
 * Copy of the interrupt enable registers for each CPU.
 */
u_int32_t av530_int_mask_reg[] = { 0, 0, 0, 0 };
u_int32_t av530_ext_int_mask_reg[] = { 0, 0, 0, 0 };

u_int av530_curspl[] = { IPL_HIGH, IPL_HIGH, IPL_HIGH, IPL_HIGH };

#ifdef MULTIPROCESSOR
/*
 * Interrupts allowed on secondary processors.
 */
#define	SLAVE_MASK	0
#define	SLAVE_EXMASK	0
#endif

/*
 * Figure out how much memory is available, by asking the PROM.
 */
vaddr_t
av530_memsize()
{
	vaddr_t memsize0, memsize1;

	memsize0 = scm_memsize(0);
	memsize1 = scm_memsize(1);

	/*
	 * What we got is the ``top of memory'', i.e. the largest addressable
	 * word address, ending in 0xffc. Round up to a multiple of a page.
	 */
	memsize0 = round_page(memsize0);
	memsize1 = round_page(memsize1);

	physmem = atop(memsize0);
	return (memsize1);
}

void
av530_startup()
{
}

void
av530_bootstrap()
{
	extern struct cmmu_p cmmu8820x;
#if 0
	extern u_char hostaddr[6];
#endif
	uint32_t whoami;

	/*
	 * Set up CMMU addresses. We need to access the WHOAMI register
	 * early since addresses differ between the 6:1 and 2:1 designs.
	 */
	cmmu = &cmmu8820x;
	whoami = (*(volatile u_int32_t *)AV_WHOAMI & 0xf0) >> 4;
	switch (whoami) {
	default:	/* 2:1 systems */
		m8820x_cmmu[0].cmmu_regs = (void *)AV530_CMMU_I0;
		m8820x_cmmu[1].cmmu_regs = (void *)AV530_CMMU_D0;
		m8820x_cmmu[2].cmmu_regs = (void *)AV530_CMMU_I1;
		m8820x_cmmu[3].cmmu_regs = (void *)AV530_CMMU_D1;
		break;
	case 3:
	case 7:		/* 6:1 systems */
		m8820x_cmmu[0].cmmu_regs = (void *)AV530_CMMU6_I0;
		m8820x_cmmu[1].cmmu_regs = (void *)AV530_CMMU6_D0;
		m8820x_cmmu[2].cmmu_regs = (void *)AV530_CMMU6_I1;
		m8820x_cmmu[3].cmmu_regs = (void *)AV530_CMMU6_D1;
		m8820x_cmmu[4].cmmu_regs = (void *)AV530_CMMU6_I2;
		m8820x_cmmu[6].cmmu_regs = (void *)AV530_CMMU6_I3;

		m8820x_cmmu[8].cmmu_regs = (void *)AV530_CMMU6_I4;
		m8820x_cmmu[9].cmmu_regs = (void *)AV530_CMMU6_D2;
		m8820x_cmmu[10].cmmu_regs = (void *)AV530_CMMU6_I5;
		m8820x_cmmu[11].cmmu_regs = (void *)AV530_CMMU6_D3;
		m8820x_cmmu[12].cmmu_regs = (void *)AV530_CMMU6_I6;
		m8820x_cmmu[14].cmmu_regs = (void *)AV530_CMMU6_I7;
		break;
	}

	/* clear and disable all interrupts */
	*(volatile u_int32_t *)AV_IENALL = 0;
	*(volatile u_int32_t *)AV_EXIENALL = 0;

#if 0
	/*
	 * Get all the information we'll need later from the PROM, while
	 * we can still use it.
	 */
	scm_getenaddr(hostaddr);
#endif
}

/*
 * Return the next ipl >= ``curlevel'' at which we can reenable interrupts
 * while keeping ``mask'' and ``exmask'' masked.
 */
u_int
av530_safe_level(u_int mask, u_int exmask, u_int curlevel)
{
	int i;

#ifdef MULTIPROCESSOR
	if (mask & AV530_CLOCK_IPI_MASK)
		curlevel = max(IPL_CLOCK, curlevel);
	mask &= ~(AV530_IPI_MASK | AV530_CLOCK_IPI_MASK);
#endif
	for (i = curlevel; i < NIPLS; i++)
		if ((int_mask_val[i] & mask) == 0 &&
		    (ext_int_mask_val[i] & exmask) == 0)
			return i;

	return (NIPLS - 1);
}

u_int
av530_getipl(void)
{
	return av530_curspl[cpu_number()];
}

u_int
av530_setipl(u_int level)
{
	u_int32_t mask, exmask, curspl, psr;
	u_int cpu = cpu_number();

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = av530_curspl[cpu];

	mask = int_mask_val[level];
	exmask = ext_int_mask_val[level];
#ifdef MULTIPROCESSOR
	if (cpu != master_cpu) {
		mask &= SLAVE_MASK;
		exmask &= SLAVE_EXMASK;
	}
	mask |= AV530_SWI_IPI_MASK(cpu);
	if (level < IPL_CLOCK)
		mask |= AV530_SWI_CLOCK_IPI_MASK(cpu);
#endif

	av530_curspl[cpu] = level;
	*(u_int32_t *)AV_IEN(cpu) = av530_int_mask_reg[cpu] = mask;
	*(u_int32_t *)AV_EXIEN(cpu) = av530_ext_int_mask_reg[cpu] = exmask;
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);

	return curspl;
}

u_int
av530_raiseipl(u_int level)
{
	u_int32_t mask, exmask, curspl, psr;
	u_int cpu = cpu_number();

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = av530_curspl[cpu];
	if (curspl < level) {
		mask = int_mask_val[level];
		exmask = ext_int_mask_val[level];
#ifdef MULTIPROCESSOR
		if (cpu != master_cpu) {
			mask &= SLAVE_MASK;
			exmask &= SLAVE_EXMASK;
		}
		mask |= AV530_SWI_IPI_MASK(cpu);
		if (level < IPL_CLOCK)
			mask |= AV530_SWI_CLOCK_IPI_MASK(cpu);
#endif

		av530_curspl[cpu] = level;
		*(u_int32_t *)AV_IEN(cpu) = av530_int_mask_reg[cpu] = mask;
		*(u_int32_t *)AV_EXIEN(cpu) =
		    av530_ext_int_mask_reg[cpu] = exmask;
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
av530_send_ipi(int ipi, cpuid_t cpu)
{
	struct cpu_info *ci = &m88k_cpus[cpu];
	uint32_t bits = 0;

	if (ci->ci_ipi & ipi)
		return;

	atomic_setbits_int(&ci->ci_ipi, ipi);
	if (ipi & ~(CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK))
		bits |= AV530_SWI_IPI_BIT(cpu);
	if (ipi & (CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK))
		bits |= AV530_SWI_CLOCK_IPI_BIT(cpu);
	*(volatile u_int32_t *)AV530_SETSWI = bits;
}

/*
 * Process inter-processor interrupts.
 */

/*
 * Unmaskable IPIs - those are processed with interrupts disabled,
 * and no lock held.
 */
void
av530_ipi_handler(struct trapframe *eframe)
{
	struct cpu_info *ci = curcpu();
	int ipi = ci->ci_ipi & (CI_IPI_DDB | CI_IPI_NOTIFY);

	*(volatile u_int32_t *)AV530_CLRSWI = AV530_SWI_IPI_BIT(ci->ci_cpuid);
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
av530_clock_ipi_handler(struct trapframe *eframe)
{
	struct cpu_info *ci = curcpu();
	int ipi = ci->ci_ipi & (CI_IPI_HARDCLOCK | CI_IPI_STATCLOCK);

	/* clear clock ipi interrupt */
	*(volatile u_int32_t *)AV530_CLRSWI =
	    AV530_SWI_CLOCK_IPI_BIT(ci->ci_cpuid);
	atomic_clearbits_int(&ci->ci_ipi, ipi);

	if (ipi & CI_IPI_HARDCLOCK)
		hardclock((struct clockframe *)eframe);
#if 0	/* no separate statclock yet */
	if (ipi & CI_IPI_STATCLOCK)
		statclock((struct clockframe *)eframe);
#endif
}

#endif

/*
 * Provide the interrupt masks for a given logical interrupt source.
 */
u_int64_t
av530_intsrc(int i)
{
	static const u_int32_t intsrc[] = {
		0,
		AV530_IRQ_ABORT,
		AV530_IRQ_ACF,
		AV530_IRQ_SF,
		0,
		AV530_IRQ_DI,
		0,
		0,
		0,
		0,
		0,
		AV530_IRQ_VME1,
		AV530_IRQ_VME2,
		AV530_IRQ_VME3,
		AV530_IRQ_VME4,
		AV530_IRQ_VME5,
		AV530_IRQ_VME6,
		AV530_IRQ_VME7
	}, ext_intsrc[] = {
		0,
		0,
		0,
		0,
		AV530_EXIRQ_PIT0OF,
		0,
		AV530_EXIRQ_DUART2,
		AV530_EXIRQ_LAN0,
		AV530_EXIRQ_LAN1,
		AV530_EXIRQ_SCSI0,
		AV530_EXIRQ_SCSI1,
		0,
		0,
		0,
		0,
		0,
		0,
		0
	};
	uint64_t isrc;

	isrc = ext_intsrc[i];
	isrc = (isrc << 32) | intsrc[i];
	return isrc;
}

/*
 * Provide the interrupt source for a given interrupt status bit.
 */
static const u_int av530_obio_vec[32] = {
	0,			/* SWI0 */
	0,			/* SWI1 */
	0,			/* SWI2 */
	0,			/* SWI3 */
	INTSRC_VME(1),		/* VME1 */
	0,
	INTSRC_VME(2),		/* VME2 */
	0,			/* SIGLPI */
	0,			/* LMI */
	0,
	INTSRC_VME(3),		/* VME3 */
	0,
	INTSRC_VME(4),		/* VME4 */
	0,
	INTSRC_VME(5),		/* VME5 */
	0,
	0,			/* HPI */
	INTSRC_DUART1,		/* DI */
	0,			/* MEM */
	INTSRC_VME(6),		/* VME6 */
	INTSRC_SYSFAIL,		/* SF */
	0,
	0,			/* KBD */
	INTSRC_VME(7),		/* VME7 */
	0,			/* SWI4 */
	0,			/* SWI5 */
	0,			/* SWI6 */
	0,			/* SWI7 */
	0,			/* DTI */
	0,			/* ARBTO */
	INTSRC_ACFAIL,		/* ACF */
	INTSRC_ABORT		/* ABORT */
};
static const u_int av530_obio_exvec[32] = {
	0,
	0,
	0,
	0,
	0,
	0,			/* PDMA */
	0,			/* IOEXP2 */
	0,
	0,			/* IOEXP1 */
	0,
	0,
	0,
	0,			/* VDMA */
	INTSRC_DUART2,		/* DUART2 */
	0,			/* ZBUF */
	0,			/* VIDEO */
	INTSRC_SCSI2,		/* SCSI1 */
	INTSRC_SCSI1,		/* SCSI0 */
	INTSRC_ETHERNET2,	/* LAN1 */
	INTSRC_ETHERNET1,	/* LAN0 */
	0,			/* SCC */
	0,			/* DMA0C */
	0,			/* DMA1C */
	0,			/* DMA2C */
	0,			/* DMA3C */
	0,			/* DMA4C */
	0,
	INTSRC_CLOCK,		/* PIT0OF */
	0,			/* PIT1OF */
	0,			/* PIT2OF */
	0,			/* PIT3OF */
	0			/* RTCOF */
};

/*
 * Device interrupt handler for AV530
 */

#define VME_VECTOR_MASK		0x1ff 	/* mask into VIACK register */
#define VME_BERR_MASK		0x100 	/* timeout during VME IACK cycle */

#define ISR_GET_CURRENT_MASK(cpu) \
	(*(volatile u_int *)AV_IST & av530_int_mask_reg[cpu])
#define EXISR_GET_CURRENT_MASK(cpu) \
	(*(volatile u_int *)AV_EXIST & av530_ext_int_mask_reg[cpu])

void
av530_intr(struct trapframe *eframe)
{
	int cpu = cpu_number();
	u_int32_t cur_mask, ign_mask;
	u_int32_t cur_exmask, ign_exmask;
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
	cur_exmask = EXISR_GET_CURRENT_MASK(cpu);
	ign_mask = 0;
	ign_exmask = 0;
	old_spl = eframe->tf_mask;

	if (cur_mask == 0 && cur_exmask == 0) {
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
	if (cur_mask & AV530_IPI_MASK) {
		av530_ipi_handler(eframe);
		cur_mask &= ~AV530_IPI_MASK;
		if (cur_mask == 0 && cur_exmask == 0)
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
	for (;;) {
		cur_mask = ISR_GET_CURRENT_MASK(cpu);
#ifdef MULTIPROCESSOR
		cur_mask &= ~AV530_IPI_MASK;
#endif
		cur_exmask = EXISR_GET_CURRENT_MASK(cpu);
		if ((cur_mask & ~ign_mask) == 0 &&
		    (cur_exmask & ~ign_exmask) == 0)
			break;

		level = av530_safe_level(cur_mask, cur_exmask, old_spl);
		av530_setipl(level);

		if (unmasked == 0) {
			set_psr(get_psr() & ~PSR_IND);
			unmasked = 1;
		}

#ifdef MULTIPROCESSOR
		/*
		 * Handle pending maskable IPIs first.
		 */
		if (cur_mask & AV530_CLOCK_IPI_MASK) {
			av530_clock_ipi_handler(eframe);
			cur_mask &= ~AV530_CLOCK_IPI_MASK;
			if ((cur_mask & ~ign_mask) == 0 &&
			    (cur_exmask & ~ign_exmask) == 0)
				break;
		}
#endif

		/* find the first bit set in the current mask */
		warn = 0;
		if (cur_mask != 0) {
			intbit = ff1(cur_mask);
			intsrc = av530_obio_vec[intbit];

			if (intsrc == 0)
				panic("%s: unexpected interrupt source"
				    " (bit %d), level %d, mask 0x%b",
				    __func__, intbit, level,
				    cur_mask, AV530_IST_STRING);
		} else {
			intbit = ff1(cur_exmask);
			intsrc = av530_obio_exvec[intbit];

			if (intsrc == 0)
				panic("%s: unexpected extended interrupt source"
				    " (bit %d), level %d, mask 0x%b",
				    __func__, intbit, level,
				    cur_exmask, AV530_EXIST_STRING);
		}

		if (IS_VME_INTSRC(intsrc)) {
			level = VME_INTSRC_LEVEL(intsrc);
			ivec = AV530_VIRQLV + (level << 2);
			vec = *(volatile u_int32_t *)ivec & VME_VECTOR_MASK;
			if (vec & VME_BERR_MASK) {
				/* no need to dump exmask for vme intr */
				printf("%s: timeout getting VME "
				    "interrupt vector, "
				    "level %d, mask 0x%b\n",
				    __func__, level,
				    cur_mask, AV530_IST_STRING);
				ign_mask |= 1 << intbit;
				continue;
			}
			list = &vmeintr_handlers[vec];
		} else {
			list = &sysconintr_handlers[intsrc];
		}

		if (SLIST_EMPTY(list)) {
			warn = 1;
		} else {
			/*
			 * Walk through all interrupt handlers in the chain
			 * for the given vector, calling each handler in turn,
			 * until some handler returns a value != 0.
			 */
			ret = 0;
			SLIST_FOREACH(intr, list, ih_link) {
				if (ISSET(intr->ih_flags, INTR_WANTFRAME))
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
			if (cur_mask != 0)
				ign_mask |= 1 << intbit;
			else
				ign_exmask |= 1 << intbit;

			if (IS_VME_INTSRC(intsrc))
				printf("%s: %s VME interrupt, "
				    "level %d, vec 0x%x, mask 0x%b\n",
				    __func__,
				    warn == 1 ? "spurious" : "unclaimed",
				    level, vec, cur_mask, AV530_IST_STRING);
			else {
				if (cur_mask != 0)
					printf("%s: %s interrupt, "
					    "level %d, bit %d, mask 0x%b\n",
					    __func__,
					    warn == 1 ?
					      "spurious" : "unclaimed",
					    level, intbit,
					    cur_mask, AV530_IST_STRING);
				else
					printf("%s: %s extended interrupt, "
					    "level %d, bit %d, mask 0x%b\n",
					    __func__,
					    warn == 1 ?
					      "spurious" : "unclaimed",
					    level, intbit,
					    cur_exmask, AV530_EXIST_STRING);
			}
		}
	}

#ifdef DIAGNOSTIC
	if (ign_mask != 0 || ign_exmask != 0) {
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

const struct vme_range *
av530_get_vme_ranges()
{
	return vme_av530;
}
