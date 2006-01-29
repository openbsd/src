/*      $OpenBSD: if_ath_cardbus.c,v 1.8 2006/01/29 20:39:37 fgsch Exp $   */
/*	$NetBSD: if_ath_cardbus.c,v 1.4 2004/08/02 19:14:28 mycroft Exp $ */

/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * CardBus bus front-end for the AR5001 Wireless LAN 802.11a/b/g CardBus.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <machine/endian.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_rssadapt.h>

#if NBPFILTER > 0 
#include <net/bpf.h>
#endif 

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/gpio/gpiovar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

#include <dev/ic/athvar.h>

/*
 * PCI configuration space registers
 */
#define	ATH_PCI_MMBA		0x10	/* memory mapped base */

struct ath_cardbus_softc {
	struct ath_softc	sc_ath;

	/* CardBus-specific goo. */
	void	*sc_ih;			/* interrupt handle */
	cardbus_devfunc_t sc_ct;	/* our CardBus devfuncs */
	cardbustag_t sc_tag;		/* our CardBus tag */
	bus_size_t sc_mapsize;		/* the size of mapped bus space region */

	pcireg_t sc_bar_val;		/* value of the BAR */

	int	sc_intrline;		/* interrupt line */
};

int	ath_cardbus_match(struct device *, void *, void *);
void	ath_cardbus_attach(struct device *, struct device *, void *);
int	ath_cardbus_detach(struct device *, int);

struct cfattach ath_cardbus_ca = {
	sizeof(struct ath_cardbus_softc),
	ath_cardbus_match,
	ath_cardbus_attach,
	ath_cardbus_detach
};


void	ath_cardbus_setup(struct ath_cardbus_softc *);

int	ath_cardbus_enable(struct ath_softc *);
void	ath_cardbus_disable(struct ath_softc *);
void	ath_cardbus_power(struct ath_softc *, int);

int
ath_cardbus_match(struct device *parent, void *match, void *aux)
{
	struct cardbus_attach_args *ca = aux;
	const char* devname;

	devname = ath_hal_probe(PCI_VENDOR(ca->ca_id),
				PCI_PRODUCT(ca->ca_id));

	if (devname)
		return (1);

	return (0);
}

void
ath_cardbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct ath_cardbus_softc *csc = (void *)self;
	struct ath_softc *sc = &csc->sc_ath;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_addr_t adr;

	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	/*
	 * Power management hooks.
	 */
	sc->sc_enable = ath_cardbus_enable;
	sc->sc_disable = ath_cardbus_disable;
	sc->sc_power = ath_cardbus_power;

	/*
	 * Map the device.
	 */
	if (Cardbus_mapreg_map(ct, ATH_PCI_MMBA, CARDBUS_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st, &sc->sc_sh, &adr, &csc->sc_mapsize) == 0) {
#if rbus
#else
		(*ct->ct_cf->cardbus_mem_open)(cc, 0, adr, adr+csc->sc_mapsize);
#endif
		csc->sc_bar_val = adr | CARDBUS_MAPREG_TYPE_MEM;
	}

	else {
	        printf(": unable to map device registers\n");
		return;
	}

	/*
	 * Set up the PCI configuration registers.
	 */
	ath_cardbus_setup(csc);

	/* Remember which interrupt line. */
	csc->sc_intrline = ca->ca_intrline;

	printf(": irq %d\n", csc->sc_intrline);

	/*
	 * Finish off the attach.
	 */
	ath_attach(PCI_PRODUCT(ca->ca_id), sc);

	/*
	 * Power down the socket.
	 */
	Cardbus_function_disable(csc->sc_ct);
}

int
ath_cardbus_detach(struct device *self, int flags)
{
	struct ath_cardbus_softc *csc = (void *)self;
	struct ath_softc *sc = &csc->sc_ath;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv;

#if defined(DIAGNOSTIC)
	if (ct == NULL)
		panic("%s: data structure lacks", sc->sc_dev.dv_xname);
#endif

	rv = ath_detach(sc, flags);
	if (rv)
		return (rv);

	/*
	 * Unhook the interrupt handler.
	 */
	if (csc->sc_ih != NULL) {
		cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf, csc->sc_ih);
		csc->sc_ih = NULL;
	}

	/*
	 * Release bus space and close window.
	 */
	Cardbus_mapreg_unmap(ct, ATH_PCI_MMBA,
		    sc->sc_st, sc->sc_sh, csc->sc_mapsize);

	return (0);
}

int
ath_cardbus_enable(struct ath_softc *sc)
{
	struct ath_cardbus_softc *csc = (void *) sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/*
	 * Power on the socket.
	 */
	Cardbus_function_enable(ct);

	/*
	 * Set up the PCI configuration registers.
	 */
	ath_cardbus_setup(csc);

	/*
	 * Map and establish the interrupt.
	 */
	csc->sc_ih = cardbus_intr_establish(cc, cf, csc->sc_intrline, IPL_NET,
	    ath_intr, sc);
	if (csc->sc_ih == NULL) {
		printf(": unable to establish irq %d\n",
		       csc->sc_intrline);
		Cardbus_function_disable(csc->sc_ct);
		return (1);
	}
	return (0);
}

void
ath_cardbus_disable(struct ath_softc *sc)
{
	struct ath_cardbus_softc *csc = (void *) sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* Unhook the interrupt handler. */
	cardbus_intr_disestablish(cc, cf, csc->sc_ih);
	csc->sc_ih = NULL;

	/* Power down the socket. */
	Cardbus_function_disable(ct);
}

void
ath_cardbus_power(struct ath_softc *sc, int why)
{
	if (why == PWR_RESUME)
		ath_enable(sc);
}

void
ath_cardbus_setup(struct ath_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t reg;

#ifdef notyet
	(void)cardbus_setpowerstate(sc->sc_dev.dv_xname, ct, csc->sc_tag,
	    PCI_PWR_D0);
#endif

	/* Program the BAR. */
	cardbus_conf_write(cc, cf, csc->sc_tag, ATH_PCI_MMBA,
	    csc->sc_bar_val);

	/* Make sure the right access type is on the CardBus bridge. */
	(*ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	(*ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	/* Enable the appropriate bits in the PCI CSR. */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	reg |= CARDBUS_COMMAND_MASTER_ENABLE | CARDBUS_COMMAND_MEM_ENABLE;
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG,
	    reg);

	/*
	 * Make sure the latency timer is set to some reasonable
	 * value.
	 */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag, CARDBUS_BHLC_REG);
	if (CARDBUS_LATTIMER(reg) < 0x20) {
		reg &= ~(CARDBUS_LATTIMER_MASK << CARDBUS_LATTIMER_SHIFT);
		reg |= (0x20 << CARDBUS_LATTIMER_SHIFT);
		cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_BHLC_REG, reg);
	}
}
