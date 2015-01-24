/*	$OpenBSD: raw_ip6.c,v 1.72 2015/01/24 00:29:06 deraadt Exp $	*/
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
#include <net/if_types.h>

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

struct rip6stat rip6stat;

/*
 * Initialize raw connection block queue.
 */
void
rip6_init()
{

	in_pcbinit(&rawin6pcbtable, 1);
}

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
int
rip6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct inpcb *in6p;
	struct inpcb *last = NULL;
	struct sockaddr_in6 rip6src;
	struct mbuf *opts = NULL;

	rip6stat.rip6s_ipackets++;

	/* Be proactive about malicious use of IPv4 mapped address */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		/* XXX stat */
		m_freem(m);
		return IPPROTO_DONE;
	}

	bzero(&rip6src, sizeof(rip6src));
	rip6src.sin6_len = sizeof(struct sockaddr_in6);
	rip6src.sin6_family = AF_INET6;
	/* KAME hack: recover scopeid */
	(void)in6_recoverscope(&rip6src, &ip6->ip6_src, m->m_pkthdr.rcvif);

	TAILQ_FOREACH(in6p, &rawin6pcbtable.inpt_queue, inp_queue) {
		if (in6p->inp_socket->so_state & SS_CANTRCVMORE)
			continue;
		if (!(in6p->inp_flags & INP_IPV6))
			continue;
		if (in6p->inp_ipv6.ip6_nxt &&
		    in6p->inp_ipv6.ip6_nxt != proto)
			continue;
#if NPF > 0
		if (m->m_pkthdr.pf.flags & PF_TAG_DIVERTED) {
			struct pf_divert *divert;

			/* XXX rdomain support */
			if ((divert = pf_find_divert(m)) == NULL)
				continue;
			if (!IN6_ARE_ADDR_EQUAL(&in6p->inp_laddr6,
			    &divert->addr.v6))
				continue;
		} else
#endif
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->inp_laddr6) &&
		    !IN6_ARE_ADDR_EQUAL(&in6p->inp_laddr6, &ip6->ip6_dst))
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->inp_faddr6) &&
		    !IN6_ARE_ADDR_EQUAL(&in6p->inp_faddr6, &ip6->ip6_src))
			continue;
		if (in6p->inp_cksum6 != -1) {
			rip6stat.rip6s_isum++;
			if (in6_cksum(m, proto, *offp,
			    m->m_pkthdr.len - *offp)) {
				rip6stat.rip6s_badsum++;
				continue;
			}
		}
		if (last) {
			struct	mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (last->inp_flags & IN6P_CONTROLOPTS)
					ip6_savecontrol(last, n, &opts);
				/* strip intermediate headers */
				m_adj(n, *offp);
				if (sbappendaddr(&last->inp_socket->so_rcv,
				    sin6tosa(&rip6src), n, opts) == 0) {
					/* should notify about lost packet */
					m_freem(n);
					if (opts)
						m_freem(opts);
					rip6stat.rip6s_fullsock++;
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
		if (sbappendaddr(&last->inp_socket->so_rcv,
		    sin6tosa(&rip6src), m, opts) == 0) {
			m_freem(m);
			if (opts)
				m_freem(opts);
			rip6stat.rip6s_fullsock++;
		} else
			sorwakeup(last->inp_socket);
	} else {
		rip6stat.rip6s_nosock++;
		if (m->m_flags & M_MCAST)
			rip6stat.rip6s_nosockmcast++;
		if (proto == IPPROTO_NONE)
			m_freem(m);
		else {
			u_int8_t *prvnxtp = ip6_get_prevhdr(m, *offp); /* XXX */
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_protounknown);
			icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_NEXTHEADER,
			    prvnxtp - mtod(m, u_int8_t *));
		}
		ip6stat.ip6s_delivered--;
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
#if 0
		if (!in6p) {
			/*
			 * As the use of sendto(2) is fairly popular,
			 * we may want to allow non-connected pcb too.
			 * But it could be too weak against attacks...
			 * We should at least check if the local
			 * address (= s) is really ours.
			 */
			in6p = in_pcblookup(&rawin6pcbtable, &sa6->sin6_addr, 0,
			    (struct in6_addr *)&sa6_src->sin6_addr, 0,
			    INPLOOKUP_WILDCARD | INPLOOKUP_IPV6,
			    rdomain);
		}
#endif

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
rip6_output(struct mbuf *m, ...)
{
	struct socket *so;
	struct sockaddr_in6 *dstsock;
	struct mbuf *control;
	struct in6_addr *dst;
	struct ip6_hdr *ip6;
	struct inpcb *in6p;
	u_int	plen = m->m_pkthdr.len;
	int error = 0;
	struct ip6_pktopts opt, *optp = NULL, *origoptp;
	struct ifnet *oifp = NULL;
	int type, code;		/* for ICMPv6 output statistics only */
	int priv = 0;
	va_list ap;
	int flags;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	dstsock = va_arg(ap, struct sockaddr_in6 *);
	control = va_arg(ap, struct mbuf *);
	va_end(ap);

	in6p = sotoinpcb(so);

	priv = 0;
	if ((so->so_state & SS_PRIV) != 0)
		priv = 1;
	dst = &dstsock->sin6_addr;
	if (control) {
		if ((error = ip6_setpktopts(control, &opt,
		    in6p->inp_outputopts6,
		    priv, so->so_proto->pr_protocol)) != 0)
			goto bad;
		optp = &opt;
	} else
		optp = in6p->inp_outputopts6;

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
		code = icmp6->icmp6_code;
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
	if (in6_embedscope(&ip6->ip6_dst, dstsock, in6p, &oifp) != 0) {
		error = EINVAL;
		goto bad;
	}
	in6p->inp_outputopts6 = origoptp;

	/*
	 * Source address selection.
	 */
	{
		struct in6_addr *in6a;

		error = in6_selectsrc(&in6a, dstsock, optp,
		    in6p->inp_moptions6, &in6p->inp_route6, &in6p->inp_laddr6,
		    in6p->inp_rtableid);
		if (error)
			goto bad;

		ip6->ip6_src = *in6a;
		if (in6p->inp_route6.ro_rt &&
		    in6p->inp_route6.ro_rt->rt_flags & RTF_UP)
			oifp = in6p->inp_route6.ro_rt->rt_ifp;
	}

	ip6->ip6_flow = in6p->inp_flowinfo & IPV6_FLOWINFO_MASK;
	ip6->ip6_vfc  &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc  |= IPV6_VERSION;
#if 0				/* ip6_plen will be filled in ip6_output. */
	ip6->ip6_plen  = htons((u_short)plen);
#endif
	ip6->ip6_nxt   = in6p->inp_ipv6.ip6_nxt;
	ip6->ip6_hlim = in6_selecthlim(in6p, oifp);

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
	if (in6p->inp_socket->so_state & SS_ISCONNECTED)
		m->m_pkthdr.pf.inp = in6p;
#endif

	error = ip6_output(m, optp, &in6p->inp_route6, flags,
	    in6p->inp_moptions6, &oifp, in6p);
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		if (oifp)
			icmp6_ifoutstat_inc(oifp, type, code);
		icmp6stat.icp6s_outhist[type]++;
	} else
		rip6stat.rip6s_opackets++;

	goto freectl;

 bad:
	if (m)
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
	struct mbuf **mp)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int dir;

	switch (level) {
	case IPPROTO_IPV6:
		switch (optname) {

		case IP_DIVERTFL:
			switch (op) {
			case PRCO_SETOPT:
				if (*mp == 0 || (*mp)->m_len < sizeof (int)) {
					error = EINVAL;
					break;
				}
				dir = *mtod(*mp, int *);
				if (inp->inp_divertfl > 0)
					error = ENOTSUP;
				else if ((dir & IPPROTO_DIVERT_RESP) || 
					   (dir & IPPROTO_DIVERT_INIT))
					inp->inp_divertfl = dir;
				else 
					error = EINVAL;
				break;

			case PRCO_GETOPT:
				*mp = m_get(M_WAIT, M_SOOPTS);
				(*mp)->m_len = sizeof(int);
				*mtod(*mp, int *) = inp->inp_divertfl;
				break;

			default:
				error = EINVAL;
				break;
			}

			if (op == PRCO_SETOPT && *mp)
				(void)m_free(*mp);
			return (error);

#ifdef MROUTING
		case MRT6_INIT:
		case MRT6_DONE:
		case MRT6_ADD_MIF:
		case MRT6_DEL_MIF:
		case MRT6_ADD_MFC:
		case MRT6_DEL_MFC:
		case MRT6_PIM:
			if (op == PRCO_SETOPT) {
				error = ip6_mrouter_set(optname, so, *mp);
				if (*mp)
					(void)m_free(*mp);
			} else if (op == PRCO_GETOPT)
				error = ip6_mrouter_get(optname, so, mp);
			else
				error = EINVAL;
			return (error);
#endif
		case IPV6_CHECKSUM:
			return (ip6_raw_ctloutput(op, so, level, optname, mp));
		default:
			return (ip6_ctloutput(op, so, level, optname, mp));
		}

	case IPPROTO_ICMPV6:
		/*
		 * XXX: is it better to call icmp6_ctloutput() directly
		 * from protosw?
		 */
		return (icmp6_ctloutput(op, so, level, optname, mp));

	default:
		if (op == PRCO_SETOPT && *mp)
			m_free(*mp);
		return EINVAL;
	}
}

extern	u_long rip6_sendspace;
extern	u_long rip6_recvspace;

int
rip6_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam, 
	struct mbuf *control, struct proc *p)
{
	struct inpcb *in6p = sotoinpcb(so);
	int error = 0;
	int s;
	int priv;

	priv = 0;
	if ((so->so_state & SS_PRIV) != 0)
		priv++;

	if (req == PRU_CONTROL)
		return (in6_control(so, (u_long)m, (caddr_t)nam,
		    (struct ifnet *)control));

	switch (req) {
	case PRU_ATTACH:
		if (in6p)
			panic("rip6_attach");
		if (!priv) {
			error = EACCES;
			break;
		}
		if ((long)nam < 0 || (long)nam >= IPPROTO_MAX) {
			error = EPROTONOSUPPORT;
			break;
		}
		s = splsoftnet();
		if ((error = soreserve(so, rip6_sendspace, rip6_recvspace)) ||
		    (error = in_pcballoc(so, &rawin6pcbtable))) {
			splx(s);
			break;
		}
		splx(s);
		in6p = sotoinpcb(so);
		in6p->inp_ipv6.ip6_nxt = (long)nam;
		in6p->inp_cksum6 = -1;

		in6p->inp_icmp6filt = malloc(sizeof(struct icmp6_filter),
		    M_PCB, M_NOWAIT);
		if (in6p->inp_icmp6filt == NULL) {
			in_pcbdetach(in6p);
			error = ENOMEM;
			break;
		}
		ICMP6_FILTER_SETPASSALL(in6p->inp_icmp6filt);
		break;

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
		/* FALLTHROUGH */
	case PRU_DETACH:
		if (in6p == 0)
			panic("rip6_detach");
#ifdef MROUTING
		if (so == ip6_mrouter)
			ip6_mrouter_done();
#endif
		if (in6p->inp_icmp6filt) {
			free(in6p->inp_icmp6filt, M_PCB, 0);
			in6p->inp_icmp6filt = NULL;
		}
		in_pcbdetach(in6p);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in6 *addr = mtod(nam, struct sockaddr_in6 *);
		struct ifaddr *ifa = NULL;

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (addr->sin6_family != AF_INET6) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * we don't support mapped address here, it would confuse
		 * users so reject it
		 */
		if (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr)) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * Currently, ifa_ifwithaddr tends to fail for a link-local
		 * address, since it implicitly expects that the link ID
		 * for the address is embedded in the sin6_addr part.
		 * For now, we'd rather keep this "as is". We'll eventually fix
		 * this in a more natural way.
		 */
		if (!IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr) &&
		    !(so->so_options & SO_BINDANY) &&
		    (ifa = ifa_ifwithaddr(sin6tosa(addr),
		    in6p->inp_rtableid)) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (ifa && ifatoia6(ifa)->ia6_flags &
		    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|
		     IN6_IFF_DETACHED|IN6_IFF_DEPRECATED)) {
			error = EADDRNOTAVAIL;
			break;
		}
		in6p->inp_laddr6 = addr->sin6_addr;
		break;
	    }

	case PRU_CONNECT:
	{
		struct sockaddr_in6 *addr = mtod(nam, struct sockaddr_in6 *);
		struct in6_addr *in6a = NULL;

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (addr->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}

		/* Source address selection. XXX: need pcblookup? */
		error = in6_selectsrc(&in6a, addr, in6p->inp_outputopts6,
		    in6p->inp_moptions6, &in6p->inp_route6,
		    &in6p->inp_laddr6, in6p->inp_rtableid);
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
		struct sockaddr_in6 tmp;
		struct sockaddr_in6 *dst;

		/* always copy sockaddr to avoid overwrites */
		if (so->so_state & SS_ISCONNECTED) {
			if (nam) {
				error = EISCONN;
				break;
			}
			/* XXX */
			bzero(&tmp, sizeof(tmp));
			tmp.sin6_family = AF_INET6;
			tmp.sin6_len = sizeof(struct sockaddr_in6);
			bcopy(&in6p->inp_faddr6, &tmp.sin6_addr,
			    sizeof(struct in6_addr));
			dst = &tmp;
		} else {
			if (nam == NULL) {
				error = ENOTCONN;
				break;
			}
			if (nam->m_len != sizeof(tmp)) {
				error = EINVAL;
				break;
			}

			tmp = *mtod(nam, struct sockaddr_in6 *);
			dst = &tmp;

			if (dst->sin6_family != AF_INET6) {
				error = EAFNOSUPPORT;
				break;
			}
		}
		error = rip6_output(m, so, dst, control);
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
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		in6_setsockaddr(in6p, nam);
		break;

	case PRU_PEERADDR:
		in6_setpeeraddr(in6p, nam);
		break;

	default:
		panic("rip6_usrreq");
	}
	if (m != NULL)
		m_freem(m);
	return (error);
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
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &rip6stat, sizeof(rip6stat)));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
