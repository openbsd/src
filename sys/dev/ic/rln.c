/*	$OpenBSD: rln.c,v 1.17 2005/11/07 03:20:00 brad Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * Driver for the Proxim RangeLAN2 wireless network adaptor.
 *
 * Information and ideas gleaned from disassembly of Dave Koberstein's
 * <davek@komacke.com> Linux driver (apparently based on Proxim source),
 * from Yoichi Shinoda's <shinoda@cs.washington.edu> BSDI driver, and
 * Geoff Voelker's <voelker@cs.washington.edu> Linux port of the same.
 *
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/rln.h>
#include <dev/ic/rlnvar.h>
#include <dev/ic/rlnreg.h>
#include <dev/ic/rlncmd.h>

/* Autoconfig definition of driver back-end. */
struct cfdriver rln_cd = {
	NULL, "rln", DV_IFNET
};

void	rlninit(struct rln_softc *);
void	rlnstart(struct ifnet*);
void	rlnwatchdog(struct ifnet*);
int	rlnioctl(struct ifnet *, u_long, caddr_t);
void	rlnstop(struct rln_softc *);

/* Interrupt handler. */
void	rlnsoftintr(void *);

/* Packet I/O. */
int	rln_transmit(struct rln_softc *, struct mbuf *,
			int, int);
struct mbuf * rlnget(struct rln_softc *, struct rln_mm_cmd *,
			int);

/* Card protocol-level functions. */
int	rln_getenaddr(struct rln_softc *, u_int8_t *);
int	rln_getpromvers(struct rln_softc *, char *, int);
int	rln_sendinit(struct rln_softc *);
#if notyet
int	rln_roamconfig(struct rln_softc *);
int	rln_roam(struct rln_softc *);
int	rln_multicast(struct rln_softc *, int);
int	rln_searchsync(struct rln_softc *);
int	rln_iosetparam(struct rln_softc *, struct rln_param *);
int	rln_lockprom(struct rln_softc *);
int	rln_ito(struct rln_softc *);
int	rln_standby(struct rln_softc *);
#endif

/* Back-end attach and configure. Assumes card has been reset. */
void
rlnconfig(sc)
	struct rln_softc * sc;
{
	struct ifnet *	ifp = &sc->sc_arpcom.ac_if;
	char		promvers[7];
	int		i;

	dprintf(" [attach %p]", sc);

	/* Use the flags supplied from config. */
	sc->sc_cardtype |= sc->sc_dev.dv_cfdata->cf_flags;

	/* Initialise values in the soft state. */
	sc->sc_pktseq = 0;	/* rln_newseq() */
	sc->sc_txseq = 0;
	sc->sc_state = 0;

	/* Initialise user-configurable params. */
	sc->sc_param.rp_roam_config = RLN_ROAM_NORMAL;
	sc->sc_param.rp_security = RLN_SECURITY_DEFAULT;
	sc->sc_param.rp_station_type = RLN_STATIONTYPE_ALTMASTER;
	sc->sc_param.rp_domain = 0;
	sc->sc_param.rp_channel = 1;
	sc->sc_param.rp_subchannel = 1;

	bzero(sc->sc_param.rp_master, sizeof sc->sc_param.rp_master);

	/* Initialise the message mailboxes. */
	for (i = 0; i < RLN_NMBOX; i++)
		sc->sc_mbox[i].mb_state = RLNMBOX_VOID;

	/* Probe for some properties. */
	printf(", %s-piece", 
	    (sc->sc_cardtype & RLN_CTYPE_ONE_PIECE) ? "one" : "two");
	if (sc->sc_cardtype & RLN_CTYPE_OEM)
		printf(" oem");
	if (sc->sc_cardtype & RLN_CTYPE_UISA)
		printf(" micro-isa");

	/* Read the card's PROM revision. */
	if (rln_getpromvers(sc, promvers, sizeof promvers)) {
		printf(": could not read PROM version\n");
		return;
	}
	printf(", fw %.7s", promvers);

	/* Fetch the card's MAC address. */
	if (rln_getenaddr(sc, sc->sc_arpcom.ac_enaddr)) {
		printf(": could not read MAC address\n");
		return;
	}
	printf(", addr %s", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	timeout_set(&sc->sc_timeout, rlnsoftintr, sc);

	/* Attach as a network interface. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = rlnstart;
	ifp->if_ioctl = rlnioctl;
	ifp->if_watchdog = rlnwatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	ether_ifattach(ifp);
}

/* Bring device up. */
void
rlninit(sc)
	struct rln_softc * sc;
{
	/* LLDInit() */
	struct ifnet * ifp = &sc->sc_arpcom.ac_if;
	int s;

	s = splnet();
	dprintf(" [init]");

	sc->sc_intsel = 0;
	sc->sc_status = 0;
	sc->sc_control = 0;
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Do a hard reset. */
	if (rln_reset(sc)) {
		printf("%s: could not reset card\n", sc->sc_dev.dv_xname);
		goto fail;
	}
	sc->sc_state = 0;	/* Also clears RLN_STATE_NEEDINIT. */

	/* Use this host's name as a master name. */
	if (!cold && sc->sc_param.rp_master[0] == '\0') {
		bcopy(hostname, sc->sc_param.rp_master, 
		    min(hostnamelen, sizeof sc->sc_param.rp_master));
	}

	rln_enable(sc, 1);

	/* Initialise operational params. */
	if (rln_sendinit(sc)) {
		printf("%s: could not set card parameters\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}
#if 0
	rln_roamconfig(sc);
	/* rln_lockprom(sc); */
	/* SendSetITO() */
	rln_multicast(sc, 1);
	rln_roam(sc);

	/* Synchronise with something. */
	rln_searchsync(sc);
#endif
	ifp->if_flags |= IFF_RUNNING;
	rlnstart(ifp);
	splx(s);

	return;

    fail:
	ifp->if_flags &= ~IFF_UP;
	splx(s);
	return;
}

/* Start outputting on interface. This is always called at splnet(). */
void
rlnstart(ifp)
	struct ifnet *	ifp;
{
	struct rln_softc * sc = (struct rln_softc *)ifp->if_softc;
	struct mbuf *	m0;
	int		len, pad, ret, s;

	dprintf(" start[");

	if (sc->sc_state & RLN_STATE_NEEDINIT)
		rlninit(sc);

	/* Don't transmit if interface is busy or not running. */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING) {
		dprintf(" %s] ", (ifp->if_flags & IFF_OACTIVE) ? 
		    "busy" : "stopped");
		return;
	}

	/* Don't transmit if we are not synchronised. */
	if ((sc->sc_state & RLN_STATE_SYNC) == 0) {
		dprintf(" nosync]");
		return;
	}

	rln_enable(sc, 1);

    startagain:
	s = splnet();
	IFQ_DEQUEUE(&ifp->if_snd, m0);
	splx(s);

	if (m0 == NULL) {
		dprintf(" empty]");
		return;
	}

#if NBPFILTER > 0
	/* Tap packet stream here for BPF listeners. */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	/* We need to use m->m_pkthdr.len, so require the header. */
	if ((m0->m_flags & M_PKTHDR) == 0) {
		printf("%s: no mbuf header\n", sc->sc_dev.dv_xname);
		goto oerror;
	}

	len = m0->m_pkthdr.len;

#define PACKETMIN	(sizeof (struct ether_header) + ETHERMIN)
#define PACKETMAX	(sizeof (struct ether_header) + ETHERMTU + 4)

	/* Packet size has to be an even number between 60 and 1518 octets. */
	pad = len & 1;
	if (len + pad < PACKETMIN)
		pad = PACKETMIN - len;

	if (len + pad > PACKETMAX) {
		printf("%s: packet too big (%d > %d)\n",
		    sc->sc_dev.dv_xname, len + pad,
		    PACKETMAX);
		++ifp->if_oerrors;
		m_freem(m0);
		goto startagain;
	}

	ret = rln_transmit(sc, m0, len, pad);
	if (ret)
		goto oerror;

	ifp->if_flags |= IFF_OACTIVE;
	m_freem(m0);

	dprintf(" sent]");
	return;

oerror:
	++ifp->if_oerrors;
	m_freem(m0);
	rln_need_reset(sc);
	return;
}

/* Transmit one packet. */
int
rln_transmit(sc, m0, len, pad)
	struct rln_softc *	sc;
	struct mbuf *	m0;
	int 		len;
	int 		pad;
{
	struct mbuf *	m;
	int 		zfirst;
	int 		actlen;
	int 		tlen = len + pad;
	struct rln_msg_tx_state state;
	static u_int8_t zeroes[60];
	struct rln_mm_sendpacket cmd = { RLN_MM_SENDPACKET };

	/* Does the packet start with a zero bit? */
	zfirst = ((*mtod(m0, u_int8_t *) & 1) == 0);

	cmd.mode = 
		RLN_MM_SENDPACKET_MODE_BIT7 | 
		   (zfirst ? RLN_MM_SENDPACKET_MODE_ZFIRST : 0) |
		   (0 ? RLN_MM_SENDPACKET_MODE_QFSK : 0),	/* sc->qfsk? */
	cmd.power = 0x70;	/* 0x70 or 0xf0 */
	cmd.length_lo = htons(4 + tlen) & 0xff;
	cmd.length_hi = (htons(4 + tlen) >> 8) & 0xff;
	cmd.xxx1 = 0;
	cmd.xxx2 = 0;
	cmd.xxx3 = 0;

	/* A unique packet-level sequence number, independent of sc_seq. */
	cmd.sequence = sc->sc_txseq;
	sc->sc_txseq++;
	if (sc->sc_txseq > RLN_MAXSEQ)
		sc->sc_txseq = 0;

	dprintf(" T[%d+%d", len, pad);

	if (rln_msg_tx_start(sc, &cmd, sizeof cmd + tlen, &state))
		goto error;

	cmd.mm_cmd.cmd_seq = rln_newseq(sc);

	/* Send the SENDPACKET command header  */
#ifdef RLNDUMP
	printf("%s: send %c%d seq %d data ", sc->sc_dev.dv_xname,
	    cmd.mm_cmd.cmd_letter, cmd.mm_cmd.cmd_fn, cmd.mm_cmd.cmd_seq);
	RLNDUMPHEX(&cmd, sizeof cmd);
	printf(":");
#endif
	rln_msg_tx_data(sc, &cmd, sizeof cmd, &state);

	/* XXX do we need to insert a hardware header here??? */

	/* Follow the header immediately with the packet payload */
	actlen = 0;
	for (m = m0; m; m = m->m_next)	{
		if (m->m_len) {
#ifdef RLNDUMP
			RLNDUMPHEX(mtod(m, void *), m->m_len);
#endif
			rln_msg_tx_data(sc, mtod(m, void *), m->m_len, &state);
		}
		if (m->m_next)
			printf("|");
		actlen += m->m_len;
	}
#ifdef DIAGNOSTIC
	if (actlen != len)
		panic("rln_transmit: len %d != %d", actlen, len);
	if (pad > sizeof zeroes)
		panic("rln_transmit: pad %d > %d", pad, sizeof zeroes);
#endif
	if (pad) {
#ifdef RLNDUMP
		printf(":");
		RLNDUMPHEX(zeroes, pad);
#endif
		rln_msg_tx_data(sc, zeroes, pad, &state);
	}

#ifdef RLNDUMP
	printf("\n");
#endif
	if (rln_msg_tx_end(sc, &state))
		goto error;
	return (0);

    error:
	dprintf(" error]");
	return (-1);
}

/* (Supposedly) called when interrupts are suspiciously absent. */
void
rlnwatchdog(ifp)
	struct ifnet * ifp;
{
	struct rln_softc * sc = (struct rln_softc *)ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;
	rlninit(sc);
	rln_enable(sc, 1);
}

/* Handle single card interrupt. */
int
rlnintr(arg)
	void *	arg;
{
	struct rln_softc * sc = (struct rln_softc *)arg;

	dprintf("!");

	/* Tell card not to interrupt any more. */
	rln_enable(sc, 0);

	if (cold)
		/* During autoconfig - must handle interrupts now. */
		rlnsoftintr(sc);
	else
		/* Handle later. */
		timeout_add(&sc->sc_timeout, 1);

	return (1);
}

/* Process earlier card interrupt at splsoftnet. */
void
rlnsoftintr(arg)
	void * arg;
{
	struct rln_softc *sc = (struct rln_softc *)arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int len;
	u_int8_t w;
	struct rln_mm_cmd hdr;
	int s;

	s = splsoftnet();
	dprintf(" si(");

    again:
	/* Save wakeup state. */
	w = rln_wakeup(sc, RLN_WAKEUP_SET);

	if ((len = rln_rx_request(sc, 300)) < 0) {
		/* Error in transfer. */
		rln_need_reset(sc);
		rln_rx_end(sc);
	} else if (len < sizeof hdr) {
		/* Short message. */
		rln_rx_end(sc);
		printf("%s: short msg (%d)\n", sc->sc_dev.dv_xname, len);
		ifp->if_ierrors++;
	} else {
		/* Valid message: read header and process. */
		rln_rx_data(sc, &hdr, sizeof hdr);
		rlnread(sc, &hdr, len);
	}

	/* Ensure that wakeup state is unchanged if transmitting. */
	if (ifp->if_flags & IFF_OACTIVE)
		w |= RLN_WAKEUP_NOCHANGE;
	rln_wakeup(sc, w);

	/* Check for more interrupts. */
	if ((sc->sc_state & RLN_STATE_NEEDINIT) == 0 && 
	    rln_status_rx_ready(sc)) {
		if (rln_status_rx_read(sc) == RLN_STATUS_RX_ERROR) {
#ifdef DIAGNOSTIC
			printf("%s: protocol error\n", sc->sc_dev.dv_xname);
#endif
			DELAY(100 * 1000);	/* Woah, baby. */
			rln_clear_nak(sc);
		} else {
#ifdef DIAGNOSTIC
			printf("%s: intr piggyback\n", sc->sc_dev.dv_xname);
#endif
			goto again;
		}
	}

	rln_eoi(sc);
	rln_enable(sc, 1);

	dprintf(")");
	splx(s);
}

/* Read and process a message from the card. */
void
rlnread(sc, hdr, len)
	struct rln_softc *sc;
	struct rln_mm_cmd *hdr;
	int len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	u_int8_t data[1538];
	u_int8_t  *buf;
	size_t	  buflen;
	struct rln_pdata pd = RLN_PDATA_INIT;
	struct rln_mm_synchronised * syncp = (struct rln_mm_synchronised *)data;
	int s;

	dprintf(" [read]");

	/* Were we waiting for this message? */
	if (rln_mbox_lock(sc, hdr->cmd_seq, (void **)&buf, &buflen) == 0) {
#ifdef DIAGNOSTIC
		if (buflen < sizeof *hdr)
			panic("rlnread buflen");
#endif
		bcopy(hdr, buf, sizeof *hdr);
		buf += sizeof *hdr;
		len -= sizeof *hdr;
		buflen -= sizeof *hdr;
		if (len) {
			if (len == buflen)		/* Expected size */
				rln_rx_pdata(sc, buf, len, &pd);
			else if (len < buflen) {	/* Underfill */
#ifdef DIAGNOSTIC
				printf("%s: underfill %d<%d, cmd %c%d\n",
					sc->sc_dev.dv_xname,
					len, buflen,
					hdr->cmd_letter, hdr->cmd_fn);
#endif
				rln_rx_pdata(sc, buf, len, &pd);
			} else {			/* Overflow */
#ifdef DIAGNOSTIC
				printf("%s: overflow %d>%d, cmd %c%d\n",
					sc->sc_dev.dv_xname,
					len, buflen,
					hdr->cmd_letter, hdr->cmd_fn);
#endif
				rln_rx_pdata(sc, buf, buflen, &pd);
				/* Drain the rest somewhere. */
				rln_rx_pdata(sc, data, len - buflen, &pd);
			}
		}
		rln_rx_end(sc);

		/* This message can now be handled by the waiter. */
		rln_mbox_unlock(sc, hdr->cmd_seq, len + sizeof *hdr);
		return;
	} 

	/* Otherwise, handle the message, right here, right now. */

	/* Check if we can cope with the size of this message. */
	if (len > sizeof data) {
		printf("%s: msg too big (%d)\n", sc->sc_dev.dv_xname, len);
		ifp->if_ierrors++;
		rln_rx_end(sc);
		/* rln_need_reset(sc); */
		return;
	}

	/* Check for error results. */
	if (hdr->cmd_error & 0x80) {
		printf("%s: command error 0x%02x command %c%d len=%d\n",
			sc->sc_dev.dv_xname,
			hdr->cmd_error & ~0x80,
			hdr->cmd_letter, hdr->cmd_fn,
			len);
		ifp->if_ierrors++;
		rln_rx_end(sc);
		rln_need_reset(sc);
		return;
	}

	/*
	 * "b1": Receiving a packet is a special case.
	 * We wish to read the data with pio straight into an 
	 * mbuf to avoid a memory-memory copy.
	 */
	if (hdr->cmd_letter == 'b' && hdr->cmd_fn == 1) {
		m = rlnget(sc, hdr, len);
		rln_rx_end(sc);
		if (m == NULL) 
			return;
		ifp->if_ipackets++;
#ifdef DIAGNOSTIC
		if (bcmp(mtod(m, u_int8_t *), "prox", 4) == 0) {
			printf("%s: proxim special packet received\n",
			    sc->sc_dev.dv_xname);
		}
#endif

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		ether_input_mbuf(ifp, m);
		return;
	}


	/* Otherwise we read the packet into a buffer on the stack. */
	bcopy(hdr, data, sizeof *hdr);
	if (len > sizeof *hdr) 
		rln_rx_pdata(sc, data + sizeof *hdr, len - sizeof *hdr, &pd);
	rln_rx_end(sc);

#ifdef RLNDUMP
	printf("%s: recv %c%d seq %d data ", sc->sc_dev.dv_xname,
	    hdr->cmd_letter, hdr->cmd_fn, hdr->cmd_seq);
	RLNDUMPHEX(hdr, sizeof hdr);
	printf(":");
	RLNDUMPHEX(data + sizeof hdr, len - sizeof hdr);
	printf("\n");
#endif

	switch (RLN_MM_CMD(hdr->cmd_letter, hdr->cmd_fn)) {
	case RLN_MM_CMD('b', 0):			/* b0: Transmit done. */
#ifdef DIAGNOSTIC
		if (len != 7)
			printf("%s: 'b0' len %d != 7\n",
			    sc->sc_dev.dv_xname, len);
#endif
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_opackets++;
		s = splnet();
		rlnstart(ifp);
		splx(s);
		break;

	case RLN_MM_CMD('a', 20):			/* a20: Card fault. */
		printf("%s: hardware fault\n", sc->sc_dev.dv_xname);
		break;

	case RLN_MM_CMD('a', 4):			/* a4: Sync'd. */
		if (bcmp(syncp->enaddr, sc->sc_arpcom.ac_enaddr,
		    ETHER_ADDR_LEN) == 0) {
			/* Sync'd to own enaddr. */
 /*
  * From http://www.proxim.com/support/faq/7400.shtml
  * 3. RLNSETUP reports that I'm synchronized to my own MAC address. What
  *    does that mean?
  *    You are the acting Master for this network. Either you are
  *    configured as the Master or as an Alternate Master. If you are an
  *    Alternate Master, you may be out of range or on a different Domain
  *    and Security ID from the true Master.
  */

			printf("%s: nothing to sync to; now master ",
			    sc->sc_dev.dv_xname);
		}
		else
			printf("%s: synchronised to ", sc->sc_dev.dv_xname);
		printf("%.11s (%s) channel %d/%d\n",
		    syncp->mastername,
		    ether_sprintf(syncp->enaddr),
		    syncp->channel,
		    syncp->subchannel);

		/* Record the new circumstances. */
		sc->sc_param.rp_channel = syncp->channel;
		sc->sc_param.rp_subchannel = syncp->subchannel;
		sc->sc_state |= RLN_STATE_SYNC;

		/* Resume sending. */
		s = splnet();
		rlnstart(ifp);
		splx(s);
		break;

	case RLN_MM_CMD('a', 5):			/* a4: Lost sync. */
		printf("%s: lost sync\n", sc->sc_dev.dv_xname);
		sc->sc_state &= ~RLN_STATE_SYNC;
		break;

	case RLN_MM_CMD('a', 18):			/* a18: Roaming. */
		printf("%s: roaming\n", sc->sc_dev.dv_xname);
		break;
	default:
#ifdef DIAGNOSTIC
		printf("%s: msg `%c%d' seq %d data {",
		    sc->sc_dev.dv_xname, 
		    hdr->cmd_letter, hdr->cmd_fn, hdr->cmd_seq);
		RLNDUMPHEX(hdr, sizeof hdr);
		printf(":");
		RLNDUMPHEX(data, len);
		printf("}\n");
#endif
		break;
	}

}

/* Extract a received network packet from the card. */
struct mbuf *
rlnget(sc, hdr, totlen)
	struct rln_softc *sc;
	struct rln_mm_cmd *hdr;
	int totlen;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int len;
	int pad;
	struct mbuf *m, **mp, *top;
	struct rln_pdata pd = RLN_PDATA_INIT;
	struct {
		u_int8_t rssi;
		u_int8_t xxx1;	/* always 00? */
		u_int16_t len;	/* payload length */
		u_int8_t xxx2;	/* always 00? */
		u_int8_t xxx3;	/* always c0? */
		u_int8_t seq;
		u_int8_t xxx4;
		struct	ether_addr to;	 /* destination station addr */
		struct	ether_addr from; /* sending station addr */
	} hwhdr;

	dprintf(" [get]");

#ifdef RLNDUMP
	/* Decode the command header: */
	printf("%s: recv %c%d seq %d data ", sc->sc_dev.dv_xname,
	    hdr->cmd_letter, hdr->cmd_fn, hdr->cmd_seq);
	RLNDUMPHEX(hdr, sizeof hdr);
	printf(":");
#endif
	totlen -= sizeof *hdr;

#ifdef DIAGNOSTIC
	if (totlen <= 0) {
		printf("%s: empty packet", sc->sc_dev.dv_xname);
		goto drop;
	}
#endif

	/* Decode the hardware header: */
	rln_rx_pdata(sc, &hwhdr, sizeof hwhdr, &pd);
	totlen -= sizeof hwhdr;
#ifdef RLNDUMP
	RLNDUMPHEX(&hwhdr, sizeof hwhdr);
	printf("/");
#endif
	/* (Most of the following code fleeced from elink3.c.) */

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto drop;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	/*
	 * Insert some leading padding in the mbuf, so that payload data is
	 * aligned.
	 */
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	m->m_data += pad;
	len = MHLEN - pad;
	top = 0;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				goto drop;
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT) {
				len = MCLBYTES;
				if (!top) {
					m->m_data += pad;
					len -= pad;
				}
			}
		}
		len = min(totlen, len);
		rln_rx_pdata(sc, mtod(m, u_int8_t *), len, &pd);
#ifdef RLNDUMP
		RLNDUMPHEX(mtod(m, u_int8_t *), len);
		if (totlen != len)
			printf("|");
#endif
		m->m_len = len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}
#ifdef RLNDUMP
	printf("\n");
#endif
	return top;

drop:
#ifdef RLNDUMP
	printf(": drop\n");
#endif
	ifp->if_iqdrops++;
	return NULL;
}

/* Interface control. */
int
rlnioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct rln_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error;
	int need_init;

	printf("%s: ioctl cmd[%c/%d] data=%x\n", sc->sc_dev.dv_xname,
		IOCGROUP(cmd), IOCBASECMD(cmd), data);

	s = splnet();
	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) != 0) {
		splx(s);
		return error;
	}

	switch (cmd) {
	case SIOCSIFADDR:
		/* Set address. */
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			rlninit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			rlninit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		need_init = 0;

		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/* Was running, want down: stop. */
			rlnstop(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/* Was not running, want up: start. */
			need_init = 1;
		} 

		if (ifp->if_flags & IFF_RUNNING) {
			if ((ifp->if_flags & IFF_PROMISC) &&
			    (sc->sc_state & RLN_STATE_PROMISC) == 0) {
				sc->sc_state |= RLN_STATE_PROMISC;
				need_init = 1;
			}
			else if ((ifp->if_flags & IFF_PROMISC) == 0 &&
			    (sc->sc_state & RLN_STATE_PROMISC)) {
				sc->sc_state &= ~RLN_STATE_PROMISC;
				need_init = 1;
			}
		}

		if (need_init)
			rlninit(sc);

		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = EOPNOTSUPP;
		break;

#if notyet
	case RLNIOSPARAM:
		error = rln_iosetparam(sc, (struct rln_param *)&data);
		break;

	case RLNIOGPARAM:
		bcopy(&sc->sc_param, (struct rln_param *)&data, 
		    sizeof sc->sc_param);
		break;
#endif

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

/* Stop output from the card. */
void
rlnstop(sc)
	struct rln_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	dprintf(" [stop]");
	ifp->if_flags &= ~IFF_RUNNING;
	rln_enable(sc, 0);
}

/* Get MAC address from card. */
int
rln_getenaddr(sc, enaddr)
	struct rln_softc *sc;
	u_int8_t * enaddr;
{
	struct rln_mm_cmd query = RLN_MM_GETENADDR;
	struct rln_mm_gotenaddr response = { RLN_MM_GETENADDR };

	if (rln_msg_txrx(sc, &query, sizeof query,
	    &response, sizeof response))
		return (-1);
	bcopy(response.enaddr, enaddr, sizeof response.enaddr);
	return (0);
};

/* Get firmware version string from card. */
int
rln_getpromvers(sc, ver, verlen)
	struct rln_softc *sc;
	char *ver;
	int verlen;
{
	struct rln_mm_cmd query = RLN_MM_GETPROMVERSION;
	struct rln_mm_gotpromversion response = { RLN_MM_GOTPROMVERSION };
	int i;

#ifdef DIAGNOSTIC
	if (verlen != sizeof response.version)
		panic("rln_getpromvers");
#endif

	if (rln_msg_txrx(sc, &query, sizeof query,
	    &response, sizeof response))
		return (-1);
	bcopy(response.version, ver, verlen);
	/* Nul trailing spaces. */
	for (i = verlen - 1; i >= 0 && ver[i] <= ' '; i--)
		ver[i] = '\0';
	return (0);
};

/* Set default operational parameters on card. */
int
rln_sendinit(sc)
	struct rln_softc *sc;
{
	struct rln_mm_init init = { RLN_MM_INIT };
	struct rln_mm_initted iresponse;
#if 0
	struct rln_mm_setmagic magic = { RLN_MM_SETMAGIC };
	struct rln_mm_disablehopping hop = { RLN_MM_DISABLEHOPPING };
	struct rln_mm_cmd response;
#endif

	bzero((char *)&init + sizeof init.mm_cmd,
		sizeof init - sizeof init.mm_cmd);

	dprintf(" [setting parameters]");
	init.opmode = (sc->sc_state & RLN_STATE_PROMISC ?
	    RLN_MM_INIT_OPMODE_PROMISC : RLN_MM_INIT_OPMODE_NORMAL);
	init.stationtype = sc->sc_param.rp_station_type;

	/* Spread-spectrum frequency hopping. */
	init.hop_period = 1;
	init.bfreq = 2;
	init.sfreq = 7;

	/* Choose channel. */
	init.channel = sc->sc_param.rp_channel;
	init.subchannel = sc->sc_param.rp_subchannel;
	init.domain = sc->sc_param.rp_domain;

	/* Name of this station when acting as master. */
	bcopy(sc->sc_param.rp_master, init.mastername, sizeof init.mastername);

	/* Security params. */
	init.sec1 = (sc->sc_param.rp_security & 0x0000ff) >> 0;
	init.sec2 = (sc->sc_param.rp_security & 0x00ff00) >> 8;
	init.sec3 = (sc->sc_param.rp_security & 0xff0000) >> 16;

	init.sync_to = 1;
	bzero(init.syncname, sizeof init.syncname);

	if (rln_msg_txrx(sc, &init, sizeof init,
	    &iresponse, sizeof iresponse))
		return (-1);
#if 0
	dprintf(" [setting magic]");
	magic.fairness_slot = 3;	/* lite: 1, norm: 3, off: -1 */
	magic.deferral_slot = 3;	/* lite: 0, norm: 3, off: -1 */
	magic.regular_mac_retry = 7;
	magic.frag_mac_retry = 10;
	magic.regular_mac_qfsk = 2;
	magic.frag_mac_qfsk = 5;
	magic.xxx1 = 0xff;
	magic.xxx2 = 0xff;
	magic.xxx3 = 0xff;
	magic.xxx4 = 0x00;
	if (rln_msg_txrx(sc, &magic, sizeof magic,
	    &response, sizeof response))
		return (-1);

	dprintf(" [disabling freq hopping]");
	hop.hopflag = RLN_MM_DISABLEHOPPING_HOPFLAG_DISABLE;
	if (rln_msg_txrx(sc, &hop, sizeof hop,
	    &response, sizeof response))
		return (-1);

#endif
	return (0);
}

#if notyet
/* Configure the way the card leaves a basestation. */
int
rln_roamconfig(sc)
	struct rln_softc *sc;
{
	struct rln_mm_setroaming roam = { RLN_MM_SETROAMING };
	struct rln_mm_cmd response;
	static int retry[3] = { 6, 6, 4 };
	static int rssi[3] = { 5, 15, 5 };

	dprintf(" [roamconfig]");
#ifdef DIAGNOSTIC
	if (sc->sc_param.rp_roam_config > 2)
		panic("roamconfig");
#endif
	roam.sync_alarm = 0;
	roam.retry_thresh = retry[sc->sc_param.rp_roam_config];
	roam.rssi_threshold = rssi[sc->sc_param.rp_roam_config];
	roam.xxx1 = 0x5a;
	roam.sync_rssi_threshold = 0;
	roam.xxx2 = 0x5a;
	roam.missed_sync = 0x4;
	if (rln_msg_txrx(sc, &roam, sizeof roam,
	    &response, sizeof response))
		return (-1);

	return (0);
}

/* Enable roaming. */
int
rln_roam(sc)
	struct rln_softc *sc;
{
	struct rln_mm_cmd roam = RLN_MM_ROAM;
	struct rln_mm_cmd response;

	return (rln_msg_txrx(sc, &roam, sizeof roam,
	    &response, sizeof response));
}

/* Enable multicast capability. */
int
rln_multicast(sc, enable)
	struct rln_softc *sc;
	int enable;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct rln_mm_multicast mcast = { RLN_MM_MULTICAST };
	struct rln_mm_cmd response;
	int ret;

	mcast.enable = enable;

	ret = rln_msg_txrx(sc, &mcast, sizeof mcast,
	    &response, sizeof response);
	if (ret == 0) {
		if (enable)
			ifp->if_flags |= IFF_MULTICAST;
		else
			ifp->if_flags &= ~IFF_MULTICAST;
	}
	return (ret);
}

/* Search for and sync with any master. */
int
rln_searchsync(sc)
	struct rln_softc *sc;
{
	struct rln_mm_search search = { RLN_MM_SEARCH };
	struct rln_mm_searching response;

	bzero(search.xxx1, sizeof search.xxx1);
	search.domain = sc->sc_param.rp_domain;
	search.roaming = 1;
	search.xxx3 = 0;
	search.xxx4 = 1;
	search.xxx5 = 0;
	bzero(search.xxx6, sizeof search.xxx6);

	return (rln_msg_txrx(sc, &search, sizeof search,
		&response, sizeof response));
}

/* Set values from an external parameter block. */
int
rln_iosetparam(sc, param)
	struct rln_softc *sc;
	struct rln_param *param;
{
	int error = 0;

	if (param->rp_roam_config > 2)
		error = EINVAL;
	if (param->rp_security > 0x00ffffff)
		error = EINVAL;
	if (param->rp_station_type > 2)
		error = EINVAL;
	if (param->rp_channel > 15)
		error = EINVAL;
	if (param->rp_subchannel > 15)
		error = EINVAL;
	if (error == 0) {
		/* Apply immediately. */
		bcopy(param, &sc->sc_param, sizeof *param);
		if (rln_sendinit(sc))
			error = EIO;
	}
	return (error);
}

/* Protect the eeprom from storing a security ID(?) */
int
rln_lockprom(sc)
	struct rln_softc *sc;
{
	struct rln_mm_cmd lock = RLN_MM_EEPROM_PROTECT;
	struct rln_mm_cmd response;

	/* XXX Always yields an error? */
	return (rln_msg_txrx(sc, &lock, sizeof lock,
	    &response, sizeof response));
}

/* Set the h/w Inactivity Time Out timer on the card. */
int
rln_ito(sc)
	struct rln_softc * sc;
{
	struct rln_mm_setito ito = { RLN_MM_MULTICAST };
	struct rln_mm_cmd response;

	ito.xxx = 3;
	ito.timeout = LLDInactivityTimeOut /* enabler, 0 or 1 */;
	ito.bd_wakeup = LLDBDWakeup /* 0 */;
	ito.pm_sync = LLDPMSync /* 0 */;
	ito.sniff_time = ito.timeout ? LLDSniffTime /* 0 */ : 0;

	if (rln_msg_txrx(sc, &ito, sizeof ito,
	    &response, sizeof response))
		return (-1);
}

/* Put the card into standby mode. */
int
rln_standby(sc)
	struct rln_softc * sc;
{
	struct rln_mm_standby standby = { RLN_MM_STANDBY };

	standby.xxx = 0;
	if (rln_msg_txrx(sc, &ito, sizeof ito, NULL, 0))
		return (-1);
}

void
rln_crypt(userkey, cardkey)
	char *userkey;		/* User's string (max 20 chars). */
	u_int8_t *cardkey;	/* 20 bits (3 bytes) */
{
	/*
	 * From <http://www.proxim.com/learn/whiteppr/rl2security.shtml> 
	 * "RangeLAN2 Security Features":
	 *
         *  The Security ID is a unique, 20 character alphanumeric
         *  string defined and configured by the user. It must be
         *  identically configured in every radio intended to
         *  communicate with others in the same network. Once
         *  configured, the Security ID is reduced to 20 bits by a
         *  proprietary algorithm confidential to Proxim. It is
         *  merged with the radio MAC address (a 12 character field
         *  unique to every radio), scrambled and stored using another
         *  proprietary, confidential algorithm.
	 */
        int32_t key;
        int8_t ret;
        int i;
	int len;
	int32_t multiplicand = 0x80000181;
	int64_t res;

	/* 
	 * This algorithm is `compatible' with Proxim's first
	 * `proprietary confidential algorithm': i.e., it appears
	 * to be functionally identical.
	 */
	len = strlen(s);
        key = 0x030201;
        for (i = 0; i < len; i++) {

                key *= userkey[i];
                res = (int64_t)multiplicand * key;
                key = key - 0xfffffd * 
		    (((key + (int32_t)(res >> 32)) >> 23) - (key >> 31));
        }

        cardkey[0] = (key >> 16) & 0xff;
        cardkey[1] = (key >> 8) & 0xff;
        cardkey[2] = key & 0xff;

	cardkey[0] |= 0x03; 	/* Restrict key space by 2 bits. */
}
#endif
