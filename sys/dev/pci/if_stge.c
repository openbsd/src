/*	$NetBSD: if_stge.c,v 1.4 2001/07/25 15:44:48 thorpej Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Device driver for the Sundance Tech. TC9021 10/100/1000
 * Ethernet controller.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/pci/if_stgereg.h>

/*
 * Transmit descriptor list size.
 */
#define	STGE_NTXDESC		256
#define	STGE_NTXDESC_MASK	(STGE_NTXDESC - 1)
#define	STGE_NEXTTX(x)		(((x) + 1) & STGE_NTXDESC_MASK)

/*
 * Receive descriptor list size.
 */
#define	STGE_NRXDESC		256
#define	STGE_NRXDESC_MASK	(STGE_NRXDESC - 1)
#define	STGE_NEXTRX(x)		(((x) + 1) & STGE_NRXDESC_MASK)

/*
 * Only interrupt every N frames.  Must be a power-of-two.
 */
#define	STGE_TXINTR_SPACING	16
#define	STGE_TXINTR_SPACING_MASK (STGE_TXINTR_SPACING - 1)

/*
 * Control structures are DMA'd to the TC9021 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct stge_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct stge_tfd scd_txdescs[STGE_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct stge_rfd scd_rxdescs[STGE_NRXDESC];
};

#define	STGE_CDOFF(x)	offsetof(struct stge_control_data, x)
#define	STGE_CDTXOFF(x)	STGE_CDOFF(scd_txdescs[(x)])
#define	STGE_CDRXOFF(x)	STGE_CDOFF(scd_rxdescs[(x)])

/*
 * Software state for transmit and receive jobs.
 */
struct stge_descsoft {
	struct mbuf *ds_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t ds_dmamap;		/* our DMA map */
};

/*
 * Software state per device.
 */
struct stge_softc {
	struct device sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct arpcom sc_arpcom;	/* ethernet common data */
	void *sc_sdhook;		/* shutdown hook */
	int sc_rev;			/* silicon revision */

	void *sc_ih;			/* interrupt cookie */

	struct mii_data sc_mii;		/* MII/media information */

	struct timeout sc_timeout;	/* tick timeout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct stge_descsoft sc_txsoft[STGE_NTXDESC];
	struct stge_descsoft sc_rxsoft[STGE_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct stge_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->scd_txdescs
#define	sc_rxdescs	sc_control_data->scd_rxdescs

#ifdef STGE_EVENT_COUNTERS
	/*
	 * Event counters.
	 */
	struct evcnt sc_ev_txstall;	/* Tx stalled */
	struct evcnt sc_ev_txdmaintr;	/* Tx DMA interrupts */
	struct evcnt sc_ev_txindintr;	/* Tx Indicate interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */

	struct evcnt sc_ev_txseg1;	/* Tx packets w/ 1 segment */
	struct evcnt sc_ev_txseg2;	/* Tx packets w/ 2 segments */
	struct evcnt sc_ev_txseg3;	/* Tx packets w/ 3 segments */
	struct evcnt sc_ev_txseg4;	/* Tx packets w/ 4 segments */
	struct evcnt sc_ev_txseg5;	/* Tx packets w/ 5 segments */
	struct evcnt sc_ev_txsegmore;	/* Tx packets w/ more than 5 segments */
	struct evcnt sc_ev_txcopy;	/* Tx packets that we had to copy */

	struct evcnt sc_ev_rxipsum;	/* IP checksums checked in-bound */
	struct evcnt sc_ev_rxtcpsum;	/* TCP checksums checked in-bound */
	struct evcnt sc_ev_rxudpsum;	/* UDP checksums checked in-bound */

	struct evcnt sc_ev_txipsum;	/* IP checksums comp. out-bound */
	struct evcnt sc_ev_txtcpsum;	/* TCP checksums comp. out-bound */
	struct evcnt sc_ev_txudpsum;	/* UDP checksums comp. out-bound */
#endif /* STGE_EVENT_COUNTERS */

	int	sc_txpending;		/* number of Tx requests pending */
	int	sc_txdirty;		/* first dirty Tx descriptor */
	int	sc_txlast;		/* last used Tx descriptor */

	int	sc_rxptr;		/* next ready Rx descriptor/descsoft */
	int	sc_rxdiscard;
	int	sc_rxlen;
	struct mbuf *sc_rxhead;
	struct mbuf *sc_rxtail;
	struct mbuf **sc_rxtailp;

	int	sc_txthresh;		/* Tx threshold */
	int	sc_usefiber;		/* if we're fiber */
	uint32_t sc_DMACtrl;		/* prototype DMACtrl register */
	uint32_t sc_MACCtrl;		/* prototype MacCtrl register */
	uint16_t sc_IntEnable;		/* prototype IntEnable register */
	uint16_t sc_ReceiveMode;	/* prototype ReceiveMode register */
	uint8_t sc_PhyCtrl;		/* prototype PhyCtrl register */
};

#define	STGE_RXCHAIN_RESET(sc)						\
do {									\
	(sc)->sc_rxtailp = &(sc)->sc_rxhead;				\
	*(sc)->sc_rxtailp = NULL;					\
	(sc)->sc_rxlen = 0;						\
} while (/*CONSTCOND*/0)

#define	STGE_RXCHAIN_LINK(sc, m)					\
do {									\
	*(sc)->sc_rxtailp = (sc)->sc_rxtail = (m);			\
	(sc)->sc_rxtailp = &(m)->m_next;				\
} while (/*CONSTCOND*/0)

#ifdef STGE_EVENT_COUNTERS
#define	STGE_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define	STGE_EVCNT_INCR(ev)	/* nothing */
#endif

#define	STGE_CDTXADDR(sc, x)	((sc)->sc_cddma + STGE_CDTXOFF((x)))
#define	STGE_CDRXADDR(sc, x)	((sc)->sc_cddma + STGE_CDRXOFF((x)))

#define	STGE_CDTXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    STGE_CDTXOFF((x)), sizeof(struct stge_tfd), (ops))

#define	STGE_CDRXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    STGE_CDRXOFF((x)), sizeof(struct stge_rfd), (ops))

#define	STGE_INIT_RXDESC(sc, x)						\
do {									\
	struct stge_descsoft *__ds = &(sc)->sc_rxsoft[(x)];		\
	struct stge_rfd *__rfd = &(sc)->sc_rxdescs[(x)];		\
									\
	/*								\
	 * Note: We scoot the packet forward 2 bytes in the buffer	\
	 * so that the payload after the Ethernet header is aligned	\
	 * to a 4-byte boundary.					\
	 */								\
	__rfd->rfd_frag.frag_word0 =					\
	    htole64(FRAG_ADDR(__ds->ds_dmamap->dm_segs[0].ds_addr + 2) |\
	    FRAG_LEN(MCLBYTES - 2));					\
	__rfd->rfd_next =						\
	    htole64((uint64_t)STGE_CDRXADDR((sc), STGE_NEXTRX((x))));	\
	__rfd->rfd_status = 0;						\
	STGE_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
} while (/*CONSTCOND*/0)

#define STGE_TIMEOUT 1000

void	stge_start(struct ifnet *);
void	stge_watchdog(struct ifnet *);
int	stge_ioctl(struct ifnet *, u_long, caddr_t);
int	stge_init(struct ifnet *);
void	stge_stop(struct ifnet *, int);

void	stge_shutdown(void *);

void	stge_reset(struct stge_softc *);
void	stge_rxdrain(struct stge_softc *);
int	stge_add_rxbuf(struct stge_softc *, int);
#if 0
void	stge_read_eeprom(struct stge_softc *, int, uint16_t *);
#endif
void	stge_tick(void *);

void	stge_stats_update(struct stge_softc *);

void	stge_set_filter(struct stge_softc *);

int	stge_intr(void *);
void	stge_txintr(struct stge_softc *);
void	stge_rxintr(struct stge_softc *);

int	stge_mii_readreg(struct device *, int, int);
void	stge_mii_writereg(struct device *, int, int, int);
void	stge_mii_statchg(struct device *);

int	stge_mediachange(struct ifnet *);
void	stge_mediastatus(struct ifnet *, struct ifmediareq *);

int	stge_match(struct device *, void *, void *);
void	stge_attach(struct device *, struct device *, void *);

int	stge_copy_small = 0;

struct cfattach stge_ca = {
	sizeof(struct stge_softc), stge_match, stge_attach,
};

struct cfdriver stge_cd = {
	0, "stge", DV_IFNET
};

uint32_t stge_mii_bitbang_read(struct device *);
void	stge_mii_bitbang_write(struct device *, uint32_t);

const struct mii_bitbang_ops stge_mii_bitbang_ops = {
	stge_mii_bitbang_read,
	stge_mii_bitbang_write,
	{
		PC_MgmtData,		/* MII_BIT_MDO */
		PC_MgmtData,		/* MII_BIT_MDI */
		PC_MgmtClk,		/* MII_BIT_MDC */
		PC_MgmtDir,		/* MII_BIT_DIR_HOST_PHY */
		0,			/* MII_BIT_DIR_PHY_HOST */
	}
};

/*
 * Devices supported by this driver.
 */
const struct stge_product {
	pci_vendor_id_t		stge_vendor;
	pci_product_id_t	stge_product;
	const char		*stge_name;
} stge_products[] = {
	{ PCI_VENDOR_SUNDANCE,	PCI_PRODUCT_SUNDANCE_ST2021,
	  "Sundance ST-2021 Gigabit Ethernet" },

	{ PCI_VENDOR_TAMARACK,		PCI_PRODUCT_TAMARACK_TC9021,
	  "Tamarack TC9021 Gigabit Ethernet" },

	{ PCI_VENDOR_TAMARACK,		PCI_PRODUCT_TAMARACK_TC9021_ALT,
	  "Tamarack TC9021 Gigabit Ethernet" },

	/*
	 * The Sundance sample boards use the Sundance vendor ID,
	 * but the Tamarack product ID.
	 */
	{ PCI_VENDOR_SUNDANCE,	PCI_PRODUCT_TAMARACK_TC9021,
	  "Sundance TC9021 Gigabit Ethernet" },

	{ PCI_VENDOR_SUNDANCE,	PCI_PRODUCT_TAMARACK_TC9021_ALT,
	  "Sundance TC9021 Gigabit Ethernet" },

	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DGE550T,
	  "D-Link DGE-550T Gigabit Ethernet" },

	{ PCI_VENDOR_ANTARES,		PCI_PRODUCT_ANTARES_TC9021,
	  "Antares Gigabit Ethernet" },

	{ 0,				0,
	  NULL },
};

static const struct stge_product *
stge_lookup(const struct pci_attach_args *pa)
{
	const struct stge_product *sp;

	for (sp = stge_products; sp->stge_name != NULL; sp++) {
		if (PCI_VENDOR(pa->pa_id) == sp->stge_vendor &&
		    PCI_PRODUCT(pa->pa_id) == sp->stge_product)
			return (sp);
	}
	return (NULL);
}

int
stge_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (stge_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
stge_attach(struct device *parent, struct device *self, void *aux)
{
	struct stge_softc *sc = (struct stge_softc *) self;
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_dma_segment_t seg;
	int ioh_valid, memh_valid;
	int i, rseg, error;
	const struct stge_product *sp;
	pcireg_t pmode;
	int pmreg;

	timeout_set(&sc->sc_timeout, stge_tick, sc);

	sp = stge_lookup(pa);
	if (sp == NULL) {
		printf("\n");
		panic("stge_attach: impossible");
	}

	sc->sc_rev = PCI_REVISION(pa->pa_class);

	/*
	 * Map the device.
	 */
	ioh_valid = (pci_mapreg_map(pa, STGE_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL, 0) == 0);
	memh_valid = (pci_mapreg_map(pa, STGE_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, NULL, 0) == 0);

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
	} else {
		printf(": unable to map device registers\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/* Enable bus mastering. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/* Get it out of power save mode if needed. */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, 0)) {
		pmode = pci_conf_read(pc, pa->pa_tag, pmreg + PCI_PMCSR) & 0x3;
		if (pmode == 3) {
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			printf(": unable to wake up from power state D3\n");
			return;
		}
		if (pmode != 0) {
			printf(": waking up from power state D%d\n", pmode);
			pci_conf_write(pc, pa->pa_tag, pmreg + PCI_PMCSR, 0);
		}
	}

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		printf(": unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, stge_intr, sc,
				       sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s, rev. %d, %s\n", sp->stge_name, sc->sc_rev, intrstr);

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct stge_control_data), PAGE_SIZE, 0, &seg, 1, &rseg,
	    0)) != 0) {
		printf("%s: unable to allocate control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct stge_control_data), (caddr_t *)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct stge_control_data), 1,
	    sizeof(struct stge_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		printf("%s: unable to create control data DMA map, "
		    "error = %d\n", sc->sc_dev.dv_xname, error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct stge_control_data), NULL,
	    0)) != 0) {
		printf("%s: unable to load control data DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.  Note that rev B.3
	 * and earlier seem to have a bug regarding multi-fragment
	 * packets.  We need to limit the number of Tx segments on
	 * such chips to 1.
	 */
	for (i = 0; i < STGE_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    STGE_NTXFRAGS, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].ds_dmamap)) != 0) {
			printf("%s: unable to create tx DMA map %d, "
			    "error = %d\n", sc->sc_dev.dv_xname, i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < STGE_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].ds_dmamap)) != 0) {
			printf("%s: unable to create rx DMA map %d, "
			    "error = %d\n", sc->sc_dev.dv_xname, i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].ds_mbuf = NULL;
	}

	/*
	 * Determine if we're copper or fiber.  It affects how we
	 * reset the card.
	 */
	if (bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl) &
	    AC_PhyMedia)
		sc->sc_usefiber = 1;
	else
		sc->sc_usefiber = 0;

	/*
	 * Reset the chip to a known state.
	 */
	stge_reset(sc);

	/*
	 * Reading the station address from the EEPROM doesn't seem
	 * to work, at least on my sample boards.  Instread, since
	 * the reset sequence does AutoInit, read it from the station
	 * address registers.
	 */
	sc->sc_arpcom.ac_enaddr[0] = bus_space_read_2(sc->sc_st, sc->sc_sh,
	    STGE_StationAddress0) & 0xff;
	sc->sc_arpcom.ac_enaddr[1] = bus_space_read_2(sc->sc_st, sc->sc_sh,
	    STGE_StationAddress0) >> 8;
	sc->sc_arpcom.ac_enaddr[2] = bus_space_read_2(sc->sc_st, sc->sc_sh,
	    STGE_StationAddress1) & 0xff;
	sc->sc_arpcom.ac_enaddr[3] = bus_space_read_2(sc->sc_st, sc->sc_sh,
	    STGE_StationAddress1) >> 8;
	sc->sc_arpcom.ac_enaddr[4] = bus_space_read_2(sc->sc_st, sc->sc_sh,
	    STGE_StationAddress2) & 0xff;
	sc->sc_arpcom.ac_enaddr[5] = bus_space_read_2(sc->sc_st, sc->sc_sh,
	    STGE_StationAddress2) >> 8;

	printf("%s: address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/*
	 * Read some important bits from the PhyCtrl register.
	 */
	sc->sc_PhyCtrl = bus_space_read_1(sc->sc_st, sc->sc_sh,
	    STGE_PhyCtrl) & (PC_PhyDuplexPolarity | PC_PhyLnkPolarity);

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = stge_mii_readreg;
	sc->sc_mii.mii_writereg = stge_mii_writereg;
	sc->sc_mii.mii_statchg = stge_mii_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, stge_mediachange,
	    stge_mediastatus);
	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0 /* MIIF_DOPAUSE */);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	ifp = &sc->sc_arpcom.ac_if;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof ifp->if_xname);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = stge_ioctl;
	ifp->if_start = stge_start;
	ifp->if_watchdog = stge_watchdog;
#ifdef fake
	ifp->if_init = stge_init;
	ifp->if_stop = stge_stop;
#endif
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * The manual recommends disabling early transmit, so we
	 * do.  It's disabled anyway, if using IP checksumming,
	 * since the entire packet must be in the FIFO in order
	 * for the chip to perform the checksum.
	 */
	sc->sc_txthresh = 0x0fff;

	/*
	 * Disable MWI if the PCI layer tells us to.
	 */
	sc->sc_DMACtrl = 0;
#ifdef fake
	if ((pa->pa_flags & PCI_FLAGS_MWI_OKAY) == 0)
		sc->sc_DMACtrl |= DMAC_MWIDisable;
#endif

#ifdef fake
	/*
	 * We can support 802.1Q VLAN-sized frames and jumbo
	 * Ethernet frames.
	 *
	 * XXX Figure out how to do hw-assisted VLAN tagging in
	 * XXX a reasonable way on this chip.
	 */
	sc->sc_arpcom.ac_capabilities |=
	    ETHERCAP_VLAN_MTU /* XXX | ETHERCAP_JUMBO_MTU */;
#endif

#ifdef STGE_CHECKSUM
	/*
	 * We can do IPv4/TCPv4/UDPv4 checksums in hardware.
	 */
	sc->sc_arpcom.ac_if.if_capabilities |= IFCAP_CSUM_IPv4 |
	    IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
#endif

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#ifdef STGE_EVENT_COUNTERS
	/*
	 * Attach event counters.
	 */
	evcnt_attach_dynamic(&sc->sc_ev_txstall, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txstall");
	evcnt_attach_dynamic(&sc->sc_ev_txdmaintr, EVCNT_TYPE_INTR,
	    NULL, sc->sc_dev.dv_xname, "txdmaintr");
	evcnt_attach_dynamic(&sc->sc_ev_txindintr, EVCNT_TYPE_INTR,
	    NULL, sc->sc_dev.dv_xname, "txindintr");
	evcnt_attach_dynamic(&sc->sc_ev_rxintr, EVCNT_TYPE_INTR,        
	    NULL, sc->sc_dev.dv_xname, "rxintr");

	evcnt_attach_dynamic(&sc->sc_ev_txseg1, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "txseg1");                      
	evcnt_attach_dynamic(&sc->sc_ev_txseg2, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "txseg2");                      
	evcnt_attach_dynamic(&sc->sc_ev_txseg3, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "txseg3");                      
	evcnt_attach_dynamic(&sc->sc_ev_txseg4, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "txseg4");                      
	evcnt_attach_dynamic(&sc->sc_ev_txseg5, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "txseg5");                      
	evcnt_attach_dynamic(&sc->sc_ev_txsegmore, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "txsegmore");                      
	evcnt_attach_dynamic(&sc->sc_ev_txcopy, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "txcopy");                      

	evcnt_attach_dynamic(&sc->sc_ev_rxipsum, EVCNT_TYPE_MISC,       
	    NULL, sc->sc_dev.dv_xname, "rxipsum");                      
	evcnt_attach_dynamic(&sc->sc_ev_rxtcpsum, EVCNT_TYPE_MISC,      
	    NULL, sc->sc_dev.dv_xname, "rxtcpsum");
	evcnt_attach_dynamic(&sc->sc_ev_rxudpsum, EVCNT_TYPE_MISC,      
	    NULL, sc->sc_dev.dv_xname, "rxudpsum");
	evcnt_attach_dynamic(&sc->sc_ev_txipsum, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txipsum");
	evcnt_attach_dynamic(&sc->sc_ev_txtcpsum, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txtcpsum");
	evcnt_attach_dynamic(&sc->sc_ev_txudpsum, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txudpsum");
#endif /* STGE_EVENT_COUNTERS */

	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	sc->sc_sdhook = shutdownhook_establish(stge_shutdown, sc);
	if (sc->sc_sdhook == NULL)
		printf("%s: WARNING: unable to establish shutdown hook\n",
		    sc->sc_dev.dv_xname);
	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < STGE_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].ds_dmamap);
	}
 fail_4:
	for (i = 0; i < STGE_NTXDESC; i++) {
		if (sc->sc_txsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].ds_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_control_data,
	    sizeof(struct stge_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * stge_shutdown:
 *
 *	Make sure the interface is stopped at reboot time.
 */
void
stge_shutdown(void *arg)
{
	struct stge_softc *sc = arg;

	stge_stop(&sc->sc_arpcom.ac_if, 1);
}

static void
stge_dma_wait(struct stge_softc *sc)
{
	int i;

	for (i = 0; i < STGE_TIMEOUT; i++) {
		delay(2);
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_DMACtrl) &
		     DMAC_TxDMAInProg) == 0)
			break;
	}

	if (i == STGE_TIMEOUT)
		printf("%s: DMA wait timed out\n", sc->sc_dev.dv_xname);
}

/*
 * stge_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
void
stge_start(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct stge_descsoft *ds;
	struct stge_tfd *tfd;
	bus_dmamap_t dmamap;
	int error, firsttx, nexttx, opending, seg, totlen;
	uint64_t csum_flags = 0;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of pending transmissions
	 * and the first descriptor we will use.
	 */
	opending = sc->sc_txpending;
	firsttx = STGE_NEXTTX(sc->sc_txlast);

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		/*
		 * Leave one unused descriptor at the end of the
		 * list to prevent wrapping completely around.
		 */
		if (sc->sc_txpending == (STGE_NTXDESC - 1)) {
			STGE_EVCNT_INCR(&sc->sc_ev_txstall);
			break;
		}

		/*
		 * Get the last and next available transmit descriptor.
		 */
		nexttx = STGE_NEXTTX(sc->sc_txlast);
		tfd = &sc->sc_txdescs[nexttx];
		ds = &sc->sc_txsoft[nexttx];

		dmamap = ds->ds_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources.  For the too-may-segments
		 * case, we simply report an error and drop the packet,
		 * since we can't sanely copy a jumbo packet to a single
		 * buffer.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_NOWAIT);
		if (error) {
			if (error == EFBIG) {
				printf("%s: Tx packet consumes too many "
				    "DMA segments, dropping...\n",
				    sc->sc_dev.dv_xname);
				IFQ_DEQUEUE(&ifp->if_snd, m0);
				m_freem(m0);
				continue;
			}
			/*
			 * Short on resources, just stop for now.
			 */
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Initialize the fragment list. */
		for (totlen = 0, seg = 0; seg < dmamap->dm_nsegs; seg++) {
			tfd->tfd_frags[seg].frag_word0 =
			    htole64(FRAG_ADDR(dmamap->dm_segs[seg].ds_addr) |
			    FRAG_LEN(dmamap->dm_segs[seg].ds_len));
			totlen += dmamap->dm_segs[seg].ds_len;
		}

#ifdef STGE_EVENT_COUNTERS
		switch (dmamap->dm_nsegs) {
		case 1:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg1);
			break;
		case 2:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg2);
			break;
		case 3:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg3);
			break;
		case 4:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg4);
			break;
		case 5:
			STGE_EVCNT_INCR(&sc->sc_ev_txseg5);
			break;
		default:
			STGE_EVCNT_INCR(&sc->sc_ev_txsegmore);
			break;
		}
#endif /* STGE_EVENT_COUNTERS */

#ifdef STGE_CHECKSUM
		/*
		 * Initialize checksumming flags in the descriptor.
		 * Byte-swap constants so the compiler can optimize.
		 */
		if (m0->m_pkthdr.csum & M_IPV4_CSUM_OUT) {
			STGE_EVCNT_INCR(&sc->sc_ev_txipsum);
			csum_flags |= htole64(TFD_IPChecksumEnable);
		}

		if (m0->m_pkthdr.csum & M_TCPV4_CSUM_OUT) {
			STGE_EVCNT_INCR(&sc->sc_ev_txtcpsum);
			csum_flags |= htole64(TFD_TCPChecksumEnable);
		}
		else if (m0->m_pkthdr.csum & M_UDPV4_CSUM_OUT) {
			STGE_EVCNT_INCR(&sc->sc_ev_txudpsum);
			csum_flags |= htole64(TFD_UDPChecksumEnable);
		}
#endif

		/*
		 * Initialize the descriptor and give it to the chip.
		 */
		tfd->tfd_control = htole64(TFD_FrameId(nexttx) |
		    TFD_WordAlign(/*totlen & */3) |
		    TFD_FragCount(seg) | csum_flags |
		    (((nexttx & STGE_TXINTR_SPACING_MASK) == 0) ?
		     TFD_TxDMAIndicate : 0));

		/* Sync the descriptor. */
		STGE_CDTXSYNC(sc, nexttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Kick the transmit DMA logic.
		 */
		bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_DMACtrl,
		    sc->sc_DMACtrl | DMAC_TxDMAPollNow);

		/*
		 * Store a pointer to the packet so we can free it later.
		 */
		ds->ds_mbuf = m0;

		/* Advance the tx pointer. */
		sc->sc_txpending++;
		sc->sc_txlast = nexttx;

#if NBPFILTER > 0
		/*
		 * Pass the packet to any BPF listeners.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif /* NBPFILTER > 0 */
	}

	if (sc->sc_txpending == (STGE_NTXDESC - 1)) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txpending != opending) {
		/*
		 * We enqueued packets.  If the transmitter was idle,
		 * reset the txdirty pointer.
		 */
		if (opending == 0)
			sc->sc_txdirty = firsttx;

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * stge_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
void
stge_watchdog(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;

	/*
	 * Sweep up first, since we don't interrupt every frame.
	 */
	stge_txintr(sc);
	if (sc->sc_txpending != 0) {
		printf("%s: device timeout\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;

		(void) stge_init(ifp);

		/* Try to get more packets going. */
		stge_start(ifp);
	}
}

/*
 * stge_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
int
stge_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct stge_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			stge_init(ifp);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			 register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			 if (ns_nullhost(*ina))
				ina->x_host = (union ns_host *)
				  &sc->sc_arpcom.ac_enaddr;
			 else
				bcopy(ina->x_host.c_host,
				      &sc->sc_arpcom.ac_enaddr,
				      ifp->if_addrlen);
			 /* Set new address. */
			 stge_init(ifp);
			 break;
		    }
#endif
		default:
			stge_init(ifp);
			break;
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU || ifr->ifr_mtu < ETHERMIN) {
			error = EINVAL;
		} else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;

	case SIOCSIFFLAGS:
		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * XXX If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP)
			stge_init(ifp);
		else if (ifp->if_flags & IFF_RUNNING)
			stge_stop(ifp, 1);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			stge_init(ifp);
			error = 0;
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = EINVAL;
	}

	splx(s);
	return (error);
}

/*
 * stge_intr:
 *
 *	Interrupt service routine.
 */
int
stge_intr(void *arg)
{
	struct stge_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	uint32_t txstat;
	int wantinit;
	uint16_t isr;

	if ((bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_IntStatus) &
	     IS_InterruptStatus) == 0)
		return (0);

	for (wantinit = 0; wantinit == 0;) {
		isr = bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_IntStatusAck);
		if ((isr & sc->sc_IntEnable) == 0)
			break;
		
		/* Receive interrupts. */
		if (isr & (IE_RxDMAComplete|IE_RFDListEnd)) {
			STGE_EVCNT_INCR(&sc->sc_ev_rxintr);
			stge_rxintr(sc);
			if (isr & IE_RFDListEnd) {
				printf("%s: receive ring overflow\n",
				    sc->sc_dev.dv_xname);
				/*
				 * XXX Should try to recover from this
				 * XXX more gracefully.
				 */
				wantinit = 1;
			}
		}

		/* Transmit interrupts. */
		if (isr & (IE_TxDMAComplete|IE_TxComplete)) {
#ifdef STGE_EVENT_COUNTERS
			if (isr & IE_TxDMAComplete)
				STGE_EVCNT_INCR(&sc->sc_ev_txdmaintr);
#endif
			stge_txintr(sc);
		}

		/* Statistics overflow. */
		if (isr & IE_UpdateStats)
			stge_stats_update(sc);

		/* Transmission errors. */
		if (isr & IE_TxComplete) {
			STGE_EVCNT_INCR(&sc->sc_ev_txindintr);
			for (;;) {
				txstat = bus_space_read_4(sc->sc_st, sc->sc_sh,
				    STGE_TxStatus);
				if ((txstat & TS_TxComplete) == 0)
					break;
				if (txstat & TS_TxUnderrun) {
					sc->sc_txthresh++;
					if (sc->sc_txthresh > 0x0fff)
						sc->sc_txthresh = 0x0fff;
					printf("%s: transmit underrun, new "
					    "threshold: %d bytes\n",
					    sc->sc_dev.dv_xname,
					    sc->sc_txthresh << 5);
				}
				if (txstat & TS_MaxCollisions)
					printf("%s: excessive collisions\n",
					    sc->sc_dev.dv_xname);
			}
			wantinit = 1;
		}

		/* Host interface errors. */
		if (isr & IE_HostError) {
			printf("%s: Host interface error\n",
			    sc->sc_dev.dv_xname);
			wantinit = 1;
		}
	}

	if (wantinit)
		stge_init(ifp);

	bus_space_write_2(sc->sc_st, sc->sc_sh, STGE_IntEnable,
	    sc->sc_IntEnable);

	/* Try to get more packets going. */
	stge_start(ifp);

	return (1);
}

/*
 * stge_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
void
stge_txintr(struct stge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct stge_descsoft *ds;
	uint64_t control;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (i = sc->sc_txdirty; sc->sc_txpending != 0;
	     i = STGE_NEXTTX(i), sc->sc_txpending--) {
		ds = &sc->sc_txsoft[i];

		STGE_CDTXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		control = letoh64(sc->sc_txdescs[i].tfd_control);
		if ((control & TFD_TFDDone) == 0)
			break;

		bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap,
		    0, ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
		m_freem(ds->ds_mbuf);
		ds->ds_mbuf = NULL;
	}

	/* Update the dirty transmit buffer pointer. */
	sc->sc_txdirty = i;

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (sc->sc_txpending == 0)
		ifp->if_timer = 0;
}

/*
 * stge_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
void
stge_rxintr(struct stge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct stge_descsoft *ds;
	struct mbuf *m, *tailm;
	uint64_t status;
	int i, len;

	for (i = sc->sc_rxptr;; i = STGE_NEXTRX(i)) {
		ds = &sc->sc_rxsoft[i];

		STGE_CDRXSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status = letoh64(sc->sc_rxdescs[i].rfd_status);

		if ((status & RFD_RFDDone) == 0)
			break;

		if (__predict_false(sc->sc_rxdiscard)) {
			STGE_INIT_RXDESC(sc, i);
			if (status & RFD_FrameEnd) {
				/* Reset our state. */
				sc->sc_rxdiscard = 0;
			}
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = ds->ds_mbuf;

		/*
		 * Add a new receive buffer to the ring.
		 */
		if (stge_add_rxbuf(sc, i) != 0) {
			/*
			 * Failed, throw away what we've done so
			 * far, and discard the rest of the packet.
			 */
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
			    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			STGE_INIT_RXDESC(sc, i);
			if ((status & RFD_FrameEnd) == 0)
				sc->sc_rxdiscard = 1;
			if (sc->sc_rxhead != NULL)
				m_freem(sc->sc_rxhead);
			STGE_RXCHAIN_RESET(sc);
			continue;
		}

#ifdef DIAGNOSTIC
		if (status & RFD_FrameStart) {
			KASSERT(sc->sc_rxhead == NULL);
			KASSERT(sc->sc_rxtailp == &sc->sc_rxhead);
		}
#endif

		STGE_RXCHAIN_LINK(sc, m);

		/*
		 * If this is not the end of the packet, keep
		 * looking.
		 */
		if ((status & RFD_FrameEnd) == 0) {
			sc->sc_rxlen += m->m_len;
			continue;
		}

		/*
		 * Okay, we have the entire packet now...
		 */
		*sc->sc_rxtailp = NULL;
		m = sc->sc_rxhead;
		tailm = sc->sc_rxtail;

		STGE_RXCHAIN_RESET(sc);

		/*
		 * If the packet had an error, drop it.  Note we
		 * count the error later in the periodic stats update.
		 */
		if (status & (RFD_RxFIFOOverrun | RFD_RxRuntFrame |
			      RFD_RxAlignmentError | RFD_RxFCSError |
			      RFD_RxLengthError)) {
			m_freem(m);
			continue;
		}

		/*
		 * No errors.
		 *
		 * Note we have configured the chip to not include
		 * the CRC at the end of the packet.
		 */
		len = RFD_RxDMAFrameLen(status);
		tailm->m_len = len - sc->sc_rxlen;

		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when we receive lots
		 * of small packets.
		 */
		if (stge_copy_small != 0 && len <= (MHLEN - 2)) {
			struct mbuf *nm;
			MGETHDR(nm, M_DONTWAIT, MT_DATA);
			if (nm == NULL) {
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			}
			nm->m_data += 2;
			nm->m_pkthdr.len = nm->m_len = len;
			m_copydata(m, 0, len, mtod(nm, caddr_t));
			m_freem(m);
			m = nm;
		}

#ifdef STGE_CHECKSUM
		/*
		 * Set the incoming checksum information for the packet.
		 */
		if (status & RFD_IPDetected) {
			STGE_EVCNT_INCR(&sc->sc_ev_rxipsum);
			m->m_pkthdr.csum |= (status & RFD_IPError) ?
			  M_IPV4_CSUM_IN_BAD : M_IPV4_CSUM_IN_OK;
			if (status & RFD_TCPDetected) {
				STGE_EVCNT_INCR(&sc->sc_ev_rxtcpsum);
				m->m_pkthdr.csum |= (status & RFD_TCPError) ?
				  M_TCP_CSUM_IN_BAD : M_TCP_CSUM_IN_OK;
			} else if (status & RFD_UDPDetected) {
				STGE_EVCNT_INCR(&sc->sc_ev_rxudpsum);
				m->m_pkthdr.csum |= (status & RFD_UDPError) ?
				  M_UDP_CSUM_IN_BAD : M_UDP_CSUM_IN_OK;
			}
		}
#endif

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;

#if NBPFILTER > 0
		/*
		 * Pass this up to any BPF listeners, but only
		 * pass if up the stack if it's for us.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif /* NBPFILTER > 0 */

		/* Pass it on. */
		ether_input_mbuf(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * stge_tick:
 *
 *	One second timer, used to tick the MII.
 */
void
stge_tick(void *arg)
{
	struct stge_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	stge_stats_update(sc);
	splx(s);

	timeout_add(&sc->sc_timeout, hz);
}

/*
 * stge_stats_update:
 *
 *	Read the TC9021 statistics counters.
 */
void
stge_stats_update(struct stge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;

	(void) bus_space_read_4(st, sh, STGE_OctetRcvOk);

	ifp->if_ipackets +=
	    bus_space_read_4(st, sh, STGE_FramesRcvdOk);

	ifp->if_ierrors +=
	    (u_int) bus_space_read_2(st, sh, STGE_FramesLostRxErrors);

	(void) bus_space_read_4(st, sh, STGE_OctetXmtdOk);

	ifp->if_opackets +=
	    bus_space_read_4(st, sh, STGE_FramesXmtdOk);

	ifp->if_collisions +=
	    bus_space_read_4(st, sh, STGE_LateCollisions) +
	    bus_space_read_4(st, sh, STGE_MultiColFrames) +
	    bus_space_read_4(st, sh, STGE_SingleColFrames);

	ifp->if_oerrors +=
	    (u_int) bus_space_read_2(st, sh, STGE_FramesAbortXSColls) +
	    (u_int) bus_space_read_2(st, sh, STGE_FramesWEXDeferal);
}

/*
 * stge_reset:
 *
 *	Perform a soft reset on the TC9021.
 */
void
stge_reset(struct stge_softc *sc)
{
	uint32_t ac;
	int i;

	ac = bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl);

	/*
	 * Only assert RstOut if we're fiber.  We need GMII clocks
	 * to be present in order for the reset to complete on fiber
	 * cards.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl,
	    ac | AC_GlobalReset | AC_RxReset | AC_TxReset |
	    AC_DMA | AC_FIFO | AC_Network | AC_Host | AC_AutoInit |
	    (sc->sc_usefiber ? AC_RstOut : 0));

	delay(50000);

	for (i = 0; i < STGE_TIMEOUT; i++) {
		delay(5000);
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, STGE_AsicCtrl) &
		     AC_ResetBusy) == 0)
			break;
	}

	if (i == STGE_TIMEOUT)
		printf("%s: reset failed to complete\n", sc->sc_dev.dv_xname);

	delay(1000);
}

/*
 * stge_init:		[ ifnet interface function ]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
int
stge_init(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct stge_descsoft *ds;
	int i, error = 0;

	/*
	 * Cancel any pending I/O.
	 */
	stge_stop(ifp, 0);

	/*
	 * Reset the chip to a known state.
	 */
	stge_reset(sc);

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0; i < STGE_NTXDESC; i++) {
		sc->sc_txdescs[i].tfd_next =
		    (uint64_t) STGE_CDTXADDR(sc, STGE_NEXTTX(i));
		sc->sc_txdescs[i].tfd_control = htole64(TFD_TFDDone);
	}
	sc->sc_txpending = 0;
	sc->sc_txdirty = 0;
	sc->sc_txlast = STGE_NTXDESC - 1;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < STGE_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf == NULL) {
			if ((error = stge_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    sc->sc_dev.dv_xname, i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				stge_rxdrain(sc);
				goto out;
			}
		} else
			STGE_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;
	sc->sc_rxdiscard = 0;
	STGE_RXCHAIN_RESET(sc);

	/* Set the station address. */
	bus_space_write_2(st, sh, STGE_StationAddress0,
	    sc->sc_arpcom.ac_enaddr[0] | sc->sc_arpcom.ac_enaddr[1] << 8);
	bus_space_write_2(st, sh, STGE_StationAddress1,
	    sc->sc_arpcom.ac_enaddr[2] | sc->sc_arpcom.ac_enaddr[3] << 8);
	bus_space_write_2(st, sh, STGE_StationAddress2,
	    sc->sc_arpcom.ac_enaddr[4] | sc->sc_arpcom.ac_enaddr[5] << 8);

	/*
	 * Set the statistics masks.  Disable all the RMON stats,
	 * and disable selected stats in the non-RMON stats registers.
	 */
	bus_space_write_4(st, sh, STGE_RMONStatisticsMask, 0xffffffff);
	bus_space_write_4(st, sh, STGE_StatisticsMask,
	    (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5) |
	    (1U << 6) | (1U << 7) | (1U << 8) | (1U << 9) | (1U << 10) |
	    (1U << 13) | (1U << 14) | (1U << 15) | (1U << 19) | (1U << 20) |
	    (1U << 21));

	/* Set up the receive filter. */
	stge_set_filter(sc);

	/*
	 * Give the transmit and receive ring to the chip.
	 */
	bus_space_write_4(st, sh, STGE_TFDListPtrHi, 0); /* NOTE: 32-bit DMA */
	bus_space_write_4(st, sh, STGE_TFDListPtrLo,
	    STGE_CDTXADDR(sc, sc->sc_txdirty));

	bus_space_write_4(st, sh, STGE_RFDListPtrHi, 0); /* NOTE: 32-bit DMA */
	bus_space_write_4(st, sh, STGE_RFDListPtrLo,
	    STGE_CDRXADDR(sc, sc->sc_rxptr));

	/*
	 * Initialize the Tx auto-poll period.  It's OK to make this number
	 * large (255 is the max, but we use 127) -- we explicitly kick the
	 * transmit engine when there's actually a packet.
	 */
	bus_space_write_1(st, sh, STGE_TxDMAPollPeriod, 127);

	/* ..and the Rx auto-poll period. */
	bus_space_write_1(st, sh, STGE_RxDMAPollPeriod, 64);

	/* Initialize the Tx start threshold. */
	bus_space_write_2(st, sh, STGE_TxStartThresh, sc->sc_txthresh);

	/*
	 * Initialize the Rx DMA interrupt control register.  We
	 * request an interrupt after every incoming packet, but
	 * defer it for 32us (64 * 512 ns).
	 */
	bus_space_write_4(st, sh, STGE_RxDMAIntCtrl,
	    RDIC_RxFrameCount(1) | RDIC_RxDMAWaitTime(512));

	/*
	 * Initialize the interrupt mask.
	 */
	sc->sc_IntEnable = IE_HostError | IE_TxComplete | IE_UpdateStats |
	    IE_TxDMAComplete | IE_RxDMAComplete | IE_RFDListEnd;
	bus_space_write_2(st, sh, STGE_IntStatus, 0xffff);
	bus_space_write_2(st, sh, STGE_IntEnable, sc->sc_IntEnable);

	/*
	 * Configure the DMA engine.
	 * XXX Should auto-tune TxBurstLimit.
	 */
	bus_space_write_4(st, sh, STGE_DMACtrl, sc->sc_DMACtrl |
	    DMAC_TxBurstLimit(3));

	/*
	 * Send a PAUSE frame when we reach 29,696 bytes in the Rx
	 * FIFO, and send an un-PAUSE frame when the FIFO is totally
	 * empty again.
	 */
	bus_space_write_4(st, sh, STGE_FlowOnTresh, 29696 / 16);
	bus_space_write_4(st, sh, STGE_FlowOffThresh, 0);

	/*
	 * Set the maximum frame size.
	 */
#ifdef fake
	bus_space_write_2(st, sh, STGE_MaxFrameSize,
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN +
	    ((sc->sc_arpcom.ac_capenable & ETHERCAP_VLAN_MTU) ?
	     ETHER_VLAN_ENCAP_LEN : 0));
#else
#if NVLAN > 0
	bus_space_write_2(st, sh, STGE_MaxFrameSize,
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN + 4);
#else
	bus_space_write_2(st, sh, STGE_MaxFrameSize,
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN);
#endif
#endif
	/*
	 * Initialize MacCtrl -- do it before setting the media,
	 * as setting the media will actually program the register.
	 *
	 * Note: We have to poke the IFS value before poking
	 * anything else.
	 */
	sc->sc_MACCtrl = MC_IFSSelect(0);
	bus_space_write_4(st, sh, STGE_MACCtrl, sc->sc_MACCtrl);
	sc->sc_MACCtrl |= MC_StatisticsEnable | MC_TxEnable | MC_RxEnable;

	if (sc->sc_rev >= 6) {		/* >= B.2 */
		/* Multi-frag frame bug work-around. */
		bus_space_write_2(st, sh, STGE_DebugCtrl,
		    bus_space_read_2(st, sh, STGE_DebugCtrl) | 0x0200);

		/* Tx Poll Now bug work-around. */
		bus_space_write_2(st, sh, STGE_DebugCtrl,
		    bus_space_read_2(st, sh, STGE_DebugCtrl) | 0x0010);
	}

	/*
	 * Set the current media.
	 */
	mii_mediachg(&sc->sc_mii);

	/*
	 * Start the one second MII clock.
	 */
	timeout_add(&sc->sc_timeout, hz);

	/*
	 * ...all done!
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

 out:
	if (error)
		printf("%s: interface not running\n", sc->sc_dev.dv_xname);
	return (error);
}

/*
 * stge_drain:
 *
 *	Drain the receive queue.
 */
void
stge_rxdrain(struct stge_softc *sc)
{
	struct stge_descsoft *ds;
	int i;

	for (i = 0; i < STGE_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
			ds->ds_mbuf->m_next = NULL;
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}
}

/*
 * stge_stop:		[ ifnet interface function ]
 *
 *	Stop transmission on the interface.
 */
void
stge_stop(struct ifnet *ifp, int disable)
{
	struct stge_softc *sc = ifp->if_softc;
	struct stge_descsoft *ds;
	int i;

	/*
	 * Stop the one second clock.
	 */
	timeout_del(&sc->sc_timeout);

	/* Down the MII. */
	mii_down(&sc->sc_mii);

	/*
	 * Disable interrupts.
	 */
	bus_space_write_2(sc->sc_st, sc->sc_sh, STGE_IntEnable, 0);

	/*
	 * Stop receiver, transmitter, and stats update.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_MACCtrl,
	    MC_StatisticsDisable | MC_TxDisable | MC_RxDisable);

	/*
	 * Stop the transmit and receive DMA.
	 */
	stge_dma_wait(sc);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_TFDListPtrHi, 0);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_TFDListPtrLo, 0);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_RFDListPtrHi, 0);
	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_RFDListPtrLo, 0);

	/*
	 * Release any queued transmit buffers.
	 */
	for (i = 0; i < STGE_NTXDESC; i++) {
		ds = &sc->sc_txsoft[i];
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}

	if (disable)
		stge_rxdrain(sc);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

#if 0
static int
stge_eeprom_wait(struct stge_softc *sc)
{
	int i;

	for (i = 0; i < STGE_TIMEOUT; i++) {
		delay(1000);
		if ((bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_EepromCtrl) &
		     EC_EepromBusy) == 0)
			return (0);
	}
	return (1);
}

/*
 * stge_read_eeprom:
 *
 *	Read data from the serial EEPROM.
 */
void
stge_read_eeprom(struct stge_softc *sc, int offset, uint16_t *data)
{

	if (stge_eeprom_wait(sc))
		printf("%s: EEPROM failed to come ready\n",
		    sc->sc_dev.dv_xname);

	bus_space_write_2(sc->sc_st, sc->sc_sh, STGE_EepromCtrl,
	    EC_EepromAddress(offset) | EC_EepromOpcode(EC_OP_RR));
	if (stge_eeprom_wait(sc))
		printf("%s: EEPROM read timed out\n",
		    sc->sc_dev.dv_xname);
	*data = bus_space_read_2(sc->sc_st, sc->sc_sh, STGE_EepromData);
}
#endif /* 0 */

/*
 * stge_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
int
stge_add_rxbuf(struct stge_softc *sc, int idx)
{
	struct stge_descsoft *ds = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)  
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	m->m_data = m->m_ext.ext_buf + 2;
	m->m_len = MCLBYTES - 2;

	if (ds->ds_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);

	ds->ds_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, ds->ds_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    sc->sc_dev.dv_xname, idx, error);
		panic("stge_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
	    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	STGE_INIT_RXDESC(sc, idx);

	return (0);
}

/*
 * stge_set_filter:
 *
 *	Set up the receive filter.
 */
void
stge_set_filter(struct stge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
        int h = 0;
        u_int32_t mchash[2] = { 0, 0 };

	sc->sc_ReceiveMode = RM_ReceiveUnicast;
	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_ReceiveMode |= RM_ReceiveBroadcast;

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_ReceiveMode |= RM_ReceiveAllFrames;
		goto allmulti;
	}

	/*
	 * Set up the multicast address filter by passing all multicast
	 * addresses through a CRC generator, and then using the low-order
	 * 6 bits as an index into the 64 bit multicast hash table.  The
	 * high order bits select the register, while the rest of the bits
	 * select the bit within the register.
	 */

	ETHER_FIRST_MULTI(step, &sc->sc_arpcom, enm);
	if (enm == NULL)
		goto done;

	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN))
			goto allmulti;
		h = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) & 
		    0x0000003F;
                if (h < 32)
                        mchash[0] |= (1 << h);
                else
                        mchash[1] |= (1 << (h - 32));
                ETHER_NEXT_MULTI(step, enm);
	}

	sc->sc_ReceiveMode |= RM_ReceiveMulticastHash;

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto done;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_ReceiveMode |= RM_ReceiveMulticast;

 done:
	if ((ifp->if_flags & IFF_ALLMULTI) == 0) {
		/*
		 * Program the multicast hash table.
		 */
		bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_HashTable0,
		    mchash[0]);
		bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_HashTable1,
		    mchash[1]);
	}

	bus_space_write_1(sc->sc_st, sc->sc_sh, STGE_ReceiveMode,
	    sc->sc_ReceiveMode);
}

/*
 * stge_mii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the MII of the TC9021.
 */
int
stge_mii_readreg(struct device *self, int phy, int reg)
{

	return (mii_bitbang_readreg(self, &stge_mii_bitbang_ops, phy, reg));
}

/*
 * stge_mii_writereg:	[mii interface function]
 *
 *	Write a PHY register on the MII of the TC9021.
 */
void
stge_mii_writereg(struct device *self, int phy, int reg, int val)
{

	mii_bitbang_writereg(self, &stge_mii_bitbang_ops, phy, reg, val);
}

/*
 * stge_mii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
void
stge_mii_statchg(struct device *self)
{
	struct stge_softc *sc = (struct stge_softc *) self;

	if (sc->sc_mii.mii_media_active & IFM_FDX)
		sc->sc_MACCtrl |= MC_DuplexSelect;
	else
		sc->sc_MACCtrl &= ~MC_DuplexSelect;

	/* XXX 802.1x flow-control? */

	bus_space_write_4(sc->sc_st, sc->sc_sh, STGE_MACCtrl, sc->sc_MACCtrl);
}

/*
 * sste_mii_bitbang_read: [mii bit-bang interface function]
 *
 *	Read the MII serial port for the MII bit-bang module.
 */
uint32_t
stge_mii_bitbang_read(struct device *self)
{
	struct stge_softc *sc = (void *) self;

	return (bus_space_read_1(sc->sc_st, sc->sc_sh, STGE_PhyCtrl));
}

/*
 * stge_mii_bitbang_write: [mii big-bang interface function]
 *
 *	Write the MII serial port for the MII bit-bang module.
 */
void
stge_mii_bitbang_write(struct device *self, uint32_t val)
{
	struct stge_softc *sc = (void *) self;

	bus_space_write_1(sc->sc_st, sc->sc_sh, STGE_PhyCtrl,
	    val | sc->sc_PhyCtrl);
}

/*
 * stge_mediastatus:	[ifmedia interface function]
 *
 *	Get the current interface media status.
 */
void
stge_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct stge_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

/*
 * stge_mediachange:	[ifmedia interface function]
 *
 *	Set hardware to newly-selected media.
 */
int
stge_mediachange(struct ifnet *ifp)
{
	struct stge_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		mii_mediachg(&sc->sc_mii);
	return (0);
}
