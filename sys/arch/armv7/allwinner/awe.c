/*	$OpenBSD: awe.c,v 1.1 2013/10/22 13:22:19 jasper Exp $	*/
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2013 Artturi Alm
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

/* TODO this should use dedicated dma for RX, atleast */

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

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <armv7/allwinner/allwinnervar.h>
#include <armv7/allwinner/awccmuvar.h>
#include <armv7/allwinner/awpiovar.h>

/* configuration registers */
#define	AWE_CR			0x0000
#define	AWE_TXMODE		0x0004
#define	AWE_TXFLOW		0x0008
#define	AWE_TXCR0		0x000c
#define	AWE_TXCR1		0x0010
#define	AWE_TXINS		0x0014
#define	AWE_TXPKTLEN0		0x0018
#define	AWE_TXPKTLEN1		0x001c
#define	AWE_TXSR		0x0020
#define	AWE_TXIO0		0x0024
#define	AWE_TXIO1		0x0028
#define	AWE_TXTSVL0		0x002c
#define	AWE_TXTSVH0		0x0030
#define	AWE_TXTSVL1		0x0034
#define	AWE_TXTSVH1		0x0038
#define	AWE_RXCR		0x003c
#define	AWE_RXHASH0		0x0040
#define	AWE_RXHASH1		0x0044
#define	AWE_RXSR		0x0048
#define	AWE_RXIO		0x004C
#define	AWE_RXFBC		0x0050
#define	AWE_INTCR		0x0054
#define	AWE_INTSR		0x0058
#define	AWE_MACCR0		0x005C
#define	AWE_MACCR1		0x0060
#define	AWE_MACIPGT		0x0064
#define	AWE_MACIPGR		0x0068
#define	AWE_MACCLRT		0x006C
#define	AWE_MACMFL		0x0070
#define	AWE_MACSUPP		0x0074
#define	AWE_MACTEST		0x0078
#define	AWE_MACMCFG		0x007C
#define	AWE_MACMCMD		0x0080
#define	AWE_MACMADR		0x0084
#define	AWE_MACMWTD		0x0088
#define	AWE_MACMRDD		0x008C
#define	AWE_MACMIND		0x0090
#define	AWE_MACSSRR		0x0094
#define	AWE_MACA0		0x0098
#define	AWE_MACA1		0x009c
#define	AWE_MACA2		0x00a0

/* i once spent hours on pretty defines, cvs up ate 'em. these shall do */
#define AWE_INTR_ENABLE		0x010f
#define AWE_INTR_DISABLE	0x0000
#define AWE_INTR_CLEAR		0x0000

#define	AWE_RX_ENABLE		0x0004
#define	AWE_TX_ENABLE		0x0003
#define	AWE_RXTX_ENABLE		0x0007

#define	AWE_RXDRQM		0x0002
#define	AWE_RXTM		0x0004
#define	AWE_RXFLUSH		0x0008
#define	AWE_RXPA		0x0010
#define	AWE_RXPCF		0x0020
#define	AWE_RXPCRCE		0x0040
#define	AWE_RXPLE		0x0080
#define	AWE_RXPOR		0x0100
#define	AWE_RXUCAD		0x10000
#define	AWE_RXDAF		0x20000
#define	AWE_RXMCO		0x100000
#define	AWE_RXMHF		0x200000
#define	AWE_RXBCO		0x400000
#define	AWE_RXSAF		0x1000000
#define	AWE_RXSAIF		0x2000000

#define	AWE_MACRXFC		0x0004
#define	AWE_MACTXFC		0x0008
#define AWE_MACSOFTRESET	0x8000

#define	AWE_MACDUPLEX		0x0001	/* full = 1 */
#define	AWE_MACFLC		0x0002
#define	AWE_MACHF		0x0004
#define	AWE_MACDCRC		0x0008
#define	AWE_MACCRC		0x0010
#define	AWE_MACPC		0x0020
#define	AWE_MACVC		0x0040
#define	AWE_MACADP		0x0080
#define	AWE_MACPRE		0x0100
#define	AWE_MACLPE		0x0200
#define	AWE_MACNB		0x1000
#define	AWE_MACBNB		0x2000
#define	AWE_MACED		0x4000

#define	AWE_RX_ERRLENOOR	0x0040
#define	AWE_RX_ERRLENCHK	0x0020
#define	AWE_RX_ERRCRC		0x0010
#define	AWE_RX_ERRRCV		0x0008 /* XXX receive code violation ? */
#define	AWE_RX_ERRMASK		0x0070

#define	AWE_MII_TIMEOUT	100
#define AWE_MAX_RXD		8
#define AWE_MAX_PKT_SIZE	ETHER_MAX_DIX_LEN

#define AWE_ROUNDUP(size, unit) (((size) + (unit) - 1) & ~((unit) - 1))

struct awe_softc {
	struct device			sc_dev;
	struct arpcom			sc_ac;
	struct mii_data			sc_mii;
	int				sc_phyno;
	bus_space_tag_t			sc_iot;
	bus_space_handle_t		sc_ioh;
	void				*sc_ih; /* Interrupt handler */
	uint32_t			intr_status; /* soft interrupt status */
	uint32_t			pauseframe;
	uint32_t			txf_inuse;
};

struct awe_softc *awe_sc;

void	awe_attach(struct device *, struct device *, void *);
void	awe_setup_interface(struct awe_softc *, struct device *);
void	awe_socware_init(struct awe_softc *);
int	awe_ioctl(struct ifnet *, u_long, caddr_t);
void	awe_start(struct ifnet *);
void	awe_watchdog(struct ifnet *);
void	awe_init(struct awe_softc *);
void	awe_stop(struct awe_softc *);
void	awe_reset(struct awe_softc *);
void	awe_iff(struct awe_softc *, struct ifnet *);
struct mbuf * awe_newbuf(void);
int	awe_intr(void *);
void	awe_recv(struct awe_softc *);
int	awe_miibus_readreg(struct device *, int, int);
void	awe_miibus_writereg(struct device *, int, int, int);
void	awe_miibus_statchg(struct device *);
int	awe_ifm_change(struct ifnet *);
void	awe_ifm_status(struct ifnet *, struct ifmediareq *);

struct cfattach awe_ca = {
	sizeof (struct awe_softc), NULL, awe_attach
};

struct cfdriver awe_cd = {
	NULL, "awe", DV_IFNET
};

void
awe_attach(struct device *parent, struct device *self, void *args)
{
	struct aw_attach_args *aw = args;
	struct awe_softc *sc = (struct awe_softc *) self;
	struct mii_data *mii;
	struct ifnet *ifp;
	int s;

	sc->sc_iot = aw->aw_iot;

	if (bus_space_map(sc->sc_iot, aw->aw_dev->mem[0].addr,
	    aw->aw_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("awe_attach: bus_space_map ioh failed!");

	awe_socware_init(sc);
	sc->txf_inuse = 0;

	sc->sc_ih = arm_intr_establish(aw->aw_dev->irq[0], IPL_NET,
	    awe_intr, sc, sc->sc_dev.dv_xname);

	printf("\n");

	s = splnet();

	printf("%s: address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	/* XXX verify flags & capabilities */
	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = awe_ioctl;
	ifp->if_start = awe_start;
	ifp->if_watchdog = awe_watchdog;
	ifp->if_capabilities = IFCAP_VLAN_MTU; /* XXX status check in recv? */

	IFQ_SET_MAXLEN(&ifp->if_snd, 1);
	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize MII/media info. */
	mii = &sc->sc_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = awe_miibus_readreg;
	mii->mii_writereg = awe_miibus_writereg;
	mii->mii_statchg = awe_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;

	ifmedia_init(&mii->mii_media, 0, awe_ifm_change, awe_ifm_status);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);

	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);
	splx(s);

	awe_sc = sc;
}

void
awe_socware_init(struct awe_softc *sc)
{
	int i;
	uint32_t reg;

	for (i = 0; i < AWPIO_EMAC_NPINS; i++)
		awpio_setcfg(i, 2); /* mux pins to EMAC */
	awccmu_enablemodule(CCMU_EMAC);

	/* MII clock cfg */
	AWCMS4(sc, AWE_MACMCFG, 15 << 2, 13 << 2);

	AWWRITE4(sc, AWE_INTCR, AWE_INTR_DISABLE);
	AWSET4(sc, AWE_INTSR, AWE_INTR_CLEAR);

#if 1
	/* set lladdr with values set in u-boot */
	reg = AWREAD4(sc, AWE_MACA0);
	sc->sc_ac.ac_enaddr[3] = reg >> 16 & 0xff;
	sc->sc_ac.ac_enaddr[4] = reg >> 8 & 0xff;
	sc->sc_ac.ac_enaddr[5] = reg & 0xff;
	reg = AWREAD4(sc, AWE_MACA1);
	sc->sc_ac.ac_enaddr[0] = reg >> 16 & 0xff;
	sc->sc_ac.ac_enaddr[1] = reg >> 8 & 0xff;
	sc->sc_ac.ac_enaddr[2] = reg & 0xff;
#else
	/* set lladdr */
	memset(sc->sc_ac.ac_enaddr, 0xff, ETHER_ADDR_LEN);
	arc4random_buf(sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);
	sc->sc_ac.ac_enaddr[0] &= ~3;
	sc->sc_ac.ac_enaddr[0] |= 2;
#endif

	sc->sc_phyno = 1;
}

void
awe_setup_interface(struct awe_softc *sc, struct device *dev)
{
	uint32_t clr_m, set_m;

	/* configure TX */
	AWCMS4(sc, AWE_TXMODE, 3, 1);	/* cpu mode */

	/* configure RX */
	clr_m = AWE_RXDRQM | AWE_RXTM | AWE_RXPA | AWE_RXPCF |
	    AWE_RXPCRCE | AWE_RXPLE | AWE_RXMHF | AWE_RXSAF |
	    AWE_RXSAIF;
	set_m = AWE_RXPOR | AWE_RXUCAD | AWE_RXDAF | AWE_RXBCO;
	AWCMS4(sc, AWE_RXCR, clr_m, set_m);

	/* configure MAC */
	AWSET4(sc, AWE_MACCR0, AWE_MACTXFC | AWE_MACRXFC);
	clr_m =	AWE_MACHF | AWE_MACDCRC | AWE_MACVC | AWE_MACADP |
	    AWE_MACPRE | AWE_MACLPE | AWE_MACNB | AWE_MACBNB |
	    AWE_MACED;
	set_m = AWE_MACFLC | AWE_MACCRC | AWE_MACPC;
	set_m |= awe_miibus_readreg(dev, sc->sc_phyno, 0) >> 8 & 1;
	AWCMS4(sc, AWE_MACCR1, clr_m, set_m);

	/* XXX */
	AWWRITE4(sc, AWE_MACIPGT, 0x0015);
	AWWRITE4(sc, AWE_MACIPGR, 0x0c12);

	/* XXX set collision window */
	AWWRITE4(sc, AWE_MACCLRT, 0x370f);

	/* set max frame length */
	AWWRITE4(sc, AWE_MACMFL, AWE_MAX_PKT_SIZE);

	/* set lladdr */
	AWWRITE4(sc, AWE_MACA0,
	    sc->sc_ac.ac_enaddr[3] << 16 |
	    sc->sc_ac.ac_enaddr[4] << 8 |
	    sc->sc_ac.ac_enaddr[5]);
	AWWRITE4(sc, AWE_MACA1,
	    sc->sc_ac.ac_enaddr[0] << 16 |
	    sc->sc_ac.ac_enaddr[1] << 8 |
	    sc->sc_ac.ac_enaddr[2]);

	awe_reset(sc);
	/* XXX possibly missing delay in here. */
}

void
awe_init(struct awe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct device *dev = (struct device *)sc;
	int phyreg;
	
	awe_reset(sc);

	AWWRITE4(sc, AWE_INTCR, AWE_INTR_DISABLE);

	AWSET4(sc, AWE_INTSR, AWE_INTR_CLEAR);

	AWSET4(sc, AWE_RXCR, AWE_RXFLUSH);

	/* soft reset */
	AWCLR4(sc, AWE_MACCR0, AWE_MACSOFTRESET);

	/* zero rx counter */
	AWWRITE4(sc, AWE_RXFBC, 0);

	awe_setup_interface(sc, dev);

	/* power up PHY */
	awe_miibus_writereg(dev, sc->sc_phyno, 0,
	    awe_miibus_readreg(dev, sc->sc_phyno, 0) & ~(1 << 11));
	delay(1000);
	phyreg = awe_miibus_readreg(dev, sc->sc_phyno, 0);

	/* set duplex */
	AWCMS4(sc, AWE_MACCR1, 1, phyreg >> 8 & 1);

	/* set speed */
	AWCMS4(sc, AWE_MACSUPP, 1 << 8, (phyreg >> 13 & 1) << 8);

	AWSET4(sc, AWE_CR, AWE_RXTX_ENABLE);

	/* Indicate we are up and running. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	AWSET4(sc, AWE_INTCR, AWE_INTR_ENABLE);

	awe_start(ifp);
}

int
awe_intr(void *arg)
{
	struct awe_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t pending;

	AWWRITE4(sc, AWE_INTCR, AWE_INTR_DISABLE);

	pending = AWREAD4(sc, AWE_INTSR);
	AWWRITE4(sc, AWE_INTSR, pending);

	/*
	 * Handle incoming packets.
	 */
	if (pending & 0x0100) {
		if (ifp->if_flags & IFF_RUNNING)
			awe_recv(sc);
	}

	pending &= 3;

	if (pending) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->txf_inuse--;
		ifp->if_opackets++;
		if (pending == 3) { /* 2 packets got sent */
			sc->txf_inuse--;
			ifp->if_opackets++;
		}
		if (sc->txf_inuse == 0)
			ifp->if_timer = 0;
		else
			ifp->if_timer = 5;
	}

	if (ifp->if_flags & IFF_RUNNING && !IFQ_IS_EMPTY(&ifp->if_snd))
		awe_start(ifp);

	AWSET4(sc, AWE_INTCR, AWE_INTR_ENABLE);

	return 1;
}

/*
 * XXX there's secondary tx fifo to be used.
 */
void
awe_start(struct ifnet *ifp)
{
	struct awe_softc *sc = ifp->if_softc;
	struct mbuf *m;
	struct mbuf *head;
	uint8_t *td;
	uint32_t fifo;
	uint32_t txbuf[AWE_MAX_PKT_SIZE / sizeof(uint32_t)]; /* XXX !!! */

	if (sc->txf_inuse > 1)
		ifp->if_flags |= IFF_OACTIVE;

	if ((ifp->if_flags & (IFF_OACTIVE | IFF_RUNNING)) != IFF_RUNNING)
		return;

	td = (uint8_t *)&txbuf[0];
	m = NULL;
	head = NULL;
trynext:
	IFQ_POLL(&ifp->if_snd, m);
	if (m == NULL)
		return;

	if (m->m_pkthdr.len > AWE_MAX_PKT_SIZE) {
		printf("awe_start: packet too big\n");
		m_freem(m);
		return;
	}

	if (sc->txf_inuse > 1) {
		printf("awe_start: tx fifos in use.\n");
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	/* select fifo */
	fifo = sc->txf_inuse;
	AWWRITE4(sc, AWE_TXINS, fifo);

	sc->txf_inuse++;

	/* set packet length */
	AWWRITE4(sc, AWE_TXPKTLEN0 + (fifo * 4), m->m_pkthdr.len);

	/* copy the actual packet to fifo XXX through 'align buffer'.. */
	m_copydata(m, 0, m->m_pkthdr.len, (caddr_t)td);
	bus_space_write_multi_4(sc->sc_iot, sc->sc_ioh,
	    AWE_TXIO0 + (fifo * 4),
	    (uint32_t *)td, AWE_ROUNDUP(m->m_pkthdr.len, 4) >> 2);

	/* transmit to PHY from fifo */
	AWSET4(sc, AWE_TXCR0 + (fifo * 4), 1);
	ifp->if_timer = 5;
	IFQ_DEQUEUE(&ifp->if_snd, m);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	m_freem(m);

	goto trynext;
}

void
awe_stop(struct awe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	awe_reset(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

void
awe_reset(struct awe_softc *sc)
{
	/* reset the controller */
	AWWRITE4(sc, AWE_CR, 0);
	delay(200);
	AWWRITE4(sc, AWE_CR, 1);
	delay(200);
}

void
awe_watchdog(struct ifnet *ifp)
{
	struct awe_softc *sc = ifp->if_softc;
	if (sc->pauseframe) {
		ifp->if_timer = 5;
		return;
	}
	printf("%s: watchdog tx timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;
	awe_init(sc);
	awe_start(ifp);
}

/*
 * XXX DMA?
 */
void
awe_recv(struct awe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t fbc, reg;
	struct mbuf *m;
	uint16_t pktstat;
	int16_t pktlen;
	int rlen;
	char rxbuf[AWE_MAX_PKT_SIZE]; /* XXX !!! */
trynext:
	fbc = AWREAD4(sc, AWE_RXFBC);
	if (!fbc)
		return;

	/*
	 * first bit of MSB is packet valid flag,
	 * it is 'padded' with 0x43414d = "MAC"
	 */
	reg = AWREAD4(sc, AWE_RXIO);
	if (reg != 0x0143414d) {	/* invalid packet */
		/* disable, flush, enable */
		AWCLR4(sc, AWE_CR, AWE_RX_ENABLE);
		AWSET4(sc, AWE_RXCR, AWE_RXFLUSH);
		while (AWREAD4(sc, AWE_RXCR) & AWE_RXFLUSH);
		AWSET4(sc, AWE_CR, AWE_RX_ENABLE);

		goto err_out;
	}
	
	m = awe_newbuf();
	if (m == NULL)
		goto err_out;

	reg = AWREAD4(sc, AWE_RXIO);
	pktstat = (uint16_t)reg >> 16;
	pktlen = (int16_t)reg; /* length of useful data */

	if (pktstat & AWE_RX_ERRMASK || pktlen < ETHER_MIN_LEN) {
		ifp->if_ierrors++;
		goto trynext;
	}
	if (pktlen > AWE_MAX_PKT_SIZE)
		pktlen = AWE_MAX_PKT_SIZE; /* XXX is truncating ok? */

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = pktlen;
	/* XXX m->m_pkthdr.csum_flags ? */
	m_adj(m, ETHER_ALIGN);

	/* read the actual packet from fifo XXX through 'align buffer'.. */
	if (pktlen & 3)
		rlen = AWE_ROUNDUP(pktlen, 4);
	else
		rlen = pktlen;
	bus_space_read_multi_4(sc->sc_iot, sc->sc_ioh,
	    AWE_RXIO, (uint32_t *)&rxbuf[0], rlen >> 2);
	memcpy(mtod(m, char *), (char *)&rxbuf[0], pktlen);

	/* push the packet up */
#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif
	ether_input_mbuf(ifp, m);
	goto trynext;
err_out:
	ifp->if_ierrors++;
}

int
awe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct awe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		if (!(ifp->if_flags & IFF_UP)) {
			ifp->if_flags |= IFF_UP;
			awe_init(sc);
		}
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				awe_init(sc);
		} else if (ifp->if_flags & IFF_RUNNING)
			awe_stop(sc);
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
			awe_iff(sc, ifp);
		error = 0;
	}

	splx(s);
	return error;
}

struct mbuf *
awe_newbuf(void)
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

void
awe_iff(struct awe_softc *sc, struct ifnet *ifp)
{
	/* XXX set interface features */
}

/*
 * MII
 */
int
awe_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct awe_softc *sc = (struct awe_softc *)dev;
	int timo = AWE_MII_TIMEOUT;

	AWWRITE4(sc, AWE_MACMADR, phy << 8 | reg);

	AWWRITE4(sc, AWE_MACMCMD, 1);
	while (AWREAD4(sc, AWE_MACMIND) & 1 && --timo)
		delay(10);
#ifdef DIAGNOSTIC
	if (!timo)
		printf("%s: awe_miibus_readreg timeout.\n",
		    sc->sc_dev.dv_xname);
#endif

	AWWRITE4(sc, AWE_MACMCMD, 0);
	
	return AWREAD4(sc, AWE_MACMRDD) & 0xffff;
}

void
awe_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct awe_softc *sc = (struct awe_softc *)dev;
	int timo = AWE_MII_TIMEOUT;

	AWWRITE4(sc, AWE_MACMADR, phy << 8 | reg);

	AWWRITE4(sc, AWE_MACMCMD, 1);
	while (AWREAD4(sc, AWE_MACMIND) & 1 && --timo)
		delay(10);
#ifdef DIAGNOSTIC
	if (!timo)
		printf("%s: awe_miibus_readreg timeout.\n",
		    sc->sc_dev.dv_xname);
#endif

	AWWRITE4(sc, AWE_MACMCMD, 0);

	AWWRITE4(sc, AWE_MACMWTD, val);
}

void
awe_miibus_statchg(struct device *dev)
{
	/* XXX */
#if 0
	struct awe_softc *sc = (struct awe_softc *)dev;

	switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
	case IFM_10_T:
	case IFM_100_TX:
	/*case IFM_1000_T: only on GMAC */
		break;
	default:
		break;
	}
#endif
}

int
awe_ifm_change(struct ifnet *ifp)
{
	struct awe_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;

	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	return mii_mediachg(mii);
}

void
awe_ifm_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct awe_softc *sc = (struct awe_softc *)ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}
