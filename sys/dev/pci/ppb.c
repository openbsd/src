/*	$OpenBSD: ppb.c,v 1.19 2007/05/21 22:10:45 kettenis Exp $	*/
/*	$NetBSD: ppb.c,v 1.16 1997/06/06 23:48:05 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

struct ppb_softc {
	struct device sc_dev;		/* generic device glue */
	pci_chipset_tag_t sc_pc;	/* our PCI chipset... */
	pcitag_t sc_tag;		/* ...and tag. */
	pci_intr_handle_t sc_ih[4];
};

int	ppbmatch(struct device *, void *, void *);
void	ppbattach(struct device *, struct device *, void *);

struct cfattach ppb_ca = {
	sizeof(struct ppb_softc), ppbmatch, ppbattach
};

struct cfdriver ppb_cd = {
	NULL, "ppb", DV_DULL
};

int	ppbprint(void *, const char *pnp);

int
ppbmatch(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	/*
	 * This device is mislabeled.  It is not a PCI bridge.
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_VIATECH &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_VIATECH_VT82C586_PWR)
		return (0);
	/*
	 * Check the ID register to see that it's a PCI bridge.
	 * If it is, we assume that we can deal with it; it _should_
	 * work in a standardized way...
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_PCI)
		return (1);

	return (0);
}

void
ppbattach(struct device *parent, struct device *self, void *aux)
{
	struct ppb_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	struct pcibus_attach_args pba;
	pcireg_t busdata;
	int pin;

	printf("\n");

	sc->sc_pc = pc;
	sc->sc_tag = pa->pa_tag;

	busdata = pci_conf_read(pc, pa->pa_tag, PPB_REG_BUSINFO);

	if (PPB_BUSINFO_SECONDARY(busdata) == 0) {
		printf("%s: not configured by system firmware\n",
		    self->dv_xname);
		return;
	}

	for (pin = PCI_INTERRUPT_PIN_A; pin <= PCI_INTERRUPT_PIN_D; pin++) {
		pa->pa_intrpin = pa->pa_rawintrpin = pin;
		pa->pa_intrline = 0;
		pci_intr_map(pa, &sc->sc_ih[pin - PCI_INTERRUPT_PIN_A]);
	}

#if 0
	/*
	 * XXX can't do this, because we're not given our bus number
	 * (we shouldn't need it), and because we've no way to
	 * decompose our tag.
	 */
	/* sanity check. */
	if (pa->pa_bus != PPB_BUSINFO_PRIMARY(busdata))
		panic("ppbattach: bus in tag (%d) != bus in reg (%d)",
		    pa->pa_bus, PPB_BUSINFO_PRIMARY(busdata));
#endif

	/*
	 * Attach the PCI bus that hangs off of it.
	 *
	 * XXX Don't pass-through Memory Read Multiple.  Should we?
	 * XXX Consult the spec...
	 */
	pba.pba_busname = "pci";
	pba.pba_iot = pa->pa_iot;
	pba.pba_memt = pa->pa_memt;
	pba.pba_dmat = pa->pa_dmat;
	pba.pba_pc = pc;
#if 0
	pba.pba_flags = pa->pa_flags & ~PCI_FLAGS_MRM_OKAY;
#endif
	pba.pba_domain = pa->pa_domain;
	pba.pba_bus = PPB_BUSINFO_SECONDARY(busdata);
	pba.pba_bridgeih = sc->sc_ih;
	pba.pba_bridgetag = &sc->sc_tag;
	pba.pba_intrswiz = pa->pa_intrswiz;
	pba.pba_intrtag = pa->pa_intrtag;

	config_found(self, &pba, ppbprint);
}

int
ppbprint(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	/* only PCIs can attach to PPBs; easy. */
	if (pnp)
		printf("pci at %s", pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
