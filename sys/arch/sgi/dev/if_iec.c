/*	$OpenBSD: if_iec.c,v 1.12 2014/12/22 02:26:53 tedu Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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
 * Heavily based upon if_mec.c with the following license terms:
 *
 *	OpenBSD: if_mec.c,v 1.22 2009/10/26 18:00:06 miod Exp
 *
 * Copyright (c) 2004 Izumi Tsutsui.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (c) 2003 Christopher SEKIYA
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IOC3 Ethernet driver
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <sgi/dev/if_iecreg.h>
#include <sgi/dev/owmacvar.h>
#include <sgi/pci/iocreg.h>
#include <sgi/pci/iocvar.h>

#ifdef IEC_DEBUG
#define IEC_DEBUG_RESET		0x01
#define IEC_DEBUG_START		0x02
#define IEC_DEBUG_STOP		0x04
#define IEC_DEBUG_INTR		0x08
#define IEC_DEBUG_RXINTR	0x10
#define IEC_DEBUG_TXINTR	0x20
#define IEC_DEBUG_MII		0x40
uint32_t iec_debug = 0xffffffff;
#define DPRINTF(x, y)	if (iec_debug & (x)) printf y
#else
#define DPRINTF(x, y)	/* nothing */
#endif

/*
 * Transmit descriptor list size.
 */
#define IEC_NTXDESC		IEC_NTXDESC_MAX
#define IEC_NEXTTX(x)		(((x) + 1) % IEC_NTXDESC_MAX)

/*
 * Software state for TX.
 */
struct iec_txsoft {
	struct mbuf *txs_mbuf;		/* Head of our mbuf chain. */
	bus_dmamap_t txs_dmamap;	/* Our DMA map. */
	uint32_t txs_flags;
};

/*
 * Receive buffer management.
 *
 * The RX buffers chip register points to a contiguous array of
 * 512 8-bit pointers to the RX buffers themselves.
 */

/*
 * Receive descriptor list sizes (these depend on the size of the SSRAM).
 */
#define	IEC_NRXDESC_SMALL	64
#define	IEC_NRXDESC_LARGE	128

/*
 * In addition to the receive descriptor themselves, we'll need an array
 * of 512 RX descriptor pointers (regardless of how many real descriptors
 * we use), aligned on a 4KB boundary.
 */

/*
 * Control structures for DMA ops.
 */
struct iec_control_data {
	/*
	 * TX descriptors and buffers.
	 */
	struct iec_txdesc icd_txdesc[IEC_NTXDESC_MAX];

	/*
	 * RX descriptors and buffers.
	 */
	struct iec_rxdesc icd_rxdesc[1];
};

/*
 * Alignment restrictions (found by trial and error, might be slightly
 * pessimistic).
 * - base address of rx desc pointer array should be aligned on a
 *   16KB boundary.
 * - base address of rx and tx desc array should be aligned on
 *   16KB boundaries (note layout of struct iec_control_data makes sure
 *   the rx desc array starts 16KB after the tx desc array).
 * - each txdesc should be 128 byte aligned (this is also enforced by
 *   struct iec_control_data layout).
 * - each rxdesc should be aligned on a 4KB boundary (this is enforced by
 *   struct iec_control_data and struct icd_rxdesc layouts).
 */
#define	IEC_DMA_BOUNDARY	0x4000

#define IEC_CDOFF(x)	offsetof(struct iec_control_data, x)
#define IEC_CDTXOFF(x)	IEC_CDOFF(icd_txdesc[(x)])
#define IEC_CDRXOFF(x)	IEC_CDOFF(icd_rxdesc[(x)])

/*
 * Software state per device.
 */
struct iec_softc {
	struct device sc_dev;		/* Generic device structures. */
	struct arpcom sc_ac;		/* Ethernet common part. */

	bus_space_tag_t sc_st;		/* bus_space tag. */
	bus_space_handle_t sc_sh;	/* bus_space handle. */
	bus_dma_tag_t sc_dmat;		/* bus_dma tag. */

	struct mii_data sc_mii;		/* MII/media information. */
	int sc_phyaddr;			/* MII address. */
	struct timeout sc_tick;		/* Tick timeout. */

	uint64_t	*sc_rxarr;	/* kva for rx pointers array. */
	bus_dmamap_t	sc_rxarrmap;	/* bus_dma map for rx pointers array. */
#define	sc_rxptrdma	sc_rxarrmap->dm_segs[0].ds_addr

	bus_dmamap_t sc_cddmamap;	/* bus_dma map for control data. */
#define sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/* Pointer to allocated control data. */
	struct iec_control_data *sc_control_data;
#define sc_txdesc	sc_control_data->icd_txdesc
#define sc_rxdesc	sc_control_data->icd_rxdesc

	/* Software state for TX descs. */
	struct iec_txsoft sc_txsoft[IEC_NTXDESC];

	int sc_txpending;		/* Number of TX requests pending. */
	int sc_txdirty;			/* First dirty TX descriptor. */
	int sc_txlast;			/* Last used TX descriptor. */

	uint32_t sc_rxci;		/* Saved RX consumer index. */
	uint32_t sc_rxpi;		/* Saved RX producer index. */
	uint32_t sc_nrxdesc;		/* Amount of RX descriptors. */

	uint32_t sc_mcr;		/* Current MCR value. */
};

#define IEC_CDTXADDR(sc, x)	((sc)->sc_cddma + IEC_CDTXOFF(x))
#define IEC_CDRXADDR(sc, x)	((sc)->sc_cddma + IEC_CDRXOFF(x))

#define IEC_TXDESCSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    IEC_CDTXOFF(x), IEC_TXDESCSIZE, (ops))
#define IEC_TXCMDSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    IEC_CDTXOFF(x), 2 * sizeof(uint32_t), (ops))

#define IEC_RXSTATSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    IEC_CDRXOFF(x), 2 * sizeof(uint32_t), (ops))
#define IEC_RXBUFSYNC(sc, x, len, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    IEC_CDRXOFF(x) + IEC_RXD_BUFOFFSET,	(len), (ops))

/* XXX these values should be moved to <net/if_ether.h> ? */
#define ETHER_PAD_LEN	(ETHER_MIN_LEN - ETHER_CRC_LEN)

struct cfdriver iec_cd = {
	NULL, "iec", DV_IFNET
};

#define	DEVNAME(sc)	((sc)->sc_dev.dv_xname)

int	iec_match(struct device *, void *, void *);
void	iec_attach(struct device *, struct device *, void *);

struct cfattach iec_ca = {
	sizeof(struct iec_softc), iec_match, iec_attach
};

int	iec_mii_readreg(struct device *, int, int);
void	iec_mii_writereg(struct device *, int, int, int);
int	iec_mii_wait(struct iec_softc *);
void	iec_statchg(struct device *);
void	iec_mediastatus(struct ifnet *, struct ifmediareq *);
int	iec_mediachange(struct ifnet *);

int	iec_alloc_physical(struct iec_softc *, bus_dmamap_t *,
	    bus_dma_segment_t *, vaddr_t *, bus_addr_t, bus_size_t,
	    const char *);
struct mbuf *
	iec_get(struct iec_softc *, uint8_t *, size_t);
void	iec_iff(struct iec_softc *);
int	iec_init(struct ifnet * ifp);
int	iec_intr(void *arg);
int	iec_ioctl(struct ifnet *, u_long, caddr_t);
void	iec_reset(struct iec_softc *);
void	iec_rxintr(struct iec_softc *, uint32_t);
int	iec_ssram_probe(struct iec_softc *);
void	iec_start(struct ifnet *);
void	iec_stop(struct ifnet *);
void	iec_tick(void *);
void	iec_txintr(struct iec_softc *, uint32_t);
void	iec_watchdog(struct ifnet *);

int
iec_match(struct device *parent, void *match, void *aux)
{
	/*
	 * We expect ioc NOT to attach us on if there is no Ethernet
	 * component.
	 */
	return 1;
}

void
iec_attach(struct device *parent, struct device *self, void *aux)
{
	struct iec_softc *sc = (void *)self;
	struct ioc_attach_args *iaa = aux;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mii_softc *child;
	bus_dma_segment_t seg1;
	bus_dma_segment_t seg2;
	bus_size_t control_size;
	int i, rc, rseg;

	sc->sc_st = iaa->iaa_memt;
	sc->sc_sh = iaa->iaa_memh;
	sc->sc_dmat = iaa->iaa_dmat;

	/*
	 * Try and figure out how much SSRAM is available, and decide
	 * how many RX buffers to use.
	 */
	i = iec_ssram_probe(sc);
	printf(": %dKB SSRAM", i);

	/*
	 * Allocate a page for RX descriptor pointers, suitable for use
	 * by the device.
	 */
	rc = iec_alloc_physical(sc, &sc->sc_rxarrmap, &seg1,
	    (vaddr_t *)&sc->sc_rxarr, IEC_DMA_BOUNDARY,
	    IEC_NRXDESC_MAX * sizeof(uint64_t), "rxdesc pointer array");
	if (rc != 0)
		return;

	/*
	 * Allocate the RX and TX descriptors.
	 */
	control_size = IEC_NTXDESC_MAX * sizeof(struct iec_txdesc) +
	    sc->sc_nrxdesc * sizeof(struct iec_rxdesc);
	rc = iec_alloc_physical(sc, &sc->sc_cddmamap, &seg2,
	    (vaddr_t *)&sc->sc_control_data, IEC_DMA_BOUNDARY,
	    control_size, "rx and tx descriptors");
	if (rc != 0)
		goto fail_1;

	/*
	 * Initialize RX pointer array.
	 */

	for (i = 0; i < IEC_NRXDESC_MAX; i++)
		sc->sc_rxarr[i] = IEC_CDRXADDR(sc, i & (sc->sc_nrxdesc - 1));

	/* Create TX buffer DMA maps. */
	for (i = 0; i < IEC_NTXDESC; i++) {
		if ((rc = bus_dmamap_create(sc->sc_dmat,
		    MCLBYTES, 1, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			printf(": unable to create tx DMA map %d, error = %d\n",
			    i, rc);
			goto fail_4;
		}
	}

	timeout_set(&sc->sc_tick, iec_tick, sc);

	bcopy(iaa->iaa_enaddr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);
	printf(", address %s\n", ether_sprintf(sc->sc_ac.ac_enaddr));

	/* Reset device. */
	iec_reset(sc);

	/* Done, now attach everything. */

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = iec_mii_readreg;
	sc->sc_mii.mii_writereg = iec_mii_writereg;
	sc->sc_mii.mii_statchg = iec_statchg;

	/* Set up PHY properties. */
	ifmedia_init(&sc->sc_mii.mii_media, 0, iec_mediachange,
	    iec_mediastatus);
	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	child = LIST_FIRST(&sc->sc_mii.mii_phys);
	if (child == NULL) {
		/* No PHY attached. */
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER | IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_MANUAL);
	} else {
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO);
		sc->sc_phyaddr = child->mii_phy;
	}

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = iec_ioctl;
	ifp->if_start = iec_start;
	ifp->if_watchdog = iec_watchdog;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	IFQ_SET_MAXLEN(&ifp->if_snd, IEC_NTXDESC - 1);
	ether_ifattach(ifp);

	/* Establish interrupt handler. */
	ioc_intr_establish(parent, iaa->iaa_dev, IPL_NET,
	    iec_intr, sc, sc->sc_dev.dv_xname);

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt. Do this in reverse order and fall through.
	 */
fail_4:
	for (i = 0; i < IEC_NTXDESC; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}

	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_control_data,
	    control_size);
	bus_dmamem_free(sc->sc_dmat, &seg2, rseg);
fail_1:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_rxarrmap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_rxarrmap);
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_rxarr,
	    IEC_NRXDESC_MAX * sizeof(uint64_t));
	bus_dmamem_free(sc->sc_dmat, &seg1, rseg);
}

/*
 * Allocate contiguous physical memory.
 */
int
iec_alloc_physical(struct iec_softc *sc, bus_dmamap_t *dmamap,
    bus_dma_segment_t *dmaseg, vaddr_t *va, bus_addr_t alignment,
    bus_size_t len, const char *what)
{
	int nseg;
	int rc;

	rc = bus_dmamem_alloc(sc->sc_dmat, len, alignment, 0, dmaseg, 1, &nseg,
	    BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to allocate %s memory: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail1;
	}

	rc = bus_dmamem_map(sc->sc_dmat, dmaseg, nseg, len,
	    (caddr_t *)va, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (rc != 0) {
		printf("%s: unable to map %s memory: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail2;
	}

	rc = bus_dmamap_create(sc->sc_dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	    dmamap);
	if (rc != 0) {
		printf("%s: unable to create %s dma map: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail3;
	}

	rc = bus_dmamap_load(sc->sc_dmat, *dmamap, (void *)*va, len, NULL,
	    BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to load %s dma map: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail4;
	}

	memset((caddr_t)*va, 0, len);
	return 0;

fail4:
	bus_dmamap_destroy(sc->sc_dmat, *dmamap);
fail3:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)*va, PAGE_SIZE);
fail2:
	bus_dmamem_free(sc->sc_dmat, dmaseg, 1);
fail1:
	return rc;
}

int
iec_mii_readreg(struct device *self, int phy, int reg)
{
	struct iec_softc *sc = (void *)self;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;

	if (iec_mii_wait(sc) != 0)
		return 0;

	bus_space_write_4(st, sh, IOC3_ENET_MICR, IOC3_ENET_MICR_READ |
	    (phy << IOC3_ENET_MICR_PHY_SHIFT) |
	    (reg & IOC3_ENET_MICR_REG_MASK));
	delay(25);

	if (iec_mii_wait(sc) == 0)
		return bus_space_read_4(st, sh, IOC3_ENET_MIDR_R) &
		    IOC3_ENET_MIDR_MASK;

	DPRINTF(IEC_DEBUG_MII, ("MII timed out reading %x\n", reg));
	return 0;
}

void
iec_mii_writereg(struct device *self, int phy, int reg, int val)
{
	struct iec_softc *sc = (void *)self;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;

	if (iec_mii_wait(sc) != 0) {
		DPRINTF(IEC_DEBUG_MII,
		    ("MII timed out writing %x: %x\n", reg, val));
		return;
	}

	bus_space_write_4(st, sh, IOC3_ENET_MIDR_W, val & IOC3_ENET_MIDR_MASK);
	delay(60);
	bus_space_write_4(st, sh, IOC3_ENET_MICR,
	    (phy << IOC3_ENET_MICR_PHY_SHIFT) |
	    (reg & IOC3_ENET_MICR_REG_MASK));
	delay(60);

	iec_mii_wait(sc);
}

int
iec_mii_wait(struct iec_softc *sc)
{
	uint32_t busy;
	int i;

	for (i = 0; i < 100; i++) {
		busy = bus_space_read_4(sc->sc_st, sc->sc_sh, IOC3_ENET_MICR);
		if ((busy & IOC3_ENET_MICR_BUSY) == 0)
			return 0;
		delay(30);
	}

	printf("%s: MII timed out\n", sc->sc_dev.dv_xname);
	return 1;
}

void
iec_statchg(struct device *self)
{
	struct iec_softc *sc = (void *)self;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint32_t tcsr;

	if ((sc->sc_mii.mii_media_active & IFM_FDX) != 0) {
		tcsr = IOC3_ENET_TCSR_FULL_DUPLEX;
		sc->sc_mcr |= IOC3_ENET_MCR_DUPLEX;
	} else {
		tcsr = IOC3_ENET_TCSR_HALF_DUPLEX;
		sc->sc_mcr &= ~IOC3_ENET_MCR_DUPLEX;
	}

	bus_space_write_4(st, sh, IOC3_ENET_MCR, sc->sc_mcr);
	bus_space_write_4(st, sh, IOC3_ENET_TCSR, tcsr);
}

void
iec_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct iec_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

int
iec_mediachange(struct ifnet *ifp)
{
	struct iec_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;

	return mii_mediachg(&sc->sc_mii);
}

int
iec_init(struct ifnet *ifp)
{
	struct iec_softc *sc = ifp->if_softc;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct iec_rxdesc *rxd;
	int i;

	/* Cancel any pending I/O. */
	iec_stop(ifp);

	/* Reset device. */
	iec_reset(sc);

	/* Setup filter for multicast or promisc mode. */
	iec_iff(sc);

	/*
	 * Initialize TX ring.
	 */

	bus_space_write_4(st, sh, IOC3_ENET_TBR_H, IEC_CDTXADDR(sc, 0) >> 32);
	bus_space_write_4(st, sh, IOC3_ENET_TBR_L,
	    (uint32_t)IEC_CDTXADDR(sc, 0));
	bus_space_write_4(st, sh, IOC3_ENET_TCIR, 0);
	bus_space_write_4(st, sh, IOC3_ENET_TPIR, 0);
	(void)bus_space_read_4(st, sh, IOC3_ENET_TCIR);

	sc->sc_txpending = 0;
	sc->sc_txdirty = 0;
	sc->sc_txlast = IEC_NTXDESC - 1;

	/*
	 * Initialize RX ring.
	 */

	/* Point the RX base register to our RX pointers array. */
	bus_space_write_4(st, sh, IOC3_ENET_RBR_H, sc->sc_rxptrdma >> 32);
	bus_space_write_4(st, sh, IOC3_ENET_RBR_L, (uint32_t)sc->sc_rxptrdma);

	sc->sc_rxci = 0;
	sc->sc_rxpi = sc->sc_nrxdesc + 1;
	bus_space_write_4(st, sh, IOC3_ENET_RCIR,
	    sc->sc_rxci * sizeof(uint64_t));
	bus_space_write_4(st, sh, IOC3_ENET_RPIR,
	    (sc->sc_rxpi * sizeof(uint64_t)) | IOC3_ENET_PIR_SET);

	/* Interrupt as soon as available RX desc reach this limit */
	bus_space_write_4(st, sh, IOC3_ENET_RCSR,
	    sc->sc_rxpi - sc->sc_rxci - 1);
	/* Set up RX timer to interrupt immediately upon reception. */
	bus_space_write_4(st, sh, IOC3_ENET_RTR, 0);

	/* Initialize RX buffers. */
	for (i = 0; i < sc->sc_nrxdesc; i++) {
		rxd = &sc->sc_rxdesc[i];
		rxd->rxd_stat = 0;
		IEC_RXSTATSYNC(sc, i, BUS_DMASYNC_PREREAD);
		IEC_RXBUFSYNC(sc, i, ETHER_MAX_LEN, BUS_DMASYNC_PREREAD);
	}

	/* Enable DMA, and RX and TX interrupts */
	sc->sc_mcr &= IOC3_ENET_MCR_LARGE_SSRAM | IOC3_ENET_MCR_PARITY_ENABLE |
	    IOC3_ENET_MCR_PADEN;
	sc->sc_mcr |= IOC3_ENET_MCR_TX_DMA | IOC3_ENET_MCR_TX |
	    IOC3_ENET_MCR_RX_DMA | IOC3_ENET_MCR_RX |
	    ((IEC_RXD_BUFOFFSET >> 1) << IOC3_ENET_MCR_RXOFF_SHIFT);
	bus_space_write_4(st, sh, IOC3_ENET_MCR, sc->sc_mcr);
	bus_space_write_4(st, sh, IOC3_ENET_IER,
	    IOC3_ENET_ISR_RX_TIMER | IOC3_ENET_ISR_RX_THRESHOLD |
	    (IOC3_ENET_ISR_TX_ALL & ~IOC3_ENET_ISR_TX_EMPTY));
	(void)bus_space_read_4(st, sh, IOC3_ENET_IER);

	timeout_add_sec(&sc->sc_tick, 1);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	iec_start(ifp);

	mii_mediachg(&sc->sc_mii);

	return 0;
}

void
iec_reset(struct iec_softc *sc)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint64_t address;
	int i;

	DPRINTF(IEC_DEBUG_RESET, ("iec_reset\n"));

	/* Reset chip. */
	bus_space_write_4(st, sh, IOC3_ENET_MCR, IOC3_ENET_MCR_RESET);
	delay(1000);
	bus_space_write_4(st, sh, IOC3_ENET_MCR, 0);
	delay(1000);

	bus_space_write_4(st, sh, IOC3_ENET_RBAR, 0);

	/* Set Ethernet address. */
	address = 0;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		address <<= 8;
		address += sc->sc_ac.ac_enaddr[ETHER_ADDR_LEN - 1 - i];
	}
	bus_space_write_4(st, sh, IOC3_ENET_MAR_H, address >> 32);
	bus_space_write_4(st, sh, IOC3_ENET_MAR_L, (uint32_t)address);

	/* Default to 100/half and let auto-negotiation work its magic. */
	bus_space_write_4(st, sh, IOC3_ENET_TCSR, IOC3_ENET_TCSR_HALF_DUPLEX);

	/* Reset collisions counter */
	(void)bus_space_read_4(st, sh, IOC3_ENET_TCDC);

	bus_space_write_4(st, sh, IOC3_ENET_RSR, 0x4d696f64);

	bus_space_write_4(st, sh, IOC3_ENET_HAR_H, 0);
	bus_space_write_4(st, sh, IOC3_ENET_HAR_L, 0);
}

void
iec_start(struct ifnet *ifp)
{
	struct iec_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct iec_txdesc *txd;
	struct iec_txsoft *txs;
	bus_dmamap_t dmamap;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint64_t txdaddr;
	int error, firstdirty, nexttx, opending;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous txpending and the first transmit descriptor.
	 */
	opending = sc->sc_txpending;
	firstdirty = IEC_NEXTTX(sc->sc_txlast);

	DPRINTF(IEC_DEBUG_START, ("iec_start: opending = %d, firstdirty = %d\n",
	    opending, firstdirty));

	for (;;) {
		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (sc->sc_txpending == IEC_NTXDESC)
			break;

		/*
		 * Get the next available transmit descriptor.
		 */
		nexttx = IEC_NEXTTX(sc->sc_txlast);
		txd = &sc->sc_txdesc[nexttx];
		txs = &sc->sc_txsoft[nexttx];

		len = m0->m_pkthdr.len;

		DPRINTF(IEC_DEBUG_START,
		    ("iec_start: len = %d, nexttx = %d\n", len, nexttx));

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (len <= IEC_TXD_BUFSIZE) {
			/*
			 * If the packet is small enough,
			 * just copy it to the buffer in txdesc and
			 * pad with zeroes.
			 */
			DPRINTF(IEC_DEBUG_START, ("iec_start: short packet\n"));

			m_copydata(m0, 0, m0->m_pkthdr.len, txd->txd_buf);
			if (len < ETHER_PAD_LEN) {
				/*
				 * XXX would IOC3_ENET_MCR_PADEN in MCR do this
				 * XXX for us?
				 */
				memset(txd->txd_buf + len, 0,
				    ETHER_PAD_LEN - len);
				len = ETHER_PAD_LEN;
			}

			txs->txs_flags = IEC_TXCMD_BUF_V;
		} else {
			/*
			 * Although the packet may fit in the txdesc
			 * itself, we do not make use of this option,
			 * and use the two data pointers to handle it.
			 * There are two pointers because each chunk must
			 * not cross a 16KB boundary.
	                 */
			dmamap = txs->txs_dmamap;
			if (bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
			    BUS_DMA_WRITE | BUS_DMA_NOWAIT) != 0) {
				struct mbuf *m;

				DPRINTF(IEC_DEBUG_START,
				    ("iec_start: re-allocating mbuf\n"));
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					printf("%s: unable to allocate "
					    "TX mbuf\n", sc->sc_dev.dv_xname);
					break;
				}
				if (len > (MHLEN - ETHER_ALIGN)) {
					MCLGET(m, M_DONTWAIT);
					if ((m->m_flags & M_EXT) == 0) {
						printf("%s: unable to allocate "
						    "TX cluster\n",
						    sc->sc_dev.dv_xname);
						m_freem(m);
						break;
					}
				}
				/*
				 * Each packet has the Ethernet header, so
				 * in many cases the header isn't 4-byte aligned
				 * and data after the header is 4-byte aligned.
				 * Thus adding 2-byte offset before copying to
				 * new mbuf avoids unaligned copy and this may
				 * improve performance.
				 */
				m->m_data += ETHER_ALIGN;
				m_copydata(m0, 0, len, mtod(m, caddr_t));
				m->m_pkthdr.len = m->m_len = len;
				m_freem(m0);
				m0 = m;
				error = bus_dmamap_load_mbuf(sc->sc_dmat,
				    dmamap, m, BUS_DMA_WRITE | BUS_DMA_NOWAIT);
				if (error) {
					printf("%s: unable to load TX buffer, "
					    "error = %d\n",
					    sc->sc_dev.dv_xname, error);
					m_freem(m);
					break;
				}
			}

			txdaddr = dmamap->dm_segs[0].ds_addr;
			txs->txs_flags = IEC_TXCMD_PTR0_V;
			DPRINTF(IEC_DEBUG_START,
			    ("iec_start: ds_addr = %p\n",
			    dmamap->dm_segs[0].ds_addr));

			DPRINTF(IEC_DEBUG_START,
			    ("iec_start: txdaddr = %p, len = %d\n",
			    txdaddr, len));

			/*
			 * Sync the DMA map for TX mbuf.
			 *
			 * XXX unaligned part doesn't have to be sync'ed,
			 *     but it's harmless...
			 */
			bus_dmamap_sync(sc->sc_dmat, dmamap, 0,
			    dmamap->dm_mapsize,	BUS_DMASYNC_PREWRITE);
		}

#if NBPFILTER > 0
		/*
		 * Pass packet to bpf if there is a listener.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif

		/*
		 * Setup the transmit descriptor.
		 * Since there is apparently no way to get status
		 * information in the TX descriptor after we issue
		 * it, we have to always ask for interrupts upon
		 * completion, so that if we get a TX error interrupt,
		 * we know which descriptor it applies to.
		 */

		txd->txd_cmd = IEC_TXCMD_TXINT | txs->txs_flags |
		    (len & IEC_TXCMD_DATALEN);

		if (txs->txs_flags & IEC_TXCMD_PTR0_V) {
			uint32_t r1, r2;

			/*
			 * The chip DMA engine can not cross 16KB boundaries.
			 * If our mbuf doesn't fit, use the second pointer.
			 */

			r1 = IEC_DMA_BOUNDARY -
			    (txdaddr & (IEC_DMA_BOUNDARY - 1));
			if (r1 >= len) {
				/* only one chunk is necessary */
				r1 = len;
				r2 = 0;
			} else
				r2 = len - r1;

			txd->txd_ptr[0] = txdaddr;
			if (r2 != 0) {
				txs->txs_flags |= IEC_TXCMD_PTR1_V;
				txd->txd_ptr[1] = txdaddr + r1;
			} else
				txd->txd_ptr[1] = 0;
			txd->txd_len = (r1 << IECTX_BUF1_LEN_SHIFT) |
			    (r2 << IECTX_BUF2_LEN_SHIFT);

			/*
			 * Store a pointer to the packet so we can
			 * free it later.
			 */
			txs->txs_mbuf = m0;
		} else {
			txd->txd_len = len << IECTX_BUF0_LEN_SHIFT;
			txd->txd_ptr[0] = 0;
			txd->txd_ptr[1] = 0;
			/*
			 * In this case all data are copied to buffer in txdesc,
			 * we can free TX mbuf here.
			 */
			m_freem(m0);
		}

		DPRINTF(IEC_DEBUG_START,
		    ("iec_start: txd_cmd = 0x%08x, txd_ptr = %p\n",
		    txd->txd_cmd, txd->txd_ptr[0]));
		DPRINTF(IEC_DEBUG_START,
		    ("iec_start: len = %d (0x%04x)\n", len, len));

		/* Sync TX descriptor. */
		IEC_TXDESCSYNC(sc, nexttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Advance the TX pointer. */
		sc->sc_txpending++;
		sc->sc_txlast = nexttx;
	}

	if (sc->sc_txpending == IEC_NTXDESC) {
		/* No more slots; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txpending != opending) {
		/* Start TX. */
		bus_space_write_4(st, sh, IOC3_ENET_TPIR,
		    IEC_NEXTTX(sc->sc_txlast) * IEC_TXDESCSIZE);

		/*
		 * If the transmitter was idle,
		 * reset the txdirty pointer and re-enable TX interrupt.
		 */
		if (opending == 0) {
			sc->sc_txdirty = firstdirty;
			bus_space_write_4(st, sh, IOC3_ENET_IER,
			    bus_space_read_4(st, sh, IOC3_ENET_IER) |
			    IOC3_ENET_ISR_TX_EMPTY);
			(void)bus_space_read_4(st, sh, IOC3_ENET_IER);
		}

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

void
iec_stop(struct ifnet *ifp)
{
	struct iec_softc *sc = ifp->if_softc;
	struct iec_txsoft *txs;
	int i;

	DPRINTF(IEC_DEBUG_STOP, ("iec_stop\n"));

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	timeout_del(&sc->sc_tick);
	mii_down(&sc->sc_mii);

	/* Disable DMA and interrupts. */
	sc->sc_mcr &= ~(IOC3_ENET_MCR_TX_DMA | IOC3_ENET_MCR_RX_DMA);
	bus_space_write_4(sc->sc_st, sc->sc_sh, IOC3_ENET_MCR, sc->sc_mcr);
	bus_space_write_4(sc->sc_st, sc->sc_sh, IOC3_ENET_IER, 0);
	(void)bus_space_read_4(sc->sc_st, sc->sc_sh, IOC3_ENET_IER);

	/* Release any TX buffers. */
	for (i = 0; i < IEC_NTXDESC; i++) {
		txs = &sc->sc_txsoft[i];
		if ((txs->txs_flags & IEC_TXCMD_PTR0_V) != 0) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
	}
}

int
iec_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iec_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			iec_init(ifp);
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				iec_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iec_stop(ifp);
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			iec_iff(sc);
		error = 0;
	}

	splx(s);
	return error;
}

void
iec_watchdog(struct ifnet *ifp)
{
	struct iec_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;

	DPRINTF(IEC_DEBUG_INTR, ("iec_watchdog: ISR %08x IER %08x\n",
	    bus_space_read_4(sc->sc_st, sc->sc_sh, IOC3_ENET_ISR),
	    bus_space_read_4(sc->sc_st, sc->sc_sh, IOC3_ENET_IER)));

	iec_init(ifp);
}

void
iec_tick(void *arg)
{
	struct iec_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add_sec(&sc->sc_tick, 1);
}

void
iec_iff(struct iec_softc *sc)
{
	struct arpcom *ac = &sc->sc_ac;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint64_t mchash = 0;
	uint32_t hash;

	sc->sc_mcr &= ~IOC3_ENET_MCR_PROMISC;
	ifp->if_flags &= ~IFF_ALLMULTI;

	if (ifp->if_flags & IFF_PROMISC || ac->ac_multirangecnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			sc->sc_mcr |= IOC3_ENET_MCR_PROMISC;
		mchash = 0xffffffffffffffffULL;
	} else {
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			hash = ether_crc32_be(enm->enm_addrlo,
			    ETHER_ADDR_LEN) >> 26;

			mchash |= 1 << hash;

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	bus_space_write_4(st, sh, IOC3_ENET_HAR_H, mchash >> 32);
	bus_space_write_4(st, sh, IOC3_ENET_HAR_L, (uint32_t)mchash);
	bus_space_write_4(st, sh, IOC3_ENET_MCR, sc->sc_mcr);
}

struct mbuf *
iec_get(struct iec_softc *sc, uint8_t *data, size_t datalen)
{
	struct mbuf *m, **mp, *head;
	size_t len, pad;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		printf("%s: unable to allocate RX mbuf\n",
		    sc->sc_dev.dv_xname);
		return NULL;
	}

	m->m_pkthdr.rcvif = &sc->sc_ac.ac_if;
	m->m_pkthdr.len = datalen;

	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	m->m_data += pad;
	len = MHLEN - pad;
	head = NULL;
	mp = &head;

	while (datalen != 0) {
		if (head != NULL) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate RX mbuf\n",
				    sc->sc_dev.dv_xname);
				m_freem(head);
				return NULL;
			}
			len = MHLEN;
		}
		if (datalen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				printf("%s: unable to allocate RX cluster\n",
				    sc->sc_dev.dv_xname);
				if (head != NULL)
					m_freem(head);
				return NULL;
			}
			len = MCLBYTES;
			if (head == NULL) {
				m->m_data += pad;
				len -= pad;
			}
		}
		m->m_len = len = min(datalen, len);
		memcpy(mtod(m, caddr_t), data, len);
		data += len;
		datalen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return head;
}

int
iec_intr(void *arg)
{
	struct iec_softc *sc = arg;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t statreg, statack;
	int handled, sent;

	DPRINTF(IEC_DEBUG_INTR, ("iec_intr: called\n"));

	handled = sent = 0;

	for (;;) {
		statreg = bus_space_read_4(st, sh, IOC3_ENET_ISR);

		DPRINTF(IEC_DEBUG_INTR,
		    ("iec_intr: INT_STAT = 0x%x\n", statreg));

		statack = statreg & bus_space_read_4(st, sh, IOC3_ENET_IER);
		if (statack == 0)
			break;
		bus_space_write_4(st, sh, IOC3_ENET_ISR, statack);

		handled = 1;

		if (statack &
		    (IOC3_ENET_ISR_RX_TIMER | IOC3_ENET_ISR_RX_THRESHOLD)) {
			iec_rxintr(sc, statreg);
		}

		if (statack & IOC3_ENET_ISR_TX_ALL) {
			iec_txintr(sc, statreg & IOC3_ENET_ISR_TX_ALL);
			sent = 1;
		}
	}

	if (sent) {
		/* Try to get more packets going. */
		iec_start(ifp);
	}

	return handled;
}

void
iec_rxintr(struct iec_softc *sc, uint32_t stat)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf *m;
	struct iec_rxdesc *rxd;
	uint64_t rxstat;
	size_t len;
	u_int packets;
	uint32_t i, ci;

	/*
	 * Figure out how many RX descriptors are available for processing.
	 */
	ci = bus_space_read_4(st, sh, IOC3_ENET_RCIR) / sizeof(uint64_t);
	packets = (ci + IEC_NRXDESC_MAX - sc->sc_rxci) % IEC_NRXDESC_MAX;
	sc->sc_rxpi = (sc->sc_rxpi + packets) % IEC_NRXDESC_MAX;

	DPRINTF(IEC_DEBUG_RXINTR, ("iec_rxintr: rx %d-%d\n", sc->sc_rxci, ci));
	while (packets-- != 0) {
		i = (sc->sc_rxci++) & (sc->sc_nrxdesc - 1);

		IEC_RXSTATSYNC(sc, i, BUS_DMASYNC_POSTREAD);
		rxd = &sc->sc_rxdesc[i];
		rxstat = rxd->rxd_stat;

		DPRINTF(IEC_DEBUG_RXINTR,
		    ("iec_rxintr: rxstat = 0x%08x, rxerr = 0x%08x\n",
		    rxstat, rxd->rxd_err));

		if ((rxstat & IEC_RXSTAT_VALID) == 0)
			goto dropit;

		len = (rxstat & IEC_RXSTAT_LEN_MASK) >> IEC_RXSTAT_LEN_SHIFT;

		if (len < ETHER_MIN_LEN ||
		    len > ETHER_MAX_LEN) {
			/* Invalid length packet; drop it. */
			DPRINTF(IEC_DEBUG_RXINTR,
			    ("iec_rxintr: wrong packet\n"));
dropit:
			ifp->if_ierrors++;
			rxd->rxd_stat = 0;
			IEC_RXSTATSYNC(sc, i, BUS_DMASYNC_PREREAD);
			continue;
		}

		if (rxd->rxd_err & (IEC_RXERR_BADPACKET | IEC_RXERR_LONGEVENT |
		     IEC_RXERR_INVPREAMB | IEC_RXERR_CODE | IEC_RXERR_FRAME |
		     IEC_RXERR_CRC)) {
			printf("%s: iec_rxintr: stat = 0x%08llx err = %08x\n",
			    sc->sc_dev.dv_xname, rxstat, rxd->rxd_err);
			goto dropit;
		}

		/*
		 * Now allocate an mbuf (and possibly a cluster) to hold
		 * the received packet.
		 */
		len -= ETHER_CRC_LEN;
		IEC_RXBUFSYNC(sc, i, len, BUS_DMASYNC_POSTREAD);
		m = iec_get(sc, rxd->rxd_buf, len);
		IEC_RXBUFSYNC(sc, i, ETHER_MAX_LEN, BUS_DMASYNC_PREREAD);
		if (m == NULL)
			goto dropit;

		rxd->rxd_stat = 0;
		IEC_RXSTATSYNC(sc, i, BUS_DMASYNC_PREREAD);

		ifp->if_ipackets++;

#if NBPFILTER > 0
		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack if it is for us.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		/* Pass it on. */
		ether_input_mbuf(ifp, m);
	}

	/* Update RX index pointers. */
	sc->sc_rxci = ci;
	bus_space_write_4(st, sh, IOC3_ENET_RPIR,
	    (sc->sc_rxpi * sizeof(uint64_t)) | IOC3_ENET_PIR_SET);
	DPRINTF(IEC_DEBUG_RXINTR, ("iec_rxintr: new rxci %d rxpi %d\n",
	    sc->sc_rxci, sc->sc_rxpi));
}

void
iec_txintr(struct iec_softc *sc, uint32_t stat)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct iec_txsoft *txs;
	bus_dmamap_t dmamap;
	uint32_t tcir;
	int i, once, last;

	ifp->if_flags &= ~IFF_OACTIVE;

	tcir = bus_space_read_4(st, sh, IOC3_ENET_TCIR) & ~IOC3_ENET_TCIR_IDLE;
	last = (tcir / IEC_TXDESCSIZE) % IEC_NTXDESC_MAX;

	DPRINTF(IEC_DEBUG_TXINTR, ("iec_txintr: dirty %d last %d\n",
	    sc->sc_txdirty, last));
	once = 0;
	for (i = sc->sc_txdirty; i != last && sc->sc_txpending != 0;
	    i = IEC_NEXTTX(i), sc->sc_txpending--) {
		txs = &sc->sc_txsoft[i];
		if ((txs->txs_flags & IEC_TXCMD_PTR0_V) != 0) {
			dmamap = txs->txs_dmamap;
			bus_dmamap_sync(sc->sc_dmat, dmamap, 0,
			    dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}

		if ((stat & IOC3_ENET_ISR_TX_EXPLICIT) == 0) {
			if (stat == IOC3_ENET_ISR_TX_EMPTY)
				continue;
			if (once == 0) {
				printf("%s: TX error: txstat = %08x\n",
				    DEVNAME(sc), stat);
				once = 1;
			}
			ifp->if_oerrors++;
		} else {
			ifp->if_collisions += IOC3_ENET_TCDC_COLLISION_MASK &
			    bus_space_read_4(st, sh, IOC3_ENET_TCDC);
			ifp->if_opackets++;
		}
	}

	/* Update the dirty TX buffer pointer. */
	sc->sc_txdirty = i;
	DPRINTF(IEC_DEBUG_INTR,
	    ("iec_txintr: sc_txdirty = %2d, sc_txpending = %2d\n",
	    sc->sc_txdirty, sc->sc_txpending));

	/* Cancel the watchdog timer if there are no pending TX packets. */
	if (sc->sc_txpending == 0)
		ifp->if_timer = 0;

	if (stat & IOC3_ENET_ISR_TX_EMPTY) {
		bus_space_write_4(st, sh, IOC3_ENET_IER,
		    bus_space_read_4(st, sh, IOC3_ENET_IER) &
		      ~IOC3_ENET_ISR_TX_EMPTY);
		(void)bus_space_read_4(st, sh, IOC3_ENET_IER);
	}
}

int
iec_ssram_probe(struct iec_softc *sc)
{
	/*
	 * Depending on the hardware, there is either 64KB or 128KB of
	 * 16-bit SSRAM, which is used as internal RX buffers by the chip.
	 */

	/* default to large size */
	sc->sc_mcr = IOC3_ENET_MCR_PARITY_ENABLE | IOC3_ENET_MCR_LARGE_SSRAM;
	bus_space_write_4(sc->sc_st, sc->sc_sh, IOC3_ENET_MCR, sc->sc_mcr);

	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    IOC3_SSRAM_BASE, 0x55aa);
	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    IOC3_SSRAM_BASE + IOC3_SSRAM_SMALL_SIZE, 0xffff ^ 0x55aa);

	if ((bus_space_read_4(sc->sc_st, sc->sc_sh, IOC3_SSRAM_BASE) &
	     IOC3_SSRAM_DATA_MASK) != 0x55aa ||
	    (bus_space_read_4(sc->sc_st, sc->sc_sh,
	     IOC3_SSRAM_BASE + IOC3_SSRAM_SMALL_SIZE) != (0xffff ^ 0x55aa))) {
		sc->sc_mcr &= ~IOC3_ENET_MCR_LARGE_SSRAM;
		sc->sc_nrxdesc = IEC_NRXDESC_SMALL;
		return IOC3_SSRAM_SMALL_SIZE / 2 / 1024;
	} else {
		sc->sc_nrxdesc = IEC_NRXDESC_LARGE;
		return IOC3_SSRAM_LARGE_SIZE / 2 / 1024;
	}
}
