/*	$OpenBSD: if_xnf.c,v 1.8 2016/01/19 17:16:19 mikeb Exp $	*/

/*
 * Copyright (c) 2015, 2016 Mike Belopuhov
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bpfilter.h"
#include "vlan.h"
#include "xen.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/queue.h>
#include <sys/timeout.h>
#include <sys/pool.h>

#include <machine/bus.h>

#include <dev/pv/xenreg.h>
#include <dev/pv/xenvar.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif


/*
 * Rx ring
 */

struct xnf_rx_req {
	uint16_t		 rxq_id;
	uint16_t		 rxq_pad;
	uint32_t		 rxq_ref;
} __packed;

struct xnf_rx_rsp {
	uint16_t		 rxp_id;
	uint16_t		 rxp_offset;
	uint16_t		 rxp_flags;
#define  XNF_RXF_CSUM		  0x0001
#define  XNF_RXF_BLANK		  0x0002
#define  XNF_RXF_CHUNK		  0x0004
#define  XNF_RXF_MGMT		  0x0008
	int16_t			 rxp_status;
} __packed;

union xnf_rx_desc {
	struct xnf_rx_req	 rxd_req;
	struct xnf_rx_rsp	 rxd_rsp;
} __packed;

#define XNF_RX_DESC		256
#define XNF_MCLEN		PAGE_SIZE
#define XNF_RX_MIN		32

struct xnf_rx_ring {
	uint32_t		 rxr_prod;
	uint32_t		 rxr_req_evt;
	uint32_t		 rxr_cons;
	uint32_t		 rxr_rsp_evt;
	uint32_t		 rxr_reserved[12];
	union xnf_rx_desc	 rxr_desc[XNF_RX_DESC];
} __packed;


/*
 * Tx ring
 */

struct xnf_tx_req {
	uint32_t		 txq_ref;
	uint16_t		 txq_offset;
	uint16_t		 txq_flags;
#define  XNF_TXF_CSUM		  0x0001
#define  XNF_TXF_VALID		  0x0002
#define  XNF_TXF_CHUNK		  0x0004
#define  XNF_TXF_ETXRA		  0x0008
	uint16_t		 txq_id;
	uint16_t		 txq_size;
} __packed;

struct xnf_tx_rsp {
	uint16_t		 txp_id;
	int16_t			 txp_status;
} __packed;

union xnf_tx_desc {
	struct xnf_tx_req	 txd_req;
	struct xnf_tx_rsp	 txd_rsp;
} __packed;

#define XNF_TX_DESC		256
#define XNF_TX_FRAG		18

struct xnf_tx_ring {
	uint32_t		 txr_prod;
	uint32_t		 txr_req_evt;
	uint32_t		 txr_cons;
	uint32_t		 txr_rsp_evt;
	uint32_t		 txr_reserved[12];
	union xnf_tx_desc	 txr_desc[XNF_TX_DESC];
} __packed;


/* Management frame, "extra info" in Xen parlance */
struct xnf_mgmt {
	uint8_t			 mg_type;
#define  XNF_MGMT_MCAST_ADD	2
#define  XNF_MGMT_MCAST_DEL	3
	uint8_t			 mg_flags;
	union {
		uint8_t		 mgu_mcaddr[ETHER_ADDR_LEN];
		uint16_t	 mgu_pad[3];
	} u;
#define mg_mcaddr		 u.mgu_mcaddr
} __packed;


struct xnf_softc {
	struct device		 sc_dev;
	struct xen_attach_args	 sc_xa;
	struct xen_softc	*sc_xen;
	bus_dma_tag_t		 sc_dmat;

	struct arpcom		 sc_ac;
	struct ifmedia		 sc_media;

	xen_intr_handle_t	 sc_xih;

	/* Rx ring */
	struct xnf_rx_ring	*sc_rx_ring;
	int			 sc_rx_cons;
	bus_dmamap_t		 sc_rx_rmap;		  /* map for the ring */
	bus_dma_segment_t	 sc_rx_seg;
	uint32_t		 sc_rx_ref;		  /* grant table ref */
	struct mbuf		*sc_rx_buf[XNF_RX_DESC];
	bus_dmamap_t		 sc_rx_dmap[XNF_RX_DESC]; /* maps for packets */
	struct mbuf		*sc_rx_cbuf[2];	  	  /* chain handling */
	struct if_rxring	 sc_rx_slots;
	struct timeout		 sc_rx_fill;

	/* Tx ring */
	struct xnf_tx_ring	*sc_tx_ring;
	int			 sc_tx_cons;
	bus_dmamap_t		 sc_tx_rmap;		  /* map for the ring */
	bus_dma_segment_t	 sc_tx_seg;
	uint32_t		 sc_tx_ref;		  /* grant table ref */
	struct mbuf		*sc_tx_buf[XNF_TX_DESC];
	bus_dmamap_t		 sc_tx_dmap[XNF_TX_DESC]; /* maps for packets */
};

int	xnf_match(struct device *, void *, void *);
void	xnf_attach(struct device *, struct device *, void *);
int	xnf_lladdr(struct xnf_softc *);
int	xnf_ioctl(struct ifnet *, u_long, caddr_t);
int	xnf_media_change(struct ifnet *);
void	xnf_media_status(struct ifnet *, struct ifmediareq *);
int	xnf_iff(struct xnf_softc *);
void	xnf_init(struct xnf_softc *);
void	xnf_stop(struct xnf_softc *);
void	xnf_start(struct ifnet *);
int	xnf_encap(struct xnf_softc *, struct mbuf *, uint32_t *);
void	xnf_intr(void *);
void	xnf_watchdog(struct ifnet *);
int	xnf_txeof(struct xnf_softc *);
int	xnf_rxeof(struct xnf_softc *);
void	xnf_rx_ring_fill(void *);
int	xnf_rx_ring_create(struct xnf_softc *);
void	xnf_rx_ring_drain(struct xnf_softc *);
void	xnf_rx_ring_destroy(struct xnf_softc *);
int	xnf_tx_ring_create(struct xnf_softc *);
void	xnf_tx_ring_drain(struct xnf_softc *);
void	xnf_tx_ring_destroy(struct xnf_softc *);
int	xnf_init_backend(struct xnf_softc *);
int	xnf_stop_backend(struct xnf_softc *);

struct cfdriver xnf_cd = {
	NULL, "xnf", DV_IFNET
};

const struct cfattach xnf_ca = {
	sizeof(struct xnf_softc), xnf_match, xnf_attach
};

int
xnf_match(struct device *parent, void *match, void *aux)
{
	struct xen_attach_args *xa = aux;

	if (strcmp("vif", xa->xa_name))
		return (0);

	return (1);
}

void
xnf_attach(struct device *parent, struct device *self, void *aux)
{
	struct xen_attach_args *xa = aux;
	struct xnf_softc *sc = (struct xnf_softc *)self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	sc->sc_xa = *xa;
	sc->sc_xen = xa->xa_parent;
	sc->sc_dmat = xa->xa_dmat;

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if (xnf_lladdr(sc)) {
		printf(": failed to obtain MAC address\n");
		return;
	}

	if (xen_intr_establish(0, &sc->sc_xih, xnf_intr, sc, ifp->if_xname)) {
		printf(": failed to establish an interrupt\n");
		return;
	}
	xen_intr_mask(sc->sc_xih);

	printf(": event channel %u, address %s\n", sc->sc_xih,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	if (xnf_rx_ring_create(sc)) {
		xen_intr_disestablish(sc->sc_xih);
		return;
	}
	if (xnf_tx_ring_create(sc)) {
		xen_intr_disestablish(sc->sc_xih);
		xnf_rx_ring_destroy(sc);
		return;
	}
	if (xnf_init_backend(sc)) {
		xen_intr_disestablish(sc->sc_xih);
		xnf_rx_ring_destroy(sc);
		xnf_tx_ring_destroy(sc);
		return;
	}

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = xnf_ioctl;
	ifp->if_start = xnf_start;
	ifp->if_watchdog = xnf_watchdog;
	ifp->if_softc = sc;

	ifp->if_capabilities = IFCAP_VLAN_MTU;

	IFQ_SET_MAXLEN(&ifp->if_snd, XNF_TX_DESC - 1);
	IFQ_SET_READY(&ifp->if_snd);

	ifmedia_init(&sc->sc_media, IFM_IMASK, xnf_media_change,
	    xnf_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_MANUAL, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_MANUAL);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->sc_rx_fill, xnf_rx_ring_fill, sc);

	/* Kick out emulated em's and re's */
	sc->sc_xen->sc_flags |= XSF_UNPLUG_NIC;
}

static int
nibble(int ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0');
	if (ch >= 'A' && ch <= 'F')
		return (10 + ch - 'A');
	if (ch >= 'a' && ch <= 'f')
		return (10 + ch - 'a');
	return (-1);
}

int
xnf_lladdr(struct xnf_softc *sc)
{
	char enaddr[ETHER_ADDR_LEN];
	char mac[32];
	int i, j, lo, hi;

	if (xs_getprop(&sc->sc_xa, "mac", mac, sizeof(mac)))
		return (-1);

	for (i = 0, j = 0; j < ETHER_ADDR_LEN; i += 3) {
		if ((hi = nibble(mac[i])) == -1 ||
		    (lo = nibble(mac[i+1])) == -1)
			return (-1);
		enaddr[j++] = hi << 4 | lo;
	}

	memcpy(sc->sc_ac.ac_enaddr, enaddr, ETHER_ADDR_LEN);
	return (0);
}

int
xnf_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct xnf_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			xnf_init(sc);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				xnf_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				xnf_stop(sc);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, command);
		break;
	case SIOCGIFRXR:
		error = if_rxr_ioctl((struct if_rxrinfo *)ifr->ifr_data,
		    NULL, XNF_MCLEN, &sc->sc_rx_slots);
		break;
	default:
		error = ether_ioctl(ifp, &sc->sc_ac, command, data);
		break;
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			xnf_iff(sc);
		error = 0;
	}

	splx(s);

	return (error);
}

int
xnf_media_change(struct ifnet *ifp)
{
	return (0);
}

void
xnf_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	ifmr->ifm_status = IFM_ACTIVE | IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER | IFM_MANUAL;
}

int
xnf_iff(struct xnf_softc *sc)
{
	return (0);
}

void
xnf_init(struct xnf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	xnf_stop(sc);

	xnf_iff(sc);

	if (xen_intr_unmask(sc->sc_xih)) {
		printf("%s: failed to enable interrupts\n", ifp->if_xname);
		xnf_stop(sc);
		return;
	}

	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);
}

void
xnf_stop(struct xnf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	ifp->if_flags &= ~IFF_RUNNING;

	xen_intr_mask(sc->sc_xih);

	timeout_del(&sc->sc_rx_fill);
	ifp->if_timer = 0;

	ifq_barrier(&ifp->if_snd);
	intr_barrier(&sc->sc_xih);

	ifq_clr_oactive(&ifp->if_snd);

	if (sc->sc_tx_ring)
		xnf_tx_ring_drain(sc);
	if (sc->sc_rx_ring)
		xnf_rx_ring_drain(sc);
}

void
xnf_start(struct ifnet *ifp)
{
	struct xnf_softc *sc = ifp->if_softc;
	struct xnf_tx_ring *txr = sc->sc_tx_ring;
	struct mbuf *m;
	int error, pkts = 0;
	uint32_t prod;

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd))
		return;

	prod = txr->txr_prod;
	membar_consumer();

	for (;;) {
		m = ifq_deq_begin(&ifp->if_snd);
		if (m == NULL)
			break;

		error = xnf_encap(sc, m, &prod);
		if (error == ENOENT) {
			/* transient */
			ifq_deq_rollback(&ifp->if_snd, m);
			ifq_set_oactive(&ifp->if_snd);
			break;
		} else if (error) {
			/* the chain is too large */
			ifq_deq_commit(&ifp->if_snd, m);
			m_freem(m);
			continue;
		}
		ifq_deq_commit(&ifp->if_snd, m);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		pkts++;
	}
	if (pkts > 0) {
		txr->txr_prod = prod;
		xen_intr_signal(sc->sc_xih);
		ifp->if_timer = 5;
	}
}

int
xnf_encap(struct xnf_softc *sc, struct mbuf *m, uint32_t *prod)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct xnf_tx_ring *txr = sc->sc_tx_ring;
	union xnf_tx_desc *txd;
	bus_dmamap_t dmap;
	int error, i, n = 0;

	if ((XNF_TX_DESC - (*prod - sc->sc_tx_cons)) < XNF_TX_FRAG) {
		error = ENOENT;
		goto errout;
	}

	i = *prod & (XNF_TX_DESC - 1);
	dmap = sc->sc_tx_dmap[i];

	error = bus_dmamap_load_mbuf(sc->sc_dmat, dmap, m, BUS_DMA_WRITE |
	    BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		if (m_defrag(m, M_DONTWAIT) ||
		    bus_dmamap_load_mbuf(sc->sc_dmat, dmap, m, BUS_DMA_WRITE |
		     BUS_DMA_NOWAIT))
			goto errout;
	} else if (error)
		goto errout;

	for (n = 0; n < dmap->dm_nsegs; n++, (*prod)++) {
		i = *prod & (XNF_TX_DESC - 1);
		if (sc->sc_tx_buf[i])
			panic("%s: cons %u(%u) prod %u next %u seg %d/%d\n",
			    ifp->if_xname, txr->txr_cons, sc->sc_tx_cons,
			    txr->txr_prod, *prod, n, dmap->dm_nsegs - 1);
		txd = &txr->txr_desc[i];
		if (n == 0) {
			if (0 && m->m_pkthdr.csum_flags & M_IPV4_CSUM_OUT)
				txd->txd_req.txq_flags = XNF_TXF_CSUM |
				    XNF_TXF_VALID;
			txd->txd_req.txq_size = m->m_pkthdr.len;
		} else
			txd->txd_req.txq_size = dmap->dm_segs[n].ds_len;
		if (n != dmap->dm_nsegs - 1)
			txd->txd_req.txq_flags |= XNF_TXF_CHUNK;
		txd->txd_req.txq_ref = dmap->dm_segs[n].ds_addr;
		txd->txd_req.txq_offset = dmap->dm_segs[n].ds_offset;
		sc->sc_tx_buf[i] = m;
		m = m->m_next;
	}

	ifp->if_opackets++;
	return (0);

 errout:
	ifp->if_oerrors++;
	return (error);
}

void
xnf_intr(void *arg)
{
	struct xnf_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (ifp->if_flags & IFF_RUNNING) {
		xnf_rxeof(sc);
		xnf_txeof(sc);
	}
}

void
xnf_watchdog(struct ifnet *ifp)
{
	struct xnf_softc *sc = ifp->if_softc;
	struct xnf_tx_ring *txr = sc->sc_tx_ring;

	printf("%s: tx prod %u cons %u,%u evt %u,%u\n",
	    ifp->if_xname, txr->txr_prod, txr->txr_cons, sc->sc_tx_cons,
	    txr->txr_req_evt, txr->txr_rsp_evt);
}

int
xnf_txeof(struct xnf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct xnf_tx_ring *txr = sc->sc_tx_ring;
	union xnf_tx_desc *txd;
	struct mbuf *m;
	bus_dmamap_t dmap;
	volatile uint32_t r;
	uint32_t cons;
	int i, id, pkts = 0;

	do {
		for (cons = sc->sc_tx_cons; cons != txr->txr_cons; cons++) {
			membar_consumer();
			i = cons & (XNF_TX_DESC - 1);
			txd = &txr->txr_desc[i];
			id = txd->txd_rsp.txp_id;
			memset(txd, 0, sizeof(*txd));
			txd->txd_req.txq_id = id;
			membar_producer();
			if (sc->sc_tx_buf[i]) {
				dmap = sc->sc_tx_dmap[i];
				bus_dmamap_unload(sc->sc_dmat, dmap);
				m = sc->sc_tx_buf[i];
				sc->sc_tx_buf[i] = NULL;
				m_free(m);
			}
			pkts++;
		}

		if (pkts > 0) {
			sc->sc_tx_cons = cons;
			membar_producer();
			txr->txr_rsp_evt = cons + 1;
			pkts = 0;
		}

		r = txr->txr_cons - sc->sc_tx_cons;
		membar_consumer();
	} while (r > 0);

	if (ifq_is_oactive(&ifp->if_snd))
		ifq_restart(&ifp->if_snd);
	else if (txr->txr_cons == txr->txr_prod)
		ifp->if_timer = 0;

	return (0);
}

int
xnf_rxeof(struct xnf_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct xnf_rx_ring *rxr = sc->sc_rx_ring;
	union xnf_rx_desc *rxd;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *fmp = sc->sc_rx_cbuf[0];
	struct mbuf *lmp = sc->sc_rx_cbuf[1];
	struct mbuf *m;
	bus_dmamap_t dmap;
	volatile uint32_t r;
	uint32_t cons;
	int i, id, flags, len, offset, pkts = 0;

	do {
		for (cons = sc->sc_rx_cons; cons != rxr->rxr_cons; cons++) {
			membar_consumer();
			i = cons & (XNF_RX_DESC - 1);
			rxd = &rxr->rxr_desc[i];
			dmap = sc->sc_rx_dmap[i];

			len = rxd->rxd_rsp.rxp_status;
			flags = rxd->rxd_rsp.rxp_flags;
			offset = rxd->rxd_rsp.rxp_offset;
			id = rxd->rxd_rsp.rxp_id;
			memset(rxd, 0, sizeof(*rxd));
			rxd->rxd_req.rxq_id = id;
			membar_producer();

			bus_dmamap_unload(sc->sc_dmat, dmap);

			m = sc->sc_rx_buf[i];
			KASSERT(m != NULL);
			sc->sc_rx_buf[i] = NULL;

			if (flags & XNF_RXF_MGMT)
				printf("%s: management data present\n",
				    ifp->if_xname);

			if (flags & XNF_RXF_CSUM)
				m->m_pkthdr.csum_flags = M_IPV4_CSUM_IN_OK;

			if_rxr_put(&sc->sc_rx_slots, 1);
			pkts++;

			if (len < 0 || (len + offset > PAGE_SIZE)) {
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			}

			m->m_len = len;
			m->m_data += offset;

			if (fmp == NULL) {
				m->m_pkthdr.len = len;
				fmp = m;
			} else {
				m->m_flags &= ~M_PKTHDR;
				lmp->m_next = m;
				fmp->m_pkthdr.len += m->m_len;
			}
			lmp = m;

			if (flags & XNF_RXF_CHUNK) {
				sc->sc_rx_cbuf[0] = fmp;
				sc->sc_rx_cbuf[1] = lmp;
				continue;
			}

			m = fmp;

			ml_enqueue(&ml, m);
			sc->sc_rx_cbuf[0] = sc->sc_rx_cbuf[1] =
			    fmp = lmp = NULL;
		}

		if (pkts > 0) {
			sc->sc_rx_cons = cons;
			membar_producer();
			rxr->rxr_rsp_evt = cons + 1;
			pkts = 0;
		}

		r = rxr->rxr_cons - sc->sc_rx_cons;
		membar_consumer();
	} while (r > 0);

	if (!ml_empty(&ml)) {
		if_input(ifp, &ml);

		xnf_rx_ring_fill(sc);
	}

	return (0);
}

void
xnf_rx_ring_fill(void *arg)
{
	struct xnf_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct xnf_rx_ring *rxr = sc->sc_rx_ring;
	bus_dmamap_t dmap;
	struct mbuf *m;
	uint32_t cons, prod;
	static int timer = 0;
	int i, n;

	cons = rxr->rxr_cons;
	prod = rxr->rxr_prod;

	n = if_rxr_get(&sc->sc_rx_slots, XNF_RX_DESC);

	/* Less than XNF_RX_MIN slots available? */
	if (n == 0 && prod - cons < XNF_RX_MIN) {
		if (ifp->if_flags & IFF_RUNNING)
			timeout_add(&sc->sc_rx_fill, 1 << timer);
		if (timer < 10)
			timer++;
		return;
	}

	for (; n > 0; prod++, n--) {
		i = prod & (XNF_RX_DESC - 1);
		if (sc->sc_rx_buf[i])
			break;
		m = MCLGETI(NULL, M_DONTWAIT, NULL, XNF_MCLEN);
		if (m == NULL)
			break;
		m->m_len = m->m_pkthdr.len = XNF_MCLEN;
		dmap = sc->sc_rx_dmap[i];
		if (bus_dmamap_load_mbuf(sc->sc_dmat, dmap, m, BUS_DMA_READ |
		    BUS_DMA_NOWAIT)) {
			m_freem(m);
			break;
		}
		sc->sc_rx_buf[i] = m;
		rxr->rxr_desc[i].rxd_req.rxq_ref = dmap->dm_segs[0].ds_addr;
	}

	if (n > 0)
		if_rxr_put(&sc->sc_rx_slots, n);

	membar_producer();
	rxr->rxr_prod = prod;

	xen_intr_signal(sc->sc_xih);
}

int
xnf_rx_ring_create(struct xnf_softc *sc)
{
	int i, rsegs;

	/* Allocate a page of memory for the ring */
	if (bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &sc->sc_rx_seg, 1, &rsegs, BUS_DMA_ZERO | BUS_DMA_WAITOK)) {
		printf("%s: failed to allocate memory for the rx ring\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	/* Map in the allocated memory into the ring structure */
	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_rx_seg, 1, PAGE_SIZE,
	    (caddr_t *)(&sc->sc_rx_ring), BUS_DMA_WAITOK)) {
		printf("%s: failed to map memory for the rx ring\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	/* Create a map to load the ring memory into */
	if (bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE, 0,
	    BUS_DMA_WAITOK, &sc->sc_rx_rmap)) {
		printf("%s: failed to create a memory map for the rx ring\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	/* Load the ring into the ring map to extract the PA */
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_rx_rmap, sc->sc_rx_ring,
	    PAGE_SIZE, NULL, BUS_DMA_WAITOK)) {
		printf("%s: failed to load the rx ring map\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	sc->sc_rx_ref = sc->sc_rx_rmap->dm_segs[0].ds_addr;

	sc->sc_rx_ring->rxr_req_evt = sc->sc_rx_ring->rxr_rsp_evt = 1;

	for (i = 0; i < XNF_RX_DESC; i++) {
		if (bus_dmamap_create(sc->sc_dmat, XNF_MCLEN, 1,
		    XNF_MCLEN, PAGE_SIZE, BUS_DMA_WAITOK, &sc->sc_rx_dmap[i])) {
			printf("%s: failed to create a memory map for the"
			    " rx slot %d\n", sc->sc_dev.dv_xname, i);
			goto errout;
		}
		sc->sc_rx_ring->rxr_desc[i].rxd_req.rxq_id = i;
	}

	if_rxr_init(&sc->sc_rx_slots, XNF_RX_MIN, XNF_RX_DESC);
	xnf_rx_ring_fill(sc);

	return (0);

 errout:
	xnf_rx_ring_destroy(sc);
	return (-1);
}

void
xnf_rx_ring_drain(struct xnf_softc *sc)
{
	struct xnf_rx_ring *rxr = sc->sc_rx_ring;

	if (sc->sc_rx_cons != rxr->rxr_cons)
		xnf_rxeof(sc);
}

void
xnf_rx_ring_destroy(struct xnf_softc *sc)
{
	int i, slots = 0;

	for (i = 0; i < XNF_RX_DESC; i++) {
		if (sc->sc_rx_buf[i] == NULL)
			continue;
		bus_dmamap_unload(sc->sc_dmat, sc->sc_rx_dmap[i]);
		m_freem(sc->sc_rx_buf[i]);
		sc->sc_rx_buf[i] = NULL;
		slots++;
	}

	if_rxr_put(&sc->sc_rx_slots, slots);

	for (i = 0; i < XNF_RX_DESC; i++) {
		if (sc->sc_rx_dmap[i] == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_rx_dmap[i]);
		sc->sc_rx_dmap[i] = NULL;
	}
	if (sc->sc_rx_rmap) {
		bus_dmamap_unload(sc->sc_dmat, sc->sc_rx_rmap);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_rx_rmap);
	}
	if (sc->sc_rx_ring) {
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_rx_ring,
		    PAGE_SIZE);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_rx_seg, 1);
	}
	sc->sc_rx_ring = NULL;
	sc->sc_rx_rmap = NULL;
	sc->sc_rx_cons = 0;
}

int
xnf_tx_ring_create(struct xnf_softc *sc)
{
	int i, rsegs;

	/* Allocate a page of memory for the ring */
	if (bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &sc->sc_tx_seg, 1, &rsegs, BUS_DMA_ZERO | BUS_DMA_WAITOK)) {
		printf("%s: failed to allocate memory for the tx ring\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	/* Map in the allocated memory into the ring structure */
	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_tx_seg, 1, PAGE_SIZE,
	    (caddr_t *)&sc->sc_tx_ring, BUS_DMA_WAITOK)) {
		printf("%s: failed to map memory for the tx ring\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	/* Create a map to load the ring memory into */
	if (bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE, 0,
	    BUS_DMA_WAITOK, &sc->sc_tx_rmap)) {
		printf("%s: failed to create a memory map for the tx ring\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	/* Load the ring into the ring map to extract the PA */
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_tx_rmap, sc->sc_tx_ring,
	    PAGE_SIZE, NULL, BUS_DMA_WAITOK)) {
		printf("%s: failed to load the tx ring map\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	sc->sc_tx_ref = sc->sc_tx_rmap->dm_segs[0].ds_addr;

	sc->sc_tx_ring->txr_req_evt = sc->sc_tx_ring->txr_rsp_evt = 1;

	for (i = 0; i < XNF_TX_DESC; i++) {
		if (bus_dmamap_create(sc->sc_dmat, XNF_MCLEN, XNF_TX_FRAG,
		    XNF_MCLEN, PAGE_SIZE, BUS_DMA_WAITOK, &sc->sc_tx_dmap[i])) {
			printf("%s: failed to create a memory map for the"
			    " tx slot %d\n", sc->sc_dev.dv_xname, i);
			goto errout;
		}
		sc->sc_tx_ring->txr_desc[i].txd_req.txq_id = i;
	}

	return (0);

 errout:
	xnf_tx_ring_destroy(sc);
	return (-1);
}

void
xnf_tx_ring_drain(struct xnf_softc *sc)
{
	struct xnf_tx_ring *txr = sc->sc_tx_ring;

	if (sc->sc_tx_cons != txr->txr_cons)
		xnf_txeof(sc);
}

void
xnf_tx_ring_destroy(struct xnf_softc *sc)
{
	int i;

	for (i = 0; i < XNF_TX_DESC; i++) {
		if (sc->sc_tx_dmap[i] == NULL)
			continue;
		bus_dmamap_unload(sc->sc_dmat, sc->sc_tx_dmap[i]);
		if (sc->sc_tx_buf[i] == NULL)
			continue;
		m_free(sc->sc_tx_buf[i]);
		sc->sc_tx_buf[i] = NULL;
	}
	for (i = 0; i < XNF_TX_DESC; i++) {
		if (sc->sc_tx_dmap[i] == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_dmap[i]);
		sc->sc_tx_dmap[i] = NULL;
	}
	if (sc->sc_tx_rmap) {
		bus_dmamap_unload(sc->sc_dmat, sc->sc_tx_rmap);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_rmap);
	}
	if (sc->sc_tx_ring) {
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_tx_ring,
		    PAGE_SIZE);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_tx_seg, 1);
	}
	sc->sc_tx_ring = NULL;
	sc->sc_tx_rmap = NULL;
}

int
xnf_init_backend(struct xnf_softc *sc)
{
	const char *prop;
	char val[32];

	/* Plumb the Rx ring */
	prop = "rx-ring-ref";
	snprintf(val, sizeof(val), "%u", sc->sc_rx_ref);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;
	/* Enable "copy" mode */
	prop = "request-rx-copy";
	snprintf(val, sizeof(val), "%u", 1);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;
	/* Enable notify mode */
	prop = "feature-rx-notify";
	snprintf(val, sizeof(val), "%u", 1);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;
	/* Request multicast filtering */
	prop = "request-multicast-control";
	snprintf(val, sizeof(val), "%u", 1);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;

	/* Plumb the Tx ring */
	prop = "tx-ring-ref";
	snprintf(val, sizeof(val), "%u", sc->sc_tx_ref);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;
	/* Enable transmit scatter-gather mode */
	prop = "feature-sg";
	snprintf(val, sizeof(val), "%u", 1);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;

	/* Disable TCP/UDP checksum offload */
	prop = "feature-csum-offload";
	if (xs_setprop(&sc->sc_xa, prop, NULL, 0))
		goto errout;
	prop = "feature-no-csum-offload";
	snprintf(val, sizeof(val), "%u", 1);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;
	prop = "feature-ipv6-csum-offload";
	if (xs_setprop(&sc->sc_xa, prop, NULL, 0))
		goto errout;
	prop = "feature-no-ipv6-csum-offload";
	snprintf(val, sizeof(val), "%u", 1);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;

	/* Plumb the event channel port */
	prop = "event-channel";
	snprintf(val, sizeof(val), "%u", sc->sc_xih);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;

	/* Connect the device */
	prop = "state";
	snprintf(val, sizeof(val), "%u", 4);
	if (xs_setprop(&sc->sc_xa, prop, val, strlen(val)))
		goto errout;

	return (0);

 errout:
	printf("%s: failed to set \"%s\" property to \"%s\"\n",
	    sc->sc_dev.dv_xname, prop, val);
	return (-1);
}
