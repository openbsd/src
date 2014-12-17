/*	$OpenBSD: ip_mroute.c,v 1.74 2014/12/17 09:57:13 mpi Exp $	*/
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
 * and PIM-SMv2 and PIM-DM support, advanced API support,
 * bandwidth metering and signaling
 */

#ifdef PIM
#define _PIM_VT 1
#endif

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
#include <sys/sysctl.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_mroute.h>
#ifdef PIM
#include <netinet/pim.h>
#include <netinet/pim_var.h>
#endif

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

#define NO_RTE_FOUND	0x1
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
#define		DEBUG_PIM	0x20

#define		VIFI_INVALID	((vifi_t) -1)

#define		EXPIRE_TIMEOUT	250		/* 4x / second */
#define		UPCALL_EXPIRE	6		/* number of timeouts */
struct timeout	expire_upcalls_ch;

static int get_sg_cnt(struct sioc_sg_req *);
static int get_vif_cnt(struct sioc_vif_req *);
static int ip_mrouter_init(struct socket *, struct mbuf *);
static int get_version(struct mbuf *);
static int set_assert(struct mbuf *);
static int get_assert(struct mbuf *);
static int add_vif(struct mbuf *);
static int del_vif(struct mbuf *);
static void update_mfc_params(struct mfc *, struct mfcctl2 *);
static void init_mfc_params(struct mfc *, struct mfcctl2 *);
static void expire_mfc(struct mfc *);
static int add_mfc(struct mbuf *);
static int del_mfc(struct mbuf *);
static int set_api_config(struct mbuf *); /* chose API capabilities */
static int get_api_support(struct mbuf *);
static int get_api_config(struct mbuf *);
static int socket_send(struct socket *, struct mbuf *,
			    struct sockaddr_in *);
static void expire_upcalls(void *);
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *);
static void phyint_send(struct ip *, struct vif *, struct mbuf *);
static void encap_send(struct ip *, struct vif *, struct mbuf *);
static void send_packet(struct vif *, struct mbuf *);

#ifdef PIM
static int pim_register_send(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static int pim_register_send_rp(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static int pim_register_send_upcall(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static struct mbuf *pim_register_prepare(struct ip *, struct mbuf *);
#endif

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

#ifdef PIM
struct pimstat pimstat;

/*
 * Note: the PIM Register encapsulation adds the following in front of a
 * data packet:
 *
 * struct pim_encap_hdr {
 *    struct ip ip;
 *    struct pim_encap_pimhdr  pim;
 * }
 *
 */

struct pim_encap_pimhdr {
	struct pim pim;
	uint32_t   flags;
};

static struct ip pim_encap_iphdr = {
#if BYTE_ORDER == LITTLE_ENDIAN
	sizeof(struct ip) >> 2,
	IPVERSION,
#else
	IPVERSION,
	sizeof(struct ip) >> 2,
#endif
	0,			/* tos */
	sizeof(struct ip),	/* total length */
	0,			/* id */
	0,			/* frag offset */ 
	ENCAP_TTL,
	IPPROTO_PIM,
	0,			/* checksum */
};

static struct pim_encap_pimhdr pim_encap_pimhdr = {
    {
	PIM_MAKE_VT(PIM_VERSION, PIM_REGISTER), /* PIM vers and message type */
	0,			/* reserved */
	0,			/* checksum */
    },
    0				/* flags */
};

static struct ifnet multicast_register_if;
static vifi_t reg_vif_num = VIFI_INVALID;
#endif /* PIM */


/*
 * Private variables.
 */
static vifi_t	   numvifs = 0;
static int have_encap_tunnel = 0;

/*
 * whether or not special PIM assert processing is enabled.
 */
static int pim_assert;
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
					  MRT_MFC_FLAGS_BORDER_VIF |
					  MRT_MFC_RP);
static u_int32_t mrt_api_config = 0;

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
		case MRT_API_CONFIG:
			error = set_api_config(*m);
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
		case MRT_API_SUPPORT:
			error = get_api_support(*m);
			break;
		case MRT_API_CONFIG:
			error = get_api_config(*m);
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
			error = ENOTTY;
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
	memset(nexpire, 0, sizeof(nexpire));

	pim_assert = 0;

	timeout_set(&expire_upcalls_ch, expire_upcalls, NULL);
	timeout_add_msec(&expire_upcalls_ch, EXPIRE_TIMEOUT);

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
	mrt_api_config = 0;

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

	memset(nexpire, 0, sizeof(nexpire));
	free(mfchashtbl, M_MRTABLE, 0);
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

/*
 * Configure API capabilities
 */
static int
set_api_config(struct mbuf *m)
{
	int i;
	u_int32_t *apival;

	if (m == NULL || m->m_len < sizeof(u_int32_t))
		return (EINVAL);

	apival = mtod(m, u_int32_t *);

	/*
	 * We can set the API capabilities only if it is the first operation
	 * after MRT_INIT. I.e.:
	 *  - there are no vifs installed
	 *  - pim_assert is not enabled
	 *  - the MFC table is empty
	 */
	if (numvifs > 0) {
		*apival = 0;
		return (EPERM);
	}
	if (pim_assert) {
		*apival = 0;
		return (EPERM);
	}
	for (i = 0; i < MFCTBLSIZ; i++) {
		if (LIST_FIRST(&mfchashtbl[i]) != NULL) {
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
static int
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
static int
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
#ifdef PIM
	if (vifcp->vifc_flags & VIFF_REGISTER) {
		/*
		 * XXX: Because VIFF_REGISTER does not really need a valid
		 * local interface (e.g. it could be 127.0.0.2), we don't
		 * check its address.
		 */
	} else
#endif
	{
		sin.sin_addr = vifcp->vifc_lcl_addr;
		ifa = ifa_ifwithaddr(sintosa(&sin), /* XXX */ 0);
		if (ifa == NULL)
			return (EADDRNOTAVAIL);
	}

	if (vifcp->vifc_flags & VIFF_TUNNEL) {
		/* tunnels are no longer supported use gif(4) instead */
		return (EOPNOTSUPP);
#ifdef PIM
	} else if (vifcp->vifc_flags & VIFF_REGISTER) {
		ifp = &multicast_register_if;
		if (mrtdebug)
			log(LOG_DEBUG, "Adding a register vif, ifp: %p\n",
			    (void *)ifp);
		if (reg_vif_num == VIFI_INVALID) {
			memset(ifp, 0, sizeof(*ifp));
			snprintf(ifp->if_xname, sizeof ifp->if_xname,
				 "register_vif");
			ifp->if_flags = IFF_LOOPBACK;
			memset(&vifp->v_route, 0, sizeof(vifp->v_route));
			reg_vif_num = vifcp->vifc_vifi;
		}
#endif
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

	vifp->v_flags = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	vifp->v_lcl_addr = vifcp->vifc_lcl_addr;
	vifp->v_rmt_addr = vifcp->vifc_rmt_addr;
	vifp->v_ifp = ifp;
	/* Initialize per vif pkt counters. */
	vifp->v_pkt_in = 0;
	vifp->v_pkt_out = 0;
	vifp->v_bytes_in = 0;
	vifp->v_bytes_out = 0;

	timeout_del(&vifp->v_repq_ch);

	splx(s);

	/* Adjust numvifs up if the vifi is higher than numvifs. */
	if (numvifs <= vifcp->vifc_vifi)
		numvifs = vifcp->vifc_vifi + 1;

	if (mrtdebug)
		log(LOG_DEBUG, "add_vif #%d, lcladdr %x, %s %x, "
		    "thresh %x\n",
		    vifcp->vifc_vifi,
		    ntohl(vifcp->vifc_lcl_addr.s_addr),
		    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
		    ntohl(vifcp->vifc_rmt_addr.s_addr),
		    vifcp->vifc_threshold);

	return (0);
}

void
reset_vif(struct vif *vifp)
{
	struct ifnet *ifp;
	struct ifreq ifr;

	if (vifp->v_flags & VIFF_TUNNEL) {
		/* empty */
	} else if (vifp->v_flags & VIFF_REGISTER) {
#ifdef PIM
		reg_vif_num = VIFI_INVALID;
#endif
	} else {
		satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = zeroin_addr;
		ifp = vifp->v_ifp;
		(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
	}
	memset(vifp, 0, sizeof(*vifp));
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
			memset(vifp, 0, sizeof(*vifp));
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
update_mfc_params(struct mfc *rt, struct mfcctl2 *mfccp)
{
	int i;

	rt->mfc_parent = mfccp->mfcc_parent;
	for (i = 0; i < numvifs; i++) {
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
static void
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

static void
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
static int
add_mfc(struct mbuf *m)
{
	struct mfcctl2 mfcctl2;
	struct mfcctl2 *mfccp;
	struct mfc *rt;
	u_int32_t hash = 0;
	struct rtdetq *rte, *nrte;
	u_short nstl;
	int s;
	int mfcctl_size = sizeof(struct mfcctl);

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
				log(LOG_ERR, "add_mfc %s o %x g %x "
				    "p %x dbx %p\n",
				    "multiple kernel entries",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);

			if (mrtdebug & DEBUG_MFC)
				log(LOG_DEBUG, "add_mfc o %x g %x "
				    "p %x dbg %p\n",
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

/*
 * Delete an mfc entry
 */
static int
del_mfc(struct mbuf *m)
{
	struct mfcctl2 mfcctl2;
	struct mfcctl2 *mfccp;
	struct mfc *rt;
	int s;
	int mfcctl_size = sizeof(struct mfcctl);
	struct mfcctl *mp = mtod(m, struct mfcctl *);

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
	free(rt, M_MRTABLE, 0);

	splx(s);
	return (0);
}

static int
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
	s = splsoftnet();
	++mrtstat.mrts_mfc_lookups;
	rt = mfc_find(&ip->ip_src, &ip->ip_dst);

	/* Entry exists, so forward if necessary */
	if (rt != NULL) {
		splx(s);
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
		if (mrtdebug & (DEBUG_FORWARD | DEBUG_MFC))
			log(LOG_DEBUG, "ip_mforward: no rte s %x g %x\n",
			    ntohl(ip->ip_src.s_addr),
			    ntohl(ip->ip_dst.s_addr));

		/*
		 * Allocate mbufs early so that we don't do extra work if we are
		 * just going to fail anyway.  Make sure to pullup the header so
		 * that other people can't step on it.
		 */
		rte = (struct rtdetq *)malloc(sizeof(*rte),
		    M_MRTABLE, M_NOWAIT);
		if (rte == NULL) {
			splx(s);
			return (ENOBUFS);
		}
		mb0 = m_copy(m, 0, M_COPYALL);
		M_PULLUP(mb0, hlen);
		if (mb0 == NULL) {
			free(rte, M_MRTABLE, 0);
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
			rt = (struct mfc *)malloc(sizeof(*rt),
			    M_MRTABLE, M_NOWAIT);
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
			nexpire[hash]++;
			for (i = 0; i < numvifs; i++) {
				rt->mfc_ttls[i] = 0;
				rt->mfc_flags[i] = 0;
			}
			rt->mfc_parent = -1;

			/* clear the RP address */
			rt->mfc_rp = zeroin_addr;

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
					free(rte, M_MRTABLE, 0);
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
	timeout_add_msec(&expire_upcalls_ch, EXPIRE_TIMEOUT);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt)
{
	struct ip  *ip = mtod(m, struct ip *);
	vifi_t vifi;
	struct vif *vifp;
	int plen = ntohs(ip->ip_len) - (ip->ip_hl << 2);

/*
 * Macro to send packet on vif.
 */
#define MC_SEND(ip, vifp, m) do {					\
	if ((vifp)->v_flags & VIFF_TUNNEL)				\
		encap_send((ip), (vifp), (m));				\
	else								\
		phyint_send((ip), (vifp), (m));				\
} while (/*CONSTCOND*/ 0)

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
		 * If we are doing PIM assert processing, send a message
		 * to the routing daemon.
		 *
		 * XXX: A PIM-SM router needs the WRONGVIF detection so it
		 * can complete the SPT switch, regardless of the type
		 * of interface (broadcast media, GRE tunnel, etc).
		 */
		if (pim_assert && (vifi < numvifs) && viftable[vifi].v_ifp) {
			struct timeval now;
			u_int32_t delta;

#ifdef PIM
			if (ifp == &multicast_register_if)
				pimstat.pims_rcv_registers_wrongiif++;
#endif

			/* Get vifi for the incoming packet */
			for (vifi = 0;
			     vifi < numvifs && viftable[vifi].v_ifp != ifp;
			     vifi++)
			    ;
			if (vifi >= numvifs) {
				/* The iif is not found: ignore the packet. */
				return (0);
			}

			if (rt->mfc_flags[vifi] &
			    MRT_MFC_FLAGS_DISABLE_WRONGVIF) {
				/* WRONGVIF disabled: ignore the packet */
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
					log(LOG_WARNING, "ip_mforward: "
					    "ip_mrouter socket queue full\n");
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
#ifdef PIM
			if (vifp->v_flags & VIFF_REGISTER)
				pim_register_send(ip, vifp, m, rt);
			else
#endif
			MC_SEND(ip, vifp, m);
		}

	return (0);
}

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

	send_packet(vifp, mb_copy);
}

static void
encap_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
	struct mbuf *mb_copy;
	struct ip *ip_copy;
	int i, len = ntohs(ip->ip_len) + sizeof(multicast_encap_iphdr);

	in_proto_cksum_out(m, NULL);

	/*
	 * copy the old packet & pullup its IP header into the
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

	send_packet(vifp, mb_copy);
}

static void
send_packet(struct vif *vifp, struct mbuf *m)
{
	int error;
	int s = splsoftnet();

	if (vifp->v_flags & VIFF_TUNNEL) {
		/* If tunnel options */
		ip_output(m, NULL, &vifp->v_route, IP_FORWARDING, NULL, NULL,
		    0);
	} else {
		/*
		 * if physical interface option, extract the options
		 * and then send
		 */
		struct ip_moptions imo;

		imo.imo_ifidx = vifp->v_ifp->if_index;
		imo.imo_ttl = mtod(m, struct ip *)->ip_ttl - IPTTLDEC;
		imo.imo_loop = 1;

		error = ip_output(m, NULL, NULL,
		    IP_FORWARDING | IP_MULTICASTOPTS, &imo, NULL, 0);

		if (mrtdebug & DEBUG_XMIT)
			log(LOG_DEBUG, "phyint_send on vif %ld err %d\n",
			    (long)(vifp - viftable), error);
	}
	splx(s);
}

#ifdef PIM
/*
 * Send the packet up to the user daemon, or eventually do kernel encapsulation
 */
static int
pim_register_send(struct ip *ip, struct vif *vifp,
	struct mbuf *m, struct mfc *rt)
{
	struct mbuf *mb_copy, *mm;

	if (mrtdebug & DEBUG_PIM)
		log(LOG_DEBUG, "pim_register_send: ");

	mb_copy = pim_register_prepare(ip, m);
	if (mb_copy == NULL)
		return (ENOBUFS);

	/*
	 * Send all the fragments. Note that the mbuf for each fragment
	 * is freed by the sending machinery.
	 */
	for (mm = mb_copy; mm; mm = mb_copy) {
		mb_copy = mm->m_nextpkt;
		mm->m_nextpkt = NULL;
		mm = m_pullup(mm, sizeof(struct ip));
		if (mm != NULL) {
			ip = mtod(mm, struct ip *);
			if ((mrt_api_config & MRT_MFC_RP) &&
			    !in_nullhost(rt->mfc_rp)) {
				pim_register_send_rp(ip, vifp, mm, rt);
			} else {
				pim_register_send_upcall(ip, vifp, mm, rt);
			}
		}
	}

	return (0);
}

/*
 * Return a copy of the data packet that is ready for PIM Register
 * encapsulation.
 * XXX: Note that in the returned copy the IP header is a valid one.
 */
static struct mbuf *
pim_register_prepare(struct ip *ip, struct mbuf *m)
{
	struct mbuf *mb_copy = NULL;
	int mtu;

	in_proto_cksum_out(m, NULL);

	/*
	 * Copy the old packet & pullup its IP header into the
	 * new mbuf so we can modify it.
	 */
	mb_copy = m_copy(m, 0, M_COPYALL);
	if (mb_copy == NULL)
		return (NULL);
	mb_copy = m_pullup(mb_copy, ip->ip_hl << 2);
	if (mb_copy == NULL)
		return (NULL);

	/* take care of the TTL */
	ip = mtod(mb_copy, struct ip *);
	--ip->ip_ttl;

	/* Compute the MTU after the PIM Register encapsulation */
	mtu = 0xffff - sizeof(pim_encap_iphdr) - sizeof(pim_encap_pimhdr);

	if (ntohs(ip->ip_len) <= mtu) {
		/* Turn the IP header into a valid one */
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
	} else {
		/* Fragment the packet */
		if (ip_fragment(mb_copy, NULL, mtu) != 0) {
			/* XXX: mb_copy was freed by ip_fragment() */
			return (NULL);
		}
	}
	return (mb_copy);
}

/*
 * Send an upcall with the data packet to the user-level process.
 */
static int
pim_register_send_upcall(struct ip *ip, struct vif *vifp,
	struct mbuf *mb_copy, struct mfc *rt)
{
	struct mbuf *mb_first;
	int len = ntohs(ip->ip_len);
	struct igmpmsg *im;
	struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };

	/* Add a new mbuf with an upcall header */
	MGETHDR(mb_first, M_DONTWAIT, MT_HEADER);
	if (mb_first == NULL) {
		m_freem(mb_copy);
		return (ENOBUFS);
	}
	mb_first->m_data += max_linkhdr;
	mb_first->m_pkthdr.len = len + sizeof(struct igmpmsg);
	mb_first->m_len = sizeof(struct igmpmsg);
	mb_first->m_next = mb_copy;

	/* Send message to routing daemon */
	im = mtod(mb_first, struct igmpmsg *);
	im->im_msgtype = IGMPMSG_WHOLEPKT;
	im->im_mbz = 0;
	im->im_vif = vifp - viftable;
	im->im_src = ip->ip_src;
	im->im_dst = ip->ip_dst;

	k_igmpsrc.sin_addr = ip->ip_src;

	mrtstat.mrts_upcalls++;

	if (socket_send(ip_mrouter, mb_first, &k_igmpsrc) < 0) {
		if (mrtdebug & DEBUG_PIM)
			log(LOG_WARNING, "mcast: pim_register_send_upcall: "
			    "ip_mrouter socket queue full");
		++mrtstat.mrts_upq_sockfull;
		return (ENOBUFS);
	}

	/* Keep statistics */
	pimstat.pims_snd_registers_msgs++;
	pimstat.pims_snd_registers_bytes += len;

	return (0);
}

/*
 * Encapsulate the data packet in PIM Register message and send it to the RP.
 */
static int
pim_register_send_rp(struct ip *ip, struct vif *vifp,
	struct mbuf *mb_copy, struct mfc *rt)
{
	struct mbuf *mb_first;
	struct ip *ip_outer;
	struct pim_encap_pimhdr *pimhdr;
	int len = ntohs(ip->ip_len);
	vifi_t vifi = rt->mfc_parent;

	if ((vifi >= numvifs) || in_nullhost(viftable[vifi].v_lcl_addr)) {
		m_freem(mb_copy);
		return (EADDRNOTAVAIL);		/* The iif vif is invalid */
	}

	/* Add a new mbuf with the encapsulating header */
	MGETHDR(mb_first, M_DONTWAIT, MT_HEADER);
	if (mb_first == NULL) {
		m_freem(mb_copy);
		return (ENOBUFS);
	}
	mb_first->m_data += max_linkhdr;
	mb_first->m_len = sizeof(pim_encap_iphdr) + sizeof(pim_encap_pimhdr);
	mb_first->m_next = mb_copy;

	mb_first->m_pkthdr.len = len + mb_first->m_len;

	/* Fill in the encapsulating IP and PIM header */
	ip_outer = mtod(mb_first, struct ip *);
	*ip_outer = pim_encap_iphdr;
	ip_outer->ip_id = htons(ip_randomid());
	ip_outer->ip_len = htons(len + sizeof(pim_encap_iphdr) +
	    sizeof(pim_encap_pimhdr));
	ip_outer->ip_src = viftable[vifi].v_lcl_addr;
	ip_outer->ip_dst = rt->mfc_rp;
	/*
	 * Copy the inner header TOS to the outer header, and take care of the
	 * IP_DF bit.
	 */
	ip_outer->ip_tos = ip->ip_tos;
	if (ntohs(ip->ip_off) & IP_DF)
		ip_outer->ip_off |= htons(IP_DF);
	pimhdr = (struct pim_encap_pimhdr *)((caddr_t)ip_outer
	    + sizeof(pim_encap_iphdr));
	*pimhdr = pim_encap_pimhdr;
	/* If the iif crosses a border, set the Border-bit */
	if (rt->mfc_flags[vifi] & MRT_MFC_FLAGS_BORDER_VIF & mrt_api_config)
		pimhdr->flags |= htonl(PIM_BORDER_REGISTER);

	mb_first->m_data += sizeof(pim_encap_iphdr);
	pimhdr->pim.pim_cksum = in_cksum(mb_first, sizeof(pim_encap_pimhdr));
	mb_first->m_data -= sizeof(pim_encap_iphdr);

	send_packet(vifp, mb_first);

	/* Keep statistics */
	pimstat.pims_snd_registers_msgs++;
	pimstat.pims_snd_registers_bytes += len;

	return (0);
}

/*
 * PIM-SMv2 and PIM-DM messages processing.
 * Receives and verifies the PIM control messages, and passes them
 * up to the listening socket, using rip_input().
 * The only message with special processing is the PIM_REGISTER message
 * (used by PIM-SM): the PIM header is stripped off, and the inner packet
 * is passed to if_simloop().
 */
void
pim_input(struct mbuf *m, ...)
{
	struct ip *ip = mtod(m, struct ip *);
	struct pim *pim;
	int minlen;
	int datalen;
	int ip_tos;
	int iphlen;
	va_list ap;

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	datalen = ntohs(ip->ip_len) - iphlen;

	/* Keep statistics */
	pimstat.pims_rcv_total_msgs++;
	pimstat.pims_rcv_total_bytes += datalen;

	/* Validate lengths */
	if (datalen < PIM_MINLEN) {
		pimstat.pims_rcv_tooshort++;
		log(LOG_ERR, "pim_input: packet size too small %d from %lx\n",
		    datalen, (u_long)ip->ip_src.s_addr);
		m_freem(m);
		return;
	}

	/*
	 * If the packet is at least as big as a REGISTER, go agead
	 * and grab the PIM REGISTER header size, to avoid another
	 * possible m_pullup() later.
	 * 
	 * PIM_MINLEN       == pimhdr + u_int32_t == 4 + 4 = 8
	 * PIM_REG_MINLEN   == pimhdr + reghdr + encap_iphdr == 4 + 4 + 20 = 28
	 */
	minlen = iphlen + (datalen >= PIM_REG_MINLEN ?
	    PIM_REG_MINLEN : PIM_MINLEN);
	/*
	 * Get the IP and PIM headers in contiguous memory, and
	 * possibly the PIM REGISTER header.
	 */
	if ((m->m_flags & M_EXT || m->m_len < minlen) &&
	    (m = m_pullup(m, minlen)) == NULL) {
		log(LOG_ERR, "pim_input: m_pullup failure\n");
		return;
	}
	/* m_pullup() may have given us a new mbuf so reset ip. */
	ip = mtod(m, struct ip *);
	ip_tos = ip->ip_tos;

	/* adjust mbuf to point to the PIM header */
	m->m_data += iphlen;
	m->m_len  -= iphlen;
	pim = mtod(m, struct pim *);

	/*
	 * Validate checksum. If PIM REGISTER, exclude the data packet.
	 *
	 * XXX: some older PIMv2 implementations don't make this distinction,
	 * so for compatibility reason perform the checksum over part of the
	 * message, and if error, then over the whole message.
	 */
	if (PIM_VT_T(pim->pim_vt) == PIM_REGISTER &&
	    in_cksum(m, PIM_MINLEN) == 0) {
		/* do nothing, checksum okay */
	} else if (in_cksum(m, datalen)) {
		pimstat.pims_rcv_badsum++;
		if (mrtdebug & DEBUG_PIM)
			log(LOG_DEBUG, "pim_input: invalid checksum");
		m_freem(m);
		return;
	}

	/* PIM version check */
	if (PIM_VT_V(pim->pim_vt) < PIM_VERSION) {
		pimstat.pims_rcv_badversion++;
		log(LOG_ERR, "pim_input: incorrect version %d, expecting %d\n",
		    PIM_VT_V(pim->pim_vt), PIM_VERSION);
		m_freem(m);
		return;
	}

	/* restore mbuf back to the outer IP */
	m->m_data -= iphlen;
	m->m_len  += iphlen;

	if (PIM_VT_T(pim->pim_vt) == PIM_REGISTER) {
		/*
		 * Since this is a REGISTER, we'll make a copy of the register
		 * headers ip + pim + u_int32 + encap_ip, to be passed up to the
		 * routing daemon.
		 */
		int s;
		struct sockaddr_in dst = { sizeof(dst), AF_INET };
		struct mbuf *mcp;
		struct ip *encap_ip;
		u_int32_t *reghdr;
		struct ifnet *vifp;

		s = splsoftnet();
		if ((reg_vif_num >= numvifs) || (reg_vif_num == VIFI_INVALID)) {
			splx(s);
			if (mrtdebug & DEBUG_PIM)
				log(LOG_DEBUG, "pim_input: register vif "
				    "not set: %d\n", reg_vif_num);
			m_freem(m);
			return;
		}
		/* XXX need refcnt? */
		vifp = viftable[reg_vif_num].v_ifp;
		splx(s);

		/* Validate length */
		if (datalen < PIM_REG_MINLEN) {
			pimstat.pims_rcv_tooshort++;
			pimstat.pims_rcv_badregisters++;
			log(LOG_ERR, "pim_input: register packet size "
			    "too small %d from %lx\n",
			    datalen, (u_long)ip->ip_src.s_addr);
			m_freem(m);
			return;
		}

		reghdr = (u_int32_t *)(pim + 1);
		encap_ip = (struct ip *)(reghdr + 1);

		if (mrtdebug & DEBUG_PIM) {
			log(LOG_DEBUG, "pim_input[register], encap_ip: "
			    "%lx -> %lx, encap_ip len %d\n",
			    (u_long)ntohl(encap_ip->ip_src.s_addr),
			    (u_long)ntohl(encap_ip->ip_dst.s_addr),
			    ntohs(encap_ip->ip_len));
		}

		/* verify the version number of the inner packet */
		if (encap_ip->ip_v != IPVERSION) {
			pimstat.pims_rcv_badregisters++;
			if (mrtdebug & DEBUG_PIM) {
				log(LOG_DEBUG, "pim_input: invalid IP version"
				    " (%d) of the inner packet\n",
				    encap_ip->ip_v);
			}
			m_freem(m);
			return;
		}

		/* verify the inner packet is destined to a mcast group */
		if (!IN_MULTICAST(encap_ip->ip_dst.s_addr)) {
			pimstat.pims_rcv_badregisters++;
			if (mrtdebug & DEBUG_PIM)
				log(LOG_DEBUG,
				    "pim_input: inner packet of register is"
				    " not multicast %lx\n",
				    (u_long)ntohl(encap_ip->ip_dst.s_addr));
			m_freem(m);
			return;
		}

		/* If a NULL_REGISTER, pass it to the daemon */
		if ((ntohl(*reghdr) & PIM_NULL_REGISTER))
			goto pim_input_to_daemon;

		/*
		 * Copy the TOS from the outer IP header to the inner
		 * IP header.
		 */
		if (encap_ip->ip_tos != ip_tos) {
			/* Outer TOS -> inner TOS */
			encap_ip->ip_tos = ip_tos;
			/* Recompute the inner header checksum. Sigh... */

			/* adjust mbuf to point to the inner IP header */
			m->m_data += (iphlen + PIM_MINLEN);
			m->m_len  -= (iphlen + PIM_MINLEN);

			encap_ip->ip_sum = 0;
			encap_ip->ip_sum = in_cksum(m, encap_ip->ip_hl << 2);

			/* restore mbuf to point back to the outer IP header */
			m->m_data -= (iphlen + PIM_MINLEN);
			m->m_len  += (iphlen + PIM_MINLEN);
		}

		/*
		 * Decapsulate the inner IP packet and loopback to forward it
		 * as a normal multicast packet. Also, make a copy of the 
		 *     outer_iphdr + pimhdr + reghdr + encap_iphdr
		 * to pass to the daemon later, so it can take the appropriate
		 * actions (e.g., send back PIM_REGISTER_STOP).
		 * XXX: here m->m_data points to the outer IP header.
		 */
		mcp = m_copy(m, 0, iphlen + PIM_REG_MINLEN);
		if (mcp == NULL) {
			log(LOG_ERR, "pim_input: pim register: could not "
			    "copy register head\n");
			m_freem(m);
			return;
		}

		/* Keep statistics */
		/* XXX: registers_bytes include only the encap. mcast pkt */
		pimstat.pims_rcv_registers_msgs++;
		pimstat.pims_rcv_registers_bytes += ntohs(encap_ip->ip_len);

		/* forward the inner ip packet; point m_data at the inner ip. */
		m_adj(m, iphlen + PIM_MINLEN);

		if (mrtdebug & DEBUG_PIM) {
			log(LOG_DEBUG,
			    "pim_input: forwarding decapsulated register: "
			    "src %lx, dst %lx, vif %d\n",
			    (u_long)ntohl(encap_ip->ip_src.s_addr),
			    (u_long)ntohl(encap_ip->ip_dst.s_addr),
			    reg_vif_num);
		}
		/* NB: vifp was collected above; can it change on us? */
		looutput(vifp, m, (struct sockaddr *)&dst, NULL);

		/* prepare the register head to send to the mrouting daemon */
		m = mcp;
	}

pim_input_to_daemon:
	/*
	 * Pass the PIM message up to the daemon; if it is a Register message,
	 * pass the 'head' only up to the daemon. This includes the
	 * outer IP header, PIM header, PIM-Register header and the
	 * inner IP header.
	 * XXX: the outer IP header pkt size of a Register is not adjust to
	 * reflect the fact that the inner multicast data is truncated.
	 */
	rip_input(m);

	return;
}

/*
 * Sysctl for pim variables.
 */
int
pim_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case PIMCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &pimstat, sizeof(pimstat)));

	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}


#endif /* PIM */
