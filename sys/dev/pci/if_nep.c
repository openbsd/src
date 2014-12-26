/*	$OpenBSD: if_nep.c,v 1.3 2014/12/26 23:06:22 kettenis Exp $	*/
/*
 * Copyright (c) 2014 Mark Kettenis
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
extern void myetheraddr(u_char *);
#endif

/*
 * The virtualization features make this a really complex device.  For
 * now we try to keep things simple and use one logical device per
 * port, using port numbers as logical device numbers.
 */

#define FZC_PIO		0x080000
#define FZC_MAC		0x180000
#define DMC		0x600000
#define FZC_DMC		0x680000
#define FZC_TXC		0x780000
#define PIO_LDSV	0x800000
#define PIO_IMASK0	0xa00000
#define PIO_IMASK1	0xb00000

#define RST_CTL			(FZC_PIO + 0x00038)
#define SYS_ERR_MASK		(FZC_PIO + 0x00090)
#define SYS_ERR_STAT		(FZC_PIO + 0x00098)

#define LDN_RXDMA(chan)		(0 + (chan))
#define LDN_TXDMA(chan)		(32 + (chan))
#define LDN_MIF			63
#define LDN_MAC(port)		(64 + (port))
#define LDN_SYSERR		68

#define LDSV0(ldg)		(PIO_LDSV + 0x00000 + (ldg) * 0x02000)
#define LDSV1(ldg)		(PIO_LDSV + 0x00008 + (ldg) * 0x02000)
#define LDSV2(ldg)		(PIO_LDSV + 0x00010 + (ldg) * 0x02000)
#define LDGIMGN(ldg)		(PIO_LDSV + 0x00018 + (ldg) * 0x20000)
#define  LDGIMGN_ARM		(1ULL << 31)
#define  LDGIMGN_TIMER		(63ULL << 0)

#define LD_IM0(idx)		(PIO_IMASK0 + 0x00000 + (idx) * 0x02000)
#define  LD_IM0_LDF_MASK	(3ULL << 0)
#define LD_IM1(idx)		(PIO_IMASK1 + 0x00000 + (idx - 64) * 0x02000)
#define  LD_IM1_LDF_MASK	(3ULL << 0)

#define SID(ldg)		(FZC_PIO + 0x10200 + (ldg) * 0x00008)
#define LDG_NUM(ldn)		(FZC_PIO + 0x20000 + (ldn) * 0x00008)

#define TXC_DMA_MAX(chan)	(FZC_TXC + 0x00000 + (chan) * 0x01000)
#define TXC_CONTROL		(FZC_TXC + 0x20000)
#define  TXC_CONTROL_TXC_ENABLED	(1ULL << 4)
#define TXC_PORT_DMA(port)	(FZC_TXC + 0x20028 + (port) * 0x00100)
#define TXC_PKT_STUFFED(port)	(FZC_TXC + 0x20030 + (port) * 0x00100)
#define TXC_PKT_XMIT(port)	(FZC_TXC + 0x20038 + (port) * 0x00100)
#define TXC_INT_STAT_DBG	(FZC_TXC + 0x20420)
#define TXC_INT_STAT		(FZC_TXC + 0x20428)
#define TXC_INT_MASK		(FZC_TXC + 0x20430)
#define  TXC_INT_MASK_PORT_INT_MASK(port) (0x3fULL << ((port) * 8))

#define XTXMAC_SW_RST(port)	(FZC_MAC + 0x00000 + (port) * 0x06000)
#define  XTXMAC_SW_RST_REG_RST		(1ULL << 1)
#define  XTXMAC_SW_RST_SOFT_RST		(1ULL << 0)
#define XTXMAC_STATUS(port)	(FZC_MAC + 0x00020 + (port) * 0x06000)
#define XTXMAC_STAT_MSK(port)	(FZC_MAC + 0x00040 + (port) * 0x06000)
#define XMAC_CONFIG(port)	(FZC_MAC + 0x00060 + (port) * 0x06000)
#define  XMAC_CONFIG_SEL_CLK_25MHZ	(1ULL << 31)
#define  XMAC_CONFIG_1G_PCS_BYPASS	(1ULL << 30)
#define  XMAC_CONFIG_MODE_MASK		(3ULL << 27)
#define  XMAC_CONFIG_MODE_XGMII		(0ULL << 27)
#define  XMAC_CONFIG_MODE_GMII		(1ULL << 27)
#define  XMAC_CONFIG_MODE_MII		(2ULL << 27)
#define  XMAC_CONFIG_LFS_DISABLE	(1ULL << 26)
#define  XMAC_CONFIG_LOOPBACK		(1ULL << 25)
#define  XMAC_CONFIG_TX_OUTPUT_EN	(1ULL << 24)
#define  XMAC_CONFIG_SEL_POR_CLK_SRC	(1ULL << 23)
#define  XMAC_CONFIG_ALWAYS_NO_CRC	(1ULL << 3)
#define  XMAC_CONFIG_VAR_MIN_IPG_EN	(1ULL << 2)
#define  XMAC_CONFIG_STRETCH_MODE	(1ULL << 1)
#define  XMAC_CONFIG_TX_ENABLE		(1ULL << 0)

#define XMAC_IPG(port)		(FZC_MAC + 0x00080 + (port) * 0x06000)
#define  XMAC_IPG_IPG_VALUE1_MASK	(0xffULL << 8)
#define  XMAC_IPG_IPG_VALUE1_12		(10ULL << 8)
#define  XMAC_IPG_IPG_VALUE_MASK	(0x07ULL << 0)
#define  XMAC_IPG_IPG_VALUE_12_15	(3ULL << 0)

#define XMAC_MIN(port)		(FZC_MAC + 0x00088 + (port) * 0x06000)
#define  XMAC_MIN_RX_MIN_PKT_SIZE_MASK	(0x3ffULL << 20)
#define  XMAC_MIN_RX_MIN_PKT_SIZE_SHIFT	20
#define  XMAC_MIN_TX_MIN_PKT_SIZE_MASK	(0x3ffULL << 0)
#define  XMAC_MIN_TX_MIN_PKT_SIZE_SHIFT	0
#define XMAC_MAX(port)		(FZC_MAC + 0x00090 + (port) * 0x06000)

#define XMAC_ADDR0(port)	(FZC_MAC + 0x000a0 + (port) * 0x06000)
#define XMAC_ADDR1(port)	(FZC_MAC + 0x000a8 + (port) * 0x06000)
#define XMAC_ADDR2(port)	(FZC_MAC + 0x000b0 + (port) * 0x06000)

#define TXMAC_FRM_CNT(port)	(FZC_MAC + 0x00170 + (port) * 0x06000)
#define TXMAC_BYTE_CNT(port)	(FZC_MAC + 0x00178 + (port) * 0x06000)

#define LINK_FAULT_CNT(port)	(FZC_MAC + 0x00180 + (port) * 0x06000)
#define XMAC_SM_REG(port)	(FZC_MAC + 0x001a8 + (port) * 0x06000)

#define BMAC_ADDR0(port)	(FZC_MAC + 0x00100 + ((port) - 2) * 0x04000)
#define BMAC_ADDR1(port)	(FZC_MAC + 0x00108 + ((port) - 2) * 0x04000)
#define BMAC_ADDR2(port)	(FZC_MAC + 0x00110 + ((port) - 2) * 0x04000)

#define PCS_MII_CTL(port)	(FZC_MAC + 0x04000 + (port) * 0x06000)
#define  PCS_MII_CTL_RESET		(1ULL << 15)
#define PCS_DPATH_MODE(port)	(FZC_MAC + 0x040a0 + (port) * 0x06000)
#define  PCS_DPATH_MODE_MII		(1ULL << 1)

#define MIF_FRAME_OUTPUT	(FZC_MAC + 0x16018)
#define  MIF_FRAME_DATA			0xffff
#define  MIF_FRAME_TA0			(1ULL << 16)
#define  MIF_FRAME_TA1			(1ULL << 17)
#define  MIF_FRAME_REG_SHIFT		18
#define  MIF_FRAME_PHY_SHIFT		23
#define  MIF_FRAME_READ			0x60020000
#define  MIF_FRAME_WRITE		0x50020000
#define MIF_CONFIG		(FZC_MAC + 0x16020)
#define  MIF_CONFIG_INDIRECT_MODE	(1ULL << 15)

#define TX_LOG_PAGE_VLD(chan)	(FZC_DMC + 0x40000 + (chan) * 0x00200)
#define  TX_LOG_PAGE_VLD_PAGE0		(1ULL << 0)
#define  TX_LOG_PAGE_VLD_PAGE1		(1ULL << 1)
#define  TX_LOG_PAGE_VLD_FUNC_SHIFT	2
#define TX_LOG_MASK1(chan)	(FZC_DMC + 0x40008 + (chan) * 0x00200)
#define TX_LOG_VALUE1(chan)	(FZC_DMC + 0x40010 + (chan) * 0x00200)
#define TX_LOG_MASK2(chan)	(FZC_DMC + 0x40018 + (chan) * 0x00200)
#define TX_LOG_VALUE2(chan)	(FZC_DMC + 0x40020 + (chan) * 0x00200)
#define TX_LOG_PAGE_RELO1(chan)	(FZC_DMC + 0x40028 + (chan) * 0x00200)
#define TX_LOG_PAGE_RELO2(chan)	(FZC_DMC + 0x40030 + (chan) * 0x00200)
#define TX_LOG_PAGE_HDL(chan)	(FZC_DMC + 0x40038 + (chan) * 0x00200)

#define TX_RNG_CFIG(chan)	(DMC + 0x40000 + (chan) * 0x00200)
#define  TX_RNG_CFIG_LEN_SHIFT		48
#define TX_RING_HDL(chan)	(DMC + 0x40010 + (chan) * 0x00200)
#define TX_RING_KICK(chan)	(DMC + 0x40018 + (chan) * 0x00200)
#define TX_ENT_MSK(chan)	(DMC + 0x40020 + (chan) * 0x00200)
#define TX_CS(chan)		(DMC + 0x40028 + (chan) * 0x00200)
#define  TX_CS_RST			(1ULL << 31)
#define TDMC_INTR_DBG(chan)	(DMC + 0x40060 + (chan) * 0x00200)
#define TXDMA_MBH(chan)		(DMC + 0x40030 + (chan) * 0x00200)
#define TXDMA_MBL(chan)		(DMC + 0x40038 + (chan) * 0x00200)
#define TX_RNG_ERR_LOGH(chan)	(DMC + 0x40048 + (chan) * 0x00200)
#define TX_RNG_ERR_LOGL(chan)	(DMC + 0x40050 + (chan) * 0x00200)

#define TXD_SOP			(1ULL << 63)
#define TXD_MARK		(1ULL << 62)
#define TXD_NUM_PTR_SHIFT	58
#define TXD_TR_LEN_SHIFT	44

struct nep_txbuf_hdr {
	uint64_t	nh_flags;
	uint64_t	nh_reserved;
};

struct nep_buf {
	bus_dmamap_t	nb_map;
	struct mbuf	*nb_m;
};

#define NEP_NTXDESC	256
#define NEP_NTXSEGS	15

struct nep_dmamem {
	bus_dmamap_t		ndm_map;
	bus_dma_segment_t	ndm_seg;
	size_t			ndm_size;
	caddr_t			ndm_kva;
};
#define NEP_DMA_MAP(_ndm)	((_ndm)->ndm_map)
#define NEP_DMA_LEN(_ndm)	((_ndm)->ndm_size)
#define NEP_DMA_DVA(_ndm)	((_ndm)->ndm_map->dm_segs[0].ds_addr)
#define NEP_DMA_KVA(_ndm)	((void *)(_ndm)->ndm_kva);

struct nep_softc {
	struct device		sc_dev;
	struct arpcom		sc_ac;
#define sc_lladdr	sc_ac.ac_enaddr
	struct mii_data		sc_mii;
#define sc_media	sc_mii.mii_media

	bus_dma_tag_t		sc_dmat;
	bus_space_tag_t		sc_memt;
	bus_space_handle_t 	sc_memh;
	bus_size_t		sc_mems;
	void			*sc_ih;

	int			sc_port;

	struct nep_dmamem	*sc_txmbox;
	struct nep_dmamem	*sc_txring;
	struct nep_buf		*sc_txbuf;
	uint64_t		*sc_txdesc;
	int			sc_tx_prod;
	int			sc_tx_cnt;
	int			sc_tx_cons;

	struct timeout		sc_tick;
};

int	nep_match(struct device *, void *, void *);
void	nep_attach(struct device *, struct device *, void *);

struct cfattach nep_ca = {
	sizeof(struct nep_softc), nep_match, nep_attach
};

struct cfdriver nep_cd = {
	NULL, "nep", DV_DULL
};

uint64_t nep_read(struct nep_softc *, uint32_t);
void	nep_write(struct nep_softc *, uint32_t, uint64_t);
int	nep_mii_readreg(struct device *, int, int);
void	nep_mii_writereg(struct device *, int, int, int);
void	nep_mii_statchg(struct device *);
void	nep_xmac_mii_statchg(struct nep_softc *);
void	nep_bmac_mii_statchg(struct nep_softc *);
int	nep_media_change(struct ifnet *);
void	nep_media_status(struct ifnet *, struct ifmediareq *);
int	nep_intr(void *);

void	nep_up(struct nep_softc *);
void	nep_down(struct nep_softc *);
void	nep_iff(struct nep_softc *);
int	nep_encap(struct nep_softc *, struct mbuf *, int *);

void	nep_start(struct ifnet *);
void	nep_tick(void *);
int	nep_ioctl(struct ifnet *, u_long, caddr_t);

struct nep_dmamem *nep_dmamem_alloc(struct nep_softc *, size_t);
void	nep_dmamem_free(struct nep_softc *, struct nep_dmamem *);

int
nep_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_NEPTUNE)
		return (1);
	return (0);
}

void
nep_attach(struct device *parent, struct device *self, void *aux)
{
	struct nep_softc *sc = (struct nep_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mii_data *mii = &sc->sc_mii;
	pcireg_t memtype;
	uint64_t val;

	sc->sc_dmat = pa->pa_dmat;

	memtype = PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT;
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0,
	    &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf(": can't map registers\n");
		return;
	}

	if (pci_intr_map_msi(pa, &ih) && pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih =  pci_intr_establish(pa->pa_pc, ih, IPL_NET,
	    nep_intr, sc, self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
		return;
	}

	printf(": %s", intrstr);

	sc->sc_port = pa->pa_function;

	nep_write(sc, SID(sc->sc_port), pa->pa_function << 5);
	nep_write(sc, LDG_NUM(LDN_RXDMA(sc->sc_port)), sc->sc_port);
	nep_write(sc, LDG_NUM(LDN_TXDMA(sc->sc_port)), sc->sc_port);
	nep_write(sc, LDG_NUM(LDN_MAC(sc->sc_port)), sc->sc_port);

	/* Port 0 gets the MIF and error interrupts. */
	if (sc->sc_port == 0) {
		nep_write(sc, LDG_NUM(LDN_MIF), sc->sc_port);
		nep_write(sc, LDG_NUM(LDN_SYSERR), sc->sc_port);
	}

#ifdef __sparc64__
	if (OF_getprop(PCITAG_NODE(pa->pa_tag), "local-mac-address",
	    sc->sc_lladdr, ETHER_ADDR_LEN) <= 0)
		myetheraddr(sc->sc_lladdr);
#endif

	printf(", address %s\n", ether_sprintf(sc->sc_lladdr));

	val = nep_read(sc, MIF_CONFIG);
	val &= ~MIF_CONFIG_INDIRECT_MODE;
	nep_write(sc, MIF_CONFIG, val);

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof(ifp->if_xname));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;
	ifp->if_ioctl = nep_ioctl;
	ifp->if_start = nep_start;

	mii->mii_ifp = ifp;
	mii->mii_readreg = nep_mii_readreg;
	mii->mii_writereg = nep_mii_writereg;
	mii->mii_statchg = nep_mii_statchg;

	ifmedia_init(&sc->sc_media, 0, nep_media_change, nep_media_status);

	/*
	 * The PHYs are wired up in reverse order on the 4x1G (RGMII)
	 * configuration.
	 */
	mii_attach(&sc->sc_dev, mii, 0xffffffff, MII_PHY_ANY,
	    sc->sc_port ^ 0x3, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc->sc_dev.dv_xname);
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->sc_tick, nep_tick, sc);

	/* Enable the MIF and error interrupts. */
	if (sc->sc_port == 0) {
		nep_write(sc, LD_IM0(LDN_MIF), 0);
		nep_write(sc, LD_IM1(LDN_SYSERR), 0);
	}
}

uint64_t
nep_read(struct nep_softc *sc, uint32_t reg)
{
	return (bus_space_read_8(sc->sc_memt, sc->sc_memh, reg));
}

void
nep_write(struct nep_softc *sc, uint32_t reg, uint64_t value)
{
	bus_space_write_8(sc->sc_memt, sc->sc_memh, reg, value);
}

int
nep_mii_readreg(struct device *self, int phy, int reg)
{
	struct nep_softc *sc = (struct nep_softc *)self;
	uint64_t frame;
	int n;

	frame = MIF_FRAME_READ;
	frame |= (reg << MIF_FRAME_REG_SHIFT) | (phy << MIF_FRAME_PHY_SHIFT);
	nep_write(sc, MIF_FRAME_OUTPUT, frame);
	for (n = 0; n < 1000; n++) {
		delay(10);
		frame = nep_read(sc, MIF_FRAME_OUTPUT);
		if (frame & MIF_FRAME_TA0)
			return (frame & MIF_FRAME_DATA);
	}

	printf("%s: %s timeout\n", sc->sc_dev.dv_xname, __func__);
	return (0);
}

void
nep_mii_writereg(struct device *self, int phy, int reg, int val)
{
	struct nep_softc *sc = (struct nep_softc *)self;
	uint64_t frame;
	int n;

	frame = MIF_FRAME_WRITE;
	frame |= (reg << MIF_FRAME_REG_SHIFT) | (phy << MIF_FRAME_PHY_SHIFT);
	frame |= (val & MIF_FRAME_DATA);
	nep_write(sc, MIF_FRAME_OUTPUT, frame);
	for (n = 0; n < 1000; n++) {
		delay(10);
		frame = nep_read(sc, MIF_FRAME_OUTPUT);
		if (frame & MIF_FRAME_TA0)
			return;
	}

	printf("%s: %s timeout\n", sc->sc_dev.dv_xname, __func__);
	return;
}

void
nep_mii_statchg(struct device *dev)
{
	struct nep_softc *sc = (struct nep_softc *)dev;

	if (sc->sc_port < 2)
		nep_xmac_mii_statchg(sc);
	else
		nep_bmac_mii_statchg(sc);
}

void
nep_xmac_mii_statchg(struct nep_softc *sc)
{
	struct mii_data *mii = &sc->sc_mii;
	uint64_t val;

	val = nep_read(sc, XMAC_CONFIG(sc->sc_port));

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX)
		val |= XMAC_CONFIG_SEL_CLK_25MHZ;
	else
		val &= ~XMAC_CONFIG_SEL_CLK_25MHZ;

	val |= XMAC_CONFIG_1G_PCS_BYPASS;

	val &= ~XMAC_CONFIG_MODE_MASK;
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T ||
	    IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX)
		val |= XMAC_CONFIG_MODE_GMII;
	else
		val |= XMAC_CONFIG_MODE_MII;

	val |= XMAC_CONFIG_LFS_DISABLE;

	if (mii->mii_media_active & IFM_LOOP)
		val |= XMAC_CONFIG_LOOPBACK;
	else
		val &= ~XMAC_CONFIG_LOOPBACK;

	val |= XMAC_CONFIG_TX_OUTPUT_EN;

	nep_write(sc, XMAC_CONFIG(sc->sc_port), val);
}

void
nep_bmac_mii_statchg(struct nep_softc *sc)
{
	printf("%s\n", __func__);
}

int
nep_media_change(struct ifnet *ifp)
{
	struct nep_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys))
		mii_mediachg(&sc->sc_mii);

	return (0);
}

void
nep_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct nep_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys)) {
		mii_pollstat(&sc->sc_mii);
		ifmr->ifm_active = sc->sc_mii.mii_media_active;
		ifmr->ifm_status = sc->sc_mii.mii_media_status;
	}
}

int
nep_intr(void *arg)
{
	struct nep_softc *sc = arg;
	uint64_t sv0, sv1, sv2;

	printf("%s: %s\n", sc->sc_dev.dv_xname, __func__);

	sv0 = nep_read(sc, LDSV0(sc->sc_port));
	sv1 = nep_read(sc, LDSV1(sc->sc_port));
	sv2 = nep_read(sc, LDSV2(sc->sc_port));

	if (sv0 || sv1 || sv2) {
		printf("%s: %s %llx %llx %llx\n", sc->sc_dev.dv_xname,
		    __func__, sv0, sv1, sv2);
		return (1);
	}

	return (0);
}

void
nep_up(struct nep_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct nep_buf *txb;
	uint64_t addr0, addr1, addr2;
	uint64_t val;
	int i, n;

	/* Allocate Tx mail box. */
	sc->sc_txmbox = nep_dmamem_alloc(sc, PAGE_SIZE);

	/* Allocate Tx descriptor ring. */
	sc->sc_txring = nep_dmamem_alloc(sc, NEP_NTXDESC * sizeof(uint64_t));
	sc->sc_txdesc = NEP_DMA_KVA(sc->sc_txring);

	sc->sc_txbuf = malloc(sizeof(struct nep_buf) * NEP_NTXDESC,
	    M_DEVBUF, M_WAITOK);
	for (i = 0; i < NEP_NTXDESC; i++) {
		txb = &sc->sc_txbuf[i];
		bus_dmamap_create(sc->sc_dmat, MCLBYTES, NEP_NTXSEGS,
		    MCLBYTES, 0, BUS_DMA_WAITOK, &txb->nb_map);
		txb->nb_m = NULL;
	}

	sc->sc_tx_prod = sc->sc_tx_cons = 0;

	if (sc->sc_port < 2) {
		/* Disable the POR loopback clock source. */
		val = nep_read(sc, XMAC_CONFIG(sc->sc_port));
		val &= ~XMAC_CONFIG_SEL_POR_CLK_SRC;
		nep_write(sc, XMAC_CONFIG(sc->sc_port), val);
	}

	addr0 = (sc->sc_lladdr[4] << 8) | sc->sc_lladdr[5];
	addr1 = (sc->sc_lladdr[2] << 8) | sc->sc_lladdr[3];
	addr2 = (sc->sc_lladdr[0] << 8) | sc->sc_lladdr[1];

	if (sc->sc_port < 2) {
		nep_write(sc, XMAC_ADDR0(sc->sc_port), addr0);
		nep_write(sc, XMAC_ADDR1(sc->sc_port), addr1);
		nep_write(sc, XMAC_ADDR2(sc->sc_port), addr2);
	} else {
		nep_write(sc, BMAC_ADDR0(sc->sc_port), addr0);
		nep_write(sc, BMAC_ADDR1(sc->sc_port), addr1);
		nep_write(sc, BMAC_ADDR2(sc->sc_port), addr2);
	}		

	val = nep_read(sc, TXC_CONTROL);
	val |= TXC_CONTROL_TXC_ENABLED;
	val |= (1ULL << sc->sc_port);
	nep_write(sc, TXC_CONTROL, val);

	nep_write(sc, TXC_PORT_DMA(sc->sc_port), 1ULL << sc->sc_port);

	val = nep_read(sc, TXC_INT_MASK);
	val &= ~TXC_INT_MASK_PORT_INT_MASK(sc->sc_port);
	nep_write(sc, TXC_INT_MASK, val);

	val = nep_read(sc, TX_CS(sc->sc_port));
	val |= TX_CS_RST;
	nep_write(sc, TX_CS(sc->sc_port), val);

	n = 1000;
	while (--n) {
		val = nep_read(sc, TX_CS(sc->sc_port));
		if ((val & TX_CS_RST) == 0)
			break;
	}
	if (n == 0)
		printf("timeout resetting transmit ring\n");
	printf("TX_CS %llx\n", val);

	nep_write(sc, TX_LOG_MASK1(sc->sc_port), 0);
	nep_write(sc, TX_LOG_VALUE1(sc->sc_port), 0);
	nep_write(sc, TX_LOG_MASK2(sc->sc_port), 0);
	nep_write(sc, TX_LOG_VALUE2(sc->sc_port), 0);
	nep_write(sc, TX_LOG_PAGE_RELO1(sc->sc_port), 0);
	nep_write(sc, TX_LOG_PAGE_RELO2(sc->sc_port), 0);
	nep_write(sc, TX_LOG_PAGE_HDL(sc->sc_port), 0);
	nep_write(sc, TX_LOG_PAGE_VLD(sc->sc_port), 0x3);

	nep_write(sc, TX_RING_KICK(sc->sc_port), 0);

	nep_write(sc, TXC_DMA_MAX(sc->sc_port), ETHER_MAX_LEN + 64);
	nep_write(sc, TX_ENT_MSK(sc->sc_port), 0);

	val = NEP_DMA_DVA(sc->sc_txring);
	val |= (NEP_DMA_LEN(sc->sc_txring) / 64) << TX_RNG_CFIG_LEN_SHIFT;
	nep_write(sc, TX_RNG_CFIG(sc->sc_port), val);

	nep_write(sc, TXDMA_MBH(sc->sc_port),
	    NEP_DMA_DVA(sc->sc_txmbox) >> 32);
	nep_write(sc, TXDMA_MBL(sc->sc_port),
	    NEP_DMA_DVA(sc->sc_txmbox) & 0xffffffff);

	nep_write(sc, TX_CS(sc->sc_port), 0);
	printf("TX_CS: %llx\n", nep_read(sc, TX_CS(sc->sc_port)));

	if (sc->sc_port < 2) {
		nep_write(sc, PCS_DPATH_MODE(sc->sc_port), PCS_DPATH_MODE_MII);
		val = nep_read(sc, PCS_MII_CTL(sc->sc_port));
		val |= PCS_MII_CTL_RESET;
		nep_write(sc, PCS_MII_CTL(sc->sc_port), val);
		n = 1000;
		while (--n) {
			val = nep_read(sc, PCS_MII_CTL(sc->sc_port));
			if ((val & PCS_MII_CTL_RESET) == 0)
				break;
		}
		if (n == 0)
			printf("timeout resetting PCS\n");

		nep_write(sc, XTXMAC_SW_RST(sc->sc_port),
		    XTXMAC_SW_RST_REG_RST | XTXMAC_SW_RST_SOFT_RST);
		n = 1000;
		while (--n) {
			val = nep_read(sc, XTXMAC_SW_RST(sc->sc_port));
			if ((val & (XTXMAC_SW_RST_REG_RST |
			    XTXMAC_SW_RST_SOFT_RST)) == 0)
				break;
		}
		if (n == 0)
			printf("timeout resetting Tx MAC\n");

		val = nep_read(sc, XMAC_CONFIG(sc->sc_port));
		val &= ~XMAC_CONFIG_ALWAYS_NO_CRC;
		val &= ~XMAC_CONFIG_VAR_MIN_IPG_EN;
		val &= ~XMAC_CONFIG_STRETCH_MODE;
		val &= ~XMAC_CONFIG_TX_ENABLE;
		nep_write(sc, XMAC_CONFIG(sc->sc_port), val);

		val = nep_read(sc, XMAC_IPG(sc->sc_port));
		val &= ~XMAC_IPG_IPG_VALUE1_MASK;	/* MII/GMII mode */
		val |= XMAC_IPG_IPG_VALUE1_12;
		val &= ~XMAC_IPG_IPG_VALUE_MASK;	/* XGMII mode */
		val |= XMAC_IPG_IPG_VALUE_12_15;
		nep_write(sc, XMAC_IPG(sc->sc_port), val);

		val = nep_read(sc, XMAC_MIN(sc->sc_port));
		val &= ~XMAC_MIN_RX_MIN_PKT_SIZE_MASK;
		val &= ~XMAC_MIN_TX_MIN_PKT_SIZE_MASK;
		val |= (64 << XMAC_MIN_RX_MIN_PKT_SIZE_SHIFT);
		val |= (64 << XMAC_MIN_TX_MIN_PKT_SIZE_SHIFT);
		nep_write(sc, XMAC_MIN(sc->sc_port), val);
		nep_write(sc, XMAC_MAX(sc->sc_port), ETHER_MAX_LEN);

		nep_write(sc, TXMAC_FRM_CNT(sc->sc_port), 0);
		nep_write(sc, TXMAC_BYTE_CNT(sc->sc_port), 0);

		val = nep_read(sc, XMAC_CONFIG(sc->sc_port));
		val |= XMAC_CONFIG_TX_ENABLE;
		nep_write(sc, XMAC_CONFIG(sc->sc_port), val);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	/* Enable Tx interrupts. */
	nep_write(sc, LD_IM1(LDN_MAC(sc->sc_port)), 0);
	nep_write(sc, LD_IM0(LDN_TXDMA(sc->sc_port)), 0);
	nep_write(sc, LDGIMGN(sc->sc_port), LDGIMGN_ARM | 2);

	timeout_add_sec(&sc->sc_tick, 1);
}

void
nep_down(struct nep_softc *sc)
{
	timeout_del(&sc->sc_tick);
}

void
nep_iff(struct nep_softc *sc)
{
	printf("%s\n", __func__);
}

int
nep_encap(struct nep_softc *sc, struct mbuf *m, int *idx)
{
	struct nep_txbuf_hdr *nh;
	uint64_t txd;
	bus_dmamap_t map;
	int cur, frag, i;
	int len, pad;
	int err;

	printf("%s: %s\n", sc->sc_dev.dv_xname, __func__);
	printf("TX_CS: %llx\n", nep_read(sc, TX_CS(sc->sc_port)));
//	printf("TX_RNG_ERR_LOGH: %llx\n",
//	       nep_read(sc, TX_RNG_ERR_LOGH(sc->sc_port)));
//	printf("TX_RNG_ERR_LOGL: %llx\n",
//	       nep_read(sc, TX_RNG_ERR_LOGL(sc->sc_port)));
	printf("SYS_ERR_STAT %llx\n", nep_read(sc, SYS_ERR_STAT));
	printf("TXC_INT_STAT_DBG %llx\n", nep_read(sc, TXC_INT_STAT_DBG));
	printf("TXC_PKT_STUFFED: %llx\n",
	       nep_read(sc, TXC_PKT_STUFFED(sc->sc_port)));
	printf("TXC_PKT_XMIT: %llx\n",
	       nep_read(sc, TXC_PKT_XMIT(sc->sc_port)));
//	printf("TX_RING_HDL: %llx\n",
//	       nep_read(sc, TX_RING_HDL(sc->sc_port)));
	printf("XMAC_CONFIG: %llx\n",
	       nep_read(sc, XMAC_CONFIG(sc->sc_port)));
	printf("XTXMAC_STATUS: %llx\n",
	       nep_read(sc, XTXMAC_STATUS(sc->sc_port)));
	printf("TXMAC_FRM_CNT: %llx\n",
	       nep_read(sc, TXMAC_FRM_CNT(sc->sc_port)));
	printf("TXMAC_BYTE_CNT: %llx\n",
	       nep_read(sc, TXMAC_BYTE_CNT(sc->sc_port)));

	/*
	 * MAC does not support padding of transmit packets that are
	 * fewer than 60 bytes.
	 */
	if (m->m_pkthdr.len < (ETHER_MIN_LEN - ETHER_CRC_LEN)) {
		struct mbuf *n;
		int padlen;

		padlen = (ETHER_MIN_LEN - ETHER_CRC_LEN) - m->m_pkthdr.len;
		MGET(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return (ENOBUFS);
		memset(mtod(n, caddr_t), 0, padlen);
		n->m_len = padlen;
		m_cat(m, n);
		m->m_pkthdr.len += padlen;
	}

	pad = mtod(m, u_long) % 16;
	len = m->m_pkthdr.len + pad;
	M_PREPEND(m, sizeof(*nh) + pad, M_DONTWAIT);
	if (m == NULL)
		return (ENOBUFS);
	nh = mtod(m, struct nep_txbuf_hdr *);
	nh->nh_flags = htole64((len << 16) | (pad / 2));
	nh->nh_reserved = 0;

	cur = frag = *idx;
	map = sc->sc_txbuf[cur].nb_map;

	err = bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT);
	m_adj(m, sizeof(*nh) + pad);
	if (err)
		return (ENOBUFS);

	if (map->dm_nsegs > (NEP_NTXDESC - sc->sc_tx_cnt - 2)) {
		bus_dmamap_unload(sc->sc_dmat, map);
		return (ENOBUFS);
	}

	/* Sync the DMA map. */
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	txd = TXD_SOP | TXD_MARK;
	txd |= ((uint64_t)map->dm_nsegs << TXD_NUM_PTR_SHIFT);
	for (i = 0; i < map->dm_nsegs; i++) {
		
		txd |= ((uint64_t)map->dm_segs[i].ds_len << TXD_TR_LEN_SHIFT);
		txd |= map->dm_segs[i].ds_addr;
		sc->sc_txdesc[frag] = htole64(txd);
		txd = 0;

		bus_dmamap_sync(sc->sc_dmat, NEP_DMA_MAP(sc->sc_txring),
		    frag * sizeof(txd), sizeof(txd), BUS_DMASYNC_PREWRITE);

		cur = frag++;
		if (frag >= NEP_NTXDESC)
			frag = 0;
		KASSERT(frag != sc->sc_tx_cons);
	}

	KASSERT(sc->sc_txbuf[cur].nb_m == NULL);
	sc->sc_txbuf[*idx].nb_map = sc->sc_txbuf[cur].nb_map;
	sc->sc_txbuf[cur].nb_map = map;
	sc->sc_txbuf[cur].nb_m = m;

	sc->sc_tx_cnt += map->dm_nsegs;
	*idx = frag;

	/* XXX toggle TX_RING_KICK_WRAP */
	nep_write(sc, TX_RING_KICK(sc->sc_port), frag << 3);

	printf("TX_CS: %llx\n", nep_read(sc, TX_CS(sc->sc_port)));
//	printf("TX_RNG_ERR_LOGH: %llx\n",
//	       nep_read(sc, TX_RNG_ERR_LOGH(sc->sc_port)));
//	printf("TX_RNG_ERR_LOGL: %llx\n",
//	       nep_read(sc, TX_RNG_ERR_LOGL(sc->sc_port)));
	return (0);
}

void
nep_start(struct ifnet *ifp)
{
	struct nep_softc *sc = (struct nep_softc *)ifp->if_softc;
	struct mbuf *m;
	int idx;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;
	if (ifp->if_flags & IFF_OACTIVE)
		return;
	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	idx = sc->sc_tx_prod;
	while (sc->sc_tx_cnt < NEP_NTXDESC) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (nep_encap(sc, m, &idx)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* Now we are committed to transmit the packet. */
		IFQ_DEQUEUE(&ifp->if_snd, m);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	}

	if (sc->sc_tx_prod != idx) {
		sc->sc_tx_prod = idx;

		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = 5;
	}
}

void
nep_tick(void *arg)
{
	struct nep_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add_sec(&sc->sc_tick, 1);
}

int
nep_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nep_softc *sc = (struct nep_softc *)ifp->if_softc;
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
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				nep_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				nep_down(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			nep_iff(sc);
		error = 0;
	}

	splx(s);
	return (error);
}

struct nep_dmamem *
nep_dmamem_alloc(struct nep_softc *sc, size_t size)
{
	struct nep_dmamem *m;
	int nsegs;

	m = malloc(sizeof(*m), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (m == NULL)
		return (NULL);

	m->ndm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &m->ndm_map) != 0)
		goto qdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &m->ndm_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &m->ndm_seg, nsegs, size, &m->ndm_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, m->ndm_map, m->ndm_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	return (m);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, m->ndm_kva, m->ndm_size);
free:
	bus_dmamem_free(sc->sc_dmat, &m->ndm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, m->ndm_map);
qdmfree:
	free(m, M_DEVBUF, sizeof(*m));

	return (NULL);
}

void
nep_dmamem_free(struct nep_softc *sc, struct nep_dmamem *m)
{
	bus_dmamap_unload(sc->sc_dmat, m->ndm_map);
	bus_dmamem_unmap(sc->sc_dmat, m->ndm_kva, m->ndm_size);
	bus_dmamem_free(sc->sc_dmat, &m->ndm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, m->ndm_map);
	free(m, M_DEVBUF, sizeof(*m));
}
