/*	$OpenBSD: be.c,v 1.2 1998/07/04 20:20:57 deraadt Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt.  All rights reserved.
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
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>
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

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>

#include <sparc/dev/qecvar.h>
#include <sparc/dev/bereg.h>
#include <sparc/dev/bevar.h>

int	bematch __P((struct device *, void *, void *));
void	beattach __P((struct device *, struct device *, void *));

void	beinit __P((struct besoftc *));
void	bestart __P((struct ifnet *));
void	bestop __P((struct besoftc *));
void	bewatchdog __P((struct ifnet *));
int	beintr __P((void *));
int	beioctl __P((struct ifnet *, u_long, caddr_t));

struct cfdriver be_cd = {
	NULL, "be", DV_IFNET
};

struct cfattach be_ca = {
	sizeof(struct besoftc), bematch, beattach
};

int
bematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	return (1);
}

void
beattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct qec_softc *qec = (struct qec_softc *)parent;
	struct besoftc *sc = (struct besoftc *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct confargs *ca = aux;
	int pri;

	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr __P((u_char *));

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", pri);
	sc->sc_rev = getpropint(ca->ca_ra.ra_node, "board-version", -1);
	printf(": rev %x", sc->sc_rev);

	sc->sc_cr = mapiodev(ca->ca_ra.ra_reg, 0, sizeof(struct be_cregs));
	sc->sc_br = mapiodev(&ca->ca_ra.ra_reg[1], 0, sizeof(struct be_bregs));
	sc->sc_tr = mapiodev(&ca->ca_ra.ra_reg[2], 0, sizeof(struct be_tregs));
	bestop(sc);

	sc->sc_mem = qec->sc_buffer;
	sc->sc_memsize = qec->sc_bufsiz;
	sc->sc_conf3 = getpropint(ca->ca_ra.ra_node, "busmaster-regval", 0);

	sc->sc_ih.ih_fun = beintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih);

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = bestart;
	ifp->if_ioctl = beioctl;
	ifp->if_watchdog = bewatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS |
	    IFF_MULTICAST;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	printf("\n");
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splnet _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
void
bestart(ifp)
	struct ifnet *ifp;
{
printf("start\n");
#if 0
	struct besoftc *sc = ifp->if_softc;
	struct mbuf *m;
	int bix;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_last_td;

	for (;;) {
		/* XXX TODO: Some magic */

		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;
#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* XXX TODO
		 * Copy the mbuf chain into the transmit buffer.
		 */
		/* XXX TODO
		 * Initialize transmit registers and start transmission
		 */

		if (++bix == TX_RING_SIZE)
			bix = 0;

		if (++sc->sc_no_td == TX_RING_SIZE) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	sc->sc_last_td = bix;
#endif
}

void
bestop(sc)
	struct besoftc *sc;
{
	int tries;

	tries = 32;
	sc->sc_br->tx_cfg = 0;
	while (sc->sc_br->tx_cfg != 0 && --tries)
		DELAY(20);

	tries = 32;
	sc->sc_br->rx_cfg = 0;
	while (sc->sc_br->rx_cfg != 0 && --tries)
		DELAY(20);
}

void
bewatchdog(ifp)
	struct ifnet *ifp;
{
printf("watchdog\n");

#if 0
	struct besoftc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	bereset(sc);
#endif
}

int
beintr(v)
	void *v;
{
#if 0
	struct besoftc *sc = (struct besoftc *)v;
	u_int32_t why;

	why = be_read32(&greg->stat);

	if (why & GREG_STAT_TXALL)
		be_tint(sc);

	printf("%s: intr: why=%08x\n", sc->sc_dev.dv_xname, why);
#endif
	return (0);
}

int
beioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct besoftc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();
	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return error;
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			beinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif /* INET */
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)
				    (sc->sc_arpcom.ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			/* Set new address. */
			beinit(sc);
			break;
		    }
#endif /* NS */
		default:
			beinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		sc->sc_promisc = ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			bestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			beinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			bestop(sc);
			beinit(sc);
		}
#ifdef IEDEBUG   
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = IED_ALL;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom):
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
#if 0
			mc_reset(sc);
#endif
			error = 0;
		}
		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return error;
}

void
beinit(sc)
	struct besoftc *sc;
{
#if 0
	u_int32_t c;
	struct be_bregs *br = sc->sc_bregs;
	struct be_cregs *cr = sc->sc_cregs;
	struct be_tregs *tr = sc->sc_tregs;

	bestop(sc);
	be_meminit(sc);

	be_write32(&treg->int_mask, 0xffff);

	c = be_read32(&treg->cfg);
	if (sc->sc_flags & HFLAG_FENABLE)
		be_write32(&treg->cfg, c & ~(TCV_CFG_BENABLE));
	else
		be_write32(&treg->cfg, c | TCV_CFG_BENABLE);

	be_tcvr_check(sc);
	switch (sc->tcvr_type) {
	case none:
		printf("%s: no transceiver type!\n", sc->sc_dev.dv_xname);
		return;
	case internal:
		be_write32(&breg->xif_cfg, 0);
		break;
	case external:
		be_write32(&breg->xif_cfg, BIGMAC_XCFG_MIIDISAB);
		break;
	}
	be_tcvr_reset(sc);

	be_reset_tx(sc);
	be_reset_rx(sc);

	be_write32(&breg->jsize, BE_DEFAULT_JSIZE);
	be_write32(&breg->ipkt_gap1, BE_DEFAULT_IPKT_GAP1);
	be_write32(&breg->ipkt_gap2, BE_DEFAULT_IPKT_GAP2);
	be_write32(&breg->htable3, 0);
	be_write32(&breg->htable2, 0);
	be_write32(&breg->htable1, 0);
	be_write32(&breg->htable0, 0);

	be_write32(&erxreg->rx_ring,
		sc->sc_block_addr +
		((u_long) &sc->sc_block->be_rxd[0]) - ((u_long)sc->sc_block));
	be_write32(&etxreg->tx_ring,
		sc->sc_block_addr +
		((u_long) &sc->sc_block->be_txd[0]) - ((u_long)sc->sc_block));

	if (sc->sc_burst & SBUS_BURST_64)
		be_write32(&greg->cfg, GREG_CFG_BURST64);
	else if (sc->sc_burst & SBUS_BURST_32)
		be_write32(&greg->cfg, GREG_CFG_BURST32);
	else if (sc->sc_burst & SBUS_BURST_16)
		be_write32(&greg->cfg, GREG_CFG_BURST16);
	else {
		printf("%s: burst size unknown\n", sc->sc_dev.dv_xname);
		be_write32(&greg->cfg, 0);
	}

	/* XXX TODO: set interrupt mask: (GOTFRAME | RCNTEXP) */
	be_write32(&greg->imask, GREG_IMASK_SENTFRAME | GREG_IMASK_TXPERR);

	be_write32(&etxreg->tx_rsize, (TX_RING_SIZE >> ETX_RSIZE_SHIFT) - 1);
	be_write32(&etxreg->cfg, be_read32(&etxreg->cfg) | ETX_CFG_DMAENABLE);
	be_write32(&breg->rx_cfg, BIGMAC_RXCFG_HENABLE);

	be_auto_negotiate(sc);
#endif
}
