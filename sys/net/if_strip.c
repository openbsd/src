/*	$NetBSD: if_strip.c,v 1.2.4.1 1996/06/05 23:23:02 thorpej Exp $	*/
/*	from: NetBSD: if_sl.c,v 1.38 1996/02/13 22:00:23 christos Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 *
 * This driver was contributed by Jonathan Stone.
 *
 * Starmode Radio IP interface (STRIP) for Metricom wireless radio.
 * This STRIP driver assumes address resolution of IP addresses to
 * Metricom MAC addresses is done via local link-level routes.
 * The link-level addresses are entered as an 8-digit packed BCD number.
 * To add a route for a radio at IP address 10.1.2.3, with radio
 * address '1234-5678', reachable via interface st0, use the command 
 *
 *	route add -host 10.1.2.3  -link st0:12:34:56:78
 */


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
 *	@(#)if_sl.c	8.6 (Berkeley) 2/1/94
 */

/*
 * Derived from: Serial Line interface written by Rick Adams (rick@seismo.gov)
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
 * 
 * Note that splimp() is used throughout to block both (tty) input
 * interrupts and network activity; thus, splimp must be >= spltty.
 */

#include "strip.h"
#if NSTRIP > 0

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#if __NetBSD__
#include <sys/systm.h>
#endif
#include <sys/syslog.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#if INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#else
#error Starmode Radio IP configured without configuring inet?
#endif

#include <net/slcompress.h>
#include <net/if_stripvar.h>
#include <net/slip.h>

#ifdef __NetBSD__	/* XXX -- jrs */
typedef u_char ttychar_t;
#else
typedef char ttychar_t;
#endif

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
#define SLMTU		1200 /*XXX*/

#define	SLIP_HIWAT	roundup(50,CBSIZE)

#ifndef __NetBSD__					/* XXX - cgd */
#define	CLISTRESERVE	1024	/* Can't let clists get too low */
#endif	/* !__NetBSD__ */

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

struct st_softc st_softc[NSTRIP];

#define STRIP_FRAME_END		0x0D		/* carriage return */


static int stripinit __P((struct st_softc *));
static 	struct mbuf *strip_btom __P((struct st_softc *, int));

/*
 * STRIP header: '*' + modem address (dddd-dddd) + '*' + mactype ('SIP0')
 * A Metricom packet looks like this: *<address>*<key><payload><CR>
 *   eg. *0000-1164*SIP0<payload><CR>
 *
 */

#define STRIP_ENCAP_SIZE(X) ((36) + (X)*65/64 + 2)
#define STRIP_HDRLEN 15
#define STRIP_MAC_ADDR_LEN 9

/*
 * Star mode packet header.
 * (may be used for encapsulations other than STRIP.)
 */
#define STARMODE_ADDR_LEN 11
struct st_header {
	u_char starmode_addr[STARMODE_ADDR_LEN];
	u_char starmode_type[4];
};

/*
 * Forward declarations for Metricom-specific functions.
 * Ideally, these would be in a library and shared across
 * different STRIP implementations: *BSD, Linux, etc.
 *
 */
static u_char* UnStuffData __P((u_char *src, u_char *end, u_char
				*dest, u_long dest_length)); 

static u_char* StuffData __P((u_char *src, u_long length, u_char *dest,
			      u_char **code_ptr_ptr));

static void RecErr __P((char *msg, struct st_softc *sc));
static void RecERR_Message __P((struct st_softc *strip_info,
				u_char *sendername, u_char *msg));
void	resetradio __P((struct st_softc *sc, struct tty *tp));
void	strip_esc __P((struct st_softc *sc, struct mbuf *m));
int	strip_newpacket __P((struct st_softc *sc, u_char *ptr, u_char *end));
struct mbuf * strip_send __P((struct st_softc *sc, struct mbuf *m0));


#ifdef DEBUG
void ipdump __P((const char *msg, u_char *p, int len));
void stripdump __P((const char *msg, u_char *p, int len));
void stripdumpm __P((const char *msg, struct mbuf *m, int len));
#define DPRINTF(x)	printf x
#define TXPRINTF(x)	printf x/* causes outrageous delays */
#define RXPRINTF(x)	printf x /* causes outrageous delays */
#else
#define DPRINTF(x)
#define TXPRINTF(x)
#define RXPRINTF(x)
#endif

#define XDPRINTF(x)	/* really verbose debugging */


/*
 * Radio reset macros.
 * The Metricom radios are not particularly well-designed for
 * use in packet mode (starmode).  There's no easy way to tell
 * when the radio is in starmode.  Worse, when the radios are reset
 * or power-cycled, they come back up in Hayes AT-emulation mode,
 * and there's no good way for this driver to tell.
 * We deal with this by peridically tickling the radio
 * with an invalid starmode command.  If the radio doesn't
 * respond with an error, the driver knows to reset the radio.
 */

#define FORCE_RESET(sc) \
 do {\
    printf("strip: XXX: reset state-machine not yet implemented in *BSD\n"); \
 } while (0)

#define CLEAR_RESET_TIMER(sc) \
 do {\
    printf("strip: clearing  reset timeout: not yet implemented in *BSD\n"); \
 } while (0)



/*
 * Called from boot code to establish sl interfaces.
 */
void
stripattach(n)
	int n;
{
	register struct st_softc *sc;
	register int i = 0;

	for (sc = st_softc; i < NSTRIP; sc++) {
		sc->sc_unit = i;		/* XXX */
		sprintf(sc->sc_if.if_xname, "st%d", i++);
		sc->sc_if.if_softc = sc;
		sc->sc_if.if_mtu = SLMTU;
		sc->sc_if.if_flags = 0;
		sc->sc_if.if_type = IFT_OTHER;
#if 0
		sc->sc_if.if_flags |= SC_AUTOCOMP /* | IFF_POINTOPOINT | IFF_MULTICAST*/;
#endif
		sc->sc_if.if_type = IFT_SLIP;
		sc->sc_if.if_ioctl = stripioctl;
		sc->sc_if.if_output = stripoutput;
		sc->sc_if.if_snd.ifq_maxlen = 50;
		sc->sc_fastq.ifq_maxlen = 32;
		if_attach(&sc->sc_if);
#if NBPFILTER > 0
		bpfattach(&sc->sc_bpf, &sc->sc_if, DLT_SLIP, SLIP_HDRLEN);
#endif
	}
}

static int
stripinit(sc)
	register struct st_softc *sc;
{
	register caddr_t p;

	if (sc->sc_ep == (u_char *) 0) {
		MCLALLOC(p, M_WAIT);
		if (p)
			sc->sc_ep = (u_char *)p + SLBUFSIZE;
		else {
			printf("sl%d: can't allocate buffer\n", sc - st_softc);
			sc->sc_if.if_flags &= ~IFF_UP;
			return (0);
		}
	}

	/* get buffer in which to unstuff input */
	if (sc->sc_rxbuf == (u_char *) 0) {
		MCLALLOC(p, M_WAIT);
		if (p)
			sc->sc_rxbuf = (u_char *)p + SLBUFSIZE - SLMAX;
		else {
			printf("sl%d: can't allocate buffer\n", sc - st_softc);
			sc->sc_if.if_flags &= ~IFF_UP;
			return (0);
		}
	}

	/* get buffer in which to stuff output */
	if (sc->sc_txbuf == (u_char *) 0) {
		MCLALLOC(p, M_WAIT);
		if (p)
			sc->sc_txbuf = (u_char *)p + SLBUFSIZE - SLMAX;
		else {
			printf("sl%d: can't allocate buffer\n", sc - st_softc);
			
			sc->sc_if.if_flags &= ~IFF_UP;
			return (0);
		}
	}

	sc->sc_buf = sc->sc_ep - SLMAX;
	sc->sc_mp = sc->sc_buf;
	sl_compress_init(&sc->sc_comp, -1);

	return (1);
}

/*
 * Line specific open routine.
 * Attach the given tty to the first available sl unit.
 */
/* ARGSUSED */
int
stripopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{
	struct proc *p = curproc;		/* XXX */
	register struct st_softc *sc;
	register int nstrip;
	int error;
#ifdef __NetBSD__
	int s;
#endif

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	if (tp->t_line == STRIPDISC)
		return (0);

	for (nstrip = NSTRIP, sc = st_softc; --nstrip >= 0; sc++)
		if (sc->sc_ttyp == NULL) {
			if (stripinit(sc) == 0)
				return (ENOBUFS);
			tp->t_sc = (caddr_t)sc;
			sc->sc_ttyp = tp;
			sc->sc_if.if_baudrate = tp->t_ospeed;
			ttyflush(tp, FREAD | FWRITE);
#ifdef __NetBSD__
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
				error = clalloc(&tp->t_outq, 3*SLMTU, 0);
				if (error) {
					splx(s);
					return(error);
				}
			} else
				sc->sc_oldbufsize = sc->sc_oldbufquot = 0;
			splx(s);
#endif /* __NetBSD__ */
			s = spltty();
			resetradio(sc, tp);
			splx(s);

			return (0);
		}
	return (ENXIO);
}

/*
 * Line specific close routine.
 * Detach the tty from the sl unit.
 */
void
stripclose(tp)
	struct tty *tp;
{
	register struct st_softc *sc;
	int s;

	ttywflush(tp);

	DPRINTF(("stripclose: closing\n"));
	s = splimp();		/* actually, max(spltty, splsoftnet) */
	tp->t_line = 0;
	sc = (struct st_softc *)tp->t_sc;
	if (sc != NULL) {
		if_down(&sc->sc_if);
		sc->sc_ttyp = NULL;
		tp->t_sc = NULL;
		MCLFREE((caddr_t)(sc->sc_ep - SLBUFSIZE));
		MCLFREE((caddr_t)(sc->sc_rxbuf - SLBUFSIZE + SLMAX)); /* XXX */
		MCLFREE((caddr_t)(sc->sc_txbuf - SLBUFSIZE + SLMAX)); /* XXX */
		sc->sc_ep = 0;
		sc->sc_mp = 0;
		sc->sc_buf = 0;
		sc->sc_rxbuf = 0;
		sc->sc_txbuf = 0;
	}
#ifdef __NetBSD__
	/* if necessary, install a new outq buffer of the appropriate size */
	if (sc->sc_oldbufsize != 0) {
		clfree(&tp->t_outq);
		clalloc(&tp->t_outq, sc->sc_oldbufsize, sc->sc_oldbufquot);
	}
#endif
	splx(s);
}

/*
 * Line specific (tty) ioctl routine.
 * Provide a way to get the sl unit number.
 */
/* ARGSUSED */
int
striptioctl(tp, cmd, data, flag)
	struct tty *tp;
	u_long cmd;
	caddr_t data;
	int flag;
{
	struct st_softc *sc = (struct st_softc *)tp->t_sc;

	switch (cmd) {
	case SLIOCGUNIT:
		*(int *)data = sc->sc_unit;
		break;

	default:
		return (-1);
	}
	return (0);
}

/*XXX*/
/*
 * Take an mbuf chain  containing a SLIP packet, byte-stuff (escape)
 * the packet, and enqueue it on the tty send queue.
 */
void
strip_esc(sc, m)
	struct st_softc  *sc;
	struct mbuf *m;
{
	register struct tty *tp = sc->sc_ttyp;
	register u_char *cp;
	register u_char *ep;
	struct mbuf *m2;
	register int len;
	u_char         *stuffstate = NULL;

	while (m) {
		cp = mtod(m, u_char *); ep = cp + m->m_len;
		/*
		 * Find out how many bytes in the string we can
		 * handle without doing something special.
		 */
		ep = StuffData(cp, m->m_len, sc->sc_txbuf, &stuffstate);
		len = ep - sc->sc_txbuf;

		/*
		 * Put n characters at once
		 * into the tty output queue.
		 */
		if (b_to_q((ttychar_t *)sc->sc_txbuf,
			   len, &tp->t_outq))
			goto bad;
		sc->sc_if.if_obytes += len;
		MFREE(m, m2);
		m = m2;
	}
	return;

bad:
	m_freem(m);
	return;
}

/* 
 *  Prepend a STRIP header to the packet.
 * (based on 4.4bsd if_ppp)
 */
struct mbuf *
strip_send(sc, m0)
    struct st_softc *sc;
    struct mbuf *m0;
{
	register struct tty *tp = sc->sc_ttyp;
	struct st_header *hdr;

	/*
	 * Send starmode header (unstuffed).
	 */
	hdr = mtod(m0, struct st_header *);
	if (b_to_q((ttychar_t *)hdr, STRIP_HDRLEN, &tp->t_outq)) {
	  	TXPRINTF(("prepend: outq overflow\n"));
		m_freem(m0);
	}

	/* XXX undo M_PREPEND() */
	m0->m_data += sizeof(struct st_header);
	m0->m_len -= sizeof(struct st_header);
	if (m0 && m0->m_flags & M_PKTHDR)
		m0->m_pkthdr.len -= sizeof(struct st_header);
	

	strip_esc(sc, m0);

	if (putc(STRIP_FRAME_END, &tp->t_outq)) {
		/*
		 * Not enough room.  Remove a char to make room
		 * and end the packet normally.
		 * If you get many collisions (more than one or two
		 * a day) you probably do not have enough clists
		 * and you should increase "nclist" in param.c.
		 */
		(void) unputc(&tp->t_outq);
		(void) putc(STRIP_FRAME_END, &tp->t_outq);
		sc->sc_if.if_collisions++;
	} else {
		++sc->sc_if.if_obytes;
		sc->sc_if.if_opackets++;
	}
	return(m0);
}



/*
 * Queue a packet.  Start transmission if not active.
 * Compression happens in slstart; if we do it here, IP TOS
 * will cause us to not compress "background" packets, because
 * ordering gets trashed.  It can be done for all packets in slstart.
 */
int
stripoutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	register struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	register struct st_softc *sc = ifp->if_softc;
	register struct ip *ip;
	register struct ifqueue *ifq;
	register struct st_header *shp;
	register const u_char *dldst;		/* link-level next-hop */
	int s;
	u_char dl_addrbuf[STARMODE_ADDR_LEN+1];


	/*
	 * Verify tty line is up and alive.
	 */
	if (sc->sc_ttyp == NULL) {
		m_freem(m);
		return (ENETDOWN);	/* sort of */
	}
	if ((sc->sc_ttyp->t_state & TS_CARR_ON) == 0 &&
	    (sc->sc_ttyp->t_cflag & CLOCAL) == 0) {
		m_freem(m);
		return (EHOSTUNREACH);
	}

#define SDL(a)          ((struct sockaddr_dl *) (a))

#ifdef DEBUG
	   if (rt) {
	   	printf("stripout, rt: dst af%d gw af%d",
		       rt_key(rt)->sa_family,
		       rt->rt_gateway->sa_family);
		if (rt_key(rt)->sa_family == AF_INET)
		  printf(" dst %x",
			 ((struct sockaddr_in *)rt_key(rt))->sin_addr.s_addr);
		printf("\n");
	}
#endif
	switch (dst->sa_family) {

            case AF_INET:
		/* XXX untested */

                if (rt != NULL && rt->rt_gwroute != NULL)
                        rt = rt->rt_gwroute;

                /* assume rt is never NULL */
                if (rt == NULL || rt->rt_gateway->sa_family != AF_LINK
                    || SDL(rt->rt_gateway)->sdl_alen != ifp->if_addrlen) {
		  	DPRINTF(("strip: could not arp starmode addr %x\n",
			 ((struct sockaddr_in *)dst)->sin_addr.s_addr));
			m_freem(m);
			return(EHOSTUNREACH);
		}
		/*bcopy(LLADDR(SDL(rt->rt_gateway)), dldst, ifp->if_addrlen);*/
                dldst = LLADDR(SDL(rt->rt_gateway));
                break;

            case AF_LINK:
		/*bcopy(LLADDR(SDL(rt->rt_gateway)), dldst, ifp->if_addrlen);*/
		dldst = LLADDR(SDL(dst));
		break;

	default:
		/*
		 * `Cannot happen' (see stripioctl).  Someday we will extend
		 * the line protocol to support other address families.
		 */
		printf("%s: af %d not supported\n", sc->sc_if.if_xname,
			dst->sa_family);
		m_freem(m);
		sc->sc_if.if_noproto++;
		return (EAFNOSUPPORT);
	}
	
	ifq = &sc->sc_if.if_snd;
	ip = mtod(m, struct ip *);
	if (sc->sc_if.if_flags & SC_NOICMP && ip->ip_p == IPPROTO_ICMP) {
		m_freem(m);
		return (ENETRESET);		/* XXX ? */
	}
	if (ip->ip_tos & IPTOS_LOWDELAY)
		ifq = &sc->sc_fastq;

	/*
	 * Add local net header.  If no space in first mbuf,
	 * add another.
	 */
	M_PREPEND(m, sizeof(struct st_header), M_DONTWAIT);
	if (m == 0) {
	  	DPRINTF(("strip: could not prepend starmode header\n"));
	  	return(ENOBUFS);
	}


	/*
	 * Unpack BCD route entry into an ASCII starmode address.
	 */

	dl_addrbuf[0] = '*';

	dl_addrbuf[1] = ((dldst[0] >> 4) & 0x0f) + '0';
	dl_addrbuf[2] = ((dldst[0]     ) & 0x0f) + '0';

	dl_addrbuf[3] = ((dldst[1] >> 4) & 0x0f) + '0';
	dl_addrbuf[4] = ((dldst[1]     ) & 0x0f) + '0';

	dl_addrbuf[5] = '-';

	dl_addrbuf[6] = ((dldst[2] >> 4) & 0x0f) + '0';
	dl_addrbuf[7] = ((dldst[2]     ) & 0x0f) + '0';

	dl_addrbuf[8] = ((dldst[3] >> 4) & 0x0f) + '0';
	dl_addrbuf[9] = ((dldst[3]     ) & 0x0f) + '0';

	dl_addrbuf[10] = '*';
	dl_addrbuf[11] = 0;
	dldst = dl_addrbuf;

	shp = mtod(m, struct st_header *);
	bcopy((caddr_t)"SIP0", (caddr_t)&shp->starmode_type,
		sizeof(shp->starmode_type));

 	bcopy((caddr_t)dldst, (caddr_t)shp->starmode_addr,
		sizeof (shp->starmode_addr));

	TXPRINTF(("strip address is %16s\n", (char *)shp));

	s = splimp();
	if (sc->sc_oqlen && sc->sc_ttyp->t_outq.c_cc == sc->sc_oqlen) {
		struct timeval tv;

		/* if output's been stalled for too long, and restart */
		timersub(&time, &sc->sc_if.if_lastchange, &tv);
		if (tv.tv_sec > 0) {
 /*XXX*/  		DPRINTF(("stripoutput: stalled, resetting\n"));
			sc->sc_otimeout++;
			stripstart(sc->sc_ttyp);
		}
	}
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		sc->sc_if.if_oerrors++;
 /*XXX*/  	TXPRINTF(("stripoutput: ifq full\n"));
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, m);
	sc->sc_if.if_lastchange = time;
	if ((sc->sc_oqlen = sc->sc_ttyp->t_outq.c_cc) == 0) {
 /*XXX*/  		TXPRINTF(("stripoutput: enqueued pkt, restarting\n"));
		stripstart(sc->sc_ttyp);
	}
/* XXX FIXME */
	stripstart(sc->sc_ttyp);
	splx(s);
	return (0);
}

/*
 * Start output on interface.  Get another datagram
 * to send from the interface queue and map it to
 * the interface before starting output.
 */
void
stripstart(tp)
	register struct tty *tp;
{
	register struct st_softc *sc = (struct st_softc *)tp->t_sc;
	register struct mbuf *m;
	register struct ip *ip;
	int s;
#if NBPFILTER > 0
	u_char bpfbuf[SLMTU + SLIP_HDRLEN];
	register int len = 0;
#endif
#ifndef __NetBSD__					/* XXX - cgd */
	extern int cfreecount;
#endif
	for (;;) {
/*XXX*/		TXPRINTF(("stripstart\n"));
		/*
		 * If there is more in the output queue, just send it now.
		 * We are being called in lieu of ttstart and must do what
		 * it would.
		 */
		if (tp->t_outq.c_cc != 0) {
			(*tp->t_oproc)(tp);
			if (tp->t_outq.c_cc > SLIP_HIWAT) {
			  	TXPRINTF(("stripstart: outq past SLIP_HIWAT\n"));
#if 0
				/* XXX can't  just stop output on
				   framed  packet-radio links!
				   */
				return;
#endif
			}
		}
		/*
		 * This happens briefly when the line shuts down.
		 */
		if (sc == NULL) {
		  	TXPRINTF(("(shutdown)\n"));
			return;
		}

#if 0						 	/* XXX - jrs*/
#if defined(__NetBSD__)					/* XXX - cgd */
		/*
		 * Do not remove the packet from the IP queue if it
		 * doesn't look like the packet will fit into the
		 * current serial output queue, with a packet full of
		 * escapes this could be as bad as SLMTU*2+2.
		 */
		if (tp->t_outq.c_cn - tp->t_outq.c_cc < 2*SLMTU+2)
			return;
#endif /* __NetBSD__ */
#endif
		/*
		 * Get a packet and send it to the interface.
		 */
		s = splimp();
		IF_DEQUEUE(&sc->sc_fastq, m);
		if (m)
			sc->sc_if.if_omcasts++;		/* XXX */
		else
			IF_DEQUEUE(&sc->sc_if.if_snd, m);
		splx(s);
		if (m == NULL) {
/*XXX*/			TXPRINTF(("(empty q)\n"));
			return;
		}
		/*
		 * We do the header compression here rather than in stripoutput
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
			register struct mbuf *m1 = m;
			register u_char *cp = bpfbuf + SLIP_HDRLEN;

			len = 0;
			do {
				register int mlen = m1->m_len;

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
			bpf_tap(sc->sc_bpf, bpfbuf, len + SLIP_HDRLEN);
		}
#endif
		sc->sc_if.if_lastchange = time;

#ifndef __NetBSD__					/* XXX - cgd */
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


		if (strip_send(sc, m) == NULL) {
/*XXX*/	 	 	DPRINTF(("stripsend: failed to send pkt\n"));
		}
	}
}



/*
 * Copy data buffer to mbuf chain; add ifnet pointer.
 */
static struct mbuf *
strip_btom(sc, len)
	register struct st_softc *sc;
	register int len;
{
	register struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	/*
	 * If we have more than MHLEN bytes, it's cheaper to
	 * queue the cluster we just filled & allocate a new one
	 * for the input buffer.  Otherwise, fill the mbuf we
	 * allocated above.  Note that code in the input routine
	 * guarantees that packet will fit in a cluster.
	 */
	if (len >= MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			/*
			 * we couldn't get a cluster - if memory's this
			 * low, it's time to start dropping packets.
			 */
			(void) m_free(m);
			return (NULL);
		}
		sc->sc_ep = mtod(m, u_char *) + SLBUFSIZE;
		m->m_data = (caddr_t)sc->sc_buf;
		m->m_ext.ext_buf = (caddr_t)((long)sc->sc_buf &~ MCLOFSET);
		TXPRINTF(("strip_btom: new cluster for sc_buf\n"));
		XDPRINTF(("XXX 1: sc_buf %x end %x hardlim %x\n",
			 sc->sc_buf, sc->sc_mp, sc->sc_ep));
	} else
		bcopy((caddr_t)sc->sc_buf, mtod(m, caddr_t), len);

	m->m_len = len;
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = &sc->sc_if;
	return (m);
}

/*
 * tty interface receiver interrupt.
 *
 * Called with a single char from the tty receiver interrupt; put
 * the char into the buffer containing a partial packet. If the
 * char is a packet delimiter, decapsulate the packet, wrap it in
 * an mbuf, and put it on the protocol input queue.
*/
void
stripinput(c, tp)
	register int c;
	register struct tty *tp;
{
	register struct st_softc *sc;
	register struct mbuf *m;
	register int len;
	int s;
#if NBPFILTER > 0
	u_char chdr[CHDR_LEN];
#endif

	tk_nin++;
	sc = (struct st_softc *)tp->t_sc;
	if (sc == NULL)
		return;
	if (c & TTY_ERRORMASK || ((tp->t_state & TS_CARR_ON) == 0 &&
	    (tp->t_cflag & CLOCAL) == 0)) {
		sc->sc_flags |= SC_ERROR;
		DPRINTF(("strip: input, error %x\n", c));	 /* XXX */
		return;
	}
	c &= TTY_CHARMASK;

	++sc->sc_if.if_ibytes;

	switch (c) {

#ifdef notanymore
	case 0x0a:
	/* (leading newline characters are ignored) */
		if (sc->sc_mp - sc->sc_buf == 0)
		  return;
#endif


	case STRIP_FRAME_END:
		len = sc->sc_mp - sc->sc_buf;

#ifdef XDEBUG
	 	if (len < 15 || sc->sc_flags & SC_ERROR)
		  	printf("stripinput: end of pkt, len %d, err %d\n",
				 len, sc->sc_flags & SC_ERROR); /*XXX*/
#endif
		if(sc->sc_flags & SC_ERROR) {
			sc->sc_flags &= ~SC_ERROR;
			goto newpack;
		}

		len = strip_newpacket(sc, sc->sc_buf, sc->sc_mp);
		if (len <= 1)
			/* less than min length packet - ignore */
			goto newpack;
#if DEBUG > 1
		ipdump("after destuff", sc->sc_buf, len);
#endif /* DEBUG */		



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
			bcopy(sc->sc_buf, chdr, CHDR_LEN);
		}
#endif

		if ((c = (*sc->sc_buf & 0xf0)) != (IPVERSION << 4)) {
			if (c & 0x80)
				c = TYPE_COMPRESSED_TCP;
			else if (c == TYPE_UNCOMPRESSED_TCP)
				*sc->sc_buf &= 0x4f; /* XXX */
			/*
			 * We've got something that's not an IP packet.
			 * If compression is enabled, try to decompress it.
			 * Otherwise, if `auto-enable' compression is on and
			 * it's a reasonable packet, decompress it and then
			 * enable compression.  Otherwise, drop it.
			 */
			if (sc->sc_if.if_flags & SC_COMPRESS) {
				len = sl_uncompress_tcp(&sc->sc_buf, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
			} else if ((sc->sc_if.if_flags & SC_AUTOCOMP) &&
			    c == TYPE_UNCOMPRESSED_TCP && len >= 40) {
				len = sl_uncompress_tcp(&sc->sc_buf, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
				sc->sc_if.if_flags |= SC_COMPRESS;
			} else
				goto error;
		}
#if NBPFILTER > 0
		if (sc->sc_bpf) {
			/*
			 * Put the SLIP pseudo-"link header" in place.
			 * We couldn't do this any earlier since
			 * decompression probably moved the buffer
			 * pointer.  Then, invoke BPF.
			 */
			register u_char *hp = sc->sc_buf - SLIP_HDRLEN;

			hp[SLX_DIR] = SLIPDIR_IN;
			bcopy(chdr, &hp[SLX_CHDR], CHDR_LEN);
			bpf_tap(sc->sc_bpf, hp, len + SLIP_HDRLEN);
		}
#endif
		m = strip_btom(sc, len);
		if (m == NULL)
			goto error;

		sc->sc_if.if_ipackets++;
		sc->sc_if.if_lastchange = time;
		s = splimp();
		if (IF_QFULL(&ipintrq)) {
			IF_DROP(&ipintrq);
			DPRINTF(("stripinput: ipintrq full\n"));
			sc->sc_if.if_ierrors++;
			sc->sc_if.if_iqdrops++;
			m_freem(m);
		} else {
			IF_ENQUEUE(&ipintrq, m);
			schednetisr(NETISR_IP);
		}
		splx(s);
		goto newpack;
	}

	if (sc->sc_mp < sc->sc_ep) {
		*sc->sc_mp++ = c;
		/*sc->sc_escape = 0;*/
		return;
	}

	/* can't put lower; would miss an extra frame */
	sc->sc_flags |= SC_ERROR;
	DPRINTF(("stripinput: overran buf\n"));
	goto quiet_error;

error:
	RXPRINTF(("stripinput: error\n"));
quiet_error:
	sc->sc_if.if_ierrors++;
	goto quiet_newpack;

newpack:
	/*DPRINTF(("stripinput: newpack\n"));*/	 /* XXX */
quiet_newpack:
	sc->sc_mp = sc->sc_buf = sc->sc_ep - SLMAX;
	/*sc->sc_escape = 0;*/
}

/*
 * Process an ioctl request.
 */
int
stripioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	register struct ifreq *ifr;
	register int s = splimp(), error = 0;

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

	default:

#ifdef DEBUG
	  printf("stripioctl: unknown request 0x%lx\n", cmd);
#endif
		error = EINVAL;
	}
	splx(s);
	return (error);
}
/*
 * Strip subroutines
 */

/*
 * Set a radio into starmode.
 */
void
resetradio(sc, tp)
	struct st_softc *sc;
	struct tty *tp;
{
#if 0
	static ttychar_t InitString[] =
		"\r\n\r\n\r\nat\r\n\r\n\r\nate0dt**starmode\r\n**\r\n";
#else
	static ttychar_t InitString[] =
		"\r\rat\r\r\rate0dt**starmode\r*\r";
#endif
	register int i;


	DPRINTF(("strip: resetting radio\n"));
	if ((i = b_to_q(InitString, sizeof(InitString) - 1, &tp->t_outq))) {
		printf("resetradio: %d chars didn't fit in tty queue\n", i);
		return;
	}
	sc->sc_if.if_obytes += sizeof(InitString) - 1;

#ifdef linux /*XXX*/
	/* reset the watchdog counter */
	sc->watchdog_doprobe = jiffies + 10 * HZ;
	sc->watchdog_doreset = jiffies + 1 * HZ;
#endif
	/*XXX jrs DANGEROUS - does this help? */
	sc->sc_if.if_lastchange = time;

	/*XXX jrs DANGEROUS - does this work? */
	(*sc->sc_ttyp->t_oproc)(tp);
}


/*
 * XXX
 * The following is taken, with permisino of the author, from
 * the LInux strip  driver. 
 */


/*
 * Process a received packet.
 */
int
strip_newpacket(sc, ptr, end)
	struct st_softc *sc;
	register u_char *ptr, *end;
{
	register int len = ptr - end;
	register u_char *name, *name_end;
	u_int packetlen;

	/* Ignore empty lines */
	if (len == 0) return 0;

	/* Catch 'OK' responses which show radio has fallen out of starmode */
	if (len >= 2 && ptr[0] == 'O' && ptr[1] == 'K') {
		printf("%s: Radio is back in AT command mode: will reset\n",
			sc->sc_if.if_xname);
		FORCE_RESET(sc);		/* Do reset ASAP */
	return 0;
	}

	/* Check for start of address marker, and then skip over it */
	if (*ptr != '*') {
		/* Catch other error messages */
		if (ptr[0] == 'E' && ptr[1] == 'R' && ptr[2] == 'R' && ptr[3] == '_')
			RecERR_Message(sc, NULL, ptr); /* XXX stuart? */
		else {
			RecErr("No initial *", sc);
			addlog("(len = %d\n", len);
		     }
		return 0;
	}

	/* skip the '*' */
	ptr++;

	/* Skip the return address */
	name = ptr;
	while (ptr < end && *ptr != '*')
		ptr++;

	/* Check for end of address marker, and skip over it */
	if (ptr == end) {
		RecErr("No second *", sc);
		XDPRINTF(("XXX 3: sc_buf %x ptr %x end %x mp %x hardlim %x\n",
			 sc->sc_buf, ptr, end, sc->sc_mp, sc->sc_ep));
		return 0;
	}
	name_end = ptr++;

	/* Check for SRIP key, and skip over it */
	if (ptr[0] != 'S' || ptr[1] != 'I' || ptr[2] != 'P' || ptr[3] != '0') {
		if (ptr[0] == 'E' && ptr[1] == 'R' && ptr[2] == 'R' &&
		    ptr[3] == '_') { 
			*name_end = 0;
			RecERR_Message(sc, name, ptr);
		 }
		else RecErr("No SRIP key", sc);
		return 0;
	}
	ptr += 4;

	/* Decode start of the IP packet header */
	ptr = UnStuffData(ptr, end, sc->sc_rxbuf, 4);
	if (!ptr) {
		RecErr("Runt packet (hdr)", sc);
		return 0;
	}

	/* XXX is this the IP header length, or what? */
	packetlen = ((u_short)sc->sc_rxbuf[2] << 8) | sc->sc_rxbuf[3];
/*	printf("Packet %02x.%02x.%02x.%02x\n",
		sc->sc_rxbuf[0], sc->sc_rxbuf[1],
		sc->sc_rxbuf[2], sc->sc_rxbuf[3]);
	printf("Got %d byte packet\n", packetlen); */

	/* Decode remainder of the IP packer */
	ptr = UnStuffData(ptr, end, sc->sc_rxbuf+4, packetlen-4);
	if (!ptr) {
		RecErr("Short packet", sc);
		return 0;
	}

	/* XXX*/ bcopy(sc->sc_rxbuf, sc->sc_buf, packetlen );

#ifdef linux
	strip_bump(sc, packetlen);
#endif
	return(packetlen);
}


/*
 * Stuffing scheme:
 * 00    Unused (reserved character)
 * 01-3F Run of 2-64 different characters
 * 40-7F Run of 1-64 different characters plus a single zero at the end
 * 80-BF Run of 1-64 of the same character
 * C0-FF Run of 1-64 zeroes (ASCII 0)
*/
typedef enum
{
	Stuff_Diff      = 0x00,
	Stuff_DiffZero  = 0x40,
	Stuff_Same      = 0x80,
	Stuff_Zero      = 0xC0,
	Stuff_NoCode    = 0xFF,		/* Special code, meaning no code selected */
	
	Stuff_CodeMask  = 0xC0,
	Stuff_CountMask = 0x3F,
	Stuff_MaxCount  = 0x3F,
	Stuff_Magic     = 0x0D		/* The value we are eliminating */
} StuffingCode;

/*
 * StuffData encodes the data starting at "src" for "length" bytes.
 * It writes it to the buffer pointed to by "dest" (which must be at least
 * as long as 1 + 65/64 of the input length). The output may be up to 1.6%
 * larger than the input for pathological input, but will usually be smaller.
 * StuffData returns the new value of the dest pointer as its result.
 *
 * "code_ptr_ptr" points to a "u_char *" which is used to hold
 * encoding state between calls, allowing an encoded packet to be
 * incrementally built up from small parts.
 * On the first call, the "u_char *" pointed to should be initialized
 * to NULL;  between subsequent calls the calling routine should leave
 * the value alone and simply pass it back unchanged so that the
 * encoder can recover its current state.
 */ 

#define StuffData_FinishBlock(X) \
	(*code_ptr = (X) ^ Stuff_Magic, code = Stuff_NoCode)

static u_char*
StuffData(u_char *src, u_long length, u_char *dest, u_char **code_ptr_ptr)
{
	u_char *end = src + length;
	u_char *code_ptr = *code_ptr_ptr;
	u_char code = Stuff_NoCode, count = 0;
	
	if (!length) return(dest);
	
	if (code_ptr) {	/* Recover state from last call, if applicable */
		code  = *code_ptr & Stuff_CodeMask;
		count = *code_ptr & Stuff_CountMask;
	}

	while (src < end) {
		switch (code) {
		/*
		 * Stuff_NoCode: If no current code, select one
		 */
		case Stuff_NoCode:
		  	code_ptr = dest++;	/* Record where we're going to put this code */
			count = 0;		/* Reset the count (zero means one instance) */
							/* Tentatively start a new block */
			if (*src == 0) {
				code = Stuff_Zero;
				src++;
			} else {
				code = Stuff_Same;
				*dest++ = *src++ ^ Stuff_Magic;
			}
			/* Note: We optimistically assume run of same -- which will be */
			/* fixed later in Stuff_Same if it turns out not to be true. */
			break;

		/*
		 * Stuff_Zero: We already have at least one zero encoded
		 */
		case Stuff_Zero:
		  	
			/* If another zero, count it, else finish this code block */
			if (*src == 0) {
				count++;
				src++;
			} else
				StuffData_FinishBlock(Stuff_Zero + count);
			break;

		/*
		 * Stuff_Same: We already have at least one byte encoded
		 */
		case Stuff_Same:
			/* If another one the same, count it */
			if ((*src ^ Stuff_Magic) == code_ptr[1]) {
				count++;
				src++;
				break;
			}
			/* else, this byte does not match this block. */
			/* If we already have two or more bytes encoded, finish this code block */
			if (count) {
				StuffData_FinishBlock(Stuff_Same + count);
				break;
			}
			/* else, we only have one so far, so switch to Stuff_Diff code */
			code = Stuff_Diff; /* and fall through to Stuff_Diff case below */

		case Stuff_Diff:	/* Stuff_Diff: We have at least two *different* bytes encoded */
			/* If this is a zero, must encode a Stuff_DiffZero, and begin a new block */
			if (*src == 0)
				StuffData_FinishBlock(Stuff_DiffZero + count);
			/* else, if we have three in a row, it is worth starting a Stuff_Same block */
			else if ((*src ^ Stuff_Magic) == dest[-1] && dest[-1] == dest[-2])
				{
				code += count-2;
				if (code == Stuff_Diff)
					code = Stuff_Same;
				StuffData_FinishBlock(code);
				code_ptr = dest-2;
				/* dest[-1] already holds the correct value */
				count = 2;		/* 2 means three bytes encoded */
				code = Stuff_Same;
				}
			/* else, another different byte, so add it to the block */
			else {
				*dest++ = *src ^ Stuff_Magic;
				count++;
			}
			src++;	/* Consume the byte */
			break;
		}

		if (count == Stuff_MaxCount)
			StuffData_FinishBlock(code + count);
		}
	if (code == Stuff_NoCode)
		*code_ptr_ptr = NULL;
	else {
		*code_ptr_ptr = code_ptr;
		StuffData_FinishBlock(code + count);
	}

	return(dest);
}



/*
 * UnStuffData decodes the data at "src", up to (but not including)
 * "end".  It writes the decoded data into the buffer pointed to by
 * "dest", up to a  maximum of "dest_length", and returns the new
 * value of "src" so that a follow-on call can read more data,
 * continuing from where the first left off. 
 *
 * There are three types of results:
 * 1. The source data runs out before extracting "dest_length" bytes:
 *    UnStuffData returns NULL to indicate failure.
 * 2. The source data produces exactly "dest_length" bytes:
 *    UnStuffData returns new_src = end to indicate that all bytes
 *    were consumed. 
 * 3. "dest_length" bytes are extracted, with more
 *     remaining. UnStuffData returns new_src < end to indicate that
 *     there are more bytes to be read.
 *
 * Note: The decoding may be destructive, in that it may alter the
 * source data in the process of decoding it (this is necessary to
 * allow a follow-on  call to resume correctly).
 */

static u_char*
UnStuffData(u_char *src, u_char *end, u_char *dest, u_long dest_length)
{
	u_char *dest_end = dest + dest_length;

	if (!src || !end || !dest || !dest_length)
		return(NULL);	/* Sanity check */

	while (src < end && dest < dest_end) {
		int count = (*src ^ Stuff_Magic) & Stuff_CountMask;
		switch ((*src ^ Stuff_Magic) & Stuff_CodeMask)
			{
			case Stuff_Diff:
				if (src+1+count >= end)
					return(NULL);
				do
					*dest++ = *++src ^ Stuff_Magic;
				while(--count >= 0 && dest < dest_end);
				if (count < 0)
					src += 1;
				else if (count == 0)
					*src = Stuff_Same ^ Stuff_Magic;
				else
					*src = (Stuff_Diff + count) ^ Stuff_Magic;
				break;
			case Stuff_DiffZero:
				if (src+1+count >= end)
					return(NULL);
				do
					*dest++ = *++src ^ Stuff_Magic;
				while(--count >= 0 && dest < dest_end);
				if (count < 0)
					*src = Stuff_Zero ^ Stuff_Magic;
				else
					*src = (Stuff_DiffZero + count) ^ Stuff_Magic;
				break;
			case Stuff_Same:
				if (src+1 >= end)
					return(NULL);
				do
					*dest++ = src[1] ^ Stuff_Magic;
				while(--count >= 0 && dest < dest_end);
				if (count < 0)
					src += 2;
				else
					*src = (Stuff_Same + count) ^ Stuff_Magic;
				break;
			case Stuff_Zero:
				do
					*dest++ = 0;
				while(--count >= 0 && dest < dest_end);
				if (count < 0)
					src += 1;
				else
					*src = (Stuff_Zero + count) ^ Stuff_Magic;
				break;
			}
	}

	if (dest < dest_end)
		return(NULL);
	else
		return(src);
}



/*
 * Log an error mesesage (for a packet received with errors?)
 * rom the STRIP driver.
 * XXX check with original author.
 */
static void
RecErr(msg, sc)
	char *msg;
	struct st_softc *sc;
{
	static const int MAX_RecErr = 80;
	u_char *ptr = sc->sc_buf;
	u_char *end = sc->sc_mp;
	u_char pkt_text[MAX_RecErr], *p = pkt_text;
	*p++ = '\"';
	while (ptr < end && p < &pkt_text[MAX_RecErr-4]) {
		if (*ptr == '\\') {
			*p++ = '\\';
			*p++ = '\\';
		} else if (*ptr >= 32 && *ptr <= 126)
			*p++ = *ptr;
		else {
			sprintf(p, "\\%02x", *ptr);
			p+= 3;
		}
		ptr++;
	}

	if (ptr == end) *p++ = '\"';
	*p++ = 0;
	addlog("%13s : %s\n", msg, pkt_text);

#ifdef linux
	set_bit(SLF_ERROR, &sc->flags);
	sc->rx_errors++;
#endif /* linux */
	sc->sc_if.if_ierrors++;
}

/*
 * Log an error message for a packet recieved from a remote Metricom
 * radio.  Update the radio-reset timer if the message is one that
 * is generated only in starmode.
 *
 * We only call this function when we have an error message from
 * the radio, which can only happen after seeing a frame delimeter
 * in the input-side routine; so it's safe to call the output side
 * to reset the radio.
 */
static void
RecERR_Message(sc, sendername, msg)
	struct st_softc *sc;
	u_char *sendername;
	u_char *msg;
{
	static const char ERR_001[] = "ERR_001 Not in StarMode!";
	static const char ERR_002[] = "ERR_002 Remap handle";
	static const char ERR_003[] = "ERR_003 Can't resolve name";
	static const char ERR_004[] = "ERR_004 Name too small or missing";
	static const char ERR_007[] = "ERR_007 Body too big";
	static const char ERR_008[] = "ERR_008 Bad character in name";

	if (!strncmp(msg, ERR_001, sizeof(ERR_001)-1)) {
		printf("Radio %s is not in StarMode\n", sendername);
	}
	else if (!strncmp(msg, ERR_002, sizeof(ERR_002)-1)) {
#ifdef notyet		/*Kernel doesn't have scanf!*/
		int handle;
		u_char newname[64];
		sscanf(msg, "ERR_002 Remap handle &%d to name %s", &handle, newname);
		printf("Radio name %s is handle %d\n", newname, handle);
#endif
		}
	else if (!strncmp(msg, ERR_003, sizeof(ERR_003)-1)) {
		printf("Radio name <unspecified> is unknown (\"Can't resolve name\" error)\n");
		}
	else if (!strncmp(msg, ERR_004, sizeof(ERR_004)-1)) {
		CLEAR_RESET_TIMER(sc);
		/* printf("%s: Received tickle response; clearing watchdog_doreset timer.\n",
			sc->sc_if.if_xname); */
	}
	else if (!strncmp(msg, ERR_007, sizeof(ERR_007)-1)) {
		/* Note: This error knoks the radio back into command mode. */
		printf("Error! Packet size <unspecified> is too big for radio.");
		FORCE_RESET(sc);		/* Do reset ASAP */
		}
	else if (!strncmp(msg, ERR_008, sizeof(ERR_008)-1)) {
		printf("Name <unspecified> contains illegal character\n");
		}
	else RecErr("Error Msg:", sc);
}

#ifdef DEBUG
void
stripdumpm(msg, m, len)
     	const char *msg;
	struct mbuf *m;
{
  stripdump(msg, mtod(m, u_char*), len);
  /*XXX*/
}

void
stripdump(msg, p, len)
	const char *msg;
	u_char *p;
	int len;
{
	register int i;


	printf(msg);
	for (i = 0; i < STRIP_HDRLEN; i++)
		printf("%c", p[i]);
	printf("\n");

	p += STRIP_HDRLEN;
	for (i = 0; i < 32; i++) {
	  	printf("%02x ", p[i]);
	}
		printf("\n");
}

void
ipdump(msg, p, len)
	const char *msg;
	u_char *p;
	int len;
{
	register int i;

	printf(msg);
	for (i = 0; i < 32; i++) {
	  	printf("%02x ", p[i]);
	}
		printf("\n");
}

#endif /* DEBUG */
#endif /* NSTRIP > 0 */
