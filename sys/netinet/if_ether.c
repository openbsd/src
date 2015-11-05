/*	$OpenBSD: if_ether.c,v 1.184 2015/11/05 12:46:23 bluhm Exp $	*/
/*	$NetBSD: if_ether.c,v 1.31 1996/05/11 12:59:58 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)if_ether.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#include "carp.h"

#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif
#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

/*
 * ARP trailer negotiation.  Trailer protocol is not IP specific,
 * but ARP request/response use IP addresses.
 */
#define ETHERTYPE_IPTRAILERS ETHERTYPE_TRAIL

struct llinfo_arp {
	LIST_ENTRY(llinfo_arp)	 la_list;
	struct rtentry		*la_rt;		/* backpointer to rtentry */
	long			 la_asked;	/* last time we QUERIED */
	struct mbuf_list	 la_ml;		/* packet hold queue */
};
#define LA_HOLD_QUEUE 10
#define LA_HOLD_TOTAL 100

/* timer values */
int	arpt_prune = (5*60*1);	/* walk list every 5 minutes */
int	arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	arpt_down = 20;		/* once declared down, don't send for 20 secs */

void arptfree(struct rtentry *);
void arptimer(void *);
struct rtentry *arplookup(u_int32_t, int, int, u_int);
void in_arpinput(struct mbuf *);
void revarpinput(struct mbuf *);
void in_revarpinput(struct mbuf *);

LIST_HEAD(, llinfo_arp) arp_list;
struct	pool arp_pool;		/* pool for llinfo_arp structures */
/* XXX hate magic numbers */
struct	niqueue arpintrq = NIQUEUE_INITIALIZER(50, NETISR_ARP);
struct	niqueue rarpintrq = NIQUEUE_INITIALIZER(50, NETISR_ARP);
int	arp_inuse, arp_allocated;
int	arp_maxtries = 5;
int	arpinit_done;
int	la_hold_total;

#ifdef NFSCLIENT
/* revarp state */
struct in_addr revarp_myip, revarp_srvip;
int revarp_finished;
struct ifnet *revarp_ifp;
#endif /* NFSCLIENT */

/*
 * Timeout routine.  Age arp_tab entries periodically.
 */
/* ARGSUSED */
void
arptimer(void *arg)
{
	struct timeout *to = (struct timeout *)arg;
	int s;
	struct llinfo_arp *la, *nla;

	s = splsoftnet();
	timeout_add_sec(to, arpt_prune);
	LIST_FOREACH_SAFE(la, &arp_list, la_list, nla) {
		struct rtentry *rt = la->la_rt;

		if (rt->rt_expire && rt->rt_expire <= time_second)
			arptfree(rt); /* timer has expired; clear */
	}
	splx(s);
}

void
arp_rtrequest(struct ifnet *ifp, int req, struct rtentry *rt)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	struct ifaddr *ifa;

	if (!arpinit_done) {
		static struct timeout arptimer_to;

		arpinit_done = 1;
		pool_init(&arp_pool, sizeof(struct llinfo_arp), 0, 0, 0, "arp",
		    NULL);
		/*
		 * We generate expiration times from time.tv_sec
		 * so avoid accidently creating permanent routes.
		 */
		if (time_second == 0) {
			time_second++;
		}

		timeout_set(&arptimer_to, arptimer, &arptimer_to);
		timeout_add_sec(&arptimer_to, 1);
	}

	if (rt->rt_flags & (RTF_GATEWAY|RTF_BROADCAST))
		return;

	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 && rt_mask(rt) &&
		    satosin(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
		if (rt->rt_flags & RTF_CLONING ||
		    ((rt->rt_flags & (RTF_LLINFO | RTF_LOCAL)) && !la)) {
			/*
			 * Give this route an expiration time, even though
			 * it's a "permanent" route, so that routes cloned
			 * from it do not need their expiration time set.
			 */
			rt->rt_expire = time_second;
			if ((rt->rt_flags & RTF_CLONING) != 0)
				break;
		}
		/*
		 * Announce a new entry if requested or warn the user
		 * if another station has this IP address.
		 */
		if (rt->rt_flags & (RTF_ANNOUNCE|RTF_LOCAL))
			arprequest(ifp,
			    &satosin(rt_key(rt))->sin_addr.s_addr,
			    &satosin(rt_key(rt))->sin_addr.s_addr,
			    (u_char *)LLADDR(satosdl(gate)));
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sizeof(struct sockaddr_dl)) {
			log(LOG_DEBUG, "%s: bad gateway value: %s\n", __func__,
			    ifp->if_xname);
			break;
		}
		satosdl(gate)->sdl_type = ifp->if_type;
		satosdl(gate)->sdl_index = ifp->if_index;
		if (la != 0)
			break; /* This happens on a route change */
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		la = pool_get(&arp_pool, PR_NOWAIT | PR_ZERO);
		rt->rt_llinfo = (caddr_t)la;
		if (la == NULL) {
			log(LOG_DEBUG, "%s: pool get failed\n", __func__);
			break;
		}
		arp_inuse++;
		arp_allocated++;
		ml_init(&la->la_ml);
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		LIST_INSERT_HEAD(&arp_list, la, la_list);

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if ((ifa->ifa_addr->sa_family == AF_INET) &&
			    ifatoia(ifa)->ia_addr.sin_addr.s_addr ==
			    satosin(rt_key(rt))->sin_addr.s_addr)
				break;
		}
		if (ifa) {
			KASSERT(ifa == rt->rt_ifa);
			rt->rt_expire = 0;
		}
		break;

	case RTM_DELETE:
		if (la == NULL)
			break;
		arp_inuse--;
		LIST_REMOVE(la, la_list);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		la_hold_total -= ml_purge(&la->la_ml);
		pool_put(&arp_pool, la);
	}
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
void
arprequest(struct ifnet *ifp, u_int32_t *sip, u_int32_t *tip, u_int8_t *enaddr)
{
	struct mbuf *m;
	struct ether_header *eh;
	struct ether_arp *ea;
	struct sockaddr sa;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;
	MH_ALIGN(m, sizeof(*ea));
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	memset(ea, 0, sizeof(*ea));
	memcpy(eh->ether_dhost, etherbroadcastaddr, sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_ARP);	/* if_output will not swap */
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(ARPOP_REQUEST);
	memcpy(eh->ether_shost, enaddr, sizeof(eh->ether_shost));
	memcpy(ea->arp_sha, enaddr, sizeof(ea->arp_sha));
	memcpy(ea->arp_spa, sip, sizeof(ea->arp_spa));
	memcpy(ea->arp_tpa, tip, sizeof(ea->arp_tpa));
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	m->m_flags |= M_BCAST;
	ifp->if_output(ifp, m, &sa, NULL);
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 0 indicates
 * that desten has been filled in and the packet should be sent
 * normally; A return value of EAGAIN indicates that the packet
 * has been taken over here, either now or for later transmission.
 * Any other return value indicates an error.
 */
int
arpresolve(struct ifnet *ifp, struct rtentry *rt0, struct mbuf *m,
    struct sockaddr *dst, u_char *desten)
{
	struct arpcom *ac = (struct arpcom *)ifp;
	struct llinfo_arp *la = NULL;
	struct sockaddr_dl *sdl;
	struct rtentry *rt = NULL;
	struct mbuf *mh;
	char addr[INET_ADDRSTRLEN];
	int error, created = 0;

	if (m->m_flags & M_BCAST) {	/* broadcast */
		memcpy(desten, etherbroadcastaddr, sizeof(etherbroadcastaddr));
		return (0);
	}
	if (m->m_flags & M_MCAST) {	/* multicast */
		ETHER_MAP_IP_MULTICAST(&satosin(dst)->sin_addr, desten);
		return (0);
	}

	if (rt0 != NULL) {
		error = rt_checkgate(ifp, rt0, dst,
		    m->m_pkthdr.ph_rtableid, &rt);
		if (error) {
			m_freem(m);
			return (error);
		}

		if ((rt->rt_flags & RTF_LLINFO) == 0) {
			log(LOG_DEBUG, "%s: %s: route contains no arp"
			    " information\n", __func__, inet_ntop(AF_INET,
				&satosin(rt_key(rt))->sin_addr, addr,
				sizeof(addr)));
			m_freem(m);
			return (EINVAL);
		}

		la = (struct llinfo_arp *)rt->rt_llinfo;
		if (la == NULL)
			log(LOG_DEBUG, "%s: %s: route without link "
			    "local address\n", __func__, inet_ntop(AF_INET,
				&satosin(dst)->sin_addr, addr, sizeof(addr)));
	} else {
		rt = arplookup(satosin(dst)->sin_addr.s_addr, 1, 0,
		    ifp->if_rdomain);
		if (rt != NULL) {
		    	created = 1;
			la = ((struct llinfo_arp *)rt->rt_llinfo);
		}
		if (la == NULL)
			log(LOG_DEBUG, "%s: %s: can't allocate llinfo\n",
			    __func__,
			    inet_ntop(AF_INET, &satosin(dst)->sin_addr,
				addr, sizeof(addr)));
	}
	if (la == NULL || rt == NULL)
		goto bad;
	sdl = satosdl(rt->rt_gateway);
	if (sdl->sdl_alen > 0 && sdl->sdl_alen != ETHER_ADDR_LEN) {
		log(LOG_DEBUG, "%s: %s: incorrect arp information\n", __func__,
		    inet_ntop(AF_INET, &satosin(dst)->sin_addr,
			addr, sizeof(addr)));
		goto bad;
	}
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time_second) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
		memcpy(desten, LLADDR(sdl), sdl->sdl_alen);
		if (created)
			rtfree(rt);
		return (0);
	}
	if (ifp->if_flags & IFF_NOARP)
		goto bad;

	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet. Insert mbuf in hold queue if below limit
	 * if above the limit free the queue without queuing the new packet.
	 */
	if (la_hold_total < LA_HOLD_TOTAL && la_hold_total < nmbclust / 64) {
		if (ml_len(&la->la_ml) >= LA_HOLD_QUEUE) {
			mh = ml_dequeue(&la->la_ml);
			la_hold_total--;
			m_freem(mh);
		}
		ml_enqueue(&la->la_ml, m);
		la_hold_total++;
	} else {
		la_hold_total -= ml_purge(&la->la_ml);
		m_freem(m);
	}

	/*
	 * Re-send the ARP request when appropriate.
	 */
#ifdef	DIAGNOSTIC
	if (rt->rt_expire == 0) {
		/* This should never happen. (Should it? -gwr) */
		printf("%s: unresolved and rt_expire == 0\n", __func__);
		/* Set expiration time to now (expired). */
		rt->rt_expire = time_second;
	}
#endif
	if (rt->rt_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != time_second) {
			rt->rt_expire = time_second;
			if (la->la_asked++ < arp_maxtries)
				arprequest(ifp,
				    &satosin(rt->rt_ifa->ifa_addr)->sin_addr.s_addr,
				    &satosin(dst)->sin_addr.s_addr,
#if NCARP > 0
				    (rt->rt_ifp->if_type == IFT_CARP) ?
					((struct arpcom *) rt->rt_ifp->if_softc
					)->ac_enaddr :
#endif
				    ac->ac_enaddr);
			else {
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += arpt_down;
				la->la_asked = 0;
				la_hold_total -= ml_purge(&la->la_ml);
			}
		}
	}
	if (created)
		rtfree(rt);
	return (EAGAIN);

bad:
	m_freem(m);
	if (created)
		rtfree(rt);
	return (EINVAL);
}

/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
arpintr(void)
{
	struct mbuf *m;
	struct arphdr *ar;
	int len;

	while ((m = niq_dequeue(&arpintrq)) != NULL) {
#ifdef DIAGNOSTIC
		if ((m->m_flags & M_PKTHDR) == 0)
			panic("arpintr");
#endif

		len = sizeof(struct arphdr);
		if (m->m_len < len && (m = m_pullup(m, len)) == NULL)
			continue;

		ar = mtod(m, struct arphdr *);
		if (ntohs(ar->ar_hrd) != ARPHRD_ETHER) {
			m_freem(m);
			continue;
		}

		len += 2 * (ar->ar_hln + ar->ar_pln);
		if (m->m_len < len && (m = m_pullup(m, len)) == NULL)
			continue;

		switch (ntohs(ar->ar_pro)) {
		case ETHERTYPE_IP:
		case ETHERTYPE_IPTRAILERS:
			in_arpinput(m);
			continue;
		}
		m_freem(m);
	}

	while ((m = niq_dequeue(&rarpintrq)) != NULL)
		revarpinput(m);
}

/*
 * ARP for Internet protocols on Ethernet, RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 */
void
in_arpinput(struct mbuf *m)
{
	struct ether_arp *ea;
	struct ifnet *ifp;
	struct arpcom *ac;
	struct ether_header *eh;
	struct llinfo_arp *la = NULL;
	struct rtentry *rt = NULL;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct in_addr isaddr, itaddr;
	struct mbuf *mh;
	u_int8_t *enaddr = NULL;
#if NCARP > 0
	uint8_t *ethshost = NULL;
#endif
	char addr[INET_ADDRSTRLEN];
	int op, changed = 0, target = 0, sender = 0;
	unsigned int len, rdomain;

	rdomain = rtable_l2(m->m_pkthdr.ph_rtableid);

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	if (ifp == NULL) {
		m_freem(m);
		return;
	}
	ac = (struct arpcom *)ifp;

	ea = mtod(m, struct ether_arp *);
	op = ntohs(ea->arp_op);
	if ((op != ARPOP_REQUEST) && (op != ARPOP_REPLY))
		goto out;

	memcpy(&itaddr, ea->arp_tpa, sizeof(itaddr));
	memcpy(&isaddr, ea->arp_spa, sizeof(isaddr));
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	if (ETHER_IS_MULTICAST(&ea->arp_sha[0])) {
		if (!memcmp(ea->arp_sha, etherbroadcastaddr,
		    sizeof (ea->arp_sha))) {
			inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
			log(LOG_ERR, "arp: ether address is broadcast for "
			    "IP address %s!\n", addr);
			goto out;
		}
	}

	/* First try: check target against our addresses */
	sin.sin_addr = itaddr;
	rt = rtalloc(sintosa(&sin), 0, rdomain);
	if (rtisvalid(rt) && ISSET(rt->rt_flags, RTF_LOCAL) &&
	    rt->rt_ifidx == ifp->if_index)
		target = 1;
	rtfree(rt);
	rt = NULL;
	
#if NCARP > 0
	if (target && op == ARPOP_REQUEST && ifp->if_type == IFT_CARP &&
	    !carp_iamatch(ifp, &ethshost))
		goto out;
#endif

	/* Second try: check sender against our addresses */
	sin.sin_addr = isaddr;
	rt = rtalloc(sintosa(&sin), 0, rdomain);
	if (rtisvalid(rt) && ISSET(rt->rt_flags, RTF_LOCAL) &&
	    rt->rt_ifidx == ifp->if_index)
		sender = 1;
	rtfree(rt);
	rt = NULL;

	if (!enaddr)
		enaddr = ac->ac_enaddr;
	if (!memcmp(ea->arp_sha, enaddr, sizeof(ea->arp_sha)))
		goto out;	/* it's from me, ignore it. */

	if (sender && isaddr.s_addr != INADDR_ANY) {
		inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
		log(LOG_ERR,
		   "duplicate IP address %s sent from ethernet address %s\n",
		   addr, ether_sprintf(ea->arp_sha));
		itaddr = isaddr;
		goto reply;
	}

	/* Do we have an ARP cache for the sender?  Create if we are target. */
	rt = arplookup(isaddr.s_addr, target, 0, rdomain);
	if (rt != NULL && (sdl = satosdl(rt->rt_gateway)) != NULL) {
		la = (struct llinfo_arp *)rt->rt_llinfo;
		if (sdl->sdl_alen) {
			if (memcmp(ea->arp_sha, LLADDR(sdl), sdl->sdl_alen)) {
				if (rt->rt_flags &
				    (RTF_PERMANENT_ARP|RTF_LOCAL)) {
					inet_ntop(AF_INET, &isaddr, addr,
					    sizeof(addr));
					log(LOG_WARNING, "arp: attempt to"
					   " overwrite permanent entry for %s"
					   " by %s on %s\n", addr,
					   ether_sprintf(ea->arp_sha),
					   ifp->if_xname);
					goto out;
				} else if (rt->rt_ifp != ifp) {
#if NCARP > 0
					if (ifp->if_type != IFT_CARP)
#endif
					{
						inet_ntop(AF_INET, &isaddr,
						    addr, sizeof(addr));
						log(LOG_WARNING, "arp: attempt"
						   " to overwrite entry for"
						   " %s on %s by %s on %s\n",
						   addr, rt->rt_ifp->if_xname,
						   ether_sprintf(ea->arp_sha),
						   ifp->if_xname);
					}
					goto out;
				} else {
					inet_ntop(AF_INET, &isaddr, addr,
					    sizeof(addr));
					log(LOG_INFO, "arp info overwritten for"
					   " %s by %s on %s\n", addr,
					   ether_sprintf(ea->arp_sha),
					   ifp->if_xname);
					rt->rt_expire = 1;/* no longer static */
				}
			changed = 1;
			}
		} else if (rt->rt_ifp != ifp &&
#if NBRIDGE > 0
		    !SAME_BRIDGE(ifp->if_bridgeport,
		    rt->rt_ifp->if_bridgeport) &&
#endif
#if NCARP > 0
		    !(rt->rt_ifp->if_type == IFT_CARP &&
		    rt->rt_ifp->if_carpdev == ifp) &&
		    !(ifp->if_type == IFT_CARP &&
		    ifp->if_carpdev == rt->rt_ifp) &&
#endif
		    1) {
			inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
			log(LOG_WARNING,
			    "arp: attempt to add entry for %s "
			    "on %s by %s on %s\n", addr,
			    rt->rt_ifp->if_xname,
			    ether_sprintf(ea->arp_sha),
			    ifp->if_xname);
			goto out;
		}
		sdl->sdl_alen = sizeof(ea->arp_sha);
		memcpy(LLADDR(sdl), ea->arp_sha, sizeof(ea->arp_sha));
		if (rt->rt_expire)
			rt->rt_expire = time_second + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		/* Notify userland that an ARP resolution has been done. */
		if (la->la_asked || changed)
			rt_sendmsg(rt, RTM_RESOLVE, rt->rt_ifp->if_rdomain);
		la->la_asked = 0;
		while ((len = ml_len(&la->la_ml)) != 0) {
			mh = ml_dequeue(&la->la_ml);
			la_hold_total--;

			ifp->if_output(ifp, mh, rt_key(rt), rt);

			if (ml_len(&la->la_ml) == len) {
				/* mbuf is back in queue. Discard. */
				while ((mh = ml_dequeue(&la->la_ml)) != NULL) {
					la_hold_total--;
					m_freem(mh);
				}
				break;
			}
		}
	}
reply:
	if (op != ARPOP_REQUEST) {
out:
		rtfree(rt);
		if_put(ifp);
		m_freem(m);
		return;
	}

	rtfree(rt);
	if (target) {
		/* We are the target and already have all info for the reply */
		memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
		memcpy(ea->arp_sha, enaddr, sizeof(ea->arp_sha));
	} else {
		rt = arplookup(itaddr.s_addr, 0, SIN_PROXY, rdomain);
		if (rt == NULL)
			goto out;
		if (rt->rt_ifp->if_type == IFT_CARP && ifp->if_type != IFT_CARP)
			goto out;
		memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
		sdl = satosdl(rt->rt_gateway);
		memcpy(ea->arp_sha, LLADDR(sdl), sizeof(ea->arp_sha));
		rtfree(rt);
	}

	memcpy(ea->arp_tpa, ea->arp_spa, sizeof(ea->arp_spa));
	memcpy(ea->arp_spa, &itaddr, sizeof(ea->arp_spa));
	ea->arp_op = htons(ARPOP_REPLY);
	ea->arp_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	eh = (struct ether_header *)sa.sa_data;
	memcpy(eh->ether_dhost, ea->arp_tha, sizeof(eh->ether_dhost));
#if NCARP > 0
	if (ethshost)
		enaddr = ethshost;
#endif
	memcpy(eh->ether_shost, enaddr, sizeof(eh->ether_shost));

	eh->ether_type = htons(ETHERTYPE_ARP);
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	ifp->if_output(ifp, m, &sa, NULL);
	if_put(ifp);
	return;
}

/*
 * Free an arp entry.
 */
void
arptfree(struct rtentry *rt)
{
	struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	struct sockaddr_dl *sdl = satosdl(rt->rt_gateway);

	if ((sdl != NULL) && (sdl->sdl_family == AF_LINK)) {
		sdl->sdl_alen = 0;
		la->la_asked = 0;
	}

	rtdeletemsg(rt, rt->rt_ifp->if_rdomain);
}

/*
 * Lookup or enter a new address in arptab.
 */
struct rtentry *
arplookup(u_int32_t addr, int create, int proxy, u_int tableid)
{
	struct rtentry *rt;
	struct sockaddr_inarp sin;
	int flags;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_other = proxy ? SIN_PROXY : 0;
	flags = (create) ? (RT_REPORT|RT_RESOLVE) : 0;

	rt = rtalloc((struct sockaddr *)&sin, flags, tableid);
	if (rt == NULL)
		return (NULL);
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK) {
		if (create && (rt->rt_flags & RTF_CLONED))
			rtdeletemsg(rt, tableid);
		rtfree(rt);
		return (NULL);
	}
	return (rt);
}

/*
 * Check whether we do proxy ARP for this address and we point to ourselves.
 */
int
arpproxy(struct in_addr in, unsigned int rtableid)
{
	struct sockaddr_dl *sdl;
	struct rtentry *rt;
	struct ifnet *ifp;
	int found = 0;

	rt = arplookup(in.s_addr, 0, SIN_PROXY, rtableid);
	if (rt == NULL)
		return (0);

	/* Check that arp information are correct. */
	sdl = satosdl(rt->rt_gateway);
	if (sdl->sdl_alen != ETHER_ADDR_LEN) {
		rtfree(rt);
		return (0);
	}

	ifp = rt->rt_ifp;
	if (!memcmp(LLADDR(sdl), LLADDR(ifp->if_sadl), sdl->sdl_alen))
		found = 1;

	rtfree(rt);
	return (found);
}

/*
 * Called from Ethernet interrupt handlers
 * when ether packet type ETHERTYPE_REVARP
 * is received.  Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
revarpinput(struct mbuf *m)
{
	struct arphdr *ar;

	if (m->m_len < sizeof(struct arphdr))
		goto out;
	ar = mtod(m, struct arphdr *);
	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER)
		goto out;
	if (m->m_len < sizeof(struct arphdr) + 2 * (ar->ar_hln + ar->ar_pln))
		goto out;
	switch (ntohs(ar->ar_pro)) {

	case ETHERTYPE_IP:
	case ETHERTYPE_IPTRAILERS:
		in_revarpinput(m);
		return;

	default:
		break;
	}
out:
	m_freem(m);
}

/*
 * RARP for Internet protocols on Ethernet.
 * Algorithm is that given in RFC 903.
 * We are only using for bootstrap purposes to get an ip address for one of
 * our interfaces.  Thus we support no user-interface.
 *
 * Since the contents of the RARP reply are specific to the interface that
 * sent the request, this code must ensure that they are properly associated.
 *
 * Note: also supports ARP via RARP packets, per the RFC.
 */
void
in_revarpinput(struct mbuf *m)
{
	struct ether_arp *ar;
	int op;

	ar = mtod(m, struct ether_arp *);
	op = ntohs(ar->arp_op);
	switch (op) {
	case ARPOP_REQUEST:
	case ARPOP_REPLY:	/* per RFC */
		in_arpinput(m);
		return;
	case ARPOP_REVREPLY:
		break;
	case ARPOP_REVREQUEST:	/* handled by rarpd(8) */
	default:
		goto out;
	}
#ifdef NFSCLIENT
	if (revarp_ifp == NULL)
		goto out;
	if (revarp_ifp->if_index != m->m_pkthdr.ph_ifidx) /* !same interface */
		goto out;
	if (revarp_finished)
		goto wake;
	if (memcmp(ar->arp_tha, ((struct arpcom *)revarp_ifp)->ac_enaddr,
	    sizeof(ar->arp_tha)))
		goto out;
	memcpy(&revarp_srvip, ar->arp_spa, sizeof(revarp_srvip));
	memcpy(&revarp_myip, ar->arp_tpa, sizeof(revarp_myip));
	revarp_finished = 1;
wake:	/* Do wakeup every time in case it was missed. */
	wakeup((caddr_t)&revarp_myip);
#endif

out:
	m_freem(m);
}

/*
 * Send a RARP request for the ip address of the specified interface.
 * The request should be RFC 903-compliant.
 */
void
revarprequest(struct ifnet *ifp)
{
	struct sockaddr sa;
	struct mbuf *m;
	struct ether_header *eh;
	struct ether_arp *ea;
	struct arpcom *ac = (struct arpcom *)ifp;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	MH_ALIGN(m, sizeof(*ea));
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	memset(ea, 0, sizeof(*ea));
	memcpy(eh->ether_dhost, etherbroadcastaddr, sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_REVARP);
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(ARPOP_REVREQUEST);
	memcpy(eh->ether_shost, ac->ac_enaddr, sizeof(ea->arp_tha));
	memcpy(ea->arp_sha, ac->ac_enaddr, sizeof(ea->arp_sha));
	memcpy(ea->arp_tha, ac->ac_enaddr, sizeof(ea->arp_tha));
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	m->m_flags |= M_BCAST;
	ifp->if_output(ifp, m, &sa, NULL);
}

#ifdef NFSCLIENT
/*
 * RARP for the ip address of the specified interface, but also
 * save the ip address of the server that sent the answer.
 * Timeout if no response is received.
 */
int
revarpwhoarewe(struct ifnet *ifp, struct in_addr *serv_in,
    struct in_addr *clnt_in)
{
	int result, count = 20;

	if (revarp_finished)
		return EIO;

	revarp_ifp = if_ref(ifp);
	while (count--) {
		revarprequest(ifp);
		result = tsleep((caddr_t)&revarp_myip, PSOCK, "revarp", hz/2);
		if (result != EWOULDBLOCK)
			break;
	}
	if_put(revarp_ifp);
	revarp_ifp = NULL;
	if (!revarp_finished)
		return ENETUNREACH;

	memcpy(serv_in, &revarp_srvip, sizeof(*serv_in));
	memcpy(clnt_in, &revarp_myip, sizeof(*clnt_in));
	return 0;
}

/* For compatibility: only saves interface address. */
int
revarpwhoami(struct in_addr *in, struct ifnet *ifp)
{
	struct in_addr server;
	return (revarpwhoarewe(ifp, &server, in));
}
#endif /* NFSCLIENT */
