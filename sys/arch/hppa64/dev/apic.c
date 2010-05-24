/*	$OpenBSD: apic.c,v 1.4 2010/05/24 15:06:03 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
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
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define	APIC_DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <hppa64/dev/elroyreg.h>
#include <hppa64/dev/elroyvar.h>


void
apic_write(volatile struct elroy_regs *r, u_int32_t reg, u_int32_t val)
{
	elroy_write32(&r->apic_addr, htole32(reg));
	elroy_write32(&r->apic_data, htole32(val));
}

u_int32_t
apic_read(volatile struct elroy_regs *r, u_int32_t reg)
{
	elroy_write32(&r->apic_addr, htole32(reg));
	return letoh32(elroy_read32(&r->apic_data));
}

void		apic_write(volatile struct elroy_regs *r, u_int32_t reg,
		    u_int32_t val);
u_int32_t	apic_read(volatile struct elroy_regs *r, u_int32_t reg);

void
apic_attach(struct elroy_softc *sc)
{
	volatile struct elroy_regs *r = sc->sc_regs;
	u_int32_t data;

	data = apic_read(r, APIC_VERSION);
	sc->sc_nints = (data & APIC_VERSION_NENT) >> APIC_VERSION_NENT_SHIFT;
	printf(" APIC ver %x, %d pins",
	    data & APIC_VERSION_MASK, sc->sc_nints);
}

int
apic_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	struct elroy_softc *sc = pc->_cookie;
	pcitag_t tag = pa->pa_tag;
	hppa_hpa_t hpa = cpu_gethpa(0);
	pcireg_t reg;

	reg = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
printf(" pin=%d line=%d ", PCI_INTERRUPT_PIN(reg), PCI_INTERRUPT_LINE(reg));
	apic_write(sc->sc_regs, APIC_ENT0(PCI_INTERRUPT_PIN(reg)),
	    PCI_INTERRUPT_LINE(reg));
	apic_write(sc->sc_regs, APIC_ENT1(PCI_INTERRUPT_PIN(reg)),
	    ((hpa & 0x0ff00000) >> 4) | ((hpa & 0x000ff000) << 12));
	*ihp = PCI_INTERRUPT_LINE(reg) + 1;
	return (*ihp == 0);
}

const char *
apic_intr_string(void *v, pci_intr_handle_t ih)
{
	static char buf[32];

	snprintf(buf, 32, "irq %ld", ih);

	return (buf);
}

void *
apic_intr_establish(void *v, pci_intr_handle_t ih,
    int pri, int (*handler)(void *), void *arg, const char *name)
{
	/* struct elroy_softc *sc = v; */
	/* volatile struct elroy_regs *r = sc->sc_regs; */
	/* void *iv = NULL; */

	/* no mapping or bogus */
	if (ih <= 0 || ih > 63)
		return (NULL);

#if 0
TODO
	if ((iv = cpu_intr_map(sc->sc_ih, pri, ih - 1, handler, arg, name))) {
		if (cold)
			sc->sc_imr |= (1 << (ih - 1));
		else
			/* r->imr = sc->sc_imr |= (1 << (ih - 1)) */;
	}
#endif

	return (arg);
}

void
apic_intr_disestablish(void *v, void *cookie)
{
#if 0
	struct elroy_softc *sc = v;
	volatile struct elroy_regs *r = sc->sc_regs;

	r->imr &= ~(1 << (ih - 1));

	TODO cpu_intr_unmap(sc->sc_ih, cookie);
#endif
}

int
apic_intr(void *v)
{

	return (0);
}
