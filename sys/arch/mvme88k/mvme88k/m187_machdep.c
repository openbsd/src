/*	$OpenBSD: m187_machdep.c,v 1.2 2004/11/08 16:39:31 miod Exp $	*/
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

#include <machine/asm_macro.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/locore.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/m88100.h>
#include <machine/mvme187.h>

#include <uvm/uvm_extern.h>

#include <mvme88k/dev/memcreg.h>

void	m187_bootstrap(void);
void	m187_ext_int(u_int, struct trapframe *);
vaddr_t	m187_memsize(void);
void	m187_setupiackvectors(void);
void	m187_startup(void);

vaddr_t obiova;
vaddr_t bugromva;
vaddr_t sramva;

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

void
m187_startup()
{
	/*
	 * Grab the SRAM space that we hardwired in pmap_bootstrap
	 */
	sramva = SRAM_START;
	uvm_map(kernel_map, (vaddr_t *)&sramva, SRAM_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (sramva != SRAM_START)
		panic("sramva %lx: SRAM not free", sramva);

	/*
	 * Grab the BUGROM space that we hardwired in pmap_bootstrap
	 */
	bugromva = BUG187_START;
	uvm_map(kernel_map, (vaddr_t *)&bugromva, BUG187_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (bugromva != BUG187_START)
		panic("bugromva %lx: BUGROM not free", bugromva);

	/*
	 * Grab the OBIO space that we hardwired in pmap_bootstrap
	 */
	obiova = OBIO_START;
	uvm_map(kernel_map, (vaddr_t *)&obiova, OBIO_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (obiova != OBIO_START)
		panic("obiova %lx: OBIO not free", obiova);
}

void
m187_setupiackvectors()
{
	u_int8_t *vaddr = (u_int8_t *)M187_IACK;

	ivec[0] = vaddr + 0x03;	/* We dont use level 0 */
	ivec[1] = vaddr + 0x07;
	ivec[2] = vaddr + 0x0b;
	ivec[3] = vaddr + 0x0f;
	ivec[4] = vaddr + 0x13;
	ivec[5] = vaddr + 0x17;
	ivec[6] = vaddr + 0x1b;
	ivec[7] = vaddr + 0x1f;
}

/*
 * Device interrupt handler for MVME187
 */

void
m187_ext_int(u_int v, struct trapframe *eframe)
{
	int mask, level;
	struct intrhand *intr;
	intrhand_t *list;
	int ret;
	u_char vec;

	/* get level and mask */
	mask = *md_intr_mask & 0x07;
	level = *(u_int8_t *)M187_ILEVEL & 0x07;

#ifdef DIAGNOSTIC
	/*
	 * It is really bizarre for the mask and level to the be the same.
	 * pcc2 for 187 blocks all interrupts at and below the mask value,
	 * so we should not be getting an interrupt at the level that is
	 * already blocked. I can't explain this case XXX nivas
	 */

	if ((mask == level) && level) {
		panic("mask == level, %d", level);
	}

	/*
	 * Interrupting level cannot be 0--0 doesn't produce an interrupt.
	 * Weird! XXX nivas
	 */

	if (level == 0) {
		panic("Bogons... level %x and mask %x", level, mask);
	}
#endif

	uvmexp.intrs++;

	/* generate IACK and get the vector */
	flush_pipeline();
	if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
		panic("Unable to get vector for this interrupt (level %x)", level);
	}
	flush_pipeline();
	flush_pipeline();
	flush_pipeline();

	/* block interrupts at level or lower */
	setipl(level);

	enable_interrupt();

	list = &intr_handlers[vec];
	if (SLIST_EMPTY(list)) {
		/* increment intr counter */
		intrcnt[M88K_SPUR_IRQ]++;
		printf("Spurious interrupt (level %x and vec %x)\n",
		       level, vec);
	} else {
#ifdef DIAGNOSTIC
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
				intrcnt[level]++;
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
	disable_interrupt();
	if (eframe->tf_dmt0 & DMT_VALID)
		m88100_trap(T_DATAFLT, eframe);

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl(mask);
}

void
m187_bootstrap()
{
	extern struct cmmu_p cmmu8820x;

	cmmu = &cmmu8820x;
	md_interrupt_func_ptr = &m187_ext_int;
	md_intr_mask = (u_int8_t *)M187_IMASK;
}
