/*	$OpenBSD: ip_mroute.c,v 1.19 2000/01/21 03:15:05 angelos Exp $	*/
/*	$NetBSD: ip_mroute.c,v 1.27 1996/05/07 02:40:50 thorpej Exp $	*/

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

#include <machine/stdarg.h>

#define IP_MULTICASTOPTS 0
#define	M_PULLUP(m, len) \
	do { \
		if ((m) && ((m)->m_flags & M_EXT || (m)->m_len < (len))) \
			(m) = m_pullup((m), (len)); \
	} while (0)

/*
 * Globals.  All but ip_mrouter and ip_mrtproto could be static,
 * except for netstat or debugging purposes.
 */
struct socket  *ip_mrouter  = NULL;
int		ip_mrtproto = IGMP_DVMRP;    /* for netstat only */

#define NO_RTE_FOUND 	0x1
#define RTE_FOUND	0x2

#define	MFCHASH(a, g) \
	((((a) >> 20) ^ ((a) >> 10) ^ (a) ^ \
	  ((g) >> 20) ^ ((g) >> 10) ^ (g)) & mfchash)
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

/*
 * Define the token bucket filter structures
 * qtable   -> each interface has an associated queue of pkts 
 */

struct pkt_queue qtable[MAXVIFS][MAXQSIZE];

static int get_sg_cnt __P((struct sioc_sg_req *));
static int get_vif_cnt __P((struct sioc_vif_req *));
static int ip_mrouter_init __P((struct socket *, struct mbuf *));
static int get_version __P((struct mbuf *));
static int set_assert __P((struct mbuf *));
static int get_assert __P((struct mbuf *));
static int add_vif __P((struct mbuf *));
static int del_vif __P((struct mbuf *));
static void update_mfc __P((struct mfcctl *, struct mfc *));
static void expire_mfc __P((struct mfc *));
static int add_mfc __P((struct mbuf *));
#ifdef UPCALL_TIMING
static void collate __P((struct timeval *));
#endif
static int del_mfc __P((struct mbuf *));
static int socket_send __P((struct socket *, struct mbuf *,
			    struct sockaddr_in *));
static void expire_upcalls __P((void *));
#ifdef RSVP_ISI
static int ip_mdq __P((struct mbuf *, struct ifnet *, struct mfc *, vifi_t));
#else
static int ip_mdq __P((struct mbuf *, struct ifnet *, struct mfc *));
#endif
static void phyint_send __P((struct ip *, struct vif *, struct mbuf *));
static void encap_send __P((struct ip *, struct vif *, struct mbuf *));
static void tbf_control __P((struct vif *, struct mbuf *, struct ip *,
			     u_int32_t));
static void tbf_queue __P((struct vif *, struct mbuf *, struct ip *));
static void tbf_process_q __P((struct vif *));
static void tbf_dequeue __P((struct vif *, int));
static void tbf_reprocess_q __P((void *));
static int tbf_dq_sel __P((struct vif *, struct ip *));
static void tbf_send_packet __P((struct vif *, struct mbuf *));
static void tbf_update_tokens __P((struct vif *));
static int priority __P((struct vif *, struct ip *));

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
static u_int32_t last_encap_src;
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
 */

#define MFCFIND(o, g, rt) { \
	register struct mfc *_rt; \
	(rt) = NULL; \
	++mrtstat.mrts_mfc_lookups; \
	for (_rt = mfchashtbl[MFCHASH(o, g)].lh_first; \
	     _rt; _rt = _rt->mfc_hash.le_next) { \
		if (_rt->mfc_origin.s_addr == (o) && \
		    _rt->mfc_mcastgrp.s_addr == (g) && \
		    _rt->mfc_stall == NULL) { \
			(rt) = _rt; \
			break; \
		} \
	} \
	if ((rt) == NULL) \
		++mrtstat.mrts_mfc_misses; \
}

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) { \
	register int xxs; \
	delta = (a).tv_usec - (b).tv_usec; \
	xxs = (a).tv_sec - (b).tv_sec; \
	switch (xxs) { \
	case 2: \
		delta += 1000000; \
		/* fall through */ \
	case 1: \
		delta += 1000000; \
		/* fall through */ \
	case 0: \
		break; \
	default: \
		delta += (1000000 * xxs); \
		break; \
	} \
}

#ifdef UPCALL_TIMING
u_int32_t upcall_data[51];
#endif /* UPCALL_TIMING */

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
int
ip_mrouter_set(cmd, so, m)
	int cmd;
	struct socket *so;
	struct mbuf **m;
{
	int error;

	if (cmd != MRT_INIT && so != ip_mrouter)
		error = EACCES;
	else
		switch (cmd) {
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
			error = EOPNOTSUPP;
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
ip_mrouter_get(cmd, so, m)
	int cmd;
	struct socket *so;
	struct mbuf **m;
{
	struct mbuf *mb;
	int error;

	if (so != ip_mrouter)
		error = EACCES;
	else {
		*m = mb = m_get(M_WAIT, MT_SOOPTS);

		switch (cmd) {
		case MRT_VERSION:
			error = get_version(mb);
			break;
		case MRT_ASSERT:
			error = get_assert(mb);
			break;
		default:
			error = EOPNOTSUPP;
			break;
		}

		if (error)
			m_free(mb);
	}

	return (error);
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
mrt_ioctl(cmd, data)
	u_long cmd;
	caddr_t data;
{
	int error;

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
get_sg_cnt(req)
	register struct sioc_sg_req *req;
{
	register struct mfc *rt;
	int s;

	s = splsoftnet();
	MFCFIND(req->src.s_addr, req->grp.s_addr, rt);
	splx(s);
	if (rt != NULL) {
		req->pktcnt = rt->mfc_pkt_cnt;
		req->bytecnt = rt->mfc_byte_cnt;
		req->wrong_if = rt->mfc_wrong_if;
	} else
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;

	return (0);
}

/*
 * returns the input and output packet and byte counts on the vif provided
 */
static int
get_vif_cnt(req)
	register struct sioc_vif_req *req;
{
	register vifi_t vifi = req->vifi;

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
ip_mrouter_init(so, m)
	struct socket *so;
	struct mbuf *m;
{
	int *v;

	if (mrtdebug)
		log(LOG_DEBUG,
		    "ip_mrouter_init: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_IGMP)
		return (EOPNOTSUPP);

	if (m == 0 || m->m_len < sizeof(int))
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

	timeout(expire_upcalls, (caddr_t)0, EXPIRE_TIMEOUT);

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
	register struct vif *vifp;
	int i;
	int s;
	
	s = splsoftnet();

	/* Clear out all the vifs currently in use. */
	for (vifi = 0; vifi < numvifs; vifi++) {
		vifp = &viftable[vifi];
		if (vifp->v_lcl_addr.s_addr != 0)
			reset_vif(vifp);
	}

	bzero((caddr_t)qtable, sizeof(qtable));
	numvifs = 0;
	pim_assert = 0;
	
	untimeout(expire_upcalls, (caddr_t)NULL);
	
	/*
	 * Free all multicast forwarding cache entries.
	 */
	for (i = 0; i < MFCTBLSIZ; i++) {
		register struct mfc *rt, *nrt;

		for (rt = mfchashtbl[i].lh_first; rt; rt = nrt) {
			nrt = rt->mfc_hash.le_next;
			
			expire_mfc(rt);
		}
	}

	free(mfchashtbl, M_MRTABLE);
	mfchashtbl = 0;
	
	/* Reset de-encapsulation cache. */
	have_encap_tunnel = 0;
	
	ip_mrouter = NULL;
	
	splx(s);
	
	if (mrtdebug)
		log(LOG_DEBUG, "ip_mrouter_done\n");
	
	return (0);
}

static int
get_version(m)
	struct mbuf *m;
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
set_assert(m)
	struct mbuf *m;
{
	int *i;

	if (m == 0 || m->m_len < sizeof(int))
		return (EINVAL);

	i = mtod(m, int *);
	pim_assert = !!*i;
	return (0);
}

/*
 * Get PIM assert processing global
 */
static int
get_assert(m)
	struct mbuf *m;
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
add_vif(m)
	struct mbuf *m;
{
	register struct vifctl *vifcp;
	register struct vif *vifp;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	struct ifreq ifr;
	int error, s;
	
	if (m == 0 || m->m_len < sizeof(struct vifctl))
		return (EINVAL);

	vifcp = mtod(m, struct vifctl *);
	if (vifcp->vifc_vifi >= MAXVIFS)
		return (EINVAL);

	vifp = &viftable[vifcp->vifc_vifi];
	if (vifp->v_lcl_addr.s_addr != 0)
		return (EADDRINUSE);
	
	/* Find the interface with an address in AF_INET family. */
	sin.sin_addr = vifcp->vifc_lcl_addr;
	ifa = ifa_ifwithaddr(sintosa(&sin));
	if (ifa == 0)
		return (EADDRNOTAVAIL);
	
	if (vifcp->vifc_flags & VIFF_TUNNEL) {
		if (vifcp->vifc_flags & VIFF_SRCRT) {
			log(LOG_ERR, "Source routed tunnels not supported.\n");
			return (EOPNOTSUPP);
		}

		/* Create a fake encapsulation interface. */
		ifp = (struct ifnet *)malloc(sizeof(*ifp), M_MRTABLE, M_WAITOK);
		bzero(ifp, sizeof(*ifp));
		sprintf(ifp->if_xname, "mdecap%d", vifcp->vifc_vifi);

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
		satosin(&ifr.ifr_addr)->sin_addr.s_addr = INADDR_ANY;
		error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr);
		if (error)
			return (error);
	}
	
	s = splsoftnet();
	/* Define parameters for the tbf structure. */
	vifp->v_tbf.q_len = 0;
	vifp->v_tbf.n_tok = 0;
	vifp->v_tbf.last_pkt_t = 0;
	
	vifp->v_flags = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	vifp->v_lcl_addr = vifcp->vifc_lcl_addr;
	vifp->v_rmt_addr = vifcp->vifc_rmt_addr;
	vifp->v_ifp = ifp;
	vifp->v_rate_limit = vifcp->vifc_rate_limit;
#ifdef RSVP_ISI
	vifp->v_rsvp_on = 0;
	vifp->v_rsvpd = NULL;
#endif /* RSVP_ISI */
	/* Initialize per vif pkt counters. */
	vifp->v_pkt_in = 0;
	vifp->v_pkt_out = 0;
	vifp->v_bytes_in = 0;
	vifp->v_bytes_out = 0;
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
reset_vif(vifp)
	register struct vif *vifp;
{
	struct ifnet *ifp;
	struct ifreq ifr;

	if (vifp->v_flags & VIFF_TUNNEL) {
		free(vifp->v_ifp, M_MRTABLE);
		if (vifp == last_encap_vif) {
			last_encap_vif = 0;
			last_encap_src = 0;
		}
	} else {
		satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr.s_addr = INADDR_ANY;
		ifp = vifp->v_ifp;
		(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);
	}
	bzero((caddr_t)vifp, sizeof(*vifp));
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif(m)
	struct mbuf *m;
{
	vifi_t *vifip;
	register struct vif *vifp;
	register vifi_t vifi;
	int s;
	
	if (m == 0 || m->m_len < sizeof(vifi_t))
		return (EINVAL);

	vifip = mtod(m, vifi_t *);
	if (*vifip >= numvifs)
		return (EINVAL);

	vifp = &viftable[*vifip];
	if (vifp->v_lcl_addr.s_addr == 0)
		return (EADDRNOTAVAIL);
	
	s = splsoftnet();
	
	reset_vif(vifp);
	
	bzero((caddr_t)qtable[*vifip], sizeof(qtable[*vifip]));
	
	/* Adjust numvifs down */
	for (vifi = numvifs; vifi > 0; vifi--)
		if (viftable[vifi-1].v_lcl_addr.s_addr != 0)
			break;
	numvifs = vifi;
	
	splx(s);
	
	if (mrtdebug)
		log(LOG_DEBUG, "del_vif %d, numvifs %d\n", *vifip, numvifs);
	
	return (0);
}

void
vif_delete(ifp)
	struct ifnet *ifp;
{
	int i;
	struct vif *vifp;

	for (i = 0; i < numvifs; i++) {
		vifp = &viftable[i];
		if (vifp->v_ifp == ifp)
			bzero((caddr_t)vifp, sizeof *vifp);
	}

	for (i = numvifs; i > 0; i--)
		if (viftable[i - 1].v_lcl_addr.s_addr != 0)
			break;
	numvifs = i;
}

static void
update_mfc(mfccp, rt)
	struct mfcctl *mfccp;
	struct mfc *rt;
{
	vifi_t vifi;

	rt->mfc_parent = mfccp->mfcc_parent;
	for (vifi = 0; vifi < numvifs; vifi++)
		rt->mfc_ttls[vifi] = mfccp->mfcc_ttls[vifi];
	rt->mfc_expire = 0;
	rt->mfc_stall = 0;
}

static void
expire_mfc(rt)
	struct mfc *rt;
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
add_mfc(m)
	struct mbuf *m;
{
	struct mfcctl *mfccp;
	struct mfc *rt;
	u_int32_t hash = 0;
	struct rtdetq *rte, *nrte;
	register u_short nstl;
	int s;

	if (m == 0 || m->m_len < sizeof(struct mfcctl))
		return (EINVAL);

	mfccp = mtod(m, struct mfcctl *);

	s = splsoftnet();
	MFCFIND(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr, rt);

	/* If an entry already exists, just update the fields */
	if (rt) {
		if (mrtdebug & DEBUG_MFC)
			log(LOG_DEBUG,"add_mfc update o %x g %x p %x\n",
			    ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);

		if (rt->mfc_expire)
			nexpire[hash]--;

		update_mfc(mfccp, rt);

		splx(s);
		return (0);
	}

	/* 
	 * Find the entry for which the upcall was made and update
	 */
	nstl = 0;
	hash = MFCHASH(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);
	for (rt = mfchashtbl[hash].lh_first; rt; rt = rt->mfc_hash.le_next) {
		if (rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr &&
		    rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr &&
		    rt->mfc_stall != NULL) {
			if (nstl++)
				log(LOG_ERR, "add_mfc %s o %x g %x p %x dbx %p\n",
				    "multiple kernel entries",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);

			if (mrtdebug & DEBUG_MFC)
				log(LOG_DEBUG,"add_mfc o %x g %x p %x dbg %p\n",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);

			if (rt->mfc_expire)
				nexpire[hash]--;

			/* free packets Qed at the end of this entry */
			for (rte = rt->mfc_stall; rte != NULL; rte = nrte) {
				nrte = rte->next;
#ifdef RSVP_ISI
				ip_mdq(rte->m, rte->ifp, rt, -1);
#else
				ip_mdq(rte->m, rte->ifp, rt);
#endif /* RSVP_ISI */
				m_freem(rte->m);
#ifdef UPCALL_TIMING
				collate(&rte->t);
#endif /* UPCALL_TIMING */
				free(rte, M_MRTABLE);
			}

			update_mfc(mfccp, rt);
		}
	}

	if (nstl == 0) {
		/*
		 * No mfc; make a new one
		 */
		if (mrtdebug & DEBUG_MFC)
			log(LOG_DEBUG,"add_mfc no upcall o %x g %x p %x\n",
			    ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);
	
		rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
		if (rt == NULL) {
			splx(s);
			return (ENOBUFS);
		}

		rt->mfc_origin = mfccp->mfcc_origin;
		rt->mfc_mcastgrp = mfccp->mfcc_mcastgrp;
		/* initialize pkt counters per src-grp */
		rt->mfc_pkt_cnt = 0;
		rt->mfc_byte_cnt = 0;
		rt->mfc_wrong_if = 0;
		timerclear(&rt->mfc_last_assert);
		update_mfc(mfccp, rt);
	    
		/* insert new entry at head of hash chain */
		LIST_INSERT_HEAD(&mfchashtbl[hash], rt, mfc_hash);
	}

	splx(s);
	return (0);
}

#ifdef UPCALL_TIMING
/*
 * collect delay statistics on the upcalls 
 */
static void collate(t)
register struct timeval *t;
{
    register u_int32_t d;
    register struct timeval tp;
    register u_int32_t delta;
    
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
del_mfc(m)
	struct mbuf *m;
{
	struct mfcctl *mfccp;
	struct mfc *rt;
	int s;

	if (m == 0 || m->m_len < sizeof(struct mfcctl))
		return (EINVAL);

	mfccp = mtod(m, struct mfcctl *);

	if (mrtdebug & DEBUG_MFC)
		log(LOG_DEBUG, "del_mfc origin %x mcastgrp %x\n",
		    ntohl(mfccp->mfcc_origin.s_addr), ntohl(mfccp->mfcc_mcastgrp.s_addr));

	s = splsoftnet();

	MFCFIND(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr, rt);
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
socket_send(s, mm, src)
    struct socket *s;
    struct mbuf *mm;
    struct sockaddr_in *src;
{
    if (s) {
	if (sbappendaddr(&s->so_rcv, sintosa(src), mm, (struct mbuf *)0) != 0) {
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
ip_mforward(m, ifp, imo)
#else
ip_mforward(m, ifp)
#endif /* RSVP_ISI */
    struct mbuf *m;
    struct ifnet *ifp;
#ifdef RSVP_ISI
    struct ip_moptions *imo;
#endif /* RSVP_ISI */
{
    register struct ip *ip = mtod(m, struct ip *);
    register struct mfc *rt;
    register u_char *ipoptions;
    static int srctun = 0;
    register struct mbuf *mm;
    int s;
#ifdef RSVP_ISI
    register struct vif *vifp;
    vifi_t vifi;
#endif /* RSVP_ISI */

    if (mrtdebug & DEBUG_FORWARD)
	log(LOG_DEBUG, "ip_mforward: src %x, dst %x, ifp %p\n",
	    ntohl(ip->ip_src.s_addr), ntohl(ip->ip_dst.s_addr), ifp);

    if (ip->ip_hl < (IP_HDR_LEN + TUNNEL_LEN) >> 2 ||
	(ipoptions = (u_char *)(ip + 1))[1] != IPOPT_LSRR) {
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
	    log(LOG_ERR, "ip_mforward: received source-routed packet from %x\n",
		ntohl(ip->ip_src.s_addr));

	return (1);
    }

#ifdef RSVP_ISI
    if (imo && ((vifi = imo->imo_multicast_vif) < numvifs)) {
	if (ip->ip_ttl < 255)
	    ip->ip_ttl++;	/* compensate for -1 in *_send routines */
	if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
	    vifp = viftable + vifi;
	    printf("Sending IPPROTO_RSVP from %x to %x on vif %d (%s%s)\n",
		ntohl(ip->ip_src), ntohl(ip->ip_dst), vifi,
		(vifp->v_flags & VIFF_TUNNEL) ? "tunnel on " : "",
		vifp->v_ifp->if_xname);
	}
	return (ip_mdq(m, ifp, rt, vifi));
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
    if (ip->ip_ttl <= 1 ||
	IN_LOCAL_GROUP(ip->ip_dst.s_addr))
	return (0);

    /*
     * Determine forwarding vifs from the forwarding cache table
     */
    s = splsoftnet();
    MFCFIND(ip->ip_src.s_addr, ip->ip_dst.s_addr, rt);

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
	 * Make a copy of the packet &
	 * send message to routing daemon
	 */

	register struct mbuf *mb0;
	register struct rtdetq *rte;
	register u_int32_t hash;
#ifdef UPCALL_TIMING
	struct timeval tp;

	microtime(&tp);
#endif /* UPCALL_TIMING */

	mrtstat.mrts_no_route++;
	if (mrtdebug & (DEBUG_FORWARD | DEBUG_MFC))
	    log(LOG_DEBUG, "ip_mforward: no rte s %x g %x\n",
		ntohl(ip->ip_src.s_addr),
		ntohl(ip->ip_dst.s_addr));

	/*
	 * Allocate mbufs early so that we don't do extra work if we are
	 * just going to fail anyway.
	 */
	rte = (struct rtdetq *)malloc(sizeof(*rte), M_MRTABLE, M_NOWAIT);
	if (rte == NULL) {
	    splx(s);
	    return (ENOBUFS);
	}
	mb0 = m_copy(m, 0, M_COPYALL);
	if (mb0 == NULL) {
	    free(rte, M_MRTABLE);
	    splx(s);
	    return (ENOBUFS);
	}
	    
	/* is there an upcall waiting for this packet? */
	hash = MFCHASH(ip->ip_src.s_addr, ip->ip_dst.s_addr);
	for (rt = mfchashtbl[hash].lh_first; rt; rt = rt->mfc_hash.le_next) {
	    if (ip->ip_src.s_addr == rt->mfc_origin.s_addr &&
		ip->ip_dst.s_addr == rt->mfc_mcastgrp.s_addr &&
		rt->mfc_stall != NULL)
		break;
	}

	if (rt == NULL) {
	    int hlen = ip->ip_hl << 2;
	    int i;
	    struct igmpmsg *im;

	    /* no upcall, so make a new entry */
	    rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
	    if (rt == NULL) {
		free(rte, M_MRTABLE);
		m_free(mb0);
		splx(s);
		return (ENOBUFS);
	    }
	    /* Make a copy of the header to send to the user level process */
	    mm = m_copy(m, 0, hlen);
	    M_PULLUP(mm, hlen);
	    if (mm == NULL) {
		free(rte, M_MRTABLE);
		m_free(mb0);
		free(rt, M_MRTABLE);
		splx(s);
		return (ENOBUFS);
	    }

	    /* 
	     * Send message to routing daemon to install 
	     * a route into the kernel table
	     */
	    sin.sin_addr = ip->ip_src;
	    
	    im = mtod(mm, struct igmpmsg *);
	    im->im_msgtype	= IGMPMSG_NOCACHE;
	    im->im_mbz		= 0;

	    mrtstat.mrts_upcalls++;

	    if (socket_send(ip_mrouter, mm, &sin) < 0) {
		log(LOG_WARNING, "ip_mforward: ip_mrouter socket queue full\n");
		++mrtstat.mrts_upq_sockfull;
		free(rte, M_MRTABLE);
		m_free(mb0);
		free(rt, M_MRTABLE);
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
	    register int npkts = 0;

	    for (p = &rt->mfc_stall; *p != NULL; p = &(*p)->next)
		if (++npkts > MAX_UPQ) {
		    mrtstat.mrts_upq_ovflw++;
		    free(rte, M_MRTABLE);
		    m_free(mb0);
		    splx(s);
		    return (0);
	        }

	    /* Add this entry to the end of the queue */
	    *p = rte;
	}

	rte->next		= NULL;
	rte->m 			= mb0;
	rte->ifp 		= ifp;
#ifdef UPCALL_TIMING
	rte->t			= tp;
#endif /* UPCALL_TIMING */


	splx(s);

	return (0);
    }
}


/*ARGSUSED*/
static void
expire_upcalls(v)
	void *v;
{
	int i;
	int s;

	s = splsoftnet();

	for (i = 0; i < MFCTBLSIZ; i++) {
		register struct mfc *rt, *nrt;

		if (nexpire[i] == 0)
			continue;

		for (rt = mfchashtbl[i].lh_first; rt; rt = nrt) {
			nrt = rt->mfc_hash.le_next;

			if (rt->mfc_expire == 0 ||
			    --rt->mfc_expire > 0)
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
	timeout(expire_upcalls, (caddr_t)0, EXPIRE_TIMEOUT);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
#ifdef RSVP_ISI
ip_mdq(m, ifp, rt, xmt_vif)
#else
ip_mdq(m, ifp, rt)
#endif /* RSVP_ISI */
    register struct mbuf *m;
    register struct ifnet *ifp;
    register struct mfc *rt;
#ifdef RSVP_ISI
    register vifi_t xmt_vif;
#endif /* RSVP_ISI */
{
    register struct ip  *ip = mtod(m, struct ip *);
    register vifi_t vifi;
    register struct vif *vifp;
    register int plen = ip->ip_len;

/*
 * Macro to send packet on vif.  Since RSVP packets don't get counted on
 * input, they shouldn't get counted on output, so statistics keeping is
 * seperate.
 */
#define MC_SEND(ip,vifp,m) {                             \
                if ((vifp)->v_flags & VIFF_TUNNEL)	 \
                    encap_send((ip), (vifp), (m));       \
                else                                     \
                    phyint_send((ip), (vifp), (m));      \
}

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
		ifp, vifi, vifi >= numvifs ? 0 : viftable[vifi].v_ifp); 
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
	    struct mbuf *mm;
	    struct igmpmsg *im;
	    int hlen = ip->ip_hl << 2;
	    struct timeval now;
	    register u_int32_t delta;

	    microtime(&now);

	    TV_DELTA(rt->mfc_last_assert, now, delta);

	    if (delta > ASSERT_MSG_TIME) {
		mm = m_copy(m, 0, hlen);
		M_PULLUP(mm, hlen);
		if (mm == NULL) {
		    return (ENOBUFS);
		}

		rt->mfc_last_assert = now;
		
		im = mtod(mm, struct igmpmsg *);
		im->im_msgtype	= IGMPMSG_WRONGVIF;
		im->im_mbz	= 0;
		im->im_vif	= vifi;

		sin.sin_addr = im->im_src;

		socket_send(ip_mrouter, m, &sin);
	    }
	}
	return (0);
    }

    /* If I sourced this packet, it counts as output, else it was input. */
    if (ip->ip_src.s_addr == viftable[vifi].v_lcl_addr.s_addr) {
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
 * check if a vif number is legal/ok. This is used by ip_output, to export
 * numvifs there, 
 */
int
legal_vif_num(vif)
    int vif;
{
    if (vif >= 0 && vif < numvifs)
       return (1);
    else
       return (0);
}
#endif /* RSVP_ISI */

static void
phyint_send(ip, vifp, m)
	struct ip *ip;
	struct vif *vifp;
	struct mbuf *m;
{
	register struct mbuf *mb_copy;
	register int hlen = ip->ip_hl << 2;

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
		tbf_control(vifp, mb_copy, mtod(mb_copy, struct ip *), ip->ip_len);
}

static void
encap_send(ip, vifp, m)
	register struct ip *ip;
	register struct vif *vifp;
	register struct mbuf *m;
{
	register struct mbuf *mb_copy;
	register struct ip *ip_copy;
	register int i, len = ip->ip_len + sizeof(multicast_encap_iphdr);

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
	ip_copy->ip_id = ip_randomid();
	HTONS(ip_copy->ip_id);
	ip_copy->ip_len = len;
	ip_copy->ip_src = vifp->v_lcl_addr;
	ip_copy->ip_dst = vifp->v_rmt_addr;
	
	/*
	 * turn the encapsulated IP header back into a valid one.
	 */
	ip = (struct ip *)((caddr_t)ip_copy + sizeof(multicast_encap_iphdr));
	--ip->ip_ttl;
	HTONS(ip->ip_len);
	HTONS(ip->ip_off);
	ip->ip_sum = 0;
#if defined(LBL) && !defined(ultrix) && !defined(i386)
	ip->ip_sum = ~oc_cksum((caddr_t)ip, ip->ip_hl << 2, 0);
#else
	mb_copy->m_data += sizeof(multicast_encap_iphdr);
	ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
	mb_copy->m_data -= sizeof(multicast_encap_iphdr);
#endif
	
	if (vifp->v_rate_limit <= 0)
		tbf_send_packet(vifp, mb_copy);
	else
		tbf_control(vifp, mb_copy, ip, ip_copy->ip_len);
}

/*
 * De-encapsulate a packet and feed it back through ip input (this
 * routine is called whenever IP gets a packet with proto type
 * ENCAP_PROTO and a local destination address).
 */
void
#if __STDC__
ipip_mroute_input(struct mbuf *m, ...)
#else
ipip_mroute_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	register int hlen;
	register struct ip *ip = mtod(m, struct ip *);
	register int s;
	register struct ifqueue *ifq;
	register struct vif *vifp;
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
	if (ip->ip_src.s_addr != last_encap_src) {
		register struct vif *vife;
	
		vifp = viftable;
		vife = vifp + numvifs;
		for (; vifp < vife; vifp++)
			if (vifp->v_flags & VIFF_TUNNEL &&
			    vifp->v_rmt_addr.s_addr == ip->ip_src.s_addr)
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
		last_encap_src = ip->ip_src.s_addr;
	} else
		vifp = last_encap_vif;

	m->m_data += hlen;
	m->m_len -= hlen;
	m->m_pkthdr.len -= hlen;
	m->m_pkthdr.rcvif = vifp->v_ifp;
	ifq = &ipintrq;
	s = splimp();
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
tbf_control(vifp, m, ip, p_len)
	register struct vif *vifp;
	register struct mbuf *m;
	register struct ip *ip;
	register u_int32_t p_len;
{

	tbf_update_tokens(vifp);

	/*
	 * If there are enough tokens, and the queue is empty, send this packet
	 * out immediately.  Otherwise, try to insert it on this vif's queue.
	 */
	if (vifp->v_tbf.q_len == 0) {
		if (p_len <= vifp->v_tbf.n_tok) {
			vifp->v_tbf.n_tok -= p_len;
			tbf_send_packet(vifp, m);
		} else if (p_len > MAX_BKT_SIZE) {
			/* drop if packet is too large */
			mrtstat.mrts_pkt2large++;
			m_freem(m);
		} else {
			/* queue packet and timeout till later */
			tbf_queue(vifp, m, ip);
			timeout(tbf_reprocess_q, vifp, 1);
		}
	} else {
		if (vifp->v_tbf.q_len >= MAXQSIZE &&
		    !tbf_dq_sel(vifp, ip)) {
			/* queue length too much, and couldn't make room */
			mrtstat.mrts_q_overflow++;
			m_freem(m);
		} else {
			/* queue length low enough, or made room */
			tbf_queue(vifp, m, ip);
			tbf_process_q(vifp);
		}
	}
}

/* 
 * adds a packet to the queue at the interface
 */
static void
tbf_queue(vifp, m, ip) 
    register struct vif *vifp;
    register struct mbuf *m;
    register struct ip *ip;
{
    register u_int32_t ql;
    register int index = (vifp - viftable);
    register int s = splsoftnet();

    ql = vifp->v_tbf.q_len;

    qtable[index][ql].pkt_m = m;
    qtable[index][ql].pkt_len = (mtod(m, struct ip *))->ip_len;
    qtable[index][ql].pkt_ip = ip;

    vifp->v_tbf.q_len++;
    splx(s);
}


/* 
 * processes the queue at the interface
 */
static void
tbf_process_q(vifp)
    register struct vif *vifp;
{
    register struct pkt_queue pkt_1;
    register int index = (vifp - viftable);
    register int s = splsoftnet();

    /* loop through the queue at the interface and send as many packets
     * as possible
     */
    while (vifp->v_tbf.q_len > 0) {
	/* locate the first packet */
	pkt_1 = qtable[index][0];

	/* determine if the packet can be sent */
	if (pkt_1.pkt_len <= vifp->v_tbf.n_tok) {
	    /* if so,
	     * reduce no of tokens, dequeue the queue,
	     * send the packet.
	     */
	    vifp->v_tbf.n_tok -= pkt_1.pkt_len;

	    tbf_dequeue(vifp, 0);
	    tbf_send_packet(vifp, pkt_1.pkt_m);
	} else
	    break;
    }
    splx(s);
}

/* 
 * removes the jth packet from the queue at the interface
 */
static void
tbf_dequeue(vifp, j)
    register struct vif *vifp;
    register int j;
{
    register u_int32_t index = vifp - viftable;
    register int i;

    for (i=j+1; i <= vifp->v_tbf.q_len - 1; i++) {
	qtable[index][i-1] = qtable[index][i];
    }		
    qtable[index][i-1].pkt_m = NULL;
    qtable[index][i-1].pkt_len = NULL;
    qtable[index][i-1].pkt_ip = NULL;

    vifp->v_tbf.q_len--;

    if (tbfdebug > 1)
	log(LOG_DEBUG, "tbf_dequeue: vif# %d qlen %d\n",vifp-viftable, i-1);
}

static void
tbf_reprocess_q(arg)
	void *arg;
{
	register struct vif *vifp = arg;

	if (ip_mrouter == NULL) 
		return;

	tbf_update_tokens(vifp);
	tbf_process_q(vifp);

	if (vifp->v_tbf.q_len)
		timeout(tbf_reprocess_q, vifp, 1);
}

/* function that will selectively discard a member of the queue
 * based on the precedence value and the priority obtained through
 * a lookup table - not yet implemented accurately!
 */
static int
tbf_dq_sel(vifp, ip)
    register struct vif *vifp;
    register struct ip *ip;
{
    register int i;
    register int s = splsoftnet();
    register u_int p;

    p = priority(vifp, ip);

    for(i=vifp->v_tbf.q_len-1;i >= 0;i--) {
	if (p > priority(vifp, qtable[vifp-viftable][i].pkt_ip)) {
	    m_freem(qtable[vifp-viftable][i].pkt_m);
	    tbf_dequeue(vifp, i);
	    splx(s);
	    mrtstat.mrts_drop_sel++;
	    return (1);
	}
    }
    splx(s);
    return (0);
}

static void
tbf_send_packet(vifp,m)
    register struct vif *vifp;
    register struct mbuf *m;
{
    int error;
    int s = splsoftnet();

    if (vifp->v_flags & VIFF_TUNNEL) {
	/* If tunnel options */
	ip_output(m, (struct mbuf *)0, &vifp->v_route,
		  IP_FORWARDING, NULL, NULL);
    } else {
	/* if physical interface option, extract the options and then send */
	struct ip *ip = mtod(m, struct ip *);
	struct ip_moptions imo;
	imo.imo_multicast_ifp  = vifp->v_ifp;
	imo.imo_multicast_ttl  = ip->ip_ttl - 1;
	imo.imo_multicast_loop = 1;
#ifdef RSVP_ISI
	imo.imo_multicast_vif  = -1;
#endif

	error = ip_output(m, (struct mbuf *)0, (struct route *)0,
			  IP_FORWARDING|IP_MULTICASTOPTS, &imo, NULL);
	if (mrtdebug & DEBUG_XMIT)
	    log(LOG_DEBUG, "phyint_send on vif %d err %d\n", vifp-viftable, error);
    }
    splx(s);
}

/* determine the current time and then
 * the elapsed time (between the last time and time now)
 * in milliseconds & update the no. of tokens in the bucket
 */
static void
tbf_update_tokens(vifp)
    register struct vif *vifp;
{
    struct timeval tp;
    register u_int32_t t;
    register u_int32_t elapsed;
    register int s = splsoftnet();

    microtime(&tp);

    t = tp.tv_sec*1000 + tp.tv_usec/1000;

    elapsed = (t - vifp->v_tbf.last_pkt_t) * vifp->v_rate_limit /8;
    vifp->v_tbf.n_tok += elapsed;
    vifp->v_tbf.last_pkt_t = t;

    if (vifp->v_tbf.n_tok > MAX_BKT_SIZE)
	vifp->v_tbf.n_tok = MAX_BKT_SIZE;

    splx(s);
}

static int
priority(vifp, ip)
    register struct vif *vifp;
    register struct ip *ip;
{
    register int prio;

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
	    default:
		prio = 50;
		break;
	}

	if (tbfdebug > 1) log(LOG_DEBUG, "port %x prio %d\n", ntohs(udp->uh_dport), prio);
    } else
	prio = 50;


    return (prio);
}

/*
 * End of token bucket filter modifications 
 */

#ifdef RSVP_ISI

int
ip_rsvp_vif_init(so, m)
    struct socket *so;
    struct mbuf *m;
{
    int i;
    register int s;

    if (rsvpdebug)
	printf("ip_rsvp_vif_init: so_type = %d, pr_protocol = %d\n",
	       so->so_type, so->so_proto->pr_protocol);

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return (EOPNOTSUPP);

    /* Check mbuf. */
    if (m == NULL || m->m_len != sizeof(int)) {
	return (EINVAL);
    }
    i = *(mtod(m, int *));

    if (rsvpdebug)
	printf("ip_rsvp_vif_init: vif = %d rsvp_on = %d\n",i,rsvp_on);

    s = splsoftnet();

    /* Check vif. */
    if (!legal_vif_num(i)) {
	splx(s);
	return (EADDRNOTAVAIL);
    }

    /* Check if socket is available. */
    if (viftable[i].v_rsvpd != NULL) {
	splx(s);
	return (EADDRINUSE);
    }

    viftable[i].v_rsvpd = so;
    /* This may seem silly, but we need to be sure we don't over-increment
     * the RSVP counter, in case something slips up.
     */
    if (!viftable[i].v_rsvp_on) {
	viftable[i].v_rsvp_on = 1;
	rsvp_on++;
    }

    splx(s);
    return (0);
}

int
ip_rsvp_vif_done(so, m)
    struct socket *so;
    struct mbuf *m;
{
    int i;
    register int s;

    if (rsvpdebug)
	printf("ip_rsvp_vif_done: so_type = %d, pr_protocol = %d\n",
	       so->so_type, so->so_proto->pr_protocol);

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return (EOPNOTSUPP);

    /* Check mbuf. */
    if (m == NULL || m->m_len != sizeof(int)) {
	return (EINVAL);
    }
    i = *(mtod(m, int *));

    s = splsoftnet();

    /* Check vif. */
    if (!legal_vif_num(i)) {
	splx(s);
        return (EADDRNOTAVAIL);
    }

    if (rsvpdebug)
	printf("ip_rsvp_vif_done: v_rsvpd = %x so = %x\n",
	       viftable[i].v_rsvpd, so);

    viftable[i].v_rsvpd = NULL;
    /* This may seem silly, but we need to be sure we don't over-decrement
     * the RSVP counter, in case something slips up.
     */
    if (viftable[i].v_rsvp_on) {
	viftable[i].v_rsvp_on = 0;
	rsvp_on--;
    }

    splx(s);
    return (0);
}

void
ip_rsvp_force_done(so)
    struct socket *so;
{
    int vifi;
    register int s;

    /* Don't bother if it is not the right type of socket. */
    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return;

    s = splsoftnet();

    /* The socket may be attached to more than one vif...this
     * is perfectly legal.
     */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_rsvpd == so) {
	    viftable[vifi].v_rsvpd = NULL;
	    /* This may seem silly, but we need to be sure we don't
	     * over-decrement the RSVP counter, in case something slips up.
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
rsvp_input(m, ifp)
    struct mbuf *m;
    struct ifnet *ifp;
{
    int vifi;
    register struct ip *ip = mtod(m, struct ip *);
    static struct sockaddr_in rsvp_src = { sizeof(sin), AF_INET };
    register int s;

    if (rsvpdebug)
	printf("rsvp_input: rsvp_on %d\n",rsvp_on);

    /* Can still get packets with rsvp_on = 0 if there is a local member
     * of the group to which the RSVP packet is addressed.  But in this
     * case we want to throw the packet away.
     */
    if (!rsvp_on) {
	m_freem(m);
	return;
    }

    /* If the old-style non-vif-associated socket is set, then use
     * it and ignore the new ones.
     */
    if (ip_rsvpd != NULL) {
	if (rsvpdebug)
	    printf("rsvp_input: Sending packet up old-style socket\n");
	rip_input(m, 0);
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
	    printf("rsvp_input: Can't find vif for packet...dropping it.\n");
	m_freem(m);
	splx(s);
	return;
    }

    if (rsvpdebug)
	printf("rsvp_input: check socket\n");

    if (viftable[vifi].v_rsvpd == NULL) {
	/* drop packet, since there is no specific socket for this
	 * interface */
	if (rsvpdebug)
	    printf("rsvp_input: No socket defined for vif %d\n",vifi);
	m_freem(m);
	splx(s);
	return;
    }

    rsvp_src.sin_addr = ip->ip_src;

    if (rsvpdebug && m)
	printf("rsvp_input: m->m_len = %d, sbspace() = %d\n",
	       m->m_len,sbspace(&viftable[vifi].v_rsvpd->so_rcv));

    if (socket_send(viftable[vifi].v_rsvpd, m, &rsvp_src) < 0)
	if (rsvpdebug)
	    printf("rsvp_input: Failed to append to socket\n");
    else
	if (rsvpdebug)
	    printf("rsvp_input: send packet up\n");
    
    splx(s);
}
#endif /* RSVP_ISI */
