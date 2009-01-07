/*	$OpenBSD: vnet.c,v 1.4 2009/01/07 21:12:35 kettenis Exp $	*/
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>

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

#include <uvm/uvm.h>

#include <dev/rndvar.h>

#include <sparc64/dev/cbusvar.h>

/* XXX the following declaration should be elsewhere */
extern void myetheraddr(u_char *);

#ifdef VNET_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

struct ldc_msg {
	uint8_t type;
	uint8_t stype;
	uint8_t ctrl;
	uint8_t env;
	uint32_t seqid;

	uint16_t major;
	uint16_t minor;
	uint32_t _reserved[13];
};

#define LDC_MSGSZ	sizeof(struct ldc_msg)

#define LDC_CTRL	0x01
#define LDC_DATA	0x02
#define LDC_ERR		0x10

#define LDC_INFO	0x01
#define LDC_ACK		0x02
#define LDC_NACK	0x04

#define LDC_VERS	0x01
#define LDC_RTS		0x02
#define LDC_RTR		0x03
#define LDC_RDX		0x04

#define LDC_MODE_RAW		0x00
#define LDC_MODE_UNRELIABLE	0x01
#define LDC_MODE_RELIABLE	0x03

#define LDC_LEN_MASK	0x3f
#define LDC_FRAG_MASK	0xc0
#define LDC_FRAG_START	0x40
#define LDC_FRAG_STOP	0x80

struct ldc_queue {
	bus_dmamap_t	lq_map;
	bus_dma_segment_t lq_seg;
	caddr_t		lq_va;
	int		lq_nentries;
};

struct ldc_cookie {
	uint64_t	addr;
	uint64_t	size;
};

struct ldc_queue *ldc_queue_alloc(bus_dma_tag_t, int);
void	ldc_queue_free(bus_dma_tag_t, struct ldc_queue *);

struct ldc_map_slot {
	uint64_t	entry;
	uint64_t	cookie;
};

#define LDC_MTE_R	0x0000000000000010ULL
#define LDC_MTE_W	0x0000000000000020ULL
#define LDC_MTE_X	0x0000000000000040ULL
#define LDC_MTE_IOR	0x0000000000000080ULL
#define LDC_MTE_IOW	0x0000000000000100ULL
#define LDC_MTE_CPR	0x0000000000000200ULL
#define LDC_MTE_CPW	0x0000000000000400ULL
#define LDC_MTE_RA_MASK	0x007fffffffffe000ULL

struct ldc_map {
	bus_dmamap_t		lm_map;
	bus_dma_segment_t	lm_seg;
	struct ldc_map_slot	*lm_slot;
	int			lm_nentries;
	int			lm_next;
	int			lm_count;
};

struct ldc_map *ldc_map_alloc(bus_dma_tag_t, int);
void	ldc_map_free(bus_dma_tag_t, struct ldc_map *);

#define VNET_TX_ENTRIES		32
#define VNET_RX_ENTRIES		32

struct vio_msg_tag {
	uint8_t		type;
	uint8_t		stype;
	uint16_t	stype_env;
	uint32_t	sid;
};

struct vio_msg {
	uint64_t 	ldc;
	uint8_t		type;
	uint8_t		stype;
	uint16_t	stype_env;
	uint32_t	sid;
	uint16_t	major;
	uint16_t	minor;
	uint8_t		dev_class;
};

struct vio_ver_info {
	struct vio_msg_tag	tag;
	uint16_t		major;
	uint16_t		minor;
	uint8_t			dev_class;
};

struct vnet_attr_info {
	struct vio_msg_tag	tag;
	uint8_t			xfer_mode;
	uint8_t			addr_type;
	uint16_t		ack_freq;
	uint32_t		_reserved;
	uint64_t		addr;
	uint64_t		mtu;
};

struct vio_dring_reg {
	struct vio_msg_tag	tag;
	uint64_t		dring_ident;
	uint32_t		num_descriptors;
	uint32_t		descriptor_size;
	uint16_t		options;
	uint16_t		_reserved;
	uint32_t		ncookies;
	struct ldc_cookie	cookie[1];
};

#define VIO_TYPE_CTRL		0x01
#define VIO_TYPE_DATA		0x02
#define VIO_TYPE_ERR		0x04

#define VIO_SUBTYPE_INFO	0x01
#define VIO_SUBTYPE_ACK		0x02
#define VIO_SUBTYPE_NACK	0x04

#define VIO_VER_INFO		0x0001
#define VIO_ATTR_INFO		0x0002
#define VIO_DRING_REG		0x0003
#define VIO_DRING_UNREG		0x0004
#define VIO_RDX			0x0005

#define VIO_PKT_DATA		0x0040
#define VIO_DESC_DATA		0x0041
#define VIO_DRING_DATA		0x0042

#define VDEV_NETWORK		0x01
#define VDEV_NETWORK_SWITCH	0x02
#define VDEV_DISK		0x03
#define VDEV_DISK_SERVER	0x04

#define VIO_TX_RING		0x0001
#define VIO_RX_RING		0x0002

#define VIO_PKT_MODE		0x01
#define VIO_DESC_MODE		0x02
#define VIO_DRING_MODE		0x03

#define VNET_ADDR_ETHERMAC	0x01

struct vnet_dring_msg {
	struct vio_msg_tag	tag;
	uint64_t		seq_no;
	uint64_t		dring_ident;
	uint32_t		start_idx;
	uint32_t		end_idx;
	uint8_t			proc_state;
	uint8_t			_reserved[7];
};

#define VIO_DP_ACTIVE	0x01
#define VIO_DP_STOPPED	0x02

struct vio_dring_hdr {
	uint8_t		dstate;
	uint8_t		ack: 1;
	uint16_t	_reserved[3];
};

#define VIO_DESC_FREE		0x01
#define VIO_DESC_READY		0x02
#define VIO_DESC_ACCEPTED	0x03
#define VIO_DESC_DONE		0x04

struct vnet_desc {
	struct vio_dring_hdr	hdr;
	uint32_t		nbytes;
	uint32_t		ncookies;
	struct ldc_cookie	cookie[2];
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

	uint64_t	sc_id;

	void		*sc_tx_ih;
	void		*sc_rx_ih;

	struct ldc_queue *sc_txq;
	struct ldc_queue *sc_rxq;

	uint64_t	sc_tx_state;
	uint64_t	sc_rx_state;

	uint32_t	sc_tx_seqid;

	uint8_t		sc_ldc_state;
#define LDC_SND_VERS	1
#define LDC_RCV_VERS	2
#define LDC_SND_RTS	3
#define LDC_SND_RTR	4
#define LDC_SND_RDX	5

	uint8_t		sc_vio_state;
#define VIO_ESTABLISHED	8

	uint32_t	sc_local_sid;
	uint64_t	sc_dring_ident;
	uint64_t	sc_seq_no;

	int		sc_tx_cnt;
	int		sc_tx_prod;
	int		sc_tx_cons;

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

void	vnet_rx_ctrl(struct vnet_softc *, struct ldc_msg *);
void	vnet_rx_ctrl_vers(struct vnet_softc *, struct ldc_msg *);
void	vnet_rx_ctrl_rtr(struct vnet_softc *, struct ldc_msg *);
void	vnet_rx_ctrl_rts(struct vnet_softc *, struct ldc_msg *);

void	vnet_rx_data(struct vnet_softc *, struct ldc_msg *);
void	vnet_rx_vio_ctrl(struct vnet_softc *, struct vio_msg *);
void	vnet_rx_vio_ver_info(struct vnet_softc *, struct vio_msg_tag *);
void	vnet_rx_vio_attr_info(struct vnet_softc *, struct vio_msg_tag *);
void	vnet_rx_vio_dring_reg(struct vnet_softc *, struct vio_msg_tag *);
void	vnet_rx_vio_rdx(struct vnet_softc *sc, struct vio_msg_tag *);
void	vnet_rx_vio_data(struct vnet_softc *sc, struct vio_msg *);
void	vnet_rx_vio_dring_data(struct vnet_softc *sc, struct vio_msg_tag *);

void	ldc_send_vers(struct vnet_softc *);
void	ldc_send_rtr(struct vnet_softc *);
void	ldc_send_rts(struct vnet_softc *);
void	ldc_send_rdx(struct vnet_softc *);
void	ldc_reset(struct vnet_softc *);

void	vio_sendmsg(struct vnet_softc *, void *, size_t);
void	vio_send_ver_info(struct vnet_softc *, uint16_t, uint16_t);
void	vnet_send_attr_info(struct vnet_softc *);
void	vnet_send_dring_reg(struct vnet_softc *);
void	vio_send_rdx(struct vnet_softc *);

void	vnet_start(struct ifnet *);
int	vnet_ioctl(struct ifnet *, u_long, caddr_t);
void	vnet_watchdog(struct ifnet *);

int	vnet_media_change(struct ifnet *);
void	vnet_media_status(struct ifnet *, struct ifmediareq *);

void	vnet_link_state(struct vnet_softc *sc);

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
	struct ifnet *ifp;
	uint64_t sysino[2];

	sc->sc_bustag = ca->ca_bustag;
	sc->sc_dmatag = ca->ca_dmatag;

	sc->sc_id = ca->ca_id;

	if (OF_getprop(ca->ca_node, "local-mac-address", sc->sc_ac.ac_enaddr,
	    ETHER_ADDR_LEN) <= 0)
		myetheraddr(sc->sc_ac.ac_enaddr);

	if (cbus_intr_map(ca->ca_node, ca->ca_tx_ino, &sysino[0]) ||
	    cbus_intr_map(ca->ca_node, ca->ca_rx_ino, &sysino[1])) {
		printf(": can't map interrupt\n");
		return;
	}
	printf(": ivec 0x%lx, 0x%lx", sysino[0], sysino[1]);

	/*
	 * Un-configure queues before registering interrupt handlers,
	 * such that we dont get any stale LDC packets or events.
	 */
	hv_ldc_tx_qconf(sc->sc_id, 0, 0);
	hv_ldc_rx_qconf(sc->sc_id, 0, 0);

	sc->sc_tx_ih = bus_intr_establish(ca->ca_bustag, sysino[0], IPL_NET,
	    0, vnet_tx_intr, sc, sc->sc_dv.dv_xname);
	sc->sc_rx_ih = bus_intr_establish(ca->ca_bustag, sysino[1], IPL_NET,
	    0, vnet_rx_intr, sc, sc->sc_dv.dv_xname);
	if (sc->sc_tx_ih == NULL || sc->sc_rx_ih == NULL) {
		printf(", can't establish interrupt\n");
		return;
	}

	sc->sc_txq = ldc_queue_alloc(sc->sc_dmatag, VNET_TX_ENTRIES);
	if (sc->sc_txq == NULL) {
		printf(", can't allocate tx queue\n");
		return;
	}

	sc->sc_rxq = ldc_queue_alloc(sc->sc_dmatag, VNET_RX_ENTRIES);
	if (sc->sc_rxq == NULL) {
		printf(", can't allocate rx queue\n");
		goto free_txqueue;
	}

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

	printf(", address %s\n", ether_sprintf(sc->sc_ac.ac_enaddr));
	return;

free_txqueue:
	ldc_queue_free(sc->sc_dmatag, sc->sc_txq);
	return;
}

int
vnet_tx_intr(void *arg)
{
	struct vnet_softc *sc = arg;
	uint64_t tx_head, tx_tail, tx_state;

	hv_ldc_tx_get_state(sc->sc_id, &tx_head, &tx_tail, &tx_state);
	if (tx_state != sc->sc_tx_state) {
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
		sc->sc_tx_state = tx_state;
	}

	return (1);
}

int
vnet_rx_intr(void *arg)
{
	struct vnet_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint64_t rx_head, rx_tail, rx_state;
	struct ldc_msg *lm;
	uint64_t *msg;
	int err;

	err = hv_ldc_rx_get_state(sc->sc_id, &rx_head, &rx_tail, &rx_state);
	if (err == H_EINVAL)
		return (0);
	if (err != H_EOK) {
		printf("hv_ldc_rx_get_state %d\n", err);
		return (0);
	}

	if (rx_state != sc->sc_rx_state) {
		sc->sc_tx_cnt = sc->sc_tx_prod = sc->sc_tx_cons = 0;
		sc->sc_tx_seqid = 0;
		sc->sc_ldc_state = 0;
		ifp->if_flags &= ~IFF_RUNNING;
		switch (rx_state) {
		case LDC_CHANNEL_DOWN:
			DPRINTF(("Rx link down\n"));
			break;
		case LDC_CHANNEL_UP:
			DPRINTF(("Rx link up\n"));
			ldc_send_vers(sc);
			break;
		case LDC_CHANNEL_RESET:
			DPRINTF(("Rx link reset\n"));
			break;
		}
		sc->sc_rx_state = rx_state;
		hv_ldc_rx_set_qhead(sc->sc_id, rx_tail);
		return (1);
	}

	msg = (uint64_t *)(sc->sc_rxq->lq_va + rx_head);
#if 0
{
	int i;

	printf("%s: rx intr, head %lx, tail %lx\n", sc->sc_dv.dv_xname,
	    rx_head, rx_tail);
	for (i = 0; i < 8; i++)
		printf("word %d: 0x%016lx\n", i, msg[i]);
}
#endif
	lm = (struct ldc_msg *)(sc->sc_rxq->lq_va + rx_head);
	switch (lm->type) {
	case LDC_CTRL:
		vnet_rx_ctrl(sc, lm);
		break;

	case LDC_DATA:
		vnet_rx_data(sc, lm);
		break;

	default:
		DPRINTF(("%0x02/%0x02/%0x02\n", lm->type, lm->stype,
		    lm->ctrl));
		ldc_reset(sc);
		break;
	}

	if (sc->sc_ldc_state == 0)
		return (1);

	rx_head += LDC_MSGSZ;
	rx_head &= ((sc->sc_rxq->lq_nentries * LDC_MSGSZ) - 1);
	err = hv_ldc_rx_set_qhead(sc->sc_id, rx_head);
	if (err != H_EOK)
		printf("%s: hv_ldc_rx_set_qhead %d\n", __func__, err);

	return (1);
}

void
vnet_rx_ctrl(struct vnet_softc *sc, struct ldc_msg *lm)
{
	switch (lm->ctrl) {
	case LDC_VERS:
		vnet_rx_ctrl_vers(sc, lm);
		break;

	case LDC_RTS:
		vnet_rx_ctrl_rts(sc, lm);
		break;

	case LDC_RTR:
		vnet_rx_ctrl_rtr(sc, lm);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/0x%02x\n", lm->stype, lm->ctrl));
		ldc_reset(sc);
		break;
	}
}

void
vnet_rx_ctrl_vers(struct vnet_softc *sc, struct ldc_msg *lm)
{
	switch (lm->stype) {
	case LDC_INFO:
		/* XXX do nothing for now. */
		break;

	case LDC_ACK:
		if (sc->sc_ldc_state != LDC_SND_VERS) {
			DPRINTF(("Spurious CTRL/ACK/VERS: state %d\n",
			    sc->sc_ldc_state));
			ldc_reset(sc);
			return;
		}
		DPRINTF(("CTRL/ACK/VERS\n"));
		ldc_send_rts(sc);
		break;

	case LDC_NACK:
		DPRINTF(("CTRL/NACK/VERS\n"));
		ldc_reset(sc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/VERS\n", lm->stype));
		ldc_reset(sc);
		break;
	}
}

void
vnet_rx_ctrl_rts(struct vnet_softc *sc, struct ldc_msg *lm)
{
	switch (lm->stype) {
	case LDC_INFO:
		if (sc->sc_ldc_state != LDC_RCV_VERS) {
			DPRINTF(("Suprious CTRL/INFO/RTS: state %d\n",
			    sc->sc_ldc_state));
			ldc_reset(sc);
			return;
		}
		DPRINTF(("CTRL/INFO/RTS\n"));
		ldc_send_rtr(sc);
		break;

	case LDC_ACK:
		DPRINTF(("CTRL/ACK/RTS\n"));
		ldc_reset(sc);
		break;

	case LDC_NACK:
		DPRINTF(("CTRL/NACK/RTS\n"));
		ldc_reset(sc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RTS\n", lm->stype));
		ldc_reset(sc);
		break;
	}
}

void
vnet_rx_ctrl_rtr(struct vnet_softc *sc, struct ldc_msg *lm)
{
	switch (lm->stype) {
	case LDC_INFO:
		if (sc->sc_ldc_state != LDC_SND_RTS) {
			DPRINTF(("Spurious CTRL/INFO/RTR: state %d\n",
			    sc->sc_ldc_state));
			ldc_reset(sc);
			return;
		}
		DPRINTF(("CTRL/INFO/RTR\n"));
		ldc_send_rdx(sc);
		break;

	case LDC_ACK:
		DPRINTF(("CTRL/ACK/RTR\n"));
		ldc_reset(sc);
		break;

	case LDC_NACK:
		DPRINTF(("CTRL/NACK/RTR\n"));
		ldc_reset(sc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RTR\n", lm->stype));
		ldc_reset(sc);
		break;
	}
}

void
vnet_rx_data(struct vnet_softc *sc, struct ldc_msg *lm)
{
	struct vio_msg *vm;

	if (lm->stype != LDC_INFO) {
		DPRINTF(("DATA/0x%02x\n", lm->stype));
		ldc_reset(sc);
		return;
	}

	if (sc->sc_ldc_state != LDC_SND_RTR &&
	    sc->sc_ldc_state != LDC_SND_RDX) {
		DPRINTF(("Spurious DATA/INFO: state %d\n", sc->sc_ldc_state));
		ldc_reset(sc);
		return;
	}

	vm = (struct vio_msg *)lm;
	switch (vm->type) {
	case VIO_TYPE_CTRL:
		if ((lm->env & LDC_FRAG_START) == 0 &&
		    (lm->env & LDC_FRAG_STOP) == 0)
			return;
		vnet_rx_vio_ctrl(sc, vm);
		break;

	case VIO_TYPE_DATA:
		if((lm->env & LDC_FRAG_START) == 0)
			return;
		vnet_rx_vio_data(sc, vm);
		break;

	default:
		DPRINTF(("Unhandled packet type 0x%02x\n", vm->type));
		ldc_reset(sc);
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
			vio_sendmsg(sc, vi, sizeof(*vi));
			return;
		}

		if (vi->major != VNET_MAJOR) {
			vi->tag.stype = VIO_SUBTYPE_NACK;
			vi->major = VNET_MAJOR;
			vi->minor = VNET_MINOR;
			vio_sendmsg(sc, vi, sizeof(*vi));
			return;
		}

		/* Allocate new session ID. */
		sc->sc_local_sid = arc4random();

		vi->tag.stype = VIO_SUBTYPE_ACK;
		vi->tag.sid = sc->sc_local_sid;
		vi->minor = VNET_MINOR;
		vio_sendmsg(sc, vi, sizeof(*vi));

		vio_send_ver_info(sc, VNET_MAJOR, VNET_MINOR);
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/VER_INFO\n"));
		break;

	default:
		DPRINTF(("CTRL/0x%02x/VER_INFO\n", vi->tag.stype));
		break;
	}
}

void
vnet_rx_vio_attr_info(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct vnet_attr_info *ai = (struct vnet_attr_info *)tag;

	switch (ai->tag.stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/ATTR_INFO\n"));

		ai->tag.stype = VIO_SUBTYPE_ACK;
		ai->tag.sid = sc->sc_local_sid;
		vio_sendmsg(sc, ai, sizeof(*ai));

		vnet_send_attr_info(sc);
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/ATTR_INFO\n"));
		break;

	default:
		DPRINTF(("CTRL/0x%02x/VER_INFO\n", ai->tag.stype));
		break;
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
		vio_sendmsg(sc, dr, sizeof(*dr));

		vnet_send_dring_reg(sc);
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/DRING_REG\n"));

		sc->sc_dring_ident = dr->dring_ident;
		sc->sc_seq_no = 1;

		vio_send_rdx(sc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/DRING_REG\n", dr->tag.stype));
		break;
	}
}

void
vnet_rx_vio_rdx(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	switch(tag->stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/RDX\n"));

		tag->stype = VIO_SUBTYPE_ACK;
		tag->sid = sc->sc_local_sid;
		vio_sendmsg(sc, tag, sizeof(*tag));
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/RDX\n"));

		/* Link is up! */
		sc->sc_vio_state = VIO_ESTABLISHED;
		vnet_link_state(sc);
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RDX (VIO)\n", tag->stype));
		break;
	}
}

void
vnet_rx_vio_data(struct vnet_softc *sc, struct vio_msg *vm)
{
	struct vio_msg_tag *tag = (struct vio_msg_tag *)&vm->type;

	if (sc->sc_vio_state != VIO_ESTABLISHED) {
		DPRINTF(("Spurious DATA/0x%02x/0x%04x\n", tag->stype,
		    tag->stype_env));
		return;
	}

	switch(tag->stype_env) {
	case VIO_DRING_DATA:
		vnet_rx_vio_dring_data(sc, tag);
		break;

	default:
		DPRINTF(("DATA/0x%02x/0x%04x\n", tag->stype, tag->stype_env));
		break;
	}
}

void
vnet_rx_vio_dring_data(struct vnet_softc *sc, struct vio_msg_tag *tag)
{
	struct vnet_dring_msg *dm = (struct vnet_dring_msg *)tag;
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
			err = hv_ldc_copy(sc->sc_id, LDC_COPY_IN, cookie,
			    desc_pa, nbytes, &nbytes);
			if (err != H_EOK) {
				printf("hv_ldc_copy_in %d\n", err);
				break;
			}

			if (desc.hdr.dstate != VIO_DESC_READY)
				break;

			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if(m == NULL)
				break;
			MCLGETI(m, M_DONTWAIT, &sc->sc_ac.ac_if, MCLBYTES);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				break;
			}
			ifp->if_ipackets++;
			m->m_pkthdr.rcvif = ifp;
			m->m_len = m->m_pkthdr.len = desc.nbytes;
			nbytes = roundup(desc.nbytes + VNET_ETHER_ALIGN, 8);

			pmap_extract(pmap_kernel(), (vaddr_t)m->m_data, &pa);
			err = hv_ldc_copy(sc->sc_id, LDC_COPY_IN,
			    desc.cookie[0].addr, pa, nbytes, &nbytes);
			if (err != H_EOK) {
#if 0
				printf("hv_ldc_copy_in data %d\n", err);
				printf("start_idx %d, end_idx %d, idx %d\n",
				    dm->start_idx, dm->end_idx, idx);
				printf("nbytes %d ncookies %d\n",
				    desc.nbytes, desc.ncookies);
				printf("cookie 0x%llx, size %lld\n",
				    desc.cookie[0].addr, desc.cookie[0].size);
				printf("pa 0x%llx\n", pa);
#endif
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
			err = hv_ldc_copy(sc->sc_id, LDC_COPY_OUT, cookie,
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
		vio_sendmsg(sc, dm, sizeof(*dm));
		break;
	}

	case VIO_SUBTYPE_ACK:
	{
		struct ldc_map *map = sc->sc_lm;
		int cons;

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

		if (sc->sc_tx_cnt < sc->sc_vd->vd_nentries)
			ifp->if_flags &= ~IFF_OACTIVE;

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
ldc_send_vers(struct vnet_softc *sc)
{
	struct ldc_msg *lm;
	uint64_t tx_head, tx_tail, tx_state;
	int err;

	err = hv_ldc_tx_get_state(sc->sc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK || tx_state != LDC_CHANNEL_UP)
		return;

	lm = (struct ldc_msg *)(sc->sc_txq->lq_va + tx_tail);
	bzero(lm, sizeof(struct ldc_msg));
	lm->type = LDC_CTRL;
	lm->stype = LDC_INFO;
	lm->ctrl = LDC_VERS;
	lm->major = 1;
	lm->minor = 0;

	tx_tail += LDC_MSGSZ;
	tx_tail &= ((sc->sc_txq->lq_nentries * LDC_MSGSZ) - 1);
	err = hv_ldc_tx_set_qtail(sc->sc_id, tx_tail);
	if (err != H_EOK) {
		printf("%s: hv_ldc_tx_set_qtail: %d\n", __func__, err);
		return;
	}

	sc->sc_ldc_state = LDC_SND_VERS;
}

void
ldc_send_rts(struct vnet_softc *sc)
{
	struct ldc_msg *lm;
	uint64_t tx_head, tx_tail, tx_state;
	int err;

	err = hv_ldc_tx_get_state(sc->sc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK || tx_state != LDC_CHANNEL_UP)
		return;

	lm = (struct ldc_msg *)(sc->sc_txq->lq_va + tx_tail);
	bzero(lm, sizeof(struct ldc_msg));
	lm->type = LDC_CTRL;
	lm->stype = LDC_INFO;
	lm->ctrl = LDC_RTS;
	lm->env = LDC_MODE_UNRELIABLE;
	lm->seqid = sc->sc_tx_seqid++;

	tx_tail += LDC_MSGSZ;
	tx_tail &= ((sc->sc_txq->lq_nentries * LDC_MSGSZ) - 1);
	err = hv_ldc_tx_set_qtail(sc->sc_id, tx_tail);
	if (err != H_EOK) {
		printf("%s: hv_ldc_tx_set_qtail: %d\n", __func__, err);
		return;
	}

	sc->sc_ldc_state = LDC_SND_RTS;
}

void
ldc_send_rtr(struct vnet_softc *sc)
{
	struct ldc_msg *lm;
	uint64_t tx_head, tx_tail, tx_state;
	int err;

	err = hv_ldc_tx_get_state(sc->sc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK || tx_state != LDC_CHANNEL_UP)
		return;

	lm = (struct ldc_msg *)(sc->sc_txq->lq_va + tx_tail);
	bzero(lm, sizeof(struct ldc_msg));
	lm->type = LDC_CTRL;
	lm->stype = LDC_INFO;
	lm->ctrl = LDC_RTR;
	lm->env = LDC_MODE_UNRELIABLE;
	lm->seqid = sc->sc_tx_seqid++;

	tx_tail += LDC_MSGSZ;
	tx_tail &= ((sc->sc_txq->lq_nentries * LDC_MSGSZ) - 1);
	err = hv_ldc_tx_set_qtail(sc->sc_id, tx_tail);
	if (err != H_EOK)
		printf("%s: hv_ldc_tx_set_qtail: %d\n", __func__, err);

	sc->sc_ldc_state = LDC_SND_RTR;
}

void
ldc_send_rdx(struct vnet_softc *sc)
{
	struct ldc_msg *lm;
	uint64_t tx_head, tx_tail, tx_state;
	int err;

	err = hv_ldc_tx_get_state(sc->sc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK || tx_state != LDC_CHANNEL_UP)
		return;

	lm = (struct ldc_msg *)(sc->sc_txq->lq_va + tx_tail);
	bzero(lm, sizeof(struct ldc_msg));
	lm->type = LDC_CTRL;
	lm->stype = LDC_INFO;
	lm->ctrl = LDC_RDX;
	lm->env = LDC_MODE_UNRELIABLE;
	lm->seqid = sc->sc_tx_seqid++;

	tx_tail += LDC_MSGSZ;
	tx_tail &= ((sc->sc_txq->lq_nentries * LDC_MSGSZ) - 1);
	err = hv_ldc_tx_set_qtail(sc->sc_id, tx_tail);
	if (err != H_EOK)
		printf("%s: hv_ldc_tx_set_qtail: %d\n", __func__, err);

	sc->sc_ldc_state = LDC_SND_RDX;
}

void
ldc_reset(struct vnet_softc *sc)
{
	int err;

	DPRINTF(("Resetting connection\n"));
	hv_ldc_tx_qconf(sc->sc_id, 0, 0);
	hv_ldc_rx_qconf(sc->sc_id, 0, 0);
	sc->sc_tx_state = sc->sc_rx_state = LDC_CHANNEL_DOWN;

	err = hv_ldc_tx_qconf(sc->sc_id,
	    sc->sc_txq->lq_map->dm_segs[0].ds_addr, sc->sc_txq->lq_nentries);
	if (err != H_EOK)
		printf("%s: hv_ldc_tx_qconf %d\n", __func__, err);

	err = hv_ldc_rx_qconf(sc->sc_id,
	    sc->sc_rxq->lq_map->dm_segs[0].ds_addr, sc->sc_rxq->lq_nentries);
	if (err != H_EOK)
		printf("%s: hv_ldc_rx_qconf %d\n", __func__, err);

	sc->sc_vio_state = 0;
	sc->sc_ldc_state = 0;
	sc->sc_tx_seqid = 0;

	vnet_link_state(sc);
}

void
vio_sendmsg(struct vnet_softc *sc, void *msg, size_t len)
{
	struct ldc_msg *lm;
	uint64_t tx_head, tx_tail, tx_state;
	int err;

#if 0
	printf("%s\n", __func__);
#endif
	err = hv_ldc_tx_get_state(sc->sc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK)
		return;

	lm = (struct ldc_msg *)(sc->sc_txq->lq_va + tx_tail);
	bzero(lm, sizeof(struct ldc_msg));
	lm->type = LDC_DATA;
	lm->stype = LDC_INFO;
	lm->env = 56 | LDC_FRAG_STOP | LDC_FRAG_START;
	lm->seqid = sc->sc_tx_seqid++;
	bcopy(msg, &lm->major, len);

#if 0
{
	uint64_t *p = (uint64_t *)(sc->sc_txq->lq_va + tx_tail);
	int i;

	for (i = 0; i < 8; i++)
		printf("word %d: 0x%016lx\n", i, p[i]);
}
#endif

	tx_tail += LDC_MSGSZ;
	tx_tail &= ((sc->sc_txq->lq_nentries * LDC_MSGSZ) - 1);
	err = hv_ldc_tx_set_qtail(sc->sc_id, tx_tail);
	if (err != H_EOK)
		printf("hv_ldc_tx_set_qtail: %d\n", err);
}

void
vio_send_ver_info(struct vnet_softc *sc, uint16_t major, uint16_t minor)
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
	vio_sendmsg(sc, &vi, sizeof(vi));
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
	vio_sendmsg(sc, &ai, sizeof(ai));
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
	vio_sendmsg(sc, &dr, sizeof(dr));
};

void
vio_send_rdx(struct vnet_softc *sc)
{
	struct vio_msg_tag tag;

	tag.type = VIO_TYPE_CTRL;
	tag.stype = VIO_SUBTYPE_INFO;
	tag.stype_env = VIO_RDX;
	tag.sid = sc->sc_local_sid;
	vio_sendmsg(sc, &tag, sizeof(tag));
}

void
vnet_start(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;
	struct ldc_map *map = sc->sc_lm;
	struct vnet_dring_msg dm;
	struct mbuf *m;
	paddr_t pa;
	caddr_t buf;
	int desc;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	/*
	 * We cannot transmit packets until a VIO connection has been
	 * established.
	 */
	if (sc->sc_vio_state != VIO_ESTABLISHED)
		return;

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
		/* Don't take slot 0; it's used by our descriptor ring. */
		if (map->lm_next == 0)
			map->lm_next++;
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

	if (desc != sc->sc_tx_prod) {
		bzero(&dm, sizeof(dm));
		dm.tag.type = VIO_TYPE_DATA;
		dm.tag.stype = VIO_SUBTYPE_INFO;
		dm.tag.stype_env = VIO_DRING_DATA;
		dm.tag.sid = sc->sc_local_sid;
		dm.seq_no = sc->sc_seq_no++;
		dm.dring_ident = sc->sc_dring_ident;
		dm.start_idx = sc->sc_tx_prod;
		dm.end_idx = -1;
		vio_sendmsg(sc, &dm, sizeof(dm));
	}

	sc->sc_tx_prod = desc;
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
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif
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

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	splx(s);
	return (error);
}

void
vnet_watchdog(struct ifnet *ifp)
{
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

	if (sc->sc_vio_state == VIO_ESTABLISHED)
		link_state = LINK_STATE_FULL_DUPLEX;
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}

void
vnet_init(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;
	int err;

	sc->sc_lm = ldc_map_alloc(sc->sc_dmatag, 2048);
	if (sc->sc_lm == NULL)
		return;

	err = hv_ldc_set_map_table(sc->sc_id,
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

	err = hv_ldc_tx_qconf(sc->sc_id,
	    sc->sc_txq->lq_map->dm_segs[0].ds_addr, sc->sc_txq->lq_nentries);
	if (err != H_EOK)
		printf("hv_ldc_tx_qconf %d\n", err);

	err = hv_ldc_rx_qconf(sc->sc_id,
	    sc->sc_rxq->lq_map->dm_segs[0].ds_addr, sc->sc_rxq->lq_nentries);
	if (err != H_EOK)
		printf("hv_ldc_rx_qconf %d\n", err);

	ldc_send_vers(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
}

void
vnet_stop(struct ifnet *ifp)
{
	struct vnet_softc *sc = ifp->if_softc;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	hv_ldc_tx_qconf(sc->sc_id, 0, 0);
	hv_ldc_rx_qconf(sc->sc_id, 0, 0);
	sc->sc_tx_state = sc->sc_rx_state = LDC_CHANNEL_DOWN;

	vnet_dring_free(sc->sc_dmatag, sc->sc_vd);

	hv_ldc_set_map_table(sc->sc_id, 0, 0);
	ldc_map_free(sc->sc_dmatag, sc->sc_lm);
}

struct ldc_queue *
ldc_queue_alloc(bus_dma_tag_t t, int nentries)
{
	struct ldc_queue *lq;
	bus_size_t size;
	caddr_t va;
	int nsegs;

	lq = malloc(sizeof(struct ldc_queue), M_DEVBUF, M_NOWAIT);
	if (lq == NULL)
		return NULL;

	size = roundup(nentries * LDC_MSGSZ, PAGE_SIZE);

	if (bus_dmamap_create(t, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &lq->lq_map) != 0)
		return (NULL);

	if (bus_dmamem_alloc(t, size, PAGE_SIZE, 0, &lq->lq_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(t, &lq->lq_seg, 1, size, &va,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(t, lq->lq_map, va, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	lq->lq_va = va;
	lq->lq_nentries = nentries;
	return (lq);

unmap:
	bus_dmamem_unmap(t, va, size);
free:
	bus_dmamem_free(t, &lq->lq_seg, 1);
destroy:
	bus_dmamap_destroy(t, lq->lq_map);

	return (NULL);
}

void
ldc_queue_free(bus_dma_tag_t t, struct ldc_queue *lq)
{
	bus_size_t size;

	size = roundup(lq->lq_nentries * LDC_MSGSZ, PAGE_SIZE);

	bus_dmamap_unload(t, lq->lq_map);
	bus_dmamem_unmap(t, lq->lq_va, size);
	bus_dmamem_free(t, &lq->lq_seg, 1);
	bus_dmamap_destroy(t, lq->lq_map);
	free(lq, M_DEVBUF);
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
	free(vd, M_DEVBUF);
}

struct ldc_map *
ldc_map_alloc(bus_dma_tag_t t, int nentries)
{
	struct ldc_map *lm;
	bus_size_t size;
	caddr_t va;
	int nsegs;

	lm = malloc(sizeof(struct ldc_map), M_DEVBUF, M_NOWAIT);
	if (lm == NULL)
		return NULL;

	size = roundup(nentries * sizeof(struct ldc_map_slot), PAGE_SIZE);

	if (bus_dmamap_create(t, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &lm->lm_map) != 0)
		return (NULL);

	if (bus_dmamem_alloc(t, size, PAGE_SIZE, 0, &lm->lm_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(t, &lm->lm_seg, 1, size, &va,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(t, lm->lm_map, va, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	lm->lm_slot = (struct ldc_map_slot *)va;
	lm->lm_nentries = nentries;
	bzero(lm->lm_slot, nentries * sizeof(struct ldc_map_slot));
	return (lm);

unmap:
	bus_dmamem_unmap(t, va, size);
free:
	bus_dmamem_free(t, &lm->lm_seg, 1);
destroy:
	bus_dmamap_destroy(t, lm->lm_map);

	return (NULL);
}

void
ldc_map_free(bus_dma_tag_t t, struct ldc_map *lm)
{
	bus_size_t size;

	size = lm->lm_nentries * sizeof(struct ldc_map_slot);
	size = roundup(size, PAGE_SIZE);

	bus_dmamap_unload(t, lm->lm_map);
	bus_dmamem_unmap(t, (caddr_t)lm->lm_slot, size);
	bus_dmamem_free(t, &lm->lm_seg, 1);
	bus_dmamap_destroy(t, lm->lm_map);
	free(lm, M_DEVBUF);
}
