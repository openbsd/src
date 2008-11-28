/*	$OpenBSD: if_mc.c,v 1.19 2008/11/28 02:44:17 brad Exp $	*/
/*	$NetBSD: if_mc.c,v 1.24 2004/10/30 18:08:34 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 David Huang <khym@azeotrope.org>
 * All rights reserved.
 *
 * Portions of this code are based on code by Denton Gentry <denny1@home.com>,
 * Charles M. Hannum, Yanagisawa Takeshi <yanagisw@aa.ap.titech.ac.jp>, and
 * Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

/*
 * Driver for the AMD Am79C940 (MACE) ethernet chip, used for onboard
 * ethernet on the Centris/Quadra 660av and Quadra 840av.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <uvm/uvm_extern.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#include <mac68k/dev/if_mcreg.h>
#include <mac68k/dev/if_mcvar.h>

struct cfdriver mc_cd = {
	NULL, "mc", DV_IFNET
};

void	mcwatchdog(struct ifnet *);
int	mcinit(struct mc_softc *sc);
int	mcstop(struct mc_softc *sc);
int	mcioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
void	mcstart(struct ifnet *ifp);
void	mcreset(struct mc_softc *sc);

u_int	maceput(struct mc_softc *sc, struct mbuf *m0);
void	mc_tint(struct mc_softc *sc);
void	mace_read(struct mc_softc *, caddr_t, int);
struct mbuf *mace_get(struct mc_softc *, caddr_t, int);
static void mace_calcladrf(struct arpcom *ac, u_int8_t *af);
static inline u_int16_t ether_cmp(void *, void *);


/*
 * Compare two Ether/802 addresses for equality, inlined and
 * unrolled for speed.  Use this like bcmp().
 *
 * XXX: Add <machine/inlines.h> for stuff like this?
 * XXX: or maybe add it to libkern.h instead?
 *
 * "I'd love to have an inline assembler version of this."
 * XXX: Who wanted that? mycroft?  I wrote one, but this
 * version in C is as good as hand-coded assembly. -gwr
 *
 * Please do NOT tweak this without looking at the actual
 * assembly code generated before and after your tweaks!
 */
static inline u_int16_t
ether_cmp(one, two)
	void *one, *two;
{
	register u_int16_t *a = (u_short *) one;
	register u_int16_t *b = (u_short *) two;
	register u_int16_t diff;

#ifdef	m68k
	/*
	 * The post-increment-pointer form produces the best
	 * machine code for m68k.  This was carefully tuned
	 * so it compiles to just 8 short (2-byte) op-codes!
	 */
	diff  = *a++ - *b++;
	diff |= *a++ - *b++;
	diff |= *a++ - *b++;
#else
	/*
	 * Most modern CPUs do better with a single expression.
	 * Note that short-cut evaluation is NOT helpful here,
	 * because it just makes the code longer, not faster!
	 */
	diff = (a[0] - b[0]) | (a[1] - b[1]) | (a[2] - b[2]);
#endif

	return (diff);
}

#define ETHER_CMP	ether_cmp

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
int
mcsetup(sc, lladdr)
	struct mc_softc	*sc;
	u_int8_t *lladdr;
{
	struct ifnet *ifp = &sc->sc_if;

	/* reset the chip and disable all interrupts */
	NIC_PUT(sc, MACE_BIUCC, SWRST);
	DELAY(100);
	NIC_PUT(sc, MACE_IMR, ~0);

	bcopy(lladdr, sc->sc_enaddr, ETHER_ADDR_LEN);
	bcopy(sc->sc_enaddr, sc->sc_ethercom.ac_enaddr, ETHER_ADDR_LEN);
	printf(": address %s\n", ether_sprintf(lladdr));

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_ioctl = mcioctl;
	ifp->if_start = mcstart;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	ifp->if_watchdog = mcwatchdog;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp);

	return (0);
}

int
mcioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct mc_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *) data;
	int s, err = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			mcinit(sc);
			arp_ifinit(&sc->sc_ethercom, ifa);
			break;
#endif
		default:
			mcinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * then stop it.
			 */
			mcstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped,
			 * then start it.
			 */
			(void)mcinit(sc);
		} else {
			/*
			 * reset the interface to pick up any other changes
			 * in flags
			 */
			mcreset(sc);
			mcstart(ifp);
		}
		break;

	default:
		err = ether_ioctl(ifp, &sc->sc_ethercom, cmd, data);
	}

	if (err == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			mcreset(sc);
		err = 0;
	}

	splx(s);
	return (err);
}

/*
 * Encapsulate a packet of type family for the local net.
 */
void
mcstart(ifp)
	struct ifnet *ifp;
{
	struct mc_softc	*sc = ifp->if_softc;
	struct mbuf	*m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	while (1) {
		if (ifp->if_flags & IFF_OACTIVE)
			return;

		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			return;

#if NBPFILTER > 0
		/*
		 * If bpf is listening on this interface, let it
		 * see the packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		ifp->if_flags |= IFF_OACTIVE;
		maceput(sc, m);

		ifp->if_opackets++;		/* # of pkts */
	}
}

/*
 * reset and restart the MACE.  Called in case of fatal
 * hardware/software errors.
 */
void
mcreset(sc)
	struct mc_softc *sc;
{
	mcstop(sc);
	mcinit(sc);
}

int
mcinit(sc)
	struct mc_softc *sc;
{
	int s;
	u_int8_t maccc, ladrf[8];

	if (sc->sc_if.if_flags & IFF_RUNNING)
		/* already running */
		return (0);

	s = splnet();

	NIC_PUT(sc, MACE_BIUCC, sc->sc_biucc);
	NIC_PUT(sc, MACE_FIFOCC, sc->sc_fifocc);
	NIC_PUT(sc, MACE_IMR, ~0); /* disable all interrupts */
	NIC_PUT(sc, MACE_PLSCC, sc->sc_plscc);

	NIC_PUT(sc, MACE_UTR, RTRD); /* disable reserved test registers */

	/* set MAC address */
	NIC_PUT(sc, MACE_IAC, ADDRCHG);
	while (NIC_GET(sc, MACE_IAC) & ADDRCHG)
		;
	NIC_PUT(sc, MACE_IAC, PHYADDR);
	bus_space_write_multi_1(sc->sc_regt, sc->sc_regh, MACE_REG(MACE_PADR),
	    sc->sc_enaddr, ETHER_ADDR_LEN);

	/* set logical address filter */
	mace_calcladrf(&sc->sc_ethercom, ladrf);

	NIC_PUT(sc, MACE_IAC, ADDRCHG);
	while (NIC_GET(sc, MACE_IAC) & ADDRCHG)
		;
	NIC_PUT(sc, MACE_IAC, LOGADDR);
	bus_space_write_multi_1(sc->sc_regt, sc->sc_regh, MACE_REG(MACE_LADRF),
	    ladrf, 8);

	NIC_PUT(sc, MACE_XMTFC, APADXMT);
	/*
	 * No need to autostrip padding on receive... Ethernet frames
	 * don't have a length field, unlike 802.3 frames, so the MACE
	 * can't figure out the length of the packet anyways.
	 */
	NIC_PUT(sc, MACE_RCVFC, 0);

	maccc = ENXMT | ENRCV;
	if (sc->sc_if.if_flags & IFF_PROMISC)
		maccc |= PROM;

	NIC_PUT(sc, MACE_MACCC, maccc);

	if (sc->sc_bus_init)
		(*sc->sc_bus_init)(sc);

	/*
	 * Enable all interrupts except receive, since we use the DMA
	 * completion interrupt for that.
	 */
	NIC_PUT(sc, MACE_IMR, RCVINTM);

	/* flag interface as "running" */
	sc->sc_if.if_flags |= IFF_RUNNING;
	sc->sc_if.if_flags &= ~IFF_OACTIVE;

	splx(s);
	return (0);
}

/*
 * close down an interface and free its buffers
 * Called on final close of device, or if mcinit() fails
 * part way through.
 */
int
mcstop(sc)
	struct mc_softc *sc;
{
	int	s = splnet();

	NIC_PUT(sc, MACE_BIUCC, SWRST);
	DELAY(100);

	sc->sc_if.if_timer = 0;
	sc->sc_if.if_flags &= ~IFF_RUNNING;

	splx(s);
	return (0);
}

/*
 * Called if any Tx packets remain unsent after 5 seconds,
 * In all cases we just reset the chip, and any retransmission
 * will be handled by higher level protocol timeouts.
 */
void
mcwatchdog(ifp)
	struct ifnet *ifp;
{
	struct mc_softc *sc = ifp->if_softc;

	printf("mcwatchdog: resetting chip\n");
	mcreset(sc);
}

/*
 * stuff packet into MACE (at splnet)
 */
u_int
maceput(sc, m)
	struct mc_softc *sc;
	struct mbuf *m;
{
	struct mbuf *n;
	u_int len, totlen = 0;
	u_char *buff;

	buff = sc->sc_txbuf;

	for (; m; m = n) {
		u_char *data = mtod(m, u_char *);
		len = m->m_len;
		totlen += len;
		bcopy(data, buff, len);
		buff += len;
		MFREE(m, n);
	}

	if (totlen > PAGE_SIZE)
		panic("%s: maceput: packet overflow", sc->sc_dev.dv_xname);

#if 0
	if (totlen < ETHERMIN + sizeof(struct ether_header)) {
		int pad = ETHERMIN + sizeof(struct ether_header) - totlen;
		bzero(sc->sc_txbuf + totlen, pad);
		totlen = ETHERMIN + sizeof(struct ether_header);
	}
#endif

	(*sc->sc_putpacket)(sc, totlen);

	sc->sc_if.if_timer = 5;	/* 5 seconds to watch for failing to transmit */
	return (totlen);
}

void
mcintr(arg)
	void *arg;
{
struct mc_softc *sc = arg;
	u_int8_t ir;

	ir = NIC_GET(sc, MACE_IR) & ~NIC_GET(sc, MACE_IMR);
	if (ir & JAB) {
#ifdef MCDEBUG
		printf("%s: jabber error\n", sc->sc_dev.dv_xname);
#endif
		sc->sc_if.if_oerrors++;
	}

	if (ir & BABL) {
#ifdef MCDEBUG
		printf("%s: babble\n", sc->sc_dev.dv_xname);
#endif
		sc->sc_if.if_oerrors++;
	}

	if (ir & CERR) {
#ifdef MCDEBUG
		printf("%s: collision error\n", sc->sc_dev.dv_xname);
#endif
		sc->sc_if.if_collisions++;
	}

	/*
	 * Pretend we have carrier; if we don't this will be cleared
	 * shortly.
	 */
	sc->sc_havecarrier = 1;

	if (ir & XMTINT)
		mc_tint(sc);

	if (ir & RCVINT)
		mc_rint(sc);
}

void
mc_tint(sc)
	struct mc_softc *sc;
{
	u_int8_t xmtrc, xmtfs;

	xmtrc = NIC_GET(sc, MACE_XMTRC);
	xmtfs = NIC_GET(sc, MACE_XMTFS);

	if ((xmtfs & XMTSV) == 0)
		return;

	if (xmtfs & UFLO) {
		printf("%s: underflow\n", sc->sc_dev.dv_xname);
		mcreset(sc);
		return;
	}

	if (xmtfs & LCOL) {
		printf("%s: late collision\n", sc->sc_dev.dv_xname);
		sc->sc_if.if_oerrors++;
		sc->sc_if.if_collisions++;
	}

	if (xmtfs & MORE)
		/* Real number is unknown. */
		sc->sc_if.if_collisions += 2;
	else if (xmtfs & ONE)
		sc->sc_if.if_collisions++;
	else if (xmtfs & RTRY) {
		printf("%s: excessive collisions\n", sc->sc_dev.dv_xname);
		sc->sc_if.if_collisions += 16;
		sc->sc_if.if_oerrors++;
	}

	if (xmtfs & LCAR) {
		sc->sc_havecarrier = 0;
		printf("%s: lost carrier\n", sc->sc_dev.dv_xname);
		sc->sc_if.if_oerrors++;
	}

	sc->sc_if.if_flags &= ~IFF_OACTIVE;
	sc->sc_if.if_timer = 0;
	mcstart(&sc->sc_if);
}

void
mc_rint(sc)
	struct mc_softc *sc;
{
#define	rxf	sc->sc_rxframe
	u_int len;

	len = (rxf.rx_rcvcnt | ((rxf.rx_rcvsts & 0xf) << 8)) - 4;

#ifdef MCDEBUG
	if (rxf.rx_rcvsts & 0xf0)
		printf("%s: rcvcnt %02x rcvsts %02x rntpc 0x%02x rcvcc 0x%02x\n",
		    sc->sc_dev.dv_xname, rxf.rx_rcvcnt, rxf.rx_rcvsts,
		    rxf.rx_rntpc, rxf.rx_rcvcc);
#endif

	if (rxf.rx_rcvsts & OFLO) {
#ifdef MCDEBUG
		printf("%s: receive FIFO overflow\n", sc->sc_dev.dv_xname);
#endif
		sc->sc_if.if_ierrors++;
		return;
	}

	if (rxf.rx_rcvsts & CLSN)
		sc->sc_if.if_collisions++;

	if (rxf.rx_rcvsts & FRAM) {
#ifdef MCDEBUG
		printf("%s: framing error\n", sc->sc_dev.dv_xname);
#endif
		sc->sc_if.if_ierrors++;
		return;
	}

	if (rxf.rx_rcvsts & FCS) {
#ifdef MCDEBUG
		printf("%s: frame control checksum error\n", sc->sc_dev.dv_xname);
#endif
		sc->sc_if.if_ierrors++;
		return;
	}

	mace_read(sc, rxf.rx_frame, len);
#undef	rxf
}

void
mace_read(sc, pkt, len)
	struct mc_softc *sc;
	caddr_t pkt;
	int len;
{
	struct ifnet *ifp = &sc->sc_if;
	struct mbuf *m;

	if (len <= sizeof(struct ether_header) ||
	    len > ETHERMTU + sizeof(struct ether_header)) {
#ifdef MCDEBUG
		printf("%s: invalid packet size %d; dropping\n",
		    sc->sc_dev.dv_xname, len);
#endif
		ifp->if_ierrors++;
		return;
	}

	m = mace_get(sc, pkt, len);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	/* Pass the packet to any BPF listeners. */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

	/* Pass the packet up. */
	ether_input_mbuf(ifp, m);
}

/*
 * Pull data off an interface.
 * Len is length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
struct mbuf *
mace_get(sc, pkt, totlen)
	struct mc_softc *sc;
	caddr_t pkt;
	int totlen;
{
	register struct mbuf *m;
	struct mbuf *top, **mp;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	m->m_pkthdr.rcvif = &sc->sc_if;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return (NULL);
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m_freem(top);
				return (NULL);
			}
			len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		bcopy(pkt, mtod(m, caddr_t), len);
		pkt += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}

/*
 * Go through the list of multicast addresses and calculate the logical
 * address filter.
 */
void
mace_calcladrf(ac, af)
	struct arpcom *ac;
	u_int8_t *af;
{
	struct ifnet *ifp = &ac->ac_if;
	struct ether_multi *enm;
	register u_int32_t crc;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	*((u_int32_t *)af) = *((u_int32_t *)af + 1) = 0;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (ETHER_CMP(enm->enm_addrlo, enm->enm_addrhi)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		crc = ether_crc32_le(enm->enm_addrlo, sizeof(enm->enm_addrlo));

		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Set the corresponding bit in the filter. */
		af[crc >> 3] |= 1 << (crc & 7);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	*((u_int32_t *)af) = *((u_int32_t *)af + 1) = 0xffffffff;
}

static u_char bbr4[] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
#define bbr(v)  ((bbr4[(v)&0xf] << 4) | bbr4[((v)>>4) & 0xf])

u_char
mc_get_enaddr(t, h, o, dst)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_char *dst;
{
	int	i;
	u_char	b, csum;

	/*
	 * The XOR of the 8 bytes of the ROM must be 0xff for it to be
	 * valid
	*/
	for (i = 0, csum = 0; i < 8; i++) {
		b = bus_space_read_1(t, h, o+16*i);
		if (i < ETHER_ADDR_LEN)
			dst[i] = bbr(b);
		csum ^= b;
	}

	return csum;
}
