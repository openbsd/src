/*	$OpenBSD: rbus_machdep.c,v 1.1 2007/08/04 16:46:03 kettenis Exp $	*/

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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/cardbus/rbus.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pccbbreg.h>

struct rbustag rbus_null;

/*
 * The PROM doesn't really understand CardBus bridges.  So it treats
 * the memory and IO window register as ordinary BARs and assigns
 * address space to them.  We re-use that address space for rbus.
 * This is a bit of a hack, but it seems to work and saves us from
 * tracking down available address space globally.
 */

rbus_tag_t
rbus_pccbb_parent_mem(struct device *self, struct pci_attach_args *pa)
{
	struct ofw_pci_register addr[5];
	int naddr, len, i;
	int space, reg;

	len = OF_getprop(PCITAG_NODE(pa->pa_tag), "assigned-addresses",
	    &addr, sizeof(addr));
	naddr = len / sizeof(struct ofw_pci_register);

	for (i = 0; i < naddr; i++) {
		space = addr[i].phys_hi & OFW_PCI_PHYS_HI_SPACEMASK;
		if (space != OFW_PCI_PHYS_HI_SPACE_MEM32)
			continue;
		reg = addr[i].phys_hi & OFW_PCI_PHYS_HI_REGISTERMASK;
		if (reg < PCI_CB_MEMBASE0 || reg > PCI_CB_IOLIMIT1)
			continue;

		return (rbus_new_root_delegate(pa->pa_memt,
		    addr[i].phys_lo, addr[i].size_lo, 0));
	}

	return &rbus_null;
}

rbus_tag_t
rbus_pccbb_parent_io(struct device *self, struct pci_attach_args *pa)
{
	struct ofw_pci_register addr[5];
	int naddr, len, i;
	int space, reg;

	len = OF_getprop(PCITAG_NODE(pa->pa_tag), "assigned-addresses",
	    &addr, sizeof(addr));
	naddr = len / sizeof(struct ofw_pci_register);

	for (i = 0; i < naddr; i++) {
		space = addr[i].phys_hi & OFW_PCI_PHYS_HI_SPACEMASK;
		if (space != OFW_PCI_PHYS_HI_SPACE_IO)
			continue;
		reg = addr[i].phys_hi & OFW_PCI_PHYS_HI_REGISTERMASK;
		if (reg < PCI_CB_MEMBASE0 || reg > PCI_CB_IOLIMIT1)
			continue;

		return (rbus_new_root_delegate(pa->pa_iot,
		    addr[i].phys_lo, addr[i].size_lo, 0));
	}

	return &rbus_null;
}
