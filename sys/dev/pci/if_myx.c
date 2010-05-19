/*	$OpenBSD: if_myx.c,v 1.12 2010/05/19 15:27:35 oga Exp $	*/

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
#include <sys/sensors.h>

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

#define MYX_DEBUG
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

#define DEVNAME(_s)	((_s)->_s##_dev.dv_xname)

struct myx_dmamem {
	bus_dmamap_t		 mxm_map;
	bus_dma_segment_t	 mxm_seg;
	int			 mxm_nsegs;
	size_t			 mxm_size;
	caddr_t			 mxm_kva;
	const char		*mxm_name;
};

struct myx_buf {
	bus_dmamap_t		 mb_dmamap;
	struct mbuf		*mb_m;
};

struct myx_softc {
	struct device		 sc_dev;
	struct arpcom		 sc_ac;

	pci_chipset_tag_t	 sc_pc;
	pcitag_t		 sc_tag;
	u_int			 sc_function;

	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_size_t		 sc_mems;

	struct myx_dmamem	 sc_cmddma;
	struct myx_dmamem	 sc_paddma;

	struct myx_dmamem	 sc_stsdma;
	struct myx_status	*sc_sts;

	struct myx_dmamem	 sc_rxdma;
	struct myx_rxdesc	*sc_rxdesc;
	struct myx_rxbufdesc	*sc_rxbufdesc[2];
	struct myx_buf		*sc_rxbuf[2];
#define  MYX_RXSMALL		 0
#define  MYX_RXBIG		 1
	int			 sc_rxactive;
	int			 sc_rxidx;

	void			*sc_irqh;
	u_int32_t		 sc_irqcoaloff;
	u_int32_t		 sc_irqclaimoff;
	u_int32_t		 sc_irqdeassertoff;

	u_int8_t		 sc_lladdr[ETHER_ADDR_LEN];
	struct ifmedia		 sc_media;

	u_int32_t		 sc_rxringsize;
	u_int32_t		 sc_rxsmallringoff;
	u_int32_t		 sc_rxbigringoff;
	int			 sc_rxndesc;
	size_t			 sc_rxdescsize;
	size_t			 sc_rxbufsize;
	size_t			 sc_rxbufdescsize;
	u_int32_t		 sc_txringsize;
	u_int32_t		 sc_txringoff;
	int			 sc_txndesc;

	u_int			 sc_phy;	/* PHY type (CX4/SR/LR) */
	u_int			 sc_hwflags;
#define  MYXFLAG_FLOW_CONTROL	 (1<<0)		/* Rx/Tx pause is enabled */
#define  MYXFLAG_PROMISC	 (1<<1)		/* promisc mode is enabled */
#define  MYXFLAG_ALLMULTI	 (1<<2)		/* allmulti is set */
	u_int8_t		 sc_active;

	struct timeout		 sc_tick;
};

int	 myx_match(struct device *, void *, void *);
void	 myx_attach(struct device *, struct device *, void *);
int	 myx_query(struct myx_softc *sc);
u_int	 myx_ether_aton(char *, u_int8_t *, u_int);
int	 myx_loadfirmware(struct myx_softc *, u_int8_t *, size_t,
	    u_int32_t, int);
void	 myx_attachhook(void *);
void	 myx_read(struct myx_softc *, bus_size_t, u_int8_t *, bus_size_t);
void	 myx_rawread(struct myx_softc *, bus_size_t, u_int8_t *, bus_size_t);
void	 myx_write(struct myx_softc *, bus_size_t, u_int8_t *, bus_size_t);
void	 myx_rawwrite(struct myx_softc *, bus_size_t, u_int8_t *, bus_size_t);
int	 myx_cmd(struct myx_softc *, u_int32_t, struct myx_cmd *, u_int32_t *);
int	 myx_boot(struct myx_softc *, u_int32_t, struct myx_bootcmd *);
int	 myx_rdma(struct myx_softc *, u_int);
int	 myx_reset(struct myx_softc *);
int	 myx_dmamem_alloc(struct myx_softc *, struct myx_dmamem *,
	    bus_size_t, u_int align, const char *);
void	 myx_dmamem_free(struct myx_softc *, struct myx_dmamem *);
int	 myx_media_change(struct ifnet *);
void	 myx_media_status(struct ifnet *, struct ifmediareq *);
void	 myx_link_state(struct myx_softc *);
void	 myx_watchdog(struct ifnet *);
void	 myx_tick(void *);
int	 myx_ioctl(struct ifnet *, u_long, caddr_t);
void	 myx_iff(struct myx_softc *);
void	 myx_init(struct ifnet *);
void	 myx_start(struct ifnet *);
void	 myx_stop(struct ifnet *);
int	 myx_setlladdr(struct myx_softc *, u_int8_t *);
int	 myx_intr(void *);
int	 myx_init_rings(struct myx_softc *);
void	 myx_free_rings(struct myx_softc *);
struct mbuf *myx_getbuf(struct myx_softc *, bus_dmamap_t, int);

struct cfdriver myx_cd = {
	NULL, "myx", DV_IFNET
};
struct cfattach myx_ca = {
	sizeof(struct myx_softc), myx_match, myx_attach
};

const struct pci_matchid myx_devices[] = {
	{ PCI_VENDOR_MYRICOM, PCI_PRODUCT_MYRICOM_Z8E }
};

int
myx_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux,
	    myx_devices, sizeof(myx_devices) / sizeof(myx_devices[0])));
}

void
myx_attach(struct device *parent, struct device *self, void *aux)
{
	struct myx_softc	*sc = (struct myx_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_intr_handle_t	 ih;
	pcireg_t		 memtype;
	const char		*intrstr;
	struct ifnet		*ifp;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_function = pa->pa_function;

	/* Map the PCI memory space */
	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, MYXBAR0);
	if (pci_mapreg_map(pa, MYXBAR0, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf(": unable to map register memory\n");
		return;
	}

	/* Get the board information and initialize the h/w */
	if (myx_query(sc) != 0)
		goto unmap;

	/*
	 * Allocate command DMA memory
	 */
	if (myx_dmamem_alloc(sc, &sc->sc_cmddma, MYXALIGN_CMD,
	    MYXALIGN_CMD, "cmd") != 0) {
		printf(": failed to allocate command DMA memory\n");
		goto unmap;
	}

	if (myx_dmamem_alloc(sc, &sc->sc_paddma,
	    MYXALIGN_CMD, MYXALIGN_CMD, "pad") != 0) {
		printf(": failed to allocate pad DMA memory\n");
		goto err2;
	}

	if (myx_dmamem_alloc(sc, &sc->sc_stsdma,
	    sizeof(struct myx_status), MYXALIGN_DATA /* XXX */, "status") != 0) {
		printf(": failed to allocate status DMA memory\n");
		goto err1;
	}
	sc->sc_sts = (struct myx_status *)sc->sc_stsdma.mxm_kva;

	/*
	 * Map and establish the interrupt
	 */
	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		goto err;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_irqh = pci_intr_establish(pa->pa_pc, ih, IPL_NET,
	    myx_intr, sc, DEVNAME(sc));
	if (sc->sc_irqh == NULL) {
		printf(": unable to establish interrupt %s\n", intrstr);
		goto err;
	}
	printf(": %s, address %s\n", intrstr,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = myx_ioctl;
	ifp->if_start = myx_start;
	ifp->if_watchdog = myx_watchdog;
	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, MYX_NTXDESC_MIN - 1);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
		    IFCAP_CSUM_UDPv4;
#endif
	ifp->if_baudrate = IF_Gbps(10);

	ifmedia_init(&sc->sc_media, 0,
	    myx_media_change, myx_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER|sc->sc_phy, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER|sc->sc_phy);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->sc_tick, myx_tick, sc);
	timeout_add_sec(&sc->sc_tick, 1);

	mountroothook_establish(myx_attachhook, sc);

	return;

 err:
	myx_dmamem_free(sc, &sc->sc_stsdma);
 err1:
	myx_dmamem_free(sc, &sc->sc_paddma);
 err2:
	myx_dmamem_free(sc, &sc->sc_cmddma);
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
myx_query(struct myx_softc *sc)
{
	u_int8_t	eeprom[MYX_EEPROM_SIZE];
	u_int		i, maxlen;

	myx_read(sc, MYX_EEPROM, eeprom, MYX_EEPROM_SIZE);

	for (i = 0; i < MYX_EEPROM_SIZE; i++) {
		maxlen = MYX_EEPROM_SIZE - i;
		if (eeprom[i] == '\0')
			break;
		if (maxlen > 4 && bcmp("MAC=", &eeprom[i], 4) == 0) {
			i += 4;
			i += myx_ether_aton(&eeprom[i],
			    sc->sc_ac.ac_enaddr, maxlen);
		}
		for (; i < MYX_EEPROM_SIZE; i++)
			if (eeprom[i] == '\0')
				break;
	}

	return (0);
}

int
myx_loadfirmware(struct myx_softc *sc, u_int8_t *fw, size_t fwlen,
    u_int32_t fwhdroff, int reload)
{
	struct myx_firmware_hdr	*fwhdr;
	u_int			 i, len, ret = 0;

	fwhdr = (struct myx_firmware_hdr *)(fw + fwhdroff);
	DPRINTF(MYXDBG_INIT, "%s(%s): "
	    "fw hdr off %d, length %d, type 0x%x, version %s\n",
	    DEVNAME(sc), __func__,
	    fwhdroff, betoh32(fwhdr->fw_hdrlength),
	    betoh32(fwhdr->fw_type),
	    fwhdr->fw_version);

	if (betoh32(fwhdr->fw_type) != MYXFW_TYPE_ETH ||
	    bcmp(MYXFW_VER, fwhdr->fw_version, strlen(MYXFW_VER)) != 0) {
		if (reload)
			printf("%s: invalid firmware type 0x%x version %s\n",
			    DEVNAME(sc), betoh32(fwhdr->fw_type),
			    fwhdr->fw_version);
		ret = 1;
		goto done;
	}

	if (!reload)
		goto done;

	/* Write the firmware to the card's SRAM */
	for (i = 0; i < fwlen; i += 256) {
		len = min(256, fwlen - i);
		myx_rawwrite(sc, i + MYX_FW, fw + i, min(256, fwlen - i));
	}

 done:
	free(fw, M_DEVBUF);
	return (ret);
}

void
myx_attachhook(void *arg)
{
	struct myx_softc	*sc = (struct myx_softc *)arg;
	size_t			 fwlen;
	u_int8_t		*fw = NULL;
	u_int32_t		 fwhdroff;
	struct myx_bootcmd	 bc;

	/*
	 * First try the firmware found in the SRAM
	 */
	myx_read(sc, MYX_HEADER_POS, (u_int8_t *)&fwhdroff, sizeof(fwhdroff));
	fwhdroff = betoh32(fwhdroff);
	fwlen = sizeof(struct myx_firmware_hdr);
	if ((fwhdroff + fwlen) > MYX_SRAM_SIZE)
		goto load;

	fw = malloc(fwlen, M_DEVBUF, M_WAIT);
	myx_rawread(sc, MYX_HEADER_POS, fw, fwlen);

	if (myx_loadfirmware(sc, fw, fwlen, fwhdroff, 0) == 0)
		goto boot;

 load:
	/*
	 * Now try the firmware stored on disk
	 */
	if (loadfirmware(MYXFW_ALIGNED /* XXX */, &fw, &fwlen) != 0) {
		printf("%s: could not load firmware\n", DEVNAME(sc));
		return;
	}
	if (fwlen > MYX_SRAM_SIZE || fwlen < MYXFW_MIN_LEN) {
		printf("%s: invalid firmware image size\n", DEVNAME(sc));
		goto err;
	}

	bcopy(fw + MYX_HEADER_POS, &fwhdroff, sizeof(fwhdroff));
	fwhdroff = betoh32(fwhdroff);
	if ((fwhdroff + sizeof(struct myx_firmware_hdr)) > fwlen) {
		printf("%s: invalid firmware image\n", DEVNAME(sc));
		goto err;
	}

	if (myx_loadfirmware(sc, fw, fwlen, fwhdroff, 1) != 0) {
		fw = NULL;
		goto err;
	}
	fw = NULL;

 boot:
	bzero(&bc, sizeof(bc));
	if (myx_boot(sc, fwlen, &bc) != 0) {
		printf("%s: failed to bootstrap the device\n", DEVNAME(sc));
		goto err;
	}
	if (myx_reset(sc) != 0)
		goto err;

	sc->sc_active = 1;
	return;

 err:
	if (fw != NULL)
		free(fw, M_DEVBUF);
}

void
myx_read(struct myx_softc *sc, bus_size_t off, u_int8_t *ptr, bus_size_t len)
{
	bus_space_barrier(sc->sc_memt, sc->sc_memh, off, len,
	    BUS_SPACE_BARRIER_READ);
	bus_space_read_region_4(sc->sc_memt, sc->sc_memh, off, ptr, len / 4);
}

void
myx_rawread(struct myx_softc *sc, bus_size_t off, u_int8_t *ptr,
    bus_size_t len)
{
	bus_space_barrier(sc->sc_memt, sc->sc_memh, off, len,
	    BUS_SPACE_BARRIER_READ);
	bus_space_read_raw_region_4(sc->sc_memt, sc->sc_memh, off, ptr, len);
}

void
myx_write(struct myx_softc *sc, bus_size_t off, u_int8_t *ptr, bus_size_t len)
{
	bus_space_write_region_4(sc->sc_memt, sc->sc_memh, off, ptr, len / 4);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, off, len,
	    BUS_SPACE_BARRIER_WRITE);
}

void
myx_rawwrite(struct myx_softc *sc, bus_size_t off, u_int8_t *ptr,
    bus_size_t len)
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

	for (i = 0; i < 20; i++) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		result = betoh32(mr->mr_result);
		data = betoh32(mr->mr_data);

		if (result != 0xffffffff)
			break;
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
myx_boot(struct myx_softc *sc, u_int32_t length, struct myx_bootcmd *bc)
{
	bus_dmamap_t		 map = sc->sc_cmddma.mxm_map;
	u_int32_t		*status;
	u_int			 i;

	bc->bc_addr_high = htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
	bc->bc_addr_low = htobe32(MYX_ADDRLOW(map->dm_segs[0].ds_addr));
	bc->bc_result = 0xffffffff;
	bc->bc_offset = htobe32(MYX_FW_BOOT);
	bc->bc_length = htobe32(length);
	bc->bc_copyto = htobe32(8);
	bc->bc_jumpto = htobe32(0);

	status = (u_int32_t *)sc->sc_cmddma.mxm_kva;
	*status = 0;

	/* Send command */
	myx_write(sc, MYX_BOOT, (u_int8_t *)bc, sizeof(struct myx_bootcmd));

	for (i = 0; i < 200; i++) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		if (*status == 0xffffffff)
			break;
		delay(1000);
	}

	DPRINTF(MYXDBG_CMD, "%s(%s): boot completed, i %d, result 0x%x\n",
	    DEVNAME(sc), __func__, i, betoh32(*status));

	if (*status != 0xffffffff)
		return (-1);

	return (0);
}

int
myx_rdma(struct myx_softc *sc, u_int do_enable)
{
	struct myx_rdmacmd	 rc;
	bus_dmamap_t		 map = sc->sc_cmddma.mxm_map;
	bus_dmamap_t		 pad = sc->sc_paddma.mxm_map;
	u_int32_t		*status;
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

	/* Send command */
	myx_write(sc, MYX_RDMA, (u_int8_t *)&rc, sizeof(struct myx_rdmacmd));

	for (i = 0; i < 20; i++) {
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		if (*status == 0xffffffff)
			break;
		delay(1000);
	}

	DPRINTF(MYXDBG_CMD, "%s(%s): dummy RDMA %s, i %d, result 0x%x\n",
	    DEVNAME(sc), __func__,
	    do_enable ? "enabled" : "disabled", i, betoh32(*status));

	if (*status != 0xffffffff)
		return (-1);

	return (0);
}

int
myx_reset(struct myx_softc *sc)
{
	struct myx_cmd		 mc;
	u_int32_t		 data;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;

	bzero(&mc, sizeof(mc));
	if (myx_cmd(sc, MYXCMD_RESET, &mc, NULL) != 0) {
		printf("%s: failed to reset the device\n", DEVNAME(sc));
		return (-1);
	}

	if (myx_rdma(sc, MYXRDMA_ON) != 0) {
		printf("%s: failed to enable dummy RDMA\n", DEVNAME(sc));
		return (-1);
	}

	if (myx_cmd(sc, MYXCMD_GET_INTRCOALDELAYOFF, &mc,
	    &sc->sc_irqcoaloff) != 0) {
		printf("%s: failed to get IRQ coal offset\n", DEVNAME(sc));
		return (-1);
	}
	data = htobe32(MYX_IRQCOALDELAY);
	myx_write(sc, sc->sc_irqcoaloff, (u_int8_t *)&data, sizeof(data));

	if (myx_cmd(sc, MYXCMD_GET_INTRACKOFF, &mc,
	    &sc->sc_irqclaimoff) != 0) {
		printf("%s: failed to get IRQ ack offset\n", DEVNAME(sc));
		return (-1);
	}

	if (myx_cmd(sc, MYXCMD_GET_INTRDEASSERTOFF, &mc,
	    &sc->sc_irqdeassertoff) != 0) {
		printf("%s: failed to get IRQ deassert offset\n", DEVNAME(sc));
		return (-1);
	}

	if (myx_cmd(sc, MYXCMD_UNSET_PROMISC, &mc, NULL) != 0) {
		printf("%s: failed to disable promisc mode\n", DEVNAME(sc));
		return (-1);
	}

	if (myx_cmd(sc, MYXCMD_FC_DEFAULT, &mc, NULL) != 0) {
		printf("%s: failed to configure flow control\n", DEVNAME(sc));
		return (-1);
	}

	if (myx_setlladdr(sc, LLADDR(ifp->if_sadl)) != 0)
		return (-1);

	return (0);
}


int
myx_media_change(struct ifnet *ifp)
{
	return (EINVAL);
}

void
myx_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct myx_softc	*sc = (struct myx_softc *)ifp->if_softc;

	imr->ifm_active = IFM_ETHER|sc->sc_phy;
	imr->ifm_status = IFM_AVALID;
	myx_link_state(sc);
	if (!LINK_STATE_IS_UP(ifp->if_link_state))
		return;
	imr->ifm_active |= IFM_FDX;
	imr->ifm_status |= IFM_ACTIVE;

	/* Flow control */
	if (sc->sc_hwflags & MYXFLAG_FLOW_CONTROL)
		imr->ifm_active |= IFM_FLOW|IFM_ETH_RXPAUSE|IFM_ETH_TXPAUSE;
}

void
myx_link_state(struct myx_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	int			 link_state = LINK_STATE_DOWN;

	if (sc->sc_sts == NULL)
		return;
	if (sc->sc_sts->ms_linkstate == MYXSTS_LINKUP)
		link_state = LINK_STATE_FULL_DUPLEX;
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
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

	if (!sc->sc_active)
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
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				myx_iff(sc);
			else
				myx_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				myx_stop(ifp);
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
myx_iff(struct myx_softc *sc)
{
	/* XXX set multicast filters etc. */
	return;
}

void
myx_init(struct ifnet *ifp)
{
	struct myx_softc	*sc = (struct myx_softc *)ifp->if_softc;
	struct myx_cmd		 mc;

	if (myx_reset(sc) != 0)
		return;

	if (myx_init_rings(sc) != 0)
		return;

	if (myx_cmd(sc, MYXCMD_SET_IFUP, &mc, NULL) != 0) {
		printf("%s: failed to start the device\n", DEVNAME(sc));
		myx_free_rings(sc);
		return;
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
}

void
myx_start(struct ifnet *ifp)
{
}

void
myx_stop(struct ifnet *ifp)
{
	struct myx_softc	*sc = (struct myx_softc *)ifp->if_softc;
	struct myx_cmd		 mc;

	bzero(&mc, sizeof(mc));
	(void)myx_cmd(sc, MYXCMD_SET_IFDOWN, &mc, NULL);
	myx_free_rings(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

int
myx_setlladdr(struct myx_softc *sc, u_int8_t *addr)
{
	struct myx_cmd		 mc;

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = addr[3] | addr[2] << 8 | addr[1] << 16 | addr[0] << 24;
	mc.mc_data1 = addr[5] | addr[4] << 8;
	if (myx_cmd(sc, MYXCMD_SET_LLADDR, &mc, NULL) != 0) {
		printf("%s: failed to set the lladdr\n", DEVNAME(sc));
		return (-1);
	}
	return (0);
}

int
myx_intr(void *arg)
{
	struct myx_softc	*sc = (struct myx_softc *)arg;
	u_int32_t		 data, valid;
	struct myx_status	*sts = sc->sc_sts;
	bus_dmamap_t		 map = sc->sc_stsdma.mxm_map;

	if (!sc->sc_active)
		return (0);

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	/*
	 * XXX The 'valid' flags should be set by the NIC, but it doesn't
	 * XXX work yet.
	 */
	valid = sts->ms_isvalid;
	if (!valid)
		return (0);

	data = 0;
	myx_write(sc, sc->sc_irqdeassertoff, (u_int8_t *)&data, sizeof(data));

	DPRINTF(MYXDBG_INTR, "%s(%s): interrupt, valid 0x%x\n",
	    DEVNAME(sc), __func__, valid);

#ifdef MYX_DEBUG
#define DPRINT_STATUS(_n)						\
	DPRINTF(MYXDBG_INTR, "%s(%s): %s: %u, 0x%x\n", DEVNAME(sc), __func__,\
	    #_n, sts->_n, sts->_n)

	DPRINT_STATUS(ms_reserved);
	DPRINT_STATUS(ms_dropped_pause);
	DPRINT_STATUS(ms_dropped_unicast);
	DPRINT_STATUS(ms_dropped_crc32err);
	DPRINT_STATUS(ms_dropped_phyerr);
	DPRINT_STATUS(ms_dropped_mcast);
	DPRINT_STATUS(ms_txdonecnt);
	DPRINT_STATUS(ms_linkstate);
	DPRINT_STATUS(ms_dropped_linkoverflow);
	DPRINT_STATUS(ms_dropped_linkerror);
	DPRINT_STATUS(ms_dropped_runt);
	DPRINT_STATUS(ms_dropped_overrun);
	DPRINT_STATUS(ms_dropped_smallbufunderrun);
	DPRINT_STATUS(ms_dropped_bigbufunderrun);
	DPRINT_STATUS(ms_rdmatags_available);
	DPRINT_STATUS(ms_txstopped);
	DPRINT_STATUS(ms_linkdowncnt);
	DPRINT_STATUS(ms_statusupdated);
	DPRINT_STATUS(ms_isvalid);
#endif

	data = htobe32(3);
	if (sts->ms_isvalid)
		myx_write(sc, sc->sc_irqclaimoff, (u_int8_t *)&data,
		    sizeof(data));
	myx_write(sc, sc->sc_irqclaimoff + sizeof(u_int32_t),
	    (u_int8_t *)&data, sizeof(data));

	return (1);
}

int
myx_init_rings(struct myx_softc *sc)
{
	struct myx_cmd		 mc;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	bus_dmamap_t		 map;
	int			 i;
	struct myx_buf		*mb;
	struct myx_rxbufdesc	*rxb;
	u_int32_t		 data;

	bzero(&mc, sizeof(mc));
	if (!(myx_cmd(sc, MYXCMD_GET_RXRINGSZ, &mc,
	    &sc->sc_rxringsize) == 0 && sc->sc_rxringsize &&
	    myx_cmd(sc, MYXCMD_GET_RXSMALLRINGOFF, &mc,
	    &sc->sc_rxsmallringoff) == 0 && sc->sc_rxsmallringoff &&
	    myx_cmd(sc, MYXCMD_GET_RXBIGRINGOFF, &mc,
	    &sc->sc_rxbigringoff) == 0 && sc->sc_rxbigringoff &&
	    myx_cmd(sc, MYXCMD_GET_TXRINGSZ, &mc,
	    &sc->sc_txringsize) == 0 && sc->sc_txringsize &&
	    myx_cmd(sc, MYXCMD_GET_TXRINGOFF, &mc,
	    &sc->sc_txringoff) == 0 && sc->sc_txringoff)) {
		printf("%s: failed to get ring sizes and offsets\n",
		    DEVNAME(sc));
		return (-1);
	}
	sc->sc_rxndesc = sc->sc_rxringsize / sizeof(struct myx_rxbufdesc);
	sc->sc_txndesc = sc->sc_txringsize / sizeof(struct myx_txdesc);
	sc->sc_rxdescsize = sc->sc_rxndesc * 2 * sizeof(struct myx_rxdesc);
	sc->sc_rxbufsize = sc->sc_rxndesc * sizeof(struct myx_buf);
	sc->sc_rxbufdescsize = sc->sc_rxndesc * sizeof(struct myx_rxbufdesc);
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->sc_txndesc - 1);
	IFQ_SET_READY(&ifp->if_snd);

	DPRINTF(MYXDBG_INIT, "%s(%s): Rx ring ndesc %u size %u bufsize %u, "
	    "Tx ring ndesc %u size %u offset 0x%x\n", DEVNAME(sc), __func__,
	    sc->sc_rxndesc, sc->sc_rxdescsize, sc->sc_rxringsize,
	    sc->sc_txndesc, sc->sc_txringsize, sc->sc_txringoff);

	/*
	 * Setup Rx DMA descriptors
	 */
	if (myx_dmamem_alloc(sc, &sc->sc_rxdma,
	    sc->sc_rxdescsize, MYXALIGN_DATA, "rxring") != 0) {
		printf(": failed to allocate Rx DMA memory\n");
		return (-1);
	}
	sc->sc_rxdesc = (struct myx_rxdesc *)sc->sc_rxdma.mxm_kva;

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = htobe32(sc->sc_rxdescsize);
	if (myx_cmd(sc, MYXCMD_SET_INTRQSZ, &mc, NULL) != 0) {
		printf("%s: failed to set Rx DMA size\n", DEVNAME(sc));
		goto err;
	}

	map = sc->sc_rxdma.mxm_map;
	mc.mc_data0 = MYX_ADDRLOW(map->dm_segs[0].ds_addr);
	mc.mc_data1 = MYX_ADDRHIGH(map->dm_segs[0].ds_addr);
	if (myx_cmd(sc, MYXCMD_SET_INTRQDMA, &mc, NULL) != 0) {
		printf("%s: failed to set Rx DMA address\n", DEVNAME(sc));
		goto err;
	}

#ifdef notyet
	/*
	 * XXX It fails to set the MTU and it always returns
	 * XXX MYXCMD_ERR_RANGE.
	 */
	bzero(&mc, sizeof(mc));
	mc.mc_data0 = ifp->if_mtu + ETHER_HDR_LEN + 4;
	if (myx_cmd(sc, MYXCMD_SET_MTU, &mc, NULL) != 0) {
		printf("%s: failed to set MTU size %d\n",
		    DEVNAME(sc), ifp->if_mtu + ETHER_HDR_LEN + 4);
		goto err;
	}
#endif

	/*
	 * Setup Rx buffer descriptors
	 */
	sc->sc_rxbuf[MYX_RXSMALL] = (struct myx_buf *)
	    malloc(sc->sc_rxbufsize, M_DEVBUF, M_WAITOK);
	sc->sc_rxbufdesc[MYX_RXSMALL] = (struct myx_rxbufdesc *)
	    malloc(sc->sc_rxbufdescsize, M_DEVBUF, M_WAITOK);
	sc->sc_rxbuf[MYX_RXBIG] = (struct myx_buf *)
	    malloc(sc->sc_rxbufsize, M_DEVBUF, M_WAITOK);
	sc->sc_rxbufdesc[MYX_RXBIG] = (struct myx_rxbufdesc *)
	    malloc(sc->sc_rxbufdescsize, M_DEVBUF, M_WAITOK);
	if (sc->sc_rxbuf[MYX_RXSMALL] == NULL ||
	    sc->sc_rxbufdesc[MYX_RXSMALL] == NULL ||
	    sc->sc_rxbuf[MYX_RXBIG] == NULL ||
	    sc->sc_rxbufdesc[MYX_RXBIG] == NULL) {
		printf("%s: failed to allocate rx buffers\n", DEVNAME(sc));
		goto err;
	}

	for (i = 0; i < sc->sc_rxndesc; i++) {
		/*
		 * Small Rx buffers and descriptors
		 */
		mb = sc->sc_rxbuf[MYX_RXSMALL] + i;
		rxb = sc->sc_rxbufdesc[MYX_RXSMALL] + i;

		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_WAITOK, &mb->mb_dmamap) != 0) {
			printf("%s: unable to create dmamap for small rx %d\n",
			    DEVNAME(sc), i);
			goto err;
		}

		map = mb->mb_dmamap;
		mb->mb_m = myx_getbuf(sc, map, 1);
		if (mb->mb_m == NULL) {
			bus_dmamap_destroy(sc->sc_dmat, map);
			goto err;
		}

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    mb->mb_m->m_pkthdr.len, BUS_DMASYNC_PREREAD);

		rxb->rb_addr_high =
		    htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
		rxb->rb_addr_low =
		    htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));

		data = sc->sc_rxsmallringoff + i * sizeof(*rxb);
		myx_write(sc, data, (u_int8_t *)rxb, sizeof(*rxb));

		/*
		 * Big Rx buffers and descriptors
		 */
		mb = sc->sc_rxbuf[MYX_RXBIG] + i;
		rxb = sc->sc_rxbufdesc[MYX_RXBIG] + i;

		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_WAITOK, &mb->mb_dmamap) != 0) {
			printf("%s: unable to create dmamap for big rx %d\n",
			    DEVNAME(sc), i);
			goto err;
		}

		map = mb->mb_dmamap;
		mb->mb_m = myx_getbuf(sc, map, 1);
		if (mb->mb_m == NULL) {
			bus_dmamap_destroy(sc->sc_dmat, map);
			goto err;
		}

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    mb->mb_m->m_pkthdr.len, BUS_DMASYNC_PREREAD);

		rxb->rb_addr_high =
		    htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));
		rxb->rb_addr_low =
		    htobe32(MYX_ADDRHIGH(map->dm_segs[0].ds_addr));

		data = sc->sc_rxbigringoff + i * sizeof(*rxb);
		myx_write(sc, data, (u_int8_t *)rxb, sizeof(*rxb));
	}

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = MYX_MAX_MTU_SMALL;
	if (myx_cmd(sc, MYXCMD_SET_SMALLBUFSZ, &mc, NULL) != 0) {
		printf("%s: failed to set small buf size\n", DEVNAME(sc));
		goto err;
	}

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = MCLBYTES;
	if (myx_cmd(sc, MYXCMD_SET_BIGBUFSZ, &mc, NULL) != 0) {
		printf("%s: failed to set big buf size\n", DEVNAME(sc));
		goto err;
	}

	/*
	 * Setup status DMA
	 */
	map = sc->sc_stsdma.mxm_map;

	bzero(&mc, sizeof(mc));
	mc.mc_data0 = MYX_ADDRLOW(map->dm_segs[0].ds_addr);
	mc.mc_data1 = MYX_ADDRHIGH(map->dm_segs[0].ds_addr);
	mc.mc_data2 = sizeof(struct myx_status);
	if (myx_cmd(sc, MYXCMD_SET_STATSDMA, &mc, NULL) != 0) {
		printf("%s: failed to set status DMA offset\n", DEVNAME(sc));
		goto err;
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0,
	    map->dm_mapsize, BUS_DMASYNC_PREWRITE);

	return (0);
 err:
	myx_free_rings(sc);
	return (-1);
}

void
myx_free_rings(struct myx_softc *sc)
{
	if (sc->sc_rxbuf[MYX_RXSMALL] != NULL) {
		free(sc->sc_rxbuf[MYX_RXSMALL], M_DEVBUF);
		sc->sc_rxbuf[MYX_RXSMALL] = NULL;
	}
	if (sc->sc_rxbufdesc[MYX_RXSMALL] != NULL) {
		free(sc->sc_rxbufdesc[MYX_RXSMALL], M_DEVBUF);
		sc->sc_rxbufdesc[MYX_RXSMALL] = NULL;
	}
	if (sc->sc_rxbuf[MYX_RXBIG] != NULL) {
		free(sc->sc_rxbuf[MYX_RXBIG], M_DEVBUF);
		sc->sc_rxbuf[MYX_RXBIG] = NULL;
	}
	if (sc->sc_rxbufdesc[MYX_RXBIG] != NULL) {
		free(sc->sc_rxbufdesc[MYX_RXBIG], M_DEVBUF);
		sc->sc_rxbufdesc[MYX_RXBIG] = NULL;
	}
	if (sc->sc_rxdesc != NULL) {
		myx_dmamem_free(sc, &sc->sc_rxdma);
		sc->sc_rxdesc = NULL;
	}
	if (sc->sc_sts != NULL) {
		myx_dmamem_free(sc, &sc->sc_stsdma);
		sc->sc_sts = NULL;
	}
	return;
}

struct mbuf *
myx_getbuf(struct myx_softc *sc, bus_dmamap_t map, int wait)
{
	struct mbuf		*m = NULL;

	MGETHDR(m, wait ? M_WAIT : M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto merr;

	MCLGET(m, wait ? M_WAIT : M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0)
		goto merr;
	m->m_len = m->m_pkthdr.len = MCLBYTES;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
	    wait ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT) != 0) {
		printf("%s: could not load mbuf dma map\n", DEVNAME(sc));
		goto err;
	}

	return (m);
 merr:
	printf("%s: unable to allocate mbuf\n", DEVNAME(sc));
 err:
	if (m != NULL)
		m_freem(m);
	return (NULL);
}
