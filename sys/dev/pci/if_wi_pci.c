/*	$OpenBSD: if_wi_pci.c,v 1.30 2002/09/12 03:48:31 millert Exp $	*/

/*
 * Copyright (c) 2001, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI attachment for the Wavelan driver.  There are two basic types
 * of PCI card supported:
 *
 * 1) Cards based on the Prism2.5 Mini-PCI chipset
 * 2) Cards that use a dumb ISA->PCI bridge
 *
 * Only the first type are "true" PCI cards.
 *
 * The latter are often sold as "PCI wireless card adapters" and are
 * sold by several vendors.  Most are simply rebadged versions of the
 * Eumitcom WL11000P or Global Sun Technology GL24110P02.
 * These cards use the PLX 9052 dumb bridge chip to connect a PCMCIA
 * wireless card to the PCI bus.  Because it is a dumb bridge and
 * not a true PCMCIA bridge, the PCMCIA subsystem is not involved
 * (or even required).  The PLX 9052 provides multiple PCI address
 * space mappings.  The primary mappings at PCI registers 0x10 (mem)
 * and 0x14 (I/O) are for the PLX chip itself, *NOT* the PCMCIA card.
 * The mem and I/O spaces for the PCMCIA card are mapped to 0x18 and
 * 0x1C respectively.
 * The PLX 9050/9052 datasheet may be downloaded from PLX at
 *	http://www.plxtech.com/products/toolbox/9050.htm
 *
 * This driver also supports the TMD7160 dumb bridge chip which is used
 * on some versions of the NDC/Sohoware NCP130.  The TMD7160 provides
 * two PCI I/O registers.  The first, at 0x14, maps to the Prism2 COR.
 * The second, at 0x18, is for the Prism2 chip itself.
 * The datasheet for the TMD7160 does not seem to be publicly available.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_ieee80211.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/if_wireg.h>
#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wivar.h>

/* For printing CIS of the actual PCMCIA card */
#define CIS_MFG_NAME_OFFSET	0x16
#define CIS_INFO_SIZE		256

const struct wi_pci_product *wi_pci_lookup(struct pci_attach_args *pa);
int	wi_pci_match(struct device *, void *, void *);
void	wi_pci_attach(struct device *, struct device *, void *);
int	wi_pci_plx_attach(struct pci_attach_args *pa, struct wi_softc *sc);
int	wi_pci_tmd_attach(struct pci_attach_args *pa, struct wi_softc *sc);
int	wi_pci_native_attach(struct pci_attach_args *pa, struct wi_softc *sc);
int	wi_pci_common_attach(struct pci_attach_args *pa, struct wi_softc *sc);
void	wi_pci_plx_print_cis(struct wi_softc *);

struct cfattach wi_pci_ca = {
	sizeof (struct wi_softc), wi_pci_match, wi_pci_attach
};

static const struct wi_pci_product {
	pci_vendor_id_t pp_vendor;
	pci_product_id_t pp_product;
	int (*pp_attach)(struct pci_attach_args *pa, struct wi_softc *sc);
} wi_pci_products[] = {
	{ PCI_VENDOR_GLOBALSUN, PCI_PRODUCT_GLOBALSUN_GL24110P, wi_pci_plx_attach },
	{ PCI_VENDOR_GLOBALSUN, PCI_PRODUCT_GLOBALSUN_GL24110P02, wi_pci_plx_attach },
	{ PCI_VENDOR_EUMITCOM, PCI_PRODUCT_EUMITCOM_WL11000P, wi_pci_plx_attach },
	{ PCI_VENDOR_USR2, PCI_PRODUCT_USR2_WL11000P, wi_pci_plx_attach },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CRWE777A, wi_pci_plx_attach },
	{ PCI_VENDOR_NETGEAR, PCI_PRODUCT_NETGEAR_MA301, wi_pci_plx_attach },
	{ PCI_VENDOR_EFFICIENTNETS, PCI_PRODUCT_EFFICIENTNETS_SS1023, wi_pci_plx_attach },
	{ PCI_VENDOR_NDC, PCI_PRODUCT_NDC_NCP130, wi_pci_plx_attach },
	{ PCI_VENDOR_NDC, PCI_PRODUCT_NDC_NCP130A2, wi_pci_tmd_attach },
	{ PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_MINI_PCI_WLAN, wi_pci_native_attach },
	{ 0, 0, 0 }
};

const struct wi_pci_product *
wi_pci_lookup(struct pci_attach_args *pa)
{
	const struct wi_pci_product *pp;

	for (pp = wi_pci_products; pp->pp_product != 0; pp++) {
		if (PCI_VENDOR(pa->pa_id) == pp->pp_vendor && 
		    PCI_PRODUCT(pa->pa_id) == pp->pp_product)
			return (pp);
	}

	return (NULL);
}

int
wi_pci_match(struct device *parent, void *match, void *aux)
{
	return (wi_pci_lookup(aux) != NULL);
}

void
wi_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct wi_softc *sc = (struct wi_softc *)self;
	struct pci_attach_args *pa = aux;
	const struct wi_pci_product *pp;

	pp = wi_pci_lookup(pa);
	if (pp->pp_attach(pa, sc) != 0)
		return;
	wi_attach(sc);
}

int
wi_pci_plx_attach(struct pci_attach_args *pa, struct wi_softc *sc)
{
	bus_space_handle_t localh, ioh, memh;
	bus_space_tag_t localt;
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_tag_t memt = pa->pa_memt;
	bus_addr_t localbase;
	bus_size_t localsize;
	u_int32_t intcsr;

	if (pci_mapreg_map(pa, WI_PLX_MEMRES, PCI_MAPREG_TYPE_MEM, 0,
	    &memt, &memh, NULL, NULL, 0) != 0) {
		printf(": can't map mem space\n");
		return (ENXIO);
	}
	sc->wi_ltag = memt;
	sc->wi_lhandle = memh;

	if (pci_mapreg_map(pa, WI_PLX_IORES,
	    PCI_MAPREG_TYPE_IO, 0, &iot, &ioh, NULL, NULL, 0) != 0) {
		printf(": can't map I/O space\n");
		return (ENXIO);
	}
	sc->wi_btag = iot;
	sc->wi_bhandle = ioh;

	/*
	 * Some cards, such as the PLX version of the NDC NCP130,
	 * don't have the PLX local registers mapped.  In general
	 * this is OK since on those cards the serial EEPROM has
	 * already set things up for us.
	 * As such, we don't consider an error here to be fatal.
	 */
	localsize = 0;
	if (pci_mapreg_type(pa->pa_pc, pa->pa_tag, WI_PLX_LOCALRES)
	    == PCI_MAPREG_TYPE_IO) {
		if (pci_io_find(pa->pa_pc, pa->pa_tag,
		    WI_PLX_LOCALRES, &localbase, &localsize) != 0)
			printf(": can't find PLX I/O space\n");
		if (localsize != 0) {
			if (bus_space_map(pa->pa_iot, localbase,
			    localsize, 0, &localh) != 0) {
				printf(": can't map PLX I/O space\n");
				localsize = 0;
			} else
				localt = pa->pa_iot;
		}
	}

	if (wi_pci_common_attach(pa, sc) != 0)
		return (ENXIO);

	if (localsize != 0) {
		intcsr = bus_space_read_4(localt, localh,
		    WI_PLX_INTCSR);

		/*
		 * The Netgear MA301 has local interrupt 1 active
		 * when there is no card in the adapter.  We bail
		 * early in this case since our attempt to check
		 * for the presence of a card later will hang the
		 * MA301.
		 */
		if (intcsr & WI_PLX_LINT1STAT) {
			printf("\n%s: no PCMCIA card detected in bridge card\n",
			    WI_PRT_ARG(sc));
			return (ENXIO);
		}

		/*
		 * Enable PCI interrupts on the PLX chip if they are
		 * not already enabled. On most adapters the serial
		 * EEPROM has done this for us but some (such as
		 * the Netgear MA301) do not.
		 */
		if (!(intcsr & WI_PLX_INTEN)) {
			intcsr |= WI_PLX_INTEN;
			bus_space_write_4(localt, localh, WI_PLX_INTCSR,
			    intcsr);
		}
	}

	/*
	 * Enable I/O mode and level interrupts on the PCMCIA card.
	 * The PCMCIA card's COR is the first byte after the CIS.
	 */
	bus_space_write_1(memt, memh, WI_PLX_COR_OFFSET, WI_COR_IOMODE);
	sc->wi_cor_offset = WI_PLX_COR_OFFSET;

	if (localsize != 0) {
		/*
		 * Test the presence of a wi(4) card by writing
		 * a magic number to the first software support
		 * register and then reading it back.
		 */
		CSR_WRITE_2(sc, WI_SW0, WI_DRVR_MAGIC);
		DELAY(1000);
		if (CSR_READ_2(sc, WI_SW0) != WI_DRVR_MAGIC) {
			printf("\n%s: no PCMCIA card detected in bridge card\n",
			    WI_PRT_ARG(sc));
			return (ENXIO);
		}

		/* Unmap registers we no longer need access to. */
		bus_space_unmap(localt, localh, localsize);

		/* Print PCMCIA card's CIS strings. */
		wi_pci_plx_print_cis(sc);
	}

	return (0);
}

int
wi_pci_tmd_attach(struct pci_attach_args *pa, struct wi_softc *sc)
{
	bus_space_handle_t localh, ioh;
	bus_space_tag_t localt;
	bus_space_tag_t iot = pa->pa_iot;
	bus_size_t localsize;

	if (pci_mapreg_map(pa, WI_TMD_LOCALRES, PCI_MAPREG_TYPE_IO,
	    0, &localt, &localh, NULL, &localsize, 0) != 0) {
		printf(": can't map TMD I/O space\n");
		return (ENXIO);
	}
	sc->wi_ltag = localt;
	sc->wi_lhandle = localh;

	if (pci_mapreg_map(pa, WI_TMD_IORES, PCI_MAPREG_TYPE_IO,
	    0, &iot, &ioh, NULL, NULL, 0) != 0) {
		printf(": can't map I/O space\n");
		return (ENXIO);
	}
	sc->wi_btag = iot;
	sc->wi_bhandle = ioh;

	if (wi_pci_common_attach(pa, sc) != 0)
		return (ENXIO);

	/* 
	 * Enable I/O mode and level interrupts on the embedded PCMCIA
	 * card.  The PCMCIA card's COR is the first byte of BAR 0.
	 */
	bus_space_write_1(localt, localh, 0, WI_COR_IOMODE);
	sc->wi_cor_offset = 0;

	return (0);
}

int
wi_pci_native_attach(struct pci_attach_args *pa, struct wi_softc *sc)
{
	bus_space_handle_t ioh;
	bus_space_tag_t iot = pa->pa_iot;

	if (pci_mapreg_map(pa, WI_PCI_CBMA, PCI_MAPREG_TYPE_MEM,
	    0, &iot, &ioh, NULL, NULL, 0) != 0) {
		printf(": can't map mem space\n");
		return (ENXIO);
	}
	sc->wi_ltag = iot;
	sc->wi_lhandle = ioh;
	sc->wi_btag = iot;
	sc->wi_bhandle = ioh;
	sc->sc_pci = 1;

	if (wi_pci_common_attach(pa, sc) != 0)
		return (ENXIO);

	/* Do a soft reset of the HFA3842 MAC core */
	bus_space_write_2(iot, ioh, WI_PCI_COR_OFFSET, WI_COR_SOFT_RESET);
	DELAY(100*1000); /* 100 m sec */
	bus_space_write_2(iot, ioh, WI_PCI_COR_OFFSET, WI_COR_CLEAR);
	DELAY(100*1000); /* 100 m sec */
	sc->wi_cor_offset = WI_PCI_COR_OFFSET;

	return (0);
}

int
wi_pci_common_attach(struct pci_attach_args *pa, struct wi_softc *sc)
{
	pci_intr_handle_t ih;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t csr;
	const char *intrstr;

	/* Enable the card. */
	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* Make sure interrupts are disabled. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return (ENXIO);
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, wi_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return (ENXIO);
	}
	printf(": %s", intrstr);

	return (0);
}

void
wi_pci_plx_print_cis(struct wi_softc *sc)
{
	int i, stringno;
	char cisbuf[CIS_INFO_SIZE];
	char *cis_strings[3];
	u_int8_t value;
	const u_int8_t cis_magic[] = {
		0x01, 0x03, 0x00, 0x00, 0xff, 0x17, 0x04, 0x67
	};

	/* Make sure the CIS data is valid. */
	for (i = 0; i < 8; i++) {
		value = bus_space_read_1(sc->wi_ltag, sc->wi_lhandle, i * 2);
		if (value != cis_magic[i])
			return;
	}

	cis_strings[0] = cisbuf;
	stringno = 0;
	for (i = 0; i < CIS_INFO_SIZE && stringno < 3; i++) {
		cisbuf[i] = bus_space_read_1(sc->wi_ltag,
		    sc->wi_lhandle, (CIS_MFG_NAME_OFFSET + i) * 2);
		if (cisbuf[i] == '\0' && ++stringno < 3)
			cis_strings[stringno] = &cisbuf[i + 1];
	}
	cisbuf[CIS_INFO_SIZE - 1] = '\0';
	printf("\n%s: \"%s, %s, %s\"", WI_PRT_ARG(sc),
	    cis_strings[0], cis_strings[1], cis_strings[2]);
}
