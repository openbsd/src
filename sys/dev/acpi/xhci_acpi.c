/*	$OpenBSD: xhci_acpi.c,v 1.1 2018/07/02 11:23:19 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis
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
#include <machine/intr.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>

struct xhci_acpi_softc {
	struct xhci_softc sc;
	struct acpi_softc *sc_acpi;
	struct aml_node *sc_node;

	bus_addr_t	sc_addr;
	bus_size_t	sc_size;

	int		sc_irq;
	int		sc_irq_flags;
	void		*sc_ih;
};

int	xhci_acpi_match(struct device *, void *, void *);
void	xhci_acpi_attach(struct device *, struct device *, void *);

struct cfattach xhci_acpi_ca = {
	sizeof(struct xhci_acpi_softc), xhci_acpi_match, xhci_acpi_attach
};

const char *xhci_hids[] = {
	"PNP0D10",
	NULL
};

int	xhci_acpi_parse_resources(int, union acpi_resource *, void *);

int
xhci_acpi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, xhci_hids, cf->cf_driver->cd_name);
}

void
xhci_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct xhci_acpi_softc *sc = (struct xhci_acpi_softc *)self;
	struct acpi_attach_args *aaa = aux;
	struct aml_value res;
	int error;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS", 0, NULL, &res)) {
		printf(": can't find registers\n");
		return;
	}

	aml_parse_resource(&res, xhci_acpi_parse_resources, sc);
	printf(" addr 0x%lx/0x%lx", sc->sc_addr, sc->sc_size);
	if (sc->sc_addr == 0 || sc->sc_size == 0) {
		printf("\n");
		return;
	}

	printf(" irq %d", sc->sc_irq);

	sc->sc.iot = aaa->aaa_memt;
	sc->sc.sc_size = sc->sc_size;
	sc->sc.sc_bus.dmatag = aaa->aaa_dmat;

	if (bus_space_map(sc->sc.iot, sc->sc_addr, sc->sc_size, 0,
	    &sc->sc.ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_ih = acpi_intr_establish(sc->sc_irq, sc->sc_irq_flags, IPL_USB,
	    xhci_intr, sc, sc->sc.sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	strlcpy(sc->sc.sc_vendor, "Generic", sizeof(sc->sc.sc_vendor));
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
#ifdef notyet
	acpi_intr_disestablish(sc->sc_ih);
#endif
unmap:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
	return;
}

int
xhci_acpi_parse_resources(int crsidx, union acpi_resource *crs, void *arg)
{
	struct xhci_acpi_softc *sc = arg;
	int type = AML_CRSTYPE(crs);

	switch (type) {
	case LR_MEM32FIXED:
		/* XHCI registers are specified by the first resource. */
		if (sc->sc_size == 0) {
			sc->sc_addr = crs->lr_m32fixed._bas;
			sc->sc_size = crs->lr_m32fixed._len;
		}
		break;
	case LR_EXTIRQ:
		sc->sc_irq = crs->lr_extirq.irq[0];
		sc->sc_irq_flags = crs->lr_extirq.flags;
		break;
	}

	return 0;
}
