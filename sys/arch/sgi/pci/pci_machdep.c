/*	$OpenBSD: pci_machdep.c,v 1.1 2009/07/13 21:19:26 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/ppbreg.h>

void
ppb_initialize(pci_chipset_tag_t pc, pcitag_t tag, uint secondary,
    uint subordinate, bus_addr_t iostart, bus_addr_t ioend,
    bus_addr_t memstart, bus_addr_t memend)
{
	pci_conf_write(pc, tag, PPB_REG_BUSINFO,
	    (secondary << 8) | (subordinate << 16));

	pci_conf_write(pc, tag, PPB_REG_MEM,
	    ((memstart & 0xfff00000) >> 16) | (memend & 0xfff00000));
	pci_conf_write(pc, tag, PPB_REG_IOSTATUS,
	    (pci_conf_read(pc, tag, PPB_REG_IOSTATUS) & 0xffff0000) |
	    ((iostart & 0x0000f000) >> 8) | (ioend & 0x0000f000));
	pci_conf_write(pc, tag, PPB_REG_IO_HI,
	    ((iostart & 0xffff0000) >> 16) | (ioend & 0xffff0000));
	pci_conf_write(pc, tag, PPB_REG_PREFMEM, 0);

	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_INVALIDATE_ENABLE |
	    PCI_COMMAND_SERR_ENABLE);
}
