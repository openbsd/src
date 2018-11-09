/*	$OpenBSD: raw_ip6.c,v 1.133 2018/11/09 13:26:12 claudio Exp $	*/
/*	$KAME: raw_ip6.c,v 1.69 2001/03/04 15:55:44 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.2 (Berkeley) 1/4/94
 */

#include "pf.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#ifdef MROUTING
#include <netinet6/ip6_mroute.h>
#endif
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6protosw.h>
#include <netinet6/raw_ip6.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#include <sys/stdarg.h>

/*
 * Raw interface to IP6 protocol.
 */

struct	inpcbtable rawin6pcbtable;

struct cpumem *rip6counters;

/*
 * Initialize raw connection block queue.
 */
void
rip6_init(void)
{
	in_pcbinit(&rawin6pcbtable, 1);
	rip6counters = counters_alloc(rip6s_ncounters);
}

int
rip6_input(struct mbuf **mp, int *offp, int proto, int af)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct inpcb *in6p;
	struct inpcb *last = NULL;
	struct in6_addr *key;
	struct sockaddr_in6 rip6src;
	struct mbuf *opts = NULL;

	KASSERT(af == AF_INET6);

	if (proto != IPPROTO_ICMPV6)
		rip6stat_inc(rip6s_ipackets);

	bzero(&rip6src, sizeof(rip6src));
	rip6src.sin6_len = sizeof(struct sockaddr_in6);
	rip6src.sin6_family = AF_INET6;
	/* KAME hack: recover scopeid */
	in6_recoverscope(&rip6src, &ip6->ip6_src);

	key = &ip6->ip6_dst;
#if NPF > 0
	if (m->m_pkthdr.pf.flags & PF_TAG_DIVERTED) {
		struct pf_divert *divert;

		divert = pf_find_divert(m);
		KASSERT(divert != NULL);
		switch (divert->type) {
		case PF_DIVERT_TO:
			key = &divert->addr.v6;
			break;
		case PF_DIVERT_REPLY:
			break;
		default:
			panic("%s: unknown divert type %d, mbuf %p, divert %p",
			    __func__, divert->type, m, divert);
		}
	}
#endif
	NET_ASSERT_LOCKED();
	TAILQ_FOREACH(in6p, &rawin6pcbtable.inpt_queue, inp_queue) {
		if (in6p->inp_socket->so_state & SS_CANTRCVMORE)
			continue;
		if (!(in6p->inp_flags & INP_IPV6))
			continue;
		if ((in6p->inp_ipv6.ip6_nxt || proto == IPPROTO_ICMPV6) &&
		    in6p->inp_ipv6.ip6_nxt != proto)
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->inp_laddr6) &&
		    !IN6_ARE_ADDR_EQUAL(&in6p->inp_laddr6, key))
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->inp_faddr6) &&
		    !IN6_ARE_ADDR_EQUAL(&in6p->inp_faddr6, &ip6->ip6_src))
			continue;
		if (proto == IPPROTO_ICMPV6 && in6p->inp_icmp6filt) {
			struct icmp6_hdr *icmp6;

			IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, *offp,
			    sizeof(*icmp6));
			if (icmp6 == NULL)
				return IPPROTO_DONE;
			if (ICMP6_FILTER_WILLBLOCK(icmp6->icmp6_type,
			    in6p->inp_icmp6filt))
				continue;
		}
		if (proto != IPPROTO_ICMPV6 && in6p->inp_cksum6 != -1) {
			rip6stat_inc(rip6s_isum);
			if (in6_cksum(m, proto, *offp,
			    m->m_pkthdr.len - *offp)) {
				rip6stat_inc(rip6s_badsum);
				continue;
			}
		}
		if (last) {
			struct	mbuf *n;
			if ((n = m_copym(m, 0, M_COPYALL, M_NOWAIT)) != NULL) {
				if (last->inp_flags & IN6P_CONTROLOPTS)
					ip6_savecontrol(last, n, &opts);
				/* strip intermediate headers */
				m_adj(n, *offp);
				if (sbappendaddr(last->inp_socket,
				    &last->inp_socket->so_rcv,
				    sin6tosa(&rip6src), n, opts) == 0) {
					/* should notify about lost packet */
					m_freem(n);
					m_freem(opts);
					rip6stat_inc(rip6s_fullsock);
				} else
					sorwakeup(last->inp_socket);
				opts = NULL;
			}
		}
		last = in6p;
	}
	if (last) {
		if (last->inp_flags & IN6P_CONTROLOPTS)
			ip6_savecontrol(last, m, &opts);
		/* strip intermediate headers */
		m_adj(m, *offp);
		if (sbappendaddr(last->inp_socket, &last->inp_socket->so_rcv,
		    sin6tosa(&rip6src), m, opts) == 0) {
			m_freem(m);
			m_freem(opts);
			rip6stat_inc(rip6s_fullsock);
		} else
			sorwakeup(last->inp_socket);
	} else {
		struct counters_ref ref;
		uint64_t *counters;

		if (proto != IPPROTO_ICMPV6) {
			rip6stat_inc(rip6s_nosock);
			if (m->m_flags & M_MCAST)
				rip6stat_inc(rip6s_nosockmcast);
		}
		if (proto == IPPROTO_NONE || proto == IPPROTO_ICMPV6) {
			m_freem(m);
		} else {
			int prvnxt = ip6_get_prevhdr(m, *offp);

			icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_NEXTHEADER, prvnxt);
		}
		counters = counters_enter(&ref, ip6counters);
		counters[ip6s_delivered]--;
		counters_leave(&ref, ip6counters);
	}
	return IPPROTO_DONE;
}

void
rip6_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *d)
{
	struct ip6_hdr *ip6;
	struct ip6ctlparam *ip6cp = NULL;
	struct sockaddr_in6 *sa6 = satosin6(sa);
	const struct sockaddr_in6 *sa6_src = NULL;
	void *cmdarg;
	void (*notify)(struct inpcb *, int) = in_rtchange;
	int nxt;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;

	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, d = NULL;
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (cmd == PRC_MSGSIZE)
		; /* special code is present, see below */
	else if (inet6ctlerrmap[cmd] == 0)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		ip6 = ip6cp->ip6c_ip6;
		cmdarg = ip6cp->ip6c_cmdarg;
		sa6_src = ip6cp->ip6c_src;
		nxt = ip6cp->ip6c_nxt;
	} else {
		ip6 = NULL;
		cmdarg = NULL;
		sa6_src = &sa6_any;
		nxt = -1;
	}

	if (ip6 && cmd == PRC_MSGSIZE) {
		int valid = 0;
		struct inpcb *in6p;

		/*
		 * Check to see if we have a valid raw IPv6 socket
		 * corresponding to the address in the ICMPv6 message
		 * payload, and the protocol (ip6_nxt) meets the socket.
		 * XXX chase extension headers, or pass final nxt value
		 * from icmp6_notify_error()
		 */
		in6p = in6_pcbhashlookup(&rawin6pcbtable, &sa6->sin6_addr, 0,
		    &sa6_src->sin6_addr, 0, rdomain);

		if (in6p && in6p->inp_ipv6.ip6_nxt &&
		    in6p->inp_ipv6.ip6_nxt == nxt)
			valid++;

		/*
		 * Depending on the value of "valid" and routing table
		 * size (mtudisc_{hi,lo}wat), we will:
		 * - recalculate the new MTU and create the
		 *   corresponding routing entry, or
		 * - ignore the MTU change notification.
		 */
		icmp6_mtudisc_update((struct ip6ctlparam *)d, valid);

		/*
		 * regardless of if we called icmp6_mtudisc_update(),
		 * we need to call in6_pcbnotify(), to notify path
		 * MTU change to the userland (2292bis-02), because
		 * some unconnected sockets may share the same
		 * destination and want to know the path MTU.
		 */
	}

	(void) in6_pcbnotify(&rawin6pcbtable, sa6, 0,
	    sa6_src, 0, rdomain, cmd, cmdarg, notify);
}

/*
 * Generate IPv6 header and pass packet to ip6_output.
 * Tack on options user may have setup with control call.
 */
int
rip6_output(struct mbuf *m, struct socket *so, struct sockaddr *dstaddr,
    struct mbuf *control)
{
	struct in6_addr *dst;
	struct ip6_hdr *ip6;
	struct inpcb *in6p;
	u_int	plen = m->m_pkthdr.len;
	int error = 0;
	struct ip6_pktopts opt, *optp = NULL, *origoptp;
	int type;		/* for ICMPv6 output statistics only */
	int priv = 0;
	int flags;

	in6p = sotoinpcb(so);

	priv = 0;
	if ((so->so_state & SS_PRIV) != 0)
		priv = 1;
	if (control) {
		if ((error = ip6_setpktopts(control, &opt,
		    in6p->inp_outputopts6,
		    priv, so->so_proto->pr_protocol)) != 0)
			goto bad;
		optp = &opt;
	} else
		optp = in6p->inp_outputopts6;

	if (dstaddr->sa_family != AF_INET6) {
		error = EAFNOSUPPORT;
		goto bad;
	}
	dst = &satosin6(dstaddr)->sin6_addr;
	if (IN6_IS_ADDR_V4MAPPED(dst)) {
		error = EADDRNOTAVAIL;
		goto bad;
	}

	/*
	 * For an ICMPv6 packet, we should know its type and code
	 * to update statistics.
	 */
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		struct icmp6_hdr *icmp6;
		if (m->m_len < sizeof(struct icmp6_hdr) &&
		    (m = m_pullup(m, sizeof(struct icmp6_hdr))) == NULL) {
			error = ENOBUFS;
			goto bad;
		}
		icmp6 = mtod(m, struct icmp6_hdr *);
		type = icmp6->icmp6_type;
	}

	M_PREPEND(m, sizeof(*ip6), M_DONTWAIT);
	if (!m) {
		error = ENOBUFS;
		goto bad;
	}
	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Next header might not be ICMP6 but use its pseudo header anyway.
	 */
	ip6->ip6_dst = *dst;

	/* KAME hack: embed scopeid */
	origoptp = in6p->inp_outputopts6;
	in6p->inp_outputopts6 = optp;
	if (in6_embedscope(&ip6->ip6_dst, satosin6(dstaddr), in6p) != 0) {
		error = EINVAL;
		goto bad;
	}
	in6p->inp_outputopts6 = origoptp;

	/*
	 * Source address selection.
	 */
	{
		struct in6_addr *in6a;

		error = in6_pcbselsrc(&in6a, satosin6(dstaddr), in6p, optp);
		if (error)
			goto bad;

		ip6->ip6_src = *in6a;
	}

	ip6->ip6_flow = in6p->inp_flowinfo & IPV6_FLOWINFO_MASK;
	ip6->ip6_vfc  &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc  |= IPV6_VERSION;
#if 0				/* ip6_plen will be filled in ip6_output. */
	ip6->ip6_plen  = htons((u_short)plen);
#endif
	ip6->ip6_nxt   = in6p->inp_ipv6.ip6_nxt;
	ip6->ip6_hlim = in6_selecthlim(in6p);

	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6 ||
	    in6p->inp_cksum6 != -1) {
		struct mbuf *n;
		int off;
		u_int16_t *sump;
		int sumoff;

		/* compute checksum */
		if (so->so_proto->pr_protocol == IPPROTO_ICMPV6)
			off = offsetof(struct icmp6_hdr, icmp6_cksum);
		else
			off = in6p->inp_cksum6;
		if (plen < off + 1) {
			error = EINVAL;
			goto bad;
		}
		off += sizeof(struct ip6_hdr);

		n = m_pulldown(m, off, sizeof(*sump), &sumoff);
		if (n == NULL) {
			m = NULL;
			error = ENOBUFS;
			goto bad;
		}
		sump = (u_int16_t *)(mtod(n, caddr_t) + sumoff);
		*sump = 0;
		*sump = in6_cksum(m, ip6->ip6_nxt, sizeof(*ip6), plen);
	}

	flags = 0;
	if (in6p->inp_flags & IN6P_MINMTU)
		flags |= IPV6_MINMTU;

	/* force routing table */
	m->m_pkthdr.ph_rtableid = in6p->inp_rtableid;

#if NPF > 0
	if (in6p->inp_socket->so_state & SS_ISCONNECTED &&
	    so->so_proto->pr_protocol != IPPROTO_ICMPV6)
		pf_mbuf_link_inpcb(m, in6p);
#endif

	error = ip6_output(m, optp, &in6p->inp_route6, flags,
	    in6p->inp_moptions6, in6p);
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		icmp6stat_inc(icp6s_outhist + type);
	} else
		rip6stat_inc(rip6s_opackets);

	goto freectl;

 bad:
	m_freem(m);

 freectl:
	if (control) {
		ip6_clearpktopts(&opt, -1);
		m_freem(control);
	}
	return (error);
}

/*
 * Raw IPv6 socket option processing.
 */
int
rip6_ctloutput(int op, struct socket *so, int level, int optname,
    struct mbuf *m)
{
#ifdef MROUTING
	int error;
#endif

	switch (level) {
	case IPPROTO_IPV6:
		switch (optname) {
#ifdef MROUTING
		case MRT6_INIT:
		case MRT6_DONE:
		case MRT6_ADD_MIF:
		case MRT6_DEL_MIF:
		case MRT6_ADD_MFC:
		case MRT6_DEL_MFC:
			if (op == PRCO_SETOPT) {
				error = ip6_mrouter_set(optname, so, m);
			} else if (op == PRCO_GETOPT)
				error = ip6_mrouter_get(optname, so, m);
			else
				error = EINVAL;
			return (error);
#endif
		case IPV6_CHECKSUM:
			return (ip6_raw_ctloutput(op, so, level, optname, m));
		default:
			return (ip6_ctloutput(op, so, level, optname, m));
		}

	case IPPROTO_ICMPV6:
		/*
		 * XXX: is it better to call icmp6_ctloutput() directly
		 * from protosw?
		 */
		return (icmp6_ctloutput(op, so, level, optname, m));

	default:
		return EINVAL;
	}
}

extern	u_long rip6_sendspace;
extern	u_long rip6_recvspace;

int
rip6_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
	struct mbuf *control, struct proc *p)
{
	struct inpcb *in6p;
	int error = 0;

	if (req == PRU_CONTROL)
		return (in6_control(so, (u_long)m, (caddr_t)nam,
		    (struct ifnet *)control));

	soassertlocked(so);

	in6p = sotoinpcb(so);
	if (in6p == NULL) {
		error = EINVAL;
		goto release;
	}

	switch (req) {
	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		in6p->inp_faddr6 = in6addr_any;
		so->so_state &= ~SS_ISCONNECTED;	/* XXX */
		break;

	case PRU_ABORT:
		soisdisconnected(so);
		if (in6p == NULL)
			panic("rip6_detach");
#ifdef MROUTING
		if (so == ip6_mrouter[in6p->inp_rtableid])
			ip6_mrouter_done(so);
#endif
		free(in6p->inp_icmp6filt, M_PCB, sizeof(struct icmp6_filter));
		in6p->inp_icmp6filt = NULL;

		in_pcbdetach(in6p);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in6 *addr;

		if ((error = in6_nam2sin6(nam, &addr)))
			break;
		/*
		 * Make sure to not enter in_pcblookup_local(), local ports
		 * are non-sensical for raw sockets.
		 */
		addr->sin6_port = 0;

		if ((error = in6_pcbaddrisavail(in6p, addr, 0, p)))
			break;

		in6p->inp_laddr6 = addr->sin6_addr;
		break;
	    }

	case PRU_CONNECT:
	{
		struct sockaddr_in6 *addr;
		struct in6_addr *in6a = NULL;

		if ((error = in6_nam2sin6(nam, &addr)))
			break;
		/* Source address selection. XXX: need pcblookup? */
		error = in6_pcbselsrc(&in6a, addr, in6p, in6p->inp_outputopts6);
		if (error)
			break;
		in6p->inp_laddr6 = *in6a;
		in6p->inp_faddr6 = addr->sin6_addr;
		soisconnected(so);
		break;
	}

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Mark the connection as being incapable of futther input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;
	/*
	 * Ship a packet out. The appropriate raw output
	 * routine handles any messaging necessary.
	 */
	case PRU_SEND:
	{
		struct sockaddr_in6 dst;

		/* always copy sockaddr to avoid overwrites */
		memset(&dst, 0, sizeof(dst));
		dst.sin6_family = AF_INET6;
		dst.sin6_len = sizeof(dst);
		if (so->so_state & SS_ISCONNECTED) {
			if (nam) {
				error = EISCONN;
				break;
			}
			dst.sin6_addr = in6p->inp_faddr6;
		} else {
			struct sockaddr_in6 *addr6;

			if (nam == NULL) {
				error = ENOTCONN;
				break;
			}
			if ((error = in6_nam2sin6(nam, &addr6)))
				break;
			dst.sin6_addr = addr6->sin6_addr;
			dst.sin6_scope_id = addr6->sin6_scope_id;
		}
		error = rip6_output(m, so, sin6tosa(&dst), control);
		control = NULL;
		m = NULL;
		break;
	}

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize
		 */
		return (0);
	/*
	 * Not supported.
	 */
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_RCVD:
	case PRU_RCVOOB:
		return (EOPNOTSUPP);	/* do not free mbuf's */

	case PRU_SOCKADDR:
		in6_setsockaddr(in6p, nam);
		break;

	case PRU_PEERADDR:
		in6_setpeeraddr(in6p, nam);
		break;

	default:
		panic("rip6_usrreq");
	}
release:
	m_freem(control);
	m_freem(m);
	return (error);
}

int
rip6_attach(struct socket *so, int proto)
{
	struct inpcb *in6p;
	int error;

	if (so->so_pcb)
		panic("rip6_attach");
	if ((so->so_state & SS_PRIV) == 0)
		return (EACCES);
	if (proto < 0 || proto >= IPPROTO_MAX)
		return EPROTONOSUPPORT;

	if ((error = soreserve(so, rip6_sendspace, rip6_recvspace)))
		return error;
	NET_ASSERT_LOCKED();
	if ((error = in_pcballoc(so, &rawin6pcbtable)))
		return error;

	in6p = sotoinpcb(so);
	in6p->inp_ipv6.ip6_nxt = proto;
	in6p->inp_cksum6 = -1;

	in6p->inp_icmp6filt = malloc(sizeof(struct icmp6_filter),
	    M_PCB, M_NOWAIT);
	if (in6p->inp_icmp6filt == NULL) {
		in_pcbdetach(in6p);
		return ENOMEM;
	}
	ICMP6_FILTER_SETPASSALL(in6p->inp_icmp6filt);
	return 0;
}

int
rip6_detach(struct socket *so)
{
	struct inpcb *in6p = sotoinpcb(so);

	soassertlocked(so);

	if (in6p == NULL)
		panic("rip6_detach");
#ifdef MROUTING
	if (so == ip6_mrouter[in6p->inp_rtableid])
		ip6_mrouter_done(so);
#endif
	free(in6p->inp_icmp6filt, M_PCB, sizeof(struct icmp6_filter));
	in6p->inp_icmp6filt = NULL;

	in_pcbdetach(in6p);

	return (0);
}

int
rip6_sysctl_rip6stat(void *oldp, size_t *oldplen, void *newp)
{
	struct rip6stat rip6stat;

	CTASSERT(sizeof(rip6stat) == rip6s_ncounters * sizeof(uint64_t));
	counters_read(ip6counters, (uint64_t *)&rip6stat, rip6s_ncounters);

	return (sysctl_rdstruct(oldp, oldplen, newp,
	    &rip6stat, sizeof(rip6stat)));
}

int
rip6_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;

	switch (name[0]) {
	case RIPV6CTL_STATS:
		return (rip6_sysctl_rip6stat(oldp, oldlenp, newp));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
