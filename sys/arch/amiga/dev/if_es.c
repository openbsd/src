/*	$OpenBSD: if_es.c,v 1.14 2002/12/09 00:45:37 millert Exp $	*/
/*	$NetBSD: if_es.c,v 1.16 1996/12/23 09:10:17 veego Exp $	*/

/*
 * Copyright (c) 1995 Michael L. Hitch
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
 *      This product includes software developed by Michael L. Hitch.
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
 * SMC 91C90 Single-Chip Ethernet Controller
 */

#include "bpfilter.h"

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

#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/if_esreg.h>

#define SWAP(x) (((x & 0xff) << 8) | ((x >> 8) & 0xff))

#define	USEPKTBUF

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * es_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	es_softc {
	struct	device sc_dev;
	struct	isr sc_isr;
	struct	arpcom sc_arpcom;	/* common Ethernet structures */
	void	*sc_base;		/* base address of board */
	short	sc_iflags;
	unsigned short sc_intctl;
#ifdef ESDEBUG
	int	sc_debug;
	short	sc_intbusy;		/* counter for interrupt rentered */
	short	sc_smcbusy;		/* counter for other rentry checks */
#endif
};

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef ESDEBUG
/* console error messages */
int	esdebug = 0;
int	estxints = 0;	/* IST_TX with TX enabled */
int	estxint2 = 0;	/* IST_TX active after IST_TX_EMPTY */
int	estxint3 = 0;	/* IST_TX interrupt processed */
int	estxint4 = 0;	/* ~TEMPTY counts */
int	estxint5 = 0;	/* IST_TX_EMPTY interrupts */
void	es_dump_smcregs(char *, union smcregs *);
#endif

int esintr(void *);
void esstart(struct ifnet *);
void eswatchdog(struct ifnet *);
int esioctl(struct ifnet *, u_long, caddr_t);
void esrint(struct es_softc *);
void estint(struct es_softc *);
void esinit(struct es_softc *);
void esreset(struct es_softc *);
void esstop(struct es_softc *);

int esmatch(struct device *, void *, void *);
void esattach(struct device *, struct device *, void *);

struct cfattach es_ca = {
	sizeof(struct es_softc), esmatch, esattach
};

struct cfdriver es_cd = {
	NULL, "es", DV_IFNET
};

int
esmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap = aux;

	/* Ameristar A4066 ethernet card */
	if (zap->manid == 1053 && zap->prodid == 10)
		return(1);

	return (0);
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
esattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct es_softc *sc = (void *)self;
	struct zbus_args *zap = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	unsigned long ser;

	sc->sc_base = zap->va;

	/*
	 * Manufacturer decides the 3 first bytes, i.e. ethernet vendor ID.
	 * (Currently only Ameristar.)
	 */
	sc->sc_arpcom.ac_enaddr[0] = 0x00;
	sc->sc_arpcom.ac_enaddr[1] = 0x00;
	sc->sc_arpcom.ac_enaddr[2] = 0x9f;

	/*
	 * Serial number for board contains last 3 bytes.
	 */
	ser = (unsigned long) zap->serno;

	sc->sc_arpcom.ac_enaddr[3] = (ser >> 16) & 0xff;
	sc->sc_arpcom.ac_enaddr[4] = (ser >>  8) & 0xff;
	sc->sc_arpcom.ac_enaddr[5] = (ser      ) & 0xff;

	/* Initialize ifnet structure. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_output = ether_output;
	ifp->if_ioctl = esioctl;
	ifp->if_start = esstart;
	ifp->if_watchdog = eswatchdog;
	/* XXX IFF_MULTICAST */
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Print additional info when attached. */
	printf(": address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	sc->sc_isr.isr_intr = esintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_isr);
}

#ifdef ESDEBUG
void
es_dump_smcregs(where, smc)
	char *where;
	union smcregs *smc;
{
	u_short cur_bank = smc->b0.bsr & BSR_MASK;

	printf("SMC registers %p from %s bank %04x\n", smc, where,
	    smc->b0.bsr);
	smc->b0.bsr = BSR_BANK0;
	printf("TCR %04x EPHSR %04x RCR %04x ECR %04x MIR %04x MCR %04x\n",
	    SWAP(smc->b0.tcr), SWAP(smc->b0.ephsr), SWAP(smc->b0.rcr),
	    SWAP(smc->b0.ecr), SWAP(smc->b0.mir), SWAP(smc->b0.mcr));
	smc->b1.bsr = BSR_BANK1;
	printf("CR %04x BAR %04x IAR %04x %04x %04x GPR %04x CTR %04x\n",
	    SWAP(smc->b1.cr), SWAP(smc->b1.bar), smc->b1.iar[0], smc->b1.iar[1],
	    smc->b1.iar[2], smc->b1.gpr, SWAP(smc->b1.ctr));
	smc->b2.bsr = BSR_BANK2;
	printf("MMUCR %04x PNR %02x ARR %02x FIFO %04x PTR %04x",
	    SWAP(smc->b2.mmucr), smc->b2.pnr, smc->b2.arr, smc->b2.fifo,
	    SWAP(smc->b2.ptr));
	printf(" DATA %04x %04x IST %02x MSK %02x\n", smc->b2.data,
	    smc->b2.datax, smc->b2.ist, smc->b2.msk);
	smc->b3.bsr = BSR_BANK3;
	printf("MT %04x %04x %04x %04x\n",
	    smc->b3.mt[0], smc->b3.mt[1], smc->b3.mt[2], smc->b3.mt[3]);
	smc->b3.bsr = cur_bank;
}
#endif

void
esstop(sc)
	struct es_softc* sc;
{
}

void
esinit(sc)
	struct es_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	union smcregs *smc = sc->sc_base;
	int s;

	s = splnet();

#ifdef ESDEBUG
	if (ifp->if_flags & IFF_RUNNING)
		es_dump_smcregs("esinit", smc);
#endif
	smc->b0.bsr = BSR_BANK0;	/* Select bank 0 */
	smc->b0.rcr = RCR_EPH_RST;
	smc->b0.rcr = 0;
	smc->b3.bsr = BSR_BANK3;	/* Select bank 3 */
	smc->b3.mt[0] = 0;		/* clear Multicast table */
	smc->b3.mt[1] = 0;
	smc->b3.mt[2] = 0;
	smc->b3.mt[3] = 0;
	/* XXX set Multicast table from Multicast list */
	smc->b1.bsr = BSR_BANK1;	/* Select bank 1 */
	smc->b1.cr = CR_RAM32K | CR_NO_WAIT_ST | CR_SET_SQLCH;
	smc->b1.ctr = CTR_AUTO_RLSE;
	smc->b1.iar[0] = *((unsigned short *) &sc->sc_arpcom.ac_enaddr[0]);
	smc->b1.iar[1] = *((unsigned short *) &sc->sc_arpcom.ac_enaddr[2]);
	smc->b1.iar[2] = *((unsigned short *) &sc->sc_arpcom.ac_enaddr[4]);
	smc->b2.bsr = BSR_BANK2;	/* Select bank 2 */
	smc->b2.mmucr = MMUCR_RESET;
	smc->b0.bsr = BSR_BANK0;	/* Select bank 0 */
	smc->b0.mcr = SWAP(0x0020);	/* reserve 8K for transmit buffers */
	smc->b0.tcr = TCR_PAD_EN | (TCR_TXENA + TCR_MON_CSN);
	smc->b0.rcr = RCR_FILT_CAR | RCR_STRIP_CRC | RCR_RXEN;
	/* XXX add multicast/promiscuous flags */
	smc->b2.bsr = BSR_BANK2;	/* Select bank 2 */
	smc->b2.msk = sc->sc_intctl = MSK_RX_OVRN | MSK_RX;

	/* Interface is now 'running', with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Attempt to start output, if any. */
	esstart(ifp);

	splx(s);
}

int
esintr(arg)
	void *arg;
{
	struct es_softc *sc = arg;
	u_short intsts, intact;
	union smcregs *smc;
	int s = splnet();

	smc = sc->sc_base;
#ifdef ESDEBUG
	while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2 &&
	    sc->sc_arpcom.ac_if.if_flags & IFF_RUNNING) {
		printf("%s: intr BSR not 2: %04x\n", sc->sc_dev.dv_xname,
		    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
	}
#endif
	intsts = smc->b2.ist;
	intact = smc->b2.msk & intsts;
	if ((intact) == 0) {
		splx(s);
		return (0);
	}
#ifdef ESDEBUG
	if (esdebug)
		printf ("%s: esintr ist %02x msk %02x",
		    sc->sc_dev.dv_xname, intsts, smc->b2.msk);
	if (sc->sc_intbusy++) {
		printf("%s: esintr re-entered\n", sc->sc_dev.dv_xname);
		panic("esintr re-entered");
	}
	if (sc->sc_smcbusy)
		printf("%s: esintr interrupted busy %d\n", sc->sc_dev.dv_xname,
		    sc->sc_smcbusy);
#endif
	smc->b2.msk = 0;
#ifdef ESDEBUG
	if (esdebug)
		printf ("=>%02x%02x pnr %02x arr %02x fifo %04x\n",
		    smc->b2.ist, smc->b2.ist, smc->b2.pnr, smc->b2.arr,
		    smc->b2.fifo);
#endif
	if (intact & IST_ALLOC) {
		sc->sc_intctl &= ~MSK_ALLOC;
#ifdef ESDEBUG
		if (esdebug || 1)
			printf ("%s: ist %02x", sc->sc_dev.dv_xname,
			    intsts);
#endif
		if ((smc->b2.arr & ARR_FAILED) == 0) {
			u_char save_pnr;
#ifdef ESDEBUG
			if (esdebug || 1)
				printf (" arr %02x\n", smc->b2.arr);
#endif
			save_pnr = smc->b2.pnr;
			smc->b2.pnr = smc->b2.arr;
			smc->b2.mmucr = MMUCR_RLSPKT;
			while (smc->b2.mmucr & MMUCR_BUSY)
				;
			smc->b2.pnr = save_pnr;
			sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		}
#ifdef ESDEBUG
		else if (esdebug || 1)
			printf (" IST_ALLOC with ARR_FAILED?\n");
#endif
	}
#ifdef ESDEBUG
	while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
		printf("%s: intr+ BSR not 2: %04x\n", sc->sc_dev.dv_xname,
		    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
	}
#endif
	while ((smc->b2.fifo & FIFO_REMPTY) == 0) {
		esrint(sc);
	}
#ifdef ESDEBUG
	while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
		printf("%s: intr++ BSR not 2: %04x\n", sc->sc_dev.dv_xname,
		    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
	}
#endif
	if (intact & IST_RX_OVRN) {
		printf ("%s: Overrun ist %02x", sc->sc_dev.dv_xname,
		    intsts);
		smc->b2.ist = ACK_RX_OVRN;
		printf ("->%02x\n", smc->b2.ist);
		sc->sc_arpcom.ac_if.if_ierrors++;
	}
	if (intact & IST_TX_EMPTY) {
		u_short ecr;
#ifdef ESDEBUG
		if (esdebug)
			printf ("%s: TX EMPTY %02x",
			    sc->sc_dev.dv_xname, intsts);
		++estxint5;		/* count # IST_TX_EMPTY ints */
#endif
		smc->b2.ist = ACK_TX_EMPTY;
		sc->sc_intctl &= ~(MSK_TX_EMPTY | MSK_TX);
#ifdef ESDEBUG
		if (esdebug)
			printf ("->%02x intcl %x pnr %02x arr %02x\n",
			    smc->b2.ist, sc->sc_intctl, smc->b2.pnr,
			    smc->b2.arr);
#endif
		if (smc->b2.ist & IST_TX) {
			intact |= IST_TX;
#ifdef ESDEBUG
			++estxint2;		/* count # TX after TX_EMPTY */
#endif
		} else {
			smc->b0.bsr = BSR_BANK0;
			ecr = smc->b0.ecr;	/* Get error counters */
			if (ecr & 0xff00)
				sc->sc_arpcom.ac_if.if_collisions += ((ecr >> 8) & 15) +
				    ((ecr >> 11) & 0x1e);
			smc->b2.bsr = BSR_BANK2;
#if 0
			smc->b2.mmucr = MMUCR_RESET_TX; /* XXX reset TX FIFO */
#endif
		}
	}
	if (intact & IST_TX) {
		u_char tx_pnr, save_pnr;
		u_short save_ptr, ephsr, tcr;
		int n = 0;
#ifdef ESDEBUG
		if (esdebug) {
			printf ("%s: TX INT ist %02x",
			    sc->sc_dev.dv_xname, intsts);
			printf ("->%02x\n", smc->b2.ist);
		}
		++estxint3;			/* count # IST_TX */
#endif
zzzz:
#ifdef ESDEBUG
		++estxint4;			/* count # ~TEMPTY */
#endif
		smc->b0.bsr = BSR_BANK0;
		ephsr = smc->b0.ephsr;		/* get EPHSR */
		tcr = smc->b0.tcr;		/* and TCR */
		smc->b2.bsr = BSR_BANK2;
		save_ptr = smc->b2.ptr;
		save_pnr = smc->b2.pnr;
		tx_pnr = smc->b2.fifo >> 8;	/* pktno from completion fifo */
		smc->b2.pnr = tx_pnr;		/* set TX packet number */
		smc->b2.ptr = PTR_READ;		/* point to status word */
#if 0 /* XXXX */
		printf("%s: esintr TXINT IST %02x PNR %02x(%d)",
		    sc->sc_dev.dv_xname, smc->b2.ist,
		    tx_pnr, n);
		printf(" Status %04x", smc->b2.data);
		printf(" EPHSR %04x\n", ephsr);
#endif
		if ((smc->b2.data & EPHSR_TX_SUC) == 0 && (tcr & TCR_TXENA) == 0) {
			/*
			 * Transmitter was stopped for some error.  Enqueue
			 * the packet again and restart the transmitter.
			 * May need some check to limit the number of retries.
			 */
			smc->b2.mmucr = MMUCR_ENQ_TX;
			smc->b0.bsr = BSR_BANK0;
			smc->b0.tcr |= TCR_TXENA;
			smc->b2.bsr = BSR_BANK2;
			sc->sc_arpcom.ac_if.if_oerrors++;
			sc->sc_intctl |= MSK_TX_EMPTY | MSK_TX;
		} else {
			/*
			 * This shouldn't have happened:  IST_TX indicates
			 * the TX completion FIFO is not empty, but the
			 * status for the packet on the completion FIFO
			 * shows that the transmit was successful.  Since
			 * AutoRelease is being used, a successful transmit
			 * should not result in a packet on the completion
			 * FIFO.  Also, that packet doesn't seem to want
			 * to be acknowledged.  If this occurs, just reset
			 * the TX FIFOs.
			 */
#if 1
			if (smc->b2.ist & IST_TX_EMPTY) {
				smc->b2.mmucr = MMUCR_RESET_TX;
				sc->sc_intctl &= ~(MSK_TX_EMPTY | MSK_TX);
			}
#endif
#ifdef ESDEBUG
			++estxints;	/* count IST_TX with TX enabled */
#endif
		}
		smc->b2.pnr = save_pnr;
		smc->b2.ptr = save_ptr;
		smc->b2.ist = ACK_TX;

		if ((smc->b2.fifo & FIFO_TEMPTY) == 0 && n++ < 32) {
#if 0 /* XXXX */
			printf("%s: multiple TX int(%2d) pnr %02x ist %02x fifo %04x",
			    sc->sc_dev.dv_xname, n, tx_pnr, smc->b2.ist, smc->b2.fifo);
			smc->w2.istmsk = ACK_TX << 8;
			printf(" %04x\n", smc->b2.fifo);
#endif
			if (tx_pnr != (smc->b2.fifo >> 8))
				goto zzzz;
		}
	}
	/* output packets */
	estint(sc);
#ifdef ESDEBUG
	while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
		printf("%s: intr+++ BSR not 2: %04x\n", sc->sc_dev.dv_xname,
		    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
	}
#endif
	smc->b2.msk = sc->sc_intctl;
#ifdef ESDEBUG
	if (--sc->sc_intbusy) {
		printf("%s: esintr busy on exit\n", sc->sc_dev.dv_xname);
		panic("esintr busy on exit");
	}
#endif
	splx(s);
	return (1);
}

void
esrint(sc)
	struct es_softc *sc;
{
	union smcregs *smc = sc->sc_base;
	u_short len;
	short cnt;
	u_short pktctlw, pktlen, *buf;
	volatile u_short *data;
#if 0
	u_long *lbuf;
	volatile u_long *ldata;
#endif
	struct ifnet *ifp;
	struct mbuf *top, **mp, *m;
	struct ether_header *eh;
#ifdef USEPKTBUF
	u_char *b, pktbuf[1530];
#endif
#ifdef ESDEBUG
	int i;
#endif

#ifdef ESDEBUG
	if (esdebug)
		printf ("%s: esrint fifo %04x", sc->sc_dev.dv_xname,
		    smc->b2.fifo);
	if (sc->sc_smcbusy++) {
		printf("%s: esrint re-entered\n", sc->sc_dev.dv_xname);
		panic("esrint re-entered");
	}
	while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
		printf("%s: rint BSR not 2: %04x\n", sc->sc_dev.dv_xname,
		    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
	}
#endif
	data = (u_short *)&smc->b2.data;
	smc->b2.ptr = PTR_RCV | PTR_AUTOINCR | PTR_READ | SWAP(0x0002);
	(void) smc->b2.mmucr;
#ifdef ESDEBUG
	if (esdebug)
		printf ("->%04x", smc->b2.fifo);
#endif
	len = *data;
	len = SWAP(len);	/* Careful of macro side-effects */
#ifdef ESDEBUG
	if (esdebug)
		printf (" length %d", len);
#endif
	smc->b2.ptr = PTR_RCV | (PTR_AUTOINCR + PTR_READ) | SWAP(0x0000);
	(void) smc->b2.mmucr;
	pktctlw = *data;
	pktlen = *data;
	pktctlw = SWAP(pktctlw);
	pktlen = SWAP(pktlen) - 6;
	if (pktctlw & RFSW_ODDFRM)
		pktlen++;
	if (len > 1530) {
		printf("%s: Corrupted packet length-sts %04x bytcnt %04x len %04x bank %04x\n",
		    sc->sc_dev.dv_xname, pktctlw, pktlen, len, smc->b2.bsr);
		/* XXX ignore packet, or just truncate? */
#if defined(ESDEBUG) && defined(DDB)
		if ((smc->b2.bsr & BSR_MASK) != BSR_BANK2)
			Debugger();
#endif
		smc->b2.bsr = BSR_BANK2;
		smc->b2.mmucr = MMUCR_REMRLS_RX;
		while (smc->b2.mmucr & MMUCR_BUSY)
			;
		++sc->sc_arpcom.ac_if.if_ierrors;
#ifdef ESDEBUG
		if (--sc->sc_smcbusy) {
			printf("%s: esrintr busy on bad packet exit\n",
			    sc->sc_dev.dv_xname);
			panic("esrintr busy on exit");
		}
#endif
		return;
	}
#ifdef USEPKTBUF
#if 0
	lbuf = (u_long *) pktbuf;
	ldata = (u_long *)data;
	cnt = (len - 4) / 4;
	while (cnt--)
		*lbuf++ = *ldata;
	if ((len - 4) & 2) {
		buf = (u_short *) lbuf;
		*buf = *data;
	}
#else
	buf = (u_short *)pktbuf;
	cnt = (len - 4) / 2;
	while (cnt--)
		*buf++ = *data;
#endif
	smc->b2.mmucr = MMUCR_REMRLS_RX;
	while (smc->b2.mmucr & MMUCR_BUSY)
		;
#ifdef ESDEBUG
	if (pktctlw & (RFSW_ALGNERR | RFSW_BADCRC | RFSW_TOOLNG | RFSW_TOOSHORT)) {
		printf ("%s: Packet error %04x\n", sc->sc_dev.dv_xname, pktctlw);
		/* count input error? */
	}
	if (esdebug) {
		printf (" pktctlw %04x pktlen %04x fifo %04x\n", pktctlw, pktlen,
		    smc->b2.fifo);
		for (i = 0; i < pktlen; ++i)
			printf ("%02x%s", pktbuf[i], ((i & 31) == 31) ? "\n" :
			    "");
		if (i & 31)
			printf ("\n");
	}
#endif
#else	/* USEPKTBUF */
	/* XXX copy directly from controller to mbuf */
#ifdef ESDEBUG
	if (pktctlw & (RFSW_ALGNERR | RFSW_BADCRC | RFSW_TOOLNG | RFSW_TOOSHORT)) {
		printf ("%s: Packet error %04x\n", sc->sc_dev.dv_xname, pktctlw);
		/* count input error? */
	}
	if (esdebug) {
		printf (" pktctlw %04x pktlen %04x fifo %04x\n", pktctlw, pktlen,
		    smc->b2.fifo);
	}
#endif
#endif /* USEPKTBUF */
	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_ipackets++;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = pktlen;
	len = MHLEN;
	top = NULL;
	mp = &top;
#ifdef USEPKTBUF
	b = pktbuf;
#endif
	while (pktlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return;
			}
			len = MLEN;
		}
		if (pktlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(pktlen, len);
#ifdef USEPKTBUF
		bcopy((caddr_t)b, mtod(m, caddr_t), len);
		b += len;
#else	/* USEPKTBUF */
		buf = mtod(m, u_short *);
		cnt = len / 2;
		while (cnt--)
			*buf++ = *data;
		if (len & 1)
			*buf = *data;	/* XXX should be byte store */
#ifdef ESDEBUG
		if (esdebug) {
			buf = mtod(m, u_short *);
			for (i = 0; i < len; ++i)
				printf ("%02x%s", ((u_char *)buf)[i],
				    ((i & 31) == 31) ? "\n" : "");
			if (i & 31)
				printf ("\n");
		}
#endif
#endif	/* USEPKTBUF */
		pktlen -= len;
		*mp = m;
		mp = &m->m_next;
	}
#ifndef USEPKTBUF
	smc->b2.mmucr = MMUCR_REMRLS_RX;
	while (smc->b2.mmucr & MMUCR_BUSY)
		;
#endif
	eh = mtod(top, struct ether_header *);
#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.  If so, hand off
	 * the raw packet to bpf.
	 */
	if (sc->sc_arpcom.ac_if.if_bpf)
		bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, top);
#endif
	top->m_pkthdr.len -= sizeof (*eh);
	top->m_len -= sizeof (*eh);
	top->m_data += sizeof (*eh);

	ether_input(ifp, eh, top);
#ifdef ESDEBUG
	if (--sc->sc_smcbusy) {
		printf("%s: esintr busy on exit\n", sc->sc_dev.dv_xname);
		panic("esintr busy on exit");
	}
#endif
}

void
estint(sc)
	struct es_softc *sc;
{

	esstart(&sc->sc_arpcom.ac_if);
}

void
esstart(ifp)
	struct ifnet *ifp;
{
	struct es_softc *sc = ifp->if_softc;
	union smcregs *smc = sc->sc_base;
	struct mbuf *m0, *m;
#ifdef USEPKTBUF
	u_short pktbuf[ETHERMTU + 2];
#else
	u_short oddbyte, needbyte;
#endif
	u_short pktctlw, pktlen;
	u_short *buf;
	volatile u_short *data;
#if 0
	u_long *lbuf;
	volatile u_long *ldata;
#endif
	short cnt;
	int i;
	u_char active_pnr;

	if ((sc->sc_arpcom.ac_if.if_flags & (IFF_RUNNING | IFF_OACTIVE)) !=
	    IFF_RUNNING)
		return;

#ifdef ESDEBUG
	if (sc->sc_smcbusy++) {
		printf("%s: esstart re-entered\n", sc->sc_dev.dv_xname);
		panic("esstart re-entred");
	}
	while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
		printf("%s: esstart BSR not 2: %04x\n", sc->sc_dev.dv_xname,
		    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
	}
#endif
	for (;;) {
#ifdef ESDEBUG
		u_short start_ptr, end_ptr;
#endif
		/*
		 * Sneak a peek at the next packet to get the length
		 * and see if the SMC 91C90 can accept it.
		 */
		m = sc->sc_arpcom.ac_if.if_snd.ifq_head;
		if (!m)
			break;
#ifdef ESDEBUG
		if (esdebug && (m->m_next || m->m_len & 1))
			printf("%s: esstart m_next %p m_len %d\n",
			    sc->sc_dev.dv_xname, m->m_next, m->m_len);
#endif
		for (m0 = m, pktlen = 0; m0; m0 = m0->m_next)
			pktlen += m0->m_len;
		pktctlw = 0;
		pktlen += 4;
		if (pktlen & 1)
			++pktlen;	/* control byte after last byte */
		else
			pktlen += 2;	/* control byte after pad byte */
		smc->b2.mmucr = MMUCR_ALLOC | (pktlen & 0x0700);
		for (i = 0; i <= 5; ++i)
			if ((smc->b2.arr & ARR_FAILED) == 0)
				break;
		if (smc->b2.arr & ARR_FAILED) {
			sc->sc_arpcom.ac_if.if_flags |= IFF_OACTIVE;
			sc->sc_intctl |= MSK_ALLOC;
			break;
		}
		active_pnr = smc->b2.pnr = smc->b2.arr;

#ifdef ESDEBUG
		while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
			printf("%s: esstart+ BSR not 2: %04x\n", sc->sc_dev.dv_xname,
			    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
		}
#endif
		IF_DEQUEUE(&sc->sc_arpcom.ac_if.if_snd, m);
		smc->b2.ptr = PTR_AUTOINCR;
		(void) smc->b2.mmucr;
		data = (u_short *)&smc->b2.data;
		*data = SWAP(pktctlw);
		*data = SWAP(pktlen);
#ifdef ESDEBUG
		while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
			printf("%s: esstart++ BSR not 2: %04x\n", sc->sc_dev.dv_xname,
			    smc->b2.bsr);
			smc->b2.bsr = BSR_BANK2;
		}
#endif
#ifdef USEPKTBUF
		i = 0;
		for (m0 = m; m; m = m->m_next) {
			bcopy(mtod(m, caddr_t), (char *)pktbuf + i, m->m_len);
			i += m->m_len;
		}

		if (i & 1)	/* Figure out where to put control byte */
			pktbuf[i/2] = (pktbuf[i/2] & 0xff00) | CTLB_ODD;
		else
			pktbuf[i/2] = 0;
		pktlen -= 4;
#ifdef ESDEBUG
		if (pktlen > sizeof(pktbuf) && i > (sizeof(pktbuf) * 2))
			printf("%s: esstart packet longer than pktbuf\n",
			    sc->sc_dev.dv_xname);
#endif
#if 0 /* doesn't quite work? */
		lbuf = (u_long *)(pktbuf);
		ldata = (u_long *)data;
		cnt = pktlen / 4;
		while(cnt--)
			*ldata = *lbuf++;
		if (pktlen & 2) {
			buf = (u_short *)lbuf;
			*data = *buf;
		}
#else
#ifdef ESDEBUG
		while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
			printf("%s: esstart++2 BSR not 2: %04x\n", sc->sc_dev.dv_xname,
			    smc->b2.bsr);
			smc->b2.bsr = BSR_BANK2;
		}
		start_ptr = SWAP(smc->b2.ptr);	/* save PTR before copy */
#endif
		buf = pktbuf;
		cnt = pktlen / 2;
		while (cnt--)
			*data = *buf++;
#ifdef ESDEBUG
		end_ptr = SWAP(smc->b2.ptr);	/* save PTR after copy */
#endif
#endif
#else	/* USEPKTBUF */
		pktctlw = 0;
		oddbyte = needbyte = 0;
		for (m0 = m; m; m = m->m_next) {
			buf = mtod(m, u_short *);
			cnt = m->m_len / 2;
			if (needbyte) {
				oddbyte |= *buf >> 8;
				*data = oddbyte;
			}
			while (cnt--)
				*data = *buf++;
			if (m->m_len & 1)
				pktctlw = (*buf & 0xff00) | CTLB_ODD;
			if (m->m_len & 1 && m->m_next)
				printf("%s: esstart odd byte count in mbuf\n",
				    sc->sc_dev.dv_xname);
		}
		*data = pktctlw;
#endif	/* USEPKTBUF */
		while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
			/*
			 * The bank select register has changed.  This seems
			 * to happen with my A2000/Zeus once in a while.  It
			 * appears that the Ethernet chip resets while
			 * copying the transmit buffer.  Requeue the current
			 * transmit buffer and reinitialize the interface.
			 * The initialize routine will take care of
			 * retransmitting the buffer.  mhitch
			 */
#ifdef DIAGNOSTIC
			printf("%s: esstart+++ BSR not 2: %04x\n",
			    sc->sc_dev.dv_xname, smc->b2.bsr);
#endif
			smc->b2.bsr = BSR_BANK2;
#ifdef ESDEBUG
			printf("start_ptr %04x end_ptr %04x cur ptr %04x\n",
			    start_ptr, end_ptr, SWAP(smc->b2.ptr));
			--sc->sc_smcbusy;
#endif
			IF_PREPEND(&sc->sc_arpcom.ac_if.if_snd, m0);
			esinit(sc);	/* It's really hosed - reset */
			return;
		}
		smc->b2.mmucr = MMUCR_ENQ_TX;
		if (smc->b2.pnr != active_pnr)
			printf("%s: esstart - PNR changed %x->%x\n",
			    sc->sc_dev.dv_xname, active_pnr, smc->b2.pnr);
#if NBPFILTER > 0
		if (sc->sc_arpcom.ac_if.if_bpf)
			bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, m0);
#endif
		m_freem(m0);
		sc->sc_arpcom.ac_if.if_opackets++;	/* move to interrupt? */
		sc->sc_intctl |= MSK_TX_EMPTY | MSK_TX;
	}
	smc->b2.msk = sc->sc_intctl;
#ifdef ESDEBUG
	while ((smc->b2.bsr & BSR_MASK) != BSR_BANK2) {
		printf("%s: esstart++++ BSR not 2: %04x\n", sc->sc_dev.dv_xname,
		    smc->b2.bsr);
		smc->b2.bsr = BSR_BANK2;
	}
	if (--sc->sc_smcbusy) {
		printf("%s: esstart busy on exit\n", sc->sc_dev.dv_xname);
		panic("esstart busy on exit");
	}
#endif
}

int
esioctl(ifp, command, data)
	register struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct es_softc *sc = ifp->if_softc;
	register struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch (command) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			esinit(sc);
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
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			/* Set new address. */
			esinit(sc);
			break;
		    }
#endif
		default:
			esinit(sc);
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
			esstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    	   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			esinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			esstop(sc);
			esinit(sc);
		}
#ifdef ESDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			esdebug = sc->sc_debug = 1;
		else
			esdebug = sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			/* XXX */
			error = 0;
		}
		break;

	default:
		error = EINVAL;
	}

	splx(s);
	return (error);
}

void
esreset(sc)
	struct es_softc *sc;
{
	int s;

	s = splnet();
	esstop(sc);
	esinit(sc);
	splx(s);
}

void
eswatchdog(ifp)
	struct ifnet *ifp;
{
	struct es_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	esreset(sc);
}
