/*	$OpenBSD: if_vio.c,v 1.21 2014/12/13 21:05:33 doug Exp $	*/

/*
 * Copyright (c) 2012 Stefan Fritsch, Alexander Fiveg.
 * Copyright (c) 2010 Minoura Makoto.
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/timeout.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/virtioreg.h>
#include <dev/pci/virtiovar.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if VIRTIO_DEBUG
#define DBGPRINT(fmt, args...) printf("%s: " fmt "\n", __func__, ## args)
#else
#define DBGPRINT(fmt, args...)
#endif

/*
 * if_vioreg.h:
 */
/* Configuration registers */
#define VIRTIO_NET_CONFIG_MAC		0 /* 8bit x 6byte */
#define VIRTIO_NET_CONFIG_STATUS	6 /* 16bit */

/* Feature bits */
#define VIRTIO_NET_F_CSUM		(1<<0)
#define VIRTIO_NET_F_GUEST_CSUM		(1<<1)
#define VIRTIO_NET_F_MAC		(1<<5)
#define VIRTIO_NET_F_GSO		(1<<6)
#define VIRTIO_NET_F_GUEST_TSO4		(1<<7)
#define VIRTIO_NET_F_GUEST_TSO6		(1<<8)
#define VIRTIO_NET_F_GUEST_ECN		(1<<9)
#define VIRTIO_NET_F_GUEST_UFO		(1<<10)
#define VIRTIO_NET_F_HOST_TSO4		(1<<11)
#define VIRTIO_NET_F_HOST_TSO6		(1<<12)
#define VIRTIO_NET_F_HOST_ECN		(1<<13)
#define VIRTIO_NET_F_HOST_UFO		(1<<14)
#define VIRTIO_NET_F_MRG_RXBUF		(1<<15)
#define VIRTIO_NET_F_STATUS		(1<<16)
#define VIRTIO_NET_F_CTRL_VQ		(1<<17)
#define VIRTIO_NET_F_CTRL_RX		(1<<18)
#define VIRTIO_NET_F_CTRL_VLAN		(1<<19)
#define VIRTIO_NET_F_CTRL_RX_EXTRA	(1<<20)
#define VIRTIO_NET_F_GUEST_ANNOUNCE	(1<<21)

/*
 * Config(8) flags. The lowest byte is reserved for generic virtio stuff.
 */

/* Workaround for vlan related bug in qemu < version 2.0 */
#define CONFFLAG_QEMU_VLAN_BUG		(1<<8)

static const struct virtio_feature_name virtio_net_feature_names[] = {
	{ VIRTIO_NET_F_CSUM,		"CSum" },
	{ VIRTIO_NET_F_GUEST_CSUM,	"GuestCSum" },
	{ VIRTIO_NET_F_MAC,		"MAC" },
	{ VIRTIO_NET_F_GSO,		"GSO" },
	{ VIRTIO_NET_F_GUEST_TSO4,	"GuestTSO4" },
	{ VIRTIO_NET_F_GUEST_TSO6,	"GuestTSO6" },
	{ VIRTIO_NET_F_GUEST_ECN,	"GuestECN" },
	{ VIRTIO_NET_F_GUEST_UFO,	"GuestUFO" },
	{ VIRTIO_NET_F_HOST_TSO4,	"HostTSO4" },
	{ VIRTIO_NET_F_HOST_TSO6,	"HostTSO6" },
	{ VIRTIO_NET_F_HOST_ECN, 	"HostECN" },
	{ VIRTIO_NET_F_HOST_UFO, 	"HostUFO" },
	{ VIRTIO_NET_F_MRG_RXBUF,	"MrgRXBuf" },
	{ VIRTIO_NET_F_STATUS,		"Status" },
	{ VIRTIO_NET_F_CTRL_VQ,		"CtrlVQ" },
	{ VIRTIO_NET_F_CTRL_RX,		"CtrlRX" },
	{ VIRTIO_NET_F_CTRL_VLAN,	"CtrlVLAN" },
	{ VIRTIO_NET_F_CTRL_RX_EXTRA,	"CtrlRXExtra" },
	{ VIRTIO_NET_F_GUEST_ANNOUNCE,	"GuestAnnounce" },
	{ 0, 				NULL }
};

/* Status */
#define VIRTIO_NET_S_LINK_UP	1

/* Packet header structure */
struct virtio_net_hdr {
	uint8_t		flags;
	uint8_t		gso_type;
	uint16_t	hdr_len;
	uint16_t	gso_size;
	uint16_t	csum_start;
	uint16_t	csum_offset;

	/* only present if VIRTIO_NET_F_MRG_RXBUF is negotiated */
	uint16_t	num_buffers;
} __packed;

#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1 /* flags */
#define VIRTIO_NET_HDR_GSO_NONE		0 /* gso_type */
#define VIRTIO_NET_HDR_GSO_TCPV4	1 /* gso_type */
#define VIRTIO_NET_HDR_GSO_UDP		3 /* gso_type */
#define VIRTIO_NET_HDR_GSO_TCPV6	4 /* gso_type */
#define VIRTIO_NET_HDR_GSO_ECN		0x80 /* gso_type, |'ed */

#define VIRTIO_NET_MAX_GSO_LEN		(65536+ETHER_HDR_LEN)

/* Control virtqueue */
struct virtio_net_ctrl_cmd {
	uint8_t	class;
	uint8_t	command;
} __packed;
#define VIRTIO_NET_CTRL_RX		0
# define VIRTIO_NET_CTRL_RX_PROMISC	0
# define VIRTIO_NET_CTRL_RX_ALLMULTI	1

#define VIRTIO_NET_CTRL_MAC		1
# define VIRTIO_NET_CTRL_MAC_TABLE_SET	0

#define VIRTIO_NET_CTRL_VLAN		2
# define VIRTIO_NET_CTRL_VLAN_ADD	0
# define VIRTIO_NET_CTRL_VLAN_DEL	1

struct virtio_net_ctrl_status {
	uint8_t	ack;
} __packed;
#define VIRTIO_NET_OK			0
#define VIRTIO_NET_ERR			1

struct virtio_net_ctrl_rx {
	uint8_t	onoff;
} __packed;

struct virtio_net_ctrl_mac_tbl {
	uint32_t nentries;
	uint8_t macs[][ETHER_ADDR_LEN];
} __packed;

struct virtio_net_ctrl_vlan {
	uint16_t id;
} __packed;

/*
 * if_viovar.h:
 */
enum vio_ctrl_state {
	FREE, INUSE, DONE, RESET
};

struct vio_softc {
	struct device		sc_dev;

	struct virtio_softc	*sc_virtio;
#define	VQRX	0
#define	VQTX	1
#define	VQCTL	2
	struct virtqueue	sc_vq[3];

	struct arpcom		sc_ac;
	struct ifmedia		sc_media;

	short			sc_ifflags;

	/* bus_dmamem */
	bus_dma_segment_t	sc_dma_seg;
	bus_dmamap_t		sc_dma_map;
	size_t			sc_dma_size;
	caddr_t			sc_dma_kva;

	int			sc_hdr_size;
	struct virtio_net_hdr	*sc_tx_hdrs;
	struct virtio_net_ctrl_cmd *sc_ctrl_cmd;
	struct virtio_net_ctrl_status *sc_ctrl_status;
	struct virtio_net_ctrl_rx *sc_ctrl_rx;
	struct virtio_net_ctrl_mac_tbl *sc_ctrl_mac_tbl_uc;
#define sc_ctrl_mac_info sc_ctrl_mac_tbl_uc
	struct virtio_net_ctrl_mac_tbl *sc_ctrl_mac_tbl_mc;

	/* kmem */
	bus_dmamap_t		*sc_arrays;
#define sc_rx_dmamaps sc_arrays
	bus_dmamap_t		*sc_tx_dmamaps;
	struct mbuf		**sc_rx_mbufs;
	struct mbuf		**sc_tx_mbufs;
	struct if_rxring	sc_rx_ring;

	enum vio_ctrl_state	sc_ctrl_inuse;

	struct timeout		sc_txtick, sc_rxtick;
};

#define VIO_DMAMEM_OFFSET(sc, p) ((caddr_t)(p) - (sc)->sc_dma_kva)
#define VIO_DMAMEM_SYNC(vsc, sc, p, size, flags)		\
	bus_dmamap_sync((vsc)->sc_dmat, (sc)->sc_dma_map,	\
	    VIO_DMAMEM_OFFSET((sc), (p)), (size), (flags))
#define VIO_DMAMEM_ENQUEUE(sc, vq, slot, p, size, write)	\
	virtio_enqueue_p((vq), (slot), (sc)->sc_dma_map,	\
	    VIO_DMAMEM_OFFSET((sc), (p)), (size), (write))
#define VIO_HAVE_MRG_RXBUF(sc)					\
	((sc)->sc_hdr_size == sizeof(struct virtio_net_hdr))

#define VIRTIO_NET_TX_MAXNSEGS		16 /* for larger chains, defrag */
#define VIRTIO_NET_CTRL_MAC_MC_ENTRIES	64 /* for more entries, use ALLMULTI */
#define VIRTIO_NET_CTRL_MAC_UC_ENTRIES	 1 /* one entry for own unicast addr */

#define VIO_CTRL_MAC_INFO_SIZE 					\
	(2*sizeof(struct virtio_net_ctrl_mac_tbl) + 		\
	 (VIRTIO_NET_CTRL_MAC_MC_ENTRIES + 			\
	  VIRTIO_NET_CTRL_MAC_UC_ENTRIES) * ETHER_ADDR_LEN)

/* cfattach interface functions */
int	vio_match(struct device *, void *, void *);
void	vio_attach(struct device *, struct device *, void *);

/* ifnet interface functions */
int	vio_init(struct ifnet *);
void	vio_stop(struct ifnet *, int);
void	vio_start(struct ifnet *);
int	vio_ioctl(struct ifnet *, u_long, caddr_t);
void	vio_get_lladr(struct arpcom *ac, struct virtio_softc *vsc);
void	vio_put_lladr(struct arpcom *ac, struct virtio_softc *vsc);

/* rx */
int	vio_add_rx_mbuf(struct vio_softc *, int);
void	vio_free_rx_mbuf(struct vio_softc *, int);
void	vio_populate_rx_mbufs(struct vio_softc *);
int	vio_rxeof(struct vio_softc *);
int	vio_rx_intr(struct virtqueue *);
void	vio_rx_drain(struct vio_softc *);
void	vio_rxtick(void *);

/* tx */
int	vio_tx_intr(struct virtqueue *);
int	vio_txeof(struct virtqueue *);
void	vio_tx_drain(struct vio_softc *);
int	vio_encap(struct vio_softc *, int, struct mbuf *, struct mbuf **);
void	vio_txtick(void *);

/* other control */
void	vio_link_state(struct ifnet *);
int	vio_config_change(struct virtio_softc *);
int	vio_ctrl_rx(struct vio_softc *, int, int);
int	vio_set_rx_filter(struct vio_softc *);
void	vio_iff(struct vio_softc *);
int	vio_media_change(struct ifnet *);
void	vio_media_status(struct ifnet *, struct ifmediareq *);
int	vio_ctrleof(struct virtqueue *);
void	vio_wait_ctrl(struct vio_softc *sc);
int	vio_wait_ctrl_done(struct vio_softc *sc);
void	vio_ctrl_wakeup(struct vio_softc *, enum vio_ctrl_state);
int	vio_alloc_mem(struct vio_softc *);
int	vio_alloc_dmamem(struct vio_softc *);
void	vio_free_dmamem(struct vio_softc *);

#if VIRTIO_DEBUG
void	vio_dump(struct vio_softc *);
#endif

int
vio_match(struct device *parent, void *match, void *aux)
{
	struct virtio_softc *va = aux;

	if (va->sc_childdevid == PCI_PRODUCT_VIRTIO_NETWORK)
		return 1;

	return 0;
}

struct cfattach vio_ca = {
	sizeof(struct vio_softc), vio_match, vio_attach, NULL
};

struct cfdriver vio_cd = {
	NULL, "vio", DV_IFNET
};

int
vio_alloc_dmamem(struct vio_softc *sc)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	int nsegs;

	if (bus_dmamap_create(vsc->sc_dmat, sc->sc_dma_size, 1,
	    sc->sc_dma_size, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &sc->sc_dma_map) != 0)
		goto err;
	if (bus_dmamem_alloc(vsc->sc_dmat, sc->sc_dma_size, 16, 0,
	    &sc->sc_dma_seg, 1, &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;
	if (bus_dmamem_map(vsc->sc_dmat, &sc->sc_dma_seg, nsegs,
	    sc->sc_dma_size, &sc->sc_dma_kva, BUS_DMA_NOWAIT) != 0)
		goto free;
	if (bus_dmamap_load(vsc->sc_dmat, sc->sc_dma_map, sc->sc_dma_kva,
	    sc->sc_dma_size, NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;
	return (0);

unmap:
	bus_dmamem_unmap(vsc->sc_dmat, sc->sc_dma_kva, sc->sc_dma_size);
free:
	bus_dmamem_free(vsc->sc_dmat, &sc->sc_dma_seg, 1);
destroy:
	bus_dmamap_destroy(vsc->sc_dmat, sc->sc_dma_map);
err:
	return (1);
}

void
vio_free_dmamem(struct vio_softc *sc)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	bus_dmamap_unload(vsc->sc_dmat, sc->sc_dma_map);
	bus_dmamem_unmap(vsc->sc_dmat, sc->sc_dma_kva, sc->sc_dma_size);
	bus_dmamem_free(vsc->sc_dmat, &sc->sc_dma_seg, 1);
	bus_dmamap_destroy(vsc->sc_dmat, sc->sc_dma_map);
}

/* allocate memory */
/*
 * dma memory is used for:
 *   sc_tx_hdrs[slot]:	 metadata array for frames to be sent (WRITE)
 *   sc_ctrl_cmd:	 command to be sent via ctrl vq (WRITE)
 *   sc_ctrl_status:	 return value for a command via ctrl vq (READ)
 *   sc_ctrl_rx:	 parameter for a VIRTIO_NET_CTRL_RX class command
 *			 (WRITE)
 *   sc_ctrl_mac_tbl_uc: unicast MAC address filter for a VIRTIO_NET_CTRL_MAC
 *			 class command (WRITE)
 *   sc_ctrl_mac_tbl_mc: multicast MAC address filter for a VIRTIO_NET_CTRL_MAC
 *			 class command (WRITE)
 * sc_ctrl_* structures are allocated only one each; they are protected by
 * sc_ctrl_inuse, which must only be accessed at splnet
 *
 * metadata headers for received frames are stored at the start of the
 * rx mbufs.
 */
/*
 * dynamically allocated memory is used for:
 *   sc_rx_dmamaps[slot]:	bus_dmamap_t array for received payload
 *   sc_tx_dmamaps[slot]:	bus_dmamap_t array for sent payload
 *   sc_rx_mbufs[slot]:		mbuf pointer array for received frames
 *   sc_tx_mbufs[slot]:		mbuf pointer array for sent frames
 */
int
vio_alloc_mem(struct vio_softc *sc)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int allocsize, r, i, txsize;
	unsigned int offset = 0;
	int rxqsize, txqsize;
	caddr_t kva;

	rxqsize = vsc->sc_vqs[0].vq_num;
	txqsize = vsc->sc_vqs[1].vq_num;

	/*
	 * For simplicity, we always allocate the full virtio_net_hdr size
	 * even if VIRTIO_NET_F_MRG_RXBUF is not negotiated and
	 * only a part of the memory is ever used.
	 */
	allocsize = sizeof(struct virtio_net_hdr) * txqsize;

	if (vsc->sc_nvqs == 3) {
		allocsize += sizeof(struct virtio_net_ctrl_cmd) * 1;
		allocsize += sizeof(struct virtio_net_ctrl_status) * 1;
		allocsize += sizeof(struct virtio_net_ctrl_rx) * 1;
		allocsize += VIO_CTRL_MAC_INFO_SIZE;
	}
	sc->sc_dma_size = allocsize;

	if (vio_alloc_dmamem(sc) != 0) {
		printf("unable to allocate dma region\n");
		return  -1;
	}

	kva = sc->sc_dma_kva;
	sc->sc_tx_hdrs = (struct virtio_net_hdr*)(kva + offset);
	offset += sizeof(struct virtio_net_hdr) * txqsize;
	if (vsc->sc_nvqs == 3) {
		sc->sc_ctrl_cmd = (void*)(kva + offset);
		offset += sizeof(*sc->sc_ctrl_cmd);
		sc->sc_ctrl_status = (void*)(kva + offset);
		offset += sizeof(*sc->sc_ctrl_status);
		sc->sc_ctrl_rx = (void*)(kva + offset);
		offset += sizeof(*sc->sc_ctrl_rx);
		sc->sc_ctrl_mac_tbl_uc = (void*)(kva + offset);
		offset += sizeof(*sc->sc_ctrl_mac_tbl_uc) +
		    ETHER_ADDR_LEN * VIRTIO_NET_CTRL_MAC_UC_ENTRIES;
		sc->sc_ctrl_mac_tbl_mc = (void*)(kva + offset);
	}

	sc->sc_arrays = mallocarray(rxqsize + txqsize,
	    2 * sizeof(bus_dmamap_t) + sizeof(struct mbuf *), M_DEVBUF,
	    M_WAITOK | M_CANFAIL | M_ZERO);
	if (sc->sc_arrays == NULL) {
		printf("unable to allocate mem for dmamaps\n");
		goto err_hdr;
	}
	allocsize = (rxqsize + txqsize) *
	    (2 * sizeof(bus_dmamap_t) + sizeof(struct mbuf *));

	sc->sc_tx_dmamaps = sc->sc_arrays + rxqsize;
	sc->sc_rx_mbufs = (void*) (sc->sc_tx_dmamaps + txqsize);
	sc->sc_tx_mbufs = sc->sc_rx_mbufs + rxqsize;

	for (i = 0; i < rxqsize; i++) {
		r = bus_dmamap_create(vsc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0,
		    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &sc->sc_rx_dmamaps[i]);
		if (r != 0)
			goto err_reqs;
	}

	txsize = ifp->if_hardmtu + sc->sc_hdr_size + ETHER_HDR_LEN;
	for (i = 0; i < txqsize; i++) {
		r = bus_dmamap_create(vsc->sc_dmat, txsize,
		    VIRTIO_NET_TX_MAXNSEGS, txsize, 0,
		    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
		    &sc->sc_tx_dmamaps[i]);
		if (r != 0)
			goto err_reqs;
	}

	return 0;

err_reqs:
	printf("dmamap creation failed, error %d\n", r);
	for (i = 0; i < txqsize; i++) {
		if (sc->sc_tx_dmamaps[i])
			bus_dmamap_destroy(vsc->sc_dmat, sc->sc_tx_dmamaps[i]);
	}
	for (i = 0; i < rxqsize; i++) {
		if (sc->sc_tx_dmamaps[i])
			bus_dmamap_destroy(vsc->sc_dmat, sc->sc_rx_dmamaps[i]);
	}
	if (sc->sc_arrays) {
		free(sc->sc_arrays, M_DEVBUF, 0);
		sc->sc_arrays = 0;
	}
err_hdr:
	vio_free_dmamem(sc);
	return -1;
}

void
vio_get_lladr(struct arpcom *ac, struct virtio_softc *vsc)
{
	int i;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		ac->ac_enaddr[i] = virtio_read_device_config_1(vsc,
		    VIRTIO_NET_CONFIG_MAC + i);
	}
}

void
vio_put_lladr(struct arpcom *ac, struct virtio_softc *vsc)
{
	int i;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		virtio_write_device_config_1(vsc, VIRTIO_NET_CONFIG_MAC + i,
		     ac->ac_enaddr[i]);
	}
}

void
vio_attach(struct device *parent, struct device *self, void *aux)
{
	struct vio_softc *sc = (struct vio_softc *)self;
	struct virtio_softc *vsc = (struct virtio_softc *)parent;
	uint32_t features;
	int i;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (vsc->sc_child != NULL) {
		printf(": child already attached for %s; something wrong...\n",
		       parent->dv_xname);
		return;
	}

	sc->sc_virtio = vsc;

	vsc->sc_child = self;
	vsc->sc_ipl = IPL_NET;
	vsc->sc_vqs = &sc->sc_vq[0];
	vsc->sc_config_change = 0;
	vsc->sc_intrhand = virtio_vq_intr;

	features = VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS |
	    VIRTIO_NET_F_CTRL_VQ | VIRTIO_NET_F_CTRL_RX |
	    VIRTIO_NET_F_MRG_RXBUF;
	/*
	 * VIRTIO_F_RING_EVENT_IDX can be switched off by setting bit 2 in the
	 * driver flags, see config(8)
	 */
	if (!(sc->sc_dev.dv_cfdata->cf_flags & 2) &&
	    !(vsc->sc_dev.dv_cfdata->cf_flags & 2))
		features |= VIRTIO_F_RING_EVENT_IDX;
	else
		printf(": RingEventIdx disabled by UKC");

	features = virtio_negotiate_features(vsc, features,
	    virtio_net_feature_names);
	if (features & VIRTIO_NET_F_MAC) {
		vio_get_lladr(&sc->sc_ac, vsc);
	} else {
		ether_fakeaddr(ifp);
		vio_put_lladr(&sc->sc_ac, vsc);
	}
	printf(": address %s\n", ether_sprintf(sc->sc_ac.ac_enaddr));

	if (features & VIRTIO_NET_F_MRG_RXBUF) {
		sc->sc_hdr_size = sizeof(struct virtio_net_hdr);
		ifp->if_hardmtu = 16000; /* arbitrary limit */
	} else {
		sc->sc_hdr_size = offsetof(struct virtio_net_hdr, num_buffers);
		ifp->if_hardmtu = MCLBYTES - sc->sc_hdr_size - ETHER_HDR_LEN;
	}

	if (virtio_alloc_vq(vsc, &sc->sc_vq[VQRX], 0, MCLBYTES, 2, "rx") != 0)
		goto err;
	vsc->sc_nvqs = 1;
	sc->sc_vq[VQRX].vq_done = vio_rx_intr;
	if (virtio_alloc_vq(vsc, &sc->sc_vq[VQTX], 1,
	    sc->sc_hdr_size + ifp->if_hardmtu + ETHER_HDR_LEN,
	    VIRTIO_NET_TX_MAXNSEGS + 1, "tx") != 0) {
		goto err;
	}
	vsc->sc_nvqs = 2;
	sc->sc_vq[VQTX].vq_done = vio_tx_intr;
	virtio_start_vq_intr(vsc, &sc->sc_vq[VQRX]);
	if (features & VIRTIO_F_RING_EVENT_IDX)
		virtio_postpone_intr_far(&sc->sc_vq[VQTX]);
	else
		virtio_stop_vq_intr(vsc, &sc->sc_vq[VQTX]);
	if ((features & VIRTIO_NET_F_CTRL_VQ)
	    && (features & VIRTIO_NET_F_CTRL_RX)) {
		if (virtio_alloc_vq(vsc, &sc->sc_vq[VQCTL], 2, NBPG, 1,
		    "control") == 0) {
			sc->sc_vq[VQCTL].vq_done = vio_ctrleof;
			virtio_start_vq_intr(vsc, &sc->sc_vq[VQCTL]);
			vsc->sc_nvqs = 3;
		}
	}

	if (vio_alloc_mem(sc) < 0)
		goto err;

	strlcpy(ifp->if_xname, self->dv_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = vio_start;
	ifp->if_ioctl = vio_ioctl;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, vsc->sc_vqs[1].vq_num - 1);
	IFQ_SET_READY(&ifp->if_snd);
	ifmedia_init(&sc->sc_media, 0, vio_media_change, vio_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);
	vsc->sc_config_change = vio_config_change;
	timeout_set(&sc->sc_txtick, vio_txtick, &sc->sc_vq[VQTX]);
	timeout_set(&sc->sc_rxtick, vio_rxtick, &sc->sc_vq[VQRX]);

	if_attach(ifp);
	ether_ifattach(ifp);

	return;

err:
	for (i = 0; i < vsc->sc_nvqs; i++)
		virtio_free_vq(vsc, &sc->sc_vq[i]);
	vsc->sc_nvqs = 0;
	vsc->sc_child = VIRTIO_CHILD_ERROR;
	return;
}

/* check link status */
void
vio_link_state(struct ifnet *ifp)
{
	struct vio_softc *sc = ifp->if_softc;
	struct virtio_softc *vsc = sc->sc_virtio;
	int link_state = LINK_STATE_FULL_DUPLEX;

	if (vsc->sc_features & VIRTIO_NET_F_STATUS) {
		int status = virtio_read_device_config_2(vsc,
		    VIRTIO_NET_CONFIG_STATUS);
		if (!(status & VIRTIO_NET_S_LINK_UP))
			link_state = LINK_STATE_DOWN;
	}
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}

int
vio_config_change(struct virtio_softc *vsc)
{
	struct vio_softc *sc = (struct vio_softc *)vsc->sc_child;
	vio_link_state(&sc->sc_ac.ac_if);
	return 1;
}

int
vio_media_change(struct ifnet *ifp)
{
	/* Ignore */
	return (0);
}

void
vio_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID;

	vio_link_state(ifp);
	if (LINK_STATE_IS_UP(ifp->if_link_state) && ifp->if_flags & IFF_UP)
		imr->ifm_status |= IFM_ACTIVE|IFM_FDX;
}

/*
 * Interface functions for ifnet
 */
int
vio_init(struct ifnet *ifp)
{
	struct vio_softc *sc = ifp->if_softc;

	vio_stop(ifp, 0);
	if_rxr_init(&sc->sc_rx_ring, 2 * ((ifp->if_hardmtu / MCLBYTES) + 1),
	    sc->sc_vq[VQRX].vq_num);
	vio_populate_rx_mbufs(sc);
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	vio_iff(sc);
	vio_link_state(ifp);
	return 0;
}

void
vio_stop(struct ifnet *ifp, int disable)
{
	struct vio_softc *sc = ifp->if_softc;
	struct virtio_softc *vsc = sc->sc_virtio;

	timeout_del(&sc->sc_txtick);
	timeout_del(&sc->sc_rxtick);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	/* only way to stop I/O and DMA is resetting... */
	virtio_reset(vsc);
	vio_rxeof(sc);
	if (vsc->sc_nvqs >= 3)
		vio_ctrleof(&sc->sc_vq[VQCTL]);
	vio_tx_drain(sc);
	if (disable)
		vio_rx_drain(sc);

	virtio_reinit_start(vsc);
	virtio_negotiate_features(vsc, vsc->sc_features, NULL);
	virtio_start_vq_intr(vsc, &sc->sc_vq[VQRX]);
	virtio_stop_vq_intr(vsc, &sc->sc_vq[VQTX]);
	if (vsc->sc_nvqs >= 3)
		virtio_start_vq_intr(vsc, &sc->sc_vq[VQCTL]);
	virtio_reinit_end(vsc);
	if (vsc->sc_nvqs >= 3) {
		if (sc->sc_ctrl_inuse != FREE)
			sc->sc_ctrl_inuse = RESET;
		wakeup(&sc->sc_ctrl_inuse);
	}
}

void
vio_start(struct ifnet *ifp)
{
	struct vio_softc *sc = ifp->if_softc;
	struct virtio_softc *vsc = sc->sc_virtio;
	struct virtqueue *vq = &sc->sc_vq[VQTX];
	struct mbuf *m;
	int queued = 0;

	vio_txeof(vq);

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

again:
	for (;;) {
		int slot, r;
		struct virtio_net_hdr *hdr;

		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		r = virtio_enqueue_prep(vq, &slot);
		if (r == EAGAIN) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		if (r != 0)
			panic("enqueue_prep for a tx buffer: %d", r);
		r = vio_encap(sc, slot, m, &sc->sc_tx_mbufs[slot]);
		if (r != 0) {
#if VIRTIO_DEBUG
			if (r != ENOBUFS)
				printf("%s: error %d\n", __func__, r);
#endif
			virtio_enqueue_abort(vq, slot);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		r = virtio_enqueue_reserve(vq, slot,
		    sc->sc_tx_dmamaps[slot]->dm_nsegs + 1);
		if (r != 0) {
			bus_dmamap_unload(vsc->sc_dmat,
			    sc->sc_tx_dmamaps[slot]);
			sc->sc_tx_mbufs[slot] = NULL;
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m != sc->sc_tx_mbufs[slot]) {
			m_freem(m);
			m = sc->sc_tx_mbufs[slot];
		}

		hdr = &sc->sc_tx_hdrs[slot];
		memset(hdr, 0, sc->sc_hdr_size);
		bus_dmamap_sync(vsc->sc_dmat, sc->sc_tx_dmamaps[slot], 0,
		    sc->sc_tx_dmamaps[slot]->dm_mapsize, BUS_DMASYNC_PREWRITE);
		VIO_DMAMEM_SYNC(vsc, sc, hdr, sc->sc_hdr_size,
		    BUS_DMASYNC_PREWRITE);
		VIO_DMAMEM_ENQUEUE(sc, vq, slot, hdr, sc->sc_hdr_size, 1);
		virtio_enqueue(vq, slot, sc->sc_tx_dmamaps[slot], 1);
		virtio_enqueue_commit(vsc, vq, slot, 0);
		queued++;
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	}
	if (ifp->if_flags & IFF_OACTIVE) {
		int r;
		if (vsc->sc_features & VIRTIO_F_RING_EVENT_IDX)
			r = virtio_postpone_intr_smart(&sc->sc_vq[VQTX]);
		else
			r = virtio_start_vq_intr(vsc, &sc->sc_vq[VQTX]);
		if (r) {
			vio_txeof(vq);
			goto again;
		}
	}

	if (queued > 0) {
		virtio_notify(vsc, vq);
		timeout_add_sec(&sc->sc_txtick, 1);
	}
}

#if VIRTIO_DEBUG
void
vio_dump(struct vio_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct virtio_softc *vsc = sc->sc_virtio;

	printf("%s status dump:\n", ifp->if_xname);
	printf("TX virtqueue:\n");
	virtio_vq_dump(&vsc->sc_vqs[VQTX]);
	printf("tx tick active: %d\n", !timeout_triggered(&sc->sc_txtick));
	printf("rx tick active: %d\n", !timeout_triggered(&sc->sc_rxtick));
	printf("RX virtqueue:\n");
	virtio_vq_dump(&vsc->sc_vqs[VQRX]);
	if (vsc->sc_nvqs == 3) {
		printf("CTL virtqueue:\n");
		virtio_vq_dump(&vsc->sc_vqs[VQCTL]);
		printf("ctrl_inuse: %d\n", sc->sc_ctrl_inuse);
	}
}
#endif

int
vio_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vio_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, r = 0;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			vio_init(ifp);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
#if VIRTIO_DEBUG
			if (ifp->if_flags & IFF_DEBUG)
				vio_dump(sc);
#endif
			if (ifp->if_flags & IFF_RUNNING)
				r = ENETRESET;
			else
				vio_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				vio_stop(ifp, 1);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		r = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	case SIOCGIFRXR:
		r = if_rxr_ioctl((struct if_rxrinfo *)ifr->ifr_data,
		    NULL, MCLBYTES, &sc->sc_rx_ring);
		break;
	default:
		r = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	if (r == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			vio_iff(sc);
		r = 0;
	}
	splx(s);
	return r;
}

/*
 * Recieve implementation
 */
/* allocate and initialize a mbuf for receive */
int
vio_add_rx_mbuf(struct vio_softc *sc, int i)
{
	struct mbuf *m;
	int r;

	m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES);
	if (m == NULL)
		return ENOBUFS;
	sc->sc_rx_mbufs[i] = m;
	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
	r = bus_dmamap_load_mbuf(sc->sc_virtio->sc_dmat, sc->sc_rx_dmamaps[i],
	    m, BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (r) {
		m_freem(m);
		sc->sc_rx_mbufs[i] = 0;
		return r;
	}

	return 0;
}

/* free a mbuf for receive */
void
vio_free_rx_mbuf(struct vio_softc *sc, int i)
{
	bus_dmamap_unload(sc->sc_virtio->sc_dmat, sc->sc_rx_dmamaps[i]);
	m_freem(sc->sc_rx_mbufs[i]);
	sc->sc_rx_mbufs[i] = NULL;
}

/* add mbufs for all the empty receive slots */
void
vio_populate_rx_mbufs(struct vio_softc *sc)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	int r, done = 0;
	u_int slots;
	struct virtqueue *vq = &sc->sc_vq[VQRX];
	int mrg_rxbuf = VIO_HAVE_MRG_RXBUF(sc);

	for (slots = if_rxr_get(&sc->sc_rx_ring, vq->vq_num);
	    slots > 0; slots--) {
		int slot;
		r = virtio_enqueue_prep(vq, &slot);
		if (r == EAGAIN)
			break;
		if (r != 0)
			panic("enqueue_prep for rx buffers: %d", r);
		if (sc->sc_rx_mbufs[slot] == NULL) {
			r = vio_add_rx_mbuf(sc, slot);
			if (r != 0) {
				virtio_enqueue_abort(vq, slot);
				break;
			}
		}
		r = virtio_enqueue_reserve(vq, slot,
		    sc->sc_rx_dmamaps[slot]->dm_nsegs + (mrg_rxbuf ? 0 : 1));
		if (r != 0) {
			vio_free_rx_mbuf(sc, slot);
			break;
		}
		bus_dmamap_sync(vsc->sc_dmat, sc->sc_rx_dmamaps[slot], 0,
		    MCLBYTES, BUS_DMASYNC_PREREAD);
		if (mrg_rxbuf) {
			virtio_enqueue(vq, slot, sc->sc_rx_dmamaps[slot], 0);
		} else {
			/*
			 * Buggy kvm wants a buffer of exactly the size of
			 * the header in this case, so we have to split in
			 * two.
			 */
			virtio_enqueue_p(vq, slot, sc->sc_rx_dmamaps[slot],
			    0, sc->sc_hdr_size, 0);
			virtio_enqueue_p(vq, slot, sc->sc_rx_dmamaps[slot],
			    sc->sc_hdr_size, MCLBYTES - sc->sc_hdr_size, 0);
		}
		virtio_enqueue_commit(vsc, vq, slot, 0);
		done = 1;
	}
	if_rxr_put(&sc->sc_rx_ring, slots);

	if (done)
		virtio_notify(vsc, vq);
	if (vq->vq_used_idx != vq->vq_avail_idx)
		timeout_del(&sc->sc_rxtick);
	else
		timeout_add_sec(&sc->sc_rxtick, 1);
}

/* dequeue received packets */
int
vio_rxeof(struct vio_softc *sc)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	struct virtqueue *vq = &sc->sc_vq[VQRX];
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf *m, *m0 = NULL, *mlast;
	int r = 0;
	int slot, len, bufs_left;
	struct virtio_net_hdr *hdr;

	while (virtio_dequeue(vsc, vq, &slot, &len) == 0) {
		r = 1;
		bus_dmamap_sync(vsc->sc_dmat, sc->sc_rx_dmamaps[slot], 0,
		    MCLBYTES, BUS_DMASYNC_POSTREAD);
		m = sc->sc_rx_mbufs[slot];
		KASSERT(m != NULL);
		bus_dmamap_unload(vsc->sc_dmat, sc->sc_rx_dmamaps[slot]);
		sc->sc_rx_mbufs[slot] = NULL;
		virtio_dequeue_commit(vq, slot);
		if_rxr_put(&sc->sc_rx_ring, 1);
		m->m_pkthdr.rcvif = ifp;
		m->m_len = m->m_pkthdr.len = len;
		m->m_pkthdr.csum_flags = 0;
		if (m0 == NULL) {
			hdr = mtod(m, struct virtio_net_hdr *);
			m_adj(m, sc->sc_hdr_size);
			m0 = mlast = m;
			if (VIO_HAVE_MRG_RXBUF(sc))
				bufs_left = hdr->num_buffers - 1;
			else
				bufs_left = 0;
		}
		else {
			m->m_flags &= ~M_PKTHDR;
			m0->m_pkthdr.len += m->m_len;
			mlast->m_next = m;
			mlast = m;
			bufs_left--;
		}

		if (bufs_left == 0) {
			ifp->if_ipackets++;
#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_IN);
#endif
			ether_input_mbuf(ifp, m0);
			m0 = NULL;
		}
	}
	if (m0 != NULL) {
		DBGPRINT("expected %d buffers, got %d", (int)hdr->num_buffers,
		    (int)hdr->num_buffers - bufs_left);
		ifp->if_ierrors++;
		m_freem(m0);
	}
	return r;
}

int
vio_rx_intr(struct virtqueue *vq)
{
	struct virtio_softc *vsc = vq->vq_owner;
	struct vio_softc *sc = (struct vio_softc *)vsc->sc_child;
	int r, sum = 0;

again:
	r = vio_rxeof(sc);
	sum += r;
	if (r) {
		vio_populate_rx_mbufs(sc);
		/* set used event index to the next slot */
		if (vsc->sc_features & VIRTIO_F_RING_EVENT_IDX) {
			if (virtio_start_vq_intr(vq->vq_owner, vq))
				goto again;
		}
	}

	return sum;
}

void
vio_rxtick(void *arg)
{
	struct virtqueue *vq = arg;
	struct virtio_softc *vsc = vq->vq_owner;
	struct vio_softc *sc = (struct vio_softc *)vsc->sc_child;
	int s;

	s = splnet();
	vio_populate_rx_mbufs(sc);
	splx(s);
}

/* free all the mbufs; called from if_stop(disable) */
void
vio_rx_drain(struct vio_softc *sc)
{
	struct virtqueue *vq = &sc->sc_vq[VQRX];
	int i;

	for (i = 0; i < vq->vq_num; i++) {
		if (sc->sc_rx_mbufs[i] == NULL)
			continue;
		vio_free_rx_mbuf(sc, i);
	}
}

/*
 * Transmition implementation
 */
/* actual transmission is done in if_start */
/* tx interrupt; dequeue and free mbufs */
/*
 * tx interrupt is actually disabled unless the tx queue is full, i.e.
 * IFF_OACTIVE is set. vio_txtick is used to make sure that mbufs
 * are dequeued and freed even if no further transfer happens.
 */
int
vio_tx_intr(struct virtqueue *vq)
{
	struct virtio_softc *vsc = vq->vq_owner;
	struct vio_softc *sc = (struct vio_softc *)vsc->sc_child;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int r;

	r = vio_txeof(vq);
	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		vio_start(ifp);
	return r;
}

void
vio_txtick(void *arg)
{
	struct virtqueue *vq = arg;
	int s = splnet();
	vio_tx_intr(vq);
	splx(s);
}

int
vio_txeof(struct virtqueue *vq)
{
	struct virtio_softc *vsc = vq->vq_owner;
	struct vio_softc *sc = (struct vio_softc *)vsc->sc_child;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf *m;
	int r = 0;
	int slot, len;

	while (virtio_dequeue(vsc, vq, &slot, &len) == 0) {
		struct virtio_net_hdr *hdr = &sc->sc_tx_hdrs[slot];
		r++;
		VIO_DMAMEM_SYNC(vsc, sc, hdr, sc->sc_hdr_size,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(vsc->sc_dmat, sc->sc_tx_dmamaps[slot], 0,
		    sc->sc_tx_dmamaps[slot]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		m = sc->sc_tx_mbufs[slot];
		bus_dmamap_unload(vsc->sc_dmat, sc->sc_tx_dmamaps[slot]);
		sc->sc_tx_mbufs[slot] = 0;
		virtio_dequeue_commit(vq, slot);
		ifp->if_opackets++;
		m_freem(m);
	}

	if (r) {
		ifp->if_flags &= ~IFF_OACTIVE;
		virtio_stop_vq_intr(vsc, &sc->sc_vq[VQTX]);
	}
	if (vq->vq_used_idx == vq->vq_avail_idx)
		timeout_del(&sc->sc_txtick);
	else if (r)
		timeout_add_sec(&sc->sc_txtick, 1);
	return r;
}

int
vio_encap(struct vio_softc *sc, int slot, struct mbuf *m,
	      struct mbuf **mnew)
{
	struct virtio_softc	*vsc = sc->sc_virtio;
	bus_dmamap_t		 dmap= sc->sc_tx_dmamaps[slot];
	struct mbuf		*m0 = NULL;
	int			 r;

	r = bus_dmamap_load_mbuf(vsc->sc_dmat, dmap, m,
	    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
	if (r == 0) {
		*mnew = m;
		return r;
	}
	if (r != EFBIG)
		return r;
	/* EFBIG: mbuf chain is too fragmented */
	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return ENOBUFS;
	if (m->m_pkthdr.len > MHLEN) {
		MCLGETI(m0, M_DONTWAIT, NULL, m->m_pkthdr.len);
		if (!(m0->m_flags & M_EXT)) {
			m_freem(m0);
			return ENOBUFS;
		}
	}
	m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
	m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
	r = bus_dmamap_load_mbuf(vsc->sc_dmat, dmap, m0,
	    BUS_DMA_NOWAIT|BUS_DMA_WRITE);
	if (r != 0) {
		m_freem(m0);
		printf("%s: tx dmamap load error %d\n", sc->sc_dev.dv_xname,
		    r);
		return ENOBUFS;
	}
	*mnew = m0;
	return 0;
}

/* free all the mbufs already put on vq; called from if_stop(disable) */
void
vio_tx_drain(struct vio_softc *sc)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	struct virtqueue *vq = &sc->sc_vq[VQTX];
	int i;

	for (i = 0; i < vq->vq_num; i++) {
		if (sc->sc_tx_mbufs[i] == NULL)
			continue;
		bus_dmamap_unload(vsc->sc_dmat, sc->sc_tx_dmamaps[i]);
		m_freem(sc->sc_tx_mbufs[i]);
		sc->sc_tx_mbufs[i] = NULL;
	}
}

/*
 * Control vq
 */
/* issue a VIRTIO_NET_CTRL_RX class command and wait for completion */
int
vio_ctrl_rx(struct vio_softc *sc, int cmd, int onoff)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	struct virtqueue *vq = &sc->sc_vq[VQCTL];
	int r, slot;

	if (vsc->sc_nvqs < 3)
		return ENOTSUP;

	splassert(IPL_NET);
	vio_wait_ctrl(sc);

	sc->sc_ctrl_cmd->class = VIRTIO_NET_CTRL_RX;
	sc->sc_ctrl_cmd->command = cmd;
	sc->sc_ctrl_rx->onoff = onoff;

	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_cmd,
	    sizeof(*sc->sc_ctrl_cmd), BUS_DMASYNC_PREWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_rx,
	    sizeof(*sc->sc_ctrl_rx), BUS_DMASYNC_PREWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_status,
	    sizeof(*sc->sc_ctrl_status), BUS_DMASYNC_PREREAD);

	r = virtio_enqueue_prep(vq, &slot);
	if (r != 0)
		panic("%s: control vq busy!?", sc->sc_dev.dv_xname);
	r = virtio_enqueue_reserve(vq, slot, 3);
	if (r != 0)
		panic("%s: control vq busy!?", sc->sc_dev.dv_xname);
	VIO_DMAMEM_ENQUEUE(sc, vq, slot, sc->sc_ctrl_cmd,
	    sizeof(*sc->sc_ctrl_cmd), 1);
	VIO_DMAMEM_ENQUEUE(sc, vq, slot, sc->sc_ctrl_rx,
	    sizeof(*sc->sc_ctrl_rx), 1);
	VIO_DMAMEM_ENQUEUE(sc, vq, slot, sc->sc_ctrl_status,
	    sizeof(*sc->sc_ctrl_status), 0);
	virtio_enqueue_commit(vsc, vq, slot, 1);

	if (vio_wait_ctrl_done(sc)) {
		r = EIO;
		goto out;
	}

	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_cmd,
	    sizeof(*sc->sc_ctrl_cmd), BUS_DMASYNC_POSTWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_rx,
	    sizeof(*sc->sc_ctrl_rx), BUS_DMASYNC_POSTWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_status,
	    sizeof(*sc->sc_ctrl_status), BUS_DMASYNC_POSTREAD);

	if (sc->sc_ctrl_status->ack == VIRTIO_NET_OK) {
		r = 0;
	} else {
		printf("%s: ctrl cmd %d failed\n", sc->sc_dev.dv_xname, cmd);
		r = EIO;
	}

	DBGPRINT("cmd %d %d: %d", cmd, (int)onoff, r);
out:
	vio_ctrl_wakeup(sc, FREE);
	return r;
}

void
vio_wait_ctrl(struct vio_softc *sc)
{
	while (sc->sc_ctrl_inuse != FREE)
		tsleep(&sc->sc_ctrl_inuse, IPL_NET, "vio_wait", 0);
	sc->sc_ctrl_inuse = INUSE;
}

int
vio_wait_ctrl_done(struct vio_softc *sc)
{
	int r = 0;
	while (sc->sc_ctrl_inuse != DONE && sc->sc_ctrl_inuse != RESET) {
		if (sc->sc_ctrl_inuse == RESET) {
			r = 1;
			break;
		}
		tsleep(&sc->sc_ctrl_inuse, IPL_NET, "vio_wait", 0);
	}
	return r;
}

void
vio_ctrl_wakeup(struct vio_softc *sc, enum vio_ctrl_state new)
{
	sc->sc_ctrl_inuse = new;
	wakeup(&sc->sc_ctrl_inuse);
}

int
vio_ctrleof(struct virtqueue *vq)
{
	struct virtio_softc *vsc = vq->vq_owner;
	struct vio_softc *sc = (struct vio_softc *)vsc->sc_child;
	int r = 0, ret, slot;

again:
	ret = virtio_dequeue(vsc, vq, &slot, NULL);
	if (ret == ENOENT)
		return r;
	virtio_dequeue_commit(vq, slot);
	r++;
	vio_ctrl_wakeup(sc, DONE);
	if (virtio_start_vq_intr(vsc, vq))
		goto again;

	return r;
}

/* issue VIRTIO_NET_CTRL_MAC_TABLE_SET command and wait for completion */
int
vio_set_rx_filter(struct vio_softc *sc)
{
	/* filter already set in sc_ctrl_mac_tbl */
	struct virtio_softc *vsc = sc->sc_virtio;
	struct virtqueue *vq = &sc->sc_vq[VQCTL];
	int r, slot;

	splassert(IPL_NET);

	if (vsc->sc_nvqs < 3)
		return ENOTSUP;

	vio_wait_ctrl(sc);

	sc->sc_ctrl_cmd->class = VIRTIO_NET_CTRL_MAC;
	sc->sc_ctrl_cmd->command = VIRTIO_NET_CTRL_MAC_TABLE_SET;

	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_cmd,
	    sizeof(*sc->sc_ctrl_cmd), BUS_DMASYNC_PREWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_mac_info,
	    VIO_CTRL_MAC_INFO_SIZE, BUS_DMASYNC_PREWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_status,
	    sizeof(*sc->sc_ctrl_status), BUS_DMASYNC_PREREAD);

	r = virtio_enqueue_prep(vq, &slot);
	if (r != 0)
		panic("%s: control vq busy!?", sc->sc_dev.dv_xname);
	r = virtio_enqueue_reserve(vq, slot, 4);
	if (r != 0)
		panic("%s: control vq busy!?", sc->sc_dev.dv_xname);
	VIO_DMAMEM_ENQUEUE(sc, vq, slot, sc->sc_ctrl_cmd,
	    sizeof(*sc->sc_ctrl_cmd), 1);
	VIO_DMAMEM_ENQUEUE(sc, vq, slot, sc->sc_ctrl_mac_tbl_uc,
	    sizeof(*sc->sc_ctrl_mac_tbl_uc) +
	    sc->sc_ctrl_mac_tbl_uc->nentries * ETHER_ADDR_LEN, 1);
	VIO_DMAMEM_ENQUEUE(sc, vq, slot, sc->sc_ctrl_mac_tbl_mc,
	    sizeof(*sc->sc_ctrl_mac_tbl_mc) +
	    sc->sc_ctrl_mac_tbl_mc->nentries * ETHER_ADDR_LEN, 1);
	VIO_DMAMEM_ENQUEUE(sc, vq, slot, sc->sc_ctrl_status,
	    sizeof(*sc->sc_ctrl_status), 0);
	virtio_enqueue_commit(vsc, vq, slot, 1);

	if (vio_wait_ctrl_done(sc)) {
		r = EIO;
		goto out;
	}

	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_cmd,
	    sizeof(*sc->sc_ctrl_cmd), BUS_DMASYNC_POSTWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_mac_info,
	    VIO_CTRL_MAC_INFO_SIZE, BUS_DMASYNC_POSTWRITE);
	VIO_DMAMEM_SYNC(vsc, sc, sc->sc_ctrl_status,
	    sizeof(*sc->sc_ctrl_status), BUS_DMASYNC_POSTREAD);

	if (sc->sc_ctrl_status->ack == VIRTIO_NET_OK) {
		r = 0;
	} else {
		printf("%s: failed setting rx filter\n", sc->sc_dev.dv_xname);
		r = EIO;
	}

out:
	vio_ctrl_wakeup(sc, FREE);
	return r;
}

void
vio_iff(struct vio_softc *sc)
{
	struct virtio_softc *vsc = sc->sc_virtio;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct arpcom *ac = &sc->sc_ac;
	struct ether_multi *enm;
	struct ether_multistep step;
	int nentries = 0;
	int promisc = 0, allmulti = 0, rxfilter = 0;
	int r;

	splassert(IPL_NET);

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (vsc->sc_nvqs < 3) {
		/* no ctrl vq; always promisc */
		ifp->if_flags |= IFF_ALLMULTI | IFF_PROMISC;
		return;
	}

	if (sc->sc_dev.dv_cfdata->cf_flags & CONFFLAG_QEMU_VLAN_BUG)
		ifp->if_flags |= IFF_PROMISC;

	if (ifp->if_flags & IFF_PROMISC || ac->ac_multirangecnt > 0 ||
	    ac->ac_multicnt >= VIRTIO_NET_CTRL_MAC_MC_ENTRIES) {
		ifp->if_flags |= IFF_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			promisc = 1;
		else
			allmulti = 1;
	} else {
		rxfilter = 1;

		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			memcpy(sc->sc_ctrl_mac_tbl_mc->macs[nentries++],
			    enm->enm_addrlo, ETHER_ADDR_LEN);

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	/* set unicast address, VirtualBox wants that */
	memcpy(sc->sc_ctrl_mac_tbl_uc->macs[0], ac->ac_enaddr, ETHER_ADDR_LEN);
	sc->sc_ctrl_mac_tbl_uc->nentries = 1;

	if (rxfilter) {
		sc->sc_ctrl_mac_tbl_mc->nentries = nentries;
		r = vio_set_rx_filter(sc);
		if (r != 0) {
			rxfilter = 0;
			allmulti = 1; /* fallback */
		}
	} else {
		sc->sc_ctrl_mac_tbl_mc->nentries = 0;
		vio_set_rx_filter(sc);
	}

	if (allmulti) {
		r = vio_ctrl_rx(sc, VIRTIO_NET_CTRL_RX_ALLMULTI, 1);
		if (r != 0) {
			allmulti = 0;
			promisc = 1; /* fallback */
		}
	} else {
		vio_ctrl_rx(sc, VIRTIO_NET_CTRL_RX_ALLMULTI, 0);
	}

	vio_ctrl_rx(sc, VIRTIO_NET_CTRL_RX_PROMISC, promisc);
}
