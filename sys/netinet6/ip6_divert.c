/*      $OpenBSD: ip6_divert.c,v 1.58 2018/10/04 17:33:41 bluhm Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_var.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_divert.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/icmp6.h>

#include <net/pfvar.h>

struct	inpcbtable	divb6table;
struct	cpumem		*div6counters;

#ifndef DIVERT_SENDSPACE
#define DIVERT_SENDSPACE	(65536 + 100)
#endif
u_int   divert6_sendspace = DIVERT_SENDSPACE;
#ifndef DIVERT_RECVSPACE
#define DIVERT_RECVSPACE	(65536 + 100)
#endif
u_int   divert6_recvspace = DIVERT_RECVSPACE;

#ifndef DIVERTHASHSIZE
#define DIVERTHASHSIZE	128
#endif

int *divert6ctl_vars[DIVERT6CTL_MAXID] = DIVERT6CTL_VARS;

int divb6hashsize = DIVERTHASHSIZE;

int	divert6_output(struct inpcb *, struct mbuf *, struct mbuf *,
	    struct mbuf *);

void
divert6_init(void)
{
	in_pcbinit(&divb6table, divb6hashsize);
	div6counters = counters_alloc(div6s_ncounters);
}

int
divert6_output(struct inpcb *inp, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control)
{
	struct sockaddr_in6 *sin6;
	int error, min_hdrlen, nxt, off, dir;
	struct ip6_hdr *ip6;

	m_freem(control);

	if ((error = in6_nam2sin6(nam, &sin6)))
		goto fail;

	/* Do basic sanity checks. */
	if (m->m_pkthdr.len < sizeof(struct ip6_hdr))
		goto fail;
	if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
		/* m_pullup() has freed the mbuf, so just return. */
		div6stat_inc(div6s_errors);
		return (ENOBUFS);
	}
	ip6 = mtod(m, struct ip6_hdr *);
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION)
		goto fail;
	if (m->m_pkthdr.len < sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen))
		goto fail;

	/*
	 * Recalculate the protocol checksum since the userspace application
	 * may have modified the packet prior to reinjection.
	 */
	off = ip6_lasthdr(m, 0, IPPROTO_IPV6, &nxt);
	if (off < sizeof(struct ip6_hdr))
		goto fail;

	dir = (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) ? PF_OUT : PF_IN);

	switch (nxt) {
	case IPPROTO_TCP:
		min_hdrlen = sizeof(struct tcphdr);
		m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
		break;
	case IPPROTO_UDP:
		min_hdrlen = sizeof(struct udphdr);
		m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
		break;
	case IPPROTO_ICMPV6:
		min_hdrlen = sizeof(struct icmp6_hdr);
		m->m_pkthdr.csum_flags |= M_ICMP_CSUM_OUT;
		break;
	default:
		min_hdrlen = 0;
		break;
	}
	if (min_hdrlen && m->m_pkthdr.len < off + min_hdrlen)
		goto fail;

	m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED_PACKET;

	if (dir == PF_IN) {
		struct rtentry *rt;
		struct ifnet *ifp;

		rt = rtalloc(sin6tosa(sin6), 0, inp->inp_rtableid);
		if (!rtisvalid(rt) || !ISSET(rt->rt_flags, RTF_LOCAL)) {
			rtfree(rt);
			error = EADDRNOTAVAIL;
			goto fail;
		}
		m->m_pkthdr.ph_ifidx = rt->rt_ifidx;
		rtfree(rt);

		/*
		 * Recalculate the protocol checksum for the inbound packet
		 * since the userspace application may have modified the packet
		 * prior to reinjection.
		 */
		in6_proto_cksum_out(m, NULL);

		ifp = if_get(m->m_pkthdr.ph_ifidx);
		if (ifp == NULL) {
			error = ENETDOWN;
			goto fail;
		}
		ipv6_input(ifp, m);
		if_put(ifp);
	} else {
		m->m_pkthdr.ph_rtableid = inp->inp_rtableid;

		error = ip6_output(m, NULL, &inp->inp_route6,
		    IP_ALLOWBROADCAST | IP_RAWOUTPUT, NULL, NULL);
	}

	div6stat_inc(div6s_opackets);
	return (error);

fail:
	div6stat_inc(div6s_errors);
	m_freem(m);
	return (error ? error : EINVAL);
}

int
divert6_packet(struct mbuf *m, int dir, u_int16_t divert_port)
{
	struct inpcb *inp;
	struct socket *sa = NULL;
	struct sockaddr_in6 addr;

	inp = NULL;
	div6stat_inc(div6s_ipackets);

	if (m->m_len < sizeof(struct ip6_hdr) &&
	    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
		div6stat_inc(div6s_errors);
		return (0);
	}

	TAILQ_FOREACH(inp, &divb6table.inpt_queue, inp_queue) {
		if (inp->inp_lport == divert_port)
			break;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_len = sizeof(addr);

	if (dir == PF_IN) {
		struct ifaddr *ifa;
		struct ifnet *ifp;

		ifp = if_get(m->m_pkthdr.ph_ifidx);
		if (ifp == NULL) {
			m_freem(m);
			return (0);
		}
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			addr.sin6_addr = satosin6(ifa->ifa_addr)->sin6_addr;
			break;
		}
		if_put(ifp);
	}

	if (inp) {
		sa = inp->inp_socket;
		if (sbappendaddr(sa, &sa->so_rcv, sin6tosa(&addr), m, NULL) == 0) {
			div6stat_inc(div6s_fullsock);
			m_freem(m);
			return (0);
		} else {
			KERNEL_LOCK();
			sorwakeup(inp->inp_socket);
			KERNEL_UNLOCK();
		}
	}

	if (sa == NULL) {
		div6stat_inc(div6s_noport);
		m_freem(m);
	}
	return (0);
}

/*ARGSUSED*/
int
divert6_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *addr,
    struct mbuf *control, struct proc *p)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;

	if (req == PRU_CONTROL) {
		return (in6_control(so, (u_long)m, (caddr_t)addr,
		    (struct ifnet *)control));
	}

	soassertlocked(so);

	if (inp == NULL) {
		error = EINVAL;
		goto release;
	}
	switch (req) {

	case PRU_BIND:
		error = in_pcbbind(inp, addr, p);
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND:
		return (divert6_output(inp, m, addr, control));

	case PRU_ABORT:
		soisdisconnected(so);
		in_pcbdetach(inp);
		break;

	case PRU_SOCKADDR:
		in6_setsockaddr(inp, addr);
		break;

	case PRU_PEERADDR:
		in6_setpeeraddr(inp, addr);
		break;

	case PRU_SENSE:
		return (0);

	case PRU_LISTEN:
	case PRU_CONNECT:
	case PRU_CONNECT2:
	case PRU_ACCEPT:
	case PRU_DISCONNECT:
	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		error =  EOPNOTSUPP;
		break;

	case PRU_RCVD:
	case PRU_RCVOOB:
		return (EOPNOTSUPP);	/* do not free mbuf's */

	default:
		panic("divert6_usrreq");
	}

release:
	m_freem(control);
	m_freem(m);
	return (error);
}

int
divert6_attach(struct socket *so, int proto)
{
	int error;

	if (so->so_pcb != NULL)
		return EINVAL;

	if ((so->so_state & SS_PRIV) == 0)
		return EACCES;

	error = in_pcballoc(so, &divb6table);
	if (error)
		return (error);

	error = soreserve(so, divert6_sendspace, divert6_recvspace);
	if (error)
		return (error);
	sotoinpcb(so)->inp_flags |= INP_HDRINCL;
	return (0);
}

int
divert6_detach(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);

	soassertlocked(so);

	if (inp == NULL)
		return (EINVAL);

	in_pcbdetach(inp);

	return (0);
}

int
divert6_sysctl_div6stat(void *oldp, size_t *oldlenp, void *newp)
{
	uint64_t counters[div6s_ncounters];
	struct div6stat div6stat;
	u_long *words = (u_long *)&div6stat;
	int i;

	CTASSERT(sizeof(div6stat) == (nitems(counters) * sizeof(u_long)));

	counters_read(div6counters, counters, nitems(counters));

	for (i = 0; i < nitems(counters); i++)
		words[i] = (u_long)counters[i];

	return (sysctl_rdstruct(oldp, oldlenp, newp,
	    &div6stat, sizeof(div6stat)));
}

/*
 * Sysctl for divert variables.
 */
int
divert6_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case DIVERT6CTL_SENDSPACE:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert6_sendspace);
		NET_UNLOCK();
		return (error);
	case DIVERT6CTL_RECVSPACE:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert6_recvspace);
		NET_UNLOCK();
		return (error);
	case DIVERT6CTL_STATS:
		return (divert6_sysctl_div6stat(oldp, oldlenp, newp));
	default:
		if (name[0] < DIVERT6CTL_MAXID) {
			NET_LOCK();
			error = sysctl_int_arr(divert6ctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);
			NET_UNLOCK();
			return (error);
		}

		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
