/*	$OpenBSD: if_xl_pci.c,v 1.3 2000/07/01 03:19:15 aaron Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: if_xl.c,v 1.72 2000/01/09 21:12:59 wpaul Exp $
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>   /* only for declaration of wakeup() used by vm.h */
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */

/*
 * The following #define causes the code to use PIO to access the
 * chip's registers instead of memory mapped mode. The reason PIO mode
 * is on by default is that the Etherlink XL manual seems to indicate
 * that only the newer revision chips (3c905B) support both PIO and
 * memory mapped access. Since we want to be compatible with the older
 * bus master chips, we use PIO here. If you comment this out, the
 * driver will use memory mapped I/O, which may be faster but which
 * might not work on some devices.
 */
#define XL_USEIOSPACE

#include <dev/ic/xlreg.h>

int xl_pci_match __P((struct device *, void *, void *));
void xl_pci_attach __P((struct device *, struct device *, void *));

int
xl_pci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_3COM)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_3COM_3CSOHO100TX:
	case PCI_PRODUCT_3COM_3C900TPO:
	case PCI_PRODUCT_3COM_3C900COMBO:
	case PCI_PRODUCT_3COM_3C900B:
	case PCI_PRODUCT_3COM_3C900BCOMBO:
	case PCI_PRODUCT_3COM_3C900BTPC:
	case PCI_PRODUCT_3COM_3C900BFL:
	case PCI_PRODUCT_3COM_3C905TX:
	case PCI_PRODUCT_3COM_3C905T4:
	case PCI_PRODUCT_3COM_3C905BTX:
	case PCI_PRODUCT_3COM_3C905BT4:
	case PCI_PRODUCT_3COM_3C905BCOMBO:
	case PCI_PRODUCT_3COM_3C905BFX:
	case PCI_PRODUCT_3COM_3C980TX:
	case PCI_PRODUCT_3COM_3C980CTX:
	case PCI_PRODUCT_3COM_3C905CTX:
	case PCI_PRODUCT_3COM_3C450:
		return (1);
	}
					
	return (0);
}

void
xl_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct xl_softc *sc = (struct xl_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_addr_t iobase;
	bus_size_t iosize;
	u_int32_t command;

	sc->xl_unit = sc->sc_dev.dv_unit;

	sc->xl_bustype = XL_BUS_PCI;
	sc->xl_cb_flags = 0;

	/*
	 * If this is a 3c905B, we have to check one extra thing.
	 * The 905B supports power management and may be placed in
	 * a low-power mode (D3 mode), typically by certain operating
	 * systems which shall not be named. The PCI BIOS is supposed
	 * to reset the NIC and bring it out of low-power mode, but  
	 * some do not. Consequently, we have to see if this chip    
	 * supports power management, and if so, make sure it's not  
	 * in low-power mode. If power management is available, the  
	 * capid byte will be 0x01.
	 * 
	 * I _think_ that what actually happens is that the chip
	 * loses its PCI configuration during the transition from
	 * D3 back to D0; this means that it should be possible for
	 * us to save the PCI iobase, membase and IRQ, put the chip
	 * back in the D0 state, then restore the PCI config ourselves.
	 */
	command = pci_conf_read(pc, pa->pa_tag, XL_PCI_CAPID) & 0xff;
	if (command == 0x01) {

		command = pci_conf_read(pc, pa->pa_tag,
		    XL_PCI_PWRMGMTCTRL);
		if (command & XL_PSTATE_MASK) {
			u_int32_t io, mem, irq;

			/* Save PCI config */
			io = pci_conf_read(pc, pa->pa_tag, XL_PCI_LOIO);
			mem = pci_conf_read(pc, pa->pa_tag, XL_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag, XL_PCI_INTLINE);

			/* Reset the power state. */
			printf("%s: chip is in D%d power mode "
			    "-- setting to D0\n",
			    sc->sc_dev.dv_xname, command & XL_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag,
			    XL_PCI_PWRMGMTCTRL, command);

			pci_conf_write(pc, pa->pa_tag, XL_PCI_LOIO, io);
			pci_conf_write(pc, pa->pa_tag, XL_PCI_LOMEM, mem);
			pci_conf_write(pc, pa->pa_tag, XL_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE |
		   PCI_COMMAND_MEM_ENABLE |
		   PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

#ifdef XL_USEIOSPACE
	if (!(command & PCI_COMMAND_IO_ENABLE)) {
		printf("%s: failed to enable i/o ports\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	/*
	 * Map control/status registers.
	 */
	if (pci_io_find(pc, pa->pa_tag, XL_PCI_LOIO, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		return;
	}
	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &sc->xl_bhandle)) {
		printf(": can't map i/o space\n");
		return;
	}
	sc->xl_btag = pa->pa_iot;
#else
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}
	if (pci_mem_find(pc, pa->pa_tag, XL_PCI_LOMEM, &iobase, &iosize, NULL)){
		printf(": can't find mem space\n");
		return;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->xl_bhandle)) {
		printf(": can't map mem space\n");
		return;
	}
	sc->xl_btag = pa->pa_memt;
#endif

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->xl_intrhand = pci_intr_establish(pc, ih, IPL_NET, xl_intr, sc,
	    self->dv_xname);
	if (sc->xl_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		return;
	}
	printf(": %s", intrstr);

	xl_attach(sc);
}

struct cfattach xl_pci_ca = {
	sizeof(struct xl_softc), xl_pci_match, xl_pci_attach,
};
