/* $OpenBSD: mpls_output.c,v 1.26 2015/12/02 08:47:00 claudio Exp $ */

/*
 * Copyright (c) 2008 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2008 Michele Marchetto <michele@openbsd.org>
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
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netmpls/mpls.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#ifdef MPLS_DEBUG
#define MPLS_LABEL_GET(l)	((ntohl((l) & MPLS_LABEL_MASK)) >> MPLS_LABEL_OFFSET)
#endif

void		mpls_do_cksum(struct mbuf *);
u_int8_t	mpls_getttl(struct mbuf *, sa_family_t);

int
mpls_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	struct sockaddr_mpls	*smpls;
	struct sockaddr_mpls	 sa_mpls;
	struct shim_hdr		*shim;
	struct rt_mpls		*rt_mpls;
	int			 error;
	u_int8_t		 ttl;

	if (rt == NULL || (dst->sa_family != AF_INET &&
	    dst->sa_family != AF_INET6 && dst->sa_family != AF_MPLS)) {
		if (!ISSET(ifp->if_xflags, IFXF_MPLS))
			return (ifp->if_output(ifp, m, dst, rt));
		else
			return (ifp->if_ll_output(ifp, m, dst, rt));
	}

	/* need to calculate checksums now if necessary */
	mpls_do_cksum(m);

	/* initialize sockaddr_mpls */
	bzero(&sa_mpls, sizeof(sa_mpls));
	smpls = &sa_mpls;
	smpls->smpls_family = AF_MPLS;
	smpls->smpls_len = sizeof(*smpls);

	ttl = mpls_getttl(m, dst->sa_family);

	rt_mpls = (struct rt_mpls *)rt->rt_llinfo;
	if (rt_mpls == NULL || (rt->rt_flags & RTF_MPLS) == 0) {
		/* no MPLS information for this entry */
		if (!ISSET(ifp->if_xflags, IFXF_MPLS)) {
#ifdef MPLS_DEBUG
			printf("MPLS_DEBUG: interface not mpls enabled\n");
#endif
			error = ENETUNREACH;
			goto bad;
		}

		return (ifp->if_ll_output(ifp, m, dst, rt));
	}

	/* to be honest here only the push operation makes sense */
	switch (rt_mpls->mpls_operation) {
	case MPLS_OP_PUSH:
		m = mpls_shim_push(m, rt_mpls);
		break;
	case MPLS_OP_POP:
		m = mpls_shim_pop(m);
		break;
	case MPLS_OP_SWAP:
		m = mpls_shim_swap(m, rt_mpls);
		break;
	default:
		error = EINVAL;
		goto bad;
	}

	if (m == NULL) {
		error = ENOBUFS;
		goto bad;
	}

	/* refetch label */
	shim = mtod(m, struct shim_hdr *);
	/* mark first label with BOS flag */
	if (dst->sa_family != AF_MPLS)
		shim->shim_label |= MPLS_BOS_MASK;

	/* write back TTL */
	shim->shim_label &= ~MPLS_TTL_MASK;
	shim->shim_label |= htonl(ttl);

#ifdef MPLS_DEBUG
	printf("MPLS: sending on %s outshim %x outlabel %d\n",
	    ifp->if_xname, ntohl(shim->shim_label),
	    MPLS_LABEL_GET(rt_mpls->mpls_label));
#endif

	/* Output iface is not MPLS-enabled */
	if (!ISSET(ifp->if_xflags, IFXF_MPLS)) {
#ifdef MPLS_DEBUG
		printf("MPLS_DEBUG: interface not mpls enabled\n");
#endif
		error = ENETUNREACH;
		goto bad;
	}

	/* reset broadcast and multicast flags, this is a P2P tunnel */
	m->m_flags &= ~(M_BCAST | M_MCAST);

	smpls->smpls_label = shim->shim_label & MPLS_LABEL_MASK;
	error = ifp->if_ll_output(ifp, m, smplstosa(smpls), rt);
	return (error);
bad:
	m_freem(m);
	return (error);
}

void
mpls_do_cksum(struct mbuf *m)
{
	struct ip *ip;
	u_int16_t hlen;

	in_proto_cksum_out(m, NULL);

	if (m->m_pkthdr.csum_flags & M_IPV4_CSUM_OUT) {
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		ip->ip_sum = in_cksum(m, hlen);
		m->m_pkthdr.csum_flags &= ~M_IPV4_CSUM_OUT;
	}
}

u_int8_t
mpls_getttl(struct mbuf *m, sa_family_t af)
{
	struct shim_hdr *shim;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6hdr;
#endif
	u_int8_t ttl = mpls_defttl;

	/* If the AF is MPLS then inherit the TTL from the present label. */
	if (af == AF_MPLS) {
		shim = mtod(m, struct shim_hdr *);
		ttl = ntohl(shim->shim_label & MPLS_TTL_MASK);
		return (ttl);
	}
	/* Else extract TTL from the encapsualted packet. */
	switch (*mtod(m, u_char *) >> 4) {
	case IPVERSION:
		if (!mpls_mapttl_ip)
			break;
		if (m->m_len < sizeof(*ip))
			break;			/* impossible */
		ip = mtod(m, struct ip *);
		ttl = ip->ip_ttl;
		break;
#ifdef INET6
	case IPV6_VERSION >> 4:
		if (!mpls_mapttl_ip6)
			break;
		if (m->m_len < sizeof(struct ip6_hdr))
			break;			/* impossible */
		ip6hdr = mtod(m, struct ip6_hdr *);
		ttl = ip6hdr->ip6_hlim;
		break;
#endif
	default:
		break;
	}
	return (ttl);
}
