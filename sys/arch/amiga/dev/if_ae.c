/*	$NetBSD: if_ae.c,v 1.4 1995/12/24 02:29:52 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Bernd Ernesti and Klaus Burkert. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by Bernd Ernesti, by Klaus
 *	Burkert, by Michael van Elst, and by the University of California,
 *	Berkeley and its contributors.
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
 *	@(#)if_le.c	8.1 (Berkeley) 6/10/93
 *
 *	This is based on the original LANCE files, as the PCnet-ISA used on
 *	the Ariadne is a LANCE-descendant optimized for the PC-ISA bus.
 *	This causes some modifications, all data that is to go into registers
 *	or to structures (buffer-descriptors, init-block) has to be
 *	byte-swapped. In addition ALL write accesses to the board have to be
 *	WORD or LONG, BYTE-access is prohibited!!
 */

#include "ae.h"
#if NAE > 0

#include "bpfilter.h"

/*
 * AMD 79C960 PCnet-ISA
 *
 * This driver will generate and accept tailer encapsulated packets even
 * though it buys us nothing.  The motivation was to avoid incompatibilities
 * with VAXen, SUNs, and others that handle and benefit from them.
 * This reasoning is dubious.
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
#include <net/netisr.h>
#include <net/route.h>

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

#if defined(CCITT) && defined(LLC)
#include <sys/socketvar.h>  
#include <netccitt/x25.h>
extern llc_ctlinput(), cons_rtrequest();
#endif  

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif  

#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/if_aereg.h>

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ae_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	ae_softc {
	struct	device sc_dev;
	struct	isr sc_isr;
	struct	arpcom sc_arpcom;	/* common Ethernet structures */
	void	*sc_base;	/* base address of board */
	struct	aereg1 *sc_r1;	/* LANCE registers */
	struct	aereg2 *sc_r2;	/* dual-port RAM */
	int	sc_rmd;		/* predicted next rmd to process */
	int	sc_tmd;		/* next tmd to use */
	int	sc_no_td;	/* number of tmds in use */
} ae_softc[NAE];

/* offsets for:	   ID,   REGS,    MEM */
int	aestd[] = { 0, 0x0370, 0x8000 };
static u_int16_t	revision;

int	aematch __P((struct device *, void *, void *));
void	aeattach __P((struct device *, struct device *, void *));
void	aewatchdog __P((int));
void	aestop __P((struct ae_softc *));
void	aememinit __P((struct ae_softc *));
void	aereset __P((struct ae_softc *));
void	aeinit __P((struct ae_softc *));
void	aestart __P((struct ifnet *));
int	aeintr __P((struct ae_softc *));
void	aetint __P((struct ae_softc *));
void	aerint __P((struct ae_softc *));
void	aeread __P((struct ae_softc *, u_char *, int));
static	void wcopyfrom __P((char *, char *, int));
static	void wcopyto __P((char *, char *, int));
static	void wzero __P((char *, int));
int	aeput __P((char *, struct mbuf *));
struct	mbuf *aeget __P((struct ae_softc *,u_char *, int));
int	aeioctl __P((struct ifnet *, u_long, caddr_t));
void	aesetladrf __P((struct arpcom *, u_int16_t *));

struct cfdriver aecd = {
	NULL, "ae", aematch, aeattach, DV_IFNET, sizeof(struct ae_softc)
};

int
aematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap;

	zap = (struct zbus_args *)aux;

	/* Ariadne ethernet card */
	if (zap->manid == 2167 && zap->prodid == 201)
		return (1);

	return (0);
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
aeattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct aereg2 *aer2;
	struct zbus_args *zap;
	struct ae_softc *sc = (void *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	unsigned long ser;
	int s = splhigh ();

	zap =(struct zbus_args *)aux;

	/*
	 * Make config msgs look nicer.
	 */
	printf("\n");

	sc->sc_base = zap->va;
	sc->sc_r1 = (struct aereg1 *)(aestd[1] + (int)zap->va);
	aer2 = sc->sc_r2 = (struct aereg2 *)(aestd[2] + (int)zap->va);

	/*
	 * Serial number for board is used as host ID.
	 */
	ser = (unsigned long) zap->serno;

	/*
	 * Manufacturer decides the 3 first bytes, i.e. ethernet vendor ID.
	 */

	/*
	 * currently borrowed from C= 
	 * the next four lines will soon have to be altered 
	 */

	sc->sc_arpcom.ac_enaddr[0] = 0x00;
	sc->sc_arpcom.ac_enaddr[1] = 0x80;
	sc->sc_arpcom.ac_enaddr[2] = 0x10;

	sc->sc_arpcom.ac_enaddr[3] = ((ser >> 16) & 0x0f) | 0xf0; /* to diff from A2065 */
	sc->sc_arpcom.ac_enaddr[4] = (ser >>  8 ) & 0xff;
	sc->sc_arpcom.ac_enaddr[5] = (ser       ) & 0xff;

	printf("%s: hardware address %s 32K", sc->sc_dev.dv_xname,
		ether_sprintf(sc->sc_arpcom.ac_enaddr));

	aestop(sc);
	delay(100);

	/* get the chip version of the lance chip */
	sc->sc_r1->aer1_rap = 0x5900;
	revision = ((sc->sc_r1->aer1_rdp >> 4) -2);
	printf("  chip-revision: B%x\n", revision);

	splx (s);

	ifp->if_unit = sc->sc_dev.dv_unit;
	ifp->if_name = aecd.cd_name;
	ifp->if_ioctl = aeioctl;
	ifp->if_watchdog = aewatchdog;
	ifp->if_output = ether_output;
	ifp->if_start = aestart;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	sc->sc_isr.isr_intr = aeintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr (&sc->sc_isr);
	return;
}

void
aewatchdog(unit)
	short unit;
{
	struct ae_softc *sc = aecd.cd_devs[unit];

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	aereset(sc);
}

void
aestop(sc)
	struct ae_softc *sc;
{
	sc->sc_r1->aer1_rap =  AE_CSR0;
	sc->sc_r1->aer1_rdp =  AE_STOP;
}

	
/*    
 * Set up the initialization block and the descriptor rings.
 */     
void
aememinit(sc)
	register struct ae_softc *sc;
{        
	register struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	/*
	 * This structure is referenced from the CARD's/PCnet-ISA's point
	 * of view, thus the 0x8000 address which is the buffer RAM area
	 * of the Ariadne card. This pointer is manipulated
	 * with the PCnet-ISA's view of memory and NOT the Amiga's. FYI.
	 */
	register struct aereg2 *aemem = (struct aereg2 *) 0x8000;
	register struct aereg2 *aer2 = sc->sc_r2;
	register int i;


#if NBPFILTER > 0
	if (ifp->if_flags & IFF_PROMISC)
		/* set the promiscuous bit */
		aer2->aer2_mode = AE_MODE | AE_PROM;
	else    
#endif
		aer2->aer2_mode = AE_MODE;
	/* you know: no BYTE access.... */
	aer2->aer2_padr[0] =
		(sc->sc_arpcom.ac_enaddr[0] << 8) | sc->sc_arpcom.ac_enaddr[1];
	aer2->aer2_padr[1] =
		(sc->sc_arpcom.ac_enaddr[2] << 8) | sc->sc_arpcom.ac_enaddr[3];
	aer2->aer2_padr[2] =
		(sc->sc_arpcom.ac_enaddr[4] << 8) | sc->sc_arpcom.ac_enaddr[5];
	aesetladrf(&sc->sc_arpcom, aer2->aer2_ladrf);

	sc->sc_no_td = sc->sc_tmd = sc->sc_rmd = 0;

	aer2->aer2_rlen = SWAP(AE_RLEN);
	aer2->aer2_rdra = SWAP((int)aemem->aer2_rmd); 
	aer2->aer2_tlen = SWAP(AE_TLEN);
	aer2->aer2_tdra = SWAP((int)aemem->aer2_tmd);

	for (i = 0; i < AERBUF; i++) {  
		aer2->aer2_rmd[i].rmd0 = SWAP((int)aemem->aer2_rbuf[i]);
		aer2->aer2_rmd[i].rmd1 = AE_OWN; 
		aer2->aer2_rmd[i].rmd2 = SWAP(-ETHER_MAX_LEN);
		aer2->aer2_rmd[i].rmd3 = 0;
	}

	for (i = 0; i < AETBUF; i++) {
		aer2->aer2_tmd[i].tmd0 = SWAP((int)aemem->aer2_tbuf[i]);
		aer2->aer2_tmd[i].tmd1 = 0;
		aer2->aer2_tmd[i].tmd2 = 0;
		aer2->aer2_tmd[i].tmd3 = 0;
	}
}

void
aereset(sc)
	struct ae_softc *sc;
{
	int s;

	s = splnet();
	aeinit(sc);
	splx(s);
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */     
void
aeinit(sc)
	struct ae_softc *sc;
{
	register struct aereg1 *aer1 = sc->sc_r1;
	register struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	register struct aereg2 *aemem = (struct aereg2 *) 0x8000;

	register int timo = 0;
	volatile int dummy;

	aestop(sc);
	delay(100);

	dummy = aer1->aer1_reset;    /* Reset PCNet-ISA */

	aememinit(sc);

	/* Give LANCE the physical address of its init block. */
	aer1->aer1_rap =  AE_CSR1;
	aer1->aer1_rdp =  SWAP((int)&aemem->aer2_mode);
	aer1->aer1_rap =  AE_CSR2;
	aer1->aer1_rdp =  0;

/*
 *	re-program LEDs to match meaning used on the Ariadne board
 */
	aer1->aer1_rap = 0x0500;
	aer1->aer1_idp = 0x9000;
	aer1->aer1_rap = 0x0600;
	aer1->aer1_idp = 0x8100;
	aer1->aer1_rap = 0x0700;
	aer1->aer1_idp = 0x8400;

/*
 * you can `ifconfig (link0|-link0) ae0' to get the following
 * behaviour:    
 *	-link0	enable autoselect between 10Base-T (UTP) and 10Base2 (BNC)
 *		if an active 10Base-T line is connected then 10Base-T
 *		is used otherwise 10Base2.
 *		this is the default behaviour, so there is no need to set
 *		-link0 when you want autoselect.
 *	link0 -link1	disable autoselect. enable BNC.
 *	link0 link1	disable autoselect. enable UTP.
 */
	if (!(ifp->if_flags & IFF_LINK0)) {
		/* enable autoselect */
		aer1->aer1_rap = 0x0200;
		aer1->aer1_idp = 0x0200;
	} else {
		/* disable autoselect */
		aer1->aer1_rap = 0x0200;
		aer1->aer1_idp = 0x0000;
		if (!(ifp->if_flags & IFF_LINK1)) {
			/* enable BNC */
			sc->sc_r2->aer2_mode = 0x0000;
		} else {
			/* enable UTP */
			sc->sc_r2->aer2_mode = 0x8000;
		}
	}

	/* Try to initialize the LANCE. */
	delay(100);
	aer1->aer1_rap =  AE_CSR0;
	aer1->aer1_rdp =  AE_INIT;

	/* Wait for initialization to finish. */
	do {
		if (++timo == 10000) {
			printf("%s: card failed to initialize\n", sc->sc_dev.dv_xname);
			break;
		}
	} while ((aer1->aer1_rdp & AE_IDON) == 0);

	/* Start the LANCE. */
	aer1->aer1_rap =  AE_CSR0;
	aer1->aer1_rdp =  AE_STRT | AE_INEA | AE_IDON;
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	aestart(ifp);
}

#define AENEXTTMP \
	if (++bix == AETBUF) bix = 0, tmd = sc->sc_r2->aer2_tmd; else ++tmd

/*
 * Start output on interface.  Get another datagram to send
 * off of the interface queue, and copy it to the interface
 * before starting the output.
 */
void
aestart(ifp)
	struct ifnet *ifp;
{
	register struct ae_softc *sc = aecd.cd_devs[ifp->if_unit];
	register int bix;
	register struct aetmd *tmd;
	register struct mbuf *m;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_tmd;
	tmd = &sc->sc_r2->aer2_tmd[bix];

	for (;;) {
		if (sc->sc_no_td >= AETBUF) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

		++sc->sc_no_td;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		sc->sc_r1->aer1_rdp =  AE_TINT | AE_INEA;
		len = aeput(sc->sc_r2->aer2_tbuf[bix], m);

#ifdef AEDEBUG
		if (len > ETHER_MAX_LEN)
			printf("packet length %d\n", len);
#endif

		ifp->if_timer = 5;

		tmd->tmd1 = AE_OWN | AE_STP | AE_ENP;
		tmd->tmd2 = SWAP(-len);
		tmd->tmd3 = 0;

		sc->sc_r1->aer1_rdp = AE_INEA | AE_TDMD;

		AENEXTTMP;
	}

	sc->sc_tmd = bix;
}

int
aeintr(sc)
	register struct ae_softc *sc;
{
	register struct aereg1 *aer1;
	register struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	register u_int16_t stat;

	/* if not even initialized, don't do anything further.. */
	if (sc->sc_base == 0)
		return (0);

	aer1 = sc->sc_r1;
	stat =  aer1->aer1_rdp;

	if ((stat & AE_INTR) == 0)
		return (0);

	aer1->aer1_rdp = (stat & (AE_INEA | AE_BABL | AE_MISS | AE_MERR |
				AE_RINT | AE_TINT | AE_IDON));
	if (stat & AE_SERR) {
			if (stat & AE_MERR) {
				printf("%s: memory error\n", sc->sc_dev.dv_xname);
				aereset(sc);
				return (1);
			}
			if (stat & AE_BABL) {
				printf("%s: babble\n", sc->sc_dev.dv_xname);
				ifp->if_oerrors++;
			}
#if 0
			if (stat & AE_CERR) {
				printf("%s: collision error\n", sc->sc_dev.dv_xname);
				ifp->if_collisions++;
			}
#endif
			if (stat & AE_MISS) {
				printf("%s: missed packet\n", sc->sc_dev.dv_xname);
				ifp->if_ierrors++;
			}
			aer1->aer1_rdp =  AE_BABL | AE_CERR | AE_MISS | AE_INEA;
		}
		if ((stat & AE_RXON) == 0) {
			printf("%s: receiver disabled\n", sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
			aereset(sc);
			return (1);
		}
		if ((stat & AE_TXON) == 0) {
			printf("%s: transmitter disabled\n", sc->sc_dev.dv_xname);
			ifp->if_oerrors++;
			aereset(sc);
			return (1);
		}
		if (stat & AE_RINT) {
			/* Reset watchdog timer. */
			ifp->if_timer = 0;
			aerint(sc);
		}
		if (stat & AE_TINT) {
			/* Reset watchdog timer. */
			ifp->if_timer = 0;
			aetint(sc);
		}

	return (1);
}

/*
 * Ethernet interface transmitter interrupt.
 * Start another output if more data to send.
 */
void
aetint(sc)
	struct ae_softc *sc;
{
	register int bix = (sc->sc_tmd - sc->sc_no_td + AETBUF) % AETBUF;
	struct aetmd *tmd = &sc->sc_r2->aer2_tmd[bix];
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	if (tmd->tmd1 & AE_OWN) {
#ifdef AEDEBUG
		printf("%s: extra tint\n", sc->sc_dev.dv_xname);
#endif
		return;
	}
	ifp->if_flags &= ~IFF_OACTIVE;

	do {
		if (sc->sc_no_td <= 0)
			break;
		ifp->if_opackets++;
		--sc->sc_no_td;

		if (tmd->tmd1 & AE_ERR) {
			if (tmd->tmd3 & AE_TBUFF)
				printf("%s: transmit buffer error\n", sc->sc_dev.dv_xname);
			if (tmd->tmd3 & AE_UFLO)
				printf("%s: underflow\n", sc->sc_dev.dv_xname);
			if (tmd->tmd3 & (AE_TBUFF | AE_UFLO)) {
				aereset(sc);
				return;
			}
			if (tmd->tmd3 & AE_LCAR)
				printf("%s: lost carrier\n", sc->sc_dev.dv_xname);
			if (tmd->tmd3 & AE_LCOL) {
				printf("%s: late collision\n", sc->sc_dev.dv_xname);
				ifp->if_collisions++;
			}
			if (tmd->tmd3 & AE_RTRY) {
				printf("%s: excessive collisions, tdr %d\n",
					sc->sc_dev.dv_xname, tmd->tmd3 & AE_TDR_MASK);
				ifp->if_collisions += 16;
			}
		} else if (tmd->tmd1 & AE_ONE) {
			ifp->if_collisions++;
		}
		else if (tmd->tmd1 & AE_MORE) {
			/* Real number is unknown. */
			ifp->if_collisions += 2;
		}
		AENEXTTMP;
	} while ((tmd->tmd1 & AE_OWN) == 0);

	aestart(ifp);
	if (sc->sc_no_td == 0)
		ifp->if_timer = 0;
}

#define	AENEXTRMP \
	if (++bix == AERBUF) bix = 0, rmd = sc->sc_r2->aer2_rmd; else ++rmd

/*
 * Ethernet interface receiver interrupt.
 * If input error just drop packet.
 * Decapsulate packet based on type and pass to type specific
 * higher-level input routine.
 */
void
aerint(sc)
	struct ae_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	register int bix = sc->sc_rmd;
	register struct aermd *rmd = &sc->sc_r2->aer2_rmd[bix];

	/*
	 * Out of sync with hardware, should never happen?
	 */
	if (rmd->rmd1 & AE_OWN) {
#ifdef AEDEBUG
		printf("%s: extra rint\n", sc->sc_dev.dv_xname);
#endif
		return;
	}

	/*
	 * Process all buffers with valid data
	 */
	do {
		sc->sc_r1->aer1_rdp =  AE_RINT | AE_INEA;
		if (rmd->rmd1 & (AE_FRAM | AE_OFLO | AE_CRC | AE_RBUFF)) {
			ifp->if_ierrors++;
			if ((rmd->rmd1 & (AE_FRAM | AE_OFLO | AE_ENP)) == (AE_FRAM | AE_ENP))
				printf("%s: framing error\n", sc->sc_dev.dv_xname);
			if ((rmd->rmd1 & (AE_OFLO | AE_ENP)) == AE_OFLO)
				printf("%s: overflow\n", sc->sc_dev.dv_xname);
			if ((rmd->rmd1 & (AE_CRC | AE_OFLO | AE_ENP)) == (AE_CRC | AE_ENP))
				printf("%s: crc mismatch\n", sc->sc_dev.dv_xname);
			if (rmd->rmd1 & AE_RBUFF)
				printf("%s: receive buffer error\n", sc->sc_dev.dv_xname);
		} else if ((rmd->rmd1 & (AE_STP | AE_ENP)) != (AE_STP | AE_ENP)) {
			do {
				rmd->rmd3 = 0;
				rmd->rmd1 = AE_OWN;
				AENEXTRMP;
			} while ((rmd->rmd1 & (AE_OWN | AE_ERR | AE_STP | AE_ENP)) == 0);

			sc->sc_rmd = bix;
			printf("%s: chained buffer\n", sc->sc_dev.dv_xname);
			if ((rmd->rmd1 & (AE_OWN | AE_ERR | AE_STP | AE_ENP)) != AE_ENP) {
				aereset(sc);
				return;
			}
		} else
			aeread(sc, sc->sc_r2->aer2_rbuf[bix], SWAP(rmd->rmd3) - 4);

		rmd->rmd1 = AE_OWN;
		rmd->rmd2 = SWAP(-ETHER_MAX_LEN);
		rmd->rmd3 = 0;

		AENEXTRMP;
	} while ((rmd->rmd1 & AE_OWN) == 0);

	sc->sc_rmd = bix;
}

void
aeread(sc, buf, len)
	register struct ae_softc *sc;
	u_char *buf;
	int len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
    	struct mbuf *m;
	struct ether_header *eh;

	if (len <= sizeof(struct ether_header) || len > ETHER_MAX_LEN) {
		printf("%s: invalid packet size %d; dropping\n",
			sc->sc_dev.dv_xname, len);
		ifp->if_ierrors++;
		return;
	}

	/* Pull packet off interface. */
	m = aeget(sc, buf, len);
	if (m == 0) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

	/* We assume that the header fit entirely in one mbuf. */
	eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
	/*
	 * Check if there's a bpf filter listening on this interface.
	 * If so, hand off the raw packet to bpf, which must deal with
	 * trailers in its own way.
	 */
	if (ifp->if_bpf) {
		bpf_mtap(ifp->if_bpf, m);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no bpf listeners.  And if we are in promiscuous
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
	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);
}

/*
 *	Here come the two replacements for bcopy() and bzero() as
 *	WORD-access for writing to the board is absolutely required!
 *	They could use some tuning as this is time-critical (copying
 *	packet-data) and should be processed as fast as possible.
 *
 *	Thanks to Michael van Elst for pointing me to the problems
 *	that kept the 1.3 code from running under NetBSD current
 *	although it did mystically under 1.0 ...
 */

#define isodd(p) (((size_t)(p)) &  1)
#define align(p) ((ushort *)(((size_t)(p)) & ~1))

/*
 *	the first function copies WORD-aligned from the source to
 *	an arbitrary destination. It assumes that the CPU can handle
 *	mis-aligned writes to the destination itself.
 */
static void
wcopyfrom(a1, a2, length) /* bcopy() word-wise */
	char *a1, *a2;
	int length;
{
	ushort i, *b1, *b2;

	if (length > 0 && isodd(a1)) {
		b1 = align(a1);
		*a2 = *b1 & 0x00ff;	/* copy first byte with word access */
		++a2;
		++b1;
		--length;
	} else
		b1 = (ushort *)a1;
	b2 = (ushort *)a2;

	i = length / 2;

	while(i--)	/* copy all words */
		*b2++ = *b1++;

	if (length & 0x0001)	/* copy trailing byte */
		a2[length-1] = *b1 >> 8;
}

/*
 *	the second function copies WORD-aligned from an arbitrary
 *	source to the destination. It assumes that the CPU can handle
 *	mis-aligned reads from the source itself.
 */
static void
wcopyto(a1, a2, length) /* bcopy() word-wise */
	char *a1, *a2;
	int length;
{
	ushort i, *b1, *b2;

	if (length > 0 && isodd(a2)) {
		b2 = align(a2);
		i = (*b2 & 0xff00) | (*a1 & 0x00ff);	/* copy first byte with word access */
		*b2 = i;
		++a1;
		++b2;
		--length;
	} else
		b2 = (ushort *)a2;
	b1 = (ushort *)a1;

	i = length / 2;

	while(i--)			/* copy all words */
		*b2++ = *b1++;

	if (length & 0x0001) {
		i = (*b2 & 0x00ff) | (*b1 & 0xff00);	/* copy trailing byte */
		*b2 = i;
	}
}

static void
wzero(a1, length) /* bzero() word-wise */
	char *a1;
	int length;
{
	ushort i, *b1;

	/*
	 *  Is the destination word-aligned?
	 *  If not, handle the leading byte...
	 */

	if((length > 0) && ((size_t)a1 & 1)) {
		b1 = (ushort *)((size_t)a1 & ~1);
		*b1 &= 0xff00;
		--length;
		++a1;
	}

	/*
	 *  Perform the main zeroing word-wise...
	 */

	b1 = (ushort *)a1;
	i = length / 2;
	while(i--)
		*b1++ = 0;

	/*
	 *  Do we have to handle a trailing byte?
	 */

	if (length & 0x0001)
		*b1 &= 0x00ff;
}

/*
 * Routine to copy from mbuf chain to transmit
 * buffer in board local memory. 
 */
int
aeput(buffer, m)
	register char *buffer; 
	register struct mbuf *m;
{
	register struct mbuf *n;
	register int len = 0, tlen = 0;  
 
	for (; m; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		}
		wcopyto(mtod(m, char *), buffer, len);
		buffer += len;
		tlen += len;
		MFREE(m, n);
	}
 
	if (tlen < ETHER_MIN_LEN) {
		wzero(buffer, ETHER_MIN_LEN - tlen);
		tlen = ETHER_MIN_LEN;
	}

	return(tlen);
}

/*
 * Routine to copy from board local memory into mbufs.
 */
struct mbuf *
aeget(sc, buffer, totlen)
	struct ae_softc *sc;
	u_char *buffer;
	int totlen;
{
	register struct mbuf *m;
	struct mbuf *top, **mp;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = &sc->sc_arpcom.ac_if;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			len = MLEN;
		}

		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		wcopyfrom((caddr_t)buffer, mtod(m, caddr_t), len);
		buffer += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}

/*
 * Process an ioctl request.
 */
int
aeioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ae_softc *sc = aecd.cd_devs[ifp->if_unit];
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			aeinit(sc);	
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(sc->sc_arpcom.ac_enaddr);
			else
				wcopyto(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			aeinit(sc); /* does ae_setaddr() */
			break;
		    }
#endif
		default:
			aeinit(sc);
			break;
		}
		break;

#if defined(CCITT) && defined(LLC)
	case SIOCSIFCONF_X25:
		ifp->if_flags |= IFF_UP;
		ifa->ifa_rtrequest = (void (*)())cons_rtrequest; /* XXX */
		error = x25_llcglue(PRC_IFUP, ifa->ifa_addr);
		if (error == 0)
			aeinit(sc);
		break;
#endif /* CCITT && LLC */

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			aestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			aeinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			aeinit(sc);
		}
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
			aereset(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;
		break;
	}
	splx(s);
	return (error);
}

/*
 * Set up the logical address filter.
 */
void 
aesetladrf(ac, af)
	struct arpcom *ac;
	u_int16_t *af;
{
	struct ifnet *ifp = &ac->ac_if; 
	struct ether_multi *enm;
	register u_char *cp, c;
	register u_int32_t crc;
	register int i, len;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if (ifp->if_flags & IFF_PROMISC)
		goto allmulti;

	af[0] = af[1] = af[2] = af[3] = 0x0000;
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
			goto allmulti;
		}

		cp = enm->enm_addrlo;
		crc = 0xffffffff;
		for (len = sizeof(enm->enm_addrlo); --len >= 0;) {
			c = *cp++;
			for (i = 8; --i >= 0;) {
				if ((crc & 0x01) ^ (c & 0x01)) {
					crc >>= 1;
					crc ^= 0xedb88320;
				} else
					crc >>= 1;
				c >>= 1;
			}
		}
		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Turn on the corresponding bit in the filter. */
		af[crc >> 4] |= 1 << SWAP(crc & 0x1f);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	af[0] = af[1] = af[2] = af[3] = 0xffff;
}

#endif /* NAE > 0 */
