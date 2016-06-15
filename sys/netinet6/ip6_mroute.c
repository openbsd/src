/*	$NetBSD: ip6_mroute.c,v 1.59 2003/12/10 09:28:38 itojun Exp $	*/
/*	$KAME: ip6_mroute.c,v 1.45 2001/03/25 08:38:51 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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

/*	BSDI ip_mroute.c,v 2.10 1996/11/14 00:29:52 jch Exp	*/

/*
 * Copyright (c) 1989 Stephen Deering
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *      @(#)ip_mroute.c 8.2 (Berkeley) 11/15/93
 */

/*
 * IP multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1994
 *
 * MROUTING Revision: 3.5.1.2 + PIM-SMv2 (pimd) Support
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <crypto/siphash.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_mroute.h>

#ifdef PIM
#include <netinet/pim.h>
#include <netinet6/pim6_var.h>

int set_pim6(int *);
int get_pim6(struct mbuf *);
int socket6_send(struct socket *, struct mbuf *, struct sockaddr_in6 *);
int register_send(struct ip6_hdr *, struct mif6 *, struct mbuf *);
#endif

int ip6_mdq(struct mbuf *, struct ifnet *, struct mf6c *);
void phyint_send6(struct ip6_hdr *, struct mif6 *, struct mbuf *);

/*
 * Globals.  All but ip6_mrouter, ip6_mrtproto and mrt6stat could be static,
 * except for netstat or debugging purposes.
 */
struct socket  *ip6_mrouter = NULL;
int		ip6_mrouter_ver = 0;
int		ip6_mrtproto = IPPROTO_PIM;    /* for netstat only */
struct mrt6stat	mrt6stat;

#define NO_RTE_FOUND	0x1
#define RTE_FOUND	0x2

struct mf6c	*mf6ctable[MF6CTBLSIZ];
SIPHASH_KEY	mf6chashkey;
u_char		n6expire[MF6CTBLSIZ];
struct mif6	mif6table[MAXMIFS];

void expire_upcalls6(void *);
#define		EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second */
#define		UPCALL_EXPIRE	6		/* number of timeouts */

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
static mifi_t nummifs = 0;
static mifi_t reg_mif_num = (mifi_t)-1;
unsigned int reg_mif_idx;

#ifdef PIM
struct pim6stat pim6stat;
static int pim6;
#endif

/*
 * Hash function for a source, group entry
 */
u_int32_t _mf6chash(const struct in6_addr *, const struct in6_addr *);
#define MF6CHASH(a, g) _mf6chash(&(a), &(g))

/*
 * Find a route for a given origin IPv6 address and Multicast group address.
 * Quality of service parameter to be added in the future!!!
 */
#define MF6CFIND(o, g, rt) do { \
	struct mf6c *_rt = mf6ctable[MF6CHASH(o,g)]; \
	rt = NULL; \
	mrt6stat.mrt6s_mfc_lookups++; \
	while (_rt) { \
		if (IN6_ARE_ADDR_EQUAL(&_rt->mf6c_origin.sin6_addr, &(o)) && \
		    IN6_ARE_ADDR_EQUAL(&_rt->mf6c_mcastgrp.sin6_addr, &(g)) && \
		    (_rt->mf6c_stall == NULL)) { \
			rt = _rt; \
			break; \
		} \
		_rt = _rt->mf6c_next; \
	} \
	if (rt == NULL) { \
		mrt6stat.mrt6s_mfc_misses++; \
	} \
} while (0)

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) do { \
	    int xxs; \
		\
	    delta = (a).tv_usec - (b).tv_usec; \
	    if ((xxs = (a).tv_sec - (b).tv_sec)) { \
	       switch (xxs) { \
		      case 2: \
			  delta += 1000000; \
			      /* FALLTHROUGH */ \
		      case 1: \
			  delta += 1000000; \
			  break; \
		      default: \
			  delta += (1000000 * xxs); \
	       } \
	    } \
} while (0)

#define TV_LT(a, b) (((a).tv_usec < (b).tv_usec && \
	      (a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

int get_sg6_cnt(struct sioc_sg_req6 *);
int get_mif6_cnt(struct sioc_mif_req6 *);
int ip6_mrouter_init(struct socket *, int, int);
int add_m6if(struct mif6ctl *);
int del_m6if(mifi_t *);
int add_m6fc(struct mf6cctl *);
int del_m6fc(struct mf6cctl *);

static struct timeout expire_upcalls6_ch;

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
int
ip6_mrouter_set(int cmd, struct socket *so, struct mbuf *m)
{
	if (cmd != MRT6_INIT && so != ip6_mrouter)
		return (EPERM);

	switch (cmd) {
	case MRT6_INIT:
		if (m == NULL || m->m_len < sizeof(int))
			return (EINVAL);
		return (ip6_mrouter_init(so, *mtod(m, int *), cmd));
	case MRT6_DONE:
		return (ip6_mrouter_done());
	case MRT6_ADD_MIF:
		if (m == NULL || m->m_len < sizeof(struct mif6ctl))
			return (EINVAL);
		return (add_m6if(mtod(m, struct mif6ctl *)));
	case MRT6_DEL_MIF:
		if (m == NULL || m->m_len < sizeof(mifi_t))
			return (EINVAL);
		return (del_m6if(mtod(m, mifi_t *)));
	case MRT6_ADD_MFC:
		if (m == NULL || m->m_len < sizeof(struct mf6cctl))
			return (EINVAL);
		return (add_m6fc(mtod(m, struct mf6cctl *)));
	case MRT6_DEL_MFC:
		if (m == NULL || m->m_len < sizeof(struct mf6cctl))
			return (EINVAL);
		return (del_m6fc(mtod(m,  struct mf6cctl *)));
#ifdef PIM
	case MRT6_PIM:
		if (m == NULL || m->m_len < sizeof(int))
			return (EINVAL);
		return (set_pim6(mtod(m, int *)));
#endif
	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Handle MRT getsockopt commands
 */
int
ip6_mrouter_get(int cmd, struct socket *so, struct mbuf **mp)
{
	if (so != ip6_mrouter)
		return (EPERM);

	*mp = m_get(M_WAIT, MT_SOOPTS);

	switch (cmd) {
#ifdef PIM
	case MRT6_PIM:
		return get_pim6(*mp);
#endif
	default:
		return EOPNOTSUPP;
	}
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
mrt6_ioctl(u_long cmd, caddr_t data)
{

	switch (cmd) {
	case SIOCGETSGCNT_IN6:
		return (get_sg6_cnt((struct sioc_sg_req6 *)data));
	case SIOCGETMIFCNT_IN6:
		return (get_mif6_cnt((struct sioc_mif_req6 *)data));
	default:
		return (ENOTTY);
	}
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
int
get_sg6_cnt(struct sioc_sg_req6 *req)
{
	struct mf6c *rt;
	int s;

	s = splsoftnet();
	MF6CFIND(req->src.sin6_addr, req->grp.sin6_addr, rt);
	splx(s);
	if (rt != NULL) {
		req->pktcnt = rt->mf6c_pkt_cnt;
		req->bytecnt = rt->mf6c_byte_cnt;
		req->wrong_if = rt->mf6c_wrong_if;
	} else
		return (ESRCH);
#if 0
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
#endif

	return 0;
}

/*
 * returns the input and output packet and byte counts on the mif provided
 */
int
get_mif6_cnt(struct sioc_mif_req6 *req)
{
	mifi_t mifi = req->mifi;

	if (mifi >= nummifs)
		return EINVAL;

	req->icount = mif6table[mifi].m6_pkt_in;
	req->ocount = mif6table[mifi].m6_pkt_out;
	req->ibytes = mif6table[mifi].m6_bytes_in;
	req->obytes = mif6table[mifi].m6_bytes_out;

	return 0;
}

int
mrt6_sysctl_mif(void *oldp, size_t *oldlenp)
{
	caddr_t where = oldp;
	size_t needed, given;
	struct mif6 *mifp;
	mifi_t mifi;
	struct mif6info minfo;

	given = *oldlenp;
	needed = 0;
	for (mifi = 0; mifi < nummifs; mifi++) {
		mifp = &mif6table[mifi];
		if (mifp->m6_ifp == NULL)
			continue;

		minfo.m6_mifi = mifi;
		minfo.m6_flags = mifp->m6_flags;
		minfo.m6_lcl_addr = mifp->m6_lcl_addr;
		minfo.m6_ifindex = mifp->m6_ifp->if_index;
		minfo.m6_pkt_in = mifp->m6_pkt_in;
		minfo.m6_pkt_out = mifp->m6_pkt_out;
		minfo.m6_bytes_in = mifp->m6_bytes_in;
		minfo.m6_bytes_out = mifp->m6_bytes_out;
		minfo.m6_rate_limit = mifp->m6_rate_limit;

		needed += sizeof(minfo);
		if (where && needed <= given) {
			int error;

			error = copyout(&minfo, where, sizeof(minfo));
			if (error)
				return (error);
			where += sizeof(minfo);
		}
	}
	if (where) {
		*oldlenp = needed;
		if (given < needed)
			return (ENOMEM);
	} else
		*oldlenp = (11 * needed) / 10;

	return (0);
}

int
mrt6_sysctl_mfc(void *oldp, size_t *oldlenp)
{
	caddr_t where = oldp;
	size_t needed, given;
	u_long i;
	u_int64_t waitings;
	struct mf6c *m;
	struct mf6cinfo minfo;
	struct rtdetq *r;

	given = *oldlenp;
	needed = 0;
	for (i = 0; i < MF6CTBLSIZ; ++i) {
		m = mf6ctable[i];
		while (m) {
			minfo.mf6c_origin = m->mf6c_origin;
			minfo.mf6c_mcastgrp = m->mf6c_mcastgrp;
			minfo.mf6c_parent = m->mf6c_parent;
			minfo.mf6c_ifset = m->mf6c_ifset;
			minfo.mf6c_pkt_cnt = m->mf6c_pkt_cnt;
			minfo.mf6c_byte_cnt = m->mf6c_byte_cnt;

			for (waitings = 0, r = m->mf6c_stall; r; r = r->next)
				waitings++;
			minfo.mf6c_stall_cnt = waitings;

			needed += sizeof(minfo);
			if (where && needed <= given) {
				int error;

				error = copyout(&minfo, where, sizeof(minfo));
				if (error)
					return (error);
				where += sizeof(minfo);
			}
			m = m->mf6c_next;
		}
	}
	if (where) {
		*oldlenp = needed;
		if (given < needed)
			return (ENOMEM);
	} else
		*oldlenp = (11 * needed) / 10;

	return (0);
}

#ifdef PIM
/*
 * Get PIM processiong global
 */
int
get_pim6(struct mbuf *m)
{
	int *i;

	i = mtod(m, int *);

	*i = pim6;

	return 0;
}

int
set_pim6(int *i)
{
	if ((*i != 1) && (*i != 0))
		return EINVAL;

	pim6 = *i;

	return 0;
}
#endif

/*
 * Enable multicast routing
 */
int
ip6_mrouter_init(struct socket *so, int v, int cmd)
{
	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_ICMPV6)
		return (EOPNOTSUPP);

	if (v != 1)
		return (ENOPROTOOPT);

	if (ip6_mrouter != NULL)
		return (EADDRINUSE);

	ip6_mrouter = so;
	ip6_mrouter_ver = cmd;

	bzero((caddr_t)mf6ctable, sizeof(mf6ctable));
	arc4random_buf(&mf6chashkey, sizeof(mf6chashkey));
	bzero((caddr_t)n6expire, sizeof(n6expire));

#ifdef PIM
	pim6 = 0;/* used for stubbing out/in pim stuff */
#endif

	timeout_set(&expire_upcalls6_ch, expire_upcalls6, NULL);
	timeout_add(&expire_upcalls6_ch, EXPIRE_TIMEOUT);

	return 0;
}

/*
 * Disable multicast routing
 */
int
ip6_mrouter_done(void)
{
	mifi_t mifi;
	int i;
	struct ifnet *ifp;
	struct in6_ifreq ifr;
	struct mf6c *rt;
	struct rtdetq *rte;
	int s;

	s = splsoftnet();

	/*
	 * For each phyint in use, disable promiscuous reception of all IPv6
	 * multicasts.
	 */
	for (mifi = 0; mifi < nummifs; mifi++) {
		if (mif6table[mifi].m6_ifp == NULL)
			continue;

		if (!(mif6table[mifi].m6_flags & MIFF_REGISTER)) {
			memset(&ifr, 0, sizeof(ifr));
			ifr.ifr_addr.sin6_family = AF_INET6;
			ifr.ifr_addr.sin6_addr= in6addr_any;
			ifp = mif6table[mifi].m6_ifp;
			(*ifp->if_ioctl)(ifp, SIOCDELMULTI,
					 (caddr_t)&ifr);
		} else {
			/* Reset register interface */
			if (reg_mif_num != (mifi_t)-1) {
				if_detach(ifp);
				free(ifp, M_DEVBUF, sizeof(*ifp));
				reg_mif_num = (mifi_t)-1;
				reg_mif_idx = 0;
			}
		}
	}
	bzero((caddr_t)mif6table, sizeof(mif6table));
	nummifs = 0;

#ifdef PIM
	pim6 = 0; /* used to stub out/in pim specific code */
#endif

	timeout_del(&expire_upcalls6_ch);

	/*
	 * Free all multicast forwarding cache entries.
	 */
	for (i = 0; i < MF6CTBLSIZ; i++) {
		rt = mf6ctable[i];
		while (rt) {
			struct mf6c *frt;

			for (rte = rt->mf6c_stall; rte != NULL; ) {
				struct rtdetq *n = rte->next;

				m_freem(rte->m);
				free(rte, M_MRTABLE, sizeof(*rte));
				rte = n;
			}
			frt = rt;
			rt = rt->mf6c_next;
			free(frt, M_MRTABLE, sizeof(*frt));
		}
	}

	bzero((caddr_t)mf6ctable, sizeof(mf6ctable));

	ip6_mrouter = NULL;
	ip6_mrouter_ver = 0;

	splx(s);

	return 0;
}

void
ip6_mrouter_detach(struct ifnet *ifp)
{
	struct rtdetq *rte;
	struct mf6c *mfc;
	mifi_t mifi;
	int i;

	/*
	 * Delete a mif which points to ifp.
	 */
	for (mifi = 0; mifi < nummifs; mifi++)
		if (mif6table[mifi].m6_ifp == ifp)
			del_m6if(&mifi);

	/*
	 * Clear rte->ifp of cache entries received on ifp.
	 */
	for (i = 0; i < MF6CTBLSIZ; i++) {
		if (n6expire[i] == 0)
			continue;

		for (mfc = mf6ctable[i]; mfc != NULL; mfc = mfc->mf6c_next) {
			for (rte = mfc->mf6c_stall; rte != NULL; rte = rte->next) {
				if (rte->ifp == ifp)
					rte->ifp = NULL;
			}
		}
	}
}

/*
 * Add a mif to the mif table
 */
int
add_m6if(struct mif6ctl *mifcp)
{
	struct mif6 *mifp;
	struct ifnet *ifp;
	struct in6_ifreq ifr;
	int error, s;

	if (mifcp->mif6c_mifi >= MAXMIFS)
		return EINVAL;
	mifp = mif6table + mifcp->mif6c_mifi;
	if (mifp->m6_ifp)
		return EADDRINUSE; /* XXX: is it appropriate? */

#ifdef PIM
	if (mifcp->mif6c_flags & MIFF_REGISTER) {
		if (reg_mif_num == (mifi_t)-1) {
			ifp = malloc(sizeof(*ifp), M_DEVBUF, M_NOWAIT|M_ZERO);
			if (ifp == NULL)
				return (ENOMEM);
			snprintf(ifp->if_xname, sizeof(ifp->if_xname),
			    "register_mif");
			ifp->if_flags |= IFF_LOOPBACK;
			if_attach(ifp);
			if_alloc_sadl(ifp);

			reg_mif_num = mifcp->mif6c_mifi;
			reg_mif_idx = ifp->if_index;
			mifcp->mif6c_pifi = ifp->if_index;
		}

		ifp = if_get(reg_mif_idx);
		KASSERT(ifp != NULL);
	} else
#endif
	{
		ifp = if_get(mifcp->mif6c_pifi);
		if (ifp == NULL)
			return ENXIO;

		/* Make sure the interface supports multicast */
		if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			if_put(ifp);
			return EOPNOTSUPP;
		}

		s = splsoftnet();

		/*
		 * Enable promiscuous reception of all IPv6 multicasts
		 * from the interface.
		 */
		memset(&ifr, 0, sizeof(ifr));
		ifr.ifr_addr.sin6_family = AF_INET6;
		ifr.ifr_addr.sin6_addr = in6addr_any;
		error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);

		splx(s);
		if (error) {
			if_put(ifp);
			return error;
		}
	}

	s = splsoftnet();

	mifp->m6_flags     = mifcp->mif6c_flags;
	mifp->m6_ifp       = ifp;
#ifdef notyet
	/* scaling up here allows division by 1024 in critical code */
	mifp->m6_rate_limit = mifcp->mif6c_rate_limit * 1024 / 1000;
#endif
	/* initialize per mif pkt counters */
	mifp->m6_pkt_in    = 0;
	mifp->m6_pkt_out   = 0;
	mifp->m6_bytes_in  = 0;
	mifp->m6_bytes_out = 0;
	splx(s);

	/* Adjust nummifs up if the mifi is higher than nummifs */
	if (nummifs <= mifcp->mif6c_mifi)
		nummifs = mifcp->mif6c_mifi + 1;

	if_put(ifp);

	return 0;
}

/*
 * Delete a mif from the mif table
 */
int
del_m6if(mifi_t *mifip)
{
	struct mif6 *mifp = mif6table + *mifip;
	mifi_t mifi;
	struct ifnet *ifp;
	struct in6_ifreq ifr;
	int s;

	if (*mifip >= nummifs)
		return EINVAL;
	if (mifp->m6_ifp == NULL)
		return EINVAL;

	ifp = mifp->m6_ifp;

	s = splsoftnet();
	if (!(mifp->m6_flags & MIFF_REGISTER)) {
		/*
		 * XXX: what if there is yet IPv4 multicast daemon
		 *      using the interface?
		 */
		memset(&ifr, 0, sizeof(ifr));
		ifr.ifr_addr.sin6_family = AF_INET6;
		ifr.ifr_addr.sin6_addr = in6addr_any;
		(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
	} else {
		if (reg_mif_num != (mifi_t)-1) {
			if_detach(ifp);
			free(ifp, M_DEVBUF, sizeof(*ifp));
			reg_mif_num = (mifi_t)-1;
			reg_mif_idx = 0;
		}
	}

	bzero((caddr_t)mifp, sizeof (*mifp));

	/* Adjust nummifs down */
	for (mifi = nummifs; mifi > 0; mifi--)
		if (mif6table[mifi - 1].m6_ifp)
			break;
	nummifs = mifi;

	splx(s);

	return 0;
}

/*
 * Add an mfc entry
 */
int
add_m6fc(struct mf6cctl *mfccp)
{
	struct mf6c *rt;
	u_long hash;
	struct rtdetq *rte;
	u_short nstl;
	char orig[INET6_ADDRSTRLEN], mcast[INET6_ADDRSTRLEN];
	int s;

	MF6CFIND(mfccp->mf6cc_origin.sin6_addr,
		 mfccp->mf6cc_mcastgrp.sin6_addr, rt);

	/* If an entry already exists, just update the fields */
	if (rt) {
		s = splsoftnet();
		rt->mf6c_parent = mfccp->mf6cc_parent;
		rt->mf6c_ifset = mfccp->mf6cc_ifset;
		splx(s);
		return 0;
	}

	/*
	 * Find the entry for which the upcall was made and update
	 */
	s = splsoftnet();

	hash = MF6CHASH(mfccp->mf6cc_origin.sin6_addr,
			mfccp->mf6cc_mcastgrp.sin6_addr);
	for (rt = mf6ctable[hash], nstl = 0; rt; rt = rt->mf6c_next) {
		if (IN6_ARE_ADDR_EQUAL(&rt->mf6c_origin.sin6_addr,
				       &mfccp->mf6cc_origin.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&rt->mf6c_mcastgrp.sin6_addr,
				       &mfccp->mf6cc_mcastgrp.sin6_addr) &&
		    (rt->mf6c_stall != NULL)) {

			if (nstl++) {
				log(LOG_ERR,
				    "add_m6fc: %s o %s g %s p %x dbx %p\n",
				    "multiple kernel entries",
				    inet_ntop(AF_INET6,
					&mfccp->mf6cc_origin.sin6_addr,
					orig, sizeof(orig)),
				    inet_ntop(AF_INET6,
					&mfccp->mf6cc_mcastgrp.sin6_addr,
					mcast, sizeof(mcast)),
				    mfccp->mf6cc_parent, rt->mf6c_stall);
			}

			rt->mf6c_origin     = mfccp->mf6cc_origin;
			rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
			rt->mf6c_parent     = mfccp->mf6cc_parent;
			rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
			/* initialize pkt counters per src-grp */
			rt->mf6c_pkt_cnt    = 0;
			rt->mf6c_byte_cnt   = 0;
			rt->mf6c_wrong_if   = 0;

			rt->mf6c_expire = 0;	/* Don't clean this guy up */
			n6expire[hash]--;

			/* free packets Qed at the end of this entry */
			for (rte = rt->mf6c_stall; rte != NULL; ) {
				struct rtdetq *n = rte->next;
				if (rte->ifp) {
					ip6_mdq(rte->m, rte->ifp, rt);
				}
				m_freem(rte->m);
				free(rte, M_MRTABLE, sizeof(*rte));
				rte = n;
			}
			rt->mf6c_stall = NULL;
		}
	}

	/*
	 * It is possible that an entry is being inserted without an upcall
	 */
	if (nstl == 0) {
		for (rt = mf6ctable[hash]; rt; rt = rt->mf6c_next) {

			if (IN6_ARE_ADDR_EQUAL(&rt->mf6c_origin.sin6_addr,
					       &mfccp->mf6cc_origin.sin6_addr)&&
			    IN6_ARE_ADDR_EQUAL(&rt->mf6c_mcastgrp.sin6_addr,
					       &mfccp->mf6cc_mcastgrp.sin6_addr)) {

				rt->mf6c_origin     = mfccp->mf6cc_origin;
				rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
				rt->mf6c_parent     = mfccp->mf6cc_parent;
				rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
				/* initialize pkt counters per src-grp */
				rt->mf6c_pkt_cnt    = 0;
				rt->mf6c_byte_cnt   = 0;
				rt->mf6c_wrong_if   = 0;

				if (rt->mf6c_expire)
					n6expire[hash]--;
				rt->mf6c_expire	   = 0;
			}
		}
		if (rt == NULL) {
			/* no upcall, so make a new entry */
			rt = malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
			if (rt == NULL) {
				splx(s);
				return ENOBUFS;
			}

			/* insert new entry at head of hash chain */
			rt->mf6c_origin     = mfccp->mf6cc_origin;
			rt->mf6c_mcastgrp   = mfccp->mf6cc_mcastgrp;
			rt->mf6c_parent     = mfccp->mf6cc_parent;
			rt->mf6c_ifset	    = mfccp->mf6cc_ifset;
			/* initialize pkt counters per src-grp */
			rt->mf6c_pkt_cnt    = 0;
			rt->mf6c_byte_cnt   = 0;
			rt->mf6c_wrong_if   = 0;
			rt->mf6c_expire     = 0;
			rt->mf6c_stall = NULL;

			/* link into table */
			rt->mf6c_next  = mf6ctable[hash];
			mf6ctable[hash] = rt;
		}
	}
	splx(s);
	return 0;
}

/*
 * Delete an mfc entry
 */
int
del_m6fc(struct mf6cctl *mfccp)
{
	struct sockaddr_in6	origin;
	struct sockaddr_in6	mcastgrp;
	struct mf6c		*rt;
	struct mf6c		**nptr;
	u_long			hash;
	int s;

	origin = mfccp->mf6cc_origin;
	mcastgrp = mfccp->mf6cc_mcastgrp;
	hash = MF6CHASH(origin.sin6_addr, mcastgrp.sin6_addr);

	s = splsoftnet();

	nptr = &mf6ctable[hash];
	while ((rt = *nptr) != NULL) {
		if (IN6_ARE_ADDR_EQUAL(&origin.sin6_addr,
				       &rt->mf6c_origin.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&mcastgrp.sin6_addr,
				       &rt->mf6c_mcastgrp.sin6_addr) &&
		    rt->mf6c_stall == NULL)
			break;

		nptr = &rt->mf6c_next;
	}
	if (rt == NULL) {
		splx(s);
		return EADDRNOTAVAIL;
	}

	*nptr = rt->mf6c_next;
	free(rt, M_MRTABLE, sizeof(*rt));

	splx(s);

	return 0;
}

int
socket6_send(struct socket *s, struct mbuf *mm, struct sockaddr_in6 *src)
{
	if (s) {
		if (sbappendaddr(&s->so_rcv, sin6tosa(src), mm, NULL) != 0) {
			sorwakeup(s);
			return 0;
		}
	}
	m_freem(mm);
	return -1;
}

/*
 * IPv6 multicast forwarding function. This function assumes that the packet
 * pointed to by "ip6" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IPv6 multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */
int
ip6_mforward(struct ip6_hdr *ip6, struct ifnet *ifp, struct mbuf *m)
{
	struct mf6c *rt;
	struct mif6 *mifp;
	struct mbuf *mm;
	int s;
	mifi_t mifi;
	struct sockaddr_in6 sin6;

	/*
	 * Don't forward a packet with Hop limit of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip6->ip6_hlim <= 1 || IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&ip6->ip6_dst))
		return 0;
	ip6->ip6_hlim--;

	/*
	 * Source address check: do not forward packets with unspecified
	 * source. It was discussed in July 2000, on ipngwg mailing list.
	 * This is rather more serious than unicast cases, because some
	 * MLD packets can be sent with the unspecified source address
	 * (although such packets must normally set 1 to the hop limit field).
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		ip6stat.ip6s_cantforward++;
		if (ip6_log_time + ip6_log_interval < time_second) {
			char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];

			ip6_log_time = time_second;

			inet_ntop(AF_INET6, &ip6->ip6_src, src, sizeof(src));
			inet_ntop(AF_INET6, &ip6->ip6_dst, dst, sizeof(dst));
			log(LOG_DEBUG, "cannot forward "
			    "from %s to %s nxt %d received on interface %u\n",
			    src, dst, ip6->ip6_nxt, m->m_pkthdr.ph_ifidx);
		}
		return 0;
	}

	/*
	 * Determine forwarding mifs from the forwarding cache table
	 */
	s = splsoftnet();
	MF6CFIND(ip6->ip6_src, ip6->ip6_dst, rt);

	/* Entry exists, so forward if necessary */
	if (rt) {
		splx(s);
		return (ip6_mdq(m, ifp, rt));
	} else {
		/*
		 * If we don't have a route for packet's origin,
		 * Make a copy of the packet &
		 * send message to routing daemon
		 */

		struct mbuf *mb0;
		struct rtdetq *rte;
		u_long hash;

		mrt6stat.mrt6s_no_route++;

		/*
		 * Allocate mbufs early so that we don't do extra work if we
		 * are just going to fail anyway.
		 */
		rte = malloc(sizeof(*rte), M_MRTABLE, M_NOWAIT);
		if (rte == NULL) {
			splx(s);
			return ENOBUFS;
		}
		mb0 = m_copym(m, 0, M_COPYALL, M_NOWAIT);
		/*
		 * Pullup packet header if needed before storing it,
		 * as other references may modify it in the meantime.
		 */
		if (mb0 &&
		    (M_READONLY(mb0) || mb0->m_len < sizeof(struct ip6_hdr)))
			mb0 = m_pullup(mb0, sizeof(struct ip6_hdr));
		if (mb0 == NULL) {
			free(rte, M_MRTABLE, sizeof(*rte));
			splx(s);
			return ENOBUFS;
		}

		/* is there an upcall waiting for this packet? */
		hash = MF6CHASH(ip6->ip6_src, ip6->ip6_dst);
		for (rt = mf6ctable[hash]; rt; rt = rt->mf6c_next) {
			if (IN6_ARE_ADDR_EQUAL(&ip6->ip6_src,
					       &rt->mf6c_origin.sin6_addr) &&
			    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
					       &rt->mf6c_mcastgrp.sin6_addr) &&
			    (rt->mf6c_stall != NULL))
				break;
		}

		if (rt == NULL) {
			struct mrt6msg *im;

			/* no upcall, so make a new entry */
			rt = malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
			if (rt == NULL) {
				free(rte, M_MRTABLE, sizeof(*rte));
				m_freem(mb0);
				splx(s);
				return ENOBUFS;
			}
			/*
			 * Make a copy of the header to send to the user
			 * level process
			 */
			mm = m_copym(mb0, 0, sizeof(struct ip6_hdr), M_NOWAIT);

			if (mm == NULL) {
				free(rte, M_MRTABLE, sizeof(*rte));
				m_freem(mb0);
				free(rt, M_MRTABLE, sizeof(*rt));
				splx(s);
				return ENOBUFS;
			}

			/*
			 * Send message to routing daemon
			 */
			(void)memset(&sin6, 0, sizeof(sin6));
			sin6.sin6_len = sizeof(sin6);
			sin6.sin6_family = AF_INET6;
			sin6.sin6_addr = ip6->ip6_src;

			im = NULL;
			switch (ip6_mrouter_ver) {
			case MRT6_INIT:
				im = mtod(mm, struct mrt6msg *);
				im->im6_msgtype = MRT6MSG_NOCACHE;
				im->im6_mbz = 0;
				break;
			default:
				free(rte, M_MRTABLE, sizeof(*rte));
				m_freem(mb0);
				free(rt, M_MRTABLE, sizeof(*rt));
				splx(s);
				return EINVAL;
			}


			for (mifp = mif6table, mifi = 0;
			     mifi < nummifs && mifp->m6_ifp != ifp;
			     mifp++, mifi++)
				;

			switch (ip6_mrouter_ver) {
			case MRT6_INIT:
				im->im6_mif = mifi;
				break;
			}

			if (socket6_send(ip6_mrouter, mm, &sin6) < 0) {
				log(LOG_WARNING, "ip6_mforward: ip6_mrouter "
				    "socket queue full\n");
				mrt6stat.mrt6s_upq_sockfull++;
				free(rte, M_MRTABLE, sizeof(*rte));
				m_freem(mb0);
				free(rt, M_MRTABLE, sizeof(*rt));
				splx(s);
				return ENOBUFS;
			}

			mrt6stat.mrt6s_upcalls++;

			/* insert new entry at head of hash chain */
			bzero(rt, sizeof(*rt));
			rt->mf6c_origin.sin6_family = AF_INET6;
			rt->mf6c_origin.sin6_len = sizeof(struct sockaddr_in6);
			rt->mf6c_origin.sin6_addr = ip6->ip6_src;
			rt->mf6c_mcastgrp.sin6_family = AF_INET6;
			rt->mf6c_mcastgrp.sin6_len = sizeof(struct sockaddr_in6);
			rt->mf6c_mcastgrp.sin6_addr = ip6->ip6_dst;
			rt->mf6c_expire = UPCALL_EXPIRE;
			n6expire[hash]++;
			rt->mf6c_parent = MF6C_INCOMPLETE_PARENT;

			/* link into table */
			rt->mf6c_next  = mf6ctable[hash];
			mf6ctable[hash] = rt;
			/* Add this entry to the end of the queue */
			rt->mf6c_stall = rte;
		} else {
			/* determine if q has overflowed */
			struct rtdetq **p;
			int npkts = 0;

			for (p = &rt->mf6c_stall; *p != NULL; p = &(*p)->next)
				if (++npkts > MAX_UPQ6) {
					mrt6stat.mrt6s_upq_ovflw++;
					free(rte, M_MRTABLE, sizeof(*rte));
					m_freem(mb0);
					splx(s);
					return 0;
				}

			/* Add this entry to the end of the queue */
			*p = rte;
		}

		rte->next = NULL;
		rte->m = mb0;
		rte->ifp = ifp;
		splx(s);

		return 0;
	}
}

/*
 * Clean up cache entries if upcalls are not serviced
 * Call from the Slow Timeout mechanism, every half second.
 */
void
expire_upcalls6(void *unused)
{
	struct rtdetq *rte;
	struct mf6c *mfc, **nptr;
	int i;
	int s;

	s = splsoftnet();

	for (i = 0; i < MF6CTBLSIZ; i++) {
		if (n6expire[i] == 0)
			continue;
		nptr = &mf6ctable[i];
		while ((mfc = *nptr) != NULL) {
			rte = mfc->mf6c_stall;
			/*
			 * Skip real cache entries
			 * Make sure it wasn't marked to not expire (shouldn't happen)
			 * If it expires now
			 */
			if (rte != NULL &&
			    mfc->mf6c_expire != 0 &&
			    --mfc->mf6c_expire == 0) {
				/*
				 * drop all the packets
				 * free the mbuf with the pkt, if, timing info
				 */
				do {
					struct rtdetq *n = rte->next;
					m_freem(rte->m);
					free(rte, M_MRTABLE, sizeof(*rte));
					rte = n;
				} while (rte != NULL);
				mrt6stat.mrt6s_cache_cleanups++;
				n6expire[i]--;

				*nptr = mfc->mf6c_next;
				free(mfc, M_MRTABLE, sizeof(*mfc));
			} else {
				nptr = &mfc->mf6c_next;
			}
		}
	}
	splx(s);
	timeout_set(&expire_upcalls6_ch, expire_upcalls6, NULL);
	timeout_add(&expire_upcalls6_ch, EXPIRE_TIMEOUT);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
int
ip6_mdq(struct mbuf *m, struct ifnet *ifp, struct mf6c *rt)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	mifi_t mifi;
	struct mif6 *mifp;
	int plen = m->m_pkthdr.len;

	/*
	 * Don't forward if it didn't arrive from the parent mif
	 * for its origin.
	 */
	mifi = rt->mf6c_parent;
	if ((mifi >= nummifs) || (mif6table[mifi].m6_ifp != ifp)) {
		/* came in the wrong interface */
		mrt6stat.mrt6s_wrong_if++;
		rt->mf6c_wrong_if++;
#ifdef PIM
		/*
		 * If we are doing PIM processing, and we are forwarding
		 * packets on this interface, send a message to the
		 * routing daemon.
		 */
		/* have to make sure this is a valid mif */
		if (mifi < nummifs && mif6table[mifi].m6_ifp) {
			mifi_t iif;

			if (pim6 && (m->m_flags & M_LOOP) == 0) {
				/*
				 * Check the M_LOOP flag to avoid an
				 * unnecessary PIM assert.
				 * XXX: M_LOOP is an ad-hoc hack...
				 */
				struct sockaddr_in6 sin6;

				struct mbuf *mm;
				struct mrt6msg *im;

				mm = m_copym(m, 0, sizeof(struct ip6_hdr),
				    M_NOWAIT);
				if (mm &&
				    (M_READONLY(mm) ||
				     mm->m_len < sizeof(struct ip6_hdr)))
					mm = m_pullup(mm, sizeof(struct ip6_hdr));
				if (mm == NULL)
					return ENOBUFS;

				im = NULL;
				switch (ip6_mrouter_ver) {
				case MRT6_INIT:
					im = mtod(mm, struct mrt6msg *);
					im->im6_msgtype = MRT6MSG_WRONGMIF;
					im->im6_mbz = 0;
					break;
				default:
					m_freem(mm);
					return EINVAL;
				}

				for (mifp = mif6table, iif = 0;
				     iif < nummifs && mifp &&
					     mifp->m6_ifp != ifp;
				     mifp++, iif++)
					;

				(void)memset(&sin6, 0, sizeof(sin6));
				sin6.sin6_len = sizeof(sin6);
				sin6.sin6_family = AF_INET6;
				switch (ip6_mrouter_ver) {
				case MRT6_INIT:
					im->im6_mif = iif;
					sin6.sin6_addr = im->im6_src;
					break;
				}

				mrt6stat.mrt6s_upcalls++;

				if (socket6_send(ip6_mrouter, mm, &sin6) < 0) {
					++mrt6stat.mrt6s_upq_sockfull;
					return ENOBUFS;
				}
			}
		}
#endif /* PIM */
		return 0;
	}			/* if wrong iif */

	/* If I sourced this packet, it counts as output, else it was input. */
	if (m->m_pkthdr.ph_ifidx == 0) {
		/* XXX: is ph_ifidx really 0 when output?? */
		mif6table[mifi].m6_pkt_out++;
		mif6table[mifi].m6_bytes_out += plen;
	} else {
		mif6table[mifi].m6_pkt_in++;
		mif6table[mifi].m6_bytes_in += plen;
	}
	rt->mf6c_pkt_cnt++;
	rt->mf6c_byte_cnt += plen;

	/*
	 * For each mif, forward a copy of the packet if there are group
	 * members downstream on the interface.
	 */
	for (mifp = mif6table, mifi = 0; mifi < nummifs; mifp++, mifi++) {
		if (IF_ISSET(mifi, &rt->mf6c_ifset)) {
			if (mif6table[mifi].m6_ifp == NULL)
				continue;

			/*
			 * check if the outgoing packet is going to break
			 * a scope boundary.
			 * XXX For packets through PIM register tunnel
			 * interface, we believe a routing daemon.
			 */
			if ((mif6table[rt->mf6c_parent].m6_flags &
			     MIFF_REGISTER) == 0 &&
			    (mif6table[mifi].m6_flags & MIFF_REGISTER) == 0 &&
			    (in6_addr2scopeid(ifp->if_index, &ip6->ip6_dst) !=
			     in6_addr2scopeid(mif6table[mifi].m6_ifp->if_index,
					      &ip6->ip6_dst) ||
			     in6_addr2scopeid(ifp->if_index, &ip6->ip6_src) !=
			     in6_addr2scopeid(mif6table[mifi].m6_ifp->if_index,
					      &ip6->ip6_src))) {
				ip6stat.ip6s_badscope++;
				continue;
			}

			mifp->m6_pkt_out++;
			mifp->m6_bytes_out += plen;
#ifdef PIM
			if (mifp->m6_flags & MIFF_REGISTER)
			    register_send(ip6, mifp, m);
			else
#endif
			    phyint_send6(ip6, mifp, m);
		}
	}
	return 0;
}

void
phyint_send6(struct ip6_hdr *ip6, struct mif6 *mifp, struct mbuf *m)
{
	struct mbuf *mb_copy;
	struct ifnet *ifp = mifp->m6_ifp;
	int error = 0;
	int s = splsoftnet();
	static struct route_in6 ro;
	struct sockaddr_in6 *dst6;

	/*
	 * Make a new reference to the packet; make sure that
	 * the IPv6 header is actually copied, not just referenced,
	 * so that ip6_output() only scribbles on the copy.
	 */
	mb_copy = m_copym(m, 0, M_COPYALL, M_NOWAIT);
	if (mb_copy &&
	    (M_READONLY(mb_copy) || mb_copy->m_len < sizeof(struct ip6_hdr)))
		mb_copy = m_pullup(mb_copy, sizeof(struct ip6_hdr));
	if (mb_copy == NULL) {
		splx(s);
		return;
	}
	/* set MCAST flag to the outgoing packet */
	mb_copy->m_flags |= M_MCAST;

	/*
	 * If we sourced the packet, call ip6_output since we may devide
	 * the packet into fragments when the packet is too big for the
	 * outgoing interface.
	 * Otherwise, we can simply send the packet to the interface
	 * sending queue.
	 */
	if (m->m_pkthdr.ph_ifidx == 0) {
		struct ip6_moptions im6o;

		im6o.im6o_ifidx = ifp->if_index;
		/* XXX: ip6_output will override ip6->ip6_hlim */
		im6o.im6o_hlim = ip6->ip6_hlim;
		im6o.im6o_loop = 1;
		error = ip6_output(mb_copy, NULL, &ro, IPV6_FORWARDING, &im6o,
		    NULL);
		splx(s);
		return;
	}

	/*
	 * If we belong to the destination multicast group
	 * on the outgoing interface, loop back a copy.
	 */
	dst6 = &ro.ro_dst;
	if (in6_hasmulti(&ip6->ip6_dst, ifp)) {
		dst6->sin6_len = sizeof(struct sockaddr_in6);
		dst6->sin6_family = AF_INET6;
		dst6->sin6_addr = ip6->ip6_dst;
		ip6_mloopback(ifp, m, &ro.ro_dst);
	}
	/*
	 * Put the packet into the sending queue of the outgoing interface
	 * if it would fit in the MTU of the interface.
	 */
	if (mb_copy->m_pkthdr.len <= ifp->if_mtu || ifp->if_mtu < IPV6_MMTU) {
		dst6->sin6_len = sizeof(struct sockaddr_in6);
		dst6->sin6_family = AF_INET6;
		dst6->sin6_addr = ip6->ip6_dst;
		error = ifp->if_output(ifp, mb_copy, sin6tosa(&ro.ro_dst),
		    NULL);
	} else {
		if (ip6_mcast_pmtu)
			icmp6_error(mb_copy, ICMP6_PACKET_TOO_BIG, 0,
			    ifp->if_mtu);
		else {
			m_freem(mb_copy); /* simply discard the packet */
		}
	}

	splx(s);
}

#ifdef PIM
int
register_send(struct ip6_hdr *ip6, struct mif6 *mif, struct mbuf *m)
{
	struct mbuf *mm;
	int i, len = m->m_pkthdr.len;
	struct sockaddr_in6 sin6;
	struct mrt6msg *im6;

	++pim6stat.pim6s_snd_registers;

	/* Make a copy of the packet to send to the user level process */
	MGETHDR(mm, M_DONTWAIT, MT_HEADER);
	if (mm == NULL)
		return ENOBUFS;
	mm->m_data += max_linkhdr;
	mm->m_len = sizeof(struct ip6_hdr);

	if ((mm->m_next = m_copym(m, 0, M_COPYALL, M_NOWAIT)) == NULL) {
		m_freem(mm);
		return ENOBUFS;
	}
	i = MHLEN - M_LEADINGSPACE(mm);
	if (i > len)
		i = len;
	mm = m_pullup(mm, i);
	if (mm == NULL)
		return ENOBUFS;
/* TODO: check it! */
	mm->m_pkthdr.len = len + sizeof(struct ip6_hdr);

	/*
	 * Send message to routing daemon
	 */
	(void)memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = ip6->ip6_src;

	im6 = mtod(mm, struct mrt6msg *);
	im6->im6_msgtype      = MRT6MSG_WHOLEPKT;
	im6->im6_mbz          = 0;

	im6->im6_mif = mif - mif6table;

	/* iif info is not given for reg. encap.n */
	mrt6stat.mrt6s_upcalls++;

	if (socket6_send(ip6_mrouter, mm, &sin6) < 0) {
		++mrt6stat.mrt6s_upq_sockfull;
		return ENOBUFS;
	}
	return 0;
}

/*
 * PIM sparse mode hook
 * Receives the pim control messages, and passes them up to the listening
 * socket, using rip6_input.
 * The only message processed is the REGISTER pim message; the pim header
 * is stripped off, and the inner packet is passed to register_mforward.
 */
int
pim6_input(struct mbuf **mp, int *offp, int proto)
{
	struct pim *pim; /* pointer to a pim struct */
	struct ip6_hdr *ip6;
	int pimlen;
	struct mbuf *m = *mp;
	int minlen;
	int off = *offp;

	++pim6stat.pim6s_rcv_total;

	ip6 = mtod(m, struct ip6_hdr *);
	pimlen = m->m_pkthdr.len - *offp;

	/*
	 * Validate lengths
	 */
	if (pimlen < PIM_MINLEN) {
		++pim6stat.pim6s_rcv_tooshort;
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/*
	 * if the packet is at least as big as a REGISTER, go ahead
	 * and grab the PIM REGISTER header size, to avoid another
	 * possible m_pullup() later.
	 *
	 * PIM_MINLEN       == pimhdr + u_int32 == 8
	 * PIM6_REG_MINLEN   == pimhdr + reghdr + eip6hdr == 4 + 4 + 40
	 */
	minlen = (pimlen >= PIM6_REG_MINLEN) ? PIM6_REG_MINLEN : PIM_MINLEN;

	/*
	 * Make sure that the IP6 and PIM headers in contiguous memory, and
	 * possibly the PIM REGISTER header
	 */
	IP6_EXTHDR_GET(pim, struct pim *, m, off, minlen);
	if (pim == NULL) {
		pim6stat.pim6s_rcv_tooshort++;
		return IPPROTO_DONE;
	}

	/* PIM version check */
	if (pim->pim_ver != PIM_VERSION) {
		++pim6stat.pim6s_rcv_badversion;
		m_freem(m);
		return (IPPROTO_DONE);
	}

#define PIM6_CHECKSUM
#ifdef PIM6_CHECKSUM
	{
		int cksumlen;

		/*
		 * Validate checksum.
		 * If PIM REGISTER, exclude the data packet
		 */
		if (pim->pim_type == PIM_REGISTER)
			cksumlen = PIM_MINLEN;
		else
			cksumlen = pimlen;

		if (in6_cksum(m, IPPROTO_PIM, off, cksumlen)) {
			++pim6stat.pim6s_rcv_badsum;
			m_freem(m);
			return (IPPROTO_DONE);
		}
	}
#endif /* PIM_CHECKSUM */

	if (pim->pim_type == PIM_REGISTER) {
		/*
		 * since this is a REGISTER, we'll make a copy of the register
		 * headers ip6+pim+u_int32_t+encap_ip6, to be passed up to the
		 * routing daemon.
		 */
		static struct sockaddr_in6 dst = { sizeof(dst), AF_INET6 };

		struct mbuf *mcp;
		struct ip6_hdr *eip6;
		u_int32_t *reghdr;

		++pim6stat.pim6s_rcv_registers;

		if ((reg_mif_num >= nummifs) || (reg_mif_num == (mifi_t) -1)) {
			m_freem(m);
			return (IPPROTO_DONE);
		}

		reghdr = (u_int32_t *)(pim + 1);

		if ((ntohl(*reghdr) & PIM_NULL_REGISTER))
			goto pim6_input_to_daemon;

		/*
		 * Validate length
		 */
		if (pimlen < PIM6_REG_MINLEN) {
			++pim6stat.pim6s_rcv_tooshort;
			++pim6stat.pim6s_rcv_badregisters;
			m_freem(m);
			return (IPPROTO_DONE);
		}

		eip6 = (struct ip6_hdr *) (reghdr + 1);
		/* verify the version number of the inner packet */
		if ((eip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
			++pim6stat.pim6s_rcv_badregisters;
			m_freem(m);
			return (IPPROTO_NONE);
		}

		/* verify the inner packet is destined to a mcast group */
		if (!IN6_IS_ADDR_MULTICAST(&eip6->ip6_dst)) {
			++pim6stat.pim6s_rcv_badregisters;
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/*
		 * make a copy of the whole header to pass to the daemon later.
		 */
		mcp = m_copym(m, 0, off + PIM6_REG_MINLEN, M_NOWAIT);
		if (mcp == NULL) {
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/*
		 * forward the inner ip6 packet; point m_data at the inner ip6.
		 */
		m_adj(m, off + PIM_MINLEN);
		if_input_local(mif6table[reg_mif_num].m6_ifp, m,
		    dst.sin6_family);

		/* prepare the register head to send to the mrouting daemon */
		m = mcp;
	}

	/*
	 * Pass the PIM message up to the daemon; if it is a register message
	 * pass the 'head' only up to the daemon. This includes the
	 * encapsulator ip6 header, pim header, register header and the
	 * encapsulated ip6 header.
	 */
  pim6_input_to_daemon:
	rip6_input(&m, offp, proto);
	return (IPPROTO_DONE);
}

/*
 * Sysctl for pim6 variables.
 */
int
pim6_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case PIM6CTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &pim6stat, sizeof(pim6stat)));

	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
#endif /* PIM */

u_int32_t
_mf6chash(const struct in6_addr *a, const struct in6_addr *g)
{
	SIPHASH_CTX ctx;

	SipHash24_Init(&ctx, &mf6chashkey);
	SipHash24_Update(&ctx, a, sizeof(*a));
	SipHash24_Update(&ctx, g, sizeof(*g));

	return (MF6CHASHMOD(SipHash24_End(&ctx)));
}
