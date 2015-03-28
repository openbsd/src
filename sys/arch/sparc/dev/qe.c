/*	$OpenBSD: qe.c,v 1.38 2015/03/28 19:07:07 miod Exp $	*/

/*
 * Copyright (c) 1998, 2000 Jason L. Wright.
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

/*
 * Driver for the SBus qec+qe QuadEthernet board.
 *
 * This driver was written using the AMD MACE Am79C940 documentation, some
 * ideas gleaned from the S/Linux driver for this card, Solaris header files,
 * and a loan of a card from Paul Southworth of the Internet Engineering
 * Group (www.ieng.com).
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

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>

#include <sparc/dev/qecvar.h>
#include <sparc/dev/qecreg.h>
#include <sparc/dev/qereg.h>
#include <sparc/dev/qevar.h>

int	qematch(struct device *, void *, void *);
void	qeattach(struct device *, struct device *, void *);

void	qeinit(struct qesoftc *);
void	qestart(struct ifnet *);
void	qestop(struct qesoftc *);
void	qewatchdog(struct ifnet *);
int	qeioctl(struct ifnet *, u_long, caddr_t);
void	qereset(struct qesoftc *);

int		qeintr(void *);
int		qe_eint(struct qesoftc *, u_int32_t);
int		qe_rint(struct qesoftc *);
int		qe_tint(struct qesoftc *);
void		qe_read(struct qesoftc *, int, int);
void		qe_mcreset(struct qesoftc *);
void		qe_ifmedia_sts(struct ifnet *, struct ifmediareq *);
int		qe_ifmedia_upd(struct ifnet *);

struct cfdriver qe_cd = {
	NULL, "qe", DV_IFNET
};

struct cfattach qe_ca = {
	sizeof(struct qesoftc), qematch, qeattach
};

int
qematch(parent, vcf, aux)
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
qeattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct qec_softc *qec = (struct qec_softc *)parent;
	struct qesoftc *sc = (struct qesoftc *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct confargs *ca = aux;
	struct bootpath *bp;
	extern void myetheraddr(u_char *);
	int pri;

	if (qec->sc_pri == 0) {
		printf(": no interrupt found on parent\n");
		return;
	}
	pri = qec->sc_pri;

	sc->sc_rev = getpropint(ca->ca_ra.ra_node, "mace-version", -1);

	sc->sc_cr = mapiodev(&ca->ca_ra.ra_reg[0], 0, sizeof(struct qe_cregs));
	sc->sc_mr = mapiodev(&ca->ca_ra.ra_reg[1], 0, sizeof(struct qe_mregs));
	sc->sc_qec = qec;
	sc->sc_qr = qec->sc_regs;
	qestop(sc);

	sc->sc_channel = getpropint(ca->ca_ra.ra_node, "channel#", -1);
	sc->sc_burst = qec->sc_burst;

	sc->sc_ih.ih_fun = qeintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, IPL_NET, self->dv_xname);

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = qestart;
	ifp->if_ioctl = qeioctl;
	ifp->if_watchdog = qewatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS |
	    IFF_MULTICAST;

	ifmedia_init(&sc->sc_ifmedia, IFM_IMASK,
	    qe_ifmedia_upd, qe_ifmedia_sts);
	ifmedia_add(&sc->sc_ifmedia,
	    IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, 0), 0, NULL);
	ifmedia_set(&sc->sc_ifmedia, IFM_ETHER | IFM_10_T);

	IFQ_SET_MAXLEN(&ifp->if_snd, QE_TX_RING_SIZE);
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	printf(" pri %d: rev %x address %s\n", pri, sc->sc_rev,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	bp = ca->ca_ra.ra_bp;
	if (bp != NULL && strcmp(bp->name, "qe") == 0 &&
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
qestart(ifp)
	struct ifnet *ifp;
{
	struct qesoftc *sc = (struct qesoftc *)ifp->if_softc;
	struct mbuf *m;
	int bix, len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_last_td;

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
		len = qec_put(sc->sc_bufs->tx_buf[bix & QE_TX_RING_MASK], m);

		/*
		 * Initialize transmit registers and start transmission
		 */
		sc->sc_desc->qe_txd[bix].tx_flags =
			QE_TXD_OWN | QE_TXD_SOP | QE_TXD_EOP |
			(len & QE_TXD_LENGTH);
		sc->sc_cr->ctrl = QE_CR_CTRL_TWAKEUP;

		if (++bix == QE_TX_RING_MAXSIZE)
			bix = 0;

		if (++sc->sc_no_td == QE_TX_RING_SIZE) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	sc->sc_last_td = bix;
}

void
qestop(sc)
	struct qesoftc *sc;
{	
	struct qe_cregs *cr = sc->sc_cr;
	struct qe_mregs *mr = sc->sc_mr;
	int n;

	mr->biucc = QE_MR_BIUCC_SWRST;
	for (n = 200; n > 0; n--) {
		if ((mr->biucc & QE_MR_BIUCC_SWRST) == 0)
			break;
		DELAY(20);
	}

	cr->ctrl = QE_CR_CTRL_RESET;
	for (n = 200; n > 0; n--) {
		if ((cr->ctrl & QE_CR_CTRL_RESET) == 0)
			break;
		DELAY(20);
	}
}

/*
 * Reset interface.
 */
void
qereset(sc)
	struct qesoftc *sc;
{
	qestop(sc);
	qeinit(sc);
}

void
qewatchdog(ifp)
	struct ifnet *ifp;
{
	struct qesoftc *sc = ifp->if_softc;
	int s;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	s = splnet();
	qereset(sc);
	splx(s);
}

/*
 * Interrupt dispatch.
 */
int
qeintr(v)
	void *v;
{
	struct qesoftc *sc = (struct qesoftc *)v;
	u_int32_t qecstat, qestat;
	int r = 0;

	qecstat = sc->sc_qr->stat >> (4 * sc->sc_channel);
	if ((qecstat & 0xf) == 0)
		return (r);

	qestat = sc->sc_cr->stat;

	if (qestat & QE_CR_STAT_ALLERRORS) {
		r |= qe_eint(sc, qestat);
		if (r == -1)
			return (1);
	}

	if (qestat & QE_CR_STAT_TXIRQ)
		r |= qe_tint(sc);

	if (qestat & QE_CR_STAT_RXIRQ)
		r |= qe_rint(sc);

	return (1);
}

/*
 * Transmit interrupt.
 */
int
qe_tint(sc)
	struct qesoftc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int bix;
	struct qe_txd txd;

	bix = sc->sc_first_td;

	for (;;) {
		if (sc->sc_no_td <= 0)
			break;

		txd.tx_flags = sc->sc_desc->qe_txd[bix].tx_flags;
		if (txd.tx_flags & QE_TXD_OWN)
			break;

		ifp->if_opackets++;

		if (++bix == QE_TX_RING_MAXSIZE)
			bix = 0;

		--sc->sc_no_td;
	}

	if (sc->sc_no_td == 0)
		ifp->if_timer = 0;

	/*
	 * If we freed up at least one descriptor and tx is blocked,
	 * unblock it and start it up again.
	 */
	if (sc->sc_first_td != bix) {
		sc->sc_first_td = bix;
		if (ifp->if_flags & IFF_OACTIVE) {
			ifp->if_flags &= ~IFF_OACTIVE;
			qestart(ifp);
		}
	}

	return (1);
}

/*
 * Receive interrupt.
 */
int
qe_rint(sc)
	struct qesoftc *sc;
{
	int bix, len;

	bix = sc->sc_last_rd;

	/*
	 * Process all buffers with valid data.
	 */
	for (;;) {
		if (sc->sc_desc->qe_rxd[bix].rx_flags & QE_RXD_OWN)
			break;

		len = (sc->sc_desc->qe_rxd[bix].rx_flags & QE_RXD_LENGTH) - 4;
		qe_read(sc, bix, len);
		sc->sc_desc->qe_rxd[(bix + QE_RX_RING_SIZE) & QE_RX_RING_MAXMASK].rx_flags =
		    QE_RXD_OWN | QE_RXD_LENGTH;

		if (++bix == QE_RX_RING_MAXSIZE)
			bix = 0;
	}

	sc->sc_last_rd = bix;

	return (1);
}

/*
 * Error interrupt.
 */
int
qe_eint(sc, why)
	struct qesoftc *sc;
	u_int32_t why;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int r = 0, rst = 0;

	if (why & QE_CR_STAT_EDEFER) {
		printf("%s: excessive tx defers.\n", sc->sc_dev.dv_xname);
		r |= 1;
		ifp->if_oerrors++;
	}

	if (why & QE_CR_STAT_CLOSS) {
		ifp->if_oerrors++;
		r |= 1;
	}

	if (why & QE_CR_STAT_ERETRIES) {
		printf("%s: excessive tx retries\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		r |= 1;
		rst = 1;
	}

	if (why & QE_CR_STAT_LCOLL) {
		printf("%s: late tx transmission\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		r |= 1;
		rst = 1;
	}

	if (why & QE_CR_STAT_FUFLOW) {
		printf("%s: tx fifo underflow\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		r |= 1;
		rst = 1;
	}

	if (why & QE_CR_STAT_JERROR) {
		printf("%s: jabber seen\n", sc->sc_dev.dv_xname);
		r |= 1;
	}

	if (why & QE_CR_STAT_BERROR) {
		printf("%s: babble seen\n", sc->sc_dev.dv_xname);
		r |= 1;
	}

	if (why & QE_CR_STAT_TCCOFLOW) {
		ifp->if_collisions += 256;
		ifp->if_oerrors += 256;
		r |= 1;
	}

	if (why & QE_CR_STAT_TXDERROR) {
		printf("%s: tx descriptor is bad\n", sc->sc_dev.dv_xname);
		rst = 1;
		r |= 1;
	}

	if (why & QE_CR_STAT_TXLERR) {
		printf("%s: tx late error\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		rst = 1;
		r |= 1;
	}

	if (why & QE_CR_STAT_TXPERR) {
		printf("%s: tx dma parity error\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		rst = 1;
		r |= 1;
	}

	if (why & QE_CR_STAT_TXSERR) {
		printf("%s: tx dma sbus error ack\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		rst = 1;
		r |= 1;
	}

	if (why & QE_CR_STAT_RCCOFLOW) {
		ifp->if_collisions += 256;
		ifp->if_ierrors += 256;
		r |= 1;
	}

	if (why & QE_CR_STAT_RUOFLOW) {
		ifp->if_ierrors += 256;
		r |= 1;
	}

	if (why & QE_CR_STAT_MCOFLOW) {
		ifp->if_ierrors += 256;
		r |= 1;
	}

	if (why & QE_CR_STAT_RXFOFLOW) {
		printf("%s: rx fifo overflow\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		r |= 1;
	}

	if (why & QE_CR_STAT_RLCOLL) {
		printf("%s: rx late collision\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		ifp->if_collisions++;
		r |= 1;
	}

	if (why & QE_CR_STAT_FCOFLOW) {
		ifp->if_ierrors += 256;
		r |= 1;
	}

	if (why & QE_CR_STAT_CECOFLOW) {
		ifp->if_ierrors += 256;
		r |= 1;
	}

	if (why & QE_CR_STAT_RXDROP) {
		printf("%s: rx packet dropped\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		r |= 1;
	}

	if (why & QE_CR_STAT_RXSMALL) {
		printf("%s: rx buffer too small\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		r |= 1;
		rst = 1;
	}

	if (why & QE_CR_STAT_RXLERR) {
		printf("%s: rx late error\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		r |= 1;
		rst = 1;
	}

	if (why & QE_CR_STAT_RXPERR) {
		printf("%s: rx dma parity error\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		r |= 1;
		rst = 1;
	}

	if (why & QE_CR_STAT_RXSERR) {
		printf("%s: rx dma sbus error ack\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		r |= 1;
		rst = 1;
	}

	if (r == 0)
		printf("%s: unexpected interrupt error: %08x\n",
			sc->sc_dev.dv_xname, why);

	if (rst) {
		printf("%s: resetting...\n", sc->sc_dev.dv_xname);
		qereset(sc);
		return (-1);
	}

	return (r);
}

int
qeioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct qesoftc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
		case AF_INET:
			qeinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
		default:
			qeinit(sc);
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
			qestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			qeinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			qestop(sc);
			qeinit(sc);
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
			qeinit(sc);
		error = 0;
	}

	splx(s);
	return (error);
}

void
qeinit(sc)
	struct qesoftc *sc;
{
	struct qe_mregs *mr = sc->sc_mr;
	struct qe_cregs *cr = sc->sc_cr;
	struct qec_softc *qec = sc->sc_qec;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s = splnet();
	int i;

	qestop(sc);

	/*
	 * Allocate descriptor ring and buffers, if not already done
	 */
	if (sc->sc_desc == NULL)
		sc->sc_desc_dva = (struct qe_desc *) dvma_malloc(
		    sizeof(struct qe_desc), &sc->sc_desc, M_NOWAIT | M_ZERO);

	if (sc->sc_bufs == NULL)
		sc->sc_bufs_dva = (struct qe_bufs *) dvma_malloc(
		    sizeof(struct qe_bufs), &sc->sc_bufs, M_NOWAIT | M_ZERO);

	for (i = 0; i < QE_TX_RING_MAXSIZE; i++)
		sc->sc_desc->qe_txd[i].tx_addr =
			(u_int32_t)sc->sc_bufs_dva->tx_buf[i & QE_TX_RING_MASK];
	for (i = 0; i < QE_RX_RING_MAXSIZE; i++) {
		sc->sc_desc->qe_rxd[i].rx_addr =
			(u_int32_t)sc->sc_bufs_dva->rx_buf[i & QE_RX_RING_MASK];
		if (i < QE_RX_RING_SIZE)
			sc->sc_desc->qe_rxd[i].rx_flags =
				QE_RXD_OWN | QE_RXD_LENGTH;
	}

	cr->rxds = (u_int32_t)sc->sc_desc_dva->qe_rxd;
	cr->txds = (u_int32_t)sc->sc_desc_dva->qe_txd;

	sc->sc_first_td = sc->sc_last_td = sc->sc_no_td = 0;
	sc->sc_last_rd = 0;

	cr->rimask = 0;
	cr->timask = 0;
	cr->qmask = 0;
	cr->mmask = QE_CR_MMASK_RXCOLL | QE_CR_MMASK_CLOSS;
	cr->ccnt = 0;
	cr->pipg = 0;
	cr->rxwbufptr = cr->rxrbufptr = sc->sc_channel * qec->sc_msize;
	cr->txwbufptr = cr->txrbufptr = cr->rxrbufptr + qec->sc_rsize;

	/*
	 * When switching from mace<->qec always guarantee an sbus
	 * turnaround (if last op was read, perform a dummy write, and
	 * vice versa).
	 */
	i = cr->qmask;		/* dummy */

	mr->biucc = QE_MR_BIUCC_BSWAP | QE_MR_BIUCC_64TS;
	mr->fifofc = QE_MR_FIFOCC_TXF16 | QE_MR_FIFOCC_RXF32 |
	    QE_MR_FIFOCC_RFWU | QE_MR_FIFOCC_TFWU;
	mr->xmtfc = QE_MR_XMTFC_APADXMT;
	mr->rcvfc = 0;
	mr->imr = QE_MR_IMR_CERRM | QE_MR_IMR_RCVINTM;
	mr->phycc = QE_MR_PHYCC_ASEL;
	mr->plscc = QE_MR_PLSCC_TP;

	qe_ifmedia_upd(ifp);

	mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_PHYADDR;
	for (i = 100; i > 0; i--) {
		if ((mr->iac & QE_MR_IAC_ADDRCHG) == 0)
			break;
		DELAY(2);
	}
	mr->padr = sc->sc_arpcom.ac_enaddr[0];
	mr->padr = sc->sc_arpcom.ac_enaddr[1];
	mr->padr = sc->sc_arpcom.ac_enaddr[2];
	mr->padr = sc->sc_arpcom.ac_enaddr[3];
	mr->padr = sc->sc_arpcom.ac_enaddr[4];
	mr->padr = sc->sc_arpcom.ac_enaddr[5];
	qe_mcreset(sc);
	mr->iac = 0;

	i = mr->mpc;	/* cleared on read */

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	mr->maccc = QE_MR_MACCC_ENXMT | QE_MR_MACCC_ENRCV |
	    ((ifp->if_flags & IFF_PROMISC) ? QE_MR_MACCC_PROM : 0);
	splx(s);
}

/*
 * Pass a packet to the higher levels.
 */
void
qe_read(sc, idx, len)
	struct qesoftc *sc;
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
	m = qec_get(ifp, sc->sc_bufs->rx_buf[idx & QE_RX_RING_MASK], len);
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
 * Reset multicast filter.
 */
void
qe_mcreset(sc)
	struct qesoftc *sc;
{
	struct arpcom *ac = &sc->sc_arpcom;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct qe_mregs *mr = sc->sc_mr;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int32_t crc;
	u_int16_t hash[4];
	u_int8_t octet, *ladrp = (u_int8_t *)&hash[0];
	int i, j;

	if (ac->ac_multirangecnt > 0)
		ifp->if_flags |= IFF_ALLMULTI;

	if (ifp->if_flags & IFF_ALLMULTI) {
		mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_LOGADDR;
		for (i = 100; i > 0; i--) {
			if ((mr->iac & QE_MR_IAC_ADDRCHG) == 0)
				break;
			DELAY(2);
		}
		for (i = 0; i < 8; i++)
			mr->ladrf = 0xff;
		return;
	}

	hash[3] = hash[2] = hash[1] = hash[0] = 0;

	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
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

	mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_LOGADDR;
	for (i = 100; i > 0; i--) {
		if ((mr->iac & QE_MR_IAC_ADDRCHG) == 0)
			break;
		DELAY(2);
	}
	for (i = 0; i < 8; i++)
		mr->ladrf = ladrp[i];
}

void
qe_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct qesoftc *sc = (struct qesoftc *)ifp->if_softc;
	u_int8_t phycc;

	ifmr->ifm_active = IFM_ETHER | IFM_10_T;
	phycc = sc->sc_mr->phycc;
	if ((phycc & QE_MR_PHYCC_DLNKTST) == 0) {
		ifmr->ifm_status |= IFM_AVALID;
		if (phycc & QE_MR_PHYCC_LNKFL)
			ifmr->ifm_status &= ~IFM_ACTIVE;
		else
			ifmr->ifm_status |= IFM_ACTIVE;
	}
}

int
qe_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct qesoftc *sc = (struct qesoftc *)ifp->if_softc;
	int media = sc->sc_ifmedia.ifm_media;

	if (IFM_TYPE(media) != IFM_ETHER)
		return (EINVAL);

	if (IFM_SUBTYPE(media) != IFM_10_T)
		return (EINVAL);

	return (0);
}
