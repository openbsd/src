/*	$OpenBSD: tcp_subr.c,v 1.25 2000/03/21 04:53:13 angelos Exp $	*/
/*	$NetBSD: tcp_subr.c,v 1.22 1996/02/13 23:44:00 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)tcp_subr.c	8.1 (Berkeley) 6/10/93
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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <dev/rndvar.h>

#ifdef INET6
#include <netinet6/ip6_var.h>
#include <netinet6/tcpipv6.h>
#include <sys/domain.h>
#endif /* INET6 */

#ifdef TCP_SIGNATURE
#include <sys/md5k.h>
#endif /* TCP_SIGNATURE */

/* patchable/settable parameters for tcp */
int	tcp_mssdflt = TCP_MSS;
int	tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;

/*
 * Configure kernel with options "TCP_DO_RFC1323=0" to disable RFC1323 stuff.
 * This is a good idea over slow SLIP/PPP links, because the timestamp
 * pretty well destroys the VJ compression (any packet with a timestamp
 * different from the previous one can't be compressed), as well as adding
 * more overhead.
 * XXX And it should be a settable per route characteristic (with this just
 * used as the default).
 */
#ifndef TCP_DO_RFC1323
#define TCP_DO_RFC1323	1
#endif
int    tcp_do_rfc1323 = TCP_DO_RFC1323;

#ifndef TCP_DO_SACK
#ifdef TCP_SACK
#define TCP_DO_SACK	1
#else
#define TCP_DO_SACK	0
#endif
#endif
int    tcp_do_sack = TCP_DO_SACK;		/* RFC 2018 selective ACKs */

#ifndef TCBHASHSIZE
#define	TCBHASHSIZE	128
#endif
int	tcbhashsize = TCBHASHSIZE;

#ifdef INET6
extern int ip6_defhlim;
#endif /* INET6 */

/*
 * Tcp initialization
 */
void
tcp_init()
{
#ifdef TCP_COMPAT_42
	tcp_iss = 1;		/* wrong */
#else /* TCP_COMPAT_42 */
	tcp_iss = arc4random() + 1;
#endif /* !TCP_COMPAT_42 */
	in_pcbinit(&tcbtable, tcbhashsize);

#ifdef INET6
	/*
	 * Since sizeof(struct ip6_hdr) > sizeof(struct ip), we
	 * do max length checks/computations only on the former.
	 */
	if (max_protohdr < (sizeof(struct ip6_hdr) + sizeof(struct tcphdr)))
		max_protohdr = (sizeof(struct ip6_hdr) + sizeof(struct tcphdr));
	if ((max_linkhdr + sizeof(struct ip6_hdr) + sizeof(struct tcphdr)) >
	    MHLEN)
		panic("tcp_init");
#endif /* INET6 */
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 *
 * To support IPv6 in addition to IPv4 and considering that the sizes of
 * the IPv4 and IPv6 headers are not the same, we now use a separate pointer
 * for the TCP header.  Also, we made the former tcpiphdr header pointer 
 * into just an IP overlay pointer, with casting as appropriate for v6. rja
 */
struct mbuf *
tcp_template(tp)
	struct tcpcb *tp;
{
	register struct inpcb *inp = tp->t_inpcb;
	register struct mbuf *m;
	register struct tcphdr *th;

	if ((m = tp->t_template) == 0) {
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return (0);

		switch (tp->pf) {
		case 0:	/*default to PF_INET*/
#ifdef INET
		case AF_INET:
			m->m_len = sizeof(struct ip);
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			m->m_len = sizeof(struct ip6_hdr);
			break;
#endif /* INET6 */
		}
		m->m_len += sizeof (struct tcphdr);

		/*
		 * The link header, network header, TCP header, and TCP options
		 * all must fit in this mbuf. For now, assume the worst case of
		 * TCP options size. Eventually, compute this from tp flags.
		 */ 
		if (m->m_len + MAX_TCPOPTLEN + max_linkhdr >= MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				return (0);
			}
		}
	}

	switch(tp->pf) {
#ifdef INET
	case AF_INET:
		{
			struct ipovly *ipovly;

			ipovly = mtod(m, struct ipovly *);

			bzero(ipovly->ih_x1, sizeof ipovly->ih_x1);
			ipovly->ih_pr = IPPROTO_TCP;
			ipovly->ih_len = htons(sizeof (struct tcpiphdr) -
				sizeof (struct ip));
			ipovly->ih_src = inp->inp_laddr;
			ipovly->ih_dst = inp->inp_faddr;

			th = (struct tcphdr *)(mtod(m, caddr_t) +
				sizeof(struct ip));
		}
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		{
			struct ip6_hdr *ipv6;

			ipv6 = mtod(m, struct ip6_hdr *);

			ipv6->ip6_src = inp->inp_laddr6;
			ipv6->ip6_dst = inp->inp_faddr6;
			ipv6->ip6_flow = htonl(0x60000000) |
			    (inp->inp_ipv6.ip6_flow & htonl(0x0fffffff));  
						  

			ipv6->ip6_nxt = IPPROTO_TCP;
			ipv6->ip6_plen = htons(sizeof(struct tcphdr)); /*XXX*/
			ipv6->ip6_hlim = in6_selecthlim(inp, NULL);	/*XXX*/

			th = (struct tcphdr *)(mtod(m, caddr_t) +
				sizeof(struct ip6_hdr));
		}
		break;
#endif /* INET6 */
	}

	th->th_sport = inp->inp_lport;
	th->th_dport = inp->inp_fport;
	th->th_seq = 0;
	th->th_ack = 0;
	th->th_x2  = 0;
	th->th_off = 5;
	th->th_flags = 0;
	th->th_win = 0;
	th->th_sum = 0;
	th->th_urp = 0;
	return (m);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */
#ifdef INET6
/* This function looks hairy, because it was so IPv4-dependent. */
#endif /* INET6 */
void
tcp_respond(tp, template, m, ack, seq, flags)
	struct tcpcb *tp;
	caddr_t template;
	register struct mbuf *m;
	tcp_seq ack, seq;
	int flags;
{
	register int tlen;
	int win = 0;
	struct route *ro = 0;
	register struct tcphdr *th;
	register struct tcpiphdr *ti = (struct tcpiphdr *)template;
#ifdef INET6
	int is_ipv6 = 0;   /* true iff IPv6 */
#endif /* INET6 */

	if (tp) {
		win = sbspace(&tp->t_inpcb->inp_socket->so_rcv);
#ifdef INET6
		/*
		 * If this is called with an unconnected
		 * socket/tp/pcb (tp->pf is 0), we lose.
		 */
		is_ipv6 = (tp->pf == PF_INET6);

		/*
		 * The route/route6 distinction is meaningless
		 * unless you're allocating space or passing parameters.
		 */
#endif /* INET6 */
		ro = &tp->t_inpcb->inp_route;
	}
#ifdef INET6
	else
		is_ipv6 = (((struct ip *)ti)->ip_v == 6);
#endif /* INET6 */
	if (m == 0) {
		m = m_gethdr(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return;
#ifdef TCP_COMPAT_42
		tlen = 1;
#else
		tlen = 0;
#endif
		m->m_data += max_linkhdr;
#ifdef INET6
		if (is_ipv6)
			bcopy(ti, mtod(m, caddr_t), sizeof(struct tcphdr) +
			    sizeof(struct ip6_hdr));
		else
#endif /* INET6 */
			bcopy(ti, mtod(m, caddr_t), sizeof(struct tcphdr) +
			    sizeof(struct ip));

		ti = mtod(m, struct tcpiphdr *);
		flags = TH_ACK;
	} else {
		m_freem(m->m_next);
		m->m_next = 0;
		m->m_data = (caddr_t)ti;
		tlen = 0;
#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
#ifdef INET6
		if (is_ipv6) {
			m->m_len = sizeof(struct tcphdr) + sizeof(struct ip6_hdr);
			xchg(((struct ip6_hdr *)ti)->ip6_dst,\
			    ((struct ip6_hdr *)ti)->ip6_src,\
			    struct in6_addr);
			th = (void *)ti + sizeof(struct ip6_hdr);
		} else
#endif /* INET6 */
		{
			m->m_len = sizeof (struct tcpiphdr);
			xchg(ti->ti_dst.s_addr, ti->ti_src.s_addr, u_int32_t);
			th = (void *)ti + sizeof(struct ip);
		}
		xchg(th->th_dport, th->th_sport, u_int16_t);
#undef xchg
	}
#ifdef INET6
	if (is_ipv6) {
		tlen += sizeof(struct tcphdr) + sizeof(struct ip6_hdr); 
		th = (struct tcphdr *)((caddr_t)ti + sizeof(struct ip6_hdr));
	} else
#endif /* INET6 */
	{
		ti->ti_len = htons((u_int16_t)(sizeof (struct tcphdr) + tlen));
		tlen += sizeof (struct tcpiphdr);
		th = (struct tcphdr *)((caddr_t)ti + sizeof(struct ip));
	}

	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = (struct ifnet *) 0;
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_x2 = 0;
	th->th_off = sizeof (struct tcphdr) >> 2;
	th->th_flags = flags;
	if (tp)
		win >>= tp->rcv_scale;
	if (win > TCP_MAXWIN)
		win = TCP_MAXWIN;
	th->th_win = htons((u_int16_t)win);
	th->th_urp = 0;

#ifdef INET6
	if (is_ipv6) {
		((struct ip6_hdr *)ti)->ip6_flow   = htonl(0x60000000);
		((struct ip6_hdr *)ti)->ip6_nxt  = IPPROTO_TCP;
		((struct ip6_hdr *)ti)->ip6_hlim =
			in6_selecthlim(tp ? tp->t_inpcb : NULL, NULL);	/*XXX*/
		((struct ip6_hdr *)ti)->ip6_plen = tlen - sizeof(struct ip6_hdr);
		th->th_sum = 0;
		th->th_sum = in6_cksum(m, IPPROTO_TCP,
		   sizeof(struct ip6_hdr), ((struct ip6_hdr *)ti)->ip6_plen);
		HTONS(((struct ip6_hdr *)ti)->ip6_plen);
		ip6_output(m, tp ? tp->t_inpcb->inp_outputopts6 : NULL,
			(struct route_in6 *)ro, 0, NULL, NULL);
	} else
#endif /* INET6 */
	{
		bzero(ti->ti_x1, sizeof ti->ti_x1);
		ti->ti_len = htons((u_short)tlen - sizeof(struct ip));
		th->th_sum = in_cksum(m, tlen);
		((struct ip *)ti)->ip_len = tlen;
		((struct ip *)ti)->ip_ttl = ip_defttl;
		ip_output(m, NULL, ro, 0, NULL, tp ? tp->t_inpcb : NULL);
	}
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 */
struct tcpcb *
tcp_newtcpcb(inp)
	struct inpcb *inp;
{
	register struct tcpcb *tp;

	tp = malloc(sizeof(*tp), M_PCB, M_NOWAIT);
	if (tp == NULL)
		return ((struct tcpcb *)0);
	bzero((char *) tp, sizeof(struct tcpcb));
	LIST_INIT(&tp->segq);
	tp->t_maxseg = tp->t_maxopd = tcp_mssdflt;

#ifdef TCP_SACK
	tp->sack_disable = tcp_do_sack ? 0 : 1;
#endif
	tp->t_flags = tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;
	tp->t_inpcb = inp;
	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = tcp_rttdflt * PR_SLOWHZ << (TCP_RTTVAR_SHIFT + 2 - 1);
	tp->t_rttmin = TCPTV_MIN;
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
	    TCPTV_MIN, TCPTV_REXMTMAX);
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
#ifdef INET6
	/*
	 * If we want to use tp->pf for a quick-n-easy way to determine
	 * the outbound dgram type, we cannot make this decision
	 * until a connection is established!  Bzero() sets pf to zero, and
	 * that's the way we want it, unless, of course, it's an AF_INET
	 * socket...
	 */
	if ((inp->inp_flags & INP_IPV6) == 0)
		tp->pf = PF_INET;  /* If AF_INET socket, we can't do v6 from it. */
#else
	tp->pf = PF_INET;
#endif

#ifdef INET6
	if (inp->inp_flags & INP_IPV6) 
		inp->inp_ipv6.ip6_hlim = ip6_defhlim;
	else
#endif /* INET6 */
		inp->inp_ip.ip_ttl = ip_defttl;

	inp->inp_ppcb = (caddr_t)tp;
	return (tp);
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop(tp, errno)
	register struct tcpcb *tp;
	int errno;
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		(void) tcp_output(tp);
		tcpstat.tcps_drops++;
	} else
		tcpstat.tcps_conndrops++;
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */
struct tcpcb *
tcp_close(tp)
	register struct tcpcb *tp;
{
	register struct ipqent *qe;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
#ifdef TCP_SACK
	struct sackhole *p, *q;
#endif
#ifdef RTV_RTT
	register struct rtentry *rt;
#ifdef INET6
	register int bound_to_specific = 0;  /* I.e. non-default */

	/*
	 * This code checks the nature of the route for this connection.
	 * Normally this is done by two simple checks in the next
	 * INET/INET6 ifdef block, but because of two possible lower layers,
	 * that check is done here.
	 *
	 * Perhaps should be doing this only for a RTF_HOST route.
	 */
	rt = inp->inp_route.ro_rt;  /* Same for route or route6. */
	if (tp->pf == PF_INET6) {
		if (rt)
			bound_to_specific =
			    !(IN6_IS_ADDR_UNSPECIFIED(&
			    ((struct sockaddr_in6 *)rt_key(rt))->sin6_addr));
	} else {
		if (rt)
			bound_to_specific =
			    (((struct sockaddr_in *)rt_key(rt))->
			    sin_addr.s_addr != INADDR_ANY);
	}
#endif /* INET6 */

	/*
	 * If we sent enough data to get some meaningful characteristics,
	 * save them in the routing entry.  'Enough' is arbitrarily 
	 * defined as the sendpipesize (default 4K) * 16.  This would
	 * give us 16 rtt samples assuming we only get one sample per
	 * window (the usual case on a long haul net).  16 samples is
	 * enough for the srtt filter to converge to within 5% of the correct
	 * value; fewer samples and we could save a very bogus rtt.
	 *
	 * Don't update the default route's characteristics and don't
	 * update anything that the user "locked".
	 */
#ifdef INET6
	/*
	 * Note that rt and bound_to_specific are set above.
	 */
	if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
	    rt && bound_to_specific) {
#else /* INET6 */
	if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
	    (rt = inp->inp_route.ro_rt) &&
	    satosin(rt_key(rt))->sin_addr.s_addr != INADDR_ANY) {
#endif /* INET6 */
		register u_long i = 0;

		if ((rt->rt_rmx.rmx_locks & RTV_RTT) == 0) {
			i = tp->t_srtt *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));
			if (rt->rt_rmx.rmx_rtt && i)
				/*
				 * filter this update to half the old & half
				 * the new values, converting scale.
				 * See route.h and tcp_var.h for a
				 * description of the scaling constants.
				 */
				rt->rt_rmx.rmx_rtt =
				    (rt->rt_rmx.rmx_rtt + i) / 2;
			else
				rt->rt_rmx.rmx_rtt = i;
		}
		if ((rt->rt_rmx.rmx_locks & RTV_RTTVAR) == 0) {
			i = tp->t_rttvar *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));
			if (rt->rt_rmx.rmx_rttvar && i)
				rt->rt_rmx.rmx_rttvar =
				    (rt->rt_rmx.rmx_rttvar + i) / 2;
			else
				rt->rt_rmx.rmx_rttvar = i;
		}
		/*
		 * update the pipelimit (ssthresh) if it has been updated
		 * already or if a pipesize was specified & the threshhold
		 * got below half the pipesize.  I.e., wait for bad news
		 * before we start updating, then update on both good
		 * and bad news.
		 */
		if (((rt->rt_rmx.rmx_locks & RTV_SSTHRESH) == 0 &&
		    (i = tp->snd_ssthresh) && rt->rt_rmx.rmx_ssthresh) ||
		    i < (rt->rt_rmx.rmx_sendpipe / 2)) {
			/*
			 * convert the limit from user data bytes to
			 * packets then to packet data bytes.
			 */
			i = (i + tp->t_maxseg / 2) / tp->t_maxseg;
			if (i < 2)
				i = 2;
#ifdef INET6
			if (tp->pf == PF_INET6)
				i *= (u_long)(tp->t_maxseg + sizeof (struct tcphdr)
				    + sizeof(struct ip6_hdr));
			else
#endif /* INET6 */
				i *= (u_long)(tp->t_maxseg +
				    sizeof (struct tcpiphdr));

			if (rt->rt_rmx.rmx_ssthresh)
				rt->rt_rmx.rmx_ssthresh =
				    (rt->rt_rmx.rmx_ssthresh + i) / 2;
			else
				rt->rt_rmx.rmx_ssthresh = i;
		}
	}
#endif /* RTV_RTT */

	/* free the reassembly queue, if any */
#ifdef INET6
	/* Reassembling TCP segments in v6 might be sufficiently different
	 * to merit two codepaths to free the reasssembly queue.
	 * If an undecided TCP socket, then the IPv4 codepath will be used 
	 * because it won't matter much anyway.
	 */
	if (tp->pf == AF_INET6) {
		while ((qe = tp->segq.lh_first) != NULL) {
			LIST_REMOVE(qe, ipqe_q);
			m_freem(qe->ipqe_m);
			FREE(qe, M_IPQ);
		}
	} else
#endif /* INET6 */
		while ((qe = tp->segq.lh_first) != NULL) {
			LIST_REMOVE(qe, ipqe_q);
			m_freem(qe->ipqe_m);
			FREE(qe, M_IPQ);
		}
#ifdef TCP_SACK
	/* Free SACK holes. */
	q = p = tp->snd_holes;
	while (p != 0) {
		q = p->next;
		free(p, M_PCB);
		p = q;
	}
#endif
	if (tp->t_template)
		(void) m_free(tp->t_template);
	free(tp, M_PCB);
	inp->inp_ppcb = 0;
	soisdisconnected(so);
	in_pcbdetach(inp);
	tcpstat.tcps_closed++;
	return ((struct tcpcb *)0);
}

void
tcp_drain()
{

}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 */
void
tcp_notify(inp, error)
	struct inpcb *inp;
	int error;
{
	register struct tcpcb *tp = (struct tcpcb *)inp->inp_ppcb;
	register struct socket *so = inp->inp_socket;

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	     (error == EHOSTUNREACH || error == ENETUNREACH ||
	      error == EHOSTDOWN)) {
		return;
	} else if (TCPS_HAVEESTABLISHED(tp->t_state) == 0 &&
	    tp->t_rxtshift > 3 && tp->t_softerror)
		so->so_error = error;
	else 
		tp->t_softerror = error;
	wakeup((caddr_t) &so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
}

#if defined(INET6) && !defined(TCP6)
void
tcp6_ctlinput(cmd, sa, d)
	int cmd;
	struct sockaddr *sa;
	void *d;
{
	(void)tcp_ctlinput(cmd, sa, NULL);	/*XXX*/
}
#endif

void *
tcp_ctlinput(cmd, sa, v)
	int cmd;
	struct sockaddr *sa;
	register void *v;
{
	register struct ip *ip = v;
	register struct tcphdr *th;
	extern int inetctlerrmap[];
	void (*notify) __P((struct inpcb *, int)) = tcp_notify;
	int errno;

	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	errno = inetctlerrmap[cmd];
	if (cmd == PRC_QUENCH)
		notify = tcp_quench;
	else if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, ip = 0;
	else if (cmd == PRC_HOSTDEAD)
		ip = 0;
	else if (errno == 0)
		return NULL;

#ifdef INET6
	if (sa->sa_family == AF_INET6) {
		if (ip) {
			struct ip6_hdr *ipv6 = (struct ip6_hdr *)ip;

			th = (struct tcphdr *)(ipv6 + 1);
#if 0 /*XXX*/
			in6_pcbnotify(&tcbtable, sa, th->th_dport,
			    &ipv6->ip6_src, th->th_sport, cmd, notify);
#endif
		} else {
#if 0 /*XXX*/
			in6_pcbnotify(&tcbtable, sa, 0,
			    (struct in6_addr *)&in6addr_any, 0, cmd, notify);
#endif
		}
	} else
#endif /* INET6 */
	{
		if (ip) {
			th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
			in_pcbnotify(&tcbtable, sa, th->th_dport, ip->ip_src,
			    th->th_sport, errno, notify);
		} else
			in_pcbnotifyall(&tcbtable, sa, errno, notify);
	}
	return NULL;
}

/*
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 */
void
tcp_quench(inp, errno)
	struct inpcb *inp;
	int errno;
{
	struct tcpcb *tp = intotcpcb(inp);

	if (tp)
		tp->snd_cwnd = tp->t_maxseg;
}

#ifdef TCP_SIGNATURE
int
tcp_signature_tdb_attach()
{
	return (0);
}

int
tcp_signature_tdb_init(tdbp, xsp, ii)
	struct tdb *tdbp;
	struct xformsw *xsp;
	struct ipsecinit *ii;
{
	char *c;
#define isdigit(c)	  (((c) >= '0') && ((c) <= '9'))
#define isalpha(c)	( (((c) >= 'A') && ((c) <= 'Z')) || \
			  (((c) >= 'a') && ((c) <= 'z')) )

	if ((ii->ii_authkeylen < 1) || (ii->ii_authkeylen > 80))
		return (EINVAL);

	c = (char *)ii->ii_authkey;

	while (c < (char *)ii->ii_authkey + ii->ii_authkeylen - 1) {
		if (isdigit(*c)) {
			if (*(c + 1) == ' ')
				return (EINVAL);
		} else {
			if (!isalpha(*c))
				return (EINVAL);
		}

		c++;
	}

	if (!isdigit(*c) && !isalpha(*c))
		return (EINVAL);

	tdbp->tdb_amxkey = malloc(ii->ii_authkeylen, M_XDATA, M_DONTWAIT);
	if (tdbp->tdb_amxkey == NULL)
		return (ENOMEM);
	bcopy(ii->ii_authkey, tdbp->tdb_amxkey, ii->ii_authkeylen);
	tdbp->tdb_amxkeylen = ii->ii_authkeylen;

	return (0);
}

int
tcp_signature_tdb_zeroize(tdbp)
	struct tdb *tdbp;
{
	if (tdbp->tdb_amxkey) {
		bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
		free(tdbp->tdb_amxkey, M_XDATA);
		tdbp->tdb_amxkey = NULL;
	}

	return (0);
}

int
tcp_signature_tdb_input(m, tdbp, skip, protoff)
	struct mbuf *m;
	struct tdb *tdbp;
	int skip, protoff;
{
	return (0);
}

int
tcp_signature_tdb_output(m, tdbp, mp, skip, protoff)
	struct mbuf *m;
	struct tdb *tdbp;
	struct mbuf **mp;
	int skip, protoff;
{
	return (EINVAL);
}

int
tcp_signature_apply(fstate, data, len)
	caddr_t fstate;
	caddr_t data;
	unsigned int len;
{
	MD5Update((MD5_CTX *)fstate, (char *)data, len);
	return 0;
}
#endif /* TCP_SIGNATURE */
