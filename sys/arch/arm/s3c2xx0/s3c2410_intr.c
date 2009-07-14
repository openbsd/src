/*	$OpenBSD: s3c2410_intr.c,v 1.3 2009/07/14 13:59:49 drahn Exp $ */
/* $NetBSD: s3c2410_intr.c,v 1.11 2008/11/24 11:29:52 dogcow Exp $ */

/*
 * Copyright (c) 2003  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IRQ handler for Samsung S3C2410 processor.
 * It has integrated interrupt controller.
 */

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: s3c2410_intr.c,v 1.11 2008/11/24 11:29:52 dogcow Exp $");
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <uvm/uvm_extern.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>

#include <arm/s3c2xx0/s3c2410reg.h>
#include <arm/s3c2xx0/s3c2410var.h>
#include <arm/s3c2xx0/s3c2xx0_intr.h>

/*
 * interrupt dispatch table.
 */

struct s3c2xx0_intr_dispatch handler[ICU_LEN];

extern volatile uint32_t *s3c2xx0_intr_mask_reg;

volatile int global_intr_mask = 0; /* mask some interrupts at all spl level */

/* interrupt masks for each level */
int s3c2xx0_imask[NIPL];
int s3c2xx0_smask[NIPL];
int s3c2xx0_ilevel[ICU_LEN];
#ifdef __HAVE_FAST_SOFTINTS
int s3c24x0_soft_imask[NIPL];
#endif

vaddr_t intctl_base;		/* interrupt controller registers */
#define icreg(offset) \
	(*(volatile uint32_t *)(intctl_base+(offset)))

#ifdef __HAVE_FAST_SOFTINTS
/*
 * Map a software interrupt queue to an interrupt priority level.
 */
static const int si_to_ipl[] = {
#ifdef SI_SOFTBIO
	[SI_SOFTBIO]	= IPL_SOFTBIO,
#endif
	[SI_SOFTCLOCK]	= IPL_SOFTCLOCK,
	[SI_SOFTNET]	= IPL_SOFTNET,
	[SI_SOFTTTY] = IPL_SOFTTTY,
};
#endif

#define PENDING_CLEAR_MASK	(~0)

int debug_update_hw;
void
s3c2xx0_update_hw_mask(void)
{
	if (debug_update_hw != NULL)
		printf("setting irq mask to ~(%x & %x) = %x\n",
		    s3c2xx0_imask[s3c2xx0_curcpl()], global_intr_mask,
		    ~(s3c2xx0_imask[s3c2xx0_curcpl()] & global_intr_mask));
	(*s3c2xx0_intr_mask_reg =
	    ~(s3c2xx0_imask[s3c2xx0_curcpl()] & global_intr_mask));
}

/*
 * called from irq_entry.
 */
void s3c2410_irq_handler(struct clockframe *);

void
s3c2410_irq_handler(struct clockframe *frame)
{
	uint32_t irqbits;
	int irqno;
	int saved_spl_level;

	saved_spl_level = s3c2xx0_curcpl();

#ifdef	DIAGNOSTIC
	if (curcpu()->ci_intr_depth > 10)
		panic("nested intr too deep");
#endif

	while ((irqbits = icreg(INTCTL_INTPND)) != 0) {

		/* Note: Only one bit in INTPND register is set */

		irqno = icreg(INTCTL_INTOFFSET);

#ifdef	DIAGNOSTIC
		if (__predict_false((irqbits & (1<<irqno)) == 0)) {
			/* This shouldn't happen */
			printf("INTOFFSET=%d, INTPND=%x\n", irqno, irqbits);
			break;
		}
#endif
		/* raise spl to stop interrupts of lower priorities */
		if (saved_spl_level < handler[irqno].level)
			s3c2xx0_setipl(handler[irqno].level);

		/* clear pending bit */
		icreg(INTCTL_SRCPND) = PENDING_CLEAR_MASK & (1 << irqno);
		icreg(INTCTL_INTPND) = PENDING_CLEAR_MASK & (1 << irqno);

		enable_interrupts(I32_bit); /* allow nested interrupts */

		(*handler[irqno].func) (
		    handler[irqno].cookie == 0
		    ? frame : handler[irqno].cookie);

		disable_interrupts(I32_bit);

		/* restore spl to that was when this interrupt happen */
		s3c2xx0_setipl(saved_spl_level);

	}

#ifdef __HAVE_FAST_SOFTINTS
#if 0
	cpu_dosoftints();
#else
	s3c2xx0_irq_do_pending();
#endif
#endif
}

/*
 * Handler for main IRQ of cascaded interrupts.
 */
static int
cascade_irq_handler(void *cookie)
{
	int index = (int)cookie - 1;
	uint32_t irqbits;
	int irqno, i;
	int save = disable_interrupts(I32_bit);

	KASSERT(0 <= index && index <= 3);

	irqbits = icreg(INTCTL_SUBSRCPND) &
	    ~icreg(INTCTL_INTSUBMSK) & (0x07 << (3*index));

	for (irqno = 3*index; irqbits; ++irqno) {
		if ((irqbits & (1<<irqno)) == 0)
			continue;

		/* clear pending bit */
		irqbits &= ~(1<<irqno);
		icreg(INTCTL_SUBSRCPND) = (1 << irqno);

		/* allow nested interrupts. SPL is already set
		 * correctly by main handler. */
		restore_interrupts(save);

		i = S3C2410_SUBIRQ_MIN + irqno;
		(* handler[i].func)(handler[i].cookie);

		disable_interrupts(I32_bit);
	}

	return 1;
}


static const uint8_t subirq_to_main[] = {
	S3C2410_INT_UART0,
	S3C2410_INT_UART0,
	S3C2410_INT_UART0,
	S3C2410_INT_UART1,
	S3C2410_INT_UART1,
	S3C2410_INT_UART1,
	S3C2410_INT_UART2,
	S3C2410_INT_UART2,
	S3C2410_INT_UART2,
	S3C24X0_INT_ADCTC,
	S3C24X0_INT_ADCTC,
};

void *
s3c24x0_intr_establish(int irqno, int level, int type,
    int (* func) (void *), void *cookie)
{
	int save;

	if (irqno < 0 || irqno >= ICU_LEN ||
	    type < IST_NONE || IST_EDGE_BOTH < type)
		panic("intr_establish: bogus irq or type");

	save = disable_interrupts(I32_bit);

	handler[irqno].cookie = cookie;
	handler[irqno].func = func;
	handler[irqno].level = level;

	if (irqno >= S3C2410_SUBIRQ_MIN) {
		/* cascaded interrupts. */
		int main_irqno;
		int i = (irqno - S3C2410_SUBIRQ_MIN);

		main_irqno = subirq_to_main[i];

		/* establish main irq if first time
		 * be careful that cookie shouldn't be 0 */
		if (handler[main_irqno].func != cascade_irq_handler)
			s3c24x0_intr_establish(main_irqno, level, type,
			    cascade_irq_handler, (void *)((i/3) + 1));

		/* unmask it in submask register */
		icreg(INTCTL_INTSUBMSK) &= ~(1<<i);

		restore_interrupts(save);
		return &handler[irqno];
	}

	s3c2xx0_update_intr_masks(irqno, level);

	/*
	 * set trigger type for external interrupts 0..3
	 */
	if (irqno <= S3C24X0_INT_EXT(3)) {
		/*
		 * Update external interrupt control
		 */
		s3c2410_setup_extint(irqno, type);
	}

	s3c2xx0_setipl(s3c2xx0_curcpl());

	restore_interrupts(save);

	return &handler[irqno];
}


static void
init_interrupt_masks(void)
{
	int i;

	for (i=0; i < NIPL; ++i)
		s3c2xx0_imask[i] = 0;

}

void
s3c2410_intr_init(struct s3c24x0_softc *sc)
{
	intctl_base = (vaddr_t) bus_space_vaddr(sc->sc_sx.sc_iot,
	    sc->sc_sx.sc_intctl_ioh);

	s3c2xx0_intr_mask_reg = (uint32_t *)(intctl_base + INTCTL_INTMSK);

	/* clear all pending interrupt */
	icreg(INTCTL_SRCPND) = ~0;
	icreg(INTCTL_INTPND) = ~0;

	/* mask all sub interrupts */
	icreg(INTCTL_INTSUBMSK) = 0x7ff;

	init_interrupt_masks();

	s3c2xx0_intr_init(handler, ICU_LEN);

}


/*
 * mask/unmask sub interrupts
 */
void
s3c2410_mask_subinterrupts(int bits)
{
	int psw = disable_interrupts(I32_bit|F32_bit);
	icreg(INTCTL_INTSUBMSK) |= bits;
	restore_interrupts(psw);
}

void
s3c2410_unmask_subinterrupts(int bits)
{
	int psw = disable_interrupts(I32_bit|F32_bit);
	icreg(INTCTL_INTSUBMSK) &= ~bits;
	restore_interrupts(psw);
}

/*
 * Update external interrupt control
 */
static const u_char s3c24x0_ist[] = {
	EXTINTR_LOW,		/* NONE */
	EXTINTR_FALLING,	/* PULSE */
	EXTINTR_FALLING,	/* EDGE */
	EXTINTR_LOW,		/* LEVEL */
	EXTINTR_HIGH,
	EXTINTR_RISING,
	EXTINTR_BOTH,
};

void
s3c2410_setup_extint(int extint, int type)
{
        uint32_t reg;
        u_int   trig;
        int     i = extint % 8;
        int     regidx = extint/8;      /* GPIO_EXTINT[0:2] */
	int	save;

        trig = s3c24x0_ist[type];

	save = disable_interrupts(I32_bit);

        reg = bus_space_read_4(s3c2xx0_softc->sc_iot,
            s3c2xx0_softc->sc_gpio_ioh,
            GPIO_EXTINT(regidx));

        reg = reg & ~(0x07 << (4*i));
        reg |= trig << (4*i);

        bus_space_write_4(s3c2xx0_softc->sc_iot, s3c2xx0_softc->sc_gpio_ioh,
            GPIO_EXTINT(regidx), reg);

	restore_interrupts(save);
}
