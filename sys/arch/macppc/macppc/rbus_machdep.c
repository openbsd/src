/*	$OpenBSD: rbus_machdep.c,v 1.3 2002/09/15 09:01:58 deraadt Exp $ */
/*	$NetBSD: rbus_machdep.c,v 1.2 1999/10/15 06:43:06 haya Exp $	*/

/*
 * Copyright (c) 1999
 *     HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>

#include <uvm/uvm_extern.h>

#include <sys/sysctl.h>

#include <sys/device.h>

#include <machine/bus.h>
#include <dev/cardbus/rbus.h>

#include <dev/pci/pcivar.h>
#include <arch/macppc/pci/pcibrvar.h>

void macppc_cardbus_init(pci_chipset_tag_t pc, pcitag_t tag);

/**********************************************************************
 * rbus_tag_t rbus_fakeparent_mem(struct pci_attach_args *pa)
 *
 *   This function makes an rbus tag for memory space.  This rbus tag
 *   shares the all memory region of ex_iomem.
 **********************************************************************/
#define RBUS_MEM_SIZE	0x10000000

rbus_tag_t
rbus_pccbb_parent_mem(self, pa)
     struct device *self;
     struct pci_attach_args *pa;
{
	bus_addr_t start;
	bus_size_t size;
	struct extent *ex;

	macppc_cardbus_init(pa->pa_pc, pa->pa_tag);

	size = RBUS_MEM_SIZE;
	if ((ex = pciaddr_search(PCIADDR_SEARCH_MEM, self, &start, size)) == NULL)
	{
		/* XXX */
		printf("failed\n");
	}

	return rbus_new_root_share(pa->pa_memt, ex, start, size, 0);
}


/**********************************************************************
 * rbus_tag_t rbus_pccbb_parent_io(struct pci_attach_args *pa)
 **********************************************************************/
#define RBUS_IO_SIZE	0x1000

rbus_tag_t
rbus_pccbb_parent_io(self, pa)
	struct device *self;
	struct pci_attach_args *pa;
{
	struct extent *ex;
	bus_addr_t start;
	bus_size_t size;


	size = RBUS_IO_SIZE;
	if ((ex = pciaddr_search(PCIADDR_SEARCH_IO, self, &start, size)) == NULL)
	{
		/* XXX */
		printf("failed\n");
	}

	return rbus_new_root_share(pa->pa_iot, ex, start, size, 0);
}


/*
 * Big ugly hack to enable bridge/fix interrupts
 */
void
macppc_cardbus_init(pc, tag)
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	u_int x;
	static int initted = 0;

	if (initted)
		return;
	initted = 1;

	/* XXX What about other bridges? */

	x = pci_conf_read(pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(x) == PCI_VENDOR_TI &&
	    PCI_PRODUCT(x) == PCI_PRODUCT_TI_PCI1211) {
		/* For CardBus card. */
		pci_conf_write(pc, tag, 0x18, 0x10010100);

		/* Route INTA to MFUNC0 */
		x = pci_conf_read(pc, tag, 0x8c);
		x |= 0x02;
		pci_conf_write(pc, tag, 0x8c, x);

		tag = pci_make_tag(pc, 0, 0, 0);
		x = pci_conf_read(pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(x) == PCI_VENDOR_MOT &&
		    PCI_PRODUCT(x) == PCI_PRODUCT_MOT_MPC106) {
			/* Set subordinate bus number to 1. */
			x = pci_conf_read(pc, tag, 0x40);
			x |= 1 << 8;
			pci_conf_write(pc, tag, 0x40, x);
		}
	}
}
