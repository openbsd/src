/*	$OpenBSD: if_xl_cardbus.c,v 1.1 2000/04/08 05:50:52 aaron Exp $ */
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

/* #define XL_DEBUG 4 */	/* define to report infomation for debugging */

#define XL_POWER_STATIC		/* do not use enable/disable functions */
				/* I'm waiting elinkxl.c uses
                                   sc->enable and sc->disable
                                   functions. */

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
#include <dev/cardbus/cardbusdevs.h>

#include <dev/mii/miivar.h>

#include <dev/ic/xlreg.h>

#if defined DEBUG && !defined XL_DEBUG
#define XL_DEBUG
#endif

#if defined XL_DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define CARDBUS_3C575BTX_FUNCSTAT_PCIREG  CARDBUS_BASE2_REG  /* means 0x18 */
#define XL_CB_INTR 4		/* intr acknowledge reg. CardBus only */
#define XL_CB_INTR_ACK 0x8000 /* intr acknowledge bit */

int xl_cardbus_match __P((struct device *, void *, void *));
void xl_cardbus_attach __P((struct device *, struct device *,void *));
int xl_cardbus_detach __P((struct device *, int));
void xl_cardbus_intr_ack __P((struct xl_softc *));

#if !defined XL_POWER_STATIC
int xl_cardbus_enable __P((struct xl_softc *sc));
void xl_cardbus_disable __P((struct xl_softc *sc));
#endif /* !defined XL_POWER_STATIC */

struct xl_cardbus_softc {
	struct xl_softc sc_softc;

	cardbus_devfunc_t sc_ct;
	int sc_intrline;
	u_int8_t sc_cardbus_flags;
#define XL_REATTACH		0x01
#define XL_ABSENT		0x02
	u_int8_t sc_cardtype;
#define XL_3C575		1
#define XL_3C575B		2

	/* CardBus function status space.  575B requests it. */
	bus_space_tag_t sc_funct;
	bus_space_handle_t sc_funch;
	bus_size_t sc_funcsize;

	bus_size_t sc_mapsize;		/* the size of mapped bus space region */
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
	{ CARDBUS_PRODUCT_3COM_3C575TX,
	  /* XL_CONF_MII, */ 0,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_3C575,
	  "3c575-TX Ethernet" },

	{ CARDBUS_PRODUCT_3COM_3C575BTX,
	  /* XL_CONF_90XB|XL_CONF_MII, */ 0,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	      CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_3C575B,
	  "3c575B-TX Ethernet" },

	{ CARDBUS_PRODUCT_3COM_3CCFE575CT,
	  /* XL_CONF_90XB|XL_CONF_MII, */ 0,
	  CARDBUS_COMMAND_IO_ENABLE | CARDBUS_COMMAND_MEM_ENABLE |
	      CARDBUS_COMMAND_MASTER_ENABLE,
	  XL_3C575B,
	  "3c575C-TX Ethernet" },

	{ 0,
	  0,
	  0,
	  NULL },
};

const struct xl_cardbus_product *xl_cardbus_lookup
    __P((const struct cardbus_attach_args *));

const struct xl_cardbus_product *
xl_cardbus_lookup(ca)
	const struct cardbus_attach_args *ca;
{
	const struct xl_cardbus_product *ecp;

	if (CARDBUS_VENDOR(ca->ca_id) != CARDBUS_VENDOR_3COM)
		return (NULL);

	for (ecp = xl_cardbus_products; ecp->ecp_name != NULL; ecp++)
		if (CARDBUS_PRODUCT(ca->ca_id) == ecp->ecp_prodid)
			return (ecp);
	return (NULL);
}

int
xl_cardbus_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cardbus_attach_args *ca = aux;

	if (xl_cardbus_lookup(ca) != NULL)
		return (1);

	return (0);
}

void
xl_cardbus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct xl_cardbus_softc *psc = (void *)self;
	struct xl_softc *sc = &psc->sc_softc;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	cardbusreg_t iob, command, bhlc;
	const struct xl_cardbus_product *ecp;
	bus_space_handle_t ioh;
	bus_addr_t adr;

	if (Cardbus_mapreg_map(ct, CARDBUS_BASE0_REG, CARDBUS_MAPREG_TYPE_IO, 0,
	    &sc->xl_btag, &ioh, &adr, &psc->sc_mapsize)) {
		printf(": can't map i/o space\n");
		return;
	}

	ecp = xl_cardbus_lookup(ca);
	if (ecp == NULL) {
		printf("\n");
		panic("xl_cardbus_attach: impossible");
	}

	printf(": 3Com %s", ecp->ecp_name);

#if 0
#if !defined XL_POWER_STATIC
	sc->enable = xl_cardbus_enable;
	sc->disable = xl_cardbus_disable;
#else
	sc->enable = NULL;
	sc->disable = NULL;
#endif
	sc->enabled = 1;
	sc->sc_dmat = ca->ca_dmat;
	sc->xl_conf = ecp->ecp_flags;
#endif
	sc->xl_bustype = XL_BUS_CARDBUS;

	iob = adr;
	sc->xl_bhandle = ioh;

#if rbus
#else
	(ct->ct_cf->cardbus_io_open)(cc, 0, iob, iob + 0x40);
#endif
	(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_IO_ENABLE);

	command = cardbus_conf_read(cc, cf, ca->ca_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	command |= ecp->ecp_csr;
	psc->sc_cardtype = ecp->ecp_cardtype;

	if (psc->sc_cardtype == XL_3C575B) {
		/* Map CardBus function status window. */
		if (Cardbus_mapreg_map(ct, CARDBUS_3C575BTX_FUNCSTAT_PCIREG,
		    CARDBUS_MAPREG_TYPE_MEM, 0, &psc->sc_funct,
		    &psc->sc_funch, 0, &psc->sc_funcsize)) {
			printf("%s: unable to map function status window\n",
			    self->dv_xname);
			return;
		}

		/*
		 * Make sure CardBus brigde can access memory space.  Usually
		 * memory access is enabled by BIOS, but some BIOSes do not
		 * enable it.
		 */
		(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);

		/* Setup interrupt acknowledge hook */
		sc->intr_ack = xl_cardbus_intr_ack;
	}

	(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);
	cardbus_conf_write(cc, cf, ca->ca_tag, CARDBUS_COMMAND_STATUS_REG,
	    command);
  
 	/*
	 * set latency timmer
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

	psc->sc_ct = ca->ca_ct;
	psc->sc_intrline = ca->ca_intrline;

#if defined XL_POWER_STATIC
	/* Map and establish the interrupt. */

	sc->xl_intrhand = cardbus_intr_establish(cc, cf, ca->ca_intrline,
	    IPL_NET, xl_intr, psc);

	if (sc->xl_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		printf(" at %d", ca->ca_intrline);
		printf("\n");
		return;
	}
	printf(": irq %d", ca->ca_intrline);
#endif

	bus_space_write_2(sc->xl_btag, sc->xl_bhandle, XL_COMMAND, XL_CMD_RESET);
	delay(400);
	{
		int i = 0;
		while (bus_space_read_2(sc->xl_btag, sc->xl_bhandle, XL_STATUS) &
		    XL_STAT_CMDBUSY) {
			if (++i > 10000) {
				printf("ex: timeout %x\n",
				    bus_space_read_2(sc->xl_btag, sc->xl_bhandle,
				        XL_STATUS));
				printf("ex: addr %x\n",
				    cardbus_conf_read(cc, cf, ca->ca_tag,
				    CARDBUS_BASE0_REG));
				return;		/* emergency exit */
			}
		}
	}

	xl_attach(sc);

	if (psc->sc_cardtype == XL_3C575B)
		bus_space_write_4(psc->sc_funct, psc->sc_funch,
		    XL_CB_INTR, XL_CB_INTR_ACK);

#if !defined XL_POWER_STATIC
	cardbus_function_disable(psc->sc_ct);  
	sc->enabled = 0;
#endif
}

void
xl_cardbus_intr_ack(sc)
	struct xl_softc *sc;
{
	struct xl_cardbus_softc *psc = (struct xl_cardbus_softc *)sc;

	bus_space_write_4(psc->sc_funct, psc->sc_funch, XL_CB_INTR,
	    XL_CB_INTR_ACK);
}

int
xl_cardbus_detach(self, arg)
	struct device *self;
	int arg;
{
	struct xl_cardbus_softc *psc = (void *)self;
	struct xl_softc *sc = &psc->sc_softc;
	struct cardbus_devfunc *ct = psc->sc_ct;
	int rv = 0;

#if defined(DIAGNOSTIC)
	if (ct == NULL) {
		panic("%s: data structure lacks\n", sc->sc_dev.dv_xname);
	}
#endif

#if 0
	rv = xl_detach(sc);
#endif
	if (rv == 0) {
		/*
		 * Unhook the interrupt handler.
		 */
		cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf, sc->xl_intrhand);

		if (psc->sc_cardtype == XL_3C575B) {
			Cardbus_mapreg_unmap(ct,
			    CARDBUS_3C575BTX_FUNCSTAT_PCIREG,
			    psc->sc_funct, psc->sc_funch, psc->sc_funcsize);
		}

		Cardbus_mapreg_unmap(ct, CARDBUS_BASE0_REG, sc->xl_btag,
		    sc->xl_bhandle, psc->sc_mapsize);
	}
	return (rv);
}

#if !defined XL_POWER_STATIC
int
xl_cardbus_enable(sc)
	struct xl_softc *sc;
{
	struct xl_cardbus_softc *csc = (struct xl_cardbus_softc *)sc;
	cardbus_function_tag_t cf = csc->sc_ct->ct_cf;
	cardbus_chipset_tag_t cc = csc->sc_ct->ct_cc;

	Cardbus_function_enable(csc->sc_ct);
	cardbus_restore_bar(csc->sc_ct);

	sc->xl_intrhand = cardbus_intr_establish(cc, cf, csc->sc_intrline,
	    IPL_NET, xl_intr, sc);
	if (NULL == sc->xl_intrhand) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	return (0);
}

void
xl_cardbus_disable(sc)
	struct xl_softc *sc;
{
	struct xl_cardbus_softc *csc = (struct xl_cardbus_softc *)sc;
	cardbus_function_tag_t cf = csc->sc_ct->ct_cf;
	cardbus_chipset_tag_t cc = csc->sc_ct->ct_cc;

	cardbus_save_bar(csc->sc_ct);
  
 	Cardbus_function_disable(csc->sc_ct);

	cardbus_intr_disestablish(cc, cf, sc->xl_intrhand);
}
#endif /* XL_POWER_STATIC */
