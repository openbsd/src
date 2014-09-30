/*	$OpenBSD: int.c,v 1.11 2014/09/30 06:51:58 jmatthew Exp $	*/
/*	$NetBSD: int.c,v 1.24 2011/07/01 18:53:46 dyoung Exp $	*/

/*
 * Copyright (c) 2009 Stephen M. Rumble 
 * Copyright (c) 2004 Christopher SEKIYA
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * INT2 (IP20, IP22) / INT3 (IP24) interrupt controllers
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/intr.h>

#include <dev/ic/i8253reg.h>

#include <sgi/localbus/intreg.h>
#include <sgi/localbus/intvar.h>
#include <sgi/sgi/ip22.h>

int	int2_match(struct device *, void *, void *);
void	int2_attach(struct device *, struct device *, void *);
int 	int2_mappable_intr(void *);

const struct cfattach int_ca = {
	sizeof(struct device), int2_match, int2_attach
};

struct cfdriver int_cd = {
	NULL, "int", DV_DULL
};

paddr_t	int2_base;
paddr_t	int2_get_base(void);

#define	int2_read(r)		*(volatile uint8_t *)(int2_base + (r))
#define	int2_write(r, v)	*(volatile uint8_t *)(int2_base + (r)) = (v)

void	int_8254_cal(void);
void	int_8254_startclock(struct cpu_info *);
uint32_t int_8254_intr0(uint32_t, struct trap_frame *);

/*
 * INT2 Interrupt handling declarations: 16 local sources on 2 levels.
 *
 * In addition to this, INT3 provides 8 so-called mappable interrupts, which
 * are cascaded to either one of the unused two INT2 VME interrupts.
 * To make things easier from a software viewpoint, we pretend there are
 * 16 of them - one set of 8 per cascaded interrupt. This allows for
 * faster recognition on where to connect these interrupts - as long as
 * interrupt vector assignment makes sure no mappable interrupt is
 * registered on both cascaded interrupts.
 */

struct int2_intrhand {
	struct intrhand	ih;
	uint32_t	flags;
#define	IH_FL_DISABLED	0x01
};

#define	INT2_NINTS	(8 + 8 + 2 * 8)
struct int2_intrhand *int2_intrhand[INT2_NINTS];

uint32_t int2_intem;
uint8_t	int2_l0imask[NIPLS], int2_l1imask[NIPLS];

void	int2_splx(int);
uint32_t int2_l0intr(uint32_t, struct trap_frame *);
void	int2_l0makemasks(void);
uint32_t int2_l1intr(uint32_t, struct trap_frame *);
void	int2_l1makemasks(void);

/*
 * Level 0 interrupt handler.
 */

uint32_t save_l0imr, save_l0isr, save_l0ipl;
#define	INTR_FUNCTIONNAME	int2_l0intr
#define	MASK_FUNCTIONNAME	int2_l0makemasks

#define	INTR_LOCAL_DECLS
#define	MASK_LOCAL_DECLS
#define	INTR_GETMASKS \
do { \
	isr = int2_read(INT2_LOCAL0_STATUS); \
	imr = int2_read(INT2_LOCAL0_MASK); \
	bit = 7; \
save_l0isr = isr; save_l0imr = imr; save_l0ipl = frame->ipl; \
} while (0)
#define	INTR_MASKPENDING \
	int2_write(INT2_LOCAL0_MASK, imr & ~isr)
#define	INTR_IMASK(ipl)		int2_l0imask[ipl]
#define	INTR_HANDLER(bit)	(struct intrhand *)int2_intrhand[bit + 0]
#define	INTR_SPURIOUS(bit) \
do { \
	printf("spurious int2 interrupt %d\n", bit); \
} while (0)
/* explicit masking with int2_intem to cope with handlers disabling themselves */
#define	INTR_MASKRESTORE \
	int2_write(INT2_LOCAL0_MASK, int2_intem & imr)
#define	INTR_MASKSIZE	8

#define	INTR_HANDLER_SKIP(ih) \
	(((struct int2_intrhand *)(ih))->flags /* & IH_FL_DISABLED */)

#include <sgi/sgi/intr_template.c>

/*
 * Level 1 interrupt handler.
 */

uint32_t save_l1imr, save_l1isr, save_l1ipl;
#define	INTR_FUNCTIONNAME	int2_l1intr
#define	MASK_FUNCTIONNAME	int2_l1makemasks

#define	INTR_LOCAL_DECLS
#define	MASK_LOCAL_DECLS
#define	INTR_GETMASKS \
do { \
	isr = int2_read(INT2_LOCAL1_STATUS); \
	imr = int2_read(INT2_LOCAL1_MASK); \
	bit = 7; \
save_l1isr = isr; save_l1imr = imr; save_l1ipl = frame->ipl; \
} while (0)
#define	INTR_MASKPENDING \
	int2_write(INT2_LOCAL1_MASK, imr & ~isr)
#define	INTR_IMASK(ipl)		int2_l1imask[ipl]
#define	INTR_HANDLER(bit)	(struct intrhand *)int2_intrhand[bit + 8]
#define	INTR_SPURIOUS(bit) \
do { \
	printf("spurious int2 interrupt %d\n", bit + 8); \
} while (0)
/* explicit masking with int2_intem to cope with handlers disabling themselves */
#define	INTR_MASKRESTORE \
	int2_write(INT2_LOCAL1_MASK,  (int2_intem >> 8) & imr)
#define	INTR_MASKSIZE	8

#define	INTR_HANDLER_SKIP(ih) \
	(((struct int2_intrhand *)(ih))->flags /* & IH_FL_DISABLED */)

#include <sgi/sgi/intr_template.c>

void *
int2_intr_establish(int irq, int level, int (*ih_fun) (void *),
    void *ih_arg, const char *ih_what)
{
	struct int2_intrhand **p, *q, *ih;
	int s;

#ifdef DIAGNOSTIC
	if (irq < 0 || irq >= INT2_NINTS)
		panic("int2_intr_establish: illegal irq %d", irq);
	/* Mappable interrupts can't be above IPL_TTY */
	if ((irq >> 3) >= 2 && level > IPL_TTY)
		return NULL;
#endif

	ih = malloc(sizeof *ih, M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return NULL;

	ih->ih.ih_next = NULL;
	ih->ih.ih_fun = ih_fun;
	ih->ih.ih_arg = ih_arg;
	ih->ih.ih_level = level;
	ih->ih.ih_irq = irq;
	if (ih_what != NULL)
		evcount_attach(&ih->ih.ih_count, ih_what, &ih->ih.ih_irq);
	ih->flags = 0;

	s = splhigh();

	for (p = &int2_intrhand[irq]; (q = *p) != NULL;
	    p = (struct int2_intrhand **)&q->ih.ih_next)
		continue;
	*p = ih;

	int2_intem |= 1 << irq;
	switch (irq >> 3) {
	case 0:
		int2_l0makemasks();
		break;
	case 1:
		int2_l1makemasks();
		break;
	/*
	 * We do not maintain masks for mappable interrupts. They are
	 * masked as a whole, by the level 0 or 1 interrupt they cascade to.
	 */
	case 2:
		int2_write(INT2_IP22_MAP_MASK0,
		    int2_read(INT2_IP22_MAP_MASK0) | (1 << (irq & 7)));
		break;
	case 3:
		int2_write(INT2_IP22_MAP_MASK1,
		    int2_read(INT2_IP22_MAP_MASK1) | (1 << (irq & 7)));
		break;
	}

	splx(s);	/* will cause hardware mask update */

	return ih;
}

void
int2_splx(int newipl)
{
	struct cpu_info *ci = curcpu();
	register_t sr;

	__asm__ (".set noreorder");
	ci->ci_ipl = newipl;
	mips_sync();
	__asm__ (".set reorder\n");

	sr = disableintr();	/* XXX overkill? */
	int2_write(INT2_LOCAL1_MASK, (int2_intem >> 8) & ~int2_l1imask[newipl]);
	int2_write(INT2_LOCAL0_MASK, int2_intem & ~int2_l0imask[newipl]);
	setsr(sr);

	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

/*
 * Mappable interrupts handler.
 */

int
int2_mappable_intr(void *arg)
{
	uint which = (unsigned long)arg;
	vaddr_t imrreg;
	uint64_t imr, isr;
	uint i, intnum;
	struct int2_intrhand *ih;
	int rc, ret;

	imrreg = which == 0 ? INT2_IP22_MAP_MASK0 : INT2_IP22_MAP_MASK1;
	isr = int2_read(INT2_IP22_MAP_STATUS);
	imr = int2_read(imrreg);

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Don't bother masking sources here - all mappable interrupts are
	 * tied to either a level 1 or level 0 interrupt, and the dispatcher
	 * is registered at IPL_TTY, so we can safely assume we are running
	 * at IPL_TTY now.
	 */
	for (i = 0; i < 8; i++) {
		intnum = i + 16 + (which << 3);
		if (isr & (1 << i)) {
			rc = 0;
			for (ih = int2_intrhand[intnum]; ih != NULL;
			    ih = (struct int2_intrhand *)ih->ih.ih_next) {
				if (ih->flags /* & IH_FL_DISABLED */)
					continue;
				ret = (*ih->ih.ih_fun)(ih->ih.ih_arg);
				if (ret != 0) {
					rc = 1;
					atomic_inc_long((unsigned long *)
					    &ih->ih.ih_count.ec_count);
				}
				if (ret == 1)
					break;
			}
			if (rc == 0)
				printf("spurious int2 mapped interrupt %d\n",
				    i);
		}
	}

	return 1;
}

int
int2_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *maa = (void *)aux;

	switch (sys_config.system_type) {
	case SGI_IP20:
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
		break;
	default:
		return 0;
	}

	return !strcmp(maa->maa_name, int_cd.cd_name);
}

void
int2_attach(struct device *parent, struct device *self, void *aux)
{
	if (int2_base == 0)
		int2_base = int2_get_base();

	printf(" addr 0x%lx\n", XKPHYS_TO_PHYS(int2_base));

	/* Clean out interrupt masks */
	int2_write(INT2_LOCAL0_MASK, 0);
	int2_write(INT2_LOCAL1_MASK, 0);
	int2_write(INT2_IP22_MAP_MASK0, 0);
	int2_write(INT2_IP22_MAP_MASK1, 0);

	/* Reset timer interrupts */
	int2_write(INT2_TIMER_CONTROL,
	    TIMER_SEL0 | TIMER_16BIT | TIMER_SWSTROBE);
	int2_write(INT2_TIMER_CONTROL,
	    TIMER_SEL1 | TIMER_16BIT | TIMER_SWSTROBE);
	int2_write(INT2_TIMER_CONTROL,
	    TIMER_SEL2 | TIMER_16BIT | TIMER_SWSTROBE);
	mips_sync();
	delay(4);
	int2_write(INT2_TIMER_CLEAR, 0x03);

	set_intr(INTPRI_L1, CR_INT_1, int2_l1intr);
	set_intr(INTPRI_L0, CR_INT_0, int2_l0intr);
	register_splx_handler(int2_splx);

	if (sys_config.system_type != SGI_IP20) {
		/* Wire mappable interrupt handlers */
		int2_intr_establish(INT2_L0_INTR(INT2_L0_IP22_MAP0), IPL_TTY,
		    int2_mappable_intr, (void *)0, NULL);
		int2_intr_establish(INT2_L1_INTR(INT2_L1_IP22_MAP1), IPL_TTY,
		    int2_mappable_intr, (void *)1, NULL);
	}

	/*
	 * The 8254 timer does not interrupt on (some?) IP24 systems.
	 */
	if (sys_config.system_type == SGI_IP20 ||
	    sys_config.system_subtype == IP22_INDIGO2)
		int_8254_cal();
}

paddr_t
int2_get_base(void)
{
	uint32_t address;

	switch (sys_config.system_type) {
	case SGI_IP20:
		address = INT2_IP20;
		break;
	default:
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
		if (sys_config.system_subtype == IP22_INDIGO2)
			address = INT2_IP22;
		else
			address = INT2_IP24;
		break;
	}

	return PHYS_TO_XKPHYS((uint64_t)address, CCA_NC);
}

/*
 * Returns nonzero if the given interrupt source is pending.
 */
int
int2_is_intr_pending(int irq)
{
	paddr_t reg;

	if (int2_base == 0)
		int2_base = int2_get_base();
	switch (irq >> 3) {
	case 0:
		reg = INT2_LOCAL0_STATUS;
		break;
	case 1:
		reg = INT2_LOCAL1_STATUS;
		break;
	case 2:
	case 3:
		reg = INT2_IP22_MAP_STATUS;
		break;
	default:
		return 0;
	}

	return int2_read(reg) & (1 << (irq & 7));
}

/*
 * Temporarily disable an interrupt handler. Note that disable/enable
 * calls can not be stacked.
 *
 * The interrupt source will become masked if it is the only handler.
 * (This is intended for panel(4) which is not supposed to be a shared
 *  interrupt)
 */
void
int2_intr_disable(void *v)
{
	struct int2_intrhand *ih = (struct int2_intrhand *)v;
	int s;

	s = splhigh();
	if ((ih->flags & IH_FL_DISABLED) == 0) {
		ih->flags |= IH_FL_DISABLED;
		if (ih == int2_intrhand[ih->ih.ih_irq] &&
		    ih->ih.ih_next == NULL) {
			/* disable interrupt source */
			int2_intem &= ~(1 << ih->ih.ih_irq);
		}
	}
	splx(s);
}

/*
 * Reenable an interrupt handler.
 */
void
int2_intr_enable(void *v)
{
	struct int2_intrhand *ih = (struct int2_intrhand *)v;
	int s;

	s = splhigh();
	if ((ih->flags & IH_FL_DISABLED) != 0) {
		ih->flags &= ~IH_FL_DISABLED;
		if (ih == int2_intrhand[ih->ih.ih_irq] &&
		    ih->ih.ih_next == NULL) {
			/* reenable interrupt source */
			int2_intem |= 1 << ih->ih.ih_irq;
		}
	}
	splx(s);
}

/*
 * A master clock is wired to TIMER_2, which in turn clocks the two other
 * timers. The master frequency is 1MHz.
 *
 * TIMER_0 and TIMER_1 interrupt on HW_INT_2 and HW_INT_3, respectively.
 *
 * NB: Apparently int2 doesn't like counting down from one, but two works.
 */

static struct evcount int_clock_count;
static int int_clock_irq = 2;

void
int_8254_cal(void)
{
	uint freq = 1000000 / 2 / hz;

	/* Timer0 is our hz. */
	int2_write(INT2_TIMER_CONTROL,
	    TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	int2_write(INT2_TIMER_0, freq & 0xff);
	mips_sync();
	delay(4);
	int2_write(INT2_TIMER_0, freq >> 8);

	/* Timer2 clocks timer0 and timer1. */
	int2_write(INT2_TIMER_CONTROL,
	    TIMER_SEL2 | TIMER_RATEGEN | TIMER_16BIT);
	int2_write(INT2_TIMER_2, 2);
	mips_sync();
	delay(4);
	int2_write(INT2_TIMER_2, 0);

	set_intr(INTPRI_CLOCK, CR_INT_2, int_8254_intr0);
	evcount_attach(&int_clock_count, "clock", &int_clock_irq);
	md_startclock = int_8254_startclock;
}

uint32_t
int_8254_intr0(uint32_t hwpend, struct trap_frame *tf)
{
	struct cpu_info *ci = curcpu();

	int2_write(INT2_TIMER_CLEAR, 0x01);
	ci->ci_pendingticks++;
	if (ci->ci_clock_started != 0) {
		if (tf->ipl < IPL_CLOCK) {
			while (ci->ci_pendingticks) {
				int_clock_count.ec_count++;
				hardclock(tf);
				ci->ci_pendingticks--;
			}
		}
	}

	return hwpend;
}

void
int_8254_startclock(struct cpu_info *ci)
{
	ci->ci_clock_started++;
}
