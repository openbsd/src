/*	$OpenBSD: sxiehci.c,v 1.9 2016/08/27 16:40:31 kettenis Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2016 Mark Kettenis <kettenis@openbsd.org>
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

/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#define USB_PMU_IRQ_ENABLE	0x800
#define ULPI_BYPASS		(1 << 0)
#define AHB_INCRX_ALIGN		(1 << 8)
#define AHB_INCR4		(1 << 9)
#define AHB_INCR8		(1 << 10)

struct sxiehci_softc {
	struct ehci_softc	sc;
	int			sc_node;
	void			*sc_ih;
};

int	sxiehci_match(struct device *, void *, void *);
void	sxiehci_attach(struct device *, struct device *, void *);
int	sxiehci_detach(struct device *, int);
int	sxiehci_activate(struct device *, int);

struct cfattach sxiehci_ca = {
	sizeof(struct sxiehci_softc), sxiehci_match, sxiehci_attach,
	sxiehci_detach, sxiehci_activate
};

void sxiehci_attach_phy(struct sxiehci_softc *);

int
sxiehci_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	if (OF_is_compatible(faa->fa_node, "allwinner,sun4i-a10-ehci"))
	    return 1;
	if (OF_is_compatible(faa->fa_node, "allwinner,sun5i-a13-ehci"))
	    return 1;
	if (OF_is_compatible(faa->fa_node, "allwinner,sun7i-a20-ehci"))
	    return 1;
	if (OF_is_compatible(faa->fa_node, "allwinner,sun8i-h3-ehci"))
	    return 1;

	return 0;
}

void
sxiehci_attach(struct device *parent, struct device *self, void *aux)
{
	struct sxiehci_softc	*sc = (struct sxiehci_softc *)self;
	struct fdt_attach_args	*faa = aux;
	usbd_status		 r;
	char			*devname = sc->sc.sc_bus.bdev.dv_xname;

	if (faa->fa_nreg < 1)
		return;

	sc->sc_node = faa->fa_node;
	sc->sc.iot = faa->fa_iot;
	sc->sc.sc_bus.dmatag = faa->fa_dmat;
	sc->sc.sc_size = faa->fa_reg[0].size;

	if (bus_space_map(sc->sc.iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc.ioh)) {
		printf(": cannot map mem space\n");
		goto out;
	}

	printf("\n");

	clock_enable_all(sc->sc_node);
	reset_deassert_all(sc->sc_node);
	sxiehci_attach_phy(sc);

	/* Disable interrupts, so we don't get any spurious ones. */
	sc->sc.sc_offs = EREAD1(&sc->sc, EHCI_CAPLENGTH);
	EOWRITE2(&sc->sc, EHCI_USBINTR, 0);

	sc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_USB,
	    ehci_intr, &sc->sc, devname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		printf("XXX - disable ehci");
#if 0
		clock_disable_all(sc->sc_node);
#endif
		goto mem0;
	}

	strlcpy(sc->sc.sc_vendor, "Allwinner", sizeof(sc->sc.sc_vendor));
	r = ehci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", devname, r);
		printf("XXX - disable ehci");
#if 0
		clock_disable_all(sc->sc_node);
#endif
		goto intr;
	}

	config_found(self, &sc->sc.sc_bus, usbctlprint);

	goto out;

intr:
	arm_intr_disestablish(sc->sc_ih);
	sc->sc_ih = NULL;
mem0:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
	sc->sc.sc_size = 0;
out:
	return;
}

void
sxiehci_attach_phy(struct sxiehci_softc *sc)
{
	uint32_t vbus_supply;
	uint32_t phys[2];
	char name[32];
	uint32_t val;
	int node;

	val = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USB_PMU_IRQ_ENABLE);
	val |= AHB_INCR8;	/* AHB INCR8 enable */
	val |= AHB_INCR4;	/* AHB burst type INCR4 enable */
	val |= AHB_INCRX_ALIGN;	/* AHB INCRX align enable */
	val |= ULPI_BYPASS;	/* ULPI bypass enable */
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USB_PMU_IRQ_ENABLE, val);

	if (OF_getpropintarray(sc->sc_node, "phys", phys,
	    sizeof(phys)) != sizeof(phys))
		return;

	node = OF_getnodebyphandle(phys[0]);
	if (node == -1)
		return;

	pinctrl_byname(node, "default");

	/*
	 * On sun4i, sun5i and sun7i, there is a single clock.  The
	 * more recent SoCs have a separate clock for each PHY.
	 */
	if (OF_is_compatible(node, "allwinner,sun4i-a10-usb-phy") ||
	    OF_is_compatible(node, "allwinner,sun5i-a13-usb-phy") ||
	    OF_is_compatible(node, "allwinner,sun7i-a20-usb-phy")) {
		clock_enable(node, "usb_phy");
	} else {
		snprintf(name, sizeof(name), "usb%d_phy", phys[1]);
		clock_enable(node, name);
	}

	snprintf(name, sizeof(name), "usb%d_reset", phys[1]);
	reset_deassert(node, name);

	snprintf(name, sizeof(name), "usb%d_vbus-supply", phys[1]);
	vbus_supply = OF_getpropint(node, name, 0);
	if (vbus_supply)
		regulator_enable(vbus_supply);
}

int
sxiehci_detach(struct device *self, int flags)
{
	struct sxiehci_softc *sc = (struct sxiehci_softc *)self;
	int rv;

	rv = ehci_detach(self, flags);
	if (rv)
		return (rv);

	if (sc->sc_ih != NULL) {
		arm_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc.sc_size) {
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}
	/* XXX */
#if 0
	sxiehci_detach_phy(sc);
	clock_disable_all(sc->sc_node);
#endif
	return (0);
}

int
sxiehci_activate(struct device *self, int act)
{
	struct sxiehci_softc *sc = (struct sxiehci_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		sc->sc.sc_bus.use_polling++;
		/* FIXME */
		sc->sc.sc_bus.use_polling--;
		break;
	case DVACT_RESUME:
		sc->sc.sc_bus.use_polling++;
		/* FIXME */
		sc->sc.sc_bus.use_polling--;
		break;
	case DVACT_POWERDOWN:
		ehci_reset(&sc->sc);
		break;
	}
	return 0;
}
