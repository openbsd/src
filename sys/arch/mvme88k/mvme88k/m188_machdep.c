/*	$OpenBSD: m188_machdep.c,v 1.1 2004/10/01 19:00:52 miod Exp $	*/
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
#include <machine/cpu_number.h>
#include <machine/locore.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <machine/m88100.h>
#include <machine/mvme188.h>

#include <uvm/uvm_extern.h>

#include <mvme88k/dev/sysconreg.h>

void	m188_reset(void);
u_int	safe_level(u_int mask, u_int curlevel);
void	setlevel(unsigned int);

void	m188_bootstrap(void);
void	m188_ext_int(u_int, struct trapframe *);
vaddr_t	m188_memsize(void);
void	m188_setupiackvectors(void);
void	m188_startup(void);

/*
 * *int_mask_reg[CPU]
 * Points to the hardware interrupt status register for each CPU.
 */
unsigned int *volatile int_mask_reg[MAX_CPUS] = {
	(unsigned int *)IEN0_REG,
	(unsigned int *)IEN1_REG,
	(unsigned int *)IEN2_REG,
	(unsigned int *)IEN3_REG
};

unsigned int m188_curspl[MAX_CPUS] = {0, 0, 0, 0};

unsigned int int_mask_val[INT_LEVEL] = {
	MASK_LVL_0,
	MASK_LVL_1,
	MASK_LVL_2,
	MASK_LVL_3,
	MASK_LVL_4,
	MASK_LVL_5,
	MASK_LVL_6,
	MASK_LVL_7
};

vaddr_t utilva;

/*
 * Figure out how much memory is available, by querying the MBus registers.
 *
 * For every 4MB segment, ask the MBus address decoder which device claimed
 * the range. Since memory is packed at low addresses, we will hit all memory
 * boards in order until reaching either a VME space or a non-claimed space.
 *
 * As a safety measure, we never check for more than 256MB - the 188 can
 * only have up to 4 memory boards, which theoretically can not be larger
 * than 64MB, and I am not aware of third-party larger memory boards.
 */
vaddr_t
m188_memsize()
{
	unsigned int pgnum;
	int32_t rmad;

#define	MVME188_MAX_MEMORY	((4 * 64) / 4)	/* 4 64MB boards */
	for (pgnum = 0; pgnum <	MVME188_MAX_MEMORY; pgnum++) {
		*(volatile int32_t *)RMAD_REG = (pgnum << 22);
		rmad = *(volatile int32_t *)RMAD_REG;

		if (rmad & 0x04)	/* not a memory board */
			break;
	}

	return (pgnum << 22);
}

void
m188_startup()
{
	/*
	 * Grab the UTIL space that we hardwired in pmap_bootstrap().
	 */
	utilva = MVME188_UTILITY;
	uvm_map(kernel_map, (vaddr_t *)&utilva, MVME188_UTILITY_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (utilva != MVME188_UTILITY)
		panic("utilva %lx: UTILITY area not free", utilva);
}

void
m188_bootstrap()
{
	extern struct cmmu_p cmmu8820x;

	cmmu = &cmmu8820x;
	md.interrupt_func = &m188_ext_int;
	md.intr_mask = NULL;
	md.intr_ipl = NULL;
	md.intr_src = NULL;

	/* clear and disable all interrupts */
	*int_mask_reg[0] = 0;
	*int_mask_reg[1] = 0;
	*int_mask_reg[2] = 0;
	*int_mask_reg[3] = 0;
}

void
m188_reset()
{
	volatile int cnt;

	*(volatile u_int32_t *)IEN0_REG = 0;
	*(volatile u_int32_t *)IEN1_REG = 0;
	*(volatile u_int32_t *)IEN2_REG = 0;
	*(volatile u_int32_t *)IEN3_REG = 0;

	if ((*(volatile u_int8_t *)GLB1) & M188_SYSCON) {
		/* Force a complete VMEbus reset */
		*(volatile u_int32_t *)GLBRES_REG = 1;
	} else {
		/* Force only a local reset */
		*(volatile u_int8_t *)GLB1 |= M188_LRST;
	}

	*(volatile u_int32_t *)UCSR_REG |= 0x2000;	/* clear SYSFAIL */
	for (cnt = 0; cnt < 5*1024*1024; cnt++)
		;
	*(volatile u_int32_t *)UCSR_REG |= 0x2000;	/* clear SYSFAIL */

	printf("reset failed\n");
}

/*
 * fill up ivec array with interrupt response vector addresses.
 */
void
m188_setupiackvectors()
{
	u_int8_t *vaddr = (u_int8_t *)M188_IACK;

	ivec[0] = vaddr;	/* We dont use level 0 */
	ivec[1] = vaddr + 0x04;
	ivec[2] = vaddr + 0x08;
	ivec[3] = vaddr + 0x0c;
	ivec[4] = vaddr + 0x10;
	ivec[5] = vaddr + 0x14;
	ivec[6] = vaddr + 0x18;
	ivec[7] = vaddr + 0x1c;
	ivec[8] = vaddr + 0x20;	/* for self inflicted interrupts */
	*ivec[8] = M188_IVEC;	/* supply a vector base for m188ih */
}

/*
 * return next safe spl to reenable interrupts.
 */
u_int
safe_level(u_int mask, u_int curlevel)
{
	int i;

	for (i = curlevel; i < 8; i++)
		if (!(int_mask_val[i] & mask))
			return i;

	panic("safe_level: no safe level for mask 0x%08x level %d found",
	       mask, curlevel);
	/* NOTREACHED */
}

void
setlevel(unsigned int level)
{
	unsigned int mask;
	int cpu = cpu_number();

	mask = int_mask_val[level];

	if (cpu != master_cpu)
		mask &= SLAVE_MASK;

	*int_mask_reg[cpu] = mask;
	m188_curspl[cpu] = level;
}

/*
 * Device interrupt handler for MVME188
 */

/* Hard coded vector table for onboard devices. */
const unsigned int obio_vec[32] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
        0, SYSCV_SCC, 0, 0, SYSCV_SYSF, SYSCV_TIMER2, 0, 0,
	0, 0, 0, 0, SYSCV_TIMER1, 0, SYSCV_ACF, SYSCV_ABRT
};

#define GET_MASK(cpu, val)	*int_mask_reg[cpu] & (val)
#define VME_VECTOR_MASK		0x1ff 		/* mask into VIACK register */
#define VME_BERR_MASK		0x100 		/* timeout during VME IACK cycle */

void
m188_ext_int(u_int v, struct trapframe *eframe)
{
	int cpu = cpu_number();
	unsigned int cur_mask;
	unsigned int level, old_spl;
	struct intrhand *intr;
	intrhand_t *list;
	int ret, intbit;
	unsigned vec;

	cur_mask = ISR_GET_CURRENT_MASK(cpu);
	old_spl = m188_curspl[cpu];
	eframe->tf_mask = old_spl;

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
		level = safe_level(cur_mask, old_spl);

		if (old_spl >= level) {
			int i;

			printf("safe level %d <= old level %d\n", level, old_spl);
			printf("cur_mask = 0x%b\n", cur_mask, IST_STRING);
			for (i = 0; i < 4; i++)
				printf("IEN%d = 0x%b  ", i, *int_mask_reg[i], IST_STRING);
			printf("\nCPU0 spl %d  CPU1 spl %d  CPU2 spl %d  CPU3 spl %d\n",
			       m188_curspl[0], m188_curspl[1],
			       m188_curspl[2], m188_curspl[3]);
			for (i = 0; i < 8; i++)
				printf("int_mask[%d] = 0x%08x\n", i, int_mask_val[i]);
			printf("--CPU %d halted--\n", cpu_number());
			spl7();
			for(;;) ;
		}

#ifdef DEBUG
		if (level > 7 || (int)level < 0) {
			panic("int level (%x) is not between 0 and 7", level);
		}
#endif

		setipl(level);

		/*
		 * Do not enable interrupts yet if we know, from cur_mask,
		 * that we have not cleared enough conditions yet.
		 * For now, only the timer interrupt requires its condition
		 * to be cleared before interrupts are enabled.
		 */
		if ((cur_mask & DTI_BIT) == 0) {
			enable_interrupt();
		}

		/* generate IACK and get the vector */

		/*
		 * This is tricky.  If you don't catch all the
		 * interrupts, you die. Game over. Insert coin...
		 * XXX smurph
		 */

		/* find the first bit set in the current mask */
		intbit = ff1(cur_mask);
		if (OBIO_INTERRUPT_MASK & (1 << intbit)) {
			vec = SYSCON_VECT + obio_vec[intbit];
			if (vec == 0) {
				panic("unknown onboard interrupt: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
		} else if (HW_FAILURE_MASK & (1 << intbit)) {
			vec = SYSCON_VECT + obio_vec[intbit];
			if (vec == 0) {
				panic("unknown hardware failure: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
		} else if (VME_INTERRUPT_MASK & (1 << intbit)) {
			if (guarded_access(ivec[level], 4, (u_char *)&vec) ==
			    EFAULT) {
				panic("unable to get vector for this vmebus "
				    "interrupt (level %x)", level);
			}
			vec &= VME_VECTOR_MASK;
			if (vec & VME_BERR_MASK) {
				printf("VME vec timeout, vec = %x, mask = 0x%b\n",
				    vec, 1 << intbit, IST_STRING);
				break;
			}
			if (vec == 0) {
				panic("unknown vme interrupt: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
		} else {
			panic("unknown interrupt: level = %d intbit = 0x%x "
			    "mask = 0x%b",
			    level, intbit, 1 << intbit, IST_STRING);
		}

		list = &intr_handlers[vec];
		if (SLIST_EMPTY(list)) {
			/* increment intr counter */
			intrcnt[M88K_SPUR_IRQ]++;
			printf("Spurious interrupt: level = %d vec = 0x%x, "
			    "intbit = %d mask = 0x%b\n",
			    level, vec, intbit, 1 << intbit, IST_STRING);
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
					intrcnt[level]++;
					intr->ih_count.ec_count++;
					break;
				}
			}
			if (ret == 0) {
				printf("Unclaimed interrupt: level = %d "
				    "vec = 0x%x, intbit = %d mask = 0x%b\n",
				    level, vec, intbit,
				    1 << intbit, IST_STRING);
				break;
			}
		}
	} while ((cur_mask = ISR_GET_CURRENT_MASK(cpu)) != 0);

	/*
	 * process any remaining data access exceptions before
	 * returning to assembler
	 */
	disable_interrupt();
out:
	if (eframe->tf_dmt0 & DMT_VALID)
		m88100_trap(T_DATAFLT, eframe);

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl(eframe->tf_mask);
}
