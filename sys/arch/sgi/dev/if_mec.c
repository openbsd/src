/*	$OpenBSD: if_mec.c,v 1.8 2005/02/17 19:05:35 miod Exp $ */
/*	$NetBSD: if_mec_mace.c,v 1.5 2004/08/01 06:36:36 tsutsui Exp $ */

/*
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
 */

/*
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
 * MACE MAC-110 ethernet driver
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

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/if_ether.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <mips64/archtype.h>
#include <mips64/arcbios.h>
#include <sgi/dev/if_mecreg.h>

#ifdef MEC_DEBUG
#define MEC_DEBUG_RESET		0x01
#define MEC_DEBUG_START		0x02
#define MEC_DEBUG_STOP		0x04
#define MEC_DEBUG_INTR		0x08
#define MEC_DEBUG_RXINTR	0x10
#define MEC_DEBUG_TXINTR	0x20
uint32_t mec_debug = 0xff;
#define DPRINTF(x, y)	if (mec_debug & (x)) printf y
#else
#define DPRINTF(x, y)	/* nothing */
#endif

/*
 * Transmit descriptor list size
 */
#define MEC_NTXDESC		64
#define MEC_NTXDESC_MASK	(MEC_NTXDESC - 1)
#define MEC_NEXTTX(x)		(((x) + 1) & MEC_NTXDESC_MASK)

/*
 * software state for TX
 */
struct mec_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	uint32_t txs_flags;
#define MEC_TXS_BUFLEN_MASK	0x0000007f	/* data len in txd_buf */
#define MEC_TXS_TXDBUF		0x00000080	/* txd_buf is used */
#define MEC_TXS_TXDPTR1		0x00000100	/* txd_ptr[0] is used */
};

/*
 * Transmit buffer descriptor
 */
#define MEC_TXDESCSIZE		128
#define MEC_NTXPTR		3
#define MEC_TXD_BUFOFFSET	\
	(sizeof(uint64_t) + MEC_NTXPTR * sizeof(uint64_t))
#define MEC_TXD_BUFSIZE		(MEC_TXDESCSIZE - MEC_TXD_BUFOFFSET)
#define MEC_TXD_BUFSTART(len)	(MEC_TXD_BUFSIZE - (len))
#define MEC_TXD_ALIGN		8
#define MEC_TXD_ROUNDUP(addr)	\
	(((addr) + (MEC_TXD_ALIGN - 1)) & ~((uint64_t)MEC_TXD_ALIGN - 1))

struct mec_txdesc {
	volatile uint64_t txd_cmd;
#define MEC_TXCMD_DATALEN	0x000000000000ffff	/* data length */
#define MEC_TXCMD_BUFSTART	0x00000000007f0000	/* start byte offset */
#define  TXCMD_BUFSTART(x)	((x) << 16)
#define MEC_TXCMD_TERMDMA	0x0000000000800000	/* stop DMA on abort */
#define MEC_TXCMD_TXINT		0x0000000001000000	/* INT after TX done */
#define MEC_TXCMD_PTR1		0x0000000002000000	/* valid 1st txd_ptr */
#define MEC_TXCMD_PTR2		0x0000000004000000	/* valid 2nd txd_ptr */
#define MEC_TXCMD_PTR3		0x0000000008000000	/* valid 3rd txd_ptr */
#define MEC_TXCMD_UNUSED	0xfffffffff0000000ULL	/* should be zero */

#define txd_stat	txd_cmd
#define MEC_TXSTAT_LEN		0x000000000000ffff	/* TX length */
#define MEC_TXSTAT_COLCNT	0x00000000000f0000	/* collision count */
#define MEC_TXSTAT_COLCNT_SHIFT	16
#define MEC_TXSTAT_LATE_COL	0x0000000000100000	/* late collision */
#define MEC_TXSTAT_CRCERROR	0x0000000000200000	/* */
#define MEC_TXSTAT_DEFERRED	0x0000000000400000	/* */
#define MEC_TXSTAT_SUCCESS	0x0000000000800000	/* TX complete */
#define MEC_TXSTAT_TOOBIG	0x0000000001000000	/* */
#define MEC_TXSTAT_UNDERRUN	0x0000000002000000	/* */
#define MEC_TXSTAT_COLLISIONS	0x0000000004000000	/* */
#define MEC_TXSTAT_EXDEFERRAL	0x0000000008000000	/* */
#define MEC_TXSTAT_COLLIDED	0x0000000010000000	/* */
#define MEC_TXSTAT_UNUSED	0x7fffffffe0000000ULL	/* should be zero */
#define MEC_TXSTAT_SENT		0x8000000000000000ULL	/* packet sent */

	uint64_t txd_ptr[MEC_NTXPTR];
#define MEC_TXPTR_UNUSED2	0x0000000000000007	/* should be zero */
#define MEC_TXPTR_DMAADDR	0x00000000fffffff8	/* TX DMA address */
#define MEC_TXPTR_LEN		0x0000ffff00000000ULL	/* buffer length */
#define  TXPTR_LEN(x)		((uint64_t)(x) << 32)
#define MEC_TXPTR_UNUSED1	0xffff000000000000ULL	/* should be zero */

	uint8_t txd_buf[MEC_TXD_BUFSIZE];
};

/*
 * Receive buffer size
 */
#define MEC_NRXDESC		16
#define MEC_NRXDESC_MASK	(MEC_NRXDESC - 1)
#define MEC_NEXTRX(x)		(((x) + 1) & MEC_NRXDESC_MASK)

/*
 * Receive buffer description
 */
#define MEC_RXDESCSIZE		4096	/* umm, should be 4kbyte aligned */
#define MEC_RXD_NRXPAD		3
#define MEC_RXD_DMAOFFSET	(1 + MEC_RXD_NRXPAD)
#define MEC_RXD_BUFOFFSET	(MEC_RXD_DMAOFFSET * sizeof(uint64_t))
#define MEC_RXD_BUFSIZE		(MEC_RXDESCSIZE - MEC_RXD_BUFOFFSET)

struct mec_rxdesc {
	volatile uint64_t rxd_stat;
#define MEC_RXSTAT_LEN		0x000000000000ffff	/* data length */
#define MEC_RXSTAT_VIOLATION	0x0000000000010000	/* code violation (?) */
#define MEC_RXSTAT_UNUSED2	0x0000000000020000	/* unknown (?) */
#define MEC_RXSTAT_CRCERROR	0x0000000000040000	/* CRC error */
#define MEC_RXSTAT_MULTICAST	0x0000000000080000	/* multicast packet */
#define MEC_RXSTAT_BROADCAST	0x0000000000100000	/* broadcast packet */
#define MEC_RXSTAT_INVALID	0x0000000000200000	/* invalid preamble */
#define MEC_RXSTAT_LONGEVENT	0x0000000000400000	/* long packet */
#define MEC_RXSTAT_BADPACKET	0x0000000000800000	/* bad packet */
#define MEC_RXSTAT_CAREVENT	0x0000000001000000	/* carrier event */
#define MEC_RXSTAT_MATCHMCAST	0x0000000002000000	/* match multicast */
#define MEC_RXSTAT_MATCHMAC	0x0000000004000000	/* match MAC */
#define MEC_RXSTAT_SEQNUM	0x00000000f8000000	/* sequence number */
#define MEC_RXSTAT_CKSUM	0x0000ffff00000000ULL	/* IP checksum */
#define MEC_RXSTAT_UNUSED1	0x7fff000000000000ULL	/* should be zero */
#define MEC_RXSTAT_RECEIVED	0x8000000000000000ULL	/* set to 1 on RX */
	uint64_t rxd_pad1[MEC_RXD_NRXPAD];
	uint8_t  rxd_buf[MEC_RXD_BUFSIZE];
};

/*
 * control structures for DMA ops
 */
struct mec_control_data {
	/*
	 * TX descriptors and buffers
	 */
	struct mec_txdesc mcd_txdesc[MEC_NTXDESC];

	/*
	 * RX descriptors and buffers
	 */
	struct mec_rxdesc mcd_rxdesc[MEC_NRXDESC];
};

/*
 * It _seems_ there are some restrictions on descriptor address:
 *
 * - Base address of txdescs should be 8kbyte aligned
 * - Each txdesc should be 128byte aligned
 * - Each rxdesc should be 4kbyte aligned
 *
 * So we should specify 64k align to allocalte txdescs.
 * In this case, sizeof(struct mec_txdesc) * MEC_NTXDESC is 8192
 * so rxdescs are also allocated at 4kbyte aligned.
 */
#define MEC_CONTROL_DATA_ALIGN	(8 * 1024)

#define MEC_CDOFF(x)	offsetof(struct mec_control_data, x)
#define MEC_CDTXOFF(x)	MEC_CDOFF(mcd_txdesc[(x)])
#define MEC_CDRXOFF(x)	MEC_CDOFF(mcd_rxdesc[(x)])

/*
 * software state per device
 */
struct mec_softc {
	struct device sc_dev;		/* generic device structures */
	struct arpcom sc_ac;		/* Ethernet common part */

	bus_space_tag_t sc_st;		/* bus_space tag */
	bus_space_handle_t sc_sh;	/* bus_space handle */
	bus_dma_tag_t sc_dmat;		/* bus_dma tag */
	void *sc_sdhook;		/* shoutdown hook */

	struct mii_data sc_mii;		/* MII/media information */
	int sc_phyaddr;			/* MII address */
	struct timeout sc_tick_ch;	/* tick timeout */

	bus_dmamap_t sc_cddmamap;	/* bus_dma map for control data */
#define sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/* pointer to allocalted control data */
	struct mec_control_data *sc_control_data;
#define sc_txdesc	sc_control_data->mcd_txdesc
#define sc_rxdesc	sc_control_data->mcd_rxdesc

	/* software state for TX descs */
	struct mec_txsoft sc_txsoft[MEC_NTXDESC];

	int sc_txpending;		/* number of TX requests pending */
	int sc_txdirty;			/* first dirty TX descriptor */
	int sc_txlast;			/* last used TX descriptor */

	int sc_rxptr;			/* next ready RX buffer */
};

#define MEC_CDTXADDR(sc, x)	((sc)->sc_cddma + MEC_CDTXOFF(x))
#define MEC_CDRXADDR(sc, x)	((sc)->sc_cddma + MEC_CDRXOFF(x))

#define MEC_TXDESCSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    MEC_CDTXOFF(x), MEC_TXDESCSIZE, (ops))
#define MEC_TXCMDSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    MEC_CDTXOFF(x), sizeof(uint64_t), (ops))

#define MEC_RXSTATSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    MEC_CDRXOFF(x), sizeof(uint64_t), (ops))
#define MEC_RXBUFSYNC(sc, x, len, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    MEC_CDRXOFF(x) + MEC_RXD_BUFOFFSET,				\
	    ETHER_ALIGN + (len), (ops))

/* XXX these values should be moved to <net/if_ether.h> ? */
#define ETHER_PAD_LEN	(ETHER_MIN_LEN - ETHER_CRC_LEN)

struct cfdriver mec_cd = {
	NULL, "mec", DV_IFNET
};

int	mec_match(struct device *, void *, void *);
void	mec_attach(struct device *, struct device *, void *);

struct cfattach mec_ca = {
	sizeof(struct mec_softc), mec_match, mec_attach
};

int	mec_mii_readreg(struct device *, int, int);
void	mec_mii_writereg(struct device *, int, int, int);
int	mec_mii_wait(struct mec_softc *);
void	mec_statchg(struct device *);
void	mec_mediastatus(struct ifnet *, struct ifmediareq *);
int	mec_mediachange(struct ifnet *);

int	mec_init(struct ifnet * ifp);
void	mec_start(struct ifnet *);
void	mec_watchdog(struct ifnet *);
void	mec_tick(void *);
int	mec_ioctl(struct ifnet *, u_long, caddr_t);
void	mec_reset(struct mec_softc *);
void	mec_setfilter(struct mec_softc *);
int	mec_intr(void *arg);
void	mec_stop(struct ifnet *, int);
void	mec_rxintr(struct mec_softc *, uint32_t);
void	mec_txintr(struct mec_softc *, uint32_t);
void	mec_shutdown(void *);

int
mec_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;

	if (ca->ca_sys != SGI_O2 || strcmp(ca->ca_name, mec_cd.cd_name))
		return (0);

	return (1);
}

void
mec_attach(struct device *parent, struct device *self, void *aux)
{
	struct mec_softc *sc = (void *)self;
	struct confargs *ca = aux;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t command;
	char *macaddr;
	struct mii_softc *child;
	bus_dma_segment_t seg;
	int i, err, rseg;

	sc->sc_st = ca->ca_iot;
	if (bus_space_map(sc->sc_st, ca->ca_baseaddr, MEC_NREGS, 0,
	    &sc->sc_sh) != 0) {
		printf(": can't map i/o space\n");
		return;
	}

	/* set up DMA structures */
	sc->sc_dmat = ca->ca_dmat;

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((err = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct mec_control_data), MEC_CONTROL_DATA_ALIGN, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to allocate control data, error = %d\n", err);
		goto fail_0;
	}

	/*
	 * XXX needs re-think...
	 * control data structures contain whole RX data buffer, so
	 * BUS_DMA_COHERENT (which disables cache) may cause some performance
	 * issue on copying data from the RX buffer to mbuf on normal memory,
	 * though we have to make sure all bus_dmamap_sync(9) ops are called
	 * proprely in that case.
	 */
	if ((err = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct mec_control_data),
	    (caddr_t *)&sc->sc_control_data, /*BUS_DMA_COHERENT*/ 0)) != 0) {
		printf(": unable to map control data, error = %d\n", err);
		goto fail_1;
	}
	memset(sc->sc_control_data, 0, sizeof(struct mec_control_data));

	if ((err = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct mec_control_data), 1,
	    sizeof(struct mec_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		printf(": unable to create control data DMA map, error = %d\n",
		    err);
		goto fail_2;
	}
	if ((err = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct mec_control_data), NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		printf(": unable to load control data DMA map, error = %d\n",
		    err);
		goto fail_3;
	}

	/* create TX buffer DMA maps */
	for (i = 0; i < MEC_NTXDESC; i++) {
		if ((err = bus_dmamap_create(sc->sc_dmat,
		    MCLBYTES, 1, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			printf(": unable to create tx DMA map %d, error = %d\n",
			    i, err);
			goto fail_4;
		}
	}

	timeout_set(&sc->sc_tick_ch, mec_tick, sc);

	/* get ethernet address from ARCBIOS */
	if ((macaddr = Bios_GetEnvironmentVariable("eaddr")) == NULL) {
		printf(": unable to get MAC address!\n");
		goto fail_4;
	}
	enaddr_aton(macaddr, sc->sc_ac.ac_enaddr);

	/* reset device */
	mec_reset(sc);

	command = bus_space_read_8(sc->sc_st, sc->sc_sh, MEC_MAC_CONTROL);

	printf(": MAC-110 rev %d, address %s\n",
	    (command & MEC_MAC_REVISION) >> MEC_MAC_REVISION_SHIFT,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	/* Done, now attach everything */

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = mec_mii_readreg;
	sc->sc_mii.mii_writereg = mec_mii_writereg;
	sc->sc_mii.mii_statchg = mec_statchg;

	/* Set up PHY properties */
	ifmedia_init(&sc->sc_mii.mii_media, 0, mec_mediachange,
	    mec_mediastatus);
	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	child = LIST_FIRST(&sc->sc_mii.mii_phys);
	if (child == NULL) {
		/* No PHY attached */
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
	ifp->if_ioctl = mec_ioctl;
	ifp->if_start = mec_start;
	ifp->if_watchdog = mec_watchdog;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	IFQ_SET_MAXLEN(&ifp->if_snd, MEC_NTXDESC - 1);
	ether_ifattach(ifp);

	/* establish interrupt */
	BUS_INTR_ESTABLISH(ca, NULL, ca->ca_intr, IST_EDGE, IPL_NET,
	    mec_intr, sc, sc->sc_dev.dv_xname);

	/* set shutdown hook to reset interface on powerdown */
	sc->sc_sdhook = shutdownhook_establish(mec_shutdown, sc);

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall though.
	 */
 fail_4:
	for (i = 0; i < MEC_NTXDESC; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_control_data,
	    sizeof(struct mec_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

int
mec_mii_readreg(struct device *self, int phy, int reg)
{
	struct mec_softc *sc = (void *)self;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint32_t val;
	int i;

	if (mec_mii_wait(sc) != 0)
		return 0;

	bus_space_write_4(st, sh, MEC_PHY_ADDRESS,
	    (phy << MEC_PHY_ADDR_DEVSHIFT) | (reg & MEC_PHY_ADDR_REGISTER));
	bus_space_write_8(st, sh, MEC_PHY_READ_INITIATE, 1);
	delay(25);

	for (i = 0; i < 20; i++) {
		delay(30);

		val = bus_space_read_4(st, sh, MEC_PHY_DATA);

		if ((val & MEC_PHY_DATA_BUSY) == 0)
			return val & MEC_PHY_DATA_VALUE;
	}
	return 0;
}

void
mec_mii_writereg(struct device *self, int phy, int reg, int val)
{
	struct mec_softc *sc = (void *)self;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;

	if (mec_mii_wait(sc) != 0) {
		printf("timed out writing %x: %x\n", reg, val);
		return;
	}

	bus_space_write_4(st, sh, MEC_PHY_ADDRESS,
	    (phy << MEC_PHY_ADDR_DEVSHIFT) | (reg & MEC_PHY_ADDR_REGISTER));

	delay(60);

	bus_space_write_4(st, sh, MEC_PHY_DATA, val & MEC_PHY_DATA_VALUE);

	delay(60);

	mec_mii_wait(sc);
}

int
mec_mii_wait(struct mec_softc *sc)
{
	uint32_t busy;
	int i, s;

	for (i = 0; i < 100; i++) {
		delay(30);

		s = splhigh();
		busy = bus_space_read_4(sc->sc_st, sc->sc_sh, MEC_PHY_DATA);
		splx(s);

		if ((busy & MEC_PHY_DATA_BUSY) == 0)
			return 0;
		if (busy == 0xffff) /* XXX ? */
			return 0;
	}

	printf("%s: MII timed out\n", sc->sc_dev.dv_xname);
	return 1;
}

void
mec_statchg(struct device *self)
{
	struct mec_softc *sc = (void *)self;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint32_t control;

	control = bus_space_read_8(st, sh, MEC_MAC_CONTROL);
	control &= ~(MEC_MAC_IPGT | MEC_MAC_IPGR1 | MEC_MAC_IPGR2 |
	    MEC_MAC_FULL_DUPLEX | MEC_MAC_SPEED_SELECT);

	/* must also set IPG here for duplex stuff ... */
	if ((sc->sc_mii.mii_media_active & IFM_FDX) != 0) {
		control |= MEC_MAC_FULL_DUPLEX;
	} else {
		/* set IPG */
		control |= MEC_MAC_IPG_DEFAULT;
	}

	bus_space_write_8(st, sh, MEC_MAC_CONTROL, control);
}

void
mec_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct mec_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

int
mec_mediachange(struct ifnet *ifp)
{
	struct mec_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;

	return mii_mediachg(&sc->sc_mii);
}

int
mec_init(struct ifnet *ifp)
{
	struct mec_softc *sc = ifp->if_softc;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct mec_rxdesc *rxd;
	int i;

	/* cancel any pending I/O */
	mec_stop(ifp, 0);

	/* reset device */
	mec_reset(sc);

	/* setup filter for multicast or promisc mode */
	mec_setfilter(sc);

	/* set the TX ring pointer to the base address */
	bus_space_write_8(st, sh, MEC_TX_RING_BASE, MEC_CDTXADDR(sc, 0));

	sc->sc_txpending = 0;
	sc->sc_txdirty = 0;
	sc->sc_txlast = MEC_NTXDESC - 1;

	/* put RX buffers into FIFO */
	for (i = 0; i < MEC_NRXDESC; i++) {
		rxd = &sc->sc_rxdesc[i];
		rxd->rxd_stat = 0;
		MEC_RXSTATSYNC(sc, i, BUS_DMASYNC_PREREAD);
		MEC_RXBUFSYNC(sc, i, ETHER_MAX_LEN, BUS_DMASYNC_PREREAD);
		bus_space_write_8(st, sh, MEC_MCL_RX_FIFO, MEC_CDRXADDR(sc, i));
	}
	sc->sc_rxptr = 0;

#if 0	/* XXX no info */
	bus_space_write_8(st, sh, MEC_TIMER, 0);
#endif

	/*
	 * MEC_DMA_TX_INT_ENABLE will be set later otherwise it causes
	 * spurious interrupts when TX buffers are empty
	 */
	bus_space_write_8(st, sh, MEC_DMA_CONTROL,
	    (MEC_RXD_DMAOFFSET << MEC_DMA_RX_DMA_OFFSET_SHIFT) |
	    (MEC_NRXDESC << MEC_DMA_RX_INT_THRESH_SHIFT) |
	    MEC_DMA_TX_DMA_ENABLE | /* MEC_DMA_TX_INT_ENABLE | */
	    MEC_DMA_RX_DMA_ENABLE | MEC_DMA_RX_INT_ENABLE);

	timeout_add(&sc->sc_tick_ch, hz);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	mec_start(ifp);

	mii_mediachg(&sc->sc_mii);

	return 0;
}

void
mec_reset(struct mec_softc *sc)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint64_t address, control;
	int i;

	/* reset chip */
	bus_space_write_8(st, sh, MEC_MAC_CONTROL, MEC_MAC_CORE_RESET);
	delay(1000);
	bus_space_write_8(st, sh, MEC_MAC_CONTROL, 0);
	delay(1000);

	/* set ethernet address */
	address = 0;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		address = address << 8;
		address += sc->sc_ac.ac_enaddr[i];
	}
	bus_space_write_8(st, sh, MEC_STATION, address);

	/* Default to 100/half and let autonegotiation work its magic */
	control = MEC_MAC_SPEED_SELECT | MEC_MAC_FILTER_MATCHMULTI |
	    MEC_MAC_IPG_DEFAULT;

	bus_space_write_8(st, sh, MEC_MAC_CONTROL, control);
	bus_space_write_8(st, sh, MEC_DMA_CONTROL, 0);

	DPRINTF(MEC_DEBUG_RESET, ("mec: control now %llx\n",
	    bus_space_read_8(st, sh, MEC_MAC_CONTROL)));
}

void
mec_start(struct ifnet *ifp)
{
	struct mec_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct mec_txdesc *txd;
	struct mec_txsoft *txs;
	bus_dmamap_t dmamap;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint64_t txdaddr;
	int error, firsttx, nexttx, opending;
	int len, bufoff, buflen, unaligned, txdlen;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous txpending and the first transmit descriptor.
	 */
	opending = sc->sc_txpending;
	firsttx = MEC_NEXTTX(sc->sc_txlast);

	DPRINTF(MEC_DEBUG_START,
	    ("mec_start: opending = %d, firsttx = %d\n", opending, firsttx));

	for (;;) {
		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (sc->sc_txpending == MEC_NTXDESC) {
			break;
		}

		/*
		 * Get the next available transmit descriptor.
		 */
		nexttx = MEC_NEXTTX(sc->sc_txlast);
		txd = &sc->sc_txdesc[nexttx];
		txs = &sc->sc_txsoft[nexttx];

		buflen = 0;
		bufoff = 0;
		txdaddr = 0; /* XXX gcc */
		txdlen = 0; /* XXX gcc */

		len = m0->m_pkthdr.len;

		DPRINTF(MEC_DEBUG_START,
		    ("mec_start: len = %d, nexttx = %d\n", len, nexttx));

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (len < ETHER_PAD_LEN) {
			/*
			 * I don't know if MEC chip does auto padding,
			 * so if the packet is small enough,
			 * just copy it to the buffer in txdesc.
			 * Maybe this is the simple way.
			 */
			DPRINTF(MEC_DEBUG_START, ("mec_start: short packet\n"));

			bufoff = MEC_TXD_BUFSTART(ETHER_PAD_LEN);
			m_copydata(m0, 0, m0->m_pkthdr.len,
			    txd->txd_buf + bufoff);
			memset(txd->txd_buf + bufoff + len, 0,
			    ETHER_PAD_LEN - len);
			len = buflen = ETHER_PAD_LEN;

			txs->txs_flags = MEC_TXS_TXDBUF | buflen;
		} else {
			/*
			 * If the packet won't fit the buffer in txdesc,
			 * we have to use concatinate pointer to handle it.
			 * While MEC can handle up to three segments to
			 * concatinate, MEC requires that both the second and
			 * third segments have to be 8 byte aligned.
			 * Since it's unlikely for mbuf clusters, we use
			 * only the first concatinate pointer. If the packet
			 * doesn't fit in one DMA segment, allocate new mbuf
			 * and copy the packet to it.
			 *
			 * Besides, if the start address of the first segments
			 * is not 8 byte aligned, such part have to be copied
			 * to the txdesc buffer. (XXX see below comments)
	                 */
			DPRINTF(MEC_DEBUG_START, ("mec_start: long packet\n"));

			dmamap = txs->txs_dmamap;
			if (bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
			    BUS_DMA_WRITE | BUS_DMA_NOWAIT) != 0) {
				struct mbuf *m;

				DPRINTF(MEC_DEBUG_START,
				    ("mec_start: re-allocating mbuf\n"));
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
				 * in many case the header isn't 4-byte aligned
				 * and data after the header is 4-byte aligned.
				 * Thus adding 2-byte offset before copying to
				 * new mbuf avoids unaligned copy and this may
				 * improve some performance.
				 * As noted above, unaligned part has to be
				 * copied to txdesc buffer so this may cause
				 * extra copy ops, but for now MEC always
				 * requires some data in txdesc buffer,
				 * so we always have to copy some data anyway.
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

			/* handle unaligned part */
			txdaddr = MEC_TXD_ROUNDUP(dmamap->dm_segs[0].ds_addr);
			txs->txs_flags = MEC_TXS_TXDPTR1;
			unaligned =
			    dmamap->dm_segs[0].ds_addr & (MEC_TXD_ALIGN - 1);
			DPRINTF(MEC_DEBUG_START,
			    ("mec_start: ds_addr = 0x%x, unaligned = %d\n",
			    (u_int)dmamap->dm_segs[0].ds_addr, unaligned));
			if (unaligned != 0) {
				buflen = MEC_TXD_ALIGN - unaligned;
				bufoff = MEC_TXD_BUFSTART(buflen);
				DPRINTF(MEC_DEBUG_START,
				    ("mec_start: unaligned, "
				    "buflen = %d, bufoff = %d\n",
				    buflen, bufoff));
				memcpy(txd->txd_buf + bufoff,
				    mtod(m0, caddr_t), buflen);
				txs->txs_flags |= MEC_TXS_TXDBUF | buflen;
			}
#if 1
			else {
				/*
				 * XXX needs hardware info XXX
				 * It seems MEC always requires some data
				 * in txd_buf[] even if buffer is
				 * 8-byte aligned otherwise DMA abort error
				 * occurs later...
				 */
				buflen = MEC_TXD_ALIGN;
				bufoff = MEC_TXD_BUFSTART(buflen);
				memcpy(txd->txd_buf + bufoff,
				    mtod(m0, caddr_t), buflen);
				DPRINTF(MEC_DEBUG_START,
				    ("mec_start: aligned, "
				    "buflen = %d, bufoff = %d\n",
				    buflen, bufoff));
				txs->txs_flags |= MEC_TXS_TXDBUF | buflen;
				txdaddr += MEC_TXD_ALIGN;
			}
#endif
			txdlen  = len - buflen;
			DPRINTF(MEC_DEBUG_START,
			    ("mec_start: txdaddr = 0x%llx, txdlen = %d\n",
			    txdaddr, txdlen));

			/*
			 * sync the DMA map for TX mbuf
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
			bpf_mtap(ifp->if_bpf, m0);
#endif

		/*
		 * setup the transmit descriptor.
		 */

		/* TXINT bit will be set later on the last packet */
		txd->txd_cmd = (len - 1);
		/* but also set TXINT bit on a half of TXDESC */
		if (sc->sc_txpending == (MEC_NTXDESC / 2))
			txd->txd_cmd |= MEC_TXCMD_TXINT;

		if (txs->txs_flags & MEC_TXS_TXDBUF)
			txd->txd_cmd |= TXCMD_BUFSTART(MEC_TXDESCSIZE - buflen);
		if (txs->txs_flags & MEC_TXS_TXDPTR1) {
			txd->txd_cmd |= MEC_TXCMD_PTR1;
			txd->txd_ptr[0] = TXPTR_LEN(txdlen - 1) | txdaddr;
			/*
			 * Store a pointer to the packet so we can
			 * free it later.
			 */
			txs->txs_mbuf = m0;
		} else {
			txd->txd_ptr[0] = 0;
			/*
			 * In this case all data are copied to buffer in txdesc,
			 * we can free TX mbuf here.
			 */
			m_freem(m0);
		}

		DPRINTF(MEC_DEBUG_START,
		    ("mec_start: txd_cmd = 0x%llx, txd_ptr = 0x%llx\n",
		    txd->txd_cmd, txd->txd_ptr[0]));
		DPRINTF(MEC_DEBUG_START,
		    ("mec_start: len = %d (0x%04x), buflen = %d (0x%02x)\n",
		    len, len, buflen, buflen));

		/* sync TX descriptor */
		MEC_TXDESCSYNC(sc, nexttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* advance the TX pointer. */
		sc->sc_txpending++;
		sc->sc_txlast = nexttx;
	}

	if (sc->sc_txpending == MEC_NTXDESC) {
		/* No more slots; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txpending != opending) {
		/*
		 * Cause a TX interrupt to happen on the last packet
		 * we enqueued.
		 */
		sc->sc_txdesc[sc->sc_txlast].txd_cmd |= MEC_TXCMD_TXINT;
		MEC_TXCMDSYNC(sc, sc->sc_txlast,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* start TX */
		bus_space_write_8(st, sh, MEC_TX_RING_PTR,
		    MEC_NEXTTX(sc->sc_txlast));

		/*
		 * If the transmitter was idle,
		 * reset the txdirty pointer and reenable TX interrupt.
		 */
		if (opending == 0) {
			sc->sc_txdirty = firsttx;
			bus_space_write_8(st, sh, MEC_TX_ALIAS,
			    MEC_TX_ALIAS_INT_ENABLE);
		}

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

void
mec_stop(struct ifnet *ifp, int disable)
{
	struct mec_softc *sc = ifp->if_softc;
	struct mec_txsoft *txs;
	int i;

	DPRINTF(MEC_DEBUG_STOP, ("mec_stop\n"));

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	timeout_del(&sc->sc_tick_ch);
	mii_down(&sc->sc_mii);

	/* release any TX buffers */
	for (i = 0; i < MEC_NTXDESC; i++) {
		txs = &sc->sc_txsoft[i];
		if ((txs->txs_flags & MEC_TXS_TXDPTR1) != 0) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
	}
}

int
mec_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct mec_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_ac, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			mec_init(ifp);
			arp_ifinit(&sc->sc_ac, ifa);
			break;
#endif
		default:
			mec_init(ifp);
			break;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU || ifr->ifr_mtu < ETHERMIN)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * XXX If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP)
			mec_init(ifp);
		else if (ifp->if_flags & IFF_RUNNING)
			mec_stop(ifp, 1);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ac) :
		    ether_delmulti(ifr, &sc->sc_ac);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				mec_init(ifp);
			error = 0;
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = ENXIO;
		break;
	}

	splx(s);
	return error;
}

void
mec_watchdog(struct ifnet *ifp)
{
	struct mec_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;

	mec_init(ifp);
}

void
mec_tick(void *arg)
{
	struct mec_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add(&sc->sc_tick_ch, hz);
}

void
mec_setfilter(struct mec_softc *sc)
{
	struct arpcom *ec = &sc->sc_ac;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	uint64_t mchash;
	uint32_t control, hash;
	int mcnt;

	control = bus_space_read_8(st, sh, MEC_MAC_CONTROL);
	control &= ~MEC_MAC_FILTER_MASK;

	if (ifp->if_flags & IFF_PROMISC) {
		control |= MEC_MAC_FILTER_PROMISC;
		bus_space_write_8(st, sh, MEC_MULTICAST, 0xffffffffffffffffULL);
		bus_space_write_8(st, sh, MEC_MAC_CONTROL, control);
		return;
	}

	mcnt = 0;
	mchash = 0;
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/* set allmulti for a range of multicast addresses */
			control |= MEC_MAC_FILTER_ALLMULTI;
			bus_space_write_8(st, sh, MEC_MULTICAST,
			    0xffffffffffffffffULL);
			bus_space_write_8(st, sh, MEC_MAC_CONTROL, control);
			return;
		}

#define mec_calchash(addr)	(ether_crc32_be((addr), ETHER_ADDR_LEN) >> 26)

		hash = mec_calchash(enm->enm_addrlo);
		mchash |= 1 << hash;
		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (mcnt > 0)
		control |= MEC_MAC_FILTER_MATCHMULTI;

	bus_space_write_8(st, sh, MEC_MULTICAST, mchash);
	bus_space_write_8(st, sh, MEC_MAC_CONTROL, control);
}

int
mec_intr(void *arg)
{
	struct mec_softc *sc = arg;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t statreg, statack, dmac;
	int handled, sent;

	DPRINTF(MEC_DEBUG_INTR, ("mec_intr: called\n"));

	handled = sent = 0;

	for (;;) {
		statreg = bus_space_read_8(st, sh, MEC_INT_STATUS);

		DPRINTF(MEC_DEBUG_INTR,
		    ("mec_intr: INT_STAT = 0x%x\n", statreg));

		statack = statreg & MEC_INT_STATUS_MASK;
		if (statack == 0)
			break;
		bus_space_write_8(st, sh, MEC_INT_STATUS, statack);

		handled = 1;

		if (statack &
		    (MEC_INT_RX_THRESHOLD |
		     MEC_INT_RX_FIFO_UNDERFLOW)) {
			mec_rxintr(sc, statreg);
		}

		dmac = bus_space_read_8(st, sh, MEC_DMA_CONTROL);
		DPRINTF(MEC_DEBUG_INTR,
		    ("mec_intr: DMA_CONT = 0x%x\n", dmac));

		if (statack &
		    (MEC_INT_TX_EMPTY |
		     MEC_INT_TX_PACKET_SENT |
		     MEC_INT_TX_ABORT)) {
			mec_txintr(sc, statreg);
			sent = 1;
		}

		if (statack &
		    (MEC_INT_TX_LINK_FAIL |
		     MEC_INT_TX_MEM_ERROR |
		     MEC_INT_TX_ABORT |
		     MEC_INT_RX_DMA_UNDERFLOW)) {
			printf("%s: mec_intr: interrupt status = 0x%x\n",
			    sc->sc_dev.dv_xname, statreg);
		}
	}

	if (sent) {
		/* try to get more packets going */
		mec_start(ifp);
	}

	return handled;
}

void
mec_rxintr(struct mec_softc *sc, uint32_t stat)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf *m;
	struct mec_rxdesc *rxd;
	uint64_t rxstat;
	u_int len;
	int i, last;

	DPRINTF(MEC_DEBUG_RXINTR, ("mec_rxintr: called\n"));

	bus_space_write_8(st, sh, MEC_RX_ALIAS, 0);

	last = (stat & MEC_INT_RX_MCL_FIFO_ALIAS) >> 8;
	/* XXX does alias count mod 32 even if 16 descs are set up? */
	last &= MEC_NRXDESC_MASK;

	if (stat & MEC_INT_RX_FIFO_UNDERFLOW)
		last = (last - 1) & MEC_NRXDESC_MASK;

	DPRINTF(MEC_DEBUG_RXINTR, ("mec_rxintr: rxptr %d last %d\n",
	    sc->sc_rxptr, last));
	for (i = sc->sc_rxptr; i != last; i = MEC_NEXTRX(i)) {

		MEC_RXSTATSYNC(sc, i, BUS_DMASYNC_POSTREAD);
		rxd = &sc->sc_rxdesc[i];
		rxstat = rxd->rxd_stat;

		DPRINTF(MEC_DEBUG_RXINTR,
		    ("mec_rxintr: rxstat = 0x%llx, rxptr = %d\n",
		    rxstat, i));
		DPRINTF(MEC_DEBUG_RXINTR, ("mec_rxintr: rxfifo = 0x%x\n",
		    (u_int)bus_space_read_8(st, sh, MEC_RX_FIFO)));

		if ((rxstat & MEC_RXSTAT_RECEIVED) == 0) {
			/* Status not received but fifo counted? Drop it! */
			goto dropit;
		}

		len = rxstat & MEC_RXSTAT_LEN;

		if (len < ETHER_MIN_LEN ||
		    len > ETHER_MAX_LEN) {
			/* invalid length packet; drop it. */
			DPRINTF(MEC_DEBUG_RXINTR,
			    ("mec_rxintr: wrong packet\n"));
 dropit:
			ifp->if_ierrors++;
			rxd->rxd_stat = 0;
			MEC_RXSTATSYNC(sc, i, BUS_DMASYNC_PREREAD);
			bus_space_write_8(st, sh, MEC_MCL_RX_FIFO,
			    MEC_CDRXADDR(sc, i));
			continue;
		}

		if (rxstat &
		    (MEC_RXSTAT_BADPACKET |
		     MEC_RXSTAT_LONGEVENT |
		     MEC_RXSTAT_INVALID   |
		     MEC_RXSTAT_CRCERROR  |
		     MEC_RXSTAT_VIOLATION)) {
			printf("%s: mec_rxintr: status = 0x%llx\n",
			    sc->sc_dev.dv_xname, rxstat);
			goto dropit;
		}

		/*
		 * now allocate an mbuf (and possibly a cluster) to hold
		 * the received packet.
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			printf("%s: unable to allocate RX mbuf\n",
			    sc->sc_dev.dv_xname);
			goto dropit;
		}
		if (len > (MHLEN - ETHER_ALIGN)) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				printf("%s: unable to allocate RX cluster\n",
				    sc->sc_dev.dv_xname);
				m_freem(m);
				m = NULL;
				goto dropit;
			}
		}

		/*
		 * Note MEC chip seems to insert 2 byte paddingat the start of
		 * RX buffer, but we copy whole buffer to avoid unaligned copy.
		 */
		MEC_RXBUFSYNC(sc, i, len + ETHER_ALIGN, BUS_DMASYNC_POSTREAD);
		memcpy(mtod(m, caddr_t), rxd->rxd_buf,
		    ETHER_ALIGN + len - ETHER_CRC_LEN);
		MEC_RXBUFSYNC(sc, i, ETHER_MAX_LEN, BUS_DMASYNC_PREREAD);
		m->m_data += ETHER_ALIGN;

		/* put RX buffer into FIFO again */
		rxd->rxd_stat = 0;
		MEC_RXSTATSYNC(sc, i, BUS_DMASYNC_PREREAD);
		bus_space_write_8(st, sh, MEC_MCL_RX_FIFO, MEC_CDRXADDR(sc, i));

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len - ETHER_CRC_LEN;

		ifp->if_ipackets++;

#if NBPFILTER > 0
		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack it its for us.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* Pass it on. */
		ether_input_mbuf(ifp, m);
	}

	/* update RX pointer */
	sc->sc_rxptr = i;

	bus_space_write_8(st, sh, MEC_RX_ALIAS,
	    (MEC_NRXDESC << MEC_DMA_RX_INT_THRESH_SHIFT) |
	    MEC_DMA_RX_INT_ENABLE);
}

void
mec_txintr(struct mec_softc *sc, uint32_t stat)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mec_txdesc *txd;
	struct mec_txsoft *txs;
	bus_dmamap_t dmamap;
	uint64_t txstat;
	int i, last;
	u_int col;

	ifp->if_flags &= ~IFF_OACTIVE;

	DPRINTF(MEC_DEBUG_TXINTR, ("mec_txintr: called\n"));

	bus_space_write_8(sc->sc_st, sc->sc_sh, MEC_TX_ALIAS, 0);
	last = (stat & MEC_INT_TX_RING_BUFFER_ALIAS) >> 16;

	DPRINTF(MEC_DEBUG_TXINTR, ("mec_txintr: dirty %d last %d\n",
	    sc->sc_txdirty, last));
	for (i = sc->sc_txdirty; i != last && sc->sc_txpending != 0;
	    i = MEC_NEXTTX(i), sc->sc_txpending--) {
		txd = &sc->sc_txdesc[i];

		MEC_TXDESCSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		txstat = txd->txd_stat;
		DPRINTF(MEC_DEBUG_TXINTR,
		    ("mec_txintr: dirty = %d, txstat = 0x%llx\n",
		    i, txstat));
		if ((txstat & MEC_TXSTAT_SENT) == 0) {
			MEC_TXCMDSYNC(sc, i, BUS_DMASYNC_PREREAD);
			break;
		}

		if ((txstat & MEC_TXSTAT_SUCCESS) == 0) {
			printf("%s: TX error: txstat = 0x%llx\n",
			    sc->sc_dev.dv_xname, txstat);
			ifp->if_oerrors++;
			continue;
		}

		txs = &sc->sc_txsoft[i];
		if ((txs->txs_flags & MEC_TXS_TXDPTR1) != 0) {
			dmamap = txs->txs_dmamap;
			bus_dmamap_sync(sc->sc_dmat, dmamap, 0,
			    dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}

		col = (txstat & MEC_TXSTAT_COLCNT) >> MEC_TXSTAT_COLCNT_SHIFT;
		ifp->if_collisions += col;
		ifp->if_opackets++;
	}

	/* update the dirty TX buffer pointer */
	sc->sc_txdirty = i;
	DPRINTF(MEC_DEBUG_INTR,
	    ("mec_txintr: sc_txdirty = %2d, sc_txpending = %2d\n",
	    sc->sc_txdirty, sc->sc_txpending));

	/* cancel the watchdog timer if there are no pending TX packets */
	if (sc->sc_txpending == 0)
		ifp->if_timer = 0;
	else if (!(stat & MEC_INT_TX_EMPTY))
		bus_space_write_8(sc->sc_st, sc->sc_sh, MEC_TX_ALIAS,
		    MEC_TX_ALIAS_INT_ENABLE);
}

void
mec_shutdown(void *arg)
{
	struct mec_softc *sc = arg;

	mec_stop(&sc->sc_ac.ac_if, 1);
}
