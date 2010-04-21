/*	$OpenBSD: av400_machdep.c,v 1.16 2010/04/21 19:33:45 miod Exp $	*/
/*
 * Copyright (c) 2006, 2007, Miodrag Vallat.
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
#include <sys/device.h>
#include <sys/errno.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/board.h>
#include <machine/bus.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <machine/m88100.h>
#include <machine/m8820x.h>
#include <machine/avcommon.h>
#include <machine/av400.h>
#include <machine/prom.h>

#include <aviion/dev/sysconvar.h>
#include <aviion/dev/vmevar.h>

u_int	av400_safe_level(u_int, u_int);

const pmap_table_entry
av400_ptable[] = {
	{ AV400_PROM,	AV400_PROM,	AV400_PROM_SIZE,
	  UVM_PROT_RW,	CACHE_INH },
#if 0	/* mapped by the hardcoded BATC entries */
	{ AV400_UTILITY,AV400_UTILITY,	AV400_UTILITY_SIZE,
	  UVM_PROT_RW,	CACHE_INH },
#endif
	{ 0, 0, (vsize_t)-1, 0, 0 }
};

const struct vme_range vme_av400[] = {
	{ VME_A16,
	  AV400_VME16_START,	AV400_VME16_END,	AV400_VME16_BASE },
	{ VME_A24,
	  AV400_VME24_START,	AV400_VME24_END,	AV400_VME24_BASE },
	{ VME_A32,
	  AV400_VME32_START1,	AV400_VME32_END1,	AV400_VME32_BASE },
	{ VME_A32,
	  AV400_VME32_START2,	AV400_VME32_END2,	AV400_VME32_BASE },
	{ 0 }
};

const struct board board_av400 = {
	"100/200/300/400/3000/4000/4300 series",
	av400_bootstrap,
	av400_memsize,
	av400_startup,
	av400_intr,
	cio_init_clocks,
	av400_getipl,
	av400_setipl,
	av400_raiseipl,
	av400_intsrc,

	av400_ptable,
	vme_av400
};

/*
 * The MVME188 interrupt arbiter has 25 orthogonal interrupt sources.
 * On the AViiON machines, there are even more interrupt sources in use,
 * but differences are minimal.
 * We fold this model in the 8-level spl model this port uses, enforcing
 * priorities manually with the interrupt masks.
 */

/*
 * Copy of the interrupt enable register for each CPU.
 * Note that, on the AV400 design, the interrupt enable registers are
 * write-only and read back as 0xffffffff.
 */
u_int32_t av400_int_mask_reg[] = { 0, 0, 0, 0 };

u_int av400_curspl[] = { IPL_HIGH, IPL_HIGH, IPL_HIGH, IPL_HIGH };

#ifdef MULTIPROCESSOR
/*
 * Interrupts allowed on secondary processors.
 */
#define	SLAVE_MASK	0	/* AV400_IRQ_SWI0 | AV400_IRQ_SWI1 */
#endif

/*
 * Figure out how much memory is available, by asking the PROM.
 */
vaddr_t
av400_memsize()
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
av400_startup()
{
}

void
av400_bootstrap()
{
	extern struct cmmu_p cmmu8820x;
	extern u_char hostaddr[6];

	/*
	 * These are the fixed assignments on AV400 designs.
	 */
	cmmu = &cmmu8820x;
	m8820x_cmmu[0].cmmu_regs = (void *)AV400_CMMU_I0;
	m8820x_cmmu[1].cmmu_regs = (void *)AV400_CMMU_D0;
	m8820x_cmmu[2].cmmu_regs = (void *)AV400_CMMU_I1;
	m8820x_cmmu[3].cmmu_regs = (void *)AV400_CMMU_D1;
	m8820x_cmmu[4].cmmu_regs = (void *)AV400_CMMU_I2;
	m8820x_cmmu[5].cmmu_regs = (void *)AV400_CMMU_D2;
	m8820x_cmmu[6].cmmu_regs = (void *)AV400_CMMU_I3;
	m8820x_cmmu[7].cmmu_regs = (void *)AV400_CMMU_D3;

	/* clear and disable all interrupts */
	*(volatile u_int32_t *)AV_IENALL = 0;

	/*
	 * Get all the information we'll need later from the PROM, while
	 * we can still use it.
	 */
	scm_getenaddr(hostaddr);
	cpuid = scm_cpuid();
	sysid = scm_sysid();
}

/*
 * Return the next ipl >= ``curlevel'' at which we can reenable interrupts
 * while keeping ``mask'' masked.
 */
u_int
av400_safe_level(u_int mask, u_int curlevel)
{
	int i;

	for (i = curlevel; i < NIPLS; i++)
		if ((int_mask_val[i] & mask) == 0)
			return i;

	return (NIPLS - 1);
}

u_int
av400_getipl(void)
{
	return av400_curspl[cpu_number()];
}

u_int
av400_setipl(u_int level)
{
	u_int32_t mask, curspl, psr;
	u_int cpu = cpu_number();

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = av400_curspl[cpu];

	mask = int_mask_val[level];
#ifdef MULTIPROCESSOR
	if (cpu != master_cpu)
		mask &= SLAVE_MASK;
#endif

	av400_curspl[cpu] = level;
	*(u_int32_t *)AV_IEN(cpu) = av400_int_mask_reg[cpu] = mask;
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);

	return curspl;
}

u_int
av400_raiseipl(u_int level)
{
	u_int32_t mask, curspl, psr;
	u_int cpu = cpu_number();

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = av400_curspl[cpu];
	if (curspl < level) {
		mask = int_mask_val[level];
#ifdef MULTIPROCESSOR
		if (cpu != master_cpu)
			mask &= SLAVE_MASK;
#endif

		av400_curspl[cpu] = level;
		*(u_int32_t *)AV_IEN(cpu) = av400_int_mask_reg[cpu] = mask;
	}
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);

	return curspl;
}

/*
 * Provide the interrupt masks for a given logical interrupt source.
 */
u_int64_t
av400_intsrc(int i)
{
	static const u_int32_t intsrc[] = {
		0,
		AV400_IRQ_ABORT,
		AV400_IRQ_ACF,
		AV400_IRQ_SF,
		AV400_IRQ_CIOI,
		AV400_IRQ_DI1,
		AV400_IRQ_DI2,
		AV400_IRQ_ECI,
		0,
		AV400_IRQ_SCI,
		0,
		AV400_IRQ_VME1,
		AV400_IRQ_VME2,
		AV400_IRQ_VME3,
		AV400_IRQ_VME4,
		AV400_IRQ_VME5,
		AV400_IRQ_VME6,
		AV400_IRQ_VME7
	};

	return ((u_int64_t)intsrc[i]);
}

/*
 * Provide the interrupt source for a given interrupt status bit.
 */
static const u_int av400_obio_vec[32] = {
	0,			/* SWI0 */
	0,			/* SWI1 */
	0,
	0,
	INTSRC_VME(1),		/* VME1 */
	INTSRC_SCSI1,		/* SCI */
	INTSRC_VME(2),		/* VME2 */
	0,
	0,
	0,			/* DVB */
	INTSRC_VME(3),		/* VME3 */
	0,			/* DWP */
	INTSRC_VME(4),		/* VME4 */
	0,			/* DTC */
	INTSRC_VME(5),		/* VME5 */
	INTSRC_ETHERNET1,	/* ECI */
	INTSRC_DUART2,		/* DI2 */
	INTSRC_DUART1,		/* DI1 */
	0,			/* PPI */
	INTSRC_VME(6),		/* VME6 */
	INTSRC_SYSFAIL,		/* SF */
	INTSRC_CLOCK,		/* CIOI */
	0,			/* KBD */
	INTSRC_VME(7),		/* VME7 */
	0,			/* PAR */
	0,			/* VID */
	0,			/* ZBUF */
	0,
	0,
	0,			/* ARBTO */
	INTSRC_ACFAIL,		/* ACF */
	INTSRC_ABORT		/* ABORT */
};

/*
 * Device interrupt handler for AV400
 */

#define VME_VECTOR_MASK		0x1ff 	/* mask into VIACK register */
#define VME_BERR_MASK		0x100 	/* timeout during VME IACK cycle */

#define ISR_GET_CURRENT_MASK(cpu) \
	(*(volatile u_int *)AV_IST & av400_int_mask_reg[cpu])

void
av400_intr(struct trapframe *eframe)
{
	int cpu = cpu_number();
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
		flush_pipeline();
		goto out;
	}

	uvmexp.intrs++;

	/*
	 * We want to service all interrupts marked in the IST register
	 * They are all valid because the mask would have prevented them
	 * from being generated otherwise.  We will service them in order of
	 * priority.
	 */
	do {
		level = av400_safe_level(cur_mask, old_spl);
		av400_setipl(level);

		if (unmasked == 0) {
			set_psr(get_psr() & ~PSR_IND);
			unmasked = 1;
		}

		/* find the first bit set in the current mask */
		warn = 0;
		intbit = ff1(cur_mask);
		intsrc = av400_obio_vec[intbit];

		if (intsrc == 0)
			panic("%s: unexpected interrupt source (bit %d), "
			    "level %d, mask 0x%b",
			    __func__, intbit, level,
			    cur_mask, AV400_IST_STRING);

		if (IS_VME_INTSRC(intsrc)) {
			level = VME_INTSRC_LEVEL(intsrc);
			ivec = AV400_VIRQLV + (level << 2);
			vec = *(volatile u_int32_t *)ivec & VME_VECTOR_MASK;
			if (vec & VME_BERR_MASK) {
				printf("%s: timeout getting VME "
				    "interrupt vector, "
				    "level %d, mask 0x%b\n",
				    __func__, level,
				    cur_mask, AV400_IST_STRING);
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
			ign_mask |= 1 << intbit;

			if (IS_VME_INTSRC(intsrc))
				printf("%s: %s VME interrupt, "
				    "level %d, vec 0x%x, mask 0x%b\n",
				    __func__,
				    warn == 1 ? "spurious" : "unclaimed",
				    level, vec,
				    cur_mask, AV400_IST_STRING);
			else
				printf("%s: %s interrupt, "
				    "level %d, bit %d, mask 0x%b\n",
				    __func__,
				    warn == 1 ? "spurious" : "unclaimed",
				    level, intbit, cur_mask, AV400_IST_STRING);
		}
	} while (((cur_mask = ISR_GET_CURRENT_MASK(cpu)) & ~ign_mask) != 0);

#ifdef DIAGNOSTIC
	if (ign_mask != 0) {
		if (++problems >= 10)
			panic("%s: broken interrupt behaviour", __func__);
	} else
		problems = 0;
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
