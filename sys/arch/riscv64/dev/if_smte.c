/*	$OpenBSD: if_smte.c,v 1.2 2026/05/11 10:25:52 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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
 * Driver for the ethernet controller on the SpacemiT K1 SoC.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

/* DMA/MAC registers */
#define DMA_CONFIG				0x0000
#define  DMA_CONFIG_DMA_64BIT_MODE		(1U << 18)
#define  DMA_CONFIG_STRICT_BURST		(1U << 17)
#define  DMA_CONFIG_BURST_LENGTH_MASK		(0x7f << 1)
#define  DMA_CONFIG_BURST_LENGTH_16		(0x10 << 1)
#define  DMA_CONFIG_SOFTWARE_RESET		(1U << 0)
#define DMA_CTRL				0x0004
#define  DMA_CTRL_START_STOP_RX_DMA		(1U << 1)
#define  DMA_CTRL_START_STOP_TX_DMA		(1U << 0)
#define DMA_STATUS_IRQ				0x0008
#define  DMA_STATUS_IRQ_RX_MISSED_FRAME		(1U << 7)
#define  DMA_STATUS_IRQ_RX_DMA_STOPPED		(1U << 6)
#define  DMA_STATUS_IRQ_RX_DES_UNAVAILABLE	(1U << 5)
#define  DMA_STATUS_IRQ_RX_TRANSFER_DONE	(1U << 4)
#define  DMA_STATUS_IRQ_TX_TRANSFER_DONE	(1U << 0)
#define DMA_INTR_ENABLE				0x000c
#define  DMA_INTR_ENABLE_RX_MISSED_FRAME	(1U << 7)
#define  DMA_INTR_ENABLE_RX_DMA_STOPPED		(1U << 6)
#define  DMA_INTR_ENABLE_RX_DES_UNAVAILABLE	(1U << 5)
#define  DMA_INTR_ENABLE_RX_TRANSFER_DONE	(1U << 4)
#define  DMA_INTR_ENABLE_TX_TRANSFER_DONE	(1U << 0)
#define DMA_TRANSMIT_AUTO_POLL_COUNTER		0x0010
#define DMA_TRANSMIT_POLL_DEMAND		0x0014
#define DMA_RECEIVE_POLL_DEMAND			0x0018
#define DMA_TRANSMIT_BASE_ADDRESS		0x001c
#define DMA_RECEIVE_BASE_ADDRESS		0x0020
#define DMA_RECEIVE_IRQ_MITIGATION		0x002c
#define  DMA_RECEIVE_IRQ_MITIGATION_FRAME_COUNTER_SHIFT 0
#define  DMA_RECEIVE_IRQ_MITIGATION_TIMEOUT_COUNTER_SHIFT 8
#define  DMA_RECEIVE_IRQ_MITIGATION_MITIGATION_ENABLE (1U << 31)
#define MAC_GLOBAL_CTRL				0x0100
#define  MAC_GLOBAL_CTRL_DUPLEX_MODE		(1U << 2)
#define  MAC_GLOBAL_CTRL_SPEED_MASK		(0x3 << 0)
#define  MAC_GLOBAL_CTRL_SPEED_10		(0x0 << 0)
#define  MAC_GLOBAL_CTRL_SPEED_100		(0x1 << 0)
#define  MAC_GLOBAL_CTRL_SPEED_1000		(0x2 << 0)
#define MAC_TRANSMIT_CTRL			0x0104
#define  MAC_TRANSMIT_CTRL_IFG_LEN_MASK		(0x7 << 4)
#define  MAC_TRANSMIT_CTRL_TX_AUTO_RETRY	(1U << 3)
#define  MAC_TRANSMIT_CTRL_TX_ENABLE		(1U << 0)
#define MAC_RECEIVE_CTRL			0x0108
#define  MAC_RECEIVE_CTRL_STORE_FORWARD		(1U << 3)
#define  MAC_RECEIVE_CTRL_RX_ENABLE		(1U << 0)
#define MAC_MAXIMUM_FRAME_SIZE			0x010c
#define MAC_TRANSMIT_JABBER_SIZE		0x0110
#define MAC_RECEIVE_JABBER_SIZE			0x0114
#define MAC_ADDR_CTRL				0x0118
#define  MAC_ADDR_CTRL_PROMISCUOUS_MODE		(1U << 8)
#define  MAC_ADDR_CTRL_MAC_ADDR1_ENABLE		(1U << 0)
#define MAC_ADDR1_HI				0x0120
#define MAC_ADDR1_ME				0x0124
#define MAC_ADDR1_LO				0x0128
#define MAC_MULTICAST_HASH_TABLE1		0x0150
#define MAC_MULTICAST_HASH_TABLE2		0x0154
#define MAC_MULTICAST_HASH_TABLE3		0x0158
#define MAC_MULTICAST_HASH_TABLE4		0x015c
#define MAC_MDIO_CTRL				0x01a0
#define  MAC_MDIO_CTRL_START_MDIO_TRANS		(1U << 15)
#define  MAC_MDIO_CTRL_MDIO_READ_WRITE		(1U << 10)
#define  MAC_MDIO_CTRL_REGISTER_ADDRESS_SHIFT	5
#define  MAC_MDIO_CTRL_PHY_ADDRESS_SHIFT	0
#define MAC_MDIO_DATA				0x01a4
#define MAC_TRANSMIT_FIFO_ALMOST_FULL		0x01c0
#define MAC_TRANSMIT_PACKET_START_THRESHOLD	0x01c4
#define MAC_RECEIVE_PACKET_START_THRESHOLD	0x01c8
#define MAC_INTR_ENABLE				0x01e4

/* APMU registers */
#define APMU_EMAC_CLK_RST_CTRL			0x0000
#define  APMU_EMAC_AXI_MST_ID			(1U << 13)
#define  APMU_EMAC_PHY_INTR_EN			(1U << 12)
#define  APMU_EMAC_RGMII_TXC_SRC_SEL		(1U << 8)
#define APMU_EMAC_RGMII_DLINE			0x0004
#define  APMU_EMAC_RGMII_DLINE_TX_DELAY_MASK	(0xff << 24)
#define  APMU_EMAC_RGMII_DLINE_TX_DELAY_SHIFT	8
#define  APMU_EMAC_RGMII_DLINE_TX_STEP_MASK	(0x3 << 20)
#define  APMU_EMAC_RGMII_DLINE_TX_STEP_15P6	(0x0 << 20)
#define  APMU_EMAC_RGMII_DLINE_TX_EN		(1U << 16)
#define  APMU_EMAC_RGMII_DLINE_RX_DELAY_MASK	(0xff << 8)
#define  APMU_EMAC_RGMII_DLINE_RX_DELAY_SHIFT	8
#define  APMU_EMAC_RGMII_DLINE_RX_STEP_MASK	(0x3 << 4)
#define  APMU_EMAC_RGMII_DLINE_RX_STEP_15P6	(0x0 << 4)
#define  APMU_EMAC_RGMII_DLINE_RX_EN		(1U << 0)

/* Descriptors */
struct smte_desc {
	uint32_t sd_desc0;
	uint32_t sd_desc1;
	uint32_t sd_addr1;
	uint32_t sd_addr2;
};

/* Rx bits */
#define RX_DESC0_FRAME_PACKET_LENGTH_MASK	(0x3fff << 0)
#define RX_DESC0_FRAME_PACKET_LENGTH_SHIFT	0
#define RX_DESC0_FRAME_RUNT			(1U << 15)
#define RX_DESC0_FRAME_CRC_ERR			(1U << 20)
#define RX_DESC0_FRAME_MAX_LEN_ERR		(1U << 21)
#define RX_DESC0_FRAME_JABBER_ERR		(1U << 22)
#define RX_DESC0_FRAME_LENGTH_ERR		(1U << 23)
#define RX_DESC0_OWN				(1U << 31)
#define RX_DESC1_SIZE1_MASK			(0xfffff << 0)
#define RX_DESC1_SIZE1_SHIFT			0
#define RX_DESC1_SIZE2_MASK			(0xfffff << 12)
#define RX_DESC1_SIZE2_SHIFT			12
#define RX_DESC1_END_RING			(1U << 26)

/* Tx bits */
#define TX_DESC0_OWN				(1U << 31)
#define TX_DESC1_SIZE1_MASK			(0xfff << 0)
#define TX_DESC1_SIZE1_SHIFT			0
#define TX_DESC1_SIZE2_MASK			(0xfff << 12)
#define TX_DESC1_SIZE2_SHIFT			12
#define TX_DESC1_END_RING			(1U << 26)
#define TX_DESC1_FIRST_SEGMENT			(1U << 29)
#define TX_DESC1_LAST_SEGMENT			(1U << 30)
#define TX_DESC1_INTERRUPT_ON_COMPLETION	(1U << 31)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct smte_buf {
	bus_dmamap_t	sb_map;
	struct mbuf	*sb_m;
};

#define SMTE_NTXDESC	1024
#define SMTE_NTXSEGS	16

#define SMTE_NRXDESC	1024

struct smte_dmamem {
	bus_dmamap_t		sdm_map;
	bus_dma_segment_t	sdm_seg;
	size_t			sdm_size;
	caddr_t			sdm_kva;
};
#define SMTE_DMA_MAP(_sdm)	((_sdm)->sdm_map)
#define SMTE_DMA_LEN(_sdm)	((_sdm)->sdm_size)
#define SMTE_DMA_DVA(_sdm)	((_sdm)->sdm_map->dm_segs[0].ds_addr)
#define SMTE_DMA_KVA(_sdm)	((void *)(_sdm)->sdm_kva)

struct smte_softc {
	struct device		sc_dev;
	int			sc_node;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
	struct regmap		*sc_apmu;
	uint32_t		sc_apmu_offset;
	void			*sc_ih;

	struct arpcom		sc_ac;
#define sc_lladdr	sc_ac.ac_enaddr
	struct mii_data		sc_mii;
#define sc_media	sc_mii.mii_media
	int			sc_link;
	int			sc_phyloc;
	uint32_t		sc_rx_delay;
	uint32_t		sc_tx_delay;

	struct smte_dmamem	*sc_txring;
	struct smte_buf		*sc_txbuf;
	struct smte_desc	*sc_txdesc;
	int			sc_tx_prod;
	int			sc_tx_cons;

	struct smte_dmamem	*sc_rxring;
	struct smte_buf		*sc_rxbuf;
	struct smte_desc	*sc_rxdesc;
	int			sc_rx_prod;
	struct if_rxring	sc_rx_ring;
	int			sc_rx_cons;

	struct timeout		sc_tick;
};

#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

int	smte_match(struct device *, void *, void *);
void	smte_attach(struct device *, struct device *, void *);
void	smte_init(struct smte_softc *sc);
void	smte_phy_setup_emac(struct smte_softc *);
void	smte_phy_setup_gmac(struct smte_softc *);

const struct cfattach smte_ca = {
	sizeof(struct smte_softc), smte_match, smte_attach
};

struct cfdriver smte_cd = {
	NULL, "smte", DV_IFNET
};

uint32_t smte_read(struct smte_softc *, bus_addr_t);
void	smte_write(struct smte_softc *, bus_addr_t, uint32_t);

int	smte_ioctl(struct ifnet *, u_long, caddr_t);
void	smte_start(struct ifqueue *);

int	smte_media_change(struct ifnet *);
void	smte_media_status(struct ifnet *, struct ifmediareq *);

int	smte_mii_readreg(struct device *, int, int);
void	smte_mii_writereg(struct device *, int, int, int);
void	smte_mii_statchg(struct device *);

void	smte_lladdr_write(struct smte_softc *);

void	smte_tick(void *);
void	smte_rxtick(void *);

int	smte_intr(void *);
void	smte_tx_proc(struct smte_softc *);
void	smte_rx_proc(struct smte_softc *);

void	smte_up(struct smte_softc *);
void	smte_down(struct smte_softc *);
void	smte_iff(struct smte_softc *);
int	smte_encap(struct smte_softc *, struct mbuf *, int *, int *);

void	smte_stop_dma(struct smte_softc *);

struct smte_dmamem *
	smte_dmamem_alloc(struct smte_softc *, bus_size_t, bus_size_t);
void	smte_dmamem_free(struct smte_softc *, struct smte_dmamem *);
struct mbuf *smte_alloc_mbuf(struct smte_softc *, bus_dmamap_t);
void	smte_fill_rx_ring(struct smte_softc *);

int
smte_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "spacemit,k1-emac");
}

void
smte_attach(struct device *parent, struct device *self, void *aux)
{
	struct smte_softc *sc = (void *)self;
	struct fdt_attach_args *faa = aux;
	char phy_mode[16] = { 0 };
	struct ifnet *ifp;
	uint32_t apmu[2];
	uint32_t phy;
	int mii_flags = 0;
	int node;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_dmat = faa->fa_dmat;
	sc->sc_node = faa->fa_node;

	if (OF_getpropintarray(sc->sc_node, "spacemit,apmu",
	    apmu, sizeof(apmu)) != sizeof(apmu)) {
		printf(": no apmu register\n");
		goto unmap;
	}

	sc->sc_apmu = regmap_byphandle(apmu[0]);
	sc->sc_apmu_offset = apmu[1];
	if (sc->sc_apmu == NULL) {
		printf(": can't get apmu registers\n");
		goto unmap;
	}
	
	OF_getprop(sc->sc_node, "phy-mode", phy_mode, sizeof(phy_mode));
	if (strcmp(phy_mode, "rgmii") == 0)
		mii_flags |= MIIF_SETDELAY;
	else if (strcmp(phy_mode, "rgmii-rxid") == 0)
		mii_flags |= MIIF_SETDELAY | MIIF_RXID;
	else if (strcmp(phy_mode, "rgmii-txid") == 0)
		mii_flags |= MIIF_SETDELAY | MIIF_TXID;
	else if (strcmp(phy_mode, "rgmii-id") == 0)
		mii_flags |= MIIF_SETDELAY | MIIF_RXID | MIIF_TXID;

	sc->sc_rx_delay =
	    OF_getpropint(sc->sc_node, "rx-internal-delay-ps", 0);
	sc->sc_tx_delay =
	    OF_getpropint(sc->sc_node, "tx-internal-delay-ps", 0);

	/* Lookup PHY. */
	phy = OF_getpropint(sc->sc_node, "phy-handle", 0);
	node = OF_getnodebyphandle(phy);
	if (node)
		sc->sc_phyloc = OF_getpropint(node, "reg", MII_PHY_ANY);
	else
		sc->sc_phyloc = MII_PHY_ANY;
	sc->sc_mii.mii_node = node;

	OF_getprop(faa->fa_node, "local-mac-address",
	    &sc->sc_lladdr, ETHER_ADDR_LEN);
	printf(": address %s\n", ether_sprintf(sc->sc_lladdr));

	smte_init(sc);

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_NET | IPL_MPSAFE,
	    smte_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: can't establish interrupt\n", sc->sc_dev.dv_xname);
		goto unmap;
	}

	timeout_set(&sc->sc_tick, smte_tick, sc);

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = smte_ioctl;
	ifp->if_qstart = smte_start;
	ifq_init_maxlen(&ifp->if_snd, SMTE_NTXDESC - 1);
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = smte_mii_readreg;
	sc->sc_mii.mii_writereg = smte_mii_writereg;
	sc->sc_mii.mii_statchg = smte_mii_statchg;

	ifmedia_init(&sc->sc_media, 0, smte_media_change, smte_media_status);

	mii_attach(self, &sc->sc_mii, 0xffffffff, sc->sc_phyloc,
	    MII_OFFSET_ANY, MIIF_NOISOLATE | mii_flags);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc->sc_dev.dv_xname);
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);
	return;

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
}

void
smte_mdio_bus_init(struct smte_softc *sc)
{
	uint32_t *reset_gpio;
	int reset_gpiolen;
	int node;

	node = OF_getnodebyname(sc->sc_node, "mdio-bus");
	if (!node)
		return;
	
	reset_gpiolen = OF_getproplen(node, "reset-gpios");
	if (reset_gpiolen <= 0)
		return;

	reset_gpio = malloc(reset_gpiolen, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "reset-gpios", reset_gpio, reset_gpiolen);
	gpio_controller_config_pin(reset_gpio, GPIO_CONFIG_OUTPUT);
	gpio_controller_set_pin(reset_gpio, 0);
	free(reset_gpio, M_TEMP, reset_gpiolen);
}

void
smte_init(struct smte_softc *sc)
{
	uint32_t rx_delay, tx_delay;
	uint32_t val;

	pinctrl_byname(sc->sc_node, "default");

	/* Enable clock. */
	clock_enable(sc->sc_node, NULL);
	reset_deassert(sc->sc_node, NULL);

	smte_mdio_bus_init(sc);

	smte_stop_dma(sc);

	val = regmap_read_4(sc->sc_apmu,
	    sc->sc_apmu_offset + APMU_EMAC_CLK_RST_CTRL);
	val |= APMU_EMAC_AXI_MST_ID;
	regmap_write_4(sc->sc_apmu,
	    sc->sc_apmu_offset + APMU_EMAC_CLK_RST_CTRL, val);

	/* Convert delays from ps to 15.6ps steps. */
	rx_delay = (sc->sc_rx_delay * 10 + 78) / 156;
	tx_delay = (sc->sc_tx_delay * 10 + 78) / 156;

	/* Program internal delays. */
	val = regmap_read_4(sc->sc_apmu,
	    sc->sc_apmu_offset + APMU_EMAC_RGMII_DLINE);
	val = APMU_EMAC_RGMII_DLINE_RX_EN | APMU_EMAC_RGMII_DLINE_TX_EN;
	val &= ~APMU_EMAC_RGMII_DLINE_RX_STEP_MASK;
	val &= ~APMU_EMAC_RGMII_DLINE_RX_DELAY_MASK;
	val |= APMU_EMAC_RGMII_DLINE_RX_STEP_15P6;
	val |= rx_delay << APMU_EMAC_RGMII_DLINE_RX_DELAY_SHIFT;
	val &= ~APMU_EMAC_RGMII_DLINE_TX_STEP_MASK;
	val &= ~APMU_EMAC_RGMII_DLINE_TX_DELAY_MASK;
	val |= APMU_EMAC_RGMII_DLINE_TX_STEP_15P6;
	val |= tx_delay << APMU_EMAC_RGMII_DLINE_TX_DELAY_SHIFT;
	regmap_write_4(sc->sc_apmu,
	    sc->sc_apmu_offset + APMU_EMAC_RGMII_DLINE, val);

	/* Set up normal address filtering. */
	smte_lladdr_write(sc);
	HWRITE4(sc, MAC_ADDR_CTRL, MAC_ADDR_CTRL_MAC_ADDR1_ENABLE);
	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE1, 0);
	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE2, 0);
	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE3, 0);
	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE4, 0);

	HWRITE4(sc, MAC_TRANSMIT_FIFO_ALMOST_FULL, 0x1f8);
	HWRITE4(sc, MAC_TRANSMIT_PACKET_START_THRESHOLD, 1518);
	HWRITE4(sc, MAC_RECEIVE_PACKET_START_THRESHOLD, 12);

	HWRITE4(sc, MAC_MAXIMUM_FRAME_SIZE, ETHER_MAX_LEN);
	HWRITE4(sc, MAC_TRANSMIT_JABBER_SIZE, ETHER_MAX_DIX_LEN);
	HWRITE4(sc, MAC_RECEIVE_JABBER_SIZE, ETHER_MAX_DIX_LEN);

	/* Enable receive interrupt coalescion. */
	HWRITE4(sc, DMA_RECEIVE_IRQ_MITIGATION,
	    (64 << DMA_RECEIVE_IRQ_MITIGATION_FRAME_COUNTER_SHIFT) |
	    ((600 * 312) << DMA_RECEIVE_IRQ_MITIGATION_TIMEOUT_COUNTER_SHIFT) |
	    DMA_RECEIVE_IRQ_MITIGATION_MITIGATION_ENABLE);

	/* Reset DMA controller. */
	HWRITE4(sc, DMA_CONFIG, DMA_CONFIG_SOFTWARE_RESET);
	delay(10000);
	HWRITE4(sc, DMA_CONFIG, 0);
	delay(10000);

	HWRITE4(sc, DMA_CONFIG, DMA_CONFIG_STRICT_BURST |
	    DMA_CONFIG_DMA_64BIT_MODE | DMA_CONFIG_BURST_LENGTH_16);
}

void
smte_lladdr_write(struct smte_softc *sc)
{
	HWRITE4(sc, MAC_ADDR1_HI,
	    sc->sc_lladdr[1] << 8 | sc->sc_lladdr[0] << 0);
	HWRITE4(sc, MAC_ADDR1_ME,
	    sc->sc_lladdr[3] << 8 | sc->sc_lladdr[2] << 0);
	HWRITE4(sc, MAC_ADDR1_LO,
	    sc->sc_lladdr[5] << 8 | sc->sc_lladdr[4] << 0);
}

void
smte_start(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	struct smte_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int error, idx, left, used;

	if (!sc->sc_link) {
		ifq_purge(ifq);
		return;
	}

	idx = sc->sc_tx_prod;
	left = sc->sc_tx_cons;
	if (left <= idx)
		left += SMTE_NTXDESC;
	left -= idx;
	used = 0;

	for (;;) {
		if (used + SMTE_NTXSEGS + 1 > left) {
			ifq_set_oactive(ifq);
			break;
		}

		m = ifq_dequeue(ifq);
		if (m == NULL)
			break;

		error = smte_encap(sc, m, &idx, &used);
		if (error == EFBIG) {
			m_freem(m); /* give up: drop it */
			ifp->if_oerrors++;
			continue;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	}

	if (used > 0) {
		sc->sc_tx_prod = idx;

		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = 5;

		HWRITE4(sc, DMA_TRANSMIT_POLL_DEMAND, 1);
	}
}

int
smte_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct smte_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)addr;
	int error = 0, s;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				smte_up(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				smte_down(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCGIFRXR:
		error = if_rxr_ioctl((struct if_rxrinfo *)ifr->ifr_data,
		    NULL, MCLBYTES, &sc->sc_rx_ring);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, addr);
		break;
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			smte_iff(sc);
		error = 0;
	}

	splx(s);
	return error;
}

int
smte_media_change(struct ifnet *ifp)
{
	struct smte_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys))
		mii_mediachg(&sc->sc_mii);

	return 0;
}

void
smte_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct smte_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys)) {
		mii_pollstat(&sc->sc_mii);
		ifmr->ifm_active = sc->sc_mii.mii_media_active;
		ifmr->ifm_status = sc->sc_mii.mii_media_status;
	}
}

int
smte_mii_readreg(struct device *self, int phy, int reg)
{
	struct smte_softc *sc = (void *)self;
	uint32_t ctrl;
	int timo;

	HWRITE4(sc, MAC_MDIO_DATA, 0);
	HWRITE4(sc, MAC_MDIO_CTRL, MAC_MDIO_CTRL_START_MDIO_TRANS |
	    MAC_MDIO_CTRL_MDIO_READ_WRITE |
	    reg << MAC_MDIO_CTRL_REGISTER_ADDRESS_SHIFT |
	    phy << MAC_MDIO_CTRL_PHY_ADDRESS_SHIFT);

	for (timo = 100; timo > 0; timo--) {
		ctrl = HREAD4(sc, MAC_MDIO_CTRL);
		if ((ctrl & MAC_MDIO_CTRL_START_MDIO_TRANS) == 0)
			return HREAD4(sc, MAC_MDIO_DATA);
		delay(100);
	}

	printf("%s: MII read timeout\n", sc->sc_dev.dv_xname);
	return 0;
}

void
smte_mii_writereg(struct device *self, int phy, int reg, int val)
{
	struct smte_softc *sc = (void *)self;
	uint32_t ctrl;
	int timo;

	HWRITE4(sc, MAC_MDIO_DATA, val);
	HWRITE4(sc, MAC_MDIO_CTRL, MAC_MDIO_CTRL_START_MDIO_TRANS |
	    reg << MAC_MDIO_CTRL_REGISTER_ADDRESS_SHIFT |
	    phy << MAC_MDIO_CTRL_PHY_ADDRESS_SHIFT);

	for (timo = 100; timo > 0; timo--) {
		ctrl = HREAD4(sc, MAC_MDIO_CTRL);
		if ((ctrl & MAC_MDIO_CTRL_START_MDIO_TRANS) == 0)
			return;
		delay(100);
	}

	printf("%s: MII write timeout\n", sc->sc_dev.dv_xname);
}

void
smte_mii_statchg(struct device *self)
{
	struct smte_softc *sc = (void *)self;
	uint32_t ctrl;

	ctrl = HREAD4(sc, MAC_GLOBAL_CTRL);
	ctrl &= ~MAC_GLOBAL_CTRL_SPEED_MASK;

	switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
	case IFM_1000_SX:
	case IFM_1000_LX:
	case IFM_1000_CX:
	case IFM_1000_T:
		ctrl |= MAC_GLOBAL_CTRL_SPEED_1000;
		sc->sc_link = 1;
		break;
	case IFM_100_TX:
		ctrl |= MAC_GLOBAL_CTRL_SPEED_100;
		sc->sc_link = 1;
		break;
	case IFM_10_T:
		ctrl |= MAC_GLOBAL_CTRL_SPEED_10;
		sc->sc_link = 1;
		break;
	default:
		sc->sc_link = 0;
		return;
	}

	if (sc->sc_link == 0)
		return;

	if ((sc->sc_mii.mii_media_active & IFM_GMASK) == IFM_FDX)
		ctrl |= MAC_GLOBAL_CTRL_DUPLEX_MODE;
	else
		ctrl &= ~MAC_GLOBAL_CTRL_DUPLEX_MODE;

	HWRITE4(sc, MAC_GLOBAL_CTRL, ctrl);
}

void
smte_tick(void *arg)
{
	struct smte_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add_sec(&sc->sc_tick, 1);
}

int
smte_intr(void *arg)
{
	struct smte_softc *sc = arg;
	uint32_t stat;

	stat = HREAD4(sc, DMA_STATUS_IRQ);

	if (stat & DMA_STATUS_IRQ_RX_TRANSFER_DONE ||
	    stat & DMA_STATUS_IRQ_RX_MISSED_FRAME)
		smte_rx_proc(sc);

	if (stat & DMA_STATUS_IRQ_TX_TRANSFER_DONE)
		smte_tx_proc(sc);

	HWRITE4(sc, DMA_STATUS_IRQ, stat);
	return 1;
}

void
smte_tx_proc(struct smte_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct smte_desc *txd;
	struct smte_buf *txb;
	int idx, txfree;

	bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_txring), 0,
	    SMTE_DMA_LEN(sc->sc_txring),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	txfree = 0;
	while (sc->sc_tx_cons != sc->sc_tx_prod) {
		idx = sc->sc_tx_cons;
		KASSERT(idx < SMTE_NTXDESC);

		txd = &sc->sc_txdesc[idx];
		if (txd->sd_desc0 & TX_DESC0_OWN)
			break;

		txb = &sc->sc_txbuf[idx];
		if (txb->sb_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, txb->sb_map, 0,
			    txb->sb_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, txb->sb_map);

			m_freem(txb->sb_m);
			txb->sb_m = NULL;
		}

		txfree++;

		if (sc->sc_tx_cons == (SMTE_NTXDESC - 1))
			sc->sc_tx_cons = 0;
		else
			sc->sc_tx_cons++;
	}

	if (sc->sc_tx_cons == sc->sc_tx_prod)
		ifp->if_timer = 0;

	if (txfree) {
		if (ifq_is_oactive(&ifp->if_snd))
			ifq_restart(&ifp->if_snd);
	}
}

void
smte_rx_proc(struct smte_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct smte_desc *rxd;
	struct smte_buf *rxb;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	int idx, len, cnt, put;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_rxring), 0,
	    SMTE_DMA_LEN(sc->sc_rxring),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	cnt = if_rxr_inuse(&sc->sc_rx_ring);
	put = 0;
	while (put < cnt) {
		idx = sc->sc_rx_cons;
		KASSERT(idx < SMTE_NRXDESC);

		rxd = &sc->sc_rxdesc[idx];
		if (rxd->sd_desc0 & RX_DESC0_OWN)
			break;

		len = rxd->sd_desc0 & RX_DESC0_FRAME_PACKET_LENGTH_MASK;
		rxb = &sc->sc_rxbuf[idx];
		KASSERT(rxb->sb_m != NULL);

		bus_dmamap_sync(sc->sc_dmat, rxb->sb_map, 0,
		    len, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, rxb->sb_map);

		m = rxb->sb_m;
		rxb->sb_m = NULL;

		if (len < ETHER_CRC_LEN || len > m->m_len ||
		    rxd->sd_desc0 & RX_DESC0_FRAME_RUNT ||
		    rxd->sd_desc0 & RX_DESC0_FRAME_CRC_ERR ||
		    rxd->sd_desc0 & RX_DESC0_FRAME_MAX_LEN_ERR ||
		    rxd->sd_desc0 & RX_DESC0_FRAME_JABBER_ERR ||
		    rxd->sd_desc0 & RX_DESC0_FRAME_LENGTH_ERR) {
			ifp->if_ierrors++;
			m_freem(m);
		} else {
			/* Strip off CRC. */
			len -= ETHER_CRC_LEN;

			m->m_pkthdr.len = m->m_len = len;
			ml_enqueue(&ml, m);
		}

		put++;
		if (sc->sc_rx_cons == (SMTE_NRXDESC - 1))
			sc->sc_rx_cons = 0;
		else
			sc->sc_rx_cons++;
	}

	if_rxr_put(&sc->sc_rx_ring, put);
	if (ifiq_input(&ifp->if_rcv, &ml))
		if_rxr_livelocked(&sc->sc_rx_ring);

	smte_fill_rx_ring(sc);

	bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_rxring), 0,
	    SMTE_DMA_LEN(sc->sc_rxring),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

void
smte_up(struct smte_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct smte_buf *txb, *rxb;
	int i;

	/* Allocate Tx descriptor ring. */
	sc->sc_txring = smte_dmamem_alloc(sc,
	    SMTE_NTXDESC * sizeof(struct smte_desc), 8);
	sc->sc_txdesc = SMTE_DMA_KVA(sc->sc_txring);

	sc->sc_txbuf = malloc(sizeof(struct smte_buf) * SMTE_NTXDESC,
	    M_DEVBUF, M_WAITOK);
	for (i = 0; i < SMTE_NTXDESC; i++) {
		txb = &sc->sc_txbuf[i];
		bus_dmamap_create(sc->sc_dmat, MCLBYTES, SMTE_NTXSEGS,
		    MCLBYTES, 0, BUS_DMA_WAITOK, &txb->sb_map);
		txb->sb_m = NULL;
	}

	bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_txring),
	    0, SMTE_DMA_LEN(sc->sc_txring), BUS_DMASYNC_PREWRITE);

	sc->sc_tx_prod = sc->sc_tx_cons = 0;

	HWRITE4(sc, DMA_TRANSMIT_BASE_ADDRESS, SMTE_DMA_DVA(sc->sc_txring));

	/* Allocate Rx descriptor ring. */
	sc->sc_rxring = smte_dmamem_alloc(sc,
	    SMTE_NRXDESC * sizeof(struct smte_desc), 8);
	sc->sc_rxdesc = SMTE_DMA_KVA(sc->sc_rxring);

	sc->sc_rxbuf = malloc(sizeof(struct smte_buf) * SMTE_NRXDESC,
	    M_DEVBUF, M_WAITOK);

	for (i = 0; i < SMTE_NRXDESC; i++) {
		rxb = &sc->sc_rxbuf[i];
		bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_WAITOK, &rxb->sb_map);
		rxb->sb_m = NULL;
	}

	if_rxr_init(&sc->sc_rx_ring, 2, SMTE_NRXDESC);

	sc->sc_rx_prod = sc->sc_rx_cons = 0;
	smte_fill_rx_ring(sc);

	bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_rxring),
	    0, SMTE_DMA_LEN(sc->sc_rxring),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	HWRITE4(sc, DMA_RECEIVE_BASE_ADDRESS, SMTE_DMA_DVA(sc->sc_rxring));

	/* Configure media. */
	if (LIST_FIRST(&sc->sc_mii.mii_phys))
		mii_mediachg(&sc->sc_mii);

	/* Program promiscuous mode and multicast filters. */
	smte_iff(sc);

	/*
	 * Enable completion interrupts.  Also enable the receive
	 * missed frame interrupt to triggera receive ring refills.
	 */
	HSET4(sc, DMA_INTR_ENABLE, DMA_INTR_ENABLE_TX_TRANSFER_DONE |
	    DMA_INTR_ENABLE_RX_TRANSFER_DONE);
	HSET4(sc, DMA_INTR_ENABLE, DMA_INTR_ENABLE_RX_MISSED_FRAME);

	HCLR4(sc, MAC_TRANSMIT_CTRL, MAC_TRANSMIT_CTRL_IFG_LEN_MASK);
	HSET4(sc, MAC_TRANSMIT_CTRL,
	    MAC_TRANSMIT_CTRL_TX_ENABLE | MAC_TRANSMIT_CTRL_TX_AUTO_RETRY);

	HSET4(sc, MAC_RECEIVE_CTRL,
	    MAC_RECEIVE_CTRL_RX_ENABLE | MAC_RECEIVE_CTRL_STORE_FORWARD);

	HWRITE4(sc, DMA_TRANSMIT_AUTO_POLL_COUNTER, 0);
	HSET4(sc, DMA_CTRL, DMA_CTRL_START_STOP_TX_DMA);
	HSET4(sc, DMA_CTRL, DMA_CTRL_START_STOP_RX_DMA);

	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	timeout_add_sec(&sc->sc_tick, 1);
}

void
smte_down(struct smte_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct smte_buf *txb, *rxb;
	int i;

	timeout_del(&sc->sc_tick);

	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);
	ifp->if_timer = 0;

	smte_stop_dma(sc);

	intr_barrier(sc->sc_ih);
	ifq_barrier(&ifp->if_snd);

	for (i = 0; i < SMTE_NTXDESC; i++) {
		txb = &sc->sc_txbuf[i];
		if (txb->sb_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, txb->sb_map, 0,
			    txb->sb_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, txb->sb_map);
			m_freem(txb->sb_m);
		}
		bus_dmamap_destroy(sc->sc_dmat, txb->sb_map);
	}

	smte_dmamem_free(sc, sc->sc_txring);
	free(sc->sc_txbuf, M_DEVBUF, 0);

	for (i = 0; i < SMTE_NRXDESC; i++) {
		rxb = &sc->sc_rxbuf[i];
		if (rxb->sb_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, rxb->sb_map, 0,
			    rxb->sb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, rxb->sb_map);
			m_freem(rxb->sb_m);
		}
		bus_dmamap_destroy(sc->sc_dmat, rxb->sb_map);
	}

	smte_dmamem_free(sc, sc->sc_rxring);
	free(sc->sc_rxbuf, M_DEVBUF, 0);
}

void
smte_iff(struct smte_softc *sc)
{
	struct arpcom *ac = &sc->sc_ac;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t crc, hashbit, hashreg;
	uint16_t hash[4];
	uint32_t val;

	smte_lladdr_write(sc);

	val = MAC_ADDR_CTRL_MAC_ADDR1_ENABLE;

	ifp->if_flags &= ~IFF_ALLMULTI;
	memset(hash, 0, sizeof(hash));
	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		val |= MAC_ADDR_CTRL_PROMISCUOUS_MODE;
	} else if (ac->ac_multirangecnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		memset(hash, 0xff, sizeof(hash));
	} else {
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			crc = ether_crc32_be(enm->enm_addrlo,
			    ETHER_ADDR_LEN) >> 26;

			hashreg = (crc >> 4);
			hashbit = (crc & 0xf);
			hash[hashreg] |= (1 << hashbit);

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE1, hash[0]);
	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE2, hash[1]);
	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE3, hash[2]);
	HWRITE4(sc, MAC_MULTICAST_HASH_TABLE4, hash[3]);
	HWRITE4(sc, MAC_ADDR_CTRL, val);
}

int
smte_encap(struct smte_softc *sc, struct mbuf *m, int *idx, int *used)
{
	struct smte_desc *txd, *txd_start;
	bus_dmamap_t map;
	int cur, frag, i;

	cur = frag = *idx;
	map = sc->sc_txbuf[cur].sb_map;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT)) {
		if (m_defrag(m, M_DONTWAIT))
			return EFBIG;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT))
			return EFBIG;
	}

	/* Sync the DMA map. */
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	txd = txd_start = &sc->sc_txdesc[frag];
	for (i = 0; i < map->dm_nsegs; i++) {
		txd->sd_addr1 = map->dm_segs[i].ds_addr;
		txd->sd_desc1 = map->dm_segs[i].ds_len;
		if (frag == (SMTE_NRXDESC - 1))
			txd->sd_desc1 |= TX_DESC1_END_RING;
		if (i == 0)
			txd->sd_desc1 |= TX_DESC1_FIRST_SEGMENT;
		if (i == (map->dm_nsegs - 1))
			txd->sd_desc1 |= TX_DESC1_LAST_SEGMENT |
			    TX_DESC1_INTERRUPT_ON_COMPLETION;
		if (i != 0)
			txd->sd_desc0 = TX_DESC0_OWN;

		bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_txring),
		    frag * sizeof(*txd), sizeof(*txd), BUS_DMASYNC_PREWRITE);

		cur = frag;
		if (frag == (SMTE_NTXDESC - 1)) {
			txd = &sc->sc_txdesc[0];
			frag = 0;
		} else {
			txd++;
			frag++;
		}
		KASSERT(frag != sc->sc_tx_cons);
	}

	txd_start->sd_desc0 = TX_DESC0_OWN;

	bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_txring),
	    *idx * sizeof(*txd), sizeof(*txd), BUS_DMASYNC_PREWRITE);

	KASSERT(sc->sc_txbuf[cur].sb_m == NULL);
	sc->sc_txbuf[*idx].sb_map = sc->sc_txbuf[cur].sb_map;
	sc->sc_txbuf[cur].sb_map = map;
	sc->sc_txbuf[cur].sb_m = m;

	*idx = frag;
	*used += map->dm_nsegs;

	return 0;
}

void
smte_stop_dma(struct smte_softc *sc)
{
	HWRITE4(sc, MAC_INTR_ENABLE, 0);
	HWRITE4(sc, DMA_INTR_ENABLE, 0);

	HWRITE4(sc, MAC_TRANSMIT_CTRL, 0);
	HWRITE4(sc, MAC_RECEIVE_CTRL, 0);
	HWRITE4(sc, DMA_CTRL, 0);
}

struct smte_dmamem *
smte_dmamem_alloc(struct smte_softc *sc, bus_size_t size, bus_size_t align)
{
	struct smte_dmamem *sdm;
	int nsegs;

	sdm = malloc(sizeof(*sdm), M_DEVBUF, M_WAITOK | M_ZERO);
	sdm->sdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &sdm->sdm_map) != 0)
		goto sdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, align, 0, &sdm->sdm_seg, 1,
	    &nsegs, BUS_DMA_WAITOK) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &sdm->sdm_seg, nsegs, size,
	    &sdm->sdm_kva, BUS_DMA_WAITOK | BUS_DMA_COHERENT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, sdm->sdm_map, sdm->sdm_kva, size,
	    NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	memset(sdm->sdm_kva, 0, size);

	return sdm;

unmap:
	bus_dmamem_unmap(sc->sc_dmat, sdm->sdm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &sdm->sdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, sdm->sdm_map);
sdmfree:
	free(sdm, M_DEVBUF, 0);

	return NULL;
}

void
smte_dmamem_free(struct smte_softc *sc, struct smte_dmamem *sdm)
{
	bus_dmamem_unmap(sc->sc_dmat, sdm->sdm_kva, sdm->sdm_size);
	bus_dmamem_free(sc->sc_dmat, &sdm->sdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, sdm->sdm_map);
	free(sdm, M_DEVBUF, 0);
}

struct mbuf *
smte_alloc_mbuf(struct smte_softc *sc, bus_dmamap_t map)
{
	struct mbuf *m = NULL;

	m = MCLGETL(NULL, M_DONTWAIT, MCLBYTES);
	if (m == NULL)
		return NULL;
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, ETHER_ALIGN);

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT) != 0) {
		printf("%s: could not load mbuf DMA map", DEVNAME(sc));
		m_freem(m);
		return NULL;
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0,
	    m->m_pkthdr.len, BUS_DMASYNC_PREREAD);

	return m;
}

void
smte_fill_rx_ring(struct smte_softc *sc)
{
	struct smte_desc *rxd;
	struct smte_buf *rxb;
	u_int slots;

	for (slots = if_rxr_get(&sc->sc_rx_ring, SMTE_NRXDESC);
	    slots > 0; slots--) {
		rxb = &sc->sc_rxbuf[sc->sc_rx_prod];
		rxb->sb_m = smte_alloc_mbuf(sc, rxb->sb_map);
		if (rxb->sb_m == NULL)
			break;

		rxd = &sc->sc_rxdesc[sc->sc_rx_prod];
		rxd->sd_desc1 = rxb->sb_map->dm_segs[0].ds_len;
		rxd->sd_addr1 = rxb->sb_map->dm_segs[0].ds_addr;
		if (sc->sc_rx_prod == (SMTE_NRXDESC - 1))
			rxd->sd_desc1 |= RX_DESC1_END_RING;

		bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_rxring),
		    sc->sc_rx_prod * sizeof(*rxd), sizeof(*rxd),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		rxd->sd_desc0 = RX_DESC0_OWN;

		bus_dmamap_sync(sc->sc_dmat, SMTE_DMA_MAP(sc->sc_rxring),
		    sc->sc_rx_prod * sizeof(*rxd), sizeof(*rxd),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		if (sc->sc_rx_prod == (SMTE_NRXDESC - 1))
			sc->sc_rx_prod = 0;
		else
			sc->sc_rx_prod++;
	}
	if_rxr_put(&sc->sc_rx_ring, slots);
}
