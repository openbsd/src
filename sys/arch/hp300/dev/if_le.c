/*	$NetBSD: if_le.c,v 1.22 1995/08/04 08:08:41 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	@(#)if_le.c	7.6 (Berkeley) 5/8/91
 */

#include "le.h"
#if NLE > 0

#include "bpfilter.h"

/*
 * AMD 7990 LANCE
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <hp300/hp300/isr.h>
#ifdef USELEDS
#include <hp300/hp300/led.h>
#endif

#include <hp300/dev/device.h>
#include <hp300/dev/if_lereg.h>


#define	ETHER_MIN_LEN	64
#define	ETHER_MAX_LEN	1518
#define	ETHER_ADDR_LEN	6


/* offsets for:	   ID,   REGS,    MEM,  NVRAM */
int	lestd[] = { 0, 0x4000, 0x8000, 0xC008 };

struct	isr le_isr[NLE];

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * arpcom.ac_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	le_softc {
	struct	arpcom sc_arpcom;	/* common Ethernet structures */
	struct	lereg0 *sc_r0;		/* DIO registers */
	struct	lereg1 *sc_r1;		/* LANCE registers */
	void	*sc_mem;
	struct	init_block *sc_init;
	struct	mds *sc_rd, *sc_td;
	u_char	*sc_rbuf, *sc_tbuf;
	int	sc_last_rd, sc_last_td;
	int	sc_no_td;
#ifdef LEDEBUG
	int	sc_debug;
#endif
} le_softc[NLE];

int leintr __P((int));
int leioctl __P((struct ifnet *, u_long, caddr_t));
void lestart __P((struct ifnet *));
void lewatchdog __P((int));
static inline void lewrcsr __P((/* struct le_softc *, u_short, u_short */));
static inline u_short lerdcsr __P((/* struct le_softc *, u_short */));
void leinit __P((struct le_softc *));
void lememinit __P((struct le_softc *));
void lereset __P((struct le_softc *));
void lestop __P((struct le_softc *));
void letint __P((int));
void lerint __P((int));
void leread __P((struct le_softc *, u_char *, int));
struct mbuf *leget __P((u_char *, int, struct ifnet *));
#ifdef LEDEBUG
void recv_print __P((struct le_softc *, int));
void xmit_print __P((struct le_softc *, int));
#endif
void lesetladrf __P((struct arpcom *, u_long *));

int leattach __P((struct hp_device *));

struct	driver ledriver = {
	leattach, "le",
};

static inline void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	register u_short port;
	register u_short val;
{
	register struct lereg0 *ler0 = sc->sc_r0;
	register struct lereg1 *ler1 = sc->sc_r1;

	do {
		ler1->ler1_rap = port;
	} while ((ler0->ler0_status & LE_ACK) == 0);
	do {
		ler1->ler1_rdp = val;
	} while ((ler0->ler0_status & LE_ACK) == 0);
}

static inline u_short
lerdcsr(sc, port)
	struct le_softc *sc;
	register u_short port;
{
	register struct lereg0 *ler0 = sc->sc_r0;
	register struct lereg1 *ler1 = sc->sc_r1;
	register u_short val;

	do {
		ler1->ler1_rap = port;
	} while ((ler0->ler0_status & LE_ACK) == 0);
	do {
		val = ler1->ler1_rdp;
	} while ((ler0->ler0_status & LE_ACK) == 0);
	return (val);
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
int
leattach(hd)
	struct hp_device *hd;
{
	register struct lereg0 *ler0;
	struct le_softc *sc = &le_softc[hd->hp_unit];
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	char *cp;
	int i;

	ler0 = sc->sc_r0 = (struct lereg0 *)(lestd[0] + (int)hd->hp_addr);
	if (ler0->ler0_id != LEID)
		return(0);
	sc->sc_r1 = (struct lereg1 *)(lestd[1] + (int)hd->hp_addr);
	sc->sc_mem = (void *)(lestd[2] + (int)hd->hp_addr);
	le_isr[hd->hp_unit].isr_intr = leintr;
	hd->hp_ipl = le_isr[hd->hp_unit].isr_ipl = LE_IPL(ler0->ler0_status);
	le_isr[hd->hp_unit].isr_arg = hd->hp_unit;
	ler0->ler0_id = 0xFF;
	DELAY(100);

	/*
	 * Read the ethernet address off the board, one nibble at a time.
	 */
	cp = (char *)(lestd[3] + (int)hd->hp_addr);
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++) {
		sc->sc_arpcom.ac_enaddr[i] = (*++cp & 0xF) << 4;
		cp++;
		sc->sc_arpcom.ac_enaddr[i] |= *++cp & 0xF;
		cp++;
	}
	printf("le%d: hardware address %s\n", hd->hp_unit,
		ether_sprintf(sc->sc_arpcom.ac_enaddr));

	isrlink(&le_isr[hd->hp_unit]);
	ler0->ler0_status = LE_IE;

	ifp->if_unit = hd->hp_unit;
	ifp->if_name = "le";
	ifp->if_output = ether_output;
	ifp->if_start = lestart;
	ifp->if_ioctl = leioctl;
	ifp->if_watchdog = lewatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	return (1);
}

void
lereset(sc)
	struct le_softc *sc;
{

	leinit(sc);
}

void
lewatchdog(unit)
	int unit;
{
	struct le_softc *sc = &le_softc[unit];

	log(LOG_ERR, "le%d: device timeout\n", unit);
	++sc->sc_arpcom.ac_if.if_oerrors;

	lereset(sc);
}

#define	LANCE_ADDR(sc, a) \
	((u_long)(a) - (u_long)sc->sc_mem)

/* LANCE initialization block set up. */
void
lememinit(sc)
	register struct le_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i;
	void *mem;
	u_long a;

	/*
	 * At this point we assume that the memory allocated to the Lance is
	 * quadword aligned.  If it isn't then the initialisation is going
	 * fail later on.
	 */
	mem = sc->sc_mem;

	sc->sc_init = mem;
#if NBPFILTER > 0
	if (ifp->if_flags & IFF_PROMISC)
		sc->sc_init->mode = LE_NORMAL | LE_PROM;
	else
#endif
		sc->sc_init->mode = LE_NORMAL;
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_init->padr[i] = sc->sc_arpcom.ac_enaddr[i^1];
	lesetladrf(&sc->sc_arpcom, sc->sc_init->ladrf);
	mem += sizeof(struct init_block);

	sc->sc_rd = mem;
	a = LANCE_ADDR(sc, mem);
	sc->sc_init->rdra = a;
	sc->sc_init->rlen = ((a >> 16) & 0xff) | (RLEN << 13);
	mem += NRBUF * sizeof(struct mds);

	sc->sc_td = mem;
	a = LANCE_ADDR(sc, mem);
	sc->sc_init->tdra = a;
	sc->sc_init->tlen = ((a >> 16) & 0xff) | (TLEN << 13);
	mem += NTBUF * sizeof(struct mds);

	/* 
	 * Set up receive ring descriptors.
	 */
	sc->sc_rbuf = mem;
	for (i = 0; i < NRBUF; i++) {
		a = LANCE_ADDR(sc, mem);
		sc->sc_rd[i].addr = a;
		sc->sc_rd[i].flags = ((a >> 16) & 0xff) | LE_OWN;
		sc->sc_rd[i].bcnt = -BUFSIZE;
		sc->sc_rd[i].mcnt = 0;
		mem += BUFSIZE;
	}

	/* 
	 * Set up transmit ring descriptors.
	 */
	sc->sc_tbuf = mem;
	for (i = 0; i < NTBUF; i++) {
		a = LANCE_ADDR(sc, mem);
		sc->sc_td[i].addr = a;
		sc->sc_td[i].flags= ((a >> 16) & 0xff);
		sc->sc_td[i].bcnt = 0xf000;
		sc->sc_td[i].mcnt = 0;
		mem += BUFSIZE;
	}
}

void
lestop(sc)
	struct le_softc *sc;
{

	lewrcsr(sc, 0, LE_STOP);
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
void
leinit(sc)
	register struct le_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int s;
	register int timo;
	u_long a;

	s = splimp();

	/* Don't want to get in a weird state. */
	lewrcsr(sc, 0, LE_STOP);
	DELAY(100);

	sc->sc_last_rd = sc->sc_last_td = sc->sc_no_td = 0;

	/* Set up LANCE init block. */
	lememinit(sc);

	/* Turn on byte swapping. */
	lewrcsr(sc, 3, LE_BSWP);

	/* Give LANCE the physical address of its init block. */
	a = LANCE_ADDR(sc, sc->sc_init);
	lewrcsr(sc, 1, a);
	lewrcsr(sc, 2, (a >> 16) & 0xff);

	/* Try to initialize the LANCE. */
	DELAY(100);
	lewrcsr(sc, 0, LE_INIT);

	/* Wait for initialization to finish. */
	for (timo = 100000; timo; timo--)
		if (lerdcsr(sc, 0) & LE_IDON)
			break;

	if (lerdcsr(sc, 0) & LE_IDON) {
		/* Start the LANCE. */
		lewrcsr(sc, 0, LE_INEA | LE_STRT | LE_IDON);
		ifp->if_flags |= IFF_RUNNING;
		ifp->if_flags &= ~IFF_OACTIVE;
		lestart(ifp);
	} else
		printf("le%d: card failed to initialize\n", ifp->if_unit);

	(void) splx(s);
}

/*
 * Controller interrupt.
 */
int
leintr(unit)
	int unit;
{
	register struct le_softc *sc = &le_softc[unit];
	register u_short isr;

	isr = lerdcsr(sc, 0);
#ifdef LEDEBUG
	if (sc->sc_debug)
		printf("le%d: leintr entering with isr=%04x\n",
		    unit, isr);
#endif
	if ((isr & LE_INTR) == 0)
		return 0;

	do {
		lewrcsr(sc, 0,
		    isr & (LE_INEA | LE_BABL | LE_MISS | LE_MERR |
			   LE_RINT | LE_TINT | LE_IDON));
		if (isr & (LE_BABL | LE_CERR | LE_MISS | LE_MERR)) {
			if (isr & LE_BABL) {
				printf("le%d: BABL\n", unit);
				sc->sc_arpcom.ac_if.if_oerrors++;
			}
#if 0
			if (isr & LE_CERR) {
				printf("le%d: CERR\n", unit);
				sc->sc_arpcom.ac_if.if_collisions++;
			}
#endif
			if (isr & LE_MISS) {
#if 0
				printf("le%d: MISS\n", unit);
#endif
				sc->sc_arpcom.ac_if.if_ierrors++;
			}
			if (isr & LE_MERR) {
				printf("le%d: MERR\n", unit);
				lereset(sc);
				goto out;
			}
		}

		if ((isr & LE_RXON) == 0) {
			printf("le%d: receiver disabled\n", unit);
			sc->sc_arpcom.ac_if.if_ierrors++;
			lereset(sc);
			goto out;
		}
		if ((isr & LE_TXON) == 0) {
			printf("le%d: transmitter disabled\n", unit);
			sc->sc_arpcom.ac_if.if_oerrors++;
			lereset(sc);
			goto out;
		}

		if (isr & LE_RINT) {
			/* Reset watchdog timer. */
			sc->sc_arpcom.ac_if.if_timer = 0;
			lerint(unit);
		}
		if (isr & LE_TINT) {
			/* Reset watchdog timer. */
			sc->sc_arpcom.ac_if.if_timer = 0;
			letint(unit);
		}

		isr = lerdcsr(sc, 0);
	} while ((isr & LE_INTR) != 0);

#ifdef LEDEBUG
	if (sc->sc_debug)
		printf("le%d: leintr returning with isr=%04x\n",
		    unit, isr);
#endif

out:
	return 1;
}

#define NEXTTDS \
	if (++tmd == NTBUF) tmd=0, cdm=sc->sc_td; else ++cdm
	
/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue, and map it to the
 * interface before starting the output.
 * Called only at splimp or interrupt level.
 */
void
lestart(ifp)
	struct ifnet *ifp;
{
	register struct le_softc *sc = &le_softc[ifp->if_unit];
	register int tmd;
	struct mds *cdm;
	struct mbuf *m0, *m;
	u_char *buffer;
	int len;

	if ((sc->sc_arpcom.ac_if.if_flags & (IFF_RUNNING | IFF_OACTIVE)) !=
	    IFF_RUNNING)
		return;

	tmd = sc->sc_last_td;
	cdm = &sc->sc_td[tmd];

	for (;;) {
		if (sc->sc_no_td >= NTBUF) {
			sc->sc_arpcom.ac_if.if_flags |= IFF_OACTIVE;
#ifdef LEDEBUG
			if (sc->sc_debug)
				printf("no_td = %d, last_td = %d\n", sc->sc_no_td,
				    sc->sc_last_td);
#endif
			break;
		}

#ifdef LEDEBUG
		if (cdm->flags & LE_OWN) {
			sc->sc_arpcom.ac_if.if_flags |= IFF_OACTIVE;
			printf("missing buffer, no_td = %d, last_td = %d\n",
			    sc->sc_no_td, sc->sc_last_td);
		}
#endif

		IF_DEQUEUE(&sc->sc_arpcom.ac_if.if_snd, m);
		if (!m)
			break;

		++sc->sc_no_td;

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		buffer = sc->sc_tbuf + (BUFSIZE * sc->sc_last_td);
		len = 0;
		for (m0 = m; m; m = m->m_next) {
			bcopy(mtod(m, caddr_t), buffer, m->m_len);
			buffer += m->m_len;
			len += m->m_len;
		}

#ifdef LEDEBUG
		if (len > ETHER_MAX_LEN)
			printf("packet length %d\n", len);
#endif

#if NBPFILTER > 0
		if (sc->sc_arpcom.ac_if.if_bpf)
			bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, m0);
#endif

		m_freem(m0);
		len = max(len, ETHER_MIN_LEN);

		/*
		 * Init transmit registers, and set transmit start flag.
		 */
		cdm->bcnt = -len;
		cdm->mcnt = 0;
		cdm->flags |= LE_OWN | LE_STP | LE_ENP;

#ifdef LEDEBUG
		if (sc->sc_debug)
			xmit_print(sc, sc->sc_last_td);
#endif
		
		lewrcsr(sc, 0, LE_INEA | LE_TDMD);

		NEXTTDS;
	}

	sc->sc_last_td = tmd;
}

void
letint(unit)
	int unit;
{
	register struct le_softc *sc = &le_softc[unit];
	register int tmd = (sc->sc_last_td - sc->sc_no_td + NTBUF) % NTBUF;
	struct mds *cdm = &sc->sc_td[tmd];

#ifdef USELEDS
	if (inledcontrol == 0)
		ledcontrol(0, 0, LED_LANXMT);
#endif

	if (cdm->flags & LE_OWN) {
		/* Race condition with loop below. */
#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("le%d: extra tint\n", unit);
#endif
		return;
	}

	sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

	do {
		if (sc->sc_no_td <= 0)
			break;
#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("trans cdm = %x\n", cdm);
#endif
		sc->sc_arpcom.ac_if.if_opackets++;
		--sc->sc_no_td;
		if (cdm->mcnt & (LE_TBUFF | LE_UFLO | LE_LCOL | LE_LCAR | LE_RTRY)) {
			if (cdm->mcnt & LE_TBUFF)
				printf("le%d: TBUFF\n", unit);
			if ((cdm->mcnt & (LE_TBUFF | LE_UFLO)) == LE_UFLO)
				printf("le%d: UFLO\n", unit);
			if (cdm->mcnt & LE_UFLO) {
				lereset(sc);
				return;
			}
#if 0
			if (cdm->mcnt & LE_LCOL) {
				printf("le%d: late collision\n", unit);
				sc->sc_arpcom.ac_if.if_collisions++;
			}
			if (cdm->mcnt & LE_LCAR)
				printf("le%d: lost carrier\n", unit);
			if (cdm->mcnt & LE_RTRY) {
				printf("le%d: excessive collisions, tdr %d\n",
				    unit, cdm->mcnt & 0x1ff);
				sc->sc_arpcom.ac_if.if_collisions += 16;
			}
#endif
		} else if (cdm->flags & LE_ONE)
			sc->sc_arpcom.ac_if.if_collisions++;
		else if (cdm->flags & LE_MORE)
			/* Real number is unknown. */
			sc->sc_arpcom.ac_if.if_collisions += 2;
		NEXTTDS;
	} while ((cdm->flags & LE_OWN) == 0);

	lestart(&sc->sc_arpcom.ac_if);
}

#define NEXTRDS \
	if (++rmd == NRBUF) rmd=0, cdm=sc->sc_rd; else ++cdm
	
/* only called from one place, so may as well integrate */
void
lerint(unit)
	int unit;
{
	register struct le_softc *sc = &le_softc[unit];
	register int rmd = sc->sc_last_rd;
	struct mds *cdm = &sc->sc_rd[rmd];

#ifdef USELEDS
	if (inledcontrol == 0)
		ledcontrol(0, 0, LED_LANRCV);
#endif

	if (cdm->flags & LE_OWN) {
		/* Race condition with loop below. */
#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("le%d: extra rint\n", unit);
#endif
		return;
	}

	/* Process all buffers with valid data. */
	do {
		if (cdm->flags & (LE_FRAM | LE_OFLO | LE_CRC | LE_RBUFF)) {
			if ((cdm->flags & (LE_FRAM | LE_OFLO | LE_ENP)) == (LE_FRAM | LE_ENP))
				printf("le%d: FRAM\n", unit);
			if ((cdm->flags & (LE_OFLO | LE_ENP)) == LE_OFLO)
				printf("le%d: OFLO\n", unit);
			if ((cdm->flags & (LE_CRC | LE_OFLO | LE_ENP)) == (LE_CRC | LE_ENP))
				printf("le%d: CRC\n", unit);
			if (cdm->flags & LE_RBUFF)
				printf("le%d: RBUFF\n", unit);
		} else if (cdm->flags & (LE_STP | LE_ENP) != (LE_STP | LE_ENP)) {
			do {
				cdm->mcnt = 0;
				cdm->flags |= LE_OWN;
				NEXTRDS;
			} while ((cdm->flags & (LE_OWN | LE_ERR | LE_STP | LE_ENP)) == 0);
			sc->sc_last_rd = rmd;
			printf("le%d: chained buffer\n", unit);
			if ((cdm->flags & (LE_OWN | LE_ERR | LE_STP | LE_ENP)) != LE_ENP) {
				lereset(sc);
				return;
			}
		} else {
#ifdef LEDEBUG
			if (sc->sc_debug)
				recv_print(sc, sc->sc_last_rd);
#endif
			leread(sc, sc->sc_rbuf + (BUFSIZE * rmd),
			    (int)cdm->mcnt);
			sc->sc_arpcom.ac_if.if_ipackets++;
		}
			
		cdm->mcnt = 0;
		cdm->flags |= LE_OWN;
		NEXTRDS;
#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("sc->sc_last_rd = %x, cdm = %x\n",
			    sc->sc_last_rd, cdm);
#endif
	} while ((cdm->flags & LE_OWN) == 0);

	sc->sc_last_rd = rmd;
}

/*
 * Pass a packet to the higher levels.
 */
void
leread(sc, buf, len)
	register struct le_softc *sc;
	u_char *buf;
	int len;
{
	struct ifnet *ifp;
	struct mbuf *m;
	struct ether_header *eh;

	len -= 4;
	if (len <= 0)
		return;

	/* Pull packet off interface. */
	ifp = &sc->sc_arpcom.ac_if;
	m = leget(buf, len, ifp);
	if (m == 0)
		return;

	/* We assume that the header fit entirely in one mbuf. */
	eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf) {
		bpf_mtap(ifp->if_bpf, m);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((ifp->if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    bcmp(eh->ether_dhost, sc->sc_arpcom.ac_enaddr,
			    sizeof(eh->ether_dhost)) != 0) {
			m_freem(m);
			return;
		}
	}
#endif

	/* We assume that the header fit entirely in one mbuf. */
	m->m_pkthdr.len -= sizeof(*eh);
	m->m_len -= sizeof(*eh);
	m->m_data += sizeof(*eh);

	ether_input(ifp, eh, m);
}

/*
 * Supporting routines
 */

/*
 * Pull data off an interface.
 * Len is length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
struct mbuf *
leget(buf, totlen, ifp)
	u_char *buf;
	int totlen;
	struct ifnet *ifp;
{
	struct mbuf *top, **mp, *m;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return 0;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return 0;
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		bcopy((caddr_t)buf, mtod(m, caddr_t), len);
		buf += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return top;
}

/*
 * Process an ioctl request.
 */
int
leioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct le_softc *sc = &le_softc[ifp->if_unit];
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			leinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(sc->sc_arpcom.ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			/* Set new address. */
			leinit(sc);
			break;
		    }
#endif
		default:
			leinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		/*
		 * If interface is marked down and it is running, then stop it
		 */
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			lestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    	   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			leinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			/*lestop(sc);*/
			leinit(sc);
		}
#ifdef LEDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = 1;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom):
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			leinit(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;
	}
	(void) splx(s);
	return error;
}

#ifdef LEDEBUG
void
recv_print(sc, no)
	struct le_softc *sc;
	int no;
{
	struct mds *rmd;
	int i, printed = 0;
	u_short len;
	
	rmd = &sc->sc_rd[no];
	len = rmd->mcnt;
	printf("%s: receive buffer %d, len = %d\n", sc->sc_dev.dv_xname, no,
	    len);
	printf("%s: status %x\n", sc->sc_dev.dv_xname, lerdcsr(sc, 0));
	for (i = 0; i < len; i++) {
		if (!printed) {
			printed = 1;
			printf("%s: data: ", sc->sc_dev.dv_xname);
		}
		printf("%x ", *(sc->sc_rbuf + (BUFSIZE*no) + i));
	}
	if (printed)
		printf("\n");
}
		
void
xmit_print(sc, no)
	struct le_softc *sc;
	int no;
{
	struct mds *rmd;
	int i, printed=0;
	u_short len;
	
	rmd = &sc->sc_td[no];
	len = -rmd->bcnt;
	printf("%s: transmit buffer %d, len = %d\n", sc->sc_dev.dv_xname, no,
	    len);
	printf("%s: status %x\n", sc->sc_dev.dv_xname, lerdcsr(sc, 0));
	printf("%s: addr %x, flags %x, bcnt %x, mcnt %x\n",
	    sc->sc_dev.dv_xname, rmd->addr, rmd->flags, rmd->bcnt, rmd->mcnt);
	for (i = 0; i < len; i++)  {
		if (!printed) {
			printed = 1;
			printf("%s: data: ", sc->sc_dev.dv_xname);
		}
		printf("%x ", *(sc->sc_tbuf + (BUFSIZE*no) + i));
	}
	if (printed)
		printf("\n");
}
#endif /* LEDEBUG */

/*
 * Set up the logical address filter.
 */
void
lesetladrf(ac, af)
	struct arpcom *ac;
	u_long *af;
{
	struct ifnet *ifp = &ac->ac_if;
	struct ether_multi *enm;
	register u_char *cp, c;
	register u_long crc;
	register int i, len;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		af[0] = af[1] = 0xffffffff;
		return;
	}

	af[0] = af[1] = 0;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			af[0] = af[1] = 0xffffffff;
			return;
		}

		cp = enm->enm_addrlo;
		crc = 0xffffffff;
		for (len = sizeof(enm->enm_addrlo); --len >= 0;) {
			c = *cp++;
			for (i = 8; --i >= 0;) {
				if ((crc & 0x01) ^ (c & 0x01)) {
					crc >>= 1;
					crc ^= 0x6db88320 | 0x80000000;
				} else
					crc >>= 1;
				c >>= 1;
			}
		}
		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Turn on the corresponding bit in the filter. */
		af[crc >> 5] |= 1 << ((crc & 0x1f) ^ 16);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
}

#endif /* NLE > 0 */
