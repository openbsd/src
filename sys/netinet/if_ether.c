/*	$OpenBSD: if_ether.c,v 1.69 2007/11/06 21:52:00 miod Exp $	*/
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

#ifdef INET
#include "carp.h"

#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/if_fddi.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#define SIN(s) ((struct sockaddr_in *)s)
#define SDL(s) ((struct sockaddr_dl *)s)
#define SRP(s) ((struct sockaddr_inarp *)s)

/*
 * ARP trailer negotiation.  Trailer protocol is not IP specific,
 * but ARP request/response use IP addresses.
 */
#define ETHERTYPE_IPTRAILERS ETHERTYPE_TRAIL

/* timer values */
int	arpt_prune = (5*60*1);	/* walk list every 5 minutes */
int	arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	arpt_down = 20;		/* once declared down, don't send for 20 secs */
#define	rt_expire rt_rmx.rmx_expire

void arptfree(struct llinfo_arp *);
void arptimer(void *);
struct llinfo_arp *arplookup(u_int32_t, int, int);
void in_arpinput(struct mbuf *);

LIST_HEAD(, llinfo_arp) llinfo_arp;
struct	ifqueue arpintrq = {0, 0, 0, 50};
int	arp_inuse, arp_allocated, arp_intimer;
int	arp_maxtries = 5;
int	useloopback = 1;	/* use loopback interface for local traffic */
int	arpinit_done = 0;

/* revarp state */
struct in_addr myip, srv_ip;
int myip_initialized = 0;
int revarp_in_progress = 0;
struct ifnet *myip_ifp = NULL;

#ifdef DDB
#include <uvm/uvm_extern.h>

void	db_print_sa(struct sockaddr *);
void	db_print_ifa(struct ifaddr *);
void	db_print_llinfo(caddr_t);
int	db_show_radix_node(struct radix_node *, void *);
#endif

/*
 * Timeout routine.  Age arp_tab entries periodically.
 */
/* ARGSUSED */
void
arptimer(arg)
	void *arg;
{
	struct timeout *to = (struct timeout *)arg;
	int s;
	struct llinfo_arp *la, *nla;

	s = splsoftnet();
	timeout_add(to, arpt_prune * hz);
	for (la = LIST_FIRST(&llinfo_arp); la != LIST_END(&llinfo_arp);
	    la = nla) {
		struct rtentry *rt = la->la_rt;

		nla = LIST_NEXT(la, la_list);
		if (rt->rt_expire && rt->rt_expire <= time_second)
			arptfree(la); /* timer has expired; clear */
	}
	splx(s);
}

/*
 * Parallel to llc_rtrequest.
 */
void
arp_rtrequest(req, rt, info)
	int req;
	struct rtentry *rt;
	struct rt_addrinfo *info;
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};
	struct in_ifaddr *ia;
	struct ifaddr *ifa;

	if (!arpinit_done) {
		static struct timeout arptimer_to;

		arpinit_done = 1;
		/*
		 * We generate expiration times from time.tv_sec
		 * so avoid accidently creating permanent routes.
		 */
		if (time_second == 0) {
			time_second++;
		}

		timeout_set(&arptimer_to, arptimer, &arptimer_to);
		timeout_add(&arptimer_to, hz);
	}

	if (rt->rt_flags & RTF_GATEWAY) {
		if (req != RTM_ADD)
			return;

		/*
		 * linklayers with particular link MTU limitation.  it is a bit
		 * awkward to have FDDI handling here, we should split ARP from
		 * netinet/if_ether.c like NetBSD does.
		 */
		switch (rt->rt_ifp->if_type) {
		case IFT_FDDI:
			if (rt->rt_ifp->if_mtu > FDDIIPMTU)
				rt->rt_rmx.rmx_mtu = FDDIIPMTU;
			break;
		}

		return;
	}

	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 &&
		    SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * Case 1: This route should come from a route to iface.
			 */
			rt_setgate(rt, rt_key(rt),
			    (struct sockaddr *)&null_sdl, 0);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			/*
			 * Give this route an expiration time, even though
			 * it's a "permanent" route, so that routes cloned
			 * from it do not need their expiration time set.
			 */
			rt->rt_expire = time_second;
			/*
			 * linklayers with particular link MTU limitation.
			 */
			switch (rt->rt_ifp->if_type) {
			case IFT_FDDI:
				if ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0 &&
				    (rt->rt_rmx.rmx_mtu > FDDIIPMTU ||
				     (rt->rt_rmx.rmx_mtu == 0 &&
				      rt->rt_ifp->if_mtu > FDDIIPMTU)))
					rt->rt_rmx.rmx_mtu = FDDIIPMTU;
				break;
			}
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			arprequest(rt->rt_ifp,
			    &SIN(rt_key(rt))->sin_addr.s_addr,
			    &SIN(rt_key(rt))->sin_addr.s_addr,
			    (u_char *)LLADDR(SDL(gate)));
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sizeof(null_sdl)) {
			log(LOG_DEBUG, "arp_rtrequest: bad gateway value\n");
			break;
		}
		SDL(gate)->sdl_type = rt->rt_ifp->if_type;
		SDL(gate)->sdl_index = rt->rt_ifp->if_index;
		if (la != 0)
			break; /* This happens on a route change */
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(la, struct llinfo_arp *, sizeof(*la));
		rt->rt_llinfo = (caddr_t)la;
		if (la == 0) {
			log(LOG_DEBUG, "arp_rtrequest: malloc failed\n");
			break;
		}
		arp_inuse++, arp_allocated++;
		Bzero(la, sizeof(*la));
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		LIST_INSERT_HEAD(&llinfo_arp, la, la_list);

		TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
			if (ia->ia_ifp == rt->rt_ifp &&
			    SIN(rt_key(rt))->sin_addr.s_addr ==
			    (IA_SIN(ia))->sin_addr.s_addr)
				break;
		}
		if (ia) {
			/*
			 * This test used to be
			 *	if (lo0ifp->if_flags & IFF_UP)
			 * It allowed local traffic to be forced through
			 * the hardware by configuring the loopback down.
			 * However, it causes problems during network
			 * configuration for boards that can't receive
			 * packets they send.  It is now necessary to clear
			 * "useloopback" and remove the route to force
			 * traffic out to the hardware.
			 *
			 * In 4.4BSD, the above "if" statement checked
			 * rt->rt_ifa against rt_key(rt).  It was changed
			 * to the current form so that we can provide a
			 * better support for multiple IPv4 addresses on a
			 * interface.
			 */
			rt->rt_expire = 0;
			Bcopy(((struct arpcom *)rt->rt_ifp)->ac_enaddr,
			    LLADDR(SDL(gate)),
			    SDL(gate)->sdl_alen = ETHER_ADDR_LEN);
			if (useloopback)
				rt->rt_ifp = lo0ifp;
			/*
			 * make sure to set rt->rt_ifa to the interface
			 * address we are using, otherwise we will have trouble
			 * with source address selection.
			 */
			ifa = &ia->ia_ifa;
			if (ifa != rt->rt_ifa) {
				IFAFREE(rt->rt_ifa);
				ifa->ifa_refcnt++;
				rt->rt_ifa = ifa;
			}
		}
		break;

	case RTM_DELETE:
		if (la == 0)
			break;
		arp_inuse--;
		LIST_REMOVE(la, la_list);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (la->la_hold)
			m_freem(la->la_hold);
		Free((caddr_t)la);
	}
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
void
arprequest(ifp, sip, tip, enaddr)
	struct ifnet *ifp;
	u_int32_t *sip, *tip;
	u_int8_t *enaddr;
{
	struct mbuf *m;
	struct ether_header *eh;
	struct ether_arp *ea;
	struct sockaddr sa;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	MH_ALIGN(m, sizeof(*ea));
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	bzero((caddr_t)ea, sizeof (*ea));
	bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
	    sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_ARP);	/* if_output will not swap */
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(ARPOP_REQUEST);
	bcopy((caddr_t)enaddr, (caddr_t)eh->ether_shost,
	      sizeof(eh->ether_shost));
	bcopy((caddr_t)enaddr, (caddr_t)ea->arp_sha, sizeof(ea->arp_sha));
	bcopy((caddr_t)sip, (caddr_t)ea->arp_spa, sizeof(ea->arp_spa));
	bcopy((caddr_t)tip, (caddr_t)ea->arp_tpa, sizeof(ea->arp_tpa));
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */
int
arpresolve(ac, rt, m, dst, desten)
	struct arpcom *ac;
	struct rtentry *rt;
	struct mbuf *m;
	struct sockaddr *dst;
	u_char *desten;
{
	struct llinfo_arp *la;
	struct sockaddr_dl *sdl;

	if (m->m_flags & M_BCAST) {	/* broadcast */
		bcopy((caddr_t)etherbroadcastaddr, (caddr_t)desten,
		    sizeof(etherbroadcastaddr));
		return (1);
	}
	if (m->m_flags & M_MCAST) {	/* multicast */
		ETHER_MAP_IP_MULTICAST(&SIN(dst)->sin_addr, desten);
		return (1);
	}
	if (rt) {
		la = (struct llinfo_arp *)rt->rt_llinfo;
		if (la == NULL)
			log(LOG_DEBUG, "arpresolve: %s: route without link "
			    "local address\n", inet_ntoa(SIN(dst)->sin_addr));
	} else {
		if ((la = arplookup(SIN(dst)->sin_addr.s_addr, 1, 0)) != NULL)
			rt = la->la_rt;
		else
			log(LOG_DEBUG,
			    "arpresolve: %s: can't allocate llinfo\n",
			    inet_ntoa(SIN(dst)->sin_addr));
	}
	if (la == 0 || rt == 0) {
		m_freem(m);
		return (0);
	}
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time_second) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
		bcopy(LLADDR(sdl), desten, sdl->sdl_alen);
		return 1;
	}
	if (((struct ifnet *)ac)->if_flags & IFF_NOARP)
		return 0;

	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */
	if (la->la_hold)
		m_freem(la->la_hold);
	la->la_hold = m;
	/*
	 * Re-send the ARP request when appropriate.
	 */
#ifdef	DIAGNOSTIC
	if (rt->rt_expire == 0) {
		/* This should never happen. (Should it? -gwr) */
		printf("arpresolve: unresolved and rt_expire == 0\n");
		/* Set expiration time to now (expired). */
		rt->rt_expire = time_second;
	}
#endif
	if (rt->rt_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != time_second) {
			rt->rt_expire = time_second;
			if (la->la_asked++ < arp_maxtries)
				arprequest(&ac->ac_if,
				    &(SIN(rt->rt_ifa->ifa_addr)->sin_addr.s_addr),
				    &(SIN(dst)->sin_addr.s_addr),
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
			}
		}
	}
	return (0);
}

/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
arpintr()
{
	struct mbuf *m;
	struct arphdr *ar;
	int s, len;

	while (arpintrq.ifq_head) {
		s = splnet();
		IF_DEQUEUE(&arpintrq, m);
		splx(s);
		if (m == NULL)
			break;
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
}

/*
 * ARP for Internet protocols on Ethernet.
 * Algorithm is that given in RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 * We no longer handle negotiations for use of trailer protocol:
 * Formerly, ARP replied for protocol type ETHERTYPE_TRAIL sent
 * along with IP replies if we wanted trailers sent to us,
 * and also sent them in response to IP replies.
 * This allowed either end to announce the desire to receive
 * trailer packets.
 * We no longer reply to requests for ETHERTYPE_TRAIL protocol either,
 * but formerly didn't normally send requests.
 */
void
in_arpinput(m)
	struct mbuf *m;
{
	struct ether_arp *ea;
	struct arpcom *ac = (struct arpcom *)m->m_pkthdr.rcvif;
	struct ether_header *eh;
	struct llinfo_arp *la = 0;
	struct rtentry *rt;
	struct in_ifaddr *ia;
#if NBRIDGE > 0
	struct in_ifaddr *bridge_ia = NULL;
#endif
#if NCARP > 0
	u_int32_t count = 0, index = 0;
#endif
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	u_int8_t *enaddr = NULL;
	int op;

	ea = mtod(m, struct ether_arp *);
	op = ntohs(ea->arp_op);
	if ((op != ARPOP_REQUEST) && (op != ARPOP_REPLY))
		goto out;
#if notyet
	if ((op == ARPOP_REPLY) && (m->m_flags & (M_BCAST|M_MCAST))) {
		log(LOG_ERR,
		    "arp: received reply to broadcast or multicast address\n");
		goto out;
	}
#endif

	bcopy((caddr_t)ea->arp_tpa, (caddr_t)&itaddr, sizeof(itaddr));
	bcopy((caddr_t)ea->arp_spa, (caddr_t)&isaddr, sizeof(isaddr));

	TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
		if (itaddr.s_addr != ia->ia_addr.sin_addr.s_addr)
			continue;

#if NCARP > 0
		if (ia->ia_ifp->if_type == IFT_CARP &&
		    ((ia->ia_ifp->if_flags & (IFF_UP|IFF_RUNNING)) ==
		    (IFF_UP|IFF_RUNNING))) {
			index++;
			if (ia->ia_ifp == m->m_pkthdr.rcvif &&
			    carp_iamatch(ia, ea->arp_sha,
			    &count, index))
				break;
		} else
#endif
			if (ia->ia_ifp == m->m_pkthdr.rcvif)
				break;
#if NBRIDGE > 0
		/*
		 * If the interface we received the packet on
		 * is part of a bridge, check to see if we need
		 * to "bridge" the packet to ourselves at this
		 * layer.  Note we still prefer a perfect match,
		 * but allow this weaker match if necessary.
		 */
		if (m->m_pkthdr.rcvif->if_bridge != NULL) {
			if (m->m_pkthdr.rcvif->if_bridge ==
			    ia->ia_ifp->if_bridge)
				bridge_ia = ia;
#if NCARP > 0
			else if (ia->ia_ifp->if_carpdev != NULL &&
			    m->m_pkthdr.rcvif->if_bridge ==
			    ia->ia_ifp->if_carpdev->if_bridge &&
			    carp_iamatch(ia, ea->arp_sha,
			    &count, index))
				bridge_ia = ia;
#endif
		}
#endif
	}

#if NBRIDGE > 0
	if (ia == NULL && bridge_ia != NULL) {
		ia = bridge_ia;
		ac = (struct arpcom *)bridge_ia->ia_ifp;
	}
#endif

	if (ia == NULL) {
		TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
			if (isaddr.s_addr != ia->ia_addr.sin_addr.s_addr)
				continue;
			if (ia->ia_ifp == m->m_pkthdr.rcvif)
				break;
		}
	}

	if (ia == NULL && m->m_pkthdr.rcvif->if_type != IFT_CARP) {
		struct ifaddr *ifa;

		TAILQ_FOREACH(ifa, &m->m_pkthdr.rcvif->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == AF_INET)
				break;
		}
		if (ifa)
			ia = (struct in_ifaddr *)ifa;
	}

	if (ia == NULL)
		goto out;

	if (!enaddr)
		enaddr = ac->ac_enaddr;
	myaddr = ia->ia_addr.sin_addr;

	if (!bcmp((caddr_t)ea->arp_sha, enaddr, sizeof (ea->arp_sha)))
		goto out;	/* it's from me, ignore it. */
	if (ETHER_IS_MULTICAST (&ea->arp_sha[0]))
		if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)etherbroadcastaddr,
		    sizeof (ea->arp_sha))) {
			log(LOG_ERR, "arp: ether address is broadcast for "
			    "IP address %s!\n", inet_ntoa(isaddr));
			goto out;
		}
	if (myaddr.s_addr && isaddr.s_addr == myaddr.s_addr) {
		log(LOG_ERR,
		   "duplicate IP address %s sent from ethernet address %s\n",
		   inet_ntoa(isaddr), ether_sprintf(ea->arp_sha));
		itaddr = myaddr;
		goto reply;
	}
	la = arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0);
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) {
		if (sdl->sdl_alen) {
		    if (bcmp(ea->arp_sha, LLADDR(sdl), sdl->sdl_alen)) {
			if (rt->rt_flags & RTF_PERMANENT_ARP) {
				log(LOG_WARNING,
				   "arp: attempt to overwrite permanent "
				   "entry for %s by %s on %s\n",
				   inet_ntoa(isaddr),
				   ether_sprintf(ea->arp_sha),
				   ac->ac_if.if_xname);
				goto out;
			} else if (rt->rt_ifp != &ac->ac_if) {
				log(LOG_WARNING,
				   "arp: attempt to overwrite entry for %s "
				   "on %s by %s on %s\n",
				   inet_ntoa(isaddr), rt->rt_ifp->if_xname,
				   ether_sprintf(ea->arp_sha),
				   ac->ac_if.if_xname);
				goto out;
			} else {
				log(LOG_INFO,
				   "arp info overwritten for %s by %s on %s\n",
				   inet_ntoa(isaddr),
				   ether_sprintf(ea->arp_sha),
				   ac->ac_if.if_xname);
				rt->rt_expire = 1; /* no longer static */
			}
		    }
		} else if (rt->rt_ifp != &ac->ac_if && !(ac->ac_if.if_bridge &&
		    (rt->rt_ifp->if_bridge == ac->ac_if.if_bridge)) &&
		    !(rt->rt_ifp->if_type == IFT_CARP &&
		    rt->rt_ifp->if_carpdev == &ac->ac_if) &&
		    !(ac->ac_if.if_type == IFT_CARP &&
		    ac->ac_if.if_carpdev == rt->rt_ifp)) {
		    log(LOG_WARNING,
			"arp: attempt to add entry for %s "
			"on %s by %s on %s\n",
			inet_ntoa(isaddr), rt->rt_ifp->if_xname,
			ether_sprintf(ea->arp_sha),
			ac->ac_if.if_xname);
		    goto out;
		}
		bcopy(ea->arp_sha, LLADDR(sdl),
		    sdl->sdl_alen = sizeof(ea->arp_sha));
		if (rt->rt_expire)
			rt->rt_expire = time_second + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;
		if (la->la_hold) {
			(*ac->ac_if.if_output)(&ac->ac_if, la->la_hold,
				rt_key(rt), rt);
			la->la_hold = 0;
		}
	}
reply:
	if (op != ARPOP_REQUEST) {
	out:
		m_freem(m);
		return;
	}
	if (itaddr.s_addr == myaddr.s_addr) {
		/* I am the target */
		bcopy(ea->arp_sha, ea->arp_tha, sizeof(ea->arp_sha));
		bcopy(enaddr, ea->arp_sha, sizeof(ea->arp_sha));
	} else {
		la = arplookup(itaddr.s_addr, 0, SIN_PROXY);
		if (la == 0)
			goto out;
		rt = la->la_rt;
		if (rt->rt_ifp->if_type == IFT_CARP &&
		    m->m_pkthdr.rcvif->if_type != IFT_CARP)
			goto out;
		bcopy(ea->arp_sha, ea->arp_tha, sizeof(ea->arp_sha));
		sdl = SDL(rt->rt_gateway);
		bcopy(LLADDR(sdl), ea->arp_sha, sizeof(ea->arp_sha));
	}

	bcopy(ea->arp_spa, ea->arp_tpa, sizeof(ea->arp_spa));
	bcopy(&itaddr, ea->arp_spa, sizeof(ea->arp_spa));
	ea->arp_op = htons(ARPOP_REPLY);
	ea->arp_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	eh = (struct ether_header *)sa.sa_data;
	bcopy(ea->arp_tha, eh->ether_dhost, sizeof(eh->ether_dhost));
#if NCARP > 0
	if (ac->ac_if.if_type == IFT_CARP && ac->ac_if.if_flags & IFF_LINK1)
		bcopy(((struct arpcom *)ac->ac_if.if_carpdev)->ac_enaddr,
		    eh->ether_shost, sizeof(eh->ether_shost));
	else
#endif
		bcopy(enaddr, eh->ether_shost, sizeof(eh->ether_shost));

	eh->ether_type = htons(ETHERTYPE_ARP);
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa, (struct rtentry *)0);
	return;
}

/*
 * Free an arp entry.
 */
void
arptfree(la)
	struct llinfo_arp *la;
{
	struct rtentry *rt = la->la_rt;
	struct sockaddr_dl *sdl;

	if (rt == 0)
		panic("arptfree");
	if (rt->rt_refcnt > 0 && (sdl = SDL(rt->rt_gateway)) &&
	    sdl->sdl_family == AF_LINK) {
		sdl->sdl_alen = 0;
		la->la_asked = 0;
		rt->rt_flags &= ~RTF_REJECT;
		return;
	}
	rtrequest(RTM_DELETE, rt_key(rt), (struct sockaddr *)0, rt_mask(rt),
	    0, (struct rtentry **)0, 0);
}

/*
 * Lookup or enter a new address in arptab.
 */
struct llinfo_arp *
arplookup(addr, create, proxy)
	u_int32_t addr;
	int create, proxy;
{
	struct rtentry *rt;
	static struct sockaddr_inarp sin;

	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_other = proxy ? SIN_PROXY : 0;
	rt = rtalloc1(sintosa(&sin), create, 0);
	if (rt == 0)
		return (0);
	rt->rt_refcnt--;
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK) {
		if (create) {
			if (rt->rt_refcnt <= 0 &&
			    (rt->rt_flags & RTF_CLONED) != 0) {
				rtrequest(RTM_DELETE,
				    (struct sockaddr *)rt_key(rt),
				    rt->rt_gateway, rt_mask(rt), rt->rt_flags,
				    0, 0);
			}
		}
		return (0);
	}
	return ((struct llinfo_arp *)rt->rt_llinfo);
}

int
arpioctl(cmd, data)
	u_long cmd;
	caddr_t data;
{

	return (EOPNOTSUPP);
}

void
arp_ifinit(ac, ifa)
	struct arpcom *ac;
	struct ifaddr *ifa;
{

	/* Warn the user if another station has this IP address. */
	arprequest(&ac->ac_if,
	    &(IA_SIN(ifa)->sin_addr.s_addr),
	    &(IA_SIN(ifa)->sin_addr.s_addr),
	    ac->ac_enaddr);
	ifa->ifa_rtrequest = arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}

/*
 * Called from Ethernet interrupt handlers
 * when ether packet type ETHERTYPE_REVARP
 * is received.  Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
revarpinput(m)
	struct mbuf *m;
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
in_revarpinput(m)
	struct mbuf *m;
{
	struct ifnet *ifp;
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
	if (!revarp_in_progress)
		goto out;
	ifp = m->m_pkthdr.rcvif;
	if (ifp != myip_ifp) /* !same interface */
		goto out;
	if (myip_initialized)
		goto wake;
	if (bcmp(ar->arp_tha, ((struct arpcom *)ifp)->ac_enaddr,
	    sizeof(ar->arp_tha)))
		goto out;
	bcopy((caddr_t)ar->arp_spa, (caddr_t)&srv_ip, sizeof(srv_ip));
	bcopy((caddr_t)ar->arp_tpa, (caddr_t)&myip, sizeof(myip));
	myip_initialized = 1;
wake:	/* Do wakeup every time in case it was missed. */
	wakeup((caddr_t)&myip);

out:
	m_freem(m);
}

/*
 * Send a RARP request for the ip address of the specified interface.
 * The request should be RFC 903-compliant.
 */
void
revarprequest(ifp)
	struct ifnet *ifp;
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
	bzero((caddr_t)ea, sizeof(*ea));
	bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
	    sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_REVARP);
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(ARPOP_REVREQUEST);
	bcopy((caddr_t)ac->ac_enaddr, (caddr_t)eh->ether_shost,
	   sizeof(ea->arp_tha));
	bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->arp_sha,
	   sizeof(ea->arp_sha));
	bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->arp_tha,
	   sizeof(ea->arp_tha));
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	ifp->if_output(ifp, m, &sa, (struct rtentry *)0);
}

/*
 * RARP for the ip address of the specified interface, but also
 * save the ip address of the server that sent the answer.
 * Timeout if no response is received.
 */
int
revarpwhoarewe(ifp, serv_in, clnt_in)
	struct ifnet *ifp;
	struct in_addr *serv_in;
	struct in_addr *clnt_in;
{
	int result, count = 20;

	if (myip_initialized)
		return EIO;

	myip_ifp = ifp;
	revarp_in_progress = 1;
	while (count--) {
		revarprequest(ifp);
		result = tsleep((caddr_t)&myip, PSOCK, "revarp", hz/2);
		if (result != EWOULDBLOCK)
			break;
	}
	revarp_in_progress = 0;
	if (!myip_initialized)
		return ENETUNREACH;

	bcopy((caddr_t)&srv_ip, serv_in, sizeof(*serv_in));
	bcopy((caddr_t)&myip, clnt_in, sizeof(*clnt_in));
	return 0;
}

/* For compatibility: only saves interface address. */
int
revarpwhoami(in, ifp)
	struct in_addr *in;
	struct ifnet *ifp;
{
	struct in_addr server;
	return (revarpwhoarewe(ifp, &server, in));
}


#ifdef DDB

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

void
db_print_sa(sa)
	struct sockaddr *sa;
{
	int len;
	u_char *p;

	if (sa == 0) {
		db_printf("[NULL]");
		return;
	}

	p = (u_char *)sa;
	len = sa->sa_len;
	db_printf("[");
	while (len > 0) {
		db_printf("%d", *p);
		p++;
		len--;
		if (len)
			db_printf(",");
	}
	db_printf("]\n");
}

void
db_print_ifa(ifa)
	struct ifaddr *ifa;
{
	if (ifa == 0)
		return;
	db_printf("  ifa_addr=");
	db_print_sa(ifa->ifa_addr);
	db_printf("  ifa_dsta=");
	db_print_sa(ifa->ifa_dstaddr);
	db_printf("  ifa_mask=");
	db_print_sa(ifa->ifa_netmask);
	db_printf("  flags=0x%x, refcnt=%d, metric=%d\n",
	    ifa->ifa_flags, ifa->ifa_refcnt, ifa->ifa_metric);
}

void
db_print_llinfo(li)
	caddr_t li;
{
	struct llinfo_arp *la;

	if (li == 0)
		return;
	la = (struct llinfo_arp *)li;
	db_printf("  la_rt=%p la_hold=%p, la_asked=0x%lx\n",
	    la->la_rt, la->la_hold, la->la_asked);
}

/*
 * Function to pass to rn_walktree().
 * Return non-zero error to abort walk.
 */
int
db_show_radix_node(rn, w)
	struct radix_node *rn;
	void *w;
{
	struct rtentry *rt = (struct rtentry *)rn;

	db_printf("rtentry=%p", rt);

	db_printf(" flags=0x%x refcnt=%d use=%ld expire=%ld\n",
	    rt->rt_flags, rt->rt_refcnt, rt->rt_use, rt->rt_expire);

	db_printf(" key="); db_print_sa(rt_key(rt));
	db_printf(" mask="); db_print_sa(rt_mask(rt));
	db_printf(" gw="); db_print_sa(rt->rt_gateway);

	db_printf(" ifp=%p ", rt->rt_ifp);
	if (rt->rt_ifp)
		db_printf("(%s)", rt->rt_ifp->if_xname);
	else
		db_printf("(NULL)");

	db_printf(" ifa=%p\n", rt->rt_ifa);
	db_print_ifa(rt->rt_ifa);

	db_printf(" genmask="); db_print_sa(rt->rt_genmask);

	db_printf(" gwroute=%p llinfo=%p\n", rt->rt_gwroute, rt->rt_llinfo);
	db_print_llinfo(rt->rt_llinfo);
	return (0);
}

/*
 * Function to print all the route trees.
 * Use this from ddb:  "call db_show_arptab"
 */
int
db_show_arptab()
{
	struct radix_node_head *rnh;
	rnh = rt_gettable(AF_INET, 0);
	db_printf("Route tree for AF_INET\n");
	if (rnh == NULL) {
		db_printf(" (not initialized)\n");
		return (0);
	}
	rn_walktree(rnh, db_show_radix_node, NULL);
	return (0);
}
#endif
#endif /* INET */
