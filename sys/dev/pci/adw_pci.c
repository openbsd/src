/*	$OpenBSD: adw_pci.c,v 1.1 1998/11/17 06:11:05 downsj Exp $	*/
/* $NetBSD: adw_pci.c,v 1.2 1998/09/26 19:53:34 dante Exp $	 */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
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
/*
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *   Single Channel Products:
 *      ABP940UW - Bus-Master PCI Ultra-Wide (240 CDB)
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/adwlib.h>
#include <dev/ic/adw.h>

/******************************************************************************/

#define PCI_CBIO        0x10

/******************************************************************************/

int adw_pci_match __P((struct device *, void *, void *));
void adw_pci_attach __P((struct device *, struct device *, void *));

struct cfattach adw_pci_ca =
{
	sizeof(ADW_SOFTC), adw_pci_match, adw_pci_attach
};

/******************************************************************************/
/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
adw_pci_match(parent, match, aux)
	struct device  *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADVSYS)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADVSYS_WIDE:
			return (1);
		}

	return 0;
}


void
adw_pci_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct pci_attach_args *pa = aux;
	ADW_SOFTC      *sc = (void *) self;
	bus_space_handle_t ioh;
	bus_addr_t adwbase;
	bus_size_t adwsize;
	pci_intr_handle_t ih;
	pci_chipset_tag_t pc = pa->pa_pc;
	u_int32_t       command;
	const char     *intrstr;
	int retval;


	sc->sc_flags = 0x0;

	/*
	 * Latency timer settings.
	 */
	{
		u_int32_t       bhlcr;

		bhlcr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);

		if ((PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_WIDE) &&
		    (PCI_LATTIMER(bhlcr) < 0x20)) {
			bhlcr &= 0xFFFF00FFUL;
			bhlcr |= 0x00002000UL;
			pci_conf_write(pa->pa_pc, pa->pa_tag,
				       PCI_BHLC_REG, bhlcr);
		}
	}


	if ((PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_WIDE) &&
	    (command & PCI_COMMAND_PARITY_ENABLE) == 0) {
		sc->cfg.control_flag |= CONTROL_FLAG_IGNORE_PERR;
	}
	/*
	 * Map Device Registers for I/O
	 */
	retval = pci_io_find(pc, pa->pa_tag, PCI_CBIO, &adwbase, &adwsize);
	if (retval == 0)
		retval = bus_space_map(pa->pa_iot, adwbase, adwsize, 0, &ioh);
	if (retval) {
		printf("\n%s: unable to map device registers\n",
		       sc->sc_dev.dv_xname);
		return;
	}
	sc->sc_iot = pa->pa_iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Initialize the board
	 */
	if (adw_init(sc))
		panic("adw_pci_attach: adw_init failed");

	/*
	 * Map Interrupt line
	 */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);

	/*
	 * Establish Interrupt handler
	 */
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, adw_intr, sc,
				       sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt", sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s\n", intrstr);

	/*
	 * Attach all the sub-devices we can find
	 */
	adw_attach(sc);
}
/******************************************************************************/
