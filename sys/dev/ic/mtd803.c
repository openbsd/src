/* $OpenBSD: mtd803.c,v 1.2 2003/08/19 04:03:53 mickey Exp $ */
/* $NetBSD: mtd803.c,v 1.3 2003/07/14 15:47:12 lukem Exp $ */

/*-
 *
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Peter Bex <Peter.Bex@student.kun.nl>.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
 * TODO:
 * - Most importantly, get some bus_dmamap_syncs in the correct places.
 *    I don't have access to a computer with PCI other than i386, and i386
 *    is just such a machine where dmamap_syncs don't do anything.
 * - Powerhook for when resuming after standby.
 * - Watchdog stuff doesn't work yet, the system crashes.(lockmgr: no context)
 * - There seems to be a CardBus version of the card. (see datasheet)
 *    Perhaps a detach function is necessary then? (free buffs, stop rx/tx etc)
 * - When you enable the TXBUN (Tx buffer unavailable) interrupt, it gets
 *    raised every time a packet is sent. Strange, since everything works anyway
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/ic/mtd803reg.h>
#include <dev/ic/mtd803var.h>

/*
 * Device driver for the MTD803 3-in-1 Fast Ethernet Controller
 * Written by Peter Bex (peter.bex@student.kun.nl)
 *
 * Datasheet at:   http://www.myson.com.tw   or   http://www.century-semi.com
 */

#define MTD_READ_1(sc, reg) \
	bus_space_read_1((sc)->bus_tag, (sc)->bus_handle, (reg))
#define MTD_WRITE_1(sc, reg, data) \
	bus_space_write_1((sc)->bus_tag, (sc)->bus_handle, (reg), (data))

#define MTD_READ_2(sc, reg) \
	bus_space_read_2((sc)->bus_tag, (sc)->bus_handle, (reg))
#define MTD_WRITE_2(sc, reg, data) \
	bus_space_write_2((sc)->bus_tag, (sc)->bus_handle, (reg), (data))

#define MTD_READ_4(sc, reg) \
	bus_space_read_4((sc)->bus_tag, (sc)->bus_handle, (reg))
#define MTD_WRITE_4(sc, reg, data) \
	bus_space_write_4((sc)->bus_tag, (sc)->bus_handle, (reg), (data))

#define MTD_SETBIT(sc, reg, x) \
	MTD_WRITE_4((sc), (reg), MTD_READ_4((sc), (reg)) | (x))
#define MTD_CLRBIT(sc, reg, x) \
	MTD_WRITE_4((sc), (reg), MTD_READ_4((sc), (reg)) & ~(x))

#define ETHER_CRC32(buf, len)	(ether_crc32_be((buf), (len)))

int mtd_mii_readreg __P((struct device *, int, int));
void mtd_mii_writereg __P((struct device *, int, int, int));
void mtd_mii_statchg __P((struct device *));

void mtd_start __P((struct ifnet *));
void mtd_stop __P((struct ifnet *, int));
int mtd_ioctl __P((struct ifnet *, u_long, caddr_t));
void mtd_setmulti __P((struct mtd_softc *));
void mtd_watchdog __P((struct ifnet *));
int mtd_mediachange __P((struct ifnet *));
void mtd_mediastatus __P((struct ifnet *, struct ifmediareq *));

int mtd_init __P((struct ifnet *));
void mtd_reset __P((struct mtd_softc *));
void mtd_shutdown __P((void *));
int mtd_init_desc __P((struct mtd_softc *));
int mtd_put __P((struct mtd_softc *, int, struct mbuf *));
struct mbuf *mtd_get __P((struct mtd_softc *, int, int));

int mtd_rxirq __P((struct mtd_softc *));
int mtd_txirq __P((struct mtd_softc *));
int mtd_bufirq __P((struct mtd_softc *));


int
mtd_config(sc)
	struct mtd_softc *sc;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int i;

	/* Read station address */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = MTD_READ_1(sc, MTD_PAR0 + i);
	printf(" address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

	if (mtd_init_desc(sc))
		return (1);

	/* Initialize ifnet structure */
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = mtd_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = mtd_start;
	ifp->if_watchdog = mtd_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/* Setup MII interface */
	sc->mii.mii_ifp = ifp;
	sc->mii.mii_readreg = mtd_mii_readreg;
	sc->mii.mii_writereg = mtd_mii_writereg;
	sc->mii.mii_statchg = mtd_mii_statchg;
	ifmedia_init(&sc->mii.mii_media, 0, mtd_mediachange,
	    mtd_mediastatus);
	mii_attach(&sc->dev, &sc->mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->mii.mii_phys) == NULL) {
		ifmedia_add(&sc->mii.mii_media, IFM_ETHER | IFM_NONE, 0,
		    NULL);
		ifmedia_set(&sc->mii.mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&sc->mii.mii_media, IFM_ETHER | IFM_AUTO);

	/* Attach interface */
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Add shutdown hook to reset card when we reboot */
	sc->sd_hook = shutdownhook_establish(mtd_shutdown, sc);

	return (0);
}


/*
 * mtd_init
 * Must be called at splnet()
 */
int
mtd_init(ifp)
	struct ifnet *ifp;
{
	struct mtd_softc *sc = ifp->if_softc;

	mtd_reset(sc);

	/*
	 * Set cache alignment and burst length. Don't really know what these
	 * mean, so their values are probably suboptimal.
	 */
	MTD_WRITE_4(sc, MTD_BCR, MTD_BCR_BLEN16);

	MTD_WRITE_4(sc, MTD_RXTXR, MTD_TX_STFWD | MTD_RX_BLEN | MTD_RX_512
			| MTD_TX_FDPLX);

	/* Promiscuous mode? */
	if (ifp->if_flags & IFF_PROMISC)
		MTD_SETBIT(sc, MTD_RXTXR, MTD_RX_PROM);
	else
		MTD_CLRBIT(sc, MTD_RXTXR, MTD_RX_PROM);

	/* Broadcast mode? */
	if (ifp->if_flags & IFF_BROADCAST)
		MTD_SETBIT(sc, MTD_RXTXR, MTD_RX_ABROAD);
	else
		MTD_CLRBIT(sc, MTD_RXTXR, MTD_RX_ABROAD);

	mtd_setmulti(sc);

	/* Enable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, MTD_IMR_MASK);
	MTD_WRITE_4(sc, MTD_ISR, MTD_ISR_ENABLE);

	/* Set descriptor base addresses */
	MTD_WRITE_4(sc, MTD_TXLBA, htole32(sc->desc_dma_map->dm_segs[0].ds_addr
				+ sizeof(struct mtd_desc) * MTD_NUM_RXD));
	MTD_WRITE_4(sc, MTD_RXLBA,
		htole32(sc->desc_dma_map->dm_segs[0].ds_addr));

	/* Enable receiver and transmitter */
	MTD_SETBIT(sc, MTD_RXTXR, MTD_RX_ENABLE);
	MTD_SETBIT(sc, MTD_RXTXR, MTD_TX_ENABLE);

	/* Interface is running */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return 0;
}


int
mtd_init_desc(sc)
	struct mtd_softc *sc;
{
	int rseg, err, i;
	bus_dma_segment_t seg;
	bus_size_t size;

	/* Allocate memory for descriptors */
	size = (MTD_NUM_RXD + MTD_NUM_TXD) * sizeof(struct mtd_desc);

	/* Allocate DMA-safe memory */
	if ((err = bus_dmamem_alloc(sc->dma_tag, size, MTD_DMA_ALIGN,
			 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate DMA buffer, error = %d\n",
			sc->dev.dv_xname, err);
		return 1;
	}

	/* Map memory to kernel addressable space */
	if ((err = bus_dmamem_map(sc->dma_tag, &seg, 1, size,
		(caddr_t *)&sc->desc, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map DMA buffer, error = %d\n",
			sc->dev.dv_xname, err);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Create a DMA map */
	if ((err = bus_dmamap_create(sc->dma_tag, size, 1,
		size, 0, BUS_DMA_NOWAIT, &sc->desc_dma_map)) != 0) {
		printf("%s: unable to create DMA map, error = %d\n",
			sc->dev.dv_xname, err);
		bus_dmamem_unmap(sc->dma_tag, (caddr_t)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Load the DMA map */
	if ((err = bus_dmamap_load(sc->dma_tag, sc->desc_dma_map, sc->desc,
		size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load DMA map, error = %d\n",
			sc->dev.dv_xname, err);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (caddr_t)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Allocate memory for the buffers */
	size = MTD_NUM_RXD * MTD_RXBUF_SIZE + MTD_NUM_TXD * MTD_TXBUF_SIZE;

	/* Allocate DMA-safe memory */
	if ((err = bus_dmamem_alloc(sc->dma_tag, size, MTD_DMA_ALIGN,
			 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate DMA buffer, error = %d\n",
			sc->dev.dv_xname, err);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (caddr_t)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Map memory to kernel addressable space */
	if ((err = bus_dmamem_map(sc->dma_tag, &seg, 1, size,
		&sc->buf, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map DMA buffer, error = %d\n",
			sc->dev.dv_xname, err);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (caddr_t)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Create a DMA map */
	if ((err = bus_dmamap_create(sc->dma_tag, size, 1,
		size, 0, BUS_DMA_NOWAIT, &sc->buf_dma_map)) != 0) {
		printf("%s: unable to create DMA map, error = %d\n",
			sc->dev.dv_xname, err);
		bus_dmamem_unmap(sc->dma_tag, sc->buf, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (caddr_t)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Load the DMA map */
	if ((err = bus_dmamap_load(sc->dma_tag, sc->buf_dma_map, sc->buf,
		size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load DMA map, error = %d\n",
			sc->dev.dv_xname, err);
		bus_dmamap_destroy(sc->dma_tag, sc->buf_dma_map);
		bus_dmamem_unmap(sc->dma_tag, sc->buf, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);

		/* Undo DMA map for descriptors */
		bus_dmamap_unload(sc->dma_tag, sc->desc_dma_map);
		bus_dmamap_destroy(sc->dma_tag, sc->desc_dma_map);
		bus_dmamem_unmap(sc->dma_tag, (caddr_t)sc->desc, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 1;
	}

	/* Descriptors are stored as a circular linked list */
	/* Fill in rx descriptors */
	for (i = 0; i < MTD_NUM_RXD; ++i) {
		sc->desc[i].stat = MTD_RXD_OWNER;
		if (i == MTD_NUM_RXD - 1) {	/* Last descriptor */
			/* Link back to first rx descriptor */
			sc->desc[i].next =
				htole32(sc->desc_dma_map->dm_segs[0].ds_addr);
		} else {
			/* Link forward to next rx descriptor */
			sc->desc[i].next =
			htole32(sc->desc_dma_map->dm_segs[0].ds_addr
					+ (i + 1) * sizeof(struct mtd_desc));
		}
		sc->desc[i].conf = MTD_RXBUF_SIZE & MTD_RXD_CONF_BUFS;
		/* Set buffer's address */
		sc->desc[i].data = htole32(sc->buf_dma_map->dm_segs[0].ds_addr
					+ i * MTD_RXBUF_SIZE);
	}

	/* Fill in tx descriptors */
	for (/* i = MTD_NUM_RXD */; i < (MTD_NUM_TXD + MTD_NUM_RXD); ++i) {
		sc->desc[i].stat = 0;	/* At least, NOT MTD_TXD_OWNER! */
		if (i == (MTD_NUM_RXD + MTD_NUM_TXD - 1)) {	/* Last descr */
			/* Link back to first tx descriptor */
			sc->desc[i].next =
				htole32(sc->desc_dma_map->dm_segs[0].ds_addr
					+MTD_NUM_RXD * sizeof(struct mtd_desc));
		} else {
			/* Link forward to next tx descriptor */
			sc->desc[i].next =
				htole32(sc->desc_dma_map->dm_segs[0].ds_addr
					+ (i + 1) * sizeof(struct mtd_desc));
		}
		/* sc->desc[i].conf = MTD_TXBUF_SIZE & MTD_TXD_CONF_BUFS; */
		/* Set buffer's address */
		sc->desc[i].data = htole32(sc->buf_dma_map->dm_segs[0].ds_addr
					+ MTD_NUM_RXD * MTD_RXBUF_SIZE
					+ (i - MTD_NUM_RXD) * MTD_TXBUF_SIZE);
	}

	return 0;
}


void
mtd_mii_statchg(self)
	struct device *self;
{
	/*struct mtd_softc *sc = (void *)self;*/

	/* Should we do something here? :) */
}


int
mtd_mii_readreg(self, phy, reg)
	struct device *self;
	int phy, reg;
{
	struct mtd_softc *sc = (void *)self;

	/* XXX */
	if (phy != 0)
		return (0);

	return (MTD_READ_2(sc, MTD_PHYBASE + reg * 2));
}


void
mtd_mii_writereg(self, phy, reg, val)
	struct device *self;
	int phy, reg, val;
{
	struct mtd_softc *sc = (void *)self;

	MTD_WRITE_2(sc, MTD_PHYBASE + reg * 2, val);
}


int
mtd_put(sc, index, m)
	struct mtd_softc *sc;
	int index;
	struct mbuf *m;
{
	int len, tlen;
	caddr_t buf = sc->buf + MTD_NUM_RXD * MTD_RXBUF_SIZE
			+ index * MTD_TXBUF_SIZE;
	struct mbuf *n;

	for (tlen = 0; m != NULL; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		} else if (tlen > MTD_TXBUF_SIZE) {
			/* XXX FIXME: No idea what to do here. */
			printf("%s: packet too large!\n",
				sc->dev.dv_xname);
			MFREE(m, n);
			continue;
		}
		memcpy(buf, mtod(m, caddr_t), len);
		buf += len;
		tlen += len;
		MFREE(m, n);
	}
	sc->desc[MTD_NUM_RXD + index].conf = MTD_TXD_CONF_PAD | MTD_TXD_CONF_CRC
		| MTD_TXD_CONF_IRQC
		| ((tlen << MTD_TXD_PKTS_SHIFT) & MTD_TXD_CONF_PKTS)
		| (tlen & MTD_TXD_CONF_BUFS);

	return tlen;
}


void
mtd_start(ifp)
	struct ifnet *ifp;
{
	struct mtd_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int len;
	int first_tx = sc->cur_tx;

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL)
			break;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* Copy mbuf chain into tx buffer */
		len = mtd_put(sc, sc->cur_tx, m);

		if (sc->cur_tx != first_tx)
			sc->desc[MTD_NUM_RXD + sc->cur_tx].stat = MTD_TXD_OWNER;

		if (++sc->cur_tx >= MTD_NUM_TXD)
			sc->cur_tx = 0;
	}
	/* Mark first & last descriptor */
	sc->desc[MTD_NUM_RXD + first_tx].conf |= MTD_TXD_CONF_FSD;

	if (sc->cur_tx == 0) {
		sc->desc[MTD_NUM_RXD + MTD_NUM_TXD - 1].conf |=MTD_TXD_CONF_LSD;
	} else {
		sc->desc[MTD_NUM_RXD + sc->cur_tx - 1].conf |= MTD_TXD_CONF_LSD;
	}

	/* Give first descriptor to chip to complete transaction */
	sc->desc[MTD_NUM_RXD + first_tx].stat = MTD_TXD_OWNER;

	/* Transmit polling demand */
	MTD_WRITE_4(sc, MTD_TXPDR, MTD_TXPDR_DEMAND);

	/* XXX FIXME: Set up a watchdog timer */
	/* ifp->if_timer = 5; */
}


void
mtd_stop (ifp, disable)
	struct ifnet *ifp;
	int disable;
{
	struct mtd_softc *sc = ifp->if_softc;

	/* Disable transmitter and receiver */
	MTD_CLRBIT(sc, MTD_RXTXR, MTD_TX_ENABLE);
	MTD_CLRBIT(sc, MTD_RXTXR, MTD_RX_ENABLE);

	/* Disable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, 0x00000000);

	/* Must do more at disable??... */
	if (disable) {
		/* Delete tx and rx descriptor base adresses */
		MTD_WRITE_4(sc, MTD_RXLBA, 0x00000000);
		MTD_WRITE_4(sc, MTD_TXLBA, 0x00000000);
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}


void
mtd_watchdog(ifp)
	struct ifnet *ifp;
{
	struct mtd_softc *sc = ifp->if_softc;
	int s;

	log(LOG_ERR, "%s: device timeout\n", sc->dev.dv_xname);
	++sc->arpcom.ac_if.if_oerrors;

	mtd_stop(ifp, 0);

	s = splnet();
	mtd_init(ifp);
	splx(s);

	return;
}


int
mtd_ioctl(ifp, cmd, data)
	struct ifnet * ifp;
	u_long cmd;
	caddr_t data;
{
	struct mtd_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splimp();
	if ((error = ether_ioctl(ifp, &sc->arpcom, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	/* Don't do anything special */
	switch(cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			mtd_init(ifp);
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif	/* INET */
		default:
			mtd_init(ifp);
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
		if (ifp->if_flags & IFF_UP)
			mtd_init(ifp);
		else
			if (ifp->if_flags & IFF_RUNNING)
				/* mtd_stop(ifp) */;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			mtd_setmulti(sc);
			 error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->mii.mii_media, cmd);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return error;
}


struct mbuf *
mtd_get(sc, index, totlen)
	struct mtd_softc *sc;
	int index;
	int totlen;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mbuf *m, *m0, *newm;
	int len;
	caddr_t buf = sc->buf + index * MTD_RXBUF_SIZE;

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return NULL;

	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = totlen;
	m = m0;
	len = MHLEN;

	while (totlen > 0) {
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (!(m->m_flags & M_EXT)) {
				m_freem(m0);
				return NULL;
			}
			len = MCLBYTES;
		}

		if (m == m0) {
			caddr_t newdata = (caddr_t)
				ALIGN(m->m_data + sizeof(struct ether_header)) -
				sizeof(struct ether_header);
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}

		m->m_len = len = min(totlen, len);
		memcpy(mtod(m, caddr_t), buf, len);
		buf += len;

		totlen -= len;
		if (totlen > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == NULL) {
				m_freem(m0);
				return NULL;
			}
			len = MLEN;
			m = m->m_next = newm;
		}
	}

	return m0;
}


int
mtd_rxirq(sc)
	struct mtd_softc *sc;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int len;
	struct mbuf *m;

	for (; !(sc->desc[sc->cur_rx].stat & MTD_RXD_OWNER);) {
		/* Error summary set? */
		if (sc->desc[sc->cur_rx].stat & MTD_RXD_ERRSUM) {
			printf("%s: received packet with errors\n",
				sc->dev.dv_xname); 
			/* Give up packet, since an error occurred */
			sc->desc[sc->cur_rx].stat = MTD_RXD_OWNER;
			sc->desc[sc->cur_rx].conf = MTD_RXBUF_SIZE &
							MTD_RXD_CONF_BUFS;
			++ifp->if_ierrors;
			if (++sc->cur_rx >= MTD_NUM_RXD)
				sc->cur_rx = 0;
			continue;
		}
		/* Get buffer length */
		len = (sc->desc[sc->cur_rx].stat & MTD_RXD_FLEN)
			>> MTD_RXD_FLEN_SHIFT;
		len -= ETHER_CRC_LEN;

		/* Check packet size */
		if (len <= sizeof(struct ether_header)) { 
			printf("%s: invalid packet size %d; dropping\n",
				sc->dev.dv_xname, len);
			sc->desc[sc->cur_rx].stat = MTD_RXD_OWNER;
			sc->desc[sc->cur_rx].conf = MTD_RXBUF_SIZE &
							MTD_RXD_CONF_BUFS;
			++ifp->if_ierrors;
			if (++sc->cur_rx >= MTD_NUM_RXD)
				sc->cur_rx = 0;
			continue;
		}

		m = mtd_get(sc, (sc->cur_rx), len);

		/* Give descriptor back to card */
		sc->desc[sc->cur_rx].conf = MTD_RXBUF_SIZE & MTD_RXD_CONF_BUFS;
		sc->desc[sc->cur_rx].stat = MTD_RXD_OWNER;

		if (++sc->cur_rx >= MTD_NUM_RXD)
			sc->cur_rx = 0;

		if (m == NULL) {
			printf("%s: error pulling packet off interface\n",
				sc->dev.dv_xname);
			++ifp->if_ierrors;
			continue;
		}

		++ifp->if_ipackets;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		/* Pass the packet up */
		ether_input_mbuf(ifp, m);
	}

	return 1;
}


int
mtd_txirq(sc)
	struct mtd_softc *sc;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;

	/* Clear timeout */
	ifp->if_timer = 0;

	ifp->if_flags &= ~IFF_OACTIVE;
	++ifp->if_opackets;

	/* XXX FIXME If there is some queued, do an mtd_start? */

	return 1;
}


int
mtd_bufirq(sc)
	struct mtd_softc *sc;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	
	/* Clear timeout */
	ifp->if_timer = 0;

	/* XXX FIXME: Do something here to make sure we get some buffers! */

	return 1;
}


int
mtd_irq_h(args)
	void *args;
{
	struct mtd_softc *sc = args;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u_int32_t status;
	int r = 0;

	if (!(ifp->if_flags & IFF_RUNNING) ||
		!(sc->dev.dv_flags & DVF_ACTIVE))
		return 0;

	/* Disable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, 0x00000000);

	for(;;) {
		status = MTD_READ_4(sc, MTD_ISR);
		if (!status)		/* We didn't ask for this */
			break;

		MTD_WRITE_4(sc, MTD_ISR, status);

		/* NOTE: Perhaps we should reset with some of these errors? */

		if (status & MTD_ISR_RXBUN) {
#ifdef MTD_DEBUG
			printf("%s: receive buffer unavailable\n",
			    sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_RXERR) {
#ifdef MTD_DEBUG
			printf("%s: receive error\n", sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_TXBUN) {
#ifdef MTD_DEBUG
			printf("%s: transmit buffer unavailable\n",
			    sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if ((status & MTD_ISR_PDF)) {
#ifdef MTD_DEBUG
			printf("%s: parallel detection fault\n",
			    sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_FBUSERR) {
#ifdef MTD_DEBUG
			printf("%s: fatal bus error\n", sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_TARERR) {
#ifdef MTD_DEBUG
			printf("%s: target error\n", sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_MASTERR) {
#ifdef MTD_DEBUG
			printf("%s: master error\n", sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_PARERR) {
#ifdef MTD_DEBUG
			printf("%s: parity error\n", sc->dev.dv_xname);
#endif
			++ifp->if_ierrors;
		}

		if (status & MTD_ISR_RXIRQ)	/* Receive interrupt */
			r |= mtd_rxirq(sc);

		if (status & MTD_ISR_TXIRQ)	/* Transmit interrupt */
			r |= mtd_txirq(sc);

		if (status & MTD_ISR_TXEARLY)	/* Transmit early */
			r |= mtd_txirq(sc);

		if (status & MTD_ISR_TXBUN)	/* Transmit buffer n/a */
			r |= mtd_bufirq(sc);

	}

	/* Enable interrupts */
	MTD_WRITE_4(sc, MTD_IMR, MTD_IMR_MASK);

	return r;
}


void
mtd_setmulti(sc)
	struct mtd_softc *sc;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u_int32_t rxtx_stat;
	u_int32_t hash[2] = {0, 0};
	u_int32_t crc;
	struct ether_multi *enm;
	struct ether_multistep step;
	int mcnt = 0;

	/* Get old status */
	rxtx_stat = MTD_READ_4(sc, MTD_RXTXR);

	if ((ifp->if_flags & IFF_ALLMULTI) || (ifp->if_flags & IFF_PROMISC)) {
		rxtx_stat |= MTD_RX_AMULTI;
		MTD_WRITE_4(sc, MTD_RXTXR, rxtx_stat);
		MTD_WRITE_4(sc, MTD_MAR0, MTD_ALL_ADDR);
		MTD_WRITE_4(sc, MTD_MAR1, MTD_ALL_ADDR);
		return;
	}

	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
	while (enm != NULL) {
		/* We need the 6 most significant bits of the CRC */
		crc = ETHER_CRC32(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26;

		hash[crc >> 5] |= 1 << (crc & 0xf);

		++mcnt;
		ETHER_NEXT_MULTI(step, enm);
	}

	/* Accept multicast bit needs to be on? */
	if (mcnt)
		rxtx_stat |= MTD_RX_AMULTI;
	else
		rxtx_stat &= ~MTD_RX_AMULTI;

	/* Write out the hash */
	MTD_WRITE_4(sc, MTD_MAR0, hash[0]);
	MTD_WRITE_4(sc, MTD_MAR1, hash[1]);
	MTD_WRITE_4(sc, MTD_RXTXR, rxtx_stat);
}


void
mtd_reset(sc)
	struct mtd_softc *sc;
{
	int i;

	MTD_SETBIT(sc, MTD_BCR, MTD_BCR_RESET);

	/* Reset descriptor status */
	sc->cur_tx = 0;
	sc->cur_rx = 0;

	/* Wait until done with reset */
	for (i = 0; i < MTD_TIMEOUT; ++i) {
		DELAY(10);
		if (!(MTD_READ_4(sc, MTD_BCR) & MTD_BCR_RESET))
			break;
	}

	if (i == MTD_TIMEOUT) {
		printf("%s: reset timed out\n", sc->dev.dv_xname);
	}

	/* Wait a little so chip can stabilize */
	DELAY(1000);
}


int
mtd_mediachange(ifp)
	struct ifnet *ifp;
{
	struct mtd_softc *sc = ifp->if_softc;

	return (mii_mediachg(&sc->mii));
}


void
mtd_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct mtd_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->mii);
	ifmr->ifm_active = sc->mii.mii_media_active;
	ifmr->ifm_status = sc->mii.mii_media_status;
}


void
mtd_shutdown (arg)
	void *arg;
{
	struct mtd_softc *sc = arg;
	struct ifnet *ifp = &sc->arpcom.ac_if;

	mtd_stop(ifp, 1);
}

struct cfdriver mtd_cd = {
	0, "mtd", DV_IFNET
};
