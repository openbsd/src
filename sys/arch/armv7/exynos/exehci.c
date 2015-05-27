/*	$OpenBSD: exehci.c,v 1.2 2015/05/27 00:06:14 jsg Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

#include "fdt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>
#if NFDT > 0
#include <machine/fdt.h>
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/exynos/exsysregvar.h>
#include <armv7/exynos/expowervar.h>
#include <armv7/exynos/exgpiovar.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

/* registers */
#define USBPHY_CTRL0			0x00
#define USBPHY_TUNE0			0x04
#define HSICPHY_CTRL1			0x10
#define HSICPHY_TUNE1			0x14
#define HSICPHY_CTRL2			0x20
#define HSICPHY_TUNE2			0x24
#define EHCI_CTRL			0x30
#define OHCI_CTRL			0x34
#define USBOTG_SYS			0x38
#define USBOTG_TUNE			0x40

/* bits and bytes */
#define CLK_24MHZ			5

#define HOST_CTRL0_PHYSWRSTALL		(1U << 31)
#define HOST_CTRL0_COMMONON_N		(1 << 9)
#define HOST_CTRL0_SIDDQ		(1 << 6)
#define HOST_CTRL0_FORCESLEEP		(1 << 5)
#define HOST_CTRL0_FORCESUSPEND		(1 << 4)
#define HOST_CTRL0_WORDINTERFACE	(1 << 3)
#define HOST_CTRL0_UTMISWRST		(1 << 2)
#define HOST_CTRL0_LINKSWRST		(1 << 1)
#define HOST_CTRL0_PHYSWRST		(1 << 0)

#define HOST_CTRL0_FSEL_MASK		(7 << 16)

#define EHCI_CTRL_ENAINCRXALIGN		(1 << 29)
#define EHCI_CTRL_ENAINCR4		(1 << 28)
#define EHCI_CTRL_ENAINCR8		(1 << 27)
#define EHCI_CTRL_ENAINCR16		(1 << 26)

int	exehci_match(struct device *, void *, void *);
void	exehci_attach(struct device *, struct device *, void *);
int	exehci_detach(struct device *, int);

struct exehci_softc {
	struct device		sc_dev;
	struct ehci_softc	*sc_ehci;
	void			*sc_ih;
	bus_dma_tag_t		sc_dmat;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_size;
	bus_space_handle_t	ph_ioh;
};

struct cfdriver exehci_cd = {
	NULL, "exehci", DV_DULL
};

struct cfattach exehci_ca = {
	sizeof (struct exehci_softc), NULL, exehci_attach,
	exehci_detach, NULL
};
struct cfattach exehci_fdt_ca = {
	sizeof (struct exehci_softc), exehci_match, exehci_attach,
	exehci_detach, NULL
};

void	exehci_setup(struct exehci_softc *);

int
exehci_match(struct device *parent, void *v, void *aux)
{
#if NFDT > 0
	struct armv7_attach_args *aa = aux;

	if (fdt_node_compatible("samsung,exynos4210-ehci", aa->aa_node))
		return 1;
#endif

	return 0;
}

void
exehci_attach(struct device *parent, struct device *self, void *aux)
{
	struct exehci_softc		*sc = (struct exehci_softc *)self;
	struct ehci_softc		*esc;
	struct armv7_attach_args	*aa = aux;
	struct armv7mem			 hmem, pmem;
	int				 irq;
	usbd_status			 r;

	sc->sc_iot = aa->aa_iot;
	sc->sc_dmat = aa->aa_dmat;

#if NFDT > 0
	if (aa->aa_node) {
		struct fdt_memory fhmem, fpmem;
		uint32_t ints[3];

		if (fdt_get_memory_address(aa->aa_node, 0, &fhmem))
			panic("%s: could not extract memory data from FDT",
			    __func__);

		/* XXX: In a different way, please. */
		void *node = fdt_find_compatible("samsung,exynos5250-usb2-phy");
		if (node == NULL || fdt_get_memory_address(node, 0, &fpmem))
			panic("%s: could not extract phy data from FDT",
			    __func__);

		/* TODO: Add interrupt FDT API. */
		if (fdt_node_property_ints(aa->aa_node, "interrupts",
		    ints, 3) != 3)
			panic("%s: could not extract interrupt data from FDT",
			    __func__);

		hmem.addr = fhmem.addr;
		hmem.size = fhmem.size;
		pmem.addr = fpmem.addr;
		pmem.size = fpmem.size;

		irq = ints[1];
	} else
#endif
	{
		hmem.addr = aa->aa_dev->mem[0].addr;
		hmem.size = aa->aa_dev->mem[0].size;
		pmem.addr = aa->aa_dev->mem[1].addr;
		pmem.size = aa->aa_dev->mem[1].size;
		irq = aa->aa_dev->irq[0];
	}

	/* Map I/O space */
	sc->sc_size = hmem.size;
	if (bus_space_map(sc->sc_iot, hmem.addr, hmem.size, 0, &sc->sc_ioh)) {
		printf(": cannot map mem space\n");
		goto out;
	}

	if (bus_space_map(sc->sc_iot, pmem.addr, pmem.size, 0, &sc->ph_ioh)) {
		printf(": cannot map mem space\n");
		goto pmem;
	}

	printf("\n");

	exehci_setup(sc);

	if ((esc = (struct ehci_softc *)config_found(self, NULL, NULL)) == NULL)
		goto hmem;

	sc->sc_ehci = esc;
	esc->iot = sc->sc_iot;
	esc->ioh = sc->sc_ioh;
	esc->sc_bus.dmatag = aa->aa_dmat;

	sc->sc_ih = arm_intr_establish(irq, IPL_USB,
	    ehci_intr, esc, esc->sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto hmem;
	}

	strlcpy(esc->sc_vendor, "Exynos 5", sizeof(esc->sc_vendor));
	r = ehci_init(esc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n",
		    esc->sc_bus.bdev.dv_xname, r);
		goto intr;
	}

	printf("\n");

	config_found((struct device *)esc, &esc->sc_bus, usbctlprint);

	goto out;

intr:
	arm_intr_disestablish(sc->sc_ih);
	sc->sc_ih = NULL;
hmem:
	bus_space_unmap(sc->sc_iot, sc->ph_ioh, hmem.size);
pmem:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
	sc->sc_size = 0;
out:
	return;
}

int
exehci_detach(struct device *self, int flags)
{
	struct exehci_softc		*sc = (struct exehci_softc *)self;
	int				 rv = 0;

	rv = ehci_detach(self, flags);
	if (rv)
		return (rv);

	if (sc->sc_ih != NULL) {
		arm_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc_size) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
	}

	return 0;
}

void
exehci_setup(struct exehci_softc *sc)
{
	uint32_t val;

	/* VBUS, GPIO_X11, only on SMDK5250 and Chromebooks */
	exgpio_set_dir(0xa9, EXGPIO_DIR_OUT);
	exgpio_set_bit(0xa9);
	delay(3000);

	exsysreg_usbhost_mode(1);
	expower_usbhost_phy_ctrl(1);

	delay(10000);

	/* Setting up host and device simultaneously */
	val = bus_space_read_4(sc->sc_iot, sc->ph_ioh, USBPHY_CTRL0);
	val &= ~(HOST_CTRL0_FSEL_MASK |
		 HOST_CTRL0_COMMONON_N |
		 /* HOST Phy setting */
		 HOST_CTRL0_PHYSWRST |
		 HOST_CTRL0_PHYSWRSTALL |
		 HOST_CTRL0_SIDDQ |
		 HOST_CTRL0_FORCESUSPEND |
		 HOST_CTRL0_FORCESLEEP);
	val |= (/* Setting up the ref freq */
		 CLK_24MHZ << 16 |
		 /* HOST Phy setting */
		 HOST_CTRL0_LINKSWRST |
		 HOST_CTRL0_UTMISWRST);
	bus_space_write_4(sc->sc_iot, sc->ph_ioh, USBPHY_CTRL0, val);
	delay(10000);
	bus_space_write_4(sc->sc_iot, sc->ph_ioh, USBPHY_CTRL0,
	    bus_space_read_4(sc->sc_iot, sc->ph_ioh, USBPHY_CTRL0) &
		~(HOST_CTRL0_LINKSWRST | HOST_CTRL0_UTMISWRST));
	delay(20000);

	/* EHCI Ctrl setting */
	bus_space_write_4(sc->sc_iot, sc->ph_ioh, EHCI_CTRL,
	    bus_space_read_4(sc->sc_iot, sc->ph_ioh, EHCI_CTRL) |
		EHCI_CTRL_ENAINCRXALIGN |
		EHCI_CTRL_ENAINCR4 |
		EHCI_CTRL_ENAINCR8 |
		EHCI_CTRL_ENAINCR16);

	/* HSIC USB Hub initialization. */
	if (1) {
		exgpio_set_dir(0xc8, EXGPIO_DIR_OUT);
		exgpio_clear_bit(0xc8);
		delay(1000);
		exgpio_set_bit(0xc8);
		delay(5000);

		val = bus_space_read_4(sc->sc_iot, sc->ph_ioh, HSICPHY_CTRL1);
		val &= ~(HOST_CTRL0_SIDDQ |
			 HOST_CTRL0_FORCESLEEP |
			 HOST_CTRL0_FORCESUSPEND);
		bus_space_write_4(sc->sc_iot, sc->ph_ioh, HSICPHY_CTRL1, val);
		val |= HOST_CTRL0_PHYSWRST;
		bus_space_write_4(sc->sc_iot, sc->ph_ioh, HSICPHY_CTRL1, val);
		delay(1000);
		val &= ~HOST_CTRL0_PHYSWRST;
		bus_space_write_4(sc->sc_iot, sc->ph_ioh, HSICPHY_CTRL1, val);
	}

	/* PHY clock and power setup time */
	delay(50000);
}

int	ehci_ex_match(struct device *, void *, void *);
void	ehci_ex_attach(struct device *, struct device *, void *);

struct cfattach ehci_ex_ca = {
	sizeof (struct ehci_softc), ehci_ex_match, ehci_ex_attach
};

int
ehci_ex_match(struct device *parent, void *v, void *aux)
{
	return 1;
}

void
ehci_ex_attach(struct device *parent, struct device *self, void *aux)
{
}
