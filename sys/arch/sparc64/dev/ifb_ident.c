/*	$OpenBSD: ifb_ident.c,v 1.1 2008/12/29 22:07:35 miod Exp $	*/

/*
 * Copyright (c) 2007, 2008 Miodrag Vallat.
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
 * Identify an Expert3D card.
 * Used by both vgafb and ifb.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <machine/fbvar.h>

static const struct pci_matchid ifb_devices[] = {
    { PCI_VENDOR_INTERGRAPH, PCI_PRODUCT_INTERGRAPH_EXPERT3D },
    { PCI_VENDOR_3DLABS,     PCI_PRODUCT_3DLABS_WILDCAT_6210 },
    { PCI_VENDOR_3DLABS,     PCI_PRODUCT_3DLABS_WILDCAT_5110 },/* Sun XVR-500 */
    { PCI_VENDOR_3DLABS,     PCI_PRODUCT_3DLABS_WILDCAT_7210 },
};

int
ifb_ident(void *aux)
{
	struct pci_attach_args *paa = aux;
	int node;
	char *name;

	if (pci_matchbyid(paa, ifb_devices,
	    sizeof(ifb_devices) / sizeof(ifb_devices[0])) != 0)
		return 1;

	node = PCITAG_NODE(paa->pa_tag);
	name = getpropstring(node, "name");
	if (strcmp(name, "SUNW,Expert3D") == 0 ||
	    strcmp(name, "SUNW,Expert3D-Lite") == 0)
		return 1;

	return 0;
}
