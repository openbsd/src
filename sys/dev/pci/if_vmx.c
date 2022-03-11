/*	$OpenBSD: if_vmx.c,v 1.69 2022/03/11 18:00:50 mpi Exp $	*/

/*
 * Copyright (c) 2013 Tsubai Masanari
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
#include "kstat.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/intrmap.h>
#include <sys/kstat.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/toeplitz.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <machine/bus.h>

#include <dev/pci/if_vmxreg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define VMX_MAX_QUEUES	MIN(VMXNET3_MAX_TX_QUEUES, VMXNET3_MAX_RX_QUEUES)

#define NTXDESC 512 /* tx ring size */
#define NTXSEGS 8 /* tx descriptors per packet */
#define NRXDESC 512
#define NTXCOMPDESC NTXDESC
#define NRXCOMPDESC (NRXDESC * 2)	/* ring1 + ring2 */

#define VMXNET3_DRIVER_VERSION 0x00010000

#define VMX_TX_GEN	htole32(VMXNET3_TX_GEN_M << VMXNET3_TX_GEN_S)
#define VMX_TXC_GEN	htole32(VMXNET3_TXC_GEN_M << VMXNET3_TXC_GEN_S)
#define VMX_RX_GEN	htole32(VMXNET3_RX_GEN_M << VMXNET3_RX_GEN_S)
#define VMX_RXC_GEN	htole32(VMXNET3_RXC_GEN_M << VMXNET3_RXC_GEN_S)

struct vmxnet3_softc;

struct vmxnet3_txring {
	struct mbuf *m[NTXDESC];
	bus_dmamap_t dmap[NTXDESC];
	struct vmxnet3_txdesc *txd;
	u_int32_t gen;
	u_int prod;
	u_int cons;
};

struct vmxnet3_rxring {
	struct vmxnet3_softc *sc;
	struct mbuf *m[NRXDESC];
	bus_dmamap_t dmap[NRXDESC];
	struct mutex mtx;
	struct if_rxring rxr;
	struct timeout refill;
	struct vmxnet3_rxdesc *rxd;
	u_int fill;
	u_int32_t gen;
	u_int8_t rid;
};

struct vmxnet3_comp_ring {
	union {
		struct vmxnet3_txcompdesc *txcd;
		struct vmxnet3_rxcompdesc *rxcd;
	};
	u_int next;
	u_int32_t gen;
};

struct vmxnet3_txqueue {
	struct vmxnet3_softc *sc; /* sigh */
	struct vmxnet3_txring cmd_ring;
	struct vmxnet3_comp_ring comp_ring;
	struct vmxnet3_txq_shared *ts;
	struct ifqueue *ifq;
	struct kstat *txkstat;
} __aligned(64);

struct vmxnet3_rxqueue {
	struct vmxnet3_softc *sc; /* sigh */
	struct vmxnet3_rxring cmd_ring[2];
	struct vmxnet3_comp_ring comp_ring;
	struct vmxnet3_rxq_shared *rs;
	struct ifiqueue *ifiq;
	struct kstat *rxkstat;
} __aligned(64);

struct vmxnet3_queue {
	struct vmxnet3_txqueue tx;
	struct vmxnet3_rxqueue rx;
	struct vmxnet3_softc *sc;
	char intrname[16];
	void *ih;
	int intr;
};

struct vmxnet3_softc {
	struct device sc_dev;
	struct arpcom sc_arpcom;
	struct ifmedia sc_media;

	bus_space_tag_t	sc_iot0;
	bus_space_tag_t	sc_iot1;
	bus_space_handle_t sc_ioh0;
	bus_space_handle_t sc_ioh1;
	bus_dma_tag_t sc_dmat;
	void *sc_ih;

	int sc_nqueues;
	struct vmxnet3_queue *sc_q;
	struct intrmap *sc_intrmap;

	struct vmxnet3_driver_shared *sc_ds;
	u_int8_t *sc_mcast;
	struct vmxnet3_upt1_rss_conf *sc_rss;

#if NKSTAT > 0
	struct rwlock		sc_kstat_lock;
	struct timeval		sc_kstat_updated;
#endif
};

#define JUMBO_LEN (1024 * 9)
#define DMAADDR(map) ((map)->dm_segs[0].ds_addr)

#define READ_BAR0(sc, reg) bus_space_read_4((sc)->sc_iot0, (sc)->sc_ioh0, reg)
#define READ_BAR1(sc, reg) bus_space_read_4((sc)->sc_iot1, (sc)->sc_ioh1, reg)
#define WRITE_BAR0(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot0, (sc)->sc_ioh0, reg, val)
#define WRITE_BAR1(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot1, (sc)->sc_ioh1, reg, val)
#define WRITE_CMD(sc, cmd) WRITE_BAR1(sc, VMXNET3_BAR1_CMD, cmd)

int vmxnet3_match(struct device *, void *, void *);
void vmxnet3_attach(struct device *, struct device *, void *);
int vmxnet3_dma_init(struct vmxnet3_softc *);
int vmxnet3_alloc_txring(struct vmxnet3_softc *, int, int);
int vmxnet3_alloc_rxring(struct vmxnet3_softc *, int, int);
void vmxnet3_txinit(struct vmxnet3_softc *, struct vmxnet3_txqueue *);
void vmxnet3_rxinit(struct vmxnet3_softc *, struct vmxnet3_rxqueue *);
void vmxnet3_txstop(struct vmxnet3_softc *, struct vmxnet3_txqueue *);
void vmxnet3_rxstop(struct vmxnet3_softc *, struct vmxnet3_rxqueue *);
void vmxnet3_link_state(struct vmxnet3_softc *);
void vmxnet3_enable_all_intrs(struct vmxnet3_softc *);
void vmxnet3_disable_all_intrs(struct vmxnet3_softc *);
int vmxnet3_intr(void *);
int vmxnet3_intr_intx(void *);
int vmxnet3_intr_event(void *);
int vmxnet3_intr_queue(void *);
void vmxnet3_evintr(struct vmxnet3_softc *);
void vmxnet3_txintr(struct vmxnet3_softc *, struct vmxnet3_txqueue *);
void vmxnet3_rxintr(struct vmxnet3_softc *, struct vmxnet3_rxqueue *);
void vmxnet3_rxfill_tick(void *);
void vmxnet3_rxfill(struct vmxnet3_rxring *);
void vmxnet3_iff(struct vmxnet3_softc *);
void vmxnet3_rx_csum(struct vmxnet3_rxcompdesc *, struct mbuf *);
void vmxnet3_stop(struct ifnet *);
void vmxnet3_reset(struct vmxnet3_softc *);
int vmxnet3_init(struct vmxnet3_softc *);
int vmxnet3_ioctl(struct ifnet *, u_long, caddr_t);
void vmxnet3_start(struct ifqueue *);
int vmxnet3_load_mbuf(struct vmxnet3_softc *, struct vmxnet3_txring *,
    struct mbuf **);
void vmxnet3_watchdog(struct ifnet *);
void vmxnet3_media_status(struct ifnet *, struct ifmediareq *);
int vmxnet3_media_change(struct ifnet *);
void *vmxnet3_dma_allocmem(struct vmxnet3_softc *, u_int, u_int, bus_addr_t *);

#if NKSTAT > 0
static void	vmx_kstat_init(struct vmxnet3_softc *);
static void	vmx_kstat_txstats(struct vmxnet3_softc *,
		    struct vmxnet3_txqueue *, int);
static void	vmx_kstat_rxstats(struct vmxnet3_softc *,
		    struct vmxnet3_rxqueue *, int);
#endif /* NKSTAT > 0 */

const struct pci_matchid vmx_devices[] = {
	{ PCI_VENDOR_VMWARE, PCI_PRODUCT_VMWARE_NET_3 }
};

const struct cfattach vmx_ca = {
	sizeof(struct vmxnet3_softc), vmxnet3_match, vmxnet3_attach
};

struct cfdriver vmx_cd = {
	NULL, "vmx", DV_IFNET
};

int
vmxnet3_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, vmx_devices, nitems(vmx_devices)));
}

void
vmxnet3_attach(struct device *parent, struct device *self, void *aux)
{
	struct vmxnet3_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	pci_intr_handle_t ih;
	const char *intrstr;
	u_int memtype, ver, macl, mach, intrcfg;
	u_char enaddr[ETHER_ADDR_LEN];
	int (*isr)(void *);
	int msix = 0;
	int i;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, 0x10);
	if (pci_mapreg_map(pa, 0x10, memtype, 0, &sc->sc_iot0, &sc->sc_ioh0,
	    NULL, NULL, 0)) {
		printf(": failed to map BAR0\n");
		return;
	}
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, 0x14);
	if (pci_mapreg_map(pa, 0x14, memtype, 0, &sc->sc_iot1, &sc->sc_ioh1,
	    NULL, NULL, 0)) {
		printf(": failed to map BAR1\n");
		return;
	}

	ver = READ_BAR1(sc, VMXNET3_BAR1_VRRS);
	if ((ver & 0x1) == 0) {
		printf(": unsupported hardware version 0x%x\n", ver);
		return;
	}
	WRITE_BAR1(sc, VMXNET3_BAR1_VRRS, 1);

	ver = READ_BAR1(sc, VMXNET3_BAR1_UVRS);
	if ((ver & 0x1) == 0) {
		printf(": incompatible UPT version 0x%x\n", ver);
		return;
	}
	WRITE_BAR1(sc, VMXNET3_BAR1_UVRS, 1);

	sc->sc_dmat = pa->pa_dmat;

	WRITE_CMD(sc, VMXNET3_CMD_GET_INTRCFG);
	intrcfg = READ_BAR1(sc, VMXNET3_BAR1_CMD);
	isr = vmxnet3_intr;
	sc->sc_nqueues = 1;

	switch (intrcfg & VMXNET3_INTRCFG_TYPE_MASK) {
	case VMXNET3_INTRCFG_TYPE_AUTO:
	case VMXNET3_INTRCFG_TYPE_MSIX:
		msix = pci_intr_msix_count(pa);
		if (msix > 0) {
			if (pci_intr_map_msix(pa, 0, &ih) == 0) {
				msix--; /* are there spares for tx/rx qs? */
				if (msix == 0)
					break;

				isr = vmxnet3_intr_event;
				sc->sc_intrmap = intrmap_create(&sc->sc_dev,
				    msix, VMX_MAX_QUEUES, INTRMAP_POWEROF2);
				sc->sc_nqueues = intrmap_count(sc->sc_intrmap);
			}
			break;
		}

		/* FALLTHROUGH */
	case VMXNET3_INTRCFG_TYPE_MSI:
		if (pci_intr_map_msi(pa, &ih) == 0)
			break;

		/* FALLTHROUGH */
	case VMXNET3_INTRCFG_TYPE_INTX:
		isr = vmxnet3_intr_intx;
		if (pci_intr_map(pa, &ih) == 0)
			break;

		printf(": failed to map interrupt\n");
		return;
	}
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET | IPL_MPSAFE,
	    isr, sc, self->dv_xname);
	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (intrstr)
		printf(": %s", intrstr);

	sc->sc_q = mallocarray(sc->sc_nqueues, sizeof(*sc->sc_q),
	    M_DEVBUF, M_WAITOK|M_ZERO);

	if (sc->sc_intrmap != NULL) {
		for (i = 0; i < sc->sc_nqueues; i++) {
			struct vmxnet3_queue *q;
			int vec;

			q = &sc->sc_q[i];
			vec = i + 1;
			if (pci_intr_map_msix(pa, vec, &ih) != 0) {
				printf(", failed to map interrupt %d\n", vec);
				return;
			}
			snprintf(q->intrname, sizeof(q->intrname), "%s:%d",
			    self->dv_xname, i);
			q->ih = pci_intr_establish_cpu(pa->pa_pc, ih,
			    IPL_NET | IPL_MPSAFE,
			    intrmap_cpu(sc->sc_intrmap, i),
			    vmxnet3_intr_queue, q, q->intrname);

			q->intr = vec;
			q->sc = sc;
		}
	}

	if (vmxnet3_dma_init(sc)) {
		printf(": failed to setup DMA\n");
		return;
	}

	printf(", %d queue%s", sc->sc_nqueues, sc->sc_nqueues > 1 ? "s" : "");

	WRITE_CMD(sc, VMXNET3_CMD_GET_MACL);
	macl = READ_BAR1(sc, VMXNET3_BAR1_CMD);
	enaddr[0] = macl;
	enaddr[1] = macl >> 8;
	enaddr[2] = macl >> 16;
	enaddr[3] = macl >> 24;
	WRITE_CMD(sc, VMXNET3_CMD_GET_MACH);
	mach = READ_BAR1(sc, VMXNET3_BAR1_CMD);
	enaddr[4] = mach;
	enaddr[5] = mach >> 8;

	WRITE_BAR1(sc, VMXNET3_BAR1_MACL, macl);
	WRITE_BAR1(sc, VMXNET3_BAR1_MACH, mach);
	printf(", address %s\n", ether_sprintf(enaddr));

	bcopy(enaddr, sc->sc_arpcom.ac_enaddr, 6);
	strlcpy(ifp->if_xname, self->dv_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST | IFF_SIMPLEX;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = vmxnet3_ioctl;
	ifp->if_qstart = vmxnet3_start;
	ifp->if_watchdog = vmxnet3_watchdog;
	ifp->if_hardmtu = VMXNET3_MAX_MTU;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	if (sc->sc_ds->upt_features & UPT1_F_CSUM)
		ifp->if_capabilities |= IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
#endif
	if (sc->sc_ds->upt_features & UPT1_F_VLAN)
		ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;

	ifq_set_maxlen(&ifp->if_snd, NTXDESC);

	ifmedia_init(&sc->sc_media, IFM_IMASK, vmxnet3_media_change,
	    vmxnet3_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_10G_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_10G_T, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_1000_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_1000_T, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);
	vmxnet3_link_state(sc);

	if_attach_queues(ifp, sc->sc_nqueues);
	if_attach_iqueues(ifp, sc->sc_nqueues);

#if NKSTAT > 0
	vmx_kstat_init(sc);
#endif

	for (i = 0; i < sc->sc_nqueues; i++) {
		ifp->if_ifqs[i]->ifq_softc = &sc->sc_q[i].tx;
		sc->sc_q[i].tx.ifq = ifp->if_ifqs[i];
		sc->sc_q[i].rx.ifiq = ifp->if_iqs[i];

#if NKSTAT > 0
		vmx_kstat_txstats(sc, &sc->sc_q[i].tx, i);
		vmx_kstat_rxstats(sc, &sc->sc_q[i].rx, i);
#endif
	}
}

int
vmxnet3_dma_init(struct vmxnet3_softc *sc)
{
	struct vmxnet3_driver_shared *ds;
	struct vmxnet3_txq_shared *ts;
	struct vmxnet3_rxq_shared *rs;
	bus_addr_t ds_pa, qs_pa, mcast_pa;
	int i, queue, qs_len, intr;
	u_int major, minor, release_code, rev;

	qs_len = sc->sc_nqueues * (sizeof *ts + sizeof *rs);
	ts = vmxnet3_dma_allocmem(sc, qs_len, VMXNET3_DMADESC_ALIGN, &qs_pa);
	if (ts == NULL)
		return -1;
	for (queue = 0; queue < sc->sc_nqueues; queue++)
		sc->sc_q[queue].tx.ts = ts++;
	rs = (void *)ts;
	for (queue = 0; queue < sc->sc_nqueues; queue++)
		sc->sc_q[queue].rx.rs = rs++;

	for (queue = 0; queue < sc->sc_nqueues; queue++) {
		intr = sc->sc_q[queue].intr;

		if (vmxnet3_alloc_txring(sc, queue, intr))
			return -1;
		if (vmxnet3_alloc_rxring(sc, queue, intr))
			return -1;
	}

	sc->sc_mcast = vmxnet3_dma_allocmem(sc, 682 * ETHER_ADDR_LEN, 32, &mcast_pa);
	if (sc->sc_mcast == NULL)
		return -1;

	ds = vmxnet3_dma_allocmem(sc, sizeof *sc->sc_ds, 8, &ds_pa);
	if (ds == NULL)
		return -1;
	sc->sc_ds = ds;
	ds->magic = VMXNET3_REV1_MAGIC;
	ds->version = VMXNET3_DRIVER_VERSION;

	/*
	 * XXX FreeBSD version uses following values:
	 * (Does the device behavior depend on them?)
	 *
	 * major = __FreeBSD_version / 100000;
	 * minor = (__FreeBSD_version / 1000) % 100;
	 * release_code = (__FreeBSD_version / 100) % 10;
	 * rev = __FreeBSD_version % 100;
	 */
	major = 0;
	minor = 0;
	release_code = 0;
	rev = 0;
#ifdef __LP64__
	ds->guest = release_code << 30 | rev << 22 | major << 14 | minor << 6
	    | VMXNET3_GOS_FREEBSD | VMXNET3_GOS_64BIT;
#else
	ds->guest = release_code << 30 | rev << 22 | major << 14 | minor << 6
	    | VMXNET3_GOS_FREEBSD | VMXNET3_GOS_32BIT;
#endif
	ds->vmxnet3_revision = 1;
	ds->upt_version = 1;
	ds->upt_features = UPT1_F_CSUM | UPT1_F_VLAN;
	ds->driver_data = ~0ULL;
	ds->driver_data_len = 0;
	ds->queue_shared = qs_pa;
	ds->queue_shared_len = qs_len;
	ds->mtu = VMXNET3_MAX_MTU;
	ds->ntxqueue = sc->sc_nqueues;
	ds->nrxqueue = sc->sc_nqueues;
	ds->mcast_table = mcast_pa;
	ds->automask = 1;
	ds->nintr = 1 + (sc->sc_intrmap != NULL ? sc->sc_nqueues : 0);
	ds->evintr = 0;
	ds->ictrl = VMXNET3_ICTRL_DISABLE_ALL;
	for (i = 0; i < ds->nintr; i++)
		ds->modlevel[i] = UPT1_IMOD_ADAPTIVE;

	if (sc->sc_nqueues > 1) {
		struct vmxnet3_upt1_rss_conf *rsscfg;
		bus_addr_t rss_pa;

		rsscfg = vmxnet3_dma_allocmem(sc, sizeof(*rsscfg), 8, &rss_pa);

		rsscfg->hash_type = UPT1_RSS_HASH_TYPE_TCP_IPV4 |
		    UPT1_RSS_HASH_TYPE_IPV4 |
		    UPT1_RSS_HASH_TYPE_TCP_IPV6 |
		    UPT1_RSS_HASH_TYPE_IPV6;
		rsscfg->hash_func = UPT1_RSS_HASH_FUNC_TOEPLITZ;
		rsscfg->hash_key_size = sizeof(rsscfg->hash_key);
		stoeplitz_to_key(rsscfg->hash_key, sizeof(rsscfg->hash_key));

		rsscfg->ind_table_size = sizeof(rsscfg->ind_table);
		for (i = 0; i < sizeof(rsscfg->ind_table); i++)
			rsscfg->ind_table[i] = i % sc->sc_nqueues;

		ds->upt_features |= UPT1_F_RSS;
		ds->rss.version = 1;
		ds->rss.len = sizeof(*rsscfg);
		ds->rss.paddr = rss_pa;

		sc->sc_rss = rsscfg;
	}

	WRITE_BAR1(sc, VMXNET3_BAR1_DSL, ds_pa);
	WRITE_BAR1(sc, VMXNET3_BAR1_DSH, (u_int64_t)ds_pa >> 32);
	return 0;
}

int
vmxnet3_alloc_txring(struct vmxnet3_softc *sc, int queue, int intr)
{
	struct vmxnet3_txqueue *tq = &sc->sc_q[queue].tx;
	struct vmxnet3_txq_shared *ts;
	struct vmxnet3_txring *ring = &tq->cmd_ring;
	struct vmxnet3_comp_ring *comp_ring = &tq->comp_ring;
	bus_addr_t pa, comp_pa;
	int idx;

	ring->txd = vmxnet3_dma_allocmem(sc, NTXDESC * sizeof ring->txd[0], 512, &pa);
	if (ring->txd == NULL)
		return -1;
	comp_ring->txcd = vmxnet3_dma_allocmem(sc,
	    NTXCOMPDESC * sizeof comp_ring->txcd[0], 512, &comp_pa);
	if (comp_ring->txcd == NULL)
		return -1;

	for (idx = 0; idx < NTXDESC; idx++) {
		if (bus_dmamap_create(sc->sc_dmat, JUMBO_LEN, NTXSEGS,
		    VMXNET3_TX_LEN_M + 1, 0, BUS_DMA_NOWAIT, &ring->dmap[idx]))
			return -1;
	}

	ts = tq->ts;
	bzero(ts, sizeof *ts);
	ts->npending = 0;
	ts->intr_threshold = 1;
	ts->cmd_ring = pa;
	ts->cmd_ring_len = NTXDESC;
	ts->comp_ring = comp_pa;
	ts->comp_ring_len = NTXCOMPDESC;
	ts->driver_data = ~0ULL;
	ts->driver_data_len = 0;
	ts->intr_idx = intr;
	ts->stopped = 1;
	ts->error = 0;
	return 0;
}

int
vmxnet3_alloc_rxring(struct vmxnet3_softc *sc, int queue, int intr)
{
	struct vmxnet3_rxqueue *rq = &sc->sc_q[queue].rx;
	struct vmxnet3_rxq_shared *rs;
	struct vmxnet3_rxring *ring;
	struct vmxnet3_comp_ring *comp_ring;
	bus_addr_t pa[2], comp_pa;
	int i, idx;

	for (i = 0; i < 2; i++) {
		ring = &rq->cmd_ring[i];
		ring->rxd = vmxnet3_dma_allocmem(sc, NRXDESC * sizeof ring->rxd[0],
		    512, &pa[i]);
		if (ring->rxd == NULL)
			return -1;
	}
	comp_ring = &rq->comp_ring;
	comp_ring->rxcd = vmxnet3_dma_allocmem(sc,
	    NRXCOMPDESC * sizeof comp_ring->rxcd[0], 512, &comp_pa);
	if (comp_ring->rxcd == NULL)
		return -1;

	for (i = 0; i < 2; i++) {
		ring = &rq->cmd_ring[i];
		ring->sc = sc;
		ring->rid = i;
		mtx_init(&ring->mtx, IPL_NET);
		timeout_set(&ring->refill, vmxnet3_rxfill_tick, ring);
		for (idx = 0; idx < NRXDESC; idx++) {
			if (bus_dmamap_create(sc->sc_dmat, JUMBO_LEN, 1,
			    JUMBO_LEN, 0, BUS_DMA_NOWAIT, &ring->dmap[idx]))
				return -1;
		}
	}

	rs = rq->rs;
	bzero(rs, sizeof *rs);
	rs->cmd_ring[0] = pa[0];
	rs->cmd_ring[1] = pa[1];
	rs->cmd_ring_len[0] = NRXDESC;
	rs->cmd_ring_len[1] = NRXDESC;
	rs->comp_ring = comp_pa;
	rs->comp_ring_len = NRXCOMPDESC;
	rs->driver_data = ~0ULL;
	rs->driver_data_len = 0;
	rs->intr_idx = intr;
	rs->stopped = 1;
	rs->error = 0;
	return 0;
}

void
vmxnet3_txinit(struct vmxnet3_softc *sc, struct vmxnet3_txqueue *tq)
{
	struct vmxnet3_txring *ring = &tq->cmd_ring;
	struct vmxnet3_comp_ring *comp_ring = &tq->comp_ring;

	ring->cons = ring->prod = 0;
	ring->gen = VMX_TX_GEN;
	comp_ring->next = 0;
	comp_ring->gen = VMX_TXC_GEN;
	bzero(ring->txd, NTXDESC * sizeof ring->txd[0]);
	bzero(comp_ring->txcd, NTXCOMPDESC * sizeof comp_ring->txcd[0]);
}

void
vmxnet3_rxfill_tick(void *arg)
{
	struct vmxnet3_rxring *ring = arg;

	if (!mtx_enter_try(&ring->mtx))
		return;

	vmxnet3_rxfill(ring);
	mtx_leave(&ring->mtx);
}

void
vmxnet3_rxfill(struct vmxnet3_rxring *ring)
{
	struct vmxnet3_softc *sc = ring->sc;
	struct vmxnet3_rxdesc *rxd;
	struct mbuf *m;
	bus_dmamap_t map;
	u_int slots;
	unsigned int prod;
	uint32_t rgen;
	uint32_t type = htole32(VMXNET3_BTYPE_HEAD << VMXNET3_RX_BTYPE_S);

	MUTEX_ASSERT_LOCKED(&ring->mtx);

	prod = ring->fill;
	rgen = ring->gen;

	for (slots = if_rxr_get(&ring->rxr, NRXDESC); slots > 0; slots--) {
		KASSERT(ring->m[prod] == NULL);

		m = MCLGETL(NULL, M_DONTWAIT, JUMBO_LEN);
		if (m == NULL)
			break;

		m->m_pkthdr.len = m->m_len = JUMBO_LEN;
		m_adj(m, ETHER_ALIGN);

		map = ring->dmap[prod];
		if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT))
			panic("load mbuf");

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);

		ring->m[prod] = m;

		rxd = &ring->rxd[prod];
		rxd->rx_addr = htole64(DMAADDR(map));
		membar_producer();
		rxd->rx_word2 = (htole32(m->m_pkthdr.len & VMXNET3_RX_LEN_M) <<
		    VMXNET3_RX_LEN_S) | type | rgen;

		if (++prod == NRXDESC) {
			prod = 0;
			rgen ^= VMX_RX_GEN;
		}
	}
	if_rxr_put(&ring->rxr, slots);

	ring->fill = prod;
	ring->gen = rgen;

	if (if_rxr_inuse(&ring->rxr) == 0)
		timeout_add(&ring->refill, 1);
}

void
vmxnet3_rxinit(struct vmxnet3_softc *sc, struct vmxnet3_rxqueue *rq)
{
	struct vmxnet3_rxring *ring;
	struct vmxnet3_comp_ring *comp_ring;
	int i;

	for (i = 0; i < 2; i++) {
		ring = &rq->cmd_ring[i];
		ring->fill = 0;
		ring->gen = VMX_RX_GEN;
		bzero(ring->rxd, NRXDESC * sizeof ring->rxd[0]);
		if_rxr_init(&ring->rxr, 2, NRXDESC - 1);
	}

	/* XXX only fill ring 0 */
	ring = &rq->cmd_ring[0];
	mtx_enter(&ring->mtx);
	vmxnet3_rxfill(ring);
	mtx_leave(&ring->mtx);

	comp_ring = &rq->comp_ring;
	comp_ring->next = 0;
	comp_ring->gen = VMX_RXC_GEN;
	bzero(comp_ring->rxcd, NRXCOMPDESC * sizeof comp_ring->rxcd[0]);
}

void
vmxnet3_txstop(struct vmxnet3_softc *sc, struct vmxnet3_txqueue *tq)
{
	struct vmxnet3_txring *ring = &tq->cmd_ring;
	int idx;

	for (idx = 0; idx < NTXDESC; idx++) {
		if (ring->m[idx]) {
			bus_dmamap_unload(sc->sc_dmat, ring->dmap[idx]);
			m_freem(ring->m[idx]);
			ring->m[idx] = NULL;
		}
	}
}

void
vmxnet3_rxstop(struct vmxnet3_softc *sc, struct vmxnet3_rxqueue *rq)
{
	struct vmxnet3_rxring *ring;
	int i, idx;

	for (i = 0; i < 2; i++) {
		ring = &rq->cmd_ring[i];
		timeout_del(&ring->refill);
		for (idx = 0; idx < NRXDESC; idx++) {
			struct mbuf *m = ring->m[idx];
			if (m == NULL)
				continue;

			ring->m[idx] = NULL;
			m_freem(m);
			bus_dmamap_unload(sc->sc_dmat, ring->dmap[idx]);
		}
	}
}

void
vmxnet3_link_state(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int x, link, speed;

	WRITE_CMD(sc, VMXNET3_CMD_GET_LINK);
	x = READ_BAR1(sc, VMXNET3_BAR1_CMD);
	speed = x >> 16;
	if (x & 1) {
		ifp->if_baudrate = IF_Mbps(speed);
		link = LINK_STATE_UP;
	} else
		link = LINK_STATE_DOWN;

	if (ifp->if_link_state != link) {
		ifp->if_link_state = link;
		if_link_state_change(ifp);
	}
}

static inline void
vmxnet3_enable_intr(struct vmxnet3_softc *sc, int irq)
{
	WRITE_BAR0(sc, VMXNET3_BAR0_IMASK(irq), 0);
}

static inline void
vmxnet3_disable_intr(struct vmxnet3_softc *sc, int irq)
{
	WRITE_BAR0(sc, VMXNET3_BAR0_IMASK(irq), 1);
}

void
vmxnet3_enable_all_intrs(struct vmxnet3_softc *sc)
{
	int i;

	sc->sc_ds->ictrl &= ~VMXNET3_ICTRL_DISABLE_ALL;
	vmxnet3_enable_intr(sc, 0);
	if (sc->sc_intrmap) {
		for (i = 0; i < sc->sc_nqueues; i++)
			vmxnet3_enable_intr(sc, sc->sc_q[i].intr);
	}
}

void
vmxnet3_disable_all_intrs(struct vmxnet3_softc *sc)
{
	int i;

	sc->sc_ds->ictrl |= VMXNET3_ICTRL_DISABLE_ALL;
	vmxnet3_disable_intr(sc, 0);
	if (sc->sc_intrmap) {
		for (i = 0; i < sc->sc_nqueues; i++)
			vmxnet3_disable_intr(sc, sc->sc_q[i].intr);
	}
}

int
vmxnet3_intr_intx(void *arg)
{
	struct vmxnet3_softc *sc = arg;

	if (READ_BAR1(sc, VMXNET3_BAR1_INTR) == 0)
		return 0;

	return (vmxnet3_intr(sc));
}

int
vmxnet3_intr(void *arg)
{
	struct vmxnet3_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	if (sc->sc_ds->event) {
		KERNEL_LOCK();
		vmxnet3_evintr(sc);
		KERNEL_UNLOCK();
	}

	if (ifp->if_flags & IFF_RUNNING) {
		vmxnet3_rxintr(sc, &sc->sc_q[0].rx);
		vmxnet3_txintr(sc, &sc->sc_q[0].tx);
		vmxnet3_enable_intr(sc, 0);
	}

	return 1;
}

int
vmxnet3_intr_event(void *arg)
{
	struct vmxnet3_softc *sc = arg;

	if (sc->sc_ds->event) {
		KERNEL_LOCK();
		vmxnet3_evintr(sc);
		KERNEL_UNLOCK();
	}

	vmxnet3_enable_intr(sc, 0);
	return 1;
}

int
vmxnet3_intr_queue(void *arg)
{
	struct vmxnet3_queue *q = arg;

	vmxnet3_rxintr(q->sc, &q->rx);
	vmxnet3_txintr(q->sc, &q->tx);
	vmxnet3_enable_intr(q->sc, q->intr);

	return 1;
}

void
vmxnet3_evintr(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int event = sc->sc_ds->event;
	struct vmxnet3_txq_shared *ts;
	struct vmxnet3_rxq_shared *rs;

	/* Clear events. */
	WRITE_BAR1(sc, VMXNET3_BAR1_EVENT, event);

	/* Link state change? */
	if (event & VMXNET3_EVENT_LINK)
		vmxnet3_link_state(sc);

	/* Queue error? */
	if (event & (VMXNET3_EVENT_TQERROR | VMXNET3_EVENT_RQERROR)) {
		WRITE_CMD(sc, VMXNET3_CMD_GET_STATUS);

		ts = sc->sc_q[0].tx.ts;
		if (ts->stopped)
			printf("%s: TX error 0x%x\n", ifp->if_xname, ts->error);
		rs = sc->sc_q[0].rx.rs;
		if (rs->stopped)
			printf("%s: RX error 0x%x\n", ifp->if_xname, rs->error);
		vmxnet3_init(sc);
	}

	if (event & VMXNET3_EVENT_DIC)
		printf("%s: device implementation change event\n",
		    ifp->if_xname);
	if (event & VMXNET3_EVENT_DEBUG)
		printf("%s: debug event\n", ifp->if_xname);
}

void
vmxnet3_txintr(struct vmxnet3_softc *sc, struct vmxnet3_txqueue *tq)
{
	struct ifqueue *ifq = tq->ifq;
	struct vmxnet3_txring *ring = &tq->cmd_ring;
	struct vmxnet3_comp_ring *comp_ring = &tq->comp_ring;
	struct vmxnet3_txcompdesc *txcd;
	bus_dmamap_t map;
	struct mbuf *m;
	u_int cons, next;
	uint32_t rgen;

	cons = ring->cons;
	if (cons == ring->prod)
		return;

	next = comp_ring->next;
	rgen = comp_ring->gen;

	/* postread */
	for (;;) {
		txcd = &comp_ring->txcd[next];
		if ((txcd->txc_word3 & VMX_TXC_GEN) != rgen)
			break;

		if (++next == NTXCOMPDESC) {
			next = 0;
			rgen ^= VMX_TXC_GEN;
		}

		m = ring->m[cons];
		ring->m[cons] = NULL;

		KASSERT(m != NULL);

		map = ring->dmap[cons];
		bus_dmamap_unload(sc->sc_dmat, map);
		m_freem(m);

		cons = (letoh32(txcd->txc_word0) >> VMXNET3_TXC_EOPIDX_S) &
		    VMXNET3_TXC_EOPIDX_M;
		cons++;
		cons %= NTXDESC;
	}
	/* preread */

	comp_ring->next = next;
	comp_ring->gen = rgen;
	ring->cons = cons;

	if (ifq_is_oactive(ifq))
		ifq_restart(ifq);
}

void
vmxnet3_rxintr(struct vmxnet3_softc *sc, struct vmxnet3_rxqueue *rq)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct vmxnet3_comp_ring *comp_ring = &rq->comp_ring;
	struct vmxnet3_rxring *ring;
	struct vmxnet3_rxcompdesc *rxcd;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	bus_dmamap_t map;
	unsigned int idx, len;
	unsigned int next, rgen;
	unsigned int done = 0;

	next = comp_ring->next;
	rgen = comp_ring->gen;

	for (;;) {
		rxcd = &comp_ring->rxcd[next];
		if ((rxcd->rxc_word3 & VMX_RXC_GEN) != rgen)
			break;

		if (++next == NRXCOMPDESC) {
			next = 0;
			rgen ^= VMX_RXC_GEN;
		}

		idx = letoh32((rxcd->rxc_word0 >> VMXNET3_RXC_IDX_S) &
		    VMXNET3_RXC_IDX_M);
		if (letoh32((rxcd->rxc_word0 >> VMXNET3_RXC_QID_S) &
		    VMXNET3_RXC_QID_M) < sc->sc_nqueues)
			ring = &rq->cmd_ring[0];
		else
			ring = &rq->cmd_ring[1];

		m = ring->m[idx];
		KASSERT(m != NULL);
		ring->m[idx] = NULL;

		map = ring->dmap[idx];
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, map);

		done++;

		if (letoh32(rxcd->rxc_word2 & VMXNET3_RXC_ERROR)) {
			ifp->if_ierrors++;
			m_freem(m);
			goto skip_buffer;
		}

		len = letoh32((rxcd->rxc_word2 >> VMXNET3_RXC_LEN_S) &
		    VMXNET3_RXC_LEN_M);
		if (len < VMXNET3_MIN_MTU) {
			m_freem(m);
			goto skip_buffer;
		}
		m->m_pkthdr.len = m->m_len = len;

		vmxnet3_rx_csum(rxcd, m);
		if (letoh32(rxcd->rxc_word2 & VMXNET3_RXC_VLAN)) {
			m->m_flags |= M_VLANTAG;
			m->m_pkthdr.ether_vtag = letoh32((rxcd->rxc_word2 >>
			    VMXNET3_RXC_VLANTAG_S) & VMXNET3_RXC_VLANTAG_M);
		}
		if (((letoh32(rxcd->rxc_word0) >> VMXNET3_RXC_RSSTYPE_S) &
		    VMXNET3_RXC_RSSTYPE_M) != VMXNET3_RXC_RSSTYPE_NONE) {
			m->m_pkthdr.ph_flowid = letoh32(rxcd->rxc_word1);
			SET(m->m_pkthdr.csum_flags, M_FLOWID);
		}

		ml_enqueue(&ml, m);

skip_buffer:
		if (rq->rs->update_rxhead) {
			u_int qid = letoh32((rxcd->rxc_word0 >>
			    VMXNET3_RXC_QID_S) & VMXNET3_RXC_QID_M);

			idx = (idx + 1) % NRXDESC;
			if (qid < sc->sc_nqueues) {
				WRITE_BAR0(sc, VMXNET3_BAR0_RXH1(qid), idx);
			} else {
				qid -= sc->sc_nqueues;
				WRITE_BAR0(sc, VMXNET3_BAR0_RXH2(qid), idx);
			}
		}
	}

	comp_ring->next = next;
	comp_ring->gen = rgen;

	if (done == 0)
		return;

	ring = &rq->cmd_ring[0];

	if (ifiq_input(rq->ifiq, &ml))
		if_rxr_livelocked(&ring->rxr);

	/* XXX Should we (try to) allocate buffers for ring 2 too? */
	mtx_enter(&ring->mtx);
	if_rxr_put(&ring->rxr, done);
	vmxnet3_rxfill(ring);
	mtx_leave(&ring->mtx);
}

void
vmxnet3_iff(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct arpcom *ac = &sc->sc_arpcom;
	struct vmxnet3_driver_shared *ds = sc->sc_ds;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int mode;
	u_int8_t *p;

	ds->mcast_tablelen = 0;
	CLR(ifp->if_flags, IFF_ALLMULTI);

	/*
	 * Always accept broadcast frames.
	 * Always accept frames destined to our station address.
	 */
	mode = VMXNET3_RXMODE_BCAST | VMXNET3_RXMODE_UCAST;

	if (ISSET(ifp->if_flags, IFF_PROMISC) || ac->ac_multirangecnt > 0 ||
	    ac->ac_multicnt > 682) {
		SET(ifp->if_flags, IFF_ALLMULTI);
		SET(mode, (VMXNET3_RXMODE_ALLMULTI | VMXNET3_RXMODE_MCAST));
		if (ifp->if_flags & IFF_PROMISC)
			SET(mode, VMXNET3_RXMODE_PROMISC);
	} else {
		p = sc->sc_mcast;
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			bcopy(enm->enm_addrlo, p, ETHER_ADDR_LEN);

			p += ETHER_ADDR_LEN;

			ETHER_NEXT_MULTI(step, enm);
		}

		if (ac->ac_multicnt > 0) {
			SET(mode, VMXNET3_RXMODE_MCAST);
			ds->mcast_tablelen = p - sc->sc_mcast;
		}
	}

	WRITE_CMD(sc, VMXNET3_CMD_SET_FILTER);
	ds->rxmode = mode;
	WRITE_CMD(sc, VMXNET3_CMD_SET_RXMODE);
}


void
vmxnet3_rx_csum(struct vmxnet3_rxcompdesc *rxcd, struct mbuf *m)
{
	if (letoh32(rxcd->rxc_word0 & VMXNET3_RXC_NOCSUM))
		return;

	if ((rxcd->rxc_word3 & (VMXNET3_RXC_IPV4 | VMXNET3_RXC_IPSUM_OK)) ==
	    (VMXNET3_RXC_IPV4 | VMXNET3_RXC_IPSUM_OK))
		m->m_pkthdr.csum_flags |= M_IPV4_CSUM_IN_OK;

	if (rxcd->rxc_word3 & VMXNET3_RXC_FRAGMENT)
		return;

	if (rxcd->rxc_word3 & (VMXNET3_RXC_TCP | VMXNET3_RXC_UDP)) {
		if (rxcd->rxc_word3 & VMXNET3_RXC_CSUM_OK)
			m->m_pkthdr.csum_flags |=
			    M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;
	}
}

void
vmxnet3_stop(struct ifnet *ifp)
{
	struct vmxnet3_softc *sc = ifp->if_softc;
	int queue;

	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);
	ifp->if_timer = 0;

	vmxnet3_disable_all_intrs(sc);

	WRITE_CMD(sc, VMXNET3_CMD_DISABLE);

	if (sc->sc_intrmap != NULL) {
		for (queue = 0; queue < sc->sc_nqueues; queue++)
			intr_barrier(sc->sc_q[queue].ih);
	} else
		intr_barrier(sc->sc_ih);

	for (queue = 0; queue < sc->sc_nqueues; queue++)
		vmxnet3_txstop(sc, &sc->sc_q[queue].tx);
	for (queue = 0; queue < sc->sc_nqueues; queue++)
		vmxnet3_rxstop(sc, &sc->sc_q[queue].rx);
}

void
vmxnet3_reset(struct vmxnet3_softc *sc)
{
	WRITE_CMD(sc, VMXNET3_CMD_RESET);
}

int
vmxnet3_init(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int queue;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	vmxnet3_stop(ifp);

#if 0
	/* Put controller into known state. */
	vmxnet3_reset(sc);
#endif

	for (queue = 0; queue < sc->sc_nqueues; queue++)
		vmxnet3_txinit(sc, &sc->sc_q[queue].tx);
	for (queue = 0; queue < sc->sc_nqueues; queue++)
		vmxnet3_rxinit(sc, &sc->sc_q[queue].rx);

	for (queue = 0; queue < sc->sc_nqueues; queue++) {
		WRITE_BAR0(sc, VMXNET3_BAR0_RXH1(queue), 0);
		WRITE_BAR0(sc, VMXNET3_BAR0_RXH2(queue), 0);
	}

	WRITE_CMD(sc, VMXNET3_CMD_ENABLE);
	if (READ_BAR1(sc, VMXNET3_BAR1_CMD)) {
		printf("%s: failed to initialize\n", ifp->if_xname);
		vmxnet3_stop(ifp);
		return EIO;
	}

	/* Program promiscuous mode and multicast filters. */
	vmxnet3_iff(sc);

	vmxnet3_enable_all_intrs(sc);

	vmxnet3_link_state(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	return 0;
}

static int
vmx_rxr_info(struct vmxnet3_softc *sc, struct if_rxrinfo *ifri)
{
	struct if_rxring_info *ifrs, *ifr;
	int error;
	unsigned int i;

	ifrs = mallocarray(sc->sc_nqueues, sizeof(*ifrs),
	    M_TEMP, M_WAITOK|M_ZERO|M_CANFAIL);
	if (ifrs == NULL)
		return (ENOMEM);

	for (i = 0; i < sc->sc_nqueues; i++) {
		struct if_rxring *rxr = &sc->sc_q[i].rx.cmd_ring[0].rxr;
		ifr = &ifrs[i];

		ifr->ifr_size = JUMBO_LEN;
		snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "%u", i);
		ifr->ifr_info = *rxr;
	}

	error = if_rxr_info_ioctl(ifri, i, ifrs);

	free(ifrs, M_TEMP, i * sizeof(*ifrs));

	return (error);
}

int
vmxnet3_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vmxnet3_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0, s;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			error = vmxnet3_init(sc);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				error = vmxnet3_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				vmxnet3_stop(ifp);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	case SIOCGIFRXR:
		error = vmx_rxr_info(sc, (struct if_rxrinfo *)ifr->ifr_data);
		break;
	default:
		error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			vmxnet3_iff(sc);
		error = 0;
	}

	splx(s);
	return error;
}

static inline int
vmx_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m)
{
	int error;

	error = bus_dmamap_load_mbuf(dmat, map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT);
	if (error != EFBIG)
		return (error);

	error = m_defrag(m, M_DONTWAIT);
	if (error != 0)
		return (error);

	return (bus_dmamap_load_mbuf(dmat, map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT));
}

void
vmxnet3_start(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	struct vmxnet3_softc *sc = ifp->if_softc;
	struct vmxnet3_txqueue *tq = ifq->ifq_softc;
	struct vmxnet3_txring *ring = &tq->cmd_ring;
	struct vmxnet3_txdesc *txd, *sop;
	bus_dmamap_t map;
        unsigned int prod, free, i;
	unsigned int post = 0;
	uint32_t rgen, gen;

	struct mbuf *m;

	free = ring->cons;
	prod = ring->prod;
	if (free <= prod)
		free += NTXDESC;
	free -= prod;

	rgen = ring->gen;

	for (;;) {
		if (free <= NTXSEGS) {
			ifq_set_oactive(ifq);
			break;
		}

		m = ifq_dequeue(ifq);
		if (m == NULL)
			break;

		map = ring->dmap[prod];

		if (vmx_load_mbuf(sc->sc_dmat, map, m) != 0) {
			ifq->ifq_errors++;
			m_freem(m);
			continue;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		ring->m[prod] = m;

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    map->dm_mapsize, BUS_DMASYNC_PREWRITE);

		gen = rgen ^ VMX_TX_GEN;
		sop = &ring->txd[prod];
		for (i = 0; i < map->dm_nsegs; i++) {
			txd = &ring->txd[prod];
			txd->tx_addr = htole64(map->dm_segs[i].ds_addr);
			txd->tx_word2 = htole32(map->dm_segs[i].ds_len <<
			    VMXNET3_TX_LEN_S) | gen;
			txd->tx_word3 = 0;

			if (++prod == NTXDESC) {
				prod = 0;
				rgen ^= VMX_TX_GEN;
			}

			gen = rgen;
		}
		txd->tx_word3 = htole32(VMXNET3_TX_EOP | VMXNET3_TX_COMPREQ);

		if (ISSET(m->m_flags, M_VLANTAG)) {
			sop->tx_word3 |= htole32(VMXNET3_TX_VTAG_MODE);
			sop->tx_word3 |= htole32((m->m_pkthdr.ether_vtag &
			    VMXNET3_TX_VLANTAG_M) << VMXNET3_TX_VLANTAG_S);
		}

		/* Change the ownership by flipping the "generation" bit */
		membar_producer();
		sop->tx_word2 ^= VMX_TX_GEN;

		free -= i;
		post = 1;
	}

	if (!post)
		return;

	ring->prod = prod;
	ring->gen = rgen;

	WRITE_BAR0(sc, VMXNET3_BAR0_TXH(0), prod);
}

void
vmxnet3_watchdog(struct ifnet *ifp)
{
	struct vmxnet3_softc *sc = ifp->if_softc;
	int s;

	printf("%s: device timeout\n", ifp->if_xname);
	s = splnet();
	vmxnet3_init(sc);
	splx(s);
}

void
vmxnet3_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct vmxnet3_softc *sc = ifp->if_softc;

	vmxnet3_link_state(sc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (ifp->if_link_state != LINK_STATE_UP)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	if (ifp->if_baudrate >= IF_Gbps(10))
		ifmr->ifm_active |= IFM_10G_T;
}

int
vmxnet3_media_change(struct ifnet *ifp)
{
	return 0;
}

void *
vmxnet3_dma_allocmem(struct vmxnet3_softc *sc, u_int size, u_int align, bus_addr_t *pa)
{
	bus_dma_tag_t t = sc->sc_dmat;
	bus_dma_segment_t segs[1];
	bus_dmamap_t map;
	caddr_t va;
	int n;

	if (bus_dmamem_alloc(t, size, align, 0, segs, 1, &n, BUS_DMA_NOWAIT))
		return NULL;
	if (bus_dmamem_map(t, segs, 1, size, &va, BUS_DMA_NOWAIT))
		return NULL;
	if (bus_dmamap_create(t, size, 1, size, 0, BUS_DMA_NOWAIT, &map))
		return NULL;
	if (bus_dmamap_load(t, map, va, size, NULL, BUS_DMA_NOWAIT))
		return NULL;
	bzero(va, size);
	*pa = DMAADDR(map);
	bus_dmamap_unload(t, map);
	bus_dmamap_destroy(t, map);
	return va;
}

#if NKSTAT > 0
/*
 * "hardware" counters are exported as separate kstats for each tx
 * and rx ring, but the request for the hypervisor to update the
 * stats is done once at the controller level. we limit the number
 * of updates at the controller level to a rate of one per second to
 * debounce this a bit.
 */
static const struct timeval vmx_kstat_rate = { 1, 0 };

/*
 * all the vmx stats are 64 bit counters, we just need their name and units.
 */
struct vmx_kstat_tpl {
	const char		*name;
	enum kstat_kv_unit	 unit;
};

static const struct vmx_kstat_tpl vmx_rx_kstat_tpl[UPT1_RxStats_count] = {
	{ "LRO packets",	KSTAT_KV_U_PACKETS },
	{ "LRO bytes",		KSTAT_KV_U_BYTES },
	{ "ucast packets",	KSTAT_KV_U_PACKETS },
	{ "ucast bytes",	KSTAT_KV_U_BYTES },
	{ "mcast packets",	KSTAT_KV_U_PACKETS },
	{ "mcast bytes",	KSTAT_KV_U_BYTES },
	{ "bcast packets",	KSTAT_KV_U_PACKETS },
	{ "bcast bytes",	KSTAT_KV_U_BYTES },
	{ "no buffers",		KSTAT_KV_U_PACKETS },
	{ "errors",		KSTAT_KV_U_PACKETS },
};

static const struct vmx_kstat_tpl vmx_tx_kstat_tpl[UPT1_TxStats_count] = {
	{ "TSO packets",	KSTAT_KV_U_PACKETS },
	{ "TSO bytes",		KSTAT_KV_U_BYTES },
	{ "ucast packets",	KSTAT_KV_U_PACKETS },
	{ "ucast bytes",	KSTAT_KV_U_BYTES },
	{ "mcast packets",	KSTAT_KV_U_PACKETS },
	{ "mcast bytes",	KSTAT_KV_U_BYTES },
	{ "bcast packets",	KSTAT_KV_U_PACKETS },
	{ "bcast bytes",	KSTAT_KV_U_BYTES },
	{ "errors",		KSTAT_KV_U_PACKETS },
	{ "discards",		KSTAT_KV_U_PACKETS },
};

static void
vmx_kstat_init(struct vmxnet3_softc *sc)
{
	rw_init(&sc->sc_kstat_lock, "vmxkstat");
}

static int
vmx_kstat_read(struct kstat *ks)
{
	struct vmxnet3_softc *sc = ks->ks_softc;
	struct kstat_kv *kvs = ks->ks_data;
	uint64_t *vs = ks->ks_ptr;
	unsigned int n, i;

	if (ratecheck(&sc->sc_kstat_updated, &vmx_kstat_rate)) {
		WRITE_CMD(sc, VMXNET3_CMD_GET_STATS);
		/* barrier? */
	}

	n = ks->ks_datalen / sizeof(*kvs);
	for (i = 0; i < n; i++)
		kstat_kv_u64(&kvs[i]) = lemtoh64(&vs[i]);

 	TIMEVAL_TO_TIMESPEC(&sc->sc_kstat_updated, &ks->ks_updated);

	return (0);
}

static struct kstat *
vmx_kstat_create(struct vmxnet3_softc *sc, const char *name, unsigned int unit,
    const struct vmx_kstat_tpl *tpls, unsigned int n, uint64_t *vs)
{
	struct kstat *ks;
	struct kstat_kv *kvs;
	unsigned int i;

	ks = kstat_create(sc->sc_dev.dv_xname, 0, name, unit,
	    KSTAT_T_KV, 0);
	if (ks == NULL)
		return (NULL);

	kvs = mallocarray(n, sizeof(*kvs), M_DEVBUF, M_WAITOK|M_ZERO);
	for (i = 0; i < n; i++) {
		const struct vmx_kstat_tpl *tpl = &tpls[i];

		kstat_kv_unit_init(&kvs[i], tpl->name,
		    KSTAT_KV_T_COUNTER64, tpl->unit);
	}

	ks->ks_softc = sc;
	kstat_set_wlock(ks, &sc->sc_kstat_lock);
	ks->ks_ptr = vs;
	ks->ks_data = kvs;
	ks->ks_datalen = n * sizeof(*kvs);
	ks->ks_read = vmx_kstat_read;
	TIMEVAL_TO_TIMESPEC(&vmx_kstat_rate, &ks->ks_interval);

	kstat_install(ks);

	return (ks);
}

static void
vmx_kstat_txstats(struct vmxnet3_softc *sc, struct vmxnet3_txqueue *tq,
    int unit)
{
	tq->txkstat = vmx_kstat_create(sc, "vmx-txstats", unit,
	    vmx_tx_kstat_tpl, nitems(vmx_tx_kstat_tpl), tq->ts->stats);
}

static void
vmx_kstat_rxstats(struct vmxnet3_softc *sc, struct vmxnet3_rxqueue *rq,
    int unit)
{
	rq->rxkstat = vmx_kstat_create(sc, "vmx-rxstats", unit,
	    vmx_rx_kstat_tpl, nitems(vmx_rx_kstat_tpl), rq->rs->stats);
}
#endif /* NKSTAT > 0 */
