/*	$OpenBSD: if_ieee1394arp.c,v 1.1 2002/08/19 02:31:02 itojun Exp $	*/
/*	$NetBSD: if_ieee1394arp.c,v 1.8 2002/06/09 16:33:37 itojun Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifdef __KERNEL_RCSID
__KERNEL_RCSID(0, "$NetBSD: if_ieee1394arp.c,v 1.8 2002/06/09 16:33:37 itojun Exp $");
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ieee1394.h>
#include <net/if_types.h>
#include <net/if_media.h>
#ifdef __NetBSD__
#include <net/ethertypes.h>
#endif
#include <net/route.h>
#ifdef __OpenBSD__
#include <net/if_arp.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#ifdef __NetBSD__
#include <netinet/if_inarp.h>
#endif
#ifdef __OpenBSD__
#include <netinet/if_ether.h>
#endif
#include <netinet/if_ieee1394arp.h>

#ifndef __NetBSD__
#define in_fmtaddr	inet_ntoa
#endif

#define	SIN(s)	((struct sockaddr_in *)s)
#define	SDL(s)	((struct sockaddr_dl *)s)

static void ieee1394arprequest(struct ifnet *, struct in_addr *,
		struct in_addr *);
static struct llinfo_arp *ieee1394arplookup(struct in_addr *, int, int);

/* XXX: timer values */
int	ieee1394arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	ieee1394arpt_down = 20;		/* once declared down, don't send for 20 secs */
int	ieee1394arp_maxtries = 5;
#define	rt_expire	rt_rmx.rmx_expire

/* import from netinet/if_arp.c */
extern LIST_HEAD(, llinfo_arp) llinfo_arp;
extern int arp_inuse, arp_allocated;
#ifdef __NetBSD__
extern int arp_locked;
#else
static int arp_locked;	/*XXX*/
#endif
extern int arpinit_done;

#ifdef __NetBSD__
#if __NetBSD_Version__ >= 105060000
extern struct arpstat arpstat;
#else
struct arpstat {
	u_quad_t	as_sndtotal;	/* total packets sent */
	u_quad_t	as_sndreply;	/* replies sent */
	u_quad_t	as_sndrequest;	/* requests sent */

	u_quad_t	as_rcvtotal;	/* total packets received */
	u_quad_t	as_rcvrequest;	/* valid requests received */
	u_quad_t	as_rcvreply;	/* replies received */
	u_quad_t	as_rcvmcast;    /* multicast/broadcast received */
	u_quad_t	as_rcvbadproto;	/* unknown protocol type received */
	u_quad_t	as_rcvbadlen;	/* bad (short) length received */
	u_quad_t	as_rcvzerotpa;	/* received w/ null target ip */
	u_quad_t	as_rcvzerospa;	/* received w/ null src ip */
	u_quad_t	as_rcvnoint;	/* couldn't map to interface */
	u_quad_t	as_rcvlocalsha;	/* received from local hw address */
	u_quad_t	as_rcvbcastsha;	/* received w/ broadcast src */
	u_quad_t	as_rcvlocalspa;	/* received for a local ip [dup!] */
	u_quad_t	as_rcvoverperm;	/* attempts to overwrite static info */
	u_quad_t	as_rcvoverint;	/* attempts to overwrite wrong if */
	u_quad_t	as_rcvover;	/* entries overwritten! */
	u_quad_t	as_rcvlenchg;	/* changes in hw add len */

	u_quad_t	as_dfrtotal;	/* deferred pending ARP resolution. */
	u_quad_t	as_dfrsent;	/* deferred, then sent */
	u_quad_t	as_dfrdropped;	/* deferred, then dropped */

	u_quad_t	as_allocfail;	/* Failures to allocate llinfo */
} arpstat;
#endif
#endif

static __inline int arp_lock_try(int);
static __inline void arp_unlock(void);

static __inline int
arp_lock_try(recurse)
	int recurse;
{
	int s;

	/*
	 * Use splvm() -- we're blocking things that would cause
	 * mbuf allocation.
	 */
	s = splvm();
	if (!recurse && arp_locked) {
		splx(s);
		return 0;
	}
	arp_locked++;
	splx(s);
	return 1;
}

static __inline void
arp_unlock()
{
	int s;

	s = splvm();
	arp_locked--;
	splx(s);
}

#define	ARP_LOCK(x)	(void)arp_lock_try(x)
#define	ARP_UNLOCK()	arp_unlock();

/*
 * Parallel to llc_rtrequest.
 */
void
ieee1394arp_rtrequest(int req, struct rtentry *rt, struct rt_addrinfo *info)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	static struct sockaddr_dl null_sdl =
	    { sizeof(null_sdl) - sizeof(null_sdl.sdl_data) +
	      sizeof(struct ieee1394_hwaddr), AF_LINK };
	struct mbuf *mold;
	int s;
	struct in_ifaddr *ia;
	struct ifaddr *ifa;

	if (!arpinit_done) {
		/*
		 * Dummy call to arp_rtrequest to initialize arp timer.
		 */
		struct rtentry rtdummy;
		rtdummy.rt_flags = RTF_GATEWAY;
		arp_rtrequest(0, &rtdummy, NULL);
	}
	if (rt->rt_flags & RTF_GATEWAY)
		return;

	ARP_LOCK(1);		/* we may already be locked here. */

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
					(struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			/*
			 * Give this route an expiration time, even though
			 * it's a "permanent" route, so that routes cloned
			 * from it do not need their expiration time set.
			 */
			rt->rt_expire = time.tv_sec;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			ieee1394arprequest(rt->rt_ifp,
			    &SIN(rt_key(rt))->sin_addr,
			    &SIN(rt_key(rt))->sin_addr);
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < null_sdl.sdl_len) {
			log(LOG_DEBUG, "ieee1394arp_rtrequest: bad gateway value\n");
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
		la = malloc(sizeof(*la), M_RTABLE, M_DONTWAIT);
		rt->rt_llinfo = (caddr_t)la;
		if (la == 0) {
			log(LOG_DEBUG, "ieee1394arp_rtrequest: malloc failed\n");
			break;
		}
		arp_inuse++, arp_allocated++;
		memset(la, 0, sizeof(*la));
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		LIST_INSERT_HEAD(&llinfo_arp, la, la_list);

#ifdef __NetBSD__
		INADDR_TO_IA(SIN(rt_key(rt))->sin_addr, ia);
		while (ia && ia->ia_ifp != rt->rt_ifp)
			NEXT_IA_WITH_SAME_ADDR(ia);
#else
		ia = NULL;
		for (ifa = rt->rt_ifp->if_addrlist.tqh_first;
		    ifa; ifa = ifa->ifa_list.tqe_next) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			if (in_hosteq(SIN(rt_key(rt))->sin_addr,
			    ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr)) {
				ia = (struct in_ifaddr *)ifa;
				break;
			}
		}
#endif
		if (ia) {
			/*
			 * This test used to be
			 *	if (loif.if_flags & IFF_UP)
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
			 * better support for multiple IPv4 address on a
			 * interface.
			 */
			rt->rt_expire = 0;
			memcpy(LLADDR(SDL(gate)),
			    LLADDR(rt->rt_ifp->if_sadl),
			    SDL(gate)->sdl_alen = rt->rt_ifp->if_addrlen);
#if NLOOP > 0
			if (useloopback)
				rt->rt_ifp = &loif[0];
#endif
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

		s = splnet();
		mold = la->la_hold;
		la->la_hold = 0;
		splx(s);

		if (mold)
			m_freem(mold);

		Free((caddr_t)la);
	}
	ARP_UNLOCK();
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- ieee1394arp header source ip address
 *	- ieee1394arp header target ip address
 */
static void
ieee1394arprequest(struct ifnet *ifp, struct in_addr *sip, struct in_addr *tip)
{
	struct mbuf *m;
	struct ieee1394_arp *iar;
	struct sockaddr sa;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*iar);
	m->m_pkthdr.len = m->m_len;
	MH_ALIGN(m, m->m_len);
	iar = mtod(m, struct ieee1394_arp *);
	memset(iar, 0, m->m_len);
	iar->iar_arp.ar_hrd = htons(ARPHRD_IEEE1394);
	iar->iar_arp.ar_pro = htons(ETHERTYPE_IP);
	iar->iar_arp.ar_hln = sizeof(struct ieee1394_hwaddr);
	iar->iar_arp.ar_pln = sizeof(struct in_addr);
	iar->iar_arp.ar_op = htons(ARPOP_REQUEST);
	memcpy(&iar->iar_sip, sip, sizeof(struct in_addr));
	memcpy(&iar->iar_tip, tip, sizeof(struct in_addr));
	memcpy(&iar->iar_sha, LLADDR(ifp->if_sadl), ifp->if_addrlen);
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	m->m_flags |= M_BCAST;
#ifdef __NetBSD__
	arpstat.as_sndtotal++;
	arpstat.as_sndrequest++;
#endif
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desthw is filled in.  If there is no entry in ieee1394arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desthw has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */
int
ieee1394arpresolve(struct ifnet *ifp, struct rtentry *rt, struct mbuf *m,
    struct sockaddr *dst, struct ieee1394_hwaddr *desthw)
{
	struct llinfo_arp *la;
	struct sockaddr_dl *sdl;
	struct mbuf *mold;
	int s;

	if (rt)
		la = (struct llinfo_arp *)rt->rt_llinfo;
	else {
		if ((la = ieee1394arplookup(&SIN(dst)->sin_addr, 1, 0)) != NULL)
			rt = la->la_rt;
	}
	if (la == 0 || rt == 0) {
#ifdef __NetBSD__
		arpstat.as_allocfail++;
#endif
		log(LOG_DEBUG, "ieee1394arpresolve: can't allocate llinfo\n");
		m_freem(m);
		return (0);
	}
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time.tv_sec) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
		memcpy(desthw, LLADDR(sdl),
		    min(sdl->sdl_alen, ifp->if_addrlen));
		return 1;
	}
	/*
	 * There is an ieee1394arptab entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */

#ifdef __NetBSD__
	arpstat.as_dfrtotal++;
#endif
	s = splnet();
	mold = la->la_hold;
	la->la_hold = m;
	splx(s);

	if (mold) {
#ifdef __NetBSD__
		arpstat.as_dfrdropped++;
#endif
		m_freem(mold);
	}

	/*
	 * Re-send the ARP request when appropriate.
	 */
#ifdef	DIAGNOSTIC
	if (rt->rt_expire == 0) {
		/* This should never happen. (Should it? -gwr) */
		printf("ieee1394arpresolve: unresolved and rt_expire == 0\n");
		/* Set expiration time to now (expired). */
		rt->rt_expire = time.tv_sec;
	}
#endif
	if (rt->rt_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != time.tv_sec) {
			rt->rt_expire = time.tv_sec;
			if (la->la_asked++ < ieee1394arp_maxtries)
				ieee1394arprequest(ifp,
				    &SIN(rt->rt_ifa->ifa_addr)->sin_addr,
				    &SIN(dst)->sin_addr);
			else {
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += ieee1394arpt_down;
				la->la_asked = 0;
			}
		}
	}
	return (0);
}

void
in_ieee1394arpinput(struct mbuf *m)
{
	struct ieee1394_arp *iar;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct llinfo_arp *la = 0;
	struct rtentry  *rt;
	struct in_ifaddr *ia;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	int op;
	struct mbuf *mold;
	int s;
#ifdef __OpenBSD__
	struct ifaddr *ifa;
#endif

	iar = mtod(m, struct ieee1394_arp *);
	op = ntohs(iar->iar_arp.ar_op);
	memcpy(&isaddr, &iar->iar_sip, sizeof(isaddr));
	memcpy(&itaddr, &iar->iar_tip, sizeof(itaddr));

	if (m->m_flags & (M_BCAST|M_MCAST)) {
#ifdef __NetBSD__
		arpstat.as_rcvmcast++;
#endif
	}

	/*
	 * If the target IP address is zero, ignore the packet.
	 * This prevents the code below from tring to answer
	 * when we are using IP address zero (booting).
	 */
	if (in_nullhost(itaddr)) {
#ifdef __NetBSD__
		arpstat.as_rcvzerotpa++;
#endif
		goto out;
	}

	/*
	 * If the source IP address is zero, this is most likely a
	 * confused host trying to use IP address zero. (Windoze?)
	 * XXX: Should we bother trying to reply to these?
	 */
	if (in_nullhost(isaddr)) {
#ifdef __NetBSD__
		arpstat.as_rcvzerospa++;
#endif
		goto out;
	}

	/*
	 * Search for a matching interface address
	 * or any address on the interface to use
	 * as a dummy address in the rest of this function
	 */
#ifdef __NetBSD__
	INADDR_TO_IA(itaddr, ia);
	while ((ia != NULL) && ia->ia_ifp != m->m_pkthdr.rcvif)
		NEXT_IA_WITH_SAME_ADDR(ia);
#else
	ia = NULL;
	for (ifa = m->m_pkthdr.rcvif->if_addrlist.tqh_first;
	    ifa; ifa = ifa->ifa_list.tqe_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (in_hosteq(itaddr,
		    ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr)) {
			ia = (struct in_ifaddr *)ifa;
			break;
		}
	}
#endif

	if (ia == NULL) {
#ifdef __NetBSD__
		INADDR_TO_IA(isaddr, ia);
		while ((ia != NULL) && ia->ia_ifp != m->m_pkthdr.rcvif)
			NEXT_IA_WITH_SAME_ADDR(ia);
#else
		for (ifa = m->m_pkthdr.rcvif->if_addrlist.tqh_first;
		    ifa; ifa = ifa->ifa_list.tqe_next) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			if (in_hosteq(isaddr,
			    ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr)) {
				ia = (struct in_ifaddr *)ifa;
				break;
			}
		}
#endif

		if (ia == NULL) {
#ifdef __NetBSD__
			IFP_TO_IA(ifp, ia);
#else
			for (ifa = ifp->if_addrlist.tqh_first;
			    ifa; ifa = ifa->ifa_list.tqe_next) {
				if (ifa->ifa_addr->sa_family == AF_INET) {
					ia = (struct in_ifaddr *)ifa;
					break;
				}
			}
#endif
			if (ia == NULL) {
#ifdef __NetBSD__
				arpstat.as_rcvnoint++;
#endif
				goto out;
			}
		}
	}

	myaddr = ia->ia_addr.sin_addr;

	if (memcmp(&iar->iar_sha, LLADDR(ifp->if_sadl), ifp->if_addrlen) == 0) {
#ifdef __NetBSD__
		arpstat.as_rcvlocalsha++;
#endif
		goto out;	/* it's from me, ignore it. */
	}

	if (memcmp(iar->iar_sha.iha_uid, ifp->if_broadcastaddr,
	    IEEE1394_ADDR_LEN) == 0) {
#ifdef __NetBSD__
		arpstat.as_rcvbcastsha++;
#endif
		log(LOG_ERR,
		    "%s: ieee1394arp: link address is broadcast for IP address %s!\n",
		    ifp->if_xname, in_fmtaddr(isaddr));
		goto out;
	}

	if (in_hosteq(isaddr, myaddr)) {
#ifdef __NetBSD__
		arpstat.as_rcvlocalspa++;
#endif
		log(LOG_ERR,
		   "duplicate IP address %s sent from link address %s\n",
		   in_fmtaddr(isaddr), ieee1394_sprintf(iar->iar_sha.iha_uid));
		itaddr = myaddr;
		goto reply;
	}
	la = ieee1394arplookup(&isaddr, in_hosteq(itaddr, myaddr), 0);
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) {
		if (sdl->sdl_alen &&
		    memcmp(&iar->iar_sha, LLADDR(sdl), sdl->sdl_alen) != 0) {
			if (rt->rt_flags & RTF_STATIC) {
#ifdef __NetBSD__
				arpstat.as_rcvoverperm++;
#endif
				log(LOG_INFO,
				    "%s tried to overwrite permanent ieee1394arp info"
				    " for %s\n",
				    ieee1394_sprintf(iar->iar_sha.iha_uid),
				    in_fmtaddr(isaddr));
				goto out;
			} else if (rt->rt_ifp != ifp) {
#ifdef __NetBSD__
				arpstat.as_rcvoverint++;
#endif
				log(LOG_INFO,
				    "%s on %s tried to overwrite "
				    "ieee1394arp info for %s on %s\n",
				    ieee1394_sprintf(iar->iar_sha.iha_uid),
				    ifp->if_xname, in_fmtaddr(isaddr),
				    rt->rt_ifp->if_xname);
				    goto out;
			} else {
#ifdef __NetBSD__
				arpstat.as_rcvover++;
#endif
				log(LOG_INFO,
				    "ieee1394arp info overwritten for %s by %s\n",
				    in_fmtaddr(isaddr),
				    ieee1394_sprintf(iar->iar_sha.iha_uid));
			}
		}
		/*
		 * sanity check for the address length.
		 * XXX this does not work for protocols with variable address
		 * length. -is
		 */
		if (sdl->sdl_alen &&
		    sdl->sdl_alen != iar->iar_arp.ar_hln) {
#ifdef __NetBSD__
			arpstat.as_rcvlenchg++;
#endif
			log(LOG_WARNING,
			    "ieee1394arp from %s: new addr len %d, was %d",
			    in_fmtaddr(isaddr), iar->iar_arp.ar_hln, sdl->sdl_alen);
		}
		if (ifp->if_addrlen != iar->iar_arp.ar_hln) {
#ifdef __NetBSD__
			arpstat.as_rcvbadlen++;
#endif
			log(LOG_WARNING,
			    "ieee1394arp from %s: addr len: new %d, i/f %d (ignored)",
			    in_fmtaddr(isaddr), iar->iar_arp.ar_hln,
			    ifp->if_addrlen);
			goto reply;
		}
		memcpy(LLADDR(sdl), &iar->iar_sha,
		    sdl->sdl_alen = iar->iar_arp.ar_hln);
		if (rt->rt_expire)
			rt->rt_expire = time.tv_sec + ieee1394arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;

		s = splnet();
		mold = la->la_hold;
		la->la_hold = 0;
		splx(s);

		if (mold) {
#ifdef __NetBSD__
			arpstat.as_dfrsent++;
#endif
			(*ifp->if_output)(ifp, mold, rt_key(rt), rt);
		}
	}
reply:
	if (op != ARPOP_REQUEST) {
		if (op == ARPOP_REPLY) {
#ifdef __NetBSD__
			arpstat.as_rcvreply++;
#endif
		}
	out:
		m_freem(m);
		return;
	}
#ifdef __NetBSD__
	arpstat.as_rcvrequest++;
#endif
	if (in_hosteq(itaddr, myaddr)) {
		/* I am the target */
		memcpy(&iar->iar_sha, LLADDR(ifp->if_sadl), ifp->if_addrlen);
	} else {
		la = ieee1394arplookup(&itaddr, 0, SIN_PROXY);
		if (la == 0)
			goto out;
		rt = la->la_rt;
		sdl = SDL(rt->rt_gateway);
		memcpy(&iar->iar_sha, LLADDR(sdl), ifp->if_addrlen);
	}

	memcpy(&iar->iar_tip, &iar->iar_sip, sizeof(struct in_addr));
	memcpy(&iar->iar_sip, &itaddr, sizeof(struct in_addr));
	iar->iar_arp.ar_op = htons(ARPOP_REPLY);
	iar->iar_arp.ar_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	iar->iar_arp.ar_hrd = htons(ARPHRD_IEEE1394);
	m->m_flags |= M_BCAST;	/* reply by broadcast */
	m->m_len = sizeof(*iar);
	m->m_pkthdr.len = m->m_len;
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
#ifdef __NetBSD__
	arpstat.as_sndtotal++;
	arpstat.as_sndreply++;
#endif
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);
	return;
}

/*
 * Lookup or enter a new address in ieee1394arptab.
 */
static struct llinfo_arp *
ieee1394arplookup(struct in_addr *addr, int create, int proxy)
{
	struct rtentry *rt;
	static struct sockaddr_inarp sin;
	const char *why = 0;

	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr = *addr;
	sin.sin_other = proxy ? SIN_PROXY : 0;
	rt = rtalloc1(sintosa(&sin), create);
	if (rt == 0)
		return (0);
	rt->rt_refcnt--;

	if (rt->rt_flags & RTF_GATEWAY)
		why = "host is not on local network";
	else if ((rt->rt_flags & RTF_LLINFO) == 0) {
#ifdef __NetBSD__
		arpstat.as_allocfail++;
#endif
		why = "could not allocate llinfo";
	} else if (rt->rt_gateway->sa_family != AF_LINK)
		why = "gateway route is not ours";
	else
		return ((struct llinfo_arp *)rt->rt_llinfo);

	if (create)
		log(LOG_DEBUG, "ieee1394arplookup: unable to enter address"
		    " for %s (%s)\n",
		    in_fmtaddr(*addr), why);
	return (0);
}

void
ieee1394arp_ifinit(struct ifnet *ifp, struct ifaddr *ifa)
{
	struct in_addr *ip;

	/*
	 * Warn the user if another station has this IP address,
	 * but only if the interface IP address is not zero.
	 */
	ip = &IA_SIN(ifa)->sin_addr;
	if (!in_nullhost(*ip))
		ieee1394arprequest(ifp, ip, ip);

	ifa->ifa_rtrequest = ieee1394arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}
