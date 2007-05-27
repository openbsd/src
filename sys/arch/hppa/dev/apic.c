/*	$OpenBSD: apic.c,v 1.2 2007/05/27 16:36:07 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * Copyright (c) 2007 Mark Kettenis
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/pdc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <hppa/dev/elroyreg.h>
#include <hppa/dev/elroyvar.h>

#define APIC_INT_LINE_MASK	0x0000ff00
#define APIC_INT_LINE_SHIFT	8
#define APIC_INT_IRQ_MASK	0x0000001f

#define APIC_INT_LINE(x) (((x) & APIC_INT_LINE_MASK) >> APIC_INT_LINE_SHIFT)
#define APIC_INT_IRQ(x) ((x) & APIC_INT_IRQ_MASK)

struct apic_iv {
	struct elroy_softc *sc;
	pci_intr_handle_t ih;
	int (*handler)(void *);
	void *arg;
	struct apic_iv *next;
};

struct apic_iv *apic_intr_list[CPU_NINTS];

#ifdef DEBUG
void	apic_dump(struct elroy_softc *);
#endif

void
apic_write(volatile struct elroy_regs *r, u_int32_t reg, u_int32_t val)
{
	elroy_write32(&r->apic_addr, htole32(reg));
	elroy_write32(&r->apic_data, htole32(val));
	elroy_read32(&r->apic_data);
}

u_int32_t
apic_read(volatile struct elroy_regs *r, u_int32_t reg)
{
	elroy_write32(&r->apic_addr, htole32(reg));
	return letoh32(elroy_read32(&r->apic_data));
}

void
apic_attach(struct elroy_softc *sc)
{
	volatile struct elroy_regs *r = sc->sc_regs;
	u_int32_t data;

	data = apic_read(r, APIC_VERSION);
	sc->sc_nints = (data & APIC_VERSION_NENT) >> APIC_VERSION_NENT_SHIFT;
	printf(" APIC ver %x, %d pins",
	    data & APIC_VERSION_MASK, sc->sc_nints);

	sc->sc_irq = malloc(sc->sc_nints * sizeof(int), M_DEVBUF, M_NOWAIT);
	if (sc->sc_irq == NULL)
		panic("apic_attach: cannot allocate irq table\n");
	memset(sc->sc_irq, 0, sc->sc_nints * sizeof(int));

#ifdef DEBUG
	apic_dump(sc);
#endif
}

int
apic_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct elroy_softc *sc = pa->pa_pc->_cookie;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t reg;
	int line;

	reg = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
#ifdef DEBUG
	printf(" pin=%d line=%d ", PCI_INTERRUPT_PIN(reg),
	    PCI_INTERRUPT_LINE(reg));
#endif
	line = PCI_INTERRUPT_LINE(reg);
	if (sc->sc_irq[line] == 0)
		sc->sc_irq[line] = cpu_intr_findirq();
	*ihp = (line << APIC_INT_LINE_SHIFT) | sc->sc_irq[line];
	return (APIC_INT_IRQ(*ihp) == 0);
}

const char *
apic_intr_string(void *v, pci_intr_handle_t ih)
{
	static char buf[32];

	snprintf(buf, 32, "line %ld irq %ld",
	    APIC_INT_LINE(ih), APIC_INT_IRQ(ih));

	return (buf);
}

void *
apic_intr_establish(void *v, pci_intr_handle_t ih,
    int pri, int (*handler)(void *), void *arg, char *name)
{
	struct elroy_softc *sc = v;
	volatile struct elroy_regs *r = sc->sc_regs;
	hppa_hpa_t hpa = cpu_gethpa(0);
	struct apic_iv *aiv, *biv;
	void *iv;
	int irq = APIC_INT_IRQ(ih);
	int line = APIC_INT_LINE(ih);
	u_int32_t ent0;

	/* no mapping or bogus */
	if (irq <= 0 || irq > 31)
		return (NULL);

	aiv = malloc(sizeof(struct apic_iv), M_DEVBUF, M_NOWAIT);
	if (aiv == NULL)
		return NULL;

	aiv->sc = sc;
	aiv->ih = ih;
	aiv->handler = handler;
	aiv->arg = arg;
	aiv->next = NULL;
	if (apic_intr_list[irq]) {
		biv = apic_intr_list[irq];
		while (biv->next)
			biv = biv->next;
		biv->next = aiv;
		return (arg);
	}

	if ((iv = cpu_intr_establish(pri, irq, apic_intr, aiv, name))) {
		ent0 = (31 - irq) & APIC_ENT0_VEC;
		ent0 |= APIC_ENT0_LOW;
		ent0 |= APIC_ENT0_LEV;
#if 0
		if (cold) {
			sc->sc_imr |= (1 << irq);
			ent0 |= APIC_ENT0_MASK;
		}
#endif
		apic_write(sc->sc_regs, APIC_ENT0(line), APIC_ENT0_MASK);
		apic_write(sc->sc_regs, APIC_ENT1(line),
		    ((hpa & 0x0ff00000) >> 4) | ((hpa & 0x000ff000) << 12));
		apic_write(sc->sc_regs, APIC_ENT0(line), ent0);

		/* Signal EOI. */
		elroy_write32(&r->apic_eoi,
		    htole32((31 - irq) & APIC_ENT0_VEC));

		apic_intr_list[irq] = aiv;
	}

	return (arg);
}

void
apic_intr_disestablish(void *v, void *cookie)
{
}

int
apic_intr(void *v)
{
	struct apic_iv *iv = v;
	struct elroy_softc *sc = iv->sc;
	volatile struct elroy_regs *r = sc->sc_regs;
	int claimed = 0;

	while (iv) {
		claimed |= iv->handler(iv->arg);
		iv = iv->next;
	}

	/* Signal EOI. */
	elroy_write32(&r->apic_eoi,
	    htole32((31 - APIC_INT_IRQ(iv->ih)) & APIC_ENT0_VEC));

	return (claimed);
}

#ifdef DEBUG
void
apic_dump(struct elroy_softc *sc)
{
	struct pdc_pat_io_num int_tbl_sz PDC_ALIGNMENT;
	struct pdc_pat_pci_rt int_tbl[5] PDC_ALIGNMENT;
	int i;

	for (i = 0; i < sc->sc_nints; i++)
		printf("0x%04x 0x%04x\n", apic_read(sc->sc_regs, APIC_ENT0(i)),
		    apic_read(sc->sc_regs, APIC_ENT1(i)));

	if (pdc_call((iodcio_t)pdc, 0, PDC_PCI_INDEX, PDC_PCI_GET_INT_TBL_SZ,
	    &int_tbl_sz, 0, 0, 0, 0, 0))
		printf("pdc_call failed\n");
	printf("int_tbl_sz=%d\n", int_tbl_sz.num);
	
	if (pdc_call((iodcio_t)pdc, 0, PDC_PCI_INDEX, PDC_PCI_GET_INT_TBL,
	    &int_tbl_sz, 0, &int_tbl, 0, 0, 0))
		printf("pdc_call failed\n");
	for (i = 0; i < 5; i++) {
		printf("type=%x, len=%d ", int_tbl[i].type, int_tbl[i].len);
		printf("itype=%d, trigger=%x ", int_tbl[i].itype, int_tbl[i].trigger);			
		printf("pin=%x, bus=%d ", int_tbl[i].pin, int_tbl[i].bus);			
		printf("line=%d, addr=%x\n", int_tbl[i].line, int_tbl[i].addr);			
	}
}
#endif
