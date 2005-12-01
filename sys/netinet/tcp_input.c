/*	$OpenBSD: tcp_input.c,v 1.194 2005/12/01 22:31:50 markus Exp $	*/
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
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>

#include <dev/rndvar.h>

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

struct	tcpiphdr tcp_saveti;

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

struct  tcpipv6hdr tcp_saveti6;

/* for the packet header length in the mbuf */
#define M_PH_LEN(m)      (((struct mbuf *)(m))->m_pkthdr.len)
#define M_V6_LEN(m)      (M_PH_LEN(m) - sizeof(struct ip6_hdr))
#define M_V4_LEN(m)      (M_PH_LEN(m) - sizeof(struct ip))
#endif /* INET6 */

int	tcprexmtthresh = 3;
int	tcptv_keep_init = TCPTV_KEEP_INIT;

extern u_long sb_max;

int tcp_rst_ppslim = 100;		/* 100pps */
int tcp_rst_ppslim_count = 0;
struct timeval tcp_rst_ppslim_last;

int tcp_ackdrop_ppslim = 100;		/* 100pps */
int tcp_ackdrop_ppslim_count = 0;
struct timeval tcp_ackdrop_ppslim_last;

#define TCP_PAWS_IDLE	(24 * 24 * 60 * 60 * PR_SLOWHZ)

/* for modulo comparisons of timestamps */
#define TSTMP_LT(a,b)	((int)((a)-(b)) < 0)
#define TSTMP_GEQ(a,b)	((int)((a)-(b)) >= 0)

/* for TCP SACK comparisons */
#define	SEQ_MIN(a,b)	(SEQ_LT(a,b) ? (a) : (b))
#define	SEQ_MAX(a,b)	(SEQ_GT(a,b) ? (a) : (b))

/*
 * Neighbor Discovery, Neighbor Unreachability Detection Upper layer hint.
 */
#ifdef INET6
#define ND6_HINT(tp) \
do { \
	if (tp && tp->t_inpcb && (tp->t_inpcb->inp_flags & INP_IPV6) && \
	    tp->t_inpcb->inp_route6.ro_rt) { \
		nd6_nud_hint(tp->t_inpcb->inp_route6.ro_rt, NULL, 0); \
	} \
} while (0)
#else
#define ND6_HINT(tp)
#endif

#ifdef TCP_ECN
/*
 * ECN (Explicit Congestion Notification) support based on RFC3168
 * implementation note:
 *   snd_last is used to track a recovery phase.
 *   when cwnd is reduced, snd_last is set to snd_max.
 *   while snd_last > snd_una, the sender is in a recovery phase and
 *   its cwnd should not be reduced again.
 *   snd_last follows snd_una when not in a recovery phase.
 */
#endif

/*
 * Macro to compute ACK transmission behavior.  Delay the ACK unless
 * we have already delayed an ACK (must send an ACK every two segments).
 * We also ACK immediately if we received a PUSH and the ACK-on-PUSH
 * option is enabled.
 */
#define	TCP_SETUP_ACK(tp, tiflags) \
do { \
	if ((tp)->t_flags & TF_DELACK || \
	    (tcp_ack_on_push && (tiflags) & TH_PUSH)) \
		tp->t_flags |= TF_ACKNOW; \
	else \
		TCP_SET_DELACK(tp); \
} while (0)

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

int
tcp_reass(tp, th, m, tlen)
	struct tcpcb *tp;
	struct tcphdr *th;
	struct mbuf *m;
	int *tlen;
{
	struct tcpqent *p, *q, *nq, *tiqe;
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
	tiqe = pool_get(&tcpqe_pool, PR_NOWAIT);
	if (tiqe == NULL) {
		tiqe = TAILQ_LAST(&tp->t_segq, tcpqehead);
		if (tiqe != NULL && th->th_seq == tp->rcv_nxt) {
			/* Reuse last entry since new segment fills a hole */
			m_freem(tiqe->tcpqe_m);
			TAILQ_REMOVE(&tp->t_segq, tiqe, tcpqe_q);
		}
		if (tiqe == NULL || th->th_seq != tp->rcv_nxt) {
			/* Flush segment queue for this connection */
			tcp_freeq(tp);
			tcpstat.tcps_rcvmemdrop++;
			m_freem(m);
			return (0);
		}
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = TAILQ_FIRST(&tp->t_segq); q != NULL;
	    p = q, q = TAILQ_NEXT(q, tcpqe_q))
		if (SEQ_GT(q->tcpqe_tcp->th_seq, th->th_seq))
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		struct tcphdr *phdr = p->tcpqe_tcp;
		int i;

		/* conversion to int (in i) handles seq wraparound */
		i = phdr->th_seq + phdr->th_reseqlen - th->th_seq;
		if (i > 0) {
		        if (i >= *tlen) {
				tcpstat.tcps_rcvduppack++;
				tcpstat.tcps_rcvdupbyte += *tlen;
				m_freem(m);
				pool_put(&tcpqe_pool, tiqe);
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
		struct tcphdr *qhdr = q->tcpqe_tcp;
		int i = (th->th_seq + *tlen) - qhdr->th_seq;

		if (i <= 0)
			break;
		if (i < qhdr->th_reseqlen) {
			qhdr->th_seq += i;
			qhdr->th_reseqlen -= i;
			m_adj(q->tcpqe_m, i);
			break;
		}
		nq = TAILQ_NEXT(q, tcpqe_q);
		m_freem(q->tcpqe_m);
		TAILQ_REMOVE(&tp->t_segq, q, tcpqe_q);
		pool_put(&tcpqe_pool, q);
	}

	/* Insert the new segment queue entry into place. */
	tiqe->tcpqe_m = m;
	th->th_reseqlen = *tlen;
	tiqe->tcpqe_tcp = th;
	if (p == NULL) {
		TAILQ_INSERT_HEAD(&tp->t_segq, tiqe, tcpqe_q);
	} else {
		TAILQ_INSERT_AFTER(&tp->t_segq, p, tiqe, tcpqe_q);
	}

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		return (0);
	q = TAILQ_FIRST(&tp->t_segq);
	if (q == NULL || q->tcpqe_tcp->th_seq != tp->rcv_nxt)
		return (0);
	if (tp->t_state == TCPS_SYN_RECEIVED && q->tcpqe_tcp->th_reseqlen)
		return (0);
	do {
		tp->rcv_nxt += q->tcpqe_tcp->th_reseqlen;
		flags = q->tcpqe_tcp->th_flags & TH_FIN;

		nq = TAILQ_NEXT(q, tcpqe_q);
		TAILQ_REMOVE(&tp->t_segq, q, tcpqe_q);
		ND6_HINT(tp);
		if (so->so_state & SS_CANTRCVMORE)
			m_freem(q->tcpqe_m);
		else
			sbappendstream(&so->so_rcv, q->tcpqe_m);
		pool_put(&tcpqe_pool, q);
		q = nq;
	} while (q != NULL && q->tcpqe_tcp->th_seq == tp->rcv_nxt);
	sorwakeup(so);
	return (flags);
}

#ifdef INET6
int
tcp6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;

#if defined(NFAITH) && 0 < NFAITH
	if (m->m_pkthdr.rcvif) {
		if (m->m_pkthdr.rcvif->if_type == IFT_FAITH) {
			/* XXX send icmp6 host/port unreach? */
			m_freem(m);
			return IPPROTO_DONE;
		}
	}
#endif

	/*
	 * draft-itojun-ipv6-tcp-to-anycast
	 * better place to put this in?
	 */
	if (m->m_flags & M_ANYCAST6) {
		if (m->m_len >= sizeof(struct ip6_hdr)) {
			struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
			icmp6_error(m, ICMP6_DST_UNREACH,
				ICMP6_DST_UNREACH_ADDR,
				(caddr_t)&ip6->ip6_dst - (caddr_t)ip6);
		} else
			m_freem(m);
		return IPPROTO_DONE;
	}

	tcp_input(m, *offp, proto);
	return IPPROTO_DONE;
}
#endif

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
void
tcp_input(struct mbuf *m, ...)
{
	struct ip *ip;
	struct inpcb *inp;
	u_int8_t *optp = NULL;
	int optlen = 0;
	int tlen, off;
	struct tcpcb *tp = 0;
	int tiflags;
	struct socket *so = NULL;
	int todrop, acked, ourfinisacked, needoutput = 0;
	int hdroptlen = 0;
	short ostate = 0;
	int iss = 0;
	u_long tiwin;
	struct tcp_opt_info opti;
	int iphlen;
	va_list ap;
	struct tcphdr *th;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif /* INET6 */
#ifdef IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct tdb *tdb;
	int error, s;
#endif /* IPSEC */
	int af;
#ifdef TCP_ECN
	u_char iptos;
#endif

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	tcpstat.tcps_rcvtotal++;

	opti.ts_present = 0;
	opti.maxseg = 0;

	/*
	 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
	 * See below for AF specific multicast.
	 */
	if (m->m_flags & (M_BCAST|M_MCAST))
		goto drop;

	/*
	 * Before we do ANYTHING, we have to figure out if it's TCP/IPv6 or
	 * TCP/IPv4.
	 */
	switch (mtod(m, struct ip *)->ip_v) {
#ifdef INET6
	case 6:
		af = AF_INET6;
		break;
#endif
	case 4:
		af = AF_INET;
		break;
	default:
		m_freem(m);
		return;	/*EAFNOSUPPORT*/
	}

	/*
	 * Get IP and TCP header together in first mbuf.
	 * Note: IP leaves IP header in first mbuf.
	 */
	switch (af) {
	case AF_INET:
#ifdef DIAGNOSTIC
		if (iphlen < sizeof(struct ip)) {
			m_freem(m);
			return;
		}
#endif /* DIAGNOSTIC */
		break;
#ifdef INET6
	case AF_INET6:
#ifdef DIAGNOSTIC
		if (iphlen < sizeof(struct ip6_hdr)) {
			m_freem(m);
			return;
		}
#endif /* DIAGNOSTIC */
		break;
#endif
	default:
		m_freem(m);
		return;
	}

	IP6_EXTHDR_GET(th, struct tcphdr *, m, iphlen, sizeof(*th));
	if (!th) {
		tcpstat.tcps_rcvshort++;
		return;
	}

	tlen = m->m_pkthdr.len - iphlen;
	ip = NULL;
#ifdef INET6
	ip6 = NULL;
#endif
	switch (af) {
	case AF_INET:
		ip = mtod(m, struct ip *);
		if (IN_MULTICAST(ip->ip_dst.s_addr) ||
		    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
			goto drop;
#ifdef TCP_ECN
		/* save ip_tos before clearing it for checksum */
		iptos = ip->ip_tos;
#endif
		/*
		 * Checksum extended TCP header and data.
		 */
		if ((m->m_pkthdr.csum_flags & M_TCP_CSUM_IN_OK) == 0) {
			if (m->m_pkthdr.csum_flags & M_TCP_CSUM_IN_BAD) {
				tcpstat.tcps_inhwcsum++;
				tcpstat.tcps_rcvbadsum++;
				goto drop;
			}
			if (in4_cksum(m, IPPROTO_TCP, iphlen, tlen) != 0) {
				tcpstat.tcps_rcvbadsum++;
				goto drop;
			}
		} else {
			m->m_pkthdr.csum_flags &= ~M_TCP_CSUM_IN_OK;
			tcpstat.tcps_inhwcsum++;
		}
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
#ifdef TCP_ECN
		iptos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
#endif

		/* Be proactive about malicious use of IPv4 mapped address */
		if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
		    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
			/* XXX stat */
			goto drop;
		}

		/*
		 * Be proactive about unspecified IPv6 address in source.
		 * As we use all-zero to indicate unbounded/unconnected pcb,
		 * unspecified IPv6 address can be used to confuse us.
		 *
		 * Note that packets with unspecified IPv6 destination is
		 * already dropped in ip6_input.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
			/* XXX stat */
			goto drop;
		}

		/* Discard packets to multicast */
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
			/* XXX stat */
			goto drop;
		}

		/*
		 * Checksum extended TCP header and data.
		 */
		if (in6_cksum(m, IPPROTO_TCP, sizeof(struct ip6_hdr), tlen)) {
			tcpstat.tcps_rcvbadsum++;
			goto drop;
		}
		break;
#endif
	}

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = th->th_off << 2;
	if (off < sizeof(struct tcphdr) || off > tlen) {
		tcpstat.tcps_rcvbadoff++;
		goto drop;
	}
	tlen -= off;
	if (off > sizeof(struct tcphdr)) {
		IP6_EXTHDR_GET(th, struct tcphdr *, m, iphlen, off);
		if (!th) {
			tcpstat.tcps_rcvshort++;
			return;
		}
		optlen = off - sizeof(struct tcphdr);
		optp = (u_int8_t *)(th + 1);
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
			opti.ts_present = 1;
			opti.ts_val = ntohl(*(u_int32_t *)(optp + 4));
			opti.ts_ecr = ntohl(*(u_int32_t *)(optp + 8));
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

	/*
	 * Locate pcb for segment.
	 */
findpcb:
	switch (af) {
#ifdef INET6
	case AF_INET6:
		inp = in6_pcbhashlookup(&tcbtable, &ip6->ip6_src, th->th_sport,
		    &ip6->ip6_dst, th->th_dport);
		break;
#endif
	case AF_INET:
		inp = in_pcbhashlookup(&tcbtable, ip->ip_src, th->th_sport,
		    ip->ip_dst, th->th_dport);
		break;
	}
	if (inp == 0) {
		int	inpl_flags = 0;
#if NPF > 0
		struct pf_mtag *t;

		if ((t = pf_find_mtag(m)) != NULL &&
		    t->flags & PF_TAG_TRANSLATE_LOCALHOST)
			inpl_flags = INPLOOKUP_WILDCARD;
#endif
		++tcpstat.tcps_pcbhashmiss;
		switch (af) {
#ifdef INET6
		case AF_INET6:
			inp = in6_pcblookup_listen(&tcbtable,
			    &ip6->ip6_dst, th->th_dport, inpl_flags);
			break;
#endif /* INET6 */
		case AF_INET:
			inp = in_pcblookup_listen(&tcbtable,
			    ip->ip_dst, th->th_dport, inpl_flags);
			break;
		}
		/*
		 * If the state is CLOSED (i.e., TCB does not exist) then
		 * all data in the incoming segment is discarded.
		 * If the TCB exists but is in CLOSED state, it is embryonic,
		 * but should either do a listen or a connect soon.
		 */
		if (inp == 0) {
			++tcpstat.tcps_noport;
			goto dropwithreset_ratelim;
		}
	}

	tp = intotcpcb(inp);
	if (tp == 0)
		goto dropwithreset_ratelim;
	if (tp->t_state == TCPS_CLOSED)
		goto drop;

	/* Unscale the window into a 32-bit value. */
	if ((tiflags & TH_SYN) == 0)
		tiwin = th->th_win << tp->snd_scale;
	else
		tiwin = th->th_win;

	so = inp->inp_socket;
	if (so->so_options & (SO_DEBUG|SO_ACCEPTCONN)) {
		union syn_cache_sa src;
		union syn_cache_sa dst;

		bzero(&src, sizeof(src));
		bzero(&dst, sizeof(dst));
		switch (af) {
#ifdef INET
		case AF_INET:
			src.sin.sin_len = sizeof(struct sockaddr_in);
			src.sin.sin_family = AF_INET;
			src.sin.sin_addr = ip->ip_src;
			src.sin.sin_port = th->th_sport;

			dst.sin.sin_len = sizeof(struct sockaddr_in);
			dst.sin.sin_family = AF_INET;
			dst.sin.sin_addr = ip->ip_dst;
			dst.sin.sin_port = th->th_dport;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			src.sin6.sin6_len = sizeof(struct sockaddr_in6);
			src.sin6.sin6_family = AF_INET6;
			src.sin6.sin6_addr = ip6->ip6_src;
			src.sin6.sin6_port = th->th_sport;

			dst.sin6.sin6_len = sizeof(struct sockaddr_in6);
			dst.sin6.sin6_family = AF_INET6;
			dst.sin6.sin6_addr = ip6->ip6_dst;
			dst.sin6.sin6_port = th->th_dport;
			break;
#endif /* INET6 */
		default:
			goto badsyn;	/*sanity*/
		}

		if (so->so_options & SO_DEBUG) {
			ostate = tp->t_state;
			switch (af) {
#ifdef INET6
			case AF_INET6:
				bcopy(ip6, &tcp_saveti6.ti6_i, sizeof(*ip6));
				bcopy(th, &tcp_saveti6.ti6_t, sizeof(*th));
				break;
#endif
			case AF_INET:
				bcopy(ip, &tcp_saveti.ti_i, sizeof(*ip));
				bcopy(th, &tcp_saveti.ti_t, sizeof(*th));
				break;
			}
		}
		if (so->so_options & SO_ACCEPTCONN) {
			if ((tiflags & (TH_RST|TH_ACK|TH_SYN)) != TH_SYN) {
				if (tiflags & TH_RST) {
					syn_cache_reset(&src.sa, &dst.sa, th);
				} else if ((tiflags & (TH_ACK|TH_SYN)) ==
				    (TH_ACK|TH_SYN)) {
					/*
					 * Received a SYN,ACK.  This should
					 * never happen while we are in
					 * LISTEN.  Send an RST.
					 */
					goto badsyn;
				} else if (tiflags & TH_ACK) {
					so = syn_cache_get(&src.sa, &dst.sa,
						th, iphlen, tlen, so, m);
					if (so == NULL) {
						/*
						 * We don't have a SYN for
						 * this ACK; send an RST.
						 */
						goto badsyn;
					} else if (so ==
					    (struct socket *)(-1)) {
						/*
						 * We were unable to create
						 * the connection.  If the
						 * 3-way handshake was
						 * completed, and RST has
						 * been sent to the peer.
						 * Since the mbuf might be
						 * in use for the reply,
						 * do not free it.
						 */
						m = NULL;
					} else {
						/*
						 * We have created a
						 * full-blown connection.
						 */
						tp = NULL;
						inp = (struct inpcb *)so->so_pcb;
						tp = intotcpcb(inp);
						if (tp == NULL)
							goto badsyn;	/*XXX*/

						/*
						 * Compute proper scaling
						 * value from buffer space
						 */
						tcp_rscale(tp, so->so_rcv.sb_hiwat);
						goto after_listen;
					}
				} else {
					/*
					 * None of RST, SYN or ACK was set.
					 * This is an invalid packet for a
					 * TCB in LISTEN state.  Send a RST.
					 */
					goto badsyn;
				}
			} else {
				/*
				 * Received a SYN.
				 */
#ifdef INET6
				/*
				 * If deprecated address is forbidden, we do
				 * not accept SYN to deprecated interface
				 * address to prevent any new inbound
				 * connection from getting established.
				 * When we do not accept SYN, we send a TCP
				 * RST, with deprecated source address (instead
				 * of dropping it).  We compromise it as it is
				 * much better for peer to send a RST, and
				 * RST will be the final packet for the
				 * exchange.
				 *
				 * If we do not forbid deprecated addresses, we
				 * accept the SYN packet.  RFC2462 does not
				 * suggest dropping SYN in this case.
				 * If we decipher RFC2462 5.5.4, it says like
				 * this:
				 * 1. use of deprecated addr with existing
				 *    communication is okay - "SHOULD continue
				 *    to be used"
				 * 2. use of it with new communication:
				 *   (2a) "SHOULD NOT be used if alternate
				 *        address with sufficient scope is
				 *        available"
				 *   (2b) nothing mentioned otherwise. 
				 * Here we fall into (2b) case as we have no
				 * choice in our source address selection - we
				 * must obey the peer.
				 *
				 * The wording in RFC2462 is confusing, and
				 * there are multiple description text for
				 * deprecated address handling - worse, they
				 * are not exactly the same.  I believe 5.5.4
				 * is the best one, so we follow 5.5.4.
				 */
				if (ip6 && !ip6_use_deprecated) {
					struct in6_ifaddr *ia6;

					if ((ia6 = in6ifa_ifpwithaddr(m->m_pkthdr.rcvif,
					    &ip6->ip6_dst)) &&
					    (ia6->ia6_flags & IN6_IFF_DEPRECATED)) {
						tp = NULL;
						goto dropwithreset;
					}
				}
#endif

				/*
				 * LISTEN socket received a SYN
				 * from itself?  This can't possibly
				 * be valid; drop the packet.
				 */
				if (th->th_dport == th->th_sport) {
					switch (af) {
#ifdef INET6
					case AF_INET6:
						if (IN6_ARE_ADDR_EQUAL(&ip6->ip6_src,
						    &ip6->ip6_dst)) {
							tcpstat.tcps_badsyn++;
							goto drop;
						}
						break;
#endif /* INET6 */
					case AF_INET:
						if (ip->ip_dst.s_addr == ip->ip_src.s_addr) {
							tcpstat.tcps_badsyn++;
							goto drop;
						}
						break;
					}
				}

				/*
				 * SYN looks ok; create compressed TCP
				 * state for it.
				 */
				if (so->so_qlen <= so->so_qlimit &&
				    syn_cache_add(&src.sa, &dst.sa, th, iphlen,
						so, m, optp, optlen, &opti))
					m = NULL;
			}
			goto drop;
		}
	}

after_listen:
#ifdef DIAGNOSTIC
	/*
	 * Should not happen now that all embryonic connections
	 * are handled with compressed state.
	 */
	if (tp->t_state == TCPS_LISTEN)
		panic("tcp_input: TCPS_LISTEN");
#endif

#ifdef IPSEC
	/* Find most recent IPsec tag */
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
        s = splnet();
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
	        tdb = gettdb(tdbi->spi, &tdbi->dst, tdbi->proto);
	} else
		tdb = NULL;
	ipsp_spd_lookup(m, af, iphlen, &error, IPSP_DIRECTION_IN,
	    tdb, inp);
	if (error) {
		splx(s);
		goto drop;
	}

	/* Latch SA */
	if (inp->inp_tdb_in != tdb) {
		if (tdb) {
		        tdb_add_inp(tdb, inp, 1);
			if (inp->inp_ipo == NULL) {
				inp->inp_ipo = ipsec_add_policy(inp, af,
				    IPSP_DIRECTION_OUT);
				if (inp->inp_ipo == NULL) {
					splx(s);
					goto drop;
				}
			}
			if (inp->inp_ipo->ipo_dstid == NULL &&
			    tdb->tdb_srcid != NULL) {
				inp->inp_ipo->ipo_dstid = tdb->tdb_srcid;
				tdb->tdb_srcid->ref_count++;
			}
			if (inp->inp_ipsec_remotecred == NULL &&
			    tdb->tdb_remote_cred != NULL) {
				inp->inp_ipsec_remotecred =
				    tdb->tdb_remote_cred;
				tdb->tdb_remote_cred->ref_count++;
			}
			if (inp->inp_ipsec_remoteauth == NULL &&
			    tdb->tdb_remote_auth != NULL) {
				inp->inp_ipsec_remoteauth =
				    tdb->tdb_remote_auth;
				tdb->tdb_remote_auth->ref_count++;
			}
		} else { /* Just reset */
		        TAILQ_REMOVE(&inp->inp_tdb_in->tdb_inp_in, inp,
				     inp_tdb_in_next);
			inp->inp_tdb_in = NULL;
		}
	}
        splx(s);
#endif /* IPSEC */

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	tp->t_rcvtime = tcp_now;
	if (TCPS_HAVEESTABLISHED(tp->t_state))
		TCP_TIMER_ARM(tp, TCPT_KEEP, tcp_keepidle);

#ifdef TCP_SACK
	if (tp->sack_enable)
		tcp_del_sackholes(tp, th); /* Delete stale SACK holes */
#endif /* TCP_SACK */

	/*
	 * Process options.
	 */
#ifdef TCP_SIGNATURE
	if (optp || (tp->t_flags & TF_SIGNATURE))
#else
	if (optp)
#endif
		if (tcp_dooptions(tp, optp, optlen, th, m, iphlen, &opti))
			goto drop;

	if (opti.ts_present && opti.ts_ecr) {
		int rtt_test;

		/* subtract out the tcp timestamp modulator */
		opti.ts_ecr -= tp->ts_modulate;
                                                     
		/* make sure ts_ecr is sensible */
		rtt_test = tcp_now - opti.ts_ecr;
		if (rtt_test < 0 || rtt_test > TCP_RTT_MAX)
			opti.ts_ecr = 0;
	}

#ifdef TCP_ECN
	/* if congestion experienced, set ECE bit in subsequent packets. */
	if ((iptos & IPTOS_ECN_MASK) == IPTOS_ECN_CE) {
		tp->t_flags |= TF_RCVD_CE;
		tcpstat.tcps_ecn_rcvce++;
	}
#endif
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
#ifdef TCP_ECN
	    (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ECE|TH_CWR|TH_ACK)) == TH_ACK &&
#else
	    (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
#endif
	    (!opti.ts_present || TSTMP_GEQ(opti.ts_val, tp->ts_recent)) &&
	    th->th_seq == tp->rcv_nxt &&
	    tiwin && tiwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {

		/*
		 * If last ACK falls within this segment's sequence numbers,
		 *  record the timestamp.
		 * Fix from Braden, see Stevens p. 870
		 */
		if (opti.ts_present && SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = tcp_now;
			tp->ts_recent = opti.ts_val;
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
				if (opti.ts_present && opti.ts_ecr)
					tcp_xmit_timer(tp, tcp_now - opti.ts_ecr);
				else if (tp->t_rtttime &&
				    SEQ_GT(th->th_ack, tp->t_rtseq))
					tcp_xmit_timer(tp,
					    tcp_now - tp->t_rtttime);
				acked = th->th_ack - tp->snd_una;
				tcpstat.tcps_rcvackpack++;
				tcpstat.tcps_rcvackbyte += acked;
				ND6_HINT(tp);
				sbdrop(&so->so_snd, acked);

				/*
				 * If we had a pending ICMP message that
				 * referres to data that have just been 
				 * acknowledged, disregard the recorded ICMP 
				 * message.
				 */
				if ((tp->t_flags & TF_PMTUD_PEND) && 
				    SEQ_GT(th->th_ack, tp->t_pmtud_th_seq))
					tp->t_flags &= ~TF_PMTUD_PEND;

				/*
				 * Keep track of the largest chunk of data 
				 * acknowledged since last PMTU update
				 */
				if (tp->t_pmtud_mss_acked < acked)
					tp->t_pmtud_mss_acked = acked;

				tp->snd_una = th->th_ack;
#if defined(TCP_SACK) || defined(TCP_ECN)
				/*
				 * We want snd_last to track snd_una so
				 * as to avoid sequence wraparound problems
				 * for very large transfers.
				 */
#ifdef TCP_ECN
				if (SEQ_GT(tp->snd_una, tp->snd_last))
#endif
				tp->snd_last = tp->snd_una;
#endif /* TCP_SACK */
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
					TCP_TIMER_DISARM(tp, TCPT_REXMT);
				else if (TCP_TIMER_ISARMED(tp, TCPT_PERSIST) == 0)
					TCP_TIMER_ARM(tp, TCPT_REXMT, tp->t_rxtcur);

				if (sb_notify(&so->so_snd))
					sowwakeup(so);
				if (so->so_snd.sb_cc)
					(void) tcp_output(tp);
				return;
			}
		} else if (th->th_ack == tp->snd_una &&
		    TAILQ_EMPTY(&tp->t_segq) &&
		    tlen <= sbspace(&so->so_rcv)) {
			/*
			 * This is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */
#ifdef TCP_SACK
			/* Clean receiver SACK report if present */
			if (tp->sack_enable && tp->rcv_numsacks)
				tcp_clean_sackreport(tp);
#endif /* TCP_SACK */
			++tcpstat.tcps_preddat;
			tp->rcv_nxt += tlen;
			tcpstat.tcps_rcvpack++;
			tcpstat.tcps_rcvbyte += tlen;
			ND6_HINT(tp);
			/*
			 * Drop TCP, IP headers and TCP options then add data
			 * to socket buffer.
			 */
			if (so->so_state & SS_CANTRCVMORE)
				m_freem(m);
			else {
				m_adj(m, iphlen + off);
				sbappendstream(&so->so_rcv, m);
			}
			sorwakeup(so);
			TCP_SETUP_ACK(tp, tiflags);
			if (tp->t_flags & TF_ACKNOW)
				(void) tcp_output(tp);
			return;
		}
	}

	/*
	 * Compute mbuf offset to TCP data segment.
	 */
	hdroptlen = iphlen + off;

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
#ifdef TCP_ECN
			/* if ECN is enabled, fall back to non-ecn at rexmit */
			if (tcp_do_ecn && !(tp->t_flags & TF_DISABLE_ECN))
				goto drop;
#endif
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
		TCP_TIMER_DISARM(tp, TCPT_REXMT);
		tp->irs = th->th_seq;
		tcp_mss(tp, opti.maxseg);
		/* Reset initial window to 1 segment for retransmit */
		if (tp->t_rxtshift > 0)
			tp->snd_cwnd = tp->t_maxseg;
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
#ifdef TCP_SACK
                /*
                 * If we've sent a SACK_PERMITTED option, and the peer
                 * also replied with one, then TF_SACK_PERMIT should have
                 * been set in tcp_dooptions().  If it was not, disable SACKs.
                 */
		if (tp->sack_enable)
			tp->sack_enable = tp->t_flags & TF_SACK_PERMIT;
#endif
#ifdef TCP_ECN
		/*
		 * if ECE is set but CWR is not set for SYN-ACK, or
		 * both ECE and CWR are set for simultaneous open,
		 * peer is ECN capable.
		 */
		if (tcp_do_ecn) {
			if ((tiflags & (TH_ACK|TH_ECE|TH_CWR))
			    == (TH_ACK|TH_ECE) ||
			    (tiflags & (TH_ACK|TH_ECE|TH_CWR))
			    == (TH_ECE|TH_CWR)) {
				tp->t_flags |= TF_ECN_PERMIT;
				tiflags &= ~(TH_ECE|TH_CWR);
				tcpstat.tcps_ecn_accepts++;
			}
		}
#endif

		if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)) {
			tcpstat.tcps_connects++;
			soisconnected(so);
			tp->t_state = TCPS_ESTABLISHED;
			TCP_TIMER_ARM(tp, TCPT_KEEP, tcp_keepidle);
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
			tcp_reass_lock(tp);
			(void) tcp_reass(tp, (struct tcphdr *)0,
				(struct mbuf *)0, &tlen);
			tcp_reass_unlock(tp);
			/*
			 * if we didn't have to retransmit the SYN,
			 * use its rtt as our initial srtt & rtt var.
			 */
			if (tp->t_rtttime)
				tcp_xmit_timer(tp, tcp_now - tp->t_rtttime);
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

#if 0
trimthenstep6:
#endif
		/*
		 * Advance th->th_seq to correspond to first data byte.
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
	 * and it's less than opti.ts_recent, drop it.
	 */
	if (opti.ts_present && (tiflags & TH_RST) == 0 && tp->ts_recent &&
	    TSTMP_LT(opti.ts_val, tp->ts_recent)) {

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
		if (todrop > tlen ||
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
		hdroptlen += todrop;	/* drop from head afterwards */
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
				iss = tp->snd_nxt + TCP_ISSINCR;
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
	 * record its timestamp if it's more recent.
	 * Cf fix from Braden, see Stevens p. 870
	 */
	if (opti.ts_present && TSTMP_GEQ(opti.ts_val, tp->ts_recent) &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
		if (SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
		    ((tiflags & (TH_SYN|TH_FIN)) != 0)))
			tp->ts_recent = opti.ts_val;
		else
			tp->ts_recent = 0;
		tp->ts_recent_age = tcp_now;
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
		if (th->th_seq != tp->last_ack_sent &&
		    th->th_seq != tp->rcv_nxt)
			goto drop;

		switch (tp->t_state) {
		case TCPS_SYN_RECEIVED:
#ifdef TCP_ECN
			/* if ECN is enabled, fall back to non-ecn at rexmit */
			if (tcp_do_ecn && !(tp->t_flags & TF_DISABLE_ECN))
				goto drop;
#endif
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
	 * error and we ACK and drop the packet.
	 */
	if (tiflags & TH_SYN)
		goto dropafterack_ratelim;

	/*
	 * If the ACK bit is off we drop the segment and return.
	 */
	if ((tiflags & TH_ACK) == 0) {
		if (tp->t_flags & TF_ACKNOW)
			goto dropafterack;
		else
			goto drop;
	}

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
		TCP_TIMER_ARM(tp, TCPT_KEEP, tcp_keepidle);
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		tcp_reass_lock(tp);
		(void) tcp_reass(tp, (struct tcphdr *)0, (struct mbuf *)0,
				 &tlen);
		tcp_reass_unlock(tp);
		tp->snd_wl1 = th->th_seq - 1;
		/* fall into ... */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < th->th_ack <= tp->snd_max
	 * then advance tp->snd_una to th->th_ack and drop
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
#ifdef TCP_ECN
		/*
		 * if we receive ECE and are not already in recovery phase,
		 * reduce cwnd by half but don't slow-start.
		 * advance snd_last to snd_max not to reduce cwnd again
		 * until all outstanding packets are acked.
		 */
		if (tcp_do_ecn && (tiflags & TH_ECE)) {
			if ((tp->t_flags & TF_ECN_PERMIT) &&
			    SEQ_GEQ(tp->snd_una, tp->snd_last)) {
				u_int win;

				win = min(tp->snd_wnd, tp->snd_cwnd) / tp->t_maxseg;
				if (win > 1) {
					tp->snd_ssthresh = win / 2 * tp->t_maxseg;
					tp->snd_cwnd = tp->snd_ssthresh;
					tp->snd_last = tp->snd_max;
					tp->t_flags |= TF_SEND_CWR;
					tcpstat.tcps_cwr_ecn++;
				}
			}
			tcpstat.tcps_ecn_rcvece++;
		}
		/*
		 * if we receive CWR, we know that the peer has reduced
		 * its congestion window.  stop sending ecn-echo.
		 */
		if ((tiflags & TH_CWR)) {
			tp->t_flags &= ~TF_RCVD_CE;
			tcpstat.tcps_ecn_rcvcwr++;
		}
#endif /* TCP_ECN */

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
			if (tlen) {
				/* Drop very old ACKs unless th_seq matches */
				if (th->th_seq != tp->rcv_nxt &&
				   SEQ_LT(th->th_ack,
				   tp->snd_una - tp->max_sndwnd)) {
					tcpstat.tcps_rcvacktooold++;
					goto drop;
				}
				break;
			}
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
				 * threshold of them, assume a packet
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
				if (TCP_TIMER_ISARMED(tp, TCPT_REXMT) == 0)
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

#if defined(TCP_SACK) || defined(TCP_ECN)
					if (SEQ_LT(th->th_ack, tp->snd_last)){
					    	/*
						 * False fast retx after
						 * timeout.  Do not cut window.
						 */
						tp->t_dupacks = 0;
						goto drop;
					}
#endif
					if (win < 2)
						win = 2;
					tp->snd_ssthresh = win * tp->t_maxseg;
#if defined(TCP_SACK)
					tp->snd_last = tp->snd_max;
#endif
#ifdef TCP_SACK
                    			if (tp->sack_enable) {
						TCP_TIMER_DISARM(tp, TCPT_REXMT);
						tp->t_rtttime = 0;
#ifdef TCP_ECN
						tp->t_flags |= TF_SEND_CWR;
#endif
#if 1 /* TCP_ECN */
						tcpstat.tcps_cwr_frecovery++;
#endif
						tcpstat.tcps_sack_recovery_episode++;
#if defined(TCP_SACK) && defined(TCP_FACK)
						tp->t_dupacks = tcprexmtthresh;
						(void) tcp_output(tp);
						/*
						 * During FR, snd_cwnd is held
						 * constant for FACK.
						 */
						tp->snd_cwnd = tp->snd_ssthresh;
#else
						/*
						 * tcp_output() will send
						 * oldest SACK-eligible rtx.
						 */
						(void) tcp_output(tp);
						tp->snd_cwnd = tp->snd_ssthresh+
					           tp->t_maxseg * tp->t_dupacks;
#endif /* TCP_FACK */
						goto drop;
					}
#endif /* TCP_SACK */
					TCP_TIMER_DISARM(tp, TCPT_REXMT);
					tp->t_rtttime = 0;
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = tp->t_maxseg;
#ifdef TCP_ECN
					tp->t_flags |= TF_SEND_CWR;
#endif
#if 1 /* TCP_ECN */
					tcpstat.tcps_cwr_frecovery++;
#endif
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
					if (tp->sack_enable) {
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
#if defined(TCP_SACK)
		if (tp->sack_enable) {
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
					           th->th_ack);
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
					    th->th_ack);
				tp->t_dupacks = 0;
			}
		}
		if (tp->t_dupacks < tcprexmtthresh)
			tp->t_dupacks = 0;
#else /* else no TCP_SACK */
		if (tp->t_dupacks >= tcprexmtthresh &&
		    tp->snd_cwnd > tp->snd_ssthresh)
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_dupacks = 0;
#endif
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			tcpstat.tcps_rcvacktoomuch++;
			goto dropafterack_ratelim;
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
		if (opti.ts_present && opti.ts_ecr)
			tcp_xmit_timer(tp, tcp_now - opti.ts_ecr);
		else if (tp->t_rtttime && SEQ_GT(th->th_ack, tp->t_rtseq))
			tcp_xmit_timer(tp, tcp_now - tp->t_rtttime);

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			TCP_TIMER_DISARM(tp, TCPT_REXMT);
			needoutput = 1;
		} else if (TCP_TIMER_ISARMED(tp, TCPT_PERSIST) == 0)
			TCP_TIMER_ARM(tp, TCPT_REXMT, tp->t_rxtcur);
		/*
		 * When new data is acked, open the congestion window.
		 * If the window gives us less than ssthresh packets
		 * in flight, open exponentially (maxseg per packet).
		 * Otherwise open linearly: maxseg per window
		 * (maxseg^2 / cwnd per packet).
		 */
		{
		u_int cw = tp->snd_cwnd;
		u_int incr = tp->t_maxseg;

		if (cw > tp->snd_ssthresh)
			incr = incr * incr / cw;
#if defined (TCP_SACK)
		if (tp->t_dupacks < tcprexmtthresh)
#endif
		tp->snd_cwnd = ulmin(cw + incr, TCP_MAXWIN<<tp->snd_scale);
		}
		ND6_HINT(tp);
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

		/*
		 * If we had a pending ICMP message that referred to data
		 * that have just been acknowledged, disregard the recorded
		 * ICMP message.
		 */
		if ((tp->t_flags & TF_PMTUD_PEND) && 
		    SEQ_GT(th->th_ack, tp->t_pmtud_th_seq))
			tp->t_flags &= ~TF_PMTUD_PEND;

		/*
		 * Keep track of the largest chunk of data acknowledged
		 * since last PMTU update
		 */
		if (tp->t_pmtud_mss_acked < acked)
		    tp->t_pmtud_mss_acked = acked;

		tp->snd_una = th->th_ack;
#ifdef TCP_ECN
		/* sync snd_last with snd_una */
		if (SEQ_GT(tp->snd_una, tp->snd_last))
			tp->snd_last = tp->snd_una;
#endif
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;
#if defined (TCP_SACK) && defined (TCP_FACK)
		if (SEQ_GT(tp->snd_una, tp->snd_fack)) {
			tp->snd_fack = tp->snd_una;
			/* Update snd_awnd for partial ACK
			 * without any SACK blocks.
			 */
			tp->snd_awnd = tcp_seq_subtract(tp->snd_nxt,
				tp->snd_fack) + tp->retran_data;
		}
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
					TCP_TIMER_ARM(tp, TCPT_2MSL, tcp_maxidle);
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
				TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * TCPTV_MSL);
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
			TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * TCPTV_MSL);
			goto dropafterack;
		}
	}

step6:
	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((tiflags & TH_ACK) && (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && SEQ_LT(tp->snd_wl2, th->th_ack)) ||
	    (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))) {
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
		        tcp_pulloutofband(so, th->th_urp, m, hdroptlen);
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
		tcp_reass_lock(tp);
		if (th->th_seq == tp->rcv_nxt && TAILQ_EMPTY(&tp->t_segq) &&
		    tp->t_state == TCPS_ESTABLISHED) {
			tcp_reass_unlock(tp);
			TCP_SETUP_ACK(tp, tiflags);
			tp->rcv_nxt += tlen;
			tiflags = th->th_flags & TH_FIN;
			tcpstat.tcps_rcvpack++;
			tcpstat.tcps_rcvbyte += tlen;
			ND6_HINT(tp);
			if (so->so_state & SS_CANTRCVMORE)
				m_freem(m);
			else {
				m_adj(m, hdroptlen);
				sbappendstream(&so->so_rcv, m);
			}
			sorwakeup(so);
		} else {
			m_adj(m, hdroptlen);
			tiflags = tcp_reass(tp, th, m, &tlen);
			tcp_reass_unlock(tp);
			tp->t_flags |= TF_ACKNOW;
		}
#ifdef TCP_SACK
		if (tp->sack_enable)
			tcp_update_sack_list(tp, th->th_seq, th->th_seq + tlen);
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
			TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * TCPTV_MSL);
			soisdisconnected(so);
			break;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			TCP_TIMER_ARM(tp, TCPT_2MSL, 2 * TCPTV_MSL);
			break;
		}
	}
	if (so->so_options & SO_DEBUG) {
		switch (tp->pf) {
#ifdef INET6
		case PF_INET6:
			tcp_trace(TA_INPUT, ostate, tp, (caddr_t) &tcp_saveti6,
			    0, tlen);
			break;
#endif /* INET6 */
		case PF_INET:
			tcp_trace(TA_INPUT, ostate, tp, (caddr_t) &tcp_saveti,
			    0, tlen);
			break;
		}
	}

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW)) {
		(void) tcp_output(tp);
	}
	return;

badsyn:
	/*
	 * Received a bad SYN.  Increment counters and dropwithreset.
	 */
	tcpstat.tcps_badsyn++;
	tp = NULL;
	goto dropwithreset;

dropafterack_ratelim:
	if (ppsratecheck(&tcp_ackdrop_ppslim_last, &tcp_ackdrop_ppslim_count,
	    tcp_ackdrop_ppslim) == 0) {
		/* XXX stat */
		goto drop;
	}
	/* ...fall into dropafterack... */

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

dropwithreset_ratelim:
	/*
	 * We may want to rate-limit RSTs in certain situations,
	 * particularly if we are sending an RST in response to
	 * an attempt to connect to or otherwise communicate with
	 * a port for which we have no socket.
	 */
	if (ppsratecheck(&tcp_rst_ppslim_last, &tcp_rst_ppslim_count,
	    tcp_rst_ppslim) == 0) {
		/* XXX stat */
		goto drop;
	}
	/* ...fall into dropwithreset... */

dropwithreset:
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond to RST.
	 */
	if (tiflags & TH_RST)
		goto drop;
	if (tiflags & TH_ACK) {
		tcp_respond(tp, mtod(m, caddr_t), m, (tcp_seq)0, th->th_ack,
		    TH_RST);
	} else {
		if (tiflags & TH_SYN)
			tlen++;
		tcp_respond(tp, mtod(m, caddr_t), m, th->th_seq + tlen,
		    (tcp_seq)0, TH_RST|TH_ACK);
	}
	return;

drop:
	/*
	 * Drop space held by incoming segment and return.
	 */
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG)) {
		switch (tp->pf) {
#ifdef INET6
		case PF_INET6:
			tcp_trace(TA_DROP, ostate, tp, (caddr_t) &tcp_saveti6,
			    0, tlen);
			break;
#endif /* INET6 */
		case PF_INET:
			tcp_trace(TA_DROP, ostate, tp, (caddr_t) &tcp_saveti,
			    0, tlen);
			break;
		}
	}

	m_freem(m);
	return;
}

int
tcp_dooptions(tp, cp, cnt, th, m, iphlen, oi)
	struct tcpcb *tp;
	u_char *cp;
	int cnt;
	struct tcphdr *th;
	struct mbuf *m;
	int iphlen;
	struct tcp_opt_info *oi;
{
	u_int16_t mss = 0;
	int opt, optlen;
#ifdef TCP_SIGNATURE
	caddr_t sigp = NULL;
	struct tdb *tdb = NULL;
#endif /* TCP_SIGNATURE */

	for (; cp && cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt)
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
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			bcopy((char *) cp + 2, (char *) &mss, sizeof(mss));
			NTOHS(mss);
			oi->maxseg = mss;
			break;

		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (!(th->th_flags & TH_SYN))
				continue;
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			tp->t_flags |= TF_RCVD_SCALE;
			tp->requested_s_scale = min(cp[2], TCP_MAX_WINSHIFT);
			break;

		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			oi->ts_present = 1;
			bcopy(cp + 2, &oi->ts_val, sizeof(oi->ts_val));
			NTOHL(oi->ts_val);
			bcopy(cp + 6, &oi->ts_ecr, sizeof(oi->ts_ecr));
			NTOHL(oi->ts_ecr);

			if (!(th->th_flags & TH_SYN))
				continue;
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			/*
			 * A timestamp received in a SYN makes
			 * it ok to send timestamp requests and replies.
			 */
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = oi->ts_val;
			tp->ts_recent_age = tcp_now;
			break;

#ifdef TCP_SACK
		case TCPOPT_SACK_PERMITTED:
			if (!tp->sack_enable || optlen!=TCPOLEN_SACK_PERMITTED)
				continue;
			if (!(th->th_flags & TH_SYN))
				continue;
			if (TCPS_HAVERCVDSYN(tp->t_state))
				continue;
			/* MUST only be set on SYN */
			tp->t_flags |= TF_SACK_PERMIT;
			break;
		case TCPOPT_SACK:
			tcp_sack_option(tp, th, cp, optlen);
			break;
#endif
#ifdef TCP_SIGNATURE
		case TCPOPT_SIGNATURE:
			if (optlen != TCPOLEN_SIGNATURE)
				continue;

			if (sigp && bcmp(sigp, cp + 2, 16))
				return (-1);

			sigp = cp + 2;
			break;
#endif /* TCP_SIGNATURE */
		}
	}

#ifdef TCP_SIGNATURE
	if (tp->t_flags & TF_SIGNATURE) {
		union sockaddr_union src, dst;

		memset(&src, 0, sizeof(union sockaddr_union));
		memset(&dst, 0, sizeof(union sockaddr_union));

		switch (tp->pf) {
		case 0:
#ifdef INET
		case AF_INET:
			src.sa.sa_len = sizeof(struct sockaddr_in);
			src.sa.sa_family = AF_INET;
			src.sin.sin_addr = mtod(m, struct ip *)->ip_src;
			dst.sa.sa_len = sizeof(struct sockaddr_in);
			dst.sa.sa_family = AF_INET;
			dst.sin.sin_addr = mtod(m, struct ip *)->ip_dst;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			src.sa.sa_len = sizeof(struct sockaddr_in6);
			src.sa.sa_family = AF_INET6;
			src.sin6.sin6_addr = mtod(m, struct ip6_hdr *)->ip6_src;
			dst.sa.sa_len = sizeof(struct sockaddr_in6);
			dst.sa.sa_family = AF_INET6;
			dst.sin6.sin6_addr = mtod(m, struct ip6_hdr *)->ip6_dst;
			break;
#endif /* INET6 */
		}

		tdb = gettdbbysrcdst(0, &src, &dst, IPPROTO_TCP);

		/*
		 * We don't have an SA for this peer, so we turn off
		 * TF_SIGNATURE on the listen socket
		 */
		if (tdb == NULL && tp->t_state == TCPS_LISTEN)
			tp->t_flags &= ~TF_SIGNATURE;

	}

	if ((sigp ? TF_SIGNATURE : 0) ^ (tp->t_flags & TF_SIGNATURE)) {
		tcpstat.tcps_rcvbadsig++;
		return (-1);
	}

	if (sigp) {
		char sig[16];

		if (tdb == NULL) {
			tcpstat.tcps_rcvbadsig++;
			return (-1);
		}

		if (tcp_signature(tdb, tp->pf, m, th, iphlen, 1, sig) < 0)
			return (-1);

		if (bcmp(sig, sigp, 16)) {
			tcpstat.tcps_rcvbadsig++;
			return (-1);
		}

		tcpstat.tcps_rcvgoodsig++;
	}
#endif /* TCP_SIGNATURE */

	return (0);
}

#if defined(TCP_SACK)
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
tcp_update_sack_list(struct tcpcb *tp, tcp_seq rcv_laststart,
    tcp_seq rcv_lastend)
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
		if (SEQ_LT(tp->rcv_nxt, rcv_laststart)) {
			/* ==> need first sack block */
			tp->sackblks[0].start = rcv_laststart;
			tp->sackblks[0].end = rcv_lastend;
			tp->rcv_numsacks = 1;
		}
		return;
	}
	/* Otherwise, sack blocks are already present. */
	for (i = 0; i < tp->rcv_numsacks; i++)
		tp->sackblks[i] = temp[i]; /* first copy back sack list */
	if (SEQ_GEQ(tp->rcv_nxt, rcv_lastend))
		return;     /* sack list remains unchanged */
	/*
	 * From here, segment just received should be (part of) the 1st sack.
	 * Go through list, possibly coalescing sack block entries.
	 */
	firstsack.start = rcv_laststart;
	firstsack.end = rcv_lastend;
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
 * Process the TCP SACK option.  tp->snd_holes is an ordered list
 * of holes (oldest to newest, in terms of the sequence space).
 */
void
tcp_sack_option(struct tcpcb *tp, struct tcphdr *th, u_char *cp, int optlen)
{
	int tmp_olen;
	u_char *tmp_cp;
	struct sackhole *cur, *p, *temp;

	if (!tp->sack_enable)
		return;
	/* SACK without ACK doesn't make sense. */
	if ((th->th_flags & TH_ACK) == 0)
	       return;
	/* Make sure the ACK on this segment is in [snd_una, snd_max]. */
	if (SEQ_LT(th->th_ack, tp->snd_una) ||
	    SEQ_GT(th->th_ack, tp->snd_max))
		return;
	/* Note: TCPOLEN_SACK must be 2*sizeof(tcp_seq) */
	if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0)
		return;
	/* Note: TCPOLEN_SACK must be 2*sizeof(tcp_seq) */
	tmp_cp = cp + 2;
	tmp_olen = optlen - 2;
	tcpstat.tcps_sack_rcv_opts++;
	if (tp->snd_numholes < 0)
		tp->snd_numholes = 0;
	if (tp->t_maxseg == 0)
		panic("tcp_sack_option"); /* Should never happen */
	while (tmp_olen > 0) {
		struct sackblk sack;

		bcopy(tmp_cp, (char *) &(sack.start), sizeof(tcp_seq));
		NTOHL(sack.start);
		bcopy(tmp_cp + sizeof(tcp_seq),
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
		if (SEQ_GT(sack.end, tp->snd_fack))
			tp->snd_fack = sack.end;
#endif /* TCP_FACK */
		if (SEQ_GT(th->th_ack, tp->snd_una)) {
			if (SEQ_LT(sack.start, th->th_ack))
				continue;
		}
		if (SEQ_GT(sack.end, tp->snd_max))
			continue;
		if (tp->snd_holes == NULL) { /* first hole */
			tp->snd_holes = (struct sackhole *)
			    pool_get(&sackhl_pool, PR_NOWAIT);
			if (tp->snd_holes == NULL) {
				/* ENOBUFS, so ignore SACKed block for now*/
				goto done;
			}
			cur = tp->snd_holes;
			cur->start = th->th_ack;
			cur->end = sack.start;
			cur->rxmit = cur->start;
			cur->next = NULL;
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
				if (((sack.end - cur->end)/tp->t_maxseg) >=
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
				if (SEQ_GEQ(sack.end, cur->end)) {
					/* Acks entire hole, so delete hole */
					if (p != cur) {
						p->next = cur->next;
						pool_put(&sackhl_pool, cur);
						cur = p->next;
					} else {
						cur = cur->next;
						pool_put(&sackhl_pool, p);
						p = cur;
						tp->snd_holes = p;
					}
					tp->snd_numholes--;
					continue;
				}
				/* otherwise, move start of hole forward */
				cur->start = sack.end;
				cur->rxmit = SEQ_MAX(cur->rxmit, cur->start);
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
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur->dups++;
				if (((sack.end - cur->end)/tp->t_maxseg) >=
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
				temp = (struct sackhole *)
				    pool_get(&sackhl_pool, PR_NOWAIT);
				if (temp == NULL)
					goto done; /* ENOBUFS */
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
				temp->next = cur->next;
				temp->start = sack.end;
				temp->end = cur->end;
				temp->dups = cur->dups;
				temp->rxmit = SEQ_MAX(cur->rxmit, temp->start);
				cur->end = sack.start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur->dups++;
				if (((sack.end - cur->end)/tp->t_maxseg) >=
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
			temp = (struct sackhole *)
			    pool_get(&sackhl_pool, PR_NOWAIT);
			if (temp == NULL)
				goto done; /* ENOBUFS */
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
done:
#if defined(TCP_SACK) && defined(TCP_FACK)
	/*
	 * Update retran_data and snd_awnd.  Go through the list of
	 * holes.   Increment retran_data by (hole->rxmit - hole->start).
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

	return;
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
	if (tp->sack_enable && tp->t_state != TCPS_LISTEN) {
		/* max because this could be an older ack just arrived */
		tcp_seq lastack = SEQ_GT(th->th_ack, tp->snd_una) ?
			th->th_ack : tp->snd_una;
		struct sackhole *cur = tp->snd_holes;
		struct sackhole *prev;
		while (cur)
			if (SEQ_LEQ(cur->end, lastack)) {
				prev = cur;
				cur = cur->next;
				pool_put(&sackhl_pool, prev);
				tp->snd_numholes--;
			} else if (SEQ_LT(cur->start, lastack)) {
				cur->start = lastack;
				if (SEQ_LT(cur->rxmit, cur->start))
					cur->rxmit = cur->start;
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
		TCP_TIMER_DISARM(tp, TCPT_REXMT);
		tp->t_rtttime = 0;
#ifndef TCP_FACK
		/*
		 * Partial window deflation.  This statement relies on the
		 * fact that tp->snd_una has not been updated yet.  In FACK
		 * hold snd_cwnd constant during fast recovery.
		 */
		if (tp->snd_cwnd > (th->th_ack - tp->snd_una)) {
			tp->snd_cwnd -= th->th_ack - tp->snd_una;
			tp->snd_cwnd += tp->t_maxseg;
		} else
			tp->snd_cwnd = tp->t_maxseg;
#endif
		return (1);
	}
	return (0);
}
#endif /* TCP_SACK */

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
void
tcp_pulloutofband(so, urgent, m, off)
	struct socket *so;
	u_int urgent;
	struct mbuf *m;
	int off;
{
        int cnt = off + urgent - 1;

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
	struct tcpcb *tp;
	short rtt;
{
	short delta;
	short rttmin;

	if (rtt < 0)
		rtt = 0;
	else if (rtt > TCP_RTT_MAX)
		rtt = TCP_RTT_MAX;

	tcpstat.tcps_rttupdated++;
	if (tp->t_srtt != 0) {
		/*
		 * delta is fixed point with 2 (TCP_RTT_BASE_SHIFT) bits
		 * after the binary point (scaled by 4), whereas
		 * srtt is stored as fixed point with 5 bits after the
		 * binary point (i.e., scaled by 32).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).
		 */
		delta = (rtt << TCP_RTT_BASE_SHIFT) -
		    (tp->t_srtt >> TCP_RTT_SHIFT);
		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1 << TCP_RTT_BASE_SHIFT;
		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 4 bits after the
		 * binary point (scaled by 16).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1 << TCP_RTT_BASE_SHIFT;
	} else {
		/*
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = (rtt + 1) << (TCP_RTT_SHIFT + TCP_RTT_BASE_SHIFT);
		tp->t_rttvar = (rtt + 1) <<
		    (TCP_RTTVAR_SHIFT + TCP_RTT_BASE_SHIFT - 1);
	}
	tp->t_rtttime = 0;
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
	rttmin = min(max(rtt + 2, tp->t_rttmin), TCPTV_REXMTMAX);
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp), rttmin, TCPTV_REXMTMAX);

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
 *
 * NOTE: offer == -1 indicates that the maxseg size changed due to
 * Path MTU discovery.
 */
int
tcp_mss(tp, offer)
	struct tcpcb *tp;
	int offer;
{
	struct rtentry *rt;
	struct ifnet *ifp;
	int mss, mssopt;
	int iphlen;
	struct inpcb *inp;

	inp = tp->t_inpcb;

	mssopt = mss = tcp_mssdflt;

	rt = in_pcbrtentry(inp);

	if (rt == NULL)
		goto out;

	ifp = rt->rt_ifp;

	switch (tp->pf) {
#ifdef INET6
	case AF_INET6:
		iphlen = sizeof(struct ip6_hdr);
		break;
#endif
	case AF_INET:
		iphlen = sizeof(struct ip);
		break;
	default:
		/* the family does not support path MTU discovery */
		goto out;
	}

#ifdef RTV_MTU
	/*
	 * if there's an mtu associated with the route and we support
	 * path MTU discovery for the underlying protocol family, use it.
	 */
	if (rt->rt_rmx.rmx_mtu) {
		/*
		 * One may wish to lower MSS to take into account options,
		 * especially security-related options.
		 */
		if (tp->pf == AF_INET6 && rt->rt_rmx.rmx_mtu < IPV6_MMTU) {
			/*
			 * RFC2460 section 5, last paragraph: if path MTU is
			 * smaller than 1280, use 1280 as packet size and
			 * attach fragment header.
			 */
			mss = IPV6_MMTU - iphlen - sizeof(struct ip6_frag) -
			    sizeof(struct tcphdr);
		} else
			mss = rt->rt_rmx.rmx_mtu - iphlen - sizeof(struct tcphdr);
	} else
#endif /* RTV_MTU */
	if (!ifp)
		/*
		 * ifp may be null and rmx_mtu may be zero in certain
		 * v6 cases (e.g., if ND wasn't able to resolve the
		 * destination host.
		 */
		goto out;
	else if (ifp->if_flags & IFF_LOOPBACK)
		mss = ifp->if_mtu - iphlen - sizeof(struct tcphdr);
	else if (tp->pf == AF_INET) {
		if (ip_mtudisc)
			mss = ifp->if_mtu - iphlen - sizeof(struct tcphdr);
		else if (inp && in_localaddr(inp->inp_faddr))
			mss = ifp->if_mtu - iphlen - sizeof(struct tcphdr);
	}
#ifdef INET6
	else if (tp->pf == AF_INET6) {
		/*
		 * for IPv6, path MTU discovery is always turned on,
		 * or the node must use packet size <= 1280.
		 */
		mss = IN6_LINKMTU(ifp) - iphlen - sizeof(struct tcphdr);
	}
#endif /* INET6 */

	/* Calculate the value that we offer in TCPOPT_MAXSEG */
	if (offer != -1) {
#ifndef INET6
		mssopt = ifp->if_mtu - iphlen - sizeof(struct tcphdr);
#else
		if (tp->pf == AF_INET6)
			mssopt = IN6_LINKMTU(ifp) - iphlen -
			    sizeof(struct tcphdr);
		else
			mssopt = ifp->if_mtu - iphlen - sizeof(struct tcphdr);
#endif

		mssopt = max(tcp_mssdflt, mssopt);
	}

 out:
	/*
	 * The current mss, t_maxseg, is initialized to the default value.
	 * If we compute a smaller value, reduce the current mss.
	 * If we compute a larger value, return it for use in sending
	 * a max seg size option, but don't store it for use
	 * unless we received an offer at least that large from peer.
	 * 
	 * However, do not accept offers lower than the minimum of
	 * the interface MTU and 216.
	 */
	if (offer > 0)
		tp->t_peermss = offer;
	if (tp->t_peermss)
		mss = min(mss, max(tp->t_peermss, 216));

	/* sanity - at least max opt. space */
	mss = max(mss, 64);

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
#ifdef TCP_SIGNATURE
	if (tp->t_flags & TF_SIGNATURE)
		mss -= TCPOLEN_SIGLEN;
#endif

	if (offer == -1) {
		/* mss changed due to Path MTU discovery */
		tp->t_flags &= ~TF_PMTUD_PEND;
		tp->t_pmtud_mtu_sent = 0;
		tp->t_pmtud_mss_acked = 0;
		if (mss < tp->t_maxseg) {
			/*
			 * Follow suggestion in RFC 2414 to reduce the
			 * congestion window by the ratio of the old
			 * segment size to the new segment size.
			 */
			tp->snd_cwnd = ulmax((tp->snd_cwnd / tp->t_maxseg) *
					     mss, mss);
		}
	} else if (tcp_do_rfc3390) {
		/* increase initial window  */
		tp->snd_cwnd = ulmin(4 * mss, ulmax(2 * mss, 4380));
	} else
		tp->snd_cwnd = mss;

	tp->t_maxseg = mss;

	return (offer != -1 ? mssopt : mss);
}

u_int
tcp_hdrsz(struct tcpcb *tp)
{
	u_int hlen;

	switch (tp->pf) {
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif
	case AF_INET:
		hlen = sizeof(struct ip);
		break;
	default:
		hlen = 0;
		break;
	}
	hlen += sizeof(struct tcphdr);

	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	    (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP)
		hlen += TCPOLEN_TSTAMP_APPA;
#ifdef TCP_SIGNATURE
	if (tp->t_flags & TF_SIGNATURE)
		hlen += TCPOLEN_SIGLEN;
#endif
	return (hlen);
}

/*
 * Set connection variables based on the effective MSS.
 * We are passed the TCPCB for the actual connection.  If we
 * are the server, we are called by the compressed state engine
 * when the 3-way handshake is complete.  If we are the client,
 * we are called when we receive the SYN,ACK from the server.
 *
 * NOTE: The t_maxseg value must be initialized in the TCPCB
 * before this routine is called!
 */
void
tcp_mss_update(tp)
	struct tcpcb *tp;
{
	int mss;
	u_long bufsize;
	struct rtentry *rt;
	struct socket *so;

	so = tp->t_inpcb->inp_socket;
	mss = tp->t_maxseg;

	rt = in_pcbrtentry(tp->t_inpcb);

	if (rt == NULL)
		return;

	bufsize = so->so_snd.sb_hiwat;
	if (bufsize < mss) {
		mss = bufsize;
		/* Update t_maxseg and t_maxopd */
		tcp_mss(tp, mss);
	} else {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		(void)sbreserve(&so->so_snd, bufsize);
	}

	bufsize = so->so_rcv.sb_hiwat;
	if (bufsize > mss) {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		(void)sbreserve(&so->so_rcv, bufsize);
	}

}

#if defined (TCP_SACK)
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
		/*
		 * snd_una has not been updated and the socket send buffer
		 * not yet drained of the acked data, so we have to leave
		 * snd_una as it was to get the correct data offset in
		 * tcp_output().
		 */
		tcp_seq onxt = tp->snd_nxt;
		u_long  ocwnd = tp->snd_cwnd;
		TCP_TIMER_DISARM(tp, TCPT_REXMT);
		tp->t_rtttime = 0;
		tp->snd_nxt = th->th_ack;
		/*
		 * Set snd_cwnd to one segment beyond acknowledged offset
		 * (tp->snd_una not yet updated when this function is called)
		 */
		tp->snd_cwnd = tp->t_maxseg + (th->th_ack - tp->snd_una);
		(void) tcp_output(tp);
		tp->snd_cwnd = ocwnd;
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
#endif /* TCP_SACK */

static int
tcp_mss_adv(struct ifnet *ifp, int af)
{
	int mss = 0;
	int iphlen;

	switch (af) {
	case AF_INET:
		if (ifp != NULL)
			mss = ifp->if_mtu;
		iphlen = sizeof(struct ip);
		break;
#ifdef INET6
	case AF_INET6: 
		if (ifp != NULL)
			mss = IN6_LINKMTU(ifp);
		iphlen = sizeof(struct ip6_hdr);
		break;
#endif  
	}
	mss = mss - iphlen - sizeof(struct tcphdr);
	return (max(mss, tcp_mssdflt));
}

/*
 * TCP compressed state engine.  Currently used to hold compressed
 * state for SYN_RECEIVED.
 */

u_long	syn_cache_count;
u_int32_t syn_hash1, syn_hash2;

#define SYN_HASH(sa, sp, dp) \
	((((sa)->s_addr^syn_hash1)*(((((u_int32_t)(dp))<<16) + \
				     ((u_int32_t)(sp)))^syn_hash2)))
#ifndef INET6
#define	SYN_HASHALL(hash, src, dst) \
do {									\
	hash = SYN_HASH(&((struct sockaddr_in *)(src))->sin_addr,	\
		((struct sockaddr_in *)(src))->sin_port,		\
		((struct sockaddr_in *)(dst))->sin_port);		\
} while (/*CONSTCOND*/ 0)
#else
#define SYN_HASH6(sa, sp, dp) \
	((((sa)->s6_addr32[0] ^ (sa)->s6_addr32[3] ^ syn_hash1) * \
	  (((((u_int32_t)(dp))<<16) + ((u_int32_t)(sp)))^syn_hash2)) \
	 & 0x7fffffff)

#define SYN_HASHALL(hash, src, dst) \
do {									\
	switch ((src)->sa_family) {					\
	case AF_INET:							\
		hash = SYN_HASH(&((struct sockaddr_in *)(src))->sin_addr, \
			((struct sockaddr_in *)(src))->sin_port,	\
			((struct sockaddr_in *)(dst))->sin_port);	\
		break;							\
	case AF_INET6:							\
		hash = SYN_HASH6(&((struct sockaddr_in6 *)(src))->sin6_addr, \
			((struct sockaddr_in6 *)(src))->sin6_port,	\
			((struct sockaddr_in6 *)(dst))->sin6_port);	\
		break;							\
	default:							\
		hash = 0;						\
	}								\
} while (/*CONSTCOND*/0)
#endif /* INET6 */

#define	SYN_CACHE_RM(sc)						\
do {									\
	(sc)->sc_flags |= SCF_DEAD;					\
	TAILQ_REMOVE(&tcp_syn_cache[(sc)->sc_bucketidx].sch_bucket,	\
	    (sc), sc_bucketq);						\
	(sc)->sc_tp = NULL;						\
	LIST_REMOVE((sc), sc_tpq);					\
	tcp_syn_cache[(sc)->sc_bucketidx].sch_length--;			\
	timeout_del(&(sc)->sc_timer);					\
	syn_cache_count--;						\
} while (/*CONSTCOND*/0)

#define	SYN_CACHE_PUT(sc)						\
do {									\
	if ((sc)->sc_ipopts)						\
		(void) m_free((sc)->sc_ipopts);				\
	if ((sc)->sc_route4.ro_rt != NULL)				\
		RTFREE((sc)->sc_route4.ro_rt);				\
	timeout_set(&(sc)->sc_timer, syn_cache_reaper, (sc));		\
	timeout_add(&(sc)->sc_timer, 0);				\
} while (/*CONSTCOND*/0)

struct pool syn_cache_pool;

/*
 * We don't estimate RTT with SYNs, so each packet starts with the default
 * RTT and each timer step has a fixed timeout value.
 */
#define	SYN_CACHE_TIMER_ARM(sc)						\
do {									\
	TCPT_RANGESET((sc)->sc_rxtcur,					\
	    TCPTV_SRTTDFLT * tcp_backoff[(sc)->sc_rxtshift], TCPTV_MIN,	\
	    TCPTV_REXMTMAX);						\
	if (!timeout_initialized(&(sc)->sc_timer))			\
		timeout_set(&(sc)->sc_timer, syn_cache_timer, (sc));	\
	timeout_add(&(sc)->sc_timer, (sc)->sc_rxtcur * (hz / PR_SLOWHZ)); \
} while (/*CONSTCOND*/0)

#define	SYN_CACHE_TIMESTAMP(sc)	tcp_now + (sc)->sc_modulate

void
syn_cache_init()
{
	int i;

	/* Initialize the hash buckets. */
	for (i = 0; i < tcp_syn_cache_size; i++)
		TAILQ_INIT(&tcp_syn_cache[i].sch_bucket);

	/* Initialize the syn cache pool. */
	pool_init(&syn_cache_pool, sizeof(struct syn_cache), 0, 0, 0,
	    "synpl", NULL);
}

void
syn_cache_insert(sc, tp)
	struct syn_cache *sc;
	struct tcpcb *tp;
{
	struct syn_cache_head *scp;
	struct syn_cache *sc2;
	int s;

	/*
	 * If there are no entries in the hash table, reinitialize
	 * the hash secrets.
	 */
	if (syn_cache_count == 0) {
		syn_hash1 = arc4random();
		syn_hash2 = arc4random();
	}

	SYN_HASHALL(sc->sc_hash, &sc->sc_src.sa, &sc->sc_dst.sa);
	sc->sc_bucketidx = sc->sc_hash % tcp_syn_cache_size;
	scp = &tcp_syn_cache[sc->sc_bucketidx];

	/*
	 * Make sure that we don't overflow the per-bucket
	 * limit or the total cache size limit.
	 */
	s = splsoftnet();
	if (scp->sch_length >= tcp_syn_bucket_limit) {
		tcpstat.tcps_sc_bucketoverflow++;
		/*
		 * The bucket is full.  Toss the oldest element in the
		 * bucket.  This will be the first entry in the bucket.
		 */
		sc2 = TAILQ_FIRST(&scp->sch_bucket);
#ifdef DIAGNOSTIC
		/*
		 * This should never happen; we should always find an
		 * entry in our bucket.
		 */
		if (sc2 == NULL)
			panic("syn_cache_insert: bucketoverflow: impossible");
#endif
		SYN_CACHE_RM(sc2);
		SYN_CACHE_PUT(sc2);
	} else if (syn_cache_count >= tcp_syn_cache_limit) {
		struct syn_cache_head *scp2, *sce;

		tcpstat.tcps_sc_overflowed++;
		/*
		 * The cache is full.  Toss the oldest entry in the
		 * first non-empty bucket we can find.
		 *
		 * XXX We would really like to toss the oldest
		 * entry in the cache, but we hope that this
		 * condition doesn't happen very often.
		 */
		scp2 = scp;
		if (TAILQ_EMPTY(&scp2->sch_bucket)) {
			sce = &tcp_syn_cache[tcp_syn_cache_size];
			for (++scp2; scp2 != scp; scp2++) {
				if (scp2 >= sce)
					scp2 = &tcp_syn_cache[0];
				if (! TAILQ_EMPTY(&scp2->sch_bucket))
					break;
			}
#ifdef DIAGNOSTIC
			/*
			 * This should never happen; we should always find a
			 * non-empty bucket.
			 */
			if (scp2 == scp)
				panic("syn_cache_insert: cacheoverflow: "
				    "impossible");
#endif
		}
		sc2 = TAILQ_FIRST(&scp2->sch_bucket);
		SYN_CACHE_RM(sc2);
		SYN_CACHE_PUT(sc2);
	}

	/*
	 * Initialize the entry's timer.
	 */
	sc->sc_rxttot = 0;
	sc->sc_rxtshift = 0;
	SYN_CACHE_TIMER_ARM(sc);

	/* Link it from tcpcb entry */
	LIST_INSERT_HEAD(&tp->t_sc, sc, sc_tpq);

	/* Put it into the bucket. */
	TAILQ_INSERT_TAIL(&scp->sch_bucket, sc, sc_bucketq);
	scp->sch_length++;
	syn_cache_count++;

	tcpstat.tcps_sc_added++;
	splx(s);
}

/*
 * Walk the timer queues, looking for SYN,ACKs that need to be retransmitted.
 * If we have retransmitted an entry the maximum number of times, expire
 * that entry.
 */
void
syn_cache_timer(void *arg)
{
	struct syn_cache *sc = arg;
	int s;

	s = splsoftnet();
	if (sc->sc_flags & SCF_DEAD) {
		splx(s);
		return;
	}

	if (__predict_false(sc->sc_rxtshift == TCP_MAXRXTSHIFT)) {
		/* Drop it -- too many retransmissions. */
		goto dropit;
	}

	/*
	 * Compute the total amount of time this entry has
	 * been on a queue.  If this entry has been on longer
	 * than the keep alive timer would allow, expire it.
	 */
	sc->sc_rxttot += sc->sc_rxtcur;
	if (sc->sc_rxttot >= tcptv_keep_init)
		goto dropit;

	tcpstat.tcps_sc_retransmitted++;
	(void) syn_cache_respond(sc, NULL);

	/* Advance the timer back-off. */
	sc->sc_rxtshift++;
	SYN_CACHE_TIMER_ARM(sc);

	splx(s);
	return;

 dropit:
	tcpstat.tcps_sc_timed_out++;
	SYN_CACHE_RM(sc);
	SYN_CACHE_PUT(sc);
	splx(s);
}

void
syn_cache_reaper(void *arg)
{
	struct syn_cache *sc = arg;
	int s;

	s = splsoftnet();
	pool_put(&syn_cache_pool, (sc));
	splx(s);
	return;
}

/*
 * Remove syn cache created by the specified tcb entry,
 * because this does not make sense to keep them
 * (if there's no tcb entry, syn cache entry will never be used)
 */
void
syn_cache_cleanup(tp)
	struct tcpcb *tp;
{
	struct syn_cache *sc, *nsc;
	int s;

	s = splsoftnet();

	for (sc = LIST_FIRST(&tp->t_sc); sc != NULL; sc = nsc) {
		nsc = LIST_NEXT(sc, sc_tpq);

#ifdef DIAGNOSTIC
		if (sc->sc_tp != tp)
			panic("invalid sc_tp in syn_cache_cleanup");
#endif
		SYN_CACHE_RM(sc);
		SYN_CACHE_PUT(sc);
	}
	/* just for safety */
	LIST_INIT(&tp->t_sc);

	splx(s);
}

/*
 * Find an entry in the syn cache.
 */
struct syn_cache *
syn_cache_lookup(src, dst, headp)
	struct sockaddr *src;
	struct sockaddr *dst;
	struct syn_cache_head **headp;
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	u_int32_t hash;
	int s;

	SYN_HASHALL(hash, src, dst);

	scp = &tcp_syn_cache[hash % tcp_syn_cache_size];
	*headp = scp;
	s = splsoftnet();
	for (sc = TAILQ_FIRST(&scp->sch_bucket); sc != NULL;
	     sc = TAILQ_NEXT(sc, sc_bucketq)) {
		if (sc->sc_hash != hash)
			continue;
		if (!bcmp(&sc->sc_src, src, src->sa_len) &&
		    !bcmp(&sc->sc_dst, dst, dst->sa_len)) {
			splx(s);
			return (sc);
		}
	}
	splx(s);
	return (NULL);
}

/*
 * This function gets called when we receive an ACK for a
 * socket in the LISTEN state.  We look up the connection
 * in the syn cache, and if its there, we pull it out of
 * the cache and turn it into a full-blown connection in
 * the SYN-RECEIVED state.
 *
 * The return values may not be immediately obvious, and their effects
 * can be subtle, so here they are:
 *
 *	NULL	SYN was not found in cache; caller should drop the
 *		packet and send an RST.
 *
 *	-1	We were unable to create the new connection, and are
 *		aborting it.  An ACK,RST is being sent to the peer
 *		(unless we got screwey sequence numbners; see below),
 *		because the 3-way handshake has been completed.  Caller
 *		should not free the mbuf, since we may be using it.  If
 *		we are not, we will free it.
 *
 *	Otherwise, the return value is a pointer to the new socket
 *	associated with the connection.
 */
struct socket *
syn_cache_get(src, dst, th, hlen, tlen, so, m)
	struct sockaddr *src;
	struct sockaddr *dst;
	struct tcphdr *th;
	unsigned int hlen, tlen;
	struct socket *so;
	struct mbuf *m;
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	struct inpcb *inp = NULL;
	struct tcpcb *tp = 0;
	struct mbuf *am;
	int s;
	struct socket *oso;

	s = splsoftnet();
	if ((sc = syn_cache_lookup(src, dst, &scp)) == NULL) {
		splx(s);
		return (NULL);
	}

	/*
	 * Verify the sequence and ack numbers.  Try getting the correct
	 * response again.
	 */
	if ((th->th_ack != sc->sc_iss + 1) ||
	    SEQ_LEQ(th->th_seq, sc->sc_irs) ||
	    SEQ_GT(th->th_seq, sc->sc_irs + 1 + sc->sc_win)) {
		(void) syn_cache_respond(sc, m);
		splx(s);
		return ((struct socket *)(-1));
	}

	/* Remove this cache entry */
	SYN_CACHE_RM(sc);
	splx(s);

	/*
	 * Ok, create the full blown connection, and set things up
	 * as they would have been set up if we had created the
	 * connection when the SYN arrived.  If we can't create
	 * the connection, abort it.
	 */
	oso = so;
	so = sonewconn(so, SS_ISCONNECTED);
	if (so == NULL)
		goto resetandabort;

	inp = sotoinpcb(oso);
#ifdef IPSEC
	/*
	 * We need to copy the required security levels
	 * from the old pcb. Ditto for any other
	 * IPsec-related information.
	 */
	{
	  struct inpcb *newinp = (struct inpcb *)so->so_pcb;
	  bcopy(inp->inp_seclevel, newinp->inp_seclevel,
		sizeof(inp->inp_seclevel));
	  newinp->inp_secrequire = inp->inp_secrequire;
	  if (inp->inp_ipo != NULL) {
		  newinp->inp_ipo = inp->inp_ipo;
		  inp->inp_ipo->ipo_ref_count++;
	  }
	  if (inp->inp_ipsec_remotecred != NULL) {
		  newinp->inp_ipsec_remotecred = inp->inp_ipsec_remotecred;
		  inp->inp_ipsec_remotecred->ref_count++;
	  }
	  if (inp->inp_ipsec_remoteauth != NULL) {
		  newinp->inp_ipsec_remoteauth
		      = inp->inp_ipsec_remoteauth;
		  inp->inp_ipsec_remoteauth->ref_count++;
	  }
	}
#endif /* IPSEC */
#ifdef INET6
	/*
	 * inp still has the OLD in_pcb stuff, set the
	 * v6-related flags on the new guy, too.
	 */
	{
	  int flags = inp->inp_flags;
	  struct inpcb *oldinpcb = inp;

	  inp = (struct inpcb *)so->so_pcb;
	  inp->inp_flags |= (flags & INP_IPV6);
	  if ((inp->inp_flags & INP_IPV6) != 0) {
	    inp->inp_ipv6.ip6_hlim =
	      oldinpcb->inp_ipv6.ip6_hlim;
	  }
	}
#else /* INET6 */
	inp = (struct inpcb *)so->so_pcb;
#endif /* INET6 */

	inp->inp_lport = th->th_dport;
	switch (src->sa_family) {
#ifdef INET6
	case AF_INET6:
		inp->inp_laddr6 = ((struct sockaddr_in6 *)dst)->sin6_addr;
		break;
#endif /* INET6 */
	case AF_INET:

		inp->inp_laddr = ((struct sockaddr_in *)dst)->sin_addr;
		inp->inp_options = ip_srcroute();
		if (inp->inp_options == NULL) {
			inp->inp_options = sc->sc_ipopts;
			sc->sc_ipopts = NULL;
		}
		break;
	}
	in_pcbrehash(inp);

	/*
	 * Give the new socket our cached route reference.
	 */
	if (src->sa_family == AF_INET)
		inp->inp_route = sc->sc_route4;         /* struct assignment */
#ifdef INET6
	else
		inp->inp_route6 = sc->sc_route6;
#endif  
	sc->sc_route4.ro_rt = NULL;

	am = m_get(M_DONTWAIT, MT_SONAME);	/* XXX */
	if (am == NULL)
		goto resetandabort;
	am->m_len = src->sa_len;
	bcopy(src, mtod(am, caddr_t), src->sa_len);

	switch (src->sa_family) {
	case AF_INET:
		/* drop IPv4 packet to AF_INET6 socket */
		if (inp->inp_flags & INP_IPV6) {
			(void) m_free(am);
			goto resetandabort;
		}
		if (in_pcbconnect(inp, am)) {
			(void) m_free(am);
			goto resetandabort;
		}
		break;
#ifdef INET6
	case AF_INET6:
		if (in6_pcbconnect(inp, am)) {
			(void) m_free(am);
			goto resetandabort;
		}
		break;
#endif
	}
	(void) m_free(am);

	tp = intotcpcb(inp);
	tp->t_flags = sototcpcb(oso)->t_flags & TF_NODELAY;
	if (sc->sc_request_r_scale != 15) {
		tp->requested_s_scale = sc->sc_requested_s_scale;
		tp->request_r_scale = sc->sc_request_r_scale;
		tp->snd_scale = sc->sc_requested_s_scale;
		tp->rcv_scale = sc->sc_request_r_scale;
		tp->t_flags |= TF_REQ_SCALE|TF_RCVD_SCALE;
	}
	if (sc->sc_flags & SCF_TIMESTAMP)
		tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;

	tp->t_template = tcp_template(tp);
	if (tp->t_template == 0) {
		tp = tcp_drop(tp, ENOBUFS);	/* destroys socket */
		so = NULL;
		m_freem(m);
		goto abort;
	}
#ifdef TCP_SACK
	tp->sack_enable = sc->sc_flags & SCF_SACK_PERMIT;
#endif

	tp->ts_modulate = sc->sc_modulate;
	tp->iss = sc->sc_iss;
	tp->irs = sc->sc_irs;
	tcp_sendseqinit(tp);
#if defined (TCP_SACK) || defined(TCP_ECN)
	tp->snd_last = tp->snd_una;
#endif /* TCP_SACK */
#if defined(TCP_SACK) && defined(TCP_FACK)
	tp->snd_fack = tp->snd_una;
	tp->retran_data = 0;
	tp->snd_awnd = 0;
#endif /* TCP_FACK */
#ifdef TCP_ECN
	if (sc->sc_flags & SCF_ECN_PERMIT) {
		tp->t_flags |= TF_ECN_PERMIT;
		tcpstat.tcps_ecn_accepts++;
	}
#endif
#ifdef TCP_SACK
	if (sc->sc_flags & SCF_SACK_PERMIT)
		tp->t_flags |= TF_SACK_PERMIT;
#endif
#ifdef TCP_SIGNATURE
	if (sc->sc_flags & SCF_SIGNATURE)
		tp->t_flags |= TF_SIGNATURE;
#endif
	tcp_rcvseqinit(tp);
	tp->t_state = TCPS_SYN_RECEIVED;
	tp->t_rcvtime = tcp_now;
	TCP_TIMER_ARM(tp, TCPT_KEEP, tcptv_keep_init);
	tcpstat.tcps_accepts++;

	tcp_mss(tp, sc->sc_peermaxseg);	 /* sets t_maxseg */
	if (sc->sc_peermaxseg)
		tcp_mss_update(tp);
	/* Reset initial window to 1 segment for retransmit */
	if (sc->sc_rxtshift > 0)
		tp->snd_cwnd = tp->t_maxseg;
	tp->snd_wl1 = sc->sc_irs;
	tp->rcv_up = sc->sc_irs + 1;

	/*
	 * This is what whould have happened in tcp_output() when
	 * the SYN,ACK was sent.
	 */
	tp->snd_up = tp->snd_una;
	tp->snd_max = tp->snd_nxt = tp->iss+1;
	TCP_TIMER_ARM(tp, TCPT_REXMT, tp->t_rxtcur);
	if (sc->sc_win > 0 && SEQ_GT(tp->rcv_nxt + sc->sc_win, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + sc->sc_win;
	tp->last_ack_sent = tp->rcv_nxt;

	tcpstat.tcps_sc_completed++;
	SYN_CACHE_PUT(sc);
	return (so);

resetandabort:
	tcp_respond(NULL, mtod(m, caddr_t), m, (tcp_seq)0, th->th_ack, TH_RST);
abort:
	if (so != NULL)
		(void) soabort(so);
	SYN_CACHE_PUT(sc);
	tcpstat.tcps_sc_aborted++;
	return ((struct socket *)(-1));
}

/*
 * This function is called when we get a RST for a
 * non-existent connection, so that we can see if the
 * connection is in the syn cache.  If it is, zap it.
 */

void
syn_cache_reset(src, dst, th)
	struct sockaddr *src;
	struct sockaddr *dst;
	struct tcphdr *th;
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	int s = splsoftnet();

	if ((sc = syn_cache_lookup(src, dst, &scp)) == NULL) {
		splx(s);
		return;
	}
	if (SEQ_LT(th->th_seq, sc->sc_irs) ||
	    SEQ_GT(th->th_seq, sc->sc_irs+1)) {
		splx(s);
		return;
	}
	SYN_CACHE_RM(sc);
	splx(s);
	tcpstat.tcps_sc_reset++;
	SYN_CACHE_PUT(sc);
}

void
syn_cache_unreach(src, dst, th)
	struct sockaddr *src;
	struct sockaddr *dst;
	struct tcphdr *th;
{
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	int s;

	s = splsoftnet();
	if ((sc = syn_cache_lookup(src, dst, &scp)) == NULL) {
		splx(s);
		return;
	}
	/* If the sequence number != sc_iss, then it's a bogus ICMP msg */
	if (ntohl (th->th_seq) != sc->sc_iss) {
		splx(s);
		return;
	}

	/*
	 * If we've retransmitted 3 times and this is our second error,
	 * we remove the entry.  Otherwise, we allow it to continue on.
	 * This prevents us from incorrectly nuking an entry during a
	 * spurious network outage.
	 *
	 * See tcp_notify().
	 */
	if ((sc->sc_flags & SCF_UNREACH) == 0 || sc->sc_rxtshift < 3) {
		sc->sc_flags |= SCF_UNREACH;
		splx(s);
		return;
	}

	SYN_CACHE_RM(sc);
	splx(s);
	tcpstat.tcps_sc_unreach++;
	SYN_CACHE_PUT(sc);
}

/*
 * Given a LISTEN socket and an inbound SYN request, add
 * this to the syn cache, and send back a segment:
 *	<SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
 * to the source.
 *
 * IMPORTANT NOTE: We do _NOT_ ACK data that might accompany the SYN.
 * Doing so would require that we hold onto the data and deliver it
 * to the application.  However, if we are the target of a SYN-flood
 * DoS attack, an attacker could send data which would eventually
 * consume all available buffer space if it were ACKed.  By not ACKing
 * the data, we avoid this DoS scenario.
 */

int
syn_cache_add(src, dst, th, iphlen, so, m, optp, optlen, oi)
	struct sockaddr *src;
	struct sockaddr *dst;
	struct tcphdr *th;
	unsigned int iphlen;
	struct socket *so;
	struct mbuf *m;
	u_char *optp;
	int optlen;
	struct tcp_opt_info *oi;
{
	struct tcpcb tb, *tp;
	long win;
	struct syn_cache *sc;
	struct syn_cache_head *scp;
	struct mbuf *ipopts;

	tp = sototcpcb(so);

	/*
	 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
	 *
	 * Note this check is performed in tcp_input() very early on.
	 */

	/*
	 * Initialize some local state.
	 */
	win = sbspace(&so->so_rcv);
	if (win > TCP_MAXWIN)
		win = TCP_MAXWIN;

#ifdef TCP_SIGNATURE
	if (optp || (tp->t_flags & TF_SIGNATURE)) {
#else
	if (optp) {
#endif
		tb.pf = tp->pf;
#ifdef TCP_SACK
		tb.sack_enable = tp->sack_enable;
#endif
		tb.t_flags = tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;
#ifdef TCP_SIGNATURE
		if (tp->t_flags & TF_SIGNATURE)
			tb.t_flags |= TF_SIGNATURE;
#endif
		tb.t_state = TCPS_LISTEN;
		if (tcp_dooptions(&tb, optp, optlen, th, m, iphlen, oi))
			return (0);
	} else
		tb.t_flags = 0;

	switch (src->sa_family) {
#ifdef INET
	case AF_INET:
		/*
		 * Remember the IP options, if any.
		 */
		ipopts = ip_srcroute();
		break;
#endif
	default:
		ipopts = NULL;
	}

	/*
	 * See if we already have an entry for this connection.
	 * If we do, resend the SYN,ACK.  We do not count this
	 * as a retransmission (XXX though maybe we should).
	 */
	if ((sc = syn_cache_lookup(src, dst, &scp)) != NULL) {
		tcpstat.tcps_sc_dupesyn++;
		if (ipopts) {
			/*
			 * If we were remembering a previous source route,
			 * forget it and use the new one we've been given.
			 */
			if (sc->sc_ipopts)
				(void) m_free(sc->sc_ipopts);
			sc->sc_ipopts = ipopts;
		}
		sc->sc_timestamp = tb.ts_recent;
		if (syn_cache_respond(sc, m) == 0) {
			tcpstat.tcps_sndacks++;
			tcpstat.tcps_sndtotal++;
		}
		return (1);
	}

	sc = pool_get(&syn_cache_pool, PR_NOWAIT);
	if (sc == NULL) {
		if (ipopts)
			(void) m_free(ipopts);
		return (0);
	}

	/*
	 * Fill in the cache, and put the necessary IP and TCP
	 * options into the reply.
	 */
	bzero(sc, sizeof(struct syn_cache));
	bzero(&sc->sc_timer, sizeof(sc->sc_timer));
	bcopy(src, &sc->sc_src, src->sa_len);
	bcopy(dst, &sc->sc_dst, dst->sa_len);
	sc->sc_flags = 0;
	sc->sc_ipopts = ipopts;
	sc->sc_irs = th->th_seq;

#ifdef TCP_COMPAT_42
	tcp_iss += TCP_ISSINCR/2;
	sc->sc_iss = tcp_iss;
#else
	sc->sc_iss = tcp_rndiss_next();
#endif
	sc->sc_peermaxseg = oi->maxseg;
	sc->sc_ourmaxseg = tcp_mss_adv(m->m_flags & M_PKTHDR ?
	    m->m_pkthdr.rcvif : NULL, sc->sc_src.sa.sa_family);
	sc->sc_win = win;
	sc->sc_timestamp = tb.ts_recent;
	if ((tb.t_flags & (TF_REQ_TSTMP|TF_RCVD_TSTMP)) ==
	    (TF_REQ_TSTMP|TF_RCVD_TSTMP))
		sc->sc_flags |= SCF_TIMESTAMP;
	if ((tb.t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
	    (TF_RCVD_SCALE|TF_REQ_SCALE)) {
		sc->sc_requested_s_scale = tb.requested_s_scale;
		sc->sc_request_r_scale = 0;
		while (sc->sc_request_r_scale < TCP_MAX_WINSHIFT &&
		    TCP_MAXWIN << sc->sc_request_r_scale <
		    so->so_rcv.sb_hiwat)
			sc->sc_request_r_scale++;
	} else {
		sc->sc_requested_s_scale = 15;
		sc->sc_request_r_scale = 15;
	}
#ifdef TCP_ECN
	/*
	 * if both ECE and CWR flag bits are set, peer is ECN capable.
	 */
	if (tcp_do_ecn &&
	    (th->th_flags & (TH_ECE|TH_CWR)) == (TH_ECE|TH_CWR))
		sc->sc_flags |= SCF_ECN_PERMIT;
#endif
#ifdef TCP_SACK
	/*
	 * Set SCF_SACK_PERMIT if peer did send a SACK_PERMITTED option
	 * (i.e., if tcp_dooptions() did set TF_SACK_PERMIT).
	 */
	if (tb.sack_enable && (tb.t_flags & TF_SACK_PERMIT))
		sc->sc_flags |= SCF_SACK_PERMIT;
#endif
#ifdef TCP_SIGNATURE
	if (tb.t_flags & TF_SIGNATURE)
		sc->sc_flags |= SCF_SIGNATURE;
#endif
	sc->sc_tp = tp;
	if (syn_cache_respond(sc, m) == 0) {
		syn_cache_insert(sc, tp);
		tcpstat.tcps_sndacks++;
		tcpstat.tcps_sndtotal++;
	} else {
		SYN_CACHE_PUT(sc);
		tcpstat.tcps_sc_dropped++;
	}
	return (1);
}

int
syn_cache_respond(sc, m)
	struct syn_cache *sc;
	struct mbuf *m;
{
	struct route *ro;
	u_int8_t *optp;
	int optlen, error;
	u_int16_t tlen;
	struct ip *ip = NULL;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
	struct tcphdr *th;
	u_int hlen;
	struct inpcb *inp;

	switch (sc->sc_src.sa.sa_family) {
	case AF_INET:
		hlen = sizeof(struct ip);
		ro = &sc->sc_route4;
		break;
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		ro = (struct route *)&sc->sc_route6;
		break;
#endif
	default:
		if (m)
			m_freem(m);
		return (EAFNOSUPPORT);
	}

	/* Compute the size of the TCP options. */
	optlen = 4 + (sc->sc_request_r_scale != 15 ? 4 : 0) +
#ifdef TCP_SACK
	    ((sc->sc_flags & SCF_SACK_PERMIT) ? 4 : 0) +
#endif
#ifdef TCP_SIGNATURE
	    ((sc->sc_flags & SCF_SIGNATURE) ? TCPOLEN_SIGLEN : 0) +
#endif
	    ((sc->sc_flags & SCF_TIMESTAMP) ? TCPOLEN_TSTAMP_APPA : 0);

	tlen = hlen + sizeof(struct tcphdr) + optlen;

	/*
	 * Create the IP+TCP header from scratch.
	 */
	if (m)
		m_freem(m);
#ifdef DIAGNOSTIC
	if (max_linkhdr + tlen > MCLBYTES)
		return (ENOBUFS);
#endif
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m && max_linkhdr + tlen > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			m = NULL;
		}
	}
	if (m == NULL)
		return (ENOBUFS);

	/* Fixup the mbuf. */
	m->m_data += max_linkhdr;
	m->m_len = m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;
	memset(mtod(m, u_char *), 0, tlen);

	switch (sc->sc_src.sa.sa_family) {
	case AF_INET:
		ip = mtod(m, struct ip *);
		ip->ip_dst = sc->sc_src.sin.sin_addr;
		ip->ip_src = sc->sc_dst.sin.sin_addr;
		ip->ip_p = IPPROTO_TCP;
		th = (struct tcphdr *)(ip + 1);
		th->th_dport = sc->sc_src.sin.sin_port;
		th->th_sport = sc->sc_dst.sin.sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_dst = sc->sc_src.sin6.sin6_addr;
		ip6->ip6_src = sc->sc_dst.sin6.sin6_addr;
		ip6->ip6_nxt = IPPROTO_TCP;
		/* ip6_plen will be updated in ip6_output() */
		th = (struct tcphdr *)(ip6 + 1);
		th->th_dport = sc->sc_src.sin6.sin6_port;
		th->th_sport = sc->sc_dst.sin6.sin6_port;
		break;
#endif
	default:
		th = NULL;
	}

	th->th_seq = htonl(sc->sc_iss);
	th->th_ack = htonl(sc->sc_irs + 1);
	th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
	th->th_flags = TH_SYN|TH_ACK;
#ifdef TCP_ECN
	/* Set ECE for SYN-ACK if peer supports ECN. */
	if (tcp_do_ecn && (sc->sc_flags & SCF_ECN_PERMIT))
		th->th_flags |= TH_ECE;
#endif
	th->th_win = htons(sc->sc_win);
	/* th_sum already 0 */
	/* th_urp already 0 */

	/* Tack on the TCP options. */
	optp = (u_int8_t *)(th + 1);
	*optp++ = TCPOPT_MAXSEG;
	*optp++ = 4;
	*optp++ = (sc->sc_ourmaxseg >> 8) & 0xff;
	*optp++ = sc->sc_ourmaxseg & 0xff;

#ifdef TCP_SACK
	/* Include SACK_PERMIT_HDR option if peer has already done so. */
	if (sc->sc_flags & SCF_SACK_PERMIT) {
		*((u_int32_t *)optp) = htonl(TCPOPT_SACK_PERMIT_HDR);
		optp += 4;
	}
#endif

	if (sc->sc_request_r_scale != 15) {
		*((u_int32_t *)optp) = htonl(TCPOPT_NOP << 24 |
		    TCPOPT_WINDOW << 16 | TCPOLEN_WINDOW << 8 |
		    sc->sc_request_r_scale);
		optp += 4;
	}

	if (sc->sc_flags & SCF_TIMESTAMP) {
		u_int32_t *lp = (u_int32_t *)(optp);
		/* Form timestamp option as shown in appendix A of RFC 1323. */
		*lp++ = htonl(TCPOPT_TSTAMP_HDR);
		sc->sc_modulate = arc4random();
		*lp++ = htonl(SYN_CACHE_TIMESTAMP(sc));
		*lp   = htonl(sc->sc_timestamp);
		optp += TCPOLEN_TSTAMP_APPA;
	}

#ifdef TCP_SIGNATURE
	if (sc->sc_flags & SCF_SIGNATURE) {
		union sockaddr_union src, dst;
		struct tdb *tdb;

		bzero(&src, sizeof(union sockaddr_union));
		bzero(&dst, sizeof(union sockaddr_union));
		src.sa.sa_len = sc->sc_src.sa.sa_len;
		src.sa.sa_family = sc->sc_src.sa.sa_family;
		dst.sa.sa_len = sc->sc_dst.sa.sa_len;
		dst.sa.sa_family = sc->sc_dst.sa.sa_family;

		switch (sc->sc_src.sa.sa_family) {
		case 0:	/*default to PF_INET*/
#ifdef INET
		case AF_INET:
			src.sin.sin_addr = mtod(m, struct ip *)->ip_src;
			dst.sin.sin_addr = mtod(m, struct ip *)->ip_dst;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			src.sin6.sin6_addr = mtod(m, struct ip6_hdr *)->ip6_src;
			dst.sin6.sin6_addr = mtod(m, struct ip6_hdr *)->ip6_dst;
			break;
#endif /* INET6 */
		}

		tdb = gettdbbysrcdst(0, &src, &dst, IPPROTO_TCP);
		if (tdb == NULL) {
			if (m)
				m_freem(m);
			return (EPERM);
		}

		/* Send signature option */
		*(optp++) = TCPOPT_SIGNATURE;
		*(optp++) = TCPOLEN_SIGNATURE;

		if (tcp_signature(tdb, sc->sc_src.sa.sa_family, m, th,
		    hlen, 0, optp) < 0) {
			if (m)
				m_freem(m);
			return (EINVAL);
		}
		optp += 16;

		/* Pad options list to the next 32 bit boundary and
		 * terminate it.
		 */
		*optp++ = TCPOPT_NOP;
		*optp++ = TCPOPT_EOL;
	}
#endif /* TCP_SIGNATURE */

	/* Compute the packet's checksum. */
	switch (sc->sc_src.sa.sa_family) {
	case AF_INET:
		ip->ip_len = htons(tlen - hlen);
		th->th_sum = 0;
		th->th_sum = in_cksum(m, tlen);
		break;
#ifdef INET6
	case AF_INET6:
		ip6->ip6_plen = htons(tlen - hlen);
		th->th_sum = 0;
		th->th_sum = in6_cksum(m, IPPROTO_TCP, hlen, tlen - hlen);
		break;
#endif
	}

	/*
	 * Fill in some straggling IP bits.  Note the stack expects
	 * ip_len to be in host order, for convenience.
	 */
	switch (sc->sc_src.sa.sa_family) {
#ifdef INET
	case AF_INET:
		ip->ip_len = htons(tlen);
		ip->ip_ttl = ip_defttl;
		/* XXX tos? */
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_plen = htons(tlen - hlen);
		/* ip6_hlim will be initialized afterwards */
		/* leave flowlabel = 0, it is legal and require no state mgmt */
		break;
#endif
	}

	/* use IPsec policy from listening socket, on SYN ACK */
	inp = sc->sc_tp ? sc->sc_tp->t_inpcb : NULL;

	switch (sc->sc_src.sa.sa_family) {
#ifdef INET
	case AF_INET:
		error = ip_output(m, sc->sc_ipopts, ro,
		    (ip_mtudisc ? IP_MTUDISC : 0), 
		    (struct ip_moptions *)NULL, inp);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ip6->ip6_hlim = in6_selecthlim(NULL,
				ro->ro_rt ? ro->ro_rt->rt_ifp : NULL);

		error = ip6_output(m, NULL /*XXX*/, (struct route_in6 *)ro, 0,
			(struct ip6_moptions *)0, NULL);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		break;
	}
	return (error);
}
