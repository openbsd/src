/*	$OpenBSD: cn30xxgmx.c,v 1.18 2015/02/11 07:05:39 dlg Exp $	*/

/*
 * Copyright (c) 2007 Internet Initiative Japan, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *  support GMX0 interface only
 *  take no thought for other GMX interface
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <machine/octeon_model.h>
#include <machine/octeonvar.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/cn30xxciureg.h>
#include <octeon/dev/cn30xxgmxreg.h>
#include <octeon/dev/cn30xxipdvar.h>
#include <octeon/dev/cn30xxasxvar.h>
#include <octeon/dev/cn30xxgmxvar.h>

#define	dprintf(...)
#define	OCTEON_ETH_KASSERT	KASSERT

#define	ADDR2UINT64(u, a) \
	do { \
		u = \
		    (((uint64_t)a[0] << 40) | ((uint64_t)a[1] << 32) | \
		     ((uint64_t)a[2] << 24) | ((uint64_t)a[3] << 16) | \
		     ((uint64_t)a[4] <<  8) | ((uint64_t)a[5] <<  0)); \
	} while (0)
#define	UINT642ADDR(a, u) \
	do { \
		a[0] = (uint8_t)((u) >> 40); a[1] = (uint8_t)((u) >> 32); \
		a[2] = (uint8_t)((u) >> 24); a[3] = (uint8_t)((u) >> 16); \
		a[4] = (uint8_t)((u) >>  8); a[5] = (uint8_t)((u) >>  0); \
	} while (0)

#define	_GMX_RD8(sc, off) \
	bus_space_read_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_gmx->sc_regh, (off))
#define	_GMX_WR8(sc, off, v) \
	bus_space_write_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_gmx->sc_regh, (off), (v))
#define	_GMX_PORT_RD8(sc, off) \
	bus_space_read_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_regh, (off))
#define	_GMX_PORT_WR8(sc, off, v) \
	bus_space_write_8((sc)->sc_port_gmx->sc_regt, (sc)->sc_port_regh, (off), (v))

struct cn30xxgmx_port_ops {
	int	(*port_ops_enable)(struct cn30xxgmx_port_softc *, int);
	int	(*port_ops_speed)(struct cn30xxgmx_port_softc *);
	int	(*port_ops_timing)(struct cn30xxgmx_port_softc *);
	int	(*port_ops_set_mac_addr)(struct cn30xxgmx_port_softc *,
		    uint8_t *, uint64_t);
	int	(*port_ops_set_filter)(struct cn30xxgmx_port_softc *);
};

int	cn30xxgmx_match(struct device *, void *, void *);
void	cn30xxgmx_attach(struct device *, struct device *, void *);
int	cn30xxgmx_print(void *, const char *);
int	cn30xxgmx_submatch(struct device *, void *, void *);
int	cn30xxgmx_port_phy_addr(int);
int	cn30xxgmx_init(struct cn30xxgmx_softc *);
int	cn30xxgmx_rx_frm_ctl_xable(struct cn30xxgmx_port_softc *,
	    uint64_t, int);
int	cn30xxgmx_rgmii_enable(struct cn30xxgmx_port_softc *, int);
int	cn30xxgmx_rgmii_speed(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_rgmii_speed_newlink(struct cn30xxgmx_port_softc *,
	    uint64_t *);
int	cn30xxgmx_rgmii_speed_speed(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_rgmii_timing(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_rgmii_set_mac_addr(struct cn30xxgmx_port_softc *,
	    uint8_t *, uint64_t);
int	cn30xxgmx_rgmii_set_filter(struct cn30xxgmx_port_softc *);
int	cn30xxgmx_tx_ovr_bp_enable(struct cn30xxgmx_port_softc *, int);
int	cn30xxgmx_rx_pause_enable(struct cn30xxgmx_port_softc *, int);

#ifdef OCTEON_ETH_DEBUG
void	cn30xxgmx_dump(void);
void	cn30xxgmx_debug_reset(void);
int	cn30xxgmx_intr_drop(void *);
int	cn30xxgmx_rgmii_speed_newlink_log(struct cn30xxgmx_port_softc *,
	    uint64_t);
#endif

static const int	cn30xxgmx_rx_adr_cam_regs[] = {
	GMX0_RX0_ADR_CAM0, GMX0_RX0_ADR_CAM1, GMX0_RX0_ADR_CAM2,
	GMX0_RX0_ADR_CAM3, GMX0_RX0_ADR_CAM4, GMX0_RX0_ADR_CAM5
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_mii = {
	/* XXX not implemented */
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_gmii = {
	.port_ops_enable = cn30xxgmx_rgmii_enable,
	.port_ops_speed = cn30xxgmx_rgmii_speed,
	.port_ops_timing = cn30xxgmx_rgmii_timing,
	.port_ops_set_mac_addr = cn30xxgmx_rgmii_set_mac_addr,
	.port_ops_set_filter = cn30xxgmx_rgmii_set_filter
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_rgmii = {
	.port_ops_enable = cn30xxgmx_rgmii_enable,
	.port_ops_speed = cn30xxgmx_rgmii_speed,
	.port_ops_timing = cn30xxgmx_rgmii_timing,
	.port_ops_set_mac_addr = cn30xxgmx_rgmii_set_mac_addr,
	.port_ops_set_filter = cn30xxgmx_rgmii_set_filter
};

struct cn30xxgmx_port_ops cn30xxgmx_port_ops_spi42 = {
	/* XXX not implemented */
};

struct cn30xxgmx_port_ops *cn30xxgmx_port_ops[] = {
	[GMX_MII_PORT] = &cn30xxgmx_port_ops_mii,
	[GMX_GMII_PORT] = &cn30xxgmx_port_ops_gmii,
	[GMX_RGMII_PORT] = &cn30xxgmx_port_ops_rgmii,
	[GMX_SPI42_PORT] = &cn30xxgmx_port_ops_spi42
};

#ifdef OCTEON_ETH_DEBUG
void	*cn30xxgmx_intr_drop_ih;

struct cn30xxgmx_port_softc *__cn30xxgmx_port_softc[3/* XXX */];
#endif

struct cfattach cn30xxgmx_ca = {sizeof(struct cn30xxgmx_softc),
    cn30xxgmx_match, cn30xxgmx_attach, NULL, NULL};

struct cfdriver cn30xxgmx_cd = {NULL, "cn30xxgmx", DV_DULL};

int
cn30xxgmx_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = (struct cfdata *)match;
	struct iobus_attach_args *aa = aux;

	if (strcmp(cf->cf_driver->cd_name, aa->aa_name) != 0)
		return 0;
	if (cf->cf_unit != aa->aa_unitno)
		return 0;
	return 1;
}

int
cn30xxgmx_port_phy_addr(int port)
{
	static const int octeon_eth_phy_table[] = {
		/* portwell cam-0100 */
		0x02, 0x03, 0x22
	};

	switch (octeon_boot_info->board_type) {
	case BOARD_TYPE_UBIQUITI_E100:	/* port 0: 7, port 1: 6 */
		if (port > 2)
			return -1;
		return 7 - port;

	default:
		if (port >= nitems(octeon_eth_phy_table))
			return -1;
		return octeon_eth_phy_table[port];
	}
}

void
cn30xxgmx_attach(struct device *parent, struct device *self, void *aux)
{
	struct cn30xxgmx_softc *sc = (void *)self;
	struct iobus_attach_args *aa = aux;
	struct cn30xxgmx_attach_args gmx_aa;
	int status;
	int i;
	struct cn30xxgmx_port_softc *port_sc;

	printf("\n");

	sc->sc_regt = aa->aa_bust; /* XXX why there are iot? */
	
	status = bus_space_map(sc->sc_regt, aa->aa_unit->addr,
	    GMX0_BASE_IF_SIZE, 0, &sc->sc_regh);
	if (status != 0)
		panic(": can't map register");

	cn30xxgmx_init(sc);

	sc->sc_ports = mallocarray(sc->sc_nports, sizeof(*sc->sc_ports),
	    M_DEVBUF, M_NOWAIT | M_ZERO);

	for (i = 0; i < sc->sc_nports; i++) {
		port_sc = &sc->sc_ports[i];
		port_sc->sc_port_gmx = sc;
		port_sc->sc_port_no = i;
		port_sc->sc_port_type = sc->sc_port_types[i];
		port_sc->sc_port_ops = cn30xxgmx_port_ops[port_sc->sc_port_type];
		status = bus_space_map(sc->sc_regt,
		    aa->aa_unit->addr + GMX0_BASE_PORT_SIZE * i,
		    GMX0_BASE_PORT_SIZE, 0, &port_sc->sc_port_regh);
		if (status != 0)
			panic(": can't map port register");

		(void)memset(&gmx_aa, 0, sizeof(gmx_aa));
		gmx_aa.ga_regt = aa->aa_bust;
		gmx_aa.ga_dmat = aa->aa_dmat;
		gmx_aa.ga_addr = aa->aa_unit->addr;
		gmx_aa.ga_name = "cnmac";
		gmx_aa.ga_portno = i;
		gmx_aa.ga_port_type = sc->sc_port_types[i];
		gmx_aa.ga_gmx = sc;
		gmx_aa.ga_gmx_port = port_sc;
		gmx_aa.ga_phy_addr = cn30xxgmx_port_phy_addr(i);
		if (gmx_aa.ga_phy_addr == -1)
			panic(": don't know phy address for port %d", i);

		config_found_sm(self, &gmx_aa,
		    cn30xxgmx_print, cn30xxgmx_submatch);

#ifdef OCTEON_ETH_DEBUG
		__cn30xxgmx_port_softc[i] = port_sc;
#endif
	}

#ifdef OCTEON_ETH_DEBUG
	if (cn30xxgmx_intr_drop_ih == NULL)
		cn30xxgmx_intr_drop_ih = octeon_intr_establish(
		   ffs64(CIU_INTX_SUM0_GMX_DRP) - 1, IPL_NET,
		   cn30xxgmx_intr_drop, NULL, "cn30xxgmx");
#endif
}

int
cn30xxgmx_print(void *aux, const char *pnp)
{
	struct cn30xxgmx_attach_args *ga = aux;
	static const char *types[] = {
		[GMX_MII_PORT] = "MII",
		[GMX_GMII_PORT] = "GMII",
		[GMX_RGMII_PORT] = "RGMII"
	};

#if DEBUG
	if (pnp)
		printf("%s at %s", ga->ga_name, pnp);
#endif

	printf(": %s", types[ga->ga_port_type]);

	return UNCONF;
}

int
cn30xxgmx_submatch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;

	return (*cf->cf_attach->ca_match)(parent, vcf, aux);
}

int
cn30xxgmx_init(struct cn30xxgmx_softc *sc)
{
	int result = 0;
	uint64_t inf_mode;
	int id;

	inf_mode = bus_space_read_8(sc->sc_regt, sc->sc_regh, GMX0_INF_MODE);
	if ((inf_mode & INF_MODE_EN) == 0) {
		printf("port are disable\n");
		sc->sc_nports = 0;
		result = 1;
		return result;
	}

	id = octeon_get_chipid();

	switch (octeon_model_family(id)) {
	case OCTEON_MODEL_FAMILY_CN31XX:
		/*
		 * CN31XX-HM-1.01
		 * 14.1 Packet Interface Introduction
		 * Table 14-1 Packet Interface Configuration
		 * 14.8 GMX Registers, Interface Mode Register, GMX0_INF_MODE
		 */
		if ((inf_mode & INF_MODE_TYPE) == 0) {
			/* all three ports configured as RGMII */
			sc->sc_nports = 3;
			sc->sc_port_types[0] = GMX_RGMII_PORT;
			sc->sc_port_types[1] = GMX_RGMII_PORT;
			sc->sc_port_types[2] = GMX_RGMII_PORT;
		} else {
			/* port 0: RGMII, port 1: GMII, port 2: disabled */
			/* XXX CN31XX-HM-1.01 says "Port 3: disabled"; typo? */
			sc->sc_nports = 2;
			sc->sc_port_types[0] = GMX_RGMII_PORT;
			sc->sc_port_types[1] = GMX_GMII_PORT;
		}
		break;
	case OCTEON_MODEL_FAMILY_CN30XX:
	case OCTEON_MODEL_FAMILY_CN50XX:
		/*
		 * CN30XX-HM-1.0
		 * 13.1 Packet Interface Introduction
		 * Table 13-1 Packet Interface Configuration
		 * 13.8 GMX Registers, Interface Mode Register, GMX0_INF_MODE
		 */
		if ((inf_mode & INF_MODE_P0MII) == 0)
			sc->sc_port_types[0] = GMX_RGMII_PORT;
		else
			sc->sc_port_types[0] = GMX_MII_PORT;
		if ((inf_mode & INF_MODE_TYPE) == 0) {
			/* port 1 and 2 are configred as RGMII ports */
			sc->sc_nports = 3;
			sc->sc_port_types[1] = GMX_RGMII_PORT;
			sc->sc_port_types[2] = GMX_RGMII_PORT;
		} else {
			/* port 1: GMII/MII, port 2: disabled */
			/* GMII or MII port is slected by GMX_PRT1_CFG[SPEED] */
			sc->sc_nports = 2;
			sc->sc_port_types[1] = GMX_GMII_PORT;
		}
		/* port 2 is in CN3010/CN5010 only */
		if ((octeon_model(id) != OCTEON_MODEL_CN3010) &&
		    (octeon_model(id) != OCTEON_MODEL_CN5010))
			if (sc->sc_nports == 3)
				sc->sc_nports = 2;
		break;
	case OCTEON_MODEL_FAMILY_CN38XX:
	case OCTEON_MODEL_FAMILY_CN56XX:
	case OCTEON_MODEL_FAMILY_CN58XX:
	default:
		printf("unsupported octeon model: 0x%x\n", octeon_get_chipid());
		sc->sc_nports = 0;
		result = 1;
		break;
	}

	return result;
}

/* XXX RGMII specific */
int
cn30xxgmx_link_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t prt_cfg;

	cn30xxgmx_tx_int_enable(sc, enable);
	cn30xxgmx_rx_int_enable(sc, enable);

	prt_cfg = _GMX_PORT_RD8(sc, GMX0_PRT0_CFG);
	if (enable) {
		if (cn30xxgmx_link_status(sc)) {
			SET(prt_cfg, PRTN_CFG_EN);
		}
	} else {
		CLR(prt_cfg, PRTN_CFG_EN);
	}
	_GMX_PORT_WR8(sc, GMX0_PRT0_CFG, prt_cfg);
	/*
	 * According to CN30XX-HM-1.0, 13.4.2 Link Status Changes:
	 * > software should read back to flush the write operation.
	 */
	(void)_GMX_PORT_RD8(sc, GMX0_PRT0_CFG);

	return 0;
}

/* XXX RGMII specific */
int
cn30xxgmx_stats_init(struct cn30xxgmx_port_softc *sc)
{
        _GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS_DRP, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS_BAD, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT0, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT1, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT3, 0x0ULL);
        _GMX_PORT_WR8(sc, GMX0_TX0_STAT9, 0x0ULL);
	return 0;
}

int
cn30xxgmx_tx_stats_rd_clr(struct cn30xxgmx_port_softc *sc, int enable)
{
	_GMX_PORT_WR8(sc, GMX0_TX0_STATS_CTL, enable ? 0x1ULL : 0x0ULL);
	return 0;
}

int
cn30xxgmx_rx_stats_rd_clr(struct cn30xxgmx_port_softc *sc, int enable)
{
	_GMX_PORT_WR8(sc, GMX0_RX0_STATS_CTL, enable ? 0x1ULL : 0x0ULL);
	return 0;
}

void
cn30xxgmx_rx_stats_dec_bad(struct cn30xxgmx_port_softc *sc)
{
	uint64_t tmp;

        tmp = _GMX_PORT_RD8(sc, GMX0_RX0_STATS_PKTS_BAD);
	_GMX_PORT_WR8(sc, GMX0_RX0_STATS_PKTS_BAD, tmp - 1);
}

int
cn30xxgmx_tx_ovr_bp_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t ovr_bp;

	ovr_bp = _GMX_RD8(sc, GMX0_TX_OVR_BP);
	if (enable) {
		CLR(ovr_bp, (1 << sc->sc_port_no) << TX_OVR_BP_EN_SHIFT);
		SET(ovr_bp, (1 << sc->sc_port_no) << TX_OVR_BP_BP_SHIFT);
		/* XXX really??? */
		SET(ovr_bp, (1 << sc->sc_port_no) << TX_OVR_BP_IGN_FULL_SHIFT);
	} else {
		SET(ovr_bp, (1 << sc->sc_port_no) << TX_OVR_BP_EN_SHIFT);
		CLR(ovr_bp, (1 << sc->sc_port_no) << TX_OVR_BP_BP_SHIFT);
		/* XXX really??? */
		SET(ovr_bp, (1 << sc->sc_port_no) << TX_OVR_BP_IGN_FULL_SHIFT);
	}
	_GMX_WR8(sc, GMX0_TX_OVR_BP, ovr_bp);
	return 0;
}

int
cn30xxgmx_rx_pause_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	if (enable) {
		cn30xxgmx_rx_frm_ctl_enable(sc, RXN_FRM_CTL_CTL_BCK);
	} else {
		cn30xxgmx_rx_frm_ctl_disable(sc, RXN_FRM_CTL_CTL_BCK);
	}

	return 0;
}

void
cn30xxgmx_tx_int_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t tx_int_xxx = 0;

	SET(tx_int_xxx,
	    TX_INT_REG_LATE_COL |
	    TX_INT_REG_XSDEF |
	    TX_INT_REG_XSCOL |
	    TX_INT_REG_UNDFLW |
	    TX_INT_REG_PKO_NXA);
	_GMX_WR8(sc, GMX0_TX_INT_REG, tx_int_xxx);
	_GMX_WR8(sc, GMX0_TX_INT_EN, enable ? tx_int_xxx : 0);
}

void
cn30xxgmx_rx_int_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t rx_int_xxx = 0;

	SET(rx_int_xxx, 0 |
	    RXN_INT_REG_PHY_DUPX |
	    RXN_INT_REG_PHY_SPD |
	    RXN_INT_REG_PHY_LINK |
	    RXN_INT_REG_IFGERR |
	    RXN_INT_REG_COLDET |
	    RXN_INT_REG_FALERR |
	    RXN_INT_REG_RSVERR |
	    RXN_INT_REG_PCTERR |
	    RXN_INT_REG_OVRERR |
	    RXN_INT_REG_NIBERR |
	    RXN_INT_REG_SKPERR |
	    RXN_INT_REG_RCVERR |
	    RXN_INT_REG_LENERR |
	    RXN_INT_REG_ALNERR |
	    RXN_INT_REG_FCSERR |
	    RXN_INT_REG_JABBER |
	    RXN_INT_REG_MAXERR |
	    RXN_INT_REG_CAREXT |
	    RXN_INT_REG_MINERR);
	_GMX_PORT_WR8(sc, GMX0_RX0_INT_REG, rx_int_xxx);
	_GMX_PORT_WR8(sc, GMX0_RX0_INT_EN, enable ? rx_int_xxx : 0);
}

int
cn30xxgmx_rx_frm_ctl_enable(struct cn30xxgmx_port_softc *sc,
    uint64_t rx_frm_ctl)
{
	/*
	 * XXX Jumbo-frame Workarounds
	 *     Current implementation of cnmac is required to
	 *     configure GMX0_RX0_JABBER[CNT] as follows:
	 *	RX0_FRM_MAX(1536) <= GMX0_RX0_JABBER <= 1536(0x600)
	 */
	_GMX_PORT_WR8(sc, GMX0_RX0_JABBER, GMX_FRM_MAX_SIZ);

	return cn30xxgmx_rx_frm_ctl_xable(sc, rx_frm_ctl, 1);
}

int
cn30xxgmx_rx_frm_ctl_disable(struct cn30xxgmx_port_softc *sc,
    uint64_t rx_frm_ctl)
{
	return cn30xxgmx_rx_frm_ctl_xable(sc, rx_frm_ctl, 0);
}

int
cn30xxgmx_rx_frm_ctl_xable(struct cn30xxgmx_port_softc *sc,
    uint64_t rx_frm_ctl, int enable)
{
	uint64_t tmp;

	tmp = _GMX_PORT_RD8(sc, GMX0_RX0_FRM_CTL);
	if (enable)
		SET(tmp, rx_frm_ctl);
	else
		CLR(tmp, rx_frm_ctl);
	_GMX_PORT_WR8(sc, GMX0_RX0_FRM_CTL, tmp);

	return 0;
}

int
cn30xxgmx_tx_thresh(struct cn30xxgmx_port_softc *sc, int cnt)
{
	_GMX_PORT_WR8(sc, GMX0_TX0_THRESH, cnt);
	return 0;
}

int
cn30xxgmx_set_mac_addr(struct cn30xxgmx_port_softc *sc, uint8_t *addr)
{
	uint64_t mac = 0;

	ADDR2UINT64(mac, addr);
	(*sc->sc_port_ops->port_ops_set_mac_addr)(sc, addr, mac);
	return 0;
}

int
cn30xxgmx_set_filter(struct cn30xxgmx_port_softc *sc)
{
	(*sc->sc_port_ops->port_ops_set_filter)(sc);
	return 0;
}

int
cn30xxgmx_port_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	(*sc->sc_port_ops->port_ops_enable)(sc, enable);
	return 0;
}

int
cn30xxgmx_reset_speed(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	if (ISSET(sc->sc_port_mii->mii_flags, MIIF_DOINGAUTO)) {
		log(LOG_WARNING,
		    "%s: autonegotiation has not been completed yet\n",
		    ifp->if_xname);
		return 1;
	}
	(*sc->sc_port_ops->port_ops_speed)(sc);
	return 0;
}

int
cn30xxgmx_reset_timing(struct cn30xxgmx_port_softc *sc)
{
	(*sc->sc_port_ops->port_ops_timing)(sc);
	return 0;
}

int
cn30xxgmx_reset_board(struct cn30xxgmx_port_softc *sc)
{

	return 0;
}

int
cn30xxgmx_reset_flowctl(struct cn30xxgmx_port_softc *sc)
{
	struct ifmedia_entry *ife = sc->sc_port_mii->mii_media.ifm_cur;

	/*
	 * Get flow control negotiation result.
	 */
#ifdef GMX_802_3X_DISABLE_AUTONEG
	/* Tentative support for SEIL-compat.. */
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		sc->sc_port_flowflags &= ~IFM_ETH_FMASK;
	}
#else
	/* Default configuration of NetBSD */
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO &&
	    (sc->sc_port_mii->mii_media_active & IFM_ETH_FMASK) !=
			sc->sc_port_flowflags) {
		sc->sc_port_flowflags =
			sc->sc_port_mii->mii_media_active & IFM_ETH_FMASK;
		sc->sc_port_mii->mii_media_active &= ~IFM_ETH_FMASK;
	}
#endif /* GMX_802_3X_DISABLE_AUTONEG */

	/*
	 * 802.3x Flow Control Capabilities
	 */
	if (sc->sc_port_flowflags & IFM_ETH_TXPAUSE) {
		cn30xxgmx_tx_ovr_bp_enable(sc, 1);
	} else {
		cn30xxgmx_tx_ovr_bp_enable(sc, 0);
	}
	if (sc->sc_port_flowflags & IFM_ETH_RXPAUSE) {
		cn30xxgmx_rx_pause_enable(sc, 1);
	} else {
		cn30xxgmx_rx_pause_enable(sc, 0);
	}

	return 0;
}

int
cn30xxgmx_rgmii_enable(struct cn30xxgmx_port_softc *sc, int enable)
{
	uint64_t mode;

	/* XXX */
	mode = _GMX_RD8(sc, GMX0_INF_MODE);
	if (ISSET(mode, INF_MODE_EN)) {
		cn30xxasx_enable(sc->sc_port_asx, 1);
	}

	return 0;
}

int
cn30xxgmx_rgmii_speed(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	uint64_t newlink;
	int baudrate;

	/* XXX */
	cn30xxgmx_link_enable(sc, 1);

	cn30xxgmx_rgmii_speed_newlink(sc, &newlink);
	if (sc->sc_link == newlink) {
		return 0;
	}
#ifdef OCTEON_ETH_DEBUG
	cn30xxgmx_rgmii_speed_newlink_log(sc, newlink);
#endif
	sc->sc_link = newlink;

	switch (sc->sc_link & RXN_RX_INBND_SPEED) {
	case RXN_RX_INBND_SPEED_2_5:
		baudrate = IF_Mbps(10);
		break;
	case RXN_RX_INBND_SPEED_25:
		baudrate = IF_Mbps(100);
		break;
	case RXN_RX_INBND_SPEED_125:
		baudrate = IF_Gbps(1);
		break;
	default:
		baudrate = 0/* XXX */;
		break;
	}
	ifp->if_baudrate = baudrate;

	cn30xxgmx_link_enable(sc, 0);

	/*
	 * According to CN30XX-HM-1.0, 13.4.2 Link Status Changes:
	 * wait a max_packet_time
	 * max_packet_time(us) = (max_packet_size(bytes) * 8) / link_speed(Mbps)
	 */
	delay((GMX_FRM_MAX_SIZ * 8) / (baudrate / 1000000));

	cn30xxgmx_rgmii_speed_speed(sc);

	cn30xxgmx_link_enable(sc, 1);
	cn30xxasx_enable(sc->sc_port_asx, 1);

	return 0;
}

int
cn30xxgmx_rgmii_speed_newlink(struct cn30xxgmx_port_softc *sc,
    uint64_t *rnewlink)
{
	uint64_t newlink;

	/* Inband status does not seem to work */
	newlink = _GMX_PORT_RD8(sc, GMX0_RX0_RX_INBND);

	*rnewlink = newlink;
	return 0;
}

#ifdef OCTEON_ETH_DEBUG
int
cn30xxgmx_rgmii_speed_newlink_log(struct cn30xxgmx_port_softc *sc,
    uint64_t newlink)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	const char *status_str;
	const char *speed_str;
	const char *duplex_str;
	int is_status_changed;
	int is_speed_changed;
	int is_linked;
	char status_buf[80/* XXX */];
	char speed_buf[80/* XXX */];

	is_status_changed = (newlink & RXN_RX_INBND_STATUS) !=
	    (sc->sc_link & RXN_RX_INBND_STATUS);
	is_speed_changed = (newlink & RXN_RX_INBND_SPEED) !=
	    (sc->sc_link & RXN_RX_INBND_SPEED);
	is_linked = ISSET(newlink, RXN_RX_INBND_STATUS);
	if (is_status_changed) {
		if (is_linked)
			status_str = "link up";
		else
			status_str = "link down";
	} else {
		if (is_linked) {
			/* any other conditions? */
			if (is_speed_changed)
				status_str = "link change";
			else
				status_str = NULL;
		} else {
			status_str = NULL;
		}
	}

	if (status_str != NULL) {
		if ((is_speed_changed && is_linked) || is_linked) {
			switch (newlink & RXN_RX_INBND_SPEED) {
			case RXN_RX_INBND_SPEED_2_5:
				speed_str = "10baseT";
				break;
			case RXN_RX_INBND_SPEED_25:
				speed_str = "100baseTX";
				break;
			case RXN_RX_INBND_SPEED_125:
				speed_str = "1000baseT";
				break;
			default:
				panic("Unknown link speed");
				break;
			}

			if (ISSET(newlink, RXN_RX_INBND_DUPLEX))
				duplex_str = "-FDX";
			else
				duplex_str = "";

			(void)snprintf(speed_buf, sizeof(speed_buf), "(%s%s)",
			    speed_str, duplex_str);
		} else {
			speed_buf[0] = '\0';
		}
		(void)snprintf(status_buf, sizeof(status_buf), "%s: %s%s%s\n",
		    ifp->if_xname, status_str, (is_speed_changed | is_linked) ? " " : "",
		    speed_buf);
		log(LOG_CRIT, status_buf);
	}

	return 0;
}
#endif

int
cn30xxgmx_rgmii_speed_speed(struct cn30xxgmx_port_softc *sc)
{
	uint64_t prt_cfg;
	uint64_t tx_clk, tx_slot, tx_burst;

	prt_cfg = _GMX_PORT_RD8(sc, GMX0_PRT0_CFG);

	switch (sc->sc_link & RXN_RX_INBND_SPEED) {
	case RXN_RX_INBND_SPEED_2_5:
		/* 10Mbps */
		/*
		 * "GMX Tx Clock Generation Registers", CN30XX-HM-1.0;
		 * > 8ns x 50 = 400ns (2.5MHz TXC clock)
		 */
		tx_clk = 50;
		/*
		 * "TX Slottime Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set SLOT to 0x40
		 */
		tx_slot = 0x40;
		/*
		 * "TX Burst-Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set BURST to 0x0
		 */
		tx_burst = 0;
		/*
		 * "GMX Tx Port Configuration Registers", CN30XX-HM-1.0;
		 * > Slot time for half-duplex operation
		 * >   0 = 512 bittimes (10/100Mbps operation)
		 */
		CLR(prt_cfg, PRTN_CFG_SLOTTIME);
		/*
		 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
		 * > Link speed
		 * >   0 = 10/100Mbps operation
		 * >     in RGMII mode: GMX0_TX(0..2)_CLK[CLK_CNT] > 1
		 */
		CLR(prt_cfg, PRTN_CFG_SPEED);
		break;
	case RXN_RX_INBND_SPEED_25:
		/* 100Mbps */
		/*
		 * "GMX Tx Clock Generation Registers", CN30XX-HM-1.0;
		 * > 8ns x 5 = 40ns (25.0MHz TXC clock)
		 */
		tx_clk = 5;
		/*
		 * "TX Slottime Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set SLOT to 0x40
		 */
		tx_slot = 0x40;
		/*
		 * "TX Burst-Counter Registers", CN30XX-HM-1.0;
		 * > 10/100Mbps: set BURST to 0x0
		 */
		tx_burst = 0;
		/*
		 * "GMX Tx Port Configuration Registers", CN30XX-HM-1.0;
		 * > Slot time for half-duplex operation
		 * >   0 = 512 bittimes (10/100Mbps operation)
		 */
		CLR(prt_cfg, PRTN_CFG_SLOTTIME);
		/*
		 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
		 * > Link speed
		 * >   0 = 10/100Mbps operation
		 * >     in RGMII mode: GMX0_TX(0..2)_CLK[CLK_CNT] > 1
		 */
		CLR(prt_cfg, PRTN_CFG_SPEED);
		break;
	case RXN_RX_INBND_SPEED_125:
		/* 1000Mbps */
		/*
		 * "GMX Tx Clock Generation Registers", CN30XX-HM-1.0;
		 * > 8ns x 1 = 8ns (125.0MHz TXC clock)
		 */
		tx_clk = 1;
		/*
		 * "TX Slottime Counter Registers", CN30XX-HM-1.0;
		 * > 1000Mbps: set SLOT to 0x200
		 */
		tx_slot = 0x200;
		/*
		 * "TX Burst-Counter Registers", CN30XX-HM-1.0;
		 * > 1000Mbps: set BURST to 0x2000
		 */
		tx_burst = 0x2000;
		/*
		 * "GMX Tx Port Configuration Registers", CN30XX-HM-1.0;
		 * > Slot time for half-duplex operation
		 * >   1 = 4096 bittimes (1000Mbps operation)
		 */
		SET(prt_cfg, PRTN_CFG_SLOTTIME);
		/*
		 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
		 * > Link speed
		 * >   1 = 1000Mbps operation
		 */
		SET(prt_cfg, PRTN_CFG_SPEED);
		break;
	default:
		/* NOT REACHED! */
		/* Following configuration is default value of system.
		*/
		tx_clk = 1;
		tx_slot = 0x200;
		tx_burst = 0x2000;
		SET(prt_cfg, PRTN_CFG_SLOTTIME);
		SET(prt_cfg, PRTN_CFG_SPEED);
		break;
	}

	/* Setup Duplex mode(negotiated) */
	/*
	 * "GMX Port Configuration Registers", CN30XX-HM-1.0;
	 * > Duplex mode: 0 = half-duplex mode, 1=full-duplex
	 */
	if (ISSET(sc->sc_link, RXN_RX_INBND_DUPLEX)) {
		/* Full-Duplex */
		SET(prt_cfg, PRTN_CFG_DUPLEX);
	} else {
		/* Half-Duplex */
		CLR(prt_cfg, PRTN_CFG_DUPLEX);
	}

	_GMX_PORT_WR8(sc, GMX0_TX0_CLK, tx_clk);
	_GMX_PORT_WR8(sc, GMX0_TX0_SLOT, tx_slot);
	_GMX_PORT_WR8(sc, GMX0_TX0_BURST, tx_burst);
	_GMX_PORT_WR8(sc, GMX0_PRT0_CFG, prt_cfg);

	return 0;
}

int
cn30xxgmx_rgmii_timing(struct cn30xxgmx_port_softc *sc)
{
	int clk_tx_setting;
	int clk_rx_setting;
	uint64_t rx_frm_ctl;

	/* RGMII TX Threshold Registers, CN30XX-HM-1.0;
	 * > Number of 16-byte ticks to accumulate in the TX FIFO before
	 * > sending on the RGMII interface. This field should be large
	 * > enough to prevent underflow on the RGMII interface and must
	 * > never be set to less than 0x4. This register cannot exceed
	 * > the TX FIFO depth of 0x40 words.
	 */
	/* Default parameter of CN30XX */
	cn30xxgmx_tx_thresh(sc, 32);

	rx_frm_ctl = 0 |
	    /* RXN_FRM_CTL_NULL_DIS |	(cn5xxx only) */
	    /* RXN_FRM_CTL_PRE_ALIGN |	(cn5xxx only) */
	    /* RXN_FRM_CTL_PAD_LEN |	(cn3xxx only) */
	    /* RXN_FRM_CTL_VLAN_LEN |	(cn3xxx only) */
	    RXN_FRM_CTL_PRE_FREE |
	    RXN_FRM_CTL_CTL_SMAC |
	    RXN_FRM_CTL_CTL_MCST |
	    RXN_FRM_CTL_CTL_DRP |
	    RXN_FRM_CTL_PRE_STRP |
	    RXN_FRM_CTL_PRE_CHK;
	cn30xxgmx_rx_frm_ctl_enable(sc, rx_frm_ctl);

	/* XXX PHY-dependent parameter */
	/* RGMII RX Clock-Delay Registers, CN30XX-HM-1.0;
	 * > Delay setting to place n RXC (RGMII receive clock) delay line.
	 * > The intrinsic delay can range from 50ps to 80ps per tap,
	 * > which corresponds to skews of 1.25ns to 2.00ns at 25 taps(CSR+1).
	 * > This is the best match for the RGMII specification which wants
	 * > 1ns - 2.6ns of skew.
	 */
	/* RGMII TX Clock-Delay Registers, CN30XX-HM-1.0;
	 * > Delay setting to place n TXC (RGMII transmit clock) delay line.
	 * > ...
	 */

	switch (octeon_boot_info->board_type) {
	default:
		/* Default parameter of CN30XX */
		clk_tx_setting = 24;
		clk_rx_setting = 24;
		break;
	case BOARD_TYPE_UBIQUITI_E100:
		clk_tx_setting = 16;
		clk_rx_setting = 0;
		break;
	}

	cn30xxasx_clk_set(sc->sc_port_asx, clk_tx_setting, clk_rx_setting);

	return 0;
}

int
cn30xxgmx_rgmii_set_mac_addr(struct cn30xxgmx_port_softc *sc, uint8_t *addr,
    uint64_t mac)
{
	int i;

	cn30xxgmx_link_enable(sc, 0);

	sc->sc_mac = mac;
	_GMX_PORT_WR8(sc, GMX0_SMAC0, mac);
	for (i = 0; i < 6; i++)
		_GMX_PORT_WR8(sc, cn30xxgmx_rx_adr_cam_regs[i], addr[i]);

	cn30xxgmx_link_enable(sc, 1);

	return 0;
}

#define	OCTEON_ETH_USE_GMX_CAM

int
cn30xxgmx_rgmii_set_filter(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	struct arpcom *ac = sc->sc_port_ac;
#ifdef OCTEON_ETH_USE_GMX_CAM
	struct ether_multi *enm;
	struct ether_multistep step;
#endif
	uint64_t cam_en = 0x01ULL;
	uint64_t ctl = 0;
	int multi = 0;

	cn30xxgmx_link_enable(sc, 0);

	SET(ctl, RXN_ADR_CTL_CAM_MODE);
	CLR(ctl, RXN_ADR_CTL_MCST_ACCEPT | RXN_ADR_CTL_MCST_AFCAM | RXN_ADR_CTL_MCST_REJECT);
	CLR(ifp->if_flags, IFF_ALLMULTI);

	/*
	 * Always accept broadcast frames.
	 */
	SET(ctl, RXN_ADR_CTL_BCST);

	if (ISSET(ifp->if_flags, IFF_PROMISC)) {
		SET(ifp->if_flags, IFF_ALLMULTI);
		CLR(ctl, RXN_ADR_CTL_CAM_MODE);
		SET(ctl, RXN_ADR_CTL_MCST_ACCEPT);
		cam_en = 0x00ULL;
	} else if (ac->ac_multirangecnt > 0 || ac->ac_multicnt > 7) {
		SET(ifp->if_flags, IFF_ALLMULTI);
		SET(ctl, RXN_ADR_CTL_MCST_ACCEPT);
	} else {
#ifdef OCTEON_ETH_USE_GMX_CAM
		/*
		 * Note first entry is self MAC address; other 7 entires are available
		 * for multicast addresses.
		 */
		ETHER_FIRST_MULTI(step, sc->sc_port_ac, enm);
		while (enm != NULL) {
			int i;

			dprintf("%d: %02x:%02x:%02x:%02x:%02x:%02x\n"
			    multi + 1,
			    enm->enm_addrlo[0], enm->enm_addrlo[1],
			    enm->enm_addrlo[2], enm->enm_addrlo[3],
			    enm->enm_addrlo[4], enm->enm_addrlo[5]);
			multi++;

			SET(cam_en, 1ULL << multi); /* XXX */

			for (i = 0; i < 6; i++) {
				uint64_t tmp;

				/* XXX */
				tmp = _GMX_PORT_RD8(sc, cn30xxgmx_rx_adr_cam_regs[i]);
				CLR(tmp, 0xffULL << (8 * multi));
				SET(tmp, (uint64_t)enm->enm_addrlo[i] << (8 * multi));
				_GMX_PORT_WR8(sc, cn30xxgmx_rx_adr_cam_regs[i], tmp);
			}

			for (i = 0; i < 6; i++)
				dprintf("cam%d = %016llx\n", i,
				    _GMX_PORT_RD8(sc, cn30xxgmx_rx_adr_cam_regs[i]));

			ETHER_NEXT_MULTI(step, enm);
		}

		if (multi)
			SET(ctl, RXN_ADR_CTL_MCST_AFCAM);
		else
			SET(ctl, RXN_ADR_CTL_MCST_REJECT);

		OCTEON_ETH_KASSERT(enm == NULL);
#else
		/*
		 * XXX
		 * Never use DMAC filter for multicast addresses, but register only
		 * single entry for self address.  FreeBSD code do so.
		 */
		SET(ifp->if_flags, IFF_ALLMULTI);
		SET(ctl, RXN_ADR_CTL_MCST_ACCEPT);
#endif
	}

	dprintf("ctl = %llx, cam_en = %llx\n", ctl, cam_en);
	_GMX_PORT_WR8(sc, GMX0_RX0_ADR_CTL, ctl);
	_GMX_PORT_WR8(sc, GMX0_RX0_ADR_CAM_EN, cam_en);

	cn30xxgmx_link_enable(sc, 1);

	return 0;
}

void
cn30xxgmx_stats(struct cn30xxgmx_port_softc *sc)
{
	struct ifnet *ifp = &sc->sc_port_ac->ac_if;
	uint64_t tmp;

	ifp->if_ipackets +=
	    (uint32_t)_GMX_PORT_RD8(sc, GMX0_RX0_STATS_PKTS);
	ifp->if_ierrors +=
	    (uint32_t)_GMX_PORT_RD8(sc, GMX0_RX0_STATS_PKTS_BAD);
	ifp->if_iqdrops +=
	    (uint32_t)_GMX_PORT_RD8(sc, GMX0_RX0_STATS_PKTS_DRP);
	ifp->if_opackets +=
	    (uint32_t)_GMX_PORT_RD8(sc, GMX0_TX0_STAT3);
	tmp = _GMX_PORT_RD8(sc, GMX0_TX0_STAT0);
	ifp->if_oerrors +=
	    (uint32_t)tmp + ((uint32_t)(tmp >> 32) * 16);
	ifp->if_collisions += ((uint32_t)tmp) * 16;
	tmp = _GMX_PORT_RD8(sc, GMX0_TX0_STAT1);
	ifp->if_collisions +=
	    ((uint32_t)tmp * 2) + (uint32_t)(tmp >> 32);
	tmp = _GMX_PORT_RD8(sc, GMX0_TX0_STAT9);
	ifp->if_oerrors += (uint32_t)(tmp >> 32);
}

/* ---- DMAC filter */

#ifdef notyet
/*
 * DMAC filter configuration
 *	accept all
 *	reject 0 addrs (virtually accept all?)
 *	reject N addrs
 *	accept N addrs
 *	accept 0 addrs (virtually reject all?)
 *	reject all
 */

/* XXX local namespace */
#define	_POLICY			CN30XXGMX_FILTER_POLICY
#define	_POLICY_ACCEPT_ALL	CN30XXGMX_FILTER_POLICY_ACCEPT_ALL
#define	_POLICY_ACCEPT		CN30XXGMX_FILTER_POLICY_ACCEPT
#define	_POLICY_REJECT		CN30XXGMX_FILTER_POLICY_REJECT
#define	_POLICY_REJECT_ALL	CN30XXGMX_FILTER_POLICY_REJECT_ALL

int	cn30xxgmx_setfilt_addrs(struct cn30xxgmx_port_softc *,
	    size_t, uint8_t **);

int
cn30xxgmx_setfilt(struct cn30xxgmx_port_softc *sc, enum _POLICY policy,
    size_t naddrs, uint8_t **addrs)
{
	uint64_t rx_adr_ctl;

	KASSERT(policy >= _POLICY_ACCEPT_ALL);
	KASSERT(policy <= _POLICY_REJECT_ALL);

	rx_adr_ctl = _GMX_PORT_RD8(sc, GMX0_RX0_ADR_CTL);
	CLR(rx_adr_ctl, RXN_ADR_CTL_CAM_MODE | RXN_ADR_CTL_MCST);

	switch (policy) {
	case _POLICY_ACCEPT_ALL:
	case _POLICY_REJECT_ALL:
		KASSERT(naddrs == 0);
		KASSERT(addrs == NULL);

		SET(rx_adr_ctl, (policy == _POLICY_ACCEPT_ALL) ?
		    RXN_ADR_CTL_MCST_ACCEPT : RXN_ADR_CTL_MCST_REJECT);
		break;
	case _POLICY_ACCEPT:
	case _POLICY_REJECT:
		if (naddrs > CN30XXGMX_FILTER_NADDRS_MAX)
			return E2BIG;
		SET(rx_adr_ctl, (policy == _POLICY_ACCEPT) ?
		    RXN_ADR_CTL_CAM_MODE : 0);
		SET(rx_adr_ctl, RXN_ADR_CTL_MCST_AFCAM);
		/* set GMX0_RXN_ADR_CAM_EN, GMX0_RXN_ADR_CAM[0-5] */
		cn30xxgmx_setfilt_addrs(sc, naddrs, addrs);
		break;
	}

	/* set GMX0_RXN_ADR_CTL[MCST] */
	_GMX_PORT_WR8(sc, GMX0_RX0_ADR_CTL, rx_adr_ctl);

	return 0;
}

int
cn30xxgmx_setfilt_addrs(struct cn30xxgmx_port_softc *sc, size_t naddrs,
    uint8_t **addrs)
{
	uint64_t rx_adr_cam_en;
	uint64_t rx_adr_cam_addrs[CN30XXGMX_FILTER_NADDRS_MAX];
	int i, j;

	KASSERT(naddrs <= CN30XXGMX_FILTER_NADDRS_MAX);

	rx_adr_cam_en = 0;
	(void)memset(rx_adr_cam_addrs, 0, sizeof(rx_adr_cam_addrs));

	for (i = 0; i < naddrs; i++) {
		SET(rx_adr_cam_en, 1ULL << i);
		for (j = 0; j < 6; j++)
			SET(rx_adr_cam_addrs[j],
			    (uint64_t)addrs[i][j] << (8 * i));
	}

	/* set GMX0_RXN_ADR_CAM_EN, GMX0_RXN_ADR_CAM[0-5] */
	_GMX_PORT_WR8(sc, GMX0_RX0_ADR_CAM_EN, rx_adr_cam_en);
	for (j = 0; j < 6; j++)
		_GMX_PORT_WR8(sc, cn30xxgmx_rx_adr_cam_regs[j],
		    rx_adr_cam_addrs[j]);

	return 0;
}
#endif

/* ---- interrupt */

#ifdef OCTEON_ETH_DEBUG
void	cn30xxgmx_intr_rml_gmx0(void);

int	cn30xxgmx_intr_rml_verbose;

void
cn30xxgmx_intr_rml_gmx0(void)
{
	struct cn30xxgmx_port_softc *sc;
	int i;
	uint64_t reg;

	sc = __cn30xxgmx_port_softc[0];
	if (sc == NULL)
		return;

	/* GMX0_RXn_INT_REG or GMX0_TXn_INT_REG */
	reg = cn30xxgmx_get_tx_int_reg(sc);
	if (cn30xxgmx_intr_rml_verbose && reg != 0)
		printf("%s: GMX_TX_INT_REG=0x%016llx\n", __func__, reg);

	for (i = 0; i < GMX_PORT_NUNITS; i++) {
		sc = __cn30xxgmx_port_softc[i];
		if (sc == NULL)
			continue;
		reg = cn30xxgmx_get_rx_int_reg(sc);
		if (cn30xxgmx_intr_rml_verbose)
			printf("%s: GMX_RX_INT_REG=0x%016llx\n", __func__, reg);
	}
}

int
cn30xxgmx_intr_drop(void *arg)
{
	octeon_xkphys_write_8(CIU_INT0_SUM0, CIU_INTX_SUM0_GMX_DRP);
	return (1);
}

uint64_t
cn30xxgmx_get_rx_int_reg(struct cn30xxgmx_port_softc *sc)
{
	uint64_t reg;
	uint64_t rx_int_reg = 0;

	reg = _GMX_PORT_RD8(sc, GMX0_RX0_INT_REG);
	/* clear */
	SET(rx_int_reg, 0 |
	    RXN_INT_REG_PHY_DUPX |
	    RXN_INT_REG_PHY_SPD |
	    RXN_INT_REG_PHY_LINK |
	    RXN_INT_REG_IFGERR |
	    RXN_INT_REG_COLDET |
	    RXN_INT_REG_FALERR |
	    RXN_INT_REG_RSVERR |
	    RXN_INT_REG_PCTERR |
	    RXN_INT_REG_OVRERR |
	    RXN_INT_REG_NIBERR |
	    RXN_INT_REG_SKPERR |
	    RXN_INT_REG_RCVERR |
	    RXN_INT_REG_LENERR |
	    RXN_INT_REG_ALNERR |
	    RXN_INT_REG_FCSERR |
	    RXN_INT_REG_JABBER |
	    RXN_INT_REG_MAXERR |
	    RXN_INT_REG_CAREXT |
	    RXN_INT_REG_MINERR);
	_GMX_PORT_WR8(sc, GMX0_RX0_INT_REG, rx_int_reg);

	return reg;
}

uint64_t
cn30xxgmx_get_tx_int_reg(struct cn30xxgmx_port_softc *sc)
{
	uint64_t reg;
	uint64_t tx_int_reg = 0;

	reg = _GMX_PORT_RD8(sc, GMX0_TX_INT_REG);
	/* clear */
	SET(tx_int_reg, 0 |
	    TX_INT_REG_LATE_COL |
	    TX_INT_REG_XSDEF |
	    TX_INT_REG_XSCOL |
	    TX_INT_REG_UNDFLW |
	    TX_INT_REG_PKO_NXA);
	_GMX_PORT_WR8(sc, GMX0_TX_INT_REG, tx_int_reg);

	return reg;
}
#endif	/* OCTEON_ETH_DEBUG */

/* ---- debug */

#ifdef OCTEON_ETH_DEBUG
#define	_ENTRY(x)	{ #x, x }

struct cn30xxgmx_dump_reg_ {
	const char *name;
	size_t	offset;
};

const struct cn30xxgmx_dump_reg_ cn30xxgmx_dump_regs_[] = {
	_ENTRY(GMX0_SMAC0),
	_ENTRY(GMX0_BIST0),
	_ENTRY(GMX0_RX_PRTS),
	_ENTRY(GMX0_RX_BP_DROP0),
	_ENTRY(GMX0_RX_BP_DROP1),
	_ENTRY(GMX0_RX_BP_DROP2),
	_ENTRY(GMX0_RX_BP_ON0),
	_ENTRY(GMX0_RX_BP_ON1),
	_ENTRY(GMX0_RX_BP_ON2),
	_ENTRY(GMX0_RX_BP_OFF0),
	_ENTRY(GMX0_RX_BP_OFF1),
	_ENTRY(GMX0_RX_BP_OFF2),
	_ENTRY(GMX0_TX_PRTS),
	_ENTRY(GMX0_TX_IFG),
	_ENTRY(GMX0_TX_JAM),
	_ENTRY(GMX0_TX_COL_ATTEMPT),
	_ENTRY(GMX0_TX_PAUSE_PKT_DMAC),
	_ENTRY(GMX0_TX_PAUSE_PKT_TYPE),
	_ENTRY(GMX0_TX_OVR_BP),
	_ENTRY(GMX0_TX_BP),
	_ENTRY(GMX0_TX_CORRUPT),
	_ENTRY(GMX0_RX_PRT_INFO),
	_ENTRY(GMX0_TX_LFSR),
	_ENTRY(GMX0_TX_INT_REG),
	_ENTRY(GMX0_TX_INT_EN),
	_ENTRY(GMX0_NXA_ADR),
	_ENTRY(GMX0_BAD_REG),
	_ENTRY(GMX0_STAT_BP),
	_ENTRY(GMX0_TX_CLK_MSK0),
	_ENTRY(GMX0_TX_CLK_MSK1),
	_ENTRY(GMX0_RX_TX_STATUS),
	_ENTRY(GMX0_INF_MODE),
};

const struct cn30xxgmx_dump_reg_ cn30xxgmx_dump_port_regs_[] = {
	_ENTRY(GMX0_RX0_INT_REG),
	_ENTRY(GMX0_RX0_INT_EN),
	_ENTRY(GMX0_PRT0_CFG),
	_ENTRY(GMX0_RX0_FRM_CTL),
	_ENTRY(GMX0_RX0_FRM_CHK),
	_ENTRY(GMX0_RX0_FRM_MIN),
	_ENTRY(GMX0_RX0_FRM_MAX),
	_ENTRY(GMX0_RX0_JABBER),
	_ENTRY(GMX0_RX0_DECISION),
	_ENTRY(GMX0_RX0_UDD_SKP),
	_ENTRY(GMX0_RX0_STATS_CTL),
	_ENTRY(GMX0_RX0_IFG),
	_ENTRY(GMX0_RX0_RX_INBND),
	_ENTRY(GMX0_RX0_ADR_CTL),
	_ENTRY(GMX0_RX0_ADR_CAM_EN),
	_ENTRY(GMX0_RX0_ADR_CAM0),
	_ENTRY(GMX0_RX0_ADR_CAM1),
	_ENTRY(GMX0_RX0_ADR_CAM2),
	_ENTRY(GMX0_RX0_ADR_CAM3),
	_ENTRY(GMX0_RX0_ADR_CAM4),
	_ENTRY(GMX0_RX0_ADR_CAM5),
	_ENTRY(GMX0_TX0_CLK),
	_ENTRY(GMX0_TX0_THRESH),
	_ENTRY(GMX0_TX0_APPEND),
	_ENTRY(GMX0_TX0_SLOT),
	_ENTRY(GMX0_TX0_BURST),
	_ENTRY(GMX0_TX0_PAUSE_PKT_TIME),
	_ENTRY(GMX0_TX0_MIN_PKT),
	_ENTRY(GMX0_TX0_PAUSE_PKT_INTERVAL),
	_ENTRY(GMX0_TX0_SOFT_PAUSE),
	_ENTRY(GMX0_TX0_PAUSE_TOGO),
	_ENTRY(GMX0_TX0_PAUSE_ZERO),
	_ENTRY(GMX0_TX0_STATS_CTL),
	_ENTRY(GMX0_TX0_CTL),
};

const struct cn30xxgmx_dump_reg_ cn30xxgmx_dump_port_stats_[] = {
	_ENTRY(GMX0_RX0_STATS_PKTS),
	_ENTRY(GMX0_RX0_STATS_OCTS),
	_ENTRY(GMX0_RX0_STATS_PKTS_CTL),
	_ENTRY(GMX0_RX0_STATS_OCTS_CTL),
	_ENTRY(GMX0_RX0_STATS_PKTS_DMAC),
	_ENTRY(GMX0_RX0_STATS_OCTS_DMAC),
	_ENTRY(GMX0_RX0_STATS_PKTS_DRP),
	_ENTRY(GMX0_RX0_STATS_OCTS_DRP),
	_ENTRY(GMX0_RX0_STATS_PKTS_BAD),
	_ENTRY(GMX0_TX0_STAT0),
	_ENTRY(GMX0_TX0_STAT1),
	_ENTRY(GMX0_TX0_STAT2),
	_ENTRY(GMX0_TX0_STAT3),
	_ENTRY(GMX0_TX0_STAT4),
	_ENTRY(GMX0_TX0_STAT5),
	_ENTRY(GMX0_TX0_STAT6),
	_ENTRY(GMX0_TX0_STAT7),
	_ENTRY(GMX0_TX0_STAT8),
	_ENTRY(GMX0_TX0_STAT9),
};

void	cn30xxgmx_dump_common(void);
void	cn30xxgmx_dump_port0(void);
void	cn30xxgmx_dump_port1(void);
void	cn30xxgmx_dump_port2(void);
void	cn30xxgmx_dump_port0_regs(void);
void	cn30xxgmx_dump_port1_regs(void);
void	cn30xxgmx_dump_port2_regs(void);
void	cn30xxgmx_dump_port0_stats(void);
void	cn30xxgmx_dump_port1_stats(void);
void	cn30xxgmx_dump_port2_stats(void);
void	cn30xxgmx_dump_port_regs(int);
void	cn30xxgmx_dump_port_stats(int);
void	cn30xxgmx_dump_common_x(int, const struct cn30xxgmx_dump_reg_ *, size_t);
void	cn30xxgmx_dump_port_x(int, const struct cn30xxgmx_dump_reg_ *, size_t);
void	cn30xxgmx_dump_x(int, const struct cn30xxgmx_dump_reg_ *, size_t, size_t, int);
void	cn30xxgmx_dump_x_index(char *, size_t, int);

void
cn30xxgmx_dump(void)
{
	cn30xxgmx_dump_common();
	cn30xxgmx_dump_port0();
	cn30xxgmx_dump_port1();
	cn30xxgmx_dump_port2();
}

void
cn30xxgmx_dump_common(void)
{
	cn30xxgmx_dump_common_x(0, cn30xxgmx_dump_regs_,
	    nitems(cn30xxgmx_dump_regs_));
}

void
cn30xxgmx_dump_port0(void)
{
	cn30xxgmx_dump_port_regs(0);
	cn30xxgmx_dump_port_stats(0);
}

void
cn30xxgmx_dump_port1(void)
{
	cn30xxgmx_dump_port_regs(1);
	cn30xxgmx_dump_port_stats(1);
}

void
cn30xxgmx_dump_port2(void)
{
	cn30xxgmx_dump_port_regs(2);
	cn30xxgmx_dump_port_stats(2);
}

void
cn30xxgmx_dump_port_regs(int portno)
{
	cn30xxgmx_dump_port_x(portno, cn30xxgmx_dump_port_regs_,
	    nitems(cn30xxgmx_dump_port_regs_));
}

void
cn30xxgmx_dump_port_stats(int portno)
{
	struct cn30xxgmx_port_softc *sc = __cn30xxgmx_port_softc[0];
	uint64_t rx_stats_ctl;
	uint64_t tx_stats_ctl;

	rx_stats_ctl = _GMX_RD8(sc, GMX0_BASE_PORT_SIZE * portno + GMX0_RX0_STATS_CTL);
	_GMX_WR8(sc, GMX0_BASE_PORT_SIZE * portno + GMX0_RX0_STATS_CTL,
	    rx_stats_ctl & ~RXN_STATS_CTL_RD_CLR);
	tx_stats_ctl = _GMX_RD8(sc, GMX0_BASE_PORT_SIZE * portno + GMX0_TX0_STATS_CTL);
	_GMX_WR8(sc, GMX0_BASE_PORT_SIZE * portno + GMX0_TX0_STATS_CTL,
	    tx_stats_ctl & ~TXN_STATS_CTL_RD_CLR);
	cn30xxgmx_dump_port_x(portno, cn30xxgmx_dump_port_stats_,
	    nitems(cn30xxgmx_dump_port_stats_));
	_GMX_WR8(sc, GMX0_BASE_PORT_SIZE * portno + GMX0_RX0_STATS_CTL, rx_stats_ctl);
	_GMX_WR8(sc, GMX0_BASE_PORT_SIZE * portno + GMX0_TX0_STATS_CTL, tx_stats_ctl);
}

void
cn30xxgmx_dump_common_x(int portno, const struct cn30xxgmx_dump_reg_ *regs, size_t size)
{
	cn30xxgmx_dump_x(portno, regs, size, 0, 0);
}

void
cn30xxgmx_dump_port_x(int portno, const struct cn30xxgmx_dump_reg_ *regs, size_t size)
{
	cn30xxgmx_dump_x(portno, regs, size, GMX0_BASE_PORT_SIZE * portno, 1);
}

void
cn30xxgmx_dump_x(int portno, const struct cn30xxgmx_dump_reg_ *regs, size_t size, size_t base, int index)
{
	struct cn30xxgmx_port_softc *sc = __cn30xxgmx_port_softc[0];
	const struct cn30xxgmx_dump_reg_ *reg;
	uint64_t tmp;
	char name[64];
	int i;

	for (i = 0; i < (int)size; i++) {
		reg = &regs[i];
		tmp = _GMX_RD8(sc, base + reg->offset);

		snprintf(name, sizeof(name), "%s", reg->name);
		if (index > 0)
			cn30xxgmx_dump_x_index(name, sizeof(name), portno);

		printf("\t%-24s: %016llx\n", name, tmp);
	}
}

/* not in libkern */
static char *strstr(const char *, const char *);
static char *
strstr(const char *s, const char *find)
{
        char c, sc;
        size_t len;

        if ((c = *find++) != 0) {
                len = strlen(find);
                do {
                        do {
                                if ((sc = *s++) == 0)
                                        return (NULL);
                        } while (sc != c);
                } while (strncmp(s, find, len) != 0);
                s--;
        }
        return (char *)s;
}

void
cn30xxgmx_dump_x_index(char *buf, size_t len, int index)
{
	static const char *patterns[] = { "_TX0_", "_RX0_", "_PRT0_" };
	int i;

	for (i = 0; i < (int)nitems(patterns); i++) {
		char *p;

		p = strstr(buf, patterns[i]);
		if (p == NULL)
			continue;
		p = strchr(p, '0');
		KASSERT(p != NULL);
		*p = '0' + index;
		return;
	}
}

void
cn30xxgmx_debug_reset(void)
{
	int i;

	for (i = 0; i < 3; i++)
		cn30xxgmx_link_enable(__cn30xxgmx_port_softc[i], 0);
	for (i = 0; i < 3; i++)
		cn30xxgmx_link_enable(__cn30xxgmx_port_softc[i], 1);
}
#endif
