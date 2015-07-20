/*	$OpenBSD: mpls_input.c,v 1.46 2015/07/20 22:16:41 rzalamena Exp $	*/

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
int		 mpls_input(struct ifnet *, struct mbuf *);

void
mpls_init(void)
{
}

int
mpls_install_handler(struct ifnet *ifp)
{
	struct ifih *ifih, *ifihn;

	ifih = malloc(sizeof(*ifih), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (ifih == NULL)
		return (-1);

	ifih->ifih_input = mpls_input;

	/* We must install mpls_input() after ether_input(). */
	SLIST_FOREACH(ifihn, &ifp->if_inputs, ifih_next)
		if (SLIST_NEXT(ifihn, ifih_next) == NULL)
			break;

	if (ifihn == NULL)
		SLIST_INSERT_HEAD(&ifp->if_inputs, ifih, ifih_next);
	else
		SLIST_INSERT_AFTER(ifihn, ifih, ifih_next);

	return (0);
}

void
mpls_uninstall_handler(struct ifnet *ifp)
{
	struct ifih *ifih;

	SLIST_FOREACH(ifih, &ifp->if_inputs, ifih_next) {
		if (ifih->ifih_input != mpls_input)
			continue;

		SLIST_REMOVE(&ifp->if_inputs, ifih, ifih, ifih_next);
		break;
	}

	free(ifih, M_DEVBUF, sizeof(*ifih));
}

int
mpls_input(struct ifnet *ifp, struct mbuf *m)
{
	struct sockaddr_mpls *smpls;
	struct sockaddr_mpls sa_mpls;
	struct shim_hdr	*shim;
	struct rtentry *rt = NULL;
	struct rt_mpls *rt_mpls;
	u_int8_t ttl;
	int i, hasbos;

	if (!ISSET(ifp->if_xflags, IFXF_MPLS)) {
		m_freem(m);
		return (1);
	}

	/* drop all broadcast and multicast packets */
	if (m->m_flags & (M_BCAST | M_MCAST)) {
		m_freem(m);
		return (1);
	}

	if (m->m_len < sizeof(*shim))
		if ((m = m_pullup(m, sizeof(*shim))) == NULL)
			return (1);

	shim = mtod(m, struct shim_hdr *);

#ifdef MPLS_DEBUG
	printf("mpls_input: iface %s label=%d, ttl=%d BoS %d\n",
	    ifp->if_xname, MPLS_LABEL_GET(shim->shim_label),
	    MPLS_TTL_GET(shim->shim_label),
	    MPLS_BOS_ISSET(shim->shim_label));
#endif

	/* check and decrement TTL */
	ttl = ntohl(shim->shim_label & MPLS_TTL_MASK);
	if (ttl-- <= 1) {
		/* TTL exceeded */
		m = mpls_do_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, 0);
		if (m == NULL)
			return (1);
		shim = mtod(m, struct shim_hdr *);
		ttl = ntohl(shim->shim_label & MPLS_TTL_MASK);
	}

	bzero(&sa_mpls, sizeof(sa_mpls));
	smpls = &sa_mpls;
	smpls->smpls_family = AF_MPLS;
	smpls->smpls_len = sizeof(*smpls);
	for (i = 0; i < mpls_inkloop; i++) {
		smpls->smpls_label = shim->shim_label & MPLS_LABEL_MASK;

#ifdef MPLS_DEBUG
		printf("smpls af %d len %d in_label %d in_ifindex %d\n",
		    smpls->smpls_family, smpls->smpls_len,
		    MPLS_LABEL_GET(smpls->smpls_label),
		    ifp->if_index);
#endif

		if (ntohl(smpls->smpls_label) < MPLS_LABEL_RESERVED_MAX) {
			hasbos = MPLS_BOS_ISSET(shim->shim_label);
			m = mpls_shim_pop(m);
			shim = mtod(m, struct shim_hdr *);

			switch (ntohl(smpls->smpls_label)) {
			case MPLS_LABEL_IPV4NULL:
				/*
				 * RFC 4182 relaxes the position of the
				 * explicit NULL labels. The no longer need
				 * to be at the beginning of the stack.
				 */
				if (hasbos) {
do_v4:
					if (mpls_ip_adjttl(m, ttl))
						goto done;
					niq_enqueue(&ipintrq, m);
					goto done;
				}
				continue;
#ifdef INET6
			case MPLS_LABEL_IPV6NULL:
				if (hasbos) {
do_v6:
					if (mpls_ip6_adjttl(m, ttl))
						goto done;
					niq_enqueue(&ip6intrq, m);
					goto done;
				}
				continue;
#endif	/* INET6 */
			case MPLS_LABEL_IMPLNULL:
				if (hasbos) {
					switch (*mtod(m, u_char *) >> 4) {
					case IPVERSION:
						goto do_v4;
#ifdef INET6
					case IPV6_VERSION >> 4:
						goto do_v6;
#endif
					default:
						m_freem(m);
						goto done;
					}
				}
				/* FALLTHROUGH */
			default:
				/* Other cases are not handled for now */
				m_freem(m);
				goto done;
			}
		}

		rt = rtalloc(smplstosa(smpls), RT_REPORT|RT_RESOLVE, 0);
		if (rt == NULL) {
			/* no entry for this label */
#ifdef MPLS_DEBUG
			printf("MPLS_DEBUG: label not found\n");
#endif
			m_freem(m);
			goto done;
		}

		rt->rt_use++;
		rt_mpls = (struct rt_mpls *)rt->rt_llinfo;

		if (rt_mpls == NULL || (rt->rt_flags & RTF_MPLS) == 0) {
#ifdef MPLS_DEBUG
			printf("MPLS_DEBUG: no MPLS information "
			    "attached\n");
#endif
			m_freem(m);
			goto done;
		}

		hasbos = MPLS_BOS_ISSET(shim->shim_label);
		switch (rt_mpls->mpls_operation) {
		case MPLS_OP_LOCAL:
			/* Packet is for us */
			m = mpls_shim_pop(m);
			if (!hasbos)
				/* redo lookup with next label */
				break;

			if (!rt->rt_gateway) {
				m_freem(m);
				goto done;
			}

			switch(rt->rt_gateway->sa_family) {
			case AF_INET:
				if (mpls_ip_adjttl(m, ttl))
					break;
				niq_enqueue(&ipintrq, m);
				break;
#ifdef INET6
			case AF_INET6:
				if (mpls_ip6_adjttl(m, ttl))
					break;
				niq_enqueue(&ip6intrq, m);
				break;
#endif
			default:
				m_freem(m);
			}
			goto done;
		case MPLS_OP_POP:
			m = mpls_shim_pop(m);
			if (!hasbos)
				/* redo lookup with next label */
				break;

			ifp = rt->rt_ifp;
#if NMPE > 0
			if (ifp->if_type == IFT_MPLS) {
				smpls = satosmpls(rt_key(rt));
				mpe_input(m, rt->rt_ifp, smpls, ttl);
				goto done;
			}
#endif
			if (ifp->if_type == IFT_MPLSTUNNEL) {
				ifp->if_output(ifp, m, rt_key(rt), rt);
				goto done;
			}

			if (!rt->rt_gateway) {
				m_freem(m);
				goto done;
			}

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

			/* Output iface is not MPLS-enabled */
			if (!ISSET(ifp->if_xflags, IFXF_MPLS)) {
				m_freem(m);
				goto done;
			}

			(*ifp->if_ll_output)(ifp, m, rt->rt_gateway, rt);
			goto done;
		case MPLS_OP_PUSH:
			m = mpls_shim_push(m, rt_mpls);
			break;
		case MPLS_OP_SWAP:
			m = mpls_shim_swap(m, rt_mpls);
			break;
		}

		if (m == NULL)
			goto done;

		/* refetch label */
		shim = mtod(m, struct shim_hdr *);

		ifp = rt->rt_ifp;
		if (ifp != NULL && rt_mpls->mpls_operation != MPLS_OP_LOCAL)
			break;

		rtfree(rt);
		rt = NULL;
	}

	if (rt == NULL) {
		m_freem(m);
		goto done;
	}

	/* write back TTL */
	shim->shim_label = (shim->shim_label & ~MPLS_TTL_MASK) | htonl(ttl);

#ifdef MPLS_DEBUG
	printf("MPLS: sending on %s outlabel %x dst af %d in %d out %d\n",
    	    ifp->if_xname, ntohl(shim->shim_label), smpls->smpls_family,
	    MPLS_LABEL_GET(smpls->smpls_label),
	    MPLS_LABEL_GET(rt_mpls->mpls_label));
#endif

	/* Output iface is not MPLS-enabled */
	if (!ISSET(ifp->if_xflags, IFXF_MPLS)) {
#ifdef MPLS_DEBUG
		printf("MPLS_DEBUG: interface not mpls enabled\n");
#endif
		goto done;
	}

	(*ifp->if_ll_output)(ifp, m, smplstosa(smpls), rt);
done:
	if (rt)
		rtfree(rt);

	return (1);
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
	int nstk;

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
		rt->rt_use++;
		rtfree(rt);
		if (icmp_reflect(m, NULL, ia))
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
