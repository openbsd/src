/*	$OpenBSD: be.c,v 1.5 1998/07/11 05:47:36 deraadt Exp $	*/

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
#include <sparc/dev/qecreg.h>
#include <sparc/dev/bereg.h>
#include <sparc/dev/bevar.h>

int	bematch __P((struct device *, void *, void *));
void	beattach __P((struct device *, struct device *, void *));

void	beinit __P((struct besoftc *));
void	be_meminit __P((struct besoftc *));
void	bestart __P((struct ifnet *));
void	bestop __P((struct besoftc *));
void	bewatchdog __P((struct ifnet *));
int	beioctl __P((struct ifnet *, u_long, caddr_t));

int	beintr __P((void *));
int	betint __P((struct besoftc *));
int	berint __P((struct besoftc *));
int	beqint __P((struct besoftc *));
int	beeint __P((struct besoftc *));
void	be_tcvr_init __P((struct besoftc *sc));
void	be_tcvr_setspeed __P((struct besoftc *sc));

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
	extern void myetheraddr __P((u_char *));
	int pri;

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;
	sc->sc_rev = getpropint(ca->ca_ra.ra_node, "board-version", -1);

	sc->sc_cr = mapiodev(ca->ca_ra.ra_reg, 0, sizeof(struct be_cregs));
	sc->sc_br = mapiodev(&ca->ca_ra.ra_reg[1], 0, sizeof(struct be_bregs));
	sc->sc_tr = mapiodev(&ca->ca_ra.ra_reg[2], 0, sizeof(struct be_tregs));
	sc->sc_qr = qec->sc_regs;
	bestop(sc);

	sc->sc_mem = qec->sc_buffer;
	sc->sc_memsize = qec->sc_bufsiz;
	sc->sc_conf3 = getpropint(ca->ca_ra.ra_node, "busmaster-regval", 0);

	sc->sc_burst = getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		sc->sc_burst = qec->sc_burst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= qec->sc_burst;

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

	printf(" pri %d: rev %x address %s\n", pri, sc->sc_rev,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

#if NBPFILTER > 0
	bpfattach(&sc->sc_arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif
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
	struct besoftc *sc = (struct besoftc *)v;
	u_int32_t why;
	int r = 0;

	why = sc->sc_qr->stat;

	if (why & QEC_STAT_TX)
		r |= betint(sc);
	if (why & QEC_STAT_RX)
		r |= berint(sc);
	if (why & QEC_STAT_BM)
		r |= beqint(sc);
	if (why & QEC_STAT_ER)
		r |= beeint(sc);
	if (r)
		printf("%s: intr: why=%08x\n", sc->sc_dev.dv_xname, why);
	return (r);
}

int
betint(sc)
	struct besoftc *sc;
{
	return (0);
}

int
berint(sc)
	struct besoftc *sc;
{
	return (0);
}

int
beqint(sc)
	struct besoftc *sc;
{
	return (0);
}

int
beeint(sc)
	struct besoftc *sc;
{
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
		if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
			splx(s);
			return error;
		}
		error = EINVAL;
		break;
	}
	splx(s);
	return error;
}

void
be_tcvr_init(sc)
	struct besoftc *sc;
{
}

void
be_tcvr_setspeed(sc)
	struct besoftc *sc;
{
}

void
beinit(sc)
	struct besoftc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s = splimp();
	int i;

	bestop(sc);

	/* init QEC */
	sc->sc_qr->msize = sc->sc_memsize;
	sc->sc_qr->rsize = sc->sc_memsize / 2;
	sc->sc_qr->tsize = sc->sc_memsize / 2;
	sc->sc_qr->psize = 2048;
	if (sc->sc_burst & SBUS_BURST_64)
		i = QEC_CTRL_B64;
	else if (sc->sc_burst & SBUS_BURST_32)
		i = QEC_CTRL_B32;
	else
		i = QEC_CTRL_B16;
	sc->sc_qr->ctrl = QEC_CTRL_BMODE | i;

	/* Allocate memory if not done yet */
	if (sc->sc_desc_dva == NULL)
		sc->sc_desc_dva = (struct be_desc *)dvma_malloc(
		    sizeof(struct be_desc), &sc->sc_desc, M_NOWAIT);
	if (sc->sc_bufs_dva == NULL)
		sc->sc_bufs_dva = (struct be_bufs *)dvma_malloc(
		    sizeof(struct be_bufs), &sc->sc_bufs, M_NOWAIT);

	/* chain descriptors into buffers */
	sc->sc_txnew = 0;
	sc->sc_rxnew = 0;
	sc->sc_txold = 0;
	sc->sc_rxold = 0;
	for (i = 0; i < RX_RING_SIZE; i++) {
		sc->sc_desc->be_rxd[i].rx_addr =
		    (u_int32_t)&sc->sc_bufs_dva->rx_buf[i][0];
		sc->sc_desc->be_rxd[i].rx_flags = 0;
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		sc->sc_desc->be_txd[i].tx_addr = 0;
		sc->sc_desc->be_txd[i].tx_flags = 0;
	}

	be_tcvr_init(sc);
	be_tcvr_setspeed(sc);

	bestop(sc);

	sc->sc_br->mac_addr2 = (sc->sc_arpcom.ac_enaddr[4] << 8) |
	    sc->sc_arpcom.ac_enaddr[5];
	sc->sc_br->mac_addr1 = (sc->sc_arpcom.ac_enaddr[2] << 8) |
	    sc->sc_arpcom.ac_enaddr[3];
	sc->sc_br->mac_addr0 = (sc->sc_arpcom.ac_enaddr[0] << 8) |
	    sc->sc_arpcom.ac_enaddr[1];

	sc->sc_br->htable3 = 0;
	sc->sc_br->htable2 = 0;
	sc->sc_br->htable1 = 0;
	sc->sc_br->htable0 = 0;

	sc->sc_br->rx_cfg = BE_RXCFG_HENABLE | BE_RXCFG_FIFO;
	DELAY(20);

	sc->sc_br->tx_cfg = BE_TXCFG_FIFO;
	sc->sc_br->rand_seed = 0xbd;

	sc->sc_br->xif_cfg = BE_XCFG_ODENABLE | BE_XCFG_RESV;

	sc->sc_cr->rxds = (u_int32_t)&sc->sc_desc_dva->be_rxd[0];
	sc->sc_cr->txds = (u_int32_t)&sc->sc_desc_dva->be_txd[0];

	sc->sc_cr->rxwbufptr = 0;
	sc->sc_cr->rxrbufptr = 0;
	sc->sc_cr->txwbufptr = sc->sc_memsize;
	sc->sc_cr->txrbufptr = sc->sc_memsize;
	
	sc->sc_br->imask = 0;

	sc->sc_cr->rimask = 0;
	sc->sc_cr->timask = 0;
	sc->sc_cr->qmask = 0;
	sc->sc_cr->bmask = 0;

	sc->sc_br->jsize = 4;

	sc->sc_cr->ccnt = 0;

	sc->sc_br->tx_cfg |= BE_TXCFG_ENABLE;
	sc->sc_br->rx_cfg |= BE_RXCFG_ENABLE;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);
}
