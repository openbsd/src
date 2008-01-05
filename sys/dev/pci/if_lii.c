/*	$OpenBSD: if_lii.c,v 1.5 2008/01/05 03:49:06 deraadt Exp $	*/

/*
 *  Copyright (c) 2007 The NetBSD Foundation.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of The NetBSD Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for Attansic/Atheros's L2 Fast Ethernet controller
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/bus.h>

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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_liireg.h>

/*#define ATL2_DEBUG*/
#ifdef ATL2_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

struct atl2_softc {
	struct device		sc_dev;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	bus_space_tag_t		sc_mmiot;
	bus_space_handle_t	sc_mmioh;

	/*
	 * We allocate a big chunk of DMA-safe memory for all data exchanges.
	 * It is unfortunate that this chip doesn't seem to do scatter-gather.
	 */
	bus_dma_tag_t		sc_dmat;
	bus_dmamap_t		sc_ringmap;
	bus_dma_segment_t	sc_ringseg;

	uint8_t			*sc_ring; /* the whole area */
	size_t			sc_ringsize;

	struct rx_pkt		*sc_rxp; /* the part used for RX */
	struct tx_pkt_status	*sc_txs; /* the parts used for TX */
	bus_addr_t		sc_txsp;
	char			*sc_txdbase;
	bus_addr_t		sc_txdp;

	unsigned int		sc_rxcur;
	/* the active area is [ack; cur[ */
	int			sc_txs_cur;
	int			sc_txs_ack;
	int			sc_txd_cur;
	int			sc_txd_ack;
	int			sc_free_tx_slots;

	void			*sc_ih;

	struct arpcom		sc_ac;
	struct mii_data		sc_mii;
	struct timeout		sc_tick;

	int			(*sc_memread)(struct atl2_softc *, uint32_t,
				     uint32_t *);
};

#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

int	atl2_match(struct device *, void *, void *);
void	atl2_attach(struct device *, struct device *, void *);

struct cfdriver lii_cd = {
	0,
	"lii",
	DV_IFNET
};

struct cfattach lii_ca = {
	sizeof(struct atl2_softc),
	atl2_match,
	atl2_attach
};

int	atl2_reset(struct atl2_softc *);
int	atl2_eeprom_present(struct atl2_softc *);
int	atl2_read_macaddr(struct atl2_softc *, uint8_t *);
int	atl2_eeprom_read(struct atl2_softc *, uint32_t, uint32_t *);
int	atl2_spi_read(struct atl2_softc *, uint32_t, uint32_t *);
void	atl2_setmulti(struct atl2_softc *);
void	atl2_tick(void *);

int	atl2_alloc_rings(struct atl2_softc *);
int	atl2_free_tx_space(struct atl2_softc *);
void	atl2_tx_put(struct atl2_softc *, struct mbuf *);

int	atl2_mii_readreg(struct device *, int, int);
void	atl2_mii_writereg(struct device *, int, int, int);
void	atl2_mii_statchg(struct device *);

int	atl2_media_change(struct ifnet *);
void	atl2_media_status(struct ifnet *, struct ifmediareq *);

int	atl2_init(struct ifnet *);
void	atl2_start(struct ifnet *);
void	atl2_stop(struct ifnet *);
void	atl2_watchdog(struct ifnet *);
int	atl2_ioctl(struct ifnet *, u_long, caddr_t);

int	atl2_intr(void *);
void	atl2_rxintr(struct atl2_softc *);
void	atl2_txintr(struct atl2_softc *);

#define AT_READ_4(sc,reg) \
    bus_space_read_4((sc)->sc_mmiot, (sc)->sc_mmioh, (reg))
#define AT_READ_2(sc,reg) \
    bus_space_read_2((sc)->sc_mmiot, (sc)->sc_mmioh, (reg))
#define AT_READ_1(sc,reg) \
    bus_space_read_1((sc)->sc_mmiot, (sc)->sc_mmioh, (reg))
#define AT_WRITE_4(sc,reg,val) \
    bus_space_write_4((sc)->sc_mmiot, (sc)->sc_mmioh, (reg), (val))
#define AT_WRITE_2(sc,reg,val) \
    bus_space_write_2((sc)->sc_mmiot, (sc)->sc_mmioh, (reg), (val))
#define AT_WRITE_1(sc,reg,val) \
    bus_space_write_1((sc)->sc_mmiot, (sc)->sc_mmioh, (reg), (val))

/*
 * Those are the default Linux parameters.
 */

#define AT_TXD_NUM		64
#define AT_TXD_BUFFER_SIZE	8192
#define AT_RXD_NUM		64

int
atl2_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	return (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ATTANSIC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATTANSIC_L2);
}

void
atl2_attach(struct device *parent, struct device *self, void *aux)
{
	struct atl2_softc *sc = (struct atl2_softc *)self;
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	pci_intr_handle_t ih;
	pcireg_t mem;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	mem = pci_mapreg_type(sc->sc_pc, sc->sc_tag, PCI_MAPREG_START);
	switch (mem) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT_1M:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		break;
	default:
		printf(": invalid base address register\n");
		return;
	}
	if (pci_mapreg_map(pa, PCI_MAPREG_START, mem, 0,
	    &sc->sc_mmiot, &sc->sc_mmioh, NULL, NULL, 0) != 0) {
		printf(": failed to map registers\n");
		return;
	}

	if (atl2_reset(sc))
		return;

	/* XXX set correct opcodes for the flash */

	if (atl2_eeprom_present(sc))
		sc->sc_memread = atl2_eeprom_read;
	else
		sc->sc_memread = atl2_spi_read;

	if (atl2_read_macaddr(sc, sc->sc_ac.ac_enaddr))
		return;

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": failed to map interrupt\n");
		/* XXX cleanup */
		return;
	}
	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_NET,
	    atl2_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": failed to establish interrupt\n");
		/* XXX cleanup */
		return;
	}

	if (atl2_alloc_rings(sc)) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		return;
	}

	timeout_set(&sc->sc_tick, atl2_tick, sc);

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = atl2_mii_readreg;
	sc->sc_mii.mii_writereg = atl2_mii_writereg;
	sc->sc_mii.mii_statchg = atl2_mii_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, atl2_media_change,
	    atl2_media_status);
	mii_attach(self, &sc->sc_mii, 0xffffffff, 1,
	    MII_OFFSET_ANY, 0);
	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	ifp->if_ioctl = atl2_ioctl;
	ifp->if_start = atl2_start;
	ifp->if_watchdog = atl2_watchdog;
	ifp->if_init = atl2_init;
	IFQ_SET_READY(&ifp->if_snd);

	printf("%s, address %s\n", pci_intr_string(sc->sc_pc, ih),
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	if_attach(ifp);
	ether_ifattach(ifp);
}

int
atl2_reset(struct atl2_softc *sc)
{
	int i;

	DPRINTF(("atl2_reset\n"));

	AT_WRITE_4(sc, ATL2_SMC, SMC_SOFT_RST);
	DELAY(1000);

	for (i = 0; i < 10; ++i) {
		if (AT_READ_4(sc, ATL2_BIS) == 0)
			break;
		DELAY(1000);
	}

	if (i == 10) {
		printf("%s: reset failed\n", DEVNAME(sc));
		return 1;
	}

	AT_WRITE_4(sc, ATL2_PHYC, PHYC_ENABLE);
	DELAY(10);

	/* Init PCI-Express module */
	/* Magic Numbers Warning */
	AT_WRITE_4(sc, 0x12fc, 0x00006500);
	AT_WRITE_4(sc, 0x1008, 0x00008000 |
	    AT_READ_4(sc, 0x1008));

	return 0;
}

int
atl2_eeprom_present(struct atl2_softc *sc)
{
	uint32_t val;

	val = AT_READ_4(sc, ATL2_SFC);
	if (val & SFC_EN_VPD)
		AT_WRITE_4(sc, ATL2_SFC, val & ~(SFC_EN_VPD));

	return pci_get_capability(sc->sc_pc, sc->sc_tag, PCI_CAP_VPD,
	    NULL, NULL) == 1;
}

int
atl2_eeprom_read(struct atl2_softc *sc, uint32_t reg, uint32_t *val)
{
	return pci_vpd_read(sc->sc_pc, sc->sc_tag, reg, 1, (pcireg_t *)val);
}

#define MAKE_SFC(cssetup, clkhi, clklo, cshold, cshi, ins) \
    ( (((cssetup) & SFC_CS_SETUP_MASK)	\
        << SFC_CS_SETUP_SHIFT)		\
    | (((clkhi) & SFC_CLK_HI_MASK)	\
        << SFC_CLK_HI_SHIFT)		\
    | (((clklo) & SFC_CLK_LO_MASK)	\
        << SFC_CLK_LO_SHIFT)		\
    | (((cshold) & SFC_CS_HOLD_MASK)	\
        << SFC_CS_HOLD_SHIFT)		\
    | (((cshi) & SFC_CS_HI_MASK)	\
        << SFC_CS_HI_SHIFT)		\
    | (((ins) & SFC_INS_MASK)		\
        << SFC_INS_SHIFT))

#define CUSTOM_SPI_CS_SETUP	2
#define CUSTOM_SPI_CLK_HI	2
#define CUSTOM_SPI_CLK_LO	2
#define CUSTOM_SPI_CS_HOLD	2
#define CUSTOM_SPI_CS_HI	3

int
atl2_spi_read(struct atl2_softc *sc, uint32_t reg, uint32_t *val)
{
	uint32_t v;
	int i;

	AT_WRITE_4(sc, ATL2_SF_DATA, 0);
	AT_WRITE_4(sc, ATL2_SF_ADDR, reg);

	v = SFC_WAIT_READY |
	    MAKE_SFC(CUSTOM_SPI_CS_SETUP, CUSTOM_SPI_CLK_HI,
	         CUSTOM_SPI_CLK_LO, CUSTOM_SPI_CS_HOLD, CUSTOM_SPI_CS_HI, 1);

	AT_WRITE_4(sc, ATL2_SFC, v);
	v |= SFC_START;
	AT_WRITE_4(sc, ATL2_SFC, v);

	for (i = 0; i < 10; ++i) {
		DELAY(1000);
		if (!(AT_READ_4(sc, ATL2_SFC) & SFC_START))
			break;
	}
	if (i == 10)
		return EBUSY;

	*val = AT_READ_4(sc, ATL2_SF_DATA);
	return 0;
}

int
atl2_read_macaddr(struct atl2_softc *sc, uint8_t *ea)
{
	uint32_t offset = 0x100;
	uint32_t val, val1, addr0 = 0, addr1 = 0;
	uint8_t found = 0;

	while ((*sc->sc_memread)(sc, offset, &val) == 0) {
		offset += 4;

		/* Each chunk of data starts with a signature */
		if ((val & 0xff) != 0x5a)
			break;
		if ((*sc->sc_memread)(sc, offset, &val1))
			break;

		offset += 4;

		val >>= 16;
		switch (val) {
		case ATL2_MAC_ADDR_0:
			addr0 = val1;
			++found;
			break;
		case ATL2_MAC_ADDR_1:
			addr1 = val1;
			++found;
			break;
		default:
			continue;
		}
	}

	if (found < 2) {
		printf(": error reading MAC address\n");
		return 1;
	}

	addr0 = htole32(addr0);
	addr1 = htole32(addr1);

	if ((addr0 == 0xffffff && (addr1 & 0xffff) == 0xffff) ||
	    (addr0 == 0 && (addr1 & 0xffff) == 0)) {
		addr0 = htole32(AT_READ_4(sc, ATL2_MAC_ADDR_0));
		addr1 = htole32(AT_READ_4(sc, ATL2_MAC_ADDR_1));
	}

	ea[0] = (addr1 & 0x0000ff00) >> 8;
	ea[1] = (addr1 & 0x000000ff);
	ea[2] = (addr0 & 0xff000000) >> 24;
	ea[3] = (addr0 & 0x00ff0000) >> 16;
	ea[4] = (addr0 & 0x0000ff00) >> 8;
	ea[5] = (addr0 & 0x000000ff);

	return 0;
}

int
atl2_mii_readreg(struct device *dev, int phy, int reg)
{
	struct atl2_softc *sc = (struct atl2_softc *)dev;
	uint32_t val;
	int i;

	val = (reg & MDIOC_REG_MASK) << MDIOC_REG_SHIFT;

	val |= MDIOC_START | MDIOC_SUP_PREAMBLE;
	val |= MDIOC_CLK_25_4 << MDIOC_CLK_SEL_SHIFT;

	val |= MDIOC_READ;

	AT_WRITE_4(sc, ATL2_MDIOC, val);

	for (i = 0; i < MDIO_WAIT_TIMES; ++i) {
		DELAY(2);
		val = AT_READ_4(sc, ATL2_MDIOC);
		if ((val & (MDIOC_START | MDIOC_BUSY)) == 0)
			break;
	}

	if (i == MDIO_WAIT_TIMES) {
		printf("%s: timeout reading PHY %d reg %d\n", DEVNAME(sc), phy,
		    reg);
	}

	return (val & 0x0000ffff);
}

void
atl2_mii_writereg(struct device *dev, int phy, int reg, int data)
{
	struct atl2_softc *sc = (struct atl2_softc *)dev;
	uint32_t val;
	int i;

	val = (reg & MDIOC_REG_MASK) << MDIOC_REG_SHIFT;
	val |= (data & MDIOC_DATA_MASK) << MDIOC_DATA_SHIFT;

	val |= MDIOC_START | MDIOC_SUP_PREAMBLE;
	val |= MDIOC_CLK_25_4 << MDIOC_CLK_SEL_SHIFT;

	/* val |= MDIOC_WRITE; */

	AT_WRITE_4(sc, ATL2_MDIOC, val);

	for (i = 0; i < MDIO_WAIT_TIMES; ++i) {
		DELAY(2);
		val = AT_READ_4(sc, ATL2_MDIOC);
		if ((val & (MDIOC_START | MDIOC_BUSY)) == 0)
			break;
	}

	if (i == MDIO_WAIT_TIMES) {
		printf("%s: timeout writing PHY %d reg %d\n", DEVNAME(sc), phy,
		    reg);
	}
}

void
atl2_mii_statchg(struct device *dev)
{
	struct atl2_softc *sc = (struct atl2_softc *)dev;
	uint32_t val;

	DPRINTF(("atl2_mii_statchg\n"));

	val = AT_READ_4(sc, ATL2_MACC);

	if ((sc->sc_mii.mii_media_active & IFM_GMASK) == IFM_FDX)
		val |= MACC_FDX;
	else
		val &= ~MACC_FDX;

	AT_WRITE_4(sc, ATL2_MACC, val);
}

int
atl2_media_change(struct ifnet *ifp)
{
	struct atl2_softc *sc = ifp->if_softc;

	DPRINTF(("atl2_media_change\n"));

	if (ifp->if_flags & IFF_UP)
		mii_mediachg(&sc->sc_mii);
	return 0;
}

void
atl2_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct atl2_softc *sc = ifp->if_softc;

	DPRINTF(("atl2_media_status\n"));

	mii_pollstat(&sc->sc_mii);
	imr->ifm_status = sc->sc_mii.mii_media_status;
	imr->ifm_active = sc->sc_mii.mii_media_active;
}

int
atl2_init(struct ifnet *ifp)
{
	struct atl2_softc *sc = ifp->if_softc;
	int error;

	DPRINTF(("atl2_init\n"));

	atl2_stop(ifp);

	memset(sc->sc_ring, 0, sc->sc_ringsize);

	/* Disable all interrupts */
	AT_WRITE_4(sc, ATL2_ISR, 0xffffffff);

	AT_WRITE_4(sc, ATL2_DESC_BASE_ADDR_HI, 0);
/* XXX
	    sc->sc_ringmap->dm_segs[0].ds_addr >> 32);
*/
	AT_WRITE_4(sc, ATL2_RXD_BASE_ADDR_LO,
	    sc->sc_ringmap->dm_segs[0].ds_addr & 0xffffffff);
	AT_WRITE_4(sc, ATL2_TXS_BASE_ADDR_LO,
	    sc->sc_txsp & 0xffffffff);
	AT_WRITE_4(sc, ATL2_TXD_BASE_ADDR_LO,
	    sc->sc_txdp & 0xffffffff);

	AT_WRITE_2(sc, ATL2_TXD_BUFFER_SIZE, AT_TXD_BUFFER_SIZE / 4);
	AT_WRITE_2(sc, ATL2_TXS_NUM_ENTRIES, AT_TXD_NUM);
	AT_WRITE_2(sc, ATL2_RXD_NUM_ENTRIES, AT_RXD_NUM);

	/*
	 * Inter Paket Gap Time = 0x60 (IPGT)
	 * Minimum inter-frame gap for RX = 0x50 (MIFG)
	 * 64-bit Carrier-Sense window = 0x40 (IPGR1)
	 * 96-bit IPG window = 0x60 (IPGR2)
	 */
	AT_WRITE_4(sc, ATL2_MIPFG, 0x60405060);

	/*
	 * Collision window = 0x37 (LCOL)
	 * Maximum # of retrans = 0xf (RETRY)
	 * Maximum binary expansion # = 0xa (ABEBT)
	 * IPG to start jam = 0x7 (JAMIPG)
	*/
	AT_WRITE_4(sc, ATL2_MHDC, 0x07a0f037 |
	     MHDC_EXC_DEF_EN);

#if 0
	/* 100 means 200us */
	AT_WRITE_2(sc, ATL2_IMTIV, 100);
	AT_WRITE_2(sc, ATL2_SMC, SMC_ITIMER_EN);

	/* 500000 means 100ms */
	AT_WRITE_2(sc, ATL2_IALTIV, 50000);
#endif

	AT_WRITE_4(sc, ATL2_MTU, ifp->if_mtu + ETHER_HDR_LEN
	    + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN);

	/* unit unknown for TX cur-through threshold */
	AT_WRITE_4(sc, ATL2_TX_CUT_THRESH, 0x177);

	AT_WRITE_2(sc, ATL2_PAUSE_ON_TH, AT_RXD_NUM * 7 / 8);
	AT_WRITE_2(sc, ATL2_PAUSE_OFF_TH, AT_RXD_NUM / 12);

	sc->sc_rxcur = 0;
	sc->sc_txs_cur = sc->sc_txs_ack = 0;
	sc->sc_txd_cur = sc->sc_txd_ack = 0;
	sc->sc_free_tx_slots = 1;
	AT_WRITE_2(sc, ATL2_MB_TXD_WR_IDX, sc->sc_txd_cur);
	AT_WRITE_2(sc, ATL2_MB_RXD_RD_IDX, sc->sc_rxcur);

	AT_WRITE_1(sc, ATL2_DMAR, DMAR_EN);
	AT_WRITE_1(sc, ATL2_DMAW, DMAW_EN);

	AT_WRITE_4(sc, ATL2_SMC, AT_READ_4(sc, ATL2_SMC) | SMC_MANUAL_INT);

	error = ((AT_READ_4(sc, ATL2_ISR) & ISR_PHY_LINKDOWN) != 0);
	AT_WRITE_4(sc, ATL2_ISR, 0x3fffffff);
	AT_WRITE_4(sc, ATL2_ISR, 0);
	if (error) {
		printf("%s: init failed\n", DEVNAME(sc));
		goto out;
	}

	atl2_setmulti(sc);
	mii_mediachg(&sc->sc_mii);

	AT_WRITE_4(sc, ATL2_IMR, IMR_NORMAL_MASK);

	timeout_add(&sc->sc_tick, hz);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

out:
	return error;
}

void
atl2_tx_put(struct atl2_softc *sc, struct mbuf *m)
{
	int left;
	struct tx_pkt_header *tph =
	    (struct tx_pkt_header *)(sc->sc_txdbase + sc->sc_txd_cur);

	memset(tph, 0, sizeof *tph);
	tph->txph_size = m->m_pkthdr.len;

	sc->sc_txd_cur = (sc->sc_txd_cur + 4) % AT_TXD_BUFFER_SIZE;

	/*
	 * We already know we have enough space, so if there is a part of the
	 * space ahead of txd_cur that is active, it doesn't matter because
	 * left will be large enough even without it.
	 */
	left  = AT_TXD_BUFFER_SIZE - sc->sc_txd_cur;

	if (left > m->m_pkthdr.len) {
		m_copydata(m, 0, m->m_pkthdr.len,
		    sc->sc_txdbase + sc->sc_txd_cur);
		sc->sc_txd_cur += m->m_pkthdr.len;
	} else {
		m_copydata(m, 0, left, sc->sc_txdbase + sc->sc_txd_cur);
		m_copydata(m, left, m->m_pkthdr.len - left, sc->sc_txdbase);
		sc->sc_txd_cur = m->m_pkthdr.len - left;
	}

	/* Round to a 32-bit boundary */
	sc->sc_txd_cur = (sc->sc_txd_cur + 3) & ~3;
	if (sc->sc_txd_cur == sc->sc_txd_ack)
		sc->sc_free_tx_slots = 0;
}

int
atl2_free_tx_space(struct atl2_softc *sc)
{
	int space;

	if (sc->sc_txd_cur >= sc->sc_txd_ack)
		space = (AT_TXD_BUFFER_SIZE - sc->sc_txd_cur) +
		    sc->sc_txd_ack;
	else
		space = sc->sc_txd_ack - sc->sc_txd_cur;

	/* Account for the tx_pkt_header */
	return (space - 4);
}

void
atl2_start(struct ifnet *ifp)
{
	struct atl2_softc *sc = ifp->if_softc;
	struct mbuf *m0;

	DPRINTF(("atl2_start\n"));

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (!sc->sc_free_tx_slots ||
		    atl2_free_tx_space(sc) < m0->m_pkthdr.len) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		atl2_tx_put(sc, m0);

		DPRINTF(("atl2_start: put %d\n", sc->sc_txs_cur));

		sc->sc_txs[sc->sc_txs_cur].txps_update = 0;
		sc->sc_txs_cur = (sc->sc_txs_cur + 1) % AT_TXD_NUM;
		if (sc->sc_txs_cur == sc->sc_txs_ack)
			sc->sc_free_tx_slots = 0;

		AT_WRITE_2(sc, ATL2_MB_TXD_WR_IDX, sc->sc_txd_cur/4);

		IFQ_DEQUEUE(&ifp->if_snd, m0);

#if NBPFILTER > 0
		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
		m_freem(m0);
	}
}

void
atl2_stop(struct ifnet *ifp)
{
	struct atl2_softc *sc = ifp->if_softc;

	timeout_del(&sc->sc_tick);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	mii_down(&sc->sc_mii);

	atl2_reset(sc);

	AT_WRITE_4(sc, ATL2_IMR, 0);
}

int
atl2_intr(void *v)
{
	struct atl2_softc *sc = v;
	uint32_t status;

	status = AT_READ_4(sc, ATL2_ISR);
	if (status == 0)
		return 0;

	DPRINTF(("atl2_intr (%x)\n", status));

	/* Clear the interrupt and disable them */
	AT_WRITE_4(sc, ATL2_ISR, status | ISR_DIS_INT);

	if (status & (ISR_PHY | ISR_MANUAL)) {
		/* Ack PHY interrupt.  Magic register */
		if (status & ISR_PHY)
			(void)atl2_mii_readreg(&sc->sc_dev, 1, 19);
		mii_mediachg(&sc->sc_mii);
	}

	if (status & (ISR_DMAR_TO_RST | ISR_DMAW_TO_RST | ISR_PHY_LINKDOWN)) {
		atl2_init(&sc->sc_ac.ac_if);
		return 1;
	}

	if (status & ISR_RX_EVENT) {
#ifdef ATL2_DEBUG
		if (!(status & ISR_RS_UPDATE))
			printf("rxintr %08x\n", status);
#endif
		atl2_rxintr(sc);
	}

	if (status & ISR_TX_EVENT)
		atl2_txintr(sc);

	/* Re-enable interrupts */
	AT_WRITE_4(sc, ATL2_ISR, 0);

	return 1;
}

void
atl2_rxintr(struct atl2_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct rx_pkt *rxp;
	struct mbuf *m;
	uint16_t size;

	DPRINTF(("atl2_rxintr\n"));

	for (;;) {
		rxp = &sc->sc_rxp[sc->sc_rxcur];
		if (rxp->rxp_update == 0)
			break;

		DPRINTF(("atl2_rxintr: getting %u (%u) [%x]\n", sc->sc_rxcur,
		    rxp->rxp_size, rxp->rxp_flags));
		sc->sc_rxcur = (sc->sc_rxcur + 1) % AT_RXD_NUM;
		rxp->rxp_update = 0;
		if (!(rxp->rxp_flags & ATL2_RXF_SUCCESS)) {
			++ifp->if_ierrors;
			continue;
		}

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			++ifp->if_ierrors;
			continue;
		}
		size = rxp->rxp_size - ETHER_CRC_LEN;
		if (size > MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				++ifp->if_ierrors;
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		/* Copy the packet withhout the FCS */
		m->m_pkthdr.len = m->m_len = size;
		memcpy(mtod(m, void *), &rxp->rxp_data[0], size);
		++ifp->if_ipackets;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		ether_input_mbuf(ifp, m);
	}

	AT_WRITE_4(sc, ATL2_MB_RXD_RD_IDX, sc->sc_rxcur);
}

void
atl2_txintr(struct atl2_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct tx_pkt_status *txs;
	struct tx_pkt_header *txph;

	DPRINTF(("atl2_txintr\n"));

	for (;;) {
		txs = &sc->sc_txs[sc->sc_txs_ack];
		if (txs->txps_update == 0)
			break;
		DPRINTF(("atl2_txintr: ack'd %d\n", sc->sc_txs_ack));
		sc->sc_txs_ack = (sc->sc_txs_ack + 1) % AT_TXD_NUM;
		sc->sc_free_tx_slots = 1;

		txs->txps_update = 0;

		txph =  (struct tx_pkt_header *)
		    (sc->sc_txdbase + sc->sc_txd_ack);

		if (txph->txph_size != txs->txps_size) {
			printf("%s: mismatched status and packet\n",
			    DEVNAME(sc));
		}

		/*
		 * Move ack by the packet size, taking the packet header in
		 * account and round to the next 32-bit boundary
		 * (7 = sizeof(header) + 3)
		 */
		sc->sc_txd_ack = (sc->sc_txd_ack + txph->txph_size + 7 ) & ~3;
		sc->sc_txd_ack %= AT_TXD_BUFFER_SIZE;

		if (txs->txps_flags & ATL2_TXF_SUCCESS)
			++ifp->if_opackets;
		else
			++ifp->if_oerrors;
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	if (sc->sc_free_tx_slots)
		atl2_start(ifp);
}

int
atl2_alloc_rings(struct atl2_softc *sc)
{
	int nsegs;
	bus_size_t bs;

	/*
	 * We need a big chunk of DMA-friendly memory because descriptors
	 * are not separate from data on that crappy hardware, which means
	 * we'll have to copy data from and to that memory zone to and from
	 * the mbufs.
	 *
	 * How lame is that?  Using the default values from the Linux driver,
	 * we allocate space for receiving up to 64 full-size Ethernet frames,
	 * and only 8kb for transmitting up to 64 Ethernet frames.
	 */

	sc->sc_ringsize = bs = AT_RXD_NUM * sizeof(struct rx_pkt)
	    + AT_TXD_NUM * sizeof(struct tx_pkt_status)
	    + AT_TXD_BUFFER_SIZE;

	if (bus_dmamap_create(sc->sc_dmat, bs, 1, bs, (1<<30),
	    BUS_DMA_NOWAIT, &sc->sc_ringmap) != 0) {
		printf(": bus_dmamap_create failed\n");
		return 1;
	}

	if (bus_dmamem_alloc(sc->sc_dmat, bs, PAGE_SIZE, (1<<30),
	    &sc->sc_ringseg, 1, &nsegs, BUS_DMA_NOWAIT) != 0) {
		printf(": bus_dmamem_alloc failed\n");
		goto fail;
	}

	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_ringseg, nsegs, bs,
	    (caddr_t *)&sc->sc_ring, BUS_DMA_NOWAIT) != 0) {
		printf(": bus_dmamem_map failed\n");
		goto fail1;
	}

	if (bus_dmamap_load(sc->sc_dmat, sc->sc_ringmap, sc->sc_ring,
	    bs, NULL, BUS_DMA_NOWAIT) != 0) {
		printf(": bus_dmamap_load failed\n");
		goto fail2;
	}

	sc->sc_rxp = (void *)sc->sc_ring;
	sc->sc_txs =
	    (void *)(sc->sc_ring + AT_RXD_NUM * sizeof(struct rx_pkt));
	sc->sc_txdbase = ((char *)sc->sc_txs)
	    + AT_TXD_NUM * sizeof(struct tx_pkt_status);
	sc->sc_txsp = sc->sc_ringmap->dm_segs[0].ds_addr
	    + ((char *)sc->sc_txs - (char *)sc->sc_ring);
	sc->sc_txdp = sc->sc_ringmap->dm_segs[0].ds_addr
	    + ((char *)sc->sc_txdbase - (char *)sc->sc_ring);

	return 0;

fail2:
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_ring, bs);
fail1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_ringseg, nsegs);
fail:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ringmap);
	return 1;
}

void
atl2_watchdog(struct ifnet *ifp)
{
	struct atl2_softc *sc = ifp->if_softc;

	printf("%s: watchdog timeout\n", DEVNAME(sc));
	++ifp->if_oerrors;
	atl2_init(ifp);
}

int
atl2_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct atl2_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)addr;
	struct ifaddr *ifa;
	int s, error;

	s = splnet();

	error = ether_ioctl(ifp, &sc->sc_ac, cmd, addr);
	if (error > 0)
		goto err;

	switch(cmd) {
	case SIOCSIFADDR:
		SET(ifp->if_flags, IFF_UP);
#ifdef INET
		ifa = (struct ifaddr *)addr;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				atl2_init(ifp);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				atl2_stop(ifp);
		}
		break;

	case SIOCADDMULTI:
		error = ether_addmulti(ifr, &sc->sc_ac);
		break;
	case SIOCDELMULTI:
		error = ether_delmulti(ifr, &sc->sc_ac);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;
	default:
		error = ENOTTY;
		break;
	}

err:
	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			atl2_setmulti(sc);
		error = 0;
	}
	splx(s);

	return error;
}

void
atl2_setmulti(struct atl2_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t val;

	/* XXX That should be done atl2_init */
	val = AT_READ_4(sc, ATL2_MACC) & MACC_FDX;

	val |= MACC_RX_EN | MACC_TX_EN | MACC_MACLP_CLK_PHY |
	    MACC_TX_FLOW_EN | MACC_RX_FLOW_EN |
	    MACC_ADD_CRC | MACC_PAD | MACC_BCAST_EN;

	if (ifp->if_flags & IFF_PROMISC)
		val |= MACC_PROMISC_EN;
	else if (ifp->if_flags & IFF_ALLMULTI)
		val |= MACC_ALLMULTI_EN;

	val |= 7 << MACC_PREAMBLE_LEN_SHIFT;
	val |= 2 << MACC_HDX_LEFT_BUF_SHIFT;

	AT_WRITE_4(sc, ATL2_MACC, val);

	/* XXX actual setmulti needed */

	/* Clear multicast hash table */
	AT_WRITE_4(sc, ATL2_MHT, 0);
	AT_WRITE_4(sc, ATL2_MHT + 4, 0);
}

void
atl2_tick(void *v)
{
	struct atl2_softc *sc = v;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add(&sc->sc_tick, hz);
}
