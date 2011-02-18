/*	$OpenBSD: if_alc.c,v 1.10 2011/02/18 17:20:15 mikeb Exp $	*/
/*-
 * Copyright (c) 2009, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Driver for Atheros AR8131/AR8132 PCIe Ethernet. */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/rndvar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_alcreg.h>

int	alc_match(struct device *, void *, void *);
void	alc_attach(struct device *, struct device *, void *);
int	alc_detach(struct device *, int);
int	alc_activate(struct device *, int);

int	alc_init(struct ifnet *);
void	alc_start(struct ifnet *);
int	alc_ioctl(struct ifnet *, u_long, caddr_t);
void	alc_watchdog(struct ifnet *);
int	alc_mediachange(struct ifnet *);
void	alc_mediastatus(struct ifnet *, struct ifmediareq *);

void	alc_aspm(struct alc_softc *);
void	alc_disable_l0s_l1(struct alc_softc *);
int	alc_dma_alloc(struct alc_softc *);
void	alc_dma_free(struct alc_softc *);
int	alc_encap(struct alc_softc *, struct mbuf **);
void	alc_get_macaddr(struct alc_softc *);
void	alc_init_cmb(struct alc_softc *);
void	alc_init_rr_ring(struct alc_softc *);
int	alc_init_rx_ring(struct alc_softc *);
void	alc_init_smb(struct alc_softc *);
void	alc_init_tx_ring(struct alc_softc *);
int	alc_intr(void *);
void	alc_mac_config(struct alc_softc *);
int	alc_miibus_readreg(struct device *, int, int);
void	alc_miibus_statchg(struct device *);
void	alc_miibus_writereg(struct device *, int, int, int);
int	alc_newbuf(struct alc_softc *, struct alc_rxdesc *);
void	alc_phy_down(struct alc_softc *);
void	alc_phy_reset(struct alc_softc *);
void	alc_reset(struct alc_softc *);
void	alc_rxeof(struct alc_softc *, struct rx_rdesc *);
int	alc_rxintr(struct alc_softc *);
void	alc_iff(struct alc_softc *);
void	alc_rxvlan(struct alc_softc *);
void	alc_start_queue(struct alc_softc *);
void	alc_stats_clear(struct alc_softc *);
void	alc_stats_update(struct alc_softc *);
void	alc_stop(struct alc_softc *);
void	alc_stop_mac(struct alc_softc *);
void	alc_stop_queue(struct alc_softc *);
void	alc_tick(void *);
void	alc_txeof(struct alc_softc *);

uint32_t alc_dma_burst[] = { 128, 256, 512, 1024, 2048, 4096, 0 };

const struct pci_matchid alc_devices[] = {
	{ PCI_VENDOR_ATTANSIC, PCI_PRODUCT_ATTANSIC_L1C },
	{ PCI_VENDOR_ATTANSIC, PCI_PRODUCT_ATTANSIC_L2C }
};

struct cfattach alc_ca = {
	sizeof (struct alc_softc), alc_match, alc_attach, NULL,
	alc_activate
};

struct cfdriver alc_cd = {
	NULL, "alc", DV_IFNET
};

int alcdebug = 0;
#define	DPRINTF(x)	do { if (alcdebug) printf x; } while (0)

#define ALC_CSUM_FEATURES	(M_TCPV4_CSUM_OUT | M_UDPV4_CSUM_OUT)

int
alc_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct alc_softc *sc = (struct alc_softc *)dev;
	uint32_t v;
	int i;

	if (phy != sc->alc_phyaddr)
		return (0);

	CSR_WRITE_4(sc, ALC_MDIO, MDIO_OP_EXECUTE | MDIO_OP_READ |
	    MDIO_SUP_PREAMBLE | MDIO_CLK_25_4 | MDIO_REG_ADDR(reg));
	for (i = ALC_PHY_TIMEOUT; i > 0; i--) {
		DELAY(5);
		v = CSR_READ_4(sc, ALC_MDIO);
		if ((v & (MDIO_OP_EXECUTE | MDIO_OP_BUSY)) == 0)
			break;
	}

	if (i == 0) {
		printf("%s: phy read timeout: phy %d, reg %d\n",
		    sc->sc_dev.dv_xname, phy, reg);
		return (0);
	}

	return ((v & MDIO_DATA_MASK) >> MDIO_DATA_SHIFT);
}

void
alc_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct alc_softc *sc = (struct alc_softc *)dev;
	uint32_t v;
	int i;

	if (phy != sc->alc_phyaddr)
		return;

	CSR_WRITE_4(sc, ALC_MDIO, MDIO_OP_EXECUTE | MDIO_OP_WRITE |
	    (val & MDIO_DATA_MASK) << MDIO_DATA_SHIFT |
	    MDIO_SUP_PREAMBLE | MDIO_CLK_25_4 | MDIO_REG_ADDR(reg));
	for (i = ALC_PHY_TIMEOUT; i > 0; i--) {
		DELAY(5);
		v = CSR_READ_4(sc, ALC_MDIO);
		if ((v & (MDIO_OP_EXECUTE | MDIO_OP_BUSY)) == 0)
			break;
	}

	if (i == 0)
		printf("%s: phy write timeout: phy %d, reg %d\n",
		    sc->sc_dev.dv_xname, phy, reg);
}

void
alc_miibus_statchg(struct device *dev)
{
	struct alc_softc *sc = (struct alc_softc *)dev;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mii_data *mii;
	uint32_t reg;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	mii = &sc->sc_miibus;

	sc->alc_flags &= ~ALC_FLAG_LINK;
	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
			sc->alc_flags |= ALC_FLAG_LINK;
			break;
		case IFM_1000_T:
			if ((sc->alc_flags & ALC_FLAG_FASTETHER) == 0)
				sc->alc_flags |= ALC_FLAG_LINK;
			break;
		default:
			break;
		}
	}
	alc_stop_queue(sc);
	/* Stop Rx/Tx MACs. */
	alc_stop_mac(sc);

	/* Program MACs with resolved speed/duplex/flow-control. */
	if ((sc->alc_flags & ALC_FLAG_LINK) != 0) {
		alc_start_queue(sc);
		alc_mac_config(sc);
		/* Re-enable Tx/Rx MACs. */
		reg = CSR_READ_4(sc, ALC_MAC_CFG);
		reg |= MAC_CFG_TX_ENB | MAC_CFG_RX_ENB;
		CSR_WRITE_4(sc, ALC_MAC_CFG, reg);
	}
	alc_aspm(sc);
}

void
alc_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct alc_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_miibus;

	mii_pollstat(mii);
	ifmr->ifm_status = mii->mii_media_status;
	ifmr->ifm_active = mii->mii_media_active;
}

int
alc_mediachange(struct ifnet *ifp)
{
	struct alc_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_miibus;
	int error;

	if (mii->mii_instance != 0) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	error = mii_mediachg(mii);

	return (error);
}

int
alc_match(struct device *dev, void *match, void *aux)
{
	return pci_matchbyid((struct pci_attach_args *)aux, alc_devices,
	    nitems(alc_devices));
}

void
alc_get_macaddr(struct alc_softc *sc)
{
	uint32_t ea[2], opt;
	int i;

	opt = CSR_READ_4(sc, ALC_OPT_CFG);
	if ((CSR_READ_4(sc, ALC_TWSI_DEBUG) & TWSI_DEBUG_DEV_EXIST) != 0) {
		/*
		 * EEPROM found, let TWSI reload EEPROM configuration.
		 * This will set ethernet address of controller.
		 */
		if ((opt & OPT_CFG_CLK_ENB) == 0) {
			opt |= OPT_CFG_CLK_ENB;
			CSR_WRITE_4(sc, ALC_OPT_CFG, opt);
			CSR_READ_4(sc, ALC_OPT_CFG);
			DELAY(1000);
		}
		CSR_WRITE_4(sc, ALC_TWSI_CFG, CSR_READ_4(sc, ALC_TWSI_CFG) |
		    TWSI_CFG_SW_LD_START);
		for (i = 100; i > 0; i--) {
			DELAY(1000);
			if ((CSR_READ_4(sc, ALC_TWSI_CFG) &
			    TWSI_CFG_SW_LD_START) == 0)
				break;
		}
		if (i == 0)
			printf("%s: reloading EEPROM timeout!\n", 
			    sc->sc_dev.dv_xname);
	} else {
		if (alcdebug)
			printf("%s: EEPROM not found!\n", sc->sc_dev.dv_xname);
	}
	if ((opt & OPT_CFG_CLK_ENB) != 0) {
		opt &= ~OPT_CFG_CLK_ENB;
		CSR_WRITE_4(sc, ALC_OPT_CFG, opt);
		CSR_READ_4(sc, ALC_OPT_CFG);
		DELAY(1000);
	}

	ea[0] = CSR_READ_4(sc, ALC_PAR0);
	ea[1] = CSR_READ_4(sc, ALC_PAR1);
	sc->alc_eaddr[0] = (ea[1] >> 8) & 0xFF;
	sc->alc_eaddr[1] = (ea[1] >> 0) & 0xFF;
	sc->alc_eaddr[2] = (ea[0] >> 24) & 0xFF;
	sc->alc_eaddr[3] = (ea[0] >> 16) & 0xFF;
	sc->alc_eaddr[4] = (ea[0] >> 8) & 0xFF;
	sc->alc_eaddr[5] = (ea[0] >> 0) & 0xFF;
}

void
alc_disable_l0s_l1(struct alc_softc *sc)
{
	uint32_t pmcfg;

	/* Another magic from vendor. */
	pmcfg = CSR_READ_4(sc, ALC_PM_CFG);
	pmcfg &= ~(PM_CFG_L1_ENTRY_TIMER_MASK | PM_CFG_CLK_SWH_L1 |
	    PM_CFG_ASPM_L0S_ENB | PM_CFG_ASPM_L1_ENB | PM_CFG_MAC_ASPM_CHK |
	    PM_CFG_SERDES_PD_EX_L1);
	pmcfg |= PM_CFG_SERDES_BUDS_RX_L1_ENB | PM_CFG_SERDES_PLL_L1_ENB |
	    PM_CFG_SERDES_L1_ENB;
	CSR_WRITE_4(sc, ALC_PM_CFG, pmcfg);
}

void
alc_phy_reset(struct alc_softc *sc)
{
	uint16_t data;

	/* Reset magic from Linux. */
	CSR_WRITE_2(sc, ALC_GPHY_CFG,
	    GPHY_CFG_HIB_EN | GPHY_CFG_HIB_PULSE | GPHY_CFG_SEL_ANA_RESET);
	CSR_READ_2(sc, ALC_GPHY_CFG);
	DELAY(10 * 1000);

	CSR_WRITE_2(sc, ALC_GPHY_CFG,
	    GPHY_CFG_EXT_RESET | GPHY_CFG_HIB_EN | GPHY_CFG_HIB_PULSE |
	    GPHY_CFG_SEL_ANA_RESET);
	CSR_READ_2(sc, ALC_GPHY_CFG);
	DELAY(10 * 1000);

	/* Load DSP codes, vendor magic. */
	data = ANA_LOOP_SEL_10BT | ANA_EN_MASK_TB | ANA_EN_10BT_IDLE |
	    ((1 << ANA_INTERVAL_SEL_TIMER_SHIFT) & ANA_INTERVAL_SEL_TIMER_MASK);
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_ADDR, MII_ANA_CFG18);
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_DATA, data);

	data = ((2 << ANA_SERDES_CDR_BW_SHIFT) & ANA_SERDES_CDR_BW_MASK) |
	    ANA_SERDES_EN_DEEM | ANA_SERDES_SEL_HSP | ANA_SERDES_EN_PLL |
	    ANA_SERDES_EN_LCKDT;
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_ADDR, MII_ANA_CFG5);
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_DATA, data);

	data = ((44 << ANA_LONG_CABLE_TH_100_SHIFT) &
	    ANA_LONG_CABLE_TH_100_MASK) |
	    ((33 << ANA_SHORT_CABLE_TH_100_SHIFT) &
	    ANA_SHORT_CABLE_TH_100_SHIFT) |
	    ANA_BP_BAD_LINK_ACCUM | ANA_BP_SMALL_BW;
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_ADDR, MII_ANA_CFG54);
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_DATA, data);

	data = ((11 << ANA_IECHO_ADJ_3_SHIFT) & ANA_IECHO_ADJ_3_MASK) |
	    ((11 << ANA_IECHO_ADJ_2_SHIFT) & ANA_IECHO_ADJ_2_MASK) |
	    ((8 << ANA_IECHO_ADJ_1_SHIFT) & ANA_IECHO_ADJ_1_MASK) |
	    ((8 << ANA_IECHO_ADJ_0_SHIFT) & ANA_IECHO_ADJ_0_MASK);
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_ADDR, MII_ANA_CFG4);
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_DATA, data);

	data = ((7 & ANA_MANUL_SWICH_ON_SHIFT) & ANA_MANUL_SWICH_ON_MASK) |
	    ANA_RESTART_CAL | ANA_MAN_ENABLE | ANA_SEL_HSP | ANA_EN_HB |
	    ANA_OEN_125M;
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_ADDR, MII_ANA_CFG0);
	alc_miibus_writereg(&sc->sc_dev, sc->alc_phyaddr,
	    ALC_MII_DBG_DATA, data);
	DELAY(1000);
}

void
alc_phy_down(struct alc_softc *sc)
{

	/* Force PHY down. */
	CSR_WRITE_2(sc, ALC_GPHY_CFG,
	    GPHY_CFG_EXT_RESET | GPHY_CFG_HIB_EN | GPHY_CFG_HIB_PULSE |
	    GPHY_CFG_SEL_ANA_RESET | GPHY_CFG_PHY_IDDQ | GPHY_CFG_PWDOWN_HW);
	DELAY(1000);
}

void
alc_aspm(struct alc_softc *sc)
{
	uint32_t pmcfg;

	pmcfg = CSR_READ_4(sc, ALC_PM_CFG);
	pmcfg &= ~PM_CFG_SERDES_PD_EX_L1;
	pmcfg |= PM_CFG_SERDES_BUDS_RX_L1_ENB;
	pmcfg |= PM_CFG_SERDES_L1_ENB;
	pmcfg &= ~PM_CFG_L1_ENTRY_TIMER_MASK;
	pmcfg |= PM_CFG_MAC_ASPM_CHK;
	if ((sc->alc_flags & ALC_FLAG_LINK) != 0) {
		pmcfg |= PM_CFG_SERDES_PLL_L1_ENB;
		pmcfg &= ~PM_CFG_CLK_SWH_L1;
		pmcfg &= ~PM_CFG_ASPM_L1_ENB;
		pmcfg &= ~PM_CFG_ASPM_L0S_ENB;
	} else {
		pmcfg &= ~PM_CFG_SERDES_PLL_L1_ENB;
		pmcfg |= PM_CFG_CLK_SWH_L1;
		pmcfg &= ~PM_CFG_ASPM_L1_ENB;
		pmcfg &= ~PM_CFG_ASPM_L0S_ENB;
	}
	CSR_WRITE_4(sc, ALC_PM_CFG, pmcfg);
}

void
alc_attach(struct device *parent, struct device *self, void *aux)
{

	struct alc_softc *sc = (struct alc_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr;
	struct ifnet *ifp;
	pcireg_t memtype;
	char *aspm_state[] = { "L0s/L1", "L0s", "L1", "L0s/l1" };
	uint16_t burst;
	int base, mii_flags, state, error = 0;
	uint32_t cap, ctl, val;

	/*
	 * Allocate IO memory
	 */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, ALC_PCIR_BAR);
	if (pci_mapreg_map(pa, ALC_PCIR_BAR, memtype, 0, &sc->sc_mem_bt,
	    &sc->sc_mem_bh, NULL, &sc->sc_mem_size, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": can't map interrupt\n");
		goto fail;
	}

	/*
	 * Allocate IRQ
	 */
	intrstr = pci_intr_string(pc, ih);
	sc->sc_irq_handle = pci_intr_establish(pc, ih, IPL_NET, alc_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_irq_handle == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}
	printf(": %s", intrstr);

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	
	/* Set PHY address. */
	sc->alc_phyaddr = ALC_PHY_ADDR;

	/* Initialize DMA parameters. */
	sc->alc_dma_rd_burst = 0;
	sc->alc_dma_wr_burst = 0;
	sc->alc_rcb = DMA_CFG_RCB_64;
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PCIEXPRESS,
	    &base, NULL)) {
		sc->alc_flags |= ALC_FLAG_PCIE;
		burst = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
		    base + PCI_PCIE_DCSR) >> 16;
		sc->alc_dma_rd_burst = (burst & 0x7000) >> 12;
		sc->alc_dma_wr_burst = (burst & 0x00e0) >> 5;
		if (alcdebug) {
			printf("%s: Read request size : %u bytes.\n",
			    sc->sc_dev.dv_xname, 
			    alc_dma_burst[sc->alc_dma_rd_burst]);
			printf("%s: TLP payload size : %u bytes.\n",
			    sc->sc_dev.dv_xname,
			    alc_dma_burst[sc->alc_dma_wr_burst]);
		}
		/* Clear data link and flow-control protocol error. */
		val = CSR_READ_4(sc, ALC_PEX_UNC_ERR_SEV);
		val &= ~(PEX_UNC_ERR_SEV_DLP | PEX_UNC_ERR_SEV_FCP);
		CSR_WRITE_4(sc, ALC_PEX_UNC_ERR_SEV, val);
		/* Disable ASPM L0S and L1. */
		cap = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
		    base + PCI_PCIE_LCAP) >> 16;
		if ((cap & 0x00000c00) != 0) {
			ctl = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
			    base + PCI_PCIE_LCSR) >> 16;
			if ((ctl & 0x08) != 0)
				sc->alc_rcb = DMA_CFG_RCB_128;
			if (alcdebug)
				printf("%s: RCB %u bytes\n",
				    sc->sc_dev.dv_xname,
				    sc->alc_rcb == DMA_CFG_RCB_64 ? 64 : 128);
			state = ctl & 0x03;
			if (alcdebug)
				printf("%s: ASPM %s %s\n",
				    sc->sc_dev.dv_xname,
				    aspm_state[state],
				    state == 0 ? "disabled" : "enabled");
			if (state != 0)
				alc_disable_l0s_l1(sc);
		}
	}

	/* Reset PHY. */
	alc_phy_reset(sc);

	/* Reset the ethernet controller. */
	alc_reset(sc);

	/*
	 * One odd thing is AR8132 uses the same PHY hardware(F1
	 * gigabit PHY) of AR8131. So atphy(4) of AR8132 reports
	 * the PHY supports 1000Mbps but that's not true. The PHY
	 * used in AR8132 can't establish gigabit link even if it
	 * shows the same PHY model/revision number of AR8131.
	 */
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATTANSIC_L2C)
		sc->alc_flags |= ALC_FLAG_FASTETHER | ALC_FLAG_JUMBO;
	else
		sc->alc_flags |= ALC_FLAG_JUMBO | ALC_FLAG_ASPM_MON;
	/*
	 * It seems that AR8131/AR8132 has silicon bug for SMB. In
	 * addition, Atheros said that enabling SMB wouldn't improve
	 * performance. However I think it's bad to access lots of
	 * registers to extract MAC statistics.
	 */
	sc->alc_flags |= ALC_FLAG_SMB_BUG;
	/*
	 * Don't use Tx CMB. It is known to have silicon bug.
	 */
	sc->alc_flags |= ALC_FLAG_CMB_BUG;
	sc->alc_rev = PCI_REVISION(pa->pa_class);
	sc->alc_chip_rev = CSR_READ_4(sc, ALC_MASTER_CFG) >>
	    MASTER_CHIP_REV_SHIFT;
	if (alcdebug) {
		printf("%s: PCI device revision : 0x%04x\n",
		    sc->sc_dev.dv_xname, sc->alc_rev);
		printf("%s: Chip id/revision : 0x%04x\n",
		    sc->sc_dev.dv_xname, sc->alc_chip_rev);
		printf("%s: %u Tx FIFO, %u Rx FIFO\n", sc->sc_dev.dv_xname,
		    CSR_READ_4(sc, ALC_SRAM_TX_FIFO_LEN) * 8,
		    CSR_READ_4(sc, ALC_SRAM_RX_FIFO_LEN) * 8);
	}

	error = alc_dma_alloc(sc);
	if (error)
		goto fail;

	/* Load station address. */
	alc_get_macaddr(sc);

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = alc_ioctl;
	ifp->if_start = alc_start;
	ifp->if_watchdog = alc_watchdog;
	ifp->if_baudrate = IF_Gbps(1);
	IFQ_SET_MAXLEN(&ifp->if_snd, ALC_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->alc_eaddr, sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#ifdef ALC_CHECKSUM
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
	    IFCAP_CSUM_UDPv4;
#endif

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	printf(", address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/* Set up MII bus. */
	sc->sc_miibus.mii_ifp = ifp;
	sc->sc_miibus.mii_readreg = alc_miibus_readreg;
	sc->sc_miibus.mii_writereg = alc_miibus_writereg;
	sc->sc_miibus.mii_statchg = alc_miibus_statchg;

	ifmedia_init(&sc->sc_miibus.mii_media, 0, alc_mediachange,
	    alc_mediastatus);
	mii_flags = 0;
	if ((sc->alc_flags & ALC_FLAG_JUMBO) != 0)
		mii_flags |= MIIF_DOPAUSE;
	mii_attach(self, &sc->sc_miibus, 0xffffffff, MII_PHY_ANY,
		MII_OFFSET_ANY, mii_flags);

	if (LIST_FIRST(&sc->sc_miibus.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc->sc_dev.dv_xname);
		ifmedia_add(&sc->sc_miibus.mii_media, IFM_ETHER | IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc->sc_miibus.mii_media, IFM_ETHER | IFM_MANUAL);
	} else 
		ifmedia_set(&sc->sc_miibus.mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->alc_tick_ch, alc_tick, sc);

	return;
fail:
	alc_dma_free(sc);
	if (sc->sc_irq_handle != NULL)
		pci_intr_disestablish(pc, sc->sc_irq_handle);
	if (sc->sc_mem_size)
		bus_space_unmap(sc->sc_mem_bt, sc->sc_mem_bh, sc->sc_mem_size);
}

int
alc_detach(struct device *self, int flags)
{
	struct alc_softc *sc = (struct alc_softc *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s;

	s = splnet();
	alc_stop(sc);
	splx(s);

	mii_detach(&sc->sc_miibus, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_miibus.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);
	alc_dma_free(sc);

	alc_phy_down(sc);
	if (sc->sc_irq_handle != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_irq_handle);
		sc->sc_irq_handle = NULL;
	}

	return (0);
}

int
alc_activate(struct device *self, int act)
{
	struct alc_softc *sc = (struct alc_softc *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int rv = 0;

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_activate_children(self, act);
		break;
	case DVACT_SUSPEND:
		if (ifp->if_flags & IFF_RUNNING)
			alc_stop(sc);
		rv = config_activate_children(self, act);
		break;
	case DVACT_RESUME:
		rv = config_activate_children(self, act);
		if (ifp->if_flags & IFF_UP)
			alc_init(ifp);
		break;
	}
	return (rv);
}

int
alc_dma_alloc(struct alc_softc *sc)
{
	struct alc_txdesc *txd;
	struct alc_rxdesc *rxd;
	int nsegs, error, i;

	/*
	 * Create DMA stuffs for TX ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, ALC_TX_RING_SZ, 1,
	    ALC_TX_RING_SZ, 0, BUS_DMA_NOWAIT, &sc->alc_cdata.alc_tx_ring_map);
	if (error)
		return (ENOBUFS);

	/* Allocate DMA'able memory for TX ring */
	error = bus_dmamem_alloc(sc->sc_dmat, ALC_TX_RING_SZ,
	    ETHER_ALIGN, 0, &sc->alc_rdata.alc_tx_ring_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error) {
		printf("%s: could not allocate DMA'able memory for Tx ring.\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->alc_rdata.alc_tx_ring_seg,
	    nsegs, ALC_TX_RING_SZ, (caddr_t *)&sc->alc_rdata.alc_tx_ring,
	    BUS_DMA_NOWAIT);
	if (error)
		return (ENOBUFS);

	/* Load the DMA map for Tx ring. */
	error = bus_dmamap_load(sc->sc_dmat, sc->alc_cdata.alc_tx_ring_map,
	    sc->alc_rdata.alc_tx_ring, ALC_TX_RING_SZ, NULL, BUS_DMA_WAITOK);
	if (error) {
		printf("%s: could not load DMA'able memory for Tx ring.\n",
		    sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat, 
		    (bus_dma_segment_t *)&sc->alc_rdata.alc_tx_ring, 1);
		return error;
	}

	sc->alc_rdata.alc_tx_ring_paddr = 
	    sc->alc_cdata.alc_tx_ring_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for RX ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, ALC_RX_RING_SZ, 1,
	    ALC_RX_RING_SZ, 0, BUS_DMA_NOWAIT, &sc->alc_cdata.alc_rx_ring_map);
	if (error)
		return (ENOBUFS);
	
	/* Allocate DMA'able memory for RX ring */
	error = bus_dmamem_alloc(sc->sc_dmat, ALC_RX_RING_SZ,
	    ETHER_ALIGN, 0, &sc->alc_rdata.alc_rx_ring_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error) {
		printf("%s: could not allocate DMA'able memory for Rx ring.\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->alc_rdata.alc_rx_ring_seg,
	    nsegs, ALC_RX_RING_SZ, (caddr_t *)&sc->alc_rdata.alc_rx_ring,
	    BUS_DMA_NOWAIT);
	if (error)
		return (ENOBUFS);

	/* Load the DMA map for Rx ring. */
	error = bus_dmamap_load(sc->sc_dmat, sc->alc_cdata.alc_rx_ring_map,
	    sc->alc_rdata.alc_rx_ring, ALC_RX_RING_SZ, NULL, BUS_DMA_WAITOK);
	if (error) {
		printf("%s: could not load DMA'able memory for Rx ring.\n",
		    sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat,
		    (bus_dma_segment_t *)sc->alc_rdata.alc_rx_ring, 1);
		return error;
	}

	sc->alc_rdata.alc_rx_ring_paddr =
	    sc->alc_cdata.alc_rx_ring_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for RX return ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, ALC_RR_RING_SZ, 1, 
	    ALC_RR_RING_SZ, 0, BUS_DMA_NOWAIT, &sc->alc_cdata.alc_rr_ring_map);
	if (error)
		return (ENOBUFS);

	/* Allocate DMA'able memory for RX return ring */
	error = bus_dmamem_alloc(sc->sc_dmat, ALC_RR_RING_SZ, 
	    ETHER_ALIGN, 0, &sc->alc_rdata.alc_rr_ring_seg, 1, 
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error) {
		printf("%s: could not allocate DMA'able memory for Rx "
		    "return ring.\n", sc->sc_dev.dv_xname);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->alc_rdata.alc_rr_ring_seg,
	    nsegs, ALC_RR_RING_SZ, (caddr_t *)&sc->alc_rdata.alc_rr_ring,
	    BUS_DMA_NOWAIT);
	if (error)
		return (ENOBUFS);

	/*  Load the DMA map for Rx return ring. */
	error = bus_dmamap_load(sc->sc_dmat, sc->alc_cdata.alc_rr_ring_map,
	    sc->alc_rdata.alc_rr_ring, ALC_RR_RING_SZ, NULL, BUS_DMA_WAITOK);
	if (error) {
		printf("%s: could not load DMA'able memory for Rx return ring."
		    "\n", sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat,
		    (bus_dma_segment_t *)&sc->alc_rdata.alc_rr_ring, 1);
		return error;
	}

	sc->alc_rdata.alc_rr_ring_paddr = 
	    sc->alc_cdata.alc_rr_ring_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for CMB block 
	 */
	error = bus_dmamap_create(sc->sc_dmat, ALC_CMB_SZ, 1, 
	    ALC_CMB_SZ, 0, BUS_DMA_NOWAIT, 
	    &sc->alc_cdata.alc_cmb_map);
	if (error) 
		return (ENOBUFS);

	/* Allocate DMA'able memory for CMB block */
	error = bus_dmamem_alloc(sc->sc_dmat, ALC_CMB_SZ, 
	    ETHER_ALIGN, 0, &sc->alc_rdata.alc_cmb_seg, 1, 
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error) {
		printf("%s: could not allocate DMA'able memory for "
		    "CMB block\n", sc->sc_dev.dv_xname);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->alc_rdata.alc_cmb_seg,
	    nsegs, ALC_CMB_SZ, (caddr_t *)&sc->alc_rdata.alc_cmb,
	    BUS_DMA_NOWAIT);
	if (error)
		return (ENOBUFS);

	/*  Load the DMA map for CMB block. */
	error = bus_dmamap_load(sc->sc_dmat, sc->alc_cdata.alc_cmb_map,
	    sc->alc_rdata.alc_cmb, ALC_CMB_SZ, NULL, 
	    BUS_DMA_WAITOK);
	if (error) {
		printf("%s: could not load DMA'able memory for CMB block\n",
		    sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat,
		    (bus_dma_segment_t *)&sc->alc_rdata.alc_cmb, 1);
		return error;
	}

	sc->alc_rdata.alc_cmb_paddr = 
	    sc->alc_cdata.alc_cmb_map->dm_segs[0].ds_addr;

	/*
	 * Create DMA stuffs for SMB block
	 */
	error = bus_dmamap_create(sc->sc_dmat, ALC_SMB_SZ, 1, 
	    ALC_SMB_SZ, 0, BUS_DMA_NOWAIT, 
	    &sc->alc_cdata.alc_smb_map);
	if (error)
		return (ENOBUFS);

	/* Allocate DMA'able memory for SMB block */
	error = bus_dmamem_alloc(sc->sc_dmat, ALC_SMB_SZ, 
	    ETHER_ALIGN, 0, &sc->alc_rdata.alc_smb_seg, 1, 
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error) {
		printf("%s: could not allocate DMA'able memory for "
		    "SMB block\n", sc->sc_dev.dv_xname);
		return error;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->alc_rdata.alc_smb_seg,
	    nsegs, ALC_SMB_SZ, (caddr_t *)&sc->alc_rdata.alc_smb,
	    BUS_DMA_NOWAIT);
	if (error)
		return (ENOBUFS);

	/*  Load the DMA map for SMB block */
	error = bus_dmamap_load(sc->sc_dmat, sc->alc_cdata.alc_smb_map,
	    sc->alc_rdata.alc_smb, ALC_SMB_SZ, NULL, 
	    BUS_DMA_WAITOK);
	if (error) {
		printf("%s: could not load DMA'able memory for SMB block\n",
		    sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat,
		    (bus_dma_segment_t *)&sc->alc_rdata.alc_smb, 1);
		return error;
	}

	sc->alc_rdata.alc_smb_paddr = 
	    sc->alc_cdata.alc_smb_map->dm_segs[0].ds_addr;


	/* Create DMA maps for Tx buffers. */
	for (i = 0; i < ALC_TX_RING_CNT; i++) {
		txd = &sc->alc_cdata.alc_txdesc[i];
		txd->tx_m = NULL;
		txd->tx_dmamap = NULL;
		error = bus_dmamap_create(sc->sc_dmat, ALC_TSO_MAXSIZE,
		    ALC_MAXTXSEGS, ALC_TSO_MAXSEGSIZE, 0, BUS_DMA_NOWAIT,
		    &txd->tx_dmamap);
		if (error) {
			printf("%s: could not create Tx dmamap.\n",
			    sc->sc_dev.dv_xname);
			return error;
		}
	}

	/* Create DMA maps for Rx buffers. */
	error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0,
	    BUS_DMA_NOWAIT, &sc->alc_cdata.alc_rx_sparemap);
	if (error) {
		printf("%s: could not create spare Rx dmamap.\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	for (i = 0; i < ALC_RX_RING_CNT; i++) {
		rxd = &sc->alc_cdata.alc_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_dmamap = NULL;
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &rxd->rx_dmamap);
		if (error) {
			printf("%s: could not create Rx dmamap.\n",
			    sc->sc_dev.dv_xname);
			return error;
		}
	}

	return (0);
}


void
alc_dma_free(struct alc_softc *sc)
{
	struct alc_txdesc *txd;
	struct alc_rxdesc *rxd;
	int i;

	/* Tx buffers */
	for (i = 0; i < ALC_TX_RING_CNT; i++) {
		txd = &sc->alc_cdata.alc_txdesc[i];
		if (txd->tx_dmamap != NULL) {
			bus_dmamap_destroy(sc->sc_dmat, txd->tx_dmamap);
			txd->tx_dmamap = NULL;
		}
	}
	/* Rx buffers */
	for (i = 0; i < ALC_RX_RING_CNT; i++) {
		rxd = &sc->alc_cdata.alc_rxdesc[i];
		if (rxd->rx_dmamap != NULL) {
			bus_dmamap_destroy(sc->sc_dmat, rxd->rx_dmamap);
			rxd->rx_dmamap = NULL;
		}
	}
	if (sc->alc_cdata.alc_rx_sparemap != NULL) {
		bus_dmamap_destroy(sc->sc_dmat, sc->alc_cdata.alc_rx_sparemap);
		sc->alc_cdata.alc_rx_sparemap = NULL;
	}

	/* Tx ring. */
	if (sc->alc_cdata.alc_tx_ring_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->alc_cdata.alc_tx_ring_map);
	if (sc->alc_cdata.alc_tx_ring_map != NULL &&
	    sc->alc_rdata.alc_tx_ring != NULL)
		bus_dmamem_free(sc->sc_dmat,
		    (bus_dma_segment_t *)sc->alc_rdata.alc_tx_ring, 1);
	sc->alc_rdata.alc_tx_ring = NULL;
	sc->alc_cdata.alc_tx_ring_map = NULL;

	/* Rx ring. */
	if (sc->alc_cdata.alc_rx_ring_map != NULL) 
		bus_dmamap_unload(sc->sc_dmat, sc->alc_cdata.alc_rx_ring_map);
	if (sc->alc_cdata.alc_rx_ring_map != NULL &&
	    sc->alc_rdata.alc_rx_ring != NULL)
		bus_dmamem_free(sc->sc_dmat, 
		    (bus_dma_segment_t *)sc->alc_rdata.alc_rx_ring, 1);
	sc->alc_rdata.alc_rx_ring = NULL;
	sc->alc_cdata.alc_rx_ring_map = NULL;

	/* Rx return ring. */
	if (sc->alc_cdata.alc_rr_ring_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->alc_cdata.alc_rr_ring_map);
	if (sc->alc_cdata.alc_rr_ring_map != NULL &&
	    sc->alc_rdata.alc_rr_ring != NULL)
		bus_dmamem_free(sc->sc_dmat, 
		    (bus_dma_segment_t *)sc->alc_rdata.alc_rr_ring, 1);
	sc->alc_rdata.alc_rr_ring = NULL;
	sc->alc_cdata.alc_rr_ring_map = NULL;

	/* CMB block */
	if (sc->alc_cdata.alc_cmb_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->alc_cdata.alc_cmb_map);
	if (sc->alc_cdata.alc_cmb_map != NULL &&
	    sc->alc_rdata.alc_cmb != NULL)
		bus_dmamem_free(sc->sc_dmat,
		    (bus_dma_segment_t *)sc->alc_rdata.alc_cmb, 1);
	sc->alc_rdata.alc_cmb = NULL;
	sc->alc_cdata.alc_cmb_map = NULL;

	/* SMB block */
	if (sc->alc_cdata.alc_smb_map != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->alc_cdata.alc_smb_map);
	if (sc->alc_cdata.alc_smb_map != NULL &&
	    sc->alc_rdata.alc_smb != NULL)
		bus_dmamem_free(sc->sc_dmat, 
		    (bus_dma_segment_t *)sc->alc_rdata.alc_smb, 1);
	sc->alc_rdata.alc_smb = NULL;
	sc->alc_cdata.alc_smb_map = NULL;
}

int
alc_encap(struct alc_softc *sc, struct mbuf **m_head)
{
	struct alc_txdesc *txd, *txd_last;
	struct tx_desc *desc;
	struct mbuf *m;
	bus_dmamap_t map;
	uint32_t cflags, poff, vtag;
	int error, idx, nsegs, prod;

	m = *m_head;
	cflags = vtag = 0;
	poff = 0;

	prod = sc->alc_cdata.alc_tx_prod;
	txd = &sc->alc_cdata.alc_txdesc[prod];
	txd_last = txd;
	map = txd->tx_dmamap;

	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, *m_head, BUS_DMA_NOWAIT);

	if (error != 0) {
		bus_dmamap_unload(sc->sc_dmat, map);
		error = EFBIG;
	}
	if (error == EFBIG) {
		if (m_defrag(*m_head, M_DONTWAIT)) {
			printf("%s: can't defrag TX mbuf\n",
			    sc->sc_dev.dv_xname);
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		error = bus_dmamap_load_mbuf(sc->sc_dmat, map, *m_head,
		    BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load defragged TX mbuf\n",
			    sc->sc_dev.dv_xname);
			m_freem(*m_head);
			*m_head = NULL;
			return (error);
		}
	} else if (error) {
		printf("%s: could not load TX mbuf\n", sc->sc_dev.dv_xname);
		return (error);
	}

	nsegs = map->dm_nsegs;

	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

	/* Check descriptor overrun. */
	if (sc->alc_cdata.alc_tx_cnt + nsegs >= ALC_TX_RING_CNT - 3) {
		bus_dmamap_unload(sc->sc_dmat, map);
		return (ENOBUFS);
	}
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	m = *m_head;
	desc = NULL;
	idx = 0;
#if NVLAN > 0
	/* Configure VLAN hardware tag insertion. */
	if (m->m_flags & M_VLANTAG) {
		vtag = htons(m->m_pkthdr.ether_vtag);
		vtag = (vtag << TD_VLAN_SHIFT) & TD_VLAN_MASK;
		cflags |= TD_INS_VLAN_TAG;
	}
#endif
	/* Configure Tx checksum offload. */
	if ((m->m_pkthdr.csum_flags & ALC_CSUM_FEATURES) != 0) {
		cflags |= TD_CUSTOM_CSUM;
		/* Set checksum start offset. */
		cflags |= ((poff >> 1) << TD_PLOAD_OFFSET_SHIFT) &
		    TD_PLOAD_OFFSET_MASK;
	} 
	for (; idx < nsegs; idx++) {
		desc = &sc->alc_rdata.alc_tx_ring[prod];
		desc->len =
		    htole32(TX_BYTES(map->dm_segs[idx].ds_len) | vtag);
		desc->flags = htole32(cflags);
		desc->addr = htole64(map->dm_segs[idx].ds_addr);
		sc->alc_cdata.alc_tx_cnt++;
		ALC_DESC_INC(prod, ALC_TX_RING_CNT);
	}
	/* Update producer index. */
	sc->alc_cdata.alc_tx_prod = prod;

	/* Finally set EOP on the last descriptor. */
	prod = (prod + ALC_TX_RING_CNT - 1) % ALC_TX_RING_CNT;
	desc = &sc->alc_rdata.alc_tx_ring[prod];
	desc->flags |= htole32(TD_EOP);

	/* Swap dmamap of the first and the last. */
	txd = &sc->alc_cdata.alc_txdesc[prod];
	map = txd_last->tx_dmamap;
	txd_last->tx_dmamap = txd->tx_dmamap;
	txd->tx_dmamap = map;
	txd->tx_m = m;

	return (0);
}

void
alc_start(struct ifnet *ifp)
{
	struct alc_softc *sc = ifp->if_softc;
	struct mbuf *m_head;
	int enq;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/* Reclaim transmitted frames. */
	if (sc->alc_cdata.alc_tx_cnt >= ALC_TX_DESC_HIWAT)
		alc_txeof(sc);

	enq = 0;
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (alc_encap(sc, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		enq = 1;
		
#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf != NULL)
			bpf_mtap_ether(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif
	}

	if (enq) {
		/* Sync descriptors. */
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_tx_ring_map, 0,
		    sc->alc_cdata.alc_tx_ring_map->dm_mapsize, 
		    BUS_DMASYNC_PREWRITE);
		/* Kick. Assume we're using normal Tx priority queue. */
		CSR_WRITE_4(sc, ALC_MBOX_TD_PROD_IDX,
		    (sc->alc_cdata.alc_tx_prod <<
		    MBOX_TD_PROD_LO_IDX_SHIFT) &
		    MBOX_TD_PROD_LO_IDX_MASK);
		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = ALC_TX_TIMEOUT;
	}
}

void
alc_watchdog(struct ifnet *ifp)
{
	struct alc_softc *sc = ifp->if_softc;

	if ((sc->alc_flags & ALC_FLAG_LINK) == 0) {
		printf("%s: watchdog timeout (missed link)\n",
		    sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		alc_init(ifp);
		return;
	}

	printf("%s: watchdog timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;
	alc_init(ifp);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		 alc_start(ifp);
}

int
alc_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct alc_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_miibus;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			alc_init(ifp);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_arpcom, ifa);
#endif
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				alc_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				alc_stop(sc);
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			alc_iff(sc);
		error = 0;
	}

	splx(s);
	return (error);
}

void
alc_mac_config(struct alc_softc *sc)
{
	struct mii_data *mii;
	uint32_t reg;

	mii = &sc->sc_miibus;
	reg = CSR_READ_4(sc, ALC_MAC_CFG);
	reg &= ~(MAC_CFG_FULL_DUPLEX | MAC_CFG_TX_FC | MAC_CFG_RX_FC |
	    MAC_CFG_SPEED_MASK);
	/* Reprogram MAC with resolved speed/duplex. */
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
	case IFM_100_TX:
		reg |= MAC_CFG_SPEED_10_100;
		break;
	case IFM_1000_T:
		reg |= MAC_CFG_SPEED_1000;
		break;
	}
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0) {
		reg |= MAC_CFG_FULL_DUPLEX;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
			reg |= MAC_CFG_TX_FC;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
			reg |= MAC_CFG_RX_FC;
	}
	CSR_WRITE_4(sc, ALC_MAC_CFG, reg);
}

void
alc_stats_clear(struct alc_softc *sc)
{
	struct smb sb, *smb;
	uint32_t *reg;
	int i;

	if ((sc->alc_flags & ALC_FLAG_SMB_BUG) == 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_smb_map, 0,
		    sc->alc_cdata.alc_smb_map->dm_mapsize, 
		    BUS_DMASYNC_POSTREAD);
		smb = sc->alc_rdata.alc_smb;
		/* Update done, clear. */
		smb->updated = 0;
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_smb_map, 0,
		    sc->alc_cdata.alc_smb_map->dm_mapsize, 
		    BUS_DMASYNC_PREWRITE);
	} else {
		for (reg = &sb.rx_frames, i = 0; reg <= &sb.rx_pkts_filtered;
		    reg++) {
			CSR_READ_4(sc, ALC_RX_MIB_BASE + i);
			i += sizeof(uint32_t);
		}
		/* Read Tx statistics. */
		for (reg = &sb.tx_frames, i = 0; reg <= &sb.tx_mcast_bytes;
		    reg++) {
			CSR_READ_4(sc, ALC_TX_MIB_BASE + i);
			i += sizeof(uint32_t);
		}
	}
}

void
alc_stats_update(struct alc_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct alc_hw_stats *stat;
	struct smb sb, *smb;
	uint32_t *reg;
	int i;

	stat = &sc->alc_stats;
	if ((sc->alc_flags & ALC_FLAG_SMB_BUG) == 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_smb_map, 0,
		    sc->alc_cdata.alc_smb_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		smb = sc->alc_rdata.alc_smb;
		if (smb->updated == 0)
			return;
	} else {
		smb = &sb;
		/* Read Rx statistics. */
		for (reg = &sb.rx_frames, i = 0; reg <= &sb.rx_pkts_filtered;
		    reg++) {
			*reg = CSR_READ_4(sc, ALC_RX_MIB_BASE + i);
			i += sizeof(uint32_t);
		}
		/* Read Tx statistics. */
		for (reg = &sb.tx_frames, i = 0; reg <= &sb.tx_mcast_bytes;
		    reg++) {
			*reg = CSR_READ_4(sc, ALC_TX_MIB_BASE + i);
			i += sizeof(uint32_t);
		}
	}

	/* Rx stats. */
	stat->rx_frames += smb->rx_frames;
	stat->rx_bcast_frames += smb->rx_bcast_frames;
	stat->rx_mcast_frames += smb->rx_mcast_frames;
	stat->rx_pause_frames += smb->rx_pause_frames;
	stat->rx_control_frames += smb->rx_control_frames;
	stat->rx_crcerrs += smb->rx_crcerrs;
	stat->rx_lenerrs += smb->rx_lenerrs;
	stat->rx_bytes += smb->rx_bytes;
	stat->rx_runts += smb->rx_runts;
	stat->rx_fragments += smb->rx_fragments;
	stat->rx_pkts_64 += smb->rx_pkts_64;
	stat->rx_pkts_65_127 += smb->rx_pkts_65_127;
	stat->rx_pkts_128_255 += smb->rx_pkts_128_255;
	stat->rx_pkts_256_511 += smb->rx_pkts_256_511;
	stat->rx_pkts_512_1023 += smb->rx_pkts_512_1023;
	stat->rx_pkts_1024_1518 += smb->rx_pkts_1024_1518;
	stat->rx_pkts_1519_max += smb->rx_pkts_1519_max;
	stat->rx_pkts_truncated += smb->rx_pkts_truncated;
	stat->rx_fifo_oflows += smb->rx_fifo_oflows;
	stat->rx_rrs_errs += smb->rx_rrs_errs;
	stat->rx_alignerrs += smb->rx_alignerrs;
	stat->rx_bcast_bytes += smb->rx_bcast_bytes;
	stat->rx_mcast_bytes += smb->rx_mcast_bytes;
	stat->rx_pkts_filtered += smb->rx_pkts_filtered;

	/* Tx stats. */
	stat->tx_frames += smb->tx_frames;
	stat->tx_bcast_frames += smb->tx_bcast_frames;
	stat->tx_mcast_frames += smb->tx_mcast_frames;
	stat->tx_pause_frames += smb->tx_pause_frames;
	stat->tx_excess_defer += smb->tx_excess_defer;
	stat->tx_control_frames += smb->tx_control_frames;
	stat->tx_deferred += smb->tx_deferred;
	stat->tx_bytes += smb->tx_bytes;
	stat->tx_pkts_64 += smb->tx_pkts_64;
	stat->tx_pkts_65_127 += smb->tx_pkts_65_127;
	stat->tx_pkts_128_255 += smb->tx_pkts_128_255;
	stat->tx_pkts_256_511 += smb->tx_pkts_256_511;
	stat->tx_pkts_512_1023 += smb->tx_pkts_512_1023;
	stat->tx_pkts_1024_1518 += smb->tx_pkts_1024_1518;
	stat->tx_pkts_1519_max += smb->tx_pkts_1519_max;
	stat->tx_single_colls += smb->tx_single_colls;
	stat->tx_multi_colls += smb->tx_multi_colls;
	stat->tx_late_colls += smb->tx_late_colls;
	stat->tx_excess_colls += smb->tx_excess_colls;
	stat->tx_abort += smb->tx_abort;
	stat->tx_underrun += smb->tx_underrun;
	stat->tx_desc_underrun += smb->tx_desc_underrun;
	stat->tx_lenerrs += smb->tx_lenerrs;
	stat->tx_pkts_truncated += smb->tx_pkts_truncated;
	stat->tx_bcast_bytes += smb->tx_bcast_bytes;
	stat->tx_mcast_bytes += smb->tx_mcast_bytes;

	/* Update counters in ifnet. */
	ifp->if_opackets += smb->tx_frames;

	ifp->if_collisions += smb->tx_single_colls +
	    smb->tx_multi_colls * 2 + smb->tx_late_colls +
	    smb->tx_abort * HDPX_CFG_RETRY_DEFAULT;

	/*
	 * XXX
	 * tx_pkts_truncated counter looks suspicious. It constantly
	 * increments with no sign of Tx errors. This may indicate
	 * the counter name is not correct one so I've removed the
	 * counter in output errors.
	 */
	ifp->if_oerrors += smb->tx_abort + smb->tx_late_colls +
	    smb->tx_underrun;

	ifp->if_ipackets += smb->rx_frames;

	ifp->if_ierrors += smb->rx_crcerrs + smb->rx_lenerrs +
	    smb->rx_runts + smb->rx_pkts_truncated +
	    smb->rx_fifo_oflows + smb->rx_rrs_errs +
	    smb->rx_alignerrs;

	if ((sc->alc_flags & ALC_FLAG_SMB_BUG) == 0) {
		/* Update done, clear. */
		smb->updated = 0;
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_smb_map, 0,
		sc->alc_cdata.alc_smb_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	}
}

int
alc_intr(void *arg)
{
	struct alc_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	uint32_t status;

	status = CSR_READ_4(sc, ALC_INTR_STATUS);
	if ((status & ALC_INTRS) == 0)
		return (0);

	/* Acknowledge and disable interrupts. */
	CSR_WRITE_4(sc, ALC_INTR_STATUS, status | INTR_DIS_INT);

	if (ifp->if_flags & IFF_RUNNING) {
		if (status & INTR_RX_PKT) {
			int error;

			error = alc_rxintr(sc);
			if (error) {
				alc_init(ifp);
				return (0);
			}
		}

		if (status & (INTR_DMA_RD_TO_RST | INTR_DMA_WR_TO_RST |
		    INTR_TXQ_TO_RST)) {
			if (status & INTR_DMA_RD_TO_RST)
				printf("%s: DMA read error! -- resetting\n",
				    sc->sc_dev.dv_xname);
			if (status & INTR_DMA_WR_TO_RST)
				printf("%s: DMA write error! -- resetting\n",
				    sc->sc_dev.dv_xname);
			if (status & INTR_TXQ_TO_RST)
				printf("%s: TxQ reset! -- resetting\n",
				    sc->sc_dev.dv_xname);
			alc_init(ifp);
			return (0);
		}

		alc_txeof(sc);
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			alc_start(ifp);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, ALC_INTR_STATUS, 0x7FFFFFFF);
	return (1);
}

void
alc_txeof(struct alc_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct alc_txdesc *txd;
	uint32_t cons, prod;
	int prog;

	if (sc->alc_cdata.alc_tx_cnt == 0)
		return;
	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_tx_ring_map, 0,
	    sc->alc_cdata.alc_tx_ring_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	if ((sc->alc_flags & ALC_FLAG_CMB_BUG) == 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_cmb_map, 0,
		    sc->alc_cdata.alc_cmb_map->dm_mapsize, 
		    BUS_DMASYNC_POSTREAD);
		prod = sc->alc_rdata.alc_cmb->cons;
	} else
		prod = CSR_READ_4(sc, ALC_MBOX_TD_CONS_IDX);
	/* Assume we're using normal Tx priority queue. */
	prod = (prod & MBOX_TD_CONS_LO_IDX_MASK) >>
	    MBOX_TD_CONS_LO_IDX_SHIFT;
	cons = sc->alc_cdata.alc_tx_cons;
	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (prog = 0; cons != prod; prog++,
	    ALC_DESC_INC(cons, ALC_TX_RING_CNT)) {
		if (sc->alc_cdata.alc_tx_cnt <= 0)
			break;
		prog++;
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->alc_cdata.alc_tx_cnt--;
		txd = &sc->alc_cdata.alc_txdesc[cons];
		if (txd->tx_m != NULL) {
			/* Reclaim transmitted mbufs. */
			bus_dmamap_unload(sc->sc_dmat, txd->tx_dmamap);
			m_freem(txd->tx_m);
			txd->tx_m = NULL;
		}
	}

	if ((sc->alc_flags & ALC_FLAG_CMB_BUG) == 0)
	    bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_cmb_map, 0,
	        sc->alc_cdata.alc_cmb_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	sc->alc_cdata.alc_tx_cons = cons;
	/*
	 * Unarm watchdog timer only when there is no pending
	 * frames in Tx queue.
	 */
	if (sc->alc_cdata.alc_tx_cnt == 0)
		ifp->if_timer = 0;
}

int
alc_newbuf(struct alc_softc *sc, struct alc_rxdesc *rxd)
{
	struct mbuf *m;
	bus_dmamap_t map;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		return (ENOBUFS);
	}

	m->m_len = m->m_pkthdr.len = RX_BUF_SIZE_MAX;

	error = bus_dmamap_load_mbuf(sc->sc_dmat,
	    sc->alc_cdata.alc_rx_sparemap, m, BUS_DMA_NOWAIT);

	if (error != 0) {
		m_freem(m);
		printf("%s: can't load RX mbuf\n", sc->sc_dev.dv_xname);
		return (error);
	}

	if (rxd->rx_m != NULL) {
		bus_dmamap_sync(sc->sc_dmat, rxd->rx_dmamap, 0,
		    rxd->rx_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, rxd->rx_dmamap);
	}
	map = rxd->rx_dmamap;
	rxd->rx_dmamap = sc->alc_cdata.alc_rx_sparemap;
	sc->alc_cdata.alc_rx_sparemap = map;
	rxd->rx_m = m;
	rxd->rx_desc->addr = htole64(rxd->rx_dmamap->dm_segs[0].ds_addr);
	return (0);
}

int
alc_rxintr(struct alc_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct rx_rdesc *rrd;
	uint32_t nsegs, status;
	int rr_cons, prog;

	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_rr_ring_map, 0,
	    sc->alc_cdata.alc_rr_ring_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_rx_ring_map, 0,
	    sc->alc_cdata.alc_rx_ring_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	rr_cons = sc->alc_cdata.alc_rr_cons;
	for (prog = 0; (ifp->if_flags & IFF_RUNNING) != 0;) {
		rrd = &sc->alc_rdata.alc_rr_ring[rr_cons];
		status = letoh32(rrd->status);
		if ((status & RRD_VALID) == 0)
			break;
		nsegs = RRD_RD_CNT(letoh32(rrd->rdinfo));
		if (nsegs == 0) {
			/* This should not happen! */
			if (alcdebug)
				printf("%s: unexpected segment count -- "
				    "resetting\n", sc->sc_dev.dv_xname);
			return (EIO);
		}
		alc_rxeof(sc, rrd);
		/* Clear Rx return status. */
		rrd->status = 0;
		ALC_DESC_INC(rr_cons, ALC_RR_RING_CNT);
		sc->alc_cdata.alc_rx_cons += nsegs;
		sc->alc_cdata.alc_rx_cons %= ALC_RR_RING_CNT;
		prog += nsegs;
	}

	if (prog > 0) {
		/* Update the consumer index. */
		sc->alc_cdata.alc_rr_cons = rr_cons;
		/* Sync Rx return descriptors. */
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_rr_ring_map, 0,
		    sc->alc_cdata.alc_rr_ring_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);
		/*
		 * Sync updated Rx descriptors such that controller see
		 * modified buffer addresses.
		 */
		bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_rx_ring_map, 0,
		    sc->alc_cdata.alc_rx_ring_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);
		/*
		 * Let controller know availability of new Rx buffers.
		 * Since alc(4) use RXQ_CFG_RD_BURST_DEFAULT descriptors
		 * it may be possible to update ALC_MBOX_RD0_PROD_IDX
		 * only when Rx buffer pre-fetching is required. In
		 * addition we already set ALC_RX_RD_FREE_THRESH to
		 * RX_RD_FREE_THRESH_LO_DEFAULT descriptors. However
		 * it still seems that pre-fetching needs more
		 * experimentation.
		 */
		CSR_WRITE_4(sc, ALC_MBOX_RD0_PROD_IDX,
		    sc->alc_cdata.alc_rx_cons);
	}

	return (0);
}

/* Receive a frame. */
void
alc_rxeof(struct alc_softc *sc, struct rx_rdesc *rrd)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct alc_rxdesc *rxd;
	struct mbuf *mp, *m;
	uint32_t rdinfo, status;
	int count, nsegs, rx_cons;

	status = letoh32(rrd->status);
	rdinfo = letoh32(rrd->rdinfo);
	rx_cons = RRD_RD_IDX(rdinfo);
	nsegs = RRD_RD_CNT(rdinfo);

	sc->alc_cdata.alc_rxlen = RRD_BYTES(status);
	if (status & (RRD_ERR_SUM | RRD_ERR_LENGTH)) {
		/*
		 * We want to pass the following frames to upper
		 * layer regardless of error status of Rx return
		 * ring.
		 *
		 *  o IP/TCP/UDP checksum is bad.
		 *  o frame length and protocol specific length
		 *     does not match.
		 *
		 *  Force network stack compute checksum for
		 *  errored frames.
		 */
		status |= RRD_TCP_UDPCSUM_NOK | RRD_IPCSUM_NOK;
		if ((RRD_ERR_CRC | RRD_ERR_ALIGN | RRD_ERR_TRUNC |
		    RRD_ERR_RUNT) != 0)
			return;
	}

	for (count = 0; count < nsegs; count++,
	    ALC_DESC_INC(rx_cons, ALC_RX_RING_CNT)) {
		rxd = &sc->alc_cdata.alc_rxdesc[rx_cons];
		mp = rxd->rx_m;
		/* Add a new receive buffer to the ring. */
		if (alc_newbuf(sc, rxd) != 0) {
			ifp->if_iqdrops++;
			/* Reuse Rx buffers. */
			if (sc->alc_cdata.alc_rxhead != NULL)
				m_freem(sc->alc_cdata.alc_rxhead);
			break;
		}

		/*
		 * Assume we've received a full sized frame.
		 * Actual size is fixed when we encounter the end of
		 * multi-segmented frame.
		 */
		mp->m_len = sc->alc_buf_size;

		/* Chain received mbufs. */
		if (sc->alc_cdata.alc_rxhead == NULL) {
			sc->alc_cdata.alc_rxhead = mp;
			sc->alc_cdata.alc_rxtail = mp;
		} else {
			mp->m_flags &= ~M_PKTHDR;
			sc->alc_cdata.alc_rxprev_tail =
			    sc->alc_cdata.alc_rxtail;
			sc->alc_cdata.alc_rxtail->m_next = mp;
			sc->alc_cdata.alc_rxtail = mp;
		}

		if (count == nsegs - 1) {
			/* Last desc. for this frame. */
			m = sc->alc_cdata.alc_rxhead;
			m->m_flags |= M_PKTHDR;
			/*
			 * It seems that L1C/L2C controller has no way
			 * to tell hardware to strip CRC bytes.
			 */
			m->m_pkthdr.len =
			    sc->alc_cdata.alc_rxlen - ETHER_CRC_LEN;
			if (nsegs > 1) {
				/* Set last mbuf size. */
				mp->m_len = sc->alc_cdata.alc_rxlen -
				    (nsegs - 1) * sc->alc_buf_size;
				/* Remove the CRC bytes in chained mbufs. */
				if (mp->m_len <= ETHER_CRC_LEN) {
					sc->alc_cdata.alc_rxtail =
					    sc->alc_cdata.alc_rxprev_tail;
					sc->alc_cdata.alc_rxtail->m_len -=
					    (ETHER_CRC_LEN - mp->m_len);
					sc->alc_cdata.alc_rxtail->m_next = NULL;
					m_freem(mp);
				} else {
					mp->m_len -= ETHER_CRC_LEN;
				}
			} else
				m->m_len = m->m_pkthdr.len;
			m->m_pkthdr.rcvif = ifp;
#if NVLAN > 0
			/*
			 * Due to hardware bugs, Rx checksum offloading
			 * was intentionally disabled.
			 */
			if (status & RRD_VLAN_TAG) {
				u_int32_t vtag = RRD_VLAN(letoh32(rrd->vtag));
				m->m_pkthdr.ether_vtag = ntohs(vtag);
				m->m_flags |= M_VLANTAG;
			}
#endif

#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap_ether(ifp->if_bpf, m,
				    BPF_DIRECTION_IN);
#endif

			{
			/* Pass it on. */
			ether_input_mbuf(ifp, m);
			}
		}
	}
	/* Reset mbuf chains. */
	ALC_RXCHAIN_RESET(sc);
}

void
alc_tick(void *xsc)
{
	struct alc_softc *sc = xsc;
	struct mii_data *mii = &sc->sc_miibus;
	int s;

	s = splnet();
	mii_tick(mii);
	alc_stats_update(sc);

	timeout_add_sec(&sc->alc_tick_ch, 1);
	splx(s);
}

void
alc_reset(struct alc_softc *sc)
{
	uint32_t reg;
	int i;

	CSR_WRITE_4(sc, ALC_MASTER_CFG, MASTER_RESET);
	for (i = ALC_RESET_TIMEOUT; i > 0; i--) {
		DELAY(10);
		if ((CSR_READ_4(sc, ALC_MASTER_CFG) & MASTER_RESET) == 0)
			break;
	}
	if (i == 0)
		printf("%s: master reset timeout!\n", sc->sc_dev.dv_xname);

	for (i = ALC_RESET_TIMEOUT; i > 0; i--) {
		if ((reg = CSR_READ_4(sc, ALC_IDLE_STATUS)) == 0)
			break;
		DELAY(10);
	}

	if (i == 0)
		printf("%s: reset timeout(0x%08x)!\n", sc->sc_dev.dv_xname, 
		    reg);
}

int
alc_init(struct ifnet *ifp)
{
	struct alc_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	uint8_t eaddr[ETHER_ADDR_LEN];
	bus_addr_t paddr;
	uint32_t reg, rxf_hi, rxf_lo;
	int error;

	/*
	 * Cancel any pending I/O.
	 */
	alc_stop(sc);
	/*
	 * Reset the chip to a known state.
	 */
	alc_reset(sc);

	/* Initialize Rx descriptors. */
	error = alc_init_rx_ring(sc);
	if (error != 0) {
		printf("%s: no memory for Rx buffers.\n", sc->sc_dev.dv_xname);
		alc_stop(sc);
		return (error);
	}
	alc_init_rr_ring(sc);
	alc_init_tx_ring(sc);
	alc_init_cmb(sc);
	alc_init_smb(sc);

	/* Reprogram the station address. */
	bcopy(LLADDR(ifp->if_sadl), eaddr, ETHER_ADDR_LEN);
	CSR_WRITE_4(sc, ALC_PAR0,
	    eaddr[2] << 24 | eaddr[3] << 16 | eaddr[4] << 8 | eaddr[5]);
	CSR_WRITE_4(sc, ALC_PAR1, eaddr[0] << 8 | eaddr[1]);
	/*
	 * Clear WOL status and disable all WOL feature as WOL
	 * would interfere Rx operation under normal environments.
	 */
	CSR_READ_4(sc, ALC_WOL_CFG);
	CSR_WRITE_4(sc, ALC_WOL_CFG, 0);
	/* Set Tx descriptor base addresses. */
	paddr = sc->alc_rdata.alc_tx_ring_paddr;
	CSR_WRITE_4(sc, ALC_TX_BASE_ADDR_HI, ALC_ADDR_HI(paddr));
	CSR_WRITE_4(sc, ALC_TDL_HEAD_ADDR_LO, ALC_ADDR_LO(paddr));
	/* We don't use high priority ring. */
	CSR_WRITE_4(sc, ALC_TDH_HEAD_ADDR_LO, 0);
	/* Set Tx descriptor counter. */
	CSR_WRITE_4(sc, ALC_TD_RING_CNT,
	    (ALC_TX_RING_CNT << TD_RING_CNT_SHIFT) & TD_RING_CNT_MASK);
	/* Set Rx descriptor base addresses. */
	paddr = sc->alc_rdata.alc_rx_ring_paddr;
	CSR_WRITE_4(sc, ALC_RX_BASE_ADDR_HI, ALC_ADDR_HI(paddr));
	CSR_WRITE_4(sc, ALC_RD0_HEAD_ADDR_LO, ALC_ADDR_LO(paddr));
	/* We use one Rx ring. */
	CSR_WRITE_4(sc, ALC_RD1_HEAD_ADDR_LO, 0);
	CSR_WRITE_4(sc, ALC_RD2_HEAD_ADDR_LO, 0);
	CSR_WRITE_4(sc, ALC_RD3_HEAD_ADDR_LO, 0);
	/* Set Rx descriptor counter. */
	CSR_WRITE_4(sc, ALC_RD_RING_CNT,
	    (ALC_RX_RING_CNT << RD_RING_CNT_SHIFT) & RD_RING_CNT_MASK);

	/*
	 * Let hardware split jumbo frames into alc_max_buf_sized chunks.
	 * if it do not fit the buffer size. Rx return descriptor holds
	 * a counter that indicates how many fragments were made by the
	 * hardware. The buffer size should be multiple of 8 bytes.
	 * Since hardware has limit on the size of buffer size, always
	 * use the maximum value.
	 * For strict-alignment architectures make sure to reduce buffer
	 * size by 8 bytes to make room for alignment fixup.
	 */
	sc->alc_buf_size = RX_BUF_SIZE_MAX;
	CSR_WRITE_4(sc, ALC_RX_BUF_SIZE, sc->alc_buf_size);

	paddr = sc->alc_rdata.alc_rr_ring_paddr;
	/* Set Rx return descriptor base addresses. */
	CSR_WRITE_4(sc, ALC_RRD0_HEAD_ADDR_LO, ALC_ADDR_LO(paddr));
	/* We use one Rx return ring. */
	CSR_WRITE_4(sc, ALC_RRD1_HEAD_ADDR_LO, 0);
	CSR_WRITE_4(sc, ALC_RRD2_HEAD_ADDR_LO, 0);
	CSR_WRITE_4(sc, ALC_RRD3_HEAD_ADDR_LO, 0);
	/* Set Rx return descriptor counter. */
	CSR_WRITE_4(sc, ALC_RRD_RING_CNT,
	    (ALC_RR_RING_CNT << RRD_RING_CNT_SHIFT) & RRD_RING_CNT_MASK);
	paddr = sc->alc_rdata.alc_cmb_paddr;
	CSR_WRITE_4(sc, ALC_CMB_BASE_ADDR_LO, ALC_ADDR_LO(paddr));
	paddr = sc->alc_rdata.alc_smb_paddr;
	CSR_WRITE_4(sc, ALC_SMB_BASE_ADDR_HI, ALC_ADDR_HI(paddr));
	CSR_WRITE_4(sc, ALC_SMB_BASE_ADDR_LO, ALC_ADDR_LO(paddr));

	/* Tell hardware that we're ready to load DMA blocks. */
	CSR_WRITE_4(sc, ALC_DMA_BLOCK, DMA_BLOCK_LOAD);

	/* Configure interrupt moderation timer. */
	sc->alc_int_rx_mod = ALC_IM_RX_TIMER_DEFAULT;
	sc->alc_int_tx_mod = ALC_IM_TX_TIMER_DEFAULT;
	reg = ALC_USECS(sc->alc_int_rx_mod) << IM_TIMER_RX_SHIFT;
	reg |= ALC_USECS(sc->alc_int_tx_mod) << IM_TIMER_TX_SHIFT;
	CSR_WRITE_4(sc, ALC_IM_TIMER, reg);
	reg = CSR_READ_4(sc, ALC_MASTER_CFG);
	reg &= ~(MASTER_CHIP_REV_MASK | MASTER_CHIP_ID_MASK);
	/*
	 * We don't want to automatic interrupt clear as task queue
	 * for the interrupt should know interrupt status.
	 */
	reg &= ~MASTER_INTR_RD_CLR;
	reg &= ~(MASTER_IM_RX_TIMER_ENB | MASTER_IM_TX_TIMER_ENB);
	if (ALC_USECS(sc->alc_int_rx_mod) != 0)
		reg |= MASTER_IM_RX_TIMER_ENB;
	if (ALC_USECS(sc->alc_int_tx_mod) != 0)
		reg |= MASTER_IM_TX_TIMER_ENB;
	CSR_WRITE_4(sc, ALC_MASTER_CFG, reg);
	/*
	 * Disable interrupt re-trigger timer. We don't want automatic
	 * re-triggering of un-ACKed interrupts.
	 */
	CSR_WRITE_4(sc, ALC_INTR_RETRIG_TIMER, ALC_USECS(0));
	/* Configure CMB. */
	CSR_WRITE_4(sc, ALC_CMB_TD_THRESH, 4);
	if ((sc->alc_flags & ALC_FLAG_CMB_BUG) == 0)
		CSR_WRITE_4(sc, ALC_CMB_TX_TIMER, ALC_USECS(5000));
	else
		CSR_WRITE_4(sc, ALC_CMB_TX_TIMER, ALC_USECS(0));
	/*
	 * Hardware can be configured to issue SMB interrupt based
	 * on programmed interval. Since there is a callout that is
	 * invoked for every hz in driver we use that instead of
	 * relying on periodic SMB interrupt.
	 */
	CSR_WRITE_4(sc, ALC_SMB_STAT_TIMER, ALC_USECS(0));
	/* Clear MAC statistics. */
	alc_stats_clear(sc);

	/*
	 * Always use maximum frame size that controller can support.
	 * Otherwise received frames that has larger frame length
	 * than alc(4) MTU would be silently dropped in hardware. This
	 * would make path-MTU discovery hard as sender wouldn't get
	 * any responses from receiver. alc(4) supports
	 * multi-fragmented frames on Rx path so it has no issue on
	 * assembling fragmented frames. Using maximum frame size also
	 * removes the need to reinitialize hardware when interface
	 * MTU configuration was changed.
	 *
	 * Be conservative in what you do, be liberal in what you
	 * accept from others - RFC 793.
	 */
	CSR_WRITE_4(sc, ALC_FRAME_SIZE, ALC_JUMBO_FRAMELEN);

	/* Disable header split(?) */
	CSR_WRITE_4(sc, ALC_HDS_CFG, 0);

	/* Configure IPG/IFG parameters. */
	CSR_WRITE_4(sc, ALC_IPG_IFG_CFG,
	    ((IPG_IFG_IPGT_DEFAULT << IPG_IFG_IPGT_SHIFT) & IPG_IFG_IPGT_MASK) |
	    ((IPG_IFG_MIFG_DEFAULT << IPG_IFG_MIFG_SHIFT) & IPG_IFG_MIFG_MASK) |
	    ((IPG_IFG_IPG1_DEFAULT << IPG_IFG_IPG1_SHIFT) & IPG_IFG_IPG1_MASK) |
	    ((IPG_IFG_IPG2_DEFAULT << IPG_IFG_IPG2_SHIFT) & IPG_IFG_IPG2_MASK));
	/* Set parameters for half-duplex media. */
	CSR_WRITE_4(sc, ALC_HDPX_CFG,
	    ((HDPX_CFG_LCOL_DEFAULT << HDPX_CFG_LCOL_SHIFT) &
	    HDPX_CFG_LCOL_MASK) |
	    ((HDPX_CFG_RETRY_DEFAULT << HDPX_CFG_RETRY_SHIFT) &
	    HDPX_CFG_RETRY_MASK) | HDPX_CFG_EXC_DEF_EN |
	    ((HDPX_CFG_ABEBT_DEFAULT << HDPX_CFG_ABEBT_SHIFT) &
	    HDPX_CFG_ABEBT_MASK) |
	    ((HDPX_CFG_JAMIPG_DEFAULT << HDPX_CFG_JAMIPG_SHIFT) &
	    HDPX_CFG_JAMIPG_MASK));
	/*
	 * Set TSO/checksum offload threshold. For frames that is
	 * larger than this threshold, hardware wouldn't do
	 * TSO/checksum offloading.
	 */
	CSR_WRITE_4(sc, ALC_TSO_OFFLOAD_THRESH,
	    (ALC_JUMBO_FRAMELEN >> TSO_OFFLOAD_THRESH_UNIT_SHIFT) &
	    TSO_OFFLOAD_THRESH_MASK);
	/* Configure TxQ. */
	reg = (alc_dma_burst[sc->alc_dma_rd_burst] <<
	    TXQ_CFG_TX_FIFO_BURST_SHIFT) & TXQ_CFG_TX_FIFO_BURST_MASK;
	reg |= (TXQ_CFG_TD_BURST_DEFAULT << TXQ_CFG_TD_BURST_SHIFT) &
	    TXQ_CFG_TD_BURST_MASK;
	CSR_WRITE_4(sc, ALC_TXQ_CFG, reg | TXQ_CFG_ENHANCED_MODE);

	/* Configure Rx free descriptor pre-fetching. */
	CSR_WRITE_4(sc, ALC_RX_RD_FREE_THRESH,
	    ((RX_RD_FREE_THRESH_HI_DEFAULT << RX_RD_FREE_THRESH_HI_SHIFT) &
	    RX_RD_FREE_THRESH_HI_MASK) |
	    ((RX_RD_FREE_THRESH_LO_DEFAULT << RX_RD_FREE_THRESH_LO_SHIFT) &
	    RX_RD_FREE_THRESH_LO_MASK));

	/*
	 * Configure flow control parameters.
	 * XON  : 80% of Rx FIFO
	 * XOFF : 30% of Rx FIFO
	 */
	reg = CSR_READ_4(sc, ALC_SRAM_RX_FIFO_LEN);
	rxf_hi = (reg * 8) / 10;
	rxf_lo = (reg * 3)/ 10;
	CSR_WRITE_4(sc, ALC_RX_FIFO_PAUSE_THRESH,
	    ((rxf_lo << RX_FIFO_PAUSE_THRESH_LO_SHIFT) &
	    RX_FIFO_PAUSE_THRESH_LO_MASK) |
	    ((rxf_hi << RX_FIFO_PAUSE_THRESH_HI_SHIFT) &
	     RX_FIFO_PAUSE_THRESH_HI_MASK));

	/* Disable RSS until I understand L1C/L2C's RSS logic. */
	CSR_WRITE_4(sc, ALC_RSS_IDT_TABLE0, 0);
	CSR_WRITE_4(sc, ALC_RSS_CPU, 0);

	/* Configure RxQ. */
	reg = (RXQ_CFG_RD_BURST_DEFAULT << RXQ_CFG_RD_BURST_SHIFT) &
	    RXQ_CFG_RD_BURST_MASK;
	reg |= RXQ_CFG_RSS_MODE_DIS;
	if ((sc->alc_flags & ALC_FLAG_ASPM_MON) != 0)
		reg |= RXQ_CFG_ASPM_THROUGHPUT_LIMIT_100M;
	CSR_WRITE_4(sc, ALC_RXQ_CFG, reg);

	/* Configure Rx DMAW request thresold. */
	CSR_WRITE_4(sc, ALC_RD_DMA_CFG,
	    ((RD_DMA_CFG_THRESH_DEFAULT << RD_DMA_CFG_THRESH_SHIFT) &
	    RD_DMA_CFG_THRESH_MASK) |
	    ((ALC_RD_DMA_CFG_USECS(0) << RD_DMA_CFG_TIMER_SHIFT) &
	    RD_DMA_CFG_TIMER_MASK));
	/* Configure DMA parameters. */
	reg = DMA_CFG_OUT_ORDER | DMA_CFG_RD_REQ_PRI;
	reg |= sc->alc_rcb;
	if ((sc->alc_flags & ALC_FLAG_CMB_BUG) == 0)
		reg |= DMA_CFG_CMB_ENB;
	if ((sc->alc_flags & ALC_FLAG_SMB_BUG) == 0)
		reg |= DMA_CFG_SMB_ENB;
	else
		reg |= DMA_CFG_SMB_DIS;
	reg |= (sc->alc_dma_rd_burst & DMA_CFG_RD_BURST_MASK) <<
	    DMA_CFG_RD_BURST_SHIFT;
	reg |= (sc->alc_dma_wr_burst & DMA_CFG_WR_BURST_MASK) <<
	    DMA_CFG_WR_BURST_SHIFT;
	reg |= (DMA_CFG_RD_DELAY_CNT_DEFAULT << DMA_CFG_RD_DELAY_CNT_SHIFT) &
	    DMA_CFG_RD_DELAY_CNT_MASK;
	reg |= (DMA_CFG_WR_DELAY_CNT_DEFAULT << DMA_CFG_WR_DELAY_CNT_SHIFT) &
	    DMA_CFG_WR_DELAY_CNT_MASK;
	CSR_WRITE_4(sc, ALC_DMA_CFG, reg);

	/*
	 * Configure Tx/Rx MACs.
	 *  - Auto-padding for short frames.
	 *  - Enable CRC generation.
	 *  Actual reconfiguration of MAC for resolved speed/duplex
	 *  is followed after detection of link establishment.
	 *  AR8131/AR8132 always does checksum computation regardless
	 *  of MAC_CFG_RXCSUM_ENB bit. Also the controller is known to
	 *  have bug in protocol field in Rx return structure so
	 *  these controllers can't handle fragmented frames. Disable
	 *  Rx checksum offloading until there is a newer controller
	 *  that has sane implementation.
	 */
	reg = MAC_CFG_TX_CRC_ENB | MAC_CFG_TX_AUTO_PAD | MAC_CFG_FULL_DUPLEX |
	    ((MAC_CFG_PREAMBLE_DEFAULT << MAC_CFG_PREAMBLE_SHIFT) &
	    MAC_CFG_PREAMBLE_MASK);
	if ((sc->alc_flags & ALC_FLAG_FASTETHER) != 0)
		reg |= MAC_CFG_SPEED_10_100;
	else
		reg |= MAC_CFG_SPEED_1000;
	CSR_WRITE_4(sc, ALC_MAC_CFG, reg);

	/* Set up the receive filter. */
	alc_iff(sc);

	alc_rxvlan(sc);

	/* Acknowledge all pending interrupts and clear it. */
	CSR_WRITE_4(sc, ALC_INTR_MASK, ALC_INTRS);
	CSR_WRITE_4(sc, ALC_INTR_STATUS, 0xFFFFFFFF);
	CSR_WRITE_4(sc, ALC_INTR_STATUS, 0);

	sc->alc_flags &= ~ALC_FLAG_LINK;
	/* Switch to the current media. */
	mii = &sc->sc_miibus;
	mii_mediachg(mii);

	timeout_add_sec(&sc->alc_tick_ch, 1);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return (0);
}

void
alc_stop(struct alc_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct alc_txdesc *txd;
	struct alc_rxdesc *rxd;
	uint32_t reg;
	int i;

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	timeout_del(&sc->alc_tick_ch);
	sc->alc_flags &= ~ALC_FLAG_LINK;

	alc_stats_update(sc);

	/* Disable interrupts. */
	CSR_WRITE_4(sc, ALC_INTR_MASK, 0);
	CSR_WRITE_4(sc, ALC_INTR_STATUS, 0xFFFFFFFF);
	alc_stop_queue(sc);

	/* Disable DMA. */
	reg = CSR_READ_4(sc, ALC_DMA_CFG);
	reg &= ~(DMA_CFG_CMB_ENB | DMA_CFG_SMB_ENB);
	reg |= DMA_CFG_SMB_DIS;
	CSR_WRITE_4(sc, ALC_DMA_CFG, reg);
	DELAY(1000);

	/* Stop Rx/Tx MACs. */
	alc_stop_mac(sc);

	/* Disable interrupts which might be touched in taskq handler. */
	CSR_WRITE_4(sc, ALC_INTR_STATUS, 0xFFFFFFFF);

	/* Reclaim Rx buffers that have been processed. */
	if (sc->alc_cdata.alc_rxhead != NULL)
		m_freem(sc->alc_cdata.alc_rxhead);
	ALC_RXCHAIN_RESET(sc);
	/*
	 * Free Tx/Rx mbufs still in the queues.
	 */
	for (i = 0; i < ALC_RX_RING_CNT; i++) {
		rxd = &sc->alc_cdata.alc_rxdesc[i];
		if (rxd->rx_m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxd->rx_dmamap);
			m_freem(rxd->rx_m);
			rxd->rx_m = NULL;
		}
	}
	for (i = 0; i < ALC_TX_RING_CNT; i++) {
		txd = &sc->alc_cdata.alc_txdesc[i];
		if (txd->tx_m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txd->tx_dmamap);
			m_freem(txd->tx_m);
			txd->tx_m = NULL;
		}
	}
}

void
alc_stop_mac(struct alc_softc *sc)
{
	uint32_t reg;
	int i;

	/* Disable Rx/Tx MAC. */
	reg = CSR_READ_4(sc, ALC_MAC_CFG);
	if ((reg & (MAC_CFG_TX_ENB | MAC_CFG_RX_ENB)) != 0) {
		reg &= ~(MAC_CFG_TX_ENB | MAC_CFG_RX_ENB);
		CSR_WRITE_4(sc, ALC_MAC_CFG, reg);
	}
	for (i = ALC_TIMEOUT; i > 0; i--) {
		reg = CSR_READ_4(sc, ALC_IDLE_STATUS);
		if (reg == 0)
			break;
		DELAY(10);
	}
	if (i == 0)
		printf("%s: could not disable Rx/Tx MAC(0x%08x)!\n",
		    sc->sc_dev.dv_xname, reg);
}

void
alc_start_queue(struct alc_softc *sc)
{
	uint32_t qcfg[] = {
		0,
		RXQ_CFG_QUEUE0_ENB,
		RXQ_CFG_QUEUE0_ENB | RXQ_CFG_QUEUE1_ENB,
		RXQ_CFG_QUEUE0_ENB | RXQ_CFG_QUEUE1_ENB | RXQ_CFG_QUEUE2_ENB,
		RXQ_CFG_ENB
	};
	uint32_t cfg;

	/* Enable RxQ. */
	cfg = CSR_READ_4(sc, ALC_RXQ_CFG);
	cfg &= ~RXQ_CFG_ENB;
	cfg |= qcfg[1];
	CSR_WRITE_4(sc, ALC_RXQ_CFG, cfg);
	/* Enable TxQ. */
	cfg = CSR_READ_4(sc, ALC_TXQ_CFG);
	cfg |= TXQ_CFG_ENB;
	CSR_WRITE_4(sc, ALC_TXQ_CFG, cfg);
}

void
alc_stop_queue(struct alc_softc *sc)
{
	uint32_t reg;
	int i;

	/* Disable RxQ. */
	reg = CSR_READ_4(sc, ALC_RXQ_CFG);
	if ((reg & RXQ_CFG_ENB) != 0) {
		reg &= ~RXQ_CFG_ENB;
		CSR_WRITE_4(sc, ALC_RXQ_CFG, reg);
	}
	/* Disable TxQ. */
	reg = CSR_READ_4(sc, ALC_TXQ_CFG);
	if ((reg & TXQ_CFG_ENB) != 0) {
		reg &= ~TXQ_CFG_ENB;
		CSR_WRITE_4(sc, ALC_TXQ_CFG, reg);
	}
	for (i = ALC_TIMEOUT; i > 0; i--) {
		reg = CSR_READ_4(sc, ALC_IDLE_STATUS);
		if ((reg & (IDLE_STATUS_RXQ | IDLE_STATUS_TXQ)) == 0)
			break;
		DELAY(10);
	}
	if (i == 0)
		printf("%s: could not disable RxQ/TxQ (0x%08x)!\n",
		    sc->sc_dev.dv_xname, reg);
}

void
alc_init_tx_ring(struct alc_softc *sc)
{
	struct alc_ring_data *rd;
	struct alc_txdesc *txd;
	int i;

	sc->alc_cdata.alc_tx_prod = 0;
	sc->alc_cdata.alc_tx_cons = 0;
	sc->alc_cdata.alc_tx_cnt = 0;

	rd = &sc->alc_rdata;
	bzero(rd->alc_tx_ring, ALC_TX_RING_SZ);
	for (i = 0; i < ALC_TX_RING_CNT; i++) {
		txd = &sc->alc_cdata.alc_txdesc[i];
		txd->tx_m = NULL;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_tx_ring_map, 0,
	    sc->alc_cdata.alc_tx_ring_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

int
alc_init_rx_ring(struct alc_softc *sc)
{
	struct alc_ring_data *rd;
	struct alc_rxdesc *rxd;
	int i;

	sc->alc_cdata.alc_rx_cons = ALC_RX_RING_CNT - 1;
	rd = &sc->alc_rdata;
	bzero(rd->alc_rx_ring, ALC_RX_RING_SZ);
	for (i = 0; i < ALC_RX_RING_CNT; i++) {
		rxd = &sc->alc_cdata.alc_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_desc = &rd->alc_rx_ring[i];
		if (alc_newbuf(sc, rxd) != 0)
			return (ENOBUFS);
	}

	/*
	 * Since controller does not update Rx descriptors, driver
	 * does have to read Rx descriptors back so BUS_DMASYNC_PREWRITE
	 * is enough to ensure coherence.
	 */
	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_rx_ring_map, 0,
	    sc->alc_cdata.alc_rx_ring_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	/* Let controller know availability of new Rx buffers. */
	CSR_WRITE_4(sc, ALC_MBOX_RD0_PROD_IDX, sc->alc_cdata.alc_rx_cons);

	return (0);
}

void
alc_init_rr_ring(struct alc_softc *sc)
{
	struct alc_ring_data *rd;

	sc->alc_cdata.alc_rr_cons = 0;
	ALC_RXCHAIN_RESET(sc);

	rd = &sc->alc_rdata;
	bzero(rd->alc_rr_ring, ALC_RR_RING_SZ);
	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_rr_ring_map, 0,
	    sc->alc_cdata.alc_rr_ring_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

void
alc_init_cmb(struct alc_softc *sc)
{
	struct alc_ring_data *rd;

	rd = &sc->alc_rdata;
	bzero(rd->alc_cmb, ALC_CMB_SZ);
	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_cmb_map, 0,
	    sc->alc_cdata.alc_cmb_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

void
alc_init_smb(struct alc_softc *sc)
{
	struct alc_ring_data *rd;

	rd = &sc->alc_rdata;
	bzero(rd->alc_smb, ALC_SMB_SZ);
	bus_dmamap_sync(sc->sc_dmat, sc->alc_cdata.alc_smb_map, 0,
	    sc->alc_cdata.alc_smb_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

void
alc_rxvlan(struct alc_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	uint32_t reg;

	reg = CSR_READ_4(sc, ALC_MAC_CFG);
	if (ifp->if_capabilities & IFCAP_VLAN_HWTAGGING)
		reg |= MAC_CFG_VLAN_TAG_STRIP;
	else
		reg &= ~MAC_CFG_VLAN_TAG_STRIP;
	CSR_WRITE_4(sc, ALC_MAC_CFG, reg);
}

void
alc_iff(struct alc_softc *sc)
{
	struct arpcom *ac = &sc->sc_arpcom;
	struct ifnet *ifp = &ac->ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t crc;
	uint32_t mchash[2];
	uint32_t rxcfg;

	rxcfg = CSR_READ_4(sc, ALC_MAC_CFG);
	rxcfg &= ~(MAC_CFG_ALLMULTI | MAC_CFG_BCAST | MAC_CFG_PROMISC);
	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * Always accept broadcast frames.
	 */
	rxcfg |= MAC_CFG_BCAST;

	if (ifp->if_flags & IFF_PROMISC || ac->ac_multirangecnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			rxcfg |= MAC_CFG_PROMISC;
		else
			rxcfg |= MAC_CFG_ALLMULTI;
		mchash[0] = mchash[1] = 0xFFFFFFFF;
	} else {
		/* Program new filter. */
		bzero(mchash, sizeof(mchash));

		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			crc = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN);

			mchash[crc >> 31] |= 1 << ((crc >> 26) & 0x1f);

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	CSR_WRITE_4(sc, ALC_MAR0, mchash[0]);
	CSR_WRITE_4(sc, ALC_MAR1, mchash[1]);
	CSR_WRITE_4(sc, ALC_MAC_CFG, rxcfg);
}
