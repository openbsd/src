/*	$OpenBSD: if_bridge.c,v 1.4 1999/03/05 21:10:52 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bridge.h"
#if NBRIDGE > 0

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#ifdef IPFILTER
#include <netinet/ip_fil_compat.h>
#include <netinet/ip_fil.h>
#endif
#endif

#include <net/if_bridge.h>

#ifndef	BRIDGE_RTABLE_SIZE
#define	BRIDGE_RTABLE_SIZE	1024
#endif
#define	BRIDGE_RTABLE_MASK	(BRIDGE_RTABLE_SIZE - 1)

/*
 * Maximum number of addresses to cache
 */
#ifndef BRIDGE_RTABLE_MAX
#define BRIDGE_RTABLE_MAX	100
#endif

/*
 * Timeout (in seconds) for entries learned dynamically
 */
#ifndef BRIDGE_RTABLE_TIMEOUT
#define BRIDGE_RTABLE_TIMEOUT	30
#endif

extern int ifqmaxlen;

/*
 * Bridge interface list
 */
struct bridge_iflist {
	LIST_ENTRY(bridge_iflist)	next;		/* next in list */
	struct				ifnet *ifp;	/* member interface */
	u_int32_t			bif_flags;	/* member flags */
};

/*
 * Bridge route node
 */
struct bridge_rtnode {
	LIST_ENTRY(bridge_rtnode)	brt_next;	/* next in list */
	struct				ifnet *brt_if;	/* destination ifs */
	u_int32_t			brt_age;	/* age counter */
	struct				ether_addr brt_addr;	/* dst addr */
};

/*
 * Software state for each bridge
 */
struct bridge_softc {
	struct				ifnet sc_if;	/* the interface */
	u_int32_t			sc_brtmax;	/* max # addresses */
	u_int32_t			sc_brtcnt;	/* current # addrs */
	u_int32_t			sc_brttimeout;	/* current # addrs */
	LIST_HEAD(, bridge_iflist)	sc_iflist;	/* interface list */
	LIST_HEAD(bridge_rthead, bridge_rtnode)	*sc_rts;/* hash table */
};

struct bridge_softc bridgectl[NBRIDGE];

void	bridgeattach __P((int));
int	bridge_ioctl __P((struct ifnet *, u_long, caddr_t));
void	bridge_start __P((struct ifnet *));
struct mbuf *	bridge_broadcast __P((struct bridge_softc *sc,
    struct ifnet *, struct ether_header *, struct mbuf *));
void	bridge_stop __P((struct bridge_softc *));
void	bridge_init __P((struct bridge_softc *));
int	bridge_bifconf __P((struct bridge_softc *, struct ifbifconf *));

int	bridge_rtfind __P((struct bridge_softc *, struct ifbaconf *));
void	bridge_rtage __P((void *));
void	bridge_rttrim __P((struct bridge_softc *));
void	bridge_rtdelete __P((struct bridge_softc *, struct ifnet *));
struct ifnet *	bridge_rtupdate __P((struct bridge_softc *,
    struct ether_addr *, struct ifnet *ifp));
struct ifnet *	bridge_rtlookup __P((struct bridge_softc *,
    struct ether_addr *));
u_int32_t	bridge_hash __P((struct ether_addr *));

#define	ETHERADDR_IS_IP_MCAST(a) \
	/* struct etheraddr *a;	*/				\
	((a)->ether_addr_octet[0] == 0x01 &&			\
	 (a)->ether_addr_octet[1] == 0x00 &&			\
	 (a)->ether_addr_octet[2] == 0x5e)

#if defined(INET) && (defined(IPFILTER) || defined(IPFILTER_LKM))
/*
 * Filter hooks
 */
#define	BRIDGE_FILTER_PASS	0
#define	BRIDGE_FILTER_DROP	1
int	bridge_filter __P((struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *));
#endif

void
bridgeattach(unused)
	int unused;
{
	int i;
	struct ifnet *ifp;

	for (i = 0; i < NBRIDGE; i++) {
		bridgectl[i].sc_brtmax = BRIDGE_RTABLE_MAX;
		bridgectl[i].sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;
		LIST_INIT(&bridgectl[i].sc_iflist);
		ifp = &bridgectl[i].sc_if;
		sprintf(ifp->if_xname, "bridge%d", i);
		ifp->if_softc = &bridgectl[i];
		ifp->if_mtu = ETHERMTU;
		ifp->if_ioctl = bridge_ioctl;
		ifp->if_output = bridge_output;
		ifp->if_start = bridge_start;
		ifp->if_type  = IFT_PROPVIRTUAL;
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
		ifp->if_hdrlen = sizeof(struct ether_header);
		if_attach(ifp);
	}
}

int
bridge_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long	cmd;
	caddr_t	data;
{
	struct proc *prc = curproc;		/* XXX */
	struct ifnet *ifs;
	struct bridge_softc *sc = (struct bridge_softc *)ifp->if_softc;
	struct ifbreq *req = (struct ifbreq *)data;
	struct ifbaconf *baconf = (struct ifbaconf *)data;
	struct ifbcachereq *bcachereq = (struct ifbcachereq *)data;
	struct ifbifconf *bifconf = (struct ifbifconf *)data;
	struct ifbcachetoreq *bcacheto = (struct ifbcachetoreq *)data;
	int	error = 0, s;
	struct bridge_iflist *p;

	s = splimp();
	switch(cmd) {
	case SIOCBRDGADD:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;

		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}

		if (ifs->if_bridge == (caddr_t)sc) {
			error = EEXIST;
			break;
		}

		if (ifs->if_bridge != NULL) {
			error = EBUSY;
			break;
		}

		if (ifs->if_type != IFT_ETHER) {
			error = EINVAL;
			break;
		}

		error = ifpromisc(ifs, 1);
		if (error != 0)
			break;

		p = (struct bridge_iflist *) malloc(
		    sizeof(struct bridge_iflist), M_DEVBUF, M_NOWAIT);
		if (p == NULL) {			/* list alloc failed */
			error = ENOMEM;
			ifpromisc(ifs, 0);		/* decr promisc cnt */
			break;
		}

		p->ifp = ifs;
		LIST_INSERT_HEAD(&sc->sc_iflist, p, next);
		ifs->if_bridge = (caddr_t)sc;
		break;
	case SIOCBRDGDEL:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;

		p = LIST_FIRST(&sc->sc_iflist);
		while (p != NULL) {
			if (strncmp(p->ifp->if_xname, req->ifbr_ifsname,
			    sizeof(p->ifp->if_xname)) == 0) {
				p->ifp->if_bridge = NULL;

				error = ifpromisc(p->ifp, 0);

				LIST_REMOVE(p, next);
				bridge_rtdelete(sc, p->ifp);
				free(p, M_DEVBUF);
				break;
			}
			p = LIST_NEXT(p, next);
		}
		if (p == NULL) {
			error = ENOENT;
			break;
		}
		break;
	case SIOCBRDGIFS:
		error = bridge_bifconf(sc, bifconf);
		break;
	case SIOCBRDGRTS:
		if ((ifp->if_flags & IFF_RUNNING) == 0) {
			error = ENETDOWN;
			break;
		}
		error = bridge_rtfind(sc, baconf);
		break;
	case SIOCBRDGGCACHE:
		bcachereq->ifbc_size = sc->sc_brtmax;
		break;
	case SIOCBRDGSCACHE:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		sc->sc_brtmax = bcachereq->ifbc_size;
		bridge_rttrim(sc);
		break;
	case SIOCBRDGSTO:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		sc->sc_brttimeout = bcacheto->ifbct_time;
		untimeout(bridge_rtage, sc);
		if (bcacheto->ifbct_time != 0)
			timeout(bridge_rtage, sc, sc->sc_brttimeout);
		break;
	case SIOCBRDGGTO:
		bcacheto->ifbct_time = sc->sc_brttimeout;
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == IFF_UP)
			bridge_init(sc);

		if ((ifp->if_flags & IFF_UP) == 0)
			bridge_stop(sc);

		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

int
bridge_bifconf(sc, bifc)
	struct bridge_softc *sc;
	struct ifbifconf *bifc;
{
	struct bridge_iflist *p;
	u_int32_t total, i;
	int error;
	struct ifbreq breq;

	p = LIST_FIRST(&sc->sc_iflist);
	while (p != NULL) {
		total++;
		p = LIST_NEXT(p, next);
	}

	if (bifc->ifbic_len == 0) {
		bifc->ifbic_len = total * sizeof(struct ifbreq);
		return (0);
	}

	p = LIST_FIRST(&sc->sc_iflist);
	i = 0;
	while (p != NULL && bifc->ifbic_len > sizeof(breq)) {
		strncpy(breq.ifbr_name, sc->sc_if.if_xname,
		    sizeof (breq.ifbr_name));
		breq.ifbr_name[sizeof(breq.ifbr_name)-1] = '\0';
		strncpy(breq.ifbr_ifsname, p->ifp->if_xname,
		    sizeof (breq.ifbr_ifsname));
		breq.ifbr_ifsname[sizeof(breq.ifbr_ifsname)-1] = '\0';
		breq.ifbr_ifsflags = p->bif_flags;
		error = copyout((caddr_t)&breq,
		    (caddr_t)(bifc->ifbic_req + i), sizeof(breq));
		if (error) {
			bifc->ifbic_len = i * sizeof(breq);
			return (error);
		}
		p = LIST_NEXT(p, next);
		i++;
		bifc->ifbic_len -= sizeof(breq);
	}
	bifc->ifbic_len = i * sizeof(breq);
	return (0);
}

void
bridge_init(sc)
	struct bridge_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	int i;
	int s;

	if ((ifp->if_flags & IFF_RUNNING) == IFF_RUNNING)
		return;

	s = splhigh();
	if (sc->sc_rts == NULL) {
		sc->sc_rts = (struct bridge_rthead *)malloc(
		    BRIDGE_RTABLE_SIZE * (sizeof(struct bridge_rthead)),
		    M_DEVBUF, M_NOWAIT);
		if (sc->sc_rts == NULL) {
			splx(s);
			return;
		}

		for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
			LIST_INIT(&sc->sc_rts[i]);
		}
	}
	ifp->if_flags |= IFF_RUNNING;
	splx(s);

	if (sc->sc_brttimeout != 0)
		timeout(bridge_rtage, sc, sc->sc_brttimeout * hz);
}

/*
 * Stop the bridge and deallocate the routing table.
 */
void
bridge_stop(sc)
	struct bridge_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	struct bridge_rtnode *n, *p;
	int i, s;

	/*
	 * If we're not running, there's nothing to do.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	untimeout(bridge_rtage, sc);

	/*
	 * Free the routing table, if necessary.
	 */
	if (sc->sc_rts != NULL) {
		s = splhigh();
		for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
			n = LIST_FIRST(&sc->sc_rts[i]);
			while (n != NULL) {
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				free(n, M_DEVBUF);
				sc->sc_brtcnt--;
				n = p;
			}
		}
		free(sc->sc_rts, M_DEVBUF);
		sc->sc_rts = NULL;
		splx(s);
	}
	ifp->if_flags &= ~IFF_RUNNING;
}

/*
 * Send output from the bridge.  The mbuf has the ethernet header
 * already attached.  We must free the mbuf before exitting.
 */
int
bridge_output(ifp, m, sa, rt)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *sa;
	struct rtentry *rt;
{
	struct ether_header *eh;
	struct ifnet *dst_if;
	struct ether_addr *src, *dst;
	struct arpcom *ac = (struct arpcom *)ifp;
	struct bridge_softc *sc;
	struct bridge_iflist *p;
	struct mbuf *mc;
	int s;

	if (m->m_len < sizeof(*eh)) {
		m = m_pullup(m, sizeof(*eh));
		if (m == NULL)
			return (0);
	}
	eh = mtod(m, struct ether_header *);
	dst = (struct ether_addr *)&eh->ether_dhost[0];
	src = (struct ether_addr *)&eh->ether_shost[0];
	sc = (struct bridge_softc *)ifp->if_bridge;

	s = splimp();

	/*
	 * If the packet is a broadcast or we don't know a better way to
	 * get there, we must broadcast with header rewriting.
	 */
	dst_if = bridge_rtlookup(sc, dst);
	if (dst_if == NULL || eh->ether_dhost[0] & 1) {
		for (p = LIST_FIRST(&sc->sc_iflist); p != NULL;
		    p = LIST_NEXT(p, next)) {
			if ((p->ifp->if_flags & IFF_RUNNING) == 0)
				continue;

			if (IF_QFULL(&p->ifp->if_snd)) {
				sc->sc_if.if_oerrors++;
				continue;
			}

			/*
			 * Make a full copy of the packet (sigh)
			 */
			mc = m_copym2(m, 0, M_COPYALL, M_NOWAIT);
			if (mc == NULL) {
				sc->sc_if.if_oerrors++;
				continue;
			}

			/*
			 * If packet does not have a multicast or broadcast
			 * destination, rewrite the header to contain
			 * the current interface's address.
			 */
			if ((eh->ether_shost[0] & 1) == 0) {
				struct arpcom *cac = (struct arpcom *)p->ifp;
				struct ether_header *ceh;
				struct ether_addr *csrc;

				if (mc->m_len < sizeof(*ceh)) {
					mc = m_pullup(mc, sizeof(*ceh));
					if (mc == NULL)
						continue;
				}
				ceh = mtod(mc, struct ether_header *);
				csrc = (struct ether_addr *)
				    &ceh->ether_shost[0];
				bcopy(cac->ac_enaddr, csrc, ETHER_ADDR_LEN);
			}

			sc->sc_if.if_opackets++;
			sc->sc_if.if_obytes += m->m_pkthdr.len;
			IF_ENQUEUE(&p->ifp->if_snd, mc);
			if ((p->ifp->if_flags & IFF_OACTIVE) == 0)
				(*p->ifp->if_start)(p->ifp);
		}
		m_freem(m);
		splx(s);
		return (0);
	}

	bcopy(ac->ac_enaddr, src, ETHER_ADDR_LEN);
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		splx(s);
		return (0);
	}
	if (IF_QFULL(&dst_if->if_snd)) {
		sc->sc_if.if_oerrors++;
		m_freem(m);
		splx(s);
		return (0);
	}
	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += m->m_pkthdr.len;
	IF_ENQUEUE(&dst_if->if_snd, m);
	if ((dst_if->if_flags & IFF_OACTIVE) == 0)
		(*dst_if->if_start)(dst_if);
	splx(s);
	return (0);
}

/*
 * Start output on the bridge.  This function should never be called.
 */
void
bridge_start(ifp)
	struct ifnet *ifp;
{
}

/*
 * Receive input from an interface.  Rebroadcast if necessary to other
 * bridge members.
 */
struct mbuf *
bridge_input(ifp, eh, m)
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
{
	struct bridge_softc *sc;
	struct arpcom *ac;
	struct ether_addr *dst, *src;
	struct ifnet *dst_if;
	int s;

	/*
	 * Make sure this interface is a bridge member.
	 */
	if (ifp == NULL || ifp->if_bridge == NULL || m == NULL)
		return (m);

	sc = (struct bridge_softc *)ifp->if_bridge;

	s = splimp();

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		splx(s);
		return (m);
	}

	sc->sc_if.if_lastchange = time;
	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	/*
	 * See if the destination of this frame matches the interface
	 * it came in on.  If so, we don't need to do anything.
	 */
	ac = (struct arpcom *)ifp;
	if (bcmp(ac->ac_enaddr, eh->ether_dhost, ETHER_ADDR_LEN) == 0) {
		splx(s);
		return (m);
	}

	dst = (struct ether_addr *)&eh->ether_dhost[0];
	src = (struct ether_addr *)&eh->ether_shost[0];

	/*
	 * If source address is not broadcast or multicast, record
	 * it's address.
	 */
	if ((eh->ether_shost[0] & 1) == 0 &&
	    !(eh->ether_shost[0] == 0 && eh->ether_shost[1] == 0 &&
	      eh->ether_shost[2] == 0 && eh->ether_shost[3] == 0 &&
	      eh->ether_shost[4] == 0 && eh->ether_shost[5] == 0))
		bridge_rtupdate(sc, src, ifp);

	/*
	 * If packet is unicast, destined for someone on "this"
	 * side of the bridge, drop it.
	 */
	dst_if = bridge_rtlookup(sc, dst);
	if ((m->m_flags & (M_BCAST|M_MCAST)) == 0 && dst_if == ifp) {
		m_freem(m);
		splx(s);
		return (NULL);
	}

	/*
	 * Multicast packets get handled a little differently:
	 * If interface is:
	 *	-link0,-link1	(default) Forward all multicast as broadcast.
	 *	-link0,link1	Drop non-IP multicast, forward as broadcast
	 *				IP multicast.
	 *	link0,-link1	Drop IP multicast, forward as broadcast
	 *				non-IP multicast.
	 *	link0,link1	Drop all multicast.
	 */
	if (m->m_flags & M_MCAST) {
		if ((sc->sc_if.if_flags & (IFF_LINK0|IFF_LINK1)) ==
		    (IFF_LINK0|IFF_LINK1)) {
			splx(s);
			return (m);
		}
		if (sc->sc_if.if_flags & IFF_LINK0 &&
		    ETHERADDR_IS_IP_MCAST(dst)) {
			splx(s);
			return (m);
		}
		if (sc->sc_if.if_flags & IFF_LINK1 &&
		    !ETHERADDR_IS_IP_MCAST(dst)) {
			splx(s);
			return (m);
		}
	}

#if defined(INET) && (defined(IPFILTER) || defined(IPFILTER_LKM))
	/*
	 * Pass the packet to the ip filtering code and drop
	 * here if necessary.
	 */
	if (bridge_filter(sc, ifp, eh, m) == BRIDGE_FILTER_DROP) {
		if (m->m_flags & (M_BCAST|M_MCAST)) {
			/*
			 * Broadcasts should be passed down if filtered
			 * by the bridge, so that they can be filtered
			 * by the interface itself.
			 */
			 splx(s);
			 return (m);
		}
		m_freem(m);
		splx(s);
		return (NULL);
	}
#endif

	/*
	 * If the packet is a multicast or broadcast, then forward it
	 * and pass it up to our higher layers.
	 */
	if (m->m_flags & (M_BCAST|M_MCAST)) {
		ifp->if_imcasts++;
		m = bridge_broadcast(sc, ifp, eh, m);
		splx(s);
		return (m);
	}

	/*
	 * If sucessful lookup, forward packet to that interface only.
	 */
	if (dst_if != NULL) {
		if ((dst_if->if_flags & IFF_RUNNING) == 0) {
			m_freem(m);
			splx(s);
			return (NULL);
		}

		if (IF_QFULL(&dst_if->if_snd)) {
			sc->sc_if.if_oerrors++;
			m_freem(m);
			splx(s);
			return (NULL);
		}

		M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
		if (m == NULL) {
			sc->sc_if.if_oerrors++;
			return (NULL);
		}
		*mtod(m, struct ether_header *) = *eh;

		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += m->m_pkthdr.len;

		IF_ENQUEUE(&dst_if->if_snd, m);
       		if ((dst_if->if_flags & IFF_OACTIVE) == 0)
			(*dst_if->if_start)(dst_if);

		splx(s);
		return (NULL);
	}

	/*
	 * Packet must be forwarded to all interfaces.
	 */
	m = bridge_broadcast(sc, ifp, eh, m);
	splx(s);
	return (m);
}

/*
 * Send a frame to all interfaces that are members of the bridge
 * (except the one it came in on).  This code assumes that it is
 * running at splnet or higher.
 */
struct mbuf *
bridge_broadcast(sc, ifp, eh, m)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
{
	struct bridge_iflist *p;
	struct mbuf *mc;

	/*
	 * Tack on ethernet header
	 */
	M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
	if (m == NULL)
		return (NULL);
	*mtod(m, struct ether_header *) = *eh;

	for (p = LIST_FIRST(&sc->sc_iflist); p; p = LIST_NEXT(p, next)) {
		/*
		 * Don't retransmit out of the same interface where
		 * the packet was received from.
		 */
		if (p->ifp->if_index == ifp->if_index)
			continue;

		if ((p->ifp->if_flags & IFF_RUNNING) == 0)
			continue;

		if (IF_QFULL(&p->ifp->if_snd)) {
			sc->sc_if.if_oerrors++;
			continue;
		}

		mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
		if (mc == NULL) {
			sc->sc_if.if_oerrors++;
			continue;
		}

		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += m->m_pkthdr.len;
		if ((eh->ether_shost[0] & 1) == 0)
			ifp->if_omcasts++;

		IF_ENQUEUE(&p->ifp->if_snd, mc);
		if ((p->ifp->if_flags & IFF_OACTIVE) == 0)
			(*p->ifp->if_start)(p->ifp);
	}

	m_adj(m, sizeof(struct ether_header));
	return (m);
}

struct ifnet *
bridge_rtupdate(sc, ea, ifp)
	struct bridge_softc *sc;
	struct ether_addr *ea;
	struct ifnet *ifp;
{
	struct bridge_rtnode *p, *q;
	u_int32_t h;
	int s, dir;

	s = splhigh();
	if (sc->sc_rts == NULL) {
		splx(s);
		return (NULL);
	}

	h = bridge_hash(ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	if (p == NULL) {
		if (sc->sc_brtcnt >= sc->sc_brtmax) {
			splx(s);
			return (NULL);
		}
		p = (struct bridge_rtnode *)
				malloc(sizeof(struct bridge_rtnode),
				    M_DEVBUF, M_NOWAIT);
		if (p == NULL) {
			splx(s);
			return (NULL);
		}

		bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
		p->brt_if = ifp;
		p->brt_age = 1;
		LIST_INSERT_HEAD(&sc->sc_rts[h], p, brt_next);
		sc->sc_brtcnt++;
		splx(s);
		return (ifp);
	}

	do {
		q = p;
		p = LIST_NEXT(p, brt_next);

		dir = bcmp(ea, &q->brt_addr, sizeof(q->brt_addr));
		if (dir == 0) {
			q->brt_if = ifp;
			q->brt_age = 1;
			splx(s);
			return (ifp);
		}

		if (dir > 0) {
			if (sc->sc_brtcnt >= sc->sc_brtmax) {
				splx(s);
				return (NULL);
			}
			p = (struct bridge_rtnode *)
					malloc(sizeof(struct bridge_rtnode),
					    M_DEVBUF, M_NOWAIT);
			if (p == NULL) {
				splx(s);
				return (NULL);
			}

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;
			LIST_INSERT_BEFORE(q, p, brt_next);
			sc->sc_brtcnt++;
			splx(s);
			return (ifp);
		}

		if (p == NULL) {
			if (sc->sc_brtcnt >= sc->sc_brtmax) {
				splx(s);
				return (NULL);
			}
			p = (struct bridge_rtnode *)
					malloc(sizeof(struct bridge_rtnode),
					    M_DEVBUF, M_NOWAIT);
			if (p == NULL) {
				splx(s);
				return (NULL);
			}

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;
			LIST_INSERT_AFTER(q, p, brt_next);
			sc->sc_brtcnt++;
			splx(s);
			return (ifp);
		}
	} while (p != NULL);

	splx(s);
	return (NULL);
}

struct ifnet *
bridge_rtlookup(sc, ea)
	struct bridge_softc *sc;
	struct ether_addr *ea;
{
	struct bridge_rtnode *p;
	u_int32_t h;
	int s, dir;

	/*
	 * Lock out everything else
	 */
	s = splhigh();

	if (sc->sc_rts == NULL) {
		splx(s);
		return (NULL);
	}

	h = bridge_hash(ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	while (p != NULL) {
		dir = bcmp(ea, &p->brt_addr, sizeof(p->brt_addr));
		if (dir == 0) {
			splx(s);
			return (p->brt_if);
		}

		if (dir > 0) {
			splx(s);
			return (NULL);
		}

		p = LIST_NEXT(p, brt_next);
	}
	splx(s);
	return (NULL);
}

/*
 * The following hash function is adapted from 'Hash Functions' by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 * "You may use this code any way you wish, private, educational, or
 *  commercial.  It's free."
 */
#define	mix(a,b,c) \
	do {						\
		a -= b; a -= c; a ^= (c >> 13);		\
		b -= c; b -= a; b ^= (a << 8);		\
		c -= a; c -= b; c ^= (b >> 13);		\
		a -= b; a -= c; a ^= (c >> 12);		\
		b -= c; b -= a; b ^= (a << 16);		\
		c -= a; c -= b; c ^= (b >> 5);		\
		a -= b; a -= c; a ^= (c >> 3);		\
		b -= c; b -= a; b ^= (a << 10);		\
		c -= a; c -= b; c ^= (b >> 15);		\
	} while(0)

u_int32_t
bridge_hash(addr)
	struct ether_addr *addr;
{
	u_int32_t a = 0x9e3779b9, b = 0x9e3779b9, c = 0xdeadbeef;

	b += addr->ether_addr_octet[5] << 8;
	b += addr->ether_addr_octet[4];
	a += addr->ether_addr_octet[3] << 24;
	a += addr->ether_addr_octet[2] << 16;
	a += addr->ether_addr_octet[1] << 8;
	a += addr->ether_addr_octet[0];

	mix(a, b, c);
	return (c & BRIDGE_RTABLE_MASK);
}

/*
 * Trim the routing table so that we've got a number of routes
 * less than or equal to the maximum.
 */
void
bridge_rttrim(sc)
	struct bridge_softc *sc;
{
	struct bridge_rtnode *n, *p;
	int s, i;

	s = splhigh();
	if (sc->sc_rts == NULL) {
		splx(s);
		return;
	}

	/*
	 * Make sure we have to trim the address table
	 */
	if (sc->sc_brtcnt <= sc->sc_brtmax) {
		splx(s);
		return;
	}

	/*
	 * Force an aging cycle, this might trim enough addresses.
	 */
	splx(s);
	bridge_rtage(sc);
	s = splhigh();

	if (sc->sc_brtcnt <= sc->sc_brtmax) {
		splx(s);
		return;
	}

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			p = LIST_NEXT(n, brt_next);
			LIST_REMOVE(n, brt_next);
			sc->sc_brtcnt--;
			free(n, M_DEVBUF);
			n = p;
			if (sc->sc_brtcnt <= sc->sc_brtmax) {
				splx(s);
				return;
			}
		}
	}
	splx(s);
}

/*
 * Perform an aging cycle
 */
void
bridge_rtage(vsc)
	void *vsc;
{
	struct bridge_softc *sc = (struct bridge_softc *)vsc;
	struct bridge_rtnode *n, *p;
	int s, i;

	s = splhigh();
	if (sc->sc_rts == NULL) {
		splx(s);
		return;
	}

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (n->brt_age) {
				n->brt_age = 0;
				n = LIST_NEXT(n, brt_next);
			}
			else {
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF);
				n = p;
			}
		}
	}
	splx(s);

	if (sc->sc_brttimeout != 0)
		timeout(bridge_rtage, sc, sc->sc_brttimeout * hz);
}

/*
 * Delete routes to a specific interface member.
 */
void
bridge_rtdelete(sc, ifp)
	struct bridge_softc *sc;
	struct ifnet *ifp;
{
	int i, s;
	struct bridge_rtnode *n, *p;

	s = splhigh();
	if (sc->sc_rts == NULL) {
		splx(s);
		return;
	}

	/*
	 * Loop through all of the hash buckets and traverse each
	 * chain looking for routes to this interface.
	 */
	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (n->brt_if == ifp) {		/* found one */
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF);
				n = p;
			}
			else
				n = LIST_NEXT(n, brt_next);
		}
	}

	splx(s);
}

/*
 * Gather all of the routes for this interface.
 */
int
bridge_rtfind(sc, baconf)
	struct bridge_softc *sc;
	struct ifbaconf *baconf;
{
	int i, s, error;
	u_int32_t cnt;
	struct bridge_rtnode *n;
	struct ifbareq bareq;

	s = splhigh();

	if (sc->sc_rts == NULL) {
		baconf->ifbac_len = 0;
		splx(s);
		return (0);
	}

	if (baconf->ifbac_len == 0) {
		baconf->ifbac_len = sc->sc_brtcnt * sizeof(struct ifbareq);
		splx(s);
		return (0);
	}

	for (i = 0, cnt = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (baconf->ifbac_len < sizeof(struct ifbareq)) {
				baconf->ifbac_len = cnt *
				    sizeof(struct ifbareq);
				splx(s);
				return (0);
			}
			bcopy(n->brt_if->if_xname, bareq.ifba_name,
			    sizeof(bareq.ifba_name));
			bcopy(&n->brt_addr, &bareq.ifba_dst,
			    sizeof(bareq.ifba_dst));
			bareq.ifba_age = n->brt_age;
			error = copyout((caddr_t)&bareq,
		    	    (caddr_t)(baconf->ifbac_req + cnt), sizeof(bareq));
			if (error) {
				baconf->ifbac_len = cnt *
				    sizeof(struct ifbareq);
				splx(s);
				return (error);
			}
			n = LIST_NEXT(n, brt_next);
			cnt++;
		}
	}

	baconf->ifbac_len = cnt * sizeof(struct ifbareq);
	splx(s);
	return (0);
}

#if defined(INET) && (defined(IPFILTER) || defined(IPFILTER_LKM))
/*
 * Filter IP packets by peeking into the ethernet frame.  This violates
 * the ISO model, but allows us to act as a IP filter at the data link
 * layer.  As a result, most of this code will look familiar to those
 * who've read net/if_ethersubr.c and netinet/ip_input.c
 */
int
bridge_filter(sc, ifp, eh, n)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *n;
{
	struct mbuf *m, *m0;
	struct ip *ip;
	u_int16_t etype;
	int hlen, r;

	if (fr_checkp == NULL)
		return (BRIDGE_FILTER_PASS);

	/*
	 * XXX TODO: Handle LSAP packets
	 */
	etype = ntohs(eh->ether_type);
	if (etype != ETHERTYPE_IP)
		return (BRIDGE_FILTER_PASS);

	/*
	 * We need a full copy because we're going to be destructive
	 * to the packet before we pass it to the ip filter code.
	 * XXX This needs to be turned into a munge -> check ->
	 * XXX unmunge section, for now, we copy.
	 */
	m = m_copym2(n, 0, M_COPYALL, M_NOWAIT);
	if (m == NULL)
		return (BRIDGE_FILTER_DROP);

	m->m_pkthdr.rcvif = &sc->sc_if;

	/*
	 * Pull up the IP header
	 */
	if (m->m_len < sizeof(struct ip)) {
		m = m_pullup(m, sizeof(struct ip));
		if (m == NULL)
			return (BRIDGE_FILTER_DROP);
	}

	/*
	 * Examine the ip header, and drop invalid packets
	 */
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION) {
		r = BRIDGE_FILTER_DROP;
		goto out;
	}
	hlen = ip->ip_hl << 2;		/* get whole header length */
	if (hlen < sizeof(struct ip)) {
		r = BRIDGE_FILTER_DROP;
		goto out;
	}

	if (hlen > m->m_len) {		/* pull up whole header */
		if ((m = m_pullup(m, hlen)) == 0) {
			r = BRIDGE_FILTER_DROP;
			goto out;
		}
		ip = mtod(m, struct ip *);
	}
	if ((ip->ip_sum = in_cksum(m, hlen)) != 0) {
		r = BRIDGE_FILTER_DROP;
		goto out;
	}

	NTOHS(ip->ip_len);
	if (ip->ip_len < hlen) {
		r = BRIDGE_FILTER_DROP;
		goto out;
	}
	NTOHS(ip->ip_id);
	NTOHS(ip->ip_off);

	if (m->m_pkthdr.len < ip->ip_len) {
		r = BRIDGE_FILTER_DROP;
		goto out;
	}
	if (m->m_pkthdr.len > ip->ip_len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip->ip_len;
			m->m_pkthdr.len = ip->ip_len;
		}
		else
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
	}

	/*
	 * Finally, we get to filter the packet!
	 */
	m0 = m;
	if (fr_checkp && (*fr_checkp)(ip, hlen, m->m_pkthdr.rcvif, 0, &m0))
		return (BRIDGE_FILTER_DROP);
	ip = mtod(m = m0, struct ip *);
	r = BRIDGE_FILTER_PASS;

out:
	m_freem(m);
	return (r);
}
#endif

#endif  /* NBRIDGE */
