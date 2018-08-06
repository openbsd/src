/* $OpenBSD: if_fec.c,v 1.6 2018/08/06 10:52:30 patrick Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <sys/mbuf.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include "bpfilter.h"

#include <net/if.h>
#include <net/if_media.h>
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

/* configuration registers */
#define ENET_EIR		0x004
#define ENET_EIMR		0x008
#define ENET_RDAR		0x010
#define ENET_TDAR		0x014
#define ENET_ECR		0x024
#define ENET_MMFR		0x040
#define ENET_MSCR		0x044
#define ENET_MIBC		0x064
#define ENET_RCR		0x084
#define ENET_TCR		0x0C4
#define ENET_PALR		0x0E4
#define ENET_PAUR		0x0E8
#define ENET_OPD		0x0EC
#define ENET_IAUR		0x118
#define ENET_IALR		0x11C
#define ENET_GAUR		0x120
#define ENET_GALR		0x124
#define ENET_TFWR		0x144
#define ENET_RDSR		0x180
#define ENET_TDSR		0x184
#define ENET_MRBR		0x188
#define ENET_RSFL		0x190
#define ENET_RSEM		0x194
#define ENET_RAEM		0x198
#define ENET_RAFL		0x19C
#define ENET_TSEM		0x1A0
#define ENET_TAEM		0x1A4
#define ENET_TAFL		0x1A8
#define ENET_TIPG		0x1AC
#define ENET_FTRL		0x1B0
#define ENET_TACC		0x1C0
#define ENET_RACC		0x1C4

#define ENET_RDAR_RDAR		(1 << 24)
#define ENET_TDAR_TDAR		(1 << 24)
#define ENET_ECR_RESET		(1 << 0)
#define ENET_ECR_ETHEREN	(1 << 1)
#define ENET_ECR_EN1588		(1 << 4)
#define ENET_ECR_SPEED		(1 << 5)
#define ENET_ECR_DBSWP		(1 << 8)
#define ENET_MMFR_TA		(2 << 16)
#define ENET_MMFR_RA_SHIFT	18
#define ENET_MMFR_PA_SHIFT	23
#define ENET_MMFR_OP_WR		(1 << 28)
#define ENET_MMFR_OP_RD		(2 << 28)
#define ENET_MMFR_ST		(1 << 30)
#define ENET_RCR_MII_MODE	(1 << 2)
#define ENET_RCR_PROM		(1 << 3)
#define ENET_RCR_FCE		(1 << 5)
#define ENET_RCR_RGMII_MODE	(1 << 6)
#define ENET_RCR_RMII_10T	(1 << 9)
#define ENET_RCR_MAX_FL(x)	(((x) & 0x3fff) << 16)
#define ENET_TCR_FDEN		(1 << 2)
#define ENET_EIR_MII		(1 << 23)
#define ENET_EIR_RXF		(1 << 25)
#define ENET_EIR_TXF		(1 << 27)
#define ENET_TFWR_STRFWD	(1 << 8)

/* statistics counters */

/* 1588 control */
#define ENET_ATCR		0x400
#define ENET_ATVR		0x404
#define ENET_ATOFF		0x408
#define ENET_ATPER		0x40C
#define ENET_ATCOR		0x410
#define ENET_ATINC		0x414
#define ENET_ATSTMP		0x418

/* capture / compare block */
#define ENET_TGSR		0x604
#define ENET_TCSR0		0x608
#define ENET_TCCR0		0x60C
#define ENET_TCSR1		0x610
#define ENET_TCCR1		0x614
#define ENET_TCSR2		0x618
#define ENET_TCCR2		0x61C
#define ENET_TCSR3		0x620
#define ENET_TCCR3		0x624

#define ENET_MII_CLK		2500000
#define ENET_ALIGNMENT		16

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

/* what should we use? */
#define ENET_MAX_TXD		32
#define ENET_MAX_RXD		32

#define ENET_MAX_PKT_SIZE	1536

#define ENET_ROUNDUP(size, unit) (((size) + (unit) - 1) & ~((unit) - 1))

/* buffer descriptor status bits */
#define ENET_RXD_EMPTY		(1 << 15)
#define ENET_RXD_WRAP		(1 << 13)
#define ENET_RXD_LAST		(1 << 11)
#define ENET_RXD_MISS		(1 << 8)
#define ENET_RXD_BC		(1 << 7)
#define ENET_RXD_MC		(1 << 6)
#define ENET_RXD_LG		(1 << 5)
#define ENET_RXD_NO		(1 << 4)
#define ENET_RXD_CR		(1 << 2)
#define ENET_RXD_OV		(1 << 1)
#define ENET_RXD_TR		(1 << 0)

#define ENET_TXD_READY		(1 << 15)
#define ENET_TXD_WRAP		(1 << 13)
#define ENET_TXD_LAST		(1 << 11)
#define ENET_TXD_TC		(1 << 10)
#define ENET_TXD_ABC		(1 << 9)
#define ENET_TXD_STATUS_MASK	0x3ff

#ifdef ENET_ENHANCED_BD
/* enhanced */
#define ENET_RXD_INT		(1 << 23)

#define ENET_TXD_INT		(1 << 30)
#endif

/*
 * Bus dma allocation structure used by
 * fec_dma_malloc and fec_dma_free.
 */
struct fec_dma_alloc {
	bus_addr_t		dma_paddr;
	caddr_t			dma_vaddr;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;
	bus_size_t		dma_size;
	int			dma_nseg;
};

struct fec_buf_desc {
	uint16_t data_length;		/* payload's length in bytes */
	uint16_t status;		/* BD's status (see datasheet) */
	uint32_t data_pointer;		/* payload's buffer address */
#ifdef ENET_ENHANCED_BD
	uint32_t enhanced_status;	/* enhanced status with IEEE 1588 */
	uint32_t reserved0;		/* reserved */
	uint32_t update_done;		/* buffer descriptor update done */
	uint32_t timestamp;		/* IEEE 1588 timestamp */
	uint32_t reserved1[2];		/* reserved */
#endif
};

struct fec_buffer {
	uint8_t data[ENET_MAX_PKT_SIZE];
};

struct fec_softc {
	struct device		sc_dev;
	struct arpcom		sc_ac;
	struct mii_data		sc_mii;
	int			sc_node;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void			*sc_ih[3]; /* Interrupt handler */
	bus_dma_tag_t		sc_dma_tag;
	struct fec_dma_alloc	txdma;		/* bus_dma glue for tx desc */
	struct fec_buf_desc	*tx_desc_base;
	struct fec_dma_alloc	rxdma;		/* bus_dma glue for rx desc */
	struct fec_buf_desc	*rx_desc_base;
	struct fec_dma_alloc	tbdma;		/* bus_dma glue for packets */
	struct fec_buffer	*tx_buffer_base;
	struct fec_dma_alloc	rbdma;		/* bus_dma glue for packets */
	struct fec_buffer	*rx_buffer_base;
	int			cur_tx;
	int			cur_rx;
	struct timeout		sc_tick;
	uint32_t		sc_phy_speed;
};

struct fec_softc *fec_sc;

int fec_match(struct device *, void *, void *);
void fec_attach(struct device *, struct device *, void *);
void fec_phy_init(struct fec_softc *, struct mii_softc *);
int fec_ioctl(struct ifnet *, u_long, caddr_t);
void fec_start(struct ifnet *);
int fec_encap(struct fec_softc *, struct mbuf *);
void fec_init_txd(struct fec_softc *);
void fec_init_rxd(struct fec_softc *);
void fec_init(struct fec_softc *);
void fec_stop(struct fec_softc *);
void fec_iff(struct fec_softc *);
struct mbuf * fec_newbuf(void);
int fec_intr(void *);
void fec_recv(struct fec_softc *);
void fec_tick(void *);
int fec_miibus_readreg(struct device *, int, int);
void fec_miibus_writereg(struct device *, int, int, int);
void fec_miibus_statchg(struct device *);
int fec_ifmedia_upd(struct ifnet *);
void fec_ifmedia_sts(struct ifnet *, struct ifmediareq *);
int fec_dma_malloc(struct fec_softc *, bus_size_t, struct fec_dma_alloc *);
void fec_dma_free(struct fec_softc *, struct fec_dma_alloc *);

struct cfattach fec_ca = {
	sizeof (struct fec_softc), fec_match, fec_attach
};

struct cfdriver fec_cd = {
	NULL, "fec", DV_IFNET
};

int
fec_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "fsl,imx6q-fec") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx6sx-fec") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx8mq-fec"));
}

void
fec_attach(struct device *parent, struct device *self, void *aux)
{
	struct fec_softc *sc = (struct fec_softc *) self;
	struct fdt_attach_args *faa = aux;
	struct mii_data *mii;
	struct mii_softc *child;
	struct ifnet *ifp;
	int tsize, rsize, tbsize, rbsize, s;
	uint32_t phy_reset_gpio[3];
	uint32_t phy_reset_duration;

	if (faa->fa_nreg < 1)
		return;

	sc->sc_node = faa->fa_node;
	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("fec_attach: bus_space_map failed!");

	sc->sc_dma_tag = faa->fa_dmat;

	pinctrl_byname(faa->fa_node, "default");

	/* power it up */
	clock_enable_all(faa->fa_node);

	/* reset PHY */
	if (OF_getpropintarray(faa->fa_node, "phy-reset-gpios", phy_reset_gpio,
	    sizeof(phy_reset_gpio)) == sizeof(phy_reset_gpio)) {
		phy_reset_duration = OF_getpropint(faa->fa_node,
		    "phy-reset-duration", 1);
		if (phy_reset_duration > 1000)
			phy_reset_duration = 1;

		/*
		 * The Linux people really screwed the pooch here.
		 * The Linux kernel always treats the gpio as
		 * active-low, even if it is marked as active-high in
		 * the device tree.  As a result the device tree for
		 * many boards incorrectly marks the gpio as
		 * active-high.  
		 */
		phy_reset_gpio[2] = GPIO_ACTIVE_LOW;
		gpio_controller_config_pin(phy_reset_gpio, GPIO_CONFIG_OUTPUT);

		/*
		 * On some Cubox-i machines we need to hold the PHY in
		 * reset a little bit longer than specified.
		 */
		gpio_controller_set_pin(phy_reset_gpio, 1);
		delay((phy_reset_duration + 1) * 1000);
		gpio_controller_set_pin(phy_reset_gpio, 0);
		delay(1000);
	}
	printf("\n");

	/* Figure out the hardware address. Must happen before reset. */
	OF_getprop(faa->fa_node, "local-mac-address", sc->sc_ac.ac_enaddr,
	    sizeof(sc->sc_ac.ac_enaddr));

	/* reset the controller */
	HSET4(sc, ENET_ECR, ENET_ECR_RESET);
	while (HREAD4(sc, ENET_ECR) & ENET_ECR_ETHEREN)
		continue;

	HWRITE4(sc, ENET_EIMR, 0);
	HWRITE4(sc, ENET_EIR, 0xffffffff);

	sc->sc_ih[0] = fdt_intr_establish_idx(faa->fa_node, 0, IPL_NET,
	    fec_intr, sc, sc->sc_dev.dv_xname);
	sc->sc_ih[1] = fdt_intr_establish_idx(faa->fa_node, 1, IPL_NET,
	    fec_intr, sc, sc->sc_dev.dv_xname);
	sc->sc_ih[2] = fdt_intr_establish_idx(faa->fa_node, 2, IPL_NET,
	    fec_intr, sc, sc->sc_dev.dv_xname);

	tsize = ENET_MAX_TXD * sizeof(struct fec_buf_desc);
	tsize = ENET_ROUNDUP(tsize, PAGE_SIZE);

	if (fec_dma_malloc(sc, tsize, &sc->txdma)) {
		printf("%s: Unable to allocate tx_desc memory\n",
		    sc->sc_dev.dv_xname);
		goto bad;
	}
	sc->tx_desc_base = (struct fec_buf_desc *)sc->txdma.dma_vaddr;

	rsize = ENET_MAX_RXD * sizeof(struct fec_buf_desc);
	rsize = ENET_ROUNDUP(rsize, PAGE_SIZE);

	if (fec_dma_malloc(sc, rsize, &sc->rxdma)) {
		printf("%s: Unable to allocate rx_desc memory\n",
		    sc->sc_dev.dv_xname);
		goto txdma;
	}
	sc->rx_desc_base = (struct fec_buf_desc *)sc->rxdma.dma_vaddr;

	tbsize = ENET_MAX_TXD * ENET_MAX_PKT_SIZE;
	tbsize = ENET_ROUNDUP(tbsize, PAGE_SIZE);

	if (fec_dma_malloc(sc, tbsize, &sc->tbdma)) {
		printf("%s: Unable to allocate tx_buffer memory\n",
		    sc->sc_dev.dv_xname);
		goto rxdma;
	}
	sc->tx_buffer_base = (struct fec_buffer *)sc->tbdma.dma_vaddr;

	rbsize = ENET_MAX_RXD * ENET_MAX_PKT_SIZE;
	rbsize = ENET_ROUNDUP(rbsize, PAGE_SIZE);

	if (fec_dma_malloc(sc, rbsize, &sc->rbdma)) {
		printf("%s: Unable to allocate rx_buffer memory\n",
		    sc->sc_dev.dv_xname);
		goto tbdma;
	}
	sc->rx_buffer_base = (struct fec_buffer *)sc->rbdma.dma_vaddr;

	sc->cur_tx = 0;
	sc->cur_rx = 0;

	s = splnet();

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = fec_ioctl;
	ifp->if_start = fec_start;
	ifp->if_capabilities = IFCAP_VLAN_MTU;

	printf("%s: address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	/*
	 * Initialize the MII clock.  The formula is:
	 *
	 * ENET_MII_CLK = ref_freq / ((phy_speed + 1) x 2)
	 * phy_speed = (((ref_freq / ENET_MII_CLK) / 2) - 1)
	 */
	sc->sc_phy_speed = clock_get_frequency(sc->sc_node, "ipg");
	sc->sc_phy_speed = (sc->sc_phy_speed + (ENET_MII_CLK - 1)) / ENET_MII_CLK;
	sc->sc_phy_speed = (sc->sc_phy_speed / 2) - 1;
	HWRITE4(sc, ENET_MSCR, (sc->sc_phy_speed << 1) | 0x100);

	/* Initialize MII/media info. */
	mii = &sc->sc_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = fec_miibus_readreg;
	mii->mii_writereg = fec_miibus_writereg;
	mii->mii_statchg = fec_miibus_statchg;

	ifmedia_init(&mii->mii_media, 0, fec_ifmedia_upd, fec_ifmedia_sts);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);

	child = LIST_FIRST(&mii->mii_phys);
	if (child)
		fec_phy_init(sc, child);

	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);
	splx(s);

	timeout_set(&sc->sc_tick, fec_tick, sc);

	fec_sc = sc;
	return;

tbdma:
	fec_dma_free(sc, &sc->tbdma);
rxdma:
	fec_dma_free(sc, &sc->rxdma);
txdma:
	fec_dma_free(sc, &sc->txdma);
bad:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
}

void
fec_phy_init(struct fec_softc *sc, struct mii_softc *child)
{
	struct device *dev = (struct device *)sc;
	int phy = child->mii_phy;
	uint32_t reg;

	if (child->mii_oui == MII_OUI_ATHEROS &&
	    child->mii_model == MII_MODEL_ATHEROS_AR8035) {
		/* disable SmartEEE */
		fec_miibus_writereg(dev, phy, 0x0d, 0x0003);
		fec_miibus_writereg(dev, phy, 0x0e, 0x805d);
		fec_miibus_writereg(dev, phy, 0x0d, 0x4003);
		reg = fec_miibus_readreg(dev, phy, 0x0e);
		fec_miibus_writereg(dev, phy, 0x0e, reg & ~0x0100);

		/* enable 125MHz clk output */
		fec_miibus_writereg(dev, phy, 0x0d, 0x0007);
		fec_miibus_writereg(dev, phy, 0x0e, 0x8016);
		fec_miibus_writereg(dev, phy, 0x0d, 0x4007);

		reg = fec_miibus_readreg(dev, phy, 0x0e) & 0xffe3;
		fec_miibus_writereg(dev, phy, 0x0e, reg | 0x18);

		/* tx clock delay */
		fec_miibus_writereg(dev, phy, 0x1d, 0x0005);
		reg = fec_miibus_readreg(dev, phy, 0x1e);
		fec_miibus_writereg(dev, phy, 0x1e, reg | 0x0100);

		PHY_RESET(child);
	}

	if (child->mii_oui == MII_OUI_MICREL &&
	    child->mii_model == MII_MODEL_MICREL_KSZ9021) {
		uint32_t rxc, rxdv, txc, txen;
		uint32_t rxd0, rxd1, rxd2, rxd3;
		uint32_t txd0, txd1, txd2, txd3;
		uint32_t val;

		rxc = OF_getpropint(sc->sc_node, "rxc-skew-ps", 1400) / 200;
		rxdv = OF_getpropint(sc->sc_node, "rxdv-skew-ps", 1400) / 200;
		txc = OF_getpropint(sc->sc_node, "txc-skew-ps", 1400) / 200;
		txen = OF_getpropint(sc->sc_node, "txen-skew-ps", 1400) / 200;
		rxd0 = OF_getpropint(sc->sc_node, "rxd0-skew-ps", 1400) / 200;
		rxd1 = OF_getpropint(sc->sc_node, "rxd1-skew-ps", 1400) / 200;
		rxd2 = OF_getpropint(sc->sc_node, "rxd2-skew-ps", 1400) / 200;
		rxd3 = OF_getpropint(sc->sc_node, "rxd3-skew-ps", 1400) / 200;
		txd0 = OF_getpropint(sc->sc_node, "txd0-skew-ps", 1400) / 200;
		txd1 = OF_getpropint(sc->sc_node, "txd1-skew-ps", 1400) / 200;
		txd2 = OF_getpropint(sc->sc_node, "txd2-skew-ps", 1400) / 200;
		txd3 = OF_getpropint(sc->sc_node, "txd3-skew-ps", 1400) / 200;

		val = ((rxc & 0xf) << 12) | ((rxdv & 0xf) << 8) |
		    ((txc & 0xf) << 4) | ((txen & 0xf) << 0);
		fec_miibus_writereg(dev, phy, 0x0b, 0x8104);
		fec_miibus_writereg(dev, phy, 0x0c, val);

		val = ((rxd3 & 0xf) << 12) | ((rxd2 & 0xf) << 8) |
		    ((rxd1 & 0xf) << 4) | ((rxd0 & 0xf) << 0);
		fec_miibus_writereg(dev, phy, 0x0b, 0x8105);
		fec_miibus_writereg(dev, phy, 0x0c, val);

		val = ((txd3 & 0xf) << 12) | ((txd2 & 0xf) << 8) |
		    ((txd1 & 0xf) << 4) | ((txd0 & 0xf) << 0);
		fec_miibus_writereg(dev, phy, 0x0b, 0x8106);
		fec_miibus_writereg(dev, phy, 0x0c, val);
	}

	if (child->mii_oui == MII_OUI_MICREL &&
	    child->mii_model == MII_MODEL_MICREL_KSZ9031) {
		uint32_t rxc, rxdv, txc, txen;
		uint32_t rxd0, rxd1, rxd2, rxd3;
		uint32_t txd0, txd1, txd2, txd3;
		uint32_t val;

		rxc = OF_getpropint(sc->sc_node, "rxc-skew-ps", 900) / 60;
		rxdv = OF_getpropint(sc->sc_node, "rxdv-skew-ps", 420) / 60;
		txc = OF_getpropint(sc->sc_node, "txc-skew-ps", 900) / 60;
		txen = OF_getpropint(sc->sc_node, "txen-skew-ps", 420) / 60;
		rxd0 = OF_getpropint(sc->sc_node, "rxd0-skew-ps", 420) / 60;
		rxd1 = OF_getpropint(sc->sc_node, "rxd1-skew-ps", 420) / 60;
		rxd2 = OF_getpropint(sc->sc_node, "rxd2-skew-ps", 420) / 60;
		rxd3 = OF_getpropint(sc->sc_node, "rxd3-skew-ps", 420) / 60;
		txd0 = OF_getpropint(sc->sc_node, "txd0-skew-ps", 420) / 60;
		txd1 = OF_getpropint(sc->sc_node, "txd1-skew-ps", 420) / 60;
		txd2 = OF_getpropint(sc->sc_node, "txd2-skew-ps", 420) / 60;
		txd3 = OF_getpropint(sc->sc_node, "txd3-skew-ps", 420) / 60;

		val = ((rxdv & 0xf) << 4) || ((txen & 0xf) << 0);
		fec_miibus_writereg(dev, phy, 0x0d, 0x0002);
		fec_miibus_writereg(dev, phy, 0x0e, 0x0004);
		fec_miibus_writereg(dev, phy, 0x0d, 0x4002);
		fec_miibus_writereg(dev, phy, 0x0e, val);

		val = ((rxd3 & 0xf) << 12) | ((rxd2 & 0xf) << 8) |
		    ((rxd1 & 0xf) << 4) | ((rxd0 & 0xf) << 0);
		fec_miibus_writereg(dev, phy, 0x0d, 0x0002);
		fec_miibus_writereg(dev, phy, 0x0e, 0x0005);
		fec_miibus_writereg(dev, phy, 0x0d, 0x4002);
		fec_miibus_writereg(dev, phy, 0x0e, val);

		val = ((txd3 & 0xf) << 12) | ((txd2 & 0xf) << 8) |
		    ((txd1 & 0xf) << 4) | ((txd0 & 0xf) << 0);
		fec_miibus_writereg(dev, phy, 0x0d, 0x0002);
		fec_miibus_writereg(dev, phy, 0x0e, 0x0006);
		fec_miibus_writereg(dev, phy, 0x0d, 0x4002);
		fec_miibus_writereg(dev, phy, 0x0e, val);

		val = ((txc & 0x1f) << 5) || ((rxc & 0x1f) << 0);
		fec_miibus_writereg(dev, phy, 0x0d, 0x0002);
		fec_miibus_writereg(dev, phy, 0x0e, 0x0008);
		fec_miibus_writereg(dev, phy, 0x0d, 0x4002);
		fec_miibus_writereg(dev, phy, 0x0e, val);
	}
}

void
fec_init_rxd(struct fec_softc *sc)
{
	int i;

	memset(sc->rx_desc_base, 0, ENET_MAX_RXD * sizeof(struct fec_buf_desc));

	for (i = 0; i < ENET_MAX_RXD; i++)
	{
		sc->rx_desc_base[i].status = ENET_RXD_EMPTY;
		sc->rx_desc_base[i].data_pointer = sc->rbdma.dma_paddr + i * ENET_MAX_PKT_SIZE;
#ifdef ENET_ENHANCED_BD
		sc->rx_desc_base[i].enhanced_status = ENET_RXD_INT;
#endif
	}

	sc->rx_desc_base[i - 1].status |= ENET_RXD_WRAP;
}

void
fec_init_txd(struct fec_softc *sc)
{
	int i;

	memset(sc->tx_desc_base, 0, ENET_MAX_TXD * sizeof(struct fec_buf_desc));

	for (i = 0; i < ENET_MAX_TXD; i++)
	{
		sc->tx_desc_base[i].data_pointer = sc->tbdma.dma_paddr + i * ENET_MAX_PKT_SIZE;
	}

	sc->tx_desc_base[i - 1].status |= ENET_TXD_WRAP;
}

void
fec_init(struct fec_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int speed = 0;

	/* reset the controller */
	HSET4(sc, ENET_ECR, ENET_ECR_RESET);
	while (HREAD4(sc, ENET_ECR) & ENET_ECR_ETHEREN)
		continue;

	/* set hw address */
	HWRITE4(sc, ENET_PALR,
	    (sc->sc_ac.ac_enaddr[0] << 24) |
	    (sc->sc_ac.ac_enaddr[1] << 16) |
	    (sc->sc_ac.ac_enaddr[2] << 8) |
	     sc->sc_ac.ac_enaddr[3]);
	HWRITE4(sc, ENET_PAUR,
	    (sc->sc_ac.ac_enaddr[4] << 24) |
	    (sc->sc_ac.ac_enaddr[5] << 16));

	/* clear outstanding interrupts */
	HWRITE4(sc, ENET_EIR, 0xffffffff);

	/* set max receive buffer size, 3-0 bits always zero for alignment */
	HWRITE4(sc, ENET_MRBR, ENET_MAX_PKT_SIZE);

	/* set descriptor */
	HWRITE4(sc, ENET_TDSR, sc->txdma.dma_paddr);
	HWRITE4(sc, ENET_RDSR, sc->rxdma.dma_paddr);

	/* init descriptor */
	fec_init_txd(sc);
	fec_init_rxd(sc);

	/* set it to full-duplex */
	HWRITE4(sc, ENET_TCR, ENET_TCR_FDEN);

	/*
	 * Set max frame length to 1518 or 1522 with VLANs,
	 * pause frames and promisc mode.
	 * XXX: RGMII mode - phy dependant
	 */
	HWRITE4(sc, ENET_RCR,
	    ENET_RCR_MAX_FL(1522) | ENET_RCR_RGMII_MODE | ENET_RCR_MII_MODE |
	    ENET_RCR_FCE);

	HWRITE4(sc, ENET_MSCR, (sc->sc_phy_speed << 1) | 0x100);

	/* RX FIFO treshold and pause */
	HWRITE4(sc, ENET_RSEM, 0x84);
	HWRITE4(sc, ENET_RSFL, 16);
	HWRITE4(sc, ENET_RAEM, 8);
	HWRITE4(sc, ENET_RAFL, 8);
	HWRITE4(sc, ENET_OPD, 0xFFF0);

	/* do store and forward, only i.MX6, needs to be set correctly else */
	HWRITE4(sc, ENET_TFWR, ENET_TFWR_STRFWD);

	/* enable gigabit-ethernet and set it to support little-endian */
	switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
	case IFM_1000_T:  /* Gigabit */
		speed |= ENET_ECR_SPEED;
		break;
	default:
		speed &= ~ENET_ECR_SPEED;
	}
	HWRITE4(sc, ENET_ECR, ENET_ECR_ETHEREN | speed | ENET_ECR_DBSWP);

#ifdef ENET_ENHANCED_BD
	HSET4(sc, ENET_ECR, ENET_ECR_EN1588);
#endif

	/* rx descriptors are ready */
	HWRITE4(sc, ENET_RDAR, ENET_RDAR_RDAR);

	/* program promiscuous mode and multicast filters */
	fec_iff(sc);

	timeout_add_sec(&sc->sc_tick, 1);

	/* Indicate we are up and running. */
	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	/* enable interrupts for tx/rx */
	HWRITE4(sc, ENET_EIMR, ENET_EIR_TXF | ENET_EIR_RXF);

	fec_start(ifp);
}

void
fec_stop(struct fec_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_timer = 0;
	ifq_clr_oactive(&ifp->if_snd);

	timeout_del(&sc->sc_tick);

	/* reset the controller */
	HSET4(sc, ENET_ECR, ENET_ECR_RESET);
	while (HREAD4(sc, ENET_ECR) & ENET_ECR_ETHEREN)
		continue;

	HWRITE4(sc, ENET_MSCR, (sc->sc_phy_speed << 1) | 0x100);
}

void
fec_iff(struct fec_softc *sc)
{
	struct arpcom *ac = &sc->sc_ac;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint64_t ghash = 0, ihash = 0;
	uint32_t h;

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		ihash = 0xffffffffffffffffLLU;
	} else if (ac->ac_multirangecnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		ghash = 0xffffffffffffffffLLU;
	} else {
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			h = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);

			ghash |= 1LLU << (((uint8_t *)&h)[3] >> 2);

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	HWRITE4(sc, ENET_GAUR, (uint32_t)(ghash >> 32));
	HWRITE4(sc, ENET_GALR, (uint32_t)ghash);

	HWRITE4(sc, ENET_IAUR, (uint32_t)(ihash >> 32));
	HWRITE4(sc, ENET_IALR, (uint32_t)ihash);
}

int
fec_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct fec_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			fec_init(sc);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				fec_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				fec_stop(sc);
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
		if (ifp->if_flags & IFF_RUNNING)
			fec_iff(sc);
		error = 0;
	}

	splx(s);
	return(error);
}

void
fec_start(struct ifnet *ifp)
{
	struct fec_softc *sc = ifp->if_softc;
	struct mbuf *m_head = NULL;

	if (ifq_is_oactive(&ifp->if_snd) || !(ifp->if_flags & IFF_RUNNING))
		return;

	for (;;) {
		m_head = ifq_deq_begin(&ifp->if_snd);
		if (m_head == NULL)
			break;

		if (fec_encap(sc, m_head)) {
			ifq_deq_rollback(&ifp->if_snd, m_head);
			ifq_set_oactive(&ifp->if_snd);
			break;
		}

		ifq_deq_commit(&ifp->if_snd, m_head);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif

		m_freem(m_head);
	}
}

int
fec_encap(struct fec_softc *sc, struct mbuf *m)
{
	if (sc->tx_desc_base[sc->cur_tx].status & ENET_TXD_READY) {
		printf("fec: tx queue full!\n");
		return EIO;
	}

	if (m->m_pkthdr.len > ENET_MAX_PKT_SIZE) {
		printf("fec: packet too big\n");
		return EIO;
	}

	/* copy in the actual packet */
	m_copydata(m, 0, m->m_pkthdr.len, (caddr_t)sc->tx_buffer_base[sc->cur_tx].data);

	sc->tx_desc_base[sc->cur_tx].data_length = m->m_pkthdr.len;

	sc->tx_desc_base[sc->cur_tx].status &= ~ENET_TXD_STATUS_MASK;
	sc->tx_desc_base[sc->cur_tx].status |= (ENET_TXD_READY | ENET_TXD_LAST | ENET_TXD_TC);

#ifdef ENET_ENHANCED_BD
	sc->tx_desc_base[sc->cur_tx].enhanced_status = ENET_TXD_INT;
	sc->tx_desc_base[sc->cur_tx].update_done = 0;
#endif

	bus_dmamap_sync(sc->tbdma.dma_tag, sc->tbdma.dma_map,
	    ENET_MAX_PKT_SIZE * sc->cur_tx, ENET_MAX_PKT_SIZE,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map,
	    sizeof(struct fec_buf_desc) * sc->cur_tx,
	    sizeof(struct fec_buf_desc),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);


	/* tx descriptors are ready */
	HWRITE4(sc, ENET_TDAR, ENET_TDAR_TDAR);

	if (sc->tx_desc_base[sc->cur_tx].status & ENET_TXD_WRAP)
		sc->cur_tx = 0;
	else
		sc->cur_tx++;

	return 0;
}

struct mbuf *
fec_newbuf(void)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		return (NULL);
	}

	return (m);
}

/*
 * Established by attachment driver at interrupt priority IPL_NET.
 */
int
fec_intr(void *arg)
{
	struct fec_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	u_int32_t status;

	/* Find out which interrupts are pending. */
	status = HREAD4(sc, ENET_EIR);

	/* Acknowledge the interrupts we are about to handle. */
	HWRITE4(sc, ENET_EIR, status);

	/*
	 * Handle incoming packets.
	 */
	if (ISSET(status, ENET_EIR_RXF)) {
		if (ifp->if_flags & IFF_RUNNING)
			fec_recv(sc);
	}

	/* Try to transmit. */
	if (ifp->if_flags & IFF_RUNNING && !IFQ_IS_EMPTY(&ifp->if_snd))
		fec_start(ifp);

	return 1;
}

void
fec_recv(struct fec_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();

	bus_dmamap_sync(sc->rbdma.dma_tag, sc->rbdma.dma_map,
	    0, sc->rbdma.dma_size,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
	    0, sc->rxdma.dma_size,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	while (!(sc->rx_desc_base[sc->cur_rx].status & ENET_RXD_EMPTY))
	{
		struct mbuf *m;
		m = fec_newbuf();

		if (m == NULL) {
			ifp->if_ierrors++;
			goto done;
		}

		m->m_pkthdr.len = m->m_len = sc->rx_desc_base[sc->cur_rx].data_length;
		m_adj(m, ETHER_ALIGN);

		memcpy(mtod(m, char *), sc->rx_buffer_base[sc->cur_rx].data,
		    sc->rx_desc_base[sc->cur_rx].data_length);

		sc->rx_desc_base[sc->cur_rx].status |= ENET_RXD_EMPTY;
		sc->rx_desc_base[sc->cur_rx].data_length = 0;

		bus_dmamap_sync(sc->rbdma.dma_tag, sc->rbdma.dma_map,
		    ENET_MAX_PKT_SIZE * sc->cur_rx, ENET_MAX_PKT_SIZE,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
		    sizeof(struct fec_buf_desc) * sc->cur_rx,
		    sizeof(struct fec_buf_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		if (sc->rx_desc_base[sc->cur_rx].status & ENET_RXD_WRAP)
			sc->cur_rx = 0;
		else
			sc->cur_rx++;

		ml_enqueue(&ml, m);
	}

done:
	/* rx descriptors are ready */
	HWRITE4(sc, ENET_RDAR, ENET_RDAR_RDAR);

	if_input(ifp, &ml);
}

void
fec_tick(void *arg)
{
	struct fec_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add_sec(&sc->sc_tick, 1);
}

/*
 * MII
 * Interrupts need ENET_ECR_ETHEREN to be set,
 * so we just read the interrupt status registers.
 */
int
fec_miibus_readreg(struct device *dev, int phy, int reg)
{
	int r = 0;
	struct fec_softc *sc = (struct fec_softc *)dev;

	HSET4(sc, ENET_EIR, ENET_EIR_MII);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ENET_MMFR,
	    ENET_MMFR_ST | ENET_MMFR_OP_RD | ENET_MMFR_TA |
	    phy << ENET_MMFR_PA_SHIFT | reg << ENET_MMFR_RA_SHIFT);

	while(!(HREAD4(sc, ENET_EIR) & ENET_EIR_MII));

	r = bus_space_read_4(sc->sc_iot, sc->sc_ioh, ENET_MMFR);

	return (r & 0xffff);
}

void
fec_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct fec_softc *sc = (struct fec_softc *)dev;

	HSET4(sc, ENET_EIR, ENET_EIR_MII);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ENET_MMFR,
	    ENET_MMFR_ST | ENET_MMFR_OP_WR | ENET_MMFR_TA |
	    phy << ENET_MMFR_PA_SHIFT | reg << ENET_MMFR_RA_SHIFT |
	    (val & 0xffff));

	while(!(HREAD4(sc, ENET_EIR) & ENET_EIR_MII));

	return;
}

void
fec_miibus_statchg(struct device *dev)
{
	struct fec_softc *sc = (struct fec_softc *)dev;
	uint32_t ecr, rcr;

	ecr = HREAD4(sc, ENET_ECR) & ~ENET_ECR_SPEED;
	rcr = HREAD4(sc, ENET_RCR) & ~ENET_RCR_RMII_10T;
	switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
	case IFM_1000_T:  /* Gigabit */
		ecr |= ENET_ECR_SPEED;
		break;
	case IFM_100_TX:
		break;
	case IFM_10_T:
		rcr |= ENET_RCR_RMII_10T;
		break;
	}
	HWRITE4(sc, ENET_ECR, ecr);
	HWRITE4(sc, ENET_RCR, rcr);

	return;
}

int
fec_ifmedia_upd(struct ifnet *ifp)
{
	struct fec_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	int err;
	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	err = mii_mediachg(mii);
	return (err);
}

void
fec_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct fec_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;

	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

/*
 * Manage DMA'able memory.
 */
int
fec_dma_malloc(struct fec_softc *sc, bus_size_t size,
    struct fec_dma_alloc *dma)
{
	int r;

	dma->dma_tag = sc->sc_dma_tag;
	r = bus_dmamem_alloc(dma->dma_tag, size, ENET_ALIGNMENT, 0, &dma->dma_seg,
	    1, &dma->dma_nseg, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: fec_dma_malloc: bus_dmammem_alloc failed; "
			"size %lu, error %d\n", sc->sc_dev.dv_xname,
			(unsigned long)size, r);
		goto fail_0;
	}

	r = bus_dmamem_map(dma->dma_tag, &dma->dma_seg, dma->dma_nseg, size,
	    &dma->dma_vaddr, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (r != 0) {
		printf("%s: fec_dma_malloc: bus_dmammem_map failed; "
			"size %lu, error %d\n", sc->sc_dev.dv_xname,
			(unsigned long)size, r);
		goto fail_1;
	}

	r = bus_dmamap_create(dma->dma_tag, size, 1,
	    size, 0, BUS_DMA_NOWAIT, &dma->dma_map);
	if (r != 0) {
		printf("%s: fec_dma_malloc: bus_dmamap_create failed; "
			"error %u\n", sc->sc_dev.dv_xname, r);
		goto fail_2;
	}

	r = bus_dmamap_load(dma->dma_tag, dma->dma_map,
			    dma->dma_vaddr, size, NULL,
			    BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: fec_dma_malloc: bus_dmamap_load failed; "
			"error %u\n", sc->sc_dev.dv_xname, r);
		goto fail_3;
	}

	dma->dma_size = size;
	dma->dma_paddr = dma->dma_map->dm_segs[0].ds_addr;
	return (0);

fail_3:
	bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
fail_2:
	bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, size);
fail_1:
	bus_dmamem_free(dma->dma_tag, &dma->dma_seg, dma->dma_nseg);
fail_0:
	dma->dma_map = NULL;
	dma->dma_tag = NULL;

	return (r);
}

void
fec_dma_free(struct fec_softc *sc, struct fec_dma_alloc *dma)
{
	if (dma->dma_tag == NULL)
		return;

	if (dma->dma_map != NULL) {
		bus_dmamap_sync(dma->dma_tag, dma->dma_map, 0,
		    dma->dma_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dma->dma_tag, dma->dma_map);
		bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, dma->dma_size);
		bus_dmamem_free(dma->dma_tag, &dma->dma_seg, dma->dma_nseg);
		bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
	}
	dma->dma_tag = NULL;
}
