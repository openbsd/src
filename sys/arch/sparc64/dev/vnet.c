/*	$OpenBSD: vnet.c,v 1.37 2015/01/25 21:42:13 kettenis Exp $	*/
/*
 * Copyright (c) 2009 Mark Kettenis
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <machine/hypervisor.h>
#include <machine/openfirm.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <uvm/uvm_extern.h>

#include <sparc64/dev/cbusvar.h>
#include <sparc64/dev/ldcvar.h>
#include <sparc64/dev/viovar.h>

#ifdef VNET_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

#define VNET_TX_ENTRIES		32
#define VNET_RX_ENTRIES		32

struct vnet_attr_info {
	struct vio_msg_tag	tag;
	uint8_t			xfer_mode;
	uint8_t			addr_type;
	uint16_t		ack_freq;
	uint32_t		_reserved1;
	uint64_t		addr;
	uint64_t		mtu;
	uint64_t		_reserved2[3];
};

/* Address types. */
#define VNET_ADDR_ETHERMAC	0x01

/* Sub-Type envelopes. */
#define VNET_MCAST_INFO		0x0101

#define VNET_NUM_MCAST		7

struct vnet_mcast_info {
	struct vio_msg_tag	tag;
	uint8_t			set;
	uint8_t			count;
	uint8_t			mcast_addr[VNET_NUM_MCAST][ETHER_ADDR_LEN];
	uint32_t		_reserved;
};

struct vnet_desc {
	struct vio_dring_hdr	hdr;
	uint32_t		nbytes;
	uint32_t		ncookies;
	struct ldc_cookie	cookie[2];
};

struct vnet_desc_msg {
	struct vio_msg_tag	tag;
	uint64_t		seq_no;
	uint64_t		desc_handle;
	uint32_t		nbytes;
	uint32_t		ncookies;
	struct ldc_cookie	cookie[1];
};

struct vnet_dring {
	bus_dmamap_t		vd_map;
	bus_dma_segment_t	vd_seg;
	struct vnet_desc	*vd_desc;
	int			vd_nentries;
};

struct vnet_dring *vnet_dring_alloc(bus_dma_tag_t, int);
void	vnet_dring_free(bus_dma_tag_t, struct vnet_dring *);

/*
 * For now, we only support vNet 1.0.
 */
#define VNET_MAJOR	1
#define VNET_MINOR	0

/*
 * The vNet protocol wants the IP header to be 64-bit aligned, so
 * define out own variant of ETHER_ALIGN.
 */
#define VNET_ETHER_ALIGN	6

struct vnet_soft_desc {
	int		vsd_map_idx;
	caddr_t		vsd_buf;
};

struct vnet_softc {
	struct device	sc_dv;
	bus_space_tag_t	sc_bustag;
	bus_dma_tag_t	sc_dmatag;

	uint64_t	sc_tx_ino;
	uint64_t	sc_rx_ino;
	void		*sc_tx_ih;
	void		*sc_rx_ih;

	struct ldc_conn	sc_lc;

	uint16_t	sc_vio_state;
#define VIO_SND_VER_INFO	0x0001
#define VIO_ACK_VER_INFO	0x0002
#define VIO_RCV_VER_INFO	0x0004
#define VIO_SND_ATTR_INFO	0x0008
#define VIO_ACK_ATTR_INFO	0x0010
#define VIO_RCV_ATTR_INFO	0x0020
#define VIO_SND_DRING_REG	0x0040
#define VIO_ACK_DRING_REG	0x0080
#define VIO_RCV_DRING_REG	0x0100
#define VIO_SND_RDX		0x0200
#define VIO_ACK_RDX		0x0400
#define VIO_RCV_RDX		0x0800

	struct timeout	sc_handshake_to;

	uint8_t		sc_xfer_mode;

	uint32_t	sc_local_sid;
	uint64_t	sc_dring_ident;
	uint64_t	sc_seq_no;

	int		sc_tx_cnt;
	int		sc_tx_prod;
	int		sc_tx_cons;

	uint8_t		sc_peer_state;

	struct ldc_map	*sc_lm;
	struct vnet_dring *sc_vd;
	struct vnet_soft_desc *sc_vsd;

	size_t		sc_peer_desc_size;
	struct ldc_cookie sc_peer_dring_cookie;
	int		sc_peer_dring_nentries;

	struct pool	sc_pool;

	struct arpcom	sc_ac;
	struct ifmedia	sc_media;
};

int	vnet_match(struct device *, void *, void *);
void	vnet_attach(struct device *, struct device *, void *);

struct cfattach vnet_ca = {
	sizeof(struct vnet_softc), vnet_match, vnet_attach
};

struct cfdriver vnet_cd = {
	NULL, "vnet", DV_IFNET
};

int	vnet_tx_intr(void *);
int	vnet_rx_intr(void *);
void	vnet_handshake(void *);

void	vio_rx_data(struct ldc_conn *, struct ldc_pkt *);
void	vnet_rx_vio_ctrl(struct vnet_softc *, struct vio_msg *);
void	vnet_rx_vio_ver_info(struct vnet_softc *, struct vio_msg_tag *);
void	vnet_rx_vio_attr_info(struct vnet_softc *, struct vio_msg_tag *);
void	vnet_rx_vio_dring_reg(struct vnet_softc *, struct vio_msg_tag *);
void	vnet_rx_vio_rdx(struct vnet_softc *sc, struct vio_msg_tag *);
void	vnet_rx_vio_data(struct vnet_softc *sc, struct vio_msg *);
void	vnet_rx_vio_desc_data(struct vnet_softc *sc, struct vio_msg_tag *);
void	vnet_rx_vio_dring_data(struct vnet_softc *sc, struct vio_msg_tag *);

void	vnet_ldc_reset(struct ldc_conn *);
void	vnet_ldc_start(struct ldc_conn *);

void	vnet_sendmsg(struct vnet_softc *, void *, size_t);
void	vnet_send_ver_info(struct vnet_softc *, uint16_t, uint16_t);
void	vnet_send_attr_info(struct vnet_softc *);
void	vnet_send_dring_reg(struct vnet_softc *);
void	vio_send_rdx(struct vnet_softc *);
void	vnet_send_dring_data(struct vnet_softc *, uint32_t);

void	vnet_start(struct ifnet *);
void	vnet_start_desc(struct ifnet *);
int	vnet_ioctl(struct ifnet *, u_long, caddr_t);
void	vnet_watchdog(struct ifnet *);

int	vnet_media_change(struct ifnet *);
void	vnet_media_status(struct ifnet *, struct ifmediareq *);

void	vnet_link_state(struct vnet_softc *sc);

void	vnet_setmulti(struct vnet_softc *, int);

void	vnet_init(struct ifnet *);
void	vnet_stop(struct ifnet *);

int
vnet_match(struct device *parent, void *match, void *aux)
{
	struct cbus_attach_args *ca = aux;

	if (strcmp(ca->ca_name, "network") == 0)
		return (1);

	return (0);
}

void
vnet_attach(struct device *parent, struct device *self, void *aux)
{
	struct vnet_softc *sc = (struct vnet_softc *)self;
	struct cbus_attach_args *ca = aux;
	struct ldc_conn *lc;
	struct ifnet *ifp;

	sc->sc_bustag = ca->ca_bustag;
	sc->sc_dmatag = ca->ca_dmatag;
	sc->sc_tx_ino = ca->ca_tx_ino;
	sc->sc_rx_ino = ca->ca_rx_ino;

	printf(": ivec 0x%llx, 0x%llx", sc->sc_tx_ino, sc->sc_rx_ino);

	/*
	 * Un-configure queues before registering interrupt handlers,
	 * such that we dont get any stale LDC packets or events.
	 */
	hv_ldc_tx_qconf(ca->ca_id, 0, 0);
	hv_ldc_rx_qconf(ca->ca_id, 0, 0);

	sc->sc_tx_ih = bus_intr_establish(ca->ca_bustag, sc->sc_tx_ino,
	    IPL_NET, 0, vnet_tx_intr, sc, sc->sc_dv.dv_xname);
	sc->sc_rx_ih = bus_intr_establish(ca->ca_bustag, sc->sc_rx_ino,
	    IPL_NET, 0, vnet_rx_intr, sc, sc->sc_dv.dv_xname);
	if (sc->sc_tx_ih == NULL || sc->sc_rx_ih == NULL) {
		printf(", can't establish interrupt\n");
		return;
	}

	lc = &sc->sc_lc;
	lc->lc_id = ca->ca_id;
	lc->lc_sc = sc;
	lc->lc_reset = vnet_ldc_reset;
	lc->lc_start = vnet_ldc_start;
	lc->lc_rx_data = vio_rx_data;

	timeout_set(&sc->sc_handshake_to, vnet_handshake, sc);

	lc->lc_txq = ldc_queue_alloc(sc->sc_dmatag, VNET_TX_ENTRIES);
	if (lc->lc_txq == NULL) {
		printf(", can't allocate tx queue\n");
		return;
	}

	lc->lc_rxq = ldc_queue_alloc(sc->sc_dmatag, VNET_RX_ENTRIES);
	if (lc->lc_rxq == NULL) {
		printf(", can't allocate rx queue\n");
		goto free_txqueue;
	}

	if (OF_getprop(ca->ca_node, "local-mac-address",
	    sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN) > 0)
		printf(", address %s", ether_sprintf(sc->sc_ac.ac_enaddr));

	/*
	 * Each interface gets its own pool.
	 */
	pool_init(&sc->sc_pool, 2048, 0, 0, 0, sc->sc_dv.dv_xname, NULL);

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vnet_ioctl;
	ifp->if_start = vnet_start;
	ifp->if_watchdog = vnet_watchdog;
	strlcpy(ifp->if_xname, sc->sc_dv.dv_xname, IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, 31); /* XXX */
	IFQ_SET_READY(&ifp->if_snd);

	ifmedia_init(&sc->sc_media, 0, vnet_media_change, vnet_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	printf("\n");
	return;

free_txqueue:
	ldc_queue_free(sc->sc_dmatag, lc->lc_txq);
}

int
vnet_tx_intr(void *arg)
{
	struct vnet_softc *sc = arg;
	struct ldc_conn *lc = &sc->sc_lc;
	uint64_t tx_head, tx_tail, tx_state;

	hv_ldc_tx_get_state(lc->lc_id, &tx_head, &tx_tail, &tx_state);
	if (tx_state != lc->lc_tx_state) {
		switch (tx_state) {
		case LDC_CHANNEL_DOWN:
			DPRINTF(("Tx link down\n"));
			break;
		case LDC_CHANNEL_UP:
			DPRINTF(("Tx link up\n"));
			break;
		case LDC_CHANNEL_RESET:
			DPRINTF(("Tx link reset\n"));
			break;
		}
		lc->lc_tx_state = tx_state;
	}

	return (1);
}

int
vnet_rx_intr(void *arg)
{
	struct vnet_softc *sc = arg;
	struct ldc_conn *lc = &sc->sc_lc;
	uint64_t rx_head, rx_tail, rx_state;
	struct ldc_pkt *lp;
	int err;

	err = hv_ldc_rx_get_state(lc->lc_id, &rx_head, &rx_tail, &rx_state);
	if (err == H_EINVAL)
		return (0);
	if (err != H_EOK) {
		printf("hv_ldc_rx_get_state %d\n", err);
		return (0);
	}

	if (rx_state != lc->lc_rx_state) {
		switch (rx_state) {
		case LDC_CHANNEL_DOWN:
			DPRINTF(("Rx link down\n"));
			lc->lc_tx_seqid = 0;
			lc->lc_state = 0;
			lc->lc_reset(lc);
			break;
		case LDC_CHANNEL_UP:
			DPRINTF(("Rx link up\n"));
			timeout_add_msec(&sc->sc_handshake_to, 500);
			break;
		case LDC_CHANNEL_RESET:
			DPRINTF(("Rx link reset\n"));
			lc->lc_tx_seqid = 0;
			lc->lc_state = 0;
			lc->lc_reset(lc);
			break;
		}
		lc->lc_rx_state = rx_state;
		return (1);
	}

	if (rx_head == rx_tail)
		return (0);

	lp = (struct ldc_pkt *)(lc->lc_rxq->lq_va + rx_head);
	switch (lp->type) {
	case LDC_CTRL:
		ldc_rx_ctrl(lc, lp);
		break;

	case LDC_DATA:
		ldc_rx_data(lc, lp);
		break;

	default:
		DPRINTF(("%0x02/%0x02/%0x02\n", lp->type, lp->stype,
		    lp->ctrl));
		ldc_reset(lc);
		break;
	}

	if (lc->lc_state == 0)
		return (1);

	rx_head += sizeof(*lp);
	rx_head &= ((lc->lc_rxq->lq_nentries * sizeof(*lp)) - 1);
	err = hv_ldc_rx_set_qhead(lc->lc_id, rx_head);
	if (err != H_EOK)
		printf("%s: hv_ldc_rx_set_qhead %d\n", __func__, err);

	return (1);
}

void
vnet_handshake(void *arg)
{
	struct vnet_softc *sc = arg;

	ldc_send_vers(&sc->sc_lc);
}

void
vio_rx_data(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	struct vio_msg *vm = (struct vio_msg *)lp;

	switch (vm->type) {
	case VIO_TYPE_CTRL:
		if ((lp->env & LDC_FRAG_START) == 0 &&
		    (lp->env & LDC_FRAG_STOP) == 0)
			return;
		vnet_rx_vio_ctrl(lc->lc_sc, vm);
		break;

	case VIO_TYPE_DATA:
		if((lp->env & LDC_FRAG_START) == 0)
			return;
		vnet_rx_vio_data(lc->lc_sc, vm);
		break;

	default:
		DPRINTF(("Unhandled packet type 0x%02x\n", vm->type));
		ldc_reset(lc);
		break;
	}
}

void
vnet_rx_vio_ctrl(struct vnet_softc *sc, struct vio_msg *vm)
{
	struct vio_msg_tag *tag = (struct vio_msg_tag *)&vm->type;

	switch (tag->stype_env) {
	case VIO_VER_INFO:
		vnet_rx_vio_ver_info(sc, tag);
		break;
	case VIO_ATTR_INFO:
		vnet_rx_vio_attr_info(sc, tag);
		break;
	case VIO_DRING_REG:
		vnet_rx_vio_dring_reg(sc, tag);
		break;
	case VIO_RDX:
		vnet_rx_vio_rdx(sc, tag);
		break;
	default:
		DPRINTF(("CTRL/0x%02x/0x%04x\n", tag->stype, tag->stype_env));
		break;
	}
}

void
vnet_rx_vio_ver_info(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct vio_ver_info *vi = (struct vio_ver_info *)tag;

	switch (vi->tag.stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/VER_INFO\n"));

		/* Make sure we're talking to a virtual network device. */
		if (vi->dev_class != VDEV_NETWORK &&
		    vi->dev_class != VDEV_NETWORK_SWITCH) {
			/* Huh, we're not talking to a network device? */
			printf("Not a network device\n");
			vi->tag.stype = VIO_SUBTYPE_NACK;
			vnet_sendmsg(sc, vi, sizeof(*vi));
			return;
		}

		if (vi->major != VNET_MAJOR) {
			vi->tag.stype = VIO_SUBTYPE_NACK;
			vi->major = VNET_MAJOR;
			vi->minor = VNET_MINOR;
			vnet_sendmsg(sc, vi, sizeof(*vi));
			return;
		}

		vi->tag.stype = VIO_SUBTYPE_ACK;
		vi->tag.sid = sc->sc_local_sid;
		vi->minor = VNET_MINOR;
		vnet_sendmsg(sc, vi, sizeof(*vi));
		sc->sc_vio_state |= VIO_RCV_VER_INFO;
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/VER_INFO\n"));
		if (!ISSET(sc->sc_vio_state, VIO_SND_VER_INFO)) {
			ldc_reset(&sc->sc_lc);
			break;
		}
		sc->sc_vio_state |= VIO_ACK_VER_INFO;
		break;

	default:
		DPRINTF(("CTRL/0x%02x/VER_INFO\n", vi->tag.stype));
		break;
	}

	if (ISSET(sc->sc_vio_state, VIO_RCV_VER_INFO) &&
	    ISSET(sc->sc_vio_state, VIO_ACK_VER_INFO))
		vnet_send_attr_info(sc);
}

void
vnet_rx_vio_attr_info(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct vnet_attr_info *ai = (struct vnet_attr_info *)tag;

	switch (ai->tag.stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/ATTR_INFO\n"));
		sc->sc_xfer_mode = ai->xfer_mode;

		ai->tag.stype = VIO_SUBTYPE_ACK;
		ai->tag.sid = sc->sc_local_sid;
		vnet_sendmsg(sc, ai, sizeof(*ai));
		sc->sc_vio_state |= VIO_RCV_ATTR_INFO;
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/ATTR_INFO\n"));
		if (!ISSET(sc->sc_vio_state, VIO_SND_ATTR_INFO)) {
			ldc_reset(&sc->sc_lc);
			break;
		}
		sc->sc_vio_state |= VIO_ACK_ATTR_INFO;
		break;

	default:
		DPRINTF(("CTRL/0x%02x/ATTR_INFO\n", ai->tag.stype));
		break;
	}

	if (ISSET(sc->sc_vio_state, VIO_RCV_ATTR_INFO) &&
	    ISSET(sc->sc_vio_state, VIO_ACK_ATTR_INFO)) {
		if (sc->sc_xfer_mode == VIO_DRING_MODE)
			vnet_send_dring_reg(sc);
		else
			vio_send_rdx(sc);
	}
}

void
vnet_rx_vio_dring_reg(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct vio_dring_reg *dr = (struct vio_dring_reg *)tag;

	switch (dr->tag.stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/DRING_REG\n"));

		sc->sc_peer_dring_nentries = dr->num_descriptors;
		sc->sc_peer_desc_size = dr->descriptor_size;
		sc->sc_peer_dring_cookie = dr->cookie[0];

		dr->tag.stype = VIO_SUBTYPE_ACK;
		dr->tag.sid = sc->sc_local_sid;
		vnet_sendmsg(sc, dr, sizeof(*dr));
		sc->sc_vio_state |= VIO_RCV_DRING_REG;
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/DRING_REG\n"));
		if (!ISSET(sc->sc_vio_state, VIO_SND_DRING_REG)) {
			ldc_reset(&sc->sc_lc);
			break;
		}

		sc->sc_dring_ident = dr->dring_ident;
		sc->sc_seq_no = 1;

		sc->sc_vio_state |= VIO_ACK_DRING_REG;
		break;

	default:
		DPRINTF(("CTRL/0x%02x/DRING_REG\n", dr->tag.stype));
		break;
	}

	if (ISSET(sc->sc_vio_state, VIO_RCV_DRING_REG) &&
	    ISSET(sc->sc_vio_state, VIO_ACK_DRING_REG))
		vio_send_rdx(sc);
}

void
vnet_rx_vio_rdx(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	switch(tag->stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/RDX\n"));

		tag->stype = VIO_SUBTYPE_ACK;
		tag->sid = sc->sc_local_sid;
		vnet_sendmsg(sc, tag, sizeof(*tag));
		sc->sc_vio_state |= VIO_RCV_RDX;
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/RDX\n"));
		if (!ISSET(sc->sc_vio_state, VIO_SND_RDX)) {
			ldc_reset(&sc->sc_lc);
			break;
		}
		sc->sc_vio_state |= VIO_ACK_RDX;
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RDX (VIO)\n", tag->stype));
		break;
	}

	if (ISSET(sc->sc_vio_state, VIO_RCV_RDX) &&
	    ISSET(sc->sc_vio_state, VIO_ACK_RDX)) {
		/* Link is up! */
		vnet_link_state(sc);

		/* Configure multicast now that we can. */
		vnet_setmulti(sc, 1);
		vnet_start(ifp);
	}
}

void
vnet_rx_vio_data(struct vnet_softc *sc, struct vio_msg *vm)
{
	struct vio_msg_tag *tag = (struct vio_msg_tag *)&vm->type;

	if (!ISSET(sc->sc_vio_state, VIO_RCV_RDX) ||
	    !ISSET(sc->sc_vio_state, VIO_ACK_RDX)) {
		DPRINTF(("Spurious DATA/0x%02x/0x%04x\n", tag->stype,
		    tag->stype_env));
		return;
	}

	switch(tag->stype_env) {
	case VIO_DESC_DATA:
		vnet_rx_vio_desc_data(sc, tag);
		break;

	case VIO_DRING_DATA:
		vnet_rx_vio_dring_data(sc, tag);
		break;

	default:
		DPRINTF(("DATA/0x%02x/0x%04x\n", tag->stype, tag->stype_env));
		break;
	}
}

void
vnet_rx_vio_desc_data(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct vnet_desc_msg *dm = (struct vnet_desc_msg *)tag;
	struct ldc_conn *lc = &sc->sc_lc;
	struct ldc_map *map = sc->sc_lm;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf *m;
	caddr_t buf;
	paddr_t pa;
	psize_t nbytes;
	int err;

	switch(tag->stype) {
	case VIO_SUBTYPE_INFO:
		buf = pool_get(&sc->sc_pool, PR_NOWAIT|PR_ZERO);
		if (buf == NULL) {
			ifp->if_ierrors++;
			goto skip;
		}
		nbytes = roundup(dm->nbytes, 8);

		pmap_extract(pmap_kernel(), (vaddr_t)buf, &pa);
		err = hv_ldc_copy(lc->lc_id, LDC_COPY_IN,
		    dm->cookie[0].addr, pa, nbytes, &nbytes);
		if (err != H_EOK) {
			pool_put(&sc->sc_pool, buf);
			ifp->if_ierrors++;
			goto skip;
		}

		/* Stupid OBP doesn't align properly. */
                m = m_devget(buf, dm->nbytes, ETHER_ALIGN, ifp);
		pool_put(&sc->sc_pool, buf);
		if (m == NULL) {
			ifp->if_ierrors++;
			goto skip;
		}

		ifp->if_ipackets++;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif /* NBPFILTER > 0 */

		/* Pass it on. */
		ether_input_mbuf(ifp, m);

	skip:
		dm->tag.stype = VIO_SUBTYPE_ACK;
		dm->tag.sid = sc->sc_local_sid;
		vnet_sendmsg(sc, dm, sizeof(*dm));
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("DATA/ACK/DESC_DATA\n"));

		if (dm->desc_handle != sc->sc_tx_cons) {
			printf("out of order\n");
			return;
		}

		map->lm_slot[sc->sc_vsd[sc->sc_tx_cons].vsd_map_idx].entry = 0;
		map->lm_count--;

		pool_put(&sc->sc_pool, sc->sc_vsd[sc->sc_tx_cons].vsd_buf);

		sc->sc_tx_cons++;
		sc->sc_tx_cons &= (sc->sc_vd->vd_nentries - 1);
		sc->sc_tx_cnt--;
		break;

	case VIO_SUBTYPE_NACK:
		DPRINTF(("DATA/NACK/DESC_DATA\n"));
		break;

	default:
		DPRINTF(("DATA/0x%02x/DESC_DATA\n", tag->stype));
		break;
	}
}

void
vnet_rx_vio_dring_data(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct vio_dring_msg *dm = (struct vio_dring_msg *)tag;
	struct ldc_conn *lc = &sc->sc_lc;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf *m;
	paddr_t pa;
	psize_t nbytes;
	int err;

	switch(tag->stype) {
	case VIO_SUBTYPE_INFO:
	{
		struct vnet_desc desc;
		uint64_t cookie;
		paddr_t desc_pa;
		int idx, ack_end_idx = -1;

		idx = dm->start_idx;
		for (;;) {
			cookie = sc->sc_peer_dring_cookie.addr;
			cookie += idx * sc->sc_peer_desc_size;
			nbytes = sc->sc_peer_desc_size;
			pmap_extract(pmap_kernel(), (vaddr_t)&desc, &desc_pa);
			err = hv_ldc_copy(lc->lc_id, LDC_COPY_IN, cookie,
			    desc_pa, nbytes, &nbytes);
			if (err != H_EOK) {
				printf("hv_ldc_copy_in %d\n", err);
				break;
			}

			if (desc.hdr.dstate != VIO_DESC_READY)
				break;

			m = MCLGETI(NULL, M_DONTWAIT, NULL, desc.nbytes);
			if (!m)
				break;
			ifp->if_ipackets++;
			m->m_pkthdr.rcvif = ifp;
			m->m_len = m->m_pkthdr.len = desc.nbytes;
			nbytes = roundup(desc.nbytes + VNET_ETHER_ALIGN, 8);

			pmap_extract(pmap_kernel(), (vaddr_t)m->m_data, &pa);
			err = hv_ldc_copy(lc->lc_id, LDC_COPY_IN,
			    desc.cookie[0].addr, pa, nbytes, &nbytes);
			if (err != H_EOK) {
				m_freem(m);
				goto skip;
			}
			m->m_data += VNET_ETHER_ALIGN;

#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif /* NBPFILTER > 0 */

			/* Pass it on. */
			ether_input_mbuf(ifp, m);

		skip:
			desc.hdr.dstate = VIO_DESC_DONE;
			nbytes = sc->sc_peer_desc_size;
			err = hv_ldc_copy(lc->lc_id, LDC_COPY_OUT, cookie,
			    desc_pa, nbytes, &nbytes);
			if (err != H_EOK)
				printf("hv_ldc_copy_out %d\n", err);

			ack_end_idx = idx;
			if (++idx == sc->sc_peer_dring_nentries)
				idx = 0;
		}

		if (ack_end_idx == -1) {
			dm->tag.stype = VIO_SUBTYPE_NACK;
		} else {
			dm->tag.stype = VIO_SUBTYPE_ACK;
			dm->end_idx = ack_end_idx;
		}
		dm->tag.sid = sc->sc_local_sid;
		dm->proc_state = VIO_DP_STOPPED;
		vnet_sendmsg(sc, dm, sizeof(*dm));
		break;
	}

	case VIO_SUBTYPE_ACK:
	{
		struct ldc_map *map = sc->sc_lm;
		int cons;

		sc->sc_peer_state = dm->proc_state;

		cons = sc->sc_tx_cons;
		while (sc->sc_vd->vd_desc[cons].hdr.dstate == VIO_DESC_DONE) {
			map->lm_slot[sc->sc_vsd[cons].vsd_map_idx].entry = 0;
			map->lm_count--;

			pool_put(&sc->sc_pool, sc->sc_vsd[cons].vsd_buf);

			sc->sc_vd->vd_desc[cons++].hdr.dstate = VIO_DESC_FREE;
			cons &= (sc->sc_vd->vd_nentries - 1);
			sc->sc_tx_cnt--;
		}
		sc->sc_tx_cons = cons;

		if (sc->sc_tx_cnt > 0 && sc->sc_peer_state != VIO_DP_ACTIVE)
			vnet_send_dring_data(sc, sc->sc_tx_cons);

		if (sc->sc_tx_cnt < sc->sc_vd->vd_nentries)
			ifp->if_flags &= ~IFF_OACTIVE;
		if (sc->sc_tx_cnt == 0)
			ifp->if_timer = 0;

		vnet_start(ifp);
		break;
	}

	case VIO_SUBTYPE_NACK:
		DPRINTF(("DATA/NACK/DRING_DATA\n"));
		break;

	default:
		DPRINTF(("DATA/0x%02x/DRING_DATA\n", tag->stype));
		break;
	}
}

void
vnet_ldc_reset(struct ldc_conn *lc)
{
	struct vnet_softc *sc = lc->lc_sc;

	timeout_del(&sc->sc_handshake_to);
	sc->sc_tx_cnt = sc->sc_tx_prod = sc->sc_tx_cons = 0;
	sc->sc_vio_state = 0;
	vnet_link_state(sc);
}

void
vnet_ldc_start(struct ldc_conn *lc)
{
	struct vnet_softc *sc = lc->lc_sc;

	timeout_del(&sc->sc_handshake_to);
	vnet_send_ver_info(sc, VNET_MAJOR, VNET_MINOR);
}

void
vnet_sendmsg(struct vnet_softc *sc, void *msg, size_t len)
{
	struct ldc_conn *lc = &sc->sc_lc;
	int err;

	err = ldc_send_unreliable(lc, msg, len);
	if (err)
		printf("%s: ldc_send_unreliable: %d\n", __func__, err);
}

void
vnet_send_ver_info(struct vnet_softc *sc, uint16_t major, uint16_t minor)
{
	struct vio_ver_info vi;

	bzero(&vi, sizeof(vi));
	vi.tag.type = VIO_TYPE_CTRL;
	vi.tag.stype = VIO_SUBTYPE_INFO;
	vi.tag.stype_env = VIO_VER_INFO;
	vi.tag.sid = sc->sc_local_sid;
	vi.major = major;
	vi.minor = minor;
	vi.dev_class = VDEV_NETWORK;
	vnet_sendmsg(sc, &vi, sizeof(vi));

	sc->sc_vio_state |= VIO_SND_VER_INFO;
}

void
vnet_send_attr_info(struct vnet_softc *sc)
{
	struct vnet_attr_info ai;
	int i;

	bzero(&ai, sizeof(ai));
	ai.tag.type = VIO_TYPE_CTRL;
	ai.tag.stype = VIO_SUBTYPE_INFO;
	ai.tag.stype_env = VIO_ATTR_INFO;
	ai.tag.sid = sc->sc_local_sid;
	ai.xfer_mode = VIO_DRING_MODE;
	ai.addr_type = VNET_ADDR_ETHERMAC;
	ai.ack_freq = 0;
	ai.addr = 0;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		ai.addr <<= 8;
		ai.addr |= sc->sc_ac.ac_enaddr[i];
	}
	ai.mtu = ETHER_MAX_LEN - ETHER_CRC_LEN;
	vnet_sendmsg(sc, &ai, sizeof(ai));

	sc->sc_vio_state |= VIO_SND_ATTR_INFO;
}

void
vnet_send_dring_reg(struct vnet_softc *sc)
{
	struct vio_dring_reg dr;

	bzero(&dr, sizeof(dr));
	dr.tag.type = VIO_TYPE_CTRL;
	dr.tag.stype = VIO_SUBTYPE_INFO;
	dr.tag.stype_env = VIO_DRING_REG;
	dr.tag.sid = sc->sc_local_sid;
	dr.dring_ident = 0;
	dr.num_descriptors = sc->sc_vd->vd_nentries;
	dr.descriptor_size = sizeof(struct vnet_desc);
	dr.options = VIO_TX_RING;
	dr.ncookies = 1;
	dr.cookie[0].addr = 0;
	dr.cookie[0].size = PAGE_SIZE;
	vnet_sendmsg(sc, &dr, sizeof(dr));

	sc->sc_vio_state |= VIO_SND_DRING_REG;
};

void
vio_send_rdx(struct vnet_softc *sc)
{
	struct vio_msg_tag tag;

	tag.type = VIO_TYPE_CTRL;
	tag.stype = VIO_SUBTYPE_INFO;
	tag.stype_env = VIO_RDX;
	tag.sid = sc->sc_local_sid;
	vnet_sendmsg(sc, &tag, sizeof(tag));

	sc->sc_vio_state |= VIO_SND_RDX;
}

void
vnet_send_dring_data(struct vnet_softc *sc, uint32_t start_idx)
{
	struct vio_dring_msg dm;

	bzero(&dm, sizeof(dm));
	dm.tag.type = VIO_TYPE_DATA;
	dm.tag.stype = VIO_SUBTYPE_INFO;
	dm.tag.stype_env = VIO_DRING_DATA;
	dm.tag.sid = sc->sc_local_sid;
	dm.seq_no = sc->sc_seq_no++;
	dm.dring_ident = sc->sc_dring_ident;
	dm.start_idx = start_idx;
	dm.end_idx = -1;
	vnet_sendmsg(sc, &dm, sizeof(dm));

	sc->sc_peer_state = VIO_DP_ACTIVE;
}

void
vnet_start(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;
	struct ldc_conn *lc = &sc->sc_lc;
	struct ldc_map *map = sc->sc_lm;
	struct mbuf *m;
	paddr_t pa;
	caddr_t buf;
	uint64_t tx_head, tx_tail, tx_state;
	int err, desc;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	/*
	 * We cannot transmit packets until a VIO connection has been
	 * established.
	 */
	if (!ISSET(sc->sc_vio_state, VIO_RCV_RDX) ||
	    !ISSET(sc->sc_vio_state, VIO_ACK_RDX))
		return;

	/*
	 * Make sure there is room in the LDC transmit queue to send a
	 * DRING_DATA message.
	 */
	err = hv_ldc_tx_get_state(lc->lc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK)
		return;
	tx_tail += sizeof(struct ldc_pkt);
	tx_tail &= ((lc->lc_txq->lq_nentries * sizeof(struct ldc_pkt)) - 1);
	if (tx_tail == tx_head) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	if (sc->sc_xfer_mode == VIO_DESC_MODE) {
		vnet_start_desc(ifp);
		return;
	}

	desc = sc->sc_tx_prod;
	while (sc->sc_vd->vd_desc[desc].hdr.dstate == VIO_DESC_FREE) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (sc->sc_tx_cnt >= sc->sc_vd->vd_nentries ||
		    map->lm_count >= map->lm_nentries) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		buf = pool_get(&sc->sc_pool, PR_NOWAIT|PR_ZERO);
		if (buf == NULL) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		m_copydata(m, 0, m->m_pkthdr.len, buf + VNET_ETHER_ALIGN);
		IFQ_DEQUEUE(&ifp->if_snd, m);

#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		pmap_extract(pmap_kernel(), (vaddr_t)buf, &pa);
		KASSERT((pa & ~PAGE_MASK) == (pa & LDC_MTE_RA_MASK));
		while (map->lm_slot[map->lm_next].entry != 0) {
			map->lm_next++;
			map->lm_next &= (map->lm_nentries - 1);
		}
		map->lm_slot[map->lm_next].entry = (pa & LDC_MTE_RA_MASK);
		map->lm_slot[map->lm_next].entry |= LDC_MTE_CPR;
		map->lm_count++;

		sc->sc_vd->vd_desc[desc].nbytes = max(m->m_pkthdr.len, 60);
		sc->sc_vd->vd_desc[desc].ncookies = 1;
		sc->sc_vd->vd_desc[desc].cookie[0].addr =
		    map->lm_next << PAGE_SHIFT | (pa & PAGE_MASK);
		sc->sc_vd->vd_desc[desc].cookie[0].size = 2048;
		membar(Sync);
		sc->sc_vd->vd_desc[desc].hdr.dstate = VIO_DESC_READY;

		sc->sc_vsd[desc].vsd_map_idx = map->lm_next;
		sc->sc_vsd[desc].vsd_buf = buf;

		desc++;
		desc &= (sc->sc_vd->vd_nentries - 1);
		sc->sc_tx_cnt++;

		m_freem(m);
	}

	if (sc->sc_tx_cnt > 0 && sc->sc_peer_state != VIO_DP_ACTIVE) {
		vnet_send_dring_data(sc, sc->sc_tx_prod);
		ifp->if_timer = 5;
	}

	sc->sc_tx_prod = desc;
}

void
vnet_start_desc(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;
	struct ldc_map *map = sc->sc_lm;
	struct vnet_desc_msg dm;
	struct mbuf *m;
	paddr_t pa;
	caddr_t buf;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (sc->sc_tx_cnt >= sc->sc_vd->vd_nentries ||
		    map->lm_count >= map->lm_nentries) {
			ifp->if_flags |= IFF_OACTIVE;
			return;
		}

		buf = pool_get(&sc->sc_pool, PR_NOWAIT|PR_ZERO);
		if (buf == NULL) {
			ifp->if_flags |= IFF_OACTIVE;
			return;
		}
		m_copydata(m, 0, m->m_pkthdr.len, buf);
		IFQ_DEQUEUE(&ifp->if_snd, m);

#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		pmap_extract(pmap_kernel(), (vaddr_t)buf, &pa);
		KASSERT((pa & ~PAGE_MASK) == (pa & LDC_MTE_RA_MASK));
		while (map->lm_slot[map->lm_next].entry != 0) {
			map->lm_next++;
			map->lm_next &= (map->lm_nentries - 1);
		}
		map->lm_slot[map->lm_next].entry = (pa & LDC_MTE_RA_MASK);
		map->lm_slot[map->lm_next].entry |= LDC_MTE_CPR;
		map->lm_count++;

		sc->sc_vsd[sc->sc_tx_prod].vsd_map_idx = map->lm_next;
		sc->sc_vsd[sc->sc_tx_prod].vsd_buf = buf;

		bzero(&dm, sizeof(dm));
		dm.tag.type = VIO_TYPE_DATA;
		dm.tag.stype = VIO_SUBTYPE_INFO;
		dm.tag.stype_env = VIO_DESC_DATA;
		dm.tag.sid = sc->sc_local_sid;
		dm.seq_no = sc->sc_seq_no++;
		dm.desc_handle = sc->sc_tx_prod;
		dm.nbytes = max(m->m_pkthdr.len, 60);
		dm.ncookies = 1;
		dm.cookie[0].addr =
			map->lm_next << PAGE_SHIFT | (pa & PAGE_MASK);
		dm.cookie[0].size = 2048;
		vnet_sendmsg(sc, &dm, sizeof(dm));

		sc->sc_tx_prod++;
		sc->sc_tx_prod &= (sc->sc_vd->vd_nentries - 1);
		sc->sc_tx_cnt++;

		m_freem(m);
	}
}

int
vnet_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vnet_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				vnet_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				vnet_stop(ifp);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * XXX Removing all multicast addresses and adding
		 * most of them back, is somewhat retarded.
		 */
		vnet_setmulti(sc, 0);
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		vnet_setmulti(sc, 1);
		if (error == ENETRESET)
			error = 0;
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	splx(s);
	return (error);
}

void
vnet_watchdog(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;

	printf("%s: watchdog timeout\n", sc->sc_dv.dv_xname);
}

int
vnet_media_change(struct ifnet *ifp)
{
	return (0);
}

void
vnet_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID;

	if (LINK_STATE_IS_UP(ifp->if_link_state) &&
	    ifp->if_flags & IFF_UP)
		imr->ifm_status |= IFM_ACTIVE;
}

void
vnet_link_state(struct vnet_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int link_state = LINK_STATE_DOWN;

	if (ISSET(sc->sc_vio_state, VIO_RCV_RDX) &&
	    ISSET(sc->sc_vio_state, VIO_ACK_RDX))
		link_state = LINK_STATE_FULL_DUPLEX;
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}

void
vnet_setmulti(struct vnet_softc *sc, int set)
{
	struct arpcom *ac = &sc->sc_ac;
	struct ether_multi *enm;
	struct ether_multistep step;
	struct vnet_mcast_info mi;
	int count = 0;

	if (!ISSET(sc->sc_vio_state, VIO_RCV_RDX) ||
	    !ISSET(sc->sc_vio_state, VIO_ACK_RDX))
		return;

	bzero(&mi, sizeof(mi));
	mi.tag.type = VIO_TYPE_CTRL;
	mi.tag.stype = VIO_SUBTYPE_INFO;
	mi.tag.stype_env = VNET_MCAST_INFO;
	mi.tag.sid = sc->sc_local_sid;
	mi.set = set ? 1 : 0;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		/* XXX What about multicast ranges? */
		bcopy(enm->enm_addrlo, mi.mcast_addr[count], ETHER_ADDR_LEN);
		ETHER_NEXT_MULTI(step, enm);

		count++;
		if (count < VNET_NUM_MCAST)
			continue;

		mi.count = VNET_NUM_MCAST;
		vnet_sendmsg(sc, &mi, sizeof(mi));
		count = 0;
	}

	if (count > 0) {
		mi.count = count;
		vnet_sendmsg(sc, &mi, sizeof(mi));
	}
}

void
vnet_init(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;
	struct ldc_conn *lc = &sc->sc_lc;
	int err;

	sc->sc_lm = ldc_map_alloc(sc->sc_dmatag, 2048);
	if (sc->sc_lm == NULL)
		return;

	err = hv_ldc_set_map_table(lc->lc_id,
	    sc->sc_lm->lm_map->dm_segs[0].ds_addr, sc->sc_lm->lm_nentries);
	if (err != H_EOK) {
		printf("hv_ldc_set_map_table %d\n", err);
		return;
	}

	sc->sc_vd = vnet_dring_alloc(sc->sc_dmatag, 128);
	if (sc->sc_vd == NULL)
		return;
	sc->sc_vsd = malloc(128 * sizeof(*sc->sc_vsd), M_DEVBUF, M_NOWAIT);
	if (sc->sc_vsd == NULL)
		return;

	sc->sc_lm->lm_slot[0].entry = sc->sc_vd->vd_map->dm_segs[0].ds_addr;
	sc->sc_lm->lm_slot[0].entry &= LDC_MTE_RA_MASK;
	sc->sc_lm->lm_slot[0].entry |= LDC_MTE_CPR | LDC_MTE_CPW;
	sc->sc_lm->lm_next = 1;
	sc->sc_lm->lm_count = 1;

	err = hv_ldc_tx_qconf(lc->lc_id,
	    lc->lc_txq->lq_map->dm_segs[0].ds_addr, lc->lc_txq->lq_nentries);
	if (err != H_EOK)
		printf("hv_ldc_tx_qconf %d\n", err);

	err = hv_ldc_rx_qconf(lc->lc_id,
	    lc->lc_rxq->lq_map->dm_segs[0].ds_addr, lc->lc_rxq->lq_nentries);
	if (err != H_EOK)
		printf("hv_ldc_rx_qconf %d\n", err);

	cbus_intr_setenabled(sc->sc_bustag, sc->sc_tx_ino, INTR_ENABLED);
	cbus_intr_setenabled(sc->sc_bustag, sc->sc_rx_ino, INTR_ENABLED);

	ldc_send_vers(lc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
}

void
vnet_stop(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;
	struct ldc_conn *lc = &sc->sc_lc;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	cbus_intr_setenabled(sc->sc_bustag, sc->sc_tx_ino, INTR_DISABLED);
	cbus_intr_setenabled(sc->sc_bustag, sc->sc_rx_ino, INTR_DISABLED);

	hv_ldc_tx_qconf(lc->lc_id, 0, 0);
	hv_ldc_rx_qconf(lc->lc_id, 0, 0);
	lc->lc_tx_state = lc->lc_rx_state = LDC_CHANNEL_DOWN;

	vnet_dring_free(sc->sc_dmatag, sc->sc_vd);

	hv_ldc_set_map_table(lc->lc_id, 0, 0);
	ldc_map_free(sc->sc_dmatag, sc->sc_lm);
}

struct vnet_dring *
vnet_dring_alloc(bus_dma_tag_t t, int nentries)
{
	struct vnet_dring *vd;
	bus_size_t size;
	caddr_t va;
	int nsegs;
	int i;

	vd = malloc(sizeof(struct vnet_dring), M_DEVBUF, M_NOWAIT);
	if (vd == NULL)
		return NULL;

	size = roundup(nentries * sizeof(struct vnet_desc), PAGE_SIZE);

	if (bus_dmamap_create(t, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &vd->vd_map) != 0)
		return (NULL);

	if (bus_dmamem_alloc(t, size, PAGE_SIZE, 0, &vd->vd_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(t, &vd->vd_seg, 1, size, &va,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(t, vd->vd_map, va, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	vd->vd_desc = (struct vnet_desc *)va;
	vd->vd_nentries = nentries;
	bzero(vd->vd_desc, nentries * sizeof(struct vnet_desc));
	for (i = 0; i < vd->vd_nentries; i++)
		vd->vd_desc[i].hdr.dstate = VIO_DESC_FREE;
	return (vd);

unmap:
	bus_dmamem_unmap(t, va, size);
free:
	bus_dmamem_free(t, &vd->vd_seg, 1);
destroy:
	bus_dmamap_destroy(t, vd->vd_map);

	return (NULL);
}

void
vnet_dring_free(bus_dma_tag_t t, struct vnet_dring *vd)
{
	bus_size_t size;

	size = vd->vd_nentries * sizeof(struct vnet_desc);
	size = roundup(size, PAGE_SIZE);

	bus_dmamap_unload(t, vd->vd_map);
	bus_dmamem_unmap(t, (caddr_t)vd->vd_desc, size);
	bus_dmamem_free(t, &vd->vd_seg, 1);
	bus_dmamap_destroy(t, vd->vd_map);
	free(vd, M_DEVBUF, 0);
}
