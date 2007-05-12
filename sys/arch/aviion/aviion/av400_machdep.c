/*	$OpenBSD: av400_machdep.c,v 1.5 2007/05/12 20:02:12 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat.
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
#include <sys/errno.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <machine/m88100.h>
#include <machine/m8820x.h>
#include <machine/avcommon.h>
#include <machine/av400.h>
#include <machine/prom.h>

#include <aviion/dev/sysconreg.h>

u_int	safe_level(u_int mask, u_int curlevel);

const pmap_table_entry
av400_ptable[] = {
	{ AV400_PROM,	AV400_PROM,	AV400_PROM_SIZE,
	  UVM_PROT_RW,	CACHE_INH },
#if 0	/* mapped by the hardcoded BATC entries */
	{ AV400_UTILITY,AV400_UTILITY,	AV400_UTILITY_SIZE,
	  UVM_PROT_RW,	CACHE_INHIBIT },
#endif
	{ 0, 0, (vsize_t)-1, 0, 0 }
};

const struct board board_av400 = {
	"100/200/300/400/3000/4000/4300 series",
	av400_bootstrap,
	av400_memsize,
	av400_startup,
	av400_intr,
	av400_init_clocks,
	av400_getipl,
	av400_setipl,
	av400_raiseipl,
	av400_ptable
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
unsigned int int_mask_reg[] = { 0, 0, 0, 0 };

u_int av400_curspl[] = { 0, 0, 0, 0 };

/*
 * external interrupt masks per spl.
 */
const unsigned int int_mask_val[INT_LEVEL] = {
	MASK_LVL_0,
	MASK_LVL_1,
	MASK_LVL_2,
	MASK_LVL_3,
	MASK_LVL_4,
	MASK_LVL_5,
	MASK_LVL_6,
	MASK_LVL_7
};

/*
 * Figure out how much memory is available, by asking the PROM.
 */
vaddr_t
av400_memsize()
{
	vaddr_t memsize;

	memsize = scm_memsize(1);

	/*
	 * What we got is the ``top of memory'', i.e. the largest addressable
	 * word address, ending in 0xffc. Round up to a multiple of a page.
	 */
	memsize = round_page(memsize);

	return (memsize);
}

void
av400_startup()
{
}

int32_t cpuid, sysid;

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
 * return next safe spl to reenable interrupts.
 */
u_int
safe_level(u_int mask, u_int curlevel)
{
	int i;

	for (i = curlevel; i < INT_LEVEL; i++)
		if ((int_mask_val[i] & mask) == 0)
			return i;

	return (INT_LEVEL - 1);
}

u_int
av400_getipl(void)
{
	return av400_curspl[cpu_number()];
}

u_int
av400_setipl(u_int level)
{
	u_int32_t mask, curspl;
	u_int cpu = cpu_number();

	curspl = av400_curspl[cpu];

	mask = int_mask_val[level];
#ifdef MULTIPROCESSOR
	if (cpu != master_cpu)
		mask &= ~SLAVE_MASK;
#endif

	*(u_int32_t *)AV_IEN(cpu) = int_mask_reg[cpu] = mask;
	av400_curspl[cpu] = level;

	return curspl;
}

u_int
av400_raiseipl(u_int level)
{
	u_int32_t mask, curspl;
	u_int cpu = cpu_number();

	curspl = av400_curspl[cpu];
	if (curspl < level) {
		mask = int_mask_val[level];
#ifdef MULTIPROCESSOR
		if (cpu != master_cpu)
			mask &= ~SLAVE_MASK;
#endif

		*(u_int32_t *)AV_IEN(cpu) = int_mask_reg[cpu] = mask;
		av400_curspl[cpu] = level;
	}
	return curspl;
}

/*
 * Device interrupt handler for AV400
 */

/*
 * Hard coded vector table for onboard devices and hardware failure
 * interrupts.
 */
const unsigned int obio_vec[32] = {
	0,		/* SWI0 */
	0,		/* SWI1 */
	0,
	0,
	0,		/* VME1 */
	SYSCV_SCSI,	/* SCI */
	0,		/* VME2 */
	0,
	0,
	0,		/* DVB */
	0,		/* VME3 */
	0,		/* DWP */
	0,		/* VME4 */
	0,		/* DTC */
	0,		/* VME5 */
	SYSCV_LE,	/* ECI */
	SYSCV_SCC2,	/* DI2 */
	SYSCV_SCC,	/* DI1 */
	0,		/* PPI */
	0,		/* VME6 */
	SYSCV_SYSF,	/* SF */
	SYSCV_TIMER2,	/* CIOI */
	0,		/* KBD */
	0,		/* VME7 */
	0,		/* PAR */
	0,		/* VID */
	0,		/* ZBUF */
	0,
	0,
	0,		/* ARBTO */	/* no vector, but always masked */
	SYSCV_ACF,	/* ACF */
	SYSCV_ABRT	/* ABORT */
};

#define VME_VECTOR_MASK		0x1ff 	/* mask into VIACK register */
#define VME_BERR_MASK		0x100 	/* timeout during VME IACK cycle */

void
av400_intr(u_int v, struct trapframe *eframe)
{
	int cpu = cpu_number();
	unsigned int cur_mask, ign_mask;
	unsigned int level, old_spl;
	struct intrhand *intr;
	intrhand_t *list;
	int ret, intbit;
	vaddr_t ivec;
	u_int vec;
	int unmasked = 0;
#ifdef DIAGNOSTIC
	static int problems = 0;
#endif

	cur_mask = ISR_GET_CURRENT_MASK(cpu);
	ign_mask = 0;
	old_spl = av400_curspl[cpu];
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
		setipl(level);

		/*
		 * Do not enable interrupts yet if we know, from cur_mask,
		 * that we have not cleared enough conditions yet.
		 * For now, only the timer interrupt requires its condition
		 * to be cleared before interrupts are enabled.
		 */
		if (unmasked == 0 /* && (cur_mask & whatever) == 0 */) {
			set_psr(get_psr() & ~PSR_IND);
			unmasked = 1;
		}

		/* find the first bit set in the current mask */
		intbit = ff1(cur_mask);
		if (OBIO_INTERRUPT_MASK & (1 << intbit)) {
			vec = obio_vec[intbit];
			if (vec == 0) {
				panic("unknown onboard interrupt: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
			vec += SYSCON_VECT;
		} else if (HW_FAILURE_MASK & (1 << intbit)) {
			vec = obio_vec[intbit];
			if (vec == 0) {
				panic("unknown hardware failure: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
			vec += SYSCON_VECT;
		} else if (VME_INTERRUPT_MASK & (1 << intbit)) {
			ivec = AV400_VIRQLV + (level << 2);
			vec = *(volatile u_int32_t *)ivec & VME_VECTOR_MASK;
			if (vec & VME_BERR_MASK) {
				printf("%s: timeout getting VME "
				    "interrupt vector, "
				    "level %d, mask 0x%b\n",
				    __func__, level,
				    cur_mask, IST_STRING);
				ign_mask |= 1 << intbit;
				continue;
			}
			if (vec == 0) {
				panic("%s: invalid VME interrupt vector, "
				    "level %d, mask 0x%b",
				    __func__, level, cur_mask, IST_STRING);
			}
		} else {
			panic("%s: unexpected interrupt source, "
			    "level %d, mask 0x%b",
			    __func__, level, cur_mask, IST_STRING);
		}

		list = &intr_handlers[vec];
		if (SLIST_EMPTY(list)) {
			printf("%s: spurious interrupt, "
			    "level %d, vec 0x%x, mask 0x%b\n",
			    __func__, level, vec, cur_mask, IST_STRING);
			ign_mask |= 1 << intbit;
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
			if (ret == 0) {
				panic("%s: unclaimed interrupt, "
				    "level %d, vec %x, mask 0x%b"
				    __func__, level, vec, cur_mask, IST_STRING);
				ign_mask |= 1 << intbit;
				break;
			}
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

/*
 * Clock routines
 */

void	av400_cio_init(unsigned);
u_int	read_cio(int);
void	write_cio(int, u_int);

struct intrhand	clock_ih;

int	av400_clockintr(void *);

struct simplelock av400_cio_lock;

#define	CIO_LOCK	simple_lock(&av400_cio_lock)
#define	CIO_UNLOCK	simple_unlock(&av400_cio_lock)

/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 4096 would
 * give us offsets in [0..4095].  Instead, we take offsets in [1..4095].
 * This is symmetric about the point 2048, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
int statvar = 8192;
int statmin;			/* statclock interval - 1/2*variance */

/*
 * Notes on the AV400 clock usage:
 *
 * Unlike the MVME188 design, we only have access to three counter/timers
 * in the Zilog Z8536 (since we can not receive the DUART timer interrupts).
 *
 * Clock is run on a Z8536 counter, kept in counter mode and retriggered
 * every interrupt (when using the Z8536 in timer mode, it _seems_ that it
 * resets at 0xffff instead of the initial count value...)
 *
 * It should be possible to run statclock on the Z8536 counter #2, but
 * this would make interrupt handling more tricky, in the case both
 * counters interrupt at the same time...
 */

void
av400_init_clocks(void)
{

	simple_lock_init(&av400_cio_lock);

#ifdef DIAGNOSTIC
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#endif
	tick = 1000000 / hz;

	av400_cio_init(tick);

	stathz = 0;

	clock_ih.ih_fn = av400_clockintr;
	clock_ih.ih_arg = 0;
	clock_ih.ih_wantframe = 1;
	clock_ih.ih_ipl = IPL_CLOCK;
	sysconintr_establish(SYSCV_TIMER2, &clock_ih, "clock");
}

int
av400_clockintr(void *eframe)
{
	CIO_LOCK;
	write_cio(CIO_CSR1, CIO_GCB | CIO_CIP);  /* Ack the interrupt */

	hardclock(eframe);

	/* restart counter */
	write_cio(CIO_CSR1, CIO_GCB | CIO_TCB | CIO_IE);
	CIO_UNLOCK;

	return (1);
}

/* Write CIO register */
void
write_cio(int reg, u_int val)
{
	int s;
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	s = splclock();
	CIO_LOCK;

	i = *cio_ctrl;				/* goto state 1 */
	*cio_ctrl = 0;				/* take CIO out of RESET */
	i = *cio_ctrl;				/* reset CIO state machine */

	*cio_ctrl = (reg & 0xff);		/* select register */
	*cio_ctrl = (val & 0xff);		/* write the value */

	CIO_UNLOCK;
	splx(s);
}

/* Read CIO register */
u_int
read_cio(int reg)
{
	int c, s;
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	s = splclock();
	CIO_LOCK;

	/* select register */
	*cio_ctrl = (reg & 0xff);
	/* delay for a short time to allow 8536 to settle */
	for (i = 0; i < 100; i++)
		;
	/* read the value */
	c = *cio_ctrl;
	CIO_UNLOCK;
	splx(s);
	return (c & 0xff);
}

/*
 * Initialize the CTC (8536)
 * Only the counter/timers are used - the IO ports are un-comitted.
 */
void
av400_cio_init(unsigned period)
{
	volatile int i;

	CIO_LOCK;

	/* Start by forcing chip into known state */
	read_cio(CIO_MICR);
	write_cio(CIO_MICR, CIO_MICR_RESET);	/* Reset the CTC */
	for (i = 0; i < 1000; i++)	 	/* Loop to delay */
		;

	/* Clear reset and start init seq. */
	write_cio(CIO_MICR, 0x00);

	/* Wait for chip to come ready */
	while ((read_cio(CIO_MICR) & CIO_MICR_RJA) == 0)
		;

	/* Initialize the 8536 for real */
	write_cio(CIO_MICR,
	    CIO_MICR_MIE /* | CIO_MICR_NV */ | CIO_MICR_RJA | CIO_MICR_DLC);
	write_cio(CIO_CTMS1, CIO_CTMS_CSC);	/* Continuous count */
	write_cio(CIO_PDCB, 0xff);		/* set port B to input */

	period <<= 1;	/* CT#1 runs at PCLK/2, hence 2MHz */
	write_cio(CIO_CT1MSB, period >> 8);
	write_cio(CIO_CT1LSB, period);
	/* enable counter #1 */
	write_cio(CIO_MCCR, CIO_MCCR_CT1E | CIO_MCCR_PBE);
	write_cio(CIO_CSR1, CIO_GCB | CIO_TCB | CIO_IE);

	CIO_UNLOCK;
}
