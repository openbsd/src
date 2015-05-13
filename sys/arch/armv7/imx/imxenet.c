/* $OpenBSD: imxenet.c,v 1.14 2015/05/13 02:39:28 jsg Exp $ */
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

#include "bpfilter.h"

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/imx/imxenet.h>
#include <armv7/imx/imxccmvar.h>
#include <armv7/imx/imxgpiovar.h>
#include <armv7/imx/imxocotpvar.h>

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

#define ENET_MII_CLK		2500
#define ENET_ALIGNMENT		16

#define ENET_HUMMINGBOARD_PHY			0
#define ENET_HUMMINGBOARD_PHY_RST		(3*32+15)
#define ENET_SABRELITE_PHY			6
#define ENET_SABRELITE_PHY_RST			(2*32+23)
#define ENET_SABRESD_PHY			1
#define ENET_SABRESD_PHY_RST			(0*32+25)
#define ENET_NITROGEN6X_PHY			6
#define ENET_NITROGEN6X_PHY_RST			(0*32+27)
#define ENET_UDOO_PHY				6
#define ENET_UDOO_PHY_RST			(2*32+23)
#define ENET_UDOO_PWR				(1*32+31)
#define ENET_UTILITE_PHY			0
#define ENET_WANDBOARD_PHY			1
#define ENET_PHYFLEX_PHY			3
#define ENET_PHYFLEX_PHY_RST			(2*32+23)
#define ENET_NOVENA_PHY				7
#define ENET_NOVENA_PHY_RST			(2*32+23)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct imxenet_softc {
	struct device			sc_dev;
	struct arpcom			sc_ac;
	struct mii_data			sc_mii;
	int				sc_phyno;
	bus_space_tag_t			sc_iot;
	bus_space_handle_t		sc_ioh;
	void				*sc_ih; /* Interrupt handler */
	bus_dma_tag_t			sc_dma_tag;
	uint32_t			intr_status;	/* soft interrupt status */
	struct imxenet_dma_alloc	txdma;		/* bus_dma glue for tx desc */
	struct imxenet_buf_desc		*tx_desc_base;
	struct imxenet_dma_alloc	rxdma;		/* bus_dma glue for rx desc */
	struct imxenet_buf_desc		*rx_desc_base;
	struct imxenet_dma_alloc	tbdma;		/* bus_dma glue for packets */
	struct imxenet_buffer		*tx_buffer_base;
	struct imxenet_dma_alloc	rbdma;		/* bus_dma glue for packets */
	struct imxenet_buffer		*rx_buffer_base;
	int				cur_tx;
	int				cur_rx;
};

struct imxenet_softc *imxenet_sc;

void imxenet_attach(struct device *, struct device *, void *);
void imxenet_chip_init(struct imxenet_softc *);
int imxenet_ioctl(struct ifnet *, u_long, caddr_t);
void imxenet_start(struct ifnet *);
int imxenet_encap(struct imxenet_softc *, struct mbuf *);
void imxenet_init_txd(struct imxenet_softc *);
void imxenet_init_rxd(struct imxenet_softc *);
void imxenet_init(struct imxenet_softc *);
void imxenet_stop(struct imxenet_softc *);
void imxenet_iff(struct imxenet_softc *);
struct mbuf * imxenet_newbuf(void);
int imxenet_intr(void *);
void imxenet_recv(struct imxenet_softc *);
int imxenet_wait_intr(struct imxenet_softc *, int, int);
int imxenet_miibus_readreg(struct device *, int, int);
void imxenet_miibus_writereg(struct device *, int, int, int);
void imxenet_miibus_statchg(struct device *);
int imxenet_ifmedia_upd(struct ifnet *);
void imxenet_ifmedia_sts(struct ifnet *, struct ifmediareq *);
int imxenet_dma_malloc(struct imxenet_softc *, bus_size_t, struct imxenet_dma_alloc *);
void imxenet_dma_free(struct imxenet_softc *, struct imxenet_dma_alloc *);

struct cfattach imxenet_ca = {
	sizeof (struct imxenet_softc), NULL, imxenet_attach
};

struct cfdriver imxenet_cd = {
	NULL, "imxenet", DV_IFNET
};

void
imxenet_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct imxenet_softc *sc = (struct imxenet_softc *) self;
	struct mii_data *mii;
	struct ifnet *ifp;
	int tsize, rsize, tbsize, rbsize, s;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("imxenet_attach: bus_space_map failed!");

	sc->sc_dma_tag = aa->aa_dmat;

	/* power it up */
	imxccm_enable_enet();

	switch (board_id)
	{
	case BOARD_ID_IMX6_CUBOXI:
	case BOARD_ID_IMX6_HUMMINGBOARD:
		/* We need to reset the AR8035 PHY twice. */
		imxgpio_clear_bit(ENET_HUMMINGBOARD_PHY_RST);
		imxgpio_set_dir(ENET_HUMMINGBOARD_PHY_RST, IMXGPIO_DIR_OUT);
		delay(2000);
		imxgpio_set_bit(ENET_HUMMINGBOARD_PHY_RST);
		delay(2000);
		imxgpio_clear_bit(ENET_HUMMINGBOARD_PHY_RST);
		delay(2000);
		imxgpio_set_bit(ENET_HUMMINGBOARD_PHY_RST);
		delay(2000);
		break;
	case BOARD_ID_IMX6_PHYFLEX:
	case BOARD_ID_IMX6_SABRELITE:
		/* phyFLEX i.MX6 and SABRE Lite PHY reset */
		imxgpio_clear_bit(ENET_SABRELITE_PHY_RST);
		imxgpio_set_dir(ENET_SABRELITE_PHY_RST, IMXGPIO_DIR_OUT);
		imxgpio_clear_bit(ENET_NITROGEN6X_PHY_RST);
		imxgpio_set_dir(ENET_NITROGEN6X_PHY_RST, IMXGPIO_DIR_OUT);
		delay(1000 * 10);
		imxgpio_set_bit(ENET_SABRELITE_PHY_RST);
		imxgpio_set_bit(ENET_NITROGEN6X_PHY_RST);
		delay(100);
		break;
	case BOARD_ID_IMX6_SABRESD:
		imxgpio_clear_bit(ENET_SABRESD_PHY_RST);
		imxgpio_set_dir(ENET_SABRESD_PHY_RST, IMXGPIO_DIR_OUT);
		delay(1000 * 10);
		imxgpio_set_bit(ENET_SABRESD_PHY_RST);
		delay(100);
		break;
	case BOARD_ID_IMX6_UDOO:
		imxgpio_set_bit(ENET_UDOO_PWR);
		imxgpio_set_dir(ENET_UDOO_PWR, IMXGPIO_DIR_OUT);
		imxgpio_clear_bit(ENET_UDOO_PHY_RST);
		imxgpio_set_dir(ENET_UDOO_PHY_RST, IMXGPIO_DIR_OUT);
		delay(1000 * 1);
		imxgpio_set_bit(ENET_UDOO_PHY_RST);
		delay(1000 * 100);
		break;
	case BOARD_ID_IMX6_NOVENA:
		imxgpio_clear_bit(ENET_NOVENA_PHY_RST);
		imxgpio_set_dir(ENET_NOVENA_PHY_RST, IMXGPIO_DIR_OUT);
		delay(1000 * 10);
		imxgpio_set_bit(ENET_NOVENA_PHY_RST);
		delay(100);
		break;
	}

	/* reset the controller */
	HSET4(sc, ENET_ECR, ENET_ECR_RESET);
	while(HREAD4(sc, ENET_ECR) & ENET_ECR_RESET);

	HWRITE4(sc, ENET_EIMR, 0);
	HWRITE4(sc, ENET_EIR, 0xffffffff);

	sc->sc_ih = arm_intr_establish(aa->aa_dev->irq[0], IPL_NET,
	    imxenet_intr, sc, sc->sc_dev.dv_xname);

	tsize = ENET_MAX_TXD * sizeof(struct imxenet_buf_desc);
	tsize = ENET_ROUNDUP(tsize, PAGE_SIZE);

	if (imxenet_dma_malloc(sc, tsize, &sc->txdma)) {
		printf("%s: Unable to allocate tx_desc memory\n",
		    sc->sc_dev.dv_xname);
		goto bad;
	}
	sc->tx_desc_base = (struct imxenet_buf_desc *)sc->txdma.dma_vaddr;

	rsize = ENET_MAX_RXD * sizeof(struct imxenet_buf_desc);
	rsize = ENET_ROUNDUP(rsize, PAGE_SIZE);

	if (imxenet_dma_malloc(sc, rsize, &sc->rxdma)) {
		printf("%s: Unable to allocate rx_desc memory\n",
		    sc->sc_dev.dv_xname);
		goto txdma;
	}
	sc->rx_desc_base = (struct imxenet_buf_desc *)sc->rxdma.dma_vaddr;

	tbsize = ENET_MAX_TXD * ENET_MAX_PKT_SIZE;
	tbsize = ENET_ROUNDUP(tbsize, PAGE_SIZE);

	if (imxenet_dma_malloc(sc, tbsize, &sc->tbdma)) {
		printf("%s: Unable to allocate tx_buffer memory\n",
		    sc->sc_dev.dv_xname);
		goto rxdma;
	}
	sc->tx_buffer_base = (struct imxenet_buffer *)sc->tbdma.dma_vaddr;

	rbsize = ENET_MAX_RXD * ENET_MAX_PKT_SIZE;
	rbsize = ENET_ROUNDUP(rbsize, PAGE_SIZE);

	if (imxenet_dma_malloc(sc, rbsize, &sc->rbdma)) {
		printf("%s: Unable to allocate rx_buffer memory\n",
		    sc->sc_dev.dv_xname);
		goto tbdma;
	}
	sc->rx_buffer_base = (struct imxenet_buffer *)sc->rbdma.dma_vaddr;

	sc->cur_tx = 0;
	sc->cur_rx = 0;

	printf("\n");

	s = splnet();

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = imxenet_ioctl;
	ifp->if_start = imxenet_start;
	ifp->if_capabilities = IFCAP_VLAN_MTU;

	memset(sc->sc_ac.ac_enaddr, 0xff, ETHER_ADDR_LEN);
	imxocotp_get_ethernet_address(sc->sc_ac.ac_enaddr);

	printf("%s: address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	/* initialize the chip */
	imxenet_chip_init(sc);

	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize MII/media info. */
	mii = &sc->sc_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = imxenet_miibus_readreg;
	mii->mii_writereg = imxenet_miibus_writereg;
	mii->mii_statchg = imxenet_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;

	ifmedia_init(&mii->mii_media, 0, imxenet_ifmedia_upd, imxenet_ifmedia_sts);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);

	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);
	splx(s);

	imxenet_sc = sc;
	return;

tbdma:
	imxenet_dma_free(sc, &sc->tbdma);
rxdma:
	imxenet_dma_free(sc, &sc->rxdma);
txdma:
	imxenet_dma_free(sc, &sc->txdma);
bad:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, aa->aa_dev->mem[0].size);
}

void
imxenet_chip_init(struct imxenet_softc *sc)
{
	struct device *dev = (struct device *) sc;
	int phy = 0;
	uint32_t reg;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ENET_MSCR,
	    (((imxccm_get_fecclk() + (ENET_MII_CLK << 2) - 1) / (ENET_MII_CLK << 2)) << 1) | 0x100);

	switch (board_id)
	{
	case BOARD_ID_IMX6_CUBOXI:
	case BOARD_ID_IMX6_HUMMINGBOARD:
		phy = ENET_HUMMINGBOARD_PHY;
		break;
	case BOARD_ID_IMX6_PHYFLEX:
		phy = ENET_PHYFLEX_PHY;
		break;
	case BOARD_ID_IMX6_SABRELITE:
		phy = ENET_SABRELITE_PHY;
		break;
	case BOARD_ID_IMX6_SABRESD:
		phy = ENET_SABRESD_PHY;
		break;
	case BOARD_ID_IMX6_UDOO:
		phy = ENET_UDOO_PHY;
		break;
	case BOARD_ID_IMX6_UTILITE:
		phy = ENET_UTILITE_PHY;
		break;
	case BOARD_ID_IMX6_NOVENA:
		phy = ENET_NOVENA_PHY;
		break;
	case BOARD_ID_IMX6_WANDBOARD:
		phy = ENET_WANDBOARD_PHY;
		break;
	}

	switch (board_id)
	{
	case BOARD_ID_IMX6_UDOO:	/* Micrel KSZ9031 */
		/* prefer master mode */
		imxenet_miibus_writereg(dev, phy, 0x9, 0x1c00);

		/* control data pad skew */
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x0002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x0004);
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x4002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x0000);

		/* rx data pad skew */
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x0002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x0005);
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x4002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x0000);

		/* tx data pad skew */
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x0002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x0006);
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x4002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x0000);

		/* gtx and rx data pad skew */
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x0002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x0008);
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x4002);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x03ff);
		break;
	case BOARD_ID_IMX6_PHYFLEX:
	case BOARD_ID_IMX6_SABRELITE:	/* Micrel KSZ9021 */
		/* prefer master mode */
		imxenet_miibus_writereg(dev, phy, 0x9, 0x1f00);

		/* min rx data delay */
		imxenet_miibus_writereg(dev, phy, 0x0b, 0x8105);
		imxenet_miibus_writereg(dev, phy, 0x0c, 0x0000);

		/* min tx data delay */
		imxenet_miibus_writereg(dev, phy, 0x0b, 0x8106);
		imxenet_miibus_writereg(dev, phy, 0x0c, 0x0000);

		/* max rx/tx clock delay, min rx/tx control delay */
		imxenet_miibus_writereg(dev, phy, 0x0b, 0x8104);
		imxenet_miibus_writereg(dev, phy, 0x0c, 0xf0f0);
		imxenet_miibus_writereg(dev, phy, 0x0b, 0x104);

		/* enable all interrupts */
		imxenet_miibus_writereg(dev, phy, 0x1b, 0xff00);
		break;
	case BOARD_ID_IMX6_NOVENA:	/* Micrel KSZ9021 */
		/* TXEN_SKEW_PS/TXC_SKEW_PS/RXDV_SKEW_PS/RXC_SKEW_PS */
		imxenet_miibus_writereg(dev, phy, 0x0b, 0x8104);
		imxenet_miibus_writereg(dev, phy, 0x0c, 0xf0f0);

		/* RXD0_SKEW_PS/RXD1_SKEW_PS/RXD2_SKEW_PS/RXD3_SKEW_PS */
		imxenet_miibus_writereg(dev, phy, 0x0b, 0x8105);
		imxenet_miibus_writereg(dev, phy, 0x0c, 0x0000);

		/* TXD0_SKEW_PS/TXD1_SKEW_PS/TXD2_SKEW_PS/TXD3_SKEW_PS */
		imxenet_miibus_writereg(dev, phy, 0x0b, 0x8106);
		imxenet_miibus_writereg(dev, phy, 0x0c, 0xffff);
		break;
	case BOARD_ID_IMX6_CUBOXI:		/* AR8035 */
	case BOARD_ID_IMX6_HUMMINGBOARD:	/* AR8035 */
	case BOARD_ID_IMX6_SABRESD:		/* AR8031 */
	case BOARD_ID_IMX6_UTILITE:
	case BOARD_ID_IMX6_WANDBOARD:		/* AR8031 */
		/* disable SmartEEE */
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x0003);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x805d);
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x4003);
		reg = imxenet_miibus_readreg(dev, phy, 0x0e);
		imxenet_miibus_writereg(dev, phy, 0x0e, reg & ~0x0100);

		/* enable 125MHz clk output for AR8031 */
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x0007);
		imxenet_miibus_writereg(dev, phy, 0x0e, 0x8016);
		imxenet_miibus_writereg(dev, phy, 0x0d, 0x4007);

		reg = imxenet_miibus_readreg(dev, phy, 0x0e) & 0xffe3;
		imxenet_miibus_writereg(dev, phy, 0x0e, reg | 0x18);

		/* tx clock delay */
		imxenet_miibus_writereg(dev, phy, 0x1d, 0x0005);
		reg = imxenet_miibus_readreg(dev, phy, 0x1e);
		imxenet_miibus_writereg(dev, phy, 0x1e, reg | 0x0100);

		/* phy power */
		reg = imxenet_miibus_readreg(dev, phy, 0x00);
		if (reg & 0x0800)
			imxenet_miibus_writereg(dev, phy, 0x00, reg & ~0x0800);
		break;
	}
}

void
imxenet_init_rxd(struct imxenet_softc *sc)
{
	int i;

	memset(sc->rx_desc_base, 0, ENET_MAX_RXD * sizeof(struct imxenet_buf_desc));

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
imxenet_init_txd(struct imxenet_softc *sc)
{
	int i;

	memset(sc->tx_desc_base, 0, ENET_MAX_TXD * sizeof(struct imxenet_buf_desc));

	for (i = 0; i < ENET_MAX_TXD; i++)
	{
		sc->tx_desc_base[i].data_pointer = sc->tbdma.dma_paddr + i * ENET_MAX_PKT_SIZE;
	}

	sc->tx_desc_base[i - 1].status |= ENET_TXD_WRAP;
}

void
imxenet_init(struct imxenet_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int speed = 0;

	/* reset the controller */
	HSET4(sc, ENET_ECR, ENET_ECR_RESET);
	while(HREAD4(sc, ENET_ECR) & ENET_ECR_RESET);

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
	imxenet_init_txd(sc);
	imxenet_init_rxd(sc);

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

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ENET_MSCR,
	    (((imxccm_get_fecclk() + (ENET_MII_CLK << 2) - 1) / (ENET_MII_CLK << 2)) << 1) | 0x100);

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
	imxenet_iff(sc);

	/* Indicate we are up and running. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* enable interrupts for tx/rx */
	HWRITE4(sc, ENET_EIMR, ENET_EIR_TXF | ENET_EIR_RXF);
	HWRITE4(sc, ENET_EIMR, 0xffffffff);

	imxenet_start(ifp);
}

void
imxenet_stop(struct imxenet_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	/* reset the controller */
	HSET4(sc, ENET_ECR, ENET_ECR_RESET);
	while(HREAD4(sc, ENET_ECR) & ENET_ECR_RESET);
}

void
imxenet_iff(struct imxenet_softc *sc)
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
imxenet_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct imxenet_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			imxenet_init(sc);
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				imxenet_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				imxenet_stop(sc);
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
			imxenet_iff(sc);
		error = 0;
	}

	splx(s);
	return(error);
}

void
imxenet_start(struct ifnet *ifp)
{
	struct imxenet_softc *sc = ifp->if_softc;
	struct mbuf *m_head = NULL;

	if ((ifp->if_flags & (IFF_OACTIVE | IFF_RUNNING)) != IFF_RUNNING)
		return;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (imxenet_encap(sc, m_head)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m_head);

		ifp->if_opackets++;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif

		m_freem(m_head);
	}
}

int
imxenet_encap(struct imxenet_softc *sc, struct mbuf *m)
{
	if (sc->tx_desc_base[sc->cur_tx].status & ENET_TXD_READY) {
		printf("imxenet: tx queue full!\n");
		return EIO;
	}

	if (m->m_pkthdr.len > ENET_MAX_PKT_SIZE) {
		printf("imxenet: packet too big\n");
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
	    sizeof(struct imxenet_buf_desc) * sc->cur_tx,
	    sizeof(struct imxenet_buf_desc),
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
imxenet_newbuf(void)
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
imxenet_intr(void *arg)
{
	struct imxenet_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	u_int32_t status;

	/* Find out which interrupts are pending. */
	status = HREAD4(sc, ENET_EIR);

	/* Acknowledge the interrupts we are about to handle. */
	HWRITE4(sc, ENET_EIR, status);

	/*
	 * Wake up the blocking process to service command
	 * related interrupt(s).
	 */
	if (ISSET(status, ENET_EIR_MII)) {
		sc->intr_status |= status;
		wakeup(&sc->intr_status);
	}

	/*
	 * Handle incoming packets.
	 */
	if (ISSET(status, ENET_EIR_RXF)) {
		if (ifp->if_flags & IFF_RUNNING)
			imxenet_recv(sc);
	}

	/* Try to transmit. */
	if (ifp->if_flags & IFF_RUNNING && !IFQ_IS_EMPTY(&ifp->if_snd))
		imxenet_start(ifp);

	return 1;
}

void
imxenet_recv(struct imxenet_softc *sc)
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
		m = imxenet_newbuf();

		if (m == NULL) {
			ifp->if_ierrors++;
			goto done;
		}

		ifp->if_ipackets++;
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
		    sizeof(struct imxenet_buf_desc) * sc->cur_rx,
		    sizeof(struct imxenet_buf_desc),
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

int
imxenet_wait_intr(struct imxenet_softc *sc, int mask, int timo)
{
	int status;
	int s;

	s = splnet();

	status = sc->intr_status;
	while (status == 0) {
		if (tsleep(&sc->intr_status, PWAIT, "hcintr", timo)
		    == EWOULDBLOCK) {
			break;
		}
		status = sc->intr_status;
	}
	sc->intr_status &= ~status;

	splx(s);
	return status;
}

/*
 * MII
 * Interrupts need ENET_ECR_ETHEREN to be set,
 * so we just read the interrupt status registers.
 */
int
imxenet_miibus_readreg(struct device *dev, int phy, int reg)
{
	int r = 0;
	struct imxenet_softc *sc = (struct imxenet_softc *)dev;

	HSET4(sc, ENET_EIR, ENET_EIR_MII);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ENET_MMFR,
	    ENET_MMFR_ST | ENET_MMFR_OP_RD | ENET_MMFR_TA |
	    phy << ENET_MMFR_PA_SHIFT | reg << ENET_MMFR_RA_SHIFT);

	while(!(HREAD4(sc, ENET_EIR) & ENET_EIR_MII));

	r = bus_space_read_4(sc->sc_iot, sc->sc_ioh, ENET_MMFR);

	return (r & 0xffff);
}

void
imxenet_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct imxenet_softc *sc = (struct imxenet_softc *)dev;

	HSET4(sc, ENET_EIR, ENET_EIR_MII);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ENET_MMFR,
	    ENET_MMFR_ST | ENET_MMFR_OP_WR | ENET_MMFR_TA |
	    phy << ENET_MMFR_PA_SHIFT | reg << ENET_MMFR_RA_SHIFT |
	    (val & 0xffff));

	while(!(HREAD4(sc, ENET_EIR) & ENET_EIR_MII));

	return;
}

void
imxenet_miibus_statchg(struct device *dev)
{
	struct imxenet_softc *sc = (struct imxenet_softc *)dev;
	int ecr;

	ecr = HREAD4(sc, ENET_ECR);
	switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
	case IFM_1000_T:  /* Gigabit */
		ecr |= ENET_ECR_SPEED;
		break;
	default:
		ecr &= ~ENET_ECR_SPEED;
	}
	HWRITE4(sc, ENET_ECR, ecr);

	return;
}

int
imxenet_ifmedia_upd(struct ifnet *ifp)
{
	struct imxenet_softc *sc = ifp->if_softc;
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
imxenet_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct imxenet_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;

	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

/*
 * Manage DMA'able memory.
 */
int
imxenet_dma_malloc(struct imxenet_softc *sc, bus_size_t size,
    struct imxenet_dma_alloc *dma)
{
	int r;

	dma->dma_tag = sc->sc_dma_tag;
	r = bus_dmamem_alloc(dma->dma_tag, size, ENET_ALIGNMENT, 0, &dma->dma_seg,
	    1, &dma->dma_nseg, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: imxenet_dma_malloc: bus_dmammem_alloc failed; "
			"size %lu, error %d\n", sc->sc_dev.dv_xname,
			(unsigned long)size, r);
		goto fail_0;
	}

	r = bus_dmamem_map(dma->dma_tag, &dma->dma_seg, dma->dma_nseg, size,
	    &dma->dma_vaddr, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (r != 0) {
		printf("%s: imxenet_dma_malloc: bus_dmammem_map failed; "
			"size %lu, error %d\n", sc->sc_dev.dv_xname,
			(unsigned long)size, r);
		goto fail_1;
	}

	r = bus_dmamap_create(dma->dma_tag, size, 1,
	    size, 0, BUS_DMA_NOWAIT, &dma->dma_map);
	if (r != 0) {
		printf("%s: imxenet_dma_malloc: bus_dmamap_create failed; "
			"error %u\n", sc->sc_dev.dv_xname, r);
		goto fail_2;
	}

	r = bus_dmamap_load(dma->dma_tag, dma->dma_map,
			    dma->dma_vaddr, size, NULL,
			    BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: imxenet_dma_malloc: bus_dmamap_load failed; "
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
imxenet_dma_free(struct imxenet_softc *sc, struct imxenet_dma_alloc *dma)
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
