/*	$OpenBSD: if_ne_pcmcia.c,v 1.2 1998/10/14 07:34:43 fgsch Exp $	*/
/*	$NetBSD: if_ne_pcmcia.c,v 1.17 1998/08/15 19:00:04 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
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
#include <sys/select.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if_types.h>
#include <net/if.h>
#include <net/if_media.h>
#ifdef __NetBSD__
#include <net/if_ether.h>
#else
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>

int ne_pcmcia_match __P((struct device *, void *, void *));
void ne_pcmcia_attach __P((struct device *, struct device *, void *));

int	ne_pcmcia_enable __P((struct dp8390_softc *));
void	ne_pcmcia_disable __P((struct dp8390_softc *));

struct ne_pcmcia_softc {
	struct ne2000_softc sc_ne2000;		/* real "ne2000" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o information */
	int sc_asic_io_window;			/* i/o window for ASIC */
	int sc_nic_io_window;			/* i/o window for NIC */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handle */
};

struct cfattach ne_pcmcia_ca = {
	sizeof(struct ne_pcmcia_softc), ne_pcmcia_match, ne_pcmcia_attach
};

struct ne2000dev {
    char *name;
    int32_t manufacturer;
    int32_t product;
    char *cis_info[4];
    int function;
    int enet_maddr;
    unsigned char enet_vendor[3];
} ne2000devs[] = {
    { PCMCIA_STR_PREMAX_PE200,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_PREMAX_PE200,
      0, 0x07f0, { 0x00, 0x20, 0xe0 } },

    { PCMCIA_STR_DIGITAL_DEPCMXX,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DIGITAL_DEPCMXX,
      0, 0x0ff0, { 0x00, 0x00, 0xe8 } },

    { PCMCIA_STR_PLANET_SMARTCOM2000,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_PLANET_SMARTCOM2000,
      0, 0xff0, { 0x00, 0x00, 0xe8 } },

    { PCMCIA_STR_DLINK_DE650,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE650,
      0, 0x0040, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_DLINK_DE660,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE660,
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_RPTI_EP401,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_RPTI_EP401,
      0, -1, { 0x00, 0x40, 0x95 } },

    { PCMCIA_STR_ACCTON_EN2212,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_ACCTON_EN2212,
      0, 0x0ff0, { 0x00, 0x00, 0xe8 } },

    /*
     * You have to add new entries which contains
     * PCMCIA_VENDOR_INVALID and/or PCMCIA_PRODUCT_INVALID 
     * in front of this comment.
     *
     * There are cards which use a generic vendor and product id but needs
     * a different handling depending on the cis_info, so ne2000_match
     * needs a table where the exceptions comes first and then the normal
     * product and vendor entries.
     */

    { PCMCIA_STR_IBM_INFOMOVER,
      PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_INFOMOVER,
      PCMCIA_CIS_IBM_INFOMOVER,
      0, 0x0ff0, { 0x08, 0x00, 0x5a } },

    { PCMCIA_STR_LINKSYS_ECARD_1, 
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
      PCMCIA_CIS_LINKSYS_ECARD_1, 
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_LINKSYS_COMBO_ECARD, 
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_LINKSYS_COMBO_ECARD, 
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_LINKSYS_TRUST_COMBO_ECARD,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_TRUST_COMBO_ECARD,
      PCMCIA_CIS_LINKSYS_TRUST_COMBO_ECARD,
      0, 0x0120, { 0x20, 0x04, 0x49 } },

    /* Although the comments above say to put VENDOR/PRODUCT INVALID IDs
       above this list, we need to keep this one below the ECARD_1, or else
       both will match the same more-generic entry rather than the more
       specific one above with proper vendor and product IDs. */
    { PCMCIA_STR_LINKSYS_ECARD_2, 
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_LINKSYS_ECARD_2,
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_IODATA_PCLAT,
      PCMCIA_VENDOR_IODATA, PCMCIA_PRODUCT_IODATA_PCLAT,
      PCMCIA_CIS_IODATA_PCLAT,
      /* two possible location, 0x01c0 or 0x0ff0 */
      0, -1, { 0x00, 0xa0, 0xb0 } },

    { PCMCIA_STR_DAYNA_COMMUNICARD_E_1,
      PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_1,
      PCMCIA_CIS_DAYNA_COMMUNICARD_E_1,
      0, 0x0110, { 0x00, 0x80, 0x19 } },

    { PCMCIA_STR_DAYNA_COMMUNICARD_E_2,
      PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_2,
      PCMCIA_CIS_DAYNA_COMMUNICARD_E_2,
      0, -1, { 0x00, 0x80, 0x19 } },
#if 0
    /* the rest of these are stolen from the linux pcnet pcmcia device
       driver.  Since I don't know the manfid or cis info strings for
       any of them, they're not compiled in until I do. */
    { "Allied Telesis LA-PCM",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x00, 0xf4 } },
    { "APEX MultiCard",
      0x0000, 0x0000, NULL, NULL, 0,
      0x03f4, { 0x00, 0x20, 0xe5 } },
    { "ASANTE FriendlyNet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x4910, { 0x00, 0x00, 0x94 } },
    { "Danpex EN-6200P2",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0xc7 } },
    { "DataTrek NetCard",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x20, 0xe8 } },
    { "Dayna CommuniCard E",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x80, 0x19 } },
    { "EP-210 Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0x33 } },
    { "Epson EEN10B",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x00, 0x48 } },
    { "ELECOM Laneed LD-CDWA",
      0x0000, 0x0000, NULL, NULL, 0,
      0x00b8, { 0x08, 0x00, 0x42 } },
    { "Grey Cell GCS2220",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0000, { 0x00, 0x47, 0x43 } },
    { "Hypertec Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x01c0, { 0x00, 0x40, 0x4c } },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x08, 0x00, 0x5a } },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x04, 0xac } },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x06, 0x29 } },
    { "IBM FME",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0374, { 0x00, 0x04, 0xac } },
    { "IBM FME",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0374, { 0x08, 0x00, 0x5a } },
    { "Katron PE-520",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0xf6 } },
    { "Kingston KNE-PCM/x",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0xc0, 0xf0 } },
    { "Kingston KNE-PCM/x",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0xe2, 0x0c, 0x0f } },
    { "Kingston KNE-PC2",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0180, { 0x00, 0xc0, 0xf0 } },
    { "Longshine LCS-8534",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0000, { 0x08, 0x00, 0x00 } },
    { "Maxtech PCN2000",
      0x0000, 0x0000, NULL, NULL, 0,
      0x5000, { 0x00, 0x00, 0xe8 } },
    { "NDC Instant-Link",
      0x0000, 0x0000, NULL, NULL, 0,
      0x003a, { 0x00, 0x80, 0xc6 } },
    { "NE2000 Compatible",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0xa0, 0x0c } },
    { "Network General Sniffer",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x00, 0x65 } },
    { "Panasonic VEL211",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x80, 0x45 } },
    { "SCM Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x20, 0xcb } },
    { "Socket EA",
      0x0000, 0x0000, NULL, NULL, 0,
      0x4000, { 0x00, 0xc0, 0x1b } },
    { "Volktek NPL-402CT",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0060, { 0x00, 0x40, 0x05 } },
#endif
};

#define	NE2000_NDEVS	(sizeof(ne2000devs) / sizeof(ne2000devs[0]))

#define ne2000_match(card, fct, n) \
((((((card)->manufacturer != PCMCIA_VENDOR_INVALID) && \
    ((card)->manufacturer == ne2000devs[(n)].manufacturer) && \
    ((card)->product != PCMCIA_PRODUCT_INVALID) && \
    ((card)->product == ne2000devs[(n)].product)) || \
   ((ne2000devs[(n)].cis_info[0]) && (ne2000devs[(n)].cis_info[1]) && \
    (strcmp((card)->cis1_info[0], ne2000devs[(n)].cis_info[0]) == 0) && \
    (strcmp((card)->cis1_info[1], ne2000devs[(n)].cis_info[1]) == 0))) && \
  ((fct) == ne2000devs[(n)].function))? \
 &ne2000devs[(n)]:NULL)

int
ne_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pcmcia_attach_args *pa = aux;
	int i;

	for (i = 0; i < NE2000_NDEVS; i++) {
		if (ne2000_match(pa->card, pa->pf->number, i))
			return (1);
	}

	return (0);
}

void
ne_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ne_pcmcia_softc *psc = (void *) self;
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct ne2000dev *ne_dev;
	struct pcmcia_mem_handle pcmh;
	bus_addr_t offset;
	int i, j, mwindow;
	u_int8_t myea[6], *enaddr = NULL;

	psc->sc_pf = pa->pf;
	cfe = pa->pf->cfe_head.sqh_first;

#if 0
	/*
	 * Some ne2000 driver's claim to have memory; others don't.
	 * Since I don't care, I don't check.
	 */

	if (cfe->num_memspace != 1) {
		printf(": unexpected number of memory spaces "
		    " %d should be 1\n", cfe->num_memspace);
		return;
	}
#endif

	if (cfe->num_iospace == 1) {
		if (cfe->iospace[0].length != NE2000_NPORTS) {
			printf(": unexpected I/O space configuration\n");
			return;
		}
	} else if (cfe->num_iospace == 2) {
		/*
		 * Some cards report a separate space for NIC and ASIC.
		 * This make some sense, but we must allocate a single
		 * NE2000_NPORTS-sized chunk, due to brain damaged
		 * address decoders on some of these cards.
		 */
		if ((cfe->iospace[0].length + cfe->iospace[1].length) !=
		    NE2000_NPORTS) {
			printf(": unexpected I/O space configuration\n");
			return;
		}
	} else {
		printf(": unexpected number of i/o spaces %d"
		    " should be 1 or 2\n", cfe->num_iospace);
	}

	if (pcmcia_io_alloc(pa->pf, 0, NE2000_NPORTS, NE2000_NPORTS,
	    &psc->sc_pcioh)) {
		printf(": can't alloc i/o space\n");
		return;
	}

	dsc->sc_regt = psc->sc_pcioh.iot;
	dsc->sc_regh = psc->sc_pcioh.ioh;

	nsc->sc_asict = psc->sc_pcioh.iot;
	if (bus_space_subregion(dsc->sc_regt, dsc->sc_regh,
	    NE2000_ASIC_OFFSET, NE2000_ASIC_NPORTS,
	    &nsc->sc_asich)) {
		printf(": can't get subregion for asic\n");
		return;
	}

#if 0
	/* Set up power management hooks. */
	dsc->sc_enable = ne_pcmcia_enable;
	dsc->sc_disable = ne_pcmcia_disable;
#endif

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(pa->pf)) {
		printf(": function enable failed\n");
		return;
	}

	dsc->sc_enabled = 1;

	/* some cards claim to be io16, but they're lying. */
	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_IO8,
	    NE2000_NIC_OFFSET, NE2000_NIC_NPORTS,
	    &psc->sc_pcioh, &psc->sc_nic_io_window)) {
		printf(": can't map NIC i/o space\n");
		return;
	}

	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_IO16,
	    NE2000_ASIC_OFFSET, NE2000_ASIC_NPORTS,
	    &psc->sc_pcioh, &psc->sc_asic_io_window)) {
		printf(": can't map ASIC i/o space\n");
		return;
	}

	printf("\n");

	/*
	 * Read the station address from the board.
	 */
	for (i = 0; i < NE2000_NDEVS; i++) {
		if ((ne_dev = ne2000_match(pa->card, pa->pf->number, i))
		    != NULL) {
			if (ne_dev->enet_maddr >= 0) {
				if (pcmcia_mem_alloc(pa->pf,
				    ETHER_ADDR_LEN * 2, &pcmh)) {
					printf("%s: can't alloc mem for"
					    " enet addr\n",
					    dsc->sc_dev.dv_xname);
					return;
				}
				if (pcmcia_mem_map(pa->pf, PCMCIA_MEM_ATTR,
				    ne_dev->enet_maddr, ETHER_ADDR_LEN * 2,
				    &pcmh, &offset, &mwindow)) {
					printf("%s: can't map mem for"
					    " enet addr\n",
					    dsc->sc_dev.dv_xname);
					return;
				}
				for (j = 0; j < ETHER_ADDR_LEN; j++)
					myea[j] = bus_space_read_1(pcmh.memt,
					    pcmh.memh, offset + (j * 2));
				pcmcia_mem_unmap(pa->pf, mwindow);
				pcmcia_mem_free(pa->pf, &pcmh);
				enaddr = myea;
			}
			break;
		}
	}

	if (enaddr != NULL) {
		/*
		 * Make sure this is what we expect.
		 */
		if (enaddr[0] != ne_dev->enet_vendor[0] ||
		    enaddr[1] != ne_dev->enet_vendor[1] ||
		    enaddr[2] != ne_dev->enet_vendor[2]) {
			printf("%s: enet addr has incorrect vendor code\n",
			    dsc->sc_dev.dv_xname);
			printf("%s: (%02x:%02x:%02x should be "
			    "%02x:%02x:%02x)\n", dsc->sc_dev.dv_xname,
			    enaddr[0], enaddr[1], enaddr[2],
			    ne_dev->enet_vendor[0],
			    ne_dev->enet_vendor[1],
			    ne_dev->enet_vendor[2]);
			return;
		}
	}

	printf("%s: %s Ethernet\n", dsc->sc_dev.dv_xname, ne_dev->name);

	ne2000_attach(nsc, enaddr);

#if 0
	pcmcia_function_disable(pa->pf);
#endif

	/* set up the interrupt */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, dp8390_intr,
	    dsc);
	if (psc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt\n",
		    dsc->sc_dev.dv_xname);
}

int
ne_pcmcia_enable(dsc)
	struct dp8390_softc *dsc;
{
	struct ne_pcmcia_softc *psc = (struct ne_pcmcia_softc *)dsc;

	/* set up the interrupt */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, dp8390_intr,
	    dsc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    dsc->sc_dev.dv_xname);
		return (1);
	}

	return (pcmcia_function_enable(psc->sc_pf));
}

void
ne_pcmcia_disable(dsc)
	struct dp8390_softc *dsc;
{
	struct ne_pcmcia_softc *psc = (struct ne_pcmcia_softc *)dsc;

	pcmcia_function_disable(psc->sc_pf);

	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
}
