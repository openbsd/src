/*	$OpenBSD: if_mvneta.c,v 1.6 2018/08/06 10:52:30 patrick Exp $	*/
/*	$NetBSD: if_mvneta.c,v 1.41 2015/04/15 10:15:40 hsuenaga Exp $	*/
/*
 * Copyright (c) 2007, 2008, 2013 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <uvm/uvm_extern.h>
#include <sys/mbuf.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#include <dev/fdt/if_mvnetareg.h>
#include <dev/fdt/mvmdiovar.h>

#ifdef __armv7__
#include <armv7/marvell/mvmbusvar.h>
#endif

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef MVNETA_DEBUG
#define DPRINTF(x)	if (mvneta_debug) printf x
#define DPRINTFN(n,x)	if (mvneta_debug >= (n)) printf x
int mvneta_debug = MVNETA_DEBUG;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define MVNETA_READ(sc, reg) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define MVNETA_WRITE(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define MVNETA_READ_FILTER(sc, reg, val, c) \
	bus_space_read_region_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val), (c))
#define MVNETA_WRITE_FILTER(sc, reg, val, c) \
	bus_space_write_region_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val), (c))

#define MVNETA_LINKUP_READ(sc) \
	MVNETA_READ(sc, MVNETA_PS0)
#define MVNETA_IS_LINKUP(sc)	(MVNETA_LINKUP_READ(sc) & MVNETA_PS0_LINKUP)

#define MVNETA_TX_RING_CNT	256
#define MVNETA_TX_RING_MSK	(MVNETA_TX_RING_CNT - 1)
#define MVNETA_TX_RING_NEXT(x)	(((x) + 1) & MVNETA_TX_RING_MSK)
#define MVNETA_TX_QUEUE_CNT	1
#define MVNETA_RX_RING_CNT	256
#define MVNETA_RX_RING_MSK	(MVNETA_RX_RING_CNT - 1)
#define MVNETA_RX_RING_NEXT(x)	(((x) + 1) & MVNETA_RX_RING_MSK)
#define MVNETA_RX_QUEUE_CNT	1

CTASSERT(MVNETA_TX_RING_CNT > 1 && MVNETA_TX_RING_NEXT(MVNETA_TX_RING_CNT) ==
	(MVNETA_TX_RING_CNT + 1) % MVNETA_TX_RING_CNT);
CTASSERT(MVNETA_RX_RING_CNT > 1 && MVNETA_RX_RING_NEXT(MVNETA_RX_RING_CNT) ==
	(MVNETA_RX_RING_CNT + 1) % MVNETA_RX_RING_CNT);

#define MVNETA_NTXSEG		30

struct mvneta_dmamem {
	bus_dmamap_t		mdm_map;
	bus_dma_segment_t	mdm_seg;
	size_t			mdm_size;
	caddr_t			mdm_kva;
};
#define MVNETA_DMA_MAP(_mdm)	((_mdm)->mdm_map)
#define MVNETA_DMA_LEN(_mdm)	((_mdm)->mdm_size)
#define MVNETA_DMA_DVA(_mdm)	((_mdm)->mdm_map->dm_segs[0].ds_addr)
#define MVNETA_DMA_KVA(_mdm)	((void *)(_mdm)->mdm_kva)

struct mvneta_buf {
	bus_dmamap_t	tb_map;
	struct mbuf	*tb_m;
};

struct mvneta_softc {
	struct device sc_dev;
	struct device *sc_mdio;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;

	struct arpcom sc_ac;
#define sc_enaddr sc_ac.ac_enaddr
	struct mii_data sc_mii;
#define sc_media sc_mii.mii_media

	struct timeout sc_tick_ch;

	struct mvneta_dmamem	*sc_txring;
	struct mvneta_buf	*sc_txbuf;
	struct mvneta_tx_desc	*sc_txdesc;
	int			 sc_tx_prod;	/* next free tx desc */
	int			 sc_tx_cnt;	/* amount of tx sent */
	int			 sc_tx_cons;	/* first tx desc sent */

	struct mvneta_dmamem	*sc_rxring;
	struct mvneta_buf	*sc_rxbuf;
	struct mvneta_rx_desc	*sc_rxdesc;
	int			 sc_rx_prod;	/* next rx desc to fill */
	struct if_rxring	 sc_rx_ring;
	int			 sc_rx_cons;	/* next rx desc recvd */

	enum {
		PHY_MODE_QSGMII,
		PHY_MODE_SGMII,
		PHY_MODE_RGMII,
		PHY_MODE_RGMII_ID,
	}			 sc_phy_mode;
	int			 sc_fixed_link;
	int			 sc_inband_status;
	int			 sc_phy;
	int			 sc_link;
};


int mvneta_miibus_readreg(struct device *, int, int);
void mvneta_miibus_writereg(struct device *, int, int, int);
void mvneta_miibus_statchg(struct device *);

void mvneta_wininit(struct mvneta_softc *);

/* Gigabit Ethernet Port part functions */
int mvneta_match(struct device *, void *, void *);
void mvneta_attach(struct device *, struct device *, void *);
void mvneta_attach_deferred(struct device *);

void mvneta_tick(void *);
int mvneta_intr(void *);

void mvneta_start(struct ifnet *);
int mvneta_ioctl(struct ifnet *, u_long, caddr_t);
void mvneta_inband_statchg(struct mvneta_softc *);
void mvneta_port_change(struct mvneta_softc *);
void mvneta_port_up(struct mvneta_softc *);
int mvneta_up(struct mvneta_softc *);
void mvneta_down(struct mvneta_softc *);
void mvneta_watchdog(struct ifnet *);

int mvneta_mediachange(struct ifnet *);
void mvneta_mediastatus(struct ifnet *, struct ifmediareq *);

int mvneta_encap(struct mvneta_softc *, struct mbuf *, uint32_t *);
void mvneta_rx_proc(struct mvneta_softc *);
void mvneta_tx_proc(struct mvneta_softc *);
uint8_t mvneta_crc8(const uint8_t *, size_t);
void mvneta_iff(struct mvneta_softc *);

struct mvneta_dmamem *mvneta_dmamem_alloc(struct mvneta_softc *,
    bus_size_t, bus_size_t);
void mvneta_dmamem_free(struct mvneta_softc *, struct mvneta_dmamem *);
void mvneta_fill_rx_ring(struct mvneta_softc *);

struct cfdriver mvneta_cd = {
	NULL, "mvneta", DV_IFNET
};

struct cfattach mvneta_ca = {
	sizeof (struct mvneta_softc), mvneta_match, mvneta_attach,
};

int
mvneta_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct mvneta_softc *sc = (struct mvneta_softc *) dev;
	return mvmdio_miibus_readreg(sc->sc_mdio, phy, reg);
}

void
mvneta_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct mvneta_softc *sc = (struct mvneta_softc *) dev;
	return mvmdio_miibus_writereg(sc->sc_mdio, phy, reg, val);
}

void
mvneta_miibus_statchg(struct device *self)
{
	struct mvneta_softc *sc = (struct mvneta_softc *)self;

	if (sc->sc_mii.mii_media_status & IFM_ACTIVE) {
		uint32_t panc = MVNETA_READ(sc, MVNETA_PANC);

		panc &= ~(MVNETA_PANC_SETMIISPEED |
			  MVNETA_PANC_SETGMIISPEED |
			  MVNETA_PANC_SETFULLDX);

		switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
		case IFM_1000_SX:
		case IFM_1000_LX:
		case IFM_1000_CX:
		case IFM_1000_T:
			panc |= MVNETA_PANC_SETGMIISPEED;
			break;
		case IFM_100_TX:
			panc |= MVNETA_PANC_SETMIISPEED;
			break;
		case IFM_10_T:
			break;
		}

		if ((sc->sc_mii.mii_media_active & IFM_GMASK) == IFM_FDX)
			panc |= MVNETA_PANC_SETFULLDX;

		MVNETA_WRITE(sc, MVNETA_PANC, panc);
	}

	mvneta_port_change(sc);
}

void
mvneta_inband_statchg(struct mvneta_softc *sc)
{
	uint32_t reg;

	sc->sc_mii.mii_media_status = IFM_AVALID;
	sc->sc_mii.mii_media_active = IFM_ETHER;

	reg = MVNETA_READ(sc, MVNETA_PS0);
	if (reg & MVNETA_PS0_LINKUP)
		sc->sc_mii.mii_media_status |= IFM_ACTIVE;
	if (reg & MVNETA_PS0_GMIISPEED)
		sc->sc_mii.mii_media_active |= IFM_1000_T;
	else if (reg & MVNETA_PS0_MIISPEED)
		sc->sc_mii.mii_media_active |= IFM_100_TX;
	else
		sc->sc_mii.mii_media_active |= IFM_10_T;
	if (reg & MVNETA_PS0_FULLDX)
		sc->sc_mii.mii_media_active |= IFM_FDX;

	mvneta_port_change(sc);
}

void
mvneta_enaddr_write(struct mvneta_softc *sc)
{
	uint32_t maddrh, maddrl;
	maddrh  = sc->sc_enaddr[0] << 24;
	maddrh |= sc->sc_enaddr[1] << 16;
	maddrh |= sc->sc_enaddr[2] << 8;
	maddrh |= sc->sc_enaddr[3];
	maddrl  = sc->sc_enaddr[4] << 8;
	maddrl |= sc->sc_enaddr[5];
	MVNETA_WRITE(sc, MVNETA_MACAH, maddrh);
	MVNETA_WRITE(sc, MVNETA_MACAL, maddrl);
}

void
mvneta_wininit(struct mvneta_softc *sc)
{
#ifdef __armv7__
	uint32_t en;
	int i;

	if (mvmbus_dram_info == NULL)
		panic("%s: mbus dram information not set up",
		    sc->sc_dev.dv_xname);

	for (i = 0; i < MVNETA_NWINDOW; i++) {
		MVNETA_WRITE(sc, MVNETA_BASEADDR(i), 0);
		MVNETA_WRITE(sc, MVNETA_S(i), 0);

		if (i < MVNETA_NREMAP)
			MVNETA_WRITE(sc, MVNETA_HA(i), 0);
	}

	en = MVNETA_BARE_EN_MASK;

	for (i = 0; i < mvmbus_dram_info->numcs; i++) {
		struct mbus_dram_window *win = &mvmbus_dram_info->cs[i];

		MVNETA_WRITE(sc, MVNETA_BASEADDR(i),
		    MVNETA_BASEADDR_TARGET(mvmbus_dram_info->targetid) |
		    MVNETA_BASEADDR_ATTR(win->attr)	|
		    MVNETA_BASEADDR_BASE(win->base));
		MVNETA_WRITE(sc, MVNETA_S(i), MVNETA_S_SIZE(win->size));

		en &= ~(1 << i);
	}

	MVNETA_WRITE(sc, MVNETA_BARE, en);
#endif
}

int
mvneta_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "marvell,armada-370-neta");
}

void
mvneta_attach(struct device *parent, struct device *self, void *aux)
{
	struct mvneta_softc *sc = (struct mvneta_softc *) self;
	struct fdt_attach_args *faa = aux;
	uint32_t ctl0, ctl2, panc;
	struct ifnet *ifp;
	int i, len, node;
	char *phy_mode;
	char *managed;

	printf("\n");

	sc->sc_iot = faa->fa_iot;
	timeout_set(&sc->sc_tick_ch, mvneta_tick, sc);
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf("%s: cannot map registers\n", self->dv_xname);
		return;
	}
	sc->sc_dmat = faa->fa_dmat;

	clock_enable(faa->fa_node, NULL);

	pinctrl_byname(faa->fa_node, "default");

	len = OF_getproplen(faa->fa_node, "phy-mode");
	if (len <= 0) {
		printf("%s: cannot extract phy-mode\n", self->dv_xname);
		return;
	}

	phy_mode = malloc(len, M_TEMP, M_WAITOK);
	OF_getprop(faa->fa_node, "phy-mode", phy_mode, len);
	if (!strncmp(phy_mode, "qsgmii", strlen("qsgmii")))
		sc->sc_phy_mode = PHY_MODE_QSGMII;
	else if (!strncmp(phy_mode, "sgmii", strlen("sgmii")))
		sc->sc_phy_mode = PHY_MODE_SGMII;
	else if (!strncmp(phy_mode, "rgmii-id", strlen("rgmii-id")))
		sc->sc_phy_mode = PHY_MODE_RGMII_ID;
	else if (!strncmp(phy_mode, "rgmii", strlen("rgmii")))
		sc->sc_phy_mode = PHY_MODE_RGMII;
	else {
		printf("%s: cannot use phy-mode %s\n", self->dv_xname,
		    phy_mode);
		return;
	}
	free(phy_mode, M_TEMP, len);

	/* TODO: check child's name to be "fixed-link" */
	if (OF_getproplen(faa->fa_node, "fixed-link") >= 0 ||
	    OF_child(faa->fa_node))
		sc->sc_fixed_link = 1;

	if ((len = OF_getproplen(faa->fa_node, "managed")) >= 0) {
		managed = malloc(len, M_TEMP, M_WAITOK);
		OF_getprop(faa->fa_node, "managed", managed, len);
		if (!strncmp(managed, "in-band-status",
		    strlen("in-band-status"))) {
			sc->sc_fixed_link = 1;
			sc->sc_inband_status = 1;
		}
		free(managed, M_TEMP, len);
	}

	if (!sc->sc_fixed_link) {
		node = OF_getnodebyphandle(OF_getpropint(faa->fa_node,
		    "phy", 0));
		if (!node) {
			printf("%s: cannot find phy in fdt\n", self->dv_xname);
			return;
		}

		if ((sc->sc_phy = OF_getpropint(node, "reg", -1)) == -1) {
			printf("%s: cannot extract phy addr\n", self->dv_xname);
			return;
		}
	}

	mvneta_wininit(sc);

	if (OF_getproplen(faa->fa_node, "local-mac-address") ==
	    ETHER_ADDR_LEN) {
		OF_getprop(faa->fa_node, "local-mac-address",
		    sc->sc_enaddr, ETHER_ADDR_LEN);
		mvneta_enaddr_write(sc);
	} else {
		uint32_t maddrh, maddrl;
		maddrh = MVNETA_READ(sc, MVNETA_MACAH);
		maddrl = MVNETA_READ(sc, MVNETA_MACAL);
		if (maddrh || maddrl) {
			sc->sc_enaddr[0] = maddrh >> 24;
			sc->sc_enaddr[1] = maddrh >> 16;
			sc->sc_enaddr[2] = maddrh >> 8;
			sc->sc_enaddr[3] = maddrh >> 0;
			sc->sc_enaddr[4] = maddrl >> 8;
			sc->sc_enaddr[5] = maddrl >> 0;
		} else
			ether_fakeaddr(&sc->sc_ac.ac_if);
	}

	printf("%s: Ethernet address %s\n", self->dv_xname,
	    ether_sprintf(sc->sc_enaddr));

	/* disable port */
	MVNETA_WRITE(sc, MVNETA_PMACC0,
	    MVNETA_READ(sc, MVNETA_PMACC0) & ~MVNETA_PMACC0_PORTEN);
	delay(200);

	/* clear all cause registers */
	MVNETA_WRITE(sc, MVNETA_PRXTXTIC, 0);
	MVNETA_WRITE(sc, MVNETA_PRXTXIC, 0);
	MVNETA_WRITE(sc, MVNETA_PMIC, 0);

	/* mask all interrupts */
	MVNETA_WRITE(sc, MVNETA_PRXTXTIM, MVNETA_PRXTXTI_PMISCICSUMMARY);
	MVNETA_WRITE(sc, MVNETA_PRXTXIM, 0);
	MVNETA_WRITE(sc, MVNETA_PMIM, MVNETA_PMI_PHYSTATUSCHNG |
	    MVNETA_PMI_LINKCHANGE | MVNETA_PMI_PSCSYNCCHNG);
	MVNETA_WRITE(sc, MVNETA_PIE, 0);

	/* enable MBUS Retry bit16 */
	MVNETA_WRITE(sc, MVNETA_ERETRY, 0x20);

	/* enable access for CPU0 */
	MVNETA_WRITE(sc, MVNETA_PCP2Q(0),
	    MVNETA_PCP2Q_RXQAE_ALL | MVNETA_PCP2Q_TXQAE_ALL);

	/* reset RX and TX DMAs */
	MVNETA_WRITE(sc, MVNETA_PRXINIT, MVNETA_PRXINIT_RXDMAINIT);
	MVNETA_WRITE(sc, MVNETA_PTXINIT, MVNETA_PTXINIT_TXDMAINIT);

	/* disable legacy WRR, disable EJP, release from reset */
	MVNETA_WRITE(sc, MVNETA_TQC_1, 0);
	for (i = 0; i < MVNETA_TX_QUEUE_CNT; i++) {
		MVNETA_WRITE(sc, MVNETA_TQTBCOUNT(i), 0);
		MVNETA_WRITE(sc, MVNETA_TQTBCONFIG(i), 0);
	}

	MVNETA_WRITE(sc, MVNETA_PRXINIT, 0);
	MVNETA_WRITE(sc, MVNETA_PTXINIT, 0);

	/* set port acceleration mode */
	MVNETA_WRITE(sc, MVNETA_PACC, MVGVE_PACC_ACCELERATIONMODE_EDM);

	MVNETA_WRITE(sc, MVNETA_PXC, MVNETA_PXC_AMNOTXES | MVNETA_PXC_RXCS);
	MVNETA_WRITE(sc, MVNETA_PXCX, 0);
	MVNETA_WRITE(sc, MVNETA_PMFS, 64);

	/* Set SDC register except IPGINT bits */
	MVNETA_WRITE(sc, MVNETA_SDC,
	    MVNETA_SDC_RXBSZ_16_64BITWORDS |
	    MVNETA_SDC_BLMR |	/* Big/Little Endian Receive Mode: No swap */
	    MVNETA_SDC_BLMT |	/* Big/Little Endian Transmit Mode: No swap */
	    MVNETA_SDC_TXBSZ_16_64BITWORDS);

	/* XXX: Disable PHY polling in hardware */
	MVNETA_WRITE(sc, MVNETA_EUC,
	    MVNETA_READ(sc, MVNETA_EUC) & ~MVNETA_EUC_POLLING);

	/* clear uni-/multicast tables */
	uint32_t dfut[MVNETA_NDFUT], dfsmt[MVNETA_NDFSMT], dfomt[MVNETA_NDFOMT];
	memset(dfut, 0, sizeof(dfut));
	memset(dfsmt, 0, sizeof(dfut));
	memset(dfomt, 0, sizeof(dfut));
	MVNETA_WRITE_FILTER(sc, MVNETA_DFUT, dfut, MVNETA_NDFUT);
	MVNETA_WRITE_FILTER(sc, MVNETA_DFSMT, dfut, MVNETA_NDFSMT);
	MVNETA_WRITE_FILTER(sc, MVNETA_DFOMT, dfut, MVNETA_NDFOMT);

	MVNETA_WRITE(sc, MVNETA_PIE,
	    MVNETA_PIE_RXPKTINTRPTENB_ALL | MVNETA_PIE_TXPKTINTRPTENB_ALL);

	MVNETA_WRITE(sc, MVNETA_EUIC, 0);

	/* Setup phy. */
	ctl0 = MVNETA_READ(sc, MVNETA_PMACC0);
	ctl2 = MVNETA_READ(sc, MVNETA_PMACC2);
	panc = MVNETA_READ(sc, MVNETA_PANC);

	/* Force link down to change in-band settings. */
	panc &= ~MVNETA_PANC_FORCELINKPASS;
	panc |= MVNETA_PANC_FORCELINKFAIL;
	MVNETA_WRITE(sc, MVNETA_PANC, panc);

	ctl0 &= ~MVNETA_PMACC0_PORTTYPE;
	ctl2 &= ~(MVNETA_PMACC2_PORTMACRESET | MVNETA_PMACC2_INBANDAN);
	panc &= ~(MVNETA_PANC_INBANDANEN | MVNETA_PANC_INBANDRESTARTAN |
	    MVNETA_PANC_SETMIISPEED | MVNETA_PANC_SETGMIISPEED |
	    MVNETA_PANC_ANSPEEDEN | MVNETA_PANC_SETFCEN |
	    MVNETA_PANC_PAUSEADV | MVNETA_PANC_ANFCEN |
	    MVNETA_PANC_SETFULLDX | MVNETA_PANC_ANDUPLEXEN);

	ctl2 |= MVNETA_PMACC2_RGMIIEN;
	switch (sc->sc_phy_mode) {
	case PHY_MODE_QSGMII:
		MVNETA_WRITE(sc, MVNETA_SERDESCFG,
		    MVNETA_SERDESCFG_QSGMII_PROTO);
		ctl2 |= MVNETA_PMACC2_PCSEN;
		break;
	case PHY_MODE_SGMII:
		MVNETA_WRITE(sc, MVNETA_SERDESCFG,
		    MVNETA_SERDESCFG_SGMII_PROTO);
		ctl2 |= MVNETA_PMACC2_PCSEN;
		break;
	default:
		break;
	}

	/* Use Auto-Negotiation for Inband Status only */
	if (sc->sc_inband_status) {
		panc &= ~(MVNETA_PANC_FORCELINKFAIL |
		    MVNETA_PANC_FORCELINKPASS);
		/* TODO: read mode from SFP */
		if (1) {
			/* 802.3z */
			ctl0 |= MVNETA_PMACC0_PORTTYPE;
			panc |= (MVNETA_PANC_INBANDANEN |
			    MVNETA_PANC_SETGMIISPEED |
			    MVNETA_PANC_SETFULLDX);
		} else {
			/* SGMII */
			ctl2 |= MVNETA_PMACC2_INBANDAN;
			panc |= (MVNETA_PANC_INBANDANEN |
			    MVNETA_PANC_ANSPEEDEN |
			    MVNETA_PANC_ANDUPLEXEN);
		}
		MVNETA_WRITE(sc, MVNETA_OMSCD,
		    MVNETA_READ(sc, MVNETA_OMSCD) | MVNETA_OMSCD_1MS_CLOCK_ENABLE);
	} else {
		MVNETA_WRITE(sc, MVNETA_OMSCD,
		    MVNETA_READ(sc, MVNETA_OMSCD) & ~MVNETA_OMSCD_1MS_CLOCK_ENABLE);
	}

	MVNETA_WRITE(sc, MVNETA_PMACC0, ctl0);
	MVNETA_WRITE(sc, MVNETA_PMACC2, ctl2);
	MVNETA_WRITE(sc, MVNETA_PANC, panc);

	/* Port reset */
	while (MVNETA_READ(sc, MVNETA_PMACC2) & MVNETA_PMACC2_PORTMACRESET)
		;

	fdt_intr_establish(faa->fa_node, IPL_NET, mvneta_intr, sc,
	    sc->sc_dev.dv_xname);

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = mvneta_start;
	ifp->if_ioctl = mvneta_ioctl;
	ifp->if_watchdog = mvneta_watchdog;
	ifp->if_capabilities = IFCAP_VLAN_MTU;

#if notyet
	/*
	 * We can do IPv4/TCPv4/UDPv4 checksums in hardware.
	 */
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
				IFCAP_CSUM_UDPv4;

	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	/*
	 * But, IPv6 packets in the stream can cause incorrect TCPv4 Tx sums.
	 */
	ifp->if_capabilities &= ~IFCAP_CSUM_TCPv4;
#endif

	IFQ_SET_MAXLEN(&ifp->if_snd, max(MVNETA_TX_RING_CNT - 1, IFQ_MAXLEN));
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof(ifp->if_xname));

	/*
	 * Do MII setup.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = mvneta_miibus_readreg;
	sc->sc_mii.mii_writereg = mvneta_miibus_writereg;
	sc->sc_mii.mii_statchg = mvneta_miibus_statchg;

	ifmedia_init(&sc->sc_mii.mii_media, 0,
	    mvneta_mediachange, mvneta_mediastatus);

	if (!sc->sc_fixed_link) {
		extern void *mvmdio_sc;
		sc->sc_mdio = mvmdio_sc;

		if (sc->sc_mdio == NULL) {
			config_defer(self, mvneta_attach_deferred);
			return;
		}

		mii_attach(self, &sc->sc_mii, 0xffffffff, sc->sc_phy,
		    MII_OFFSET_ANY, 0);
		if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
			printf("%s: no PHY found!\n", self->dv_xname);
			ifmedia_add(&sc->sc_mii.mii_media,
			    IFM_ETHER|IFM_MANUAL, 0, NULL);
			ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
		} else
			ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	} else {
		ifmedia_add(&sc->sc_mii.mii_media,
		    IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media,
		    IFM_ETHER|IFM_MANUAL);

		if (sc->sc_inband_status) {
			mvneta_inband_statchg(sc);
		} else {
			sc->sc_mii.mii_media_status = IFM_AVALID|IFM_ACTIVE;
			sc->sc_mii.mii_media_active = IFM_ETHER|IFM_1000_T|IFM_FDX;
			mvneta_miibus_statchg(self);
		}

		ifp->if_baudrate = ifmedia_baudrate(sc->sc_mii.mii_media_active);
		ifp->if_link_state = LINK_STATE_FULL_DUPLEX;
	}

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	return;
}

void
mvneta_attach_deferred(struct device *self)
{
	struct mvneta_softc *sc = (struct mvneta_softc *) self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	extern void *mvmdio_sc;
	sc->sc_mdio = mvmdio_sc;
	if (sc->sc_mdio == NULL) {
		printf("%s: mdio bus not yet attached\n", self->dv_xname);
		return;
	}

	mii_attach(self, &sc->sc_mii, 0xffffffff, sc->sc_phy,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", self->dv_xname);
		ifmedia_add(&sc->sc_mii.mii_media,
		    IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	return;
}

void
mvneta_tick(void *arg)
{
	struct mvneta_softc *sc = arg;
	struct mii_data *mii = &sc->sc_mii;
	int s;

	s = splnet();
	mii_tick(mii);
	splx(s);

	timeout_add_sec(&sc->sc_tick_ch, 1);
}

int
mvneta_intr(void *arg)
{
	struct mvneta_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t ic, misc;

	ic = MVNETA_READ(sc, MVNETA_PRXTXTIC);

	if (ic & MVNETA_PRXTXTI_PMISCICSUMMARY) {
		misc = MVNETA_READ(sc, MVNETA_PMIC);
		MVNETA_WRITE(sc, MVNETA_PMIC, 0);
		if (sc->sc_inband_status && (misc &
		    (MVNETA_PMI_PHYSTATUSCHNG |
		    MVNETA_PMI_LINKCHANGE |
		    MVNETA_PMI_PSCSYNCCHNG))) {
			mvneta_inband_statchg(sc);
		}
	}

	if (!(ifp->if_flags & IFF_RUNNING))
		return 1;

	if (ic & MVNETA_PRXTXTI_TBTCQ(0))
		mvneta_tx_proc(sc);

	if (ic & MVNETA_PRXTXTI_RBICTAPQ(0))
		mvneta_rx_proc(sc);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		mvneta_start(ifp);

	return 1;
}

void
mvneta_start(struct ifnet *ifp)
{
	struct mvneta_softc *sc = ifp->if_softc;
	struct mbuf *m_head = NULL;
	int idx;

	DPRINTFN(3, ("mvneta_start (idx %d)\n", sc->sc_tx_prod));

	if (!(ifp->if_flags & IFF_RUNNING))
		return;
	if (ifq_is_oactive(&ifp->if_snd))
		return;
	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	/* If Link is DOWN, can't start TX */
	if (!MVNETA_IS_LINKUP(sc))
		return;

	bus_dmamap_sync(sc->sc_dmat, MVNETA_DMA_MAP(sc->sc_txring), 0,
	    MVNETA_DMA_LEN(sc->sc_txring),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	idx = sc->sc_tx_prod;
	while (sc->sc_tx_cnt < MVNETA_TX_RING_CNT) {
		m_head = ifq_deq_begin(&ifp->if_snd);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (mvneta_encap(sc, m_head, &idx)) {
			ifq_deq_rollback(&ifp->if_snd, m_head);
			ifq_set_oactive(&ifp->if_snd);
			break;
		}

		/* now we are committed to transmit the packet */
		ifq_deq_commit(&ifp->if_snd, m_head);

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif
	}

	if (sc->sc_tx_prod != idx) {
		sc->sc_tx_prod = idx;

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}
}

int
mvneta_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct mvneta_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)addr;
	int s, error = 0;

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
				mvneta_up(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				mvneta_down(sc);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		DPRINTFN(2, ("mvneta_ioctl MEDIA\n"));
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	case SIOCGIFRXR:
		error = if_rxr_ioctl((struct if_rxrinfo *)ifr->ifr_data,
		    NULL, MCLBYTES, &sc->sc_rx_ring);
		break;
	default:
		DPRINTFN(2, ("mvneta_ioctl ETHER\n"));
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, addr);
		break;
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			mvneta_iff(sc);
		error = 0;
	}

	splx(s);

	return error;
}

void
mvneta_port_change(struct mvneta_softc *sc)
{
	if (!!(sc->sc_mii.mii_media_status & IFM_ACTIVE) != sc->sc_link) {
		sc->sc_link = !sc->sc_link;

		if (sc->sc_link) {
			if (!sc->sc_inband_status) {
				uint32_t panc = MVNETA_READ(sc, MVNETA_PANC);
				panc &= ~MVNETA_PANC_FORCELINKFAIL;
				panc |= MVNETA_PANC_FORCELINKPASS;
				MVNETA_WRITE(sc, MVNETA_PANC, panc);
			}
			mvneta_port_up(sc);
		} else {
			if (!sc->sc_inband_status) {
				uint32_t panc = MVNETA_READ(sc, MVNETA_PANC);
				panc &= ~MVNETA_PANC_FORCELINKPASS;
				panc |= MVNETA_PANC_FORCELINKFAIL;
				MVNETA_WRITE(sc, MVNETA_PANC, panc);
			}
		}
	}
}

void
mvneta_port_up(struct mvneta_softc *sc)
{
	/* Enable port RX/TX. */
	MVNETA_WRITE(sc, MVNETA_RQC, MVNETA_RQC_ENQ(0));
	MVNETA_WRITE(sc, MVNETA_TQC, MVNETA_TQC_ENQ(0));
}

int
mvneta_up(struct mvneta_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mvneta_buf *txb, *rxb;
	int i;

	DPRINTFN(2, ("mvneta_up\n"));

	/* Allocate Tx descriptor ring. */
	sc->sc_txring = mvneta_dmamem_alloc(sc,
	    MVNETA_TX_RING_CNT * sizeof(struct mvneta_tx_desc), 32);
	sc->sc_txdesc = MVNETA_DMA_KVA(sc->sc_txring);

	sc->sc_txbuf = malloc(sizeof(struct mvneta_buf) * MVNETA_TX_RING_CNT,
	    M_DEVBUF, M_WAITOK);

	for (i = 0; i < MVNETA_TX_RING_CNT; i++) {
		txb = &sc->sc_txbuf[i];
		bus_dmamap_create(sc->sc_dmat, MCLBYTES, MVNETA_NTXSEG,
		    MCLBYTES, 0, BUS_DMA_WAITOK, &txb->tb_map);
		txb->tb_m = NULL;
	}

	sc->sc_tx_prod = sc->sc_tx_cons = 0;
	sc->sc_tx_cnt = 0;

	/* Allocate Rx descriptor ring. */
	sc->sc_rxring = mvneta_dmamem_alloc(sc,
	    MVNETA_RX_RING_CNT * sizeof(struct mvneta_rx_desc), 32);
	sc->sc_rxdesc = MVNETA_DMA_KVA(sc->sc_rxring);

	sc->sc_rxbuf = malloc(sizeof(struct mvneta_buf) * MVNETA_RX_RING_CNT,
	    M_DEVBUF, M_WAITOK);

	for (i = 0; i < MVNETA_RX_RING_CNT; i++) {
		rxb = &sc->sc_rxbuf[i];
		bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_WAITOK, &rxb->tb_map);
		rxb->tb_m = NULL;
	}

	/* Set Rx descriptor ring data. */
	MVNETA_WRITE(sc, MVNETA_PRXDQA(0), MVNETA_DMA_DVA(sc->sc_rxring));
	MVNETA_WRITE(sc, MVNETA_PRXDQS(0), MVNETA_RX_RING_CNT |
	    ((MCLBYTES >> 3) << 19));
	MVNETA_WRITE(sc, MVNETA_PRXDQTH(0), 0);
	MVNETA_WRITE(sc, MVNETA_PRXC(0), 0);

	/* Set Tx queue bandwidth. */
	MVNETA_WRITE(sc, MVNETA_TQTBCOUNT(0), 0x03ffffff);
	MVNETA_WRITE(sc, MVNETA_TQTBCONFIG(0), 0x03ffffff);

	/* Set Tx descriptor ring data. */
	MVNETA_WRITE(sc, MVNETA_PTXDQA(0), MVNETA_DMA_DVA(sc->sc_txring));
	MVNETA_WRITE(sc, MVNETA_PTXDQS(0),
	    MVNETA_PTXDQS_DQS(MVNETA_TX_RING_CNT));

	sc->sc_rx_prod = sc->sc_rx_cons = 0;

	if_rxr_init(&sc->sc_rx_ring, 2, MVNETA_RX_RING_CNT);
	mvneta_fill_rx_ring(sc);

	/* TODO: correct frame size */
	MVNETA_WRITE(sc, MVNETA_PMACC0,
	    (MVNETA_READ(sc, MVNETA_PMACC0) & MVNETA_PMACC0_PORTTYPE) |
	    MVNETA_PMACC0_FRAMESIZELIMIT(MCLBYTES - MVNETA_HWHEADER_SIZE));

	/* set max MTU */
	MVNETA_WRITE(sc, MVNETA_TXMTU, MVNETA_TXMTU_MAX);
	MVNETA_WRITE(sc, MVNETA_TXTKSIZE, 0xffffffff);
	MVNETA_WRITE(sc, MVNETA_TXQTKSIZE(0), 0x7fffffff);

	/* enable port */
	MVNETA_WRITE(sc, MVNETA_PMACC0,
	    MVNETA_READ(sc, MVNETA_PMACC0) | MVNETA_PMACC0_PORTEN);

	mvneta_enaddr_write(sc);

	/* Program promiscuous mode and multicast filters. */
	mvneta_iff(sc);

	if (!sc->sc_fixed_link)
		mii_mediachg(&sc->sc_mii);

	if (sc->sc_link)
		mvneta_port_up(sc);

	/* Enable interrupt masks */
	MVNETA_WRITE(sc, MVNETA_PRXTXTIM, MVNETA_PRXTXTI_RBICTAPQ(0) |
	    MVNETA_PRXTXTI_TBTCQ(0) | MVNETA_PRXTXTI_PMISCICSUMMARY);
	MVNETA_WRITE(sc, MVNETA_PMIM, MVNETA_PMI_PHYSTATUSCHNG |
	    MVNETA_PMI_LINKCHANGE | MVNETA_PMI_PSCSYNCCHNG);

	timeout_add_sec(&sc->sc_tick_ch, 1);

	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	return 0;
}

void
mvneta_down(struct mvneta_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t reg, txinprog, txfifoemp;
	struct mvneta_buf *txb, *rxb;
	int i, cnt;

	DPRINTFN(2, ("mvneta_down\n"));

	timeout_del(&sc->sc_tick_ch);

	/* Stop Rx port activity. Check port Rx activity. */
	reg = MVNETA_READ(sc, MVNETA_RQC);
	if (reg & MVNETA_RQC_ENQ_MASK)
		/* Issue stop command for active channels only */
		MVNETA_WRITE(sc, MVNETA_RQC, MVNETA_RQC_DISQ_DISABLE(reg));

	/* Stop Tx port activity. Check port Tx activity. */
	if (MVNETA_READ(sc, MVNETA_TQC) & MVNETA_TQC_ENQ(0))
		MVNETA_WRITE(sc, MVNETA_TQC, MVNETA_TQC_DISQ(0));

	txinprog = MVNETA_PS_TXINPROG_(0);
	txfifoemp = MVNETA_PS_TXFIFOEMP_(0);

#define RX_DISABLE_TIMEOUT		0x1000000
#define TX_FIFO_EMPTY_TIMEOUT		0x1000000
	/* Wait for all Rx activity to terminate. */
	cnt = 0;
	do {
		if (cnt >= RX_DISABLE_TIMEOUT) {
			printf("%s: timeout for RX stopped. rqc 0x%x\n",
			    sc->sc_dev.dv_xname, reg);
			break;
		}
		cnt++;

		/*
		 * Check Receive Queue Command register that all Rx queues
		 * are stopped
		 */
		reg = MVNETA_READ(sc, MVNETA_RQC);
	} while (reg & 0xff);

	/* Double check to verify that TX FIFO is empty */
	cnt = 0;
	while (1) {
		do {
			if (cnt >= TX_FIFO_EMPTY_TIMEOUT) {
				printf("%s: timeout for TX FIFO empty. status "
				    "0x%x\n", sc->sc_dev.dv_xname, reg);
				break;
			}
			cnt++;

			reg = MVNETA_READ(sc, MVNETA_PS);
		} while (!(reg & txfifoemp) || reg & txinprog);

		if (cnt >= TX_FIFO_EMPTY_TIMEOUT)
			break;

		/* Double check */
		reg = MVNETA_READ(sc, MVNETA_PS);
		if (reg & txfifoemp && !(reg & txinprog))
			break;
		else
			printf("%s: TX FIFO empty double check failed."
			    " %d loops, status 0x%x\n", sc->sc_dev.dv_xname,
			    cnt, reg);
	}

	delay(200);

	/* disable port */
	MVNETA_WRITE(sc, MVNETA_PMACC0,
	    MVNETA_READ(sc, MVNETA_PMACC0) & ~MVNETA_PMACC0_PORTEN);
	delay(200);

	/* mask all interrupts */
	MVNETA_WRITE(sc, MVNETA_PRXTXTIM, MVNETA_PRXTXTI_PMISCICSUMMARY);
	MVNETA_WRITE(sc, MVNETA_PRXTXIM, 0);

	/* clear all cause registers */
	MVNETA_WRITE(sc, MVNETA_PRXTXTIC, 0);
	MVNETA_WRITE(sc, MVNETA_PRXTXIC, 0);

	/* Free RX and TX mbufs still in the queues. */
	for (i = 0; i < MVNETA_TX_RING_CNT; i++) {
		txb = &sc->sc_txbuf[i];
		if (txb->tb_m) {
			bus_dmamap_sync(sc->sc_dmat, txb->tb_map, 0,
			    txb->tb_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, txb->tb_map);
			m_freem(txb->tb_m);
		}
		bus_dmamap_destroy(sc->sc_dmat, txb->tb_map);
	}

	mvneta_dmamem_free(sc, sc->sc_txring);
	free(sc->sc_txbuf, M_DEVBUF, 0);

	for (i = 0; i < MVNETA_RX_RING_CNT; i++) {
		rxb = &sc->sc_rxbuf[i];
		if (rxb->tb_m) {
			bus_dmamap_sync(sc->sc_dmat, rxb->tb_map, 0,
			    rxb->tb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, rxb->tb_map);
			m_freem(rxb->tb_m);
		}
		bus_dmamap_destroy(sc->sc_dmat, rxb->tb_map);
	}

	mvneta_dmamem_free(sc, sc->sc_rxring);
	free(sc->sc_rxbuf, M_DEVBUF, 0);

	/* reset RX and TX DMAs */
	MVNETA_WRITE(sc, MVNETA_PRXINIT, MVNETA_PRXINIT_RXDMAINIT);
	MVNETA_WRITE(sc, MVNETA_PTXINIT, MVNETA_PTXINIT_TXDMAINIT);
	MVNETA_WRITE(sc, MVNETA_PRXINIT, 0);
	MVNETA_WRITE(sc, MVNETA_PTXINIT, 0);

	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);
}

void
mvneta_watchdog(struct ifnet *ifp)
{
	struct mvneta_softc *sc = ifp->if_softc;

	/*
	 * Reclaim first as there is a possibility of losing Tx completion
	 * interrupts.
	 */
	mvneta_tx_proc(sc);
	if (sc->sc_tx_cnt != 0) {
		printf("%s: watchdog timeout\n", sc->sc_dev.dv_xname);

		ifp->if_oerrors++;
	}
}

/*
 * Set media options.
 */
int
mvneta_mediachange(struct ifnet *ifp)
{
	struct mvneta_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys))
		mii_mediachg(&sc->sc_mii);

	return (0);
}

/*
 * Report current media status.
 */
void
mvneta_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct mvneta_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys)) {
		mii_pollstat(&sc->sc_mii);
		ifmr->ifm_active = sc->sc_mii.mii_media_active;
		ifmr->ifm_status = sc->sc_mii.mii_media_status;
	}

	if (sc->sc_fixed_link) {
		ifmr->ifm_active = sc->sc_mii.mii_media_active;
		ifmr->ifm_status = sc->sc_mii.mii_media_status;
	}
}

int
mvneta_encap(struct mvneta_softc *sc, struct mbuf *m, uint32_t *idx)
{
	struct mvneta_tx_desc *txd;
	bus_dmamap_t map;
	uint32_t cmdsts;
	int i, current, first, last;

	DPRINTFN(3, ("mvneta_encap\n"));

	first = last = current = *idx;
	map = sc->sc_txbuf[current].tb_map;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT))
		return (ENOBUFS);

	if (map->dm_nsegs > (MVNETA_TX_RING_CNT - sc->sc_tx_cnt - 2)) {
		bus_dmamap_unload(sc->sc_dmat, map);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(2, ("mvneta_encap: dm_nsegs=%d\n", map->dm_nsegs));

	cmdsts = MVNETA_TX_L4_CSUM_NOT;
#if notyet
	int m_csumflags;
	if (m_csumflags & M_CSUM_IPv4)
		cmdsts |= MVNETA_TX_GENERATE_IP_CHKSUM;
	if (m_csumflags & M_CSUM_TCPv4)
		cmdsts |=
		    MVNETA_TX_GENERATE_L4_CHKSUM | MVNETA_TX_L4_TYPE_TCP;
	if (m_csumflags & M_CSUM_UDPv4)
		cmdsts |=
		    MVNETA_TX_GENERATE_L4_CHKSUM | MVNETA_TX_L4_TYPE_UDP;
	if (m_csumflags & (M_CSUM_IPv4 | M_CSUM_TCPv4 | M_CSUM_UDPv4)) {
		const int iphdr_unitlen = sizeof(struct ip) / sizeof(uint32_t);

		cmdsts |= MVNETA_TX_IP_NO_FRAG |
		    MVNETA_TX_IP_HEADER_LEN(iphdr_unitlen);	/* unit is 4B */
	}
#endif

	for (i = 0; i < map->dm_nsegs; i++) {
		txd = &sc->sc_txdesc[current];
		memset(txd, 0, sizeof(*txd));
		txd->bufptr = map->dm_segs[i].ds_addr;
		txd->bytecnt = map->dm_segs[i].ds_len;
		txd->cmdsts = cmdsts |
		    MVNETA_TX_ZERO_PADDING;
		if (i == 0)
		    txd->cmdsts |= MVNETA_TX_FIRST_DESC;
		if (i == (map->dm_nsegs - 1))
		    txd->cmdsts |= MVNETA_TX_LAST_DESC;

		bus_dmamap_sync(sc->sc_dmat, MVNETA_DMA_MAP(sc->sc_txring),
		    current * sizeof(*txd), sizeof(*txd),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		last = current;
		current = MVNETA_TX_RING_NEXT(current);
		KASSERT(current != sc->sc_tx_cons);
	}

	KASSERT(sc->sc_txbuf[last].tb_m == NULL);
	sc->sc_txbuf[first].tb_map = sc->sc_txbuf[last].tb_map;
	sc->sc_txbuf[last].tb_map = map;
	sc->sc_txbuf[last].tb_m = m;

	sc->sc_tx_cnt += map->dm_nsegs;
	*idx = current;

	/* Let him know we sent another packet. */
	MVNETA_WRITE(sc, MVNETA_PTXSU(0), map->dm_nsegs);

	DPRINTFN(3, ("mvneta_encap: completed successfully\n"));

	return 0;
}

void
mvneta_rx_proc(struct mvneta_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mvneta_rx_desc *rxd;
	struct mvneta_buf *rxb;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	uint32_t rxstat;
	int i, idx, len, ready;

	DPRINTFN(3, ("%s: %d\n", __func__, sc->sc_rx_cons));

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	bus_dmamap_sync(sc->sc_dmat, MVNETA_DMA_MAP(sc->sc_rxring), 0,
	    MVNETA_DMA_LEN(sc->sc_rxring),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	ready = MVNETA_PRXS_ODC(MVNETA_READ(sc, MVNETA_PRXS(0)));
	MVNETA_WRITE(sc, MVNETA_PRXSU(0), ready);

	for (i = 0; i < ready; i++) {
		idx = sc->sc_rx_cons;
		KASSERT(idx < MVNETA_RX_RING_CNT);

		rxd = &sc->sc_rxdesc[idx];

#ifdef DIAGNOSTIC
		if ((rxd->cmdsts &
		    (MVNETA_RX_LAST_DESC | MVNETA_RX_FIRST_DESC)) !=
		    (MVNETA_RX_LAST_DESC | MVNETA_RX_FIRST_DESC))
			panic("%s: buffer size is smaller than packet",
			    __func__);
#endif

		len = rxd->bytecnt;
		rxb = &sc->sc_rxbuf[idx];
		KASSERT(rxb->tb_m);

		bus_dmamap_sync(sc->sc_dmat, rxb->tb_map, 0,
		    len, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, rxb->tb_map);

		m = rxb->tb_m;
		rxb->tb_m = NULL;
		m->m_pkthdr.len = m->m_len = len;

		rxstat = rxd->cmdsts;
		if (rxstat & MVNETA_ERROR_SUMMARY) {
#if 0
			int err = rxstat & MVNETA_RX_ERROR_CODE_MASK;

			if (err == MVNETA_RX_CRC_ERROR)
				ifp->if_ierrors++;
			if (err == MVNETA_RX_OVERRUN_ERROR)
				ifp->if_ierrors++;
			if (err == MVNETA_RX_MAX_FRAME_LEN_ERROR)
				ifp->if_ierrors++;
			if (err == MVNETA_RX_RESOURCE_ERROR)
				ifp->if_ierrors++;
#else
			ifp->if_ierrors++;
#endif
			panic("%s: handle input errors", __func__);
			continue;
		}

#if notyet
		if (rxstat & MVNETA_RX_IP_FRAME_TYPE) {
			int flgs = 0;

			/* Check IPv4 header checksum */
			flgs |= M_CSUM_IPv4;
			if (!(rxstat & MVNETA_RX_IP_HEADER_OK))
				flgs |= M_CSUM_IPv4_BAD;
			else if ((bufsize & MVNETA_RX_IP_FRAGMENT) == 0) {
				/*
				 * Check TCPv4/UDPv4 checksum for
				 * non-fragmented packet only.
				 *
				 * It seemd that sometimes
				 * MVNETA_RX_L4_CHECKSUM_OK bit was set to 0
				 * even if the checksum is correct and the
				 * packet was not fragmented. So we don't set
				 * M_CSUM_TCP_UDP_BAD even if csum bit is 0.
				 */

				if (((rxstat & MVNETA_RX_L4_TYPE_MASK) ==
					MVNETA_RX_L4_TYPE_TCP) &&
				    ((rxstat & MVNETA_RX_L4_CHECKSUM_OK) != 0))
					flgs |= M_CSUM_TCPv4;
				else if (((rxstat & MVNETA_RX_L4_TYPE_MASK) ==
					MVNETA_RX_L4_TYPE_UDP) &&
				    ((rxstat & MVNETA_RX_L4_CHECKSUM_OK) != 0))
					flgs |= M_CSUM_UDPv4;
			}
			m->m_pkthdr.csum_flags = flgs;
		}
#endif

		/* Skip on first 2byte (HW header) */
		m_adj(m, MVNETA_HWHEADER_SIZE);

		ml_enqueue(&ml, m);

		if_rxr_put(&sc->sc_rx_ring, 1);

		sc->sc_rx_cons = MVNETA_RX_RING_NEXT(idx);
	}

	mvneta_fill_rx_ring(sc);

	if_input(ifp, &ml);
}

void
mvneta_tx_proc(struct mvneta_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mvneta_tx_desc *txd;
	struct mvneta_buf *txb;
	int i, idx, sent;

	DPRINTFN(3, ("%s\n", __func__));

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	bus_dmamap_sync(sc->sc_dmat, MVNETA_DMA_MAP(sc->sc_txring), 0,
	    MVNETA_DMA_LEN(sc->sc_txring),
	    BUS_DMASYNC_POSTREAD);

	sent = MVNETA_PTXS_TBC(MVNETA_READ(sc, MVNETA_PTXS(0)));
	MVNETA_WRITE(sc, MVNETA_PTXSU(0), MVNETA_PTXSU_NORB(sent));

	for (i = 0; i < sent; i++) {
		idx = sc->sc_tx_cons;
		KASSERT(idx < MVNETA_TX_RING_CNT);

		txd = &sc->sc_txdesc[idx];
		txb = &sc->sc_txbuf[idx];
		if (txb->tb_m) {
			bus_dmamap_sync(sc->sc_dmat, txb->tb_map, 0,
			    txb->tb_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, txb->tb_map);

			m_freem(txb->tb_m);
			txb->tb_m = NULL;
		}

		ifq_clr_oactive(&ifp->if_snd);

		sc->sc_tx_cnt--;

		if (txd->cmdsts & MVNETA_ERROR_SUMMARY) {
			int err = txd->cmdsts & MVNETA_TX_ERROR_CODE_MASK;

			if (err == MVNETA_TX_LATE_COLLISION_ERROR)
				ifp->if_collisions++;
			if (err == MVNETA_TX_UNDERRUN_ERROR)
				ifp->if_oerrors++;
			if (err == MVNETA_TX_EXCESSIVE_COLLISION_ERRO)
				ifp->if_collisions++;
		}

		sc->sc_tx_cons = MVNETA_TX_RING_NEXT(sc->sc_tx_cons);
	}

	if (sc->sc_tx_cnt == 0)
		ifp->if_timer = 0;
}

uint8_t
mvneta_crc8(const uint8_t *data, size_t size)
{
	int bit;
	uint8_t byte;
	uint8_t crc = 0;
	const uint8_t poly = 0x07;

	while(size--)
	  for (byte = *data++, bit = NBBY-1; bit >= 0; bit--)
	    crc = (crc << 1) ^ ((((crc >> 7) ^ (byte >> bit)) & 1) ? poly : 0);

	return crc;
}

CTASSERT(MVNETA_NDFSMT == MVNETA_NDFOMT);

void
mvneta_iff(struct mvneta_softc *sc)
{
	struct arpcom *ac = &sc->sc_ac;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t dfut[MVNETA_NDFUT], dfsmt[MVNETA_NDFSMT], dfomt[MVNETA_NDFOMT];
	uint32_t pxc;
	int i;
	const uint8_t special[ETHER_ADDR_LEN] = {0x01,0x00,0x5e,0x00,0x00,0x00};

	pxc = MVNETA_READ(sc, MVNETA_PXC);
	pxc &= ~(MVNETA_PXC_RB | MVNETA_PXC_RBIP | MVNETA_PXC_RBARP | MVNETA_PXC_UPM);
	ifp->if_flags &= ~IFF_ALLMULTI;
	memset(dfut, 0, sizeof(dfut));
	memset(dfsmt, 0, sizeof(dfsmt));
	memset(dfomt, 0, sizeof(dfomt));

	if (ifp->if_flags & IFF_PROMISC || ac->ac_multirangecnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			pxc |= MVNETA_PXC_UPM;
		for (i = 0; i < MVNETA_NDFSMT; i++) {
			dfsmt[i] = dfomt[i] =
			    MVNETA_DF(0, MVNETA_DF_QUEUE(0) | MVNETA_DF_PASS) |
			    MVNETA_DF(1, MVNETA_DF_QUEUE(0) | MVNETA_DF_PASS) |
			    MVNETA_DF(2, MVNETA_DF_QUEUE(0) | MVNETA_DF_PASS) |
			    MVNETA_DF(3, MVNETA_DF_QUEUE(0) | MVNETA_DF_PASS);
		}
	} else {
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			/* chip handles some IPv4 multicast specially */
			if (memcmp(enm->enm_addrlo, special, 5) == 0) {
				i = enm->enm_addrlo[5];
				dfsmt[i>>2] |=
				    MVNETA_DF(i&3, MVNETA_DF_QUEUE(0) | MVNETA_DF_PASS);
			} else {
				i = mvneta_crc8(enm->enm_addrlo, ETHER_ADDR_LEN);
				dfomt[i>>2] |=
				    MVNETA_DF(i&3, MVNETA_DF_QUEUE(0) | MVNETA_DF_PASS);
			}

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	MVNETA_WRITE(sc, MVNETA_PXC, pxc);

	/* Set Destination Address Filter Unicast Table */
	i = sc->sc_enaddr[5] & 0xf;		/* last nibble */
	dfut[i>>2] = MVNETA_DF(i&3, MVNETA_DF_QUEUE(0) | MVNETA_DF_PASS);
	MVNETA_WRITE_FILTER(sc, MVNETA_DFUT, dfut, MVNETA_NDFUT);

	/* Set Destination Address Filter Multicast Tables */
	MVNETA_WRITE_FILTER(sc, MVNETA_DFSMT, dfsmt, MVNETA_NDFSMT);
	MVNETA_WRITE_FILTER(sc, MVNETA_DFOMT, dfomt, MVNETA_NDFOMT);
}

struct mvneta_dmamem *
mvneta_dmamem_alloc(struct mvneta_softc *sc, bus_size_t size, bus_size_t align)
{
	struct mvneta_dmamem *mdm;
	int nsegs;

	mdm = malloc(sizeof(*mdm), M_DEVBUF, M_WAITOK | M_ZERO);
	mdm->mdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &mdm->mdm_map) != 0)
		goto mdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, align, 0, &mdm->mdm_seg, 1,
	    &nsegs, BUS_DMA_WAITOK) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &mdm->mdm_seg, nsegs, size,
	    &mdm->mdm_kva, BUS_DMA_WAITOK|BUS_DMA_COHERENT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, mdm->mdm_map, mdm->mdm_kva, size,
	    NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	bzero(mdm->mdm_kva, size);

	return (mdm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
mdmfree:
	free(mdm, M_DEVBUF, 0);

	return (NULL);
}

void
mvneta_dmamem_free(struct mvneta_softc *sc, struct mvneta_dmamem *mdm)
{
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, mdm->mdm_size);
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
	free(mdm, M_DEVBUF, 0);
}

struct mbuf *
mvneta_alloc_mbuf(struct mvneta_softc *sc, bus_dmamap_t map)
{
	struct mbuf *m = NULL;

	m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES);
	if (!m)
		return (NULL);
	m->m_len = m->m_pkthdr.len = MCLBYTES;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT) != 0) {
		printf("%s: could not load mbuf DMA map", sc->sc_dev.dv_xname);
		m_freem(m);
		return (NULL);
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0,
	    m->m_pkthdr.len, BUS_DMASYNC_PREREAD);

	return (m);
}

void
mvneta_fill_rx_ring(struct mvneta_softc *sc)
{
	struct mvneta_rx_desc *rxd;
	struct mvneta_buf *rxb;
	u_int slots;

	for (slots = if_rxr_get(&sc->sc_rx_ring, MVNETA_RX_RING_CNT);
	    slots > 0; slots--) {
		rxb = &sc->sc_rxbuf[sc->sc_rx_prod];
		rxb->tb_m = mvneta_alloc_mbuf(sc, rxb->tb_map);
		if (rxb->tb_m == NULL)
			break;

		rxd = &sc->sc_rxdesc[sc->sc_rx_prod];
		memset(rxd, 0, sizeof(*rxd));
		rxd->bufptr = rxb->tb_map->dm_segs[0].ds_addr;

		bus_dmamap_sync(sc->sc_dmat, MVNETA_DMA_MAP(sc->sc_rxring),
		    sc->sc_rx_prod * sizeof(*rxd), sizeof(*rxd),
		    BUS_DMASYNC_PREWRITE);

		sc->sc_rx_prod = MVNETA_RX_RING_NEXT(sc->sc_rx_prod);

		/* Tell him that there's a new free desc. */
		MVNETA_WRITE(sc, MVNETA_PRXSU(0),
		    MVNETA_PRXSU_NOOFNEWDESCRIPTORS(1));
	}

	if_rxr_put(&sc->sc_rx_ring, slots);
}
