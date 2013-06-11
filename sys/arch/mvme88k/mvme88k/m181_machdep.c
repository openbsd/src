/*	$OpenBSD: m181_machdep.c,v 1.3 2013/06/11 21:06:31 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
 * MVME180 ``AngelFire'' and MVME181 ``AngelFire-2'' support routines.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/timetc.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/board.h>
#include <machine/mmu.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/pmap_table.h>
#include <machine/trap.h>

#include <machine/m88100.h>
#include <machine/m8820x.h>
#include <machine/mvme181.h>

#include <mvme88k/mvme88k/clockvar.h>
#include <dev/ic/mc68681reg.h>

const struct pmap_table m181_pmap_table[] = {
	{ M181_SSR,		PAGE_SIZE,	UVM_PROT_RW, CACHE_INH },
	{ M181_SCR,		PAGE_SIZE,	UVM_PROT_RW, CACHE_INH },
	{ M181_CPEI,		PAGE_SIZE,	UVM_PROT_RW, CACHE_INH },
	{ M181_DUART,		PAGE_SIZE,	UVM_PROT_RW, CACHE_INH },
	{ M181_VMEVEC,		PAGE_SIZE,	UVM_PROT_RW, CACHE_INH },
	{ M181_CLRABRT,		PAGE_SIZE,	UVM_PROT_RW, CACHE_INH },
	{ 0, 0xffffffff, 0, 0 },
};

const struct board board_mvme181 = {
	.bootstrap = m181_bootstrap,
	.memsize = m181_memsize,
	.cpuspeed = m181_cpuspeed,
	.reboot = m181_reboot,
	.is_syscon = m181_is_syscon,
	.intr = m181_intr,
	.nmi = NULL,
	.nmi_wrapup = NULL,
	.getipl = m181_getipl,
	.setipl = m181_setipl,
	.raiseipl = m181_raiseipl,
	.intsrc_available = m181_intsrc_available,
	.intsrc_enable = m181_intsrc_enable,
	.intsrc_disable = m181_intsrc_disable,
	.intsrc_establish = m181_intsrc_establish,
	.intsrc_disestablish = m181_intsrc_disestablish,
	.init_clocks = m181_init_clocks,
	.delay = dumb_delay,
	.init_vme = m181_init_vme,
#ifdef MULTIPROCESSOR
	.send_ipi = NULL,
	.smp_setup = m88100_smp_setup,
#endif
	.ptable = m181_pmap_table,
	.cmmu = &cmmu8820x
};

u_int	m181_safe_level(u_int, u_int);

u_int	m181_curspl = IPL_HIGH;

/*
 * The AngelFire interrupt controller has up to 16 interrupt sources.
 * We fold this model in the 8-level spl model this port uses, enforcing
 * priorities manually with the interrupt masks.
 */

intrhand_t angelfire_intr_handlers[INTSRC_VME];

u_int32_t m181_int_mask, m181_int_sticky;

/*
 * Early initialization.
 */
void
m181_bootstrap()
{
	int i;

	/*
	 * Initialize interrupt controller masks.
	 */

	m181_int_mask = M181_IRQ_DUART | M181_IRQ_ABORT;
	if (brdtyp == BRD_180)
		m181_int_mask |= M181_IRQ_VME4_180;
	else
		m181_int_mask |= M181_IRQ_PARITY | M181_IRQ_VME1 |
		    M181_IRQ_VME2 | M181_IRQ_VME3 | M181_IRQ_VME4 |
		    M181_IRQ_VME5 | M181_IRQ_VME6 | M181_IRQ_VME7;

	m181_int_sticky = *(volatile uint32_t *)M181_SSR &
	    (M181_SYSCON | M181_BOARDMODE);

	*(u_int32_t *)M181_SCR = m181_int_sticky;

	for (i = 0; i < INTSRC_VME; i++)
		SLIST_INIT(&angelfire_intr_handlers[i]);
}

/*
 * Figure out how much memory is available.
 *
 * AngelFire boards always have 8MB onboard; MVME224 and/or MVME236 boards
 * may be used. SVR3/m88k officially only supports up to two MVME224 boards
 * and up to four MVME236 boards, which would allow for up to 2x8 + 4x16MB
 * extra memory.
 *
 * However, Motorola eventually produced 16MB and 32MB MVME224A boards, and
 * the MVME224 documentation says up to five MVME224 boards may be present
 * in the same chassis, when using them in VSB configuration (which hints
 * that more than five may be possible, in VME configuration).
 *
 * The real limiting factor, is the location of the CSR register in VME
 * A16 (short I/O) space. MVME224 boards use a fixed assignment, depending
 * upon their base address in VME address space; while MVME236 gives full
 * control of the 14 higher bits.
 *
 * Add to this, that the on-board memory may be configured to answer to a
 * different base address than zero, and you have a nightmare, with no
 * chance to be able to figure out which memory boards are available by
 * looking for their CSR registers.
 *
 * Better fall back to the good old `try and find memory until we fault or
 * whatever we hit does not have memory properties' logic. If one ever sets
 * up a noncontiguous memory layout, only the lower part will be found.
 */
vaddr_t
m181_memsize()
{
#define	M181_ONBOARD_SIZE	(8 * 1024 * 1024)
#define	M181_MEMORY_LIMIT	(128 * 1024 * 1024) /* got more? call me */
#define	M181_SAFE_STRIDE	(64 * 1024) /* conservative... 1MB could work */
#define	M181_MEMORY_PATTERN	0x55aac33c
	paddr_t pa;
	uint32_t r1, r2;

	for (pa = M181_ONBOARD_SIZE; pa <= M181_MEMORY_LIMIT;
	    pa += M181_SAFE_STRIDE) {
		/* memory ought to be accessible at any size */
		if (badaddr(pa, 8) != 0 || badaddr(pa, 4) != 0 ||
		    badaddr(pa, 2) != 0 || badaddr(pa, 1) != 0)
			break;

		/* what gets written to memory ought to be read back */
		r1 = *(volatile uint32_t *)pa;
		*(volatile uint32_t *)pa = M181_MEMORY_PATTERN;
		r2 = *(volatile uint32_t *)pa;
		if (r2 != M181_MEMORY_PATTERN)
			break;

		/* and all bits should be writeable as either 1 or 0 */
		*(volatile uint32_t *)pa = ~M181_MEMORY_PATTERN;
		r2 = *(volatile uint32_t *)pa;
		if (r2 != ~M181_MEMORY_PATTERN)
			break;

		*(volatile uint32_t *)pa = r1;
		r2 = *(volatile uint32_t *)pa;
		if (r2 != r1)
			break;
	}
	return pa;
}

/*
 * Reboot the system.
 * AngelFire boards can't reboot themselves (even the BUG won't).
 */
void
m181_reboot(int howto)
{
	printf("Reboot not available on MVME%x. "
	    "Please manually reset the board.\n", brdtyp);
}

/*
 * Return whether we are the VME bus system controller.
 */
int
m181_is_syscon()
{
	return ISSET(*(volatile uint32_t *)M181_SSR, M181_SYSCON);
}

/*
 * Return the next ipl >= ``curlevel'' at which we can reenable interrupts
 * while keeping ``mask'' masked.
 */
u_int
m181_safe_level(u_int mask, u_int curlevel)
{
	int i;

	for (i = curlevel; i < NIPLS; i++)
		if ((int_mask_val[i] & mask) == 0)
			return i;

	return NIPLS - 1;
}

/*
 * Provide the interrupt source for a given interrupt status bit.
 */
const u_int m181_vec[16] = {
	0,
	0,
	0,
	0,
	INTSRC_VME,		/* MVME180 VME4 */
	INTSRC_PARERR,
	INTSRC_DUART,
	INTSRC_ABORT,
	0,
	INTSRC_VME,		/* VME1 */
	INTSRC_VME,		/* VME2 */
	INTSRC_VME,		/* VME3 */
	INTSRC_VME,		/* VME4 */
	INTSRC_VME,		/* VME5 */
	INTSRC_VME,		/* VME6 */
	INTSRC_VME		/* VME7 */
};

/*
 * Device interrupt handler for MVME181
 */
void
m181_intr(struct trapframe *eframe)
{
	u_int32_t cur_mask, ign_mask;
	u_int level, vmelevel, old_spl;
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

	cur_mask = *(volatile u_int32_t *)M181_SSR & m181_int_mask;
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

#ifdef MULTIPROCESSOR
	if (old_spl < IPL_SCHED)
		__mp_lock(&kernel_lock);
#endif

	uvmexp.intrs++;

	for (;;) {
		level = m181_safe_level(cur_mask, old_spl);
		m181_setipl(level);

		if (unmasked == 0) {
			set_psr(get_psr() & ~PSR_IND);
			unmasked = 1;
		}

		/* find the first bit set in the current mask */
		warn = 0;
		intbit = ff1(cur_mask);
		intsrc = m181_vec[intbit];

		if (intsrc == 0)
			panic("%s: unexpected interrupt source (bit %d), "
			    "level %d, mask 0x%04x",
			    __func__, intbit, level, cur_mask);

		if (intsrc == INTSRC_VME) {
			if (intbit < 8)
				vmelevel = 4;	/* MVME180, level 4 */
			else
				vmelevel = intbit & 7;
			ivec = M181_VMEVEC + (vmelevel << 1);
			vec = *(volatile uint16_t *)ivec;
			if (vec & 0xff00) {
				/*
				 * Vector number is wrong.  This is likely
				 * a bus error or timeout reading the VME
				 * interrupt vector.  In either case, we
				 * can't service the interrupt correctly.
				 *
				 * As a cheap bandaid, if only one VME
				 * interrupt is registered with this IPL,
				 * we can reasonably safely assume that
				 * this is our vector. (this probably loses
				 * bigtime on the 180)
				 */
				vec = vmevec_hints[vmelevel];
				if (vec == (u_int)-1) {
					printf("%s: invalid VME "
					    "interrupt vector %04x, "
					    "level %d, mask %04x\n",
					    __func__, ivec, vmelevel, cur_mask);
					ign_mask |= 1 << intbit;
					continue;
				}
			}
			list = &intr_handlers[vec];
		} else {
			list = &angelfire_intr_handlers[intsrc];
		}

		if (SLIST_EMPTY(list)) {
			warn = 1;
		} else {
			/*
			 * Walk through all interrupt handlers in the chain
			 * for the given vector, calling each handler in
			 * turn, until some handler returns a nonzero value.
			 */
			ret = 0;
			SLIST_FOREACH(intr, list, ih_link) {
				if (intr->ih_wantframe != 0)
					ret = (*intr->ih_fn)((void *)eframe);
				else
					ret = (*intr->ih_fn)(intr->ih_arg);
				if (ret > 0) {
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
				    "level %d, vec 0x%x, mask %04x\n",
				    __func__,
				    warn == 1 ? "spurious" : "unclaimed",
				    level, vec, cur_mask);
			else
				printf("%s: %s interrupt, "
				    "level %d, bit %d, mask %04x\n",
				    __func__,
				    warn == 1 ? "spurious" : "unclaimed",
				    level, intbit, cur_mask);
		}

		/*
		 * Read updated pending interrupt mask.
		 */
		cur_mask = *(volatile u_int32_t *)M181_SSR & m181_int_mask;
		if ((cur_mask & ~ign_mask) == 0)
			break;
	}

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
	 * Process any remaining data access exceptions before
	 * returning to assembler.
	 */
	if (eframe->tf_dmt0 & DMT_VALID)
		m88100_trap(T_DATAFLT, eframe);

	/*
	 * Disable interrupts before returning to assembler, the spl will
	 * be restored later.
	 */
	set_psr(get_psr() | PSR_IND);
}

u_int
m181_getipl(void)
{
	return m181_curspl;
}

u_int
m181_setipl(u_int level)
{
	u_int curspl, psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	curspl = m181_curspl;

	m181_curspl = level;
	*(u_int32_t *)M181_SCR = int_mask_val[level] | m181_int_sticky;

	set_psr(psr);
	return curspl;
}

u_int
m181_raiseipl(u_int level)
{
	u_int curspl, psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	curspl = m181_curspl;
	if (curspl < level) {
		m181_curspl = level;
		*(u_int32_t *)M181_SCR = int_mask_val[level] | m181_int_sticky;
	}

	set_psr(psr);
	return curspl;
}

/* Interrupt masks per logical interrupt source */
const u_int32_t m181_intsrc[] = {
	0,
	M181_IRQ_ABORT,
	0,
	0,
	M181_IRQ_PARITY,
	0,
	0,
	M181_IRQ_DUART,

	M181_IRQ_VME1,
	M181_IRQ_VME2,
	M181_IRQ_VME3,
	M181_IRQ_VME4,
	M181_IRQ_VME5,
	M181_IRQ_VME6,
	M181_IRQ_VME7
};

int
m181_intsrc_available(u_int intsrc, int ipl)
{
	if (intsrc == INTSRC_VME) {
#if 0 /* unwise */
		/*
		 * The original AngelFire board apparently only allows
		 * VME interrupts at level 4.
		 */
		if (brdtyp == BRD_180 && ipl != 4)
			return EINVAL;
#endif
		return 0;
	}

	if (m181_intsrc[intsrc] == 0)
		return ENXIO;

	return 0;
}

void
m181_intsrc_enable(u_int intsrc, int ipl)
{
	u_int32_t psr;
	u_int32_t intmask;
	int i;

	if (intsrc == INTSRC_VME) {
		/*
		 * The original AngelFire board apparently only allows
		 * VME interrupts at level 4.
		 */
		if (brdtyp == BRD_180)
			intmask = M181_IRQ_VME4_180;
		else
			intmask = m181_intsrc[INTSRC_VME + (ipl - 1)];
	} else
		intmask = m181_intsrc[intsrc];

	psr = get_psr();
	set_psr(psr | PSR_IND);

	for (i = IPL_NONE; i < ipl; i++)
		int_mask_val[i] |= intmask;

	setipl(getipl());
	set_psr(psr);
}

void
m181_intsrc_disable(u_int intsrc, int ipl)
{
	u_int32_t psr;
	u_int32_t intmask;
	int i;

	if (intsrc == INTSRC_VME) {
		/*
		 * The original AngelFire board apparently only allows
		 * VME interrupts at level 4.
		 */
		if (brdtyp == BRD_180)
			intmask = M181_IRQ_VME4_180;
		else
			intmask = m181_intsrc[INTSRC_VME + (ipl - 1)];
	} else
		intmask = m181_intsrc[intsrc];

	psr = get_psr();
	set_psr(psr | PSR_IND);

	for (i = 0; i < NIPLS; i++)
		int_mask_val[i] &= ~intmask;

	setipl(getipl());
	set_psr(psr);
}

int
m181_intsrc_establish(u_int intsrc, struct intrhand *ih, const char *name)
{
	intrhand_t *list;

#ifdef DIAGNOSTIC
	if (intsrc == INTSRC_VME)
		return EINVAL;
#endif

	/*
	 * Unlike MVME188, timer interrupts from the duart chip are not
	 * received on a separate input.
	 */
	if (intsrc == INTSRC_DTIMER)
		intsrc = INTSRC_DUART;

	list = &angelfire_intr_handlers[intsrc];
	if (!SLIST_EMPTY(list) && intsrc != INTSRC_DUART) {
#ifdef DIAGNOSTIC
		printf("%s: interrupt source %u already registered\n",
		    __func__, intsrc);
#endif
		return EINVAL;
	}

	if (m181_intsrc_available(intsrc, ih->ih_ipl) != 0)
		return EINVAL;

	evcount_attach(&ih->ih_count, name, &ih->ih_ipl);
	SLIST_INSERT_HEAD(list, ih, ih_link);
	m181_intsrc_enable(intsrc, ih->ih_ipl);

	return 0;
}

void
m181_intsrc_disestablish(u_int intsrc, struct intrhand *ih)
{
	intrhand_t *list;

#ifdef DIAGNOSTIC
	if (intsrc == INTSRC_VME)
		return;
#endif

	/*
	 * Unlike MVME188, timer interrupts from the duart chip are not
	 * received on a separate input.
	 */
	if (intsrc == INTSRC_DTIMER)
		intsrc = INTSRC_DUART;

	list = &angelfire_intr_handlers[intsrc];
	evcount_detach(&ih->ih_count);
	SLIST_REMOVE(list, ih, intrhand, ih_link);

	if (SLIST_EMPTY(list))
		m181_intsrc_disable(intsrc, ih->ih_ipl);
}

/*
 * Clock routines
 */

/*
 * Notes on the MVME181 clock usage:
 *
 * We have only one timer source, the two counter/timers in the DUART
 * (MC68681/MC68692), which share the DUART serial interrupt.
 */

#define	DART_REG(x)	((volatile uint8_t *)(M181_DUART | ((x) << 2) | 0x03))

u_int	m181_get_tc(struct timecounter *);
int	m181_clockintr(void *);
int	m181_clkint;

struct timecounter m181_timecounter = {
	.tc_get_timecount = m181_get_tc,
	.tc_counter_mask = 0xffffffff,
	.tc_frequency = 100,
	.tc_name = "duart",
	.tc_quality = 0
};

u_int
m181_get_tc(struct timecounter *tc)
{
	return (u_int)clock_ih.ih_count.ec_count;
}

void
m181_init_clocks(void)
{
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

	stathz = profhz = 0;

	/* the DART clock runs at 3.6864 MHz, CT#1 will run in PCLK/16 mode. */
	m181_clkint = (3686400 / 16) / hz;
	/* in timer mode, interrupts occur every second cycle */
	m181_clkint >>= 1;

	/* clear the counter/timer output OP3 while we program the DART */
	*DART_REG(DART_OPCR) = DART_OPCR_OP3;
	/* do the stop counter/timer command */
	(void)*DART_REG(DART_CTSTOP);
	/* set counter/timer to timer mode, PCLK/16 */
	*DART_REG(DART_ACR) = DART_ACR_BRG_SET_2 | DART_ACR_CT_TIMER_CLK_16;
	*DART_REG(DART_CTUR) = m181_clkint >> 8;
	*DART_REG(DART_CTLR) = m181_clkint & 0xff;
	/* give the start counter/timer command */
	(void)*DART_REG(DART_CTSTART);

	clock_ih.ih_fn = m181_clockintr;
	clock_ih.ih_arg = 0;
	clock_ih.ih_wantframe = 1;
	clock_ih.ih_ipl = IPL_CLOCK;
	m181_intsrc_establish(INTSRC_DTIMER, &clock_ih, "clock");

	tc_init(&m181_timecounter);
}

int
m181_clockintr(void *eframe)
{
	u_int8_t isr;

	isr = *DART_REG(DART_ISR);
	if ((isr & DART_ISR_CT) == 0)
		return 0;

	/* acknowledge and clear interrupt */
	(void)*DART_REG(DART_CTSTOP);

	hardclock(eframe);

	return 1;
}

/*
 * Return the processor speed in MHz.
 * Since there is no easy way to figure out this information, we'll simply
 * spin for a known amount of time incrementing a counter and deduct the
 * processor speed.
 */
int
m181_cpuspeed(const struct mvmeprom_brdid *brdid)
{
	static int af_cpuspeed = 0;
	u_int cycles, clkspan;

	if (af_cpuspeed == 0) {
		cycles = 0;
		clkspan = (3686400 / 16) / 100;

		*DART_REG(DART_OPCR) = DART_OPCR_OP3;
		(void)*DART_REG(DART_CTSTOP);
		*DART_REG(DART_ACR) =
		    DART_ACR_BRG_SET_2 | DART_ACR_CT_COUNTER_CLK_16;
		*DART_REG(DART_CTUR) = clkspan >> 8;
		*DART_REG(DART_CTLR) = clkspan & 0xff;
		(void)*DART_REG(DART_CTSTART);

		do {
			cycles++;
		} while ((*DART_REG(DART_ISR) & DART_ISR_CT) == 0);
		(void)*DART_REG(DART_CTSTOP);

		if (cycles < 10000)	/* observed ~9220 @20MHz */
			af_cpuspeed = 20;
		else
			af_cpuspeed = 25;
	}

	return af_cpuspeed;
}

/*
 * Setup VME bus access and return the lower interrupt number usable by VME
 * boards.
 */
u_int
m181_init_vme(const char *devname)
{
	return 0;	/* all vectors available */
}
