/*	$OpenBSD: if_xl_cardbus.c,v 1.19 2007/05/05 13:24:03 deraadt Exp $ */
/*	$NetBSD: if_xl_cardbus.c,v 1.13 2000/03/07 00:32:52 mycroft Exp $	*/

/*
 * CardBus specific routines for 3Com 3C575-family CardBus ethernet adapter
 *
 * Copyright (c) 1998 and 1999
 *       HAYAKAWA Koichi.  All rights reserved.
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
 *      This product includes software developed by the author.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HAYAKAWA KOICHI ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL TAKESHI OHASHI OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
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

#include <machine/cpu.h>
#include <machine/bus.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/xlreg.h>

#if defined XL_DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define CARDBUS_3C575BTX_FUNCSTAT_PCIREG  CARDBUS_BASE2_REG  /* means 0x18 */

int xl_cardbus_match(struct device *, void *, void *);
void xl_cardbus_attach(struct device *, struct device *,void *);
int xl_cardbus_detach(struct device *, int);
void xl_cardbus_intr_ack(struct xl_softc *);

#define XL_CARDBUS_BOOMERANG	0x0001
#define XL_CARDBUS_CYCLONE	0x0002

#define XL_CARDBUS_INTR		0x0004
#define XL_CARDBUS_INTR_ACK	0x8000

struct xl_cardbus_softc {
	struct xl_softc sc_softc;

	cardbus_devfunc_t sc_ct;
	int sc_intrline;
	u_int8_t sc_cardbus_flags;
	u_int8_t sc_cardtype;

	/* CardBus function status space.  575B requests it. */
	bus_space_tag_t sc_funct;
	bus_space_handle_t sc_funch;
	bus_size_t sc_funcsize;

	bus_size_t sc_mapsize;		/* size of mapped bus space region */
};

struct cfattach xl_cardbus_ca = {
	sizeof(struct xl_cardbus_softc), xl_cardbus_match,
	    xl_cardbus_attach, xl_cardbus_detach
};

const struct xl_cardbus_product {
	u_int32_t	ecp_prodid;	/* CardBus product ID */
	int		ecp_flags;	/* initial softc flags */
	pcireg_t	ecp_csr;	/* PCI CSR flags */
	int		ecp_cardtype;	/* card type */
	const char	*ecp_name;	/* device name */
} xl_cardbus_products[] = {
	{ PCI_PRODUCT_3COM_3C575,
	  XL_FLAG_PHYOK | XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_8BITROM,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_CARDBUS_BOOMERANG,
	  "3c575-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3CCFE575BT,
	  XL_FLAG_PHYOK | XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_8BITROM |
	      XL_FLAG_INVERT_LED_PWR,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	      CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_CARDBUS_CYCLONE,
	  "3c575B-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3CCFE575CT,
	  XL_FLAG_PHYOK | XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_8BITROM |
	      XL_FLAG_INVERT_MII_PWR,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	      CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_CARDBUS_CYCLONE,
	  "3c575C-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3CCFEM656,
	  XL_FLAG_PHYOK | XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_8BITROM |
	      XL_FLAG_INVERT_LED_PWR | XL_FLAG_INVERT_MII_PWR,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	      CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_CARDBUS_CYCLONE,
	  "3c656-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3CCFEM656B,
	  XL_FLAG_PHYOK | XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_8BITROM |
	      XL_FLAG_INVERT_LED_PWR | XL_FLAG_INVERT_MII_PWR,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	      CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_CARDBUS_CYCLONE,
	  "3c656B-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3CCFEM656C,
	  XL_FLAG_PHYOK | XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_8BITROM |
	      XL_FLAG_INVERT_MII_PWR,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	      CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_CARDBUS_CYCLONE,
	  "3c656C-TX Ethernet" },

	{ 0,
	  0,
	  0,
	  0,
	  NULL },
};

const struct xl_cardbus_product *xl_cardbus_lookup(const struct cardbus_attach_args *);

const struct xl_cardbus_product *
xl_cardbus_lookup(const struct cardbus_attach_args *ca)
{
	const struct xl_cardbus_product *ecp;

	if (CARDBUS_VENDOR(ca->ca_id) != PCI_VENDOR_3COM)
		return (NULL);

	for (ecp = xl_cardbus_products; ecp->ecp_name != NULL; ecp++)
		if (CARDBUS_PRODUCT(ca->ca_id) == ecp->ecp_prodid)
			return (ecp);
	return (NULL);
}

int
xl_cardbus_match(struct device *parent, void *match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (xl_cardbus_lookup(ca) != NULL)
		return (1);

	return (0);
}

void
xl_cardbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct xl_cardbus_softc *csc = (void *)self;
	struct xl_softc *sc = &csc->sc_softc;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	cardbusreg_t iob, command, bhlc;
	const struct xl_cardbus_product *ecp;
	bus_space_handle_t ioh;
	bus_addr_t adr;

	if (Cardbus_mapreg_map(ct, CARDBUS_BASE0_REG, CARDBUS_MAPREG_TYPE_IO, 0,
	    &sc->xl_btag, &ioh, &adr, &csc->sc_mapsize)) {
		printf(": can't map i/o space\n");
		return;
	}

	ecp = xl_cardbus_lookup(ca);
	if (ecp == NULL) {
		printf("\n");
		panic("xl_cardbus_attach: impossible");
	}

	printf(": 3Com %s", ecp->ecp_name);

	sc->xl_flags = ecp->ecp_flags;
	sc->sc_dmat = ca->ca_dmat;

	iob = adr;
	sc->xl_bhandle = ioh;

	(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_IO_ENABLE);

	command = cardbus_conf_read(cc, cf, ca->ca_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	command |= ecp->ecp_csr;
	csc->sc_cardtype = ecp->ecp_cardtype;

	if (csc->sc_cardtype == XL_CARDBUS_CYCLONE) {
		/* map CardBus function status window */
		if (Cardbus_mapreg_map(ct, CARDBUS_BASE2_REG,
		    CARDBUS_MAPREG_TYPE_MEM, 0, &csc->sc_funct,
		    &csc->sc_funch, 0, &csc->sc_funcsize)) {
			printf("%s: unable to map function status window\n",
			    self->dv_xname);
			return;
		}

		/*
		 * Make sure CardBus bridge can access memory space.  Usually
		 * memory access is enabled by BIOS, but some BIOSes do not
		 * enable it.
		 */
		(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	}

	(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);
	cardbus_conf_write(cc, cf, ca->ca_tag, CARDBUS_COMMAND_STATUS_REG,
	    command);
  
 	/*
	 * set latency timer
	 */
	bhlc = cardbus_conf_read(cc, cf, ca->ca_tag, CARDBUS_BHLC_REG);
	if (CARDBUS_LATTIMER(bhlc) < 0x20) {
		/* at least the value of latency timer should 0x20. */
		DPRINTF(("if_xl_cardbus: lattimer 0x%x -> 0x20\n",
		    CARDBUS_LATTIMER(bhlc)));
		bhlc &= ~(CARDBUS_LATTIMER_MASK << CARDBUS_LATTIMER_SHIFT);
		bhlc |= (0x20 << CARDBUS_LATTIMER_SHIFT);
		cardbus_conf_write(cc, cf, ca->ca_tag, CARDBUS_BHLC_REG, bhlc);
	}

	csc->sc_ct = ca->ca_ct;
	csc->sc_intrline = ca->ca_intrline;

	/* Map and establish the interrupt. */

	sc->xl_intrhand = cardbus_intr_establish(cc, cf, ca->ca_intrline,
	    IPL_NET, xl_intr, csc, self->dv_xname);

	if (sc->xl_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		printf(" at %d", ca->ca_intrline);
		printf("\n");
		return;
	}
	printf(": irq %d", ca->ca_intrline);

	sc->intr_ack = xl_cardbus_intr_ack;

	xl_attach(sc);

	if (csc->sc_cardtype == XL_CARDBUS_CYCLONE)
		bus_space_write_4(csc->sc_funct, csc->sc_funch,
		    XL_CARDBUS_INTR, XL_CARDBUS_INTR_ACK);

}

int
xl_detach(struct xl_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	extern void xl_freetxrx(struct xl_softc *);

	/* Unhook our tick handler. */
	timeout_del(&sc->xl_stsup_tmo);

	xl_freetxrx(sc);

	/* Detach all PHYs */
	if (sc->xl_hasmii)
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);

	if (sc->sc_sdhook != NULL)
		shutdownhook_disestablish(sc->sc_sdhook);
	if (sc->sc_pwrhook != NULL)
		powerhook_disestablish(sc->sc_pwrhook);

	return (0);
}

int
xl_cardbus_detach(struct device *self, int arg)
{
	struct xl_cardbus_softc *csc = (void *)self;
	struct xl_softc *sc = &csc->sc_softc;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv = 0;

#if defined(DIAGNOSTIC)
	if (ct == NULL) {
		panic("%s: data structure lacks", sc->sc_dev.dv_xname);
	}
#endif

	rv = xl_detach(sc);
	if (rv == 0) {
		/*
		 * Unhook the interrupt handler.
		 */
		cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf,
		    sc->xl_intrhand);

		if (csc->sc_cardtype == XL_CARDBUS_CYCLONE) {
			Cardbus_mapreg_unmap(ct, CARDBUS_BASE2_REG,
			    csc->sc_funct, csc->sc_funch, csc->sc_funcsize);
		}

		Cardbus_mapreg_unmap(ct, CARDBUS_BASE0_REG, sc->xl_btag,
		    sc->xl_bhandle, csc->sc_mapsize);
	}
	return (rv);
}

void
xl_cardbus_intr_ack(struct xl_softc *sc)
{
	struct xl_cardbus_softc *csc = (struct xl_cardbus_softc *)sc;

	bus_space_write_4(csc->sc_funct, csc->sc_funch, XL_CARDBUS_INTR,
	    XL_CARDBUS_INTR_ACK);
}
