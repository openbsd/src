/*      $OpenBSD: ip_divert.c,v 1.89 2022/10/17 14:49:02 mvs Exp $ */

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
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_divert.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#include <net/pfvar.h>

struct	inpcbtable	divbtable;
struct	cpumem		*divcounters;

#ifndef DIVERT_SENDSPACE
#define DIVERT_SENDSPACE	(65536 + 100)
#endif
u_int   divert_sendspace = DIVERT_SENDSPACE;
#ifndef DIVERT_RECVSPACE
#define DIVERT_RECVSPACE	(65536 + 100)
#endif
u_int   divert_recvspace = DIVERT_RECVSPACE;

#ifndef DIVERTHASHSIZE
#define DIVERTHASHSIZE	128
#endif

const struct sysctl_bounded_args divertctl_vars[] = {
	{ DIVERTCTL_RECVSPACE, &divert_recvspace, 0, INT_MAX },
	{ DIVERTCTL_SENDSPACE, &divert_sendspace, 0, INT_MAX },
};

const struct pr_usrreqs divert_usrreqs = {
	.pru_attach	= divert_attach,
	.pru_detach	= divert_detach,
	.pru_lock	= divert_lock,
	.pru_unlock	= divert_unlock,
	.pru_bind	= divert_bind,
	.pru_shutdown	= divert_shutdown,
	.pru_send	= divert_send,
	.pru_control	= in_control,
	.pru_sockaddr	= in_sockaddr,
	.pru_peeraddr	= in_peeraddr,
};

int divbhashsize = DIVERTHASHSIZE;

int	divert_output(struct inpcb *, struct mbuf *, struct mbuf *,
	    struct mbuf *);
void
divert_init(void)
{
	in_pcbinit(&divbtable, divbhashsize);
	divcounters = counters_alloc(divs_ncounters);
}

int
divert_output(struct inpcb *inp, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control)
{
	struct sockaddr_in *sin;
	int error, min_hdrlen, off, dir;
	struct ip *ip;

	m_freem(control);

	if ((error = in_nam2sin(nam, &sin)))
		goto fail;

	/* Do basic sanity checks. */
	if (m->m_pkthdr.len < sizeof(struct ip))
		goto fail;
	if ((m = m_pullup(m, sizeof(struct ip))) == NULL) {
		/* m_pullup() has freed the mbuf, so just return. */
		divstat_inc(divs_errors);
		return (ENOBUFS);
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION)
		goto fail;
	off = ip->ip_hl << 2;
	if (off < sizeof(struct ip) || ntohs(ip->ip_len) < off ||
	    m->m_pkthdr.len < ntohs(ip->ip_len))
		goto fail;

	dir = (sin->sin_addr.s_addr == INADDR_ANY ? PF_OUT : PF_IN);

	switch (ip->ip_p) {
	case IPPROTO_TCP:
		min_hdrlen = sizeof(struct tcphdr);
		m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
		break;
	case IPPROTO_UDP:
		min_hdrlen = sizeof(struct udphdr);
		m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
		break;
	case IPPROTO_ICMP:
		min_hdrlen = ICMP_MINLEN;
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

		rt = rtalloc(sintosa(sin), 0, inp->inp_rtableid);
		if (!rtisvalid(rt) || !ISSET(rt->rt_flags, RTF_LOCAL)) {
			rtfree(rt);
			error = EADDRNOTAVAIL;
			goto fail;
		}
		m->m_pkthdr.ph_ifidx = rt->rt_ifidx;
		rtfree(rt);

		/*
		 * Recalculate IP and protocol checksums for the inbound packet
		 * since the userspace application may have modified the packet
		 * prior to reinjection.
		 */
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, off);
		in_proto_cksum_out(m, NULL);

		ifp = if_get(m->m_pkthdr.ph_ifidx);
		if (ifp == NULL) {
			error = ENETDOWN;
			goto fail;
		}
		ipv4_input(ifp, m);
		if_put(ifp);
	} else {
		m->m_pkthdr.ph_rtableid = inp->inp_rtableid;

		error = ip_output(m, NULL, &inp->inp_route,
		    IP_ALLOWBROADCAST | IP_RAWOUTPUT, NULL, NULL, 0);
	}

	divstat_inc(divs_opackets);
	return (error);

fail:
	m_freem(m);
	divstat_inc(divs_errors);
	return (error ? error : EINVAL);
}

void
divert_packet(struct mbuf *m, int dir, u_int16_t divert_port)
{
	struct inpcb *inp = NULL;
	struct socket *so;
	struct sockaddr_in sin;

	divstat_inc(divs_ipackets);

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL) {
		divstat_inc(divs_errors);
		goto bad;
	}

	mtx_enter(&divbtable.inpt_mtx);
	TAILQ_FOREACH(inp, &divbtable.inpt_queue, inp_queue) {
		if (inp->inp_lport != divert_port)
			continue;
		in_pcbref(inp);
		break;
	}
	mtx_leave(&divbtable.inpt_mtx);
	if (inp == NULL) {
		divstat_inc(divs_noport);
		goto bad;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);

	if (dir == PF_IN) {
		struct ifaddr *ifa;
		struct ifnet *ifp;

		ifp = if_get(m->m_pkthdr.ph_ifidx);
		if (ifp == NULL) {
			divstat_inc(divs_errors);
			goto bad;
		}
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			sin.sin_addr = satosin(ifa->ifa_addr)->sin_addr;
			break;
		}
		if_put(ifp);
	}

	mtx_enter(&inp->inp_mtx);
	so = inp->inp_socket;
	if (sbappendaddr(so, &so->so_rcv, sintosa(&sin), m, NULL) == 0) {
		mtx_leave(&inp->inp_mtx);
		divstat_inc(divs_fullsock);
		goto bad;
	}
	mtx_leave(&inp->inp_mtx);
	sorwakeup(so);

	in_pcbunref(inp);
	return;

 bad:
	if (inp != NULL)
		in_pcbunref(inp);
	m_freem(m);
}

int
divert_attach(struct socket *so, int proto, int wait)
{
	int error;

	if (so->so_pcb != NULL)
		return EINVAL;
	if ((so->so_state & SS_PRIV) == 0)
		return EACCES;

	error = in_pcballoc(so, &divbtable, wait);
	if (error)
		return error;

	error = soreserve(so, divert_sendspace, divert_recvspace);
	if (error)
		return error;

	sotoinpcb(so)->inp_flags |= INP_HDRINCL;
	return (0);
}

int
divert_detach(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);

	soassertlocked(so);

	if (inp == NULL)
		return (EINVAL);

	in_pcbdetach(inp);
	return (0);
}

void
divert_lock(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);

	NET_ASSERT_LOCKED();
	mtx_enter(&inp->inp_mtx);
}

void
divert_unlock(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);

	NET_ASSERT_LOCKED();
	mtx_leave(&inp->inp_mtx);
}

int
divert_bind(struct socket *so, struct mbuf *addr, struct proc *p)
{
	struct inpcb *inp = sotoinpcb(so);

	soassertlocked(so);
	return in_pcbbind(inp, addr, p);
}

int
divert_shutdown(struct socket *so)
{
	soassertlocked(so);
	socantsendmore(so);
	return (0);
}

int
divert_send(struct socket *so, struct mbuf *m, struct mbuf *addr,
    struct mbuf *control)
{
	struct inpcb *inp = sotoinpcb(so);

	soassertlocked(so);
	return (divert_output(inp, m, addr, control));
}

int
divert_sysctl_divstat(void *oldp, size_t *oldlenp, void *newp)
{
	uint64_t counters[divs_ncounters];
	struct divstat divstat;
	u_long *words = (u_long *)&divstat;
	int i;

	CTASSERT(sizeof(divstat) == (nitems(counters) * sizeof(u_long)));
	memset(&divstat, 0, sizeof divstat);
	counters_read(divcounters, counters, nitems(counters));

	for (i = 0; i < nitems(counters); i++)
		words[i] = (u_long)counters[i];

	return (sysctl_rdstruct(oldp, oldlenp, newp,
	    &divstat, sizeof(divstat)));
}

/*
 * Sysctl for divert variables.
 */
int
divert_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case DIVERTCTL_STATS:
		return (divert_sysctl_divstat(oldp, oldlenp, newp));
	default:
		NET_LOCK();
		error = sysctl_bounded_arr(divertctl_vars,
		    nitems(divertctl_vars), name, namelen, oldp, oldlenp, newp,
		    newlen);
		NET_UNLOCK();
		return (error);
	}
	/* NOTREACHED */
}
