/* $OpenBSD: if_cpsw.c,v 1.8 2013/09/12 00:19:11 dlg Exp $ */
/*	$NetBSD: if_cpsw.c,v 1.3 2013/04/17 14:36:34 bouyer Exp $	*/

/*
 * Copyright (c) 2013 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <arch/armv7/omap/omapvar.h>
#include <arch/armv7/omap/sitara_cm.h>
#include <arch/armv7/omap/if_cpswreg.h>

#define CPSW_TXFRAGS	16

#define OMAP2SCM_MAC_ID0_LO	0x630
#define OMAP2SCM_MAC_ID0_HI	0x634

#define CPSW_CPPI_RAM_SIZE (0x2000)
#define CPSW_CPPI_RAM_TXDESCS_SIZE (CPSW_CPPI_RAM_SIZE/2)
#define CPSW_CPPI_RAM_RXDESCS_SIZE \
    (CPSW_CPPI_RAM_SIZE - CPSW_CPPI_RAM_TXDESCS_SIZE)
#define CPSW_CPPI_RAM_TXDESCS_BASE (CPSW_CPPI_RAM_OFFSET + 0x0000)
#define CPSW_CPPI_RAM_RXDESCS_BASE \
    (CPSW_CPPI_RAM_OFFSET + CPSW_CPPI_RAM_TXDESCS_SIZE)

#define CPSW_NTXDESCS (CPSW_CPPI_RAM_TXDESCS_SIZE/sizeof(struct cpsw_cpdma_bd))
#define CPSW_NRXDESCS (CPSW_CPPI_RAM_RXDESCS_SIZE/sizeof(struct cpsw_cpdma_bd))

#define CPSW_PAD_LEN (ETHER_MIN_LEN - ETHER_CRC_LEN)

#define TXDESC_NEXT(x) cpsw_txdesc_adjust((x), 1)
#define TXDESC_PREV(x) cpsw_txdesc_adjust((x), -1)

#define RXDESC_NEXT(x) cpsw_rxdesc_adjust((x), 1)
#define RXDESC_PREV(x) cpsw_rxdesc_adjust((x), -1)

/* __BIT(n): nth bit, where __BIT(0) == 0x1. */
#define __BIT(__n) \
    (((uint32_t)(__n) >= NBBY * sizeof(uint32_t)) ? 0 : ((uint32_t)1 << (uint32_t)(__n)))

/* __BITS(m, n): bits m through n, m < n. */
#define __BITS(__m, __n) \
    ((__BIT(MAX((__m), (__n)) + 1) - 1) ^ (__BIT(MIN((__m), (__n))) - 1))

/* find least significant bit that is set */
#define __LOWEST_SET_BIT(__mask) ((((__mask) - 1) & (__mask)) ^ (__mask))

#define __PRIuBIT	PRIuMAX
#define __PRIuBITS	__PRIuBIT

#define __PRIxBIT	PRIxMAX
#define __PRIxBITS	__PRIxBIT

#define __SHIFTOUT(__x, __mask) (((__x) & (__mask)) / __LOWEST_SET_BIT(__mask))

struct cpsw_ring_data {
	bus_dmamap_t tx_dm[CPSW_NTXDESCS];
	struct mbuf *tx_mb[CPSW_NTXDESCS];
	bus_dmamap_t rx_dm[CPSW_NRXDESCS];
	struct mbuf *rx_mb[CPSW_NRXDESCS];
};

struct cpsw_softc {
	struct device  sc_dev;
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	bus_dma_tag_t sc_bdt;
	bus_space_handle_t sc_bsh_txdescs;
	bus_space_handle_t sc_bsh_rxdescs;
	bus_addr_t sc_txdescs_pa;
	bus_addr_t sc_rxdescs_pa;
	struct arpcom sc_ac;
	struct mii_data sc_mii;
	void *sc_ih;
	struct cpsw_ring_data *sc_rdp;
	volatile u_int sc_txnext;
	volatile u_int sc_txhead;
	volatile u_int sc_rxhead;
	void *sc_rxthih;
	void *sc_rxih;
	void *sc_txih;
	void *sc_miscih;
	void *sc_txpad;
	bus_dmamap_t sc_txpad_dm;
#define sc_txpad_pa sc_txpad_dm->dm_segs[0].ds_addr
	volatile bool sc_txrun;
	volatile bool sc_rxrun;
	volatile bool sc_txeoq;
	volatile bool sc_rxeoq;
	struct timeout sc_tick;
};

#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)

void	cpsw_get_mac_addr(struct cpsw_softc *);
void cpsw_attach(struct device *, struct device *, void *);

void cpsw_start(struct ifnet *);
int cpsw_ioctl(struct ifnet *, u_long, caddr_t);
void cpsw_watchdog(struct ifnet *);
int cpsw_init(struct ifnet *);
void cpsw_stop(struct ifnet *);

int cpsw_mii_readreg(struct device *, int, int);
void cpsw_mii_writereg(struct device *, int, int, int);
void cpsw_mii_statchg(struct device *);

void cpsw_tick(void *);

int cpsw_new_rxbuf(struct cpsw_softc * const, const u_int);
int	cpsw_mediachange(struct ifnet *);
void	cpsw_mediastatus(struct ifnet *, struct ifmediareq *);

int cpsw_rxthintr(void *);
int cpsw_rxintr(void *);
int cpsw_txintr(void *);
int cpsw_miscintr(void *);

struct cfattach cpsw_ca = {
	sizeof(struct cpsw_softc),
	NULL,
	cpsw_attach
};

struct cfdriver cpsw_cd = {
	NULL,
	"cpsw",
	DV_IFNET
};

/*
 *  * Return the number of bytes in the mbuf chain, m.
 *   */
static __inline u_int
m_length(const struct mbuf *m)
{
	const struct mbuf *m0;
	u_int pktlen;

	if ((m->m_flags & M_PKTHDR) != 0)
		return m->m_pkthdr.len;

	pktlen = 0;
	for (m0 = m; m0 != NULL; m0 = m0->m_next)
		pktlen += m0->m_len;
	return pktlen;
}

static inline u_int
cpsw_txdesc_adjust(u_int x, int y)
{
	return (((x) + y) & (CPSW_NTXDESCS - 1));
}

static inline u_int
cpsw_rxdesc_adjust(u_int x, int y)
{
	return (((x) + y) & (CPSW_NRXDESCS - 1));
}

static inline uint32_t
cpsw_read_4(struct cpsw_softc * const sc, bus_size_t const offset)
{
	return bus_space_read_4(sc->sc_bst, sc->sc_bsh, offset);
}

static inline void
cpsw_write_4(struct cpsw_softc * const sc, bus_size_t const offset,
    uint32_t const value)
{
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, offset, value);
}

static inline void
cpsw_set_txdesc_next(struct cpsw_softc * const sc, const u_int i, uint32_t n)
{
	const bus_size_t o = sizeof(struct cpsw_cpdma_bd) * i + 0;
	bus_space_write_4(sc->sc_bst, sc->sc_bsh_txdescs, o, n);
}

static inline void
cpsw_set_rxdesc_next(struct cpsw_softc * const sc, const u_int i, uint32_t n)
{
	const bus_size_t o = sizeof(struct cpsw_cpdma_bd) * i + 0;
	bus_space_write_4(sc->sc_bst, sc->sc_bsh_rxdescs, o, n);
}

static inline void
cpsw_get_txdesc(struct cpsw_softc * const sc, const u_int i,
    struct cpsw_cpdma_bd * const bdp)
{
	const bus_size_t o = sizeof(struct cpsw_cpdma_bd) * i;
	uint32_t * const dp = bdp->word;
	const bus_size_t c = nitems(bdp->word);

	bus_space_read_region_4(sc->sc_bst, sc->sc_bsh_txdescs, o, dp, c);
}

static inline void
cpsw_set_txdesc(struct cpsw_softc * const sc, const u_int i,
    struct cpsw_cpdma_bd * const bdp)
{
	const bus_size_t o = sizeof(struct cpsw_cpdma_bd) * i;
	uint32_t * const dp = bdp->word;
	const bus_size_t c = nitems(bdp->word);

	bus_space_write_region_4(sc->sc_bst, sc->sc_bsh_txdescs, o, dp, c);
}

static inline void
cpsw_get_rxdesc(struct cpsw_softc * const sc, const u_int i,
    struct cpsw_cpdma_bd * const bdp)
{
	const bus_size_t o = sizeof(struct cpsw_cpdma_bd) * i;
	uint32_t * const dp = bdp->word;
	const bus_size_t c = nitems(bdp->word);

	bus_space_read_region_4(sc->sc_bst, sc->sc_bsh_rxdescs, o, dp, c);
}

static inline void
cpsw_set_rxdesc(struct cpsw_softc * const sc, const u_int i,
    struct cpsw_cpdma_bd * const bdp)
{
	const bus_size_t o = sizeof(struct cpsw_cpdma_bd) * i;
	uint32_t * const dp = bdp->word;
	const bus_size_t c = nitems(bdp->word);

	bus_space_write_region_4(sc->sc_bst, sc->sc_bsh_rxdescs, o, dp, c);
}

static inline bus_addr_t
cpsw_txdesc_paddr(struct cpsw_softc * const sc, u_int x)
{
	KASSERT(x < CPSW_NTXDESCS);
	return sc->sc_txdescs_pa + sizeof(struct cpsw_cpdma_bd) * x;
}

static inline bus_addr_t
cpsw_rxdesc_paddr(struct cpsw_softc * const sc, u_int x)
{
	KASSERT(x < CPSW_NRXDESCS);
	return sc->sc_rxdescs_pa + sizeof(struct cpsw_cpdma_bd) * x;
}

void
cpsw_get_mac_addr(struct cpsw_softc *sc)
{
	struct arpcom *ac = &sc->sc_ac;
	u_int32_t	mac_lo = 0, mac_hi = 0;

	sitara_cm_reg_read_4(OMAP2SCM_MAC_ID0_LO, &mac_lo);
	sitara_cm_reg_read_4(OMAP2SCM_MAC_ID0_HI, &mac_hi);

	if ((mac_lo == 0) && (mac_hi == 0))
		printf("%s: invalid ethernet address\n", DEVNAME(sc));
	else {
		ac->ac_enaddr[0] = (mac_hi >>  0) & 0xff;
		ac->ac_enaddr[1] = (mac_hi >>  8) & 0xff;
		ac->ac_enaddr[2] = (mac_hi >> 16) & 0xff;
		ac->ac_enaddr[3] = (mac_hi >> 24) & 0xff;
		ac->ac_enaddr[4] = (mac_lo >>  0) & 0xff;
		ac->ac_enaddr[5] = (mac_lo >>  8) & 0xff;
	}
}

void
cpsw_attach(struct device *parent, struct device *self, void *aux)
{
	struct cpsw_softc *sc = (struct cpsw_softc *)self;
	struct omap_attach_args *oa = aux;
	struct arpcom * const ac = &sc->sc_ac;
	struct ifnet * const ifp = &ac->ac_if;
	int error;
	u_int i;

	timeout_set(&sc->sc_tick, cpsw_tick, sc);

	cpsw_get_mac_addr(sc);

	sc->sc_rxthih = arm_intr_establish(oa->oa_dev->irq[0] + CPSW_INTROFF_RXTH,
	    IPL_NET, cpsw_rxthintr, sc, DEVNAME(sc));
	sc->sc_rxih = arm_intr_establish(oa->oa_dev->irq[0] + CPSW_INTROFF_RX,
	    IPL_NET, cpsw_rxintr, sc, DEVNAME(sc));
	sc->sc_txih = arm_intr_establish(oa->oa_dev->irq[0] + CPSW_INTROFF_TX,
	    IPL_NET, cpsw_txintr, sc, DEVNAME(sc));
	sc->sc_miscih = arm_intr_establish(oa->oa_dev->irq[0] + CPSW_INTROFF_MISC,
	    IPL_NET, cpsw_miscintr, sc, DEVNAME(sc));

	sc->sc_bst = oa->oa_iot;
	sc->sc_bdt = oa->oa_dmat;

	error = bus_space_map(sc->sc_bst, oa->oa_dev->mem[0].addr, oa->oa_dev->mem[0].size, 0,
	    &sc->sc_bsh);
	if (error) {
		printf("can't map registers: %d\n", error);
		return;
	}

	sc->sc_txdescs_pa = oa->oa_dev->mem[0].addr + CPSW_CPPI_RAM_TXDESCS_BASE;
	error = bus_space_subregion(sc->sc_bst, sc->sc_bsh,
	    CPSW_CPPI_RAM_TXDESCS_BASE, CPSW_CPPI_RAM_TXDESCS_SIZE,
	    &sc->sc_bsh_txdescs);
	if (error) {
		printf("can't subregion tx ring SRAM: %d\n", error);
		return;
	}
	printf(" txdescs at %p", (void *)sc->sc_bsh_txdescs);

	sc->sc_rxdescs_pa = oa->oa_dev->mem[0].addr + CPSW_CPPI_RAM_RXDESCS_BASE;
	error = bus_space_subregion(sc->sc_bst, sc->sc_bsh,
	    CPSW_CPPI_RAM_RXDESCS_BASE, CPSW_CPPI_RAM_RXDESCS_SIZE,
	    &sc->sc_bsh_rxdescs);
	if (error) {
		printf("can't subregion rx ring SRAM: %d\n", error);
		return;
	}
	printf(" rxdescs at %p", (void *)sc->sc_bsh_rxdescs);

	sc->sc_rdp = malloc(sizeof(*sc->sc_rdp), M_TEMP, M_WAITOK);
	KASSERT(sc->sc_rdp != NULL);

	for (i = 0; i < CPSW_NTXDESCS; i++) {
		if ((error = bus_dmamap_create(sc->sc_bdt, MCLBYTES,
		    CPSW_TXFRAGS, MCLBYTES, 0, 0,
		    &sc->sc_rdp->tx_dm[i])) != 0) {
			printf("unable to create tx DMA map: %d\n", error);
		}
		sc->sc_rdp->tx_mb[i] = NULL;
	}

	for (i = 0; i < CPSW_NRXDESCS; i++) {
		if ((error = bus_dmamap_create(sc->sc_bdt, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rdp->rx_dm[i])) != 0) {
			printf("unable to create rx DMA map: %d\n", error);
		}
		sc->sc_rdp->rx_mb[i] = NULL;
	}

	/* XXX Not sure if this is the correct way to do this. orig below.
	    sc->sc_txpad = kmem_zalloc(ETHER_MIN_LEN, KM_SLEEP);
	*/
	sc->sc_txpad = malloc(ETHER_MIN_LEN, M_TEMP, M_WAITOK);
	KASSERT(sc->sc_txpad != NULL);
	bus_dmamap_create(sc->sc_bdt, ETHER_MIN_LEN, 1, ETHER_MIN_LEN, 0,
	    BUS_DMA_WAITOK, &sc->sc_txpad_dm);
	bus_dmamap_load(sc->sc_bdt, sc->sc_txpad_dm, sc->sc_txpad,
	    ETHER_MIN_LEN, NULL, BUS_DMA_WAITOK|BUS_DMA_WRITE);
	bus_dmamap_sync(sc->sc_bdt, sc->sc_txpad_dm, 0, ETHER_MIN_LEN,
	    BUS_DMASYNC_PREWRITE);

	printf(", address %s\n", ether_sprintf(ac->ac_enaddr));

	ifp->if_softc = sc;
	ifp->if_capabilities = 0;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = cpsw_start;
	ifp->if_ioctl = cpsw_ioctl;
	ifp->if_watchdog = cpsw_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, CPSW_NTXDESCS - 1);
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);

	cpsw_stop(ifp);

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = cpsw_mii_readreg;
	sc->sc_mii.mii_writereg = cpsw_mii_writereg;
	sc->sc_mii.mii_statchg = cpsw_mii_statchg;

	ifmedia_init(&sc->sc_mii.mii_media, 0, cpsw_mediachange,
	    cpsw_mediastatus);
	mii_attach(self, &sc->sc_mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("no PHY found!\n");
		ifmedia_add(&sc->sc_mii.mii_media,
		    IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else {
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	}

	if_attach(ifp);
	ether_ifattach(ifp);

	return;
}

int
cpsw_mediachange(struct ifnet *ifp)
{
	struct cpsw_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys))
		mii_mediachg(&sc->sc_mii);

	return (0);
}

void
cpsw_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cpsw_softc *sc = ifp->if_softc;

	if (LIST_FIRST(&sc->sc_mii.mii_phys)) {
		mii_pollstat(&sc->sc_mii);
		ifmr->ifm_active = sc->sc_mii.mii_media_active;
		ifmr->ifm_status = sc->sc_mii.mii_media_status;
	}
}

void
cpsw_start(struct ifnet *ifp)
{
	struct cpsw_softc * const sc = ifp->if_softc;
	struct cpsw_ring_data * const rdp = sc->sc_rdp;
	struct cpsw_cpdma_bd bd;
	uint32_t * const dw = bd.word;
	struct mbuf *m;
	bus_dmamap_t dm;
	u_int sopi;	/* Start of Packet Index */
	u_int eopi = ~0;
	u_int seg;
	u_int txfree;
	int txstart = -1;
	int error;
	bool pad;
	u_int mlen;

	if (__predict_false((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) !=
	    IFF_RUNNING)) {
		return;
	}

	if (sc->sc_txnext >= sc->sc_txhead)
		txfree = CPSW_NTXDESCS - 1 + sc->sc_txhead - sc->sc_txnext;
	else
		txfree = sc->sc_txhead - sc->sc_txnext - 1;

	while (txfree > 0) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		dm = rdp->tx_dm[sc->sc_txnext];

		error = bus_dmamap_load_mbuf(sc->sc_bdt, dm, m, BUS_DMA_NOWAIT);
		if (error == EFBIG) {
			printf("won't fit\n");
			IFQ_DEQUEUE(&ifp->if_snd, m);
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		} else if (error != 0) {
			printf("error\n");
			break;
		}

		if (dm->dm_nsegs + 1 >= txfree) {
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_bdt, dm);
			break;
		}

		mlen = m_length(m);
		pad = mlen < CPSW_PAD_LEN;

		KASSERT(rdp->tx_mb[sc->sc_txnext] == NULL);
		rdp->tx_mb[sc->sc_txnext] = m;
		IFQ_DEQUEUE(&ifp->if_snd, m);

		bus_dmamap_sync(sc->sc_bdt, dm, 0, dm->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		if (txstart == -1)
			txstart = sc->sc_txnext;
		sopi = eopi = sc->sc_txnext;
		for (seg = 0; seg < dm->dm_nsegs; seg++) {
			dw[0] = cpsw_txdesc_paddr(sc,
			    TXDESC_NEXT(sc->sc_txnext));
			dw[1] = dm->dm_segs[seg].ds_addr;
			dw[2] = dm->dm_segs[seg].ds_len;
			dw[3] = 0;

			if (seg == 0)
				dw[3] |= CPDMA_BD_SOP | CPDMA_BD_OWNER |
				    MAX(mlen, CPSW_PAD_LEN);

			if (seg == dm->dm_nsegs - 1 && !pad)
				dw[3] |= CPDMA_BD_EOP;

			cpsw_set_txdesc(sc, sc->sc_txnext, &bd);
			txfree--;
			eopi = sc->sc_txnext;
			sc->sc_txnext = TXDESC_NEXT(sc->sc_txnext);
		}
		if (pad) {
			dw[0] = cpsw_txdesc_paddr(sc,
			    TXDESC_NEXT(sc->sc_txnext));
			dw[1] = sc->sc_txpad_pa;
			dw[2] = CPSW_PAD_LEN - mlen;
			dw[3] = CPDMA_BD_EOP;

			cpsw_set_txdesc(sc, sc->sc_txnext, &bd);
			txfree--;
			eopi = sc->sc_txnext;
			sc->sc_txnext = TXDESC_NEXT(sc->sc_txnext);
		}
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	}

	if (txstart >= 0) {
		ifp->if_timer = 5;
		/* terminate the new chain */
		KASSERT(eopi == TXDESC_PREV(sc->sc_txnext));
		cpsw_set_txdesc_next(sc, TXDESC_PREV(sc->sc_txnext), 0);
		
		/* link the new chain on */
		cpsw_set_txdesc_next(sc, TXDESC_PREV(txstart),
		    cpsw_txdesc_paddr(sc, txstart));
		if (sc->sc_txeoq) {
			/* kick the dma engine */
			sc->sc_txeoq = false;
			cpsw_write_4(sc, CPSW_CPDMA_TX_HDP(0),
			    cpsw_txdesc_paddr(sc, txstart));
		}
	}
}

int
cpsw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s = splnet();
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				cpsw_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				cpsw_stop(ifp);
		}
		break;
	case SIOCSIFMEDIA:
		ifr->ifr_media &= ~IFM_ETH_FMASK;
		/* FALLTHROUGH */
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}
	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			cpsw_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

void
cpsw_watchdog(struct ifnet *ifp)
{
	printf("device timeout\n");

	ifp->if_oerrors++;
	cpsw_init(ifp);
	cpsw_start(ifp);
}

static int
cpsw_mii_wait(struct cpsw_softc * const sc, int reg)
{
	u_int tries;

	for(tries = 0; tries < 1000; tries++) {
		if ((cpsw_read_4(sc, reg) & __BIT(31)) == 0)
			return 0;
		delay(1);
	}
	return ETIMEDOUT;
}

int
cpsw_mii_readreg(struct device *dev, int phy, int reg)
{
	struct cpsw_softc * const sc = (struct cpsw_softc *)dev;
	uint32_t v;

	if (cpsw_mii_wait(sc, MDIOUSERACCESS0) != 0)
		return 0;

	cpsw_write_4(sc, MDIOUSERACCESS0, (1 << 31) |
	    ((reg & 0x1F) << 21) | ((phy & 0x1F) << 16));

	if (cpsw_mii_wait(sc, MDIOUSERACCESS0) != 0)
		return 0;

	v = cpsw_read_4(sc, MDIOUSERACCESS0);
	if (v & __BIT(29))
		return v & 0xffff;
	else
		return 0;
}

void
cpsw_mii_writereg(struct device *dev, int phy, int reg, int val)
{
	struct cpsw_softc * const sc = (struct cpsw_softc *)dev;
	uint32_t v;

	KASSERT((val & 0xffff0000UL) == 0);

	if (cpsw_mii_wait(sc, MDIOUSERACCESS0) != 0)
		goto out;

	cpsw_write_4(sc, MDIOUSERACCESS0, (1 << 31) | (1 << 30) |
	    ((reg & 0x1F) << 21) | ((phy & 0x1F) << 16) | val);

	if (cpsw_mii_wait(sc, MDIOUSERACCESS0) != 0)
		goto out;

	v = cpsw_read_4(sc, MDIOUSERACCESS0);
	if ((v & __BIT(29)) == 0)
out:
		printf("%s error\n", __func__);

}

void
cpsw_mii_statchg(struct device *self)
{
	return;
}

int
cpsw_new_rxbuf(struct cpsw_softc * const sc, const u_int i)
{
	struct cpsw_ring_data * const rdp = sc->sc_rdp;
	const u_int h = RXDESC_PREV(i);
	struct cpsw_cpdma_bd bd;
	uint32_t * const dw = bd.word;
	struct mbuf *m;
	int error = ENOBUFS;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		goto reuse;
	}

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		goto reuse;
	}

	/* We have a new buffer, prepare it for the ring. */

	if (rdp->rx_mb[i] != NULL)
		bus_dmamap_unload(sc->sc_bdt, rdp->rx_dm[i]);

	m->m_len = m->m_pkthdr.len = MCLBYTES;

	rdp->rx_mb[i] = m;

	error = bus_dmamap_load_mbuf(sc->sc_bdt, rdp->rx_dm[i], rdp->rx_mb[i],
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		printf("can't load rx DMA map %d: %d\n", i, error);
	}

	bus_dmamap_sync(sc->sc_bdt, rdp->rx_dm[i],
	    0, rdp->rx_dm[i]->dm_mapsize, BUS_DMASYNC_PREREAD);

	error = 0;

reuse:
	/* (re-)setup the descriptor */
	dw[0] = 0;
	dw[1] = rdp->rx_dm[i]->dm_segs[0].ds_addr;
	dw[2] = MIN(0x7ff, rdp->rx_dm[i]->dm_segs[0].ds_len);
	dw[3] = CPDMA_BD_OWNER;

	cpsw_set_rxdesc(sc, i, &bd);
	/* and link onto ring */
	cpsw_set_rxdesc_next(sc, h, cpsw_rxdesc_paddr(sc, i));

	return error;
}

int
cpsw_init(struct ifnet *ifp)
{
	struct cpsw_softc * const sc = ifp->if_softc;
	struct arpcom *ac = &sc->sc_ac;
	struct mii_data * const mii = &sc->sc_mii;
	int i;

	cpsw_stop(ifp);

	sc->sc_txnext = 0;
	sc->sc_txhead = 0;

	/* Reset wrapper */
	cpsw_write_4(sc, CPSW_WR_SOFT_RESET, 1);
	while(cpsw_read_4(sc, CPSW_WR_SOFT_RESET) & 1);

	/* Reset SS */
	cpsw_write_4(sc, CPSW_SS_SOFT_RESET, 1);
	while(cpsw_read_4(sc, CPSW_SS_SOFT_RESET) & 1);

	/* Clear table (30) and enable ALE(31) and set passthrough (4) */
	cpsw_write_4(sc, CPSW_ALE_CONTROL, (3 << 30) | 0x10);

	/* Reset and init Sliver port 1 and 2 */
	for (i = 0; i < 2; i++) {
		/* Reset */
		cpsw_write_4(sc, CPSW_SL_SOFT_RESET(i), 1);
		while(cpsw_read_4(sc, CPSW_SL_SOFT_RESET(i)) & 1);
		/* Set Slave Mapping */
		cpsw_write_4(sc, CPSW_SL_RX_PRI_MAP(i), 0x76543210);
		cpsw_write_4(sc, CPSW_PORT_P_TX_PRI_MAP(i+1), 0x33221100);
		cpsw_write_4(sc, CPSW_SL_RX_MAXLEN(i), 0x5f2);
		/* Set MAC Address */
		cpsw_write_4(sc, CPSW_PORT_P_SA_HI(i+1),
		    ac->ac_enaddr[0] | (ac->ac_enaddr[1] << 8) |
		    (ac->ac_enaddr[2] << 16) | (ac->ac_enaddr[3] << 24));
		cpsw_write_4(sc, CPSW_PORT_P_SA_LO(i+1),
		    ac->ac_enaddr[4] | (ac->ac_enaddr[5] << 8));

		/* Set MACCONTROL for ports 0,1: FULLDUPLEX(1), GMII_EN(5),
		   IFCTL_A(15), IFCTL_B(16) FIXME */
		cpsw_write_4(sc, CPSW_SL_MACCONTROL(i), 1 | (1<<5) | (1<<15) | (1<<16));

		/* Set ALE port to forwarding(3) */
		cpsw_write_4(sc, CPSW_ALE_PORTCTL(i+1), 3);
	}

	/* Set Host Port Mapping */
	cpsw_write_4(sc, CPSW_PORT_P0_CPDMA_TX_PRI_MAP, 0x76543210);
	cpsw_write_4(sc, CPSW_PORT_P0_CPDMA_RX_CH_MAP, 0);

	/* Set ALE port to forwarding(3) */
	cpsw_write_4(sc, CPSW_ALE_PORTCTL(0), 3);

	cpsw_write_4(sc, CPSW_SS_PTYPE, 0);
	cpsw_write_4(sc, CPSW_SS_STAT_PORT_EN, 7);

	cpsw_write_4(sc, CPSW_CPDMA_SOFT_RESET, 1);
	while(cpsw_read_4(sc, CPSW_CPDMA_SOFT_RESET) & 1);

	for (i = 0; i < 8; i++) {
		cpsw_write_4(sc, CPSW_CPDMA_TX_HDP(i), 0);
		cpsw_write_4(sc, CPSW_CPDMA_RX_HDP(i), 0);
		cpsw_write_4(sc, CPSW_CPDMA_TX_CP(i), 0);
		cpsw_write_4(sc, CPSW_CPDMA_RX_CP(i), 0);
	}

	bus_space_set_region_4(sc->sc_bst, sc->sc_bsh_txdescs, 0, 0,
	    CPSW_CPPI_RAM_TXDESCS_SIZE/4);

	sc->sc_txhead = 0;
	sc->sc_txnext = 0;

	cpsw_write_4(sc, CPSW_CPDMA_RX_FREEBUFFER(0), 0);

	bus_space_set_region_4(sc->sc_bst, sc->sc_bsh_rxdescs, 0, 0,
	    CPSW_CPPI_RAM_RXDESCS_SIZE/4);

	/* Initialize RX Buffer Descriptors */
	cpsw_set_rxdesc_next(sc, RXDESC_PREV(0), 0);
	for (i = 0; i < CPSW_NRXDESCS; i++) {
		cpsw_new_rxbuf(sc, i);
	}
	sc->sc_rxhead = 0;

	/* align layer 3 header to 32-bit */
	cpsw_write_4(sc, CPSW_CPDMA_RX_BUFFER_OFFSET, ETHER_ALIGN);

	/* Clear all interrupt Masks */
	cpsw_write_4(sc, CPSW_CPDMA_RX_INTMASK_CLEAR, 0xFFFFFFFF);
	cpsw_write_4(sc, CPSW_CPDMA_TX_INTMASK_CLEAR, 0xFFFFFFFF);

	/* Enable TX & RX DMA */
	cpsw_write_4(sc, CPSW_CPDMA_TX_CONTROL, 1);
	cpsw_write_4(sc, CPSW_CPDMA_RX_CONTROL, 1);

	/* Enable TX and RX interrupt receive for core 0 */
	cpsw_write_4(sc, CPSW_WR_C_TX_EN(0), 1);
	cpsw_write_4(sc, CPSW_WR_C_RX_EN(0), 1);
	cpsw_write_4(sc, CPSW_WR_C_MISC_EN(0), 0x1F);

	/* Enable host Error Interrupt */
	cpsw_write_4(sc, CPSW_CPDMA_DMA_INTMASK_SET, 2);

	/* Enable interrupts for TX and RX Channel 0 */
	cpsw_write_4(sc, CPSW_CPDMA_TX_INTMASK_SET, 1);
	cpsw_write_4(sc, CPSW_CPDMA_RX_INTMASK_SET, 1);

	/* Ack stalled irqs */
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_RXTH);
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_RX);
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_TX);
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_MISC);

	/* Initialze MDIO - ENABLE, PREAMBLE=0, FAULTENB, CLKDIV=0xFF */
	/* TODO Calculate MDCLK=CLK/(CLKDIV+1) */
	cpsw_write_4(sc, MDIOCONTROL, (1<<30) | (1<<18) | 0xFF);

	mii_mediachg(mii);

	/* Write channel 0 RX HDP */
	cpsw_write_4(sc, CPSW_CPDMA_RX_HDP(0), cpsw_rxdesc_paddr(sc, 0));
	sc->sc_rxrun = true;
	sc->sc_rxeoq = false;

	sc->sc_txrun = true;
	sc->sc_txeoq = true;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	timeout_add_sec(&sc->sc_tick, 1);

	return 0;
}

void
cpsw_stop(struct ifnet *ifp)
{
	struct cpsw_softc * const sc = ifp->if_softc;
	struct cpsw_ring_data * const rdp = sc->sc_rdp;
	u_int i;

#if 0
	/* XXX find where disable comes from */
	printf("%s: ifp %p disable %d\n", __func__, ifp, disable);
#endif
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	timeout_del(&sc->sc_tick);

	mii_down(&sc->sc_mii);

	cpsw_write_4(sc, CPSW_CPDMA_TX_INTMASK_CLEAR, 1);
	cpsw_write_4(sc, CPSW_CPDMA_RX_INTMASK_CLEAR, 1);
	cpsw_write_4(sc, CPSW_WR_C_TX_EN(0), 0x0);
	cpsw_write_4(sc, CPSW_WR_C_RX_EN(0), 0x0);
	cpsw_write_4(sc, CPSW_WR_C_MISC_EN(0), 0x1F);

	cpsw_write_4(sc, CPSW_CPDMA_TX_TEARDOWN, 0);
	cpsw_write_4(sc, CPSW_CPDMA_RX_TEARDOWN, 0);
	i = 0;
	while ((sc->sc_txrun || sc->sc_rxrun) && i < 10000) {
		delay(10);
		if ((sc->sc_txrun == true) && cpsw_txintr(sc) == 0)
			sc->sc_txrun = false;
		if ((sc->sc_rxrun == true) && cpsw_rxintr(sc) == 0)
			sc->sc_rxrun = false;
		i++;
	}
	/* printf("%s toredown complete in %u\n", __func__, i); */

	/* Reset wrapper */
	cpsw_write_4(sc, CPSW_WR_SOFT_RESET, 1);
	while(cpsw_read_4(sc, CPSW_WR_SOFT_RESET) & 1);

	/* Reset SS */
	cpsw_write_4(sc, CPSW_SS_SOFT_RESET, 1);
	while(cpsw_read_4(sc, CPSW_SS_SOFT_RESET) & 1);

	for (i = 0; i < 2; i++) {
		cpsw_write_4(sc, CPSW_SL_SOFT_RESET(i), 1);
		while(cpsw_read_4(sc, CPSW_SL_SOFT_RESET(i)) & 1);
	}

	/* Reset CPDMA */
	cpsw_write_4(sc, CPSW_CPDMA_SOFT_RESET, 1);
	while(cpsw_read_4(sc, CPSW_CPDMA_SOFT_RESET) & 1);

	/* Release any queued transmit buffers. */
	for (i = 0; i < CPSW_NTXDESCS; i++) {
		bus_dmamap_unload(sc->sc_bdt, rdp->tx_dm[i]);
		m_freem(rdp->tx_mb[i]);
		rdp->tx_mb[i] = NULL;
	}

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
	ifp->if_timer = 0;

	/* XXX Not sure what this is doing calling disable here
	    where is disable set?
	*/
#if 0
	if (!disable)
		return;
#endif

	for (i = 0; i < CPSW_NRXDESCS; i++) {
		bus_dmamap_unload(sc->sc_bdt, rdp->rx_dm[i]);
		m_freem(rdp->rx_mb[i]);
		rdp->rx_mb[i] = NULL;
	}
}

int
cpsw_rxthintr(void *arg)
{
	struct cpsw_softc * const sc = arg;

	/* this won't deassert the interrupt though */
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_RXTH);

	return 1;
}

int
cpsw_rxintr(void *arg)
{
	struct cpsw_softc * const sc = arg;
	struct ifnet * const ifp = &sc->sc_ac.ac_if;
	struct cpsw_ring_data * const rdp = sc->sc_rdp;
	struct cpsw_cpdma_bd bd;
	const uint32_t * const dw = bd.word;
	bus_dmamap_t dm;
	struct mbuf *m;
	u_int i;
	u_int len, off;

	for (;;) {
		KASSERT(sc->sc_rxhead < CPSW_NRXDESCS);

		i = sc->sc_rxhead;
		dm = rdp->rx_dm[i];
		m = rdp->rx_mb[i];

		KASSERT(dm != NULL);
		KASSERT(m != NULL);

		cpsw_get_rxdesc(sc, i, &bd);

		if (ISSET(dw[3], CPDMA_BD_OWNER))
			break;

		if (ISSET(dw[3], CPDMA_BD_TDOWNCMPLT)) {
			sc->sc_rxrun = false;
			return 1;
		}

		if ((dw[3] & (CPDMA_BD_SOP|CPDMA_BD_EOP)) !=
		    (CPDMA_BD_SOP|CPDMA_BD_EOP)) {
			/* Debugger(); */
		}

		bus_dmamap_sync(sc->sc_bdt, dm, 0, dm->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);

		if (cpsw_new_rxbuf(sc, i) != 0) {
			/* drop current packet, reuse buffer for new */
			ifp->if_ierrors++;
			goto next;
		}

		off = __SHIFTOUT(dw[2], (uint32_t)__BITS(26, 16));
		len = __SHIFTOUT(dw[3], (uint32_t)__BITS(10,  0));

		if (ISSET(dw[3], CPDMA_BD_PASSCRC))
			len -= ETHER_CRC_LEN;

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;
		m->m_data += off;

		ifp->if_ipackets++;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif
		ether_input_mbuf(ifp, m);

next:
		sc->sc_rxhead = RXDESC_NEXT(sc->sc_rxhead);
		if (ISSET(dw[3], CPDMA_BD_EOQ)) {
			sc->sc_rxeoq = true;
			break;
		} else {
			sc->sc_rxeoq = false;
		}
		cpsw_write_4(sc, CPSW_CPDMA_RX_CP(0),
		    cpsw_rxdesc_paddr(sc, i));
	}

	if (sc->sc_rxeoq) {
		printf("rxeoq\n");
		/* Debugger(); */
	}

	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_RX);

	return 1;
}

void
cpsw_tick(void *arg)
{
	struct cpsw_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add_sec(&sc->sc_tick, 1);
}

int
cpsw_txintr(void *arg)
{
	struct cpsw_softc * const sc = arg;
	struct ifnet * const ifp = &sc->sc_ac.ac_if;
	struct cpsw_ring_data * const rdp = sc->sc_rdp;
	struct cpsw_cpdma_bd bd;
	const uint32_t * const dw = bd.word;
	bool handled = false;
	uint32_t tx0_cp;
	u_int cpi;

	KASSERT(sc->sc_txrun);

	tx0_cp = cpsw_read_4(sc, CPSW_CPDMA_TX_CP(0));

	if (tx0_cp == 0xfffffffc) {
		cpsw_write_4(sc, CPSW_CPDMA_TX_CP(0), 0xfffffffc);
		cpsw_write_4(sc, CPSW_CPDMA_TX_HDP(0), 0);
		sc->sc_txrun = false;
		return 0;
	}

	cpi = (tx0_cp - sc->sc_txdescs_pa) / sizeof(struct cpsw_cpdma_bd);

	for (;;) {
		tx0_cp = cpsw_read_4(sc, CPSW_CPDMA_TX_CP(0));
		cpi = (tx0_cp - sc->sc_txdescs_pa) / sizeof(struct cpsw_cpdma_bd);
		KASSERT(sc->sc_txhead < CPSW_NTXDESCS);


		cpsw_get_txdesc(sc, sc->sc_txhead, &bd);

		if (dw[2] == 0) {
			/* Debugger(); */
		}

		if (ISSET(dw[3], CPDMA_BD_SOP) == 0)
			goto next;

		if (ISSET(dw[3], CPDMA_BD_OWNER)) {
			printf("pwned %x %x %x\n", cpi, sc->sc_txhead, sc->sc_txnext);
			break;
		}

		if (ISSET(dw[3], CPDMA_BD_TDOWNCMPLT)) {
			sc->sc_txrun = false;
			return 1;
		}

		bus_dmamap_sync(sc->sc_bdt, rdp->tx_dm[sc->sc_txhead],
		    0, rdp->tx_dm[sc->sc_txhead]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_bdt, rdp->tx_dm[sc->sc_txhead]);

		m_freem(rdp->tx_mb[sc->sc_txhead]);
		rdp->tx_mb[sc->sc_txhead] = NULL;

		ifp->if_opackets++;

		handled = true;

		ifp->if_flags &= ~IFF_OACTIVE;

next:
		if (ISSET(dw[3], CPDMA_BD_EOP) && ISSET(dw[3], CPDMA_BD_EOQ)) {
			sc->sc_txeoq = true;
		}
		if (sc->sc_txhead == cpi) {
			cpsw_write_4(sc, CPSW_CPDMA_TX_CP(0),
			    cpsw_txdesc_paddr(sc, cpi));
			sc->sc_txhead = TXDESC_NEXT(sc->sc_txhead);
			break;
		}
		sc->sc_txhead = TXDESC_NEXT(sc->sc_txhead);
		if (ISSET(dw[3], CPDMA_BD_EOP) && ISSET(dw[3], CPDMA_BD_EOQ)) {
			sc->sc_txeoq = true;
			break;
		}
	}

	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_TX);

	if ((sc->sc_txnext != sc->sc_txhead) && sc->sc_txeoq) {
		if (cpsw_read_4(sc, CPSW_CPDMA_TX_HDP(0)) == 0) {
			sc->sc_txeoq = false;
			cpsw_write_4(sc, CPSW_CPDMA_TX_HDP(0),
			    cpsw_txdesc_paddr(sc, sc->sc_txhead));
		}
	}

	if (handled && sc->sc_txnext == sc->sc_txhead)
		ifp->if_timer = 0;

	if (handled)
		cpsw_start(ifp);

	return handled;
}

int
cpsw_miscintr(void *arg)
{
	struct cpsw_softc * const sc = arg;
	uint32_t miscstat;
	uint32_t dmastat;
	uint32_t stat;

	miscstat = cpsw_read_4(sc, CPSW_WR_C_MISC_STAT(0));
	printf("%s %x FIRE\n", __func__, miscstat);

#define CPSW_MISC_HOST_PEND __BIT32(2)
#define CPSW_MISC_STAT_PEND __BIT32(3)

	if (ISSET(miscstat, CPSW_MISC_HOST_PEND)) {
		/* Host Error */
		dmastat = cpsw_read_4(sc, CPSW_CPDMA_DMA_INTSTAT_MASKED);
		printf("CPSW_CPDMA_DMA_INTSTAT_MASKED %x\n", dmastat);

		printf("rxhead %02x\n", sc->sc_rxhead);

		stat = cpsw_read_4(sc, CPSW_CPDMA_DMASTATUS);
		printf("CPSW_CPDMA_DMASTATUS %x\n", stat);
		stat = cpsw_read_4(sc, CPSW_CPDMA_TX_HDP(0));
		printf("CPSW_CPDMA_TX0_HDP %x\n", stat);
		stat = cpsw_read_4(sc, CPSW_CPDMA_TX_CP(0));
		printf("CPSW_CPDMA_TX0_CP %x\n", stat);
		stat = cpsw_read_4(sc, CPSW_CPDMA_RX_HDP(0));
		printf("CPSW_CPDMA_RX0_HDP %x\n", stat);
		stat = cpsw_read_4(sc, CPSW_CPDMA_RX_CP(0));
		printf("CPSW_CPDMA_RX0_CP %x\n", stat);

		/* Debugger(); */

		cpsw_write_4(sc, CPSW_CPDMA_DMA_INTMASK_CLEAR, dmastat);
		dmastat = cpsw_read_4(sc, CPSW_CPDMA_DMA_INTSTAT_MASKED);
		printf("CPSW_CPDMA_DMA_INTSTAT_MASKED %x\n", dmastat);
	}

	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, CPSW_INTROFF_MISC);

	return 1;
}
