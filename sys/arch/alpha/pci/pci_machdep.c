/*	$NetBSD: pci_machdep.c,v 1.3 1995/11/23 02:38:07 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Machine-specific functions for PCI autoconfiguration.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <vm/vm.h>

#include <dev/isa/isavar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include "pcivga.h"
#if NPCIVGA
#include <alpha/pci/pcivgavar.h>
#endif

#include "tga.h"
#if NTGA
#include <alpha/pci/tgavar.h>
#endif

void
pci_display_console(pcf, pcfa, pmf, pmfa, ppf, ppfa, bus, device, function)
	__const struct pci_conf_fns *pcf;
	__const struct pci_mem_fns *pmf;
	__const struct pci_pio_fns *ppf;
	void *pcfa, *pmfa, *ppfa;
	pci_bus_t bus;
	pci_device_t device;
	pci_function_t function;
{
	pci_conftag_t tag;
	pci_confreg_t id, class;

	tag = PCI_MAKE_TAG(bus, device, function);
	id = PCI_CONF_READ(pcf, pcfa, tag, PCI_ID_REG);
	if (id == 0 || id == 0xffffffff)
		panic("pci_display_console: no device at %d/%d/%d",
		    bus, device, function);
	class = PCI_CONF_READ(pcf, pcfa, tag, PCI_CLASS_REG);

#if NPCIVGA
	if (DEVICE_IS_PCIVGA(class, id)) {
		pcivga_console(pcf, pcfa, pmf, pmfa, ppf, ppfa, bus,
		    device, function);
		return;
	}
#endif

#if NTGA
	if (DEVICE_IS_TGA(class, id)) {
		tga_console(pcf, pcfa, pmf, pmfa, ppf, ppfa, bus,
		    device, function);
		return;
	}
#endif

	panic("pci_display_console: unconfigured device at %d/%d/%d",
		    bus, device, function);
}
