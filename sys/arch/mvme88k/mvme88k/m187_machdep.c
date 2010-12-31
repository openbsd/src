/*	$OpenBSD: m187_machdep.c,v 1.22 2010/12/31 21:38:08 miod Exp $	*/
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
#include <machine/m88100.h>
#include <machine/mvme187.h>

#include <mvme88k/dev/memcreg.h>
#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/mvme88k/clockvar.h>

void	m187_bootstrap(void);
void	m187_ext_int(struct trapframe *);
u_int	m187_getipl(void);
vaddr_t	m187_memsize(void);
u_int	m187_raiseipl(u_int);
u_int	m187_setipl(u_int);

/*
 * Figure out how much memory is available, by querying the memory controllers.
 */
vaddr_t
m187_memsize()
{
	struct memcreg *memc;
	vaddr_t x;

	memc = (struct memcreg *)MEM_CTLR;
	x = MEMC_MEMCONF_RTOB(memc->memc_memconf);

	memc = (struct memcreg *)(MEM_CTLR + 0x100);
	if (!badaddr((vaddr_t)&memc->memc_memconf, 1))
		x += MEMC_MEMCONF_RTOB(memc->memc_memconf);

	return x;
}

/*
 * Device interrupt handler for MVME187
 */

void
m187_ext_int(struct trapframe *eframe)
{
	int level;
	struct intrhand *intr;
	intrhand_t *list;
	int ret;
	vaddr_t ivec;
	u_int8_t vec;

	level = *(u_int8_t *)M187_ILEVEL & 0x07;

	/* generate IACK and get the vector */
	ivec = M187_IACK + (level << 2) + 0x03;
	vec = *(volatile u_int8_t *)ivec;

#ifdef MULTIPROCESSOR
	if (eframe->tf_mask < IPL_SCHED)
		__mp_lock(&kernel_lock);
#endif

	uvmexp.intrs++;

	/* block interrupts at level or lower */
	m187_setipl(level);
	flush_pipeline();
	set_psr(get_psr() & ~PSR_IND);

	list = &intr_handlers[vec];
	if (SLIST_EMPTY(list)) {
		/* increment intr counter */
		printf("Spurious interrupt (level %x and vec %x)\n",
		       level, vec);
	} else {
#ifdef DEBUG
		intr = SLIST_FIRST(list);
		if (intr->ih_ipl != level) {
			panic("Handler ipl %x not the same as level %x. "
			    "vec = 0x%x",
			    intr->ih_ipl, level, vec);
		}
#endif

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
	}

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

#ifdef MULTIPROCESSOR
	if (eframe->tf_mask < IPL_SCHED)
		__mp_unlock(&kernel_lock);
#endif
}

u_int
m187_getipl(void)
{
	return *(u_int8_t *)M187_IMASK & 0x07;
}

u_int
m187_setipl(u_int level)
{
	u_int curspl, psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = *(u_int8_t *)M187_IMASK & 0x07;
	*(u_int8_t *)M187_IMASK = level;
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);
	return curspl;
}

u_int
m187_raiseipl(u_int level)
{
	u_int curspl, psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	curspl = *(u_int8_t *)M187_IMASK & 0x07;
	if (curspl < level)
		*(u_int8_t *)M187_IMASK = level;
	/*
	 * We do not flush the pipeline here, because interrupts are disabled,
	 * and set_psr() will synchronize the pipeline.
	 */
	set_psr(psr);
	return curspl;
}

void
m187_bootstrap()
{
	extern int cpuspeed;
	extern struct cmmu_p cmmu8820x;

	/*
	 * Find out the processor speed, from the PCC2 prescaler
	 * adjust register.
	 */
	cpuspeed = 256 - *(volatile u_int8_t *)(PCC2_BASE + PCCTWO_PSCALEADJ);

	cmmu = &cmmu8820x;
	md_interrupt_func_ptr = m187_ext_int;
	md_getipl = m187_getipl;
	md_setipl = m187_setipl;
	md_raiseipl = m187_raiseipl;
	md_init_clocks = m1x7_init_clocks;
	md_delay = m1x7_delay;
#ifdef MULTIPROCESSOR
	md_smp_setup = m88100_smp_setup;
#endif
}
