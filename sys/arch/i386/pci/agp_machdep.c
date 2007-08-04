/*	$OpenBSD: agp_machdep.c,v 1.8 2007/08/04 19:40:25 reyk Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/agpvar.h>

#include <machine/cpufunc.h>

const struct agp_product agp_products[] = {
	{ PCI_VENDOR_ALI, -1, agp_ali_attach },
	{ PCI_VENDOR_AMD, -1, agp_amd_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82810_GC, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82810_DC100_GC, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82810E_GC, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82815_FULL_GRAPH, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830MP_IV, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82852GM_AGP, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865_IGD, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IV, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82965_IGD_1, agp_i810_attach },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82965GM_IGD_1, agp_i810_attach },
	{ PCI_VENDOR_INTEL, -1, agp_intel_attach },
	{ PCI_VENDOR_SIS, -1, agp_sis_attach },
	{ PCI_VENDOR_VIATECH, -1, agp_via_attach },
	{ 0, 0, NULL }
};

void
agp_flush_cache(void)
{
	wbinvd();
}
