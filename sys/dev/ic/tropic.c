/*	$OpenBSD: tropic.c,v 1.8 2004/05/12 06:35:10 tedu Exp $	*/
/*	$NetBSD: tropic.c,v 1.6 1999/12/17 08:26:31 fvdl Exp $	*/

/* 
 * Ported to NetBSD by Onno van der Linden
 * Many thanks to Larry Lile for sending me the IBM TROPIC documentation.
 *
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <net/if_token.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifndef ifr_mtu
#define	ifr_mtu	ifr_metric
#endif

#include <machine/cpu.h>
#include <machine/bus.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>

static void tr_shutdown(void *);

void	tr_rint(struct tr_softc *);
void	tr_xint(struct tr_softc *);
void	tr_oldxint(struct tr_softc *);
struct	mbuf *tr_get(struct tr_softc *, int, struct ifnet *);
void	tr_opensap(struct tr_softc *, u_char);
int	tr_mbcopy(struct tr_softc *, bus_size_t, struct mbuf *);
void	tr_bcopy(struct tr_softc *, u_char *, int);
void	tr_start(struct ifnet *);
void	tr_oldstart(struct ifnet *);
void	tr_watchdog(struct ifnet *);
int	tr_mediachange(struct ifnet *);
void	tr_mediastatus(struct ifnet *, struct ifmediareq *);
int	tropic_mediachange(struct tr_softc *);
void	tropic_mediastatus(struct tr_softc *, struct ifmediareq *);
void	tr_reinit(void *);

struct cfdriver tr_cd = {
	NULL, "tr", DV_IFNET
};

/*
 * TODO:
 * clean up tr_intr: more subroutines
 * IFF_LINK0 == IFM_TOK_SRCRT change to link flag implies media flag change
 * IFF_LINK1 == IFM_TOK_ALLR  change to link flag implies media flag change
 * XXX Create receive_done queue to kill "ASB not free", but does this ever
 * XXX happen ?
 */

static	int media[] = {
	IFM_TOKEN | IFM_TOK_UTP4,
	IFM_TOKEN | IFM_TOK_STP4,
	IFM_TOKEN | IFM_TOK_UTP16,
	IFM_TOKEN | IFM_TOK_STP16,
	IFM_TOKEN | IFM_TOK_UTP4,
	IFM_TOKEN | IFM_TOK_UTP16,
	IFM_TOKEN | IFM_TOK_STP4,
	IFM_TOKEN | IFM_TOK_STP16
};

int
tropic_mediachange(sc)
	struct tr_softc *sc;
{
	if (IFM_TYPE(sc->sc_media.ifm_media) != IFM_TOKEN)
		return EINVAL;

	switch (IFM_SUBTYPE(sc->sc_media.ifm_media)) {
	case IFM_TOK_STP16:
	case IFM_TOK_UTP16:
		if ((sc->sc_init_status & RSP_16) == 0) {
			tr_stop(sc);
			if (tr_setspeed(sc, 16))
				return EINVAL;
			if (tr_reset(sc))
				return EINVAL;
			if (tr_config(sc))
				return EINVAL;
		}
		break;
	case IFM_TOK_STP4:
	case IFM_TOK_UTP4:
		if ((sc->sc_init_status & RSP_16) != 0) {
			tr_stop(sc);
			if (tr_setspeed(sc, 4))
				return EINVAL;
			if (tr_reset(sc))
				return EINVAL;
			if (tr_config(sc))
				return EINVAL;
		}
		break;
	}
/*
 * XXX Handle Early Token Release !!!!
 */
	return 0;
}

void
tropic_mediastatus(sc, ifmr)
	struct tr_softc *sc;
	struct ifmediareq *ifmr;
{
	struct ifmedia	*ifm = &sc->sc_media;

	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

int
tr_config(sc)
	struct tr_softc *sc;
{
	if (sc->sc_init_status & FAST_PATH_TRANSMIT) {
		int i;

		for (i=0; i < SRB_CFP_CMDSIZE; i++)
			SRB_OUTB(sc, sc->sc_srb, i, 0);

		SRB_OUTB(sc, sc->sc_srb, SRB_CMD, DIR_CONFIG_FAST_PATH_RAM);

		SRB_OUTW(sc, sc->sc_srb, SRB_CFP_RAMSIZE,
		    (16 + (sc->sc_nbuf * FP_BUF_LEN) / 8));
		SRB_OUTW(sc, sc->sc_srb, SRB_CFP_BUFSIZE, FP_BUF_LEN);

		/* tell adapter: command in SRB */
		ACA_SETB(sc, ACA_ISRA_o, CMD_IN_SRB);

		for (i = 0; i < 30000; i++) {
			if (ACA_RDB(sc, ACA_ISRP_o) & SRB_RESP_INT)
				break;
			delay(100);
		}

		if (i == 30000 && sc->sc_srb == ACA_RDW(sc, ACA_WRBR)) {
			printf("No response for fast path cfg\n");
			return 1;
		}

		ACA_RSTB(sc, ACA_ISRP_o, ~(SRB_RESP_INT));


		if ((SRB_INB(sc, sc->sc_srb, SRB_RETCODE) != 0)) {
			printf("cfg fast path returned: %02x\n",
				SRB_INB(sc, sc->sc_srb, SRB_RETCODE));
			return 1;
		}

		sc->sc_txca = SRB_INW(sc, sc->sc_srb, SRB_CFPRESP_FPXMIT);
		sc->sc_srb = SRB_INW(sc, sc->sc_srb, SRB_CFPRESP_SRBADDR);
	}
	else {
		if (sc->sc_init_status & RSP_16)
			sc->sc_maxmtu = sc->sc_dhb16maxsz;
		else
			sc->sc_maxmtu = sc->sc_dhb4maxsz;
/*
 * XXX Not completely true because Fast Path Transmit has 514 byte buffers
 * XXX and TR_MAX_LINK_HDR is only correct when source-routing is used.
 * XXX depending on wether source routing is used change the calculation
 * XXX use IFM_TOK_SRCRT (IFF_LINK0)
 * XXX recompute sc_minbuf !!
 */
		sc->sc_maxmtu -= TR_MAX_LINK_HDR;
	}
	return 0;
}

int
tr_attach(sc)
	struct tr_softc *sc;
{
	int	nmedia, *mediaptr, *defmediaptr;
	int	i, temp;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	if (sc->sc_init_status & FAST_PATH_TRANSMIT) {
		bus_size_t srb;
		int	nbuf = 0;

		srb = sc->sc_srb;

		switch (sc->sc_memsize) {
		case 65536:
			nbuf = 58;
			sc->sc_maxmtu = IPMTU_4MBIT_MAX;
			break;
		case 32768:
			nbuf = 29;
			sc->sc_maxmtu = IPMTU_4MBIT_MAX;
			break;
		case 16384:
			nbuf = 13;
			sc->sc_maxmtu = IPMTU_4MBIT_MAX;
			break;
		case 8192:
			nbuf = 5;
			sc->sc_maxmtu = ISO88025_MTU;
		}

		sc->sc_minbuf = ((sc->sc_maxmtu + 511) / 512) + 1;
		sc->sc_nbuf = nbuf;

/*
 *  Create circular queues caching the buffer pointers ?
 */
	}
	else {
/*
 * MAX_MACFRAME_SIZE = DHB_SIZE - 6
 * IPMTU = MAX_MACFRAME_SIZE - (14 + 18 + 8)
 * (14 = header, 18 = sroute, 8 = llcsnap)
 */

		switch (sc->sc_memsize) {
		case 8192:
			sc->sc_dhb4maxsz = 2048;
			sc->sc_dhb16maxsz = 2048;
			break;
		case 16384:
			sc->sc_dhb4maxsz = 4096;
			sc->sc_dhb16maxsz = 4096;
			break;
		case 32768:
			sc->sc_dhb4maxsz = 4464;
			sc->sc_dhb16maxsz = 8192;
			break;
		case 65536:
			sc->sc_dhb4maxsz = 4464;
			sc->sc_dhb16maxsz = 8192;
			break;
		}
		switch (MM_INB(sc, TR_DHB4_OFFSET)) {
		case 0xF:
			if (sc->sc_dhb4maxsz > 2048)
				sc->sc_dhb4maxsz = 2048;
			break;
		case 0xE:
			if (sc->sc_dhb4maxsz > 4096)
				sc->sc_dhb4maxsz = 4096;
			break;
		case 0xD:
			if (sc->sc_dhb4maxsz > 4464)
				sc->sc_dhb4maxsz = 4464;
			break;
		}

		switch (MM_INB(sc, TR_DHB16_OFFSET)) {
		case 0xF:
			if (sc->sc_dhb16maxsz > 2048)
				sc->sc_dhb16maxsz = 2048;
			break;
		case 0xE:
			if (sc->sc_dhb16maxsz > 4096)
				sc->sc_dhb16maxsz = 4096;
			break;
		case 0xD:
			if (sc->sc_dhb16maxsz > 8192)
				sc->sc_dhb16maxsz = 8192;
			break;
		case 0xC:
			if (sc->sc_dhb16maxsz > 8192)
				sc->sc_dhb16maxsz = 8192;
			break;
		case 0xB:
			if (sc->sc_dhb16maxsz > 8192)
				sc->sc_dhb16maxsz = 8192;
			break;
		}
	}

	if (tr_config(sc))
		return 1;

	/*
	 * init network-visible interface 
	 */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_ioctl = tr_ioctl;
	if (sc->sc_init_status & FAST_PATH_TRANSMIT)
		ifp->if_start = tr_start;
	else
		ifp->if_start = tr_oldstart;
	ifp->if_flags = IFF_BROADCAST | IFF_NOTRAILERS;
	ifp->if_watchdog = tr_watchdog;
	IFQ_SET_READY(&ifp->if_snd);

	switch (MM_INB(sc, TR_MEDIAS_OFFSET)) {
	case 0xF:
		nmedia = 1;
		mediaptr = &media[6];
		break;
	case 0xE:
		nmedia = 2;
		mediaptr = &media[0];
		break;
	case 0xD:
		nmedia = 1;
		mediaptr = &media[4];
		break;
	default:
		nmedia = 0;
		mediaptr = NULL;
	}

	switch (MM_INB(sc, TR_RATES_OFFSET)) {
	case 0xF:
		/* 4 Mbps */
		break;
	case 0xE:
		/* 16 Mbps */
		if (mediaptr)
			mediaptr += nmedia;
		break;
	case 0xD:
		/* 4/16 Mbps */
		nmedia *= 2;
		break;
	}

	switch (MM_INB(sc, TR_MEDIA_OFFSET)) {
	case 0xF:
		/* STP */
		defmediaptr = &media[6];
		break;
	case 0xE:
		/* UTP */
		defmediaptr = &media[4];
		break;
	case 0xD:
		/* STP and UTP == a single shielded RJ45 which supports both */
		/* XXX additional types in net/if_media.h ?? */
		defmediaptr = &media[4];
		break;
	default:
		defmediaptr = NULL;
	}

	if (defmediaptr && (sc->sc_init_status & RSP_16))
		++defmediaptr;

	if (sc->sc_mediachange == NULL && sc->sc_mediastatus == NULL) {
		switch (MM_INB(sc, TR_TYP_OFFSET)) {
		case 0x0D:
		case 0x0C:
			sc->sc_mediachange = tropic_mediachange;
			sc->sc_mediastatus = tropic_mediastatus;
		}
	}

	ifmedia_init(&sc->sc_media, 0, tr_mediachange, tr_mediastatus);
	if (mediaptr != NULL) {
		for (i = 0; i < nmedia; i++)
			ifmedia_add(&sc->sc_media, mediaptr[i], 0, NULL);
		if (defmediaptr)
			ifmedia_set(&sc->sc_media, *defmediaptr);
		else
			ifmedia_set(&sc->sc_media, 0);
	}
	else {
		ifmedia_add(&sc->sc_media, IFM_TOKEN | IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_TOKEN | IFM_MANUAL);
	}

	if_attach(ifp);

	for (i = 0, temp = 0; i < ISO88025_ADDR_LEN; i++, temp += 4) {
		sc->sc_arpcom.ac_enaddr[i] =
		    (MM_INB(sc, (TR_MAC_OFFSET + temp)) & 0xf) << 4;
		sc->sc_arpcom.ac_enaddr[i] |=
		    MM_INB(sc, (TR_MAC_OFFSET + temp + 2)) & 0xf;
	}

	token_ifattach(ifp);

	printf("\n%s: address %s ring speed %d Mbps\n",
		sc->sc_dev.dv_xname, token_sprintf(sc->sc_arpcom.ac_enaddr),
		(sc->sc_init_status & RSP_16) ? 16 : 4);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_IEEE802, sizeof(struct token_header));
#endif

/*
 * XXX rnd stuff
 */
	shutdownhook_establish(tr_shutdown, sc);
	return 0;
}

int
tr_setspeed(sc, speed)
struct tr_softc *sc;
u_int8_t speed;
{
	SRB_OUTB(sc, sc->sc_srb, SRB_CMD, DIR_SET_DEFAULT_RING_SPEED);
	SRB_OUTB(sc, sc->sc_srb, CMD_RETCODE, 0xfe);
	SRB_OUTB(sc, sc->sc_srb, SRB_SET_DEFRSP, speed);
	/* Tell adapter: command in SRB. */
	ACA_SETB(sc, ACA_ISRA_o, CMD_IN_SRB);

	/* Wait for it to complete. */
	tr_sleep(sc);

	if ((SRB_INB(sc, sc->sc_srb, SRB_RETCODE) != 0)) {
		printf("set default ringspeed returned: %02x\n",
			SRB_INB(sc, sc->sc_srb, SRB_RETCODE));
		return 1;
	}
	return 0;
}

int
tr_mediachange(ifp)
	struct ifnet *ifp;
{
	struct tr_softc *sc = ifp->if_softc;

	if (sc->sc_mediachange)
		return ((*sc->sc_mediachange)(sc));
	return EINVAL;
}

void
tr_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct tr_softc *sc = ifp->if_softc;

/* set LINK0 and/or LINK1 */
	if (sc->sc_mediastatus)
		(*sc->sc_mediastatus)(sc, ifmr);
}

int
tr_reset(sc)
struct tr_softc *sc;
{
	int i;

	sc->sc_srb = 0;

	/* 
	 * Reset the card.
	 */
	/* latch on an unconditional adapter reset */
	bus_space_write_1(sc->sc_piot, sc->sc_pioh, TR_RESET, 0);
	delay(50000); /* delay 50ms */
	/*
	 * XXX set paging if we have the right type of card
	 */
	/* turn off adapter reset */
	bus_space_write_1(sc->sc_piot, sc->sc_pioh, TR_RELEASE, 0);

	/* Enable interrupts. */

	ACA_SETB(sc, ACA_ISRP_e, INT_ENABLE);

	/* Wait for an answer from the adapter. */

	for (i = 0; i < 35000; i++) {
		if (ACA_RDB(sc, ACA_ISRP_o) & SRB_RESP_INT)
			break;
		delay(100);
	}

	if (i == 35000 && sc->sc_srb == 0) {
		printf("No response from adapter after reset\n");
		return 1;
	}

	ACA_RSTB(sc, ACA_ISRP_o, ~(SRB_RESP_INT));

	ACA_OUTB(sc, ACA_RRR_e, (sc->sc_maddr >> 12));
	sc->sc_srb = ACA_RDW(sc, ACA_WRBR);
	if (SRB_INB(sc, sc->sc_srb, SRB_CMD) != 0x80) {
		printf("Initialization incomplete, status: %02x\n",
			SRB_INB(sc, sc->sc_srb, SRB_CMD));
		return 1;
	}
	if (SRB_INB(sc, sc->sc_srb, SRB_INIT_BUC) != 0) {
		printf("Bring Up Code %02x\n",
		    SRB_INB(sc, sc->sc_srb, SRB_INIT_BUC));
		return 1;
	}

	sc->sc_init_status = SRB_INB(sc, sc->sc_srb, SRB_INIT_STATUS);

	sc->sc_xmit_head = sc->sc_xmit_tail = 0;

	/* XXX should depend on sc_resvdmem. */
	if (MM_INB(sc, TR_RAM_OFFSET) == 0xB && sc->sc_memsize == 65536)
		for (i = 0; i < 512; i++)
			SR_OUTB(sc, 0xfe00 + i, 0);
	return 0;
}

/*
 * tr_stop - stop interface (issue a DIR CLOSE ADAPTER command)
 */
void
tr_stop(sc)
struct tr_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	if ((ifp->if_flags & IFF_RUNNING) != 0) {
/*
 * transmitter cannot be used from now on
 */
		ifp->if_flags |= IFF_OACTIVE;

		/* Close command. */
		SRB_OUTB(sc, sc->sc_srb, SRB_CMD, DIR_CLOSE);
		/* Tell adapter: command in SRB. */
		ACA_SETB(sc, ACA_ISRA_o, CMD_IN_SRB);

		/* Wait for it to complete. */
		tr_sleep(sc);
		sc->sc_srb = ACA_RDW(sc, ACA_WRBR);
	}
}

static void
tr_shutdown(arg)
	void *arg;
{
	struct tr_softc *sc = arg;

	tr_stop(sc);
}

void
tr_reinit(arg)
	void *arg;
{
	if (tr_reset((struct tr_softc *) arg))
		return;
	if (tr_config((struct tr_softc *) arg))
		return;
	tr_init(arg);
}

/*
 *  tr_init - initialize network interface, open adapter for packet
 *	     reception and start any pending output
 */
void
tr_init(arg)
	void *arg;
{
	struct tr_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_size_t open_srb;
	int s, num_dhb;
	int	resvdmem, availmem, dhbsize;

	if ((ifp->if_flags & IFF_RUNNING) != 0)
		return;

	s = splimp();

	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_xmit_head = sc->sc_xmit_tail = 0; /* XXX tr_reset() */

	open_srb = sc->sc_srb;

	/* Zero SRB. */
	bus_space_set_region_1(sc->sc_memt, sc->sc_sramh,
	    open_srb, 0, SRB_OPEN_CMDSIZE);

	/* Open command. */
	SRB_OUTB(sc, open_srb, SRB_CMD, DIR_OPEN_ADAPTER);
/*
 * XXX handle IFM_TOK_ETR !!!!
 */
	/* Set open parameters in SRB. */
	SRB_OUTW(sc, open_srb, SRB_OPEN_OPTIONS, OPEN_PASS_BCON_MAC);

	num_dhb = 1;

	if ((sc->sc_init_status & FAST_PATH_TRANSMIT) == 0) {
		availmem = sc->sc_memsize;
		resvdmem = RESVDMEM_SIZE + sc->sc_memreserved;

		/* allow MAX of two SAPS */
		SRB_OUTB(sc, open_srb, SRB_OPEN_DLCMAXSAP, 2);
		resvdmem += 2 * SAPCB_SIZE;

		/* allow MAX of 4 stations */
		SRB_OUTB(sc, open_srb, SRB_OPEN_DLCMAXSTA, 4);
		resvdmem += 4 * LSCB_SIZE;

		if (sc->sc_init_status & RSP_16) {
			dhbsize = sc->sc_dhb16maxsz;
		}
		else {
			dhbsize = sc->sc_dhb4maxsz;
		}
#if 0	/* XXXchb unneeded? */
		if (dhbsize > 2048)
			num_dhb = 2;
#endif
		SRB_OUTW(sc, open_srb, SRB_OPEN_DHBLEN, dhbsize);
		sc->sc_nbuf = (dhbsize + 511) / 512;
		/*
		 * Try to leave room for two fullsized packets when
		 * requesting DHBs.
		 */
		availmem -= resvdmem;
		num_dhb = (availmem / dhbsize) - 2;
		if (num_dhb > 2)
			num_dhb = 2;	/* firmware can't cope with more DHBs */
		if (num_dhb < 1)
			num_dhb = 1;	/* we need at least one */
	}
	else
		SRB_OUTW(sc, open_srb, SRB_OPEN_DHBLEN, DHB_LENGTH);

	SRB_OUTB(sc, open_srb, SRB_OPEN_NUMDHB, num_dhb);
	SRB_OUTW(sc, open_srb, SRB_OPEN_RCVBUFLEN, RCV_BUF_LEN);
	SRB_OUTW(sc, open_srb, SRB_OPEN_NUMRCVBUF, sc->sc_nbuf);

	/* Tell adapter: command in SRB. */
	ACA_SETB(sc, ACA_ISRA_o, CMD_IN_SRB);

	splx(s);
}

/*
 *  tr_oldstart - Present transmit request to adapter
 */
void
tr_oldstart(ifp)
struct ifnet *ifp;
{
	struct tr_softc *sc = ifp->if_softc;
	bus_size_t srb = sc->sc_srb;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	ifp->if_flags |= IFF_OACTIVE;

	/* Load SRB to request transmit. */
	SRB_OUTB(sc, srb, SRB_CMD, XMIT_UI_FRM);
	SRB_OUTW(sc, srb, XMIT_STATIONID, sc->exsap_station);
	ACA_SETB(sc, ACA_ISRA_o, CMD_IN_SRB);
}

void
tr_start(ifp)
struct ifnet *ifp;
{
	struct tr_softc *sc = ifp->if_softc;
	bus_size_t first_txbuf, txbuf;
	struct mbuf	*m0, *m;
	int	size, bufspace;
	bus_size_t framedata;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;


next:
	if (sc->sc_xmit_buffers < sc->sc_minbuf)
		return;

	/* if data in queue, copy mbuf chain to fast path buffers */
	IFQ_DEQUEUE(&ifp->if_snd, m0);

	if (m0 == 0)
		return;
#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif
	first_txbuf = txbuf = TXCA_INW(sc, TXCA_FREE_QUEUE_HEAD) - XMIT_NEXTBUF;
	framedata = txbuf + XMIT_FP_DATA;
	size = 0;
	bufspace = FP_BUF_LEN - XMIT_FP_DATA;
	--sc->sc_xmit_buffers;
	for (m = m0; m; m = m->m_next) {
		int len = m->m_len;
		char *ptr = mtod(m, char *);

		while (len >= bufspace) {
			--sc->sc_xmit_buffers;
			bus_space_write_region_1(sc->sc_memt, sc->sc_sramh,
			    framedata, ptr, bufspace);
			size += bufspace;
			ptr += bufspace;
			len -= bufspace;
			TXB_OUTW(sc, txbuf, XMIT_BUFLEN,
			    (FP_BUF_LEN - XMIT_FP_DATA));
			txbuf = TXB_INW(sc, txbuf, XMIT_NEXTBUF) - XMIT_NEXTBUF;
			framedata =  txbuf + XMIT_FP_DATA;
			bufspace = FP_BUF_LEN - XMIT_FP_DATA;
		}
		if (len > 0) {
			bus_space_write_region_1(sc->sc_memt, sc->sc_sramh,
			    framedata, ptr, len);
			size += len;
			bufspace -= len;
			framedata += len;
		}
	}
	TXB_OUTW(sc, txbuf, XMIT_BUFLEN, (FP_BUF_LEN - XMIT_FP_DATA - bufspace));
	m_freem(m0);		/* free mbuf chain */

	TXB_OUTB(sc, first_txbuf, XMIT_RETCODE, 0xfe);
	TXB_OUTW(sc, first_txbuf, XMIT_FRAMELEN, size);
	TXB_OUTW(sc, first_txbuf, XMIT_LASTBUF, (txbuf + XMIT_NEXTBUF));
	TXB_OUTB(sc, first_txbuf, XMIT_CMD, XMIT_DIR_FRAME);
	TXB_OUTW(sc, first_txbuf, XMIT_STATIONID, 0);
	TXB_OUTB(sc, first_txbuf, XMIT_CMDCORR, sc->sc_xmit_correlator);
	sc->sc_xmit_correlator = (sc->sc_xmit_correlator + 1) & 0x7f;

	/*
	 * To prevent race conditions on 8-bit cards when reading or writing
	 * 16-bit values. See page 4-12 of the IBM manual.
	 */
	TXCA_OUTW(sc, TXCA_FREE_QUEUE_HEAD, 1);
	TXCA_OUTW(sc, TXCA_FREE_QUEUE_HEAD, TXB_INW(sc, txbuf, XMIT_NEXTBUF));

	ACA_SETB(sc, ACA_ISRA_o, XMIT_REQ);

	ifp->if_flags |= IFF_OACTIVE;
	ifp->if_opackets++;
#if 1
/* XXX do while construction */
	goto next;
#endif
}

/*
 *  tr_intr - interrupt handler.  Find the cause of the interrupt and
 *  service it.
 */
int
tr_intr(arg)
	void *arg;
{
	struct tr_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_char status;	/* holds status from adapter status register */
	u_char command;	/* holds command from status or request block */
	u_char retcode;	/* holds return value from status or request block */
	int rc = 0;	/* 0 = unclaimed interrupt, 1 = interrupt claimed */

	status = ACA_RDB(sc, ACA_ISRP_o);
	while (status != 0) {

		/* Is this interrupt caused by an adapter check? */
		if (status & ADAP_CHK_INT) {
			printf("%s: adapter check 0x%04x\n",
			    sc->sc_dev.dv_xname,
			    (unsigned int)ntohs(ACA_RDW(sc, ACA_WWCR)));

			/* Clear this interrupt bit */
			ACA_RSTB(sc, ACA_ISRP_o, ~(ADAP_CHK_INT));

			rc = 1;		/* Claim interrupt. */
			break;		/* Terminate loop. */
		}
		else if (status & XMIT_COMPLETE) {
			ACA_RSTB(sc, ACA_ISRP_o, ~(XMIT_COMPLETE));
			tr_xint(sc);
			rc = 1;
		}

		/*
		 * Process SRB_RESP_INT, ASB_FREE_INT, ARB_CMD_INT
		 * & SSB_RESP_INT in that order, ISRP-L Hi to Lo
		 */
		else if (status & SRB_RESP_INT) { /* Adapter response in SRB? */
			bus_size_t sap_srb;
			bus_size_t srb;
#ifdef TROPICDEBUG
			bus_size_t log_srb;
#endif
			if (sc->sc_srb == 0)
				sc->sc_srb = ACA_RDW(sc, ACA_WRBR);
			srb = sc->sc_srb; /* pointer to SRB */
			retcode = SRB_INB(sc, srb, SRB_RETCODE);
			command = SRB_INB(sc, srb, SRB_CMD);
			switch (command) {
			case 0x80: /* 0x80 == initialization complete */
			case DIR_CONFIG_FAST_PATH_RAM:
				break;
			case XMIT_DIR_FRAME:	/* Response to xmit request */
			case XMIT_UI_FRM:	/* Response to xmit request */
				/* Response not valid? */
				if (retcode != 0xff)
					printf("%s: error on xmit request =%x\n",
					    sc->sc_dev.dv_xname, retcode);
				break;

			case DIR_OPEN_ADAPTER:	/* open-adapter-cmd response */
				/* Open successful? */
				if (retcode == 0) {
					ifp->if_flags |= IFF_UP | IFF_RUNNING;
					/* Save new ACA ctrl block addresses */
					sc->sc_ssb = SRB_INW(sc, srb,
					    SRB_OPENRESP_SSBADDR);
					sc->sc_arb = SRB_INW(sc, srb,
					    SRB_OPENRESP_ARBADDR);
					sc->sc_srb = SRB_INW(sc, srb,
					    SRB_OPENRESP_SRBADDR);
					sc->sc_asb = SRB_INW(sc, srb,
					    SRB_OPENRESP_ASBADDR);

					/*
					 * XXX, what about LLC_{X25,ISO}_LSAP ?
					 * open two more saps .....
					 */
					if (sc->sc_init_status &
					    FAST_PATH_TRANSMIT) {
						sc->sc_xmit_buffers =
						    TXCA_INW(sc, TXCA_BUFFER_COUNT);
						sc->sc_nbuf =
						    sc->sc_xmit_buffers;
#ifdef TROPICDEBUG
						printf("buffers = %d\n",
						    sc->sc_xmit_buffers);
#endif
						sc->sc_xmit_correlator = 0;
						wakeup(&sc->tr_sleepevent);
					}
					else
						tr_opensap(sc, LLC_SNAP_LSAP); 
				}
				else {
					printf("%s: Open error = %x\n",
					    sc->sc_dev.dv_xname,
					    SRB_INB(sc, srb, SRB_RETCODE));
					ifp->if_flags &= ~IFF_RUNNING;
					ifp->if_flags &= ~IFF_UP;
/*
 * XXX untimeout depending on the error, timeout in other cases
 * XXX error 0x24 && autospeed mode: open again !!!!
 */
					if (!timeout_initialized(&sc->init_timeout))
						timeout_set(&sc->init_timeout,
						    tr_init, sc);
					timeout_add(&sc->init_timeout, hz * 30);
				}
				break;

			case DIR_CLOSE:	/* Response to close adapter command */
				/* Close not successful? */
				if (retcode != 0)
					printf("%s: close error = %x\n",
					    sc->sc_dev.dv_xname, retcode);
				else {
					ifp->if_flags &= ~IFF_RUNNING;
					ifp->if_flags &= ~IFF_UP;
					ifp->if_flags &= ~IFF_OACTIVE;
					wakeup(&sc->tr_sleepevent);
				}
				break;
			case DIR_SET_DEFAULT_RING_SPEED:
				wakeup(&sc->tr_sleepevent);
				break;

			case DLC_OPEN_SAP:     	/* Response to open sap cmd */
				sap_srb = sc->sc_srb;
				if (SRB_INB(sc, sap_srb, SRB_OPNSAP_SAPVALUE)
				    == LLC_SNAP_LSAP)
					sc->exsap_station =
					    SRB_INW(sc, sap_srb,
					        SRB_OPNSAP_STATIONID);
				printf("%s: Token Ring opened\n",
				    sc->sc_dev.dv_xname);
				wakeup(&sc->tr_sleepevent);
				break;
/* XXX DLC_CLOSE_SAP not needed ? */
			case DLC_CLOSE_SAP: /* Response to close sap cmd */
				break;
			case DIR_READ_LOG:   /* Response to read log */
				/* Cmd not successful? */
				if (retcode != 0)
					printf("%s: read error log cmd err =%x\n",
					    sc->sc_dev.dv_xname, retcode);
#ifdef TROPICDEBUG
				log_srb = sc->sc_srb;
				printf("%s: ERROR LOG:\n",sc->sc_dev.dv_xname);
				printf("%s: Line=%d, Internal=%d, Burst=%d\n",
				    sc->sc_dev.dv_xname,
				    (SRB_INB(sc, log_srb, SRB_LOG_LINEERRS)),
				    (SRB_INB(sc, log_srb, SRB_LOG_INTERRS)),
				    (SRB_INB(sc, log_srb, SRB_LOG_BRSTERRS)));
				printf("%s: A/C=%d, Abort=%d, Lost frames=%d\n",
				    sc->sc_dev.dv_xname,
				    (SRB_INB(sc, log_srb, SRB_LOG_ACERRS)),
				    (SRB_INB(sc, log_srb, SRB_LOG_ABRTERRS)),
				    (SRB_INB(sc, log_srb, SRB_LOG_LOSTFRMS)));
				printf("%s: Receive congestion=%d, Frame copied=%d, Frequency=%d\n",
				    sc->sc_dev.dv_xname,
				    (SRB_INB(sc, log_srb, SRB_LOG_RCVCONG)),
				    (SRB_INB(sc, log_srb, SRB_LOG_FCPYERRS)),
				    (SRB_INB(sc, log_srb, SRB_LOG_FREQERRS)));
				printf("%s: Token=%d\n",sc->sc_dev.dv_xname,
				    (SRB_INB(sc, log_srb, SRB_LOG_TOKENERRS)));
#endif /* TROPICDEBUG */
				ifp->if_flags &= ~IFF_OACTIVE;
				break;
			default:
				printf("%s: bad SRB command encountered %x\n",
				    sc->sc_dev.dv_xname, command);
				break;
			}
			/* clear the SRB-response interrupt bit */
			ACA_RSTB(sc, ACA_ISRP_o, ~(SRB_RESP_INT));

		}

		else if (status & ASB_FREE_INT) { /* Is ASB Free? */
			bus_size_t asb = sc->sc_asb;

			/*
			 * Remove message from asb queue, first element in
			 * structure is the command. command == REC_DATA?
			 * size = 8 : size = 10
			 * reply in isra_l with (RESP_IN_ASB | ASB_FREE)
			 */
			retcode = ASB_INB(sc, asb, CMD_RETCODE);
			command = ASB_INB(sc, asb, CMD_CMD);
			switch (command) {
			case REC_DATA:		/* Receive */
				/* Response not valid? */
				if (retcode != 0xff)
				printf("%s: ASB bad receive response =%x\n",
				    sc->sc_dev.dv_xname, retcode);
				break;
			case XMIT_DIR_FRAME:	/* Transmit */
			case XMIT_UI_FRM:   	/* Transmit */
				/* Response not valid? */
				if (retcode != 0xff)
				printf("%s: ASB response err on xmit =%x\n",
				    sc->sc_dev.dv_xname, retcode);
				break;
			default:
				printf("%s: Invalid command in ASB =%x\n",
				    sc->sc_dev.dv_xname, command);
				break;
			}
			/* Clear this interrupt bit */
			ACA_RSTB(sc, ACA_ISRP_o, ~(ASB_FREE_INT));
		}
		else if (status & ARB_CMD_INT) { /* Command for PC to handle? */
			bus_size_t arb = sc->sc_arb;

			command = ARB_INB(sc, arb, ARB_CMD);
			switch (command) {
			case DLC_STATUS:    /* DLC status change */	
				printf("%s: ARB new DLC  status = 0x%x\n",
				    sc->sc_dev.dv_xname,
				    ARB_INW(sc, arb, ARB_DLCSTAT_STATUS));
				break;
			case REC_DATA:		/* Adapter has data for PC */
				/* Call receive interrupt handler */
				tr_rint(sc);
				break;

			case RING_STAT_CHANGE:	/* Ring status change */
				if (ARB_INW(sc, arb, ARB_RINGSTATUS) &
				    (SIGNAL_LOSS + LOBE_FAULT)){
					printf("%s: SIGNAL LOSS/LOBE FAULT\n",
					    sc->sc_dev.dv_xname);
					ifp->if_flags &= ~IFF_RUNNING;
					ifp->if_flags &= ~IFF_UP;
					IFQ_PURGE(&ifp->if_snd);
					if (!timeout_initialized(&sc->reinit_timeout))
						timeout_set(&sc->reinit_timeout,
						    tr_reinit, sc);
					timeout_add(&sc->reinit_timeout, hz * 30);
				}
				else {
#ifdef TROPICDEBUG
					if (ARB_INW(sc, arb, ARB_RINGSTATUS) &
					    ~(SOFT_ERR))
						printf(
					"%s: ARB new ring status = 0x%x\n",
						    sc->sc_dev.dv_xname,
						    ARB_INW(sc, arb,
							ARB_RINGSTATUS));
#endif /* TROPICDEBUG */
				}
				if (ARB_INW(sc, arb, ARB_RINGSTATUS) &
				    LOG_OFLOW){
/*
 * XXX CMD_IN_SRB, handle with SRB_FREE_INT ?
 */
					ifp->if_flags |= IFF_OACTIVE;
					SRB_OUTB(sc, sc->sc_srb, SRB_CMD,
					    DIR_READ_LOG);
					/* Read & reset err log cmnd in SRB. */
					ACA_SETB(sc, ACA_ISRA_o, CMD_IN_SRB);
				}
				break;

			case XMIT_DATA_REQ: /* Adapter wants data to transmit */
				/* Call transmit interrupt handler */
				tr_oldxint(sc);
				break;

			default:
				printf("%s: Invalid command in ARB =%x\n",
				    sc->sc_dev.dv_xname, command);
				break;
			}

			/* Clear this interrupt bit */
			ACA_RSTB(sc, ACA_ISRP_o, ~(ARB_CMD_INT)); 

			/* Tell adapter that ARB is now free */
			ACA_SETB(sc, ACA_ISRA_o, ARB_FREE);
		}


		else if (status & SSB_RESP_INT) {  /* SSB resp. to SRB cmd? */
			bus_size_t	ssb = sc->sc_ssb;

			retcode = SSB_INB(sc, ssb, SSB_RETCODE);
			command = SSB_INB(sc, ssb, SSB_CMD);
			switch (command) {
			case XMIT_UI_FRM:
			case XMIT_DIR_FRAME:  /* SSB response to SRB xmit cmd */
				/* collect status on last packet */
				if (retcode != 0) {
					printf("xmit return code = 0x%x\n",
					    retcode);
					/* XXXchb */
					if (retcode == 0x22) {
						printf("FS = 0x%2x\n",
						    SSB_INB(sc, ssb,
						        SSB_XMITERR));
					}
					ifp->if_oerrors++;
				}
				else
					ifp->if_opackets++;

				ifp->if_flags &= ~IFF_OACTIVE;
/*
 * XXX should this be done here ?
 */
				/* if data on send queue */
				if (!IFQ_IS_EMPTY(&ifp->if_snd))
					tr_oldstart(ifp);
				break;

			case XMIT_XID_CMD:
				printf("tr_int: xmit XID return code = 0x%x\n",
				    retcode);
				break;
			default:
				printf("%s: SSB error, invalid command =%x\n",
				    sc->sc_dev.dv_xname, command);
			}
			/* clear this interrupt bit */
			ACA_RSTB(sc, ACA_ISRP_o, ~(SSB_RESP_INT));

			/* tell adapter that SSB is available */
			ACA_SETB(sc, ACA_ISRA_o, SSB_FREE);
		}
		rc = 1;		/* Claim responsibility for interrupt */
		status = ACA_RDB(sc, ACA_ISRP_o);
	}
	/* Is this interrupt caused by an adapter error or access violation? */
	if (ACA_RDB(sc, ACA_ISRP_e) & (TCR_INT | ERR_INT | ACCESS_INT)) {
		printf("%s: adapter error, ISRP_e = %x\n",
		    sc->sc_dev.dv_xname, ACA_RDB(sc, ACA_ISRP_e));

		/* Clear these interrupt bits */
		ACA_RSTB(sc, ACA_ISRP_e, ~(TCR_INT | ERR_INT | ACCESS_INT));
		rc = 1;		/* Claim responsibility for interrupt */

	}

	/* Clear IRQ latch in order to reenable interrupts. */
	bus_space_write_1(sc->sc_piot, sc->sc_pioh, TR_CLEARINT, 0);
	return (rc);
}

#ifdef notyet
int asb_reply_rcv()
{
}

int asb_reply_xmit()
{
}

int asb_response(bus_size_t asb, size_t len)
{
	if (empty_queue) {
		answer with RESP_IN_ASB | ASB_FREE
	}
	else {
		put asb in queue
	}
}
#endif


/*
 *  U-B receive interrupt.
 *
 * in the original version, this routine had three tasks:
 *
 *	1. move the data into the receive buffer and set up various pointers
 *	   in the tr_softc struct
 *	2. switch on the type field for ip and arp, dropping all else
 *	3. resetting the adaptor status block info (asb) and updating the
 *	   tr_softc struct
 *		determine lan message type, pull packet off interface and
 *		pass to an appropriate higher-level routine
 *
 */
void
tr_rint(sc)
struct tr_softc *sc;
{
	bus_size_t arb = sc->sc_arb;
	bus_size_t asb = sc->sc_asb;
	struct rbcb *rbc = &sc->rbc;
	struct mbuf *m;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

#ifdef TROPICDEBUG
	printf("tr_rint: arb.command = %x, arb.station_id= %x\n",
	    ARB_INB(sc, arb, ARB_CMD), ARB_INW(sc, arb, ARB_STATIONID));
	printf("arb.buf_addr = %x, arb.lan_hdr_len = %x\n",
	    ARB_INW(sc, arb, ARB_RXD_BUFADDR),
	    ARB_INB(sc, arb, ARB_RXD_LANHDRLEN));
	printf("arb.dlc_hdr_len = %d, arb.frame_len = %d\n",
	    ARB_INB(sc, arb, ARB_RXD_DLCHDRLEN),
	    ARB_INW(sc, arb, ARB_RXD_FRAMELEN));
	printf("arb.msg_type = %x\n", ARB_INB(sc, arb, ARB_RXD_MSGTYPE));
#endif /* TROPICDEBUG */
	/*
	 * copy the offset in RAM of the first receive buffer from the
	 * receive-data block of the adapter request block associated
	 * with the unit's softc struct into the receive control block.
	 */
	rbc->rbufp = ARB_INW(sc, arb, ARB_RXD_BUFADDR);

	/*
	 * copy the pointer to data in first receive buffer
	 */
	rbc->rbuf_datap = rbc->rbufp + RB_DATA;
	/*
	 * the token-ring header is viewed as two header structs: the physical
	 * header (aka TR header) with access, frame, dest, src, and routing
	 * information, and the logical link control header (aka LLC header)
	 * with dsap, ssap, llc, proto and type fields.
	 *
	 * rfc1042 requires support for unnumbered information (UI) commands,
	 * but does not specify a required semantic, so we'll discard them.
	 *
	 */

	/*
	 * if there is a second receive buffer, set up the next pointer
	 */
	if (RB_INW(sc, rbc->rbufp, RB_NEXTBUF))
		rbc->rbufp_next = RB_INW(sc, rbc->rbufp, RB_NEXTBUF) -
		    RB_NEXTBUF;
	else
		rbc->rbufp_next = 0;	/* we're finished */

	rbc->data_len = RB_INW(sc, rbc->rbufp, RB_BUFLEN);
	/*
	 * At this point we move the packet from the adapter to a chain
	 * of mbufs
	 */
	m = tr_get(sc, ARB_INW(sc, arb, ARB_RXD_FRAMELEN), ifp);
/*
 * XXX Clear ARB interrupt here?
 */
/*
 * XXX create a queue where the responses are buffered
 * XXX but is it really needed ?
 */

	if (ASB_INB(sc, asb, RECV_RETCODE) != 0xff)
		printf("tr_rint: ASB IS NOT FREE!!!\n");
	/*
	 * Load receive response into ASB.
	 */
	ASB_OUTB(sc, asb, RECV_CMD, REC_DATA);
	ASB_OUTW(sc, asb, RECV_STATIONID, ARB_INW(sc, arb, ARB_STATIONID));
	ASB_OUTW(sc, asb, RECV_RESP_RECBUFADDR,
	    ARB_INW(sc, arb, ARB_RXD_BUFADDR));

	if (m == 0) {
		/*
		 * Tell adapter data lost, no mbufs.
		 */
		ASB_OUTB(sc, asb, RECV_RETCODE, 0x20);
		ACA_SETB(sc, ACA_ISRA_o, RESP_IN_ASB);
		++ifp->if_ierrors;
#ifdef TROPICDEBUG
		printf("tr_rint: packet dropped\n");
#endif /* TROPICDEBUG */
	}
	else {
		/*
		 * Indicate successful receive.
		 */
		ASB_OUTB(sc, asb, RECV_RETCODE, 0);
		ACA_SETB(sc, ACA_ISRA_o, RESP_IN_ASB);
		++ifp->if_ipackets;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		token_input(ifp, m);
	}
}

/*
 *  Interrupt handler for old style "adapter requires data to transmit".
 */
void
tr_oldxint(sc)
struct tr_softc *sc;
{
	bus_size_t arb = sc->sc_arb;	/* pointer to ARB */
	bus_size_t asb = sc->sc_asb;	/* pointer to ASB */
	bus_size_t dhb;			/* pointer to DHB */
	struct mbuf *m0;		/* pointer to top of mbuf chain */
	u_short size = 0;
	char	command;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct token_header *trh;
	int i;
	u_int8_t hlen;

/*
 * XXX xmit_asb_response()
 */
	if (ASB_INB(sc, asb, XMIT_RETCODE) != 0xff)
		printf("tr_oldxint: ASB IS NOT FREE!!!\n");

	/* load parameters into ASB */
	ASB_OUTB(sc, asb, XMIT_CMDCORR, ARB_INB(sc, arb, ARB_XMT_CMDCORR));
	ASB_OUTW(sc, asb, XMIT_STATIONID, ARB_INW(sc, arb, ARB_STATIONID));
	ASB_OUTB(sc, asb, XMIT_RETCODE, 0);
/*
 * XXX LLC_{X25,ISO}_LSAP
 */
	ASB_OUTB(sc, asb, XMIT_REMSAP, LLC_SNAP_LSAP);

	/* XXX if num_dhb == 2 this should alternate between the two buffers */
	dhb = ARB_INW(sc, arb, ARB_XMT_DHBADDR);

	command = SRB_INB(sc, sc->sc_srb, SRB_CMD);

	if (command == XMIT_XID_CMD || command == XMIT_TEST_CMD) {
		ASB_OUTB(sc, asb, XMIT_CMD, command);
		ASB_OUTW(sc, asb, XMIT_FRAMELEN, 0x11);
/*
 * XXX 0xe == sizeof(struct token_header)
 */
		ASB_OUTB(sc, asb, XMIT_HDRLEN, 0x0e);

		SR_OUTB(sc, (dhb + 0), TOKEN_AC);
		SR_OUTB(sc, (dhb + 1), TOKEN_FC);
		/* Load destination and source addresses. */
		for (i=0; i < ISO88025_ADDR_LEN; i++) {
			SR_OUTB(sc, (dhb + 2 + i), 0xff);
			SR_OUTB(sc, (dhb + 8 + i), 0x00);
		}
	}
	else {
/*
 * XXX what's command here ?  command = 0x0d (always ?)
 */
		/* if data in queue, copy mbuf chain to DHB */
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 != 0) {
#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m0);
#endif
			/* Pull packet off interface send queue, fill DHB. */
			trh = mtod(m0, struct token_header *);
			hlen = sizeof(struct token_header);
			if (trh->token_shost[0] & TOKEN_RI_PRESENT) {
/*
 * XXX assumes route info is in the same mbuf as the token-ring header
 */
				struct token_rif	*rif;

				rif = TOKEN_RIF(trh);
				hlen += ((ntohs(rif->tr_rcf) & TOKEN_RCF_LEN_MASK) >> 8);
			}
			size = tr_mbcopy(sc, dhb, m0);
			m_freem(m0);

			ASB_OUTB(sc, asb, XMIT_CMD, XMIT_UI_FRM);  
			ASB_OUTB(sc, asb, XMIT_HDRLEN, hlen);

			/* Set size of transmission frame in ASB. */
			ASB_OUTW(sc, asb, XMIT_FRAMELEN, size);
		}
		else {
			printf("%s: unexpected empty mbuf send queue\n",
				sc->sc_dev.dv_xname);

			/* Set size of transmission frame in ASB to zero. */
			ASB_OUTW(sc, asb, XMIT_FRAMELEN, 0);
		}
	}
/*
 * XXX asb_response(void *asb, len)
 */
	/* tell adapter that there is a response in the ASB */
	ACA_SETB(sc, ACA_ISRA_o, RESP_IN_ASB);
}

/*
 *  Interrupt handler for fast path transmit complete
 */
void
tr_xint(sc)
struct tr_softc *sc;
{
	u_short	tail;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_size_t txbuf;

	/*
	 * To prevent race conditions on 8-bit cards when reading or writing
	 * 16-bit values. See page 4-12 of the IBM manual.
	 * XXX use volatile ?
	 */
	do {
		tail = TXCA_INW(sc, TXCA_COMPLETION_QUEUE_TAIL);
	} while (tail != TXCA_INW(sc, TXCA_COMPLETION_QUEUE_TAIL));
	while (tail != TXCA_INW(sc, TXCA_FREE_QUEUE_TAIL)) {
		txbuf =  TXCA_INW(sc, TXCA_FREE_QUEUE_TAIL) - XMIT_NEXTBUF;
		txbuf =  TXB_INW(sc, txbuf, XMIT_NEXTBUF) - XMIT_NEXTBUF;
		if (TXB_INB(sc, txbuf, XMIT_RETCODE) != 0) {
			ifp->if_oerrors++;
			printf("tx: retcode = %x\n",
			    TXB_INB(sc, txbuf, XMIT_RETCODE));
		}
		sc->sc_xmit_buffers +=
		    (TXB_INW(sc, txbuf, XMIT_FRAMELEN) + 514 - 1) / 514;
		tail = TXB_INW(sc, txbuf, XMIT_LASTBUF);
		TXCA_OUTW(sc, TXCA_FREE_QUEUE_TAIL, tail);
		tail = TXCA_INW(sc, TXCA_COMPLETION_QUEUE_TAIL);
		do {
			tail = TXCA_INW(sc, TXCA_COMPLETION_QUEUE_TAIL);
		} while (tail != TXCA_INW(sc, TXCA_COMPLETION_QUEUE_TAIL));
	}
	if (sc->sc_xmit_buffers == sc->sc_nbuf)
		ifp->if_flags &= ~IFF_OACTIVE;
	tr_start(ifp);
}


/*
 * copy out the packet byte-by-byte in reasonably optimal fashion
 */
int
tr_mbcopy(sc, dhb, m0)
struct tr_softc *sc;
bus_size_t dhb;
struct mbuf *m0;
{
	bus_size_t addr = dhb;
	int len, size = 0;
	char *ptr;
	struct mbuf *m;

	for (m = m0; m; m = m->m_next) {
		len = m->m_len;
		ptr = mtod(m, char *);

		bus_space_write_region_1(sc->sc_memt, sc->sc_sramh,
		    addr, ptr, len);
		size += len;
		addr += len;
	}
	return (size);
}

/*
 * Pull read data off an interface.
 * Len is length of data, with local net header stripped.
 * Off is non-zero if a trailer protocol was used, and
 * gives the offset of the trailer information.
 * XXX trailer information, really ????
 * We copy the trailer information and then all the normal
 * data into mbufs.
 *
 * called from tr_rint - receive interrupt routine
 */
struct mbuf *
tr_get(sc, totlen, ifp)
struct tr_softc *sc;
int totlen;
struct ifnet *ifp;  
{
	int len;
	struct mbuf *m, *m0, *newm;

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == 0)
		return (0);

	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = totlen;
	len = MHLEN;

	m = m0;
	while (totlen > 0) {
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m0);
				return 0;
			}
			len = MCLBYTES;
		}

		/*
		 * Make sure data after the MAC header is aligned.
		 */
		if (m == m0) {
			caddr_t newdata = (caddr_t)
			   ALIGN(m->m_data + sizeof(struct token_header)) - 
			   sizeof(struct token_header);
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}
		m->m_len = len = min(totlen, len);
		tr_bcopy(sc, mtod(m, char *), len);
		totlen -= len;
		if (totlen > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == 0){
				m_freem(m0);
				return (0);
			}
			m->m_next = newm;
			m = newm;
			len = MLEN;
		}
		/*
		 * ignore trailers case again
		 */
	}
	return (m0);
}

/*
 *  tr_ioctl - process an ioctl request
 */
int
tr_ioctl(ifp, cmd, data)
struct ifnet *ifp;
u_long cmd;
caddr_t data;
{
	struct tr_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *) data;
	int s;
	int error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
		/* XXX if not running  */
			if ((ifp->if_flags & IFF_RUNNING) == 0) {
				tr_init(sc);   /* before arp_ifinit */
				tr_sleep(sc);
			}
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif /* INET */
		default:
			/* XXX if not running */
			if ((ifp->if_flags & IFF_RUNNING) == 0) {
				tr_init(sc);   /* before arpwhohas */
				tr_sleep(sc);
			}
			break;
		}
		break;
	case SIOCSIFFLAGS:
		/*
		 * 1- If the adapter is DOWN , turn the device off
		 *       ie. adapter down but still running
		 * 2- If the adapter is UP, turn the device on
		 *       ie. adapter up but not running yet
		 */
		if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) == IFF_RUNNING) {
			tr_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		}
		else if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) == IFF_UP) {
			tr_init(sc);
			tr_sleep(sc);
		}
		else {
/*
 * XXX handle other flag changes
 */
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
#ifdef SIOCSIFMTU
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > sc->sc_maxmtu)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
#endif
	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}


/*
 *  tr_bcopy - like bcopy except that it knows about the structure of
 *	      adapter receive buffers.
 */
void 
tr_bcopy(sc, dest, len)
struct tr_softc *sc;	/* pointer to softc struct for this adapter */
u_char *dest;		/* destination address */
int len;		/* number of bytes to copy */
{
	struct rbcb *rbc = &sc->rbc;	/* pointer to rec buf ctl blk */

	/* While amount of data needed >= amount in current receive buffer. */
	while (len >= rbc->data_len) {
		/* Copy all data from receive buffer to destination. */

		bus_space_read_region_1(sc->sc_memt, sc->sc_sramh,
		    rbc->rbuf_datap, dest, (bus_size_t)rbc->data_len);
		len -= rbc->data_len;	/* update length left to transfer */
		dest += rbc->data_len;	/* update destination address */

		/* Make next receive buffer current receive buffer. */
		rbc->rbufp = rbc->rbufp_next;
		if (rbc->rbufp != 0) { /* More receive buffers? */

			/* Calculate pointer to next receive buffer. */
			rbc->rbufp_next = RB_INW(sc, rbc->rbufp, RB_NEXTBUF);
			if (rbc->rbufp_next != 0)
				rbc->rbufp_next -= RB_NEXTBUF;

			/* Get pointer to data in current receive buffer. */
			rbc->rbuf_datap = rbc->rbufp + RB_DATA;

			/* Get length of data in current receive buffer. */
			rbc->data_len = RB_INW(sc, rbc->rbufp, RB_BUFLEN);
		}
		else {
			if (len != 0)	/* len should equal zero. */
				printf("tr_bcopy: residual data not copied\n");
			return;
		}
	}

	/* Amount of data needed is < amount in current receive buffer. */

	bus_space_read_region_1(sc->sc_memt, sc->sc_sramh,
	    rbc->rbuf_datap, dest, (bus_size_t)len);
	rbc->data_len -= len;	/* Update count of data in receive buffer. */
	rbc->rbuf_datap += len;	/* Update pointer to receive buffer data. */
}

/*
 *  tr_opensap - open the token ring SAP interface
 */
void
tr_opensap(sc, type) 
struct tr_softc *sc;
u_char type;
{
	bus_size_t srb = sc->sc_srb;

/************************************************************************
 ** To use the SAP level interface, we will have to execute a          ** 
 ** DLC.OPEN.SAP (pg.6-61 of the Token Ring Tech. Ref.) after we have  **
 ** received a good return code from the DIR.OPEN.ADAPTER command.     **
 ** We will open the IP SAP x'aa'.                                     **
 **                                                                    **
 ** STEPS:                                                             **
 **      1) Reset SRB response interrupt bit                           **
 **      2) Use the open_sap srb.                                      **
 **      3) Fill the following fields:                                 **
 **            command    - x'15'                                      **
 **            sap_value  - x'aa'                                      **
 **            sap_options- x'24'                                      **
 **                                                                    **
 ***********************************************************************/

	ACA_RSTB(sc, ACA_ISRP_o, ~(SRB_RESP_INT));

	SRB_OUTB(sc, srb, SRB_CMD, DLC_OPEN_SAP);  
	SRB_OUTB(sc, srb, SRB_RETCODE, 0x00);  
	SRB_OUTW(sc, srb, SRB_OPNSAP_STATIONID, 0x0000);
	SRB_OUTB(sc, srb, SRB_OPNSAP_TIMERT1, 0x00);
	SRB_OUTB(sc, srb, SRB_OPNSAP_TIMERT2, 0x00);
	SRB_OUTB(sc, srb, SRB_OPNSAP_TIMERTI, 0x00);
	SRB_OUTB(sc, srb, SRB_OPNSAP_MAXOUT, 0x00);
	SRB_OUTB(sc, srb, SRB_OPNSAP_MAXIN, 0x00);
	SRB_OUTB(sc, srb, SRB_OPNSAP_MAXOUTINCR, 0x00);
	SRB_OUTB(sc, srb, SRB_OPNSAP_MAXRETRY, 0x00);
	SRB_OUTB(sc, srb, SRB_OPNSAP_GSAPMAXMEMB, 0x00);
	SRB_OUTW(sc, srb, SRB_OPNSAP_MAXIFIELD, 0x0088);  
	SRB_OUTB(sc, srb, SRB_OPNSAP_SAPVALUE, type);     
	SRB_OUTB(sc, srb, SRB_OPNSAP_SAPOPTIONS, 0x24);
	SRB_OUTB(sc, srb, SRB_OPNSAP_STATIONCNT, 0x01);
	SRB_OUTB(sc, srb, SRB_OPNSAP_SAPGSAPMEMB, 0x00);

	ACA_SETB(sc, ACA_ISRP_e, INT_ENABLE);
	ACA_SETB(sc, ACA_ISRA_o, CMD_IN_SRB);
}

/*
 *  tr_sleep - sleep to wait for adapter to open
 */
void
tr_sleep(sc)
struct tr_softc *sc;
{
	int error;

	error = tsleep(&sc->tr_sleepevent, 1, "trsleep", hz * 30);
	if (error == EWOULDBLOCK)
		printf("%s: sleep event timeout\n", sc->sc_dev.dv_xname);
}

void
tr_watchdog(ifp)
struct ifnet	*ifp;
{
	struct tr_softc	*sc = ifp->if_softc;

	log(LOG_ERR,"%s: device timeout\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;

	tr_reset(sc);
}
