/*	$OpenBSD: lapic.c,v 1.10 2007/05/25 16:22:11 art Exp $	*/
/* $NetBSD: lapic.c,v 1.2 2003/05/08 01:04:35 fvdl Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <dev/ic/i8253reg.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/mpbiosvar.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <machine/segments.h>

#include <machine/apicvar.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>

#include "ioapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

struct evcount clk_count;
struct evcount ipi_count;

void		lapic_delay(int);
void		lapic_microtime(struct timeval *);
static u_int32_t lapic_gettick(void);
void		lapic_clockintr(void *, struct intrframe);
void	 	lapic_map(paddr_t);

void lapic_hwmask(struct pic *, int);
void lapic_hwunmask(struct pic *, int);
void lapic_setup(struct pic *, struct cpu_info *, int, int, int);

extern char idt_allocmap[];

struct pic local_pic = {
	{0, {NULL}, NULL, 0, "lapic", NULL, 0, 0},
	PIC_LAPIC,
#ifdef MULTIPROCESSOR
	{},
#endif
	lapic_hwmask,
	lapic_hwunmask,
	lapic_setup,
	lapic_setup,
};

void
lapic_map(paddr_t lapic_base)
{
	int s;
	pt_entry_t *pte;
	vaddr_t va = (vaddr_t)&local_apic;

	disable_intr();
	s = lapic_tpr;

	/*
	 * Map local apic.  If we have a local apic, it's safe to assume
	 * we're on a 486 or better and can use invlpg and non-cacheable PTE's
	 *
	 * Whap the PTE "by hand" rather than calling pmap_kenter_pa because
	 * the latter will attempt to invoke TLB shootdown code just as we
	 * might have changed the value of cpu_number()..
	 */

	pte = kvtopte(va);
	*pte = lapic_base | PG_RW | PG_V | PG_N | pmap_pg_g;
	invlpg(va);

#ifdef MULTIPROCESSOR
	cpu_init_first();	/* catch up to changed cpu_number() */
#endif

	lapic_tpr = s;
	enable_intr();
}

/*
 * enable local apic
 */
void
lapic_enable(void)
{
	i82489_writereg(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);
}

void
lapic_set_lvt(void)
{
	struct cpu_info *ci = curcpu();
	int i;
	struct mp_intr_map *mpi;
	uint32_t lint0;

#ifdef MULTIPROCESSOR
	if (mp_verbose) {
		apic_format_redir (ci->ci_dev->dv_xname, "prelint", 0, 0,
		    i82489_readreg(LAPIC_LVINT0));
		apic_format_redir (ci->ci_dev->dv_xname, "prelint", 1, 0,
		    i82489_readreg(LAPIC_LVINT1));
	}
#endif

#if NIOAPIC > 0
	/*
	 * Disable ExtINT by default when using I/O APICs.
	 */
	if (nioapics > 0) {
		lint0 = i82489_readreg(LAPIC_LVINT0);
		lint0 |= LAPIC_LVT_MASKED;
		i82489_writereg(LAPIC_LVINT0, lint0);
	}
#endif

	for (i = 0; i < mp_nintr; i++) {
		mpi = &mp_intrs[i];
		if (mpi->ioapic == NULL && (mpi->cpu_id == MPS_ALL_APICS
					    || mpi->cpu_id == ci->ci_apicid)) {
#ifdef DIAGNOSTIC
			if (mpi->ioapic_pin > 1)
				panic("lapic_set_lvt: bad pin value %d",
				    mpi->ioapic_pin);
#endif
			if (mpi->ioapic_pin == 0)
				i82489_writereg(LAPIC_LVINT0, mpi->redir);
			else
				i82489_writereg(LAPIC_LVINT1, mpi->redir);
		}
	}

#ifdef MULTIPROCESSOR
	if (mp_verbose) {
		apic_format_redir (ci->ci_dev->dv_xname, "timer", 0, 0,
		    i82489_readreg(LAPIC_LVTT));
		apic_format_redir (ci->ci_dev->dv_xname, "pcint", 0, 0,
		    i82489_readreg(LAPIC_PCINT));
		apic_format_redir (ci->ci_dev->dv_xname, "lint", 0, 0,
		    i82489_readreg(LAPIC_LVINT0));
		apic_format_redir (ci->ci_dev->dv_xname, "lint", 1, 0,
		    i82489_readreg(LAPIC_LVINT1));
		apic_format_redir (ci->ci_dev->dv_xname, "err", 0, 0,
		    i82489_readreg(LAPIC_LVERR));
	}
#endif
}

/*
 * Initialize fixed idt vectors for use by local apic.
 */
void
lapic_boot_init(paddr_t lapic_base)
{
	static u_int64_t clk_irq = 0;
	static u_int64_t ipi_irq = 0;

	lapic_map(lapic_base);

#ifdef MULTIPROCESSOR
	idt_allocmap[LAPIC_IPI_VECTOR] = 1;
	idt_vec_set(LAPIC_IPI_VECTOR, Xintr_lapic_ipi);
	idt_allocmap[LAPIC_IPI_INVLTLB] = 1;
	idt_vec_set(LAPIC_IPI_INVLTLB, Xipi_invltlb);
	idt_allocmap[LAPIC_IPI_INVLPG] = 1;
	idt_vec_set(LAPIC_IPI_INVLPG, Xipi_invlpg);
	idt_allocmap[LAPIC_IPI_INVLRANGE] = 1;
	idt_vec_set(LAPIC_IPI_INVLRANGE, Xipi_invlrange);
#endif
	idt_allocmap[LAPIC_SPURIOUS_VECTOR] = 1;
	idt_vec_set(LAPIC_SPURIOUS_VECTOR, Xintrspurious);

	idt_allocmap[LAPIC_TIMER_VECTOR] = 1;
	idt_vec_set(LAPIC_TIMER_VECTOR, Xintr_lapic_ltimer);

	evcount_attach(&clk_count, "clock", (void *)&clk_irq, &evcount_intr);
	evcount_attach(&ipi_count, "ipi", (void *)&ipi_irq, &evcount_intr);
}

static inline u_int32_t
lapic_gettick(void)
{
	return i82489_readreg(LAPIC_CCR_TIMER);
}

#include <sys/kernel.h>		/* for hz */

u_int32_t lapic_tval;

/*
 * this gets us up to a 4GHz busclock....
 */
u_int32_t lapic_per_second;
u_int32_t lapic_frac_usec_per_cycle;
u_int64_t lapic_frac_cycle_per_usec;
u_int32_t lapic_delaytab[26];

void
lapic_clockintr(void *arg, struct intrframe frame)
{

	hardclock((struct clockframe *)&frame);

	clk_count.ec_count++;
}

void
lapic_initclocks(void)
{
	/*
	 * Start local apic countdown timer running, in repeated mode.
	 *
	 * Mask the clock interrupt and set mode,
	 * then set divisor,
	 * then unmask and set the vector.
	 */
	i82489_writereg (LAPIC_LVTT, LAPIC_LVTT_TM|LAPIC_LVTT_M);
	i82489_writereg (LAPIC_DCR_TIMER, LAPIC_DCRT_DIV1);
	i82489_writereg (LAPIC_ICR_TIMER, lapic_tval);
	i82489_writereg (LAPIC_LVTT, LAPIC_LVTT_TM|LAPIC_TIMER_VECTOR);
}

extern int gettick(void);	/* XXX put in header file */
extern u_long rtclock_tval; /* XXX put in header file */
extern void (*initclock_func)(void); /* XXX put in header file */

/*
 * Calibrate the local apic count-down timer (which is running at
 * bus-clock speed) vs. the i8254 counter/timer (which is running at
 * a fixed rate).
 *
 * The Intel MP spec says: "An MP operating system may use the IRQ8
 * real-time clock as a reference to determine the actual APIC timer clock
 * speed."
 *
 * We're actually using the IRQ0 timer.  Hmm.
 */
void
lapic_calibrate_timer(struct cpu_info *ci)
{
	unsigned int starttick, tick1, tick2, endtick;
	unsigned int startapic, apic1, apic2, endapic;
	u_int64_t dtick, dapic, tmp;
	int i;

	if (mp_verbose)
		printf("%s: calibrating local timer\n", ci->ci_dev->dv_xname);

	/*
	 * Configure timer to one-shot, interrupt masked,
	 * large positive number.
	 */
	i82489_writereg (LAPIC_LVTT, LAPIC_LVTT_M);
	i82489_writereg (LAPIC_DCR_TIMER, LAPIC_DCRT_DIV1);
	i82489_writereg (LAPIC_ICR_TIMER, 0x80000000);

	starttick = gettick();
	startapic = lapic_gettick();

	for (i=0; i<hz; i++) {
		DELAY(2);
		do {
			tick1 = gettick();
			apic1 = lapic_gettick();
		} while (tick1 < starttick);
		DELAY(2);
		do {
			tick2 = gettick();
			apic2 = lapic_gettick();
		} while (tick2 > starttick);
	}

	endtick = gettick();
	endapic = lapic_gettick();

	dtick = hz * rtclock_tval + (starttick-endtick);
	dapic = startapic-endapic;

	/*
	 * there are TIMER_FREQ ticks per second.
	 * in dtick ticks, there are dapic bus clocks.
	 */
	tmp = (TIMER_FREQ * dapic) / dtick;

	lapic_per_second = tmp;

	printf("%s: apic clock running at %lldMHz\n",
	    ci->ci_dev->dv_xname, tmp / (1000 * 1000));

	if (lapic_per_second != 0) {
		/*
		 * reprogram the apic timer to run in periodic mode.
		 * XXX need to program timer on other cpu's, too.
		 */
		lapic_tval = (lapic_per_second * 2) / hz;
		lapic_tval = (lapic_tval / 2) + (lapic_tval & 0x1);

		i82489_writereg (LAPIC_LVTT, LAPIC_LVTT_TM|LAPIC_LVTT_M
		    |LAPIC_TIMER_VECTOR);
		i82489_writereg (LAPIC_DCR_TIMER, LAPIC_DCRT_DIV1);
		i82489_writereg (LAPIC_ICR_TIMER, lapic_tval);

		/*
		 * Compute fixed-point ratios between cycles and
		 * microseconds to avoid having to do any division
		 * in lapic_delay and lapic_microtime.
		 */

		tmp = (1000000 * (u_int64_t)1<<32) / lapic_per_second;
		lapic_frac_usec_per_cycle = tmp;

		tmp = (lapic_per_second * (u_int64_t)1<<32) / 1000000;

		lapic_frac_cycle_per_usec = tmp;

		/*
		 * Compute delay in cycles for likely short delays in usec.
		 */
		for (i=0; i<26; i++)
			lapic_delaytab[i] = (lapic_frac_cycle_per_usec * i) >>
			    32;

		/*
		 * Now that the timer's calibrated, use the apic timer routines
		 * for all our timing needs..
		 */
		delay_func = lapic_delay;
		initclock_func = lapic_initclocks;
	}
}

/*
 * delay for N usec.
 */

void
lapic_delay(int usec)
{
	int32_t tick, otick;
	int64_t deltat;		/* XXX may want to be 64bit */

	otick = lapic_gettick();

	if (usec <= 0)
		return;
	if (usec <= 25)
		deltat = lapic_delaytab[usec];
	else
		deltat = (lapic_frac_cycle_per_usec * usec) >> 32;

	while (deltat > 0) {
		tick = lapic_gettick();
		if (tick > otick)
			deltat -= lapic_tval - (tick - otick);
		else
			deltat -= otick - tick;
		otick = tick;

		x86_pause();
	}
}

/*
 * XXX the following belong mostly or partly elsewhere..
 */

static __inline void i82489_icr_wait(void);

static __inline void
i82489_icr_wait(void)
{
#ifdef DIAGNOSTIC
	unsigned j = 100000;
#endif /* DIAGNOSTIC */

	while ((i82489_readreg(LAPIC_ICRLO) & LAPIC_DLSTAT_BUSY) != 0) {
		x86_pause();
#ifdef DIAGNOSTIC
		j--;
		if (j == 0)
			panic("i82489_icr_wait: busy");
#endif /* DIAGNOSTIC */
	}
}

int
x86_ipi_init(int target)
{

	if ((target&LAPIC_DEST_MASK)==0) {
		i82489_writereg(LAPIC_ICRHI, target<<LAPIC_ID_SHIFT);
	}

	i82489_writereg(LAPIC_ICRLO, (target & LAPIC_DEST_MASK) |
	    LAPIC_DLMODE_INIT | LAPIC_LVL_ASSERT );

	i82489_icr_wait();

	delay(10000);

	i82489_writereg(LAPIC_ICRLO, (target & LAPIC_DEST_MASK) |
	     LAPIC_DLMODE_INIT | LAPIC_LVL_TRIG | LAPIC_LVL_DEASSERT);

	i82489_icr_wait();

	return (i82489_readreg(LAPIC_ICRLO) & LAPIC_DLSTAT_BUSY)?EBUSY:0;
}

int
x86_ipi(int vec, int target, int dl)
{
	int result, s;

	s = splclock();

	i82489_icr_wait();

	if ((target & LAPIC_DEST_MASK) == 0)
		i82489_writereg(LAPIC_ICRHI, target << LAPIC_ID_SHIFT);

	i82489_writereg(LAPIC_ICRLO,
	    (target & LAPIC_DEST_MASK) | vec | dl | LAPIC_LVL_ASSERT);

	i82489_icr_wait();

	result = (i82489_readreg(LAPIC_ICRLO) & LAPIC_DLSTAT_BUSY) ? EBUSY : 0;

	splx(s);

	return result;
}


/*
 * Using 'pin numbers' as:
 * 0 - timer
 * 1 - unused
 * 2 - PCINT
 * 3 - LVINT0
 * 4 - LVINT1
 * 5 - LVERR
 */

void
lapic_hwmask(struct pic *pic, int pin)
{
	int reg;
	u_int32_t val;

	reg = LAPIC_LVTT + (pin << 4);
	val = i82489_readreg(reg);
	val |= LAPIC_LVT_MASKED;
	i82489_writereg(reg, val);
}

void
lapic_hwunmask(struct pic *pic, int pin)
{
	int reg;
	u_int32_t val;

	reg = LAPIC_LVTT + (pin << 4);
	val = i82489_readreg(reg);
	val &= ~LAPIC_LVT_MASKED;
	i82489_writereg(reg, val);
}

void
lapic_setup(struct pic *pic, struct cpu_info *ci, int pin, int idtvec, int type)
{
}
