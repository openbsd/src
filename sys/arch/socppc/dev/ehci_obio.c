/*	$OpenBSD: ehci_obio.c,v 1.5 2015/01/24 20:59:42 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <sys/rwlock.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#include <dev/pci/pcidevs.h>

#define USB_EHCI_OFFSET		0x00100
#define USB_SNOOP1		0x00400 - USB_EHCI_OFFSET
#define  USB_SNOOP_2GB		0x1e000000
#define USB_CONTROL		0x00500 - USB_EHCI_OFFSET
#define  USB_CONTROL_USB_EN	0x04000000

int	ehci_obio_match(struct device *, void *, void *);
void	ehci_obio_attach(struct device *, struct device *, void *);

struct cfattach ehci_obio_ca = {
	sizeof(struct ehci_softc), ehci_obio_match, ehci_obio_attach
};

struct cfdriver ehci_obio_cd = {
	NULL, "ehci", DV_DULL
};

struct powerpc_bus_dma_tag ehci_bus_dma_tag = {
	NULL,
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_alloc_range,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap
};

int
ehci_obio_match(struct device *parent, void *cfdata, void *aux)
{
	struct obio_attach_args *oa = aux;
	char buf[32];

	if (OF_getprop(oa->oa_node, "compatible", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "fsl-usb2-mph") != 0)
		return (0);

	return (1);
}

void
ehci_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct ehci_softc *sc = (void *)self;
	struct obio_attach_args *oa = aux;
	usbd_status r;
	int s;

	sc->iot = oa->oa_iot;
	sc->sc_size = 1028;
	if (bus_space_map(sc->iot, oa->oa_offset + USB_EHCI_OFFSET,
	    sc->sc_size, 0, &sc->ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_id_vendor = PCI_VENDOR_FREESCALE;
	strlcpy(sc->sc_vendor, "Freescale", sizeof sc->sc_vendor);

	sc->sc_bus.dmatag = &ehci_bus_dma_tag;

	bus_space_write_4(sc->iot, sc->ioh, USB_CONTROL, USB_CONTROL_USB_EN);
	bus_space_write_4(sc->iot, sc->ioh, USB_SNOOP1, USB_SNOOP_2GB);

	s = splhardusb();
	sc->sc_offs = EREAD1(sc, EHCI_CAPLENGTH);
	EOWRITE2(sc, EHCI_USBINTR, 0);

	intr_establish(oa->oa_ivec, IST_LEVEL, IPL_USB, ehci_intr, sc,
	    sc->sc_bus.bdev.dv_xname);

	r = ehci_init(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf(": init failed, error=%d\n", r);
		goto unmap_ret;
	}
	splx(s);

	printf("\n");

	/* Attach usb device. */
	config_found(self, &sc->sc_bus, usbctlprint);

	return;

unmap_ret:
	bus_space_unmap(sc->iot, sc->ioh, sc->sc_size);
	splx(s);
}
