/*	$OpenBSD: if_myx.c,v 1.26 2011/06/22 21:04:31 deraadt Exp $	*/

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
#include <sys/timeout.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

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
	const char		*mxm_name;
};

struct myx_buf {
	SIMPLEQ_ENTRY(myx_buf)	 mb_entry;
	bus_dmamap_t		 mb_map;
	struct mbuf		*mb_m;
};
SIMPLEQ_HEAD(myx_buf_list, myx_buf);
struct pool *myx_buf_pool;

struct myx_softc {
	struct device		 sc_dev;
	struct arpcom		 sc_ac;

	pci_chipset_tag_t	 sc_pc;
	pci_intr_handle_t	 sc_ih;
	pcitag_t		 sc_tag;
	u_int			 sc_function;

	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_size_t		 sc_mems;

	struct myx_dmamem	 sc_zerodma;
	struct myx_dmamem	 sc_cmddma;
	struct myx_dmamem	 sc_paddma;

	struct myx_dmamem	 sc_sts_dma;
	volatile struct myx_status	*sc_sts;

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
	u_int32_t		 sc_rx_ring_offset[2];
	struct myx_buf_list	 sc_rx_buf_free[2];
	struct myx_buf_list	 sc_rx_buf_list[2];
	u_int			 sc_rx_ring_idx[2];
#define  MYX_RXSMALL		 0
#define  MYX_RXBIG		 1

	bus_size_t		 sc_tx_boundary;
	u_int			 sc_tx_ring_count;
	u_int32_t		 sc_tx_ring_offset;
	u_int			 sc_tx_nsegs;
	u_int32_t		 sc_tx_count; /* shadows ms_txdonecnt */
	u_int			 sc_tx_free;
	struct myx_buf_list	 sc_tx_buf_free;
	struct myx_buf_list	 sc_tx_buf_list;
	u_int			 sc_tx_ring_idx;

	u_int8_t		 sc_lladdr[ETHER_ADDR_LEN];
	struct ifmedia		 sc_media;

	u_int			 sc_hwflags;
#define  MYXFLAG_FLOW_CONTROL	 (1<<0)		/* Rx/Tx pause is enabled */
	volatile u_int8_t	 sc_linkdown;

	struct timeout		 sc_tick;
};

int	 myx_match(struct device *, void *, void *);
void	 myx_attach(struct device *, struct device *, void *);
int	 myx_query(struct myx_softc *sc, char *, size_t);
u_int	 myx_ether_aton(char *, u_int8_t *, u_int);
void	 myx_attachhook(void *);
int	 myx_loadfirmware(struct myx_softc *, const char *);
int	 myx_probe_firmware(struct myx_softc *);

void	 myx_read(struct myx_softc *, bus_size_t, void *, bus_size_t);
void	 myx_write(struct myx_softc *, bus_size_t, void *, bus_size_t);

int	 myx_cmd(struct myx_softc *, u_int32_t, struct myx_cmd *, u_int32_t *);
int	 myx_boot(struct myx_softc *, u_int32_t);

int	 myx_rdma(struct myx_softc *, u_int);
int	 myx_dmamem_alloc(struct myx_softc *, struct myx_dmamem *,
	    bus_size_t, u_int align, const char *);
void	 myx_dmamem_free(struct myx_softc *, struct myx_dmamem *);
int	 myx_media_change(struct ifnet *);
void	 myx_media_status(struct ifnet *, struct ifmediareq *);
void	 myx_link_state(struct myx_softc *);
void	 myx_watchdog(struct ifnet *);
void	 myx_tick(void *);
int	 myx_ioctl(struct ifnet *, u_long, caddr_t);
void	 myx_up(struct myx_softc *);
void	 myx_iff(struct myx_softc *);
void	 myx_down(struct myx_softc *);

void	 myx_start(struct ifnet *);
int	 myx_load_buf(struct myx_softc *, struct myx_buf *, struct mbuf *);
int	 myx_setlladdr(struct myx_softc *, u_int32_t, u_int8_t *);
int	 myx_intr(void *);
int	 myx_rxeof(struct myx_softc *);
void	 myx_txeof(struct myx_softc *, u_int32_t);

struct myx_buf *	myx_buf_alloc(struct myx_softc *, bus_size_t, int,
			    bus_size_t, bus_size_t);
void			myx_buf_free(struct myx_softc *, struct myx_buf *);
struct myx_buf *	myx_buf_get(struct myx_buf_list *);
void			myx_buf_put(struct myx_buf_list *, struct myx_buf *);
struct myx_buf *	myx_buf_fill(struct myx_softc *, int);

void			myx_rx_zero(struct myx_softc *, int);
int			myx_rx_fill(struct myx_softc *, int);

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
	sc->sc_function = pa->pa_function;

	SIMPLEQ_INIT(&sc->sc_rx_buf_free[MYX_RXSMALL]);
	SIMPLEQ_INIT(&sc->sc_rx_buf_list[MYX_RXSMALL]);
	SIMPLEQ_INIT(&sc->sc_rx_buf_free[MYX_RXBIG]);
	SIMPLEQ_INIT(&sc->sc_rx_buf_list[MYX_RXBIG]);

	SIMPLEQ_INIT(&sc->sc_tx_buf_free);
	SIMPLEQ_INIT(&sc->sc_tx_buf_list);

	/* Map the PCI memory space */
	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, MYXBAR0);
	if (pci_mapreg_map(pa, MYXBAR0, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf(": unable to map register memory\n");
		return;
	}

	/* Get board details (mac/part) */
	bzero(part, sizeof(part));
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
	if (myx_buf_pool == NULL) {
		myx_buf_pool = malloc(sizeof(*myx_buf_pool), M_DEVBUF,
		    M_WAITOK);
		if (myx_buf_pool == NULL) {
			printf("%s: unable to allocate buf pool\n",
			    DEVNAME(sc));
			goto unmap;
		}
		pool_init(myx_buf_pool, sizeof(struct myx_buf),
		    0, 0, 0, "myxbufs", &pool_allocator_nointr);
	}

	if (mountroothook_establish(myx_attachhook, sc) == NULL) {
		printf("%s: unable to establish mountroot hook\n", DEVNAME(sc));
		goto unmap;
	}

	return;

 unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

u_int
myx_ether_aton(char *mac, u_int8_t *lladdr, u_int maxlen)
{
	u_int		i, j;
	u_int8_t	digit;

	bzero(lladdr, ETHER_ADDR_LEN);
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
		if (maxlen > 4 && bcmp("MAC=", &strings[i], 4) == 0) {
			i += 4;
			i += myx_ether_aton(&strings[i],
			    sc->sc_ac.ac_enaddr, maxlen);
		} else if (maxlen > 3 && bcmp("PC=", &strings[i], 3) == 0) {
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

	bcopy(fw + MYX_HEADER_POS, &offset, sizeof(offset));
	offset = betoh32(offset);
	if ((offset + sizeof(hdr)) > fwlen) {
		printf("%s: invalid firmware %s\n", DEVNAME(sc), filename);
		goto err;
	}

	bcopy(fw + offset, &hdr, sizeof(hdr));
	DPRINTF(MYXDBG_INIT, "%s: "
	    "fw hdr off %u, length %u, type 0x%x, version %s\n",
	    DEVNAME(sc), offset, betoh32(hdr.fw_hdrlength),
	    betoh32(hdr.fw_type), hdr.fw_version);

	if (betoh32(hdr.fw_type) != MYXFW_TYPE_ETH ||
	    bcmp(MYXFW_VER, hdr.fw_version, strlen(MYXFW_VER)) != 0) {
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
	free(fw, M_DEVBUF);
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
	    MYXALIGN_CMD, "cmd") != 0) {
		printf("%s: failed to allocate command DMA memory\n",
		    DEVNAME(sc));
		return;
	}

	/* Try the firmware stored on disk */
	if (myx_loadfirmware(sc, MYXFW_ALIGNED) != 0) {
		/* error printed by myx_loadfirmware */
		goto freecmd;
	}

	bzero(&mc, sizeof(mc));

	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
		goto freecmd;
	}

	sc->sc_tx_boundary = 4096;

	if (myx_probe_firmware(sc) != 0) {
		printf("%s: error while selecting firmware\n", DEVNAME(sc));
		goto freecmd;
	}

	sc->sc_irqh = pci_intr_establish(sc->sc_pc, sc->sc_ih, IPL_NET,
	    myx_intr, sc, DEVNAME(sc));
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
	ifp->if_baudrate = 0;

	ifmedia_init(&sc->sc_media, 0, myx_media_change, myx_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->sc_tick, myx_tick, sc);

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

	if (myx_dmamem_alloc(sc, &test, 4096, 4096, "test") != 0)
		return (1);
	map = test.mxm_map;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc.mc_data2 = htobe32(4096 * 0x10000);
	if (myx_cmd(sc, MYXCMD_UNALIGNED_DMA_TEST, &mc, NULL) != 0) {
		printf("%s: DMA read test failed\n", DEVNAME(sc));
		goto fail;
	}

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc.mc_data2 = htobe32(4096 * 0x1);
	if (myx_cmd(sc, MYXCMD_UNALIGNED_DMA_TEST, &mc, NULL) != 0) {
		printf("%s: DMA write test failed\n", DEVNAME(sc));
		goto fail;
	}

	bzero(&mc, sizeof(mc));
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
    bus_size_t size, u_int align, const char *mname)
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

	mxm->mxm_name = mname;

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

	bzero(&bc, sizeof(bc));
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

	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID;

	myx_link_state(sc);

	if (!LINK_STATE_IS_UP(ifp->if_link_state))
		return;

	imr->ifm_active |= IFM_FDX;
	imr->ifm_status |= IFM_ACTIVE;

	/* Flow control */
	if (sc->sc_hwflags & MYXFLAG_FLOW_CONTROL)
		imr->ifm_active |= IFM_FLOW | IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE;
}

void
myx_link_state(struct myx_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	int			 link_state = LINK_STATE_DOWN;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	if (betoh32(sc->sc_sts->ms_linkstate) == MYXSTS_LINKUP)
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

void
myx_tick(void *arg)
{
	struct myx_softc	*sc = (struct myx_softc *)arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	myx_link_state(sc);
	timeout_add_sec(&sc->sc_tick, 1);
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
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				myx_iff(sc);
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

void
myx_up(struct myx_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	struct myx_buf		*mb;
	struct myx_cmd		mc;
	bus_dmamap_t		map;
	size_t			size;
	u_int			maxpkt;
	u_int32_t		r;
	int			i;

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
		return;
	}

	if (myx_dmamem_alloc(sc, &sc->sc_zerodma,
	    64, MYXALIGN_CMD, "zero") != 0) {
		printf("%s: failed to allocate zero pad memory\n",
		    DEVNAME(sc));
		return;
	}
	bzero(sc->sc_zerodma.mxm_kva, 64);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_zerodma.mxm_map, 0,
	    sc->sc_zerodma.mxm_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	if (myx_dmamem_alloc(sc, &sc->sc_paddma,
	    MYXALIGN_CMD, MYXALIGN_CMD, "pad") != 0) {
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

	m_clsetwms(ifp, MCLBYTES, 2, sc->sc_rx_ring_count - 2);
	m_clsetwms(ifp, 12 * 1024, 2, sc->sc_rx_ring_count - 2);

	bzero(&mc, sizeof(mc));
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
	    size, MYXALIGN_DATA, "intrq") != 0) {
		goto free_pad;
	}
	sc->sc_intrq = (struct myx_intrq_desc *)sc->sc_intrq_dma.mxm_kva;
	map = sc->sc_intrq_dma.mxm_map;
	bzero(sc->sc_intrq, size);
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(size);
	if (myx_cmd(sc, MYXCMD_SET_INTRQSZ, &mc, NULL) != 0) {
		printf("%s: failed to set intrq size\n", DEVNAME(sc));
		goto free_intrq;
	}

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	if (myx_cmd(sc, MYXCMD_SET_INTRQDMA, &mc, NULL) != 0) {
		printf("%s: failed to set intrq address\n", DEVNAME(sc));
		goto free_intrq;
	}

	/*
	 * get interrupt offsets
	 */

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_INTRACKOFF, &mc,
	    &sc->sc_irqclaimoff) != 0) {
		printf("%s: failed to get IRQ ack offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_INTRDEASSERTOFF, &mc,
	    &sc->sc_irqdeassertoff) != 0) {
		printf("%s: failed to get IRQ deassert offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	bzero(&mc, sizeof(mc));
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

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_UNSET_PROMISC, &mc, NULL) != 0) {
		printf("%s: failed to disable promisc mode\n", DEVNAME(sc));
		goto free_intrq;
	}

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_FC_DEFAULT, &mc, NULL) != 0) {
		printf("%s: failed to configure flow control\n", DEVNAME(sc));
		goto free_intrq;
	}

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_TXRINGOFF, &mc,
	    &sc->sc_tx_ring_offset) != 0) {
		printf("%s: unable to get tx ring offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_RXSMALLRINGOFF, &mc,
	    &sc->sc_rx_ring_offset[MYX_RXSMALL]) != 0) {
		printf("%s: unable to get small rx ring offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_GET_RXBIGRINGOFF, &mc,
	    &sc->sc_rx_ring_offset[MYX_RXBIG]) != 0) {
		printf("%s: unable to get big rx ring offset\n", DEVNAME(sc));
		goto free_intrq;
	}

	/* Allocate Interrupt Data */
	if (myx_dmamem_alloc(sc, &sc->sc_sts_dma,
	    sizeof(struct myx_status), MYXALIGN_DATA, "status") != 0) {
		printf("%s: failed to allocate status DMA memory\n",
		    DEVNAME(sc));
		goto free_intrq;
	}
	sc->sc_sts = (struct myx_status *)sc->sc_sts_dma.mxm_kva;
	map = sc->sc_sts_dma.mxm_map;
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	mc.mc_data1 = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	mc.mc_data2 = htobe32(sizeof(struct myx_status));
	if (myx_cmd(sc, MYXCMD_SET_STATSDMA, &mc, NULL) != 0) {
		printf("%s: failed to set status DMA offset\n", DEVNAME(sc));
		goto free_sts;
	}

	maxpkt = ifp->if_hardmtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(maxpkt);
	if (myx_cmd(sc, MYXCMD_SET_MTU, &mc, NULL) != 0) {
		printf("%s: failed to set MTU size %d\n", DEVNAME(sc), maxpkt);
		goto free_sts;
	}

	for (i = 0; i < sc->sc_tx_ring_count; i++) {
		mb = myx_buf_alloc(sc, maxpkt, sc->sc_tx_nsegs,
		    sc->sc_tx_boundary, sc->sc_tx_boundary);
		if (mb == NULL)
			goto free_tx_bufs;

		myx_buf_put(&sc->sc_tx_buf_free, mb);
	}

	for (i = 0; i < sc->sc_rx_ring_count; i++) {
		mb = myx_buf_alloc(sc, MCLBYTES, 1, 4096, 4096);
		if (mb == NULL)
			goto free_rxsmall_bufs;

		myx_buf_put(&sc->sc_rx_buf_free[MYX_RXSMALL], mb);
	}

	for (i = 0; i < sc->sc_rx_ring_count; i++) {
		mb = myx_buf_alloc(sc, 12 * 1024, 1, 12 * 1024, 0);
		if (mb == NULL)
			goto free_rxbig_bufs;

		myx_buf_put(&sc->sc_rx_buf_free[MYX_RXBIG], mb);
	}

	myx_rx_zero(sc, MYX_RXSMALL);
	if (myx_rx_fill(sc, MYX_RXSMALL) != 0) {
		printf("%s: failed to fill small rx ring\n", DEVNAME(sc));
		goto free_rxbig_bufs;
	}

	myx_rx_zero(sc, MYX_RXBIG);
	if (myx_rx_fill(sc, MYX_RXBIG) != 0) {
		printf("%s: failed to fill big rx ring\n", DEVNAME(sc));
		goto free_rxsmall;
	}

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(MCLBYTES - ETHER_ALIGN);
	if (myx_cmd(sc, MYXCMD_SET_SMALLBUFSZ, &mc, NULL) != 0) {
		printf("%s: failed to set small buf size\n", DEVNAME(sc));
		goto free_rxbig;
	}

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(16384);
	if (myx_cmd(sc, MYXCMD_SET_BIGBUFSZ, &mc, NULL) != 0) {
		printf("%s: failed to set big buf size\n", DEVNAME(sc));
		goto free_rxbig;
	}

	if (myx_cmd(sc, MYXCMD_SET_IFUP, &mc, NULL) != 0) {
		printf("%s: failed to start the device\n", DEVNAME(sc));
		goto free_rxbig;
	}

	CLR(ifp->if_flags, IFF_OACTIVE);
	SET(ifp->if_flags, IFF_RUNNING);

	myx_iff(sc);

	return;

free_rxbig:
	while ((mb = myx_buf_get(&sc->sc_rx_buf_list[MYX_RXBIG])) != NULL) {
		bus_dmamap_sync(sc->sc_dmat, mb->mb_map, 0,
		    mb->mb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, mb->mb_map);
		m_freem(mb->mb_m);
		myx_buf_free(sc, mb);
	}
free_rxsmall:
	while ((mb = myx_buf_get(&sc->sc_rx_buf_list[MYX_RXSMALL])) != NULL) {
		bus_dmamap_sync(sc->sc_dmat, mb->mb_map, 0,
		    mb->mb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, mb->mb_map);
		m_freem(mb->mb_m);
		myx_buf_free(sc, mb);
	}
free_rxbig_bufs:
	while ((mb = myx_buf_get(&sc->sc_rx_buf_free[MYX_RXBIG])) != NULL)
		myx_buf_free(sc, mb);
free_rxsmall_bufs:
	while ((mb = myx_buf_get(&sc->sc_rx_buf_free[MYX_RXSMALL])) != NULL)
		myx_buf_free(sc, mb);
free_tx_bufs:
	while ((mb = myx_buf_get(&sc->sc_tx_buf_free)) != NULL)
		myx_buf_free(sc, mb);
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

	bzero(&mc, sizeof(mc));
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

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(addr[0] << 24 | addr[1] << 16 | addr[2] << 8 | addr[3]);
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

	if (myx_cmd(sc, ISSET(ifp->if_flags, IFF_PROMISC) ?
	    MYXCMD_SET_PROMISC : MYXCMD_UNSET_PROMISC, &mc, NULL) != 0) {
		printf("%s: failed to configure promisc mode\n", DEVNAME(sc));
		return;
	}

	CLR(ifp->if_flags, IFF_ALLMULTI);

	if (myx_cmd(sc, MYXCMD_SET_ALLMULTI, &mc, NULL) != 0) {
		printf("%s: failed to enable ALLMULTI\n", DEVNAME(sc));
		return;
	}

	if (myx_cmd(sc, MYXCMD_UNSET_MCAST, &mc, NULL) != 0) {
		printf("%s: failed to leave all mcast groups \n", DEVNAME(sc));
		return;
	}

	if (sc->sc_ac.ac_multirangecnt > 0) {
		SET(ifp->if_flags, IFF_ALLMULTI);
		return;
	}

	ETHER_FIRST_MULTI(step, &sc->sc_ac, enm);
	while (enm != NULL) {
		if (myx_setlladdr(sc, MYXCMD_SET_MCASTGROUP,
		    enm->enm_addrlo) != 0) {
			printf("%s: failed to join mcast group\n", DEVNAME(sc));
			return;
		}

		ETHER_NEXT_MULTI(step, enm);
	}

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_UNSET_ALLMULTI, &mc, NULL) != 0) {
		printf("%s: failed to disable ALLMULTI\n", DEVNAME(sc));
		return;
	}
}

void
myx_down(struct myx_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	bus_dmamap_t		 map = sc->sc_sts_dma.mxm_map;
	struct myx_buf		*mb;
	struct myx_cmd		 mc;
	int			 s;

	s = splnet();
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	sc->sc_linkdown = sc->sc_sts->ms_linkdown;
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	bzero(&mc, sizeof(mc));
	(void)myx_cmd(sc, MYXCMD_SET_IFDOWN, &mc, NULL);

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	while (sc->sc_linkdown == sc->sc_sts->ms_linkdown) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);

		tsleep(sc->sc_sts, 0, "myxdown", 0);

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
	}

	CLR(ifp->if_flags, IFF_RUNNING);
	splx(s);

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
	}

	CLR(ifp->if_flags, IFF_RUNNING | IFF_OACTIVE);

	while ((mb = myx_buf_get(&sc->sc_rx_buf_list[MYX_RXBIG])) != NULL) {
		bus_dmamap_sync(sc->sc_dmat, mb->mb_map, 0,
		    mb->mb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, mb->mb_map);
		m_freem(mb->mb_m);
		myx_buf_free(sc, mb);
	}

	while ((mb = myx_buf_get(&sc->sc_rx_buf_list[MYX_RXSMALL])) != NULL) {
		bus_dmamap_sync(sc->sc_dmat, mb->mb_map, 0,
		    mb->mb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, mb->mb_map);
		m_freem(mb->mb_m);
		myx_buf_free(sc, mb);
	}

	while ((mb = myx_buf_get(&sc->sc_rx_buf_free[MYX_RXBIG])) != NULL)
		myx_buf_free(sc, mb);

	while ((mb = myx_buf_get(&sc->sc_rx_buf_free[MYX_RXSMALL])) != NULL)
		myx_buf_free(sc, mb);

	while ((mb = myx_buf_get(&sc->sc_tx_buf_list)) != NULL) {
		bus_dmamap_sync(sc->sc_dmat, mb->mb_map, 0,
		    mb->mb_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, mb->mb_map);
		m_freem(mb->mb_m);
		myx_buf_free(sc, mb);
	}

	while ((mb = myx_buf_get(&sc->sc_tx_buf_free)) != NULL)
		myx_buf_free(sc, mb);

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
myx_start(struct ifnet *ifp)
{
	struct myx_tx_desc		txd;
	struct myx_softc		*sc = ifp->if_softc;
	bus_dmamap_t			map;
	bus_dmamap_t			zmap = sc->sc_zerodma.mxm_map;;
	struct myx_buf			*mb;
	struct mbuf			*m;
	u_int32_t			offset = sc->sc_tx_ring_offset;
	u_int				idx;
	u_int				i;
	u_int8_t			flags;

	if (!ISSET(ifp->if_flags, IFF_RUNNING) ||
	    ISSET(ifp->if_flags, IFF_OACTIVE) ||
	    IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	idx = sc->sc_tx_ring_idx;

	for (;;) {
		if (sc->sc_tx_free <= sc->sc_tx_nsegs) {
			SET(ifp->if_flags, IFF_OACTIVE);
			break;
		}

		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		mb = myx_buf_get(&sc->sc_tx_buf_free);
		if (mb == NULL) {
			SET(ifp->if_flags, IFF_OACTIVE);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (myx_load_buf(sc, mb, m) != 0) {
			m_freem(m);
			myx_buf_put(&sc->sc_tx_buf_free, mb);
			ifp->if_oerrors++;
			break;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		mb->mb_m = m;
		map = mb->mb_map;

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    map->dm_mapsize, BUS_DMASYNC_POSTWRITE);

		sc->sc_tx_free -= map->dm_nsegs;

		myx_buf_put(&sc->sc_tx_buf_list, mb);

		flags = MYXTXD_FLAGS_NO_TSO;
		if (m->m_pkthdr.len < 1520)
			flags |= MYXTXD_FLAGS_SMALL;

		for (i = 1; i < map->dm_nsegs; i++) {
			bzero(&txd, sizeof(txd));
			txd.tx_addr = htobe64(map->dm_segs[i].ds_addr);
			txd.tx_length = htobe16(map->dm_segs[i].ds_len);
			txd.tx_flags = flags;

			/* complicated maths is cool */
			myx_write(sc, offset + sizeof(txd) *
			    ((idx + i) % sc->sc_tx_ring_count),
			    &txd, sizeof(txd));
		}

		/* pad runt frames */
		if (map->dm_mapsize < 60) {
			bzero(&txd, sizeof(txd));
			txd.tx_addr = htobe64(zmap->dm_segs[0].ds_addr);
			txd.tx_length = htobe16(60 - map->dm_mapsize);
			txd.tx_flags = flags;

			myx_write(sc, offset + sizeof(txd) *
			    ((idx + i) % sc->sc_tx_ring_count),
			    &txd, sizeof(txd));

			i++;
		}

		/* commit by posting the first descriptor */
		bzero(&txd, sizeof(txd));
		txd.tx_addr = htobe64(map->dm_segs[0].ds_addr);
		txd.tx_length = htobe16(map->dm_segs[0].ds_len);
		txd.tx_nsegs = i;
		txd.tx_flags = flags | MYXTXD_FLAGS_FIRST;

		myx_write(sc, offset + idx * sizeof(txd),
		    &txd, sizeof(txd));

		idx += i;
		idx %= sc->sc_tx_ring_count;
	}

	sc->sc_tx_ring_idx = idx;
}

int
myx_load_buf(struct myx_softc *sc, struct myx_buf *mb, struct mbuf *m)
{
	bus_dma_tag_t			dmat = sc->sc_dmat;
	bus_dmamap_t			dmap = mb->mb_map;

	switch (bus_dmamap_load_mbuf(dmat, dmap, m, BUS_DMA_NOWAIT)) {
	case 0:
		break;

	case EFBIG: /* mbuf chain is too fragmented */
		if (m_defrag(m, M_DONTWAIT) == 0 &&
		    bus_dmamap_load_mbuf(dmat, dmap, m, BUS_DMA_NOWAIT) == 0)
			break;
	default:
		return (1);
	}

	mb->mb_m = m;
	return (0);
}

int
myx_intr(void *arg)
{
	struct myx_softc	*sc = (struct myx_softc *)arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	volatile struct myx_status *sts = sc->sc_sts;
	bus_dmamap_t		 map = sc->sc_sts_dma.mxm_map;
	u_int32_t		 data;
	int			 refill = 0;
	u_int8_t		 valid = 0;
	int			 i;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return (0);

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	valid = sts->ms_isvalid;
	if (valid == 0x0) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);
		return (0);
	}
	sts->ms_isvalid = 0;

	if (sc->sc_intx) {
		data = htobe32(0);
		myx_write(sc, sc->sc_irqdeassertoff, &data, sizeof(data));
	}

	if (!ISSET(ifp->if_flags, IFF_UP) &&
	    sc->sc_linkdown != sts->ms_linkdown) {
		/* myx_down is waiting for us */
		wakeup_one(sc->sc_sts);
	}

	if (sts->ms_statusupdated)
		myx_link_state(sc);

	do {
		data = betoh32(sts->ms_txdonecnt);
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);

		if (data != sc->sc_tx_count)
			myx_txeof(sc, data);

		refill |= myx_rxeof(sc);

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
	} while (sts->ms_isvalid);

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	data = htobe32(3);
	if (valid & 0x1)
		myx_write(sc, sc->sc_irqclaimoff, &data, sizeof(data));
	myx_write(sc, sc->sc_irqclaimoff + sizeof(u_int32_t),
	    &data, sizeof(data));

	if (ISSET(ifp->if_flags, IFF_OACTIVE)) {
		CLR(ifp->if_flags, IFF_OACTIVE);
		myx_start(ifp);
	}

	for (i = 0; i < 2; i++) {
		if (ISSET(refill, 1 << i))
			myx_rx_fill(sc, i);
	}

	return (1);
}

void
myx_txeof(struct myx_softc *sc, u_int32_t done_count)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct myx_buf *mb;
	struct mbuf *m;
	bus_dmamap_t map;

	do {
		mb = myx_buf_get(&sc->sc_tx_buf_list);
		if (mb == NULL) {
			printf("oh noes, no mb!\n");
			break;
		}

		m = mb->mb_m;
		map = mb->mb_map;

		sc->sc_tx_free += map->dm_nsegs;
		if (map->dm_mapsize < 60)
			sc->sc_tx_free += 1;

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);
		m_freem(m);

		myx_buf_put(&sc->sc_tx_buf_free, mb);

		ifp->if_opackets++;
	} while (++sc->sc_tx_count != done_count);
}

int
myx_rxeof(struct myx_softc *sc)
{
	static const struct myx_intrq_desc zerodesc = { 0, 0 };
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct myx_buf *mb;
	struct mbuf *m;
	int ring;
	int rings = 0;
	u_int len;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_intrq_dma.mxm_map, 0,
	    sc->sc_intrq_dma.mxm_map->dm_mapsize, BUS_DMASYNC_POSTREAD);

	while ((len = betoh16(sc->sc_intrq[sc->sc_intrq_idx].iq_length)) != 0) {
		sc->sc_intrq[sc->sc_intrq_idx] = zerodesc;

		if (++sc->sc_intrq_idx >= sc->sc_intrq_count)
			sc->sc_intrq_idx = 0;

		ring = (len <= (MCLBYTES - ETHER_ALIGN)) ?
		    MYX_RXSMALL : MYX_RXBIG;

		mb = myx_buf_get(&sc->sc_rx_buf_list[ring]);
		if (mb == NULL) {
			printf("oh noes, no mb!\n");
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, mb->mb_map, 0,
		    mb->mb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, mb->mb_map);

		m = mb->mb_m;
		m->m_data += ETHER_ALIGN;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		ether_input_mbuf(ifp, m);

		myx_buf_put(&sc->sc_rx_buf_free[ring], mb);

		SET(rings, 1 << ring);
		ifp->if_ipackets++;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_intrq_dma.mxm_map, 0,
	    sc->sc_intrq_dma.mxm_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	return (rings);
}

void
myx_rx_zero(struct myx_softc *sc, int ring)
{
	struct myx_rx_desc rxd;
	u_int32_t offset = sc->sc_rx_ring_offset[ring];
	int idx;

	sc->sc_rx_ring_idx[ring] = 0;

	memset(&rxd, 0xff, sizeof(rxd));
	for (idx = 0; idx < sc->sc_rx_ring_count; idx++) {
		myx_write(sc, offset + idx * sizeof(rxd),
		    &rxd, sizeof(rxd));
	}
}

int
myx_rx_fill(struct myx_softc *sc, int ring)
{
	struct myx_rx_desc rxd;
	struct myx_buf *mb;
	u_int32_t offset = sc->sc_rx_ring_offset[ring];
	u_int idx;
	int ret = 1;

	idx = sc->sc_rx_ring_idx[ring];
	while ((mb = myx_buf_fill(sc, ring)) != NULL) {
		rxd.rx_addr = htobe64(mb->mb_map->dm_segs[0].ds_addr);

		myx_buf_put(&sc->sc_rx_buf_list[ring], mb);
		myx_write(sc, offset + idx * sizeof(rxd),
		    &rxd, sizeof(rxd));

		if (++idx >= sc->sc_rx_ring_count)
			idx = 0;

		ret = 0;
	}
	sc->sc_rx_ring_idx[ring] = idx;

	return (ret);
}

struct myx_buf *
myx_buf_fill(struct myx_softc *sc, int ring)
{
	static size_t sizes[2] = { MCLBYTES, 12 * 1024 };
	struct myx_buf *mb;
	struct mbuf *m;

	m = MCLGETI(NULL, M_DONTWAIT, &sc->sc_ac.ac_if, sizes[ring]);
	if (m == NULL)
		return (NULL);
	m->m_len = m->m_pkthdr.len = sizes[ring];

	mb = myx_buf_get(&sc->sc_rx_buf_free[ring]);
	if (mb == NULL)
		goto mfree;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, mb->mb_map, m,
	    BUS_DMA_NOWAIT) != 0)
		goto put;

	mb->mb_m = m;
	bus_dmamap_sync(sc->sc_dmat, mb->mb_map, 0, mb->mb_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	return (mb);

mfree:
	m_freem(m);
put:
	myx_buf_put(&sc->sc_rx_buf_free[ring], mb);

	return (NULL);
}

struct myx_buf *
myx_buf_alloc(struct myx_softc *sc, bus_size_t size, int nsegs,
    bus_size_t maxsegsz, bus_size_t boundary)
{
	struct myx_buf *mb;

	mb = pool_get(myx_buf_pool, PR_WAITOK);
	if (mb == NULL)
		return (NULL);

	if (bus_dmamap_create(sc->sc_dmat, size, nsegs, maxsegsz, boundary,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &mb->mb_map) != 0) {
		pool_put(myx_buf_pool, mb);
		return (NULL);
	}

	return (mb);
}

void
myx_buf_free(struct myx_softc *sc, struct myx_buf *mb)
{
	bus_dmamap_destroy(sc->sc_dmat, mb->mb_map);
	pool_put(myx_buf_pool, mb);
}

struct myx_buf *
myx_buf_get(struct myx_buf_list *mbl)
{
	struct myx_buf *mb;

	mb = SIMPLEQ_FIRST(mbl);
	if (mb == NULL)
		return (NULL);

	SIMPLEQ_REMOVE_HEAD(mbl, mb_entry);

	return (mb);
}

void
myx_buf_put(struct myx_buf_list *mbl, struct myx_buf *mb)
{
	SIMPLEQ_INSERT_TAIL(mbl, mb, mb_entry);
}
