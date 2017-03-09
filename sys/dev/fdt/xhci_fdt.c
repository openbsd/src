/*	$OpenBSD: xhci_fdt.c,v 1.1 2017/03/09 19:26:39 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>

struct xhci_fdt_softc {
	struct xhci_softc	sc;
	void			*sc_ih;
};

int	xhci_fdt_match(struct device *, void *, void *);
void	xhci_fdt_attach(struct device *, struct device *, void *);

struct cfattach xhci_fdt_ca = {
	sizeof(struct xhci_fdt_softc), xhci_fdt_match, xhci_fdt_attach
};

int
xhci_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "snps,dwc3");
}

void
xhci_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct xhci_fdt_softc *sc = (struct xhci_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;
	int error;

	if (faa->fa_nreg < 1)
		return;

	sc->sc.iot = faa->fa_iot;
	sc->sc.sc_size = faa->fa_reg[0].size;
	sc->sc.sc_bus.dmatag = faa->fa_dmat;

	if (bus_space_map(sc->sc.iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc.ioh))
		panic("%s: bus_space_map failed!", __func__);

	sc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_USB,
	    xhci_intr, sc, sc->sc.sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	printf("\n");

	if ((error = xhci_init(&sc->sc)) != 0) {
		printf("%s: init failed, error=%d\n",
		    sc->sc.sc_bus.bdev.dv_xname, error);
		goto disestablish_ret;
	}

	/* Attach usb device. */
	config_found(self, &sc->sc.sc_bus, usbctlprint);

	/* Now that the stack is ready, config' the HC and enable interrupts. */
	xhci_config(&sc->sc);

	return;

disestablish_ret:
	arm_intr_disestablish_fdt(sc->sc_ih);
unmap:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
}
