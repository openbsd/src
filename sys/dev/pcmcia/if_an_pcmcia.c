/*	$OpenBSD: if_an_pcmcia.c,v 1.11 2002/11/19 18:36:18 jason Exp $	*/

/*
 * Copyright (c) 1999 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bus.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/anvar.h>
#include <dev/ic/anreg.h>

int  an_pcmcia_match(struct device *, void *, void *);
void an_pcmcia_attach(struct device *, struct device *, void *);
int  an_pcmcia_detach(struct device *, int);
int  an_pcmcia_activate(struct device *, enum devact);

struct an_pcmcia_softc {
	struct an_softc sc_an;

	struct pcmcia_io_handle sc_pcioh;
	int sc_io_window;
	struct pcmcia_function *sc_pf;
};

struct cfattach an_pcmcia_ca = {   
	sizeof(struct an_pcmcia_softc), an_pcmcia_match, an_pcmcia_attach,
	an_pcmcia_detach, an_pcmcia_activate
};

int
an_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->pf->function != PCMCIA_FUNCTION_NETWORK)
		return 0;

	switch (pa->manufacturer) {
	case PCMCIA_VENDOR_AIRONET:
		switch (pa->product) {
		case PCMCIA_PRODUCT_AIRONET_PC4500:
		case PCMCIA_PRODUCT_AIRONET_PC4800:
		case PCMCIA_PRODUCT_AIRONET_350:
			return 1;
		}
	}

	return 0;
}

void
an_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct an_pcmcia_softc *psc = (struct an_pcmcia_softc *)self;
	struct an_softc *sc = (struct an_softc *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;

	psc->sc_pf = pa->pf;
	cfe = SIMPLEQ_FIRST(&pa->pf->cfe_head);

	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(pa->pf)) {
		printf(": function enable failed\n");
		return;
	}

	if (pcmcia_io_alloc(pa->pf, 0, AN_IOSIZ, AN_IOSIZ, &psc->sc_pcioh)) {
		printf(": can't alloc i/o space\n");
		pcmcia_function_disable(pa->pf);
		return;
	}

	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_IO16, 0, AN_IOSIZ,
	    &psc->sc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		pcmcia_io_free(pa->pf, &psc->sc_pcioh);
		pcmcia_function_disable(pa->pf);
		return;
	}

	sc->an_btag = psc->sc_pcioh.iot;
	sc->an_bhandle = psc->sc_pcioh.ioh;

	sc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, an_intr, sc, "");
	if (sc->sc_ih == NULL)
		printf("no irq");

	an_attach(sc);
}

int
an_pcmcia_detach(dev, flags)
	struct device *dev;
	int flags;
{
	struct an_pcmcia_softc *psc = (struct an_pcmcia_softc *)dev;
	struct an_softc *sc = (struct an_softc *)dev;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	if (sc->an_gone) {
		printf ("%s: already detached\n", sc->sc_dev.dv_xname);
		return 0;
	}

	if (ifp->if_flags & IFF_RUNNING)
		an_stop(sc);

	pcmcia_io_unmap(psc->sc_pf, psc->sc_io_window);
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);

	ether_ifdetach(ifp);
	if_detach(ifp);

	sc->an_gone = 1;

	return 0;
}

int
an_pcmcia_activate(dev, act)
	struct device *dev;
	enum devact act;
{
	struct an_pcmcia_softc *psc = (struct an_pcmcia_softc *)dev;
	struct an_softc *sc = &psc->sc_an;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		pcmcia_function_enable(psc->sc_pf);
		sc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET,
		    an_intr, sc, sc->sc_dev.dv_xname);
		an_init(sc);
		break;

	case DVACT_DEACTIVATE:
		ifp->if_timer = 0;
		if (ifp->if_flags & IFF_RUNNING)
			an_stop(sc);
		pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
		pcmcia_function_disable(psc->sc_pf);
		break;
	}

	splx(s);
	return (0);
}
