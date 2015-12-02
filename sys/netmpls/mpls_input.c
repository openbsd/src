/*	$OpenBSD: mpls_input.c,v 1.52 2015/12/02 08:47:00 claudio Exp $	*/

/*
 * Copyright (c) 2008 Claudio Jeker <claudio@openbsd.org>
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

#include "mpe.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#include <netmpls/mpls.h>

#ifdef MPLS_DEBUG
#define MPLS_LABEL_GET(l)	((ntohl((l) & MPLS_LABEL_MASK)) >> MPLS_LABEL_OFFSET)
#define MPLS_TTL_GET(l)		(ntohl((l) & MPLS_TTL_MASK))
#endif

int	mpls_ip_adjttl(struct mbuf *, u_int8_t);
#ifdef INET6
int	mpls_ip6_adjttl(struct mbuf *, u_int8_t);
#endif

struct mbuf	*mpls_do_error(struct mbuf *, int, int, int);

void
mpls_init(void)
{
}

void
mpls_input(struct mbuf *m)
{
	struct sockaddr_mpls *smpls;
	struct sockaddr_mpls sa_mpls;
	struct shim_hdr	*shim;
	struct rtentry *rt;
	struct rt_mpls *rt_mpls;
	struct ifnet   *ifp = NULL;
	u_int8_t ttl;
	int hasbos;

	/* drop all broadcast and multicast packets */
	if (m->m_flags & (M_BCAST | M_MCAST)) {
		m_freem(m);
		return;
	}

	if (m->m_len < sizeof(*shim))
		if ((m = m_pullup(m, sizeof(*shim))) == NULL)
			return;

	shim = mtod(m, struct shim_hdr *);

#ifdef MPLS_DEBUG
	printf("mpls_input: iface %d label=%d, ttl=%d BoS %d\n",
	    m->m_pkthdr.ph_ifidx, MPLS_LABEL_GET(shim->shim_label),
	    MPLS_TTL_GET(shim->shim_label),
	    MPLS_BOS_ISSET(shim->shim_label));
#endif

	/* check and decrement TTL */
	ttl = ntohl(shim->shim_label & MPLS_TTL_MASK);
	if (ttl-- <= 1) {
		/* TTL exceeded */
		m = mpls_do_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, 0);
		if (m == NULL)
			return;
		shim = mtod(m, struct shim_hdr *);
		ttl = ntohl(shim->shim_label & MPLS_TTL_MASK);
	}

	bzero(&sa_mpls, sizeof(sa_mpls));
	smpls = &sa_mpls;
	smpls->smpls_family = AF_MPLS;
	smpls->smpls_len = sizeof(*smpls);
	smpls->smpls_label = shim->shim_label & MPLS_LABEL_MASK;

	hasbos = MPLS_BOS_ISSET(shim->shim_label);

	if (ntohl(smpls->smpls_label) < MPLS_LABEL_RESERVED_MAX) {
		m = mpls_shim_pop(m);
		if (!hasbos) {
			/*
			 * RFC 4182 relaxes the position of the
			 * explicit NULL labels. They no longer need
			 * to be at the beginning of the stack.
			 * In this case the label is ignored and the decision
			 * is made based on the lower one.
			 */
			shim = mtod(m, struct shim_hdr *);
			smpls->smpls_label = shim->shim_label & MPLS_LABEL_MASK;
			hasbos = MPLS_BOS_ISSET(shim->shim_label);
		} else {
			switch (ntohl(smpls->smpls_label)) {
			case MPLS_LABEL_IPV4NULL:
do_v4:
				if (mpls_ip_adjttl(m, ttl))
					return;
				niq_enqueue(&ipintrq, m);
				return;
#ifdef INET6
			case MPLS_LABEL_IPV6NULL:
do_v6:
				if (mpls_ip6_adjttl(m, ttl))
					return;
				niq_enqueue(&ip6intrq, m);
				return;
#endif	/* INET6 */
			case MPLS_LABEL_IMPLNULL:
				switch (*mtod(m, u_char *) >> 4) {
				case IPVERSION:
					goto do_v4;
#ifdef INET6
				case IPV6_VERSION >> 4:
					goto do_v6;
#endif
				default:
					m_freem(m);
					return;
				}
			default:
				/* Other cases are not handled for now */
				m_freem(m);
				return;
			}
		}
	}

	rt = rtalloc(smplstosa(smpls), RT_REPORT|RT_RESOLVE, 0);
	if (rt == NULL) {
		/* no entry for this label */
#ifdef MPLS_DEBUG
		printf("MPLS_DEBUG: label not found\n");
#endif
		m_freem(m);
		return;
	}

	rt_mpls = (struct rt_mpls *)rt->rt_llinfo;
	if (rt_mpls == NULL || (rt->rt_flags & RTF_MPLS) == 0) {
#ifdef MPLS_DEBUG
		printf("MPLS_DEBUG: no MPLS information attached\n");
#endif
		m_freem(m);
		goto done;
	}

	switch (rt_mpls->mpls_operation) {
	case MPLS_OP_POP:
		m = mpls_shim_pop(m);
		if (!hasbos)
			/* just forward to gw */
			break;

		/* last label popped so decide where to push it to */
		ifp = if_get(rt->rt_ifidx);
		if (ifp == NULL) {
			m_freem(m);
			goto done;
		}
#if NMPE > 0
		if (ifp->if_type == IFT_MPLS) {
			smpls = satosmpls(rt_key(rt));
			mpe_input(m, ifp, smpls, ttl);
			goto done;
		}
#endif
		if (ifp->if_type == IFT_MPLSTUNNEL) {
			ifp->if_output(ifp, m, rt_key(rt), rt);
			goto done;
		}

		KASSERT(rt->rt_gateway);

		switch(rt->rt_gateway->sa_family) {
		case AF_INET:
			if (mpls_ip_adjttl(m, ttl))
				goto done;
			break;
#ifdef INET6
		case AF_INET6:
			if (mpls_ip6_adjttl(m, ttl))
				goto done;
			break;
#endif
		default:
			m_freem(m);
			goto done;
		}

		/* shortcut sending out the packet */
		KERNEL_LOCK();
		if (!ISSET(ifp->if_xflags, IFXF_MPLS))
			(*ifp->if_output)(ifp, m, rt->rt_gateway, rt);
		else
			(*ifp->if_ll_output)(ifp, m, rt->rt_gateway, rt);
		KERNEL_UNLOCK();
		goto done;
	case MPLS_OP_PUSH:
		/* this does not make much sense but it does not hurt */
		m = mpls_shim_push(m, rt_mpls);
		break;
	case MPLS_OP_SWAP:
		m = mpls_shim_swap(m, rt_mpls);
		break;
	default:
		m_freem(m);
		goto done;
	}

	if (m == NULL)
		goto done;

	/* refetch label and write back TTL */
	shim = mtod(m, struct shim_hdr *);
	shim->shim_label = (shim->shim_label & ~MPLS_TTL_MASK) | htonl(ttl);

	ifp = if_get(rt->rt_ifidx);
	if (ifp == NULL) {
		m_freem(m);
		goto done;
	}
#ifdef MPLS_DEBUG
	printf("MPLS: sending on %s outlabel %x dst af %d in %d out %d\n",
    	    ifp->if_xname, ntohl(shim->shim_label), smpls->smpls_family,
	    MPLS_LABEL_GET(smpls->smpls_label),
	    MPLS_LABEL_GET(rt_mpls->mpls_label));
#endif

	/* Output iface is not MPLS-enabled */
	if (!ISSET(ifp->if_xflags, IFXF_MPLS)) {
#ifdef MPLS_DEBUG
		printf("MPLS_DEBUG: interface %s not mpls enabled\n",
		    ifp->if_xname);
#endif
		m_freem(m);
		goto done;
	}

	KERNEL_LOCK();
	(*ifp->if_ll_output)(ifp, m, smplstosa(smpls), rt);
	KERNEL_UNLOCK();
done:
	if_put(ifp);
	rtfree(rt);
}

int
mpls_ip_adjttl(struct mbuf *m, u_int8_t ttl)
{
	struct ip *ip;
	int hlen;

	if (mpls_mapttl_ip) {
		if (m->m_len < sizeof(struct ip) &&
		    (m = m_pullup(m, sizeof(struct ip))) == NULL)
			return -1;
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		if (m->m_len < hlen) {
			if ((m = m_pullup(m, hlen)) == NULL)
				return -1;
			ip = mtod(m, struct ip *);
		}
		/* make sure we have a valid header */
		if (in_cksum(m, hlen) != 0) {
			m_free(m);
			return -1;
		}

		/* set IP ttl from MPLS ttl */
		ip->ip_ttl = ttl;

		/* recalculate checksum */
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);
	}
	return 0;
}

#ifdef INET6
int
mpls_ip6_adjttl(struct mbuf *m, u_int8_t ttl)
{
	struct ip6_hdr *ip6hdr;

	if (mpls_mapttl_ip6) {
		if (m->m_len < sizeof(struct ip6_hdr) &&
		    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL)
			return -1;

		ip6hdr = mtod(m, struct ip6_hdr *);

		/* set IPv6 ttl from MPLS ttl */
		ip6hdr->ip6_hlim = ttl;
	}
	return 0;
}
#endif	/* INET6 */

struct mbuf *
mpls_do_error(struct mbuf *m, int type, int code, int destmtu)
{
	struct shim_hdr stack[MPLS_INKERNEL_LOOP_MAX];
	struct sockaddr_mpls sa_mpls;
	struct sockaddr_mpls *smpls;
	struct rtentry *rt = NULL;
	struct shim_hdr *shim;
	struct in_ifaddr *ia;
	struct icmp *icp;
	struct ip *ip;
	int nstk, error;

	for (nstk = 0; nstk < MPLS_INKERNEL_LOOP_MAX; nstk++) {
		if (m->m_len < sizeof(*shim) &&
		    (m = m_pullup(m, sizeof(*ip))) == NULL)
			return (NULL);
		stack[nstk] = *mtod(m, struct shim_hdr *);
		m_adj(m, sizeof(*shim));
		if (MPLS_BOS_ISSET(stack[nstk].shim_label))
			break;
	}
	shim = &stack[0];

	switch (*mtod(m, u_char *) >> 4) {
	case IPVERSION:
		if (m->m_len < sizeof(*ip) &&
		    (m = m_pullup(m, sizeof(*ip))) == NULL)
			return (NULL);
		m = icmp_do_error(m, type, code, 0, destmtu);
		if (m == NULL)
			return (NULL);

		if (icmp_do_exthdr(m, ICMP_EXT_MPLS, 1, stack,
		    (nstk + 1) * sizeof(*shim)))
			return (NULL);

		/* set ip_src to something usable, based on the MPLS label */
		bzero(&sa_mpls, sizeof(sa_mpls));
		smpls = &sa_mpls;
		smpls->smpls_family = AF_MPLS;
		smpls->smpls_len = sizeof(*smpls);
		smpls->smpls_label = shim->shim_label & MPLS_LABEL_MASK;

		rt = rtalloc(smplstosa(smpls), RT_REPORT|RT_RESOLVE, 0);
		if (rt == NULL) {
			/* no entry for this label */
			m_freem(m);
			return (NULL);
		}
		if (rt->rt_ifa->ifa_addr->sa_family == AF_INET)
			ia = ifatoia(rt->rt_ifa);
		else {
			/* XXX this needs fixing, if the MPLS is on an IP
			 * less interface we need to find some other IP to
			 * use as source.
			 */
			rtfree(rt);
			m_freem(m);
			return (NULL);
		}
		rtfree(rt);
		KERNEL_LOCK();
		error = icmp_reflect(m, NULL, ia);
		KERNEL_UNLOCK();
		if (error)
			return (NULL);

		ip = mtod(m, struct ip *);
		/* stuff to fix up which is normaly done in ip_output */
		ip->ip_v = IPVERSION;
		ip->ip_id = htons(ip_randomid());
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, sizeof(*ip));

		/* stolen from icmp_send() */
		icp = (struct icmp *)(mtod(m, caddr_t) + sizeof(*ip));
		icp->icmp_cksum = 0;
		icp->icmp_cksum = in4_cksum(m, 0, sizeof(*ip),
		    ntohs(ip->ip_len) - sizeof(*ip));

		break;
#ifdef INET6
	case IPV6_VERSION >> 4:
#endif
	default:
		m_freem(m);
		return (NULL);
	}

	/* add mpls stack back to new packet */
	M_PREPEND(m, (nstk + 1) * sizeof(*shim), M_NOWAIT);
	if (m == NULL)
		return (NULL);
	m_copyback(m, 0, (nstk + 1) * sizeof(*shim), stack, M_NOWAIT);

	/* change TTL to default */
	shim = mtod(m, struct shim_hdr *);
	shim->shim_label =
	    (shim->shim_label & ~MPLS_TTL_MASK) | htonl(mpls_defttl);

	return (m);
}
