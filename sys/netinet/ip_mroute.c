/*	$OpenBSD: ip_mroute.c,v 1.38 2004/11/24 01:25:42 mcbride Exp $	*/
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
 *
 * MROUTING Revision: 1.2
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_mroute.h>

#include <sys/stdarg.h>

#define IP_MULTICASTOPTS 0
#define	M_PULLUP(m, len)						 \
	do {								 \
		if ((m) && ((m)->m_flags & M_EXT || (m)->m_len < (len))) \
			(m) = m_pullup((m), (len));			 \
	} while (/*CONSTCOND*/ 0)

/*
 * Globals.  All but ip_mrouter and ip_mrtproto could be static,
 * except for netstat or debugging purposes.
 */
struct socket  *ip_mrouter  = NULL;
int		ip_mrtproto = IGMP_DVMRP;    /* for netstat only */

#define NO_RTE_FOUND 	0x1
#define RTE_FOUND	0x2

#define	MFCHASH(a, g)							\
	((((a).s_addr >> 20) ^ ((a).s_addr >> 10) ^ (a).s_addr ^	\
	  ((g).s_addr >> 20) ^ ((g).s_addr >> 10) ^ (g).s_addr) & mfchash)
LIST_HEAD(mfchashhdr, mfc) *mfchashtbl;
u_long	mfchash;

u_char		nexpire[MFCTBLSIZ];
struct vif	viftable[MAXVIFS];
struct mrtstat	mrtstat;
u_int		mrtdebug = 0;	  /* debug level 	*/
#define		DEBUG_MFC	0x02
#define		DEBUG_FORWARD	0x04
#define		DEBUG_EXPIRE	0x08
#define		DEBUG_XMIT	0x10
u_int       	tbfdebug = 0;     /* tbf debug level 	*/
#ifdef RSVP_ISI
u_int		rsvpdebug = 0;	  /* rsvp debug level   */
extern struct socket *ip_rsvpd;
extern int rsvp_on;
#endif /* RSVP_ISI */

#define		EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second */
#define		UPCALL_EXPIRE	6		/* number of timeouts */
struct timeout	expire_upcalls_ch;

/*
 * Define the token bucket filter structures
 */

#define		TBF_REPROCESS	(hz / 100)	/* 100x / second */

static int get_sg_cnt(struct sioc_sg_req *);
static int get_vif_cnt(struct sioc_vif_req *);
static int ip_mrouter_init(struct socket *, struct mbuf *);
static int get_version(struct mbuf *);
static int set_assert(struct mbuf *);
static int get_assert(struct mbuf *);
static int add_vif(struct mbuf *);
static int del_vif(struct mbuf *);
static void update_mfc_params(struct mfc *, struct mfcctl *);
static void init_mfc_params(struct mfc *, struct mfcctl *);
static void expire_mfc(struct mfc *);
static int add_mfc(struct mbuf *);
#ifdef UPCALL_TIMING
static void collate(struct timeval *);
#endif
static int del_mfc(struct mbuf *);
static int socket_send(struct socket *, struct mbuf *,
			    struct sockaddr_in *);
static void expire_upcalls(void *);
#ifdef RSVP_ISI
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *, vifi_t);
#else
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *);
#endif
static void phyint_send(struct ip *, struct vif *, struct mbuf *);
static void encap_send(struct ip *, struct vif *, struct mbuf *);
static void tbf_control(struct vif *, struct mbuf *, struct ip *,
			     u_int32_t);
static void tbf_queue(struct vif *, struct mbuf *);
static void tbf_process_q(struct vif *);
static void tbf_reprocess_q(void *);
static int tbf_dq_sel(struct vif *, struct ip *);
static void tbf_send_packet(struct vif *, struct mbuf *);
static void tbf_update_tokens(struct vif *);
static int priority(struct vif *, struct ip *);

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
#if 0
struct ifnet multicast_decap_if[MAXVIFS];
#endif

#define	ENCAP_TTL	64
#define	ENCAP_PROTO	IPPROTO_IPIP	/* 4 */

/* prototype IP hdr for encapsulated packets */
struct ip multicast_encap_iphdr = {
#if BYTE_ORDER == LITTLE_ENDIAN
	sizeof(struct ip) >> 2, IPVERSION,
#else
	IPVERSION, sizeof(struct ip) >> 2,
#endif
	0,				/* tos */
	sizeof(struct ip),		/* total length */
	0,				/* id */
	0,				/* frag offset */
	ENCAP_TTL, ENCAP_PROTO,
	0,				/* checksum */
};

/*
 * Private variables.
 */
static vifi_t	   numvifs = 0;
static int have_encap_tunnel = 0;

/*
 * one-back cache used by ipip_mroute_input to locate a tunnel's vif
 * given a datagram's src ip address.
 */
static struct in_addr last_encap_src;
static struct vif *last_encap_vif;

/*
 * whether or not special PIM assert processing is enabled.
 */
static int pim_assert;
/*
 * Rate limit for assert notification messages, in usec
 */
#define ASSERT_MSG_TIME		3000000

/*
 * Find a route for a given origin IP address and Multicast group address
 * Type of service parameter to be added in the future!!!
 * Statistics are updated by the caller if needed
 * (mrtstat.mrts_mfc_lookups and mrtstat.mrts_mfc_misses)
 */
static struct mfc *
mfc_find(struct in_addr *o, struct in_addr *g)
{
	struct mfc *rt;

	LIST_FOREACH(rt, &mfchashtbl[MFCHASH(*o, *g)], mfc_hash) {
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
		/* fall through */					\
	case 1:								\
		delta += 1000000;					\
		/* fall through */					\
	case 0:								\
		break;							\
	default:							\
		delta += (1000000 * xxs);				\
		break;							\
	}								\
} while (/*CONSTCOND*/ 0)

#ifdef UPCALL_TIMING
u_int32_t upcall_data[51];
#endif /* UPCALL_TIMING */

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
int
ip_mrouter_set(struct socket *so, int optname, struct mbuf **m)
{
	int error;

	if (optname != MRT_INIT && so != ip_mrouter)
		error = ENOPROTOOPT;
	else
		switch (optname) {
		case MRT_INIT:
			error = ip_mrouter_init(so, *m);
			break;
		case MRT_DONE:
			error = ip_mrouter_done();
			break;
		case MRT_ADD_VIF:
			error = add_vif(*m);
			break;
		case MRT_DEL_VIF:
			error = del_vif(*m);
			break;
		case MRT_ADD_MFC:
			error = add_mfc(*m);
			break;
		case MRT_DEL_MFC:
			error = del_mfc(*m);
			break;
		case MRT_ASSERT:
			error = set_assert(*m);
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}

	if (*m)
		m_free(*m);
	return (error);
}

/*
 * Handle MRT getsockopt commands
 */
int
ip_mrouter_get(struct socket *so, int optname, struct mbuf **m)
{
	int error;

	if (so != ip_mrouter)
		error = ENOPROTOOPT;
	else {
		*m = m_get(M_WAIT, MT_SOOPTS);

		switch (optname) {
		case MRT_VERSION:
			error = get_version(*m);
			break;
		case MRT_ASSERT:
			error = get_assert(*m);
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}

		if (error)
			m_free(*m);
	}

	return (error);
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
mrt_ioctl(struct socket *so, u_long cmd, caddr_t data)
{
	int error;

	if (so != ip_mrouter)
		error = EINVAL;
	else
		switch (cmd) {
		case SIOCGETVIFCNT:
			error = get_vif_cnt((struct sioc_vif_req *)data);
			break;
		case SIOCGETSGCNT:
			error = get_sg_cnt((struct sioc_sg_req *)data);
			break;
		default:
			error = EINVAL;
			break;
		}

	return (error);
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
static int
get_sg_cnt(struct sioc_sg_req *req)
{
	int s;
	struct mfc *rt;

	s = splsoftnet();
	rt = mfc_find(&req->src, &req->grp);
	if (rt == NULL) {
		splx(s);
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
		return (EADDRNOTAVAIL);
	}
	req->pktcnt = rt->mfc_pkt_cnt;
	req->bytecnt = rt->mfc_byte_cnt;
	req->wrong_if = rt->mfc_wrong_if;
	splx(s);

	return (0);
}

/*
 * returns the input and output packet and byte counts on the vif provided
 */
static int
get_vif_cnt(struct sioc_vif_req *req)
{
	vifi_t vifi = req->vifi;

	if (vifi >= numvifs)
		return (EINVAL);

	req->icount = viftable[vifi].v_pkt_in;
	req->ocount = viftable[vifi].v_pkt_out;
	req->ibytes = viftable[vifi].v_bytes_in;
	req->obytes = viftable[vifi].v_bytes_out;

	return (0);
}

/*
 * Enable multicast routing
 */
static int
ip_mrouter_init(struct socket *so, struct mbuf *m)
{
	int *v;

	if (mrtdebug)
		log(LOG_DEBUG,
		    "ip_mrouter_init: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_IGMP)
		return (EOPNOTSUPP);

	if (m == NULL || m->m_len < sizeof(int))
		return (EINVAL);

	v = mtod(m, int *);
	if (*v != 1)
		return (EINVAL);

	if (ip_mrouter != NULL)
		return (EADDRINUSE);

	ip_mrouter = so;

	mfchashtbl = hashinit(MFCTBLSIZ, M_MRTABLE, M_WAITOK, &mfchash);
	bzero((caddr_t)nexpire, sizeof(nexpire));

	pim_assert = 0;

	timeout_set(&expire_upcalls_ch, expire_upcalls, NULL);
	timeout_add(&expire_upcalls_ch, EXPIRE_TIMEOUT);

	if (mrtdebug)
		log(LOG_DEBUG, "ip_mrouter_init\n");

	return (0);
}

/*
 * Disable multicast routing
 */
int
ip_mrouter_done()
{
	vifi_t vifi;
	struct vif *vifp;
	int i;
	int s;

	s = splsoftnet();

	/* Clear out all the vifs currently in use. */
	for (vifi = 0; vifi < numvifs; vifi++) {
		vifp = &viftable[vifi];
		if (!in_nullhost(vifp->v_lcl_addr))
			reset_vif(vifp);
	}

	numvifs = 0;
	pim_assert = 0;

	timeout_del(&expire_upcalls_ch);

	/*
	 * Free all multicast forwarding cache entries.
	 */
	for (i = 0; i < MFCTBLSIZ; i++) {
		struct mfc *rt, *nrt;

		for (rt = LIST_FIRST(&mfchashtbl[i]); rt; rt = nrt) {
			nrt = LIST_NEXT(rt, mfc_hash);

			expire_mfc(rt);
		}
	}

	bzero((caddr_t)nexpire, sizeof(nexpire));
	free(mfchashtbl, M_MRTABLE);
	mfchashtbl = NULL;

	/* Reset de-encapsulation cache. */
	have_encap_tunnel = 0;

	ip_mrouter = NULL;

	splx(s);

	if (mrtdebug)
		log(LOG_DEBUG, "ip_mrouter_done\n");

	return (0);
}

void
ip_mrouter_detach(struct ifnet *ifp)
{
	int vifi, i;
	struct vif *vifp;
	struct mfc *rt;
	struct rtdetq *rte;

	/* XXX not sure about side effect to userland routing daemon */
	for (vifi = 0; vifi < numvifs; vifi++) {
		vifp = &viftable[vifi];
		if (vifp->v_ifp == ifp)
			reset_vif(vifp);
	}
	for (i = 0; i < MFCTBLSIZ; i++) {
		if (nexpire[i] == 0)
			continue;
		LIST_FOREACH(rt, &mfchashtbl[i], mfc_hash) {
			for (rte = rt->mfc_stall; rte; rte = rte->next) {
				if (rte->ifp == ifp)
					rte->ifp = NULL;
			}
		}
	}
}

static int
get_version(struct mbuf *m)
{
	int *v = mtod(m, int *);

	*v = 0x0305;	/* XXX !!!! */
	m->m_len = sizeof(int);
	return (0);
}

/*
 * Set PIM assert processing global
 */
static int
set_assert(struct mbuf *m)
{
	int *i;

	if (m == NULL || m->m_len < sizeof(int))
		return (EINVAL);

	i = mtod(m, int *);
	pim_assert = !!*i;
	return (0);
}

/*
 * Get PIM assert processing global
 */
static int
get_assert(struct mbuf *m)
{
	int *i = mtod(m, int *);

	*i = pim_assert;
	m->m_len = sizeof(int);
	return (0);
}

static struct sockaddr_in sin = { sizeof(sin), AF_INET };

/*
 * Add a vif to the vif table
 */
static int
add_vif(struct mbuf *m)
{
	struct vifctl *vifcp;
	struct vif *vifp;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	struct ifreq ifr;
	int error, s;

	if (m == NULL || m->m_len < sizeof(struct vifctl))
		return (EINVAL);

	vifcp = mtod(m, struct vifctl *);
	if (vifcp->vifc_vifi >= MAXVIFS)
		return (EINVAL);
	if (in_nullhost(vifcp->vifc_lcl_addr))
		return (EADDRNOTAVAIL);

	vifp = &viftable[vifcp->vifc_vifi];
	if (!in_nullhost(vifp->v_lcl_addr))
		return (EADDRINUSE);

	/* Find the interface with an address in AF_INET family. */
	sin.sin_addr = vifcp->vifc_lcl_addr;
	ifa = ifa_ifwithaddr(sintosa(&sin));
	if (ifa == NULL)
		return (EADDRNOTAVAIL);
	ifp = ifa->ifa_ifp;

	if (vifcp->vifc_flags & VIFF_TUNNEL) {
		if (vifcp->vifc_flags & VIFF_SRCRT) {
			log(LOG_ERR, "Source routed tunnels not supported.\n");
			return (EOPNOTSUPP);
		}

		/* Create a fake encapsulation interface. */
		ifp = (struct ifnet *)malloc(sizeof(*ifp), M_MRTABLE, M_WAITOK);
		bzero(ifp, sizeof(*ifp));
		snprintf(ifp->if_xname, sizeof ifp->if_xname,
		    "mdecap%d", vifcp->vifc_vifi);

		/* Prepare cached route entry. */
		bzero(&vifp->v_route, sizeof(vifp->v_route));

		/*
		 * Tell ipip_mroute_input() to start looking at
		 * encapsulated packets.
		 */
		have_encap_tunnel = 1;
	} else {
		/* Use the physical interface associated with the address. */
		ifp = ifa->ifa_ifp;

		/* Make sure the interface supports multicast. */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return (EOPNOTSUPP);

		/* Enable promiscuous reception of all IP multicasts. */
		satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = zeroin_addr;
		error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
		if (error)
			return (error);
	}

	s = splsoftnet();

	/* Define parameters for the tbf structure. */
	vifp->tbf_q = NULL;
	vifp->tbf_t = &vifp->tbf_q;
	microtime(&vifp->tbf_last_pkt_t);
	vifp->tbf_n_tok = 0;
	vifp->tbf_q_len = 0;
	vifp->tbf_max_q_len = MAXQSIZE;

	vifp->v_flags = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	/* scaling up here allows division by 1024 in critical code */
	vifp->v_rate_limit = vifcp->vifc_rate_limit * 1024 / 1000;
	vifp->v_lcl_addr = vifcp->vifc_lcl_addr;
	vifp->v_rmt_addr = vifcp->vifc_rmt_addr;
	vifp->v_ifp = ifp;
	/* Initialize per vif pkt counters. */
	vifp->v_pkt_in = 0;
	vifp->v_pkt_out = 0;
	vifp->v_bytes_in = 0;
	vifp->v_bytes_out = 0;

	timeout_del(&vifp->v_repq_ch);

#ifdef RSVP_ISI
	vifp->v_rsvp_on = 0;
	vifp->v_rsvpd = NULL;
#endif /* RSVP_ISI */

	splx(s);

	/* Adjust numvifs up if the vifi is higher than numvifs. */
	if (numvifs <= vifcp->vifc_vifi)
		numvifs = vifcp->vifc_vifi + 1;

	if (mrtdebug)
		log(LOG_DEBUG, "add_vif #%d, lcladdr %x, %s %x, thresh %x, rate %d\n",
		    vifcp->vifc_vifi,
		    ntohl(vifcp->vifc_lcl_addr.s_addr),
		    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
		    ntohl(vifcp->vifc_rmt_addr.s_addr),
		    vifcp->vifc_threshold,
		    vifcp->vifc_rate_limit);

	return (0);
}

void
reset_vif(struct vif *vifp)
{
	struct mbuf *m, *n;
	struct ifnet *ifp;
	struct ifreq ifr;

	timeout_set(&vifp->v_repq_ch, tbf_reprocess_q, vifp);

	/*
	 * Free packets queued at the interface
	 */
	for (m = vifp->tbf_q; m != NULL; m = n) {
		n = m->m_nextpkt;
		m_freem(m);
	}

	if (vifp->v_flags & VIFF_TUNNEL) {
		free(vifp->v_ifp, M_MRTABLE);
		if (vifp == last_encap_vif) {
			last_encap_vif = NULL;
			last_encap_src = zeroin_addr;
		}
	} else {
		satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = zeroin_addr;
		ifp = vifp->v_ifp;
		(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
	}
	bzero((caddr_t)vifp, sizeof(*vifp));
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif(struct mbuf *m)
{
	vifi_t *vifip;
	struct vif *vifp;
	vifi_t vifi;
	int s;

	if (m == NULL || m->m_len < sizeof(vifi_t))
		return (EINVAL);

	vifip = mtod(m, vifi_t *);
	if (*vifip >= numvifs)
		return (EINVAL);

	vifp = &viftable[*vifip];
	if (in_nullhost(vifp->v_lcl_addr))
		return (EADDRNOTAVAIL);

	s = splsoftnet();

	reset_vif(vifp);

	/* Adjust numvifs down */
	for (vifi = numvifs; vifi > 0; vifi--)
		if (!in_nullhost(viftable[vifi - 1].v_lcl_addr))
			break;
	numvifs = vifi;

	splx(s);

	if (mrtdebug)
		log(LOG_DEBUG, "del_vif %d, numvifs %d\n", *vifip, numvifs);

	return (0);
}

void
vif_delete(struct ifnet *ifp)
{
	int i;
	struct vif *vifp;
	struct mfc *rt;
	struct rtdetq *rte;

	for (i = 0; i < numvifs; i++) {
		vifp = &viftable[i];
		if (vifp->v_ifp == ifp)
			bzero((caddr_t)vifp, sizeof *vifp);
	}

	for (i = numvifs; i > 0; i--)
		if (!in_nullhost(viftable[i - 1].v_lcl_addr))
			break;
	numvifs = i;

	for (i = 0; i < MFCTBLSIZ; i++) {
		if (nexpire[i] == 0)
			continue;
		LIST_FOREACH(rt, &mfchashtbl[i], mfc_hash) {
			for (rte = rt->mfc_stall; rte; rte = rte->next) {
				if (rte->ifp == ifp)
					rte->ifp = NULL;
			}
		}
	}
}

/*
 * update an mfc entry without resetting counters and S,G addresses.
 */
static void
update_mfc_params(struct mfc *rt, struct mfcctl *mfccp)
{
	int i;

	rt->mfc_parent = mfccp->mfcc_parent;
	for (i = 0; i < numvifs; i++) {
		rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
	}
}

/*
 * fully initialize an mfc entry from the parameter.
 */
static void
init_mfc_params(struct mfc *rt, struct mfcctl *mfccp)
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

static void
expire_mfc(struct mfc *rt)
{
	struct rtdetq *rte, *nrte;

	for (rte = rt->mfc_stall; rte != NULL; rte = nrte) {
		nrte = rte->next;
		m_freem(rte->m);
		free(rte, M_MRTABLE);
	}

	LIST_REMOVE(rt, mfc_hash);
	free(rt, M_MRTABLE);
}

/*
 * Add an mfc entry
 */
static int
add_mfc(struct mbuf *m)
{
	struct mfcctl *mfccp;
	struct mfc *rt;
	u_int32_t hash = 0;
	struct rtdetq *rte, *nrte;
	u_short nstl;
	int s;

	if (m == NULL || m->m_len < sizeof(struct mfcctl))
		return (EINVAL);

	mfccp = mtod(m, struct mfcctl *);

	s = splsoftnet();
	rt = mfc_find(&mfccp->mfcc_origin, &mfccp->mfcc_mcastgrp);

	/* If an entry already exists, just update the fields */
	if (rt) {
		if (mrtdebug & DEBUG_MFC)
			log(LOG_DEBUG, "add_mfc update o %x g %x p %x\n",
			    ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);

		update_mfc_params(rt, mfccp);

		splx(s);
		return (0);
	}

	/*
	 * Find the entry for which the upcall was made and update
	 */
	nstl = 0;
	hash = MFCHASH(mfccp->mfcc_origin, mfccp->mfcc_mcastgrp);
	LIST_FOREACH(rt, &mfchashtbl[hash], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
		    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp) &&
		    rt->mfc_stall != NULL) {
			if (nstl++)
				log(LOG_ERR, "add_mfc %s o %x g %x p %x dbx %p\n",
				    "multiple kernel entries",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);

			if (mrtdebug & DEBUG_MFC)
				log(LOG_DEBUG, "add_mfc o %x g %x p %x dbg %p\n",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);

			rte = rt->mfc_stall;
			init_mfc_params(rt, mfccp);
			rt->mfc_stall = NULL;

			rt->mfc_expire = 0; /* Don't clean this guy up */
			nexpire[hash]--;

			/* free packets Qed at the end of this entry */
			for (; rte != NULL; rte = nrte) {
				nrte = rte->next;
				if (rte->ifp) {
#ifdef RSVP_ISI
					ip_mdq(rte->m, rte->ifp, rt, -1);
#else
					ip_mdq(rte->m, rte->ifp, rt);
#endif /* RSVP_ISI */
				}
				m_freem(rte->m);
#ifdef UPCALL_TIMING
				collate(&rte->t);
#endif /* UPCALL_TIMING */
				free(rte, M_MRTABLE);
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
		if (mrtdebug & DEBUG_MFC)
			log(LOG_DEBUG, "add_mfc no upcall o %x g %x p %x\n",
			    ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);

		LIST_FOREACH(rt, &mfchashtbl[hash], mfc_hash) {
			if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
			    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp)) {
				init_mfc_params(rt, mfccp);
				if (rt->mfc_expire)
					nexpire[hash]--;
				rt->mfc_expire = 0;
				break; /* XXX */
			}
		}
		if (rt == NULL) {	/* no upcall, so make a new entry */
			rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE,
						  M_NOWAIT);
			if (rt == NULL) {
				splx(s);
				return (ENOBUFS);
			}

			init_mfc_params(rt, mfccp);
			rt->mfc_expire	= 0;
			rt->mfc_stall	= NULL;

			/* insert new entry at head of hash chain */
			LIST_INSERT_HEAD(&mfchashtbl[hash], rt, mfc_hash);
		}
	}

	splx(s);
	return (0);
}

#ifdef UPCALL_TIMING
/*
 * collect delay statistics on the upcalls
 */
static void
collate(struct timeval *t)
{
	u_int32_t d;
	struct timeval tp;
	u_int32_t delta;

	microtime(&tp);

	if (timercmp(t, &tp, <)) {
		TV_DELTA(tp, *t, delta);

		d = delta >> 10;
		if (d > 50)
			d = 50;

		++upcall_data[d];
	}
}
#endif /* UPCALL_TIMING */

/*
 * Delete an mfc entry
 */
static int
del_mfc(struct mbuf *m)
{
	struct mfcctl *mfccp;
	struct mfc *rt;
	int s;

	if (m == NULL || m->m_len < sizeof(struct mfcctl))
		return (EINVAL);

	mfccp = mtod(m, struct mfcctl *);

	if (mrtdebug & DEBUG_MFC)
		log(LOG_DEBUG, "del_mfc origin %x mcastgrp %x\n",
		    ntohl(mfccp->mfcc_origin.s_addr),
		    ntohl(mfccp->mfcc_mcastgrp.s_addr));

	s = splsoftnet();

	rt = mfc_find(&mfccp->mfcc_origin, &mfccp->mfcc_mcastgrp);
	if (rt == NULL) {
		splx(s);
		return (EADDRNOTAVAIL);
	}

	LIST_REMOVE(rt, mfc_hash);
	free(rt, M_MRTABLE);

	splx(s);
	return (0);
}

static int
socket_send(struct socket *s, struct mbuf *mm, struct sockaddr_in *src)
{
	if (s != NULL) {
		if (sbappendaddr(&s->so_rcv, sintosa(src), mm,
		    (struct mbuf *)NULL) != 0) {
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
#ifdef RSVP_ISI
ip_mforward(struct mbuf *m, struct ifnet *ifp, struct ip_moptions *imo)
#else
ip_mforward(struct mbuf *m, struct ifnet *ifp)
#endif /* RSVP_ISI */
{
	struct ip *ip = mtod(m, struct ip *);
	struct mfc *rt;
	static int srctun = 0;
	struct mbuf *mm;
	int s;
	vifi_t vifi;

	if (mrtdebug & DEBUG_FORWARD)
		log(LOG_DEBUG, "ip_mforward: src %x, dst %x, ifp %p\n",
		    ntohl(ip->ip_src.s_addr), ntohl(ip->ip_dst.s_addr), ifp);

	if (ip->ip_hl < (IP_HDR_LEN + TUNNEL_LEN) >> 2 ||
	    ((u_char *)(ip + 1))[1] != IPOPT_LSRR) {
		/*
		 * Packet arrived via a physical interface or
		 * an encapuslated tunnel.
		 */
	} else {
		/*
		 * Packet arrived through a source-route tunnel.
		 * Source-route tunnels are no longer supported.
		 */
		if ((srctun++ % 1000) == 0)
			log(LOG_ERR,
			    "ip_mforward: received source-routed packet from %x\n",
			    ntohl(ip->ip_src.s_addr));

		return (1);
	}

#ifdef RSVP_ISI
	if (imo && ((vifi = imo->imo_multicast_vif) < numvifs)) {
		if (ip->ip_ttl < 255)
			ip->ip_ttl++;	/* compensate for -1 in *_send routines */
		if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
			struct vif *vifp = viftable + vifi;
			printf("Sending IPPROTO_RSVP from %x to %x on vif %d (%s%s)\n",
			    ntohl(ip->ip_src), ntohl(ip->ip_dst), vifi,
			    (vifp->v_flags & VIFF_TUNNEL) ? "tunnel on " : "",
			    vifp->v_ifp->if_xname);
		}
		return (ip_mdq(m, ifp, (struct mfc *)NULL, vifi));
	}
	if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
		printf("Warning: IPPROTO_RSVP from %x to %x without vif option\n",
		    ntohl(ip->ip_src), ntohl(ip->ip_dst));
	}
#endif /* RSVP_ISI */

	/*
	 * Don't forward a packet with time-to-live of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip->ip_ttl <= 1 || IN_LOCAL_GROUP(ip->ip_dst.s_addr))
		return (0);

	/*
	 * Determine forwarding vifs from the forwarding cache table
	 */
	s = splsoftnet();
	++mrtstat.mrts_mfc_lookups;
	rt = mfc_find(&ip->ip_src, &ip->ip_dst);

	/* Entry exists, so forward if necessary */
	if (rt != NULL) {
		splx(s);
#ifdef RSVP_ISI
		return (ip_mdq(m, ifp, rt, -1));
#else
		return (ip_mdq(m, ifp, rt));
#endif /* RSVP_ISI */
	} else {
		/*
		 * If we don't have a route for packet's origin,
		 * Make a copy of the packet & send message to routing daemon
		 */

		struct mbuf *mb0;
		struct rtdetq *rte;
		u_int32_t hash;
		int hlen = ip->ip_hl << 2;
#ifdef UPCALL_TIMING
		struct timeval tp;

		microtime(&tp);
#endif /* UPCALL_TIMING */

		++mrtstat.mrts_mfc_misses;

		mrtstat.mrts_no_route++;
		if (mrtdebug & (DEBUG_FORWARD | DEBUG_MFC))
			log(LOG_DEBUG, "ip_mforward: no rte s %x g %x\n",
			    ntohl(ip->ip_src.s_addr),
			    ntohl(ip->ip_dst.s_addr));

		/*
		 * Allocate mbufs early so that we don't do extra work if we are
		 * just going to fail anyway.  Make sure to pullup the header so
		 * that other people can't step on it.
		 */
		rte = (struct rtdetq *)malloc(sizeof(*rte), M_MRTABLE, M_NOWAIT);
		if (rte == NULL) {
			splx(s);
			return (ENOBUFS);
		}
		mb0 = m_copy(m, 0, M_COPYALL);
		M_PULLUP(mb0, hlen);
		if (mb0 == NULL) {
			free(rte, M_MRTABLE);
			splx(s);
			return (ENOBUFS);
		}

		/* is there an upcall waiting for this flow? */
		hash = MFCHASH(ip->ip_src, ip->ip_dst);
		LIST_FOREACH(rt, &mfchashtbl[hash], mfc_hash) {
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
			for (vifi = 0; vifi < numvifs &&
				 viftable[vifi].v_ifp != ifp; vifi++)
				;
			if (vifi >= numvifs) /* vif not found, drop packet */
				goto non_fatal;

			/* no upcall, so make a new entry */
			rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
			if (rt == NULL)
				goto fail;
			/*
			 * Make a copy of the header to send to the user level
			 * process
			 */
			mm = m_copy(m, 0, hlen);
			M_PULLUP(mm, hlen);
			if (mm == NULL)
				goto fail1;

			/*
			 * Send message to routing daemon to install
			 * a route into the kernel table
			 */

			im = mtod(mm, struct igmpmsg *);
			im->im_msgtype = IGMPMSG_NOCACHE;
			im->im_mbz = 0;
			im->im_vif = vifi;

			mrtstat.mrts_upcalls++;

			sin.sin_addr = ip->ip_src;
			if (socket_send(ip_mrouter, mm, &sin) < 0) {
				log(LOG_WARNING,
				    "ip_mforward: ip_mrouter socket queue full\n");
				++mrtstat.mrts_upq_sockfull;
			fail1:
				free(rt, M_MRTABLE);
			fail:
				free(rte, M_MRTABLE);
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
			nexpire[hash]++;
			for (i = 0; i < numvifs; i++)
				rt->mfc_ttls[i] = 0;
			rt->mfc_parent = -1;

			/* link into table */
			LIST_INSERT_HEAD(&mfchashtbl[hash], rt, mfc_hash);
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
					free(rte, M_MRTABLE);
					m_freem(mb0);
					splx(s);
					return (0);
				}

			/* Add this entry to the end of the queue */
			*p = rte;
		}

		rte->next = NULL;
		rte->m = mb0;
		rte->ifp = ifp;
	#ifdef UPCALL_TIMING
		rte->t = tp;
	#endif /* UPCALL_TIMING */

		splx(s);

		return (0);
	}
}


/*ARGSUSED*/
static void
expire_upcalls(void *v)
{
	int i;
	int s;

	s = splsoftnet();

	for (i = 0; i < MFCTBLSIZ; i++) {
		struct mfc *rt, *nrt;

		if (nexpire[i] == 0)
			continue;

		for (rt = LIST_FIRST(&mfchashtbl[i]); rt; rt = nrt) {
			nrt = LIST_NEXT(rt, mfc_hash);

			if (rt->mfc_expire == 0 || --rt->mfc_expire > 0)
				continue;
			nexpire[i]--;

			++mrtstat.mrts_cache_cleanups;
			if (mrtdebug & DEBUG_EXPIRE)
				log(LOG_DEBUG,
				    "expire_upcalls: expiring (%x %x)\n",
				    ntohl(rt->mfc_origin.s_addr),
				    ntohl(rt->mfc_mcastgrp.s_addr));

			expire_mfc(rt);
		}
	}

	splx(s);
	timeout_add(&expire_upcalls_ch, EXPIRE_TIMEOUT);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
#ifdef RSVP_ISI
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt, vifi_t xmt_vif)
#else
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt)
#endif /* RSVP_ISI */
{
	struct ip  *ip = mtod(m, struct ip *);
	vifi_t vifi;
	struct vif *vifp;
	int plen = ntohs(ip->ip_len) - (ip->ip_hl << 2);

/*
 * Macro to send packet on vif.  Since RSVP packets don't get counted on
 * input, they shouldn't get counted on output, so statistics keeping is
 * separate.
 */
#define MC_SEND(ip, vifp, m) do {					\
	if ((vifp)->v_flags & VIFF_TUNNEL)				\
		encap_send((ip), (vifp), (m));				\
	else								\
		phyint_send((ip), (vifp), (m));				\
} while (/*CONSTCOND*/ 0)

#ifdef RSVP_ISI
	/*
	 * If xmt_vif is not -1, send on only the requested vif.
	 *
	 * (since vifi_t is u_short, -1 becomes MAXUSHORT, which > numvifs.
	 */
	if (xmt_vif < numvifs) {
		MC_SEND(ip, viftable + xmt_vif, m);
		return (1);
	}
#endif /* RSVP_ISI */

	/*
	 * Don't forward if it didn't arrive from the parent vif for its origin.
	 */
	vifi = rt->mfc_parent;
	if ((vifi >= numvifs) || (viftable[vifi].v_ifp != ifp)) {
		/* came in the wrong interface */
		if (mrtdebug & DEBUG_FORWARD)
			log(LOG_DEBUG, "wrong if: ifp %p vifi %d vififp %p\n",
			    ifp, vifi,
			    vifi >= numvifs ? 0 : viftable[vifi].v_ifp);
		++mrtstat.mrts_wrong_if;
		++rt->mfc_wrong_if;
		/*
		 * If we are doing PIM assert processing, and we are forwarding
		 * packets on this interface, and it is a broadcast medium
		 * interface (and not a tunnel), send a message to the routing daemon.
		 */
		if (pim_assert && rt->mfc_ttls[vifi] &&
		    (ifp->if_flags & IFF_BROADCAST) &&
		    !(viftable[vifi].v_flags & VIFF_TUNNEL)) {
			struct timeval now;
			u_int32_t delta;

			/* Get vifi for the incoming packet */
			for (vifi = 0;
			     vifi < numvifs && viftable[vifi].v_ifp != ifp;
			     vifi++)
			    ;
			if (vifi >= numvifs) {
				/* The iif is not found: ignore the packet. */
				return (0);
			}

			microtime(&now);

			TV_DELTA(rt->mfc_last_assert, now, delta);

			if (delta > ASSERT_MSG_TIME) {
				struct igmpmsg *im;
				int hlen = ip->ip_hl << 2;
				struct mbuf *mm = m_copy(m, 0, hlen);

				M_PULLUP(mm, hlen);
				if (mm == NULL)
					return (ENOBUFS);

				rt->mfc_last_assert = now;

				im = mtod(mm, struct igmpmsg *);
				im->im_msgtype	= IGMPMSG_WRONGVIF;
				im->im_mbz	= 0;
				im->im_vif	= vifi;

				mrtstat.mrts_upcalls++;

				sin.sin_addr = im->im_src;
				if (socket_send(ip_mrouter, mm, &sin) < 0) {
					log(LOG_WARNING,
					    "ip_mforward: ip_mrouter socket queue full\n");
					++mrtstat.mrts_upq_sockfull;
					return (ENOBUFS);
				}
			}
		}
		return (0);
	}

	/* If I sourced this packet, it counts as output, else it was input. */
	if (in_hosteq(ip->ip_src, viftable[vifi].v_lcl_addr)) {
		viftable[vifi].v_pkt_out++;
		viftable[vifi].v_bytes_out += plen;
	} else {
		viftable[vifi].v_pkt_in++;
		viftable[vifi].v_bytes_in += plen;
	}
	rt->mfc_pkt_cnt++;
	rt->mfc_byte_cnt += plen;

	/*
	 * For each vif, decide if a copy of the packet should be forwarded.
	 * Forward if:
	 *		- the ttl exceeds the vif's threshold
	 *		- there are group members downstream on interface
	 */
	for (vifp = viftable, vifi = 0; vifi < numvifs; vifp++, vifi++)
		if ((rt->mfc_ttls[vifi] > 0) &&
			(ip->ip_ttl > rt->mfc_ttls[vifi])) {
			vifp->v_pkt_out++;
			vifp->v_bytes_out += plen;
			MC_SEND(ip, vifp, m);
		}

	return (0);
}

#ifdef RSVP_ISI
/*
 * check if a vif number is legal/ok. This is used by ip_output.
 */
int
legal_vif_num(int vif)
{
	if (vif >= 0 && vif < numvifs)
		return (1);
	else
		return (0);
}
#endif /* RSVP_ISI */

static void
phyint_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
	struct mbuf *mb_copy;
	int hlen = ip->ip_hl << 2;

	/*
	 * Make a new reference to the packet; make sure that
	 * the IP header is actually copied, not just referenced,
	 * so that ip_output() only scribbles on the copy.
	 */
	mb_copy = m_copy(m, 0, M_COPYALL);
	M_PULLUP(mb_copy, hlen);
	if (mb_copy == NULL)
		return;

	if (vifp->v_rate_limit <= 0)
		tbf_send_packet(vifp, mb_copy);
	else
		tbf_control(vifp, mb_copy, mtod(mb_copy, struct ip *),
		    ntohs(ip->ip_len));
}

static void
encap_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
	struct mbuf *mb_copy;
	struct ip *ip_copy;
	int i, len = ntohs(ip->ip_len) + sizeof(multicast_encap_iphdr);

	/* Take care of delayed checksums */
	if (m->m_pkthdr.csum & (M_TCPV4_CSUM_OUT | M_UDPV4_CSUM_OUT)) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum &= ~(M_UDPV4_CSUM_OUT | M_TCPV4_CSUM_OUT);
	}

	/*
	 * copy the old packet & pullup it's IP header into the
	 * new mbuf so we can modify it.  Try to fill the new
	 * mbuf since if we don't the ethernet driver will.
	 */
	MGETHDR(mb_copy, M_DONTWAIT, MT_DATA);
	if (mb_copy == NULL)
		return;
	mb_copy->m_data += max_linkhdr;
	mb_copy->m_pkthdr.len = len;
	mb_copy->m_len = sizeof(multicast_encap_iphdr);

	if ((mb_copy->m_next = m_copy(m, 0, M_COPYALL)) == NULL) {
		m_freem(mb_copy);
		return;
	}
	i = MHLEN - max_linkhdr;
	if (i > len)
		i = len;
	mb_copy = m_pullup(mb_copy, i);
	if (mb_copy == NULL)
		return;

	/*
	 * fill in the encapsulating IP header.
	 */
	ip_copy = mtod(mb_copy, struct ip *);
	*ip_copy = multicast_encap_iphdr;
	ip_copy->ip_id = htons(ip_randomid());
	ip_copy->ip_len = htons(len);
	ip_copy->ip_src = vifp->v_lcl_addr;
	ip_copy->ip_dst = vifp->v_rmt_addr;

	/*
	 * turn the encapsulated IP header back into a valid one.
	 */
	ip = (struct ip *)((caddr_t)ip_copy + sizeof(multicast_encap_iphdr));
	--ip->ip_ttl;
	ip->ip_sum = 0;
	mb_copy->m_data += sizeof(multicast_encap_iphdr);
	ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
	mb_copy->m_data -= sizeof(multicast_encap_iphdr);

	if (vifp->v_rate_limit <= 0)
		tbf_send_packet(vifp, mb_copy);
	else
		tbf_control(vifp, mb_copy, ip, ntohs(ip_copy->ip_len));
}

/*
 * De-encapsulate a packet and feed it back through ip input (this
 * routine is called whenever IP gets a packet with proto type
 * ENCAP_PROTO and a local destination address).
 */
void
ipip_mroute_input(struct mbuf *m, ...)
{
	int hlen;
	struct ip *ip = mtod(m, struct ip *);
	int s;
	struct ifqueue *ifq;
	struct vif *vifp;
	va_list ap;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	if (!have_encap_tunnel) {
		rip_input(m, 0);
		return;
	}

	/*
	 * dump the packet if we don't have an encapsulating tunnel
	 * with the source.
	 * Note:  This code assumes that the remote site IP address
	 * uniquely identifies the tunnel (i.e., that this site has
	 * at most one tunnel with the remote site).
	 */
	if (!in_hosteq(ip->ip_src, last_encap_src)) {
		struct vif *vife;

		vifp = viftable;
		vife = vifp + numvifs;
		for (; vifp < vife; vifp++)
			if (vifp->v_flags & VIFF_TUNNEL &&
			    in_hosteq(vifp->v_rmt_addr, ip->ip_src))
				break;
		if (vifp == vife) {
			mrtstat.mrts_cant_tunnel++; /*XXX*/
			m_freem(m);
			if (mrtdebug)
				log(LOG_DEBUG,
				    "ip_mforward: no tunnel with %x\n",
				    ntohl(ip->ip_src.s_addr));
			return;
		}
		last_encap_vif = vifp;
		last_encap_src = ip->ip_src;
	} else
		vifp = last_encap_vif;

	m->m_data += hlen;
	m->m_len -= hlen;
	m->m_pkthdr.len -= hlen;
	m->m_pkthdr.rcvif = vifp->v_ifp;
	ifq = &ipintrq;
	s = splnet();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
	} else {
		IF_ENQUEUE(ifq, m);
		/*
		 * normally we would need a "schednetisr(NETISR_IP)"
		 * here but we were called by ip_input and it is going
		 * to loop back & try to dequeue the packet we just
		 * queued as soon as we return so we avoid the
		 * unnecessary software interrrupt.
		 */
	}
	splx(s);
}

/*
 * Token bucket filter module
 */
static void
tbf_control(struct vif *vifp, struct mbuf *m, struct ip *ip, u_int32_t len)
{

	if (len > MAX_BKT_SIZE) {
		/* drop if packet is too large */
		mrtstat.mrts_pkt2large++;
		m_freem(m);
		return;
	}

	tbf_update_tokens(vifp);

	/*
	 * If there are enough tokens, and the queue is empty, send this packet
	 * out immediately.  Otherwise, try to insert it on this vif's queue.
	 */
	if (vifp->tbf_q_len == 0) {
		if (len <= vifp->tbf_n_tok) {
			vifp->tbf_n_tok -= len;
			tbf_send_packet(vifp, m);
		} else {
			/* queue packet and timeout till later */
			tbf_queue(vifp, m);
			timeout_add(&vifp->v_repq_ch, TBF_REPROCESS);
		}
	} else {
		if (vifp->tbf_q_len >= vifp->tbf_max_q_len &&
		    !tbf_dq_sel(vifp, ip)) {
			/* queue full, and couldn't make room */
			mrtstat.mrts_q_overflow++;
			m_freem(m);
		} else {
			/* queue length low enough, or made room */
			tbf_queue(vifp, m);
			tbf_process_q(vifp);
		}
	}
}

/*
 * adds a packet to the queue at the interface
 */
static void
tbf_queue(struct vif *vifp, struct mbuf *m)
{
	int s = splsoftnet();

	/* insert at tail */
	*vifp->tbf_t = m;
	vifp->tbf_t = &m->m_nextpkt;
	vifp->tbf_q_len++;

	splx(s);
}


/*
 * processes the queue at the interface
 */
static void
tbf_process_q(struct vif *vifp)
{
	struct mbuf *m;
	int len;
	int s = splsoftnet();

	/*
	 * Loop through the queue at the interface and send as many packets
	 * as possible.
	 */
	for (m = vifp->tbf_q; m != NULL; m = vifp->tbf_q) {
		len = ntohs(mtod(m, struct ip *)->ip_len);

		/* determine if the packet can be sent */
		if (len <= vifp->tbf_n_tok) {
			/* if so,
			 * reduce no of tokens, dequeue the packet,
			 * send the packet.
			 */
			if ((vifp->tbf_q = m->m_nextpkt) == NULL)
				vifp->tbf_t = &vifp->tbf_q;
			--vifp->tbf_q_len;

			m->m_nextpkt = NULL;
			vifp->tbf_n_tok -= len;
			tbf_send_packet(vifp, m);
		} else
			break;
	}
	splx(s);
}

static void
tbf_reprocess_q(void *arg)
{
	struct vif *vifp = arg;

	if (ip_mrouter == NULL)
		return;

	tbf_update_tokens(vifp);
	tbf_process_q(vifp);

	if (vifp->tbf_q_len != 0)
		timeout_add(&vifp->v_repq_ch, TBF_REPROCESS);
}

/* function that will selectively discard a member of the queue
 * based on the precedence value and the priority
 */
static int
tbf_dq_sel(struct vif *vifp, struct ip *ip)
{
	u_int p;
	struct mbuf **mp, *m;
	int s = splsoftnet();

	p = priority(vifp, ip);

	for (mp = &vifp->tbf_q, m = *mp;
	    m != NULL;
	    mp = &m->m_nextpkt, m = *mp) {
		if (p > priority(vifp, mtod(m, struct ip *))) {
			if ((*mp = m->m_nextpkt) == NULL)
				vifp->tbf_t = mp;
			--vifp->tbf_q_len;

			m_freem(m);
			mrtstat.mrts_drop_sel++;
			splx(s);
			return (1);
		}
	}
	splx(s);
	return (0);
}

static void
tbf_send_packet(struct vif *vifp, struct mbuf *m)
{
	int error;
	int s = splsoftnet();

	if (vifp->v_flags & VIFF_TUNNEL) {
		/* If tunnel options */
		ip_output(m, (struct mbuf *)NULL, &vifp->v_route,
		    IP_FORWARDING, (struct ip_moptions *)NULL,
		    (struct socket *)NULL);
	} else {
		/* if physical interface option, extract the options and then send */
		struct ip_moptions imo;

		imo.imo_multicast_ifp = vifp->v_ifp;
		imo.imo_multicast_ttl = mtod(m, struct ip *)->ip_ttl - 1;
		imo.imo_multicast_loop = 1;
#ifdef RSVP_ISI
		imo.imo_multicast_vif = -1;
#endif

		error = ip_output(m, (struct mbuf *)NULL, (struct route *)NULL,
		    IP_FORWARDING|IP_MULTICASTOPTS, &imo,
		    (struct socket *)NULL);

		if (mrtdebug & DEBUG_XMIT)
			log(LOG_DEBUG, "phyint_send on vif %ld err %d\n",
			    (long)(vifp - viftable), error);
	}
	splx(s);
}

/* determine the current time and then
 * the elapsed time (between the last time and time now)
 * in milliseconds & update the no. of tokens in the bucket
 */
static void
tbf_update_tokens(struct vif *vifp)
{
	struct timeval tp;
	u_int32_t tm;
	int s = splsoftnet();

	microtime(&tp);

	TV_DELTA(tp, vifp->tbf_last_pkt_t, tm);

	/*
	 * This formula is actually
	 * "time in seconds" * "bytes/second".
	 *
	 * (tm / 1000000) * (v_rate_limit * 1000 * (1000/1024) / 8)
	 *
	 * The (1000/1024) was introduced in add_vif to optimize
	 * this divide into a shift.
	 */
	vifp->tbf_n_tok += tm * vifp->v_rate_limit / 8192;
	vifp->tbf_last_pkt_t = tp;

	if (vifp->tbf_n_tok > MAX_BKT_SIZE)
		vifp->tbf_n_tok = MAX_BKT_SIZE;

	splx(s);
}

static int
priority(struct vif *vifp, struct ip *ip)
{
	int prio = 50;	/* the lowest priority -- default case */

	/* temporary hack; may add general packet classifier some day */

	/*
	 * The UDP port space is divided up into four priority ranges:
	 * [0, 16384)     : unclassified - lowest priority
	 * [16384, 32768) : audio - highest priority
	 * [32768, 49152) : whiteboard - medium priority
	 * [49152, 65536) : video - low priority
	 */
	if (ip->ip_p == IPPROTO_UDP) {
		struct udphdr *udp = (struct udphdr *)(((char *)ip) + (ip->ip_hl << 2));

		switch (ntohs(udp->uh_dport) & 0xc000) {
		case 0x4000:
			prio = 70;
			break;
		case 0x8000:
			prio = 60;
			break;
		case 0xc000:
			prio = 55;
			break;
		}

		if (tbfdebug > 1)
			log(LOG_DEBUG, "port %x prio %d\n",
			    ntohs(udp->uh_dport), prio);
	}

	return (prio);
}

/*
 * End of token bucket filter modifications
 */
#ifdef RSVP_ISI
int
ip_rsvp_vif_init(struct socket *so, struct mbuf *m)
{
	int vifi, s;

	if (rsvpdebug)
		printf("ip_rsvp_vif_init: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
		return (EOPNOTSUPP);

	/* Check mbuf. */
	if (m == NULL || m->m_len != sizeof(int)) {
		return (EINVAL);
	}
	vifi = *(mtod(m, int *));

	if (rsvpdebug)
		printf("ip_rsvp_vif_init: vif = %d rsvp_on = %d\n",
		       vifi, rsvp_on);

	s = splsoftnet();

	/* Check vif. */
	if (!legal_vif_num(vifi)) {
		splx(s);
		return (EADDRNOTAVAIL);
	}

	/* Check if socket is available. */
	if (viftable[vifi].v_rsvpd != NULL) {
		splx(s);
		return (EADDRINUSE);
	}

	viftable[vifi].v_rsvpd = so;
	/* This may seem silly, but we need to be sure we don't over-increment
	 * the RSVP counter, in case something slips up.
	 */
	if (!viftable[vifi].v_rsvp_on) {
		viftable[vifi].v_rsvp_on = 1;
		rsvp_on++;
	}

	splx(s);
	return (0);
}

int
ip_rsvp_vif_done(struct socket *so, struct mbuf *m)
{
	int vifi, s;

	if (rsvpdebug)
		printf("ip_rsvp_vif_done: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
		return (EOPNOTSUPP);

	/* Check mbuf. */
	if (m == NULL || m->m_len != sizeof(int)) {
		return (EINVAL);
	}
	vifi = *(mtod(m, int *));

	s = splsoftnet();

	/* Check vif. */
	if (!legal_vif_num(vifi)) {
		splx(s);
		return (EADDRNOTAVAIL);
	}

	if (rsvpdebug)
		printf("ip_rsvp_vif_done: v_rsvpd = %x so = %x\n",
		    viftable[vifi].v_rsvpd, so);

	viftable[vifi].v_rsvpd = NULL;
	/*
	 * This may seem silly, but we need to be sure we don't over-decrement
	 * the RSVP counter, in case something slips up.
	 */
	if (viftable[vifi].v_rsvp_on) {
		viftable[vifi].v_rsvp_on = 0;
		rsvp_on--;
	}

	splx(s);
	return (0);
}

void
ip_rsvp_force_done(struct socket *so)
{
	int vifi, s;

	/* Don't bother if it is not the right type of socket. */
	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
		return;

	s = splsoftnet();

	/*
	 * The socket may be attached to more than one vif...this
	 * is perfectly legal.
	 */
	for (vifi = 0; vifi < numvifs; vifi++) {
		if (viftable[vifi].v_rsvpd == so) {
			viftable[vifi].v_rsvpd = NULL;
			/*
			 * This may seem silly, but we need to be sure we don't
			 * over-decrement the RSVP counter, in case something
			 * slips up.
			 */
			if (viftable[vifi].v_rsvp_on) {
				viftable[vifi].v_rsvp_on = 0;
				rsvp_on--;
			}
		}
	}

	splx(s);
	return;
}

void
rsvp_input(struct mbuf *m, struct ifnet *ifp)
{
	int vifi, s;
	struct ip *ip = mtod(m, struct ip *);
	static struct sockaddr_in rsvp_src = { sizeof(sin), AF_INET };

	if (rsvpdebug)
		printf("rsvp_input: rsvp_on %d\n", rsvp_on);

	/*
	 * Can still get packets with rsvp_on = 0 if there is a local member
	 * of the group to which the RSVP packet is addressed.  But in this
	 * case we want to throw the packet away.
	 */
	if (!rsvp_on) {
		m_freem(m);
		return;
	}

	/*
	 * If the old-style non-vif-associated socket is set, then use
	 * it and ignore the new ones.
	 */
	if (ip_rsvpd != NULL) {
		if (rsvpdebug)
			printf("rsvp_input: "
			    "Sending packet up old-style socket\n");
		rip_input(m, 0);	/*XXX*/
		return;
	}

	s = splsoftnet();

	if (rsvpdebug)
		printf("rsvp_input: check vifs\n");

	/* Find which vif the packet arrived on. */
	for (vifi = 0; vifi < numvifs; vifi++) {
		if (viftable[vifi].v_ifp == ifp)
			break;
	}

	if (vifi == numvifs) {
		/* Can't find vif packet arrived on. Drop packet. */
		if (rsvpdebug)
			printf("rsvp_input: "
			    "Can't find vif for packet...dropping it.\n");
		m_freem(m);
		splx(s);
		return;
	}

	if (rsvpdebug)
		printf("rsvp_input: check socket\n");

	if (viftable[vifi].v_rsvpd == NULL) {
		/*
	 	 * drop packet, since there is no specific socket for this
		 * interface
		 */
		if (rsvpdebug)
			printf("rsvp_input: No socket defined for vif %d\n",
			    vifi);
		m_freem(m);
		splx(s);
		return;
	}

	rsvp_src.sin_addr = ip->ip_src;

	if (rsvpdebug && m)
		printf("rsvp_input: m->m_len = %d, sbspace() = %d\n",
		    m->m_len, sbspace(&viftable[vifi].v_rsvpd->so_rcv));

	if (socket_send(viftable[vifi].v_rsvpd, m, &rsvp_src) < 0)
		if (rsvpdebug)
			printf("rsvp_input: Failed to append to socket\n");
	else
		if (rsvpdebug)
			printf("rsvp_input: send packet up\n");

	splx(s);
}
#endif /* RSVP_ISI */
