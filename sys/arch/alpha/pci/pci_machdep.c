/*	$NetBSD: pci_machdep.c,v 1.5 1996/04/12 06:08:49 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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
pci_display_console(bc, pc, bus, device, function)
	bus_chipset_tag_t bc;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	pcitag_t tag;
	pcireg_t id, class;
	int match, nmatch;
	void (*fn) __P((bus_chipset_tag_t, pci_chipset_tag_t, int, int, int));

	tag = pci_make_tag(pc, bus, device, function);
	id = pci_conf_read(pc, tag, PCI_ID_REG);
	if (id == 0 || id == 0xffffffff)
		panic("pci_display_console: no device at %d/%d/%d",
		    bus, device, function);
	class = pci_conf_read(pc, tag, PCI_CLASS_REG);

	match = 0;
	fn = NULL;

#if NPCIVGA
	nmatch = DEVICE_IS_PCIVGA(class, id);
	if (nmatch > match) {
		match = nmatch;
		fn = pcivga_console;
	}
#endif
#if NTGA
	nmatch = DEVICE_IS_TGA(class, id);
	if (nmatch > match) {
		match = nmatch;
		fn = tga_console;
	}
#endif

	if (fn != NULL)
		(*fn)(bc, pc, bus, device, function);
	else
		panic("pci_display_console: unconfigured device at %d/%d/%d",
		    bus, device, function);
}
