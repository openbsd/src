/*	$OpenBSD: ip_mroute.c,v 1.105 2017/01/06 14:01:19 rzalamena Exp $	*/
/*	$NetBSD: ip_mroute.c,v 1.85 2004/04/26 01:31:57 matt Exp $	*/

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
 * Modified by Charles M. Hannum, NetBSD, May 1995.
 * Modified by Ahmed Helmy, SGI, June 1996
 * Modified by George Edmond Eddy (Rusty), ISI, February 1998
 * Modified by Pavlin Radoslavov, USC/ISI, May 1998, August 1999, October 2000
 * Modified by Hitoshi Asaeda, WIDE, August 2000
 * Modified by Pavlin Radoslavov, ICSI, October 2002
 *
 * MROUTING Revision: 1.2
 * advanced API support, bandwidth metering and signaling
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/timeout.h>

#include <crypto/siphash.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_mroute.h>

/*
 * Globals.  All but ip_mrouter and ip_mrtproto could be static,
 * except for netstat or debugging purposes.
 */
struct socket	*ip_mrouter[RT_TABLEID_MAX];
int		ip_mrtproto = IGMP_DVMRP;    /* for netstat only */

#define NO_RTE_FOUND	0x1
#define RTE_FOUND	0x2

u_int32_t _mfchash(unsigned int, struct in_addr, struct in_addr);

#define	MFCHASH(r, a, g) _mfchash((r), (a), (g))
LIST_HEAD(mfchashhdr, mfc) *mfchashtbl[RT_TABLEID_MAX];
u_long	mfchash[RT_TABLEID_MAX];
SIPHASH_KEY mfchashkey[RT_TABLEID_MAX];

u_char		nexpire[RT_TABLEID_MAX][MFCTBLSIZ];
struct mrtstat	mrtstat;

#define		EXPIRE_TIMEOUT	250		/* 4x / second */
#define		UPCALL_EXPIRE	6		/* number of timeouts */
struct timeout	expire_upcalls_ch[RT_TABLEID_MAX];

int get_sg_cnt(unsigned int, struct sioc_sg_req *);
int get_vif_cnt(unsigned int, struct sioc_vif_req *);
int ip_mrouter_init(struct socket *, struct mbuf *);
int get_version(struct mbuf *);
int add_vif(struct socket *, struct mbuf *);
int del_vif(struct socket *, struct mbuf *);
void update_mfc_params(struct mfc *, struct mfcctl2 *);
void init_mfc_params(struct mfc *, struct mfcctl2 *);
void expire_mfc(struct mfc *);
int add_mfc(struct socket *, struct mbuf *);
int del_mfc(struct socket *, struct mbuf *);
int set_api_config(struct socket *, struct mbuf *); /* chose API capabilities */
int get_api_support(struct mbuf *);
int get_api_config(struct mbuf *);
int socket_send(struct socket *, struct mbuf *,
			    struct sockaddr_in *);
void expire_upcalls(void *);
int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *);
struct ifnet *if_lookupbyvif(vifi_t, unsigned int);

/*
 * Rate limit for assert notification messages, in usec
 */
#define ASSERT_MSG_TIME		3000000

/*
 * Kernel multicast routing API capabilities and setup.
 * If more API capabilities are added to the kernel, they should be
 * recorded in `mrt_api_support'.
 */
static const u_int32_t mrt_api_support = (MRT_MFC_FLAGS_DISABLE_WRONGVIF |
					  MRT_MFC_RP);
static u_int32_t mrt_api_config = 0;

/*
 * Find a route for a given origin IP address and Multicast group address
 * Type of service parameter to be added in the future!!!
 * Statistics are updated by the caller if needed
 * (mrtstat.mrts_mfc_lookups and mrtstat.mrts_mfc_misses)
 */
static struct mfc *
mfc_find(unsigned int rtableid, struct in_addr *o, struct in_addr *g)
{
	struct mfc *rt;
	u_int32_t hash;

	if (mfchashtbl[rtableid] == NULL)
		return (NULL);

	hash = MFCHASH(rtableid, *o, *g);
	LIST_FOREACH(rt, &mfchashtbl[rtableid][hash], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, *o) &&
		    in_hosteq(rt->mfc_mcastgrp, *g) &&
		    (rt->mfc_stall == NULL))
			break;
	}

	return (rt);
}

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) do {					\
	int xxs;							\
	delta = (a).tv_usec - (b).tv_usec;				\
	xxs = (a).tv_sec - (b).tv_sec;					\
	switch (xxs) {							\
	case 2:								\
		delta += 1000000;					\
		/* FALLTHROUGH */					\
	case 1:								\
		delta += 1000000;					\
		/* FALLTHROUGH */					\
	case 0:								\
		break;							\
	default:							\
		delta += (1000000 * xxs);				\
		break;							\
	}								\
} while (/*CONSTCOND*/ 0)

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
int
ip_mrouter_set(struct socket *so, int optname, struct mbuf **mp)
{
	struct inpcb *inp = sotoinpcb(so);
	int error;

	if (optname != MRT_INIT &&
	    so != ip_mrouter[inp->inp_rtableid])
		error = ENOPROTOOPT;
	else
		switch (optname) {
		case MRT_INIT:
			error = ip_mrouter_init(so, *mp);
			break;
		case MRT_DONE:
			error = ip_mrouter_done(so);
			break;
		case MRT_ADD_VIF:
			error = add_vif(so, *mp);
			break;
		case MRT_DEL_VIF:
			error = del_vif(so, *mp);
			break;
		case MRT_ADD_MFC:
			error = add_mfc(so, *mp);
			break;
		case MRT_DEL_MFC:
			error = del_mfc(so, *mp);
			break;
		case MRT_API_CONFIG:
			error = set_api_config(so, *mp);
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}

	m_free(*mp);
	return (error);
}

/*
 * Handle MRT getsockopt commands
 */
int
ip_mrouter_get(struct socket *so, int optname, struct mbuf **mp)
{
	struct inpcb *inp = sotoinpcb(so);
	int error;

	if (so != ip_mrouter[inp->inp_rtableid])
		error = ENOPROTOOPT;
	else {
		*mp = m_get(M_WAIT, MT_SOOPTS);

		switch (optname) {
		case MRT_VERSION:
			error = get_version(*mp);
			break;
		case MRT_API_SUPPORT:
			error = get_api_support(*mp);
			break;
		case MRT_API_CONFIG:
			error = get_api_config(*mp);
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}

		if (error)
			m_free(*mp);
	}

	return (error);
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
mrt_ioctl(struct socket *so, u_long cmd, caddr_t data)
{
	struct inpcb *inp = sotoinpcb(so);
	int error;

	if (so != ip_mrouter[inp->inp_rtableid])
		error = EINVAL;
	else
		switch (cmd) {
		case SIOCGETVIFCNT:
			error = get_vif_cnt(inp->inp_rtableid,
			    (struct sioc_vif_req *)data);
			break;
		case SIOCGETSGCNT:
			error = get_sg_cnt(inp->inp_rtableid,
			    (struct sioc_sg_req *)data);
			break;
		default:
			error = ENOTTY;
			break;
		}

	return (error);
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
int
get_sg_cnt(unsigned int rtableid, struct sioc_sg_req *req)
{
	struct mfc *rt;

	rt = mfc_find(rtableid, &req->src, &req->grp);
	if (rt == NULL) {
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
		return (EADDRNOTAVAIL);
	}
	req->pktcnt = rt->mfc_pkt_cnt;
	req->bytecnt = rt->mfc_byte_cnt;
	req->wrong_if = rt->mfc_wrong_if;

	return (0);
}

/*
 * returns the input and output packet and byte counts on the vif provided
 */
int
get_vif_cnt(unsigned int rtableid, struct sioc_vif_req *req)
{
	struct ifnet	*ifp;
	struct vif	*v;
	vifi_t		 vifi = req->vifi;

	if ((ifp = if_lookupbyvif(vifi, rtableid)) == NULL)
		return (EINVAL);

	v = (struct vif *)ifp->if_mcast;
	req->icount = v->v_pkt_in;
	req->ocount = v->v_pkt_out;
	req->ibytes = v->v_bytes_in;
	req->obytes = v->v_bytes_out;

	return (0);
}

int
mrt_sysctl_vif(void *oldp, size_t *oldlenp)
{
	caddr_t where = oldp;
	size_t needed, given;
	struct ifnet *ifp;
	struct vif *vifp;
	struct vifinfo vinfo;

	given = *oldlenp;
	needed = 0;
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if ((vifp = (struct vif *)ifp->if_mcast) == NULL)
			continue;

		vinfo.v_vifi = vifp->v_id;
		vinfo.v_flags = vifp->v_flags;
		vinfo.v_threshold = vifp->v_threshold;
		vinfo.v_lcl_addr = vifp->v_lcl_addr;
		vinfo.v_rmt_addr = vifp->v_rmt_addr;
		vinfo.v_pkt_in = vifp->v_pkt_in;
		vinfo.v_pkt_out = vifp->v_pkt_out;
		vinfo.v_bytes_in = vifp->v_bytes_in;
		vinfo.v_bytes_out = vifp->v_bytes_out;

		needed += sizeof(vinfo);
		if (where && needed <= given) { 
			int error;

			error = copyout(&vinfo, where, sizeof(vinfo));
			if (error)
				return (error);
			where += sizeof(vinfo);
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
mrt_sysctl_mfc(void *oldp, size_t *oldlenp)
{
	caddr_t where = oldp;
	size_t needed, given;
	u_long i;
	unsigned int rtableid;
	struct mfc *m;
	struct mfcinfo minfo;

	given = *oldlenp;
	needed = 0;
	for (rtableid = 0; rtableid < RT_TABLEID_MAX; rtableid++) {
		if (mfchashtbl[rtableid] == NULL)
			continue;

		for (i = 0; i < MFCTBLSIZ; ++i) {
			LIST_FOREACH(m, &mfchashtbl[rtableid][i], mfc_hash) {
				minfo.mfc_origin = m->mfc_origin;
				minfo.mfc_mcastgrp = m->mfc_mcastgrp;
				minfo.mfc_parent = m->mfc_parent;
				minfo.mfc_pkt_cnt = m->mfc_pkt_cnt;
				minfo.mfc_byte_cnt = m->mfc_byte_cnt;
				memcpy(minfo.mfc_ttls, m->mfc_ttls, MAXVIFS);

				needed += sizeof(minfo);
				if (where && needed <= given) {
					int error;

					error = copyout(&minfo, where,
					    sizeof(minfo));
					if (error)
						return (error);
					where += sizeof(minfo);
				}
			}
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

/*
 * Enable multicast routing
 */
int
ip_mrouter_init(struct socket *so, struct mbuf *m)
{
	struct inpcb *inp = sotoinpcb(so);
	unsigned int rtableid = inp->inp_rtableid;
	int *v;

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_IGMP)
		return (EOPNOTSUPP);

	if (m == NULL || m->m_len < sizeof(int))
		return (EINVAL);

	v = mtod(m, int *);
	if (*v != 1)
		return (EINVAL);

	if (ip_mrouter[rtableid] != NULL)
		return (EADDRINUSE);

	ip_mrouter[rtableid] = so;

	mfchashtbl[rtableid] =
	    hashinit(MFCTBLSIZ, M_MRTABLE, M_WAITOK, &mfchash[rtableid]);
	arc4random_buf(&mfchashkey[rtableid], sizeof(mfchashkey[rtableid]));
	memset(nexpire[rtableid], 0, sizeof(nexpire[rtableid]));

	timeout_set_proc(&expire_upcalls_ch[rtableid], expire_upcalls,
	    &inp->inp_rtableid);
	timeout_add_msec(&expire_upcalls_ch[rtableid], EXPIRE_TIMEOUT);

	return (0);
}

u_int32_t
_mfchash(unsigned int rtableid, struct in_addr o, struct in_addr g)
{
	SIPHASH_CTX ctx;

	SipHash24_Init(&ctx, &mfchashkey[rtableid]);
	SipHash24_Update(&ctx, &o.s_addr, sizeof(o.s_addr));
	SipHash24_Update(&ctx, &g.s_addr, sizeof(g.s_addr));

	return (SipHash24_End(&ctx) & mfchash[rtableid]);
}

/*
 * Disable multicast routing
 */
int
ip_mrouter_done(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);
	struct ifnet *ifp;
	int i;
	unsigned int rtableid = inp->inp_rtableid;

	splsoftassert(IPL_SOFTNET);

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rtableid)
			continue;

		vif_delete(ifp);
	}

	mrt_api_config = 0;

	timeout_del(&expire_upcalls_ch[rtableid]);

	/*
	 * Free all multicast forwarding cache entries.
	 */
	for (i = 0; i < MFCTBLSIZ; i++) {
		struct mfc *rt, *nrt;

		for (rt = LIST_FIRST(&mfchashtbl[rtableid][i]); rt; rt = nrt) {
			nrt = LIST_NEXT(rt, mfc_hash);

			expire_mfc(rt);
		}
	}

	memset(nexpire[rtableid], 0, sizeof(nexpire[rtableid]));
	hashfree(mfchashtbl[rtableid], MFCTBLSIZ, M_MRTABLE);
	mfchashtbl[rtableid] = NULL;

	ip_mrouter[rtableid] = NULL;

	return (0);
}

int
get_version(struct mbuf *m)
{
	int *v = mtod(m, int *);

	*v = 0x0305;	/* XXX !!!! */
	m->m_len = sizeof(int);
	return (0);
}

/*
 * Configure API capabilities
 */
int
set_api_config(struct socket *so, struct mbuf *m)
{
	struct inpcb *inp = sotoinpcb(so);
	struct ifnet *ifp;
	int i;
	u_int32_t *apival;
	unsigned int rtableid = inp->inp_rtableid;

	if (m == NULL || m->m_len < sizeof(u_int32_t))
		return (EINVAL);

	apival = mtod(m, u_int32_t *);

	/*
	 * We can set the API capabilities only if it is the first operation
	 * after MRT_INIT. I.e.:
	 *  - there are no vifs installed
	 *  - the MFC table is empty
	 */
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rtableid)
			continue;
		if (ifp->if_mcast == NULL)
			continue;

		*apival = 0;
		return (EPERM);
	}
	for (i = 0; i < MFCTBLSIZ; i++) {
		if (mfchashtbl[rtableid] == NULL)
			break;

		if (LIST_FIRST(&mfchashtbl[rtableid][i]) != NULL) {
			*apival = 0;
			return (EPERM);
		}
	}

	mrt_api_config = *apival & mrt_api_support;
	*apival = mrt_api_config;

	return (0);
}

/*
 * Get API capabilities
 */
int
get_api_support(struct mbuf *m)
{
	u_int32_t *apival;

	if (m == NULL || m->m_len < sizeof(u_int32_t))
		return (EINVAL);

	apival = mtod(m, u_int32_t *);

	*apival = mrt_api_support;

	return (0);
}

/*
 * Get API configured capabilities
 */
int
get_api_config(struct mbuf *m)
{
	u_int32_t *apival;

	if (m == NULL || m->m_len < sizeof(u_int32_t))
		return (EINVAL);

	apival = mtod(m, u_int32_t *);

	*apival = mrt_api_config;

	return (0);
}

static struct sockaddr_in sin = { sizeof(sin), AF_INET };

int
add_vif(struct socket *so, struct mbuf *m)
{
	struct inpcb *inp = sotoinpcb(so);
	struct vifctl *vifcp;
	struct vif *vifp;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	struct ifreq ifr;
	int error;
	unsigned int rtableid = inp->inp_rtableid;

	splsoftassert(IPL_SOFTNET);

	if (m == NULL || m->m_len < sizeof(struct vifctl))
		return (EINVAL);

	vifcp = mtod(m, struct vifctl *);
	if (vifcp->vifc_vifi >= MAXVIFS)
		return (EINVAL);
	if (in_nullhost(vifcp->vifc_lcl_addr))
		return (EADDRNOTAVAIL);
	if (if_lookupbyvif(vifcp->vifc_vifi, rtableid) != NULL)
		return (EADDRINUSE);

	/* Tunnels are no longer supported use gif(4) instead. */
	if (vifcp->vifc_flags & VIFF_TUNNEL)
		return (EOPNOTSUPP);
	{
		sin.sin_addr = vifcp->vifc_lcl_addr;
		ifa = ifa_ifwithaddr(sintosa(&sin), rtableid);
		if (ifa == NULL)
			return (EADDRNOTAVAIL);
	}

	/* Use the physical interface associated with the address. */
	ifp = ifa->ifa_ifp;
	if (ifp->if_mcast != NULL)
		return (EADDRINUSE);

	{
		/* Make sure the interface supports multicast. */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return (EOPNOTSUPP);

		/* Enable promiscuous reception of all IP multicasts. */
		memset(&ifr, 0, sizeof(ifr));
		satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = zeroin_addr;
		error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
		if (error)
			return (error);
	}

	vifp = malloc(sizeof(*vifp), M_MRTABLE, M_WAITOK | M_ZERO);
	ifp->if_mcast = (caddr_t)vifp;

	vifp->v_id = vifcp->vifc_vifi;
	vifp->v_flags = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	vifp->v_lcl_addr = vifcp->vifc_lcl_addr;
	vifp->v_rmt_addr = vifcp->vifc_rmt_addr;

	return (0);
}

int
del_vif(struct socket *so, struct mbuf *m)
{
	struct inpcb *inp = sotoinpcb(so);
	struct ifnet *ifp;
	vifi_t *vifip;
	unsigned int rtableid = inp->inp_rtableid;

	splsoftassert(IPL_SOFTNET);

	if (m == NULL || m->m_len < sizeof(vifi_t))
		return (EINVAL);

	vifip = mtod(m, vifi_t *);
	if ((ifp = if_lookupbyvif(*vifip, rtableid)) == NULL)
		return (EADDRNOTAVAIL);

	vif_delete(ifp);
	return (0);
}

void
vif_delete(struct ifnet *ifp)
{
	struct vif	*v;
	struct ifreq	 ifr;

	if ((v = (struct vif *)ifp->if_mcast) == NULL)
		return;

	ifp->if_mcast = NULL;

	memset(&ifr, 0, sizeof(ifr));
	satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
	satosin(&ifr.ifr_addr)->sin_family = AF_INET;
	satosin(&ifr.ifr_addr)->sin_addr = zeroin_addr;
	(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);

	free(v, M_MRTABLE, sizeof(*v));
}

/*
 * update an mfc entry without resetting counters and S,G addresses.
 */
void
update_mfc_params(struct mfc *rt, struct mfcctl2 *mfccp)
{
	int i;

	rt->mfc_parent = mfccp->mfcc_parent;
	for (i = 0; i < MAXVIFS; i++) {
		rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
		rt->mfc_flags[i] = mfccp->mfcc_flags[i] & mrt_api_config &
		    MRT_MFC_FLAGS_ALL;
	}
	/* set the RP address */
	if (mrt_api_config & MRT_MFC_RP)
		rt->mfc_rp = mfccp->mfcc_rp;
	else
		rt->mfc_rp = zeroin_addr;
}

/*
 * fully initialize an mfc entry from the parameter.
 */
void
init_mfc_params(struct mfc *rt, struct mfcctl2 *mfccp)
{
	rt->mfc_origin     = mfccp->mfcc_origin;
	rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;

	update_mfc_params(rt, mfccp);

	/* initialize pkt counters per src-grp */
	rt->mfc_pkt_cnt    = 0;
	rt->mfc_byte_cnt   = 0;
	rt->mfc_wrong_if   = 0;
	timerclear(&rt->mfc_last_assert);
}

void
expire_mfc(struct mfc *rt)
{
	struct rtdetq *rte, *nrte;

	for (rte = rt->mfc_stall; rte != NULL; rte = nrte) {
		nrte = rte->next;
		m_freem(rte->m);
		free(rte, M_MRTABLE, 0);
	}

	LIST_REMOVE(rt, mfc_hash);
	free(rt, M_MRTABLE, 0);
}

/*
 * Add an mfc entry
 */
int
add_mfc(struct socket *so, struct mbuf *m)
{
	struct inpcb *inp = sotoinpcb(so);
	struct mfcctl2 mfcctl2;
	struct mfcctl2 *mfccp;
	struct mfc *rt;
	u_int32_t hash = 0;
	struct rtdetq *rte, *nrte;
	u_short nstl;
	int mfcctl_size = sizeof(struct mfcctl);
	unsigned int rtableid = inp->inp_rtableid;

	splsoftassert(IPL_SOFTNET);

	if (mrt_api_config & MRT_API_FLAGS_ALL)
		mfcctl_size = sizeof(struct mfcctl2);

	if (m == NULL || m->m_len < mfcctl_size)
		return (EINVAL);

	/*
	 * select data size depending on API version.
	 */
	if (mrt_api_config & MRT_API_FLAGS_ALL) {
		struct mfcctl2 *mp2 = mtod(m, struct mfcctl2 *);
		bcopy(mp2, (caddr_t)&mfcctl2, sizeof(*mp2));
	} else {
		struct mfcctl *mp = mtod(m, struct mfcctl *);
		bcopy(mp, (caddr_t)&mfcctl2, sizeof(*mp));
		memset((caddr_t)&mfcctl2 + sizeof(struct mfcctl), 0,
		    sizeof(mfcctl2) - sizeof(struct mfcctl));
	}
	mfccp = &mfcctl2;

	/* No hash table allocated for this. */
	if (mfchashtbl[rtableid] == NULL)
		return (0);

	rt = mfc_find(rtableid, &mfccp->mfcc_origin, &mfccp->mfcc_mcastgrp);

	/* If an entry already exists, just update the fields */
	if (rt) {
		update_mfc_params(rt, mfccp);
		return (0);
	}

	/*
	 * Find the entry for which the upcall was made and update
	 */
	nstl = 0;
	hash = MFCHASH(rtableid, mfccp->mfcc_origin, mfccp->mfcc_mcastgrp);
	LIST_FOREACH(rt, &mfchashtbl[rtableid][hash], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
		    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp) &&
		    rt->mfc_stall != NULL) {
			if (nstl++) {
				log(LOG_ERR, "add_mfc %s o %x g %x "
				    "p %x dbx %p\n",
				    "multiple kernel entries",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);
			}

			rte = rt->mfc_stall;
			init_mfc_params(rt, mfccp);
			rt->mfc_stall = NULL;

			rt->mfc_expire = 0; /* Don't clean this guy up */
			nexpire[rtableid][hash]--;

			/* free packets Qed at the end of this entry */
			for (; rte != NULL; rte = nrte) {
				nrte = rte->next;
				if (rte->ifp) {
					ip_mdq(rte->m, rte->ifp, rt);
				}
				m_freem(rte->m);
				free(rte, M_MRTABLE, 0);
			}
		}
	}

	/*
	 * It is possible that an entry is being inserted without an upcall
	 */
	if (nstl == 0) {
		/*
		 * No mfc; make a new one
		 */
		LIST_FOREACH(rt, &mfchashtbl[rtableid][hash], mfc_hash) {
			if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
			    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp)) {
				init_mfc_params(rt, mfccp);
				if (rt->mfc_expire)
					nexpire[rtableid][hash]--;
				rt->mfc_expire = 0;
				break; /* XXX */
			}
		}
		if (rt == NULL) {	/* no upcall, so make a new entry */
			rt = malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
			if (rt == NULL)
				return (ENOBUFS);

			init_mfc_params(rt, mfccp);
			rt->mfc_expire	= 0;
			rt->mfc_stall	= NULL;

			/* insert new entry at head of hash chain */
			LIST_INSERT_HEAD(&mfchashtbl[rtableid][hash], rt, mfc_hash);
		}
	}

	return (0);
}

/*
 * Delete an mfc entry
 */
int
del_mfc(struct socket *so, struct mbuf *m)
{
	struct inpcb *inp = sotoinpcb(so);
	struct mfcctl2 mfcctl2;
	struct mfcctl2 *mfccp;
	struct mfc *rt;
	int mfcctl_size = sizeof(struct mfcctl);
	struct mfcctl *mp = mtod(m, struct mfcctl *);
	unsigned int rtableid = inp->inp_rtableid;

	splsoftassert(IPL_SOFTNET);

	/*
	 * XXX: for deleting MFC entries the information in entries
	 * of size "struct mfcctl" is sufficient.
	 */

	if (m == NULL || m->m_len < mfcctl_size)
		return (EINVAL);

	bcopy(mp, (caddr_t)&mfcctl2, sizeof(*mp));
	memset((caddr_t)&mfcctl2 + sizeof(struct mfcctl), 0,
	    sizeof(mfcctl2) - sizeof(struct mfcctl));

	mfccp = &mfcctl2;

	rt = mfc_find(rtableid, &mfccp->mfcc_origin, &mfccp->mfcc_mcastgrp);
	if (rt == NULL)
		return (EADDRNOTAVAIL);

	LIST_REMOVE(rt, mfc_hash);
	free(rt, M_MRTABLE, 0);

	return (0);
}

int
socket_send(struct socket *s, struct mbuf *mm, struct sockaddr_in *src)
{
	if (s != NULL) {
		if (sbappendaddr(&s->so_rcv, sintosa(src), mm, NULL) != 0) {
			sorwakeup(s);
			return (0);
		}
	}
	m_freem(mm);
	return (-1);
}

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

#define IP_HDR_LEN  20	/* # bytes of fixed IP header (excluding options) */
#define TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

int
ip_mforward(struct mbuf *m, struct ifnet *ifp)
{
	struct ip *ip = mtod(m, struct ip *);
	struct vif *v;
	struct mfc *rt;
	static int srctun = 0;
	struct mbuf *mm;
	int s;
	unsigned int rtableid = ifp->if_rdomain;

	if (ip->ip_hl < (IP_HDR_LEN + TUNNEL_LEN) >> 2 ||
	    ((u_char *)(ip + 1))[1] != IPOPT_LSRR) {
		/*
		 * Packet arrived via a physical interface or
		 * an encapsulated tunnel or a register_vif.
		 */
	} else {
		/*
		 * Packet arrived through a source-route tunnel.
		 * Source-route tunnels are no longer supported.
		 */
		if ((srctun++ % 1000) == 0)
			log(LOG_ERR, "ip_mforward: received source-routed "
			    "packet from %x\n", ntohl(ip->ip_src.s_addr));

		return (1);
	}

	/*
	 * Don't forward a packet with time-to-live of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip->ip_ttl <= 1 || IN_LOCAL_GROUP(ip->ip_dst.s_addr))
		return (0);

	/*
	 * Determine forwarding vifs from the forwarding cache table
	 */
	++mrtstat.mrts_mfc_lookups;
	rt = mfc_find(rtableid, &ip->ip_src, &ip->ip_dst);

	/* Entry exists, so forward if necessary */
	if (rt != NULL) {
		return (ip_mdq(m, ifp, rt));
	} else {
		/*
		 * If we don't have a route for packet's origin,
		 * Make a copy of the packet & send message to routing daemon
		 */

		struct mbuf *mb0;
		struct rtdetq *rte;
		u_int32_t hash;
		int hlen = ip->ip_hl << 2;

		++mrtstat.mrts_mfc_misses;

		mrtstat.mrts_no_route++;
		/*
		 * Allocate mbufs early so that we don't do extra work if we are
		 * just going to fail anyway.  Make sure to pullup the header so
		 * that other people can't step on it.
		 */
		rte = malloc(sizeof(*rte), M_MRTABLE, M_NOWAIT);
		if (rte == NULL)
			return (ENOBUFS);
		mb0 = m_copym(m, 0, M_COPYALL, M_NOWAIT);
		if (mb0 == NULL ||
		    (mb0 = m_pullup(mb0, hlen)) == NULL) {
			free(rte, M_MRTABLE, 0);
			return (ENOBUFS);
		}

		/* is there an upcall waiting for this flow? */
		hash = MFCHASH(rtableid, ip->ip_src, ip->ip_dst);
		LIST_FOREACH(rt, &mfchashtbl[rtableid][hash], mfc_hash) {
			if (in_hosteq(ip->ip_src, rt->mfc_origin) &&
			    in_hosteq(ip->ip_dst, rt->mfc_mcastgrp) &&
			    rt->mfc_stall != NULL)
				break;
		}

		if (rt == NULL) {
			int i;
			struct igmpmsg *im;

			/*
			 * Locate the vifi for the incoming interface for
			 * this packet.
			 * If none found, drop packet.
			 */
			if ((v = (struct vif *)ifp->if_mcast) == NULL)
				goto non_fatal;

			/* no upcall, so make a new entry */
			rt = malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
			if (rt == NULL)
				goto fail;
			/*
			 * Make a copy of the header to send to the user level
			 * process
			 */
			mm = m_copym(m, 0, hlen, M_NOWAIT);
			if (mm == NULL ||
			    (mm = m_pullup(mm, hlen)) == NULL)
				goto fail1;

			/*
			 * Send message to routing daemon to install
			 * a route into the kernel table
			 */

			im = mtod(mm, struct igmpmsg *);
			im->im_msgtype = IGMPMSG_NOCACHE;
			im->im_mbz = 0;
			im->im_vif = v->v_id;

			mrtstat.mrts_upcalls++;

			sin.sin_addr = ip->ip_src;
			if (socket_send(ip_mrouter[rtableid], mm, &sin) < 0) {
				log(LOG_WARNING, "ip_mforward: ip_mrouter "
				    "socket queue full\n");
				++mrtstat.mrts_upq_sockfull;
			fail1:
				free(rt, M_MRTABLE, 0);
			fail:
				free(rte, M_MRTABLE, 0);
				m_freem(mb0);
				splx(s);
				return (ENOBUFS);
			}

			/* insert new entry at head of hash chain */
			rt->mfc_origin = ip->ip_src;
			rt->mfc_mcastgrp = ip->ip_dst;
			rt->mfc_pkt_cnt = 0;
			rt->mfc_byte_cnt = 0;
			rt->mfc_wrong_if = 0;
			rt->mfc_expire = UPCALL_EXPIRE;
			nexpire[rtableid][hash]++;
			for (i = 0; i < MAXVIFS; i++) {
				rt->mfc_ttls[i] = 0;
				rt->mfc_flags[i] = 0;
			}
			rt->mfc_parent = -1;

			/* clear the RP address */
			rt->mfc_rp = zeroin_addr;

			/* link into table */
			LIST_INSERT_HEAD(&mfchashtbl[rtableid][hash], rt, mfc_hash);
			/* Add this entry to the end of the queue */
			rt->mfc_stall = rte;
		} else {
			/* determine if q has overflowed */
			struct rtdetq **p;
			int npkts = 0;

			/*
			 * XXX ouch! we need to append to the list, but we
			 * only have a pointer to the front, so we have to
			 * scan the entire list every time.
			 */
			for (p = &rt->mfc_stall; *p != NULL; p = &(*p)->next)
				if (++npkts > MAX_UPQ) {
					mrtstat.mrts_upq_ovflw++;
				non_fatal:
					free(rte, M_MRTABLE, 0);
					m_freem(mb0);
					return (0);
				}

			/* Add this entry to the end of the queue */
			*p = rte;
		}

		rte->next = NULL;
		rte->m = mb0;
		rte->ifp = ifp;

		return (0);
	}
}


/*ARGSUSED*/
void
expire_upcalls(void *v)
{
	unsigned int rtableid = *(unsigned int *)v;
	int i, s;


	NET_LOCK(s);
	for (i = 0; i < MFCTBLSIZ; i++) {
		struct mfc *rt, *nrt;

		if (nexpire[rtableid][i] == 0)
			continue;

		for (rt = LIST_FIRST(&mfchashtbl[rtableid][i]); rt; rt = nrt) {
			nrt = LIST_NEXT(rt, mfc_hash);

			if (rt->mfc_expire == 0 || --rt->mfc_expire > 0)
				continue;
			nexpire[rtableid][i]--;

			++mrtstat.mrts_cache_cleanups;
			expire_mfc(rt);
		}
	}
	timeout_add_msec(&expire_upcalls_ch[rtableid], EXPIRE_TIMEOUT);
	NET_UNLOCK(s);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
int
ip_mdq(struct mbuf *m, struct ifnet *ifp0, struct mfc *rt)
{
	struct ip  *ip = mtod(m, struct ip *);
	struct vif *v = (struct vif *)ifp0->if_mcast;
	struct ifnet *ifp;
	struct mbuf *mc;
	int i;
	unsigned int rtableid = ifp0->if_rdomain;
	struct ip_moptions imo;

	/* Sanity check: we have all promised pointers. */
	if (v == NULL || rt == NULL)
		return (-1);

	/*
	 * Don't forward if it didn't arrive from the parent vif for its origin.
	 */
	if (rt->mfc_parent != v->v_id) {
		/* came in the wrong interface */
		++mrtstat.mrts_wrong_if;
		++rt->mfc_wrong_if;
		return (0);
	}

	/* If I sourced this packet, it counts as output, else it was input. */
	if (in_hosteq(ip->ip_src, v->v_lcl_addr)) {
		v->v_pkt_out++;
		v->v_bytes_out += m->m_pkthdr.len;
	} else {
		v->v_pkt_in++;
		v->v_bytes_in += m->m_pkthdr.len;
	}
	rt->mfc_pkt_cnt++;
	rt->mfc_byte_cnt += m->m_pkthdr.len;

	/*
	 * For each vif, decide if a copy of the packet should be forwarded.
	 * Forward if:
	 *		- the ttl exceeds the vif's threshold
	 *		- there are group members downstream on interface
	 */
	for (i = 0; i < MAXVIFS; i++) {
		if (rt->mfc_ttls[i] == 0)
			continue;
		if (ip->ip_ttl <= rt->mfc_ttls[i])
			continue;
		if ((ifp = if_lookupbyvif(i, rtableid)) == NULL)
			continue;

		v = (struct vif *)ifp->if_mcast;
		v->v_pkt_out++;
		v->v_bytes_out += m->m_pkthdr.len;

		/*
		 * Make a new reference to the packet; make sure
		 * that the IP header is actually copied, not
		 * just referenced, so that ip_output() only
		 * scribbles on the copy.
		 */
		mc = m_dup_pkt(m, max_linkhdr, M_NOWAIT);
		if (mc == NULL)
			return (-1);

		/*
		 * if physical interface option, extract the options
		 * and then send
		 */
		imo.imo_ifidx = ifp->if_index;
		imo.imo_ttl = ip->ip_ttl - IPTTLDEC;
		imo.imo_loop = 1;

		ip_output(mc, NULL, NULL, IP_FORWARDING, &imo, NULL, 0);
	}

	return (0);
}

struct ifnet *
if_lookupbyvif(vifi_t vifi, unsigned int rtableid)
{
	struct vif	*v;
	struct ifnet	*ifp;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rtableid)
			continue;
		if ((v = (struct vif *)ifp->if_mcast) == NULL)
			continue;
		if (v->v_id != vifi)
			continue;

		return (ifp);
	}

	return (NULL);
}
