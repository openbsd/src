/*	$OpenBSD: ssio.c,v 1.3 2007/06/20 17:41:04 kettenis Exp $	*/

/*
 * Copyright (c) 2007 Mark Kettenis
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
 * Driver for the National Semiconductor PC87560 Legacy I/O chip.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <hppa/dev/ssiovar.h>

/* PCI config space. */
#define SSIO_PCI_DMA_RC2	0x64
#define SSIO_PCI_INT_TC1	0x67
#define SSIO_PCI_INT_TC2	0x68
#define SSIO_PCI_INT_RC1	0x69
#define SSIO_PCI_INT_RC2	0x6a
#define SSIO_PCI_INT_RC3	0x6b
#define SSIO_PCI_INT_RC4	0x6c
#define SSIO_PCI_INT_RC5	0x6d
#define SSIO_PCI_INT_RC6	0x6e
#define SSIO_PCI_INT_RC7	0x6f
#define SSIO_PCI_INT_RC8	0x70
#define SSIO_PCI_INT_RC9	0x71
#define SSIO_PCI_SP1BAR		0x94
#define SSIO_PCI_SP2BAR		0x98
#define SSIO_PCI_PPBAR		0x9c

#define SSIO_PCI_INT_TC1_MASK	0xff
#define SSIO_PCI_INT_TC1_SHIFT	24

#define SSIO_PCI_INT_TC2_MASK	0xff
#define SSIO_PCI_INT_TC2_SHIFT	0

#define SSIO_PCI_INT_RC1_MASK	0xff
#define SSIO_PCI_INT_RC1_SHIFT	8

#define SSIO_PCI_INT_RC2_MASK	0xff
#define SSIO_PCI_INT_RC2_SHIFT	16

#define SSIO_PCI_INT_RC3_MASK	0xff
#define SSIO_PCI_INT_RC3_SHIFT	24

#define SSIO_PCI_INT_RC4_MASK	0xff
#define SSIO_PCI_INT_RC4_SHIFT	0

#define SSIO_PCI_INT_RC5_MASK	0xff
#define SSIO_PCI_INT_RC5_SHIFT	8

#define SSIO_PCI_INT_RC6_MASK	0xff
#define SSIO_PCI_INT_RC6_SHIFT	16

#define SSIO_PCI_INT_RC7_MASK	0xff
#define SSIO_PCI_INT_RC7_SHIFT	24

#define SSIO_PCI_INT_RC8_MASK	0xff
#define SSIO_PCI_INT_RC8_SHIFT	0

#define SSIO_PCI_INT_RC9_MASK	0xff
#define SSIO_PCI_INT_RC9_SHIFT	8

/* Cascaded i8259-compatible PICs. */
#define SSIO_PIC1	0x20
#define SSIO_PIC2	0xa0
#define SSIO_NINTS	16

int	ssio_match(struct device *, void *, void *);
void	ssio_attach(struct device *, struct device *, void *);

struct ssio_iv {
	int (*handler)(void *);
	void *arg;
};

struct ssio_iv ssio_intr_table[SSIO_NINTS];

struct ssio_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ic1h;
	bus_space_handle_t sc_ic2h;
	void *sc_ih;
};

struct cfattach ssio_ca = {
	sizeof(struct ssio_softc), ssio_match, ssio_attach
};

struct cfdriver ssio_cd = {
	NULL, "ssio", DV_DULL
};

const struct pci_matchid ssio_devices[] = {
	{ PCI_VENDOR_NS, PCI_PRODUCT_NS_PC87560 }
};

int	ssio_intr(void *);
int	ssio_print(void *, const char *);

int
ssio_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ssio_devices,
	    sizeof(ssio_devices) / sizeof (ssio_devices[0])));
}

void
ssio_attach(struct device *parent, struct device *self, void *aux)
{
	struct ssio_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	struct ssio_attach_args saa;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t reg;

	sc->sc_iot = pa->pa_iot;
	if (bus_space_map(sc->sc_iot, SSIO_PIC1, 2, 0, &sc->sc_ic1h)) {
		printf(": unable to map PIC1 registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, SSIO_PIC2, 2, 0, &sc->sc_ic2h)) {
		printf(": unable to map PIC2 registers\n");
		goto unmap_ic1;
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": unable to map interrupt\n");
		goto unmap_ic2;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	/* XXX Probably should be IPL_NESTED. */
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_TTY, ssio_intr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt\n");
		goto unmap_ic2;
	}

	printf(": %s\n", intrstr);

	/*
	 * We use the following interrupt mapping:
	 *
	 * USB (INTD#)		IRQ 1
	 * Serial Port 1	IRQ 4
	 * Serial Port 2	IRQ 3
	 * Parallel Port	IRQ 5
	 *
	 * USB is set to level triggered, all others to edge triggered.
	 *
	 * We disable all other interrupts since we don't need them.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, SSIO_PCI_DMA_RC2);
	reg &= ~(SSIO_PCI_INT_TC1_MASK << SSIO_PCI_INT_TC1_SHIFT);
	reg |= 0x02 << SSIO_PCI_INT_TC1_SHIFT;
	pci_conf_write(pa->pa_pc, pa->pa_tag, SSIO_PCI_DMA_RC2, reg);

	reg = 0;
	reg |= 0x34 << SSIO_PCI_INT_RC1_SHIFT;	/* SP1, SP2 */
	reg |= 0x05 << SSIO_PCI_INT_RC2_SHIFT;	/* PP */
	pci_conf_write(pa->pa_pc, pa->pa_tag, SSIO_PCI_INT_TC2, reg);

	reg = 0;
	reg |= 0x10 << SSIO_PCI_INT_RC5_SHIFT;	/* INTD# (USB) */
	pci_conf_write(pa->pa_pc, pa->pa_tag, SSIO_PCI_INT_RC4, reg);

	/* Program PIC1. */
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 0, 0x11);
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 1, 0x00);
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 1, 0x04);
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 1, 0x01);

	/* Priority (3-7,0-2). */
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 0, 0xc2);

	/* Program PIC2. */
	bus_space_write_1(sc->sc_iot, sc->sc_ic2h, 0, 0x11);
	bus_space_write_1(sc->sc_iot, sc->sc_ic2h, 1, 0x00);
	bus_space_write_1(sc->sc_iot, sc->sc_ic2h, 1, 0x02);
	bus_space_write_1(sc->sc_iot, sc->sc_ic2h, 1, 0x01);

	/* Unmask all interrupts. */
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 1, 0x00);
	bus_space_write_1(sc->sc_iot, sc->sc_ic2h, 1, 0x00);

	/* Serial Port 1. */
	saa.saa_name = "com";
	saa.saa_iot = sc->sc_iot;
	saa.saa_iobase = pci_conf_read(pa->pa_pc, pa->pa_tag, SSIO_PCI_SP1BAR);
	saa.saa_iobase &= 0xfffffffe;
	saa.saa_irq = 4;
	config_found(self, &saa, ssio_print);

	/* Serial Port 2. */
	saa.saa_name = "com";
	saa.saa_iot = sc->sc_iot;
	saa.saa_iobase = pci_conf_read(pa->pa_pc, pa->pa_tag, SSIO_PCI_SP2BAR);
	saa.saa_iobase &= 0xfffffffe;
	saa.saa_irq = 3;
	config_found(self, &saa, ssio_print);

	/* Parallel Port. */
	saa.saa_name = "lpt";
	saa.saa_iot = sc->sc_iot;
	saa.saa_iobase = pci_conf_read(pa->pa_pc, pa->pa_tag, SSIO_PCI_PPBAR);
	saa.saa_iobase &= 0xfffffffe;
	saa.saa_irq = 5;
	config_found(self, &saa, ssio_print);

	return;

unmap_ic2:
	bus_space_unmap(sc->sc_iot, sc->sc_ic2h, 2);
unmap_ic1:
	bus_space_unmap(sc->sc_iot, sc->sc_ic1h, 2);
}

int
ssio_intr(void *v)
{
	struct ssio_softc *sc = v;
	struct ssio_iv *iv;
	int claimed = 0;
	int irq, isr;

	/* Poll for interrupt. */
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 0, 0x0c);
	irq = bus_space_read_1(sc->sc_iot, sc->sc_ic1h, 0);
	irq &= 0x07;

	if (irq  == 7) {
		bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 0, 0x0b);
		isr = bus_space_read_1(sc->sc_iot, sc->sc_ic1h, 0);
		if ((isr & 0x80) == 0)
			/* Spurious interrupt. */
			return (0);
	}

	iv = &ssio_intr_table[irq];
	if (iv->handler)
		claimed = iv->handler(iv->arg);

	/* Signal EOI. */
	bus_space_write_1(sc->sc_iot, sc->sc_ic1h, 0, 0x60 | (irq & 0x0f));

	return (claimed);
}

void *
ssio_intr_establish(int pri, int irq, int (*handler)(void *), void *arg,
    const char *name)
{
	struct ssio_iv *iv;

	if (irq < 0 || irq >= SSIO_NINTS || ssio_intr_table[irq].handler)
		return (NULL);

	iv = &ssio_intr_table[irq];
	iv->handler = handler;
	iv->arg = arg;

	return (iv);
}

int
ssio_print(void *aux, const char *pnp)
{
	struct ssio_attach_args *saa = aux;

	if (pnp)
		printf("%s at %s\n", saa->saa_name, pnp);
	return (UNCONF);
}
