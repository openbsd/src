/*	$OpenBSD: raw_ipv6.c,v 1.27 2000/07/27 06:29:10 itojun Exp $	*/

/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

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
 *	@(#)raw_ip.c	8.7 (Berkeley) 5/15/95
 *	$Id: raw_ipv6.c,v 1.27 2000/07/27 06:29:10 itojun Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/ip6protosw.h>

#undef IPSEC

/*
 * Globals
 */

struct inpcbtable rawin6pcbtable;
struct sockaddr_in6 rip6src = { sizeof(struct sockaddr_in6), AF_INET6 };

/*
 * Nominal space allocated to a raw ip socket.
 */

#define	RIPV6SNDQ		8192
#define	RIPV6RCVQ		8192

#if 0
u_long rip6_sendspace = RIPV6SNDQ;
u_long rip6_recvspace = RIPV6RCVQ;
#else
extern u_long rip6_sendspace;
extern u_long rip6_recvspace;
#endif

/*
 * External globals
 */

#if 0
extern struct ip6_hdrstat ipv6stat;
#endif

/*
 * Raw IPv6 PCB initialization.
 */
void
rip6_init()
{
	in_pcbinit(&rawin6pcbtable, 1);
}

/*
 * At the point where this function gets called, we don't know the nexthdr of
 * the current header to be processed, only its offset. So we have to go find
 * it the hard way. In the case where there's no chained headers, this is not
 * really painful.
 *
 * The good news is that all fields have been sanity checked.
 *
 * Assumes m has already been pulled up by extra. -cmetz
 */
#if __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__
static __inline__ int
#else /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ */
static int
#endif /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ */
ipv6_findnexthdr(struct mbuf *m, size_t extra)
{
	caddr_t p = mtod(m, caddr_t);
	int nexthdr = IPPROTO_IPV6;
	unsigned int hl;

	do {
		switch (nexthdr) {
		case IPPROTO_IPV6:
			hl = sizeof(struct ip6_hdr);

			if ((extra -= hl) < 0)
				return -1;

			nexthdr = ((struct ip6_hdr *)p)->ip6_nxt;
			break;
		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
			if (extra < sizeof(struct ip6_ext))
				return -1;

			hl = sizeof(struct ip6_ext) +
			    (((struct ip6_ext *)p)->ip6e_len << 3);

			if ((extra -= hl) < 0)
				return -1;

			nexthdr = ((struct ip6_ext *)p)->ip6e_nxt;
			break;
		case IPPROTO_ROUTING:
			if (extra < sizeof(struct ip6_rthdr0))
				return -1;

			hl = sizeof(struct ip6_rthdr0) +
			    (((struct ip6_rthdr0 *)p)->ip6r0_len << 3);

			if ((extra -= hl) < 0)
				return -1;

			nexthdr = ((struct ip6_rthdr0 *)p)->ip6r0_nxt;
			break;
#ifdef IPSEC
		case IPPROTO_AH:
			if (extra < sizeof(struct ip6_hdr_srcroute0))
				return -1;

			hl = sizeof(struct ip6_hdr_srcroute0) +
			((struct ip6_hdr_srcroute0 *)p)->i6sr_len << 3;

			if ((extra -= hl) < 0)
				return -1;

			nexthdr = ((struct ip6_hdr_srcroute0 *)p)->i6sr_nexthdr;
			break;
#endif /* IPSEC */
		default:
			return -1;
		}
		p += hl;
	} while (extra > 0);

	return nexthdr;
}

/*
 * If no HLP's are found for an IPv6 datagram, this routine is called.
 */
int
rip6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;
	/* Will have been pulled up by ipv6_input(). */
	register struct ip6_hdr *ip6;
	register struct inpcb *inp;
	int nexthdr, icmp6type;
	int foundone = 0;
	struct mbuf *m2 = NULL, *opts = NULL;
	struct sockaddr_in6 srcsa;
	int extra = *offp;

#ifdef DIAGNOSTIC
	if (m->m_len < sizeof(*ip6))
		panic("too short mbuf to rip6_input");
#endif
	ip6 = mtod(m, struct ip6_hdr *);

	/* Be proactive about malicious use of IPv4 mapped address */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		/* XXX stat */
		goto ret;
	}

	bzero(&opts, sizeof(opts));
	bzero(&srcsa, sizeof(struct sockaddr_in6));
	srcsa.sin6_family = AF_INET6;
	srcsa.sin6_len = sizeof(struct sockaddr_in6);
#if 0 /*XXX inbound flowinfo */
	srcsa.sin6_flowinfo = ip6->ip6_flow & IPV6_FLOWINFO_MASK;
#endif
	/* KAME hack: recover scopeid */
	(void)in6_recoverscope(&srcsa, &ip6->ip6_src, m->m_pkthdr.rcvif);

	if (m->m_len < extra) {
		if (!(m = m_pullup2(m, extra)))
			return IPPROTO_DONE;
		ip6 = mtod(m, struct ip6_hdr *);
	}

	if ((nexthdr = ipv6_findnexthdr(m, extra)) < 0)
		goto ret;

	if (nexthdr == IPPROTO_ICMPV6) {
		if (m->m_len < extra + sizeof(struct icmp6_hdr)) {
			m = m_pullup2(m, extra + sizeof(struct icmp6_hdr));
			if (!m)
				goto ret;

			ip6 = mtod(m, struct ip6_hdr *);
		}
		icmp6type = ((struct icmp6_hdr *)(mtod(m, caddr_t) + extra))->icmp6_type;
	} else
		icmp6type = -1;

	/*
	 * Locate raw PCB for incoming datagram.
	 */
	for (inp = rawin6pcbtable.inpt_queue.cqh_first;
	     inp != (struct inpcb *)&rawin6pcbtable.inpt_queue;
	     inp = inp->inp_queue.cqe_next) {
		if (!(inp->inp_flags & INP_IPV6))
			continue;
		if (inp->inp_ipv6.ip6_nxt && inp->inp_ipv6.ip6_nxt != nexthdr)
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) && 
		    !IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, &ip6->ip6_dst))
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6) && 
		    !IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6, &ip6->ip6_src))
			continue;
		/*
		 * inp_icmp6filt must not be NULL, but we add a check for
		 * safety
		 */
		if (icmp6type >= 0 && inp->inp_icmp6filt && 
		    ICMP6_FILTER_WILLBLOCK(icmp6type, inp->inp_icmp6filt))
			continue;

		foundone = 1;

		/*
		 * Note the inefficiency here; this is a consequence of the
		 * interfaces of the functions being used. The raw code is
		 * not performance critical enough to require an immediate fix.
		 * - cmetz
		 */
		if ((m2 = m_copym(m, 0, (int)M_COPYALL, M_DONTWAIT))) {
			m_adj(m2, extra);
			if (inp->inp_flags & IN6P_CONTROLOPTS)
				ip6_savecontrol(inp, &opts, ip6, m);
			else
				opts = NULL;
			if (sbappendaddr(&inp->inp_socket->so_rcv,
			    (struct sockaddr *)&srcsa, m2, opts)) {
				sorwakeup(inp->inp_socket);
			} else {
				m_freem(m2);
			}
		}
	}

	if (!foundone) {
		/*
		 * We should send an ICMPv6 protocol unreachable here,
		 * though original UCB 4.4-lite BSD's IPv4 does not do so.
		 */
#if 0
		ipv6stat.ips_noproto++;
		ipv6stat.ips_delivered--;
#endif
	}

ret:
	if (m)
		m_freem(m);

	return IPPROTO_DONE;
}

void
rip6_ctlinput(cmd, sa, d)
	int cmd;
	struct sockaddr *sa;
	void *d;
{
	struct sockaddr_in6 sa6;
	register struct ip6_hdr *ip6;
	struct mbuf *m;
	int off;
	void (*notify) __P((struct inpcb *, int)) = in_rtchange;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;

	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, d = NULL;
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (inet6ctlerrmap[cmd] == 0)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		struct ip6ctlparam *ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
	} else {
		m = NULL;
		ip6 = NULL;
	}

	/* translate addresses into internal form */
	sa6 = *(struct sockaddr_in6 *)sa;
	if (IN6_IS_ADDR_LINKLOCAL(&sa6.sin6_addr) && m && m->m_pkthdr.rcvif)
		sa6.sin6_addr.s6_addr16[1] = htons(m->m_pkthdr.rcvif->if_index);

	if (ip6) {
		/*
		 * XXX: We assume that when IPV6 is non NULL,
		 * M and OFF are valid.
		 */
		struct in6_addr s;

		/* translate addresses into internal form */
		memcpy(&s, &ip6->ip6_src, sizeof(s));
		if (IN6_IS_ADDR_LINKLOCAL(&s))
			s.s6_addr16[1] = htons(m->m_pkthdr.rcvif->if_index);

		(void) in6_pcbnotify(&rawin6pcbtable, (struct sockaddr *)&sa6,
					0, &s, 0, cmd, notify);
	} else {
		(void) in6_pcbnotify(&rawin6pcbtable, (struct sockaddr *)&sa6,
					0, &zeroin6_addr, 0, cmd, notify);
	}
}

/*
 * Output function for raw IPv6.  Called from rip6_usrreq(), and
 * ipv6_icmp_usrreq().
 */
int
rip6_output(struct mbuf *m, ...)
{
	register struct ip6_hdr *ip6;
	register struct inpcb *inp;
	int flags;
	int error = 0;
#if 0
	struct ifnet *forceif = NULL;
#endif
	struct ip6_pktopts opt, *optp = NULL, *origoptp;
	struct ifnet *oifp = NULL;
	va_list ap;
	struct socket *so;
	struct sockaddr_in6 *dst;
	struct mbuf *control;
	struct in6_addr *in6a;
	u_int8_t type, code;
	int plen = m->m_pkthdr.len;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	dst = va_arg(ap, struct sockaddr_in6 *);
	control = va_arg(ap, struct mbuf *);
	va_end(ap);

	inp = sotoinpcb(so);
	flags = (so->so_options & SO_DONTROUTE);

	if (control) {
		error = ip6_setpktoptions(control, &opt,
		    so->so_state & SS_PRIV);
		if (error != 0)
			goto bad;
		optp = &opt;
	} else
		optp = NULL;

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

	M_PREPEND(m, sizeof(struct ip6_hdr), M_WAIT);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = dst->sin6_flowinfo & IPV6_FLOWINFO_MASK;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	ip6->ip6_nxt = inp->inp_ipv6.ip6_nxt;
	/* ip6_src will be filled in later */

	/* KAME hack: embed scopeid */
	origoptp = inp->inp_outputopts6;
	inp->inp_outputopts6 = optp;
	if (in6_embedscope(&ip6->ip6_dst, dst, inp, &oifp) != 0) {
		error = EINVAL;
		goto bad;
	}
	inp->inp_outputopts6 = origoptp;

	/* source address selection */
	in6a = in6_selectsrc(dst, optp, inp->inp_moptions6, &inp->inp_route6,
	    &inp->inp_laddr6, &error);
	if (in6a == NULL) {
		if (error == 0)
			error = EADDRNOTAVAIL;
		goto bad;
	}
	ip6->ip6_src = *in6a;
	if (inp->inp_route6.ro_rt)	/* what if oifp contradicts ? */
		oifp = ifindex2ifnet[inp->inp_route6.ro_rt->rt_ifp->if_index];

	ip6->ip6_hlim = in6_selecthlim(inp, oifp);

	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6 ||
	    inp->inp_csumoffset != -1) {
		struct mbuf *n;
		int off;
		u_int16_t *p;

#define	offsetof(type, member)	((size_t)(&((type *)0)->member)) /* XXX */
		/* compute checksum */
		if (so->so_proto->pr_protocol == IPPROTO_ICMPV6)
			off = offsetof(struct icmp6_hdr, icmp6_cksum);
		else
			off = inp->inp_csumoffset;
		if (plen < off + 1) {
			error = EINVAL;
			goto bad;
		}
		off += sizeof(struct ip6_hdr);

		n = m;
		while (n && n->m_len <= off) {
			off -= n->m_len;
			n = n->m_next;
		}
		if (!n)
			goto bad;
		p = (u_int16_t *)(mtod(n, caddr_t) + off);
		*p = 0;
		*p = in6_cksum(m, ip6->ip6_nxt, sizeof(*ip6), plen);
	}

	error = ip6_output(m, optp, &inp->inp_route6, flags,
	    inp->inp_moptions6, &oifp);
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		if (oifp)
			icmp6_ifoutstat_inc(oifp, type, code);
		icmp6stat.icp6s_outhist[type]++;
	}

	goto freectl;

bad:
	if (m)
		m_freem(m);

freectl:
	if (control)
		m_freem(control);
	return error;
}

/*
 * Handles [gs]etsockopt() calls.
 */
int
rip6_ctloutput(op, so, level, optname, m)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **m;
{
	register struct inpcb *inp = sotoinpcb(so);
	int error;

	if ((level != IPPROTO_IPV6) && (level != IPPROTO_ICMPV6)) {
		if (op == PRCO_SETOPT && *m)
			(void)m_free(*m);
		return(EINVAL);
	}

	switch (optname) {
	case MRT6_INIT:
	case MRT6_DONE:
	case MRT6_ADD_MIF:
	case MRT6_DEL_MIF:
	case MRT6_ADD_MFC:
	case MRT6_DEL_MFC:
	case MRT6_PIM:
		if (level != IPPROTO_IPV6) {
			if (op == PRCO_SETOPT && *m)
				(void)m_free(*m);
			return EINVAL;
		}

		if (op == PRCO_SETOPT) {
			error = ip6_mrouter_set(optname, so, *m);
			if (*m)
				(void)m_free(*m);
		} else if (op == PRCO_GETOPT)
			error = ip6_mrouter_get(optname, so, m);
		else
			error = EINVAL;
		return (error);

	case IPV6_CHECKSUM:
		if (op == PRCO_SETOPT || op == PRCO_GETOPT) {
			if (op == PRCO_SETOPT) {
				if (!m || !*m || (*m)->m_len != sizeof(int))
					return(EINVAL);
				inp->inp_csumoffset = *(mtod(*m, int *));
				m_freem(*m);
			} else {
				*m = m_get(M_WAIT, MT_SOOPTS);
				(*m)->m_len = sizeof(int);
				*(mtod(*m, int *)) = inp->inp_csumoffset;
			}
			return 0;
		}
		break;

	case ICMP6_FILTER:
		if (level != IPPROTO_ICMPV6) {
			if (op == PRCO_SETOPT && *m)
				(void)m_free(*m);
			return EINVAL;
		}

		if (op == PRCO_SETOPT || op == PRCO_GETOPT) {
			if (op == PRCO_SETOPT) {
				if (!m || !*m ||
				    (*m)->m_len != sizeof(struct icmp6_filter))
					return(EINVAL);
				bcopy(mtod(*m, struct icmp6_filter *),
				    inp->inp_icmp6filt,
				    sizeof(struct icmp6_filter));
				m_freem(*m);
			} else {
				*m = m_get(M_WAIT, MT_SOOPTS);
				(*m)->m_len = sizeof(struct icmp6_filter);
				*mtod(*m, struct icmp6_filter *) =
				    *inp->inp_icmp6filt;
			}
			return 0;
		}
		break;

	default:
		break;
	}
	return ip6_ctloutput(op, so, level, optname, m);
}

#if 1
#define MAYBESTATIC static
#define MAYBEINLINE __inline__
#else /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ */
#define MAYBESTATIC
#define MAYBEINLINE
#endif /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ */

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_attach(struct socket *so, int proto)
{
	register struct inpcb *inp = sotoinpcb(so);
	register int error = 0;

	if (inp)
		panic("rip6_attach - Already got PCB");

	if ((so->so_state & SS_PRIV) == 0) {
		error = EACCES;
		return error;
	}
	if ((error = soreserve(so, rip6_sendspace, rip6_recvspace)) ||
	    (error = in_pcballoc(so, &rawin6pcbtable))) {
		return error;
	}

	inp = sotoinpcb(so);
	/*nam;  Nam contains protocol type, apparently. */
#ifdef	__alpha__
	inp->inp_ipv6.ip6_nxt = (u_long)proto; 
#else
	inp->inp_ipv6.ip6_nxt = (int)proto;
#endif
	if (inp->inp_ipv6.ip6_nxt == IPPROTO_ICMPV6)
		inp->inp_csumoffset = 2;
	inp->inp_icmp6filt = (struct icmp6_filter *)
	malloc(sizeof(struct icmp6_filter), M_PCB, M_NOWAIT);
	ICMP6_FILTER_SETPASSALL(inp->inp_icmp6filt);
	return error;
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_detach(struct socket *so)
{
	register struct inpcb *inp = sotoinpcb(so);

	if (inp == 0)
		panic("rip6_detach");
#ifdef MROUTING
	/* More MROUTING stuff. */
#endif
	if (inp->inp_icmp6filt) {
		free(inp->inp_icmp6filt, M_PCB);
		inp->inp_icmp6filt = NULL;
	}
	in_pcbdetach(inp);
	return 0;
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_abort(struct socket *so)
{
	soisdisconnected(so);
	return rip6_usrreq_detach(so);
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_disconnect(struct socket *so)
{
	if ((so->so_state & SS_ISCONNECTED) == 0)
		return ENOTCONN;
	return rip6_usrreq_abort(so);
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_bind(struct socket *so, struct sockaddr *nam)
{
	register struct inpcb *inp = sotoinpcb(so);
	register struct sockaddr_in6 *addr = (struct sockaddr_in6 *)nam;

	/* 'ifnet' is declared in one of the net/ header files. */
	if ((ifnet.tqh_first == 0) || (addr->sin6_family != AF_INET6))
		return EADDRNOTAVAIL;

	/*
	 * Currently, ifa_ifwithaddr tends to fail for a link-local
	 * address, since it implicitly expects that the link identifier
	 * for the address is embedded in the sin6_addr part.
	 * For now, we'd rather keep this "as is". We'll eventually fix
	 * this in a more natural way.
	 */
	if (!IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr) &&
	     ifa_ifwithaddr((struct sockaddr *)addr) == 0) {
		return EADDRNOTAVAIL;
	}

	inp->inp_laddr6 = addr->sin6_addr;
	return 0;
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_connect(struct socket *so, struct sockaddr *nam)
{
	register struct inpcb *inp = sotoinpcb(so);
	register struct sockaddr_in6 *addr = (struct sockaddr_in6 *) nam;
	int error;
	struct in6_addr *in6a;

	if (addr->sin6_family != AF_INET6)
		return EAFNOSUPPORT;

	in6a = in6_selectsrc(addr, inp->inp_outputopts6, inp->inp_moptions6,
	    &inp->inp_route6, &inp->inp_laddr6, &error);
	if (in6a == NULL) {
		if (error == 0)
			error = EADDRNOTAVAIL;
		return error;
	}
	inp->inp_laddr6 = *in6a;

	/* Will structure assignment work with this compiler? */
	inp->inp_faddr6 = addr->sin6_addr; 

	soisconnected(so);
	return 0;
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_shutdown(struct socket *so)
{
	socantsendmore(so);
	return 0;
}

static int rip6_usrreq_send __P((struct socket *so, int flags, struct mbuf *m,
	struct sockaddr *addr, struct mbuf *control));

static int
rip6_usrreq_send(struct socket *so, int flags, struct mbuf *m,
		 struct sockaddr *addr, struct mbuf *control)
{
	register struct inpcb *inp = sotoinpcb(so);
	register int error = 0;
	struct sockaddr_in6 *dst, tmp;

	if (inp == 0) {
		m_freem(m);
		return EINVAL;
	}

	/*
	 * Check "connected" status, and if there is a supplied destination
	 * address.
	 */
	if (so->so_state & SS_ISCONNECTED) {
		if (addr)
			return EISCONN;

		bzero(&tmp, sizeof(tmp));
		tmp.sin6_family = AF_INET6;
		tmp.sin6_len = sizeof(tmp);
		tmp.sin6_addr = inp->inp_faddr6;
		dst = &tmp;
	} else {
		if (addr == NULL)
			return ENOTCONN;

		dst = (struct sockaddr_in6 *)addr;
	}

	error = rip6_output(m,so,dst,control);
	/* m = NULL; */
	return error;
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_control(struct socket *so, u_long cmd, caddr_t data,
		    struct ifnet *ifp)
{
	/*
	 * Notice that IPv4 raw sockets don't pass PRU_CONTROL.  I wonder
	 * if they panic as well?
	 */
	return in6_control(so, cmd, data, ifp, 0);
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_sense(struct socket *so, struct stat *sb)
{
	/* services stat(2) call. */
	return 0;
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_sockaddr(struct socket *so, struct mbuf *nam)
{
	register struct inpcb *inp = sotoinpcb(so);
	return in6_setsockaddr(inp, nam);
}

MAYBESTATIC MAYBEINLINE int
rip6_usrreq_peeraddr(struct socket *so, struct mbuf *nam)
{
	register struct inpcb *inp = sotoinpcb(so);
	return in6_setpeeraddr(inp, nam);
}

/*
 * Handles PRU_* for raw IPv6 sockets.
 */
int
rip6_usrreq(so, req, m, nam, control, p)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{
	register int error = 0;

#ifdef MROUTING
	/*
	 * Ummm, like, multicast routing stuff goes here, huh huh huh.
	 *
	 * Seriously, this would be for user-level multicast routing daemons.
	 * With multicast being a requirement for IPv6, code like what might go
	 * here may go away.
	 */
#endif

	switch (req) {
	case PRU_ATTACH:
		error = rip6_usrreq_attach(so, (long)nam);
		break;
	case PRU_DISCONNECT:
		error = rip6_usrreq_disconnect(so);
		break;
	case PRU_ABORT:
		error = rip6_usrreq_abort(so);
		break;
	case PRU_DETACH:
		error = rip6_usrreq_detach(so);
		break;
	case PRU_BIND:
		if (nam->m_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		/*
		 * Be strict regarding sockaddr_in6 fields.
		 */
		error = rip6_usrreq_bind(so, mtod(nam, struct sockaddr *));
		break;
	case PRU_CONNECT:
		/*
		 * Be strict regarding sockaddr_in6 fields.
		 */
		if (nam->m_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		error = rip6_usrreq_connect(so, mtod(nam, struct sockaddr *));
		break;
	case PRU_SHUTDOWN:
		error = rip6_usrreq_shutdown(so);
		break;
	case PRU_SEND:
		/*
		 * Be strict regarding sockaddr_in6 fields.
		 */
		if (nam->m_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		error = rip6_usrreq_send(so, 0, m, mtod(nam, struct sockaddr *),
		    control);
		m = NULL;
		break;
	case PRU_CONTROL:
		return rip6_usrreq_control(so, (u_long)m, (caddr_t) nam,
		    (struct ifnet *) control);
	case PRU_SENSE:
		return rip6_usrreq_sense(so, NULL); /* XXX */
	case PRU_CONNECT2:
	case PRU_RCVOOB:
	case PRU_LISTEN:
	case PRU_SENDOOB:
	case PRU_RCVD:
	case PRU_ACCEPT:
		error = EOPNOTSUPP;
		break;
	case PRU_SOCKADDR:
		error = rip6_usrreq_sockaddr(so, nam);
		break;
	case PRU_PEERADDR:
		error = rip6_usrreq_peeraddr(so, nam);
		break;
	default:
		panic("rip6_usrreq - unknown req\n");
	}
	if (m != NULL)
		m_freem(m);
	return error;
}
