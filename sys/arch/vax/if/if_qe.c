/*	$OpenBSD: if_qe.c,v 1.21 2007/09/17 01:33:33 krw Exp $	*/
/*      $NetBSD: if_qe.c,v 1.51 2002/06/08 12:28:37 ragge Exp $ */
/*
 * Copyright (c) 1999 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * Driver for DEQNA/DELQA ethernet cards.
 * Things that is still to do:
 *	Handle ubaresets. Does not work at all right now.
 *	Fix ALLMULTI reception. But someone must tell me how...
 *	Collect statistics.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>

#include <arch/vax/qbus/ubavar.h>
#include <arch/vax/if/if_qereg.h>

#define RXDESCS	30	/* # of receive descriptors */
#define TXDESCS	60	/* # transmit descs */

/*
 * Structure containing the elements that must be in DMA-safe memory.
 */
struct qe_cdata {
	struct qe_ring	qc_recv[RXDESCS+1];	/* Receive descriptors */
	struct qe_ring	qc_xmit[TXDESCS+1];	/* Transmit descriptors */
	u_int8_t	qc_setup[128];		/* Setup packet layout */
};

struct	qe_softc {
	struct device	sc_dev;		/* Configuration common part	*/
	struct evcount	sc_intrcnt;	/* Interrupt counting		*/
	int		sc_cvec;
	struct arpcom	sc_ac;		/* Ethernet common part		*/
#define sc_if	sc_ac.ac_if		/* network-visible interface	*/
	bus_space_tag_t sc_iot;
	bus_addr_t	sc_ioh;
	bus_dma_tag_t	sc_dmat;
	struct qe_cdata *sc_qedata;	/* Descriptor struct		*/
	struct qe_cdata *sc_pqedata;	/* Unibus address of above	*/
	struct mbuf*	sc_txmbuf[TXDESCS];
	struct mbuf*	sc_rxmbuf[RXDESCS];
	bus_dmamap_t	sc_xmtmap[TXDESCS];
	bus_dmamap_t	sc_rcvmap[RXDESCS];
	struct ubinfo	sc_ui;
	int		sc_intvec;	/* Interrupt vector		*/
	int		sc_nexttx;
	int		sc_inq;
	int		sc_lastack;
	int		sc_nextrx;
	int		sc_setup;	/* Setup packet in queue	*/
};

static	int	qematch(struct device *, struct cfdata *, void *);
static	void	qeattach(struct device *, struct device *, void *);
static	void	qeinit(struct qe_softc *);
static	void	qestart(struct ifnet *);
static	void	qeintr(void *);
static	int	qeioctl(struct ifnet *, u_long, caddr_t);
static	int	qe_add_rxbuf(struct qe_softc *, int);
static	void	qe_setup(struct qe_softc *);
static	void	qetimeout(struct ifnet *);

struct	cfattach qe_ca = {
	sizeof(struct qe_softc), (cfmatch_t)qematch, qeattach
};

struct cfdriver qe_cd = {
	NULL, "qe", DV_IFNET
};

#define	QE_WCSR(csr, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, csr, val)
#define	QE_RCSR(csr) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, csr)

#define	LOWORD(x)	((int)(x) & 0xffff)
#define	HIWORD(x)	(((int)(x) >> 16) & 0x3f)

/*
 * Check for present DEQNA. Done by sending a fake setup packet
 * and wait for interrupt.
 */
int
qematch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct	qe_softc ssc;
	struct	qe_softc *sc = &ssc;
	struct	uba_attach_args *ua = aux;
	struct	uba_softc *ubasc = (struct uba_softc *)parent;
	struct ubinfo ui;

#define	PROBESIZE	4096
	struct qe_ring *ring;
	struct	qe_ring *rp;
	int error;

	ring = malloc(PROBESIZE, M_TEMP, M_WAITOK | M_ZERO);
	bzero(sc, sizeof(struct qe_softc));
	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;

	ubasc->uh_lastiv -= 4;
	QE_WCSR(QE_CSR_CSR, QE_RESET);
	QE_WCSR(QE_CSR_VECTOR, ubasc->uh_lastiv);

	/*
	 * Map the ring area. Actually this is done only to be able to 
	 * send and receive a internal packet; some junk is loopbacked
	 * so that the DEQNA has a reason to interrupt.
	 */
	ui.ui_size = PROBESIZE;
	ui.ui_vaddr = (caddr_t)&ring[0];
	if ((error = uballoc((void *)parent, &ui, UBA_CANTWAIT)))
		return 0;

	/*
	 * Init a simple "fake" receive and transmit descriptor that
	 * points to some unused area. Send a fake setup packet.
	 */
	rp = (void *)ui.ui_baddr;
	ring[0].qe_flag = ring[0].qe_status1 = QE_NOTYET;
	ring[0].qe_addr_lo = LOWORD(&rp[4]);
	ring[0].qe_addr_hi = HIWORD(&rp[4]) | QE_VALID | QE_EOMSG | QE_SETUP;
	ring[0].qe_buf_len = -64;

	ring[2].qe_flag = ring[2].qe_status1 = QE_NOTYET;
	ring[2].qe_addr_lo = LOWORD(&rp[4]);
	ring[2].qe_addr_hi = HIWORD(&rp[4]) | QE_VALID;
	ring[2].qe_buf_len = -(1500/2);

	QE_WCSR(QE_CSR_CSR, QE_RCSR(QE_CSR_CSR) & ~QE_RESET);
	DELAY(1000);

	/*
	 * Start the interface and wait for the packet.
	 */
	QE_WCSR(QE_CSR_CSR, QE_INT_ENABLE|QE_XMIT_INT|QE_RCV_INT);
	QE_WCSR(QE_CSR_RCLL, LOWORD(&rp[2]));
	QE_WCSR(QE_CSR_RCLH, HIWORD(&rp[2]));
	QE_WCSR(QE_CSR_XMTL, LOWORD(rp));
	QE_WCSR(QE_CSR_XMTH, HIWORD(rp));
	DELAY(10000);

	/*
	 * All done with the bus resources.
	 */
	ubfree((void *)parent, &ui);
	free(ring, M_TEMP);
	return 1;
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
qeattach(struct device *parent, struct device *self, void *aux)
{
	struct	uba_attach_args *ua = aux;
	struct	uba_softc *ubasc = (struct uba_softc *)parent;
	struct	qe_softc *sc = (struct qe_softc *)self;
	struct	ifnet *ifp = (struct ifnet *)&sc->sc_if;
	struct	qe_ring *rp;
	int i, error;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;

        /*
         * Allocate DMA safe memory for descriptors and setup memory.
         */

	sc->sc_ui.ui_size = sizeof(struct qe_cdata);
	if ((error = ubmemalloc((struct uba_softc *)parent, &sc->sc_ui, 0))) {
		printf(": unable to ubmemalloc(), error = %d\n", error);
		return;
	}
	sc->sc_pqedata = (struct qe_cdata *)sc->sc_ui.ui_baddr;
	sc->sc_qedata = (struct qe_cdata *)sc->sc_ui.ui_vaddr;

	/*
	 * Zero the newly allocated memory.
	 */
	bzero(sc->sc_qedata, sizeof(struct qe_cdata));
	/*
	 * Create the transmit descriptor DMA maps. We take advantage
	 * of the fact that the Qbus address space is big, and therefore 
	 * allocate map registers for all transmit descriptors also,
	 * so that we can avoid this each time we send a packet.
	 */
	for (i = 0; i < TXDESCS; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    1, MCLBYTES, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
		    &sc->sc_xmtmap[i]))) {
			printf(": unable to create tx DMA map %d, error = %d\n",
			    i, error);
			goto fail_4;
		}
	}

	/*
	 * Create receive buffer DMA maps.
	 */
	for (i = 0; i < RXDESCS; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &sc->sc_rcvmap[i]))) {
			printf(": unable to create rx DMA map %d, error = %d\n",
			    i, error);
			goto fail_5;
		}
	}
	/*
	 * Pre-allocate the receive buffers.
	 */
	for (i = 0; i < RXDESCS; i++) {
		if ((error = qe_add_rxbuf(sc, i)) != 0) {
			printf(": unable to allocate or map rx buffer %d\n,"
			    " error = %d\n", i, error);
			goto fail_6;
		}
	}

	/*
	 * Create ring loops of the buffer chains.
	 * This is only done once.
	 */

	rp = sc->sc_qedata->qc_recv;
	rp[RXDESCS].qe_addr_lo = LOWORD(&sc->sc_pqedata->qc_recv[0]);
	rp[RXDESCS].qe_addr_hi = HIWORD(&sc->sc_pqedata->qc_recv[0]) |
	    QE_VALID | QE_CHAIN;
	rp[RXDESCS].qe_flag = rp[RXDESCS].qe_status1 = QE_NOTYET;

	rp = sc->sc_qedata->qc_xmit;
	rp[TXDESCS].qe_addr_lo = LOWORD(&sc->sc_pqedata->qc_xmit[0]);
	rp[TXDESCS].qe_addr_hi = HIWORD(&sc->sc_pqedata->qc_xmit[0]) |
	    QE_VALID | QE_CHAIN;
	rp[TXDESCS].qe_flag = rp[TXDESCS].qe_status1 = QE_NOTYET;

	/*
	 * Get the vector that were set at match time, and remember it.
	 */
	sc->sc_intvec = ubasc->uh_lastiv;
	QE_WCSR(QE_CSR_CSR, QE_RESET);
	DELAY(1000);
	QE_WCSR(QE_CSR_CSR, QE_RCSR(QE_CSR_CSR) & ~QE_RESET);

	/*
	 * Read out ethernet address and tell which type this card is.
	 */
	for (i = 0; i < 6; i++)
		sc->sc_ac.ac_enaddr[i] = QE_RCSR(i * 2) & 0xff;

	QE_WCSR(QE_CSR_VECTOR, sc->sc_intvec | 1);
	printf(": %s, address %s\n",
		QE_RCSR(QE_CSR_VECTOR) & 1 ? "delqa" : "deqna",
		ether_sprintf(sc->sc_ac.ac_enaddr));

	QE_WCSR(QE_CSR_VECTOR, QE_RCSR(QE_CSR_VECTOR) & ~1); /* ??? */

	uba_intr_establish(ua->ua_icookie, ua->ua_cvec, qeintr,
		sc, &sc->sc_intrcnt);
	sc->sc_cvec = ua->ua_cvec;
	evcount_attach(&sc->sc_intrcnt, sc->sc_dev.dv_xname,
	    (void *)&sc->sc_cvec, &evcount_intr);

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof ifp->if_xname);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = qestart;
	ifp->if_ioctl = qeioctl;
	ifp->if_watchdog = qetimeout;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_6:
	for (i = 0; i < RXDESCS; i++) {
		if (sc->sc_rxmbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_xmtmap[i]);
			m_freem(sc->sc_rxmbuf[i]);
		}
	}
 fail_5:
	for (i = 0; i < RXDESCS; i++) {
		if (sc->sc_xmtmap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_xmtmap[i]);
	}
 fail_4:
	for (i = 0; i < TXDESCS; i++) {
		if (sc->sc_rcvmap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_rcvmap[i]);
	}
}

/*
 * Initialization of interface.
 */
void
qeinit(struct qe_softc *sc)
{
	struct ifnet *ifp = (struct ifnet *)&sc->sc_if;
	struct qe_cdata *qc = sc->sc_qedata;
	int i;


	/*
	 * Reset the interface.
	 */
	QE_WCSR(QE_CSR_CSR, QE_RESET);
	DELAY(1000);
	QE_WCSR(QE_CSR_CSR, QE_RCSR(QE_CSR_CSR) & ~QE_RESET);
	QE_WCSR(QE_CSR_VECTOR, sc->sc_intvec);

	sc->sc_nexttx = sc->sc_inq = sc->sc_lastack = 0;
	/*
	 * Release and init transmit descriptors.
	 */
	for (i = 0; i < TXDESCS; i++) {
		if (sc->sc_txmbuf[i]) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_xmtmap[i]);
			m_freem(sc->sc_txmbuf[i]);
			sc->sc_txmbuf[i] = 0;
		}
		qc->qc_xmit[i].qe_addr_hi = 0; /* Clear valid bit */
		qc->qc_xmit[i].qe_status1 = qc->qc_xmit[i].qe_flag = QE_NOTYET;
	}


	/*
	 * Init receive descriptors.
	 */
	for (i = 0; i < RXDESCS; i++)
		qc->qc_recv[i].qe_status1 = qc->qc_recv[i].qe_flag = QE_NOTYET;
	sc->sc_nextrx = 0;

	/*
	 * Write the descriptor addresses to the device.
	 * Receiving packets will be enabled in the interrupt routine.
	 */
	QE_WCSR(QE_CSR_CSR, QE_INT_ENABLE|QE_XMIT_INT|QE_RCV_INT);
	QE_WCSR(QE_CSR_RCLL, LOWORD(sc->sc_pqedata->qc_recv));
	QE_WCSR(QE_CSR_RCLH, HIWORD(sc->sc_pqedata->qc_recv));

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Send a setup frame.
	 * This will start the transmit machinery as well.
	 */
	qe_setup(sc);

}

/*
 * Start output on interface.
 */
void
qestart(struct ifnet *ifp)
{
	struct qe_softc *sc = ifp->if_softc;
	struct qe_cdata *qc = sc->sc_qedata;
	paddr_t	buffer;
	struct mbuf *m, *m0;
	int idx, len, s, i, totlen, error;
	short orword, csr;

	if ((QE_RCSR(QE_CSR_CSR) & QE_RCV_ENABLE) == 0)
		return;

	s = splnet();
	while (sc->sc_inq < (TXDESCS - 1)) {

		if (sc->sc_setup) {
			qe_setup(sc);
			continue;
		}
		idx = sc->sc_nexttx;
		IFQ_POLL(&ifp->if_snd, m);
		if (m == 0)
			goto out;
		/*
		 * Count number of mbufs in chain.
		 * Always do DMA directly from mbufs, therefore the transmit
		 * ring is really big.
		 */
		for (m0 = m, i = 0; m0; m0 = m0->m_next)
			if (m0->m_len)
				i++;
		if (i >= TXDESCS)
			panic("qestart");

		if ((i + sc->sc_inq) >= (TXDESCS - 1)) {
			ifp->if_flags |= IFF_OACTIVE;
			goto out;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		/*
		 * m now points to a mbuf chain that can be loaded.
		 * Loop around and set it.
		 */
		totlen = 0;
		for (m0 = m; m0; m0 = m0->m_next) {
			error = bus_dmamap_load(sc->sc_dmat, sc->sc_xmtmap[idx],
			    mtod(m0, void *), m0->m_len, 0, 0);
			buffer = sc->sc_xmtmap[idx]->dm_segs[0].ds_addr;
			len = m0->m_len;
			if (len == 0)
				continue;

			totlen += len;
			/* Word alignment calc */
			orword = 0;
			if (totlen == m->m_pkthdr.len) {
				if (totlen < ETHER_MIN_LEN)
					len += (ETHER_MIN_LEN - totlen);
				orword |= QE_EOMSG;
				sc->sc_txmbuf[idx] = m;
			}
			if ((buffer & 1) || (len & 1))
				len += 2;
			if (buffer & 1)
				orword |= QE_ODDBEGIN;
			if ((buffer + len) & 1)
				orword |= QE_ODDEND;
			qc->qc_xmit[idx].qe_buf_len = -(len/2);
			qc->qc_xmit[idx].qe_addr_lo = LOWORD(buffer);
			qc->qc_xmit[idx].qe_addr_hi = HIWORD(buffer);
			qc->qc_xmit[idx].qe_flag =
			    qc->qc_xmit[idx].qe_status1 = QE_NOTYET;
			qc->qc_xmit[idx].qe_addr_hi |= (QE_VALID | orword);
			if (++idx == TXDESCS)
				idx = 0;
			sc->sc_inq++;
		}
#ifdef DIAGNOSTIC
		if (totlen != m->m_pkthdr.len)
			panic("qestart: len fault");
#endif

		/*
		 * Kick off the transmit logic, if it is stopped.
		 */
		csr = QE_RCSR(QE_CSR_CSR);
		if (csr & QE_XL_INVALID) {
			QE_WCSR(QE_CSR_XMTL,
			    LOWORD(&sc->sc_pqedata->qc_xmit[sc->sc_nexttx]));
			QE_WCSR(QE_CSR_XMTH,
			    HIWORD(&sc->sc_pqedata->qc_xmit[sc->sc_nexttx]));
		}
		sc->sc_nexttx = idx;
	}
	if (sc->sc_inq == (TXDESCS - 1))
		ifp->if_flags |= IFF_OACTIVE;

out:	if (sc->sc_inq)
		ifp->if_timer = 5; /* If transmit logic dies */
	splx(s);
}

static void
qeintr(void *arg)
{
	struct qe_softc *sc = arg;
	struct qe_cdata *qc = sc->sc_qedata;
	struct ifnet *ifp = &sc->sc_if;
	struct ether_header *eh;
	struct mbuf *m;
	int csr, status1, status2, len;

	csr = QE_RCSR(QE_CSR_CSR);

	QE_WCSR(QE_CSR_CSR, QE_RCV_ENABLE | QE_INT_ENABLE | QE_XMIT_INT |
	    QE_RCV_INT | QE_ILOOP);

	if (csr & QE_RCV_INT)
		while (qc->qc_recv[sc->sc_nextrx].qe_status1 != QE_NOTYET) {
			status1 = qc->qc_recv[sc->sc_nextrx].qe_status1;
			status2 = qc->qc_recv[sc->sc_nextrx].qe_status2;

			m = sc->sc_rxmbuf[sc->sc_nextrx];
			len = ((status1 & QE_RBL_HI) |
			    (status2 & QE_RBL_LO)) + 60;
			qe_add_rxbuf(sc, sc->sc_nextrx);
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = len;
			if (++sc->sc_nextrx == RXDESCS)
				sc->sc_nextrx = 0;
			eh = mtod(m, struct ether_header *);
#if NBPFILTER > 0
			if (ifp->if_bpf) {
				bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
				if ((ifp->if_flags & IFF_PROMISC) != 0 &&
				    bcmp(sc->sc_ac.ac_enaddr, eh->ether_dhost,
				    ETHER_ADDR_LEN) != 0 &&
				    ((eh->ether_dhost[0] & 1) == 0)) {
					m_freem(m);
					continue;
				}
			}
#endif
			/*
			 * ALLMULTI means PROMISC in this driver.
			 */
			if ((ifp->if_flags & IFF_ALLMULTI) &&
			    ((eh->ether_dhost[0] & 1) == 0) &&
			    bcmp(sc->sc_ac.ac_enaddr, eh->ether_dhost,
			    ETHER_ADDR_LEN)) {
				m_freem(m);
				continue;
			}

			if ((status1 & QE_ESETUP) == 0)
				ether_input_mbuf(ifp, m);
			else
				m_freem(m);
		}

	if (csr & (QE_XMIT_INT|QE_XL_INVALID)) {
		while (qc->qc_xmit[sc->sc_lastack].qe_status1 != QE_NOTYET) {
			int idx = sc->sc_lastack;

			sc->sc_inq--;
			if (++sc->sc_lastack == TXDESCS)
				sc->sc_lastack = 0;

			/* XXX collect statistics */
			qc->qc_xmit[idx].qe_addr_hi &= ~QE_VALID;
			qc->qc_xmit[idx].qe_status1 =
			    qc->qc_xmit[idx].qe_flag = QE_NOTYET;

			if (qc->qc_xmit[idx].qe_addr_hi & QE_SETUP)
				continue;
			bus_dmamap_unload(sc->sc_dmat, sc->sc_xmtmap[idx]);
			if (sc->sc_txmbuf[idx]) {
				m_freem(sc->sc_txmbuf[idx]);
				sc->sc_txmbuf[idx] = 0;
			}
		}
		ifp->if_timer = 0;
		ifp->if_flags &= ~IFF_OACTIVE;
		qestart(ifp); /* Put in more in queue */
	}
	/*
	 * How can the receive list get invalid???
	 * Verified that it happens anyway.
	 */
	if ((qc->qc_recv[sc->sc_nextrx].qe_status1 == QE_NOTYET) &&
	    (QE_RCSR(QE_CSR_CSR) & QE_RL_INVALID)) {
		QE_WCSR(QE_CSR_RCLL,
		    LOWORD(&sc->sc_pqedata->qc_recv[sc->sc_nextrx]));
		QE_WCSR(QE_CSR_RCLH,
		    HIWORD(&sc->sc_pqedata->qc_recv[sc->sc_nextrx]));
	}
}

/*
 * Process an ioctl request.
 */
int
qeioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct qe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s = splnet(), error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			qeinit(sc);
			arp_ifinit(&sc->sc_ac, ifa);
			break;
#endif
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * stop it. (by disabling receive mechanism).
			 */
			QE_WCSR(QE_CSR_CSR,
			    QE_RCSR(QE_CSR_CSR) & ~QE_RCV_ENABLE);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface it marked up and it is stopped, then
			 * start it.
			 */
			qeinit(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0) {
			/*
			 * Send a new setup packet to match any new changes.
			 * (Like IFF_PROMISC etc)
			 */
			qe_setup(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * Update our multicast list.
		 */
		error = (cmd == SIOCADDMULTI) ?
			ether_addmulti(ifr, &sc->sc_ac):
			ether_delmulti(ifr, &sc->sc_ac);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			qe_setup(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;

	}
	splx(s);
	return (error);
}

/*
 * Add a receive buffer to the indicated descriptor.
 */
int
qe_add_rxbuf(struct qe_softc *sc, int i) 
{
	struct mbuf *m;
	struct qe_ring *rp;
	vaddr_t addr;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (sc->sc_rxmbuf[i] != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->sc_rcvmap[i]);

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_rcvmap[i],
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL, BUS_DMA_NOWAIT);
	if (error)
		panic("%s: can't load rx DMA map %d, error = %d",
		    sc->sc_dev.dv_xname, i, error);
	sc->sc_rxmbuf[i] = m;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_rcvmap[i], 0,
	    sc->sc_rcvmap[i]->dm_mapsize, BUS_DMASYNC_PREREAD);

	/*
	 * We know that the mbuf cluster is page aligned. Also, be sure
	 * that the IP header will be longword aligned.
	 */
	m->m_data += 2;
	addr = sc->sc_rcvmap[i]->dm_segs[0].ds_addr + 2;
	rp = &sc->sc_qedata->qc_recv[i];
	rp->qe_flag = rp->qe_status1 = QE_NOTYET;
	rp->qe_addr_lo = LOWORD(addr);
	rp->qe_addr_hi = HIWORD(addr) | QE_VALID;
	rp->qe_buf_len = -(m->m_ext.ext_size - 2)/2;

	return (0);
}

/*
 * Create a setup packet and put in queue for sending.
 */
void
qe_setup(struct qe_softc *sc)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	struct qe_cdata *qc = sc->sc_qedata;
	struct ifnet *ifp = &sc->sc_if;
	u_int8_t *enaddr = sc->sc_ac.ac_enaddr;
	int i, j, k, idx, s;

	s = splnet();
	if (sc->sc_inq == (TXDESCS - 1)) {
		sc->sc_setup = 1;
		splx(s);
		return;
	}
	sc->sc_setup = 0;
	/*
	 * Init the setup packet with valid info.
	 */
	memset(qc->qc_setup, 0xff, sizeof(qc->qc_setup)); /* Broadcast */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		qc->qc_setup[i * 8 + 1] = enaddr[i]; /* Own address */

	/*
	 * Multicast handling. The DEQNA can handle up to 12 direct 
	 * ethernet addresses.
	 */
	j = 3; k = 0;
	ifp->if_flags &= ~IFF_ALLMULTI;
	ETHER_FIRST_MULTI(step, &sc->sc_ac, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6)) {
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			qc->qc_setup[i * 8 + j + k] = enm->enm_addrlo[i];
		j++;
		if (j == 8) {
			j = 1; k += 64;
		}
		if (k > 64) {
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}
		ETHER_NEXT_MULTI(step, enm);
	}
	idx = sc->sc_nexttx;
	qc->qc_xmit[idx].qe_buf_len = -64;

	/*
	 * How is the DEQNA turned in ALLMULTI mode???
	 * Until someone tells me, fall back to PROMISC when more than
	 * 12 ethernet addresses.
	 */
	if (ifp->if_flags & IFF_ALLMULTI)
		ifp->if_flags |= IFF_PROMISC;
	else if (ifp->if_pcount == 0)
		ifp->if_flags &= ~IFF_PROMISC;
	if (ifp->if_flags & IFF_PROMISC)
		qc->qc_xmit[idx].qe_buf_len = -65;

	qc->qc_xmit[idx].qe_addr_lo = LOWORD(sc->sc_pqedata->qc_setup);
	qc->qc_xmit[idx].qe_addr_hi =
	    HIWORD(sc->sc_pqedata->qc_setup) | QE_SETUP | QE_EOMSG;
	qc->qc_xmit[idx].qe_status1 = qc->qc_xmit[idx].qe_flag = QE_NOTYET;
	qc->qc_xmit[idx].qe_addr_hi |= QE_VALID;

	if (QE_RCSR(QE_CSR_CSR) & QE_XL_INVALID) {
		QE_WCSR(QE_CSR_XMTL,
		    LOWORD(&sc->sc_pqedata->qc_xmit[idx]));
		QE_WCSR(QE_CSR_XMTH,
		    HIWORD(&sc->sc_pqedata->qc_xmit[idx]));
	}

	sc->sc_inq++;
	if (++sc->sc_nexttx == TXDESCS)
		sc->sc_nexttx = 0;
	splx(s);
}

/*
 * Check for dead transmit logic. Not uncommon.
 */
void
qetimeout(struct ifnet *ifp)
{
	struct qe_softc *sc = ifp->if_softc;

	if (sc->sc_inq == 0)
		return;

	printf("%s: xmit logic died, resetting...\n", sc->sc_dev.dv_xname);
	/*
	 * Do a reset of interface, to get it going again.
	 * Will it work by just restart the transmit logic?
	 */
	qeinit(sc);
}
