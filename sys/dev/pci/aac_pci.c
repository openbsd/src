/*	$OpenBSD: aac_pci.c,v 1.14 2004/11/23 04:02:25 marco Exp $	*/

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2000 Niklas Hallqvist
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
 *	$FreeBSD: /c/ncvs/src/sys/dev/aac/aac_pci.c,v 1.1 2000/09/13 03:20:34 msmith Exp $
 */

/*
 * This driver would not have rewritten for OpenBSD if it was not for the
 * hardware donation from Nocom.  I want to thank them for their support.
 * Of course, credit should go to Mike Smith for the original work he did
 * in the FreeBSD driver where I found lots of inspiration.
 * - Niklas Hallqvist
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/aacreg.h>
#include <dev/ic/aacvar.h>

int	aac_pci_probe(struct device *, void *, void *);
void	aac_pci_attach(struct device *, struct device *, void *);

/* Adaptec */
#define PCI_PRODUCT_ADP2_AACASR2200S   0x0285
#define PCI_PRODUCT_ADP2_AACASR2120S   0x0286
#define PCI_PRODUCT_ADP2_AACADPSATA2C  0x0289
#define PCI_PRODUCT_ADP2_AACADPSATA4C  0x0290
#define PCI_PRODUCT_ADP2_AACADPSATA6C  0x0291
#define PCI_PRODUCT_ADP2_AACADPSATA8C  0x0292
#define PCI_PRODUCT_ADP2_AACADPSATA16C 0x0293

/* Dell */
#define PCI_PRODUCT_ADP2_AACCERCSATA6C 0x0291
#define PCI_PRODUCT_ADP2_AACPERC320DC  0x0287

struct aac_sub_ident {
	u_int16_t subvendor;
	u_int16_t subdevice;
	char *desc;
} aac_sub_identifiers[] = {
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AACADPSATA2C, "Adaptec 1210SA" }, /* guess */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AACADPSATA4C, "Adaptec 2410SA" },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AACADPSATA6C, "Adaptec 2610SA" },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AACADPSATA8C, "Adaptec 2810SA" }, /* guess */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AACADPSATA16C, "Adaptec 21610SA" }, /* guess */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AACASR2120S, "Adaptec 2120S" },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AACASR2200S, "Adaptec 2200S" },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_ADP2_AACCERCSATA6C, "Dell CERC-SATA" },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_ADP2_AACPERC320DC, "Dell PERC 320/DC" },
	{ 0, 0, "" }
};

struct aac_ident {
	u_int16_t vendor;
	u_int16_t device;
	u_int16_t subvendor;
	u_int16_t subdevice;
	int	hwif;
} aac_identifiers[] = {
	/* Dell PERC 2/Si models */
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_2SI, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_2SI, AAC_HWIF_I960RX },
	/* Dell PERC 3/Di models */
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_2, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_3, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_SUB2, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_SUB3, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI_2, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_2_SUB, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI_3, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_3_SUB, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI_3, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_3_SUB2, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3DI_3, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3DI_3_SUB3, AAC_HWIF_I960RX },
	/* Dell PERC 3/Si models */
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3SI, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3SI, AAC_HWIF_I960RX },
	{ PCI_VENDOR_DELL, PCI_PRODUCT_DELL_PERC_3SI_2, PCI_VENDOR_DELL,
	    PCI_PRODUCT_DELL_PERC_3SI_2_SUB, AAC_HWIF_I960RX },
	/* Adaptec SATA RAID 2 channel XXX guess */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AACADPSATA2C, AAC_HWIF_I960RX },
	/* Adaptec SATA RAID 4 channel */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AACADPSATA4C, AAC_HWIF_I960RX },
	/* Adaptec SATA RAID 6 channel XXX guess */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AACADPSATA6C, AAC_HWIF_I960RX },
	/* Adaptec SATA RAID 8 channel XXX guess */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AACADPSATA8C, AAC_HWIF_I960RX },
	/* Adaptec SATA RAID 16 channel XXX guess */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AACADPSATA16C, AAC_HWIF_I960RX },
	/* Dell CERC-SATA */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_DELL,
	    PCI_PRODUCT_ADP2_AACCERCSATA6C, AAC_HWIF_I960RX },
	/* Dell PERC 320/DC */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_DELL,
	    PCI_PRODUCT_ADP2_AACPERC320DC, AAC_HWIF_I960RX },
	/* Adaptec ADP-2622 */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AAC2622, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AAC2622, AAC_HWIF_I960RX },
	/* Adaptec ADP-364 */
	{ PCI_VENDOR_DEC, PCI_PRODUCT_DEC_CPQ42XX, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AAC364, AAC_HWIF_STRONGARM },
	/* Adaptec ADP-3642 */
	{ PCI_VENDOR_DEC, PCI_PRODUCT_DEC_CPQ42XX, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AAC3642, AAC_HWIF_STRONGARM },
	/* Dell PERC 2/QC */
	{ PCI_VENDOR_DEC, PCI_PRODUCT_DEC_CPQ42XX, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_PERC_2QC, AAC_HWIF_STRONGARM },
	/* HP NetRAID-4M */
	{ PCI_VENDOR_DEC, PCI_PRODUCT_DEC_CPQ42XX, PCI_VENDOR_HP,
	    PCI_PRODUCT_HP_NETRAID_4M, AAC_HWIF_STRONGARM },
	/* Adaptec ASR-2120S */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AACASR2120S, AAC_HWIF_I960RX },
	/* Adaptec ASR-2200S */
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_ASR2200S, PCI_VENDOR_ADP2,
	    PCI_PRODUCT_ADP2_AACASR2200S, AAC_HWIF_I960RX },
	{ 0, 0, 0, 0 }
};

struct cfattach aac_pci_ca = {
	sizeof (struct aac_softc), aac_pci_probe, aac_pci_attach
};

/*
 * Determine whether this is one of our supported adapters.
 */
int
aac_pci_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
        struct pci_attach_args *pa = aux;
	struct aac_ident *m;
	u_int32_t subsysid;

	for (m = aac_identifiers; m->vendor != 0; m++)
		if (m->vendor == PCI_VENDOR(pa->pa_id) &&
		    m->device == PCI_PRODUCT(pa->pa_id)) {
			subsysid = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    PCI_SUBSYS_ID_REG);
			if (m->subvendor == PCI_VENDOR(subsysid) &&
			    m->subdevice == PCI_PRODUCT(subsysid))
				return (1);
		}
	return (0);
}

void
aac_pci_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	struct aac_softc *sc = (void *)self;
	u_int16_t command;
	bus_addr_t membase;
	bus_size_t memsize;
	pci_intr_handle_t ih;
	const char *intrstr;
	int state = 0;
	struct aac_ident *m;
	struct aac_sub_ident *subid;
	u_int32_t subsysid;

	printf(": ");
	subsysid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	if ((PCI_VENDOR(pa->pa_id) != PCI_VENDOR(subsysid)) ||
	    (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT(subsysid))) {
		for (subid = aac_sub_identifiers; subid->subvendor != 0;
		    subid++) {
			if (subid->subvendor == PCI_VENDOR(subsysid) &&
			    subid->subdevice == PCI_PRODUCT(subsysid)) {
				printf("%s ", subid->desc);
				break;
			}
		}
	}

	/*
	 * Verify that the adapter is correctly set up in PCI space.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	AAC_DPRINTF(AAC_D_MISC, ("pci command status reg 0x08x "));
	if (!(command & PCI_COMMAND_MASTER_ENABLE)) {
		printf("can't enable bus-master feature\n");
		goto bail_out;
	}
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf("memory window not available\n");
		goto bail_out;
	}

	/*
	 * Map control/status registers.
	 */
	if (pci_mapreg_map(pa, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0, &sc->sc_memt,
	    &sc->sc_memh, &membase, &memsize, AAC_REGSIZE)) {
		printf("can't find mem space\n");
		goto bail_out;
	}
	state++;

	if (pci_intr_map(pa, &ih)) {
		printf("couldn't map interrupt\n");
		goto bail_out;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, aac_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto bail_out;
	}
	state++;
	if (intrstr != NULL)
		printf("%s\n", intrstr);

	sc->sc_dmat = pa->pa_dmat;
 
	for (m = aac_identifiers; m->vendor != 0; m++)
		if (m->vendor == PCI_VENDOR(pa->pa_id) &&
		    m->device == PCI_PRODUCT(pa->pa_id)) {
			if (m->subvendor == PCI_VENDOR(subsysid) &&
			    m->subdevice == PCI_PRODUCT(subsysid)) {
				sc->sc_hwif = m->hwif;
				switch(sc->sc_hwif) {
				case AAC_HWIF_I960RX:
					AAC_DPRINTF(AAC_D_MISC,
					    ("set hardware up for i960Rx"));
					sc->sc_if = aac_rx_interface;
					break;

				case AAC_HWIF_STRONGARM:
					AAC_DPRINTF(AAC_D_MISC,
					    ("set hardware up for StrongARM"));
					sc->sc_if = aac_sa_interface;
					break;
				}
				break;
			}
		}

	if (aac_attach(sc))
		goto bail_out;

	return;

 bail_out:
	if (state > 1)
		pci_intr_disestablish(pc, sc->sc_ih);
	if (state > 0)
		bus_space_unmap(sc->sc_memt, sc->sc_memh, memsize);
	return;
}
