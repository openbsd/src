/*	$OpenBSD: apic.c,v 1.1 2007/05/21 22:43:38 kettenis Exp $	*/

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

#ifdef DEBUG
	apic_dump(sc);
#endif
}

int
apic_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t reg;

	reg = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
#ifdef DEBUG
	printf(" pin=%d line=%d ", PCI_INTERRUPT_PIN(reg),
	    PCI_INTERRUPT_LINE(reg));
#endif
	*ihp = PCI_INTERRUPT_LINE(reg) + 1;
	return (*ihp == 0);
}

const char *
apic_intr_string(void *v, pci_intr_handle_t ih)
{
	static char buf[32];

	snprintf(buf, 32, "irq %ld", (ih + 8 - 1));

	return (buf);
}

void *
apic_intr_establish(void *v, pci_intr_handle_t ih,
    int pri, int (*handler)(void *), void *arg, char *name)
{
	struct elroy_softc *sc = v;
	volatile struct elroy_regs *r = sc->sc_regs;
	hppa_hpa_t hpa = cpu_gethpa(0);
	u_int32_t ent0;
	struct apic_iv *aiv, *biv;
	void *iv;

	/* no mapping or bogus */
	if (ih <= 0 || ih > 31)
		return (NULL);

	aiv = malloc(sizeof(struct apic_iv), M_DEVBUF, M_NOWAIT);
	if (aiv == NULL)
		return NULL;

	aiv->sc = sc;
	aiv->ih = ih;
	aiv->handler = handler;
	aiv->arg = arg;
	aiv->next = NULL;
	if (apic_intr_list[(ih + 8 - 1)]) {
		biv = apic_intr_list[(ih + 8 - 1)];
		while (biv->next)
			biv = biv->next;
		biv->next = aiv;
		return (arg);
	}

	if ((iv = cpu_intr_establish(pri, (ih + 8 - 1), apic_intr, aiv, name))) {
		ent0 = (31 - (ih + 8 - 1)) & APIC_ENT0_VEC;
		ent0 |= APIC_ENT0_LOW;
		ent0 |= APIC_ENT0_LEV;
#if 0
		if (cold) {
			sc->sc_imr |= (1 << (ih + 8 - 1));
			ent0 |= APIC_ENT0_MASK;
		}
#endif
		apic_write(sc->sc_regs, APIC_ENT0(ih - 1), APIC_ENT0_MASK);
		apic_write(sc->sc_regs, APIC_ENT1(ih - 1),
		    ((hpa & 0x0ff00000) >> 4) | ((hpa & 0x000ff000) << 12));
		apic_write(sc->sc_regs, APIC_ENT0(ih - 1), ent0);

		elroy_write32(&r->apic_eoi, htole32((31 - (ih + 8 - 1)) & APIC_ENT0_VEC));

		apic_intr_list[(ih + 8 - 1)] = aiv;
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

	elroy_write32(&r->apic_eoi, htole32((31 - (iv->ih + 8 - 1)) & APIC_ENT0_VEC));

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
