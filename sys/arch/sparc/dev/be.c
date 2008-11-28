/*	$OpenBSD: be.c,v 1.43 2008/11/28 02:44:17 brad Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt and Jason L. Wright.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/if_media.h>

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

int	bematch(struct device *, void *, void *);
void	beattach(struct device *, struct device *, void *);

void	beinit(struct besoftc *);
void	bestart(struct ifnet *);
void	bestop(struct besoftc *);
void	bewatchdog(struct ifnet *);
int	beioctl(struct ifnet *, u_long, caddr_t);
void	bereset(struct besoftc *);

int	beintr(void *);
int	berint(struct besoftc *);
int	betint(struct besoftc *);
int	beqint(struct besoftc *, u_int32_t);
int	beeint(struct besoftc *, u_int32_t);
void	be_read(struct besoftc *, int, int);

void	be_tcvr_idle(struct besoftc *);
void	be_tcvr_init(struct besoftc *);
void	be_tcvr_write(struct besoftc *, u_int8_t, u_int16_t);
void	be_tcvr_write_bit(struct besoftc *, int);
int	be_tcvr_read_bit1(struct besoftc *);
int	be_tcvr_read_bit2(struct besoftc *);
int	be_tcvr_read(struct besoftc *, u_int8_t);
void	be_ifmedia_sts(struct ifnet *, struct ifmediareq *);
int	be_ifmedia_upd(struct ifnet *);
void	be_mcreset(struct besoftc *);
void	betick(void *);
void	be_tx_harvest(struct besoftc *);

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
	struct bootpath *bp;
	extern void myetheraddr(u_char *);
	int pri, bmsr;

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;
	sc->sc_rev = getpropint(ca->ca_ra.ra_node, "board-version", -1);

	timeout_set(&sc->sc_tick, betick, sc);

	sc->sc_cr = mapiodev(&ca->ca_ra.ra_reg[0], 0, sizeof(struct be_cregs));
	sc->sc_br = mapiodev(&ca->ca_ra.ra_reg[1], 0, sizeof(struct be_bregs));
	sc->sc_tr = mapiodev(&ca->ca_ra.ra_reg[2], 0, sizeof(struct be_tregs));
	sc->sc_qec = qec;
	sc->sc_qr = qec->sc_regs;
	bestop(sc);

	sc->sc_channel = getpropint(ca->ca_ra.ra_node, "channel#", -1);
	if (sc->sc_channel == -1)
		sc->sc_channel = 0;

	sc->sc_burst = getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		sc->sc_burst = qec->sc_burst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= qec->sc_burst;

	sc->sc_ih.ih_fun = beintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, IPL_NET, sc->sc_dev.dv_xname);

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	be_tcvr_init(sc);

	ifmedia_init(&sc->sc_ifmedia, 0, be_ifmedia_upd, be_ifmedia_sts);
	bmsr = be_tcvr_read(sc, PHY_BMSR);
	if (bmsr == BE_TCVR_READ_INVALID)
		return;

	if (bmsr & PHY_BMSR_10BASET_HALF) {
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER | IFM_10_T, 0, NULL);
		ifmedia_add(&sc->sc_ifmedia,
		    IFM_ETHER | IFM_10_T | IFM_HDX, 0, NULL);
		sc->sc_ifmedia.ifm_media = IFM_ETHER | IFM_10_T | IFM_HDX;
	}

	if (bmsr & PHY_BMSR_10BASET_FULL) {
		ifmedia_add(&sc->sc_ifmedia,
		    IFM_ETHER | IFM_10_T | IFM_FDX, 0, NULL);
		sc->sc_ifmedia.ifm_media = IFM_ETHER | IFM_10_T | IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BASETX_HALF) {
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER | IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->sc_ifmedia,
		    IFM_ETHER | IFM_100_TX | IFM_HDX, 0, NULL);
		sc->sc_ifmedia.ifm_media = IFM_ETHER | IFM_100_TX | IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BASETX_FULL) {
		ifmedia_add(&sc->sc_ifmedia,
		    IFM_ETHER | IFM_100_TX | IFM_FDX, 0, NULL);
		sc->sc_ifmedia.ifm_media = IFM_ETHER | IFM_100_TX | IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BASET4) {
		ifmedia_add(&sc->sc_ifmedia,
		    IFM_ETHER | IFM_100_T4, 0, NULL);
		sc->sc_ifmedia.ifm_media = IFM_ETHER | IFM_100_T4;
	}

	if (bmsr & PHY_BMSR_ANC) {
		ifmedia_add(&sc->sc_ifmedia,
		    IFM_ETHER | IFM_AUTO, 0, NULL);
		sc->sc_ifmedia.ifm_media = IFM_ETHER | IFM_AUTO;
	}

	ifmedia_set(&sc->sc_ifmedia, sc->sc_ifmedia.ifm_media);

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = bestart;
	ifp->if_ioctl = beioctl;
	ifp->if_watchdog = bewatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS |
	    IFF_MULTICAST;

	IFQ_SET_MAXLEN(&ifp->if_snd, BE_TX_RING_SIZE);
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	printf(" pri %d: rev %x address %s\n", pri, sc->sc_rev,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	bp = ca->ca_ra.ra_bp;
	if (bp != NULL && strcmp(bp->name, "be") == 0 &&
	    sc->sc_dev.dv_unit == bp->val[1])
		bp->dev = &sc->sc_dev;
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
	struct besoftc *sc = (struct besoftc *)ifp->if_softc;
	struct mbuf *m;
	int bix, len, cnt;

	if (sc->sc_no_td > 0) {
		/* Try to free previous stuff */
		be_tx_harvest(sc);
	}

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_last_td;
	cnt = sc->sc_no_td;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		IFQ_DEQUEUE(&ifp->if_snd, m);

#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		len = qec_put(sc->sc_bufs->tx_buf[bix & BE_TX_RING_MASK], m);

		/*
		 * Initialize transmit registers and start transmission
		 */
		sc->sc_desc->be_txd[bix].tx_flags =
			BE_TXD_OWN | BE_TXD_SOP | BE_TXD_EOP |
			(len & BE_TXD_LENGTH);
		sc->sc_cr->ctrl = BE_CR_CTRL_TWAKEUP;

		if (++bix == BE_TX_RING_MAXSIZE)
			bix = 0;

		if (++cnt == BE_TX_RING_SIZE) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	if (cnt > BE_TX_HIGH_WATER) {
		/* turn on interrupt */
		sc->sc_tx_intr = 1;
		sc->sc_cr->timask = 0;
	}

	if (cnt != sc->sc_no_td) {
		ifp->if_timer = 5;
		sc->sc_last_td = bix;
		sc->sc_no_td = cnt;
	}
}

void
bestop(sc)
	struct besoftc *sc;
{
	int tries;

	sc->sc_arpcom.ac_if.if_timer = 0;
	if (timeout_pending(&sc->sc_tick))
		timeout_del(&sc->sc_tick);

	tries = 32;
	sc->sc_br->tx_cfg = 0;
	while (sc->sc_br->tx_cfg != 0 && --tries)
		DELAY(20);

	tries = 32;
	sc->sc_br->rx_cfg = 0;
	while (sc->sc_br->rx_cfg != 0 && --tries)
		DELAY(20);
}

/*
 * Reset interface.
 */
void
bereset(sc)
	struct besoftc *sc;
{
	int s;

	s = splnet();
	bestop(sc);
	beinit(sc);
	splx(s);
}

void
bewatchdog(ifp)
	struct ifnet *ifp;
{
	struct besoftc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	bereset(sc);
}

int
beintr(v)
	void *v;
{
	struct besoftc *sc = (struct besoftc *)v;
	u_int32_t whyq, whyb, whyc;
	int r = 0;

	whyq = sc->sc_qr->stat;		/* qec status */
	whyc = sc->sc_cr->stat;		/* be channel status */
	whyb = sc->sc_br->stat;		/* be status */

	if (whyq & QEC_STAT_BM)
		r |= beeint(sc, whyb);

	if (whyq & QEC_STAT_ER)
		r |= beqint(sc, whyc);

	if (sc->sc_tx_intr && (whyq & QEC_STAT_TX) && (whyc & BE_CR_STAT_TXIRQ))
		r |= betint(sc);

	if (whyq & QEC_STAT_RX && whyc & BE_CR_STAT_RXIRQ)
		r |= berint(sc);

	return (r);
}

/*
 * QEC Interrupt.
 */
int
beqint(sc, why)
	struct besoftc *sc;
	u_int32_t why;
{
	int r = 0, rst = 0;

	if (why & BE_CR_STAT_TXIRQ)
		r |= 1;
	if (why & BE_CR_STAT_RXIRQ)
		r |= 1;

	if (why & BE_CR_STAT_ERRORS) {
		r |= 1;
		rst = 1;
	}

	if (rst || r == 0) {
		printf("%s:%s qstat=%b\n", sc->sc_dev.dv_xname,
		    (r) ? "" : " unexpected",
		    why, BE_CR_STAT_BITS);
		printf("%s: resetting\n", sc->sc_dev.dv_xname);
		bereset(sc);
	}

	return r;
}

/*
 * Error interrupt.
 */
int
beeint(sc, why)
	struct besoftc *sc;
	u_int32_t why;
{
	int r = 0;

	if (why & (BE_BR_STAT_RFIFOVF | BE_BR_STAT_TFIFO_UND |
		   BE_BR_STAT_MAXPKTERR)) {
		r |= 1;
	}

	printf("%s:%s stat=%b\n", sc->sc_dev.dv_xname,
	    (r) ? "" : " unexpected", why, BE_BR_STAT_BITS);

	printf("%s: resetting\n", sc->sc_dev.dv_xname);
	bereset(sc);

	return r;
}

void
be_tx_harvest(sc)
	struct besoftc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int bix, cnt;
	struct be_txd txd;

	bix = sc->sc_first_td;
	cnt = sc->sc_no_td;

	for (;;) {
		if (cnt <= 0)
			break;

		txd.tx_flags = sc->sc_desc->be_txd[bix].tx_flags;

		if (txd.tx_flags & BE_TXD_OWN)
			break;

		ifp->if_opackets++;

		if (++bix == BE_TX_RING_MAXSIZE)
			bix = 0;

		--cnt;
	}

	if (cnt <= 0)
		ifp->if_timer = 0;

	if (sc->sc_no_td != cnt) {
		sc->sc_first_td = bix;
		sc->sc_no_td = cnt;
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	if (sc->sc_no_td < BE_TX_LOW_WATER) {
		/* turn off interrupt */
		sc->sc_tx_intr = 0;
		sc->sc_cr->timask = 0xffffffff;
	}
}

/*
 * Transmit interrupt.
 */
int
betint(sc)
	struct besoftc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	bestart(ifp);
	return (1);
}

/*
 * Receive interrupt.
 */
int
berint(sc)
	struct besoftc *sc;
{
	int bix, len;

	bix = sc->sc_last_rd;

	/*
	 * Process all buffers with valid data.
	 */
	for (;;) {
		if (sc->sc_desc->be_rxd[bix].rx_flags & BE_RXD_OWN)
			break;

		len = sc->sc_desc->be_rxd[bix].rx_flags & BE_RXD_LENGTH;
		be_read(sc, bix, len);

		sc->sc_desc->be_rxd[(bix + BE_RX_RING_SIZE) & BE_RX_RING_MAXMASK].rx_flags =
		    BE_RXD_OWN | (BE_PKT_BUF_SZ & BE_RXD_LENGTH);

		if (++bix == BE_RX_RING_MAXSIZE)
			bix = 0;
	}

	sc->sc_last_rd = bix;

	return 1;
}

void
betick(vsc)
	void *vsc;
{
	struct besoftc *sc = vsc;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct be_bregs *br = sc->sc_br;
	int s;

	s = splnet();
	/*
	 * Get collision counters
	 */
	ifp->if_collisions += br->nc_ctr + br->fc_ctr + br->ex_ctr + br->lt_ctr;
	br->nc_ctr = 0;
	br->fc_ctr = 0;
	br->ex_ctr = 0;
	br->lt_ctr = 0;
	bestart(ifp);
	splx(s);
	timeout_add_sec(&sc->sc_tick, 1);
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
		default:
			beinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
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
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			be_mcreset(sc);
		error = 0;
	}

	splx(s);
	return error;
}

void
beinit(sc)
	struct besoftc *sc;
{
	struct be_bregs *br = sc->sc_br;
	struct be_cregs *cr = sc->sc_cr;
	struct qec_softc *qec = sc->sc_qec;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s = splnet();
	int i;

	/*
	 * Allocate descriptor ring and buffers, if not already done
	 */
	if (sc->sc_desc == NULL)
		sc->sc_desc_dva = (struct be_desc *) dvma_malloc(
			sizeof(struct be_desc), &sc->sc_desc, M_NOWAIT);
	if (sc->sc_bufs == NULL)
		sc->sc_bufs_dva = (struct be_bufs *) dvma_malloc(
			sizeof(struct be_bufs), &sc->sc_bufs, M_NOWAIT);
	
	for (i = 0; i < BE_TX_RING_MAXSIZE; i++) {
		sc->sc_desc->be_txd[i].tx_addr =
			(u_int32_t)sc->sc_bufs_dva->tx_buf[i & BE_TX_RING_MASK];
		sc->sc_desc->be_txd[i].tx_flags = 0;
	}
	for (i = 0; i < BE_RX_RING_MAXSIZE; i++) {
		sc->sc_desc->be_rxd[i].rx_addr =
		    (u_int32_t)sc->sc_bufs_dva->rx_buf[i & BE_RX_RING_MASK];
		if (i < BE_RX_RING_SIZE)
			sc->sc_desc->be_rxd[i].rx_flags =
			    BE_RXD_OWN | (BE_PKT_BUF_SZ & BE_RXD_LENGTH);
		else
			sc->sc_desc->be_rxd[i].rx_flags = 0;
	}

	sc->sc_first_td = sc->sc_last_td = sc->sc_no_td = 0;
	sc->sc_last_rd = 0;

	be_tcvr_init(sc);

	be_ifmedia_upd(ifp);

	bestop(sc);

	br->mac_addr2 = (sc->sc_arpcom.ac_enaddr[4] << 8) |
	    sc->sc_arpcom.ac_enaddr[5];
	br->mac_addr1 = (sc->sc_arpcom.ac_enaddr[2] << 8) |
	    sc->sc_arpcom.ac_enaddr[3];
	br->mac_addr0 = (sc->sc_arpcom.ac_enaddr[0] << 8) |
	    sc->sc_arpcom.ac_enaddr[1];

	br->rx_cfg = BE_BR_RXCFG_HENABLE | BE_BR_RXCFG_FIFO;

	be_mcreset(sc);

	DELAY(20);

	br->tx_cfg = BE_BR_TXCFG_FIFO;
	br->rand_seed = 0xbd;

	br->xif_cfg = BE_BR_XCFG_ODENABLE | BE_BR_XCFG_RESV;

	cr->rxds = (u_int32_t)sc->sc_desc_dva->be_rxd;
	cr->txds = (u_int32_t)sc->sc_desc_dva->be_txd;

	cr->rxwbufptr = cr->rxrbufptr = sc->sc_channel * qec->sc_msize;
	cr->txwbufptr = cr->txrbufptr = cr->rxrbufptr + qec->sc_rsize;

	/*
	 * Turn off counter expiration interrupts as well as
	 * 'gotframe' and 'sentframe'
	 */
	br->imask = BE_BR_IMASK_GOTFRAME	|
		    BE_BR_IMASK_RCNTEXP		|
		    BE_BR_IMASK_ACNTEXP		|
		    BE_BR_IMASK_CCNTEXP		|
		    BE_BR_IMASK_LCNTEXP		|
		    BE_BR_IMASK_CVCNTEXP	|
		    BE_BR_IMASK_SENTFRAME	|
		    BE_BR_IMASK_NCNTEXP		|
		    BE_BR_IMASK_ECNTEXP		|
		    BE_BR_IMASK_LCCNTEXP	|
		    BE_BR_IMASK_FCNTEXP		|
		    BE_BR_IMASK_DTIMEXP;

	cr->rimask = 0;

	/* disable tx interrupts initially */
	cr->timask = 0xffffffff;
	sc->sc_tx_intr = 0;

	cr->qmask = 0;
	cr->bmask = 0;

	br->jsize = 4;

	cr->ccnt = 0;

	br->tx_cfg |= BE_BR_TXCFG_ENABLE;
	br->rx_cfg |= BE_BR_RXCFG_ENABLE;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);

	timeout_add_sec(&sc->sc_tick, 1);
	bestart(ifp);
}

/*
 * Set the tcvr to an idle state
 */
void
be_tcvr_idle(sc)
	struct besoftc *sc;
{
	struct be_tregs *tr = sc->sc_tr;
	volatile u_int32_t x;
	int i = 20;

	while (i--) {
		tr->mgmt_pal = MGMT_PAL_INT_MDIO | MGMT_PAL_EXT_MDIO |
			       MGMT_PAL_OENAB;
		x = tr->mgmt_pal;
		tr->mgmt_pal = MGMT_PAL_INT_MDIO | MGMT_PAL_EXT_MDIO |
			       MGMT_PAL_OENAB | MGMT_PAL_DCLOCK;
		x = tr->mgmt_pal;
	}
}

/*
 * Initialize the transceiver and figure out whether we're using the
 * external or internal one.
 */
void
be_tcvr_init(sc)
	struct besoftc *sc;
{
	volatile u_int32_t x;
	struct be_tregs *tr = sc->sc_tr;

	be_tcvr_idle(sc);

	if (sc->sc_rev != 1) {
		printf("%s: rev %d PAL not supported.\n",
			sc->sc_dev.dv_xname,
			sc->sc_rev);
		return;
	}

	tr->mgmt_pal = MGMT_PAL_INT_MDIO | MGMT_PAL_EXT_MDIO | MGMT_PAL_DCLOCK;
	x = tr->mgmt_pal;

	tr->mgmt_pal = MGMT_PAL_INT_MDIO | MGMT_PAL_EXT_MDIO;
	x = tr->mgmt_pal;
	DELAY(200);

	if (tr->mgmt_pal & MGMT_PAL_EXT_MDIO) {
		sc->sc_tcvr_type = BE_TCVR_EXTERNAL;
		tr->tcvr_pal = ~(TCVR_PAL_EXTLBACK | TCVR_PAL_MSENSE |
				 TCVR_PAL_LTENABLE);
		x = tr->tcvr_pal;
	}
	else if (tr->mgmt_pal & MGMT_PAL_INT_MDIO) {
		sc->sc_tcvr_type = BE_TCVR_INTERNAL;
		tr->tcvr_pal = ~(TCVR_PAL_EXTLBACK | TCVR_PAL_MSENSE |
				 TCVR_PAL_LTENABLE | TCVR_PAL_SERIAL);
		x = tr->tcvr_pal;
	}
	else {
		printf("%s: no internal or external transceiver found.\n",
			sc->sc_dev.dv_xname);
	}
}

int
be_tcvr_read(sc, reg)
	struct besoftc *sc;
	u_int8_t reg;
{
	int phy, i;
	u_int32_t ret = 0;

	if (sc->sc_tcvr_type == BE_TCVR_INTERNAL)
		phy = BE_PHY_INTERNAL;
	else if (sc->sc_tcvr_type == BE_TCVR_EXTERNAL)
		phy = BE_PHY_EXTERNAL;
	else {
		printf("%s: invalid tcvr type\n", sc->sc_dev.dv_xname);
		return BE_TCVR_READ_INVALID;
	}

	be_tcvr_idle(sc);

	be_tcvr_write_bit(sc, 0);
	be_tcvr_write_bit(sc, 1);
	be_tcvr_write_bit(sc, 1);
	be_tcvr_write_bit(sc, 0);

	for (i = 4; i >= 0; i--)
		be_tcvr_write_bit(sc, (phy >> i) & 1);

	for (i = 4; i >= 0; i--)
		be_tcvr_write_bit(sc, (reg >> i) & 1);

	if (sc->sc_tcvr_type == BE_TCVR_EXTERNAL) {
		(void) be_tcvr_read_bit2(sc);
		(void) be_tcvr_read_bit2(sc);

		for (i = 15; i >= 0; i--) {
			int b;

			b = be_tcvr_read_bit2(sc);
			ret |= (b & 1) << i;
		}

		(void) be_tcvr_read_bit2(sc);
		(void) be_tcvr_read_bit2(sc);
		(void) be_tcvr_read_bit2(sc);
	}
	else {
		(void) be_tcvr_read_bit1(sc);
		(void) be_tcvr_read_bit1(sc);

		for (i = 15; i >= 0; i--) {
			int b;

			b = be_tcvr_read_bit1(sc);
			ret |= (b & 1) << i;
		}

		(void) be_tcvr_read_bit1(sc);
		(void) be_tcvr_read_bit1(sc);
		(void) be_tcvr_read_bit1(sc);
	}
	return ret;
}

int
be_tcvr_read_bit1(sc)
	struct besoftc *sc;
{
	volatile u_int32_t x;
	struct be_tregs *tr = sc->sc_tr;
	int ret = 0;

	if (sc->sc_tcvr_type == BE_TCVR_INTERNAL) {
		tr->mgmt_pal = MGMT_PAL_EXT_MDIO;
		x = tr->mgmt_pal;
		tr->mgmt_pal = MGMT_PAL_EXT_MDIO | MGMT_PAL_DCLOCK;
		x = tr->mgmt_pal;
		DELAY(20);
		ret = (tr->mgmt_pal & MGMT_PAL_INT_MDIO) >> 3;
	} else if (sc->sc_tcvr_type == BE_TCVR_EXTERNAL) {
		tr->mgmt_pal = MGMT_PAL_INT_MDIO;
		x = tr->mgmt_pal;
		tr->mgmt_pal = MGMT_PAL_INT_MDIO | MGMT_PAL_DCLOCK;
		x = tr->mgmt_pal;
		DELAY(20);
		ret = (tr->mgmt_pal & MGMT_PAL_EXT_MDIO) >> 2;
	} else {
		printf("%s: invalid tcvr type\n", sc->sc_dev.dv_xname);
	}
	return (ret & 1);
}

int
be_tcvr_read_bit2(sc)
	struct besoftc *sc;
{
	volatile u_int32_t x;
	struct be_tregs *tr = sc->sc_tr;
	int ret = 0;

	if (sc->sc_tcvr_type == BE_TCVR_INTERNAL) {
		tr->mgmt_pal = MGMT_PAL_EXT_MDIO;
		x = tr->mgmt_pal;
		DELAY(20);
		ret = (tr->mgmt_pal & MGMT_PAL_INT_MDIO) >> 3;
		tr->mgmt_pal = MGMT_PAL_EXT_MDIO | MGMT_PAL_DCLOCK;
		x = tr->mgmt_pal;
	} else if (sc->sc_tcvr_type == BE_TCVR_EXTERNAL) {
		tr->mgmt_pal = MGMT_PAL_INT_MDIO;
		x = tr->mgmt_pal;
		DELAY(20);
		ret = (tr->mgmt_pal & MGMT_PAL_EXT_MDIO) >> 2;
		tr->mgmt_pal = MGMT_PAL_INT_MDIO | MGMT_PAL_DCLOCK;
		x = tr->mgmt_pal;
	} else {
		printf("%s: invalid tcvr type\n", sc->sc_dev.dv_xname);
	}
	return ret;
}

void
be_tcvr_write(sc, reg, val)
	struct besoftc *sc;
	u_int8_t reg;
	u_int16_t val;
{
	int phy, i;

	if (sc->sc_tcvr_type == BE_TCVR_INTERNAL)
		phy = BE_PHY_INTERNAL;
	else if (sc->sc_tcvr_type == BE_TCVR_EXTERNAL)
		phy = BE_PHY_EXTERNAL;
	else {
		printf("%s: invalid tcvr type\n", sc->sc_dev.dv_xname);
		return;
	}

	be_tcvr_idle(sc);

	be_tcvr_write_bit(sc, 0);
	be_tcvr_write_bit(sc, 1);
	be_tcvr_write_bit(sc, 0);
	be_tcvr_write_bit(sc, 1);

	for (i = 4; i >= 0; i--)
		be_tcvr_write_bit(sc, (phy >> i) & 1);

	for (i = 4; i >= 0; i--)
		be_tcvr_write_bit(sc, (reg >> i) & 1);

	be_tcvr_write_bit(sc, 1);
	be_tcvr_write_bit(sc, 0);

	for (i = 15; i >= 0; i--)
		be_tcvr_write_bit(sc, (val >> i) & 1);
}

void
be_tcvr_write_bit(sc, bit)
	struct besoftc *sc;
	int bit;
{
	volatile u_int32_t x;

	if (sc->sc_tcvr_type == BE_TCVR_INTERNAL) {
		bit = ((bit & 1) << 3) | MGMT_PAL_OENAB | MGMT_PAL_EXT_MDIO;
		sc->sc_tr->mgmt_pal = bit;
		x = sc->sc_tr->mgmt_pal;
		sc->sc_tr->mgmt_pal = bit | MGMT_PAL_DCLOCK;
		x = sc->sc_tr->mgmt_pal;
	} else {
		bit = ((bit & 1) << 2) | MGMT_PAL_OENAB | MGMT_PAL_INT_MDIO;
		sc->sc_tr->mgmt_pal = bit;
		x = sc->sc_tr->mgmt_pal;
		sc->sc_tr->mgmt_pal = bit | MGMT_PAL_DCLOCK;
		x = sc->sc_tr->mgmt_pal;
	}
}

/*
 * Pass a packet to the higher levels.
 */
void
be_read(sc, idx, len)
	struct besoftc *sc;
	int idx, len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;

	if (len <= sizeof(struct ether_header) ||
	    len > ETHERMTU + sizeof(struct ether_header)) {

		printf("%s: invalid packet size %d; dropping\n",
			ifp->if_xname, len);

		ifp->if_ierrors++;
		return;
	}

	/*
	 * Pull packet off interface.
	 */
	m = qec_get(ifp, sc->sc_bufs->rx_buf[idx & BE_RX_RING_MASK], len);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}
	ifp->if_ipackets++;


#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif
	/* Pass the packet up. */
	ether_input_mbuf(ifp, m);
}

/*
 * Get current media settings.
 */
void
be_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct besoftc *sc = ifp->if_softc;
	int bmcr, bmsr;

	bmcr = be_tcvr_read(sc, PHY_BMCR);

	switch (bmcr & (PHY_BMCR_SPEED | PHY_BMCR_DUPLEX)) {
	case (PHY_BMCR_SPEED | PHY_BMCR_DUPLEX):
		ifmr->ifm_active = IFM_ETHER | IFM_100_TX | IFM_FDX;
		break;
	case PHY_BMCR_SPEED:
		ifmr->ifm_active = IFM_ETHER | IFM_100_TX | IFM_HDX;
		break;
	case PHY_BMCR_DUPLEX:
		ifmr->ifm_active = IFM_ETHER | IFM_10_T | IFM_FDX;
		break;
	case 0:
		ifmr->ifm_active = IFM_ETHER | IFM_10_T | IFM_HDX;
		break;
	}

	bmsr = be_tcvr_read(sc, PHY_BMSR);
	if (bmsr & PHY_BMSR_LINKSTATUS)
		ifmr->ifm_status |=  IFM_AVALID | IFM_ACTIVE;
	else {
		ifmr->ifm_status |=  IFM_AVALID;
		ifmr->ifm_status &= ~IFM_ACTIVE;
	}
}

/*
 * Set media options.
 */
int
be_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct besoftc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	int bmcr, tries;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	be_tcvr_write(sc, PHY_BMCR,
		PHY_BMCR_LOOPBACK | PHY_BMCR_PDOWN | PHY_BMCR_ISOLATE);
	be_tcvr_write(sc, PHY_BMCR, PHY_BMCR_RESET);

	for (tries = 16; tries >= 0; tries--) {
		bmcr = be_tcvr_read(sc, PHY_BMCR);
		if ((bmcr & PHY_BMCR_RESET) == 0)
			break;
		DELAY(20);
	}
	if (tries == 0) {
		printf("%s: bmcr reset failed\n", sc->sc_dev.dv_xname);
		return (EIO);
	}

	bmcr = be_tcvr_read(sc, PHY_BMCR);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_T4) {
		bmcr |= PHY_BMCR_SPEED;
		bmcr &= ~PHY_BMCR_DUPLEX;
	}

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX) {
		bmcr |= PHY_BMCR_SPEED;
	}

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_10_T) {
		bmcr &= ~PHY_BMCR_SPEED;
	}

	if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
		bmcr |= PHY_BMCR_DUPLEX;
		sc->sc_br->tx_cfg |= BE_BR_TXCFG_FULLDPLX;
	}
	else {
		bmcr &= ~PHY_BMCR_DUPLEX;
		sc->sc_br->tx_cfg &= ~BE_BR_TXCFG_FULLDPLX;
	}

	be_tcvr_write(sc, PHY_BMCR, bmcr & (~PHY_BMCR_ISOLATE));

	for (tries = 32; tries >= 0; tries--) {
		bmcr = be_tcvr_read(sc, PHY_BMCR);
		if ((bmcr & PHY_BMCR_ISOLATE) == 0)
			break;
		DELAY(20);
	}
	if (tries == 0) {
		printf("%s: bmcr unisolate failed\n", sc->sc_dev.dv_xname);
		return (EIO);
	}

	return (0);
}

void
be_mcreset(sc)
	struct besoftc *sc;
{
	struct arpcom *ac = &sc->sc_arpcom;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct be_bregs *br = sc->sc_br;
	u_int32_t crc;
	u_int16_t hash[4];
	u_int8_t octet;
	int i, j;
	struct ether_multi *enm;
	struct ether_multistep step;

	if (ifp->if_flags & IFF_PROMISC) {
		br->rx_cfg |= BE_BR_RXCFG_PMISC;
		return;
	}
	else
		br->rx_cfg &= ~BE_BR_RXCFG_PMISC;

	if (ifp->if_flags & IFF_ALLMULTI) {
		br->htable3 = 0xffff;
		br->htable2 = 0xffff;
		br->htable1 = 0xffff;
		br->htable0 = 0xffff;
		return;
	}

	hash[3] = hash[2] = hash[1] = hash[0] = 0;

	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast
			 * addresses.  For now, just accept all
			 * multicasts, rather than trying to set only
			 * those filter bits needed to match the range.
			 * (At this time, the only use of address
			 * ranges is for IP multicast routing, for
			 * which the range is big enough to require
			 * all bits set.)
			 */
			br->htable3 = 0xffff;
			br->htable2 = 0xffff;
			br->htable1 = 0xffff;
			br->htable0 = 0xffff;
			ifp->if_flags |= IFF_ALLMULTI;
			return;
		}

		crc = 0xffffffff;

		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			octet = enm->enm_addrlo[i];

			for (j = 0; j < 8; j++) {
				if ((crc & 1) ^ (octet & 1)) {
					crc >>= 1;
					crc ^= MC_POLY_LE;
				}
				else
					crc >>= 1;
				octet >>= 1;
			}
		}

		crc >>= 26;
		hash[crc >> 4] |= 1 << (crc & 0xf);
		ETHER_NEXT_MULTI(step, enm);
	}

	br->htable3 = hash[3];
	br->htable2 = hash[2];
	br->htable1 = hash[1];
	br->htable0 = hash[0];
	ifp->if_flags &= ~IFF_ALLMULTI;
}
