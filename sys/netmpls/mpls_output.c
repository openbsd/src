/* $OpenBSD: mpls_output.c,v 1.8 2010/05/07 13:33:17 claudio Exp $ */

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
#include <net/route.h>

#include <netmpls/mpls.h>

extern int	mpls_inkloop;

#ifdef MPLS_DEBUG
#define MPLS_LABEL_GET(l)	((ntohl((l) & MPLS_LABEL_MASK)) >> MPLS_LABEL_OFFSET)
#endif

struct mbuf *
mpls_output(struct mbuf *m, struct rtentry *rt0)
{
	struct ifnet		*ifp = m->m_pkthdr.rcvif;
	struct sockaddr_mpls	*smpls;
	struct sockaddr_mpls	 sa_mpls;
	struct shim_hdr		*shim;
	struct rtentry		*rt = rt0;
	struct rt_mpls		*rt_mpls;
	int			 i;

	if (!mpls_enable) {
		m_freem(m);
		goto bad;
	}

	/* reset broadcast and multicast flags, this is a P2P tunnel */
	m->m_flags &= ~(M_BCAST | M_MCAST);

	for (i = 0; i < mpls_inkloop; i++) {
		if (rt == NULL) {
			shim = mtod(m, struct shim_hdr *);

			bzero(&sa_mpls, sizeof(sa_mpls));
			smpls = &sa_mpls;
			smpls->smpls_family = AF_MPLS;
			smpls->smpls_len = sizeof(*smpls);
			smpls->smpls_label = shim->shim_label & MPLS_LABEL_MASK;

			rt = rtalloc1(smplstosa(smpls), RT_REPORT, 0);
			if (rt == NULL) {
				/* no entry for this label */
#ifdef MPLS_DEBUG
				printf("MPLS_DEBUG: label not found\n");
#endif
				m_freem(m);
				goto bad;
			}
			rt->rt_use++;
		}

		rt_mpls = (struct rt_mpls *)rt->rt_llinfo;
		if (rt_mpls == NULL || (rt->rt_flags & RTF_MPLS) == 0) {
			/* no MPLS information for this entry */
#ifdef MPLS_DEBUG
			printf("MPLS_DEBUG: no MPLS information attached\n");
#endif
			m_freem(m);
			goto bad;
		}

		switch (rt_mpls->mpls_operation & (MPLS_OP_PUSH | MPLS_OP_POP |
		    MPLS_OP_SWAP)) {

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
			m_freem(m);
			goto bad;
		}

		if (m == NULL)
			goto bad;

		/* refetch label */
		shim = mtod(m, struct shim_hdr *);
		ifp = rt->rt_ifp;

		if (ifp != NULL)
			break;

		if (rt0 != rt)
			RTFREE(rt);

		rt = NULL;
	}

	/* write back TTL */
	shim->shim_label &= ~MPLS_TTL_MASK;
	shim->shim_label |= MPLS_BOS_MASK | htonl(mpls_defttl);

#ifdef MPLS_DEBUG
	printf("MPLS: sending on %s outshim %x outlabel %d\n",
	    ifp->if_xname, ntohl(shim->shim_label),
	    MPLS_LABEL_GET(rt_mpls->mpls_label));
#endif

	if (rt != rt0)
		RTFREE(rt);

	return (m);
bad:
	if (rt != rt0)
		RTFREE(rt);

	return (NULL);
}
