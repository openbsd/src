/*	$NetBSD: if_bah.c,v 1.13 1995/12/24 02:29:55 mycroft Exp $ */

/*
 * Copyright (c) 1994, 1995 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

/*
 * Driver for the Commodore Busines Machines ARCnet card.
 */

#define BAHASMCOPY /**/
#define BAHSOFTCOPY /**/
/* #define BAHTIMINGS /**/
/* #define BAH_DEBUG 3 /**/

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/if_arc.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <sys/kernel.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/if_bahreg.h>

/* these should be elsewhere */

#define ARC_MIN_LEN 1
#define ARC_MIN_FORBID_LEN 254
#define ARC_MAX_FORBID_LEN 256
#define ARC_MAX_LEN 508
#define ARC_ADDR_LEN 1

/* for watchdog timer. This should be more than enough. */
#define ARCTIMEOUT (5*IFNET_SLOWHZ)

/*
 * This currently uses 2 bufs for tx, 2 for rx
 *
 * New rx protocol:
 *
 * rx has a fillcount variable. If fillcount > (NRXBUF-1), 
 * rx can be switched off from rx hard int. 
 * Else rx is restarted on the other receiver.
 * rx soft int counts down. if it is == (NRXBUF-1), it restarts
 * the receiver.
 * To ensure packet ordering (we need that for 1201 later), we have a counter
 * which is incremented modulo 256 on each receive and a per buffer
 * variable, which is set to the counter on filling. The soft int can
 * compare both values to determine the older packet.
 *
 * Transmit direction:
 * 
 * bah_start checks tx_fillcount
 * case 2: return
 *
 * else fill tx_act ^ 1 && inc tx_fillcount
 *
 * check tx_fillcount again.
 * case 2: set IFF_OACTIVE to stop arc_output from filling us.
 * case 1: start tx
 *
 * tint clears IFF_OCATIVE, decrements and checks tx_fillcount
 * case 1: start tx on tx_act ^ 1, softcall bah_start
 * case 0: softcall bah_start
 *
 * #define fill(i) get mbuf && copy mbuf to chip(i)
 */

#ifdef BAHTIMINGS
/*
 * ARCnet stats; per interface.
 */
struct bah_stats {
	u_long	mincopyin;
	u_long	maxcopyin;		/* divided by byte count */
	u_long	mincopyout;
	u_long	maxcopyout;
	u_long	minsend;
	u_long	maxsend;
	u_long	lasttxstart_mics;
	struct	timeval lasttxstart_tv;
};

#error BAHTIMINGS CODE IS BROKEN; use of clkread() is bogus
#endif

/*
 * Arcnet software status per interface
 */
struct bah_softc {
	struct	device	sc_dev;
	struct	arccom	sc_arccom;	/* Common arcnet structures */
	struct	isr	sc_isr;
	struct	a2060	*sc_base;
	u_long	sc_recontime;		/* seconds only, I'm lazy */
	u_long	sc_reconcount;		/* for the above */
	u_long	sc_reconcount_excessive; /* for the above */
#define ARC_EXCESSIVE_RECONS 20
#define ARC_EXCESSIVE_RECONS_REWARN 400
	u_char	sc_bufstat[4];		/* use for packet no for rx */
	u_char	sc_intmask;
	u_char	sc_rx_packetno;
	u_char	sc_rx_act;		/* 2..3 */
	u_char	sc_tx_act;		/* 0..1 */
	u_char	sc_rx_fillcount;
	u_char	sc_tx_fillcount;
	u_char	sc_broadcast[2];	/* is it a broadcast packet? */
	u_char	sc_retransmits[2];	/* unused at the moment */
#ifdef BAHTIMINGS
	struct	bah_stats sc_stats;
#endif
};

int	bahmatch __P((struct device *, void *, void *));
void	bahattach __P((struct device *, struct device *, void *));
void	bah_init __P((struct bah_softc *));
void	bah_reset __P((struct bah_softc *));
void	bah_stop __P((struct bah_softc *));
void	bah_start __P((struct ifnet *));
int	bahintr __P((struct bah_softc *sc));
int	bah_ioctl __P((struct ifnet *, unsigned long, caddr_t));
void	bah_watchdog __P((int));
void	movepout __P((u_char *from, u_char __volatile *to, int len));
void	movepin __P((u_char __volatile *from, u_char *to, int len));
void	bah_srint __P((struct bah_softc *sc, void *dummy));
void	callstart __P((struct bah_softc *sc, void *dummy));

#ifdef BAHTIMINGS
int	clkread();
#endif

struct cfdriver bahcd = {
	NULL, "bah", bahmatch, bahattach, DV_IFNET, sizeof(struct bah_softc)
};

int
bahmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap = aux;

	if ((zap->manid == 514 || zap->manid == 1053) && zap->prodid == 9)
		return (1);

	return (0);
}

void
bahattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct bah_softc *sc = (void *)self;
	struct zbus_args *zap = aux;
	struct ifnet *ifp = &sc->sc_arccom.ac_if;
	int i, s, linkaddress;

#if (defined(BAH_DEBUG) && (BAH_DEBUG > 2))
	printf("\n%s: attach(0x%x, 0x%x, 0x%x)\n",
	    sc->sc_dev.dv_xname, parent, self, aux);
#endif
	s = splhigh();
	sc->sc_base = zap->va;

	/*
	 * read the arcnet address from the board
	 */

	sc->sc_base->kick1 = 0x0;
	sc->sc_base->kick2 = 0x0;
	DELAY(120);

	sc->sc_base->kick1 = 0xFF;
	sc->sc_base->kick2 = 0xFF;
	do {
		DELAY(120);
	} while (!(sc->sc_base->status & ARC_POR)); 

	linkaddress = sc->sc_base->dipswitches;

#ifdef BAHTIMINGS
	printf(": link addr 0x%02x(%ld), with timer\n",
	    linkaddress, linkaddress);
#else
	printf(": link addr 0x%02x(%ld)\n", linkaddress, linkaddress);
#endif

	sc->sc_arccom.ac_anaddr = linkaddress;

	/* clear the int mask... */

	sc->sc_base->status = sc->sc_intmask = 0;

	sc->sc_base->command = ARC_CONF(CONF_LONG);
	sc->sc_base->command = ARC_CLR(CLR_POR|CLR_RECONFIG);
	sc->sc_recontime = sc->sc_reconcount = 0;

	/* and reenable kernel int level */
	splx(s);

	/*
	 * set interface to stopped condition (reset)
	 */
	bah_stop(sc); 

	ifp->if_unit = sc->sc_dev.dv_unit;
	ifp->if_name = bahcd.cd_name;
	ifp->if_output = arc_output;
	ifp->if_start = bah_start;
	ifp->if_ioctl = bah_ioctl;
	ifp->if_timer = 0;
	ifp->if_watchdog  = bah_watchdog;

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX |
	    IFF_NOTRAILERS | IFF_NOARP;

	ifp->if_mtu = ARCMTU;

	if_attach(ifp);
	arc_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_ARCNET, ARC_HDRLEN);
#endif

	/* under heavy load we need four of them: */
	alloc_sicallback();
	alloc_sicallback();
	alloc_sicallback();
	alloc_sicallback();

	sc->sc_isr.isr_intr = bahintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_isr);
}

/*
 * Initialize device
 *
 */
void
bah_init(sc)
	struct bah_softc *sc;
{
	struct ifnet *ifp;
	int s;

	ifp = &sc->sc_arccom.ac_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		s = splnet();
		ifp->if_flags |= IFF_RUNNING;
		bah_reset(sc);
		bah_start(ifp);
		splx(s);
	}
}

/*
 * Reset the interface...
 *
 * this assumes that it is called inside a critical section...
 *
 */
void
bah_reset(sc)
	struct bah_softc *sc;
{
	struct ifnet *ifp;
	int i, s, linkaddress;

	ifp = &sc->sc_arccom.ac_if;

#ifdef BAH_DEBUG
	printf("%s: reset\n", sc->sc_dev.dv_xname);
#endif
	/* stop hardware in case it still runs */

	sc->sc_base->kick1 = 0;
	sc->sc_base->kick2 = 0;
	DELAY(120);

	/* and restart it */
	sc->sc_base->kick1 = 0xFF;
	sc->sc_base->kick2 = 0xFF;

	do {
		DELAY(120);
	} while (!(sc->sc_base->status & ARC_POR)); 

	linkaddress = sc->sc_base->dipswitches;

#if defined(BAH_DEBUG) && (BAH_DEBUG > 2)
	printf("bah%ld: reset: card reset, link addr = 0x%02x (%ld)\n",
	    ifp->if_unit, linkaddress, linkaddress);
#endif
	sc->sc_arccom.ac_anaddr = linkaddress;

	/* tell the routing level about the (possibly changed) link address */
	arc_ifattach(ifp);

	/* POR is NMI, but we need it below: */
	sc->sc_intmask = ARC_RECON|ARC_POR;
	sc->sc_base->status	= sc->sc_intmask;
	sc->sc_base->command = ARC_CONF(CONF_LONG);
	
#ifdef BAH_DEBUG
	printf("%s: reset: chip configured, status=0x%02x\n",
	    sc->sc_dev.dv_xname, sc->sc_base->status);
#endif

	sc->sc_base->command = ARC_CLR(CLR_POR|CLR_RECONFIG);

#ifdef BAH_DEBUG
	printf("%s: reset: bits cleared, status=0x%02x\n",
	    sc->sc_dev.dv_xname, sc->sc_base->status);
#endif

	sc->sc_reconcount_excessive = ARC_EXCESSIVE_RECONS;

	/* start receiver */

	sc->sc_intmask  |= ARC_RI;

	sc->sc_bufstat[2] =
	    sc->sc_bufstat[3] =
	    sc->sc_rx_packetno = 
	    sc->sc_rx_fillcount = 0;

	sc->sc_rx_act = 2;

	sc->sc_base->command = ARC_RXBC(2);
	sc->sc_base->status	= sc->sc_intmask;

#ifdef BAH_DEBUG
	printf("%s: reset: started receiver, status=0x%02x\n",
	    sc->sc_dev.dv_xname, sc->sc_base->status);
#endif

	/* and init transmitter status */
	sc->sc_tx_act = 0;
	sc->sc_tx_fillcount = 0;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

#ifdef BAHTIMINGS
	bzero((caddr_t)&(sc->sc_stats), sizeof(sc->sc_stats));
	sc->sc_stats.mincopyin =
	    sc->sc_stats.mincopyout =
	    sc->sc_stats.minsend = ULONG_MAX;
#endif

	bah_start(ifp);
}

/*
 * Take interface offline
 */
void
bah_stop(sc)
	struct bah_softc *sc;
{
	int s;
	
	/* Stop the interrupts */
	sc->sc_base->status = 0;

	/* Stop the interface */
	sc->sc_base->kick1 = 0;
	sc->sc_base->kick2 = 0;

	/* Stop watchdog timer */
	sc->sc_arccom.ac_if.if_timer = 0;

#ifdef BAHTIMINGS
	log(LOG_DEBUG,"%s: to board: %6lu .. %6lu ns/byte\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_stats.mincopyout, sc->sc_stats.maxcopyout);

	log(LOG_DEBUG,"%s: from board: %6lu .. %6lu ns/byte\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_stats.mincopyin, sc->sc_stats.maxcopyin);
	
	log(LOG_DEBUG,"%s: send time: %6lu .. %6lu mics/byte\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_stats.minsend, sc->sc_stats.maxsend);

	sc->sc_stats.minsend = 
	    sc->sc_stats.mincopyout = 
	    sc->sc_stats.mincopyin = ULONG_MAX;
	sc->sc_stats.maxsend = 
	    sc->sc_stats.maxcopyout = 
	    sc->sc_stats.maxcopyin = 0;
#endif
}

__inline void 
movepout(from, to, len)
	u_char *from;
	__volatile u_char *to;
	int len;
{
#ifdef BAHASMCOPY
	u_short shortd;
	u_long longd, longd1, longd2, longd3, longd4;

	if ((len > 3) && ((long)from) & 3) {
		switch (((long)from) & 3) {
		case 3:
			*to = *from++;
			to += 2; --len;
			break;
		case 1:
			*to = *from++;
			to += 2; --len;
		case 2:
			shortd = *((u_short *)from)++;
			asm("movepw %0,%1@(0)" : : "d"(shortd), "a"(to));
			to += 4; len -= 2;
			break;
		default:
		}

		while (len >= 32) {
			longd1 = *((u_long *)from)++;
			longd2 = *((u_long *)from)++;
			longd3 = *((u_long *)from)++;
			longd4 = *((u_long *)from)++;
			asm("movepl %0,%1@(0)"  : : "d"(longd1), "a"(to));
			asm("movepl %0,%1@(8)"  : : "d"(longd2), "a"(to));
			asm("movepl %0,%1@(16)" : : "d"(longd3), "a"(to));
			asm("movepl %0,%1@(24)" : : "d"(longd4), "a"(to));

			longd1 = *((u_long *)from)++;
			longd2 = *((u_long *)from)++;
			longd3 = *((u_long *)from)++;
			longd4 = *((u_long *)from)++;
			asm("movepl %0,%1@(32)" : : "d"(longd1), "a"(to));
			asm("movepl %0,%1@(40)" : : "d"(longd2), "a"(to));
			asm("movepl %0,%1@(48)" : : "d"(longd3), "a"(to));
			asm("movepl %0,%1@(56)" : : "d"(longd4), "a"(to));

			to += 64; len -= 32;
		}
		while (len > 0) {
			longd = *((u_long *)from)++;
			asm("movepl %0,%1@(0)" : : "d"(longd), "a"(to));
			to += 8; len -= 4;
		}
	}
#endif
	while (len > 0) {
		*to = *from++;
		to += 2;
		--len;
	}
}

/*
 * Start output on interface. Get another datagram to send
 * off the interface queue, and copy it to the
 * interface becore starting the output
 *
 * this assumes that it is called inside a critical section...
 * XXX hm... does it still?
 *
 */
void
bah_start(ifp)
	struct ifnet *ifp;
{
	struct bah_softc *sc;
	struct mbuf *m,*mp;
	__volatile u_char *bah_ram_ptr;
	int i, len, tlen, offset, s, buffer;
#ifdef BAHTIMINGS
	u_long copystart, lencopy, perbyte;
#endif

	sc = bahcd.cd_devs[ifp->if_unit];

#if defined(BAH_DEBUG) && (BAH_DEBUG > 3)
	printf("%s: start(0x%x)\n", sc->sc_dev.dv_xname, ifp);
#endif

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	s = splnet();

	if (sc->sc_tx_fillcount >= 2) {
		splx(s);
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m);
	buffer = sc->sc_tx_act ^ 1;

	splx(s);

	if (m == 0)
		return;

#if NBPFILTER > 0
	/*
	 * If bpf is listening on this interface, let it
	 * see the packet before we commit it to the wire
	 *
	 * (can't give the copy in A2060 card RAM to bpf, because
	 * that RAM is just accessed as on every other byte)
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

	/* we need the data length beforehand */
	for (mp = m, tlen=0; mp; mp = mp->m_next)
		tlen += mp->m_len;

#ifdef BAH_DEBUG
	m = m_pullup(m,3);	/* gcc does structure padding */
	printf("%s: start: filling %ld from %ld to %ld type %ld\n",
	    sc->sc_dev.dv_xname, buffer, mtod(m, u_char *)[0],
	    mtod(m, u_char *)[1], mtod(m, u_char *)[2]);
#else
	m = m_pullup(m, 2);
#endif
	bah_ram_ptr = sc->sc_base->buffers + buffer*512*2;

	/* write the addresses to RAM and throw them away */

	/*
	 * Hardware does this: Yet Another Microsecond Saved.
	 * (btw, timing code says usually 2 microseconds)
	 * bah_ram_ptr[0*2] = mtod(m, u_char *)[0];
	 */
	bah_ram_ptr[1 * 2] = mtod(m, u_char *)[1];
	m_adj(m, 2);
		
	/* correct total length for that */
	tlen -= 2;
	if (tlen < ARC_MIN_FORBID_LEN) {
		offset = 256 - tlen;
		bah_ram_ptr[2 * 2] = offset;
	} else {
		if (tlen <= ARC_MAX_FORBID_LEN) 
			offset = 512 - 3 - tlen;
		else {
			if (tlen > ARC_MAX_LEN)
				tlen = ARC_MAX_LEN;
			offset = 512 - tlen;
		}

		bah_ram_ptr[2 * 2] = 0;
		bah_ram_ptr[3 * 2] = offset;
	}
	bah_ram_ptr += offset * 2;

	/* lets loop again through the mbuf chain */

	for (mp = m; mp; mp = mp->m_next) {
		if (len = mp->m_len) {		/* YAMS */
#ifdef BAHTIMINGS
			lencopy = len;
			copystart = clkread();
#endif
			movepout(mtod(mp, caddr_t), bah_ram_ptr, len);

#ifdef BAHTIMINGS
			perbyte = 1000 * (clkread() - copystart) / lencopy;
			sc->sc_stats.mincopyout = 
			    ulmin(sc->sc_stats.mincopyout, perbyte);
			sc->sc_stats.maxcopyout =
			    ulmax(sc->sc_stats.maxcopyout, perbyte);
#endif
			bah_ram_ptr += len*2;
		}
	}

	sc->sc_broadcast[buffer] = (m->m_flags & M_BCAST) != 0;
	sc->sc_retransmits[buffer] = (m->m_flags & M_BCAST) ? 1 : 5;

	/* actually transmit the packet */
	s = splnet();

	if (++sc->sc_tx_fillcount > 1) { 
		/*
		 * We are filled up to the rim. No more bufs for the moment,
		 * please.
		 */
		ifp->if_flags |= IFF_OACTIVE;
	} else {
#ifdef BAH_DEBUG
		printf("%s: start: starting transmitter on buffer %d\n", 
		    sc->sc_dev.dv_xname, buffer);
#endif
		/* Transmitter was off, start it */
		sc->sc_tx_act = buffer;

		/*
		 * We still can accept another buf, so don't:
		 * ifp->if_flags |= IFF_OACTIVE;
		 */
		sc->sc_intmask |= ARC_TA;
		sc->sc_base->command = ARC_TX(buffer);
		sc->sc_base->status  = sc->sc_intmask;

		sc->sc_arccom.ac_if.if_timer = ARCTIMEOUT;
#ifdef BAHTIMINGS
		bcopy((caddr_t)&time,
		    (caddr_t)&(sc->sc_stats.lasttxstart_tv),
		    sizeof(struct timeval));

		sc->sc_stats.lasttxstart_mics = clkread();
#endif
	}
	splx(s);
	m_freem(m);

	/*
	 * After 10 times reading the docs, I realized
	 * that in the case the receiver NAKs the buffer request,
	 * the hardware retries till shutdown.
	 * This is integrated now in the code above.
	 */

	return;
}

void 
callstart(sc, dummy)
	struct bah_softc *sc;
	void *dummy;
{

	bah_start(&sc->sc_arccom.ac_if);
}

__inline void
movepin(from, to, len)
	__volatile u_char *from;
	u_char *to;
	int len;
{
#ifdef BAHASMCOPY
	unsigned long	longd, longd1, longd2, longd3, longd4;
	ushort		shortd;

	if ((len > 3) && (((long)to) & 3)) {
		switch (((long)to) & 3) {
		case 3: *to++ = *from;
			from += 2; --len;
			break;
		case 1: *to++ = *from;
			from += 2; --len;
		case 2:	asm ("movepw %1@(0),%0": "=d" (shortd) : "a" (from));
			*((ushort *)to)++ = shortd;
			from += 4; len -= 2;
			break;
		default:
		}

		while (len >= 32) {
			asm("movepl %1@(0),%0"  : "=d"(longd1) : "a" (from));
			asm("movepl %1@(8),%0"  : "=d"(longd2) : "a" (from));
			asm("movepl %1@(16),%0" : "=d"(longd3) : "a" (from));
			asm("movepl %1@(24),%0" : "=d"(longd4) : "a" (from));
			*((unsigned long *)to)++ = longd1;
			*((unsigned long *)to)++ = longd2;
			*((unsigned long *)to)++ = longd3;
			*((unsigned long *)to)++ = longd4;

			asm("movepl %1@(32),%0" : "=d"(longd1) : "a" (from));
			asm("movepl %1@(40),%0" : "=d"(longd2) : "a" (from));
			asm("movepl %1@(48),%0" : "=d"(longd3) : "a" (from));
			asm("movepl %1@(56),%0" : "=d"(longd4) : "a" (from));
			*((unsigned long *)to)++ = longd1;
			*((unsigned long *)to)++ = longd2;
			*((unsigned long *)to)++ = longd3;
			*((unsigned long *)to)++ = longd4;

			from += 64; len -= 32;
		}
		while (len > 0) {
			asm("movepl %1@(0),%0" : "=d"(longd) : "a" (from));
			*((unsigned long *)to)++ = longd;
			from += 8; len -= 4;
		}

	}
#endif /* BAHASMCOPY */
	while (len > 0) {
		*to++ = *from;
		from += 2;
		--len;
	}

}

/*
 * Arcnet interface receiver soft interrupt:
 * get the stuff out of any filled buffer we find.
 */
void
bah_srint(sc, dummy)
	struct bah_softc *sc;
	void *dummy;
{
	int buffer, buffer1, len, len1, amount, offset, s, i, type;
	u_char __volatile *bah_ram_ptr;
	struct mbuf *m, *dst, *head;
	struct arc_header *ah;
	struct ifnet *ifp;
#ifdef BAHTIMINGS
	u_long copystart, lencopy, perbyte;
#endif

	head = 0;
	ifp = &sc->sc_arccom.ac_if;

	s = splnet();
	if (sc->sc_rx_fillcount <= 1)
		buffer = sc->sc_rx_act ^ 1;
	else {

		i = ((unsigned)(sc->sc_bufstat[2] - sc->sc_bufstat[3])) % 256;
		if (i < 64)
			buffer = 3;
		else if (i > 192)
			buffer = 2;
		else {
			log(LOG_WARNING,
			    "%s: rx srint: which is older, %ld or %ld?\nn",
			    sc->sc_dev.dv_xname,
			    sc->sc_bufstat[2], sc->sc_bufstat[3]);
			log(LOG_WARNING, "%s: (filled %ld)\n",
			    sc->sc_dev.dv_xname, sc->sc_rx_fillcount);
			splx(s);
			return;
		}
	}
	splx(s);

	/* Allocate header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);

	if (m == 0) {
		/* 
	 	 * in case s.th. goes wrong with mem, drop it
	 	 * to make sure the receiver can be started again
		 * count it as input error (we dont have any other
		 * detectable)
	 	 */
		ifp->if_ierrors++;
		goto cleanup;
	}
			
	m->m_pkthdr.rcvif = ifp;

	/*
	 * Align so that IP packet will be longword aligned. Here we
	 * assume that m_data of new packet is longword aligned.
	 * When implementing PHDS, we might have to change it to 2,
	 * (2*sizeof(ulong) - ARC_HDRNEWLEN)), packet type dependent.
	 */

	bah_ram_ptr = sc->sc_base->buffers + buffer*512*2;
	offset = bah_ram_ptr[2*2];
	if (offset)
		len = 256 - offset;
	else {
		offset = bah_ram_ptr[3*2];
		len = 512 - offset;
	}
	type = bah_ram_ptr[offset*2];
	m->m_data += 1 + arc_isphds(type);

	head = m;
	ah = mtod(head, struct arc_header *);
		
	ah->arc_shost = bah_ram_ptr[0*2];
	ah->arc_dhost = bah_ram_ptr[1*2];

	m->m_pkthdr.len = len+2; /* whole packet length */
	m->m_len = 2;		 /* mbuf filled with ARCnet addresses */
	bah_ram_ptr += offset*2; /* ram buffer continues there */

	while (len > 0) {
	
		len1 = len;
		amount = M_TRAILINGSPACE(m);

		if (amount == 0) {
			dst = m;
			MGET(m, M_DONTWAIT, MT_DATA);
		
			if (m == 0) {
				ifp->if_ierrors++;
				goto cleanup;
			}
		
			if (len1 >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);
	
			m->m_len = 0;
			dst->m_next = m;
			amount = M_TRAILINGSPACE(m);
		}

		if (amount < len1)
			len1 = amount;

#ifdef BAHTIMINGS
		lencopy = len;
		copystart = clkread();
#endif

		movepin(bah_ram_ptr, mtod(m, u_char *) + m->m_len, len1);

#ifdef BAHTIMINGS
		perbyte = 1000 * (clkread() - copystart) / lencopy;
		sc->sc_stats.mincopyin =
		    ulmin(sc->sc_stats.mincopyin, perbyte);
		sc->sc_stats.maxcopyin =
		    ulmax(sc->sc_stats.maxcopyin, perbyte);
#endif

		m->m_len += len1;
		bah_ram_ptr += len1*2;
		len -= len1;
	}

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, head);
#endif

	arc_input(&sc->sc_arccom.ac_if, head);

	/* arc_input has freed it, we dont need to... */

	head = NULL;
	ifp->if_ipackets++;
	
cleanup:

	if (head == NULL)
		m_freem(head);

	s = splnet();

	if (--sc->sc_rx_fillcount == 1) {

		/* was off, restart it on buffer just emptied */
		sc->sc_rx_act = buffer;
		sc->sc_intmask |= ARC_RI;

		/* this also clears the RI flag interupt: */
		sc->sc_base->command = ARC_RXBC(buffer);
		sc->sc_base->status = sc->sc_intmask;

#ifdef BAH_DEBUG
		printf("%s: srint: restarted rx on buf %ld\n",
		    sc->sc_dev.dv_xname, buffer);
#endif
	}
	splx(s);
}

__inline static void
bah_tint(sc)
	struct bah_softc *sc;
{
	int buffer;
	u_char __volatile *bah_ram_ptr;
	int isr;
	int clknow;

	buffer = sc->sc_tx_act;
	isr = sc->sc_base->status;

	/*
	 * XXX insert retransmit code etc. here; and dont forget
	 * to not retransmit if this is a timeout int.
	 * For now just: 
	 */ 

	if (!(isr & ARC_TMA) && !(sc->sc_broadcast[buffer]))
		sc->sc_arccom.ac_if.if_oerrors++;
	else
		sc->sc_arccom.ac_if.if_opackets++;

#ifdef BAHTIMINGS
	clknow = clkread();

	sc->sc_stats.minsend = ulmin(sc->sc_stats.minsend,
	    clknow - sc->sc_stats.lasttxstart_mics);

	sc->sc_stats.maxsend = ulmax(sc->sc_stats.maxsend,
	    clknow - sc->sc_stats.lasttxstart_mics);
#endif

	/* We know we can accept another buffer at this point. */
	sc->sc_arccom.ac_if.if_flags &= ~IFF_OACTIVE;

	if (--sc->sc_tx_fillcount > 0) {

		/* 
		 * start tx on other buffer.
		 * This also clears the int flag
		 */
		buffer ^= 1;
		sc->sc_tx_act = buffer;

		/*
		 * already given:
		 * sc->sc_intmask |= ARC_TA; 
		 * sc->sc_base->status = sc->sc_intmask;
		 */
		sc->sc_base->command = ARC_TX(buffer);
		/* init watchdog timer */
		sc->sc_arccom.ac_if.if_timer = ARCTIMEOUT;

#ifdef BAHTIMINGS
		bcopy((caddr_t)&time,
		    (caddr_t)&(sc->sc_stats.lasttxstart_tv),
		    sizeof(struct timeval));

		sc->sc_stats.lasttxstart_mics = clkread();
#endif
 
#if defined(BAH_DEBUG) && (BAH_DEBUG > 1)
		printf("%s: tint: starting tx on buffer %d, status 0x%02x\n", 
		    sc->sc_dev.dv_xname, buffer, sc->sc_base->status);
#endif
	} else {
		/* have to disable TX interrupt */
		sc->sc_intmask &= ~ARC_TA;
		sc->sc_base->status = sc->sc_intmask;
		/* ... and watchdog timer */
		sc->sc_arccom.ac_if.if_timer = 0;

#ifdef BAH_DEBUG
		printf("%s: tint: no more buffers to send, status 0x%02x\n",
		    sc->sc_dev.dv_xname, sc->sc_base->status);
#endif
	}

#ifdef BAHSOFTCOPY
	/* schedule soft int to fill a new buffer for us */
	add_sicallback((sifunc_t)callstart, sc, NULL);
#else
	/* call it directly */
	callstart(sc, NULL);
#endif
}

/*
 * Our interrupt routine
 */
int
bahintr(sc)
	struct bah_softc *sc;
{
	u_char isr, maskedisr;
	int buffer;
	int unit;
	u_long newsec;

	isr = sc->sc_base->status;
	maskedisr = isr & sc->sc_intmask;
	if (!maskedisr) 
		return (0);

#if defined(BAH_DEBUG) && (BAH_DEBUG>1)
	printf("%s: intr: status 0x%02x, intmask 0x%02x\n",
	    sc->sc_dev.dv_xname, isr, sc->sc_intmask);
#endif

	if (maskedisr & ARC_POR) {
		sc->sc_arccom.ac_anaddr = sc->sc_base->dipswitches;
		sc->sc_base->command = ARC_CLR(CLR_POR);
		log(LOG_WARNING, "%s: intr: got spurious power on reset int\n",
		    sc->sc_dev.dv_xname);
	}

	if (maskedisr & ARC_RECON) {
		/*
		 * we dont need to:
		 * sc->sc_base->command = ARC_CONF(CONF_LONG);
		 */
		sc->sc_base->command = ARC_CLR(CLR_RECONFIG);
		sc->sc_arccom.ac_if.if_collisions++;

		/*
		 * If more than 2 seconds per reconfig:
		 *	Reset time and counter.
		 * else:
		 *	If more than ARC_EXCESSIVE_RECONFIGS reconfigs
		 *	since last burst, complain and set treshold for
		 *	warnings to ARC_EXCESSIVE_RECONS_REWARN.
		 *
		 * This allows for, e.g., new stations on the cable, or
		 * cable switching as long as it is over after (normally)
		 * 16 seconds.
		 *
		 * XXX TODO: check timeout bits in status word and double
		 * time if necessary.
		 */

		newsec = time.tv_sec;
		if (newsec - sc->sc_recontime > 2 * sc->sc_reconcount) {
			sc->sc_recontime = newsec;
			sc->sc_reconcount = 0;
			sc->sc_reconcount_excessive = ARC_EXCESSIVE_RECONS;
		} else if (++sc->sc_reconcount > sc->sc_reconcount_excessive) {
			sc->sc_reconcount_excessive = 
			    ARC_EXCESSIVE_RECONS_REWARN;
			log(LOG_WARNING,
			    "%s: excessive token losses, cable problem?\n",
			    sc->sc_dev.dv_xname);
			sc->sc_recontime = newsec;
			sc->sc_reconcount = 0;
		}
	}

	if (maskedisr & ARC_RI) {

#if defined(BAH_DEBUG) && (BAH_DEBUG > 1)
		printf("%s: intr: hard rint, act %ld 2:%ld 3:%ld\n",
		    sc->sc_dev.dv_xname,
		    sc->sc_rx_act, sc->sc_bufstat[2], sc->sc_bufstat[3]);
#endif
	
		buffer = sc->sc_rx_act;
		sc->sc_rx_packetno = (sc->sc_rx_packetno+1)%256;
		sc->sc_bufstat[buffer] = sc->sc_rx_packetno;

		if (++sc->sc_rx_fillcount > 1) {
			sc->sc_intmask &= ~ARC_RI;
			sc->sc_base->status = sc->sc_intmask;
		} else {

			buffer ^= 1;
			sc->sc_rx_act = buffer;

			/*
			 * Start receiver on other receive buffer.
			 * This also clears the RI interupt flag.
			 */
			sc->sc_base->command = ARC_RXBC(buffer);
			/* we are in the RX intr, so mask is ok for RX */

#ifdef BAH_DEBUG
			printf("%s: started rx for buffer %ld, status 0x%02x\n",
			    sc->sc_dev.dv_xname, sc->sc_rx_act,
			    sc->sc_base->status);
#endif
		}

#ifdef BAHSOFTCOPY
		/* this one starts a soft int to copy out of the hw */
		add_sicallback((sifunc_t)bah_srint, sc,NULL);
#else
		/* this one does the copy here */
		bah_srint(sc,NULL);
#endif
	}

	if (maskedisr & ARC_TA) 
		bah_tint(sc);

	return (1);
}

/*
 * Process an ioctl request. 
 * This code needs some work - it looks pretty ugly.
 */
int
bah_ioctl(ifp, command, data)
	register struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct bah_softc *sc;
	register struct ifaddr *ifa;
	int s, error;

	error = 0;
	sc = bahcd.cd_devs[ifp->if_unit];
	ifa = (struct ifaddr *)data;
	s = splnet();

#if defined(BAH_DEBUG) && (BAH_DEBUG > 2) 
	printf("%s: ioctl() called, cmd = 0x%x\n",
	    sc->sc_dev.dv_xname, command);
#endif

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			bah_init(sc);
			sc->sc_arccom.ac_ipaddr = IA_SIN(ifa)->sin_addr;
			/*arpwhohas(&sc->sc_arccom, &IA_SIN(ifa)->sin_addr);*/
			break;
#endif
		default:
			bah_init(sc);
			break;
		}

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, 
			 * then stop it.
			 */
			bah_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			bah_init(sc);
		} 
		break;

		/* Multicast not supported */

	default:
		error = EINVAL;
	}

	splx(s);
	return (error);
}

/*
 * watchdog routine for transmitter.
 *
 * We need this, because else a receiver whose hardware is alive, but whose
 * software has not enabled the Receiver, would make our hardware wait forever
 * Discovered this after 20 times reading the docs.
 *
 * Only thing we do is disable transmitter. We'll get an transmit timeout,
 * and the int handler will have to decide not to retransmit (in case
 * retransmission is implemented).
 *
 * This one assumes being called inside splnet(), and that imp >= ipl2
 */

void
bah_watchdog(unit)
int unit;
{
	struct bah_softc *sc;
	struct ifnet *ifp;

	sc = bahcd.cd_devs[unit];
	ifp = &(sc->sc_arccom.ac_if);

	sc->sc_base->command = ARC_TXDIS;
	return;
}
