/*	$OpenBSD: eisa_machdep.c,v 1.4 2015/09/08 10:21:50 deraadt Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/bus.h>
#include <sgi/localbus/intreg.h>
#include <sgi/localbus/intvar.h>

#include <dev/ic/i8259reg.h>
#include <dev/isa/isareg.h>
#include <dev/eisa/eisavar.h>

#define	IO_EXTNMI	0x0461
#define	IO_ELCR0	0x04d0
#define	IO_ELCR1	0x04d1

#define	IRQ_CASCADE	2

int	eisa_intr(void *);

/*
 * Magic registers (from IRIX IP22.h). They appear in EISA I/O space.
 */

#define	EIU_MODE_REG	0x0001ffc0
#define	EIU_STAT_REG	0x0001ffc4
#define	EIU_PREMPT_REG	0x0001ffc8
#define	EIU_QUIET_REG	0x0001ffcc
#define	EIU_INTRPT_ACK	0x00010004

#define	eiu_read(o) \
	*(volatile uint32_t *)PHYS_TO_XKPHYS(EISA_IO_BASE | (o), CCA_NC)
#define	eiu_write(o,v) \
	*(volatile uint32_t *)PHYS_TO_XKPHYS(EISA_IO_BASE | (o), CCA_NC) = (v)

#define	eisa_io_read(o) \
	*(volatile uint8_t *)PHYS_TO_XKPHYS(EISA_IO_BASE | (o), CCA_NC)
#define	eisa_io_write(o,v) \
	*(volatile uint8_t *)PHYS_TO_XKPHYS(EISA_IO_BASE | (o), CCA_NC) = (v)

#define	EISA_INT2_IRQNO	INT2_MAP1_INTR(INT2_MAP_EISA)

/*
 * EISA interrupt handlers.
 */

#define	EISA_NINTS	16

struct eisa_intrhand {
	SLIST_ENTRY(eisa_intrhand)	  ei_list;
	int				(*ei_fn)(void *);
	void				 *ei_arg;
	int				  ei_type;

	int				  ei_intr;
	struct evcount			  ei_evcnt;
};

SLIST_HEAD(, eisa_intrhand) eisa_ih[EISA_NINTS];

int
eisa_intr_map(eisa_chipset_tag_t ec, u_int irq, eisa_intr_handle_t *ihp)
{
	*ihp = -1;

	if (irq >= EISA_NINTS)
		return 1;

	if (irq == IRQ_CASCADE)	/* cascade */
		irq = 9;

	*ihp = irq;
	return 0;
}

const char *
eisa_intr_string(eisa_chipset_tag_t ec, eisa_intr_handle_t ih)
{
	static char irqstr[32];

	snprintf(irqstr, sizeof irqstr, "eisa irq %d", ih);
	return irqstr;
}

void *
eisa_intr_establish(eisa_chipset_tag_t ec, eisa_intr_handle_t ih, int type,
    int level, int (*func)(void *), void *arg, char *what)
{
	struct eisa_intrhand *first = NULL, *eih;

	if (ih >= EISA_NINTS || ih == IRQ_CASCADE)
		return NULL;

	if (!SLIST_EMPTY(&eisa_ih[ih])) {
		first = SLIST_FIRST(&eisa_ih[ih]);
		/* can't share edge and level interrupts */
		if (first->ei_type != type)
			return NULL;
	}

	eih = malloc(sizeof(*eih), M_DEVBUF, M_NOWAIT);
	if (eih == NULL)
		return NULL;

	eih->ei_fn = func;
	eih->ei_arg = arg;
	eih->ei_type = type;
	eih->ei_intr = ih;
	evcount_attach(&eih->ei_evcnt, what, &eih->ei_intr);

	if (first == NULL) {
		/* Update ELCR */
		if (type == IST_LEVEL) {
			if (ih >= 8)
				eisa_io_write(IO_ELCR1,
				    eisa_io_read(IO_ELCR1) | (1 << (ih & 7)));
			else
				eisa_io_write(IO_ELCR0,
				    eisa_io_read(IO_ELCR0) | (1 << ih));
		}
		/* Update OCW */
		if (ih >= 8)
			eisa_io_write(IO_ICU2 + PIC_OCW1,
			    eisa_io_read(IO_ICU2 + PIC_OCW1) & ~(1 << (ih & 7)));
		else
			eisa_io_write(IO_ICU1 + PIC_OCW1,
			    eisa_io_read(IO_ICU1 + PIC_OCW1) & ~(1 << ih));
	}

	SLIST_INSERT_HEAD(&eisa_ih[ih], eih, ei_list);

	return eih;
}

void
eisa_intr_disestablish(eisa_chipset_tag_t ec, void *cookie)
{
	struct eisa_intrhand *eih = (struct eisa_intrhand *)cookie;
	unsigned int ih = (unsigned int)eih->ei_intr;

#ifdef DIAGNOSTIC
	if (ih >= EISA_NINTS || ih == IRQ_CASCADE)
		return;
#endif

	SLIST_REMOVE(&eisa_ih[ih], eih, eisa_intrhand, ei_list);

	if (SLIST_EMPTY(&eisa_ih[ih])) {
		/* Reset ELCR */
		if (ih >= 8)
			eisa_io_write(IO_ELCR1,
			    eisa_io_read(IO_ELCR1) & ~(1 << (ih & 7)));
		else
			eisa_io_write(IO_ELCR0,
			    eisa_io_read(IO_ELCR0) & ~(1 << ih));
		/* Update OCW */
		if (ih >= 8)
			eisa_io_write(IO_ICU2 + PIC_OCW1,
			    eisa_io_read(IO_ICU2 + PIC_OCW1) | (1 << (ih & 7)));
		else
			eisa_io_write(IO_ICU1 + PIC_OCW1,
			    eisa_io_read(IO_ICU1 + PIC_OCW1) | (1 << ih));
	}

	evcount_detach(&eih->ei_evcnt);
	free(eih, M_DEVBUF, sizeof *eih);
}

int
eisa_intr(void *arg)
{
	struct device *dv = (struct device *)arg;
	struct eisa_intrhand *eih;
	uint irq;
	int rc, handled;

	irq = eisa_io_read(EIU_INTRPT_ACK);
	if (irq >= EISA_NINTS) {
		/* anything better to do? */
		panic("%s: unexpected irq value %08x", dv->dv_xname, irq);
		return -1;
	}

	handled = 0;
	SLIST_FOREACH(eih, &eisa_ih[irq], ei_list) {
		rc = (*eih->ei_fn)(eih->ei_arg);
		if (rc != 0) {
			handled = 1;
			eih->ei_evcnt.ec_count++;
			if (rc > 0)
				break;
		}
	}
	if (handled == 0)
		printf("%s: spurious irq %d\n", dv->dv_xname, irq);

	if (irq >= 8)
		eisa_io_write(IO_ICU2 + PIC_OCW2, OCW2_EOI);
	eisa_io_write(IO_ICU1 + PIC_OCW2, OCW2_EOI);

	return handled;
}

void
eisa_attach_hook(struct device *parent, struct device *self,
    struct eisabus_attach_args *eba)
{
	unsigned int irq;

	/*
	 * Unlock the bus. Magic sequence taken from Linux.
	 */

	eiu_write(EIU_PREMPT_REG, 0x0000ffff);
	eiu_write(EIU_QUIET_REG, 1);
	eiu_write(EIU_MODE_REG, 0x40f3c07f);

	/*
	 * Reset the bus.
	 */

	eisa_io_write(IO_EXTNMI, 0x01);
	delay(10000);
	eisa_io_write(IO_EXTNMI, 0x00);
	/* some boards need enough time to reset, or they won't probe */
	delay(250000);

	/*
	 * Setup i8259...
	 */

	/* Program PIC1. */
	eisa_io_write(IO_ICU1 + PIC_ICW1, ICW1_SELECT | ICW1_IC4);
	eisa_io_write(IO_ICU1 + PIC_ICW2, ICW2_VECTOR(0));
	eisa_io_write(IO_ICU1 + PIC_ICW3, ICW3_CASCADE(IRQ_CASCADE));
	eisa_io_write(IO_ICU1 + PIC_ICW4, ICW4_8086);
	eisa_io_write(IO_ICU1 + PIC_OCW1, 0xff);

	/* Program PIC2. */
	eisa_io_write(IO_ICU2 + PIC_ICW1, ICW1_SELECT | ICW1_IC4);
	eisa_io_write(IO_ICU2 + PIC_ICW2, ICW2_VECTOR(8));
	eisa_io_write(IO_ICU2 + PIC_ICW3, ICW3_SIC(IRQ_CASCADE));
	eisa_io_write(IO_ICU2 + PIC_ICW4, ICW4_8086);
	eisa_io_write(IO_ICU2 + PIC_OCW1, 0xff);

	/* Interrupts are edge-triggered by default. */
	eisa_io_write(IO_ELCR0, 0x00);
	eisa_io_write(IO_ELCR1, 0x00);

	/* Unmask the cascade interrupt */
	eisa_io_write(IO_ICU1 + PIC_OCW1, 0xff ^ (1 << IRQ_CASCADE));

	/*
	 * Setup interrupt handling.
	 */

	for (irq = 0; irq < EISA_NINTS; irq++)
		SLIST_INIT(&eisa_ih[irq]);

	int2_intr_establish(EISA_INT2_IRQNO, IPL_TTY, eisa_intr, self,
	    self->dv_xname);
	printf(" irq %d", EISA_INT2_IRQNO);
}

int
eisa_maxslots(eisa_chipset_tag_t ec)
{
	/*
	 * Indigo2 systems can only have up to four slots (and only three
	 * for Impact models), but slot numbering starts at 1 (#0 being
	 * the host).
	 */
	return 1 + 4;
}
