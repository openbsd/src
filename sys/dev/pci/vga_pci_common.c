/*
 * Copyright (c) 2008 Owain G. Ainsworth <oga@nicotinebsd.org>
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

#include "vga.h"
#if defined(__i386__) || defined(__amd64__)
#include "acpi.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>

#include <dev/pci/drm/i915/i915_devlist.h>
#include <dev/pci/drm/radeon/radeon_devlist.h>
#include <dev/pci/drm/amd/amdgpu/amdgpu_devlist.h>

#ifdef RAMDISK_HOOKS
static const struct pci_matchid aperture_blacklist[] = {
	/* server adapters found in mga200 drm driver */
	{ PCI_VENDOR_MATROX,	PCI_PRODUCT_MATROX_G200E_SE },
	{ PCI_VENDOR_MATROX,	PCI_PRODUCT_MATROX_G200E_SE_B },
	{ PCI_VENDOR_MATROX,	PCI_PRODUCT_MATROX_G200EH },
	{ PCI_VENDOR_MATROX,	PCI_PRODUCT_MATROX_G200ER },
	{ PCI_VENDOR_MATROX,	PCI_PRODUCT_MATROX_G200EV },
	{ PCI_VENDOR_MATROX,	PCI_PRODUCT_MATROX_G200EW },

	/* server adapters found in ast drm driver */
	{ PCI_VENDOR_ASPEED,	PCI_PRODUCT_ASPEED_AST2000 },
	{ PCI_VENDOR_ASPEED,	PCI_PRODUCT_ASPEED_AST2100 },

	/* ati adapters found in servers */
	{ PCI_VENDOR_ATI,		PCI_PRODUCT_ATI_RAGEXL },
	{ PCI_VENDOR_ATI,		PCI_PRODUCT_ATI_ES1000 },

	/* xgi found in some poweredges/supermicros/tyans */
	{ PCI_VENDOR_XGI,		PCI_PRODUCT_XGI_VOLARI_Z7 },
	{ PCI_VENDOR_XGI,		PCI_PRODUCT_XGI_VOLARI_Z9 },
};

int
vga_aperture_needed(struct pci_attach_args *pa)
{
	if (pci_matchbyid(pa, i915_devices, nitems(i915_devices)) ||
	    pci_matchbyid(pa, aperture_blacklist, nitems(aperture_blacklist)))
		return (0);
#if defined(__amd64__) || defined(__i386__) || defined(__loongson__) || \
    defined(__macppc__) || defined(__sparc64__)
	if (pci_matchbyid(pa, radeon_devices, nitems(radeon_devices)))
		return (0);
#endif
#ifdef __amd64__
	if (pci_matchbyid(pa, amdgpu_devices, nitems(amdgpu_devices)))
		return (0);
#endif
	return (1);
}
#endif /* RAMDISK_HOOKS */
