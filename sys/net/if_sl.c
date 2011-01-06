/*	$OpenBSD: if_sl.c,v 1.43 2011/01/06 11:52:41 claudio Exp $	*/
/*	$NetBSD: if_sl.c,v 1.39.4.1 1996/06/02 16:26:31 thorpej Exp $	*/

/*
 * Copyright (c) 1987, 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)if_sl.c	8.6 (Berkeley) 2/1/94
 */

/*
 * Serial Line interface
 *
 * Rick Adams
 * Center for Seismic Studies
 * 1300 N 17th Street, Suite 1450
 * Arlington, Virginia 22209
 * (703)276-7900
 * rick@seismo.ARPA
 * seismo!rick
 *
 * Pounded on heavily by Chris Torek (chris@mimsy.umd.edu, umcp-cs!chris).
 * N.B.: this belongs in netinet, not net, the way it stands now.
 * Should have a link-layer type designation, but wouldn't be
 * backwards-compatible.
 *
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 * W. Jolitz added slip abort.
 *
 * Hacked almost beyond recognition by Van Jacobson (van@helios.ee.lbl.gov).
 * Added priority queuing for "interactive" traffic; hooks for TCP
 * header compression; ICMP filtering (at 2400 baud, some cretin
 * pinging you can use up all your bandwidth).  Made low clist behavior
 * more robust and slightly less likely to hang serial line.
 * Sped up a bunch of things.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/systm.h>
#endif

#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#else
#error Huh? Slip without inet?
#endif

#include <net/slcompress.h>
#include <net/if_slvar.h>
#include <net/slip.h>

#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

/*
 * SLMAX is a hard limit on input packet size.  To simplify the code
 * and improve performance, we require that packets fit in an mbuf
 * cluster, and if we get a compressed packet, there's enough extra
 * room to expand the header into a max length tcp/ip header (128
 * bytes).  So, SLMAX can be at most
 *	MCLBYTES - 128
 *
 * SLMTU is a hard limit on output packet size.  To insure good
 * interactive response, SLMTU wants to be the smallest size that
 * amortizes the header cost.  (Remember that even with
 * type-of-service queuing, we have to wait for any in-progress
 * packet to finish.  I.e., we wait, on the average, 1/2 * mtu /
 * cps, where cps is the line speed in characters per second.
 * E.g., 533ms wait for a 1024 byte MTU on a 9600 baud line.  The
 * average compressed header size is 6-8 bytes so any MTU > 90
 * bytes will give us 90% of the line bandwidth.  A 100ms wait is
 * tolerable (500ms is not), so want an MTU around 296.  (Since TCP
 * will send 256 byte segments (to allow for 40 byte headers), the
 * typical packet size on the wire will be around 260 bytes).  In
 * 4.3tahoe+ systems, we can set an MTU in a route so we do that &
 * leave the interface MTU relatively high (so we don't IP fragment
 * when acting as a gateway to someone using a stupid MTU).
 *
 * Similar considerations apply to SLIP_HIWAT:  It's the amount of
 * data that will be queued 'downstream' of us (i.e., in clists
 * waiting to be picked up by the tty output interrupt).  If we
 * queue a lot of data downstream, it's immune to our t.o.s. queuing.
 * E.g., if SLIP_HIWAT is 1024, the interactive traffic in mixed
 * telnet/ftp will see a 1 sec wait, independent of the mtu (the
 * wait is dependent on the ftp window size but that's typically
 * 1k - 4k).  So, we want SLIP_HIWAT just big enough to amortize
 * the cost (in idle time on the wire) of the tty driver running
 * off the end of its clists & having to call back slstart for a
 * new packet.  For a tty interface with any buffering at all, this
 * cost will be zero.  Even with a totally brain dead interface (like
 * the one on a typical workstation), the cost will be <= 1 character
 * time.  So, setting SLIP_HIWAT to ~100 guarantees that we'll lose
 * at most 1% while maintaining good interactive response.
 */
#if NBPFILTER > 0
#define	BUFOFFSET	(128+sizeof(struct ifnet **)+SLIP_HDRLEN)
#else
#define	BUFOFFSET	(128+sizeof(struct ifnet **))
#endif
#define	SLMAX		(MCLBYTES - BUFOFFSET)
#define	SLBUFSIZE	(SLMAX + BUFOFFSET)
#ifndef SLMTU
#define	SLMTU		296
#endif
#if (SLMTU < 3)
#error Huh?  SLMTU way too small.
#endif
#define	SLIP_HIWAT	roundup(50,CBSIZE)
#if !(defined(__NetBSD__) || defined(__OpenBSD__))		/* XXX - cgd */
#define	CLISTRESERVE	1024	/* Can't let clists get too low */
#endif	/* !NetBSD */

/*
 * SLIP ABORT ESCAPE MECHANISM:
 *	(inspired by HAYES modem escape arrangement)
 *	1sec escape 1sec escape 1sec escape { 1sec escape 1sec escape }
 *	within window time signals a "soft" exit from slip mode by remote end
 *	if the IFF_DEBUG flag is on.
 */
#define	ABT_ESC		'\033'	/* can't be t_intr - distant host must know it*/
#define	ABT_IDLE	1	/* in seconds - idle before an escape */
#define	ABT_COUNT	3	/* count of escapes for abort */
#define	ABT_WINDOW	(ABT_COUNT*2+2)	/* in seconds - time to count */


#define FRAME_END	 	0xc0		/* Frame End */
#define FRAME_ESCAPE		0xdb		/* Frame Esc */
#define TRANS_FRAME_END	 	0xdc		/* transposed frame end */
#define TRANS_FRAME_ESCAPE 	0xdd		/* transposed frame esc */

static int slinit(struct sl_softc *);
static struct mbuf *sl_btom(struct sl_softc *, int);

int	sl_clone_create(struct if_clone *, int);
int	sl_clone_destroy(struct ifnet *);

LIST_HEAD(, sl_softc) sl_softc_list;
struct if_clone sl_cloner =
    IF_CLONE_INITIALIZER("sl", sl_clone_create, sl_clone_destroy);

/*
 * Called from boot code to establish sl interfaces.
 */
void
slattach(n)
	int n;
{
	LIST_INIT(&sl_softc_list);
	if_clone_attach(&sl_cloner);
}

int
sl_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct sl_softc *sc;
	int s;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!sc)
		return (ENOMEM);

	sc->sc_unit = unit;	/* XXX */
	snprintf(sc->sc_if.if_xname, sizeof sc->sc_if.if_xname, "%s%d",
	    ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_mtu = SLMTU;
	sc->sc_if.if_flags =
	    IFF_POINTOPOINT | SC_AUTOCOMP | IFF_MULTICAST;
	sc->sc_if.if_type = IFT_SLIP;
	sc->sc_if.if_ioctl = slioctl;
	sc->sc_if.if_output = sloutput;
	IFQ_SET_MAXLEN(&sc->sc_if.if_snd, 50);
	sc->sc_fastq.ifq_maxlen = 32;
	IFQ_SET_READY(&sc->sc_if.if_snd);
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
#if NBPFILTER > 0
	bpfattach(&sc->sc_bpf, &sc->sc_if, DLT_SLIP, SLIP_HDRLEN);
#endif
	s = splnet();
	LIST_INSERT_HEAD(&sl_softc_list, sc, sc_list);
	splx(s);

	return (0);
}

int
sl_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct sl_softc *sc = ifp->if_softc;
	int s;

	if (sc->sc_ttyp != NULL)
		return (EBUSY);

	s = splnet();
	LIST_REMOVE(sc, sc_list);
	splx(s);

	if_detach(ifp);

	free(sc, M_DEVBUF);
	return (0);
}

static int
slinit(sc)
	struct sl_softc *sc;
{
	if (sc->sc_ep == (u_char *) 0) {
		MGETHDR(sc->sc_mbuf, M_WAIT, MT_DATA);
		if (sc->sc_mbuf)
			MCLGET(sc->sc_mbuf, M_WAIT);
		if (sc->sc_mbuf == NULL || sc->sc_mbuf->m_ext.ext_buf == NULL) {
			printf("sl%d: can't allocate buffer\n", sc->sc_unit);
			sc->sc_if.if_flags &= ~IFF_UP;
			return (0);
		}
	}
	sc->sc_ep = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
	    sc->sc_mbuf->m_ext.ext_size;
	sc->sc_mp = sc->sc_pktstart = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
	    BUFOFFSET;

	sl_compress_init(&sc->sc_comp);

	return (1);
}

/*
 * Line specific open routine.
 * Attach the given tty to the first available sl unit.
 */
/* ARGSUSED */
int
slopen(dev, tp)
	dev_t dev;
	struct tty *tp;
{
	struct proc *p = curproc;		/* XXX */
	struct sl_softc *sc;
	int error, s;

	if ((error = suser(p, 0)) != 0)
		return (error);

	if (tp->t_line == SLIPDISC)
		return (0);

	LIST_FOREACH(sc, &sl_softc_list, sc_list)
		if (sc->sc_ttyp == NULL) {
			if (slinit(sc) == 0)
				return (ENOBUFS);
			tp->t_sc = (caddr_t)sc;
			sc->sc_ttyp = tp;
			sc->sc_if.if_baudrate = tp->t_ospeed;
			s = spltty();
			tp->t_state |= TS_ISOPEN | TS_XCLUDE;
			splx(s);
			ttyflush(tp, FREAD | FWRITE);
#if defined(__NetBSD__) || defined(__OpenBSD__)
			/*
			 * make sure tty output queue is large enough
			 * to hold a full-sized packet (including frame
			 * end, and a possible extra frame end).  full-sized
			 * packet occupies a max of 2*SLMTU bytes (because
			 * of possible escapes), and add two on for frame
			 * ends.
			 */
			s = spltty();
			if (tp->t_outq.c_cn < 2*SLMTU+2) {
				sc->sc_oldbufsize = tp->t_outq.c_cn;
				sc->sc_oldbufquot = tp->t_outq.c_cq != 0;

				clfree(&tp->t_outq);
				clalloc(&tp->t_outq, 3*SLMTU, 0);
			} else
				sc->sc_oldbufsize = sc->sc_oldbufquot = 0;
			splx(s);
#endif /* NetBSD */
			return (0);
		}
	return (ENXIO);
}

/*
 * Line specific close routine.
 * Detach the tty from the sl unit.
 */
void
slclose(tp)
	struct tty *tp;
{
	struct sl_softc *sc;
	int s;

	ttywflush(tp);
	tp->t_line = 0;
	sc = (struct sl_softc *)tp->t_sc;
	if (sc != NULL) {
		s = spltty();

		if_down(&sc->sc_if);
		sc->sc_ttyp = NULL;
		tp->t_sc = NULL;

		m_freem(sc->sc_mbuf);
		sc->sc_mbuf = NULL;
		sc->sc_ep = sc->sc_mp = sc->sc_pktstart = NULL;

#if defined(__NetBSD__) || defined(__OpenBSD__)
		/* if necessary, install a new outq buffer of the appropriate size */
		if (sc->sc_oldbufsize != 0) {
			clfree(&tp->t_outq);
			clalloc(&tp->t_outq, sc->sc_oldbufsize, sc->sc_oldbufquot);
		}
#endif
		splx(s);
	}
}

/*
 * Line specific (tty) ioctl routine.
 * Provide a way to get the sl unit number.
 */
/* ARGSUSED */
int
sltioctl(tp, cmd, data, flag)
	struct tty *tp;
	u_long cmd;
	caddr_t data;
	int flag;
{
	struct sl_softc *sc = (struct sl_softc *)tp->t_sc;

	switch (cmd) {
	case SLIOCGUNIT:
		*(int *)data = sc->sc_unit;	/* XXX */
		break;

	default:
		return (-1);
	}
	return (0);
}

/*
 * Queue a packet.  Start transmission if not active.
 * Compression happens in slstart; if we do it here, IP TOS
 * will cause us to not compress "background" packets, because
 * ordering gets trashed.  It can be done for all packets in slstart.
 */
int
sloutput(ifp, m, dst, rtp)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rtp;
{
	struct sl_softc *sc = ifp->if_softc;
	struct ip *ip;
	int s, error;

	/*
	 * `Cannot happen' (see slioctl).  Someday we will extend
	 * the line protocol to support other address families.
	 */
	if (dst->sa_family != AF_INET) {
		printf("%s: af%d not supported\n", sc->sc_if.if_xname,
			dst->sa_family);
		m_freem(m);
		sc->sc_if.if_noproto++;
		return (EAFNOSUPPORT);
	}

#ifdef DIAGNOSTIC
	if (ifp->if_rdomain != rtable_l2(m->m_pkthdr.rdomain)) {
		printf("%s: trying to send packet on wrong domain. "
		    "if %d vs. mbuf %d, AF %d\n", ifp->if_xname,
		    ifp->if_rdomain, rtable_l2(m->m_pkthdr.rdomain),
		    dst->sa_family);
	}
#endif

	if (sc->sc_ttyp == NULL) {
		m_freem(m);
		return (ENETDOWN);	/* sort of */
	}
	if ((sc->sc_ttyp->t_state & TS_CARR_ON) == 0 &&
	    (sc->sc_ttyp->t_cflag & CLOCAL) == 0) {
		m_freem(m);
		return (EHOSTUNREACH);
	}
	ip = mtod(m, struct ip *);
	if (sc->sc_if.if_flags & SC_NOICMP && ip->ip_p == IPPROTO_ICMP) {
		m_freem(m);
		return (ENETRESET);		/* XXX ? */
	}
	s = spltty();
	if (sc->sc_oqlen && sc->sc_ttyp->t_outq.c_cc == sc->sc_oqlen) {
		struct timeval tv, tm;

		getmicrotime(&tm);
		/* if output's been stalled for too long, and restart */
		timersub(&tm, &sc->sc_lastpacket, &tv);
		if (tv.tv_sec > 0) {
			sc->sc_otimeout++;
			slstart(sc->sc_ttyp);
		}
	}

	(void) splnet();
	IFQ_ENQUEUE(&sc->sc_if.if_snd, m, NULL, error);
	if (error) {
		splx(s);
		sc->sc_if.if_oerrors++;
		return (error);
	}

	(void) spltty();
	getmicrotime(&sc->sc_lastpacket);
	if ((sc->sc_oqlen = sc->sc_ttyp->t_outq.c_cc) == 0)
		slstart(sc->sc_ttyp);
	splx(s);
	return (0);
}

/*
 * Start output on interface.  Get another datagram
 * to send from the interface queue and map it to
 * the interface before starting output.
 */
void
slstart(tp)
	struct tty *tp;
{
	struct sl_softc *sc = (struct sl_softc *)tp->t_sc;
	struct mbuf *m;
	u_char *cp;
	struct ip *ip;
	int s;
	struct mbuf *m2;
#if NBPFILTER > 0
	u_char bpfbuf[SLMTU + SLIP_HDRLEN];
	int len = 0;
#endif
#if !(defined(__NetBSD__) || defined(__OpenBSD__))	/* XXX - cgd */
	extern int cfreecount;
#endif

	for (;;) {
		/*
		 * If there is more in the output queue, just send it now.
		 * We are being called in lieu of ttstart and must do what
		 * it would.
		 */
		if (tp->t_outq.c_cc != 0) {
			(*tp->t_oproc)(tp);
			if (tp->t_outq.c_cc > SLIP_HIWAT)
				return;
		}
		/*
		 * This happens briefly when the line shuts down.
		 */
		if (sc == NULL)
			return;

#if defined(__NetBSD__) || defined(__OpenBSD__)		/* XXX - cgd */
		/*
		 * Do not remove the packet from the IP queue if it
		 * doesn't look like the packet will fit into the
		 * current serial output queue, with a packet full of
		 * escapes this could be as bad as SLMTU*2+2.
		 */
		if (tp->t_outq.c_cn - tp->t_outq.c_cc < 2*SLMTU+2)
			return;
#endif /* NetBSD */

		/*
		 * Get a packet and send it to the interface.
		 */
		s = splnet();
		IF_DEQUEUE(&sc->sc_fastq, m);
		if (m)
			sc->sc_if.if_omcasts++;		/* XXX */
		else
			IFQ_DEQUEUE(&sc->sc_if.if_snd, m);
		splx(s);
		if (m == NULL)
			return;

		/*
		 * We do the header compression here rather than in sloutput
		 * because the packets will be out of order if we are using TOS
		 * queueing, and the connection id compression will get
		 * munged when this happens.
		 */
#if NBPFILTER > 0
		if (sc->sc_bpf) {
			/*
			 * We need to save the TCP/IP header before it's
			 * compressed.  To avoid complicated code, we just
			 * copy the entire packet into a stack buffer (since
			 * this is a serial line, packets should be short
			 * and/or the copy should be negligible cost compared
			 * to the packet transmission time).
			 */
			struct mbuf *m1 = m;

			cp = bpfbuf + SLIP_HDRLEN;
			len = 0;
			do {
				int mlen = m1->m_len;

				bcopy(mtod(m1, caddr_t), cp, mlen);
				cp += mlen;
				len += mlen;
			} while ((m1 = m1->m_next) != NULL);
		}
#endif
		if ((ip = mtod(m, struct ip *))->ip_p == IPPROTO_TCP) {
			if (sc->sc_if.if_flags & SC_COMPRESS)
				*mtod(m, u_char *) |= sl_compress_tcp(m, ip,
				    &sc->sc_comp, 1);
		}
#if NBPFILTER > 0
		if (sc->sc_bpf) {
			/*
			 * Put the SLIP pseudo-"link header" in place.  The
			 * compressed header is now at the beginning of the
			 * mbuf.
			 */
			bpfbuf[SLX_DIR] = SLIPDIR_OUT;
			bcopy(mtod(m, caddr_t), &bpfbuf[SLX_CHDR], CHDR_LEN);
			bpf_tap(sc->sc_bpf, bpfbuf, len + SLIP_HDRLEN,
			    BPF_DIRECTION_OUT);
		}
#endif
		getmicrotime(&sc->sc_lastpacket);

#if !(defined(__NetBSD__) || defined(__OpenBSD__))		/* XXX - cgd */
		/*
		 * If system is getting low on clists, just flush our
		 * output queue (if the stuff was important, it'll get
		 * retransmitted).
		 */
		if (cfreecount < CLISTRESERVE + SLMTU) {
			m_freem(m);
			sc->sc_if.if_collisions++;
			continue;
		}
#endif /* !__NetBSD__ */
		/*
		 * The extra FRAME_END will start up a new packet, and thus
		 * will flush any accumulated garbage.  We do this whenever
		 * the line may have been idle for some time.
		 */
		if (tp->t_outq.c_cc == 0) {
			++sc->sc_if.if_obytes;
			(void) putc(FRAME_END, &tp->t_outq);
		}

		while (m) {
			u_char *ep;

			cp = mtod(m, u_char *); ep = cp + m->m_len;
			while (cp < ep) {
				/*
				 * Find out how many bytes in the string we can
				 * handle without doing something special.
				 */
				u_char *bp = cp;

				while (cp < ep) {
					switch (*cp++) {
					case FRAME_ESCAPE:
					case FRAME_END:
						--cp;
						goto out;
					}
				}
				out:
				if (cp > bp) {
					/*
					 * Put n characters at once
					 * into the tty output queue.
					 */
#if defined(__NetBSD__) || defined(__OpenBSD__)		/* XXX - cgd */
					if (b_to_q((u_char *)bp, cp - bp,
#else
					if (b_to_q((char *)bp, cp - bp,
#endif
					    &tp->t_outq))
						break;
					sc->sc_if.if_obytes += cp - bp;
				}
				/*
				 * If there are characters left in the mbuf,
				 * the first one must be special..
				 * Put it out in a different form.
				 */
				if (cp < ep) {
					if (putc(FRAME_ESCAPE, &tp->t_outq))
						break;
					if (putc(*cp++ == FRAME_ESCAPE ?
					   TRANS_FRAME_ESCAPE : TRANS_FRAME_END,
					   &tp->t_outq)) {
						(void) unputc(&tp->t_outq);
						break;
					}
					sc->sc_if.if_obytes += 2;
				}
			}
			MFREE(m, m2);
			m = m2;
		}

		if (putc(FRAME_END, &tp->t_outq)) {
			/*
			 * Not enough room.  Remove a char to make room
			 * and end the packet normally.
			 * If you get many collisions (more than one or two
			 * a day) you probably do not have enough clists
			 * and you should increase "nclist" in param.c.
			 */
			(void) unputc(&tp->t_outq);
			(void) putc(FRAME_END, &tp->t_outq);
			sc->sc_if.if_collisions++;
		} else {
			++sc->sc_if.if_obytes;
			sc->sc_if.if_opackets++;
		}
	}
}

/*
 * Copy data buffer to mbuf chain; add ifnet pointer.
 */
static struct mbuf *
sl_btom(sc, len)
	struct sl_softc *sc;
	int len;
{
	struct mbuf *m;

	/*
	 * Allocate a new input buffer and swap.
	 */
	m = sc->sc_mbuf;
	MGETHDR(sc->sc_mbuf, M_DONTWAIT, MT_DATA);
	if (sc->sc_mbuf == NULL) {
		sc->sc_mbuf = m;
		return (NULL);
	}
	MCLGET(sc->sc_mbuf, M_DONTWAIT);
	if ((sc->sc_mbuf->m_flags & M_EXT) == 0) {
		/*
		 * we couldn't get a cluster - if memory's this
		 * low, it's time to start dropping packets.
		 */
		m_freem(sc->sc_mbuf);
		sc->sc_mbuf = m;
		return (NULL);
	}
	sc->sc_ep = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
		sc->sc_mbuf->m_ext.ext_size;
	
	m->m_data = sc->sc_pktstart;

	m->m_len = len;
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = &sc->sc_if;
	return (m);
}

/*
 * tty interface receiver interrupt.
 */
void
slinput(c, tp)
	int c;
	struct tty *tp;
{
	struct sl_softc *sc;
	struct mbuf *m;
	int len;
	int s;
#if NBPFILTER > 0
	u_char chdr[CHDR_LEN];
#endif

	tk_nin++;
	sc = (struct sl_softc *)tp->t_sc;
	if (sc == NULL)
		return;
	if (c & TTY_ERRORMASK || ((tp->t_state & TS_CARR_ON) == 0 &&
	    (tp->t_cflag & CLOCAL) == 0)) {
		sc->sc_flags |= SC_ERROR;
		return;
	}
	c &= TTY_CHARMASK;

	++sc->sc_if.if_ibytes;

	if (sc->sc_if.if_flags & IFF_DEBUG) {
		if (c == ABT_ESC) {
			/*
			 * If we have a previous abort, see whether
			 * this one is within the time limit.
			 */
			if (sc->sc_abortcount &&
			    time_second >= sc->sc_starttime + ABT_WINDOW)
				sc->sc_abortcount = 0;
			/*
			 * If we see an abort after "idle" time, count it;
			 * record when the first abort escape arrived.
			 */
			if (time_second >= sc->sc_lasttime + ABT_IDLE) {
				if (++sc->sc_abortcount == 1)
					sc->sc_starttime = time_second;
				if (sc->sc_abortcount >= ABT_COUNT) {
					slclose(tp);
					return;
				}
			}
		} else
			sc->sc_abortcount = 0;
		sc->sc_lasttime = time_second;
	}

	switch (c) {

	case TRANS_FRAME_ESCAPE:
		if (sc->sc_escape)
			c = FRAME_ESCAPE;
		break;

	case TRANS_FRAME_END:
		if (sc->sc_escape)
			c = FRAME_END;
		break;

	case FRAME_ESCAPE:
		sc->sc_escape = 1;
		return;

	case FRAME_END:
		if(sc->sc_flags & SC_ERROR) {
			sc->sc_flags &= ~SC_ERROR;
			goto newpack;
		}
		len = sc->sc_mp - sc->sc_pktstart;
		if (len < 3)
			/* less than min length packet - ignore */
			goto newpack;

#if NBPFILTER > 0
		if (sc->sc_bpf) {
			/*
			 * Save the compressed header, so we
			 * can tack it on later.  Note that we
			 * will end up copying garbage in some
			 * cases but this is okay.  We remember
			 * where the buffer started so we can
			 * compute the new header length.
			 */
			bcopy(sc->sc_pktstart, chdr, CHDR_LEN);
		}
#endif

		if ((c = (*sc->sc_pktstart & 0xf0)) != (IPVERSION << 4)) {
			if (c & 0x80)
				c = TYPE_COMPRESSED_TCP;
			else if (c == TYPE_UNCOMPRESSED_TCP)
				*sc->sc_pktstart &= 0x4f; /* XXX */
			/*
			 * We've got something that's not an IP packet.
			 * If compression is enabled, try to decompress it.
			 * Otherwise, if `auto-enable' compression is on and
			 * it's a reasonable packet, decompress it and then
			 * enable compression.  Otherwise, drop it.
			 */
			if (sc->sc_if.if_flags & SC_COMPRESS) {
				len = sl_uncompress_tcp(&sc->sc_pktstart, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
			} else if ((sc->sc_if.if_flags & SC_AUTOCOMP) &&
			    c == TYPE_UNCOMPRESSED_TCP && len >= 40) {
				len = sl_uncompress_tcp(&sc->sc_pktstart, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
				sc->sc_if.if_flags |= SC_COMPRESS;
			} else
				goto error;
		}

		m = sl_btom(sc, len);
		if (m == NULL)
			goto error;

		/* mark incoming routing domain */
		m->m_pkthdr.rdomain = sc->sc_if.if_rdomain;

#if NBPFILTER > 0
		if (sc->sc_bpf) {
			/*
			 * Put the SLIP pseudo-"link header" in place.
			 * Note this M_PREPEND() should bever fail,
			 * since we know we always have enough space
			 * in the input buffer.
			 */
			u_char *hp;

			M_PREPEND(m, SLIP_HDRLEN, M_DONTWAIT);
			if (m == NULL)
				goto error;

			hp = mtod(m, u_char *);
			hp[SLX_DIR] = SLIPDIR_IN;
			memcpy(&hp[SLX_CHDR], chdr, CHDR_LEN);

			s = splnet();
			bpf_mtap(sc->sc_bpf, m, BPF_DIRECTION_IN);
			splx(s);

			m_adj(m, SLIP_HDRLEN);
		}
#endif

		sc->sc_if.if_ipackets++;
		getmicrotime(&sc->sc_lastpacket);
		s = splnet();
		if (IF_QFULL(&ipintrq)) {
			IF_DROP(&ipintrq);
			sc->sc_if.if_ierrors++;
			sc->sc_if.if_iqdrops++;
			m_freem(m);
			if (!ipintrq.ifq_congestion)
				if_congestion(&ipintrq);
		} else {
			IF_ENQUEUE(&ipintrq, m);
			schednetisr(NETISR_IP);
		}
		splx(s);
		goto newpack;
	}
	if (sc->sc_mp < sc->sc_ep) {
		*sc->sc_mp++ = c;
		sc->sc_escape = 0;
		return;
	}

	/* can't put lower; would miss an extra frame */
	sc->sc_flags |= SC_ERROR;

error:
	sc->sc_if.if_ierrors++;
newpack:
	sc->sc_mp = sc->sc_pktstart = (u_char *) sc->sc_mbuf->m_ext.ext_buf +
	    BUFOFFSET;
	sc->sc_escape = 0;
}

/*
 * Process an ioctl request.
 */
int
slioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct sl_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr;
	int s = splnet(), error = 0;
	struct sl_stats *slsp;

	switch (cmd) {

	case SIOCSIFADDR:
		if (ifa->ifa_addr->sa_family == AF_INET)
			ifp->if_flags |= IFF_UP;
		else
			error = EAFNOSUPPORT;
		break;

	case SIOCSIFDSTADDR:
		if (ifa->ifa_addr->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {

#ifdef INET
		case AF_INET:
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCGSLSTATS:
		slsp = &((struct ifslstatsreq *) data)->stats;
		bzero(slsp, sizeof(*slsp));
		/* slsp->sl = sc->sc_stats; */
		slsp->sl.sl_ibytes = sc->sc_if.if_ibytes;
		slsp->sl.sl_obytes = sc->sc_if.if_obytes;
		slsp->sl.sl_ipackets = sc->sc_if.if_ipackets;
		slsp->sl.sl_opackets = sc->sc_if.if_opackets;
#ifdef INET
		slsp->vj.vjs_packets = sc->sc_comp.sls_packets;
		slsp->vj.vjs_compressed = sc->sc_comp.sls_compressed;
		slsp->vj.vjs_searches = sc->sc_comp.sls_searches;
		slsp->vj.vjs_misses = sc->sc_comp.sls_misses;
		slsp->vj.vjs_uncompressedin = sc->sc_comp.sls_uncompressedin;
		slsp->vj.vjs_compressedin = sc->sc_comp.sls_compressedin;
		slsp->vj.vjs_errorin = sc->sc_comp.sls_errorin;
		slsp->vj.vjs_tossed = sc->sc_comp.sls_tossed;
#endif /* INET */
		break;

	default:
		error = ENOTTY;
	}
	splx(s);
	return (error);
}
