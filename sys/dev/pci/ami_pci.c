/*	$OpenBSD: ami_pci.c,v 1.22 2004/12/26 00:35:42 marco Exp $	*/

/*
 * Copyright (c) 2001 Michael Shalayeff
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/amireg.h>
#include <dev/ic/amivar.h>

#define	AMI_BAR		0x10
#define	AMI_PCI_MEMSIZE	0x1000
#define	AMI_SUBSYSID	0x2c
#define	PCI_EBCR	0x40
#define	AMI_WAKEUP	0x64

/* "Quartz" i960 Config space */
#define	AMI_PCI_INIT	0x9c
#define		AMI_INITSTAT(i)	(((i) >>  8) & 0xff)
#define		AMI_INITTARG(i)	(((i) >> 16) & 0xff)
#define		AMI_INITCHAN(i)	(((i) >> 24) & 0xff)
#define	AMI_PCI_SIG	0xa0
#define		AMI_SIGNATURE_1	0xcccc		/* older adapters */
#define		AMI_SIGNATURE_2	0x3344		/* newer adapters */
#define	AMI_PCI_SGL	0xa4
#define		AMI_SGL_LHC	0x00000299
#define		AMI_SGL_HLC	0x00000199

int	ami_pci_find_device(void *);
int	ami_pci_match(struct device *, void *, void *);
void	ami_pci_attach(struct device *, struct device *, void *);

struct cfattach ami_pci_ca = {
	sizeof(struct ami_softc), ami_pci_match, ami_pci_attach
};

static const
struct	ami_pci_device {
	int	vendor;
	int	product;
	int	flags;
#define	AMI_CHECK_SIGN	0x001
} ami_pci_devices[] = {
	{ PCI_VENDOR_AMI,	PCI_PRODUCT_AMI_MEGARAID,	0 },
	{ PCI_VENDOR_AMI,	PCI_PRODUCT_AMI_MEGARAID428,	AMI_BROKEN },
	{ PCI_VENDOR_AMI,	PCI_PRODUCT_AMI_MEGARAID434,	AMI_BROKEN },
	{ PCI_VENDOR_DELL,	PCI_PRODUCT_DELL_PERC_4DI,	0 },
	{ PCI_VENDOR_DELL,	PCI_PRODUCT_DELL_PERC_4DI_2,	0 },
	{ PCI_VENDOR_DELL,	PCI_PRODUCT_DELL_PERC_4EDI,	0 },	
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_80960RP_ATU,
	    AMI_CHECK_SIGN | AMI_BROKEN },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID,	0 },
	{ 0 }
};

static const
struct	ami_pci_subsys {
	pcireg_t id;
	char	name[12];
} ami_pci_subsys[] = {
	/* only those of a special name are listed here */
	{ 0x09A0101E,	"Dell 466v1" },
	{ 0x11111111,	"Dell 466v2" },
	{ 0x11121111,	"Dell 438" },
	{ 0x11111028,	"Dell 466v3" },
	{ 0x10c6103c,	"HP 438" },
	{ 0x10c7103c,	"HP T5/T6" },
	{ 0x10cc103c,	"HP T7" },
	{ 0x10cd103c,	"HP 466" },
	{ 0x45231000,	"LSI 523" },
	{ 0 }
};

static const
struct ami_pci_vendor {
	u_int16_t id;
	char name[8];
} ami_pci_vendors[] = {
	{ 0x101e, "AMI" },
	{ 0x1028, "Dell" },
	{ 0x103c, "HP" },
	{ 0x1000, "LSI" },
	{ 0 }
};

int ami_pci_find_device(aux)
	void *aux;
{
	int i;
	struct pci_attach_args *pa = aux;

	for (i = 0; ami_pci_devices[i].vendor; i++) {
		if (ami_pci_devices[i].vendor == PCI_VENDOR(pa->pa_id) &&
		    ami_pci_devices[i].product == PCI_PRODUCT(pa->pa_id)) {
#ifdef AMI_DEBUG
		    	printf(" apfd %i ", i);
#endif /* AMI_DEBUG */
			return (i);
		}
	}

	return (-1);
}

int
ami_pci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	int i;
	pcireg_t sig;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_I2O)
		return (0);

	if ((i = ami_pci_find_device(aux)) != -1) {
#ifdef AMI_DEBUG
		printf("\nvendor: %04x  product: %04x\n",
			ami_pci_devices[i].vendor,
			ami_pci_devices[i].product);
#endif /* AMI_DEBUG */

		if (!(ami_pci_devices[i].flags & AMI_CHECK_SIGN))
			return (1);
		/* some cards have 0x11223344, but some only 16bit */
		sig = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    AMI_PCI_SIG) & 0xffff;
		if (sig == AMI_SIGNATURE_1 ||
		    sig == AMI_SIGNATURE_2)
			return (1);
	}

	return (0);
}

void
ami_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ami_softc *sc = (struct ami_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr, *model = NULL, *lhc;
	const struct ami_pci_subsys *ssp;
	bus_size_t size;
	pcireg_t csr;
	int i;
#if 0
	/* reset */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_EBCR,
	    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_EBCR) | 0x20);
	pci_conf_write(pa->pa_pc, pa->pa_tag, AMI_WAKEUP, 0);
#endif
	csr = pci_mapreg_type(pa->pa_pc, pa->pa_tag, AMI_BAR);
	csr |= PCI_MAPREG_MEM_TYPE_32BIT;
	if (pci_mapreg_map(pa, AMI_BAR, csr, 0,
	    &sc->iot, &sc->ioh, NULL, &size, AMI_PCI_MEMSIZE)) {
		printf(": can't map controller pci space\n");
		return;
	}

	if (csr == PCI_MAPREG_TYPE_IO) {
		sc->sc_init = ami_schwartz_init;
		sc->sc_exec = ami_schwartz_exec;
		sc->sc_done = ami_schwartz_done;
	} else {
		sc->sc_init = ami_quartz_init;
		sc->sc_exec = ami_quartz_exec;
		sc->sc_done = ami_quartz_done;
	}
	sc->dmat = pa->pa_dmat;

	/* enable bus mastering (should not it be mi?) */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		bus_space_unmap(sc->iot, sc->ioh, size);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, ami_intr, sc,
	    sc->sc_dev.dv_xname);
	if (!sc->sc_ih) {
		printf(": can't establish interrupt");
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->iot, sc->ioh, size);
	}

	printf(": %s", intrstr);

	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	for (ssp = ami_pci_subsys; ssp->id; ssp++)
		if (ssp->id == csr) {
			model = ssp->name;
			break;
		}

	if (!model && PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMI)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_AMI_MEGARAID428:
			model = "AMI 428";
			break;
		case PCI_PRODUCT_AMI_MEGARAID434:
			model = "AMI 434";
			break;
		}

	/* XXX 438 is netraid 3si for hp cards, but we get to know
	   they are hp too late in md code */

	if (!model) {
		const struct ami_pci_vendor *vp;
		static char modelbuf[32];

		for (vp = ami_pci_vendors;
		     vp->id && vp->id != (csr & 0xffff); vp++);
		if (vp->id)
			snprintf(modelbuf, sizeof modelbuf, "%s %x", vp->name,
			    (csr >> 16) & 0xffff);
		else
			snprintf(modelbuf, sizeof modelbuf, "unknown 0x%08x",
			    csr);
		model = modelbuf;
	}

	switch (pci_conf_read(pa->pa_pc, pa->pa_tag, AMI_PCI_SGL)) {
	case AMI_SGL_LHC:	lhc = "64b/lhc";	break;
	case AMI_SGL_HLC:	lhc = "64b/hlc";	break;
	default:		lhc = "32b";
	}

	if ((i = ami_pci_find_device(aux)) != -1) {
		if (ami_pci_devices[i].flags & AMI_BROKEN)
			sc->sc_quirks = AMI_BROKEN;
		else
			sc->sc_quirks = 0x0000;
	}
	else {
		/* this device existed at _match() should never happen */
		panic("ami device dissapeared between match() and attach()\n");
	}

	printf(" %s/%s\n%s", model, lhc, sc->sc_dev.dv_xname);

	if (ami_attach(sc)) {
		pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
		sc->sc_ih = NULL;
		bus_space_unmap(sc->iot, sc->ioh, size);
	}
}
