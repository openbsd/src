/*	$OpenBSD: qe.c,v 1.6 1999/02/08 13:39:30 jason Exp $	*/

/*
 * Copyright (c) 1998 Jason L. Wright.
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sparc/dev/qereg.h>
#include <sparc/dev/qevar.h>

int	qematch __P((struct device *, void *, void *));
void	qeattach __P((struct device *, struct device *, void *));

void	qeinit __P((struct qesoftc *));
void	qestart __P((struct ifnet *));
void	qestop __P((struct qesoftc *));
void	qewatchdog __P((struct ifnet *));
int	qeioctl __P((struct ifnet *, u_long, caddr_t));
void	qereset __P((struct qesoftc *));

int		qeintr __P((void *));
int		qe_eint __P((struct qesoftc *, u_int32_t));
int		qe_rint __P((struct qesoftc *));
int		qe_tint __P((struct qesoftc *));
int		qe_put __P((struct qesoftc *, int, struct mbuf *));
void		qe_read __P((struct qesoftc *, int, int));
struct mbuf *	qe_get __P((struct qesoftc *, int, int));
void		qe_mcreset __P((struct qesoftc *));

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
	extern void myetheraddr __P((u_char *));
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
	intr_establish(pri, &sc->sc_ih);

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = qestart;
	ifp->if_ioctl = qeioctl;
	ifp->if_watchdog = qewatchdog;
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

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		len = qe_put(sc, bix, m);

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
	int tries;

	tries = 200;
	mr->biucc = QE_MR_BIUCC_SWRST;
	while ((mr->biucc & QE_MR_BIUCC_SWRST) && --tries)
		DELAY(20);

	tries = 200;
	cr->ctrl = QE_CR_CTRL_RESET;
	while ((cr->ctrl & QE_CR_CTRL_RESET) && --tries)
		DELAY(20);
}

/*
 * Reset interface.
 */
void
qereset(sc)
	struct qesoftc *sc;
{
	int s;

	s = splnet();
	qestop(sc);
	qeinit(sc);
	splx(s);
}

void
qewatchdog(ifp)
	struct ifnet *ifp;
{
	struct qesoftc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	qereset(sc);
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
		return r;

	qestat = sc->sc_cr->stat;

	if (qestat & QE_CR_STAT_ALLERRORS) {
		r |= qe_eint(sc, qestat);
		if (r == -1)
			return 1;
	}

	if (qestat & QE_CR_STAT_TXIRQ)
		r |= qe_tint(sc);

	if (qestat & QE_CR_STAT_RXIRQ)
		r |= qe_rint(sc);

	return r;
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

		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_opackets++;

		if (++bix == QE_TX_RING_MAXSIZE)
			bix = 0;

		--sc->sc_no_td;
	}

	sc->sc_first_td = bix;

	qestart(ifp);

	if (sc->sc_no_td == 0)
		ifp->if_timer = 0;

	return 1;
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
		sc->sc_desc->qe_rxd[(bix + QE_RX_RING_SIZE) % QE_RX_RING_MAXSIZE].rx_flags =
		    QE_RXD_OWN | QE_RXD_LENGTH;

		if (++bix == QE_RX_RING_MAXSIZE)
			bix = 0;
	}

	sc->sc_last_rd = bix;

	return 1;
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
		printf("%s: no carrier, link down?\n", sc->sc_dev.dv_xname);
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
		return -1;
	}

	return r;
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
#ifdef INET
		case AF_INET:
			qeinit(sc);
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
			qeinit(sc);
			break;
		    }
#endif /* NS */
		default:
			qeinit(sc);
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
			qe_mcreset(sc);
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
qeinit(sc)
	struct qesoftc *sc;
{
	struct qe_mregs *mr = sc->sc_mr;
	struct qe_cregs *cr = sc->sc_cr;
	struct qec_softc *qec = sc->sc_qec;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s = splimp();
	int i;

	/*
	 * Allocate descriptor ring and buffers, if not already done
	 */
	if (sc->sc_desc == NULL)
		sc->sc_desc_dva = (struct qe_desc *) dvma_malloc(
			sizeof(struct qe_desc), &sc->sc_desc, M_NOWAIT);
	if (sc->sc_bufs == NULL)
		sc->sc_bufs_dva = (struct qe_bufs *) dvma_malloc(
			sizeof(struct qe_bufs), &sc->sc_bufs, M_NOWAIT);
	
	for (i = 0; i < QE_TX_RING_MAXSIZE; i++) {
		sc->sc_desc->qe_txd[i].tx_addr =
			(u_int32_t) &sc->sc_bufs_dva->tx_buf[i % QE_TX_RING_SIZE][0];
		sc->sc_desc->qe_txd[i].tx_flags = 0;
	}
	for (i = 0; i < QE_RX_RING_MAXSIZE; i++) {
		sc->sc_desc->qe_rxd[i].rx_addr =
			(u_int32_t) &sc->sc_bufs_dva->rx_buf[i % QE_RX_RING_SIZE][0];
		if ((i / QE_RX_RING_SIZE) == 0)
			sc->sc_desc->qe_rxd[i].rx_flags =
				QE_RXD_OWN | QE_RXD_LENGTH;
		else
			sc->sc_desc->qe_rxd[i].rx_flags = 0;
	}

	cr->rxds = (u_int32_t) &sc->sc_desc_dva->qe_rxd[0];
	cr->txds = (u_int32_t) &sc->sc_desc_dva->qe_txd[0];

	sc->sc_first_td = sc->sc_last_td = sc->sc_no_td = 0;
	sc->sc_last_rd = 0;

	qestop(sc);

	cr->rimask = 0;
	cr->timask = 0;
	cr->qmask = 0;
	cr->mmask = QE_CR_MMASK_RXCOLL;
	cr->rxwbufptr = cr->rxrbufptr = sc->sc_channel * qec->sc_msize;
	cr->txwbufptr = cr->txrbufptr = cr->rxrbufptr + qec->sc_rsize;
	cr->ccnt = 0;
	cr->pipg = 0;

	mr->phycc = QE_MR_PHYCC_ASEL;
	mr->xmtfc = QE_MR_XMTFC_APADXMT;
	mr->rcvfc = 0;
	mr->imr = QE_MR_IMR_CERRM | QE_MR_IMR_RCVINTM;
	mr->biucc = QE_MR_BIUCC_BSWAP | QE_MR_BIUCC_64TS;
	mr->fifofc = QE_MR_FIFOCC_TXF16 | QE_MR_FIFOCC_RXF32 |
	    QE_MR_FIFOCC_RFWU | QE_MR_FIFOCC_TFWU;
	mr->plscc = QE_MR_PLSCC_TP;

	mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_PHYADDR;
	mr->padr = sc->sc_arpcom.ac_enaddr[0];
	mr->padr = sc->sc_arpcom.ac_enaddr[1];
	mr->padr = sc->sc_arpcom.ac_enaddr[2];
	mr->padr = sc->sc_arpcom.ac_enaddr[3];
	mr->padr = sc->sc_arpcom.ac_enaddr[4];
	mr->padr = sc->sc_arpcom.ac_enaddr[5];

	mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_LOGADDR;
	for (i = 0; i < 8; i++)
		mr->ladrf = 0;
	mr->iac = 0;

	delay(50000);
	if ((mr->phycc & QE_MR_PHYCC_LNKFL) == QE_MR_PHYCC_LNKFL)
		printf("%s: no carrier\n", sc->sc_dev.dv_xname);

	i = mr->mpc;	/* cleared on read */

	mr->maccc = QE_MR_MACCC_ENXMT | QE_MR_MACCC_ENRCV |
		((ifp->if_flags & IFF_PROMISC) ? QE_MR_MACCC_PROM : 0);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);
}

/*
 * Routine to copy from mbuf chain to transmit buffer in
 * network buffer memory.
 */
int
qe_put(sc, idx, m)
	struct qesoftc *sc;
	int idx;
	struct mbuf *m;
{
	struct mbuf *n;
	int len, tlen = 0, boff = 0;

	for (; m; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		}
		bcopy(mtod(m, caddr_t),
		      &sc->sc_bufs->tx_buf[idx % QE_TX_RING_SIZE][boff], len);
		boff += len;
		tlen += len;
		MFREE(m, n);
	}
	return tlen;
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
	struct ether_header *eh;
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
	m = qe_get(sc, idx, len);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}
	ifp->if_ipackets++;

	/* We assume that the header fit entirely in one mbuf. */
	eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif
	/* Pass the packet up, with the ether header sort-of removed. */
	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);
}

/*
 * Pull data off an interface.
 * Len is the length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present,
 * we copy into clusters.
 */
struct mbuf *
qe_get(sc, idx, totlen)
	struct qesoftc *sc;
	int idx, totlen;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	struct mbuf *top, **mp;
	int len, pad, boff = 0;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	m->m_data += pad;
	len = MHLEN - pad;
	top = NULL;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return NULL;
			}
			len = MLEN;
		}
		if (top && totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		bcopy(&sc->sc_bufs->rx_buf[idx % QE_RX_RING_SIZE][boff],
		      mtod(m, caddr_t), len);
		boff += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
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
	u_int8_t octet, maccc = 0, *ladrp = (u_int8_t *)&hash[0];
	int i, j;

	if (ifp->if_flags & IFF_ALLMULTI) {
		mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_LOGADDR;
		for (i = 0; i < 8; i++)
			mr->ladrf = 0xff;
		mr->iac = 0;
	}
	else if (ifp->if_flags & IFF_PROMISC) {
		maccc |= QE_MR_MACCC_PROM;
	}
	else {

		hash[3] = hash[2] = hash[1] = hash[0] = 0;

		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
			    ETHER_ADDR_LEN)) {
				/*
				 * We must listen to a range of multicast
				 * addresses. For now, just accept all
				 * multicasts, rather than trying to set only
				 * those filter bits needed to match the range.
				 * (At this time, the only use of address
				 * ranges is for IP multicast routing, for
				 * which the range is big enough to require
				 * all bits set.)
				 */
				mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_LOGADDR;
				for (i = 0; i < 8; i++)
					mr->ladrf = 0xff;
				mr->iac = 0;
				ifp->if_flags |= IFF_ALLMULTI;
				break;
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

		mr->iac = QE_MR_IAC_ADDRCHG | QE_MR_IAC_LOGADDR;
		for (i = 0; i < 8; i++)
			mr->ladrf = ladrp[i];
		mr->iac = 0;
	}

	mr->maccc = maccc | QE_MR_MACCC_ENXMT | QE_MR_MACCC_ENRCV;
}
