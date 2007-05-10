/*	$OpenBSD: if_de.c,v 1.8 2007/05/10 17:59:26 deraadt Exp $ */
/*	$NetBSD: if_de.c,v 1.11 2001/11/13 07:11:24 lukem Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)if_de.c	7.12 (Berkeley) 12/16/90
 */

/*
 * DEC DEUNA interface
 *
 *	Lou Salkind
 *	New York University
 *
 *	Rewritten by Ragge 30 April 2000 to match new world.
 *
 * TODO:
 *	timeout routine (get statistics)
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_dl.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>

#include <dev/qbus/ubavar.h>
#include <dev/qbus/if_dereg.h>
#include <dev/qbus/if_uba.h>

/*
 * Be careful with transmit/receive buffers, each entry steals 4 map
 * registers, and there is only 496 on one unibus...
 */
#define NRCV	7	/* number of receive buffers (must be > 1) */
#define NXMT	3	/* number of transmit buffers */

/*
 * Structure containing the elements that must be in DMA-safe memory.
 */
struct	de_cdata {
	/* the following structures are always mapped in */
	struct	de_pcbb dc_pcbb;	/* port control block */
	struct	de_ring dc_xrent[NXMT]; /* transmit ring entries */
	struct	de_ring dc_rrent[NRCV]; /* receive ring entries */
	struct	de_udbbuf dc_udbbuf;	/* UNIBUS data buffer */
	/* end mapped area */
};

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ds_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 * We also have, for each interface, a UBA interface structure, which
 * contains information about the UNIBUS resources held by the interface:
 * map registers, buffered data paths, etc.  Information is cached in this
 * structure for use by the if_uba.c routines in running the interface
 * efficiently.
 */
struct	de_softc {
	struct	device sc_dev;		/* Configuration common part */
	struct	ethercom sc_ec;		/* Ethernet common part */
#define sc_if	sc_ec.ec_if		/* network-visible interface */
	bus_space_tag_t sc_iot;
	bus_addr_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	int	sc_flags;
#define	DSF_MAPPED	1
	struct ubinfo sc_ui;
	struct de_cdata *sc_dedata;	/* Control structure */
	struct de_cdata *sc_pdedata;	/* Bus-mapped control structure */
	struct	ifubinfo sc_ifuba;      /* UNIBUS resources */
	struct	ifrw sc_ifr[NRCV];      /* UNIBUS receive buffer maps */
	struct	ifxmt sc_ifw[NXMT];     /* UNIBUS receive buffer maps */

	int	sc_xindex;		/* UNA index into transmit chain */
	int	sc_rindex;		/* UNA index into receive chain */
	int	sc_xfree;		/* index for next transmit buffer */
	int	sc_nxmit;		/* # of transmits in progress */
	void *sc_sh;			/* shutdownhook cookie */
};

static	int dematch(struct device *, struct cfdata *, void *);
static	void deattach(struct device *, struct device *, void *);
static	void dewait(struct de_softc *, char *);
static	int deinit(struct ifnet *);
static	int deioctl(struct ifnet *, u_long, caddr_t);
static	void dereset(struct device *);
static	void destop(struct ifnet *, int);
static	void destart(struct ifnet *);
static	void derecv(struct de_softc *);
static	void deintr(void *);
static	void deshutdown(void *);

struct	cfattach de_ca = {
	sizeof(struct de_softc), dematch, deattach
};

#define DE_WCSR(csr, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, csr, val)
#define DE_WLOW(val) \
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DE_PCSR0, val)
#define DE_WHIGH(val) \
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, DE_PCSR0 + 1, val)
#define DE_RCSR(csr) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, csr)

#define LOWORD(x)	((int)(x) & 0xffff)
#define HIWORD(x)	(((int)(x) >> 16) & 0x3)
/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.  We get the ethernet address here.
 */
void
deattach(struct device *parent, struct device *self, void *aux)
{
	struct uba_attach_args *ua = aux;
	struct de_softc *sc = (struct de_softc *)self;
	struct ifnet *ifp = &sc->sc_if;
	u_int8_t myaddr[ETHER_ADDR_LEN];
	int csr1, error;
	char *c;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;

	/*
	 * What kind of a board is this?
	 * The error bits 4-6 in pcsr1 are a device id as long as
	 * the high byte is zero.
	 */
	csr1 = DE_RCSR(DE_PCSR1);
	if (csr1 & 0xff60)
		c = "broken";
	else if (csr1 & 0x10)
		c = "delua";
	else
		c = "deuna";

	/*
	 * Reset the board and temporarily map
	 * the pcbb buffer onto the Unibus.
	 */
	DE_WCSR(DE_PCSR0, 0);		/* reset INTE */
	DELAY(100);
	DE_WCSR(DE_PCSR0, PCSR0_RSET);
	dewait(sc, "reset");

	sc->sc_ui.ui_size = sizeof(struct de_cdata);
	if ((error = ubmemalloc((struct uba_softc *)parent, &sc->sc_ui, 0)))
		return printf(": failed ubmemalloc(), error = %d\n", error);
	sc->sc_dedata = (struct de_cdata *)sc->sc_ui.ui_vaddr;

	/*
	 * Tell the DEUNA about our PCB
	 */
	DE_WCSR(DE_PCSR2, LOWORD(sc->sc_ui.ui_baddr));
	DE_WCSR(DE_PCSR3, HIWORD(sc->sc_ui.ui_baddr));
	DE_WLOW(CMD_GETPCBB);
	dewait(sc, "pcbb");

	sc->sc_dedata->dc_pcbb.pcbb0 = FC_RDPHYAD;
	DE_WLOW(CMD_GETCMD);
	dewait(sc, "read addr ");

	bcopy((caddr_t)&sc->sc_dedata->dc_pcbb.pcbb2, myaddr, sizeof (myaddr));
	printf(": %s, address %s\n", c, ether_sprintf(myaddr));

	uba_intr_establish(ua->ua_icookie, ua->ua_cvec, deintr, sc, 
	    &sc->sc_intrcnt);
	uba_reset_establish(dereset, &sc->sc_dev);

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof ifp->if_xname);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST|IFF_ALLMULTI;
	ifp->if_ioctl = deioctl;
	ifp->if_start = destart;
	ifp->if_init = deinit;
	ifp->if_stop = destop;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp, myaddr);
	ubmemfree((struct uba_softc *)parent, &sc->sc_ui);

	sc->sc_sh = shutdownhook_establish(deshutdown, sc);
}

void
destop(struct ifnet *ifp, int a)
{
	struct de_softc *sc = ifp->if_softc;

	DE_WLOW(0);
	DELAY(5000);
	DE_WLOW(PCSR0_RSET);
}


/*
 * Reset of interface after UNIBUS reset.
 */
void
dereset(struct device *dev)
{
	struct de_softc *sc = (void *)dev;

	sc->sc_if.if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	sc->sc_flags &= ~DSF_MAPPED;
	sc->sc_pdedata = NULL;	/* All mappings lost */
	DE_WCSR(DE_PCSR0, PCSR0_RSET);
	dewait(sc, "reset");
	deinit(&sc->sc_if);
}

/*
 * Initialization of interface; clear recorded pending
 * operations, and reinitialize UNIBUS usage.
 */
int
deinit(struct ifnet *ifp)
{
	struct de_softc *sc = ifp->if_softc;
	struct de_cdata *dc, *pdc;
	struct ifrw *ifrw;
	struct ifxmt *ifxp;
	struct de_ring *rp;
	int s, error;

	if (ifp->if_flags & IFF_RUNNING)
		return 0;
	if ((sc->sc_flags & DSF_MAPPED) == 0) {
		if (if_ubaminit(&sc->sc_ifuba, (void *)sc->sc_dev.dv_parent,
		    MCLBYTES, sc->sc_ifr, NRCV, sc->sc_ifw, NXMT)) {
			printf("%s: can't initialize\n", sc->sc_dev.dv_xname);
			ifp->if_flags &= ~IFF_UP;
			return 0;
		}
		sc->sc_ui.ui_size = sizeof(struct de_cdata);
		if ((error = ubmemalloc((void *)sc->sc_dev.dv_parent,
		    &sc->sc_ui, 0))) {
			printf(": unable to ubmemalloc(), error = %d\n", error);
			return 0;
		}
		sc->sc_pdedata = (struct de_cdata *)sc->sc_ui.ui_baddr;
		sc->sc_dedata = (struct de_cdata *)sc->sc_ui.ui_vaddr;
		sc->sc_flags |= DSF_MAPPED;
	}

	/*
	 * Tell the DEUNA about our PCB
	 */
	DE_WCSR(DE_PCSR2, LOWORD(sc->sc_pdedata));
	DE_WCSR(DE_PCSR3, HIWORD(sc->sc_pdedata));
	DE_WLOW(0);		/* reset INTE */
	DELAY(500);
	DE_WLOW(CMD_GETPCBB);
	dewait(sc, "pcbb");

	dc = sc->sc_dedata;
	pdc = sc->sc_pdedata;
	/* set the transmit and receive ring header addresses */
	dc->dc_pcbb.pcbb0 = FC_WTRING;
	dc->dc_pcbb.pcbb2 = LOWORD(&pdc->dc_udbbuf);
	dc->dc_pcbb.pcbb4 = HIWORD(&pdc->dc_udbbuf);

	dc->dc_udbbuf.b_tdrbl = LOWORD(&pdc->dc_xrent[0]);
	dc->dc_udbbuf.b_tdrbh = HIWORD(&pdc->dc_xrent[0]);
	dc->dc_udbbuf.b_telen = sizeof (struct de_ring) / sizeof(u_int16_t);
	dc->dc_udbbuf.b_trlen = NXMT;
	dc->dc_udbbuf.b_rdrbl = LOWORD(&pdc->dc_rrent[0]);
	dc->dc_udbbuf.b_rdrbh = HIWORD(&pdc->dc_rrent[0]);
	dc->dc_udbbuf.b_relen = sizeof (struct de_ring) / sizeof(u_int16_t);
	dc->dc_udbbuf.b_rrlen = NRCV;

	DE_WLOW(CMD_GETCMD);
	dewait(sc, "wtring");

	sc->sc_dedata->dc_pcbb.pcbb0 = FC_WTMODE;
	sc->sc_dedata->dc_pcbb.pcbb2 = MOD_TPAD|MOD_HDX|MOD_DRDC|MOD_ENAL;
	DE_WLOW(CMD_GETCMD);
	dewait(sc, "wtmode");

	/* set up the receive and transmit ring entries */
	ifxp = &sc->sc_ifw[0];
	for (rp = &dc->dc_xrent[0]; rp < &dc->dc_xrent[NXMT]; rp++) {
		rp->r_segbl = LOWORD(ifxp->ifw_info);
		rp->r_segbh = HIWORD(ifxp->ifw_info);
		rp->r_flags = 0;
		ifxp++;
	}
	ifrw = &sc->sc_ifr[0];
	for (rp = &dc->dc_rrent[0]; rp < &dc->dc_rrent[NRCV]; rp++) {
		rp->r_slen = MCLBYTES - 2;
		rp->r_segbl = LOWORD(ifrw->ifrw_info);
		rp->r_segbh = HIWORD(ifrw->ifrw_info);
		rp->r_flags = RFLG_OWN;
		ifrw++;
	}

	/* start up the board (rah rah) */
	s = splnet();
	sc->sc_rindex = sc->sc_xindex = sc->sc_xfree = sc->sc_nxmit = 0;
	sc->sc_if.if_flags |= IFF_RUNNING;
	DE_WLOW(PCSR0_INTE);			/* avoid interlock */
	destart(&sc->sc_if);		/* queue output packets */
	DE_WLOW(CMD_START|PCSR0_INTE);
	splx(s);
	return 0;
}

/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue,
 * and map it to the interface before starting the output.
 * Must be called from ipl >= our interrupt level.
 */
void
destart(struct ifnet *ifp)
{
	struct de_softc *sc = ifp->if_softc;
	struct de_cdata *dc;
	struct de_ring *rp;
	struct mbuf *m;
	int nxmit, len;

	/*
	 * the following test is necessary, since
	 * the code is not reentrant and we have
	 * multiple transmission buffers.
	 */
	if (sc->sc_if.if_flags & IFF_OACTIVE)
		return;
	dc = sc->sc_dedata;
	for (nxmit = sc->sc_nxmit; nxmit < NXMT; nxmit++) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

		rp = &dc->dc_xrent[sc->sc_xfree];
		if (rp->r_flags & XFLG_OWN)
			panic("deuna xmit in progress");
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		len = if_ubaput(&sc->sc_ifuba, &sc->sc_ifw[sc->sc_xfree], m);
		rp->r_slen = len;
		rp->r_tdrerr = 0;
		rp->r_flags = XFLG_STP|XFLG_ENP|XFLG_OWN;

		sc->sc_xfree++;
		if (sc->sc_xfree == NXMT)
			sc->sc_xfree = 0;
	}
	if (sc->sc_nxmit != nxmit) {
		sc->sc_nxmit = nxmit;
		if (ifp->if_flags & IFF_RUNNING)
			DE_WLOW(PCSR0_INTE|CMD_PDMD);
	}
}

/*
 * Command done interrupt.
 */
void
deintr(void *arg)
{
	struct ifxmt *ifxp;
	struct de_cdata *dc;
	struct de_softc *sc = arg;
	struct de_ring *rp;
	short csr0;

	/* save flags right away - clear out interrupt bits */
	csr0 = DE_RCSR(DE_PCSR0);
	DE_WHIGH(csr0 >> 8);


	sc->sc_if.if_flags |= IFF_OACTIVE;	/* prevent entering destart */
	/*
	 * if receive, put receive buffer on mbuf
	 * and hang the request again
	 */
	derecv(sc);

	/*
	 * Poll transmit ring and check status.
	 * Be careful about loopback requests.
	 * Then free buffer space and check for
	 * more transmit requests.
	 */
	dc = sc->sc_dedata;
	for ( ; sc->sc_nxmit > 0; sc->sc_nxmit--) {
		rp = &dc->dc_xrent[sc->sc_xindex];
		if (rp->r_flags & XFLG_OWN)
			break;

		sc->sc_if.if_opackets++;
		ifxp = &sc->sc_ifw[sc->sc_xindex];
		/* check for unusual conditions */
		if (rp->r_flags & (XFLG_ERRS|XFLG_MTCH|XFLG_ONE|XFLG_MORE)) {
			if (rp->r_flags & XFLG_ERRS) {
				/* output error */
				sc->sc_if.if_oerrors++;
			} else if (rp->r_flags & XFLG_ONE) {
				/* one collision */
				sc->sc_if.if_collisions++;
			} else if (rp->r_flags & XFLG_MORE) {
				/* more than one collision */
				sc->sc_if.if_collisions += 2;	/* guess */
			}
		}
		if_ubaend(&sc->sc_ifuba, ifxp);
		/* check if next transmit buffer also finished */
		sc->sc_xindex++;
		if (sc->sc_xindex == NXMT)
			sc->sc_xindex = 0;
	}
	sc->sc_if.if_flags &= ~IFF_OACTIVE;
	destart(&sc->sc_if);

	if (csr0 & PCSR0_RCBI) {
		DE_WLOW(PCSR0_INTE|CMD_PDMD);
	}
}

/*
 * Ethernet interface receiver interface.
 * If input error just drop packet.
 * Otherwise purge input buffered data path and examine 
 * packet to determine type.  If can't determine length
 * from type, then have to drop packet.	 Othewise decapsulate
 * packet based on type and pass to type specific higher-level
 * input routine.
 */
void
derecv(struct de_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct de_ring *rp;
	struct de_cdata *dc;
	struct mbuf *m;
	int len;

	dc = sc->sc_dedata;
	rp = &dc->dc_rrent[sc->sc_rindex];
	while ((rp->r_flags & RFLG_OWN) == 0) {
		sc->sc_if.if_ipackets++;
		len = (rp->r_lenerr&RERR_MLEN) - ETHER_CRC_LEN;
		/* check for errors */
		if ((rp->r_flags & (RFLG_ERRS|RFLG_FRAM|RFLG_OFLO|RFLG_CRC)) ||
		    (rp->r_lenerr & (RERR_BUFL|RERR_UBTO))) {
			sc->sc_if.if_ierrors++;
			goto next;
		}
		m = if_ubaget(&sc->sc_ifuba, &sc->sc_ifr[sc->sc_rindex],
		    ifp, len);
		if (m == 0) {
			sc->sc_if.if_ierrors++;
			goto next;
		}
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		(*ifp->if_input)(ifp, m);

		/* hang the receive buffer again */
next:		rp->r_lenerr = 0;
		rp->r_flags = RFLG_OWN;

		/* check next receive buffer */
		sc->sc_rindex++;
		if (sc->sc_rindex == NRCV)
			sc->sc_rindex = 0;
		rp = &dc->dc_rrent[sc->sc_rindex];
	}
}

/*
 * Process an ioctl request.
 */
int
deioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int s, error = 0;

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET)
		error = 0;

	splx(s);
	return (error);
}

/*
 * Await completion of the named function
 * and check for errors.
 */
void
dewait(struct de_softc *sc, char *fn)
{
	int csr0;

	while ((DE_RCSR(DE_PCSR0) & PCSR0_INTR) == 0)
		;
	csr0 = DE_RCSR(DE_PCSR0);
	DE_WHIGH(csr0 >> 8);
	if (csr0 & PCSR0_PCEI) {
		char bits[64];
		printf("%s: %s failed, csr0=%s ", sc->sc_dev.dv_xname, fn,
		    bitmask_snprintf(csr0, PCSR0_BITS, bits, sizeof(bits)));
		printf("csr1=%s\n", bitmask_snprintf(DE_RCSR(DE_PCSR1),
		    PCSR1_BITS, bits, sizeof(bits)));
	}
}

int
dematch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct uba_attach_args *ua = aux;
	struct de_softc ssc;
	struct de_softc *sc = &ssc;
	int i;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	/*
	 * Make sure self-test is finished before we screw with the board.
	 * Self-test on a DELUA can take 15 seconds (argh).
	 */
	for (i = 0;
	    (i < 160) &&
	    (DE_RCSR(DE_PCSR0) & PCSR0_FATI) == 0 &&
	    (DE_RCSR(DE_PCSR1) & PCSR1_STMASK) == STAT_RESET;
	    ++i)
		DELAY(50000);
	if (((DE_RCSR(DE_PCSR0) & PCSR0_FATI) != 0) ||
	    (((DE_RCSR(DE_PCSR1) & PCSR1_STMASK) != STAT_READY) &&
	    ((DE_RCSR(DE_PCSR1) & PCSR1_STMASK) != STAT_RUN)))
		return(0);

	DE_WCSR(DE_PCSR0, 0);
	DELAY(5000);
	DE_WCSR(DE_PCSR0, PCSR0_RSET);
	while ((DE_RCSR(DE_PCSR0) & PCSR0_INTR) == 0)
		;
	/* make board interrupt by executing a GETPCBB command */
	DE_WCSR(DE_PCSR0, PCSR0_INTE);
	DE_WCSR(DE_PCSR2, 0);
	DE_WCSR(DE_PCSR3, 0);
	DE_WCSR(DE_PCSR0, PCSR0_INTE|CMD_GETPCBB);
	DELAY(50000);

	return 1;
}

void
deshutdown(void *arg)
{
	struct de_softc *sc = arg;

	DE_WCSR(DE_PCSR0, 0);
	DELAY(1000);
	DE_WCSR(DE_PCSR0, PCSR0_RSET);
	dewait(sc, "shutdown");
}
