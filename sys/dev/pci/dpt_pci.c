/*	$OpenBSD: dpt_pci.c,v 1.9 2014/12/19 22:44:58 guenther Exp $	*/
/*	$NetBSD: dpt_pci.c,v 1.2 1999/09/29 17:33:02 ad Exp $	*/

/*
 * Copyright (c) 1999 Andy Doran <ad@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * PCI front-end for DPT EATA SCSI driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/endian.h>

#include <machine/bus.h>

#ifdef __NetBSD__
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#endif /* __OpenBSD__ */

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/dptreg.h>
#include <dev/ic/dptvar.h>

#define	PCI_CBMA	0x14	/* Configuration base memory address */
#define	PCI_CBIO	0x10	/* Configuration base I/O address */

#ifdef __NetBSD__
int	dpt_pci_match(struct device *, struct cfdata *, void *);
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
int	dpt_pci_match(struct device *, void *, void *);
#endif /* __OpenBSD__ */
void	dpt_pci_attach(struct device *, struct device *, void *);
int	dpt_activate(struct device *, int);

struct cfattach dpt_pci_ca = {
	sizeof(struct dpt_softc), dpt_pci_match, dpt_pci_attach, NULL,
	dpt_activate
};

int
dpt_pci_match(parent, match, aux)
	struct device *parent;
#ifdef __NetBSD__
	struct cfdata *match;
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	void *match;
#endif /* __OpenBSD__ */
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_DPT && 
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_DPT_SC_RAID)
		return (1);
 
	return (0);
}

void
dpt_pci_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct pci_attach_args *pa;
	struct dpt_softc *sc;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	const char *intrstr;

	sc = (struct dpt_softc *)self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;
	printf(": ");

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0, &sc->sc_iot, 
	    &sc->sc_ioh, NULL, NULL, 0)) {
		printf("can't map i/o space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf("can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
#ifdef __NetBSD__
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, dpt_intr, sc);
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, dpt_intr, sc,
				       sc->sc_dv.dv_xname);
#endif /* __OpenBSD__ */
	if (sc->sc_ih == NULL) {
		printf("can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	/* Read the EATA configuration */
	if (dpt_readcfg(sc)) {
		printf("%s: readcfg failed - see dpt(4)\n", 
		    sc->sc_dv.dv_xname);
		return;	
	}

	/* Now attach to the bus-independant code */
	dpt_init(sc, intrstr);
}

int
dpt_activate(struct device *self, int act)
{
	int ret = 0;

	switch (act) {
	case DVACT_POWERDOWN:
		ret = config_activate_children(self, act);
		dpt_shutdown(self);
		break;
	default:
		ret = config_activate_children(self, act);
		break;
	}

	return (ret);
}
