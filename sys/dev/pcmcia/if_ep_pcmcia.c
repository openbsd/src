/*	$OpenBSD: if_ep_pcmcia.c,v 1.8 1998/03/17 00:00:57 deraadt Exp $       */

/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/isa/isavar.h>		/*XXX*/
#include <dev/pcmcia/pcmciavar.h>

int ep_pcmcia_match __P((struct device *, void *, void *));
void ep_pcmcia_attach __P((struct device *, struct device *, void *));
int ep_pcmcia_detach __P((struct device *));

int ep_pcmcia_isasetup __P((struct device *, void *, void *,
    struct pcmcia_link *));
int epmod __P((struct pcmcia_link *, struct device *, struct pcmcia_conf *,
    struct cfdata *));
int ep_remove __P((struct pcmcia_link *, struct device *));

struct cfattach ep_pcmcia_ca = {
	sizeof(struct ep_softc), ep_pcmcia_match, ep_pcmcia_attach,
	ep_pcmcia_detach
};

/* additional setup needed for pcmcia devices */
int
ep_pcmcia_isasetup(parent, match, aux, pc_link)
	struct device	*parent;
	void		*match;
	void		*aux;
	struct pcmcia_link *pc_link;
{
	struct ep_softc *sc = (void *) match;
	struct isa_attach_args *ia = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	extern int ifqmaxlen;

	bus_space_write_2(iot, ioh, EP_COMMAND, WINDOW_SELECT | 0);
	bus_space_write_2(iot, ioh, EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);
	bus_space_write_2(iot, ioh, EP_W0_RESOURCE_CFG, 0x3f00);

	/*
	 * ok til here. Now try to figure out which link we have.
	 * try coax first...
	 */
#ifdef EP_COAX_DEFAULT
	bus_space_write_2(iot, ioh, EP_W0_ADDRESS_CFG, 0xC000);
#else
	/* COAX as default is reported to be a problem */
	bus_space_write_2(iot, ioh, EP_W0_ADDRESS_CFG, 0x0000);
#endif
	ifp->if_snd.ifq_maxlen = ifqmaxlen;

	ia->ia_iosize = 0x10;
	ia->ia_msize = 0;

 	sc->bustype = EP_BUS_PCMCIA;
	sc->pcmcia_flags = (pc_link->flags & PCMCIA_REATTACH) ? EP_REATTACH:0;
	return 1;
}

/* modify config entry */
int
epmod(pc_link, self, pc_cf, cf)
	struct pcmcia_link *pc_link;
	struct device  *self;
	struct pcmcia_conf *pc_cf;
	struct cfdata  *cf;
{
	int             err;
/*	struct pcmciadevs *dev = pc_link->device;*/
/*	struct ep_softc *sc = (void *) self;*/

	if ((err = PCMCIA_BUS_CONFIG(pc_link->adapter, pc_link, self, pc_cf,
	    cf)) != 0) {
		printf("bus_config failed %d\n", err);
		return err;
	}

	if (pc_cf->io[0].len > 0x10)
		pc_cf->io[0].len = 0x10;
#if 0
	pc_cf->cfgtype = DOSRESET;
#endif
	pc_cf->cfgtype = 1;

	return 0;
}

int
ep_remove(pc_link, self)
	struct pcmcia_link *pc_link;
	struct device  *self;
{
	struct ep_softc *sc = (void *) self;
	struct ifnet   *ifp = &sc->sc_arpcom.ac_if;

	if_down(ifp);
	epstop(sc);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_UP);
	sc->pcmcia_flags = EP_ABSENT;
	return PCMCIA_BUS_UNCONFIG(pc_link->adapter, pc_link);
}

static struct pcmcia_3com {
	struct pcmcia_device pcd;
} pcmcia_3com = {
	{
		"PCMCIA 3COM 3C589", epmod,
		ep_pcmcia_isasetup, NULL, ep_remove
	},
};

struct pcmciadevs pcmcia_ep_devs[] = {
	{
		"ep", 0, "3Com Corporation", "3C589",
		NULL, NULL,
		(void *) -1, (void *) &pcmcia_3com
	},
};

int
ep_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	return pcmcia_slave_match(parent, match, aux, pcmcia_ep_devs,
	    sizeof(pcmcia_ep_devs)/sizeof(pcmcia_ep_devs[0]));
}

void
ep_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	if (bus_space_map(iot, ia->ia_iobase, ia->ia_iosize, 0, &ioh))
		panic("ep_isa_attach: can't map i/o space");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	
	epconfig(sc, EP_CHIPSET_3C509);
	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq,
	    IST_EDGE, IPL_NET, epintr, sc, sc->sc_dev.dv_xname);
}

/*
 * No detach; network devices are too well linked into the rest of the
 * kernel.
 */
int
ep_pcmcia_detach(self)
	struct device *self;
{
	return EBUSY;
}
