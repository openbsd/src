/*	$OpenBSD: rl2.c,v 1.3 1999/07/14 03:51:08 d Exp $	*/
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
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/rl2.h>
#include <dev/ic/rl2var.h>
#include <dev/ic/rl2reg.h>
#include <dev/ic/rl2cmd.h>

/* Autoconfig definition of driver back-end. */
struct cfdriver rln_cd = {
	NULL, "rln", DV_IFNET
};

static void	rl2init __P((struct rl2_softc *));
static void	rl2start __P((struct ifnet*));
static void	rl2watchdog __P((struct ifnet*));
static int	rl2ioctl __P((struct ifnet *, u_long, caddr_t));
static int	rl2_probe __P((struct rl2_softc *));
static void	rl2stop __P((struct rl2_softc *));

/* Interrupt handler. */
static void	rl2softintr __P((void *));

/* Packet I/O. */
static int	rl2_transmit __P((struct rl2_softc *, struct mbuf *,
			int, int));
static struct mbuf * rl2get __P((struct rl2_softc *, struct rl2_mm_cmd *,
			int));

/* Card protocol-level functions. */
static int	rl2_getenaddr __P((struct rl2_softc *, u_int8_t *));
static int	rl2_getpromvers __P((struct rl2_softc *, char *, int));
static int	rl2_sendinit __P((struct rl2_softc *));
#if notyet
static int	rl2_roamconfig __P((struct rl2_softc *));
static int	rl2_roam __P((struct rl2_softc *));
static int	rl2_multicast __P((struct rl2_softc *, int));
static int	rl2_searchsync __P((struct rl2_softc *));
static int	rl2_iosetparam __P((struct rl2_softc *, struct rl2_param *));
static int	rl2_lockprom __P((struct rl2_softc *));
static int	rl2_ito __P((struct rl2_softc *));
static int	rl2_standby __P((struct rl2_softc *));
#endif

/* Back-end attach and configure. */
void
rl2config(sc)
	struct rl2_softc * sc;
{
	struct ifnet *	ifp = &sc->sc_arpcom.ac_if;
	char		promvers[7];
	int		i;

	dprintf(" [attach %p]", sc);

	/* Use the flags supplied from config. */
	sc->sc_cardtype |= sc->sc_dev.dv_cfdata->cf_flags;

	/* Initialise values in the soft state. */
	sc->sc_pktseq = 0;	/* rl2_newseq() */
	sc->sc_txseq = 0;

	/* Initialise user-configurable params. */
	sc->sc_param.rp_roam_config = RL2_ROAM_NORMAL;
	sc->sc_param.rp_security = RL2_SECURITY_DEFAULT;
	sc->sc_param.rp_station_type = RL2_STATIONTYPE_ALTMASTER;
	sc->sc_param.rp_domain = 0;
	sc->sc_param.rp_channel = 1;
	sc->sc_param.rp_subchannel = 1;

	bzero(sc->sc_param.rp_master, sizeof sc->sc_param.rp_master);

	/* Initialise the message mailboxes. */
	for (i = 0; i < RL2_NMBOX; i++)
		sc->sc_mbox[i].mb_state = RL2MBOX_VOID;

	/* Keep the sys admin informed. */
	printf(", %s-piece", 
	    (sc->sc_cardtype & RL2_CTYPE_ONE_PIECE) ? "one" : "two");
	if (sc->sc_cardtype & RL2_CTYPE_OEM)
		printf(" oem");
	if (sc->sc_cardtype & RL2_CTYPE_UISA)
		printf(" micro-isa");

	/* Probe/reset the card. */
	if (rl2_probe(sc))
		return;

	/* Read the card's PROM revision. */
	if (rl2_getpromvers(sc, promvers, sizeof promvers)) {
		printf(": could not read PROM version\n");
		return;
	}
	printf(", fw %.7s", promvers);

	/* Fetch the card's MAC address. */
	if (rl2_getenaddr(sc, sc->sc_arpcom.ac_enaddr)) {
		printf(": could not read MAC address\n");
		return;
	}
	printf(", addr %s", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/* Attach as a network interface. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = rl2start;
	ifp->if_ioctl = rl2ioctl;
	ifp->if_watchdog = rl2watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	if_attach(ifp);
	ether_ifattach(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof (struct ether_header));
#endif
}

/* Bring device up. */
static void
rl2init(sc)
	struct rl2_softc * sc;
{
	/* LLDInit() */
	struct ifnet * ifp = &sc->sc_arpcom.ac_if;
	int s;
	extern int cold;

	dprintf(" [init]");

	sc->sc_intsel = 0;
	sc->sc_status = 0;
	sc->sc_control = 0;
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Do a hard reset. */
	if (rl2_reset(sc)) {
		printf("%s: could not reset card\n", sc->sc_dev.dv_xname);
		goto fail;
	}
	sc->sc_state = 0;	/* Also clears RL2_STATE_NEEDINIT. */

	/* Use this host's name as a master name. */
	if (!cold && sc->sc_param.rp_master[0] == '\0') {
		bcopy(hostname, sc->sc_param.rp_master, 
		    min(hostnamelen, sizeof sc->sc_param.rp_master));
	}

	rl2_enable(sc, 1);

	/* Initialise operational params. */
	if (rl2_sendinit(sc)) {
		printf("%s: could not set card parameters\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}
#if 0
	rl2_roamconfig(sc);
	/* rl2_lockprom(sc); */		/* XXX error? */

	/* SendSetITO() */

	rl2_multicast(sc, 1);
	rl2_roam(sc);

	/* Synchronise with something. */
	rl2_searchsync(sc);
#endif
	s = splnet();
	ifp->if_flags |= IFF_RUNNING;
	rl2start(ifp);
	splx(s);

	return;

    fail:
	ifp->if_flags &= ~IFF_UP;
	return;
}

/* Start outputting on interface. This is always called at splnet(). */
static void
rl2start(ifp)
	struct ifnet *	ifp;
{
	struct rl2_softc * sc = (struct rl2_softc *)ifp->if_softc;
	struct mbuf *	m0;
	int		len, pad, ret;

	dprintf(" start[");

	if (sc->sc_state & RL2_STATE_NEEDINIT)
		rl2init(sc);

	/* Don't transmit if interface is busy or not running. */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING) {
		dprintf(" %s] ", (ifp->if_flags & IFF_OACTIVE) ? 
		    "busy" : "stopped");
		return;
	}

	/* Don't transmit if we are not synchronised. */
	if ((sc->sc_state & RL2_STATE_SYNC) == 0) {
		dprintf(" nosync]");
		return;
	}

    startagain:
	IF_DEQUEUE(&ifp->if_snd, m0);

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

	ret = rl2_transmit(sc, m0, len, pad);
	if (ret)
		goto oerror;

	ifp->if_flags |= IFF_OACTIVE;
	m_freem(m0);

	dprintf(" sent]");
	return;

oerror:
	++ifp->if_oerrors;
	m_freem(m0);
	rl2_need_reset(sc);
	return;
}

/* Transmit one packet. */
static int
rl2_transmit(sc, m0, len, pad)
	struct rl2_softc *	sc;
	struct mbuf *	m0;
	int 		len;
	int 		pad;
{
	struct mbuf *	m;
	int 		zfirst;
	int 		actlen;
	int 		tlen = len + pad;
	struct rl2_msg_tx_state state;
	static u_int8_t zeroes[60];
	struct rl2_mm_sendpacket cmd = { RL2_MM_SENDPACKET };

	/* Does the packet start with a zero bit? */
	zfirst = ((*mtod(m0, u_int8_t *) & 1) == 0);

	cmd.mode = 
		RL2_MM_SENDPACKET_MODE_BIT7 | 
		   (zfirst ? RL2_MM_SENDPACKET_MODE_ZFIRST : 0) |
		   (0 ? RL2_MM_SENDPACKET_MODE_QFSK : 0),	/* sc->qfsk? */
	cmd.power = 0x70;	/* 0x70 or 0xf0 */
	cmd.length_lo = htons(4 + tlen) & 0xff;
	cmd.length_hi = (htons(4 + tlen) >> 8) & 0xff;
	cmd.xxx1 = 0;
	cmd.xxx2 = 0;
	cmd.xxx3 = 0;

	/* A unique packet-level sequence number. XXX related to sc_seq? */
	cmd.sequence = sc->sc_txseq;
	sc->sc_txseq++;
	if (sc->sc_txseq > RL2_MAXSEQ)
		sc->sc_txseq = 0;

	dprintf(" T[%d+%d", len, pad);

	if (rl2_msg_tx_start(sc, &cmd, sizeof cmd + tlen, &state))
		goto error;

	cmd.mm_cmd.cmd_seq = rl2_newseq(sc);

#ifdef RL2DUMP
	printf("%s: send %c%d seq %d data ", sc->sc_dev.dv_xname,
	    cmd.mm_cmd.cmd_letter, cmd.mm_cmd.cmd_fn, cmd.mm_cmd.cmd_seq);
	RL2DUMPHEX(&cmd, sizeof cmd);
	printf(":");
#endif
	rl2_msg_tx_data(sc, &cmd, sizeof cmd, &state);

	actlen = 0;
	for (m = m0; m; m = m->m_next)	{
		if (m->m_len) {
#ifdef RL2DUMP
			RL2DUMPHEX(mtod(m, void *), m->m_len);
			printf("|");
#endif
			rl2_msg_tx_data(sc, mtod(m, void *), m->m_len, &state);
		}
		actlen += m->m_len;
	}
#ifdef DIAGNOSTIC
	if (actlen != len)
		panic("rl2_transmit: len %d != %d", actlen, len);
	if (pad > sizeof zeroes)
		panic("rl2_transmit: pad %d > %d", pad, sizeof zeroes);
#endif
	if (pad) {
#ifdef RL2DUMP
		RL2DUMPHEX(zeroes, pad);
#endif
		rl2_msg_tx_data(sc, zeroes, pad, &state);
	}

#ifdef RL2DUMP
	printf("\n");
#endif
	if (rl2_msg_tx_end(sc, &state))
		goto error;
	return (0);

    error:
	dprintf(" error]");
	return (-1);
}

/* (Supposedly) called when interrupts are suspiciously absent. */
static void
rl2watchdog(ifp)
	struct ifnet * ifp;
{
	struct rl2_softc * sc = (struct rl2_softc *)ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;
	rl2init(sc);
	rl2_enable(sc, 1);
}

/* Handle single card interrupt. */
int
rl2intr(arg)
	void *	arg;
{
	struct rl2_softc * sc = (struct rl2_softc *)arg;
	extern int cold;

	dprintf("!");

	/* Tell card not to interrupt any more. */
	rl2_enable(sc, 0);

	if (cold)
		/* During autoconfig - must handle interrupts now. */
		rl2softintr(sc);
	else
		/* Handle later. */
		timeout(rl2softintr, sc, 1);

	return (1);
}

/* Process earlier card interrupt at splnetintr. */
static void
rl2softintr(arg)
	void * arg;
{
	struct rl2_softc *sc = (struct rl2_softc *)arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int len;
	u_int8_t w;
	struct rl2_mm_cmd hdr;

	dprintf(" si(");

    again:
	/* Save wakeup state. */
	w = rl2_wakeup(sc, RL2_WAKEUP_SET);

	if ((len = rl2_rx_request(sc, 300)) < 0) {
		/* Error in transfer. */
		rl2_need_reset(sc);
		rl2_rx_end(sc);
	} else if (len < sizeof hdr) {
		/* Short message. */
		rl2_rx_end(sc);
		printf("%s: short msg (%d)\n", sc->sc_dev.dv_xname, len);
		ifp->if_ierrors++;
	} else {
		/* Valid message: read header and process. */
		rl2_rx_data(sc, &hdr, sizeof hdr);
		rl2read(sc, &hdr, len);
	}

	/* Ensure that wakeup state is unchanged if transmitting. */
	if (ifp->if_flags & IFF_OACTIVE)
		w |= RL2_WAKEUP_NOCHANGE;
	rl2_wakeup(sc, w);

	/* Check for more interrupts. */
	if (rl2_status_rx_ready(sc)) {
#ifdef DIAGNOSTIC
		printf("%s: intr piggyback\n", sc->sc_dev.dv_xname);
#endif
		goto again;
	}

	/* Some cards need this? */
	rl2_eoi(sc);

	/* Re-enable card. */
	rl2_enable(sc, 1);

	dprintf(")");
}

/* Read and process a message from the card. */
void
rl2read(sc, hdr, len)
	struct rl2_softc *sc;
	struct rl2_mm_cmd *hdr;
	int len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	struct ether_header *eh;
	u_int8_t data[1538];
	u_int8_t  *buf;
	size_t	  buflen;
	struct rl2_pdata pd = RL2_PDATA_INIT;
	struct rl2_mm_synchronised * syncp = (struct rl2_mm_synchronised *)data;
	int s;

	dprintf(" [read]");

	/* Were we waiting for this message? */
	if (rl2_mbox_lock(sc, hdr->cmd_seq, (void **)&buf, &buflen) == 0) {
#ifdef DIAGNOSTIC
		if (buflen < sizeof *hdr)
			panic("rl2read buflen");
#endif
		bcopy(hdr, buf, sizeof *hdr);
		buf += sizeof *hdr;
		len -= sizeof *hdr;
		buflen -= sizeof *hdr;
		if (len) {
			if (len == buflen)		/* Expected size */
				rl2_rx_pdata(sc, buf, len, &pd);
			else if (len < buflen) {	/* Underfill */
#ifdef DIAGNOSTIC
				printf("%s: underfill %d<%d, cmd %c%d\n",
					sc->sc_dev.dv_xname,
					len, buflen,
					hdr->cmd_letter, hdr->cmd_fn);
#endif
				rl2_rx_pdata(sc, buf, len, &pd);
			} else {			/* Overflow */
#ifdef DIAGNOSTIC
				printf("%s: overflow %d>%d, cmd %c%d\n",
					sc->sc_dev.dv_xname,
					len, buflen,
					hdr->cmd_letter, hdr->cmd_fn);
#endif
				rl2_rx_pdata(sc, buf, buflen, &pd);
				/* Drain the rest somewhere. */
				rl2_rx_pdata(sc, data, len - buflen, &pd);
			}
		}
		rl2_rx_end(sc);

		/* This message can now be handled by the waiter. */
		rl2_mbox_unlock(sc, hdr->cmd_seq, len + sizeof *hdr);
		return;
	} 

	/* Otherwise, handle the message, right here, right now. */

	/* Check if we can cope with the size of this message. */
	if (len > sizeof data) {
		printf("%s: msg too big (%d)\n", sc->sc_dev.dv_xname, len);
		ifp->if_ierrors++;
		rl2_rx_end(sc);
		/* rl2_need_reset(sc); */
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
		rl2_rx_end(sc);
		rl2_need_reset(sc);
		return;
	}

	/*
	 * "b1": Receiving a packet is a special case.
	 * We wish to read the data with pio straight into an 
	 * mbuf to avoid a memory-memory copy.
	 */
	if (hdr->cmd_letter == 'b' && hdr->cmd_fn == 1) {
		m = rl2get(sc, hdr, len);
		rl2_rx_end(sc);
		if (m == NULL) 
			return;
		ifp->if_ipackets++;
#ifdef DIAGNOSTIC
		if (bcmp(mtod(m, u_int8_t *), "prox", 4) == 0) {
			printf("%s: proxim special packet received\n",
			    sc->sc_dev.dv_xname);
		}
#endif
		/* XXX Jean's driver dealt with RFC893 trailers here */
		eh = mtod(m, struct ether_header *);
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		m_adj(m, sizeof (struct ether_header));
		ether_input(ifp, eh, m);
		return;
	}


	/* Otherwise we read the packet into a buffer on the stack. */
	bcopy(hdr, data, sizeof *hdr);
	if (len > sizeof *hdr) 
		rl2_rx_pdata(sc, data + sizeof *hdr, len - sizeof *hdr, &pd);
	rl2_rx_end(sc);

#ifdef RL2DUMP
	printf("%s: recv %c%d seq %d data ", sc->sc_dev.dv_xname,
	    hdr->cmd_letter, hdr->cmd_fn, hdr->cmd_seq);
	RL2DUMPHEX(hdr, sizeof hdr);
	printf(":");
	RL2DUMPHEX(data + sizeof hdr, len - sizeof hdr);
	printf("\n");
#endif

	switch (RL2_MM_CMD(hdr->cmd_letter, hdr->cmd_fn)) {
	case RL2_MM_CMD('b', 0):			/* b0: Transmit done. */
#ifdef DIAGNOSTIC
		if (len != 7)
			printf("%s: 'b0' len %d != 7\n",
			    sc->sc_dev.dv_xname, len);
#endif
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_opackets++;
		s = splnet();
		rl2start(ifp);
		splx(s);
		break;

	case RL2_MM_CMD('a', 20):			/* a20: Card fault. */
		printf("%s: hardware fault\n", sc->sc_dev.dv_xname);
		break;

	case RL2_MM_CMD('a', 4):			/* a4: Sync'd. */
		if (bcmp(syncp->enaddr, sc->sc_arpcom.ac_enaddr,
		    ETHER_ADDR_LEN) == 0) {
			/* Sync'd to own enaddr. */
 /*
  * From http://www.proxim.com/support/faq/7400.shtml
  * 3. RL2SETUP reports that I'm synchronized to my own MAC address. What
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
		sc->sc_state |= RL2_STATE_SYNC;

		/* Resume sending. */
		s = splnet();
		rl2start(ifp);
		splx(s);
		break;

	case RL2_MM_CMD('a', 5):			/* a4: Lost sync. */
		printf("%s: lost sync\n", sc->sc_dev.dv_xname);
		sc->sc_state &= ~RL2_STATE_SYNC;
		break;

	case RL2_MM_CMD('a', 18):			/* a18: Roaming. */
		printf("%s: roaming\n", sc->sc_dev.dv_xname);
		break;
	default:
#ifdef DIAGNOSTIC
		printf("%s: msg `%c%d' seq %d data {",
		    sc->sc_dev.dv_xname, 
		    hdr->cmd_letter, hdr->cmd_fn, hdr->cmd_seq);
		RL2DUMPHEX(hdr, sizeof hdr);
		printf(":");
		RL2DUMPHEX(data, len);
		printf("}\n");
#endif
		break;
	}

}

/* Extract a received network packet from the card. */
static struct mbuf *
rl2get(sc, hdr, totlen)
	struct rl2_softc *sc;
	struct rl2_mm_cmd *hdr;
	int totlen;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int len;
	struct mbuf *m, **mp, *top;
	struct rl2_pdata pd = RL2_PDATA_INIT;
	u_int8_t  hwhdr[20];

	dprintf(" [get]");

#ifdef RL2DUMP
	printf("%s: recv %c%d seq %d data ", sc->sc_dev.dv_xname,
	    hdr->cmd_letter, hdr->cmd_fn, hdr->cmd_seq);
	RL2DUMPHEX(hdr, sizeof hdr);
	printf(":");
#endif

	totlen -= sizeof *hdr;
#ifdef DIAGNOSTIC
	if (totlen <= 0) {
		printf("%s: empty packet", sc->sc_dev.dv_xname);
		goto drop;
	}
#endif

	totlen -= sizeof hwhdr;
	/* Skip the hardware header. */
	rl2_rx_pdata(sc, hwhdr, sizeof hwhdr, &pd);
#ifdef RL2DUMP
	RL2DUMPHEX(hwhdr, sizeof hwhdr);
	printf("/");
#endif
	/* (Most of the following code fleeced from elink3.c.) */

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto drop;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (!m)	 {
				m_freem(top);
				goto drop;
			}
			len = MLEN;
		}
		if (top && totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		len = min(totlen, len);
		rl2_rx_pdata(sc, mtod(m, u_int8_t *), len, &pd);
#ifdef RL2DUMP
		RL2DUMPHEX(mtod(m, u_int8_t *), len);
		if (totlen != len)
			printf("|");
#endif
		m->m_len = len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}
#ifdef RL2DUMP
	printf("\n");
#endif
	return m;

drop:
#ifdef RL2DUMP
	printf(": drop\n");
#endif
	ifp->if_iqdrops++;
	return NULL;
}

/* Interface control. */
static int
rl2ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct rl2_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error;
	int need_init;

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
			rl2init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			rl2init(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		need_init = 0;

		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/* Was running, want down: stop. */
			rl2stop(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/* Was not running, want up: start. */
			need_init = 1;
		} 

		if (ifp->if_flags & IFF_RUNNING) {
			if ((ifp->if_flags & IFF_PROMISC) &&
			    (sc->sc_state & RL2_STATE_PROMISC) == 0) {
				sc->sc_state |= RL2_STATE_PROMISC;
				need_init = 1;
			}
			else if ((ifp->if_flags & IFF_PROMISC) == 0 &&
			    (sc->sc_state & RL2_STATE_PROMISC)) {
				sc->sc_state &= ~RL2_STATE_PROMISC;
				need_init = 1;
			}
		}

		/* XXX Deal with other flag changes? */

		if (need_init)
			rl2init(sc);

		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = EOPNOTSUPP;
		break;

#if notyet
	case RL2IOSPARAM:
		error = rl2_iosetparam(sc, (struct rl2_param *)&data);
		break;

	case RL2IOGPARAM:
		bcopy(&sc->sc_param, (struct rl2_param *)&data, 
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
static void
rl2stop(sc)
	struct rl2_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	dprintf(" [stop]");
	/* XXX Should kill interrupts? */
	/* rl2_enable(sc, 0); */
	ifp->if_flags &= ~IFF_RUNNING;
}

/* Test for existence of card. */
static int
rl2_probe(sc)
	struct rl2_softc *sc;
{

	dprintf(" [probe]");
	/* If we can reset it, it's there. */
	return (rl2_reset(sc));
}

/* Get MAC address from card. */
static int
rl2_getenaddr(sc, enaddr)
	struct rl2_softc *sc;
	u_int8_t * enaddr;
{
	struct rl2_mm_cmd query = RL2_MM_GETENADDR;
	struct rl2_mm_gotenaddr response = { RL2_MM_GETENADDR };

	if (rl2_msg_txrx(sc, &query, sizeof query,
	    &response, sizeof response))
		return (-1);
	bcopy(response.enaddr, enaddr, sizeof response.enaddr);
	return (0);
};

/* Get firmware version string from card. */
static int
rl2_getpromvers(sc, ver, verlen)
	struct rl2_softc *sc;
	char *ver;
	int verlen;
{
	struct rl2_mm_cmd query = RL2_MM_GETPROMVERSION;
	struct rl2_mm_gotpromversion response = { RL2_MM_GOTPROMVERSION };
	int i;

#ifdef DIAGNOSTIC
	if (verlen != sizeof response.version)
		panic("rl2_getpromvers");
#endif

	if (rl2_msg_txrx(sc, &query, sizeof query,
	    &response, sizeof response))
		return (-1);
	bcopy(response.version, ver, verlen);
	/* Nul trailing spaces. */
	for (i = verlen - 1; i >= 0 && ver[i] <= ' '; i--)
		ver[i] = '\0';
	return (0);
};

/* Set default operational parameters on card. */
static int
rl2_sendinit(sc)
	struct rl2_softc *sc;
{
	struct rl2_mm_init init = { RL2_MM_INIT };
	struct rl2_mm_initted iresponse;
#if 0
	struct rl2_mm_setmagic magic = { RL2_MM_SETMAGIC };
	struct rl2_mm_disablehopping hop = { RL2_MM_DISABLEHOPPING };
	struct rl2_mm_cmd response;
#endif

	bzero((char*)&init + sizeof init.mm_cmd,
		sizeof init - sizeof init.mm_cmd);

	dprintf(" [setting parameters]");
	init.opmode = (sc->sc_state & RL2_STATE_PROMISC ?
	    RL2_MM_INIT_OPMODE_PROMISC : RL2_MM_INIT_OPMODE_NORMAL);
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

	if (rl2_msg_txrx(sc, &init, sizeof init,
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
	if (rl2_msg_txrx(sc, &magic, sizeof magic,
	    &response, sizeof response))
		return (-1);

	dprintf(" [disabling freq hopping]");
	hop.hopflag = RL2_MM_DISABLEHOPPING_HOPFLAG_DISABLE;
	if (rl2_msg_txrx(sc, &hop, sizeof hop,
	    &response, sizeof response))
		return (-1);

#endif
	return (0);
}

#if notyet
/* Configure the way the card leaves a basestation. */
static int
rl2_roamconfig(sc)
	struct rl2_softc *sc;
{
	struct rl2_mm_setroaming roam = { RL2_MM_SETROAMING };
	struct rl2_mm_cmd response;
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
	if (rl2_msg_txrx(sc, &roam, sizeof roam,
	    &response, sizeof response))
		return (-1);

	return (0);
}

/* Enable roaming. */
static int
rl2_roam(sc)
	struct rl2_softc *sc;
{
	struct rl2_mm_cmd roam = RL2_MM_ROAM;
	struct rl2_mm_cmd response;

	return (rl2_msg_txrx(sc, &roam, sizeof roam,
	    &response, sizeof response));
}

/* Enable multicast capability. */
static int
rl2_multicast(sc, enable)
	struct rl2_softc *sc;
	int enable;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct rl2_mm_multicast mcast = { RL2_MM_MULTICAST };
	struct rl2_mm_cmd response;
	int ret;

	mcast.enable = enable;

	ret = rl2_msg_txrx(sc, &mcast, sizeof mcast,
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
static int
rl2_searchsync(sc)
	struct rl2_softc *sc;
{
	struct rl2_mm_search search = { RL2_MM_SEARCH };
	struct rl2_mm_searching response;

	bzero(search.xxx1, sizeof search.xxx1);
	search.domain = sc->sc_param.rp_domain;
	search.roaming = 1;
	search.xxx3 = 0;
	search.xxx4 = 1;
	search.xxx5 = 0;
	bzero(search.xxx6, sizeof search.xxx6);

	return (rl2_msg_txrx(sc, &search, sizeof search,
		&response, sizeof response));
}

/* Set values from an external parameter block. */
static int
rl2_iosetparam(sc, param)
	struct rl2_softc *sc;
	struct rl2_param *param;
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
		if (rl2_sendinit(sc))
			error = EIO;
	}
	return (error);
}

/* Protect the eeprom from storing a security ID(?) */
static int
rl2_lockprom(sc)
	struct rl2_softc *sc;
{
	struct rl2_mm_cmd lock = RL2_MM_EEPROM_PROTECT;
	struct rl2_mm_cmd response;

	/* XXX Always yields an error? */
	return (rl2_msg_txrx(sc, &lock, sizeof lock,
	    &response, sizeof response));
}

/* Set the h/w Inactivity Time Out timer on the card. */
static int
rl2_ito(sc)
	struct rl2_softc * sc;
{
	struct rl2_mm_setito ito = { RL2_MM_MULTICAST };
	struct rl2_mm_cmd response;

	ito.xxx = 3;
	ito.timeout = LLDInactivityTimeOut /* enabler, 0 or 1 */;
	ito.bd_wakeup = LLDBDWakeup /* 0 */;
	ito.pm_sync = LLDPMSync /* 0 */;
	ito.sniff_time = ito.timeout ? LLDSniffTime /* 0 */ : 0;

	if (rl2_msg_txrx(sc, &ito, sizeof ito,
	    &response, sizeof response))
		return (-1);
}

/* Put the card into standby mode. */
static int
rl2_standby(sc)
	struct rl2_softc * sc;
{
	struct rl2_mm_standby standby = { RL2_MM_STANDBY };

	standby.xxx = 0;
	if (rl2_msg_txrx(sc, &ito, sizeof ito, NULL, 0))
		return (-1);
}
#endif
