/*	$OpenBSD: if_myx.c,v 1.82 2015/08/15 01:17:01 dlg Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@openbsd.org>
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

/*
 * Driver for the Myricom Myri-10G Lanai-Z8E Ethernet chipsets.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/timeout.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/atomic.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_myxreg.h>

#ifdef MYX_DEBUG
#define MYXDBG_INIT	(1<<0)	/* chipset initialization */
#define MYXDBG_CMD	(2<<0)	/* commands */
#define MYXDBG_INTR	(3<<0)	/* interrupts */
#define MYXDBG_ALL	0xffff	/* enable all debugging messages */
int myx_debug = MYXDBG_ALL;
#define DPRINTF(_lvl, _arg...)	do {					\
	if (myx_debug & (_lvl))						\
		printf(_arg);						\
} while (0)
#else
#define DPRINTF(_lvl, arg...)
#endif

#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

struct myx_dmamem {
	bus_dmamap_t		 mxm_map;
	bus_dma_segment_t	 mxm_seg;
	int			 mxm_nsegs;
	size_t			 mxm_size;
	caddr_t			 mxm_kva;
};

struct pool *myx_mcl_pool;

struct myx_slot {
	bus_dmamap_t		 ms_map;
	struct mbuf		*ms_m;
};

struct myx_rx_ring {
	struct myx_softc	*mrr_softc;
	struct timeout		 mrr_refill;
	struct if_rxring	 mrr_rxr;
	struct myx_slot		*mrr_slots;
	u_int32_t		 mrr_offset;
	u_int			 mrr_running;
	u_int			 mrr_prod;
	u_int			 mrr_cons;
	struct mbuf		*(*mrr_mclget)(void);
};

enum myx_state {
	MYX_S_OFF = 0,
	MYX_S_RUNNING,
	MYX_S_DOWN
};

struct myx_softc {
	struct device		 sc_dev;
	struct arpcom		 sc_ac;

	pci_chipset_tag_t	 sc_pc;
	pci_intr_handle_t	 sc_ih;
	pcitag_t		 sc_tag;

	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_size_t		 sc_mems;

	struct myx_dmamem	 sc_zerodma;
	struct myx_dmamem	 sc_cmddma;
	struct myx_dmamem	 sc_paddma;

	struct myx_dmamem	 sc_sts_dma;
	volatile struct myx_status	*sc_sts;
	struct mutex		 sc_sts_mtx;

	int			 sc_intx;
	void			*sc_irqh;
	u_int32_t		 sc_irqcoaloff;
	u_int32_t		 sc_irqclaimoff;
	u_int32_t		 sc_irqdeassertoff;

	struct myx_dmamem	 sc_intrq_dma;
	struct myx_intrq_desc	*sc_intrq;
	u_int			 sc_intrq_count;
	u_int			 sc_intrq_idx;

	u_int			 sc_rx_ring_count;
#define  MYX_RXSMALL		 0
#define  MYX_RXBIG		 1
	struct myx_rx_ring	 sc_rx_ring[2];

	bus_size_t		 sc_tx_boundary;
	u_int			 sc_tx_ring_count;
	u_int32_t		 sc_tx_ring_offset;
	u_int			 sc_tx_nsegs;
	u_int32_t		 sc_tx_count; /* shadows ms_txdonecnt */
	u_int			 sc_tx_ring_idx;

	u_int			 sc_tx_free;
	u_int			 sc_tx_prod;
	u_int			 sc_tx_cons;
	struct myx_slot		*sc_tx_slots;

	struct ifmedia		 sc_media;

	volatile enum myx_state	 sc_state;
	volatile u_int8_t	 sc_linkdown;
};

#define MYX_RXSMALL_SIZE	MCLBYTES
#define MYX_RXBIG_SIZE		(9 * 1024)

int	 myx_match(struct device *, void *, void *);
void	 myx_attach(struct device *, struct device *, void *);
int	 myx_pcie_dc(struct myx_softc *, struct pci_attach_args *);
int	 myx_query(struct myx_softc *sc, char *, size_t);
u_int	 myx_ether_aton(char *, u_int8_t *, u_int);
void	 myx_attachhook(void *);
int	 myx_loadfirmware(struct myx_softc *, const char *);
int	 myx_probe_firmware(struct myx_softc *);

void	 myx_read(struct myx_softc *, bus_size_t, void *, bus_size_t);
void	 myx_write(struct myx_softc *, bus_size_t, void *, bus_size_t);

#if defined(__LP64__)
#define _myx_bus_space_write bus_space_write_raw_region_8
typedef u_int64_t myx_bus_t;
#else
#define _myx_bus_space_write bus_space_write_raw_region_4
typedef u_int32_t myx_bus_t;
#endif
#define myx_bus_space_write(_sc, _o, _a, _l) \
    _myx_bus_space_write((_sc)->sc_memt, (_sc)->sc_memh, (_o), (_a), (_l))

int	 myx_cmd(struct myx_softc *, u_int32_t, struct myx_cmd *, u_int32_t *);
int	 myx_boot(struct myx_softc *, u_int32_t);

int	 myx_rdma(struct myx_softc *, u_int);
int	 myx_dmamem_alloc(struct myx_softc *, struct myx_dmamem *,
	    bus_size_t, u_int align);
void	 myx_dmamem_free(struct myx_softc *, struct myx_dmamem *);
int	 myx_media_change(struct ifnet *);
void	 myx_media_status(struct ifnet *, struct ifmediareq *);
void	 myx_link_state(struct myx_softc *, u_int32_t);
void	 myx_watchdog(struct ifnet *);
int	 myx_ioctl(struct ifnet *, u_long, caddr_t);
int	 myx_rxrinfo(struct myx_softc *, struct if_rxrinfo *);
void	 myx_up(struct myx_softc *);
void	 myx_iff(struct myx_softc *);
void	 myx_down(struct myx_softc *);

void	 myx_start(struct ifnet *);
void	 myx_write_txd_tail(struct myx_softc *, struct myx_slot *, u_int8_t,
	    u_int32_t, u_int);
int	 myx_load_mbuf(struct myx_softc *, struct myx_slot *, struct mbuf *);
int	 myx_setlladdr(struct myx_softc *, u_int32_t, u_int8_t *);
int	 myx_intr(void *);
void	 myx_rxeof(struct myx_softc *);
void	 myx_txeof(struct myx_softc *, u_int32_t);

int			myx_buf_fill(struct myx_softc *, struct myx_slot *,
			    struct mbuf *(*)(void));
struct mbuf *		myx_mcl_small(void);
struct mbuf *		myx_mcl_big(void);

int			myx_rx_init(struct myx_softc *, int, bus_size_t);
int			myx_rx_fill(struct myx_softc *, struct myx_rx_ring *);
void			myx_rx_empty(struct myx_softc *, struct myx_rx_ring *);
void			myx_rx_free(struct myx_softc *, struct myx_rx_ring *);

int			myx_tx_init(struct myx_softc *, bus_size_t);
void			myx_tx_empty(struct myx_softc *);
void			myx_tx_free(struct myx_softc *);

void			myx_refill(void *);

static inline void
myx_sts_enter(struct myx_softc *sc)
{
	bus_dmamap_t		 map = sc->sc_sts_dma.mxm_map;

	mtx_enter(&sc->sc_sts_mtx);
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
}

static inline void
myx_sts_leave(struct myx_softc *sc)
{
	bus_dmamap_t		 map = sc->sc_sts_dma.mxm_map;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	mtx_leave(&sc->sc_sts_mtx);
}

struct cfdriver myx_cd = {
	NULL, "myx", DV_IFNET
};
struct cfattach myx_ca = {
	sizeof(struct myx_softc), myx_match, myx_attach
};

const struct pci_matchid myx_devices[] = {
	{ PCI_VENDOR_MYRICOM, PCI_PRODUCT_MYRICOM_Z8E },
	{ PCI_VENDOR_MYRICOM, PCI_PRODUCT_MYRICOM_Z8E_9 }
};

int
myx_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, myx_devices, nitems(myx_devices)));
}

void
myx_attach(struct device *parent, struct device *self, void *aux)
{
	struct myx_softc	*sc = (struct myx_softc *)self;
	struct pci_attach_args	*pa = aux;
	char			 part[32];
	pcireg_t		 memtype;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	sc->sc_rx_ring[MYX_RXSMALL].mrr_softc = sc;
	sc->sc_rx_ring[MYX_RXSMALL].mrr_mclget = myx_mcl_small;
	timeout_set(&sc->sc_rx_ring[MYX_RXSMALL].mrr_refill, myx_refill,
	    &sc->sc_rx_ring[MYX_RXSMALL]);
	sc->sc_rx_ring[MYX_RXBIG].mrr_softc = sc;
	sc->sc_rx_ring[MYX_RXBIG].mrr_mclget = myx_mcl_big;
	timeout_set(&sc->sc_rx_ring[MYX_RXBIG].mrr_refill, myx_refill,
	    &sc->sc_rx_ring[MYX_RXBIG]);

	mtx_init(&sc->sc_sts_mtx, IPL_NET);

	/* Map the PCI memory space */
	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, MYXBAR0);
	if (pci_mapreg_map(pa, MYXBAR0, memtype, BUS_SPACE_MAP_PREFETCHABLE,
	    &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf(": unable to map register memory\n");
		return;
	}

	/* Get board details (mac/part) */
	memset(part, 0, sizeof(part));
	if (myx_query(sc, part, sizeof(part)) != 0)
		goto unmap;

	/* Map the interrupt */
	if (pci_intr_map_msi(pa, &sc->sc_ih) != 0) {
		if (pci_intr_map(pa, &sc->sc_ih) != 0) {
			printf(": unable to map interrupt\n");
			goto unmap;
		}
		sc->sc_intx = 1;
	}

	printf(": %s, model %s, address %s\n",
	    pci_intr_string(pa->pa_pc, sc->sc_ih),
	    part[0] == '\0' ? "(unknown)" : part,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	/* this is sort of racy */
	if (myx_mcl_pool == NULL) {
		extern struct kmem_pa_mode kp_dma_contig;

		myx_mcl_pool = malloc(sizeof(*myx_mcl_pool), M_DEVBUF,
		    M_WAITOK);
		if (myx_mcl_pool == NULL) {
			printf("%s: unable to allocate mcl pool\n",
			    DEVNAME(sc));
			goto unmap;
		}
		pool_init(myx_mcl_pool, MYX_RXBIG_SIZE, MYX_BOUNDARY, 0,
		    0, "myxmcl", NULL);
		pool_setipl(myx_mcl_pool, IPL_NET);
		pool_set_constraints(myx_mcl_pool, &kp_dma_contig);
	}

	if (myx_pcie_dc(sc, pa) != 0)
		printf("%s: unable to configure PCI Express\n", DEVNAME(sc));

	if (mountroothook_establish(myx_attachhook, sc) == NULL) {
		printf("%s: unable to establish mountroot hook\n", DEVNAME(sc));
		goto unmap;
	}

	return;

 unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
myx_pcie_dc(struct myx_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t dcsr;
	pcireg_t mask = PCI_PCIE_DCSR_MPS | PCI_PCIE_DCSR_ERO;
	pcireg_t dc = ((fls(4096) - 8) << 12) | PCI_PCIE_DCSR_ERO;
	int reg;

	if (pci_get_capability(sc->sc_pc, pa->pa_tag, PCI_CAP_PCIEXPRESS,
	    &reg, NULL) == 0)
		return (-1);

	reg += PCI_PCIE_DCSR;
	dcsr = pci_conf_read(sc->sc_pc, pa->pa_tag, reg);
	if ((dcsr & mask) != dc) {
		CLR(dcsr, mask);
		SET(dcsr, dc);
		pci_conf_write(sc->sc_pc, pa->pa_tag, reg, dcsr);
	}

	return (0);
}

u_int
myx_ether_aton(char *mac, u_int8_t *lladdr, u_int maxlen)
{
	u_int		i, j;
	u_int8_t	digit;

	memset(lladdr, 0, ETHER_ADDR_LEN);
	for (i = j = 0; mac[i] != '\0' && i < maxlen; i++) {
		if (mac[i] >= '0' && mac[i] <= '9')
			digit = mac[i] - '0';
		else if (mac[i] >= 'A' && mac[i] <= 'F')
			digit = mac[i] - 'A' + 10;
		else if (mac[i] >= 'a' && mac[i] <= 'f')
			digit = mac[i] - 'a' + 10;
		else
			continue;
		if ((j & 1) == 0)
			digit <<= 4;
		lladdr[j++/2] |= digit;
	}

	return (i);
}

int
myx_query(struct myx_softc *sc, char *part, size_t partlen)
{
	struct myx_gen_hdr hdr;
	u_int32_t	offset;
	u_int8_t	strings[MYX_STRING_SPECS_SIZE];
	u_int		i, len, maxlen;

	myx_read(sc, MYX_HEADER_POS, &offset, sizeof(offset));
	offset = betoh32(offset);
	if (offset + sizeof(hdr) > sc->sc_mems) {
		printf(": header is outside register window\n");
		return (1);
	}

	myx_read(sc, offset, &hdr, sizeof(hdr));
	offset = betoh32(hdr.fw_specs);
	len = min(betoh32(hdr.fw_specs_len), sizeof(strings));

	bus_space_read_region_1(sc->sc_memt, sc->sc_memh, offset, strings, len);

	for (i = 0; i < len; i++) {
		maxlen = len - i;
		if (strings[i] == '\0')
			break;
		if (maxlen > 4 && memcmp("MAC=", &strings[i], 4) == 0) {
			i += 4;
			i += myx_ether_aton(&strings[i],
			    sc->sc_ac.ac_enaddr, maxlen);
		} else if (maxlen > 3 && memcmp("PC=", &strings[i], 3) == 0) {
			i += 3;
			i += strlcpy(part, &strings[i], min(maxlen, partlen));
		}
		for (; i < len; i++) {
			if (strings[i] == '\0')
				break;
		}
	}

	return (0);
}

int
myx_loadfirmware(struct myx_softc *sc, const char *filename)
{
	struct myx_gen_hdr	hdr;
	u_int8_t		*fw;
	size_t			fwlen;
	u_int32_t		offset;
	u_int			i, ret = 1;

	if (loadfirmware(filename, &fw, &fwlen) != 0) {
		printf("%s: could not load firmware %s\n", DEVNAME(sc),
		    filename);
		return (1);
	}
	if (fwlen > MYX_SRAM_SIZE || fwlen < MYXFW_MIN_LEN) {
		printf("%s: invalid firmware %s size\n", DEVNAME(sc), filename);
		goto err;
	}

	memcpy(&offset, fw + MYX_HEADER_POS, sizeof(offset));
	offset = betoh32(offset);
	if ((offset + sizeof(hdr)) > fwlen) {
		printf("%s: invalid firmware %s\n", DEVNAME(sc), filename);
		goto err;
	}

	memcpy(&hdr, fw + offset, sizeof(hdr));
	DPRINTF(MYXDBG_INIT, "%s: "
	    "fw hdr off %u, length %u, type 0x%x, version %s\n",
	    DEVNAME(sc), offset, betoh32(hdr.fw_hdrlength),
	    betoh32(hdr.fw_type), hdr.fw_version);

	if (betoh32(hdr.fw_type) != MYXFW_TYPE_ETH ||
	    memcmp(MYXFW_VER, hdr.fw_version, strlen(MYXFW_VER)) != 0) {
		printf("%s: invalid firmware type 0x%x version %s\n",
		    DEVNAME(sc), betoh32(hdr.fw_type), hdr.fw_version);
		goto err;
	}

	/* Write the firmware to the card's SRAM */
	for (i = 0; i < fwlen; i += 256)
		myx_write(sc, i + MYX_FW, fw + i, min(256, fwlen - i));

	if (myx_boot(sc, fwlen) != 0) {
		printf("%s: failed to boot %s\n", DEVNAME(sc), filename);
		goto err;
	}

	ret = 0;

err:
	free(fw, M_DEVBUF, 0);
	return (ret);
}

void
myx_attachhook(void *arg)
{
	struct myx_softc	*sc = (struct myx_softc *)arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	struct myx_cmd		 mc;

	/* Allocate command DMA memory */
	if (myx_dmamem_alloc(sc, &sc->sc_cmddma, MYXALIGN_CMD,
	    MYXALIGN_CMD) != 0) {
		printf("%s: failed to allocate command DMA memory\n",
		    DEVNAME(sc));
		return;
	}

	/* Try the firmware stored on disk */
	if (myx_loadfirmware(sc, MYXFW_ALIGNED) != 0) {
		/* error printed by myx_loadfirmware */
		goto freecmd;
	}

	memset(&mc, 0, sizeof(mc));

	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
		goto freecmd;
	}

	sc->sc_tx_boundary = 4096;

	if (myx_probe_firmware(sc) != 0) {
		printf("%s: error while selecting firmware\n", DEVNAME(sc));
		goto freecmd;
	}

	sc->sc_irqh = pci_intr_establish(sc->sc_pc, sc->sc_ih,
	    IPL_NET | IPL_MPSAFE, myx_intr, sc, DEVNAME(sc));
	if (sc->sc_irqh == NULL) {
		printf("%s: unable to establish interrupt\n", DEVNAME(sc));
		goto freecmd;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = myx_ioctl;
	ifp->if_start = myx_start;
	ifp->if_watchdog = myx_watchdog;
	ifp->if_hardmtu = 9000;
	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, 1);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
	    IFCAP_CSUM_UDPv4;
#endif

	ifmedia_init(&sc->sc_media, 0, myx_media_change, myx_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	return;

freecmd:
	myx_dmamem_free(sc, &sc->sc_cmddma);
}

int
myx_probe_firmware(struct myx_softc *sc)
{
	struct myx_dmamem test;
	bus_dmamap_t map;
	struct myx_cmd mc;
	pcireg_t csr;
	int offset;
	int width = 0;

	if (pci_get_capability(sc->sc_pc, sc->sc_tag, PCI_CAP_PCIEXPRESS,
	    &offset, NULL)) {
		csr = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    offset + PCI_PCIE_LCSR);
		width = (csr >> 20) & 0x3f;

		if (width <= 4) {
			/*
			 * if the link width is 4 or less we can use the
			 * aligned firmware.
			 */
			return (0);
		}
	}

	if (myx_dmamem_alloc(sc, &test, 4096, 4096) != 0)
		return (1);
	map = test.mxm_map;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc.mc_data2 = htobe32(4096 * 0x10000);
	if (myx_cmd(sc, MYXCMD_UNALIGNED_DMA_TEST, &mc, NULL) != 0) {
		printf("%s: DMA read test failed\n", DEVNAME(sc));
		goto fail;
	}

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc.mc_data2 = htobe32(4096 * 0x1);
	if (myx_cmd(sc, MYXCMD_UNALIGNED_DMA_TEST, &mc, NULL) != 0) {
		printf("%s: DMA write test failed\n", DEVNAME(sc));
		goto fail;
	}

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc.mc_data2 = htobe32(4096 * 0x10001);
	if (myx_cmd(sc, MYXCMD_UNALIGNED_DMA_TEST, &mc, NULL) != 0) {
		printf("%s: DMA read/write test failed\n", DEVNAME(sc));
		goto fail;
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	myx_dmamem_free(sc, &test);
	return (0);

fail:
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	myx_dmamem_free(sc, &test);

	if (myx_loadfirmware(sc, MYXFW_UNALIGNED) != 0) {
		printf("%s: unable to load %s\n", DEVNAME(sc),
		    MYXFW_UNALIGNED);
		return (1);
	}

	sc->sc_tx_boundary = 2048;

	printf("%s: using unaligned firmware\n", DEVNAME(sc));
	return (0);
}

void
myx_read(struct myx_softc *sc, bus_size_t off, void *ptr, bus_size_t len)
{
	bus_space_barrier(sc->sc_memt, sc->sc_memh, off, len,
	    BUS_SPACE_BARRIER_READ);
	bus_space_read_raw_region_4(sc->sc_memt, sc->sc_memh, off, ptr, len);
}

void
myx_write(struct myx_softc *sc, bus_size_t off, void *ptr, bus_size_t len)
{
	bus_space_write_raw_region_4(sc->sc_memt, sc->sc_memh, off, ptr, len);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, off, len,
	    BUS_SPACE_BARRIER_WRITE);
}

int
myx_dmamem_alloc(struct myx_softc *sc, struct myx_dmamem *mxm,
    bus_size_t size, u_int align)
{
	mxm->mxm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, mxm->mxm_size, 1,
	    mxm->mxm_size, 0, BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
	    &mxm->mxm_map) != 0)
		return (1);
	if (bus_dmamem_alloc(sc->sc_dmat, mxm->mxm_size,
	    align, 0, &mxm->mxm_seg, 1, &mxm->mxm_nsegs,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;
	if (bus_dmamem_map(sc->sc_dmat, &mxm->mxm_seg, mxm->mxm_nsegs,
	    mxm->mxm_size, &mxm->mxm_kva, BUS_DMA_WAITOK) != 0)
		goto free;
	if (bus_dmamap_load(sc->sc_dmat, mxm->mxm_map, mxm->mxm_kva,
	    mxm->mxm_size, NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	return (0);
 unmap:
	bus_dmamem_unmap(sc->sc_dmat, mxm->mxm_kva, mxm->mxm_size);
 free:
	bus_dmamem_free(sc->sc_dmat, &mxm->mxm_seg, 1);
 destroy:
	bus_dmamap_destroy(sc->sc_dmat, mxm->mxm_map);
	return (1);
}

void
myx_dmamem_free(struct myx_softc *sc, struct myx_dmamem *mxm)
{
	bus_dmamap_unload(sc->sc_dmat, mxm->mxm_map);
	bus_dmamem_unmap(sc->sc_dmat, mxm->mxm_kva, mxm->mxm_size);
	bus_dmamem_free(sc->sc_dmat, &mxm->mxm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mxm->mxm_map);
}

int
myx_cmd(struct myx_softc *sc, u_int32_t cmd, struct myx_cmd *mc, u_int32_t *r)
{
	bus_dmamap_t		 map = sc->sc_cmddma.mxm_map;
	struct myx_response	*mr;
	u_int			 i;
	u_int32_t		 result, data;
#ifdef MYX_DEBUG
	static const char *cmds[MYXCMD_MAX] = {
		"CMD_NONE",
		"CMD_RESET",
		"CMD_GET_VERSION",
		"CMD_SET_INTRQDMA",
		"CMD_SET_BIGBUFSZ",
		"CMD_SET_SMALLBUFSZ",
		"CMD_GET_TXRINGOFF",
		"CMD_GET_RXSMALLRINGOFF",
		"CMD_GET_RXBIGRINGOFF",
		"CMD_GET_INTRACKOFF",
		"CMD_GET_INTRDEASSERTOFF",
		"CMD_GET_TXRINGSZ",
		"CMD_GET_RXRINGSZ",
		"CMD_SET_INTRQSZ",
		"CMD_SET_IFUP",
		"CMD_SET_IFDOWN",
		"CMD_SET_MTU",
		"CMD_GET_INTRCOALDELAYOFF",
		"CMD_SET_STATSINTVL",
		"CMD_SET_STATSDMA_OLD",
		"CMD_SET_PROMISC",
		"CMD_UNSET_PROMISC",
		"CMD_SET_LLADDR",
		"CMD_SET_FC",
		"CMD_UNSET_FC",
		"CMD_DMA_TEST",
		"CMD_SET_ALLMULTI",
		"CMD_UNSET_ALLMULTI",
		"CMD_SET_MCASTGROUP",
		"CMD_UNSET_MCASTGROUP",
		"CMD_UNSET_MCAST",
		"CMD_SET_STATSDMA",
		"CMD_UNALIGNED_DMA_TEST",
		"CMD_GET_UNALIGNED_STATUS"
	};
#endif

	mc->mc_cmd = htobe32(cmd);
	mc->mc_addr_high = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc->mc_addr_low = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));

	mr = (struct myx_response *)sc->sc_cmddma.mxm_kva;
	mr->mr_result = 0xffffffff;

	/* Send command */
	myx_write(sc, MYX_CMD, (u_int8_t *)mc, sizeof(struct myx_cmd));
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	for (i = 0; i < 20; i++) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		result = betoh32(mr->mr_result);
		data = betoh32(mr->mr_data);

		if (result != 0xffffffff)
			break;

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);
		delay(1000);
	}

	DPRINTF(MYXDBG_CMD, "%s(%s): %s completed, i %d, "
	    "result 0x%x, data 0x%x (%u)\n", DEVNAME(sc), __func__,
	    cmds[cmd], i, result, data, data);

	if (result != 0)
		return (-1);

	if (r != NULL)
		*r = data;
	return (0);
}

int
myx_boot(struct myx_softc *sc, u_int32_t length)
{
	struct myx_bootcmd	 bc;
	bus_dmamap_t		 map = sc->sc_cmddma.mxm_map;
	u_int32_t		*status;
	u_int			 i, ret = 1;

	memset(&bc, 0, sizeof(bc));
	bc.bc_addr_high = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	bc.bc_addr_low = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	bc.bc_result = 0xffffffff;
	bc.bc_offset = htobe32(MYX_FW_BOOT);
	bc.bc_length = htobe32(length - 8);
	bc.bc_copyto = htobe32(8);
	bc.bc_jumpto = htobe32(0);

	status = (u_int32_t *)sc->sc_cmddma.mxm_kva;
	*status = 0;

	/* Send command */
	myx_write(sc, MYX_BOOT, &bc, sizeof(bc));
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	for (i = 0; i < 200; i++) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		if (*status == 0xffffffff) {
			ret = 0;
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);
		delay(1000);
	}

	DPRINTF(MYXDBG_CMD, "%s: boot completed, i %d, result %d\n",
	    DEVNAME(sc), i, ret);

	return (ret);
}

int
myx_rdma(struct myx_softc *sc, u_int do_enable)
{
	struct myx_rdmacmd	 rc;
	bus_dmamap_t		 map = sc->sc_cmddma.mxm_map;
	bus_dmamap_t		 pad = sc->sc_paddma.mxm_map;
	u_int32_t		*status;
	int			 ret = 1;
	u_int			 i;

	/*
	 * It is required to setup a _dummy_ RDMA address. It also makes
	 * some PCI-E chipsets resend dropped messages.
	 */
	rc.rc_addr_high = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	rc.rc_addr_low = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	rc.rc_result = 0xffffffff;
	rc.rc_rdma_high = htobe32(MYX_ADDRHIGH(pad->dm_segs[0].ds_addr));
	rc.rc_rdma_low = htobe32(MYX_ADDRLOW(pad->dm_segs[0].ds_addr));
	rc.rc_enable = htobe32(do_enable);

	status = (u_int32_t *)sc->sc_cmddma.mxm_kva;
	*status = 0;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	/* Send command */
	myx_write(sc, MYX_RDMA, &rc, sizeof(rc));

	for (i = 0; i < 20; i++) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);

		if (*status == 0xffffffff) {
			ret = 0;
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);
		delay(1000);
	}

	DPRINTF(MYXDBG_CMD, "%s(%s): dummy RDMA %s, i %d, result 0x%x\n",
	    DEVNAME(sc), __func__,
	    do_enable ? "enabled" : "disabled", i, betoh32(*status));

	return (ret);
}

int
myx_media_change(struct ifnet *ifp)
{
	/* ignore */
	return (0);
}

void
myx_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct myx_softc	*sc = (struct myx_softc *)ifp->if_softc;
	u_int32_t		 sts;

	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	if (!ISSET(ifp->if_flags, IFF_RUNNING)) {
		imr->ifm_status = 0;
		return;
	}

	myx_sts_enter(sc);
	sts = sc->sc_sts->ms_linkstate;
	myx_sts_leave(sc);

	myx_link_state(sc, sts);

	imr->ifm_status = IFM_AVALID;
	if (!LINK_STATE_IS_UP(ifp->if_link_state))
		return;

	imr->ifm_active |= IFM_FDX | IFM_FLOW |
	    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE;
	imr->ifm_status |= IFM_ACTIVE;
}

void
myx_link_state(struct myx_softc *sc, u_int32_t sts)
{
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	int			 link_state = LINK_STATE_DOWN;

	if (betoh32(sts) == MYXSTS_LINKUP)
		link_state = LINK_STATE_FULL_DUPLEX;
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
		ifp->if_baudrate = LINK_STATE_IS_UP(ifp->if_link_state) ?
		    IF_Gbps(10) : 0;
	}
}

void
myx_watchdog(struct ifnet *ifp)
{
	return;
}

int
myx_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct myx_softc	*sc = (struct myx_softc *)ifp->if_softc;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
	int			 s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				myx_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				myx_down(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCGIFRXR:
		error = myx_rxrinfo(sc, (struct if_rxrinfo *)ifr->ifr_data);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			myx_iff(sc);
		error = 0;
	}

	splx(s);
	return (error);
}

int
myx_rxrinfo(struct myx_softc *sc, struct if_rxrinfo *ifri)
{
	struct if_rxring_info ifr[2];

	memset(ifr, 0, sizeof(ifr));

	ifr[0].ifr_size = MYX_RXSMALL_SIZE;
	ifr[0].ifr_info = sc->sc_rx_ring[0].mrr_rxr;

	ifr[1].ifr_size = MYX_RXBIG_SIZE;
	ifr[1].ifr_info = sc->sc_rx_ring[1].mrr_rxr;

	return (if_rxr_info_ioctl(ifri, nitems(ifr), ifr));
}

void
myx_up(struct myx_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	struct myx_cmd		mc;
	bus_dmamap_t		map;
	size_t			size;
	u_int			maxpkt;
	u_int32_t		r;

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
		return;
	}

	if (myx_dmamem_alloc(sc, &sc->sc_zerodma,
	    64, MYXALIGN_CMD) != 0) {
		printf("%s: failed to allocate zero pad memory\n",
		    DEVNAME(sc));
		return;
	}
	memset(sc->sc_zerodma.mxm_kva, 0, 64);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_zerodma.mxm_map, 0,
	    sc->sc_zerodma.mxm_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	if (myx_dmamem_alloc(sc, &sc->sc_paddma,
	    MYXALIGN_CMD, MYXALIGN_CMD) != 0) {
		printf("%s: failed to allocate pad DMA memory\n",
		    DEVNAME(sc));
		goto free_zero;
	}
	bus_dmamap_sync(sc->sc_dmat, sc->sc_paddma.mxm_map, 0,
	    sc->sc_paddma.mxm_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (myx_rdma(sc, MYXRDMA_ON) != 0) {
		printf("%s: failed to enable dummy RDMA\n", DEVNAME(sc));
		goto free_pad;
	}

	if (myx_cmd(sc, MYXCMD_GET_RXRINGSZ, &mc, &r) != 0) {
		printf("%s: unable to get rx ring size\n", DEVNAME(sc));
		goto free_pad;
	}
	sc->sc_rx_ring_count = r / sizeof(struct myx_rx_desc);

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_TXRINGSZ, &mc, &r) != 0) {
		printf("%s: unable to get tx ring size\n", DEVNAME(sc));
		goto free_pad;
	}
	sc->sc_tx_ring_idx = 0;
	sc->sc_tx_ring_count = r / sizeof(struct myx_tx_desc);
	sc->sc_tx_free = sc->sc_tx_ring_count - 1;
	sc->sc_tx_nsegs = min(16, sc->sc_tx_ring_count / 4); /* magic */
	sc->sc_tx_count = 0;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->sc_tx_ring_count - 1);
	IFQ_SET_READY(&ifp->if_snd);

	/* Allocate Interrupt Queue */

	sc->sc_intrq_count = sc->sc_rx_ring_count * 2;
	sc->sc_intrq_idx = 0;

	size = sc->sc_intrq_count * sizeof(struct myx_intrq_desc);
	if (myx_dmamem_alloc(sc, &sc->sc_intrq_dma,
	    size, MYXALIGN_DATA) != 0) {
		goto free_pad;
	}
	sc->sc_intrq = (struct myx_intrq_desc *)sc->sc_intrq_dma.mxm_kva;
	map = sc->sc_intrq_dma.mxm_map;
	memset(sc->sc_intrq, 0, size);
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(size);
	if (myx_cmd(sc, MYXCMD_SET_INTRQSZ, &mc, NULL) != 0) {
		printf("%s: failed to set intrq size\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	if (myx_cmd(sc, MYXCMD_SET_INTRQDMA, &mc, NULL) != 0) {
		printf("%s: failed to set intrq address\n", DEVNAME(sc));
		goto free_intrq;
	}

	/*
	 * get interrupt offsets
	 */

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_INTRACKOFF, &mc,
	    &sc->sc_irqclaimoff) != 0) {
		printf("%s: failed to get IRQ ack offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_INTRDEASSERTOFF, &mc,
	    &sc->sc_irqdeassertoff) != 0) {
		printf("%s: failed to get IRQ deassert offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_INTRCOALDELAYOFF, &mc,
	    &sc->sc_irqcoaloff) != 0) {
		printf("%s: failed to get IRQ coal offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	/* Set an appropriate interrupt coalescing period */
	r = htobe32(MYX_IRQCOALDELAY);
	myx_write(sc, sc->sc_irqcoaloff, &r, sizeof(r));

	if (myx_setlladdr(sc, MYXCMD_SET_LLADDR, LLADDR(ifp->if_sadl)) != 0) {
		printf("%s: failed to configure lladdr\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_UNSET_PROMISC, &mc, NULL) != 0) {
		printf("%s: failed to disable promisc mode\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_FC_DEFAULT, &mc, NULL) != 0) {
		printf("%s: failed to configure flow control\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_TXRINGOFF, &mc,
	    &sc->sc_tx_ring_offset) != 0) {
		printf("%s: unable to get tx ring offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_RXSMALLRINGOFF, &mc,
	    &sc->sc_rx_ring[MYX_RXSMALL].mrr_offset) != 0) {
		printf("%s: unable to get small rx ring offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_RXBIGRINGOFF, &mc,
	    &sc->sc_rx_ring[MYX_RXBIG].mrr_offset) != 0) {
		printf("%s: unable to get big rx ring offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	/* Allocate Interrupt Data */
	if (myx_dmamem_alloc(sc, &sc->sc_sts_dma,
	    sizeof(struct myx_status), MYXALIGN_DATA) != 0) {
		printf("%s: failed to allocate status DMA memory\n",
		    DEVNAME(sc));
		goto free_intrq;
	}
	sc->sc_sts = (struct myx_status *)sc->sc_sts_dma.mxm_kva;
	map = sc->sc_sts_dma.mxm_map;
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc.mc_data2 = htobe32(sizeof(struct myx_status));
	if (myx_cmd(sc, MYXCMD_SET_STATSDMA, &mc, NULL) != 0) {
		printf("%s: failed to set status DMA offset\n", DEVNAME(sc));
		goto free_sts;
	}

	maxpkt = ifp->if_hardmtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(maxpkt);
	if (myx_cmd(sc, MYXCMD_SET_MTU, &mc, NULL) != 0) {
		printf("%s: failed to set MTU size %d\n", DEVNAME(sc), maxpkt);
		goto free_sts;
	}

	if (myx_tx_init(sc, maxpkt) != 0)
		goto free_sts;

	if (myx_rx_init(sc, MYX_RXSMALL, MCLBYTES) != 0)
		goto free_tx_ring;

	if (myx_rx_fill(sc, &sc->sc_rx_ring[MYX_RXSMALL]) != 0)
		goto free_rx_ring_small;

	if (myx_rx_init(sc, MYX_RXBIG, MYX_RXBIG_SIZE) != 0)
		goto empty_rx_ring_small;

	if (myx_rx_fill(sc, &sc->sc_rx_ring[MYX_RXBIG]) != 0)
		goto free_rx_ring_big;

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_RXSMALL_SIZE - ETHER_ALIGN);
	if (myx_cmd(sc, MYXCMD_SET_SMALLBUFSZ, &mc, NULL) != 0) {
		printf("%s: failed to set small buf size\n", DEVNAME(sc));
		goto empty_rx_ring_big;
	}

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(16384);
	if (myx_cmd(sc, MYXCMD_SET_BIGBUFSZ, &mc, NULL) != 0) {
		printf("%s: failed to set big buf size\n", DEVNAME(sc));
		goto empty_rx_ring_big;
	}

	mtx_enter(&sc->sc_sts_mtx);
	sc->sc_state = MYX_S_RUNNING;
	mtx_leave(&sc->sc_sts_mtx);

	if (myx_cmd(sc, MYXCMD_SET_IFUP, &mc, NULL) != 0) {
		printf("%s: failed to start the device\n", DEVNAME(sc));
		goto empty_rx_ring_big;
	}

	CLR(ifp->if_flags, IFF_OACTIVE);
	SET(ifp->if_flags, IFF_RUNNING);
	myx_iff(sc);
	myx_start(ifp);

	return;

empty_rx_ring_big:
	myx_rx_empty(sc, &sc->sc_rx_ring[MYX_RXBIG]);
free_rx_ring_big:
	myx_rx_free(sc, &sc->sc_rx_ring[MYX_RXBIG]);
empty_rx_ring_small:
	myx_rx_empty(sc, &sc->sc_rx_ring[MYX_RXSMALL]);
free_rx_ring_small:
	myx_rx_free(sc, &sc->sc_rx_ring[MYX_RXSMALL]);
free_tx_ring:
	myx_tx_free(sc);
free_sts:
	bus_dmamap_sync(sc->sc_dmat, sc->sc_sts_dma.mxm_map, 0,
	    sc->sc_sts_dma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	myx_dmamem_free(sc, &sc->sc_sts_dma);
free_intrq:
	bus_dmamap_sync(sc->sc_dmat, sc->sc_intrq_dma.mxm_map, 0,
	    sc->sc_intrq_dma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	myx_dmamem_free(sc, &sc->sc_intrq_dma);
free_pad:
	bus_dmamap_sync(sc->sc_dmat, sc->sc_paddma.mxm_map, 0,
	    sc->sc_paddma.mxm_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	myx_dmamem_free(sc, &sc->sc_paddma);

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
	}
free_zero:
	bus_dmamap_sync(sc->sc_dmat, sc->sc_zerodma.mxm_map, 0,
	    sc->sc_zerodma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	myx_dmamem_free(sc, &sc->sc_zerodma);
}

int
myx_setlladdr(struct myx_softc *sc, u_int32_t cmd, u_int8_t *addr)
{
	struct myx_cmd		 mc;

	memset(&mc, 0, sizeof(mc));
	mc.mc_data0 = htobe32(addr[0] << 24 | addr[1] << 16 |
	    addr[2] << 8 | addr[3]);
	mc.mc_data1 = htobe32(addr[4] << 8 | addr[5]);

	if (myx_cmd(sc, cmd, &mc, NULL) != 0) {
		printf("%s: failed to set the lladdr\n", DEVNAME(sc));
		return (-1);
	}
	return (0);
}

void
myx_iff(struct myx_softc *sc)
{
	struct myx_cmd		mc;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	u_int8_t *addr;

	CLR(ifp->if_flags, IFF_ALLMULTI);

	if (myx_cmd(sc, ISSET(ifp->if_flags, IFF_PROMISC) ?
	    MYXCMD_SET_PROMISC : MYXCMD_UNSET_PROMISC, &mc, NULL) != 0) {
		printf("%s: failed to configure promisc mode\n", DEVNAME(sc));
		return;
	}

	if (myx_cmd(sc, MYXCMD_SET_ALLMULTI, &mc, NULL) != 0) {
		printf("%s: failed to enable ALLMULTI\n", DEVNAME(sc));
		return;
	}

	if (myx_cmd(sc, MYXCMD_UNSET_MCAST, &mc, NULL) != 0) {
		printf("%s: failed to leave all mcast groups \n", DEVNAME(sc));
		return;
	}

	if (ISSET(ifp->if_flags, IFF_PROMISC) ||
	    sc->sc_ac.ac_multirangecnt > 0) {
		SET(ifp->if_flags, IFF_ALLMULTI);
		return;
	}

	ETHER_FIRST_MULTI(step, &sc->sc_ac, enm);
	while (enm != NULL) {
		addr = enm->enm_addrlo;

		memset(&mc, 0, sizeof(mc));
		mc.mc_data0 = htobe32(addr[0] << 24 | addr[1] << 16 |
		    addr[2] << 8 | addr[3]);
		mc.mc_data1 = htobe32(addr[4] << 24 | addr[5] << 16);
		if (myx_cmd(sc, MYXCMD_SET_MCASTGROUP, &mc, NULL) != 0) {
			printf("%s: failed to join mcast group\n", DEVNAME(sc));
			return;
		}

		ETHER_NEXT_MULTI(step, enm);
	}

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_UNSET_ALLMULTI, &mc, NULL) != 0) {
		printf("%s: failed to disable ALLMULTI\n", DEVNAME(sc));
		return;
	}
}

void
myx_down(struct myx_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	volatile struct myx_status *sts = sc->sc_sts;
	bus_dmamap_t		 map = sc->sc_sts_dma.mxm_map;
	struct myx_cmd		 mc;
	int			 s;
	int			 ring;

	myx_sts_enter(sc);
	sc->sc_linkdown = sts->ms_linkdown;
	sc->sc_state = MYX_S_DOWN;

	memset(&mc, 0, sizeof(mc));
	(void)myx_cmd(sc, MYXCMD_SET_IFDOWN, &mc, NULL);

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	while (sc->sc_state != MYX_S_OFF)
		msleep(sts, &sc->sc_sts_mtx, 0, "myxdown", 0);
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	mtx_leave(&sc->sc_sts_mtx);

	s = splnet();
	if (ifp->if_link_state != LINK_STATE_UNKNOWN) {
		ifp->if_link_state = LINK_STATE_UNKNOWN;
		ifp->if_baudrate = 0;
		if_link_state_change(ifp);
	}
	splx(s);

	memset(&mc, 0, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
	}

	CLR(ifp->if_flags, IFF_RUNNING | IFF_OACTIVE);

	for (ring = 0; ring < 2; ring++) {
		struct myx_rx_ring *mrr = &sc->sc_rx_ring[ring];

		timeout_del(&mrr->mrr_refill);
		myx_rx_empty(sc, mrr);
		myx_rx_free(sc, mrr);
	}

	myx_tx_empty(sc);
	myx_tx_free(sc);

	/* the sleep shizz above already synced this dmamem */
	myx_dmamem_free(sc, &sc->sc_sts_dma);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_intrq_dma.mxm_map, 0,
	    sc->sc_intrq_dma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	myx_dmamem_free(sc, &sc->sc_intrq_dma);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_paddma.mxm_map, 0,
	    sc->sc_paddma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	myx_dmamem_free(sc, &sc->sc_paddma);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_zerodma.mxm_map, 0,
	    sc->sc_zerodma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	myx_dmamem_free(sc, &sc->sc_zerodma);
}

void
myx_write_txd_tail(struct myx_softc *sc, struct myx_slot *ms, u_int8_t flags,
    u_int32_t offset, u_int idx)
{
	struct myx_tx_desc		txd;
	bus_dmamap_t			zmap = sc->sc_zerodma.mxm_map;
	bus_dmamap_t			map = ms->ms_map;
	int				i;

	for (i = 1; i < map->dm_nsegs; i++) {
		memset(&txd, 0, sizeof(txd));
		txd.tx_addr = htobe64(map->dm_segs[i].ds_addr);
		txd.tx_length = htobe16(map->dm_segs[i].ds_len);
		txd.tx_flags = flags;

		myx_bus_space_write(sc,
		    offset + sizeof(txd) * ((idx + i) % sc->sc_tx_ring_count),
		    &txd, sizeof(txd));
	}

	/* pad runt frames */
	if (map->dm_mapsize < 60) {
		memset(&txd, 0, sizeof(txd));
		txd.tx_addr = htobe64(zmap->dm_segs[0].ds_addr);
		txd.tx_length = htobe16(60 - map->dm_mapsize);
		txd.tx_flags = flags;

		myx_bus_space_write(sc,
		    offset + sizeof(txd) * ((idx + i) % sc->sc_tx_ring_count),
		    &txd, sizeof(txd));
	}
}

void
myx_start(struct ifnet *ifp)
{
	struct myx_tx_desc		txd;
	struct myx_softc		*sc = ifp->if_softc;
	struct myx_slot			*ms;
	bus_dmamap_t			map;
	struct mbuf			*m;
	u_int32_t			offset = sc->sc_tx_ring_offset;
	u_int				idx, cons, prod;
	u_int				free, used;
	u_int8_t			flags;

	if (!ISSET(ifp->if_flags, IFF_RUNNING) ||
	    ISSET(ifp->if_flags, IFF_OACTIVE) ||
	    IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	cons = prod = sc->sc_tx_prod;
	free = sc->sc_tx_free;
	used = 0;

	for (;;) {
		if (used + sc->sc_tx_nsegs > free) {
			SET(ifp->if_flags, IFF_OACTIVE);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		ms = &sc->sc_tx_slots[prod];

		if (myx_load_mbuf(sc, ms, m) != 0) {
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		map = ms->ms_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    map->dm_mapsize, BUS_DMASYNC_POSTWRITE);

		used += map->dm_nsegs + (map->dm_mapsize < 60 ? 1 : 0);

		if (++prod >= sc->sc_tx_ring_count)
			prod = 0;
	}

	if (cons == prod)
		return;

	atomic_sub_int(&sc->sc_tx_free, used);

	ms = &sc->sc_tx_slots[cons];
	idx = sc->sc_tx_ring_idx;

	for (;;) {
		idx += ms->ms_map->dm_nsegs +
		    (ms->ms_map->dm_mapsize < 60 ? 1 : 0);
		if (idx >= sc->sc_tx_ring_count)
			idx -= sc->sc_tx_ring_count;

		if (++cons >= sc->sc_tx_ring_count)
			cons = 0;

		if (cons == prod)
			break;

		ms = &sc->sc_tx_slots[cons];
		map = ms->ms_map;

		flags = MYXTXD_FLAGS_NO_TSO;
		if (map->dm_mapsize < 1520)
			flags |= MYXTXD_FLAGS_SMALL;

		memset(&txd, 0, sizeof(txd));
		txd.tx_addr = htobe64(map->dm_segs[0].ds_addr);
		txd.tx_length = htobe16(map->dm_segs[0].ds_len);
		txd.tx_nsegs = map->dm_nsegs + (map->dm_mapsize < 60 ? 1 : 0);
		txd.tx_flags = flags | MYXTXD_FLAGS_FIRST;
		myx_bus_space_write(sc,
		    offset + sizeof(txd) * idx, &txd, sizeof(txd));

		myx_write_txd_tail(sc, ms, flags, offset, idx);
	}

	/* go back and post first packet */
	ms = &sc->sc_tx_slots[sc->sc_tx_prod];
	map = ms->ms_map;

	flags = MYXTXD_FLAGS_NO_TSO;
	if (map->dm_mapsize < 1520)
		flags |= MYXTXD_FLAGS_SMALL;

	memset(&txd, 0, sizeof(txd));
	txd.tx_addr = htobe64(map->dm_segs[0].ds_addr);
	txd.tx_length = htobe16(map->dm_segs[0].ds_len);
	txd.tx_nsegs = map->dm_nsegs + (map->dm_mapsize < 60 ? 1 : 0);
	txd.tx_flags = flags | MYXTXD_FLAGS_FIRST;

	/* make sure the first descriptor is seen after the others */
	myx_write_txd_tail(sc, ms, flags, offset, sc->sc_tx_ring_idx);

	myx_bus_space_write(sc,
	    offset + sizeof(txd) * sc->sc_tx_ring_idx, &txd,
	    sizeof(txd) - sizeof(myx_bus_t));

	bus_space_barrier(sc->sc_memt, sc->sc_memh, offset,
	    sizeof(txd) * sc->sc_tx_ring_count, BUS_SPACE_BARRIER_WRITE);

	myx_bus_space_write(sc,
	    offset + sizeof(txd) * (sc->sc_tx_ring_idx + 1) - sizeof(myx_bus_t),
	    (u_int8_t *)&txd + sizeof(txd) - sizeof(myx_bus_t),
	    sizeof(myx_bus_t));

	bus_space_barrier(sc->sc_memt, sc->sc_memh,
	    offset + sizeof(txd) * sc->sc_tx_ring_idx, sizeof(txd),
	    BUS_SPACE_BARRIER_WRITE);

	/* commit */
	sc->sc_tx_ring_idx = idx;
	sc->sc_tx_prod = prod;
}

int
myx_load_mbuf(struct myx_softc *sc, struct myx_slot *ms, struct mbuf *m)
{
	bus_dma_tag_t			dmat = sc->sc_dmat;
	bus_dmamap_t			dmap = ms->ms_map;

	switch (bus_dmamap_load_mbuf(dmat, dmap, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT)) {
	case 0:
		break;

	case EFBIG: /* mbuf chain is too fragmented */
		if (m_defrag(m, M_DONTWAIT) == 0 &&
		    bus_dmamap_load_mbuf(dmat, dmap, m,
		    BUS_DMA_STREAMING | BUS_DMA_NOWAIT) == 0)
			break;
	default:
		return (1);
	}

	ms->ms_m = m;
	return (0);
}

int
myx_intr(void *arg)
{
	struct myx_softc	*sc = (struct myx_softc *)arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	volatile struct myx_status *sts = sc->sc_sts;
	enum myx_state		 state = MYX_S_RUNNING;
	bus_dmamap_t		 map = sc->sc_sts_dma.mxm_map;
	u_int32_t		 data, link = 0xffffffff;
	u_int8_t		 valid = 0;

	mtx_enter(&sc->sc_sts_mtx);
	if (sc->sc_state == MYX_S_OFF) {
		mtx_leave(&sc->sc_sts_mtx);
		return (0);
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	valid = sts->ms_isvalid;
	if (valid == 0x0) {
		myx_sts_leave(sc);
		return (0);
	}

	if (sc->sc_intx) {
		data = htobe32(0);
		bus_space_write_raw_region_4(sc->sc_memt, sc->sc_memh,
		    sc->sc_irqdeassertoff, &data, sizeof(data));
	}
	sts->ms_isvalid = 0;

	do {
		data = sts->ms_txdonecnt;

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE |
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	} while (sts->ms_isvalid);

	if (sts->ms_statusupdated) {
		link = sts->ms_linkstate;

		if (sc->sc_state == MYX_S_DOWN &&
		    sc->sc_linkdown != sts->ms_linkdown)
			state = MYX_S_DOWN;
	}
	myx_sts_leave(sc);

	data = betoh32(data);
	if (data != sc->sc_tx_count)
		myx_txeof(sc, data);

	data = htobe32(3);
	if (valid & 0x1) {
		myx_rxeof(sc);

		bus_space_write_raw_region_4(sc->sc_memt, sc->sc_memh,
		    sc->sc_irqclaimoff, &data, sizeof(data));
	}
	bus_space_write_raw_region_4(sc->sc_memt, sc->sc_memh,
	    sc->sc_irqclaimoff + sizeof(data), &data, sizeof(data));

	if (state == MYX_S_DOWN) {
		/* myx_down is waiting for us */
		mtx_enter(&sc->sc_sts_mtx);
		sc->sc_state = MYX_S_OFF;
		wakeup(sts);
		mtx_leave(&sc->sc_sts_mtx);

		return (1);
	}

	if (link != 0xffffffff) {
		KERNEL_LOCK();
		myx_link_state(sc, link);
		KERNEL_UNLOCK();
	}

	if (ISSET(ifp->if_flags, IFF_OACTIVE)) {
		KERNEL_LOCK();
		CLR(ifp->if_flags, IFF_OACTIVE);
		myx_start(ifp);
		KERNEL_UNLOCK();
	}

	return (1);
}

void
myx_refill(void *xmrr)
{
	struct myx_rx_ring *mrr = xmrr;
	struct myx_softc *sc = mrr->mrr_softc;

	myx_rx_fill(sc, mrr);

	if (mrr->mrr_prod == mrr->mrr_cons)
		timeout_add(&mrr->mrr_refill, 1);
}

void
myx_txeof(struct myx_softc *sc, u_int32_t done_count)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct myx_slot *ms;
	bus_dmamap_t map;
	u_int free = 0;
	u_int cons;

	cons = sc->sc_tx_cons;

	do {
		ms = &sc->sc_tx_slots[cons];
		map = ms->ms_map;

		free += map->dm_nsegs + (map->dm_mapsize < 60 ? 1 : 0);

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);
		m_freem(ms->ms_m);

		ifp->if_opackets++;

		if (++cons >= sc->sc_tx_ring_count)
			cons = 0;
	} while (++sc->sc_tx_count != done_count);

	sc->sc_tx_cons = cons;
	atomic_add_int(&sc->sc_tx_free, free);
}

void
myx_rxeof(struct myx_softc *sc)
{
	static const struct myx_intrq_desc zerodesc = { 0, 0 };
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct myx_rx_ring *mrr;
	struct myx_slot *ms;
	struct mbuf *m;
	int ring;
	u_int rxfree[2] = { 0 , 0 };
	u_int len;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_intrq_dma.mxm_map, 0,
	    sc->sc_intrq_dma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);

	while ((len = betoh16(sc->sc_intrq[sc->sc_intrq_idx].iq_length)) != 0) {
		sc->sc_intrq[sc->sc_intrq_idx] = zerodesc;

		if (++sc->sc_intrq_idx >= sc->sc_intrq_count)
			sc->sc_intrq_idx = 0;

		ring = (len <= (MYX_RXSMALL_SIZE - ETHER_ALIGN)) ?
		    MYX_RXSMALL : MYX_RXBIG;

		mrr = &sc->sc_rx_ring[ring];
		ms = &mrr->mrr_slots[mrr->mrr_cons];

		if (++mrr->mrr_cons >= sc->sc_rx_ring_count)
			mrr->mrr_cons = 0;

		bus_dmamap_sync(sc->sc_dmat, ms->ms_map, 0,
		    ms->ms_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, ms->ms_map);

		m = ms->ms_m;
		m->m_data += ETHER_ALIGN;
		m->m_pkthdr.len = m->m_len = len;

		ml_enqueue(&ml, m);

		rxfree[ring]++;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_intrq_dma.mxm_map, 0,
	    sc->sc_intrq_dma.mxm_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	for (ring = MYX_RXSMALL; ring <= MYX_RXBIG; ring++) {
		if (rxfree[ring] == 0)
			continue;

		mrr = &sc->sc_rx_ring[ring];

		if_rxr_put(&mrr->mrr_rxr, rxfree[ring]);
		myx_rx_fill(sc, mrr);
		if (mrr->mrr_prod == mrr->mrr_cons)
			timeout_add(&mrr->mrr_refill, 0);
	}

	if_input(ifp, &ml);
}

static int
myx_rx_fill_slots(struct myx_softc *sc, struct myx_rx_ring *mrr, u_int slots)
{
	struct myx_rx_desc rxd;
	struct myx_slot *ms;
	u_int32_t offset = mrr->mrr_offset;
	u_int p, first, fills;

	first = p = mrr->mrr_prod;
	if (myx_buf_fill(sc, &mrr->mrr_slots[first], mrr->mrr_mclget) != 0)
		return (slots);

	if (++p >= sc->sc_rx_ring_count)
		p = 0;

	for (fills = 1; fills < slots; fills++) {
		ms = &mrr->mrr_slots[p];

		if (myx_buf_fill(sc, ms, mrr->mrr_mclget) != 0)
			break;

		rxd.rx_addr = htobe64(ms->ms_map->dm_segs[0].ds_addr);
		myx_bus_space_write(sc, offset + p * sizeof(rxd),
		    &rxd, sizeof(rxd));

		if (++p >= sc->sc_rx_ring_count)
			p = 0;
	}

	mrr->mrr_prod = p;

	/* make sure the first descriptor is seen after the others */
	if (fills > 1) {
		bus_space_barrier(sc->sc_memt, sc->sc_memh,
		    offset, sizeof(rxd) * sc->sc_rx_ring_count,
		    BUS_SPACE_BARRIER_WRITE);
	}

	ms = &mrr->mrr_slots[first];
	rxd.rx_addr = htobe64(ms->ms_map->dm_segs[0].ds_addr);
	myx_bus_space_write(sc, offset + first * sizeof(rxd),
	    &rxd, sizeof(rxd));

	return (slots - fills);
}

int
myx_rx_init(struct myx_softc *sc, int ring, bus_size_t size)
{
	struct myx_rx_desc rxd;
	struct myx_rx_ring *mrr = &sc->sc_rx_ring[ring];
	struct myx_slot *ms;
	u_int32_t offset = mrr->mrr_offset;
	int rv;
	int i;

	mrr->mrr_slots = mallocarray(sizeof(*ms), sc->sc_rx_ring_count,
	    M_DEVBUF, M_WAITOK);
	if (mrr->mrr_slots == NULL)
		return (ENOMEM);

	memset(&rxd, 0xff, sizeof(rxd));
	for (i = 0; i < sc->sc_rx_ring_count; i++) {
		ms = &mrr->mrr_slots[i];
		rv = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &ms->ms_map);
		if (rv != 0)
			goto destroy;

		myx_bus_space_write(sc, offset + i * sizeof(rxd),
		    &rxd, sizeof(rxd));
	}

	if_rxr_init(&mrr->mrr_rxr, 2, sc->sc_rx_ring_count - 2);
	mrr->mrr_prod = mrr->mrr_cons = 0;

	return (0);

destroy:
	while (i-- > 0) {
		ms = &mrr->mrr_slots[i];
		bus_dmamap_destroy(sc->sc_dmat, ms->ms_map);
	}
	free(mrr->mrr_slots, M_DEVBUF, sizeof(*ms) * sc->sc_rx_ring_count);
	return (rv);
}

static inline int
myx_rx_ring_enter(struct myx_rx_ring *mrr)
{
	return (atomic_inc_int_nv(&mrr->mrr_running) == 1);
}

static inline int
myx_rx_ring_leave(struct myx_rx_ring *mrr)
{
	if (atomic_cas_uint(&mrr->mrr_running, 1, 0) == 1)
		return (1);

	mrr->mrr_running = 1;

	return (0);
}

int
myx_rx_fill(struct myx_softc *sc, struct myx_rx_ring *mrr)
{
	u_int slots;

	slots = if_rxr_get(&mrr->mrr_rxr, sc->sc_rx_ring_count);
	if (slots == 0)
		return (1);

	slots = myx_rx_fill_slots(sc, mrr, slots);
	if (slots > 0)
		if_rxr_put(&mrr->mrr_rxr, slots);

	return (0);
}

void
myx_rx_empty(struct myx_softc *sc, struct myx_rx_ring *mrr)
{
	struct myx_slot *ms;

	while (mrr->mrr_cons != mrr->mrr_prod) {
		ms = &mrr->mrr_slots[mrr->mrr_cons];

		if (++mrr->mrr_cons >= sc->sc_rx_ring_count)
			mrr->mrr_cons = 0;

		bus_dmamap_sync(sc->sc_dmat, ms->ms_map, 0,
		    ms->ms_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, ms->ms_map);
		m_freem(ms->ms_m);
	}

	if_rxr_init(&mrr->mrr_rxr, 2, sc->sc_rx_ring_count - 2);
}

void
myx_rx_free(struct myx_softc *sc, struct myx_rx_ring *mrr)
{
	struct myx_slot *ms;
	int i;

	for (i = 0; i < sc->sc_rx_ring_count; i++) {
		ms = &mrr->mrr_slots[i];
		bus_dmamap_destroy(sc->sc_dmat, ms->ms_map);
	}

	free(mrr->mrr_slots, M_DEVBUF, sizeof(*ms) * sc->sc_rx_ring_count);
}

struct mbuf *
myx_mcl_small(void)
{
	struct mbuf *m;

	m = MCLGETI(NULL, M_DONTWAIT, NULL, MYX_RXSMALL_SIZE);
	if (m == NULL)
		return (NULL);

	m->m_len = m->m_pkthdr.len = MYX_RXSMALL_SIZE;

	return (m);
}

struct mbuf *
myx_mcl_big(void)
{
	struct mbuf *m;
	void *mcl;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	mcl = pool_get(myx_mcl_pool, PR_NOWAIT);
	if (mcl == NULL) {
		m_free(m);
		return (NULL);
	}

	MEXTADD(m, mcl, MYX_RXBIG_SIZE, M_EXTWR, m_extfree_pool, myx_mcl_pool);
	m->m_len = m->m_pkthdr.len = MYX_RXBIG_SIZE;

	return (m);
}

int
myx_buf_fill(struct myx_softc *sc, struct myx_slot *ms,
    struct mbuf *(*mclget)(void))
{
	struct mbuf *m;
	int rv;

	m = (*mclget)();
	if (m == NULL)
		return (ENOMEM);

	rv = bus_dmamap_load_mbuf(sc->sc_dmat, ms->ms_map, m, BUS_DMA_NOWAIT);
	if (rv != 0) {
		m_freem(m);
		return (rv);
	}

	bus_dmamap_sync(sc->sc_dmat, ms->ms_map, 0,
	    ms->ms_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	ms->ms_m = m;

	return (0);
}

int
myx_tx_init(struct myx_softc *sc, bus_size_t size)
{
	struct myx_slot *ms;
	int rv;
	int i;

	sc->sc_tx_slots = mallocarray(sizeof(*ms), sc->sc_tx_ring_count,
	    M_DEVBUF, M_WAITOK);
	if (sc->sc_tx_slots == NULL)
		return (ENOMEM);

	for (i = 0; i < sc->sc_tx_ring_count; i++) {
		ms = &sc->sc_tx_slots[i];
		rv = bus_dmamap_create(sc->sc_dmat, size, sc->sc_tx_nsegs,
		    sc->sc_tx_boundary, sc->sc_tx_boundary,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &ms->ms_map);
		if (rv != 0)
			goto destroy;
	}

	sc->sc_tx_prod = sc->sc_tx_cons = 0;

	return (0);

destroy:
	while (i-- > 0) {
		ms = &sc->sc_tx_slots[i];
		bus_dmamap_destroy(sc->sc_dmat, ms->ms_map);
	}
	free(sc->sc_tx_slots, M_DEVBUF, sizeof(*ms) * sc->sc_tx_ring_count);
	return (rv);
}

void
myx_tx_empty(struct myx_softc *sc)
{
	struct myx_slot *ms;
	u_int cons = sc->sc_tx_cons;
	u_int prod = sc->sc_tx_prod;

	while (cons != prod) {
		ms = &sc->sc_tx_slots[cons];
		
		bus_dmamap_sync(sc->sc_dmat, ms->ms_map, 0,
		    ms->ms_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ms->ms_map);
		m_freem(ms->ms_m);

		if (++cons >= sc->sc_tx_ring_count)
			cons = 0;
	}

	sc->sc_tx_cons = cons;
}

void
myx_tx_free(struct myx_softc *sc)
{
	struct myx_slot *ms;
	int i;

	for (i = 0; i < sc->sc_tx_ring_count; i++) {
		ms = &sc->sc_tx_slots[i];
		bus_dmamap_destroy(sc->sc_dmat, ms->ms_map);
	}

	free(sc->sc_tx_slots, M_DEVBUF, sizeof(*ms) * sc->sc_tx_ring_count);
}
