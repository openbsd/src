/*	$OpenBSD: if_bridge.c,v 1.22 2000/01/10 22:18:29 angelos Exp $	*/

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
#include "bpfilter.h"

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
#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_llc.h>
#include <net/route.h>
#include <net/netisr.h>

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

#if NBPFILTER > 0
#include <net/bpf.h>
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
#define BRIDGE_RTABLE_TIMEOUT	240
#endif

/*
 * This really should be defined in if_llc.h but in case it isn't.
 */
#ifndef llc_snap
#define llc_snap        llc_un.type_snap
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
	u_int8_t			brt_flags;	/* address flags */
	u_int8_t			brt_age;	/* age counter */
	struct				ether_addr brt_addr;	/* dst addr */
};

/*
 * Software state for each bridge
 */
struct bridge_softc {
	struct				ifnet sc_if;	/* the interface */
	u_int32_t			sc_brtmax;	/* max # addresses */
	u_int32_t			sc_brtcnt;	/* current # addrs */
	u_int32_t			sc_brttimeout;	/* timeout ticks */
	LIST_HEAD(, bridge_iflist)	sc_iflist;	/* interface list */
	LIST_HEAD(bridge_rthead, bridge_rtnode)	*sc_rts;/* hash table */
};

struct bridge_softc bridgectl[NBRIDGE];

void	bridgeattach __P((int));
int	bridge_ioctl __P((struct ifnet *, u_long, caddr_t));
void	bridge_start __P((struct ifnet *));
void	bridge_broadcast __P((struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *));
void	bridge_stop __P((struct bridge_softc *));
void	bridge_init __P((struct bridge_softc *));
int	bridge_bifconf __P((struct bridge_softc *, struct ifbifconf *));

int	bridge_rtfind __P((struct bridge_softc *, struct ifbaconf *));
void	bridge_rtage __P((void *));
void	bridge_rttrim __P((struct bridge_softc *));
void	bridge_rtdelete __P((struct bridge_softc *, struct ifnet *));
int	bridge_rtdaddr __P((struct bridge_softc *, struct ether_addr *));
int	bridge_rtflush __P((struct bridge_softc *, int));
struct ifnet *	bridge_rtupdate __P((struct bridge_softc *,
    struct ether_addr *, struct ifnet *ifp, int, u_int8_t));
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
    struct ether_header *, struct mbuf **));
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
		ifp->if_type = IFT_PROPVIRTUAL;
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
		ifp->if_hdrlen = sizeof(struct ether_header);
		if_attach(ifp);
#if NBPFILTER > 0
		bpfattach(&bridgectl[i].sc_if.if_bpf, ifp,
		    DLT_EN10MB, sizeof(struct ether_header));
#endif
	}
}

int
bridge_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t	data;
{
	struct proc *prc = curproc;		/* XXX */
	struct ifnet *ifs;
	struct bridge_softc *sc = (struct bridge_softc *)ifp->if_softc;
	struct ifbreq *req = (struct ifbreq *)data;
	struct ifbaconf *baconf = (struct ifbaconf *)data;
	struct ifbareq *bareq = (struct ifbareq *)data;
	struct ifbcachereq *bcachereq = (struct ifbcachereq *)data;
	struct ifbifconf *bifconf = (struct ifbifconf *)data;
	struct ifbcachetoreq *bcacheto = (struct ifbcachetoreq *)data;
	struct ifreq ifreq;
	int error = 0, s;
	struct bridge_iflist *p;

	s = splimp();
	switch (cmd) {
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

		if (ifs->if_type == IFT_ETHER) {
			if ((ifs->if_flags & IFF_UP) == 0) {
				/*
				 * Bring interface up long enough to set
				 * promiscuous flag, then shut it down again.
				 */
				strncpy(ifreq.ifr_name, req->ifbr_ifsname,
				    sizeof(ifreq.ifr_name) - 1);
				ifreq.ifr_name[sizeof(ifreq.ifr_name) - 1] = '\0';
				ifs->if_flags |= IFF_UP;
				ifreq.ifr_flags = ifs->if_flags;
				error = (*ifs->if_ioctl)(ifs, SIOCSIFFLAGS,
				    (caddr_t)&ifreq);
				if (error != 0)
					break;

				error = ifpromisc(ifs, 1);
				if (error != 0)
					break;

				strncpy(ifreq.ifr_name, req->ifbr_ifsname,
				    sizeof(ifreq.ifr_name) - 1);
				ifreq.ifr_name[sizeof(ifreq.ifr_name) - 1] = '\0';
				ifs->if_flags &= ~IFF_UP;
				ifreq.ifr_flags = ifs->if_flags;
				error = (*ifs->if_ioctl)(ifs, SIOCSIFFLAGS,
				    (caddr_t)&ifreq);
				if (error != 0) {
					ifpromisc(ifs, 0);
					break;
				}
			} else {
				error = ifpromisc(ifs, 1);
				if (error != 0)
					break;
			}
		}
		else if (ifs->if_type != IFT_ENC) {
			error = EINVAL;
			break;
		}

		p = (struct bridge_iflist *) malloc(
		    sizeof(struct bridge_iflist), M_DEVBUF, M_NOWAIT);
		if (p == NULL && ifs->if_type == IFT_ETHER) {
			error = ENOMEM;
			ifpromisc(ifs, 0);
			break;
		}

		p->ifp = ifs;
		p->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
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
	case SIOCBRDGGIFFLGS:
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		if ((caddr_t)sc != ifs->if_bridge) {
			error = ESRCH;
			break;
		}
		p = LIST_FIRST(&sc->sc_iflist);
		while (p != NULL && p->ifp != ifs) {
			p = LIST_NEXT(p, next);
		}
		if (p == NULL) {
			error = ESRCH;
			break;
		}
		req->ifbr_ifsflags = p->bif_flags;
		break;
	case SIOCBRDGSIFFLGS:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		if ((caddr_t)sc != ifs->if_bridge) {
			error = ESRCH;
			break;
		}
		p = LIST_FIRST(&sc->sc_iflist);
		while (p != NULL && p->ifp != ifs) {
			p = LIST_NEXT(p, next);
		}
		if (p == NULL) {
			error = ESRCH;
			break;
		}
		p->bif_flags = req->ifbr_ifsflags;
		break;
	case SIOCBRDGRTS:
		error = bridge_rtfind(sc, baconf);
		break;
	case SIOCBRDGFLUSH:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;

		error = bridge_rtflush(sc, req->ifbr_ifsflags);
		break;
	case SIOCBRDGSADDR:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;

		ifs = ifunit(bareq->ifba_ifsname);
		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}

		if (ifs->if_bridge == NULL ||
		    ifs->if_bridge != (caddr_t)sc) {
			error = ESRCH;
			break;
		}

		ifs = bridge_rtupdate(sc, &bareq->ifba_dst, ifs, 1,
		    bareq->ifba_flags);
		if (ifs == NULL)
			error = ENOMEM;
		break;
	case SIOCBRDGDADDR:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		error = bridge_rtdaddr(sc, &bareq->ifba_dst);
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

/* Detach an interface from a bridge.  */
void
bridge_ifdetach(ifp)
	struct ifnet *ifp;
{
	struct bridge_softc *bsc = (struct bridge_softc *)ifp->if_bridge;
	struct bridge_iflist *bif;

	for (bif = LIST_FIRST(&bsc->sc_iflist); bif;
	    bif = LIST_NEXT(bif, next))
		if (bif->ifp == ifp) {
			LIST_REMOVE(bif, next);
			bridge_rtdelete(bsc, ifp);
			free(bif, M_DEVBUF);
			break;
		}
}

int
bridge_bifconf(sc, bifc)
	struct bridge_softc *sc;
	struct ifbifconf *bifc;
{
	struct bridge_iflist *p;
	u_int32_t total = 0, i;
	int error = 0;
	struct ifbreq breq;

	p = LIST_FIRST(&sc->sc_iflist);
	while (p != NULL) {
		total++;
		p = LIST_NEXT(p, next);
	}

	if (bifc->ifbic_len == 0) {
		i = total;
		goto done;
	}

	p = LIST_FIRST(&sc->sc_iflist);
	i = 0;
	while (p != NULL && bifc->ifbic_len > i * sizeof(breq)) {
		strncpy(breq.ifbr_name, sc->sc_if.if_xname,
		    sizeof(breq.ifbr_name)-1);
		breq.ifbr_name[sizeof(breq.ifbr_name) - 1] = '\0';
		strncpy(breq.ifbr_ifsname, p->ifp->if_xname,
		    sizeof(breq.ifbr_ifsname)-1);
		breq.ifbr_ifsname[sizeof(breq.ifbr_ifsname) - 1] = '\0';
		breq.ifbr_ifsflags = p->bif_flags;
		error = copyout((caddr_t)&breq,
		    (caddr_t)(bifc->ifbic_req + i), sizeof(breq));
		if (error)
			goto done;
		p = LIST_NEXT(p, next);
		i++;
		bifc->ifbic_len -= sizeof(breq);
	}
done:
	bifc->ifbic_len = i * sizeof(breq);
	return (error);
}

void
bridge_init(sc)
	struct bridge_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	int i, s;

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

	/*
	 * If we're not running, there's nothing to do.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	untimeout(bridge_rtage, sc);

	bridge_rtflush(sc, IFBF_FLUSHDYN);

	ifp->if_flags &= ~IFF_RUNNING;
}

/*
 * Send output from the bridge.  The mbuf has the ethernet header
 * already attached.  We must enqueue or free the mbuf before exiting.
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
	 * If bridge is down, but original output interface is up,
	 * go ahead and send out that interface.  Otherwise the packet
	 * is dropped below.
	 */
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		dst_if = ifp;
		goto sendunicast;
	}

	/*
	 * If the packet is a broadcast or we don't know a better way to
	 * get there, send to all interfaces.
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

			mc = m_copym(m, 0, M_COPYALL, M_NOWAIT);
			if (mc == NULL) {
				sc->sc_if.if_oerrors++;
				continue;
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

sendunicast:
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
 * Loop through each bridge interface and process their input queues.
 */
void
bridgeintr(void)
{
	int i, s;
	struct bridge_softc *sc;
	struct ifnet *bifp, *src_if, *dst_if;
	struct bridge_iflist *ifl;
	struct ether_addr *dst, *src;
	struct ether_header *eh;
	struct mbuf *m;

	for (i = 0; i < NBRIDGE; i++) {
		sc = &bridgectl[i];
		bifp = &sc->sc_if;
		for (;;) {
			s = splimp();
			IF_DEQUEUE(&bifp->if_snd, m);
			splx(s);
			if (m == NULL)
				break;

			src_if = m->m_pkthdr.rcvif;
			if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
				m_freem(m);
				continue;
			}

#if NBPFILTER > 0
			if (sc->sc_if.if_bpf)
				bpf_mtap(sc->sc_if.if_bpf, m);
#endif

			sc->sc_if.if_lastchange = time;
			sc->sc_if.if_ipackets++;
			sc->sc_if.if_ibytes += m->m_pkthdr.len;

			ifl = LIST_FIRST(&sc->sc_iflist);
			while (ifl != NULL && ifl->ifp != src_if) {
				ifl = LIST_NEXT(ifl, next);
			}
			if (ifl == NULL) {
				m_freem(m);
				continue;
			}

			if (m->m_len < sizeof(*eh)) {
				m = m_pullup(m, sizeof(*eh));
				if (m == NULL)
					continue;
			}
			eh = mtod(m, struct ether_header *);
			dst = (struct ether_addr *)&eh->ether_dhost[0];
			src = (struct ether_addr *)&eh->ether_shost[0];

			/*
			 * If interface is learning, and if source address
			 * is not broadcast or multicast, record it's address.
	 		 */
			if ((ifl->bif_flags & IFBIF_LEARNING) &&
			    (eh->ether_shost[0] & 1) == 0 &&
			    !(eh->ether_shost[0] == 0 &&
			      eh->ether_shost[1] == 0 &&
			      eh->ether_shost[2] == 0 &&
			      eh->ether_shost[3] == 0 &&
			      eh->ether_shost[4] == 0 &&
			      eh->ether_shost[5] == 0))
				bridge_rtupdate(sc, src, src_if, 0, IFBAF_DYNAMIC);

			/*
			 * If packet is unicast, destined for someone on "this"
			 * side of the bridge, drop it.
			 */
			dst_if = bridge_rtlookup(sc, dst);
			if ((m->m_flags & (M_BCAST | M_MCAST)) == 0 &&
			    dst_if == src_if) {
				m_freem(m);
				continue;
			}

			/*
			 * Multicast packets get handled a little differently:
			 * If interface is:
			 *	-link0,-link1	(default) Forward all multicast
			 *			as broadcast.
			 *	-link0,link1	Drop non-IP multicast, forward
			 *			as broadcast IP multicast.
			 *	link0,-link1	Drop IP multicast, forward as
			 *			broadcast non-IP multicast.
			 *	link0,link1	Drop all multicast.
			 */
			if (m->m_flags & M_MCAST) {
				if ((sc->sc_if.if_flags &
				    (IFF_LINK0 | IFF_LINK1)) ==
				    (IFF_LINK0 | IFF_LINK1)) {
					m_freem(m);
					continue;
				}
				if (sc->sc_if.if_flags & IFF_LINK0 &&
				    ETHERADDR_IS_IP_MCAST(dst)) {
					m_freem(m);
					continue;
				}
				if (sc->sc_if.if_flags & IFF_LINK1 &&
				    !ETHERADDR_IS_IP_MCAST(dst)) {
					m_freem(m);
					continue;
				}
			}

			if ((ifl->bif_flags & IFBIF_BLOCKNONIP) &&
			    (eh->ether_type != htons(ETHERTYPE_IP)) &&
			    (eh->ether_type != htons(ETHERTYPE_IPV6)) &&
			    (eh->ether_type != htons(ETHERTYPE_ARP)) &&
			    (eh->ether_type != htons(ETHERTYPE_REVARP))) {
				m_freem(m);
				continue;
			}

#if defined(INET) && (defined(IPFILTER) || defined(IPFILTER_LKM))
			if (bridge_filter(sc, src_if, eh, &m) ==
			    BRIDGE_FILTER_DROP) {
				if (m != NULL)
					m_freem(m);
				continue;
			}
#endif

			/*
			 * If the packet is a multicast or broadcast, then
			 * forward it to all interfaces.
			 */
			if (m->m_flags & (M_BCAST | M_MCAST)) {
				bifp->if_imcasts++;
				bridge_broadcast(sc, src_if, eh, m);
				continue;
			}

			if (dst_if != NULL) {
				if ((dst_if->if_flags & IFF_RUNNING) == 0) {
					m_freem(m);
					continue;
				}
				s = splimp();
				if (IF_QFULL(&dst_if->if_snd)) {
					sc->sc_if.if_oerrors++;
					m_freem(m);
					splx(s);
					continue;
				}
				sc->sc_if.if_opackets++;
				sc->sc_if.if_obytes += m->m_pkthdr.len;
				IF_ENQUEUE(&dst_if->if_snd, m);
				if ((dst_if->if_flags & IFF_OACTIVE) == 0)
					(*dst_if->if_start)(dst_if);
				splx(s);
				continue;
			}

			bridge_broadcast(sc, src_if, eh, m);
			dst_if = NULL;
		}
	}
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
	int s;
	struct bridge_iflist *ifl;
	struct arpcom *ac;
	struct ether_header *neh;
	struct mbuf *mc;

	/*
	 * Make sure this interface is a bridge member.
	 */
	if (ifp == NULL || ifp->if_bridge == NULL || m == NULL)
		return (m);

	sc = (struct bridge_softc *)ifp->if_bridge;
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return (m);

	if (m->m_flags & (M_BCAST | M_MCAST)) {
		/*
		 * make a copy of 'm' with 'eh' tacked on to the
		 * beginning.  Return 'm' for local processing
		 * and enqueue the copy.  Schedule netisr.
		 */
		mc = m_copym2(m, 0, M_COPYALL, M_NOWAIT);
		if (mc == NULL)
			return (m);
		M_PREPEND(mc, sizeof(*eh), M_DONTWAIT);
		if (mc == NULL)
			return (m);
		neh = mtod(mc, struct ether_header *);
		bcopy(eh, neh, sizeof(struct ether_header));
		s = splimp();
		if (IF_QFULL(&sc->sc_if.if_snd)) {
			m_freem(mc);
			splx(s);
			return (m);
		}
		IF_ENQUEUE(&sc->sc_if.if_snd, mc);
		splx(s);
		schednetisr(NETISR_BRIDGE);
		return (m);
	}

	/*
	 * Unicast, make sure it's not for us.
	 */
	ifl = LIST_FIRST(&sc->sc_iflist);
	while (ifl != NULL) {
		if (ifl->ifp->if_type == IFT_ETHER) {
			ac = (struct arpcom *)ifl->ifp;
			if (bcmp(ac->ac_enaddr, eh->ether_dhost,
			    ETHER_ADDR_LEN) == 0) {
				bridge_rtupdate(sc,
				    (struct ether_addr *)&eh->ether_dhost[0],
				    ifp, 0, IFBAF_DYNAMIC);
				return (m);
			}
			if (bcmp(ac->ac_enaddr, eh->ether_shost,
			    ETHER_ADDR_LEN) == 0) {
				m_freem(m);
				return (NULL);
			}
		}
		ifl = LIST_NEXT(ifl, next);
	}
	M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
	if (m == NULL)
		return (NULL);
	neh = mtod(m, struct ether_header *);
	bcopy(eh, neh, sizeof(struct ether_header));
	s = splimp();
	if (IF_QFULL(&sc->sc_if.if_snd)) {
		m_freem(m);
		splx(s);
		return (NULL);
	}
	IF_ENQUEUE(&sc->sc_if.if_snd, m);
	splx(s);
	schednetisr(NETISR_BRIDGE);
	return (NULL);
}

/*
 * Send a frame to all interfaces that are members of the bridge
 * (except the one it came in on).  This code assumes that it is
 * running at splnet or higher.
 */
void
bridge_broadcast(sc, ifp, eh, m)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
{
	struct bridge_iflist *p;
	struct mbuf *mc;
	int used = 0;

	for (p = LIST_FIRST(&sc->sc_iflist); p; p = LIST_NEXT(p, next)) {
		/*
		 * Don't retransmit out of the same interface where
		 * the packet was received from.
		 */
		if (p->ifp->if_index == ifp->if_index)
			continue;

		if ((p->bif_flags & IFBIF_DISCOVER) == 0 &&
		    (m->m_flags & (M_BCAST | M_MCAST)) == 0)
			continue;

		if ((p->bif_flags & IFBIF_BLOCKNONIP) &&
		    (eh->ether_type != htons(ETHERTYPE_IP)) &&
		    (eh->ether_type != htons(ETHERTYPE_IPV6)) &&
		    (eh->ether_type != htons(ETHERTYPE_ARP)) &&
		    (eh->ether_type != htons(ETHERTYPE_REVARP)))
			continue;

		if ((p->ifp->if_flags & IFF_RUNNING) == 0)
			continue;

		if (IF_QFULL(&p->ifp->if_snd)) {
			sc->sc_if.if_oerrors++;
			continue;
		}

		/* If last one, reuse the passed-in mbuf */
		if (LIST_NEXT(p, next) == NULL) {
			mc = m;
			used = 1;
		}
		else {
			mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if (mc == NULL) {
				sc->sc_if.if_oerrors++;
				continue;
			}
		}

		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += m->m_pkthdr.len;
		if ((eh->ether_shost[0] & 1) == 0)
			ifp->if_omcasts++;

		IF_ENQUEUE(&p->ifp->if_snd, mc);
		if ((p->ifp->if_flags & IFF_OACTIVE) == 0)
			(*p->ifp->if_start)(p->ifp);
	}

	if (!used)
		m_freem(m);
}

struct ifnet *
bridge_rtupdate(sc, ea, ifp, setflags, flags)
	struct bridge_softc *sc;
	struct ether_addr *ea;
	struct ifnet *ifp;
	int setflags;
	u_int8_t flags;
{
	struct bridge_rtnode *p, *q;
	u_int32_t h;
	int s, dir;

	s = splhigh();
	if (sc->sc_rts == NULL) {
		if (setflags && flags == IFBAF_STATIC) {
			sc->sc_rts = (struct bridge_rthead *)malloc(
			    BRIDGE_RTABLE_SIZE *
			    (sizeof(struct bridge_rthead)),M_DEVBUF,M_NOWAIT);

			if (sc->sc_rts == NULL)
				goto done;

			for (h = 0; h < BRIDGE_RTABLE_SIZE; h++)
				LIST_INIT(&sc->sc_rts[h]);
		}
		else
			goto done;
	}

	h = bridge_hash(ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	if (p == NULL) {
		if (sc->sc_brtcnt >= sc->sc_brtmax)
			goto done;
		p = (struct bridge_rtnode *)malloc(
		    sizeof(struct bridge_rtnode), M_DEVBUF, M_NOWAIT);
		if (p == NULL)
			goto done;

		bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
		p->brt_if = ifp;
		p->brt_age = 1;

		if (setflags)
			p->brt_flags = flags;
		else
			p->brt_flags = IFBAF_DYNAMIC;

		LIST_INSERT_HEAD(&sc->sc_rts[h], p, brt_next);
		sc->sc_brtcnt++;
		goto want;
	}

	do {
		q = p;
		p = LIST_NEXT(p, brt_next);

		dir = bcmp(ea, &q->brt_addr, sizeof(q->brt_addr));
		if (dir == 0) {
			if (setflags) {
				q->brt_if = ifp;
				q->brt_flags = flags;
			}

			if (q->brt_if == ifp)
				q->brt_age = 1;
			ifp = q->brt_if;
			goto want;
		}

		if (dir > 0) {
			if (sc->sc_brtcnt >= sc->sc_brtmax)
				goto done;
			p = (struct bridge_rtnode *)malloc(
			    sizeof(struct bridge_rtnode), M_DEVBUF, M_NOWAIT);
			if (p == NULL)
				goto done;

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;

			if (setflags)
				p->brt_flags = flags;
			else
				p->brt_flags = IFBAF_DYNAMIC;

			LIST_INSERT_BEFORE(q, p, brt_next);
			sc->sc_brtcnt++;
			goto want;
		}

		if (p == NULL) {
			if (sc->sc_brtcnt >= sc->sc_brtmax)
				goto done;
			p = (struct bridge_rtnode *)malloc(
			    sizeof(struct bridge_rtnode), M_DEVBUF, M_NOWAIT);
			if (p == NULL)
				goto done;

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;

			if (setflags)
				p->brt_flags = flags;
			else
				p->brt_flags = IFBAF_DYNAMIC;
			LIST_INSERT_AFTER(q, p, brt_next);
			sc->sc_brtcnt++;
			goto want;
		}
	} while (p != NULL);

done:
	ifp = NULL;
want:
	splx(s);
	return (ifp);
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

	if (sc->sc_rts == NULL)
		goto fail;

	h = bridge_hash(ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	while (p != NULL) {
		dir = bcmp(ea, &p->brt_addr, sizeof(p->brt_addr));
		if (dir == 0) {
			splx(s);
			return (p->brt_if);
		}
		if (dir > 0)
			goto fail;
		p = LIST_NEXT(p, brt_next);
	}
fail:
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
	if (sc->sc_rts == NULL)
		goto done;

	/*
	 * Make sure we have to trim the address table
	 */
	if (sc->sc_brtcnt <= sc->sc_brtmax)
		goto done;

	/*
	 * Force an aging cycle, this might trim enough addresses.
	 */
	splx(s);
	bridge_rtage(sc);
	s = splhigh();

	if (sc->sc_brtcnt <= sc->sc_brtmax)
		goto done;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			p = LIST_NEXT(n, brt_next);
			if ((n->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF);
				n = p;
				if (sc->sc_brtcnt <= sc->sc_brtmax)
					goto done;
			}
		}
	}

done:
	if (sc->sc_rts != NULL && sc->sc_brtcnt == 0 &&
	    (sc->sc_if.if_flags & IFF_UP) == 0) {
		free(sc->sc_rts, M_DEVBUF);
		sc->sc_rts = NULL;
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
			if ((n->brt_flags & IFBAF_TYPEMASK) == IFBAF_STATIC) {
				n->brt_age = !n->brt_age;
				if (n->brt_age)
					n->brt_age = 0;
				n = LIST_NEXT(n, brt_next);
			}
			else if (n->brt_age) {
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
 * Remove all dynamic addresses from the cache
 */
int
bridge_rtflush(sc, full)
	struct bridge_softc *sc;
	int full;
{
	int s, i;
	struct bridge_rtnode *p, *n;

	s = splhigh();
	if (sc->sc_rts == NULL)
		goto done;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (full ||
			    (n->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF);
				n = p;
			} else
				n = LIST_NEXT(n, brt_next);
		}
	}

	if (sc->sc_brtcnt == 0 && (sc->sc_if.if_flags & IFF_UP) == 0) {
		free(sc->sc_rts, M_DEVBUF);
		sc->sc_rts = NULL;
	}

done:
	splx(s);
	return (0);
}

/*
 * Remove an address from the cache
 */
int
bridge_rtdaddr(sc, ea)
	struct bridge_softc *sc;
	struct ether_addr *ea;
{
	int h, s;
	struct bridge_rtnode *p;

	s = splhigh();
	if (sc->sc_rts == NULL)
		goto done;

	h = bridge_hash(ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	while (p != NULL) {
		if (bcmp(ea, &p->brt_addr, sizeof(p->brt_addr)) == 0) {
			LIST_REMOVE(p, brt_next);
			sc->sc_brtcnt--;
			free(p, M_DEVBUF);
			if (sc->sc_brtcnt == 0 &&
			    (sc->sc_if.if_flags & IFF_UP) == 0) {
				free(sc->sc_rts, M_DEVBUF);
				sc->sc_rts = NULL;
			}
			splx(s);
			return (0);
		}
		p = LIST_NEXT(p, brt_next);
	}

done:
	splx(s);
	return (ENOENT);
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
	if (sc->sc_rts == NULL)
		goto done;

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
			} else
				n = LIST_NEXT(n, brt_next);
		}
	}
	if (sc->sc_brtcnt == 0 && (sc->sc_if.if_flags & IFF_UP) == 0) {
		free(sc->sc_rts, M_DEVBUF);
		sc->sc_rts = NULL;
	}

done:
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
	int i, s, error = 0;
	u_int32_t cnt = 0;
	struct bridge_rtnode *n;
	struct ifbareq bareq;

	s = splhigh();

	if (sc->sc_rts == NULL || baconf->ifbac_len == 0)
		goto done;

	for (i = 0, cnt = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (baconf->ifbac_len <
			    (cnt + 1) * sizeof(struct ifbareq))
				goto done;
			bcopy(sc->sc_if.if_xname, bareq.ifba_name,
			    sizeof(bareq.ifba_name));
			bcopy(n->brt_if->if_xname, bareq.ifba_ifsname,
			    sizeof(bareq.ifba_ifsname));
			bcopy(&n->brt_addr, &bareq.ifba_dst,
			    sizeof(bareq.ifba_dst));
			bareq.ifba_age = n->brt_age;
			bareq.ifba_flags = n->brt_flags;
			error = copyout((caddr_t)&bareq,
		    	    (caddr_t)(baconf->ifbac_req + cnt), sizeof(bareq));
			if (error)
				goto done;
			n = LIST_NEXT(n, brt_next);
			cnt++;
		}
	}
done:
	baconf->ifbac_len = cnt * sizeof(struct ifbareq);
	splx(s);
	return (error);
}

#if defined(INET) && (defined(IPFILTER) || defined(IPFILTER_LKM))

struct ehllc {
	struct ether_header eh;
	struct llc llc;
};

/*
 * Filter IP packets by peeking into the ethernet frame.  This violates
 * the ISO model, but allows us to act as a IP filter at the data link
 * layer.  As a result, most of this code will look familiar to those
 * who've read net/if_ethersubr.c and netinet/ip_input.c
 */
int
bridge_filter(sc, ifp, eh, np)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf **np;
{
	struct mbuf *m = *np;
	struct ehllc *ehllc;
	struct ip *ip;
	u_int16_t etype;
	int hlen, r, off = sizeof(struct ether_header);

	if (fr_checkp == NULL)
		return (BRIDGE_FILTER_PASS);

	if (m->m_len < sizeof(struct ehllc)) {
		m = m_pullup(m, sizeof(struct ehllc));
		*np = m;
		if (m == NULL)
			return (BRIDGE_FILTER_DROP);
	}

	ehllc = mtod(m, struct ehllc *);
	etype = ntohs(ehllc->eh.ether_type);
	if (etype != ETHERTYPE_IP) {
		if (etype > ETHERMTU)	/* Can't be SNAP */
			return (BRIDGE_FILTER_PASS);

		if (ehllc->llc.llc_control != LLC_UI ||
		    ehllc->llc.llc_dsap != LLC_SNAP_LSAP ||
		    ehllc->llc.llc_ssap != LLC_SNAP_LSAP ||
		    ehllc->llc.llc_snap.org_code[0] != 0 ||
		    ehllc->llc.llc_snap.org_code[1] != 0 ||
		    ehllc->llc.llc_snap.org_code[2] != 0 ||
		    ntohs(ehllc->llc.llc_snap.ether_type) != ETHERTYPE_IP)
			return (BRIDGE_FILTER_PASS);
		off += 8;
	}

	/*
	 * We need a full copy because we're going to be destructive
	 * to the packet before we pass it to the ip filter code.
	 * XXX This needs to be turned into a munge -> check ->
	 * XXX unmunge section, for now, we copy.
	 * XXX Copy at offset 0 so that the mbuf header is copied, too.
	 */
	m = m_copym2(m, 0, M_COPYALL, M_NOWAIT);
	m_adj(m, off);
	if (m == NULL)
		return (BRIDGE_FILTER_DROP);

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
		} else
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
	}

	/* Finally, we get to filter the packet! */
	if (fr_checkp && (*fr_checkp)(ip, hlen, ifp, 0, &m))
		return (BRIDGE_FILTER_DROP);

	r = BRIDGE_FILTER_PASS;

out:
	m_freem(m);
	return (r);
}
#endif

#endif  /* NBRIDGE */
