/*	$OpenBSD: tcp_input.c,v 1.27 1999/01/27 10:04:57 niklas Exp $	*/
/*	$NetBSD: tcp_input.c,v 1.23 1996/02/13 23:43:44 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994
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
 *	@(#)tcp_input.c	8.5 (Berkeley) 4/10/94
 */

/*
%%% portions-copyright-nrl-95
Portions of this software are Copyright 1995-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.
*/

#ifndef TUBA_INCLUDE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>
#include <dev/rndvar.h>
#include <machine/stdarg.h>
#include <sys/md5k.h>

#ifdef INET6
#include <sys/domain.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/tcpipv6.h>

struct	tcpiphdr tcp_saveti;
struct  tcpipv6hdr tcp_saveti6;

/* for the packet header length in the mbuf */
#define M_PH_LEN(m)      (((struct mbuf *)(m))->m_pkthdr.len)
#define M_V6_LEN(m)      (M_PH_LEN(m) - sizeof(struct ipv6))
#define M_V4_LEN(m)      (M_PH_LEN(m) - sizeof(struct ip))
#endif /* INET6 */

int	tcprexmtthresh = 3;
struct	tcpiphdr tcp_saveti;
int	tcptv_keep_init = TCPTV_KEEP_INIT;

extern u_long sb_max;

#endif /* TUBA_INCLUDE */
#define TCP_PAWS_IDLE	(24 * 24 * 60 * 60 * PR_SLOWHZ)

/* for modulo comparisons of timestamps */
#define TSTMP_LT(a,b)	((int)((a)-(b)) < 0)
#define TSTMP_GEQ(a,b)	((int)((a)-(b)) >= 0)

#ifdef TCPCOOKIE
/*
 * Code originally by Matt Blaze and John Ioannidis. This code implements
 * a cookie-like extension for TCP. Adapted to OpenBSD by Angelos D.
 * Keromytis.
 */

#ifndef TCK_NFRIENDS
#define TCK_NFRIENDS 16
#endif /* TCK_NFRIENDS */

static struct in_addr tck_friends[TCK_NFRIENDS];
static int tck_nfriends = 0;
static int tck_initialized = 0;

#define TCK_PORT  333			/* Unused port! */

static int
tck_isafriend(struct in_addr f)
{
	register int i;
	
	for (i = tck_nfriends - 1; i >= 0; i--)
	  if (tck_friends[i].s_addr == f.s_addr)
	    return 1;

	return 0;
}

static void
tck_delat(int n)
{
	int i;
	
	if ((n >= tck_nfriends) || (tck_nfriends == 0))
	  return;

	for (i = n + 1; i < tck_nfriends ; i++)
	  tck_friends[i - 1] = tck_friends[i];

	tck_nfriends--;
}

static void
tck_addfriend(struct in_addr f)
{
#ifdef DEBUG_TCPCOOKIE
	printf("tck_addfriend: 0x%08x\n", ntohl(f.s_addr));
#endif /* DEBUG_TCPCOOKIE */

	if (tck_isafriend(f))
	  return;

	if (tck_nfriends == TCK_NFRIENDS)
	  tck_delat(0);

	tck_friends[tck_nfriends++] = f;
}

/*
 * static void
 * tck_delfriend(struct in_addr f)
 * {
 *	int i;
 *
 *      for (i = tck_nfriends - 1; i >= 0; i--)
 *        if (tck_friends[i].s_addr == f.s_addr)
 *	    goto found1;
 *	
 *	return;
 *
 * found1:
 *	tck_delat(i);
 * }
*/

static u_int32_t
tck_makecookie(f)
	struct in_addr f;
{
	static MD5_CTX ctx;
	u_int8_t buf[16];
	MD5_CTX ctx2;

	if (tck_initialized == 0) {	/* This only happens once per reboot */
		tck_initialized = 1;

		get_random_bytes((void *) buf, 16);
		MD5Init(&ctx);
		MD5Update(&ctx, buf, 16);
	}
	ctx2 = ctx;
	MD5Update(&ctx2, (void *) &f, sizeof(f));
	MD5Final(buf, &ctx2);		/* This may not be necessary */
	return ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}	

static int
tck_chkcookie(ti)
	struct tcpiphdr *ti;
{
#ifdef DEBUG_TCPCOOKIE
	printf("tck_chkcookie: src = 0x%08x, cookie = 0x%08x, seq = 0x%08x, ack = 0x%08x\n", ntohl(ti->ti_src.s_addr), tck_makecookie(ti->ti_src), ti->ti_seq, ti->ti_ack);
#endif /* DEBUG_TCPCOOKIE */

	if (tck_makecookie(ti->ti_src) == ti->ti_seq) {
		/* seq in host order */
		tck_addfriend(ti->ti_src);
		return 1;
	}
	return 0;
}

#endif /* TCPCOOKIE */

/*
 * Insert segment ti into reassembly queue of tcp with
 * control block tp.  Return TH_FIN if reassembly now includes
 * a segment with FIN.  The macro form does the common case inline
 * (segment is the next to be received on an established connection,
 * and the queue is empty), avoiding linkage into and removal
 * from the queue and repetition of various conversions.
 * Set DELACK for segments received in order, but ack immediately
 * when segments are out of order (so fast retransmit can work).
 */

#ifndef TUBA_INCLUDE

int
tcp_reass(tp, th, m, tlen)
	register struct tcpcb *tp;
	register struct tcphdr *th;
	struct mbuf *m;
	int *tlen;
{
	register struct ipqent *p, *q, *nq, *tiqe;
	struct socket *so = tp->t_inpcb->inp_socket;
	int flags;

	/*
	 * Call with th==0 after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (th == 0)
		goto present;

	/*
	 * Allocate a new queue entry, before we throw away any data.
	 * If we can't, just drop the packet.  XXX
	 */
	MALLOC(tiqe, struct ipqent *, sizeof (struct ipqent), M_IPQ, M_NOWAIT);
	if (tiqe == NULL) {
		tcpstat.tcps_rcvmemdrop++;
		m_freem(m);
		return (0);
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = tp->segq.lh_first; q != NULL;
	    p = q, q = q->ipqe_q.le_next)
		if (SEQ_GT(q->ipqe_tcp->th_seq, th->th_seq))
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		register struct tcphdr *phdr = p->ipqe_tcp;
		register int i;

		/* conversion to int (in i) handles seq wraparound */
		i = phdr->th_seq + phdr->th_reseqlen - th->th_seq;
		if (i > 0) {
		        if (i >= *tlen) {
				tcpstat.tcps_rcvduppack++;
				tcpstat.tcps_rcvdupbyte += *tlen;
				m_freem(m);
				FREE(tiqe, M_IPQ);
				return (0);
			}
			m_adj(m, i);
			*tlen -= i;
			th->th_seq += i;
		}
	}
	tcpstat.tcps_rcvoopack++;
	tcpstat.tcps_rcvoobyte += *tlen;

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	for (; q != NULL; q = nq) {
		register struct tcphdr *qhdr = q->ipqe_tcp;
		register int i = (th->th_seq + *tlen) - qhdr->th_seq;

		if (i <= 0)
			break;
		if (i < qhdr->th_reseqlen) {
			qhdr->th_seq += i;
			qhdr->th_reseqlen -= i;
			m_adj(q->ipqe_m, i);
			break;
		}
		nq = q->ipqe_q.le_next;
		m_freem(q->ipqe_m);
		LIST_REMOVE(q, ipqe_q);
		FREE(q, M_IPQ);
	}

	/* Insert the new fragment queue entry into place. */
	tiqe->ipqe_m = m;
	th->th_reseqlen = *tlen;
	tiqe->ipqe_tcp = th;
	if (p == NULL) {
		LIST_INSERT_HEAD(&tp->segq, tiqe, ipqe_q);
	} else {
		LIST_INSERT_AFTER(p, tiqe, ipqe_q);
	}

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		return (0);
	q = tp->segq.lh_first;
	if (q == NULL || q->ipqe_tcp->th_seq != tp->rcv_nxt)
		return (0);
	if (tp->t_state == TCPS_SYN_RECEIVED && q->ipqe_tcp->th_reseqlen)
		return (0);
	do {
		tp->rcv_nxt += q->ipqe_tcp->th_reseqlen;
		flags = q->ipqe_tcp->th_flags & TH_FIN;

		nq = q->ipqe_q.le_next;
		LIST_REMOVE(q, ipqe_q);
		if (so->so_state & SS_CANTRCVMORE)
			m_freem(q->ipqe_m);
		else
			sbappend(&so->so_rcv, q->ipqe_m);
		FREE(q, M_IPQ);
		q = nq;
	} while (q != NULL && q->ipqe_tcp->th_seq == tp->rcv_nxt);
	sorwakeup(so);
	return (flags);
}

/*
 * First check for a port-specific bomb. We do not want to drop half-opens
 * for other ports if this is the only port being bombed.  We only check
 * the bottom 40 half open connections, to avoid wasting too much time.
 *
 * Or, otherwise it is more likely a generic syn bomb, so delete the oldest
 * half-open connection.
 */
void
tcpdropoldhalfopen(avoidtp, port)
	struct tcpcb *avoidtp;
	u_int16_t port;
{
	register struct inpcb *inp;
	register struct tcpcb *tp;
	int ncheck = 40;
	int s;

	s = splnet();
	inp = tcbtable.inpt_queue.cqh_first;
	if (inp)						/* XXX */
	for (; inp != (struct inpcb *)&tcbtable.inpt_queue && --ncheck;
	    inp = inp->inp_queue.cqe_prev) {
		if ((tp = (struct tcpcb *)inp->inp_ppcb) &&
		    tp != avoidtp &&
		    tp->t_state == TCPS_SYN_RECEIVED &&
		    port == inp->inp_lport) {
			tcp_close(tp);
			goto done;
		}
	}

	inp = tcbtable.inpt_queue.cqh_first;
	if (inp)						/* XXX */
	for (; inp != (struct inpcb *)&tcbtable.inpt_queue;
	    inp = inp->inp_queue.cqe_prev) {
		if ((tp = (struct tcpcb *)inp->inp_ppcb) &&
		    tp != avoidtp &&
		    tp->t_state == TCPS_SYN_RECEIVED) {
			tcp_close(tp);
			goto done;
		}
	}
done:
	splx(s);
}

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
void
#if __STDC__
tcp_input(struct mbuf *m, ...)
#else
tcp_input(m, va_alist)
	register struct mbuf *m;
#endif
{
	register struct tcpiphdr *ti;
	register struct inpcb *inp;
	caddr_t optp = NULL;
	int optlen = 0;
	int len, tlen, off;
	register struct tcpcb *tp = 0;
	register int tiflags;
	struct socket *so = NULL;
	int todrop, acked, ourfinisacked, needoutput = 0;
	short ostate = 0;
	struct in_addr laddr;
	int dropsocket = 0;
	int iss = 0;
	u_long tiwin;
	u_int32_t ts_val, ts_ecr;
	int ts_present = 0;
	int iphlen;
	va_list ap;
	register struct tcphdr *th;
#ifdef INET6
	struct in6_addr laddr6;
	unsigned short is_ipv6;     /* Type of incoming datagram. */
	struct ipv6 *ipv6 = NULL;
#endif /* INET6 */

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	tcpstat.tcps_rcvtotal++;

#ifdef INET6
	/*
	 * Before we do ANYTHING, we have to figure out if it's TCP/IPv6 or
	 * TCP/IPv4.
 	 */
	is_ipv6 = mtod(m, struct ip *)->ip_v == 6;
#endif /* INET6 */

	/*
	 * Get IP and TCP header together in first mbuf.
	 * Note: IP leaves IP header in first mbuf.
	 */
#ifndef INET6
	ti = mtod(m, struct tcpiphdr *);
#else /* INET6 */
	if (!is_ipv6)
#endif /* INET6 */
	if (iphlen > sizeof (struct ip))
		ip_stripoptions(m, (struct mbuf *)0);
	if (m->m_len < iphlen + sizeof(struct tcphdr)) {
		if ((m = m_pullup2(m, iphlen + sizeof(struct tcphdr))) == 0) {
			tcpstat.tcps_rcvshort++;
			return;
		}
#ifndef INET6
		ti = mtod(m, struct tcpiphdr *);
#endif /* INET6 */
	}

	tlen = m->m_pkthdr.len - iphlen;

#ifdef INET6
	/*
	 * After that, do initial segment processing which is still very
	 * dependent on what IP version you're using.
	 */

	if (is_ipv6) {
#ifdef DIAGNOSTIC
	  if (iphlen < sizeof(struct ipv6)) {
	    m_freem(m);
	    return;
	  }
#endif /* DIAGNOSTIC */

	  /* strip off any options */
	  if (iphlen > sizeof(struct ipv6)) {
	    ipv6_stripoptions(m, iphlen);
	    iphlen = sizeof(struct ipv6);
	  }

	  ti = NULL;
	  ipv6 = mtod(m, struct ipv6 *);

	  if (in6_cksum(m, IPPROTO_TCP, tlen, sizeof(struct ipv6))) {
	    tcpstat.tcps_rcvbadsum++;
	    goto drop;
	  } /* endif in6_cksum */
	} else {
	  ti = mtod(m, struct tcpiphdr *);
#endif /* INET6 */

	/*
	 * Checksum extended TCP header and data.
	 */
#ifndef INET6
	tlen = ((struct ip *)ti)->ip_len;
#endif /* INET6 */
	len = sizeof (struct ip) + tlen;
	bzero(ti->ti_x1, sizeof ti->ti_x1);
	ti->ti_len = (u_int16_t)tlen;
	HTONS(ti->ti_len);
	if ((ti->ti_sum = in_cksum(m, len)) != 0) {
		tcpstat.tcps_rcvbadsum++;
		goto drop;
	}
#ifdef INET6
	}
#endif /* INET6 */
#endif /* TUBA_INCLUDE */

	th = (struct tcphdr *)(mtod(m, caddr_t) + iphlen);

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = th->th_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
		tcpstat.tcps_rcvbadoff++;
		goto drop;
	}
	tlen -= off;
	if (off > sizeof (struct tcphdr)) {
		if (m->m_len < iphlen + off) {
			if ((m = m_pullup2(m, iphlen + off)) == 0) {
				tcpstat.tcps_rcvshort++;
				return;
			}
#ifdef INET6
			if (is_ipv6)
			  ipv6 = mtod(m, struct ipv6 *);
			else
#endif /* INET6 */
			ti = mtod(m, struct tcpiphdr *);
			th = (struct tcphdr *)(mtod(m, caddr_t) + iphlen);
		}
		optlen = off - sizeof (struct tcphdr);
		optp = mtod(m, caddr_t) + iphlen + sizeof(struct tcphdr);
		/* 
		 * Do quick retrieval of timestamp options ("options
		 * prediction?").  If timestamp is the only option and it's
		 * formatted as recommended in RFC 1323 appendix A, we
		 * quickly get the values now and not bother calling
		 * tcp_dooptions(), etc.
		 */
		if ((optlen == TCPOLEN_TSTAMP_APPA ||
		     (optlen > TCPOLEN_TSTAMP_APPA &&
			optp[TCPOLEN_TSTAMP_APPA] == TCPOPT_EOL)) &&
		     *(u_int32_t *)optp == htonl(TCPOPT_TSTAMP_HDR) &&
		     (th->th_flags & TH_SYN) == 0) {
			ts_present = 1;
			ts_val = ntohl(*(u_int32_t *)(optp + 4));
			ts_ecr = ntohl(*(u_int32_t *)(optp + 8));
			optp = NULL;	/* we've parsed the options */
		}
	}
	tiflags = th->th_flags;

	/*
	 * Convert TCP protocol specific fields to host format.
	 */
	NTOHL(th->th_seq);
	NTOHL(th->th_ack);
	NTOHS(th->th_win);
	NTOHS(th->th_urp);

#ifdef TCPCOOKIE
	/* 
	 * If this looks like a cookie response, check it.
	 * If it is, the check routine also adds the source
	 * of the packet to the friends list.
	 */

#ifdef INET6
	if (!is_ipv6 && (tiflags & TH_RST) && (ntohs(th->th_dport) == TCK_PORT))
#else /* INET6 */
	if ((tiflags & TH_RST) && (ntohs(ti->ti_dport) == TCK_PORT))
#endif /* INET6 */
	  if (tck_chkcookie(ti))
	    goto drop;			/* RST is no longer needed */
#endif /* TCPCOOKIE */

	/*
	 * Locate pcb for segment.
	 */
findpcb:
#ifdef INET6
	if (is_ipv6) {
	  inp = in6_pcbhashlookup(&tcbtable, &ipv6->ipv6_src, th->th_sport,
				 &ipv6->ipv6_dst, th->th_dport);
	} else
#endif /* INET6 */
	inp = in_pcbhashlookup(&tcbtable, ti->ti_src, ti->ti_sport,
	    ti->ti_dst, ti->ti_dport);
	if (inp == 0) {
		++tcpstat.tcps_pcbhashmiss;
#ifdef INET6
		if (is_ipv6)
			inp = in_pcblookup(&tcbtable, &ipv6->ipv6_src,
			    th->th_sport, &ipv6->ipv6_dst, th->th_dport,
			    INPLOOKUP_WILDCARD | INPLOOKUP_IPV6);
		else
#endif /* INET6 */
		inp = in_pcblookup(&tcbtable, &ti->ti_src, ti->ti_sport,
		    &ti->ti_dst, ti->ti_dport, INPLOOKUP_WILDCARD);
		/*
		 * If the state is CLOSED (i.e., TCB does not exist) then
		 * all data in the incoming segment is discarded.
		 * If the TCB exists but is in CLOSED state, it is embryonic,
		 * but should either do a listen or a connect soon.
		 */
		if (inp == 0) {
			++tcpstat.tcps_noport;
			goto dropwithreset;
		}
	}

	tp = intotcpcb(inp);
	if (tp == 0)
		goto dropwithreset;
	if (tp->t_state == TCPS_CLOSED)
		goto drop;
	
	/* Unscale the window into a 32-bit value. */
	if ((tiflags & TH_SYN) == 0)
		tiwin = th->th_win << tp->snd_scale;
	else
		tiwin = th->th_win;

	so = inp->inp_socket;
	if (so->so_options & (SO_DEBUG|SO_ACCEPTCONN)) {
		if (so->so_options & SO_DEBUG) {
			ostate = tp->t_state;
#ifdef INET6
			if (is_ipv6)
			  tcp_saveti6 = *(mtod(m, struct tcpipv6hdr *));
			else
#endif /* INET6 */
			tcp_saveti = *ti;
		}
		if (so->so_options & SO_ACCEPTCONN) {
			struct socket *so1;

			so1 = sonewconn(so, 0);
			if (so1 == NULL) {
				tcpdropoldhalfopen(tp, th->th_dport);
				so1 = sonewconn(so, 0);
				if (so1 == NULL)
					goto drop;
			}
			so = so1;
			/*
			 * This is ugly, but ....
			 *
			 * Mark socket as temporary until we're
			 * committed to keeping it.  The code at
			 * ``drop'' and ``dropwithreset'' check the
			 * flag dropsocket to see if the temporary
			 * socket created here should be discarded.
			 * We mark the socket as discardable until
			 * we're committed to it below in TCPS_LISTEN.
			 */
			dropsocket++;
#ifdef INET6
			/*
			 * inp still has the OLD in_pcb stuff, set the
			 * v6-related flags on the new guy, too.   This is
			 * done particularly for the case where an AF_INET6
			 * socket is bound only to a port, and a v4 connection
			 * comes in on that port.
			 * we also copy the flowinfo from the original pcb 
			 * to the new one.
			 */
			{
			  int flags = inp->inp_flags;
			  struct inpcb *oldinpcb = inp;
			  
			  inp = (struct inpcb *)so->so_pcb;
			  inp->inp_flags |= (flags & (INP_IPV6 | INP_IPV6_UNDEC
						      | INP_IPV6_MAPPED));
			  if ((inp->inp_flags & INP_IPV6) &&
			      !(inp->inp_flags & INP_IPV6_MAPPED)) {
			    inp->inp_ipv6.ipv6_hoplimit = 
			      oldinpcb->inp_ipv6.ipv6_hoplimit;
			    inp->inp_ipv6.ipv6_versfl = 
			      oldinpcb->inp_ipv6.ipv6_versfl;
			  }
			}
#else /* INET6 */
			inp = (struct inpcb *)so->so_pcb;
#endif /* INET6 */
			inp->inp_lport = th->th_dport;
#ifdef INET6
			if (is_ipv6) {
			  inp->inp_laddr6 = ipv6->ipv6_dst;
			  inp->inp_fflowinfo = htonl(0x0fffffff) & 
			    ipv6->ipv6_versfl;
			  
			  /*inp->inp_options = ipv6_srcroute();*/ /* soon. */
			  /* still need to tweak outbound options
			     processing to include this mbuf in
			     the right place and put the correct
			     NextHdr values in the right places.
			     XXX  rja */
			} else {
			  if (inp->inp_flags & INP_IPV6) {/* v4 to v6 socket */
			    CREATE_IPV6_MAPPED(inp->inp_laddr6,
			      ti->ti_dst.s_addr);
			  } else {
#endif /* INET6 */
			    inp->inp_laddr = ti->ti_dst;
			    inp->inp_options = ip_srcroute();
#if INET6
			  }
			};
#endif /* INET6 */
			in_pcbrehash(inp);
			tp = intotcpcb(inp);
			tp->t_state = TCPS_LISTEN;

			/* Compute proper scaling value from buffer space
			 */
			while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
			   TCP_MAXWIN << tp->request_r_scale < so->so_rcv.sb_hiwat)
				tp->request_r_scale++;
		}
	}

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	tp->t_idle = 0;
	if (tp->t_state != TCPS_SYN_RECEIVED)
		tp->t_timer[TCPT_KEEP] = tcp_keepidle;

#ifdef TCP_SACK
	if (!tp->sack_disable)
		tcp_del_sackholes(tp, th); /* Delete stale SACK holes */
#endif /* TCP_SACK */

	/*
	 * Process options if not in LISTEN state,
	 * else do it below (after getting remote address).
	 */
	if (optp && tp->t_state != TCPS_LISTEN)
		tcp_dooptions(tp, optp, optlen, th,
			&ts_present, &ts_val, &ts_ecr);

#ifdef TCP_SACK
	if (!tp->sack_disable) {
		tp->rcv_laststart = th->th_seq; /* last rec'vd segment*/
		tp->rcv_lastend = th->th_seq + tlen;
	}
#endif /* TCP_SACK */
	/* 
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    (!ts_present || TSTMP_GEQ(ts_val, tp->ts_recent)) &&
	    th->th_seq == tp->rcv_nxt &&
	    tiwin && tiwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {

		/* 
		 * If last ACK falls within this segment's sequence numbers,
		 *  record the timestamp.
		 * Fix from Braden, see Stevens p. 870
		 */
		if (ts_present && SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = tcp_now;
			tp->ts_recent = ts_val;
		}

		if (tlen == 0) {
			if (SEQ_GT(th->th_ack, tp->snd_una) &&
			    SEQ_LEQ(th->th_ack, tp->snd_max) &&
			    tp->snd_cwnd >= tp->snd_wnd &&
			    tp->t_dupacks == 0) {
				/*
				 * this is a pure ack for outstanding data.
				 */
				++tcpstat.tcps_predack;
				if (ts_present)
					tcp_xmit_timer(tp, tcp_now-ts_ecr+1);
				else if (tp->t_rtt &&
					    SEQ_GT(th->th_ack, tp->t_rtseq))
					tcp_xmit_timer(tp, tp->t_rtt);
				acked = th->th_ack - tp->snd_una;
				tcpstat.tcps_rcvackpack++;
				tcpstat.tcps_rcvackbyte += acked;
				sbdrop(&so->so_snd, acked);
				tp->snd_una = th->th_ack;
#if defined(TCP_SACK) && defined(TCP_FACK)
				tp->snd_fack = tp->snd_una;
				tp->retran_data = 0;
#endif /* TCP_FACK */
				m_freem(m);

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
				if (tp->snd_una == tp->snd_max)
					tp->t_timer[TCPT_REXMT] = 0;
				else if (tp->t_timer[TCPT_PERSIST] == 0)
					tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

				if (sb_notify(&so->so_snd))
					sowwakeup(so);
				if (so->so_snd.sb_cc)
					(void) tcp_output(tp);
				return;
			}
		} else if (th->th_ack == tp->snd_una &&
		    tp->segq.lh_first == NULL &&
		    tlen <= sbspace(&so->so_rcv)) {
			/*
			 * This is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */
#ifdef TCP_SACK
			/* Clean receiver SACK report if present */
			if (!tp->sack_disable && tp->rcv_numsacks)
				tcp_clean_sackreport(tp);
#endif /* TCP_SACK */
			++tcpstat.tcps_preddat;
			tp->rcv_nxt += tlen;
			tcpstat.tcps_rcvpack++;
			tcpstat.tcps_rcvbyte += tlen;
			/*
			 * Drop TCP, IP headers and TCP options then add data
			 * to socket buffer.
			 */
			m->m_data += iphlen + off;
			m->m_len -= iphlen + off;
			sbappend(&so->so_rcv, m);
			sorwakeup(so);
			if (th->th_flags & TH_PUSH)
				tp->t_flags |= TF_ACKNOW;
			else
				tp->t_flags |= TF_DELACK;
			return;
		}
	}

	/*
	 * Drop TCP, IP headers and TCP options.
	 */
	m->m_data += iphlen + off;
	m->m_len  -= iphlen + off;

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	{ int win;

	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));
	}

	switch (tp->t_state) {

	/*
	 * If the state is LISTEN then ignore segment if it contains an RST.
	 * If the segment contains an ACK then it is bad and send a RST.
	 * If it does not contain a SYN then it is not interesting; drop it.
	 * If it is from this socket, drop it, it must be forged.
	 * Don't bother responding if the destination was a broadcast.
	 * Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
	 * tp->iss, and send a segment:
	 *     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
	 * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
	 * Fill in remote peer address fields if not previously specified.
	 * Enter SYN_RECEIVED state, and process any other fields of this
	 * segment in this state.
	 */
	case TCPS_LISTEN: {
		struct mbuf *am;
		register struct sockaddr_in *sin;
#ifdef INET6
		register struct sockaddr_in6 *sin6;
#endif /* INET6 */

		if (tiflags & TH_RST)
			goto drop;
		if (tiflags & TH_ACK)
			goto dropwithreset;
		if ((tiflags & TH_SYN) == 0)
			goto drop;
		if (th->th_dport == th->th_sport) {
#ifdef INET6
		  if (is_ipv6) {
		    if (IN6_ARE_ADDR_EQUAL(&ipv6->ipv6_src, &ipv6->ipv6_dst))
		      goto drop;
		  } else {
#endif /* INET6 */
		    if (ti->ti_dst.s_addr == ti->ti_src.s_addr)
		      goto drop;
#ifdef INET6
		  }
#endif /* INET6 */
		}

#ifdef TCPCOOKIE
		/*
		 * If source address is on friends list, proceed, otherwise
		 * try to obtain a cookie and drop the frame.
		 */
		
		if (!tck_isafriend(ti->ti_src)) {
			u_int32_t acookie;

			acookie = tck_makecookie(ti->ti_src);
			ti->ti_dport = htons(TCK_PORT);
			tcp_respond(tp, ti, m, acookie, acookie, TH_ACK);
			/* destroy temporarily created socket */
			if (dropsocket)
				(void) soabort(so);
			return;
		}
#endif /* TCPCOOKIE */

		/*
		 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
		 * in_broadcast() should never return true on a received
		 * packet with M_BCAST not set.
		 */
		if (m->m_flags & (M_BCAST|M_MCAST))
		  goto drop;
#ifdef INET6
		if (is_ipv6) {
			/* XXX What about IPv6 Anycasting ?? :-(  rja */
			if (IN6_IS_ADDR_MULTICAST(&ipv6->ipv6_dst))
				goto drop;
		} else
#endif /* INET6 */
		if (IN_MULTICAST(ti->ti_dst.s_addr))
			goto drop;
		am = m_get(M_DONTWAIT, MT_SONAME);	/* XXX */
		if (am == NULL)
			goto drop;
#ifdef INET6
		if (is_ipv6) {
		  /*
		   * This is probably the place to set the tp->pf value.
		   * (Don't forget to do it in the v4 code as well!)
		   *
		   * Also, remember to blank out things like flowlabel, or
		   * set flowlabel for accepted sockets in v6.
		   *
		   * FURTHERMORE, this is PROBABLY the place where the whole
		   * business of key munging is set up for passive
		   * connections.
		   */
		  am->m_len = sizeof(struct sockaddr_in6);
		  sin6 = mtod(am, struct sockaddr_in6 *);
		  sin6->sin6_family = AF_INET6;
		  sin6->sin6_len = sizeof(struct sockaddr_in6);
		  sin6->sin6_addr = ipv6->ipv6_src;
		  sin6->sin6_port = th->th_sport;
		  sin6->sin6_flowinfo = htonl(0x0fffffff) &
		    inp->inp_ipv6.ipv6_versfl;
		  laddr6 = inp->inp_laddr6;
		  if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
		    inp->inp_laddr6 = ipv6->ipv6_dst;
		  /* This is a good optimization. */
		  if (in6_pcbconnect(inp, am)) {
		    inp->inp_laddr6 = laddr6;
		    (void) m_free(am);
		    goto drop;
		  } /* endif in6_pcbconnect() */
		  tp->pf = PF_INET6;
		} else {
		  /*
		   * Letting v4 incoming datagrams to reach valid 
		   * PF_INET6 sockets causes some overhead here.
		   */
		  if (inp->inp_flags & INP_IPV6) {
		    if (!(inp->inp_flags & (INP_IPV6_UNDEC|INP_IPV6_MAPPED))) {
		      (void) m_free(am);
		      goto drop;
		    }

		    am->m_len = sizeof(struct sockaddr_in6);
		    
		    sin6 = mtod(am, struct sockaddr_in6 *);
		    sin6->sin6_family = AF_INET6;
		    sin6->sin6_len = sizeof(*sin6);
		    CREATE_IPV6_MAPPED(sin6->sin6_addr, ti->ti_src.s_addr);
		    sin6->sin6_port = th->th_sport;
		    sin6->sin6_flowinfo = 0;

		    laddr6 = inp->inp_laddr6;
		    if (inp->inp_laddr.s_addr == INADDR_ANY)
		      CREATE_IPV6_MAPPED(inp->inp_laddr6, ti->ti_dst.s_addr);
		    
		    /*
		     * The pcb initially has the v6 default hoplimit
		     * set. We're sending v4 packets so we need to set
		     * the v4 ttl and tos.
		     */
		    inp->inp_ip.ip_ttl = ip_defttl;
		    inp->inp_ip.ip_tos = 0;
		    
		    if (in6_pcbconnect(inp, am)) {
		      inp->inp_laddr6 = laddr6;
		      (void) m_freem(am);
		      goto drop;
		    }
		    tp->pf = PF_INET;
		  } else { 
#endif /* INET6 */
		am->m_len = sizeof (struct sockaddr_in);
		sin = mtod(am, struct sockaddr_in *);
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ti->ti_src;
		sin->sin_port = ti->ti_sport;
		bzero((caddr_t)sin->sin_zero, sizeof(sin->sin_zero));
		laddr = inp->inp_laddr;
		if (inp->inp_laddr.s_addr == INADDR_ANY)
			inp->inp_laddr = ti->ti_dst;
		if (in_pcbconnect(inp, am)) {
			inp->inp_laddr = laddr;
			(void) m_free(am);
			goto drop;
		}
		(void) m_free(am);
#ifdef INET6
		  }  /* if (inp->inp_flags & INP_IPV6) */
		} /* if (is_ipv6) */
#endif /* INET6 */
		tp->t_template = tcp_template(tp);
		if (tp->t_template == 0) {
			tp = tcp_drop(tp, ENOBUFS);
			dropsocket = 0;		/* socket is already gone */
			goto drop;
		}
		if (optp)
			tcp_dooptions(tp, optp, optlen, th,
				&ts_present, &ts_val, &ts_ecr);
#ifdef TCP_SACK
		/*
		 * If peer did not send a SACK_PERMITTED option (i.e., if
		 * tcp_dooptions() did not set TF_SACK_PERMIT), set 
                 * sack_disable to 1 if it is currently 0.
                 */
                if (!tp->sack_disable)
                        if ((tp->t_flags & TF_SACK_PERMIT) == 0) 
                                tp->sack_disable = 1;
#endif

		if (iss)
			tp->iss = iss;
		else
			tp->iss = tcp_iss;
#ifdef TCP_COMPAT_42
		tcp_iss += TCP_ISSINCR/2;
#else /* TCP_COMPAT_42 */
		tcp_iss += arc4random() % (TCP_ISSINCR / 2) + 1;
#endif /* !TCP_COMPAT_42 */
		tp->irs = th->th_seq;
		tcp_sendseqinit(tp);
#if defined (TCP_SACK) || defined (TCP_NEWRENO)
		tp->snd_last = tp->snd_una;
#endif /* TCP_SACK || TCP_NEWRENO */
#if defined(TCP_SACK) && defined(TCP_FACK)
		tp->snd_fack = tp->snd_una;
		tp->retran_data = 0;
		tp->snd_awnd = 0;
#endif /* TCP_FACK */
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		tp->t_state = TCPS_SYN_RECEIVED;
		tp->t_timer[TCPT_KEEP] = tcptv_keep_init;
		dropsocket = 0;		/* committed to socket */
		tcpstat.tcps_accepts++;
		goto trimthenstep6;
		}

	/*
	 * If the state is SYN_RECEIVED:
	 * 	if seg contains SYN/ACK, send an RST.
	 *	if seg contains an ACK, but not for our SYN/ACK, send an RST
  	 */

	case TCPS_SYN_RECEIVED:
		if (tiflags & TH_ACK) {
			if (tiflags & TH_SYN) {
				tcpstat.tcps_badsyn++;
				goto dropwithreset;
			}
			if (SEQ_LEQ(th->th_ack, tp->snd_una) ||
			    SEQ_GT(th->th_ack, tp->snd_max))
				goto dropwithreset;
		}
		break;

	/*
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if ((tiflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		     SEQ_GT(th->th_ack, tp->snd_max)))
			goto dropwithreset;
		if (tiflags & TH_RST) {
			if (tiflags & TH_ACK)
				tp = tcp_drop(tp, ECONNREFUSED);
			goto drop;
		}
		if ((tiflags & TH_SYN) == 0)
			goto drop;
		if (tiflags & TH_ACK) {
			tp->snd_una = th->th_ack;
			if (SEQ_LT(tp->snd_nxt, tp->snd_una))
				tp->snd_nxt = tp->snd_una;
		}
		tp->t_timer[TCPT_REXMT] = 0;
		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
#ifdef TCP_SACK
                /*
                 * If we've sent a SACK_PERMITTED option, and the peer
                 * also replied with one, then TF_SACK_PERMIT should have
                 * been set in tcp_dooptions().  If it was not, disable SACKs.
                 */
                if (!tp->sack_disable)
                        if ((tp->t_flags & TF_SACK_PERMIT) == 0) 
                                tp->sack_disable = 1;
#endif
		if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)) {
			tcpstat.tcps_connects++;
			soisconnected(so);
			tp->t_state = TCPS_ESTABLISHED;
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
			(void) tcp_reass(tp, (struct tcphdr *)0,
				(struct mbuf *)0, &tlen);
			/*
			 * if we didn't have to retransmit the SYN,
			 * use its rtt as our initial srtt & rtt var.
			 */
			if (tp->t_rtt)
				tcp_xmit_timer(tp, tp->t_rtt);
			/*
			 * Since new data was acked (the SYN), open the
			 * congestion window by one MSS.  We do this
			 * here, because we won't go through the normal
			 * ACK processing below.  And since this is the
			 * start of the connection, we know we are in
			 * the exponential phase of slow-start.
			 */
			tp->snd_cwnd += tp->t_maxseg;
		} else
			tp->t_state = TCPS_SYN_RECEIVED;

trimthenstep6:
		/*
		 * Advance ti->ti_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		th->th_seq++;
		if (tlen > tp->rcv_wnd) {
			todrop = tlen - tp->rcv_wnd;
			m_adj(m, -todrop);
			tlen = tp->rcv_wnd;
			tiflags &= ~TH_FIN;
			tcpstat.tcps_rcvpackafterwin++;
			tcpstat.tcps_rcvbyteafterwin += todrop;
		}
		tp->snd_wl1 = th->th_seq - 1;
		tp->rcv_up = th->th_seq;
		goto step6;
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check timestamp, if present.
	 * Then check that at least some bytes of segment are within 
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 * 
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if (ts_present && (tiflags & TH_RST) == 0 && tp->ts_recent &&
	    TSTMP_LT(ts_val, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if ((int)(tcp_now - tp->ts_recent_age) > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += tlen;
			tcpstat.tcps_pawsdrop++;
			goto dropafterack;
		}
	}

	todrop = tp->rcv_nxt - th->th_seq;
	if (todrop > 0) {
		if (tiflags & TH_SYN) {
			tiflags &= ~TH_SYN;
			th->th_seq++;
			if (th->th_urp > 1) 
				th->th_urp--;
			else
				tiflags &= ~TH_URG;
			todrop--;
		}
		if (todrop >= tlen ||
		    (todrop == tlen && (tiflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN must be to the left of the
			 * window.  At this point, FIN must be a
			 * duplicate or out-of-sequence, so drop it.
			 */
			tiflags &= ~TH_FIN;
			/*
			 * Send ACK to resynchronize, and drop any data,
			 * but keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			tcpstat.tcps_rcvdupbyte += todrop = tlen;
			tcpstat.tcps_rcvduppack++;
		} else {
			tcpstat.tcps_rcvpartduppack++;
			tcpstat.tcps_rcvpartdupbyte += todrop;
		}
		m_adj(m, todrop);
		th->th_seq += todrop;
		tlen -= todrop;
		if (th->th_urp > todrop)
			th->th_urp -= todrop;
		else {
			tiflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && tlen) {
		tp = tcp_close(tp);
		tcpstat.tcps_rcvafterclose++;
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt+tp->rcv_wnd);
	if (todrop > 0) {
		tcpstat.tcps_rcvpackafterwin++;
		if (todrop >= tlen) {
			tcpstat.tcps_rcvbyteafterwin += tlen;
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 */
			if (tiflags & TH_SYN &&
			    tp->t_state == TCPS_TIME_WAIT &&
			    SEQ_GT(th->th_seq, tp->rcv_nxt)) {
				iss = tp->rcv_nxt + TCP_ISSINCR;
				tp = tcp_close(tp);
				goto findpcb;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				tcpstat.tcps_rcvwinprobe++;
			} else
				goto dropafterack;
		} else
			tcpstat.tcps_rcvbyteafterwin += todrop;
		m_adj(m, -todrop);
		tlen -= todrop;
		tiflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 * Fix from Braden, see Stevens p. 870
	 */
	if (ts_present && TSTMP_GEQ(ts_val, tp->ts_recent) &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
		tp->ts_recent_age = tcp_now;
		tp->ts_recent = ts_val;
	}

	/*
	 * If the RST bit is set examine the state:
	 *    SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *    CLOSING, LAST_ACK, TIME_WAIT STATES
	 *	Close the tcb.
	 */
	if (tiflags & TH_RST) {
		if ((th->th_seq != tp->rcv_nxt) &&
		    (th->th_ack && ((SEQ_GT(th->th_ack, tp->snd_nxt) ||
		      SEQ_LT(th->th_ack, (tp->snd_nxt - tp->snd_wnd))))))
			goto drop;

		switch (tp->t_state) {
		case TCPS_SYN_RECEIVED:
			so->so_error = ECONNREFUSED;
			goto close;

		case TCPS_ESTABLISHED:
		case TCPS_FIN_WAIT_1:
		case TCPS_FIN_WAIT_2:
		case TCPS_CLOSE_WAIT:
			so->so_error = ECONNRESET;
		close:
			tp->t_state = TCPS_CLOSED;
			tcpstat.tcps_drops++;
			tp = tcp_close(tp);
			goto drop;
		case TCPS_CLOSING:
		case TCPS_LAST_ACK:
		case TCPS_TIME_WAIT:
			tp = tcp_close(tp);
			goto drop;
		}
	}

	/*
	 * If a SYN is in the window, then this is an
	 * error and we send an RST and drop the connection.
	 */
	if (tiflags & TH_SYN) {
		tp = tcp_drop(tp, ECONNRESET);
		goto dropwithreset;
	}

	/*
	 * If the ACK bit is off we drop the segment and return.
	 */
	if ((tiflags & TH_ACK) == 0)
		goto drop;
	
	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {

	/*
	 * In SYN_RECEIVED state, the ack ACKs our SYN, so enter
	 * ESTABLISHED state and continue processing.
	 * The ACK was checked above.
	 */
	case TCPS_SYN_RECEIVED:
		tcpstat.tcps_connects++;
		soisconnected(so);
		tp->t_state = TCPS_ESTABLISHED;
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		(void) tcp_reass(tp, (struct tcphdr *)0, (struct mbuf *)0,
				 &tlen);
		tp->snd_wl1 = th->th_seq - 1;
		/* fall into ... */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < ti->ti_ack <= tp->snd_max
	 * then advance tp->snd_una to ti->ti_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			/*
			 * Duplicate/old ACK processing.
			 * Increments t_dupacks:
			 *	Pure duplicate (same seq/ack/window, no data)
			 * Doesn't affect t_dupacks:
			 *	Data packets.
			 *	Normal window updates (window opens)
			 * Resets t_dupacks:
			 *	New data ACKed.
			 *	Window shrinks
			 *	Old ACK
			 */
			if (tlen)
				break;
			/*
			 * If we get an old ACK, there is probably packet
			 * reordering going on.  Be conservative and reset
			 * t_dupacks so that we are less agressive in
			 * doing a fast retransmit.
			 */
			if (th->th_ack != tp->snd_una) {
				tp->t_dupacks = 0;
				break;
			}
			if (tiwin == tp->snd_wnd) {
				tcpstat.tcps_rcvdupack++;
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change), the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 *
				 * We know we're losing at the current
				 * window size so do congestion avoidance
				 * (set ssthresh to half the current window
				 * and pull our congestion window back to
				 * the new ssthresh).
				 *
				 * Dup acks mean that packets have left the
				 * network (they're now cached at the receiver) 
				 * so bump cwnd by the amount in the receiver
				 * to keep a constant cwnd packets in the
				 * network.
				 */
				if (tp->t_timer[TCPT_REXMT] == 0)
					tp->t_dupacks = 0;
#if defined(TCP_SACK) && defined(TCP_FACK)
				/* 
				 * In FACK, can enter fast rec. if the receiver
				 * reports a reass. queue longer than 3 segs.
				 */
				else if (++tp->t_dupacks == tcprexmtthresh ||
				    ((SEQ_GT(tp->snd_fack, tcprexmtthresh * 
				    tp->t_maxseg + tp->snd_una)) &&
				    SEQ_GT(tp->snd_una, tp->snd_last))) {
#else
				else if (++tp->t_dupacks == tcprexmtthresh) {
#endif /* TCP_FACK */
					tcp_seq onxt = tp->snd_nxt;
					u_long win =
					    ulmin(tp->snd_wnd, tp->snd_cwnd) /
						2 / tp->t_maxseg;

#if defined(TCP_SACK) || defined(TCP_NEWRENO) 
					if (SEQ_LT(th->th_ack, tp->snd_last)){
					    	/* 
						 * False fast retx after 
						 * timeout.  Do not cut window.
						 */
						tp->snd_cwnd += tp->t_maxseg;
						tp->t_dupacks = 0;
						(void) tcp_output(tp); 
						goto drop;
					}
#endif
					if (win < 2)
						win = 2;
					tp->snd_ssthresh = win * tp->t_maxseg;
#if defined(TCP_SACK) || defined(TCP_NEWRENO)
					tp->snd_last = tp->snd_max;
#endif
#ifdef TCP_SACK
                    			if (!tp->sack_disable) {
						tp->t_timer[TCPT_REXMT] = 0;
						tp->t_rtt = 0;
						tcpstat.tcps_sndrexmitfast++;
#if defined(TCP_SACK) && defined(TCP_FACK) 
						(void) tcp_output(tp);
						/*
						 * During FR, snd_cwnd is held
						 * constant for FACK.
						 */
						tp->snd_cwnd = tp->snd_ssthresh;
						tp->t_dupacks = tcprexmtthresh;
#else
						/* 
						 * tcp_output() will send
						 * oldest SACK-eligible rtx.
						 */
						(void) tcp_output(tp);
						tp->snd_cwnd = tp->snd_ssthresh+
					           tp->t_maxseg * tp->t_dupacks;
#endif /* TCP_FACK */
						/* 
						 * It is possible for 
						 * tcp_output to fail to send
						 * a segment.  If so, make 
						 * sure that REMXT timer is set.
						 */ 
						if (SEQ_GT(tp->snd_max, 
						    tp->snd_una) &&
                        			tp->t_timer[TCPT_REXMT] == 0 &&
                        			tp->t_timer[TCPT_PERSIST] == 0)
                        			tp->t_timer[TCPT_REXMT] = 
						    tp->t_rxtcur;
						goto drop;
					}
#endif /* TCP_SACK */
					tp->t_timer[TCPT_REXMT] = 0;
					tp->t_rtt = 0;
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = tp->t_maxseg;
					tcpstat.tcps_sndrexmitfast++;
					(void) tcp_output(tp);

					tp->snd_cwnd = tp->snd_ssthresh +
					    tp->t_maxseg * tp->t_dupacks;
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (tp->t_dupacks > tcprexmtthresh) {
#if defined(TCP_SACK) && defined(TCP_FACK)
					/* 
					 * while (awnd < cwnd) 
					 *         sendsomething(); 
					 */
					if (!tp->sack_disable) {
						if (tp->snd_awnd < tp->snd_cwnd)
							tcp_output(tp);
						goto drop;
					}
#endif /* TCP_FACK */
					tp->snd_cwnd += tp->t_maxseg;
					(void) tcp_output(tp);
					goto drop;
				}
			} else if (tiwin < tp->snd_wnd) {
				/*
				 * The window was retracted!  Previous dup
				 * ACKs may have been due to packets arriving
				 * after the shrunken window, not a missing
				 * packet, so play it safe and reset t_dupacks
				 */
				tp->t_dupacks = 0;
			}
			break;
		}
		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
#ifdef TCP_NEWRENO
		if (tp->t_dupacks >= tcprexmtthresh && !tcp_newreno(tp, th)) {
			/* Out of fast recovery */
			tp->snd_cwnd = tp->snd_ssthresh;
			/* 
			 * Window inflation should have left us with approx.
			 * snd_ssthresh outstanding data.  But in case we
			 * would be inclined to send a burst, better to do
			 * it via the slow start mechanism.
			 */
			if (tcp_seq_subtract(tp->snd_max, th->th_ack) <
			    tp->snd_ssthresh)
				tp->snd_cwnd = tcp_seq_subtract(tp->snd_max,
				    th->th_ack) + tp->t_maxseg;	
			tp->t_dupacks = 0;
		}
#elif defined(TCP_SACK)
		if (!tp->sack_disable) {
			if (tp->t_dupacks >= tcprexmtthresh) {
				/* Check for a partial ACK */
				if (tcp_sack_partialack(tp, th)) {
#if defined(TCP_SACK) && defined(TCP_FACK)
					/* Force call to tcp_output */
					if (tp->snd_awnd < tp->snd_cwnd) 
						needoutput = 1;
#else
					tp->snd_cwnd += tp->t_maxseg;
					needoutput = 1;
#endif /* TCP_FACK */
				} else {
					/* Out of fast recovery */
					tp->snd_cwnd = tp->snd_ssthresh;
					if (tcp_seq_subtract(tp->snd_max, 
					    th->th_ack) < tp->snd_ssthresh)
						tp->snd_cwnd = 
						   tcp_seq_subtract(tp->snd_max,
					           th->th_ack) + tp->t_maxseg;
					tp->t_dupacks = 0;
#if defined(TCP_SACK) && defined(TCP_FACK)
					if (SEQ_GT(th->th_ack, tp->snd_fack))
						tp->snd_fack = th->th_ack;
#endif /* TCP_FACK */
				}
			} 
		} else {
			if (tp->t_dupacks >= tcprexmtthresh && 
			    !tcp_newreno(tp, th)) {
				/* Out of fast recovery */
				tp->snd_cwnd = tp->snd_ssthresh;
				if (tcp_seq_subtract(tp->snd_max, th->th_ack) <
			  	    tp->snd_ssthresh)
					tp->snd_cwnd = 
					    tcp_seq_subtract(tp->snd_max,
					    th->th_ack) + tp->t_maxseg;
				tp->t_dupacks = 0;
			}
		}
#else /* else neither TCP_NEWRENO nor TCP_SACK */
		if (tp->t_dupacks >= tcprexmtthresh &&
		    tp->snd_cwnd > tp->snd_ssthresh)
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_dupacks = 0;
#endif
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			tcpstat.tcps_rcvacktoomuch++;
			goto dropafterack;
		}
		acked = th->th_ack - tp->snd_una;
		tcpstat.tcps_rcvackpack++;
		tcpstat.tcps_rcvackbyte += acked;

		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 */
		if (ts_present)
			tcp_xmit_timer(tp, tcp_now-ts_ecr+1);
		else if (tp->t_rtt && SEQ_GT(th->th_ack, tp->t_rtseq))
			tcp_xmit_timer(tp,tp->t_rtt);

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			tp->t_timer[TCPT_REXMT] = 0;
			needoutput = 1;
		} else if (tp->t_timer[TCPT_PERSIST] == 0)
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
		/*
		 * When new data is acked, open the congestion window.
		 * If the window gives us less than ssthresh packets
		 * in flight, open exponentially (maxseg per packet).
		 * Otherwise open linearly: maxseg per window
		 * (maxseg^2 / cwnd per packet).
		 */
		{
		register u_int cw = tp->snd_cwnd;
		register u_int incr = tp->t_maxseg;

		if (cw > tp->snd_ssthresh)
			incr = incr * incr / cw;
#if defined (TCP_NEWRENO) || defined (TCP_SACK)
		if (SEQ_GEQ(th->th_ack, tp->snd_last)) 
#endif
		tp->snd_cwnd = min(cw + incr, TCP_MAXWIN<<tp->snd_scale);
		}
		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop(&so->so_snd, (int)so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			sbdrop(&so->so_snd, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		if (sb_notify(&so->so_snd))
			sowwakeup(so);
		tp->snd_una = th->th_ack;
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;
#if defined (TCP_SACK) && defined (TCP_FACK)
		if (SEQ_GT(tp->snd_una, tp->snd_fack))
			tp->snd_fack = tp->snd_una;
#endif

		switch (tp->t_state) {

		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				/*
				 * If we can't receive any more
				 * data, then closing user can proceed.
				 * Starting the timer is contrary to the
				 * specification, but if we don't get a FIN
				 * we'll hang forever.
				 */
				if (so->so_state & SS_CANTRCVMORE) {
					soisdisconnected(so);
					tp->t_timer[TCPT_2MSL] = tcp_maxidle;
				}
				tp->t_state = TCPS_FIN_WAIT_2;
			}
			break;

		/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
				tp->t_state = TCPS_TIME_WAIT;
				tcp_canceltimers(tp);
				tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
				soisdisconnected(so);
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				tp = tcp_close(tp);
				goto drop;
			}
			break;

		/*
		 * In TIME_WAIT state the only thing that should arrive
		 * is a retransmission of the remote FIN.  Acknowledge
		 * it and restart the finack timer.
		 */
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			goto dropafterack;
		}
	}

step6:
	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if (((tiflags & TH_ACK) && SEQ_LT(tp->snd_wl1, th->th_seq)) ||
	    (tp->snd_wl1 == th->th_seq && SEQ_LT(tp->snd_wl2, th->th_ack)) ||
	    (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)) {
		/* keep track of pure window updates */
		if (tlen == 0 &&
		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
			tcpstat.tcps_rcvwinupd++;
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((tiflags & TH_URG) && th->th_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * This is a kludge, but if we receive and accept
		 * random urgent pointers, we'll crash in
		 * soreceive.  It's hard to imagine someone
		 * actually wanting to send this much urgent data.
		 */
		if (th->th_urp + so->so_rcv.sb_cc > sb_max) {
			th->th_urp = 0;			/* XXX */
			tiflags &= ~TH_URG;		/* XXX */
			goto dodata;			/* XXX */
		}
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side. 
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original 
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(th->th_seq+th->th_urp, tp->rcv_up)) {
			tp->rcv_up = th->th_seq + th->th_urp;
			so->so_oobmark = so->so_rcv.sb_cc +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0)
				so->so_state |= SS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (th->th_urp <= (u_int16_t) tlen
#ifdef SO_OOBINLINE
		     && (so->so_options & SO_OOBINLINE) == 0
#endif
		     )
		        tcp_pulloutofband(so, th->th_urp, m); /* XXX? */
	} else
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
dodata:							/* XXX */

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((tlen || (tiflags & TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
	  if (th->th_seq == tp->rcv_nxt && tp->segq.lh_first == NULL &&
	      tp->t_state == TCPS_ESTABLISHED) {
	    if (th->th_flags & TH_PUSH)
	      tp->t_flags |= TF_ACKNOW;
	    else
	      tp->t_flags |= TF_DELACK;
	    (tp)->rcv_nxt += tlen;
	    tiflags = th->th_flags & TH_FIN;
	    tcpstat.tcps_rcvpack++;
	    tcpstat.tcps_rcvbyte += tlen;
	    sbappend(&so->so_rcv, m);
	    sorwakeup(so);
	  } else {
	    tiflags = tcp_reass(tp, th, m, &tlen);
	    tp->t_flags |= TF_ACKNOW;
	  }
#ifdef TCP_SACK
		if (!tp->sack_disable)
			tcp_update_sack_list(tp); 
#endif 

	  /* 
	   * variable len never referenced again in modern BSD,
	   * so why bother computing it ??
	   */
#if 0
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 */
		len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
#endif /* 0 */
	} else {
		m_freem(m);
		tiflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.  Ignore a FIN received before
	 * the connection is fully established.
	 */
	if ((tiflags & TH_FIN) && TCPS_HAVEESTABLISHED(tp->t_state)) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {

		/*
		 * In ESTABLISHED STATE enter the CLOSE_WAIT state.
		 */
		case TCPS_ESTABLISHED:
			tp->t_state = TCPS_CLOSE_WAIT;
			break;

		/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tp->t_state = TCPS_CLOSING;
			break;

		/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other 
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			tp->t_state = TCPS_TIME_WAIT;
			tcp_canceltimers(tp);
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			soisdisconnected(so);
			break;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			break;
		}
	}
	if (so->so_options & SO_DEBUG) {
#ifdef INET6
		if (tp->pf == PF_INET6)
			tcp_trace(TA_INPUT, ostate, tp, (struct tcpiphdr *) &tcp_saveti6, 0, tlen);
		else
#endif /* INET6 */
			tcp_trace(TA_INPUT, ostate, tp, &tcp_saveti, 0, tlen);
	}

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW)) {
		(void) tcp_output(tp);
#ifdef TCP_SACK
	/* 
	 * In SACK, it is possible for tcp_output() to fail to send a segment 
	 * after the retransmission timer has been turned off.  Make sure that
	 * the retransmission timer is set if we are in fast recovery. 
	 */
		if (needoutput && SEQ_GT(tp->snd_max, tp->snd_una) && 
		    tp->t_timer[TCPT_REXMT] == 0 && 
		    tp->t_timer[TCPT_PERSIST] == 0)
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
#endif
	}
	return;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 */
	if (tiflags & TH_RST)
		goto drop;
	m_freem(m);
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
	return;

dropwithreset:
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was broadcast/multicast.
	 */
	if ((tiflags & TH_RST) || m->m_flags & (M_BCAST|M_MCAST))
	  goto drop;
#ifdef INET6
	if (is_ipv6) {
	  /* For following calls to tcp_respond */
	  ti = mtod(m, struct tcpiphdr *);
	  if (IN6_IS_ADDR_MULTICAST(&ipv6->ipv6_dst))
	    goto drop;
	} else {
#endif /* INET6 */
	    if (IN_MULTICAST(ti->ti_dst.s_addr))
	      goto drop;
#ifdef INET6
	}
#endif /* INET6 */
	if (tiflags & TH_ACK)
		tcp_respond(tp, ti, m, (tcp_seq)0, th->th_ack, TH_RST);
	else {
		if (tiflags & TH_SYN)
			tlen++;
		tcp_respond(tp, ti, m, th->th_seq+tlen, (tcp_seq)0,
		    TH_RST|TH_ACK);
	}
	/* destroy temporarily created socket */
	if (dropsocket)
		(void) soabort(so);
	return;

drop:
	/*
	 * Drop space held by incoming segment and return.
	 */
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG)) {
#ifdef INET6
	  if (tp->pf == PF_INET6)
	    tcp_trace(TA_DROP, ostate, tp, (struct tcpiphdr *)&tcp_saveti6, 0, tlen);
	  else
#endif /* INET6 */
	    tcp_trace(TA_DROP, ostate, tp, &tcp_saveti, 0, tlen);
	}

	m_freem(m);
	/* destroy temporarily created socket */
	if (dropsocket)
		(void) soabort(so);
	return;
#ifndef TUBA_INCLUDE
}

void
tcp_dooptions(tp, cp, cnt, th, ts_present, ts_val, ts_ecr)
	struct tcpcb *tp;
	u_char *cp;
	int cnt;
	struct tcphdr *th;
	int *ts_present;
	u_int32_t *ts_val, *ts_ecr;
{
	u_int16_t mss = 0;
	int opt, optlen;

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[1];
			if (optlen <= 0)
				break;
		}
		switch (opt) {

		default:
			continue;

		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG)
				continue;
			if (!(th->th_flags & TH_SYN))
				continue;
			bcopy((char *) cp + 2, (char *) &mss, sizeof(mss));
			NTOHS(mss);
			break;

		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (!(th->th_flags & TH_SYN))
				continue;
			tp->t_flags |= TF_RCVD_SCALE;
			tp->requested_s_scale = min(cp[2], TCP_MAX_WINSHIFT);
			break;

		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			*ts_present = 1;
			bcopy((char *)cp + 2, (char *) ts_val, sizeof(*ts_val));
			NTOHL(*ts_val);
			bcopy((char *)cp + 6, (char *) ts_ecr, sizeof(*ts_ecr));
			NTOHL(*ts_ecr);

			/* 
			 * A timestamp received in a SYN makes
			 * it ok to send timestamp requests and replies.
			 */
			if (th->th_flags & TH_SYN) {
				tp->t_flags |= TF_RCVD_TSTMP;
				tp->ts_recent = *ts_val;
				tp->ts_recent_age = tcp_now;
			}
			break;
		
#ifdef TCP_SACK 
		case TCPOPT_SACK_PERMITTED:
			if (tp->sack_disable || optlen!=TCPOLEN_SACK_PERMITTED)
				continue;
			if (th->th_flags & TH_SYN)
				/* MUST only be set on SYN */
				tp->t_flags |= TF_SACK_PERMIT;
			break;
		case TCPOPT_SACK:
			if (tcp_sack_option(tp, th, cp, optlen))
				continue;
			break;
#endif          
		}
	}
	/* Update t_maxopd and t_maxseg after all options are processed */
	if (th->th_flags & TH_SYN)
		(void) tcp_mss(tp, mss);	/* sets t_maxseg */
}

#if defined(TCP_SACK) || defined(TCP_NEWRENO)
u_long 
tcp_seq_subtract(a, b)
	u_long a, b;
{ 
	return ((long)(a - b)); 
}
#endif


#ifdef TCP_SACK 
/*
 * This function is called upon receipt of new valid data (while not in header
 * prediction mode), and it updates the ordered list of sacks. 
 */
void 
tcp_update_sack_list(tp)
	struct tcpcb *tp; 
{    
	/* 
	 * First reported block MUST be the most recent one.  Subsequent
	 * blocks SHOULD be in the order in which they arrived at the
	 * receiver.  These two conditions make the implementation fully
	 * compliant with RFC 2018.
	 */     
	int i, j = 0, count = 0, lastpos = -1;
	struct sackblk sack, firstsack, temp[MAX_SACK_BLKS];
    
	/* First clean up current list of sacks */
	for (i = 0; i < tp->rcv_numsacks; i++) {
		sack = tp->sackblks[i];
		if (sack.start == 0 && sack.end == 0) {
			count++; /* count = number of blocks to be discarded */
			continue;
		}
		if (SEQ_LEQ(sack.end, tp->rcv_nxt)) {
			tp->sackblks[i].start = tp->sackblks[i].end = 0;
			count++;
		} else { 
			temp[j].start = tp->sackblks[i].start;
			temp[j++].end = tp->sackblks[i].end;
		}
	}   
	tp->rcv_numsacks -= count;
	if (tp->rcv_numsacks == 0) { /* no sack blocks currently (fast path) */
		tcp_clean_sackreport(tp);
		if (SEQ_LT(tp->rcv_nxt, tp->rcv_laststart)) {
			/* ==> need first sack block */
			tp->sackblks[0].start = tp->rcv_laststart;
			tp->sackblks[0].end = tp->rcv_lastend;
			tp->rcv_numsacks = 1;
		}
		return;
	}
	/* Otherwise, sack blocks are already present. */
	for (i = 0; i < tp->rcv_numsacks; i++)
		tp->sackblks[i] = temp[i]; /* first copy back sack list */
	if (SEQ_GEQ(tp->rcv_nxt, tp->rcv_lastend)) 
		return;     /* sack list remains unchanged */
	/* 
	 * From here, segment just received should be (part of) the 1st sack.
	 * Go through list, possibly coalescing sack block entries.
	 */
	firstsack.start = tp->rcv_laststart;
	firstsack.end = tp->rcv_lastend;
	for (i = 0; i < tp->rcv_numsacks; i++) {
		sack = tp->sackblks[i];
		if (SEQ_LT(sack.end, firstsack.start) ||
		    SEQ_GT(sack.start, firstsack.end))
			continue; /* no overlap */
		if (sack.start == firstsack.start && sack.end == firstsack.end){
			/* 
			 * identical block; delete it here since we will
			 * move it to the front of the list.
			 */
			tp->sackblks[i].start = tp->sackblks[i].end = 0;
			lastpos = i;    /* last posn with a zero entry */
			continue;
		}
		if (SEQ_LEQ(sack.start, firstsack.start))
			firstsack.start = sack.start; /* merge blocks */
		if (SEQ_GEQ(sack.end, firstsack.end))
			firstsack.end = sack.end;     /* merge blocks */
		tp->sackblks[i].start = tp->sackblks[i].end = 0;
		lastpos = i;    /* last posn with a zero entry */
	}
	if (lastpos != -1) {    /* at least one merge */
		for (i = 0, j = 1; i < tp->rcv_numsacks; i++) {
			sack = tp->sackblks[i];
			if (sack.start == 0 && sack.end == 0)
				continue;
			temp[j++] = sack;
		}
		tp->rcv_numsacks = j; /* including first blk (added later) */
		for (i = 1; i < tp->rcv_numsacks; i++) /* now copy back */
			tp->sackblks[i] = temp[i];
	} else {        /* no merges -- shift sacks by 1 */
		if (tp->rcv_numsacks < MAX_SACK_BLKS)
			tp->rcv_numsacks++;
		for (i = tp->rcv_numsacks-1; i > 0; i--)
			tp->sackblks[i] = tp->sackblks[i-1];
	}
	tp->sackblks[0] = firstsack;
	return;
}  

/*
 * Process the TCP SACK option.  Returns 1 if tcp_dooptions() should continue,
 * and 0 otherwise, if the option was fine.  tp->snd_holes is an ordered list
 * of holes (oldest to newest, in terms of the sequence space).  
 */             
int
tcp_sack_option(tp, th, cp, optlen)
	struct tcpcb *tp;
	struct tcphdr *th;
	u_char *cp;
	int    optlen;
{       
	int tmp_olen;
	u_char *tmp_cp;
	struct sackhole *cur, *p, *temp;
   
	if (tp->sack_disable)
		return 1;
           
	/* Note: TCPOLEN_SACK must be 2*sizeof(tcp_seq) */
	if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0)
		return 1;
	tmp_cp = cp + 2;
	tmp_olen = optlen - 2;
	if (tp->snd_numholes < 0)
		tp->snd_numholes = 0;
	if (tp->t_maxseg == 0)
		panic("tcp_sack_option"); /* Should never happen */
	while (tmp_olen > 0) {
		struct sackblk sack;
            
		bcopy((char *) tmp_cp, (char *) &(sack.start), sizeof(tcp_seq));
		NTOHL(sack.start); 
		bcopy((char *) tmp_cp + sizeof(tcp_seq),
		    (char *) &(sack.end), sizeof(tcp_seq));
		NTOHL(sack.end);
		tmp_olen -= TCPOLEN_SACK;
		tmp_cp += TCPOLEN_SACK;
		if (SEQ_LEQ(sack.end, sack.start))
			continue; /* bad SACK fields */
		if (SEQ_LEQ(sack.end, tp->snd_una)) 
			continue; /* old block */
#if defined(TCP_SACK) && defined(TCP_FACK)
		/* Updates snd_fack.  */
		if (SEQ_GEQ(sack.end, tp->snd_fack))
			tp->snd_fack = sack.end;
#endif /* TCP_FACK */
		if (tp->snd_holes == 0) { /* first hole */
			tp->snd_holes = (struct sackhole *)
			    malloc(sizeof(struct sackhole), M_PCB, M_NOWAIT);
			cur = tp->snd_holes;
			cur->start = th->th_ack;
			cur->end = sack.start;
			cur->rxmit = cur->start;
			cur->next = 0;
			tp->snd_numholes = 1;
			tp->rcv_lastsack = sack.end;
			/* 
			 * dups is at least one.  If more data has been 
			 * SACKed, it can be greater than one.
			 */
			cur->dups = min(tcprexmtthresh, 
			    ((sack.end - cur->end)/tp->t_maxseg));
			if (cur->dups < 1)
				cur->dups = 1;
			continue; /* with next sack block */
		}
		/* Go thru list of holes:  p = previous,  cur = current */
		p = cur = tp->snd_holes;
		while (cur) {
			if (SEQ_LEQ(sack.end, cur->start)) 
				/* SACKs data before the current hole */ 
				break; /* no use going through more holes */
			if (SEQ_GEQ(sack.start, cur->end)) {
				/* SACKs data beyond the current hole */
				cur->dups++;
				if ( ((sack.end - cur->end)/tp->t_maxseg) >=
					tcprexmtthresh)
					cur->dups = tcprexmtthresh;
				p = cur;
				cur = cur->next;
				continue;
			}
			if (SEQ_LEQ(sack.start, cur->start)) {
				/* Data acks at least the beginning of hole */
#if defined(TCP_SACK) && defined(TCP_FACK)
				if (SEQ_GT(sack.end, cur->rxmit))
					tp->retran_data -= 
				    	    tcp_seq_subtract(cur->rxmit, 
					    cur->start);
				else
					tp->retran_data -=
					    tcp_seq_subtract(sack.end, 
					    cur->start);
#endif /* TCP_FACK */
				if (SEQ_GEQ(sack.end,cur->end)){
					/* Acks entire hole, so delete hole */
					if (p != cur) {
						p->next = cur->next;
						free(cur, M_PCB);
						cur = p->next;
					} else {
						cur=cur->next;
						free(p, M_PCB);
						p = cur;
						tp->snd_holes = p;
					}
					tp->snd_numholes--;
					continue;
				}
				/* otherwise, move start of hole forward */
				cur->start = sack.end;
				cur->rxmit = max (cur->rxmit, cur->start);
				p = cur;
				cur = cur->next;
				continue;
			}
			/* move end of hole backward */
			if (SEQ_GEQ(sack.end, cur->end)) {
#if defined(TCP_SACK) && defined(TCP_FACK)
				if (SEQ_GT(cur->rxmit, sack.start)) 
					tp->retran_data -= 
					    tcp_seq_subtract(cur->rxmit, 
					    sack.start);
#endif /* TCP_FACK */
				cur->end = sack.start;
				cur->rxmit = min (cur->rxmit, cur->end);
				cur->dups++;
				if ( ((sack.end - cur->end)/tp->t_maxseg) >=
					tcprexmtthresh)
					cur->dups = tcprexmtthresh;
				p = cur;
				cur = cur->next;
				continue;
			}
			if (SEQ_LT(cur->start, sack.start) &&
			    SEQ_GT(cur->end, sack.end)) {
				/* 
				 * ACKs some data in middle of a hole; need to 
				 * split current hole
				 */
#if defined(TCP_SACK) && defined(TCP_FACK)
				if (SEQ_GT(cur->rxmit, sack.end)) 
					tp->retran_data -= 
					    tcp_seq_subtract(sack.end, 
					    sack.start);
				else if (SEQ_GT(cur->rxmit, sack.start))
					tp->retran_data -= 
					    tcp_seq_subtract(cur->rxmit, 
					    sack.start);
#endif /* TCP_FACK */
				temp = (struct sackhole *)malloc(sizeof(*temp),
				    M_PCB,M_NOWAIT);
				temp->next = cur->next;
				temp->start = sack.end;
				temp->end = cur->end;
				temp->dups = cur->dups;
				temp->rxmit = max (cur->rxmit, temp->start);
				cur->end = sack.start;
				cur->rxmit = min (cur->rxmit, cur->end);
				cur->dups++;
				if ( ((sack.end - cur->end)/tp->t_maxseg) >=
					tcprexmtthresh)
					cur->dups = tcprexmtthresh;
				cur->next = temp;
				p = temp;
				cur = p->next;
				tp->snd_numholes++;
			}
		}
		/* At this point, p points to the last hole on the list */
		if (SEQ_LT(tp->rcv_lastsack, sack.start)) {
			/*
			 * Need to append new hole at end.
			 * Last hole is p (and it's not NULL).
			 */
			temp = (struct sackhole *) malloc(sizeof(*temp),
			    M_PCB, M_NOWAIT);
			temp->start = tp->rcv_lastsack;
			temp->end = sack.start;
			temp->dups = min(tcprexmtthresh, 
			    ((sack.end - sack.start)/tp->t_maxseg));
			if (temp->dups < 1)
				temp->dups = 1;
			temp->rxmit = temp->start;
			temp->next = 0;
			p->next = temp;
			tp->rcv_lastsack = sack.end;
			tp->snd_numholes++;
		}
	}
#if defined(TCP_SACK) && defined(TCP_FACK)
	/* 
	 * Update retran_data, snd_fack, and snd_awnd.  Go through the list of 
	 * holes.   Increment retran_data by (hole->rxmit - hole->start).
	 * snd_fack gets the highest value of hole->end. 
	 */
	tp->retran_data = 0;
	cur = tp->snd_holes;
	while (cur) {
		tp->retran_data += cur->rxmit - cur->start;
		cur = cur->next;
	}
	tp->snd_awnd = tcp_seq_subtract(tp->snd_nxt, tp->snd_fack) + 
	    tp->retran_data;
#endif /* TCP_FACK */

	return 0;
}   

/*
 * Delete stale (i.e, cumulatively ack'd) holes.  Hole is deleted only if
 * it is completely acked; otherwise, tcp_sack_option(), called from 
 * tcp_dooptions(), will fix up the hole.
 */
void
tcp_del_sackholes(tp, th)
	struct tcpcb *tp;
	struct tcphdr *th;
{
	if (!tp->sack_disable && tp->t_state != TCPS_LISTEN) {
		/* max because this could be an older ack just arrived */
		tcp_seq lastack = max(th->th_ack, tp->snd_una);
		struct sackhole *cur = tp->snd_holes;
		struct sackhole *prev = cur;
		while (cur)
			if (SEQ_LEQ(cur->end, lastack)) {
				cur = cur->next;
				free(prev, M_PCB);
				prev = cur;
				tp->snd_numholes--;
			} else if (SEQ_LT(cur->start, lastack)) {
				cur->start = lastack;
				break;
			} else
				break;
		tp->snd_holes = cur;
	}
}

/* 
 * Delete all receiver-side SACK information.
 */
void
tcp_clean_sackreport(tp)
	struct tcpcb *tp;
{
	int i;

	tp->rcv_numsacks = 0;
	for (i = 0; i < MAX_SACK_BLKS; i++)
		tp->sackblks[i].start = tp->sackblks[i].end=0;

}

/* 
 * Checks for partial ack.  If partial ack arrives, turn off retransmission
 * timer, deflate the window, do not clear tp->t_dupacks, and return 1.
 * If the ack advances at least to tp->snd_last, return 0.
 */
int
tcp_sack_partialack(tp, th)
	struct tcpcb *tp;
	struct tcphdr *th;
{
	if (SEQ_LT(th->th_ack, tp->snd_last)) {
		/* Turn off retx. timer (will start again next segment) */
		tp->t_timer[TCPT_REXMT] = 0;
		tp->t_rtt = 0;
#ifndef TCP_FACK
		/* 
		 * Partial window deflation.  This statement relies on the 
		 * fact that tp->snd_una has not been updated yet.  In FACK
		 * hold snd_cwnd constant during fast recovery.
		 */
		tp->snd_cwnd -= (th->th_ack - tp->snd_una - tp->t_maxseg);
#endif
		return 1;
	}
	return 0;
}
#endif TCP_SACK

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
void
tcp_pulloutofband(so, urgent, m)
	struct socket *so;
	u_int urgent;
	register struct mbuf *m;
{
        int cnt = urgent - 1;
	
	while (cnt >= 0) {
		if (m->m_len > cnt) {
			char *cp = mtod(m, caddr_t) + cnt;
			struct tcpcb *tp = sototcpcb(so);

			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp+1, cp, (unsigned)(m->m_len - cnt - 1));
			m->m_len--;
			return;
		}
		cnt -= m->m_len;
		m = m->m_next;
		if (m == 0)
			break;
	}
	panic("tcp_pulloutofband");
}

/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
void
tcp_xmit_timer(tp, rtt)
	register struct tcpcb *tp;
	short rtt;
{
	register short delta;

	tcpstat.tcps_rttupdated++;
	--rtt;
	if (tp->t_srtt != 0) {
		/*
		 * srtt is stored as fixed point with 3 bits after the
		 * binary point (i.e., scaled by 8).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).  Adjust rtt to origin 0.
		 */
		delta = (rtt << 2) - (tp->t_srtt >> TCP_RTT_SHIFT);
		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1;
		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 2 bits after the
		 * binary point (scaled by 4).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1;
	} else {
		/* 
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = rtt << (TCP_RTT_SHIFT + 2);
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT + 2 - 1);
	}
	tp->t_rtt = 0;
	tp->t_rxtshift = 0;

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
	    rtt + 2, TCPTV_REXMTMAX);
	
	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;
}

/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing
 * interface without forcing IP to fragment; if bigger than
 * an mbuf cluster (MCLBYTES), round down to nearest multiple of MCLBYTES
 * to utilize large mbufs.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 *
 * Also take into account the space needed for options that we
 * send regularly.  Make maxseg shorter by that amount to assure
 * that we can send maxseg amount of data even when the options
 * are present.  Store the upper limit of the length of options plus
 * data in maxopd.
 */
int
tcp_mss(tp, offer)
	register struct tcpcb *tp;
	u_int offer;
{
	struct route *ro;
	register struct rtentry *rt;
	struct ifnet *ifp;
	register int rtt, mss;
	u_long bufsize;
	struct inpcb *inp;
	struct socket *so;

	inp = tp->t_inpcb;
	ro = &inp->inp_route;
	so = inp->inp_socket;

	if ((rt = ro->ro_rt) == (struct rtentry *)0) {
		/* No route yet, so try to acquire one */
#ifdef INET6
	  /*
	   * Get a new IPv6 route if an IPv6 destination, otherwise, get
	   * and IPv4 route (including those pesky IPv4-mapped addresses).
	   */
	  bzero(ro,sizeof(struct route6));
	  if (sotopf(so) == AF_INET6) {
	    if (IN6_IS_ADDR_V4MAPPED(&inp->inp_faddr6)) {
	      /* Get an IPv4 route. */
	      ro->ro_dst.sa_family = AF_INET;
	      ro->ro_dst.sa_len = sizeof(ro->ro_dst);
	      ((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
		inp->inp_faddr;
	      rtalloc(ro);
	    } else {
	      ro->ro_dst.sa_family = AF_INET6;
	      ro->ro_dst.sa_len = sizeof(struct sockaddr_in6);
	      ((struct sockaddr_in6 *) &ro->ro_dst)->sin6_addr =
		inp->inp_faddr6;
	      rtalloc(ro);
	    }
	  } else
#endif /* INET6 */
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(ro->ro_dst);
			satosin(&ro->ro_dst)->sin_addr = inp->inp_faddr;
			rtalloc(ro);
		}
		if ((rt = ro->ro_rt) == (struct rtentry *)0) {
			tp->t_maxopd = tp->t_maxseg = tcp_mssdflt;
			return (tcp_mssdflt);
		}
	}
	ifp = rt->rt_ifp;

#ifdef RTV_MTU	/* if route characteristics exist ... */
	/*
	 * While we're here, check if there's an initial rtt
	 * or rttvar.  Convert from the route-table units
	 * to scaled multiples of the slow timeout timer.
	 */
	if (tp->t_srtt == 0 && (rtt = rt->rt_rmx.rmx_rtt)) {
		/*
		 * XXX the lock bit for MTU indicates that the value
		 * is also a minimum value; this is subject to time.
		 */
		if (rt->rt_rmx.rmx_locks & RTV_RTT)
			tp->t_rttmin = rtt / (RTM_RTTUNIT / PR_SLOWHZ);
		tp->t_srtt = rtt / (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));
		if (rt->rt_rmx.rmx_rttvar)
			tp->t_rttvar = rt->rt_rmx.rmx_rttvar /
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));
		else
			/* default variation is +- 1 rtt */
			tp->t_rttvar =
			    tp->t_srtt * TCP_RTTVAR_SCALE / TCP_RTT_SCALE;
		TCPT_RANGESET((long) tp->t_rxtcur,
		    ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1,
		    tp->t_rttmin, TCPTV_REXMTMAX);
	}
	/*
	 * if there's an mtu associated with the route, use it
	 */
	if (rt->rt_rmx.rmx_mtu)
#ifdef INET6
	{
	  /*
	   * One may wish to lower MSS to take into account options,
	   * especially security-related options.
	   */
	  if (tp->pf == AF_INET6) 
	    mss = rt->rt_rmx.rmx_mtu - sizeof(struct tcpipv6hdr);
	  else
#endif /* INET6 */
		mss = rt->rt_rmx.rmx_mtu - sizeof(struct tcpiphdr);
#ifdef INET6
	}
#endif /* INET6 */
	else
#endif /* RTV_MTU */
	{
	  /*
	   *  ifp may be null and rmx_mtu may be zero in certain
	   *  v6 cases (e.g., if ND wasn't able to resolve the 
	   *  destination host.
	   */
		mss = ifp ? ifp->if_mtu - sizeof(struct tcpiphdr) : 0;
#ifdef INET6
		if (tp->pf == AF_INET)
#endif /* INET6 */
		if (!in_localaddr(inp->inp_faddr))
			mss = min(mss, tcp_mssdflt);
	}
	/*
	 * The current mss, t_maxseg, is initialized to the default value.
	 * If we compute a smaller value, reduce the current mss.
	 * If we compute a larger value, return it for use in sending
	 * a max seg size option, but don't store it for use
	 * unless we received an offer at least that large from peer.
	 * However, do not accept offers under 32 bytes.
	 */
	if (offer)
		mss = min(mss, offer);
	mss = max(mss, 64);		/* sanity - at least max opt. space */
	/*
	 * maxopd stores the maximum length of data AND options
	 * in a segment; maxseg is the amount of data in a normal
	 * segment.  We need to store this value (maxopd) apart
	 * from maxseg, because now every segment carries options
	 * and thus we normally have somewhat less data in segments.
	 */
	tp->t_maxopd = mss;

 	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	    (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP)
		mss -= TCPOLEN_TSTAMP_APPA;

#if	(MCLBYTES & (MCLBYTES - 1)) == 0
		if (mss > MCLBYTES)
			mss &= ~(MCLBYTES-1);
#else
		if (mss > MCLBYTES)
			mss = mss / MCLBYTES * MCLBYTES;
#endif
	/*
	 * If there's a pipesize, change the socket buffer
	 * to that size.  Make the socket buffers an integral
	 * number of mss units; if the mss is larger than
	 * the socket buffer, decrease the mss.
	 */
#ifdef RTV_SPIPE
	if ((bufsize = rt->rt_rmx.rmx_sendpipe) == 0)
#endif
		bufsize = so->so_snd.sb_hiwat;
	if (bufsize < mss)
		mss = bufsize;
	else {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		(void)sbreserve(&so->so_snd, bufsize);
	}
	tp->t_maxseg = mss;

#ifdef RTV_RPIPE
	if ((bufsize = rt->rt_rmx.rmx_recvpipe) == 0)
#endif
		bufsize = so->so_rcv.sb_hiwat;
	if (bufsize > mss) {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		(void)sbreserve(&so->so_rcv, bufsize);
	}
	tp->snd_cwnd = mss;

#ifdef RTV_SSTHRESH
	if (rt->rt_rmx.rmx_ssthresh) {
		/*
		 * There's some sort of gateway or interface
		 * buffer limit on the path.  Use this to set
		 * the slow start threshhold, but set the
		 * threshold to no less than 2*mss.
		 */
		tp->snd_ssthresh = max(2 * mss, rt->rt_rmx.rmx_ssthresh);
	}
#endif /* RTV_MTU */
	return (mss);
}
#endif /* TUBA_INCLUDE */

#if defined(TCP_NEWRENO) || defined (TCP_SACK)
/* 
 * Checks for partial ack.  If partial ack arrives, force the retransmission
 * of the next unacknowledged segment, do not clear tp->t_dupacks, and return
 * 1.  By setting snd_nxt to ti_ack, this forces retransmission timer to
 * be started again.  If the ack advances at least to tp->snd_last, return 0.
 */
int
tcp_newreno(tp, th)
	struct tcpcb *tp;
	struct tcphdr *th;
{
	if (SEQ_LT(th->th_ack, tp->snd_last)) {
		tcp_seq onxt = tp->snd_nxt;
		tcp_seq ouna = tp->snd_una;	/* snd_una not yet updated */
		u_long  ocwnd = tp->snd_cwnd;
		tp->t_timer[TCPT_REXMT] = 0;
		tp->t_rtt = 0;
		tp->snd_nxt = th->th_ack;
		tp->snd_cwnd = tp->t_maxseg;
		tp->snd_una = th->th_ack;
		(void) tcp_output(tp);
		tp->snd_cwnd = ocwnd;
		tp->snd_una = ouna;
		if (SEQ_GT(onxt, tp->snd_nxt))
			tp->snd_nxt = onxt;
		/* 
		 * Partial window deflation.  Relies on fact that tp->snd_una 
		 * not updated yet.  
		 */
		tp->snd_cwnd -= (th->th_ack - tp->snd_una - tp->t_maxseg);
		return 1;
    }
    return 0;
}
#endif /* TCP_NEWRENO || TCP_SACK */
