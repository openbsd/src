/*	$OpenBSD: uhci_pci.c,v 1.1 1999/08/13 05:32:29 fgsch Exp $	*/
/*	$NetBSD: uhci_pci.c,v 1.7 1999/05/20 09:52:35 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

#if defined(__NetBSD__)
int	uhci_pci_match __P((struct device *, struct cfdata *, void *));
#else
int	uhci_pci_match __P((struct device *, void *, void *));
#endif
void	uhci_pci_attach __P((struct device *, struct device *, void *));

struct cfattach uhci_pci_ca = {
	sizeof(uhci_softc_t), uhci_pci_match, uhci_pci_attach
};

int
uhci_pci_match(parent, match, aux)
	struct device *parent;
#if defined(__NetBSD__)
	struct cfdata *match;
#else
	void *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_UHCI)
		return 1;
 
	return 0;
}

void
uhci_pci_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	uhci_softc_t *sc = (uhci_softc_t *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t csr;
#if defined(__NetBSD__)
	char *typestr, *vendor;
#else
	char *typestr;
#endif
	char devinfo[256];
	usbd_status r;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
			   &sc->iot, &sc->ioh, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_bus.bdev.dv_xname);
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", 
		       sc->sc_bus.bdev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
#if defined(__NetBSD__)
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_USB, uhci_intr, sc);
#else
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_USB, uhci_intr, sc,
	    sc->sc_bus.bdev.dv_xname);
#endif
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_bus.bdev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_bus.bdev.dv_xname, intrstr);

	switch(pci_conf_read(pc, pa->pa_tag, PCI_USBREV) & PCI_USBREV_MASK) {
	case PCI_USBREV_PRE_1_0:
		typestr = "pre 1.0";
		break;
	case PCI_USBREV_1_0:
		typestr = "1.0";
		break;
	default:
		typestr = "unknown";
		break;
	}
	printf("%s: USB version %s\n", sc->sc_bus.bdev.dv_xname, typestr);

#if defined(__NetBSD__)
	/* Figure out vendor for root hub descriptor. */
	vendor = pci_findvendor(pa->pa_id);
	sc->sc_id_vendor = PCI_VENDOR(pa->pa_id);
	if (vendor)
		strncpy(sc->sc_vendor, vendor, sizeof(sc->sc_vendor) - 1);
	else
		sprintf(sc->sc_vendor, "vendor 0x%04x", PCI_VENDOR(pa->pa_id));
#endif
	
	r = uhci_init(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", 
		       sc->sc_bus.bdev.dv_xname, r);
		return;
	}

	/* Attach usb device. */
	config_found((void *)sc, &sc->sc_bus, usbctlprint);
}
