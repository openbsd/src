/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2016 Mike Belopuhov <mike@esdenera.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * The OpenBSD port was done under funding by Esdenera Networks GmbH.
 */

#include "bpfilter.h"
#include "vlan.h"
#include "hyperv.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/task.h>
#include <sys/timeout.h>

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#include <dev/pv/hypervreg.h>
#include <dev/pv/hypervvar.h>
#include <dev/pv/rndisreg.h>
#include <dev/pv/if_hvnreg.h>

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

#define HVN_DEBUG			1

#define HVN_NVS_BUFSIZE		  	PAGE_SIZE
#define HVN_NVS_MSGSIZE			32

/*
 * RNDIS control interface
 */
#define HVN_RNDIS_CTLREQS		4
#define HVN_RNDIS_CMPBUFSZ		512

#define HVN_RNDIS_MSG_LEN		\
	(sizeof(struct rndis) + RNDIS_VLAN_PPI_SIZE + RNDIS_CSUM_PPI_SIZE)

struct rndis_cmd {
	uint32_t			 rc_id;
	struct hvn_nvs_rndis		 rc_msg;
	struct rndis			*rc_req;
	bus_dmamap_t			 rc_dmap;
	uint64_t			 rc_gpa;
	struct rndis			 rc_cmp;
	uint32_t			 rc_cmplen;
	uint8_t				 rc_cmpbuf[HVN_RNDIS_CMPBUFSZ];
	struct mutex			 rc_mtx;
	TAILQ_ENTRY(rndis_cmd)		 rc_entry;
};
TAILQ_HEAD(rndis_queue, rndis_cmd);

/*
 * Tx ring
 */
#define HVN_TX_DESC			128
#define HVN_TX_FRAGS			31
#define HVN_TX_FRAG_SIZE		PAGE_SIZE
#define HVN_TX_PKT_SIZE			16384

struct hvn_tx_desc {
	uint32_t			 txd_id;
	int				 txd_ready;
	struct vmbus_gpa		 txd_sgl[HVN_TX_FRAGS + 1];
	int				 txd_nsge;
	struct mbuf			*txd_buf;
	bus_dmamap_t			 txd_dmap;
	struct vmbus_gpa		 txd_gpa;
	struct hvn_nvs_rndis		 txd_cmd;
	struct rndis			*txd_req;
};

struct hvn_softc {
	struct device			 sc_dev;
	struct hv_softc			*sc_hvsc;
	struct hv_channel		*sc_chan;
	bus_dma_tag_t			 sc_dmat;

	struct arpcom			 sc_ac;
	struct ifmedia			 sc_media;
	int				 sc_link_state;
	int				 sc_promisc;

	/* NVS protocol */
	int				 sc_proto;
	uint32_t			 sc_nvstid;
	uint8_t				 sc_nvsrsp[HVN_NVS_MSGSIZE];
	uint8_t				*sc_nvsbuf;
	struct mutex			 sc_nvslck;

	/* RNDIS protocol */
	int				 sc_ndisver;
	uint32_t			 sc_rndisrid;
	struct rndis_queue		 sc_cntl_sq; /* submission queue */
	struct mutex			 sc_cntl_sqlck;
	struct rndis_queue		 sc_cntl_cq; /* completion queue */
	struct mutex			 sc_cntl_cqlck;
	struct rndis_queue		 sc_cntl_fq; /* free queue */
	struct mutex			 sc_cntl_fqlck;
	struct rndis_cmd		 sc_cntl_msgs[HVN_RNDIS_CTLREQS];

	/* Rx ring */
	void				*sc_rx_ring;
	int				 sc_rx_size;
	uint32_t			 sc_rx_hndl;

	/* Tx ring */
	uint32_t			 sc_tx_next;
	uint32_t			 sc_tx_avail;
	struct hvn_tx_desc		 sc_tx_desc[HVN_TX_DESC];
	bus_dmamap_t			 sc_tx_rmap;
	void				*sc_tx_msgs;
	bus_dma_segment_t		 sc_tx_mseg;
};

int	hvn_match(struct device *, void *, void *);
void	hvn_attach(struct device *, struct device *, void *);
int	hvn_ioctl(struct ifnet *, u_long, caddr_t);
int	hvn_media_change(struct ifnet *);
void	hvn_media_status(struct ifnet *, struct ifmediareq *);
int	hvn_iff(struct hvn_softc *);
void	hvn_init(struct hvn_softc *);
void	hvn_stop(struct hvn_softc *);
void	hvn_start(struct ifnet *);
int	hvn_encap(struct hvn_softc *, struct mbuf *, struct hvn_tx_desc **);
void	hvn_decap(struct hvn_softc *, struct hvn_tx_desc *);
void	hvn_txeof(struct hvn_softc *, uint64_t);
void	hvn_rxeof(struct hvn_softc *, void *);
int	hvn_rx_ring_create(struct hvn_softc *);
int	hvn_rx_ring_destroy(struct hvn_softc *);
int	hvn_tx_ring_create(struct hvn_softc *);
void	hvn_tx_ring_destroy(struct hvn_softc *);
int	hvn_get_lladdr(struct hvn_softc *);
int	hvn_set_lladdr(struct hvn_softc *);
void	hvn_get_link_status(struct hvn_softc *);
void	hvn_link_status(struct hvn_softc *);

/* NSVP */
int	hvn_nvs_attach(struct hvn_softc *);
void	hvn_nvs_intr(void *);
int	hvn_nvs_cmd(struct hvn_softc *, void *, size_t, uint64_t, int);
int	hvn_nvs_ack(struct hvn_softc *, uint64_t);
void	hvn_nvs_detach(struct hvn_softc *);

/* RNDIS */
int	hvn_rndis_attach(struct hvn_softc *);
int	hvn_rndis_cmd(struct hvn_softc *, struct rndis_cmd *, int);
void	hvn_rndis_filter(struct hvn_softc *sc, uint64_t, void *);
void	hvn_rndis_input(struct hvn_softc *, caddr_t, uint32_t,
	    struct mbuf_list *);
void	hvn_rndis_complete(struct hvn_softc *, caddr_t, uint32_t);
int	hvn_rndis_output(struct hvn_softc *, struct hvn_tx_desc *);
void	hvn_rndis_status(struct hvn_softc *, caddr_t, uint32_t);
int	hvn_rndis_query(struct hvn_softc *, uint32_t, void *, size_t *);
int	hvn_rndis_set(struct hvn_softc *, uint32_t, void *, size_t);
int	hvn_rndis_open(struct hvn_softc *);
int	hvn_rndis_close(struct hvn_softc *);
void	hvn_rndis_detach(struct hvn_softc *);

struct cfdriver hvn_cd = {
	NULL, "hvn", DV_IFNET
};

const struct cfattach hvn_ca = {
	sizeof(struct hvn_softc), hvn_match, hvn_attach
};

int
hvn_match(struct device *parent, void *match, void *aux)
{
	struct hv_attach_args *aa = aux;

	if (strcmp("network", aa->aa_ident))
		return (0);

	return (1);
}

void
hvn_attach(struct device *parent, struct device *self, void *aux)
{
	struct hv_attach_args *aa = aux;
	struct hvn_softc *sc = (struct hvn_softc *)self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	sc->sc_hvsc = (struct hv_softc *)parent;
	sc->sc_chan = aa->aa_chan;
	sc->sc_dmat = aa->aa_dmat;

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	DPRINTF("\n");

	if (hvn_nvs_attach(sc)) {
		printf(": failed to init NVSP\n");
		return;
	}

	if (hvn_rx_ring_create(sc)) {
		printf(": failed to create Rx ring\n");
		goto detach;
	}

	if (hvn_tx_ring_create(sc)) {
		printf(": failed to create Tx ring\n");
		goto detach;
	}

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = hvn_ioctl;
	ifp->if_start = hvn_start;
	ifp->if_softc = sc;

#ifdef notyet
	ifp->if_capabilities = IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
	    IFCAP_CSUM_UDPv4;
#endif
#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	IFQ_SET_MAXLEN(&ifp->if_snd, HVN_TX_DESC - 1);

	ifmedia_init(&sc->sc_media, IFM_IMASK, hvn_media_change,
	    hvn_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_MANUAL, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_MANUAL);

	if_attach(ifp);

	if (hvn_rndis_attach(sc)) {
		printf(": failed to init RNDIS\n");
		goto detach;
	}

	if (hvn_get_lladdr(sc)) {
		printf(": failed to obtain an ethernet address\n");
		hvn_rndis_detach(sc);
		goto detach;
	}

	DPRINTF("%s:", sc->sc_dev.dv_xname);
	printf(" channel %u: NVS %u.%u NDIS %u.%u, address %s\n",
	    sc->sc_chan->ch_id, sc->sc_proto >> 16, sc->sc_proto & 0xffff,
	    sc->sc_ndisver >> 16 , sc->sc_ndisver & 0xffff,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	ether_ifattach(ifp);
	return;

 detach:
	hvn_rx_ring_destroy(sc);
	hvn_tx_ring_destroy(sc);
	hvn_nvs_detach(sc);
	if (ifp->if_start)
		if_detach(ifp);
}

int
hvn_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct hvn_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			hvn_init(sc);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				hvn_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				hvn_stop(sc);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, command);
		break;
	default:
		error = ether_ioctl(ifp, &sc->sc_ac, command, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			hvn_iff(sc);
		error = 0;
	}

	splx(s);

	return (error);
}

int
hvn_media_change(struct ifnet *ifp)
{
	return (0);
}

void
hvn_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct hvn_softc *sc = ifp->if_softc;

	hvn_get_link_status(sc);
	hvn_link_status(sc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER | IFM_MANUAL;
	if (sc->sc_link_state == LINK_STATE_UP)
		ifmr->ifm_status |= IFM_ACTIVE;
}

void
hvn_link_status(struct hvn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (sc->sc_link_state != ifp->if_link_state) {
		ifp->if_link_state = sc->sc_link_state;
		if_link_state_change(ifp);
	}
}

int
hvn_iff(struct hvn_softc *sc)
{
	/* XXX */
	sc->sc_promisc = 0;

	return (0);
}

void
hvn_init(struct hvn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	hvn_stop(sc);

	hvn_iff(sc);

	if (hvn_rndis_open(sc) == 0) {
		ifp->if_flags |= IFF_RUNNING;
		ifq_clr_oactive(&ifp->if_snd);
	}
}

void
hvn_stop(struct hvn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (ifp->if_flags & IFF_RUNNING) {
		ifp->if_flags &= ~IFF_RUNNING;
		hvn_rndis_close(sc);
	}

	ifq_barrier(&ifp->if_snd);
	intr_barrier(sc->sc_chan);

	ifq_clr_oactive(&ifp->if_snd);
}

void
hvn_start(struct ifnet *ifp)
{
	struct hvn_softc *sc = ifp->if_softc;
	struct hvn_tx_desc *txd;
	struct mbuf *m;

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd))
		return;

	for (;;) {
		if (!sc->sc_tx_avail) {
			/* transient */
			ifq_set_oactive(&ifp->if_snd);
			break;
		}

		m = ifq_dequeue(&ifp->if_snd);
		if (m == NULL)
			break;

		if (hvn_encap(sc, m, &txd)) {
			/* the chain is too large */
			ifp->if_oerrors++;
			m_freem(m);
			continue;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		if (hvn_rndis_output(sc, txd)) {
			hvn_decap(sc, txd);
			m_freem(m);
		}

		sc->sc_tx_next++;
		sc->sc_tx_next %= HVN_TX_DESC;

		atomic_dec_int(&sc->sc_tx_avail);

		ifp->if_opackets++;
	}
}

int
hvn_encap(struct hvn_softc *sc, struct mbuf *m, struct hvn_tx_desc **txd0)
{
	struct hvn_tx_desc *txd;
	struct rndis_pkt *pkt;
	bus_dma_segment_t *seg;
	size_t rlen;
	int i;

	/* XXX use queues? */
	txd = &sc->sc_tx_desc[sc->sc_tx_next];
	while (!txd->txd_ready) {
		sc->sc_tx_next++;
		sc->sc_tx_next %= HVN_TX_DESC;
		txd = &sc->sc_tx_desc[sc->sc_tx_next];
	}

	memset(txd->txd_req, 0, HVN_RNDIS_MSG_LEN);
	txd->txd_req->msg_type = RNDIS_PACKET_MSG;

	pkt = (struct rndis_pkt *)&txd->txd_req->msg;
	pkt->data_offset = sizeof(struct rndis_pkt);
	pkt->data_length = m->m_pkthdr.len;
	pkt->pkt_info_offset = sizeof(struct rndis_pkt);
	rlen = RNDIS_MESSAGE_SIZE(*pkt);

	if (bus_dmamap_load_mbuf(sc->sc_dmat, txd->txd_dmap, m, BUS_DMA_READ |
	    BUS_DMA_NOWAIT)) {
		DPRINTF("%s: failed to load mbuf\n", sc->sc_dev.dv_xname);
		return (-1);
	}
	txd->txd_buf = m;

	/* Per-packet info adjusts rlen */

	/* Final length value for the RNDIS header and data */
	txd->txd_req->msg_len = pkt->data_length + rlen;

	/* Attach an RNDIS message into the first slot */
	txd->txd_sgl[0].gpa_page = txd->txd_gpa.gpa_page;
	txd->txd_sgl[0].gpa_ofs = txd->txd_gpa.gpa_ofs;
	txd->txd_sgl[0].gpa_len = rlen;
	txd->txd_nsge = txd->txd_dmap->dm_nsegs + 1;

	for (i = 0; i < txd->txd_dmap->dm_nsegs; i++) {
		seg = &txd->txd_dmap->dm_segs[i];
		txd->txd_sgl[1 + i].gpa_page = atop(seg->ds_addr);
		txd->txd_sgl[1 + i].gpa_ofs = seg->ds_addr & PAGE_MASK;
		txd->txd_sgl[1 + i].gpa_len = seg->ds_len;
	}

	*txd0 = txd;
	return (0);
}

void
hvn_decap(struct hvn_softc *sc, struct hvn_tx_desc *txd)
{
	bus_dmamap_sync(sc->sc_dmat, txd->txd_dmap, 0, 0,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, txd->txd_dmap);
	txd->txd_buf = NULL;
	txd->txd_nsge = 0;
}

void
hvn_txeof(struct hvn_softc *sc, uint64_t tid)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct hvn_tx_desc *txd;
	struct mbuf *m;
	uint32_t id = tid >> 32;

	if ((tid & 0xffffffff) != 0)
		return;
	if (id > HVN_TX_DESC)
		panic("tx packet index too large: %u", id);

	txd = &sc->sc_tx_desc[id];

	if ((m = txd->txd_buf) == NULL)
		panic("%s: no mbuf @%u\n", sc->sc_dev.dv_xname, id);
	txd->txd_buf = NULL;

	bus_dmamap_sync(sc->sc_dmat, txd->txd_dmap, 0, 0,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, txd->txd_dmap);
	m_freem(m);

	txd->txd_ready = 1;

	atomic_inc_int(&sc->sc_tx_avail);

	if (ifq_is_oactive(&ifp->if_snd))
		ifq_restart(&ifp->if_snd);
}

int
hvn_rx_ring_create(struct hvn_softc *sc)
{
	struct hvn_nvs_rxbuf_conn cmd;
	struct hvn_nvs_rxbuf_conn_resp *rsp;
	uint64_t tid;

	if (sc->sc_proto <= HVN_NVS_PROTO_VERSION_2)
		sc->sc_rx_size = 15 * 1024 * 1024;	/* 15MB */
	else
		sc->sc_rx_size = 16 * 1024 * 1024; 	/* 16MB */
	sc->sc_rx_ring = km_alloc(sc->sc_rx_size, &kv_any, &kp_zero,
	    cold ? &kd_nowait : &kd_waitok);
	if (sc->sc_rx_ring == NULL) {
		DPRINTF("%s: failed to allocate Rx ring buffer\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	if (hv_handle_alloc(sc->sc_chan, sc->sc_rx_ring, sc->sc_rx_size,
	    &sc->sc_rx_hndl)) {
		DPRINTF("%s: failed to obtain a PA handle\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.nvs_type = HVN_NVS_TYPE_RXBUF_CONN;
	cmd.nvs_gpadl = sc->sc_rx_hndl;
	cmd.nvs_sig = HVN_NVS_RXBUF_SIG;

	tid = atomic_inc_int_nv(&sc->sc_nvstid);
	if (hvn_nvs_cmd(sc, &cmd, sizeof(cmd), tid, 100))
		goto errout;

	rsp = (struct hvn_nvs_rxbuf_conn_resp *)&sc->sc_nvsrsp;
	if (rsp->nvs_status != HVN_NVS_STATUS_OK) {
		DPRINTF("%s: failed to set up the Rx ring\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	if (rsp->nvs_nsect > 1) {
		DPRINTF("%s: invalid number of Rx ring sections: %d\n",
		    sc->sc_dev.dv_xname, rsp->nvs_nsect);
		hvn_rx_ring_destroy(sc);
		return (-1);
	}
	return (0);

 errout:
	if (sc->sc_rx_hndl) {
		hv_handle_free(sc->sc_chan, sc->sc_rx_hndl);
		sc->sc_rx_hndl = 0;
	}
	if (sc->sc_rx_ring) {
		km_free(sc->sc_rx_ring, sc->sc_rx_size, &kv_any, &kp_zero);
		sc->sc_rx_ring = NULL;
	}
	return (-1);
}

int
hvn_rx_ring_destroy(struct hvn_softc *sc)
{
	struct hvn_nvs_rxbuf_disconn cmd;
	uint64_t tid;

	if (sc->sc_rx_ring == NULL)
		return (0);

	memset(&cmd, 0, sizeof(cmd));
	cmd.nvs_type = HVN_NVS_TYPE_RXBUF_DISCONN;
	cmd.nvs_sig = HVN_NVS_RXBUF_SIG;

	tid = atomic_inc_int_nv(&sc->sc_nvstid);
	if (hvn_nvs_cmd(sc, &cmd, sizeof(cmd), tid, 0))
		return (-1);

	delay(100);

	hv_handle_free(sc->sc_chan, sc->sc_rx_hndl);

	sc->sc_rx_hndl = 0;

	km_free(sc->sc_rx_ring, sc->sc_rx_size, &kv_any, &kp_zero);
	sc->sc_rx_ring = NULL;

	return (0);
}

int
hvn_tx_ring_create(struct hvn_softc *sc)
{
	struct hvn_tx_desc *txd;
	bus_dma_segment_t *seg;
	size_t msgsize;
	int i, rsegs;
	paddr_t pa;

	msgsize = roundup(HVN_RNDIS_MSG_LEN, 128);

	/* Allocate memory to store RNDIS messages */
	if (bus_dmamem_alloc(sc->sc_dmat, msgsize * HVN_TX_DESC, PAGE_SIZE, 0,
	    &sc->sc_tx_mseg, 1, &rsegs, BUS_DMA_ZERO | BUS_DMA_WAITOK)) {
		DPRINTF("%s: failed to allocate memory for RDNIS messages\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_tx_mseg, 1, msgsize *
	    HVN_TX_DESC, (caddr_t *)&sc->sc_tx_msgs, BUS_DMA_WAITOK)) {
		DPRINTF("%s: failed to establish mapping for RDNIS messages\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	if (bus_dmamap_create(sc->sc_dmat, msgsize * HVN_TX_DESC, 1,
	    msgsize * HVN_TX_DESC, 0, BUS_DMA_WAITOK, &sc->sc_tx_rmap)) {
		DPRINTF("%s: failed to create map for RDNIS messages\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_tx_rmap, sc->sc_tx_msgs,
	    msgsize * HVN_TX_DESC, NULL, BUS_DMA_WAITOK)) {
		DPRINTF("%s: failed to create map for RDNIS messages\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}

	for (i = 0; i < HVN_TX_DESC; i++) {
		txd = &sc->sc_tx_desc[i];
		if (bus_dmamap_create(sc->sc_dmat, HVN_TX_PKT_SIZE,
		    HVN_TX_FRAGS, HVN_TX_FRAG_SIZE, 0, BUS_DMA_WAITOK,
		    &txd->txd_dmap)) {
			DPRINTF("%s: failed to create map for TX descriptors\n",
			    sc->sc_dev.dv_xname);
			goto errout;
		}
		seg = &sc->sc_tx_rmap->dm_segs[0];
		pa = seg->ds_addr + (msgsize * i);
		txd->txd_gpa.gpa_page = atop(pa);
		txd->txd_gpa.gpa_ofs = pa & PAGE_MASK;
		txd->txd_gpa.gpa_len = msgsize;
		txd->txd_req = (struct rndis *)((caddr_t)sc->sc_tx_msgs +
		    (msgsize * i));
		txd->txd_id = i;
		txd->txd_ready = 1;
	}
	sc->sc_tx_avail = HVN_TX_DESC;

	return (0);

 errout:
	hvn_tx_ring_destroy(sc);
	return (-1);
}

void
hvn_tx_ring_destroy(struct hvn_softc *sc)
{
	struct hvn_tx_desc *txd;
	int i;

	for (i = 0; i < HVN_TX_DESC; i++) {
		txd = &sc->sc_tx_desc[i];
		if (txd->txd_dmap == NULL)
			continue;
		bus_dmamap_sync(sc->sc_dmat, txd->txd_dmap, 0, 0,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txd->txd_dmap);
		bus_dmamap_destroy(sc->sc_dmat, txd->txd_dmap);
		txd->txd_dmap = NULL;
		if (txd->txd_buf == NULL)
			continue;
		m_free(txd->txd_buf);
		txd->txd_buf = NULL;
	}
	if (sc->sc_tx_rmap) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_tx_rmap, 0, 0,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_tx_rmap);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_rmap);
	}
	if (sc->sc_tx_msgs) {
		size_t msgsize = roundup(HVN_RNDIS_MSG_LEN, 128);

		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_tx_msgs,
		    msgsize * HVN_TX_DESC);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_tx_mseg, 1);
	}
	sc->sc_tx_rmap = NULL;
	sc->sc_tx_msgs = NULL;
}

int
hvn_get_lladdr(struct hvn_softc *sc)
{
	char enaddr[ETHER_ADDR_LEN];
	size_t addrlen = ETHER_ADDR_LEN;
	int rv;

	rv = hvn_rndis_query(sc, RNDIS_OID_802_3_PERMANENT_ADDRESS,
	    enaddr, &addrlen);
	if (rv == 0 && addrlen == ETHER_ADDR_LEN)
		memcpy(sc->sc_ac.ac_enaddr, enaddr, ETHER_ADDR_LEN);
	return (rv);
}

int
hvn_set_lladdr(struct hvn_softc *sc)
{
	return (hvn_rndis_set(sc, RNDIS_OID_802_3_CURRENT_ADDRESS,
	    sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN));
}

void
hvn_get_link_status(struct hvn_softc *sc)
{
	uint32_t state;
	size_t len = sizeof(state);

	if (hvn_rndis_query(sc, RNDIS_OID_GEN_MEDIA_CONNECT_STATUS,
	    &state, &len) == 0)
		sc->sc_link_state = (state == 0) ? LINK_STATE_UP :
		    LINK_STATE_DOWN;
}

int
hvn_nvs_attach(struct hvn_softc *sc)
{
	const uint32_t protos[] = {
		HVN_NVS_PROTO_VERSION_5, HVN_NVS_PROTO_VERSION_4,
		HVN_NVS_PROTO_VERSION_2, HVN_NVS_PROTO_VERSION_1
	};
	struct hvn_nvs_init cmd;
	struct hvn_nvs_init_resp *rsp;
	struct hvn_nvs_ndis_init ncmd;
	uint64_t tid;
	uint32_t ndisver;
	int i;

	/* 4 page sized buffer for channel messages */
	sc->sc_nvsbuf = km_alloc(HVN_NVS_BUFSIZE, &kv_any, &kp_zero,
	    (cold ? &kd_nowait : &kd_waitok));
	if (sc->sc_nvsbuf == NULL) {
		DPRINTF("%s: failed to allocate channel data buffer\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	sc->sc_chan->ch_buflen = PAGE_SIZE * 4;

	/* Associate our interrupt handler with the channel */
	if (hv_channel_open(sc->sc_chan, NULL, 0, hvn_nvs_intr, sc)) {
		DPRINTF("%s: failed to open channel\n", sc->sc_dev.dv_xname);
		km_free(sc->sc_nvsbuf, HVN_NVS_BUFSIZE, &kv_any, &kp_zero);
		return (-1);
	}

	mtx_init(&sc->sc_nvslck, IPL_NET);

	memset(&cmd, 0, sizeof(cmd));
	cmd.nvs_type = HVN_NVS_TYPE_INIT;
	for (i = 0; i < nitems(protos); i++) {
		cmd.nvs_ver_min = cmd.nvs_ver_max = protos[i];
		tid = atomic_inc_int_nv(&sc->sc_nvstid);
		if (hvn_nvs_cmd(sc, &cmd, sizeof(cmd), tid, 100))
			return (-1);
		rsp = (struct hvn_nvs_init_resp *)&sc->sc_nvsrsp;
		if (rsp->nvs_status == HVN_NVS_STATUS_OK) {
			sc->sc_proto = protos[i];
			break;
		}
	}
	if (!sc->sc_proto) {
		DPRINTF("%s: failed to negotiate NVSP version\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}

	memset(&ncmd, 0, sizeof(ncmd));
	ncmd.nvs_type = HVN_NVS_TYPE_NDIS_INIT;
	if (sc->sc_proto <= HVN_NVS_PROTO_VERSION_4)
		ndisver = NDIS_VERSION_6_1;
	else
		ndisver = NDIS_VERSION_6_30;
	ncmd.nvs_ndis_major = (ndisver & 0xffff0000) >> 16;
	ncmd.nvs_ndis_minor = (ndisver & 0x0000ffff);

	tid = atomic_inc_int_nv(&sc->sc_nvstid);
	if (hvn_nvs_cmd(sc, &ncmd, sizeof(ncmd), tid, 100))
		return (-1);

	sc->sc_ndisver = ndisver;

	return (0);
}

void
hvn_nvs_intr(void *arg)
{
	struct hvn_softc *sc = arg;
	struct vmbus_chanpkt_hdr *cph;
	struct hvn_nvs_hdr *nvs;
	uint64_t rid;
	uint32_t rlen;
	int rv;

	for (;;) {
		rv = hv_channel_recv(sc->sc_chan, sc->sc_nvsbuf,
		    HVN_NVS_BUFSIZE, &rlen, &rid, 1);
		if (rv != 0 || rlen == 0) {
			if (rv != EAGAIN)
				printf("%s: failed to receive an NVSP "
				    "packet\n", sc->sc_dev.dv_xname);
			break;
		}
		cph = (struct vmbus_chanpkt_hdr *)sc->sc_nvsbuf;
		nvs = (struct hvn_nvs_hdr *)VMBUS_CHANPKT_CONST_DATA(cph);

		if (cph->cph_type == VMBUS_CHANPKT_TYPE_COMP) {
			switch (nvs->nvs_type) {
			case HVN_NVS_TYPE_INIT_RESP:
			case HVN_NVS_TYPE_RXBUF_CONNRESP:
			case HVN_NVS_TYPE_CHIM_CONNRESP:
			case HVN_NVS_TYPE_SUBCH_RESP:
				/* copy the response back */
				memcpy(&sc->sc_nvsrsp, nvs, HVN_NVS_MSGSIZE);
				wakeup_one(&sc->sc_nvsrsp);
				break;
			case HVN_NVS_TYPE_RNDIS_ACK:
				hvn_txeof(sc, cph->cph_tid);
				break;
			default:
				printf("%s: unhandled NVSP packet type %d "
				    "on completion\n", sc->sc_dev.dv_xname,
				    nvs->nvs_type);
			}
		} else if (cph->cph_type == VMBUS_CHANPKT_TYPE_RXBUF) {
			switch (nvs->nvs_type) {
			case HVN_NVS_TYPE_RNDIS:
				hvn_rndis_filter(sc, cph->cph_tid, cph);
				break;
			default:
				printf("%s: unhandled NVSP packet type %d "
				    "on receive\n", sc->sc_dev.dv_xname,
				    nvs->nvs_type);
			}
		} else
			printf("%s: unknown NVSP packet type %u\n",
			    sc->sc_dev.dv_xname, cph->cph_type);
	}
}

int
hvn_nvs_cmd(struct hvn_softc *sc, void *cmd, size_t cmdsize, uint64_t tid,
    int timo)
{
	struct hvn_nvs_hdr *hdr = cmd;
	int tries = 10;
	int rv;

	do {
		rv = hv_channel_send(sc->sc_chan, cmd, cmdsize,
		    tid, VMBUS_CHANPKT_TYPE_INBAND,
		    timo ? VMBUS_CHANPKT_FLAG_RC : 0);
		if (rv == EAGAIN) {
			if (timo)
				tsleep(cmd, PRIBIO, "hvnsend", timo / 10);
			else
				delay(100);
		} else if (rv) {
			DPRINTF("%s: NVSP operation %d send error %d\n",
			    sc->sc_dev.dv_xname, hdr->nvs_type, rv);
			return (rv);
		}
	} while (rv != 0 && --tries > 0);

	if (timo) {
		mtx_enter(&sc->sc_nvslck);
		rv = msleep(&sc->sc_nvsrsp, &sc->sc_nvslck, PRIBIO, "hvnvsp",
		    timo);
		mtx_leave(&sc->sc_nvslck);
#ifdef HVN_DEBUG
		switch (rv) {
		case EINTR:
			rv = 0;
			break;
		case EWOULDBLOCK:
			printf("%s: NVSP opertaion %d timed out\n",
			    sc->sc_dev.dv_xname, hdr->nvs_type);
		}
	}
#endif
	return (rv);
}

int
hvn_nvs_ack(struct hvn_softc *sc, uint64_t tid)
{
	struct hvn_nvs_rndis_ack cmd;
	int tries = 5;
	int rv;

	cmd.nvs_type = HVN_NVS_TYPE_RNDIS_ACK;
	cmd.nvs_status = HVN_NVS_STATUS_OK;
	do {
		rv = hv_channel_send(sc->sc_chan, &cmd, sizeof(cmd),
		    tid, VMBUS_CHANPKT_TYPE_COMP, 0);
		if (rv == EAGAIN)
			delay(100);
		else if (rv) {
			DPRINTF("%s: NVSP acknowledgement error %d\n",
			    sc->sc_dev.dv_xname, rv);
			return (rv);
		}
	} while (rv != 0 && --tries > 0);
	return (rv);
}

void
hvn_nvs_detach(struct hvn_softc *sc)
{
	if (hv_channel_close(sc->sc_chan) == 0) {
		km_free(sc->sc_nvsbuf, HVN_NVS_BUFSIZE, &kv_any, &kp_zero);
		sc->sc_nvsbuf = NULL;
	}
}

static inline struct rndis_cmd *
hvn_alloc_cmd(struct hvn_softc *sc)
{
	struct rndis_cmd *rc;

	mtx_enter(&sc->sc_cntl_fqlck);
	while ((rc = TAILQ_FIRST(&sc->sc_cntl_fq)) == NULL)
		msleep(&sc->sc_cntl_fq, &sc->sc_cntl_fqlck,
		    PRIBIO, "hvnrr", 1);
	TAILQ_REMOVE(&sc->sc_cntl_fq, rc, rc_entry);
	mtx_leave(&sc->sc_cntl_fqlck);
	return (rc);
}

static inline void
hvn_submit_cmd(struct hvn_softc *sc, struct rndis_cmd *rc)
{
	mtx_enter(&sc->sc_cntl_sqlck);
	TAILQ_INSERT_TAIL(&sc->sc_cntl_sq, rc, rc_entry);
	mtx_leave(&sc->sc_cntl_sqlck);
}

static inline struct rndis_cmd *
hvn_complete_cmd(struct hvn_softc *sc, uint32_t id)
{
	struct rndis_cmd *rc;

	mtx_enter(&sc->sc_cntl_sqlck);
	TAILQ_FOREACH(rc, &sc->sc_cntl_sq, rc_entry) {
		if (rc->rc_id == id) {
			TAILQ_REMOVE(&sc->sc_cntl_sq, rc, rc_entry);
			break;
		}
	}
	mtx_leave(&sc->sc_cntl_sqlck);
	if (rc != NULL) {
		mtx_enter(&sc->sc_cntl_cqlck);
		TAILQ_INSERT_TAIL(&sc->sc_cntl_cq, rc, rc_entry);
		mtx_leave(&sc->sc_cntl_cqlck);
	}
	return (rc);
}

static inline int
hvn_rollback_cmd(struct hvn_softc *sc, struct rndis_cmd *rc)
{
	struct rndis_cmd *rn;

	mtx_enter(&sc->sc_cntl_sqlck);
	TAILQ_FOREACH(rn, &sc->sc_cntl_sq, rc_entry) {
		if (rn == rc) {
			TAILQ_REMOVE(&sc->sc_cntl_sq, rc, rc_entry);
			mtx_leave(&sc->sc_cntl_sqlck);
			return (0);
		}
	}
	mtx_leave(&sc->sc_cntl_sqlck);
	return (-1);
}

static inline void
hvn_free_cmd(struct hvn_softc *sc, struct rndis_cmd *rc)
{
	memset(rc->rc_req, 0, sizeof(*rc->rc_req));
	memset(&rc->rc_cmp, 0, sizeof(rc->rc_cmp));
	memset(&rc->rc_msg, 0, sizeof(rc->rc_msg));
	mtx_enter(&sc->sc_cntl_fqlck);
	TAILQ_INSERT_TAIL(&sc->sc_cntl_fq, rc, rc_entry);
	mtx_leave(&sc->sc_cntl_fqlck);
	wakeup(&sc->sc_cntl_fq);
}

int
hvn_rndis_attach(struct hvn_softc *sc)
{
	struct rndis_init_req *req;
	struct rndis_init_comp *cmp;
	struct rndis_cmd *rc;
	int i, rv;

	/* RNDIS control message queues */
	TAILQ_INIT(&sc->sc_cntl_sq);
	TAILQ_INIT(&sc->sc_cntl_cq);
	TAILQ_INIT(&sc->sc_cntl_fq);
	mtx_init(&sc->sc_cntl_sqlck, IPL_NET);
	mtx_init(&sc->sc_cntl_cqlck, IPL_NET);
	mtx_init(&sc->sc_cntl_fqlck, IPL_NET);

	for (i = 0; i < HVN_RNDIS_CTLREQS; i++) {
		rc = &sc->sc_cntl_msgs[i];
		if (bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1,
		    PAGE_SIZE, 0, BUS_DMA_WAITOK, &rc->rc_dmap)) {
			DPRINTF("%s: failed to create RNDIS command map\n",
			    sc->sc_dev.dv_xname);
			goto errout;
		}
		rc->rc_req = km_alloc(PAGE_SIZE, &kv_any, &kp_zero,
		    &kd_waitok);
		if (rc->rc_req == NULL) {
			DPRINTF("%s: failed to allocate RNDIS command\n",
			    sc->sc_dev.dv_xname);
			bus_dmamap_destroy(sc->sc_dmat, rc->rc_dmap);
			goto errout;
		}
		if (bus_dmamap_load(sc->sc_dmat, rc->rc_dmap, rc->rc_req,
		    PAGE_SIZE, NULL, BUS_DMA_WAITOK)) {
			DPRINTF("%s: failed to load RNDIS command map\n",
			    sc->sc_dev.dv_xname);
			km_free(rc->rc_req, PAGE_SIZE, &kv_any, &kp_zero);
			bus_dmamap_destroy(sc->sc_dmat, rc->rc_dmap);
			goto errout;
		}
		rc->rc_gpa = atop(rc->rc_dmap->dm_segs[0].ds_addr);
		mtx_init(&rc->rc_mtx, IPL_NET);
		TAILQ_INSERT_TAIL(&sc->sc_cntl_fq, rc, rc_entry);
	}

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_INITIALIZE_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req);
	rc->rc_cmplen = RNDIS_MESSAGE_SIZE(*cmp);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_init_req *)&rc->rc_req->msg;
	req->request_id = rc->rc_id;
	req->major_version = RNDIS_MAJOR_VERSION;
	req->minor_version = RNDIS_MINOR_VERSION;
	req->max_xfer_size = 2048; /* XXX */

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_cmd(sc, rc, 500)) != 0) {
		DPRINTF("%s: INITIALIZE_MSG failed, error %u\n",
		    sc->sc_dev.dv_xname, rv);
		hvn_free_cmd(sc, rc);
		goto errout;
	}
	cmp = (struct rndis_init_comp *)&rc->rc_cmp.msg;
	if (cmp->status != RNDIS_STATUS_SUCCESS) {
		DPRINTF("%s: failed to init RNDIS, error %#x\n",
		    sc->sc_dev.dv_xname, cmp->status);
		hvn_free_cmd(sc, rc);
		goto errout;
	}

	hvn_free_cmd(sc, rc);

	return (0);

errout:
	for (i = 0; i < HVN_RNDIS_CTLREQS; i++) {
		rc = &sc->sc_cntl_msgs[i];
		if (rc->rc_req == NULL)
			continue;
		TAILQ_REMOVE(&sc->sc_cntl_fq, rc, rc_entry);
		km_free(rc->rc_req, PAGE_SIZE, &kv_any, &kp_zero);
		rc->rc_req = NULL;
		bus_dmamap_destroy(sc->sc_dmat, rc->rc_dmap);
	}
	return (-1);
}

int
hvn_rndis_cmd(struct hvn_softc *sc, struct rndis_cmd *rc, int timo)
{
	struct hvn_nvs_rndis *msg = &rc->rc_msg;
	struct vmbus_gpa sgl[1];
	int tries = 10;
	int rv;

	KASSERT(timo > 0);

	msg->nvs_type = HVN_NVS_TYPE_RNDIS;
	msg->nvs_rndis_mtype = HVN_NVS_RNDIS_MTYPE_CTRL;
	msg->nvs_chim_idx = HVN_NVS_CHIM_IDX_INVALID;

	sgl[0].gpa_page = rc->rc_gpa;
	sgl[0].gpa_len = rc->rc_req->msg_len;
	sgl[0].gpa_ofs = 0;

	hvn_submit_cmd(sc, rc);

	do {
		rv = hv_channel_send_sgl(sc->sc_chan, sgl, 1, &rc->rc_msg,
		    sizeof(*msg), rc->rc_id);
		if (rv == EAGAIN)
			tsleep(rc, PRIBIO, "hvnsendbuf", timo / 10);
		else if (rv) {
			DPRINTF("%s: RNDIS operation %d send error %d\n",
			    sc->sc_dev.dv_xname, rc->rc_req->msg_type, rv);
			return (rv);
		}
	} while (rv != 0 && --tries > 0);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_POSTWRITE);

	mtx_enter(&rc->rc_mtx);
	rv = msleep(rc, &rc->rc_mtx, PRIBIO, "rndisctl", timo);
	mtx_leave(&rc->rc_mtx);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_POSTREAD);

#ifdef HVN_DEBUG
	switch (rv) {
	case EINTR:
		rv = 0;
		break;
	case EWOULDBLOCK:
		if (hvn_rollback_cmd(sc, rc)) {
			/* failed to rollback? go for one sleep cycle */
			tsleep(rc, PRIBIO, "rndisctl2", 1);
			rv = 0;
			break;
		}
		printf("%s: RNDIS opertaion %d timed out\n", sc->sc_dev.dv_xname,
		    rc->rc_req->msg_type);
	}
#endif
	return (rv);
}

void
hvn_rndis_filter(struct hvn_softc *sc, uint64_t tid, void *arg)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct vmbus_chanpkt_prplist *cp = arg;
	uint32_t off, len, type;
	int i;

	if (sc->sc_rx_ring == NULL) {
		DPRINTF("%s: invalid rx ring\n", sc->sc_dev.dv_xname);
		return;
	}
	for (i = 0; i < cp->cp_range_cnt; i++) {
		off = cp->cp_range[i].gpa_ofs;
		len = cp->cp_range[i].gpa_len;

		KASSERT(off + len <= sc->sc_rx_size);
		KASSERT(len >= RNDIS_HEADER_SIZE + 4);

		memcpy(&type, (caddr_t)sc->sc_rx_ring + off, sizeof(type));
		switch (type) {
		/* data message */
		case RNDIS_PACKET_MSG:
			hvn_rndis_input(sc, (caddr_t)sc->sc_rx_ring +
			    off, len, &ml);
			break;
		/* completion messages */
		case RNDIS_INITIALIZE_CMPLT:
		case RNDIS_QUERY_CMPLT:
		case RNDIS_SET_CMPLT:
		case RNDIS_RESET_CMPLT:
		case RNDIS_KEEPALIVE_CMPLT:
			hvn_rndis_complete(sc, (caddr_t)sc->sc_rx_ring +
			    off, len);
			break;
		/* notification message */
		case RNDIS_INDICATE_STATUS_MSG:
			hvn_rndis_status(sc, (caddr_t)sc->sc_rx_ring +
			    off, len);
			break;
		default:
			printf("%s: unhandled RNDIS message type %u\n",
			    sc->sc_dev.dv_xname, type);
		}
	}
	hvn_nvs_ack(sc, tid);

	if (MBUF_LIST_FIRST(&ml))
		if_input(ifp, &ml);
}

static inline struct mbuf *
hvn_devget(struct hvn_softc *sc, caddr_t buf, uint32_t len)
{
	struct mbuf *m;

	if (len + ETHER_ALIGN <= MHLEN)
		MGETHDR(m, M_NOWAIT, MT_DATA);
	else
		m = MCLGETI(NULL, M_NOWAIT, NULL, len + ETHER_ALIGN);
	if (m == NULL)
		return (NULL);
	m->m_len = m->m_pkthdr.len = len;
	m_adj(m, ETHER_ALIGN);

	if (m_copyback(m, 0, len, buf, M_NOWAIT)) {
		m_freem(m);
		return (NULL);
	}

	return (m);
}

void
hvn_rndis_input(struct hvn_softc *sc, caddr_t buf, uint32_t len,
    struct mbuf_list *ml)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct rndis_pkt *pkt;
	struct rndis_pkt_info *ppi;
	struct rndis_tcp_ip_csum_info *csum;
	struct ndis_8021q_info *vlan;
	struct mbuf *m;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (len < RNDIS_HEADER_SIZE + sizeof(*pkt)) {
		printf("%s: data packet too short: %u\n",
		    sc->sc_dev.dv_xname, len);
		return;
	}

	pkt = (struct rndis_pkt *)(buf + RNDIS_HEADER_SIZE);

	if (pkt->data_offset + pkt->data_length > len) {
		printf("%s: data packet out of bounds: %u@%u\n",
		    sc->sc_dev.dv_xname, pkt->data_offset, pkt->data_length);
		return;
	}

	if ((m = hvn_devget(sc, buf + RNDIS_HEADER_SIZE + pkt->data_offset,
	    pkt->data_length)) == NULL) {
		ifp->if_ierrors++;
		return;
	}

	while (pkt->pkt_info_length > 0) {
		if (pkt->pkt_info_offset + pkt->pkt_info_length > len) {
			printf("%s: PPI out of bounds: %u@%u\n",
			    sc->sc_dev.dv_xname, pkt->pkt_info_length,
			    pkt->pkt_info_offset);
			break;
		}
		ppi = (struct rndis_pkt_info *)((caddr_t)pkt +
		    pkt->pkt_info_offset);
		if (ppi->size > pkt->pkt_info_length) {
			printf("%s: invalid PPI size: %u/%u\n",
			    sc->sc_dev.dv_xname, ppi->size,
			    pkt->pkt_info_length);
			break;
		}
		switch (ppi->type) {
		case tcpip_chksum_info:
			csum = (struct rndis_tcp_ip_csum_info *)
			    ((caddr_t)ppi + ppi->size);
			if (csum->recv.ip_csum_succeeded)
				m->m_pkthdr.csum_flags |= M_IPV4_CSUM_IN_OK;
			if (csum->recv.tcp_csum_succeeded)
				m->m_pkthdr.csum_flags |= M_TCP_CSUM_IN_OK;
			if (csum->recv.udp_csum_succeeded)
				m->m_pkthdr.csum_flags |= M_UDP_CSUM_IN_OK;
			break;
		case ieee_8021q_info:
			vlan = (struct ndis_8021q_info *)
			    ((caddr_t)ppi + ppi->size);
#if NVLAN > 0
			m->m_pkthdr.ether_vtag = vlan->vlan_id;
			m->m_flags |= M_VLANTAG;
#endif
			break;
		default:
			DPRINTF("%s: unhandled PPI %u\n", sc->sc_dev.dv_xname,
			    ppi->type);
		}
		pkt->pkt_info_length -= ppi->size;
	}

	ml_enqueue(ml, m);
}

void
hvn_rndis_complete(struct hvn_softc *sc, caddr_t buf, uint32_t len)
{
	struct rndis_cmd *rc;
	uint32_t id;

	memcpy(&id, buf + RNDIS_HEADER_SIZE, sizeof(id));
	if ((rc = hvn_complete_cmd(sc, id)) != NULL) {
		if (len < rc->rc_cmplen)
			printf("%s: RNDIS response %u too short: %u\n",
			    sc->sc_dev.dv_xname, id, len);
		else
			memcpy(&rc->rc_cmp, buf, rc->rc_cmplen);
		if (len > rc->rc_cmplen &&
		    len - rc->rc_cmplen > HVN_RNDIS_CMPBUFSZ)
			printf("%s: RNDIS response %u too large: %u\n",
			    sc->sc_dev.dv_xname, id, len);
		else if (len > rc->rc_cmplen)
			memcpy(&rc->rc_cmpbuf, buf + rc->rc_cmplen,
			    len - rc->rc_cmplen);
		wakeup_one(rc);
	} else
		DPRINTF("%s: failed to complete RNDIS request id %u\n",
		    sc->sc_dev.dv_xname, id);
}

int
hvn_rndis_output(struct hvn_softc *sc, struct hvn_tx_desc *txd)
{
	struct hvn_nvs_rndis *cmd = &txd->txd_cmd;
	int rv;

	cmd->nvs_type = HVN_NVS_TYPE_RNDIS;
	cmd->nvs_rndis_mtype = HVN_NVS_RNDIS_MTYPE_DATA;
	cmd->nvs_chim_idx = HVN_NVS_CHIM_IDX_INVALID;

	rv = hv_channel_send_sgl(sc->sc_chan, txd->txd_sgl, txd->txd_nsge,
	    &txd->txd_cmd, sizeof(*cmd), (uint64_t)txd->txd_id << 32);
	if (rv) {
		DPRINTF("%s: RNDIS data send error %d\n",
		    sc->sc_dev.dv_xname, rv);
		return (rv);
	}

	return (0);
}

void
hvn_rndis_status(struct hvn_softc *sc, caddr_t buf, uint32_t len)
{
	uint32_t sta;

	memcpy(&sta, buf + RNDIS_HEADER_SIZE, sizeof(sta));
	switch (sta) {
	case RNDIS_STATUS_MEDIA_CONNECT:
		sc->sc_link_state = LINK_STATE_UP;
		break;
	case RNDIS_STATUS_MEDIA_DISCONNECT:
		sc->sc_link_state = LINK_STATE_DOWN;
		break;
	/* Ignore these */
	case RNDIS_STATUS_OFFLOAD_CURRENT_CONFIG:
		return;
	default:
		DPRINTF("%s: unhandled status %#x\n", sc->sc_dev.dv_xname, sta);
		return;
	}
	KERNEL_LOCK();
	hvn_link_status(sc);
	KERNEL_UNLOCK();
}

int
hvn_rndis_query(struct hvn_softc *sc, uint32_t oid, void *res, size_t *length)
{
	struct rndis_cmd *rc;
	struct rndis_query_req *req;
	struct rndis_query_comp *cmp;
	size_t olength = *length;
	int rv;

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_QUERY_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req);
	rc->rc_cmplen = RNDIS_MESSAGE_SIZE(*cmp);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_query_req *)&rc->rc_req->msg;
	req->request_id = rc->rc_id;
	req->oid = oid;
	req->info_buffer_offset = sizeof(*req);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_cmd(sc, rc, 500)) != 0) {
		DPRINTF("%s: QUERY_MSG failed, error %d\n",
		    sc->sc_dev.dv_xname, rv);
		hvn_free_cmd(sc, rc);
		return (rv);
	}

	cmp = (struct rndis_query_comp *)&rc->rc_cmp.msg;
	switch (cmp->status) {
	case RNDIS_STATUS_SUCCESS:
		if (cmp->info_buffer_length > olength) {
			rv = EINVAL;
			break;
		}
		memcpy(res, rc->rc_cmpbuf, cmp->info_buffer_length);
		*length = cmp->info_buffer_length;
		break;
	default:
		*length = 0;
		rv = EIO;
	}

	hvn_free_cmd(sc, rc);

	return (rv);
}

int
hvn_rndis_set(struct hvn_softc *sc, uint32_t oid, void *data, size_t length)
{
	struct rndis_cmd *rc;
	struct rndis_set_req *req;
	struct rndis_set_comp *cmp;
	int rv;

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_SET_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req) + length;
	rc->rc_cmplen = RNDIS_MESSAGE_SIZE(*cmp);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_set_req *)&rc->rc_req->msg;
	memset(req, 0, sizeof(*req));
	req->request_id = rc->rc_id;
	req->oid = oid;
	req->info_buffer_offset = sizeof(*req);

	if (length > 0) {
		KASSERT(sizeof(*req) + length < sizeof(struct rndis));
		req->info_buffer_length = length;
		memcpy((caddr_t)(req + 1), data, length);
	}

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_cmd(sc, rc, 500)) != 0) {
		DPRINTF("%s: SET_MSG failed, error %u\n",
		    sc->sc_dev.dv_xname, rv);
		hvn_free_cmd(sc, rc);
		return (rv);
	}

	cmp = (struct rndis_set_comp *)&rc->rc_cmp.msg;
	if (cmp->status != RNDIS_STATUS_SUCCESS)
		rv = EIO;

	hvn_free_cmd(sc, rc);

	return (rv);
}

int
hvn_rndis_open(struct hvn_softc *sc)
{
	uint32_t filter;
	int rv;

	if (sc->sc_promisc)
		filter = NDIS_PACKET_TYPE_PROMISCUOUS;
	else
		filter = NDIS_PACKET_TYPE_BROADCAST |
		    NDIS_PACKET_TYPE_ALL_MULTICAST |
		    NDIS_PACKET_TYPE_DIRECTED;

	rv = hvn_rndis_set(sc, RNDIS_OID_GEN_CURRENT_PACKET_FILTER,
	    &filter, sizeof(filter));
	if (rv)
		DPRINTF("%s: failed to set RNDIS filter to %#x\n",
		    sc->sc_dev.dv_xname, filter);
	return (rv);
}

int
hvn_rndis_close(struct hvn_softc *sc)
{
	uint32_t filter = 0;
	int rv;

	rv = hvn_rndis_set(sc, RNDIS_OID_GEN_CURRENT_PACKET_FILTER,
	    &filter, sizeof(filter));
	if (rv)
		DPRINTF("%s: failed to clear RNDIS filter\n",
		    sc->sc_dev.dv_xname);
	return (rv);
}

void
hvn_rndis_detach(struct hvn_softc *sc)
{
	struct rndis_cmd *rc;
	struct rndis_halt_req *req;
	int rv;

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_HALT_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_halt_req *)&rc->rc_req->msg;
	req->request_id = rc->rc_id;

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_cmd(sc, rc, 500)) != 0)
		DPRINTF("%s: HALT_MSG failed, error %u\n",
		    sc->sc_dev.dv_xname, rv);

	hvn_free_cmd(sc, rc);
}
