/*	$OpenBSD: if_bridge.c,v 1.106 2002/12/09 22:32:01 jason Exp $	*/

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include "bpfilter.h"
#include "gif.h"
#include "pf.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
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
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_icmp.h>
#endif

#ifdef IPSEC
#include <netinet/ip_ipsp.h>

#include <net/if_enc.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#define	BRIDGE_IN	PF_IN
#define	BRIDGE_OUT	PF_OUT
#else
#define	BRIDGE_IN	0
#define	BRIDGE_OUT	1
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
#ifndef	BRIDGE_RTABLE_MAX
#define	BRIDGE_RTABLE_MAX	100
#endif

/* spanning tree defaults */
#define	BSTP_DEFAULT_MAX_AGE		(20 * 256)
#define	BSTP_DEFAULT_HELLO_TIME		(2 * 256)
#define	BSTP_DEFAULT_FORWARD_DELAY	(15 * 256)
#define	BSTP_DEFAULT_HOLD_TIME		(1 * 256)
#define	BSTP_DEFAULT_BRIDGE_PRIORITY	0x8000
#define	BSTP_DEFAULT_PORT_PRIORITY	0x80
#define	BSTP_DEFAULT_PATH_COST		55

/*
 * Timeout (in seconds) for entries learned dynamically
 */
#ifndef	BRIDGE_RTABLE_TIMEOUT
#define	BRIDGE_RTABLE_TIMEOUT	240
#endif

extern int ifqmaxlen;

struct bridge_softc *bridgectl;
int nbridge;

void	bridgeattach(int);
int	bridge_ioctl(struct ifnet *, u_long, caddr_t);
void	bridge_start(struct ifnet *);
void	bridgeintr_frame(struct bridge_softc *, struct mbuf *);
void	bridge_broadcast(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *);
void	bridge_span(struct bridge_softc *, struct ether_header *,
    struct mbuf *);
void	bridge_stop(struct bridge_softc *);
void	bridge_init(struct bridge_softc *);
int	bridge_bifconf(struct bridge_softc *, struct ifbifconf *);

void	bridge_timer(void *);
int	bridge_rtfind(struct bridge_softc *, struct ifbaconf *);
void	bridge_rtage(struct bridge_softc *);
void	bridge_rttrim(struct bridge_softc *);
int	bridge_rtdaddr(struct bridge_softc *, struct ether_addr *);
int	bridge_rtflush(struct bridge_softc *, int);
struct ifnet *	bridge_rtupdate(struct bridge_softc *,
    struct ether_addr *, struct ifnet *ifp, int, u_int8_t);
struct ifnet *	bridge_rtlookup(struct bridge_softc *,
    struct ether_addr *);
u_int32_t	bridge_hash(struct bridge_softc *, struct ether_addr *);
int bridge_blocknonip(struct ether_header *, struct mbuf *);
int		bridge_addrule(struct bridge_iflist *,
    struct ifbrlreq *, int out);
int		bridge_flushrule(struct bridge_iflist *);
int	bridge_brlconf(struct bridge_softc *, struct ifbrlconf *);
u_int8_t bridge_filterrule(struct brl_head *, struct ether_header *);
#if NPF > 0
struct mbuf *bridge_filter(struct bridge_softc *, int, struct ifnet *,
    struct ether_header *, struct mbuf *m);
#endif
int	bridge_ifenqueue(struct bridge_softc *, struct ifnet *, struct mbuf *);
void	bridge_fragment(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *);
#ifdef INET
void	bridge_send_icmp_err(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *, int, struct llc *, int, int);
#endif
#ifdef IPSEC
int bridge_ipsec(int, int, int, struct mbuf *);
#endif

#define	ETHERADDR_IS_IP_MCAST(a) \
	/* struct etheraddr *a;	*/				\
	((a)->ether_addr_octet[0] == 0x01 &&			\
	 (a)->ether_addr_octet[1] == 0x00 &&			\
	 (a)->ether_addr_octet[2] == 0x5e)

void
bridgeattach(n)
	int n;
{
	struct bridge_softc *sc;
	struct ifnet *ifp;
	int i;

	bridgectl = malloc(n * sizeof(*sc), M_DEVBUF, M_NOWAIT);
	if (!bridgectl)
		return;
	nbridge = n;
	bzero(bridgectl, n * sizeof(*sc));
	for (sc = bridgectl, i = 0; i < nbridge; i++, sc++) {

		sc->sc_brtmax = BRIDGE_RTABLE_MAX;
		sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;
		sc->sc_bridge_max_age = BSTP_DEFAULT_MAX_AGE;
		sc->sc_bridge_hello_time = BSTP_DEFAULT_HELLO_TIME;
		sc->sc_bridge_forward_delay= BSTP_DEFAULT_FORWARD_DELAY;
		sc->sc_bridge_priority = BSTP_DEFAULT_BRIDGE_PRIORITY;
		sc->sc_hold_time = BSTP_DEFAULT_HOLD_TIME;
		timeout_set(&sc->sc_brtimeout, bridge_timer, sc);
		LIST_INIT(&sc->sc_iflist);
		LIST_INIT(&sc->sc_spanlist);
		ifp = &sc->sc_if;
		sprintf(ifp->if_xname, "bridge%d", i);
		ifp->if_softc = sc;
		ifp->if_mtu = ETHERMTU;
		ifp->if_ioctl = bridge_ioctl;
		ifp->if_output = bridge_output;
		ifp->if_start = bridge_start;
		ifp->if_type = IFT_BRIDGE;
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
		ifp->if_hdrlen = sizeof(struct ether_header);
		if_attach(ifp);
		if_alloc_sadl(ifp);
#if NBPFILTER > 0
		bpfattach(&sc->sc_if.if_bpf, ifp,
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
	struct ifbrparam *bparam = (struct ifbrparam *)data;
	struct ifbifconf *bifconf = (struct ifbifconf *)data;
	struct ifbrlreq *brlreq = (struct ifbrlreq *)data;
	struct ifbrlconf *brlconf = (struct ifbrlconf *)data;
	struct ifreq ifreq;
	int error = 0, s;
	struct bridge_iflist *p;

	s = splnet();
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

		/* If it's in the span list, it can't be a member. */
		LIST_FOREACH(p, &sc->sc_spanlist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p != LIST_END(&sc->sc_spanlist)) {
			error = EBUSY;
			break;
		}

		if (ifs->if_type == IFT_ETHER) {
			if ((ifs->if_flags & IFF_UP) == 0) {
				/*
				 * Bring interface up long enough to set
				 * promiscuous flag, then shut it down again.
				 */
				strlcpy(ifreq.ifr_name, req->ifbr_ifsname,
				    IFNAMSIZ);
				ifs->if_flags |= IFF_UP;
				ifreq.ifr_flags = ifs->if_flags;
				error = (*ifs->if_ioctl)(ifs, SIOCSIFFLAGS,
				    (caddr_t)&ifreq);
				if (error != 0)
					break;

				error = ifpromisc(ifs, 1);
				if (error != 0)
					break;

				strlcpy(ifreq.ifr_name, req->ifbr_ifsname,
				    IFNAMSIZ);
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
#if NGIF > 0
		else if (ifs->if_type == IFT_GIF) {
			/* Nothing needed */
		}
#endif /* NGIF */
		else {
			error = EINVAL;
			break;
		}

		p = (struct bridge_iflist *) malloc(
		    sizeof(struct bridge_iflist), M_DEVBUF, M_NOWAIT);
		if (p == NULL) {
			if (ifs->if_type == IFT_ETHER)
				ifpromisc(ifs, 0);
			error = ENOMEM;
			break;
		}
		bzero(p, sizeof(struct bridge_iflist));

		p->ifp = ifs;
		p->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
		p->bif_priority = BSTP_DEFAULT_PORT_PRIORITY;
		p->bif_path_cost = BSTP_DEFAULT_PATH_COST;
		SIMPLEQ_INIT(&p->bif_brlin);
		SIMPLEQ_INIT(&p->bif_brlout);
		LIST_INSERT_HEAD(&sc->sc_iflist, p, next);
		ifs->if_bridge = (caddr_t)sc;
		break;
	case SIOCBRDGDEL:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;

		LIST_FOREACH(p, &sc->sc_iflist, next) {
			if (strncmp(p->ifp->if_xname, req->ifbr_ifsname,
			    sizeof(p->ifp->if_xname)) == 0) {
				p->ifp->if_bridge = NULL;

				error = ifpromisc(p->ifp, 0);

				LIST_REMOVE(p, next);
				bridge_rtdelete(sc, p->ifp, 0);
				bridge_flushrule(p);
				free(p, M_DEVBUF);
				break;
			}
		}
		if (p == LIST_END(&sc->sc_iflist)) {
			error = ENOENT;
			break;
		}
		break;
	case SIOCBRDGIFS:
		error = bridge_bifconf(sc, bifconf);
		break;
	case SIOCBRDGADDS:
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
		LIST_FOREACH(p, &sc->sc_spanlist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p != LIST_END(&sc->sc_spanlist)) {
			error = EBUSY;
			break;
		}
		p = (struct bridge_iflist *)malloc(
		    sizeof(struct bridge_iflist), M_DEVBUF, M_NOWAIT);
		if (p == NULL) {
			error = ENOMEM;
			break;
		}
		bzero(p, sizeof(struct bridge_iflist));
		p->ifp = ifs;
		SIMPLEQ_INIT(&p->bif_brlin);
		SIMPLEQ_INIT(&p->bif_brlout);
		LIST_INSERT_HEAD(&sc->sc_spanlist, p, next);
		break;
	case SIOCBRDGDELS:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		LIST_FOREACH(p, &sc->sc_spanlist, next) {
			if (strncmp(p->ifp->if_xname, req->ifbr_ifsname,
			    sizeof(p->ifp->if_xname)) == 0) {
				LIST_REMOVE(p, next);
				free(p, M_DEVBUF);
				break;
			}
		}
		if (p == LIST_END(&sc->sc_spanlist)) {
			error = ENOENT;
			break;
		}
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
		LIST_FOREACH(p, &sc->sc_iflist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p == LIST_END(&sc->sc_iflist)) {
			error = ESRCH;
			break;
		}
		req->ifbr_ifsflags = p->bif_flags;
		req->ifbr_state = p->bif_state;
		req->ifbr_priority = p->bif_priority;
		req->ifbr_path_cost = p->bif_path_cost;
		req->ifbr_portno = p->ifp->if_index & 0xff;
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
		LIST_FOREACH(p, &sc->sc_iflist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p == LIST_END(&sc->sc_iflist)) {
			error = ESRCH;
			break;
		}
		if (req->ifbr_ifsflags & IFBIF_RO_MASK) {
			error = EINVAL;
			break;
		}
		if ((req->ifbr_ifsflags & IFBIF_STP) &&
		    (ifs->if_type != IFT_ETHER)) {
			error = EINVAL;
			break;
		}
		p->bif_flags = req->ifbr_ifsflags;
		break;
	case SIOCBRDGSIFPRIO:
	case SIOCBRDGSIFCOST:
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
		LIST_FOREACH(p, &sc->sc_iflist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p == LIST_END(&sc->sc_iflist)) {
			error = ESRCH;
			break;
		}
		if (cmd == SIOCBRDGSIFPRIO)
			p->bif_priority = req->ifbr_priority;
		else {
			if (req->ifbr_path_cost < 1)
				error = EINVAL;
			else
				p->bif_path_cost = req->ifbr_path_cost;
		}
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
		bparam->ifbrp_csize = sc->sc_brtmax;
		break;
	case SIOCBRDGSCACHE:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		sc->sc_brtmax = bparam->ifbrp_csize;
		bridge_rttrim(sc);
		break;
	case SIOCBRDGSTO:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		sc->sc_brttimeout = bparam->ifbrp_ctime;
		timeout_del(&sc->sc_brtimeout);
		if (bparam->ifbrp_ctime != 0)
			timeout_add(&sc->sc_brtimeout, sc->sc_brttimeout * hz);
		break;
	case SIOCBRDGGTO:
		bparam->ifbrp_ctime = sc->sc_brttimeout;
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == IFF_UP)
			bridge_init(sc);

		if ((ifp->if_flags & IFF_UP) == 0)
			bridge_stop(sc);

		break;
	case SIOCBRDGARL:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		ifs = ifunit(brlreq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		if (ifs->if_bridge == NULL ||
		    ifs->if_bridge != (caddr_t)sc) {
			error = ESRCH;
			break;
		}
		LIST_FOREACH(p, &sc->sc_iflist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p == LIST_END(&sc->sc_iflist)) {
			error = ESRCH;
			break;
		}
		if ((brlreq->ifbr_action != BRL_ACTION_BLOCK &&
		    brlreq->ifbr_action != BRL_ACTION_PASS) ||
		    (brlreq->ifbr_flags & (BRL_FLAG_IN|BRL_FLAG_OUT)) == 0) {
			error = EINVAL;
			break;
		}
		if (brlreq->ifbr_flags & BRL_FLAG_IN) {
			error = bridge_addrule(p, brlreq, 0);
			if (error)
				break;
		}
		if (brlreq->ifbr_flags & BRL_FLAG_OUT) {
			error = bridge_addrule(p, brlreq, 1);
			if (error)
				break;
		}
		break;
	case SIOCBRDGFRL:
		if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
			break;
		ifs = ifunit(brlreq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		if (ifs->if_bridge == NULL ||
		    ifs->if_bridge != (caddr_t)sc) {
			error = ESRCH;
			break;
		}
		LIST_FOREACH(p, &sc->sc_iflist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p == LIST_END(&sc->sc_iflist)) {
			error = ESRCH;
			break;
		}
		error = bridge_flushrule(p);
		break;
	case SIOCBRDGGRL:
		error = bridge_brlconf(sc, brlconf);
		break;
	case SIOCBRDGGPRI:
	case SIOCBRDGGMA:
	case SIOCBRDGGHT:
	case SIOCBRDGGFD:
		break;
	case SIOCBRDGSPRI:
	case SIOCBRDGSFD:
	case SIOCBRDGSMA:
	case SIOCBRDGSHT:
		error = suser(prc->p_ucred, &prc->p_acflag);
		break;
	default:
		error = EINVAL;
	}

	if (!error)
		error = bstp_ioctl(ifp, cmd, data);

	splx(s);
	return (error);
}

/* Detach an interface from a bridge.  */
void
bridge_ifdetach(ifp)
	struct ifnet *ifp;
{
	struct bridge_softc *sc = (struct bridge_softc *)ifp->if_bridge;
	struct bridge_iflist *bif;

	LIST_FOREACH(bif, &sc->sc_iflist, next) {
		if (bif->ifp == ifp) {
			LIST_REMOVE(bif, next);
			bridge_rtdelete(sc, ifp, 0);
			bridge_flushrule(bif);
			free(bif, M_DEVBUF);
			ifp->if_bridge = NULL;
			break;
		}
	}
}

int
bridge_bifconf(sc, bifc)
	struct bridge_softc *sc;
	struct ifbifconf *bifc;
{
	struct bridge_iflist *p;
	u_int32_t total = 0, i = 0;
	int error = 0;
	struct ifbreq breq;

	LIST_FOREACH(p, &sc->sc_iflist, next) {
		total++;
	}
	LIST_FOREACH(p, &sc->sc_spanlist, next) {
		total++;
	}
	if (bifc->ifbic_len == 0) {
		i = total;
		goto done;
	}

	LIST_FOREACH(p, &sc->sc_iflist, next) {
		if (bifc->ifbic_len < sizeof(breq))
			break;
		strlcpy(breq.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(breq.ifbr_ifsname, p->ifp->if_xname, IFNAMSIZ);
		breq.ifbr_ifsflags = p->bif_flags;
		breq.ifbr_state = p->bif_state;
		breq.ifbr_priority = p->bif_priority;
		breq.ifbr_path_cost = p->bif_path_cost;
		breq.ifbr_portno = p->ifp->if_index & 0xff;
		error = copyout((caddr_t)&breq,
		    (caddr_t)(bifc->ifbic_req + i), sizeof(breq));
		if (error)
			goto done;
		i++;
		bifc->ifbic_len -= sizeof(breq);
	}
	LIST_FOREACH(p, &sc->sc_spanlist, next) {
		if (bifc->ifbic_len < sizeof(breq))
			break;
		strlcpy(breq.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(breq.ifbr_ifsname, p->ifp->if_xname, IFNAMSIZ);
		breq.ifbr_ifsflags = p->bif_flags | IFBIF_SPAN;
		breq.ifbr_state = p->bif_state;
		breq.ifbr_priority = p->bif_priority;
		breq.ifbr_path_cost = p->bif_path_cost;
		breq.ifbr_portno = p->ifp->if_index & 0xff;
		error = copyout((caddr_t)&breq,
		    (caddr_t)(bifc->ifbic_req + i), sizeof(breq));
		if (error)
			goto done;
		i++;
		bifc->ifbic_len -= sizeof(breq);
	}

done:
	bifc->ifbic_len = i * sizeof(breq);
	return (error);
}

int
bridge_brlconf(sc, bc)
	struct bridge_softc *sc;
	struct ifbrlconf *bc;
{
	struct ifnet *ifp;
	struct bridge_iflist *ifl;
	struct brl_node *n;
	struct ifbrlreq req;
	int error = 0;
	u_int32_t i = 0, total = 0;

	ifp = ifunit(bc->ifbrl_ifsname);
	if (ifp == NULL)
		return (ENOENT);
	if (ifp->if_bridge == NULL || ifp->if_bridge != (caddr_t)sc)
		return (ESRCH);
	LIST_FOREACH(ifl, &sc->sc_iflist, next) {
		if (ifl->ifp == ifp)
			break;
	}
	if (ifl == LIST_END(&sc->sc_iflist))
		return (ESRCH);

	SIMPLEQ_FOREACH(n, &ifl->bif_brlin, brl_next) {
		total++;
	}
	SIMPLEQ_FOREACH(n, &ifl->bif_brlout, brl_next) {
		total++;
	}

	if (bc->ifbrl_len == 0) {
		i = total;
		goto done;
	}

	SIMPLEQ_FOREACH(n, &ifl->bif_brlin, brl_next) {
		if (bc->ifbrl_len < sizeof(req))
			goto done;
		strlcpy(req.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(req.ifbr_ifsname, ifl->ifp->if_xname, IFNAMSIZ);
		req.ifbr_action = n->brl_action;
		req.ifbr_flags = n->brl_flags;
		req.ifbr_src = n->brl_src;
		req.ifbr_dst = n->brl_dst;
		error = copyout((caddr_t)&req,
		    (caddr_t)(bc->ifbrl_buf + (i * sizeof(req))), sizeof(req));
		if (error)
			goto done;
		i++;
		bc->ifbrl_len -= sizeof(req);
	}

	SIMPLEQ_FOREACH(n, &ifl->bif_brlout, brl_next) {
		if (bc->ifbrl_len < sizeof(req))
			goto done;
		strlcpy(req.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(req.ifbr_ifsname, ifl->ifp->if_xname, IFNAMSIZ);
		req.ifbr_action = n->brl_action;
		req.ifbr_flags = n->brl_flags;
		req.ifbr_src = n->brl_src;
		req.ifbr_dst = n->brl_dst;
		error = copyout((caddr_t)&req,
		    (caddr_t)(bc->ifbrl_buf + (i * sizeof(req))), sizeof(req));
		if (error)
			goto done;
		i++;
		bc->ifbrl_len -= sizeof(req);
	}

done:
	bc->ifbrl_len = i * sizeof(req);
	return (error);
}

void
bridge_init(sc)
	struct bridge_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	int i;

	if ((ifp->if_flags & IFF_RUNNING) == IFF_RUNNING)
		return;

	if (sc->sc_rts == NULL) {
		sc->sc_rts = (struct bridge_rthead *)malloc(
		    BRIDGE_RTABLE_SIZE * (sizeof(struct bridge_rthead)),
		    M_DEVBUF, M_NOWAIT);
		if (sc->sc_rts == NULL)
			return;
		for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
			LIST_INIT(&sc->sc_rts[i]);
		}
		sc->sc_hashkey = arc4random();
	}
	ifp->if_flags |= IFF_RUNNING;

	if (sc->sc_brttimeout != 0)
		timeout_add(&sc->sc_brtimeout, sc->sc_brttimeout * hz);
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

	timeout_del(&sc->sc_brtimeout);

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
	int s, error, len;
#ifdef IPSEC
	struct m_tag *mtag;
#endif /* IPSEC */

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
	if (dst_if == NULL || ETHER_IS_MULTICAST(eh->ether_dhost)) {
		struct bridge_iflist *p;
		struct mbuf *mc;
		int used = 0;

#ifdef IPSEC
		/*
		 * Don't send out the packet if IPsec is needed, and
		 * notify IPsec to do its own crypto for now.
		 */
		if ((mtag = m_tag_find(m, PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED,
		    NULL)) != NULL) {
			ipsp_skipcrypto_unmark((struct tdb_ident *)(mtag + 1));
			m_freem(m);
			splx(s);
			return (0);
		}
#endif /* IPSEC */

		/* Catch packets that need TCP/UDP/IP hardware checksumming */
		if (m->m_pkthdr.csum & M_IPV4_CSUM_OUT ||
		    m->m_pkthdr.csum & M_TCPV4_CSUM_OUT ||
		    m->m_pkthdr.csum & M_UDPV4_CSUM_OUT) {
			m_freem(m);
			splx(s);
			return (0);
		}

		bridge_span(sc, NULL, m);

		LIST_FOREACH(p, &sc->sc_iflist, next) {
			dst_if = p->ifp;
			if ((dst_if->if_flags & IFF_RUNNING) == 0)
				continue;

			/*
			 * If this is not the original output interface,
			 * and the interface is participating in spanning
			 * tree, make sure the port is in a state that
			 * allows forwarding.
			 */
			if (dst_if != ifp &&
			    (p->bif_flags & IFBIF_STP) &&
			    (p->bif_state != BSTP_IFSTATE_FORWARDING))
				continue;

#ifdef ALTQ
			if (ALTQ_IS_ENABLED(&dst_if->if_snd) == 0)
#endif
			if (IF_QFULL(&dst_if->if_snd)) {
				IF_DROP(&dst_if->if_snd);
				sc->sc_if.if_oerrors++;
				continue;
			}
			if (LIST_NEXT(p, next) == LIST_END(&sc->sc_iflist)) {
				used = 1;
				mc = m;
			} else {
				struct mbuf *m1, *m2, *mx;

				m1 = m_copym2(m, 0, sizeof(struct ether_header),
				    M_DONTWAIT);
				if (m1 == NULL) {
					sc->sc_if.if_oerrors++;
					continue;
				}
				m2 = m_copym2(m, sizeof(struct ether_header),
				    M_COPYALL, M_DONTWAIT);
				if (m2 == NULL) {
					m_freem(m1);
					sc->sc_if.if_oerrors++;
					continue;
				}

				for (mx = m1; mx->m_next != NULL; mx = mx->m_next)
					/*EMPTY*/;
				mx->m_next = m2;

				if (m1->m_flags & M_PKTHDR) {
					len = 0;
					for (mx = m1; mx != NULL; mx = mx->m_next)
						len += mx->m_len;
					m1->m_pkthdr.len = len;
				}
				mc = m1;
			}

			error = bridge_ifenqueue(sc, dst_if, mc);
			if (error)
				continue;
		}
		if (!used)
			m_freem(m);
		splx(s);
		return (0);
	}

sendunicast:
	bridge_span(sc, NULL, m);
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		splx(s);
		return (0);
	}
	bridge_ifenqueue(sc, dst_if, m);
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
	struct bridge_softc *sc;
	struct mbuf *m;
	int i, s;

	for (i = 0; i < nbridge; i++) {
		sc = &bridgectl[i];
		for (;;) {
			s = splimp();
			IF_DEQUEUE(&sc->sc_if.if_snd, m);
			splx(s);
			if (m == NULL)
				break;
			bridgeintr_frame(sc, m);
		}
	}
}

/*
 * Process a single frame.  Frame must be freed or queued before returning.
 */
void
bridgeintr_frame(sc, m)
	struct bridge_softc *sc;
	struct mbuf *m;
{
	int s, len;
	struct ifnet *src_if, *dst_if;
	struct bridge_iflist *ifl;
	struct ether_addr *dst, *src;
	struct ether_header eh;

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		return;
	}

	src_if = m->m_pkthdr.rcvif;

#if NBPFILTER > 0
	if (sc->sc_if.if_bpf)
		bpf_mtap(sc->sc_if.if_bpf, m);
#endif

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	LIST_FOREACH(ifl, &sc->sc_iflist, next) {
		if (ifl->ifp == src_if)
			break;
	}
	if (ifl == LIST_END(&sc->sc_iflist)) {
		m_freem(m);
		return;
	}

	if ((ifl->bif_flags & IFBIF_STP) &&
	    (ifl->bif_state == BSTP_IFSTATE_BLOCKING ||
	    ifl->bif_state == BSTP_IFSTATE_LISTENING ||
	    ifl->bif_state == BSTP_IFSTATE_DISABLED)) {
		m_freem(m);
		return;
	}

	if (m->m_pkthdr.len < sizeof(eh)) {
		m_freem(m);
		return;
	}
	m_copydata(m, 0, sizeof(struct ether_header), (caddr_t)&eh);
	dst = (struct ether_addr *)&eh.ether_dhost[0];
	src = (struct ether_addr *)&eh.ether_shost[0];

	/*
	 * If interface is learning, and if source address
	 * is not broadcast or multicast, record it's address.
	 */
	if ((ifl->bif_flags & IFBIF_LEARNING) &&
	    (eh.ether_shost[0] & 1) == 0 &&
	    !(eh.ether_shost[0] == 0 && eh.ether_shost[1] == 0 &&
	    eh.ether_shost[2] == 0 && eh.ether_shost[3] == 0 &&
	    eh.ether_shost[4] == 0 && eh.ether_shost[5] == 0))
		bridge_rtupdate(sc, src, src_if, 0, IFBAF_DYNAMIC);

	if ((ifl->bif_flags & IFBIF_STP) &&
	    (ifl->bif_state == BSTP_IFSTATE_LEARNING)) {
		m_freem(m);
		return;
	}

	/*
	 * At this point, the port either doesn't participate in stp or
	 * it's in the forwarding state
	 */

	/*
	 * If packet is unicast, destined for someone on "this"
	 * side of the bridge, drop it.
	 */
	if ((m->m_flags & (M_BCAST | M_MCAST)) == 0) {
		dst_if = bridge_rtlookup(sc, dst);
		if (dst_if == src_if) {
			m_freem(m);
			return;
		}
	} else
		dst_if = NULL;

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
			return;
		}
		if (sc->sc_if.if_flags & IFF_LINK0 &&
		    ETHERADDR_IS_IP_MCAST(dst)) {
			m_freem(m);
			return;
		}
		if (sc->sc_if.if_flags & IFF_LINK1 &&
		    !ETHERADDR_IS_IP_MCAST(dst)) {
			m_freem(m);
			return;
		}
	}

	if (ifl->bif_flags & IFBIF_BLOCKNONIP && bridge_blocknonip(&eh, m)) {
		m_freem(m);
		return;
	}

	if (bridge_filterrule(&ifl->bif_brlin, &eh) == BRL_ACTION_BLOCK) {
		m_freem(m);
		return;
	}
#if NPF > 0
	m = bridge_filter(sc, BRIDGE_IN, src_if, &eh, m);
	if (m == NULL)
		return;
#endif
	/*
	 * If the packet is a multicast or broadcast OR if we don't
	 * know any better, forward it to all interfaces.
	 */
	if ((m->m_flags & (M_BCAST | M_MCAST)) || dst_if == NULL) {
		sc->sc_if.if_imcasts++;
		s = splimp();
		bridge_broadcast(sc, src_if, &eh, m);
		splx(s);
		return;
	}

	/*
	 * At this point, we're dealing with a unicast frame going to a
	 * different interface
	 */
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		return;
	}
	LIST_FOREACH(ifl, &sc->sc_iflist, next) {
		if (ifl->ifp == dst_if)
			break;
	}
	if (ifl == LIST_END(&sc->sc_iflist)) {
		m_freem(m);
		return;
	}
	if ((ifl->bif_flags & IFBIF_STP) &&
	    (ifl->bif_state == BSTP_IFSTATE_DISABLED ||
	    ifl->bif_state == BSTP_IFSTATE_BLOCKING)) {
		m_freem(m);
		return;
	}
	if (bridge_filterrule(&ifl->bif_brlout, &eh) == BRL_ACTION_BLOCK) {
		m_freem(m);
		return;
	}
#if NPF > 0
	m = bridge_filter(sc, BRIDGE_OUT, dst_if, &eh, m);
	if (m == NULL)
		return;
#endif

	len = m->m_pkthdr.len;
	if ((len - sizeof(struct ether_header)) > dst_if->if_mtu)
		bridge_fragment(sc, dst_if, &eh, m);
	else {
		s = splimp();
		bridge_ifenqueue(sc, dst_if, m);
		splx(s);
	}
}

/*
 * Receive input from an interface.  Queue the packet for bridging if its
 * not for us, and schedule an interrupt.
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
	struct mbuf *mc;

	/*
	 * Make sure this interface is a bridge member.
	 */
	if (ifp == NULL || ifp->if_bridge == NULL || m == NULL)
		return (m);

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("bridge_input(): no HDR");

	m->m_flags &= ~M_PROTO1;	/* Loop prevention */

	sc = (struct bridge_softc *)ifp->if_bridge;
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return (m);

	LIST_FOREACH(ifl, &sc->sc_iflist, next) {
		if (ifl->ifp == ifp)
			break;
	}
	if (ifl == LIST_END(&sc->sc_iflist))
		return (m);

	bridge_span(sc, eh, m);

	if (m->m_flags & (M_BCAST | M_MCAST)) {
		/* Tap off 802.1D packets, they do not get forwarded */
		if (bcmp(eh->ether_dhost, bstp_etheraddr, ETHER_ADDR_LEN) == 0) {
			m = bstp_input(sc, ifp, eh, m);
			if (m == NULL)
				return (NULL);
		}

		/*
		 * No need to queue frames for ifs in the blocking, disabled,
		 *  or listening state
		 */
		if ((ifl->bif_flags & IFBIF_STP) &&
		    ((ifl->bif_state == BSTP_IFSTATE_BLOCKING) ||
		    (ifl->bif_state == BSTP_IFSTATE_LISTENING) ||
		    (ifl->bif_state == BSTP_IFSTATE_DISABLED)))
			return (m);

		/*
		 * make a copy of 'm' with 'eh' tacked on to the
		 * beginning.  Return 'm' for local processing
		 * and enqueue the copy.  Schedule netisr.
		 */
		mc = m_copym2(m, 0, M_COPYALL, M_NOWAIT);
		if (mc == NULL)
			return (m);
		M_PREPEND(mc, sizeof(struct ether_header), M_DONTWAIT);
		if (mc == NULL)
			return (m);
		bcopy(eh, mtod(mc, caddr_t), sizeof(struct ether_header));
		s = splimp();
		if (IF_QFULL(&sc->sc_if.if_snd)) {
			m_freem(mc);
			splx(s);
			return (m);
		}
		IF_ENQUEUE(&sc->sc_if.if_snd, mc);
		splx(s);
		schednetisr(NETISR_BRIDGE);
		if (ifp->if_type == IFT_GIF) {
			LIST_FOREACH(ifl, &sc->sc_iflist, next) {
				if (ifl->ifp->if_type == IFT_ETHER)
					break;
			}
			if (ifl != LIST_END(&sc->sc_iflist)) {
				m->m_flags |= M_PROTO1;
				m->m_pkthdr.rcvif = ifl->ifp;
				ether_input(ifl->ifp, eh, m);
				m = NULL;
			}
		}
		return (m);
	}

	/*
	 * No need to queue frames for ifs in the blocking, disabled, or
	 * listening state
	 */
	if ((ifl->bif_flags & IFBIF_STP) &&
	    ((ifl->bif_state == BSTP_IFSTATE_BLOCKING) ||
	    (ifl->bif_state == BSTP_IFSTATE_LISTENING) ||
	    (ifl->bif_state == BSTP_IFSTATE_DISABLED)))
		return (m);


	/*
	 * Unicast, make sure it's not for us.
	 */
	LIST_FOREACH(ifl, &sc->sc_iflist, next) {
		if (ifl->ifp->if_type != IFT_ETHER)
			continue;
		ac = (struct arpcom *)ifl->ifp;
		if (bcmp(ac->ac_enaddr, eh->ether_dhost, ETHER_ADDR_LEN) == 0) {
			if (ifl->bif_flags & IFBIF_LEARNING)
				bridge_rtupdate(sc,
				    (struct ether_addr *)&eh->ether_shost,
				    ifp, 0, IFBAF_DYNAMIC);
			m->m_pkthdr.rcvif = ifl->ifp;
			if (ifp->if_type == IFT_GIF) {
				m->m_flags |= M_PROTO1;
				ether_input(ifl->ifp, eh, m);
				m = NULL;
			}
			return (m);
		}
		if (bcmp(ac->ac_enaddr, eh->ether_shost, ETHER_ADDR_LEN) == 0) {
			m_freem(m);
			return (NULL);
		}
	}
	M_PREPEND(m, sizeof(struct ether_header), M_DONTWAIT);
	if (m == NULL)
		return (NULL);
	bcopy(eh, mtod(m, caddr_t), sizeof(struct ether_header));
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
	struct ifnet *dst_if;
	int len = m->m_pkthdr.len, used = 0;

	splassert(IPL_NET);

	LIST_FOREACH(p, &sc->sc_iflist, next) {
		/*
		 * Don't retransmit out of the same interface where
		 * the packet was received from.
		 */
		dst_if = p->ifp;
		if (dst_if->if_index == ifp->if_index)
			continue;

		if ((p->bif_flags & IFBIF_STP) &&
		    (p->bif_state != BSTP_IFSTATE_FORWARDING))
			continue;

		if ((p->bif_flags & IFBIF_DISCOVER) == 0 &&
		    (m->m_flags & (M_BCAST | M_MCAST)) == 0)
			continue;

		if ((dst_if->if_flags & IFF_RUNNING) == 0)
			continue;

#ifdef ALTQ
		if (ALTQ_IS_ENABLED(&dst_if->if_snd) == 0)
#endif
		if (IF_QFULL(&dst_if->if_snd)) {
			IF_DROP(&dst_if->if_snd);
			sc->sc_if.if_oerrors++;
			continue;
		}

		/* Drop non-IP frames if the appropriate flag is set. */
		if (p->bif_flags & IFBIF_BLOCKNONIP &&
		    bridge_blocknonip(eh, m))
			continue;

		if (bridge_filterrule(&p->bif_brlout, eh) == BRL_ACTION_BLOCK)
			continue;

		/* If last one, reuse the passed-in mbuf */
		if (LIST_NEXT(p, next) == LIST_END(&sc->sc_iflist)) {
			mc = m;
			used = 1;
		} else {
			struct mbuf *m1, *m2, *mx;

			m1 = m_copym2(m, 0, sizeof(struct ether_header),
			    M_DONTWAIT);
			if (m1 == NULL) {
				sc->sc_if.if_oerrors++;
				continue;
			}
			m2 = m_copym2(m, sizeof(struct ether_header),
			    M_COPYALL, M_DONTWAIT);
			if (m2 == NULL) {
				m_freem(m1);
				sc->sc_if.if_oerrors++;
				continue;
			}

			for (mx = m1; mx->m_next != NULL; mx = mx->m_next)
				/*EMPTY*/;
			mx->m_next = m2;

			if (m1->m_flags & M_PKTHDR) {
				int len = 0;

				for (mx = m1; mx != NULL; mx = mx->m_next)
					len += mx->m_len;
				m1->m_pkthdr.len = len;
			}
			mc = m1;
		}

#if NPF > 0
		mc = bridge_filter(sc, BRIDGE_OUT, dst_if, eh, mc);
		if (mc == NULL)
			continue;
#endif

		if ((len - sizeof(struct ether_header)) > dst_if->if_mtu)
			bridge_fragment(sc, dst_if, eh, mc);
		else {
			bridge_ifenqueue(sc, dst_if, mc);
		}
	}

	if (!used)
		m_freem(m);
}

void
bridge_span(sc, eh, morig)
	struct bridge_softc *sc;
	struct ether_header *eh;
	struct mbuf *morig;
{
	struct bridge_iflist *p;
	struct ifnet *ifp;
	struct mbuf *mc, *m;
	int error;

	if (LIST_EMPTY(&sc->sc_spanlist))
		return;

	m = m_copym2(morig, 0, M_COPYALL, M_NOWAIT);
	if (m == NULL)
		return;
	if (eh != NULL) {
		M_PREPEND(m, sizeof(struct ether_header), M_DONTWAIT);
		if (m == NULL)
			return;
		bcopy(eh, mtod(m, caddr_t), sizeof(struct ether_header));
	}

	LIST_FOREACH(p, &sc->sc_spanlist, next) {
		ifp = p->ifp;

		if ((ifp->if_flags & IFF_RUNNING) == 0)
			continue;

#ifdef ALTQ
		if (ALTQ_IS_ENABLED(&ifp->if_snd) == 0)
#endif
			if (IF_QFULL(&ifp->if_snd)) {
				IF_DROP(&ifp->if_snd);
				sc->sc_if.if_oerrors++;
				continue;
			}

		mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
		if (mc == NULL) {
			sc->sc_if.if_oerrors++;
			continue;
		}

		error = bridge_ifenqueue(sc, ifp, m);
		if (error)
			continue;
	}
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
	int dir;

	if (sc->sc_rts == NULL) {
		if (setflags && flags == IFBAF_STATIC) {
			sc->sc_rts = (struct bridge_rthead *)malloc(
			    BRIDGE_RTABLE_SIZE *
			    (sizeof(struct bridge_rthead)),M_DEVBUF,M_NOWAIT);
			if (sc->sc_rts == NULL)
				goto done;

			for (h = 0; h < BRIDGE_RTABLE_SIZE; h++) {
				LIST_INIT(&sc->sc_rts[h]);
			}
			sc->sc_hashkey = arc4random();
		} else
			goto done;
	}

	h = bridge_hash(sc, ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	if (p == LIST_END(&sc->sc_rts[h])) {
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

		dir = memcmp(ea, &q->brt_addr, sizeof(q->brt_addr));
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

		if (p == LIST_END(&sc->sc_rts[h])) {
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
	} while (p != LIST_END(&sc->sc_rts[h]));

done:
	ifp = NULL;
want:
	return (ifp);
}

struct ifnet *
bridge_rtlookup(sc, ea)
	struct bridge_softc *sc;
	struct ether_addr *ea;
{
	struct bridge_rtnode *p;
	u_int32_t h;
	int dir;

	if (sc->sc_rts == NULL)
		goto fail;

	h = bridge_hash(sc, ea);
	LIST_FOREACH(p, &sc->sc_rts[h], brt_next) {
		dir = memcmp(ea, &p->brt_addr, sizeof(p->brt_addr));
		if (dir == 0)
			return (p->brt_if);
		if (dir > 0)
			goto fail;
	}
fail:
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
bridge_hash(sc, addr)
	struct bridge_softc *sc;
	struct ether_addr *addr;
{
	u_int32_t a = 0x9e3779b9, b = 0x9e3779b9, c = sc->sc_hashkey;

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
	int i;

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
	bridge_rtage(sc);

	if (sc->sc_brtcnt <= sc->sc_brtmax)
		goto done;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != LIST_END(&sc->sc_rts[i])) {
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
}

void
bridge_timer(vsc)
	void *vsc;
{
	struct bridge_softc *sc = vsc;
	int s;

	s = splsoftnet();
	bridge_rtage(sc);
	splx(s);
}

/*
 * Perform an aging cycle
 */
void
bridge_rtage(sc)
	struct bridge_softc *sc;
{
	struct bridge_rtnode *n, *p;
	int i;

	if (sc->sc_rts == NULL)
		return;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != LIST_END(&sc->sc_rts[i])) {
			if ((n->brt_flags & IFBAF_TYPEMASK) == IFBAF_STATIC) {
				n->brt_age = !n->brt_age;
				if (n->brt_age)
					n->brt_age = 0;
				n = LIST_NEXT(n, brt_next);
			} else if (n->brt_age) {
				n->brt_age = 0;
				n = LIST_NEXT(n, brt_next);
			} else {
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF);
				n = p;
			}
		}
	}

	if (sc->sc_brttimeout != 0)
		timeout_add(&sc->sc_brtimeout, sc->sc_brttimeout * hz);
}

/*
 * Remove all dynamic addresses from the cache
 */
int
bridge_rtflush(sc, full)
	struct bridge_softc *sc;
	int full;
{
	int i;
	struct bridge_rtnode *p, *n;

	if (sc->sc_rts == NULL)
		return (0);

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != LIST_END(&sc->sc_rts[i])) {
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
	int h;
	struct bridge_rtnode *p;

	if (sc->sc_rts == NULL)
		return (ENOENT);

	h = bridge_hash(sc, ea);
	LIST_FOREACH(p, &sc->sc_rts[h], brt_next) {
		if (bcmp(ea, &p->brt_addr, sizeof(p->brt_addr)) == 0) {
			LIST_REMOVE(p, brt_next);
			sc->sc_brtcnt--;
			free(p, M_DEVBUF);
			if (sc->sc_brtcnt == 0 &&
			    (sc->sc_if.if_flags & IFF_UP) == 0) {
				free(sc->sc_rts, M_DEVBUF);
				sc->sc_rts = NULL;
			}
			return (0);
		}
	}

	return (ENOENT);
}
/*
 * Delete routes to a specific interface member.
 */
void
bridge_rtdelete(sc, ifp, dynonly)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	int dynonly;
{
	int i;
	struct bridge_rtnode *n, *p;

	if (sc->sc_rts == NULL)
		return;

	/*
	 * Loop through all of the hash buckets and traverse each
	 * chain looking for routes to this interface.
	 */
	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != LIST_END(&sc->sc_rts[i])) {
			if (n->brt_if != ifp) {
				/* Not ours */
				n = LIST_NEXT(n, brt_next);
				continue;
			}
			if (dynonly &&
			    (n->brt_flags & IFBAF_TYPEMASK) != IFBAF_DYNAMIC) {
				/* only deleting dynamics */
				n = LIST_NEXT(n, brt_next);
				continue;
			}
			p = LIST_NEXT(n, brt_next);
			LIST_REMOVE(n, brt_next);
			sc->sc_brtcnt--;
			free(n, M_DEVBUF);
			n = p;
		}
	}
	if (sc->sc_brtcnt == 0 && (sc->sc_if.if_flags & IFF_UP) == 0) {
		free(sc->sc_rts, M_DEVBUF);
		sc->sc_rts = NULL;
	}
}

/*
 * Gather all of the routes for this interface.
 */
int
bridge_rtfind(sc, baconf)
	struct bridge_softc *sc;
	struct ifbaconf *baconf;
{
	int i, error = 0, onlycnt = 0;
	u_int32_t cnt = 0;
	struct bridge_rtnode *n;
	struct ifbareq bareq;

	if (sc->sc_rts == NULL)
		goto done;

	if (baconf->ifbac_len == 0)
		onlycnt = 1;

	for (i = 0, cnt = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		LIST_FOREACH(n, &sc->sc_rts[i], brt_next) {
			if (!onlycnt) {
				if (baconf->ifbac_len < sizeof(struct ifbareq))
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
				baconf->ifbac_len -= sizeof(struct ifbareq);
			}
			cnt++;
		}
	}
done:
	baconf->ifbac_len = cnt * sizeof(struct ifbareq);
	return (error);
}

/*
 * Block non-ip frames:
 * Returns 0 if frame is ip, and 1 if it should be dropped.
 */
int
bridge_blocknonip(eh, m)
	struct ether_header *eh;
	struct mbuf *m;
{
	struct llc llc;
	u_int16_t etype;

	if (m->m_pkthdr.len < sizeof(struct ether_header))
		return (1);

	etype = ntohs(eh->ether_type);
	switch (etype) {
	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
	case ETHERTYPE_IP:
	case ETHERTYPE_IPV6:
		return (0);
	}

	if (etype > ETHERMTU)
		return (1);

	if (m->m_pkthdr.len <
	    (sizeof(struct ether_header) + LLC_SNAPFRAMELEN))
		return (1);

	m_copydata(m, sizeof(struct ether_header), LLC_SNAPFRAMELEN,
	    (caddr_t)&llc);

	etype = ntohs(llc.llc_snap.ether_type);
	if (llc.llc_dsap == LLC_SNAP_LSAP &&
	    llc.llc_ssap == LLC_SNAP_LSAP &&
	    llc.llc_control == LLC_UI &&
	    llc.llc_snap.org_code[0] == 0 &&
	    llc.llc_snap.org_code[1] == 0 &&
	    llc.llc_snap.org_code[2] == 0 &&
	    (etype == ETHERTYPE_ARP || etype == ETHERTYPE_REVARP ||
	    etype == ETHERTYPE_IP || etype == ETHERTYPE_IPV6)) {
		return (0);
	}

	return (1);
}

u_int8_t
bridge_filterrule(h, eh)
	struct brl_head *h;
	struct ether_header *eh;
{
	struct brl_node *n;
	u_int8_t flags;

	SIMPLEQ_FOREACH(n, h, brl_next) {
		flags = n->brl_flags & (BRL_FLAG_SRCVALID|BRL_FLAG_DSTVALID);
		if (flags == 0)
			return (n->brl_action);
		if (flags == (BRL_FLAG_SRCVALID|BRL_FLAG_DSTVALID)) {
			if (bcmp(eh->ether_shost, &n->brl_src, ETHER_ADDR_LEN))
				continue;
			if (bcmp(eh->ether_dhost, &n->brl_src, ETHER_ADDR_LEN))
				continue;
			return (n->brl_action);
		}
		if (flags == BRL_FLAG_SRCVALID) {
			if (bcmp(eh->ether_shost, &n->brl_src, ETHER_ADDR_LEN))
				continue;
			return (n->brl_action);
		}
		if (flags == BRL_FLAG_DSTVALID) {
			if (bcmp(eh->ether_dhost, &n->brl_dst, ETHER_ADDR_LEN))
				continue;
			return (n->brl_action);
		}
	}
	return (BRL_ACTION_PASS);
}

int
bridge_addrule(bif, req, out)
	struct bridge_iflist *bif;
	struct ifbrlreq *req;
	int out;
{
	struct brl_node *n;

	n = (struct brl_node *)malloc(sizeof(struct brl_node), M_DEVBUF, M_NOWAIT);
	if (n == NULL)
		return (ENOMEM);
	bcopy(&req->ifbr_src, &n->brl_src, sizeof(struct ether_addr));
	bcopy(&req->ifbr_dst, &n->brl_dst, sizeof(struct ether_addr));
	n->brl_action = req->ifbr_action;
	n->brl_flags = req->ifbr_flags;
	if (out) {
		n->brl_flags &= ~BRL_FLAG_IN;
		n->brl_flags |= BRL_FLAG_OUT;
		SIMPLEQ_INSERT_TAIL(&bif->bif_brlout, n, brl_next);
	} else {
		n->brl_flags &= ~BRL_FLAG_OUT;
		n->brl_flags |= BRL_FLAG_IN;
		SIMPLEQ_INSERT_TAIL(&bif->bif_brlin, n, brl_next);
	}
	return (0);
}

int
bridge_flushrule(bif)
	struct bridge_iflist *bif;
{
	struct brl_node *p;

	while (!SIMPLEQ_EMPTY(&bif->bif_brlin)) {
		p = SIMPLEQ_FIRST(&bif->bif_brlin);
		SIMPLEQ_REMOVE_HEAD(&bif->bif_brlin, p, brl_next);
		free(p, M_DEVBUF);
	}
	while (!SIMPLEQ_EMPTY(&bif->bif_brlout)) {
		p = SIMPLEQ_FIRST(&bif->bif_brlout);
		SIMPLEQ_REMOVE_HEAD(&bif->bif_brlout, p, brl_next);
		free(p, M_DEVBUF);
	}
	return (0);
}

#ifdef IPSEC
int
bridge_ipsec(dir, af, hlen, m)
	int dir, af, hlen;
	struct mbuf *m;
{
	union sockaddr_union dst;
	struct timeval tv;
	struct tdb *tdb;
	u_int32_t spi;
	u_int16_t cpi;
	int error, off;
	u_int8_t proto = 0;
#ifdef INET
	struct ip *ip;
#endif /* INET */
#ifdef INET6
	struct ip6_hdr *ip6;
#endif /* INET6 */

	if (dir == BRIDGE_IN) {
		switch (af) {
#ifdef INET
		case AF_INET:
			if (m->m_pkthdr.len - hlen < 2 * sizeof(u_int32_t))
				break;

			ip = mtod(m, struct ip *);
			proto = ip->ip_p;
			off = offsetof(struct ip, ip_p);

			if (proto != IPPROTO_ESP && proto != IPPROTO_AH &&
			    proto != IPPROTO_IPCOMP)
				goto skiplookup;

			bzero(&dst, sizeof(union sockaddr_union));
			dst.sa.sa_family = AF_INET;
			dst.sin.sin_len = sizeof(struct sockaddr_in);
			m_copydata(m, offsetof(struct ip, ip_dst),
			    sizeof(struct in_addr),
			    (caddr_t)&dst.sin.sin_addr);

			if (ip->ip_p == IPPROTO_ESP)
				m_copydata(m, hlen, sizeof(u_int32_t),
				    (caddr_t)&spi);
			else if (ip->ip_p == IPPROTO_AH)
				m_copydata(m, hlen + sizeof(u_int32_t),
				    sizeof(u_int32_t), (caddr_t)&spi);
			else if (ip->ip_p == IPPROTO_IPCOMP) {
				m_copydata(m, hlen + sizeof(u_int16_t),
				    sizeof(u_int16_t), (caddr_t)&cpi);
				spi = ntohl(htons(cpi));
			}
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			if (m->m_pkthdr.len - hlen < 2 * sizeof(u_int32_t))
				break;

			ip6 = mtod(m, struct ip6_hdr *);

			/* XXX We should chase down the header chain */
			proto = ip6->ip6_nxt;
			off = offsetof(struct ip6_hdr, ip6_nxt);

			if (proto != IPPROTO_ESP && proto != IPPROTO_AH &&
			    proto != IPPROTO_IPCOMP)
				goto skiplookup;

			bzero(&dst, sizeof(union sockaddr_union));
			dst.sa.sa_family = AF_INET6;
			dst.sin6.sin6_len = sizeof(struct sockaddr_in6);
			m_copydata(m, offsetof(struct ip6_hdr, ip6_nxt),
			    sizeof(struct in6_addr),
			    (caddr_t)&dst.sin6.sin6_addr);

			if (proto == IPPROTO_ESP)
				m_copydata(m, hlen, sizeof(u_int32_t),
				    (caddr_t)&spi);
			else if (proto == IPPROTO_AH)
				m_copydata(m, hlen + sizeof(u_int32_t),
				    sizeof(u_int32_t), (caddr_t)&spi);
			else if (proto == IPPROTO_IPCOMP) {
				m_copydata(m, hlen + sizeof(u_int16_t),
				    sizeof(u_int16_t), (caddr_t)&cpi);
				spi = ntohl(htons(cpi));
			}
			break;
#endif /* INET6 */
		default:
			return (0);
		}

		if (proto == 0)
			goto skiplookup;

		tdb = gettdb(spi, &dst, proto);
		if (tdb != NULL && (tdb->tdb_flags & TDBF_INVALID) == 0 &&
		    tdb->tdb_xform != NULL) {
			if (tdb->tdb_first_use == 0) {
				int pri;

				pri = splhigh();
				tdb->tdb_first_use = time.tv_sec;
				splx(pri);

				tv.tv_usec = 0;

				/* Check for wrap-around. */
				if (tdb->tdb_exp_first_use + tdb->tdb_first_use
				    < tdb->tdb_first_use)
					tv.tv_sec = ((unsigned long)-1) / 2;
				else
					tv.tv_sec = tdb->tdb_exp_first_use +
					    tdb->tdb_first_use;

				if (tdb->tdb_flags & TDBF_FIRSTUSE)
					timeout_add(&tdb->tdb_first_tmo,
					    hzto(&tv));

				/* Check for wrap-around. */
				if (tdb->tdb_first_use +
				    tdb->tdb_soft_first_use
				    < tdb->tdb_first_use)
					tv.tv_sec = ((unsigned long)-1) / 2;
				else
					tv.tv_sec = tdb->tdb_first_use +
					    tdb->tdb_soft_first_use;

				if (tdb->tdb_flags & TDBF_SOFT_FIRSTUSE)
					timeout_add(&tdb->tdb_sfirst_tmo,
					    hzto(&tv));
			}

			(*(tdb->tdb_xform->xf_input))(m, tdb, hlen, off);
			return (1);
		} else {
 skiplookup:
			/* XXX do an input policy lookup */
			return (0);
		}
	} else { /* Outgoing from the bridge. */
		tdb = ipsp_spd_lookup(m, af, hlen, &error,
		    IPSP_DIRECTION_OUT, NULL, NULL);
		if (tdb != NULL) {
			/*
			 * We don't need to do loop detection, the
			 * bridge will do that for us.
			 */
#if NPF > 0
			switch (af) {
#ifdef INET
			case AF_INET:
				if (pf_test(dir, &encif[0].sc_if,
				    &m) != PF_PASS) {
					m_freem(m);
					return (1);
				}
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				if (pf_test6(dir, &encif[0].sc_if,
				    &m) != PF_PASS) {
					m_freem(m);
					return (1);
				}
				break;
#endif /* INET6 */
			}
			if (m == NULL)
				return (1);
#endif /* NPF */
#ifdef INET
			if (af == AF_INET) {
				ip = mtod(m, struct ip *);
				HTONS(ip->ip_len);
				HTONS(ip->ip_id);
				HTONS(ip->ip_off);
			}
#endif /* INET */
			error = ipsp_process_packet(m, tdb, af, 0);
			return (1);
		} else
			return (0);
	}

	return (0);
}
#endif /* IPSEC */

#if NPF > 0
/*
 * Filter IP packets by peeking into the ethernet frame.  This violates
 * the ISO model, but allows us to act as a IP filter at the data link
 * layer.  As a result, most of this code will look familiar to those
 * who've read net/if_ethersubr.c and netinet/ip_input.c
 */
struct mbuf *
bridge_filter(sc, dir, ifp, eh, m)
	struct bridge_softc *sc;
	int dir;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
{
	struct llc llc;
	int hassnap = 0;
	struct ip *ip;
	int hlen;
	u_int16_t etype;

	etype = ntohs(eh->ether_type);

	if (etype != ETHERTYPE_IP && etype != ETHERTYPE_IPV6) {
		if (etype > ETHERMTU ||
		    m->m_pkthdr.len < (LLC_SNAPFRAMELEN +
		    sizeof(struct ether_header)))
			return (m);

		m_copydata(m, sizeof(struct ether_header),
		    LLC_SNAPFRAMELEN, (caddr_t)&llc);

		if (llc.llc_dsap != LLC_SNAP_LSAP ||
		    llc.llc_ssap != LLC_SNAP_LSAP ||
		    llc.llc_control != LLC_UI ||
		    llc.llc_snap.org_code[0] ||
		    llc.llc_snap.org_code[1] ||
		    llc.llc_snap.org_code[2])
			return (m);

		etype = ntohs(llc.llc_snap.ether_type);
		if (etype != ETHERTYPE_IP && etype != ETHERTYPE_IPV6)
			return (m);
		hassnap = 1;
	}

	m_adj(m, sizeof(struct ether_header));
	if (hassnap)
		m_adj(m, LLC_SNAPFRAMELEN);

	switch (etype) {

	case ETHERTYPE_IP:
		if (m->m_pkthdr.len < sizeof(struct ip))
			goto dropit;

		/* Copy minimal header, and drop invalids */
		if (m->m_len < sizeof(struct ip) &&
		    (m = m_pullup(m, sizeof(struct ip))) == NULL) {
			ipstat.ips_toosmall++;
			return (NULL);
		}
		ip = mtod(m, struct ip *);

		if (ip->ip_v != IPVERSION) {
			ipstat.ips_badvers++;
			goto dropit;
		}

		hlen = ip->ip_hl << 2;	/* get whole header length */
		if (hlen < sizeof(struct ip)) {
			ipstat.ips_badhlen++;
			goto dropit;
		}

		if (hlen > m->m_len) {
			if ((m = m_pullup(m, hlen)) == NULL) {
				ipstat.ips_badhlen++;
				return (NULL);
			}
			ip = mtod(m, struct ip *);
		}

		if ((ip->ip_sum = in_cksum(m, hlen)) != 0) {
			ipstat.ips_badsum++;
			goto dropit;
		}

		NTOHS(ip->ip_len);
		if (ip->ip_len < hlen)
			goto dropit;
		NTOHS(ip->ip_id);
		NTOHS(ip->ip_off);

		if (m->m_pkthdr.len < ip->ip_len)
			goto dropit;
		if (m->m_pkthdr.len > ip->ip_len) {
			if (m->m_len == m->m_pkthdr.len) {
				m->m_len = ip->ip_len;
				m->m_pkthdr.len = ip->ip_len;
			} else
				m_adj(m, ip->ip_len - m->m_pkthdr.len);
		}

#ifdef IPSEC
		if ((sc->sc_if.if_flags & IFF_LINK2) == IFF_LINK2 &&
		    bridge_ipsec(dir, AF_INET, hlen, m))
			return (NULL);
#endif /* IPSEC */

#if NPF > 0
		/* Finally, we get to filter the packet! */
		m->m_pkthdr.rcvif = ifp;
		if (pf_test(dir, ifp, &m) != PF_PASS)
			goto dropit;
		if (m == NULL)
			goto dropit;
#endif /* NPF */

		/* Rebuild the IP header */
		if (m->m_len < hlen && ((m = m_pullup(m, hlen)) == NULL))
			return (NULL);
		if (m->m_len < sizeof(struct ip))
			goto dropit;
		ip = mtod(m, struct ip *);
		HTONS(ip->ip_len);
		HTONS(ip->ip_id);
		HTONS(ip->ip_off);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);

		break;

#ifdef INET6
	case ETHERTYPE_IPV6: {
		struct ip6_hdr *ip6;

		if (m->m_len < sizeof(struct ip6_hdr)) {
			if ((m = m_pullup(m, sizeof(struct ip6_hdr)))
			    == NULL) {
				ip6stat.ip6s_toosmall++;
				return (NULL);
			}
		}

		ip6 = mtod(m, struct ip6_hdr *);

		if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
			ip6stat.ip6s_badvers++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
			goto dropit;
		}

#ifdef IPSEC
		hlen = sizeof(struct ip6_hdr);

		if ((sc->sc_if.if_flags & IFF_LINK2) == IFF_LINK2 &&
		    bridge_ipsec(dir, AF_INET6, hlen, m))
			return (NULL);
#endif /* IPSEC */

#if NPF > 0
		if (pf_test6(dir, ifp, &m) != PF_PASS)
			goto dropit;
		if (m == NULL)
			return (NULL);
#endif /* NPF */

		break;
	}
#endif /* INET6 */

	default:
		goto dropit;
		break;
	}

	/* Reattach SNAP header */
	if (hassnap) {
		M_PREPEND(m, LLC_SNAPFRAMELEN, M_DONTWAIT);
		if (m == NULL)
			goto dropit;
		bcopy(&llc, mtod(m, caddr_t), LLC_SNAPFRAMELEN);
	}

	/* Reattach ethernet header */
	M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
	if (m == NULL)
		goto dropit;
	bcopy(eh, mtod(m, caddr_t), sizeof(*eh));

	return (m);

dropit:
	if (m != NULL)
		m_freem(m);
	return (NULL);
}
#endif /* NPF > 0 */

void
bridge_fragment(sc, ifp, eh, m)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
{
	struct llc llc;
	struct mbuf *m0;
	int s, len, error = 0;
	int hassnap = 0;
#ifdef INET
	u_int16_t etype;
	struct ip *ip;
#endif

#ifndef INET
	goto dropit;
#else
	etype = ntohs(eh->ether_type);
	if (etype != ETHERTYPE_IP) {
		if (etype > ETHERMTU ||
		    m->m_pkthdr.len < (LLC_SNAPFRAMELEN +
		    sizeof(struct ether_header)))
			goto dropit;
		
		m_copydata(m, sizeof(struct ether_header),
		    LLC_SNAPFRAMELEN, (caddr_t)&llc);
		
		if (llc.llc_dsap != LLC_SNAP_LSAP ||
		    llc.llc_ssap != LLC_SNAP_LSAP ||
		    llc.llc_control != LLC_UI ||
		    llc.llc_snap.org_code[0] ||
		    llc.llc_snap.org_code[1] ||
		    llc.llc_snap.org_code[2] ||
		    llc.llc_snap.ether_type != htons(ETHERTYPE_IP))
			goto dropit;
		
		hassnap = 1;
	}
	
	m_adj(m, sizeof(struct ether_header));
	if (hassnap)
		m_adj(m, LLC_SNAPFRAMELEN);

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		goto dropit;
	ip = mtod(m, struct ip *);
	NTOHS(ip->ip_len);
	NTOHS(ip->ip_off);

	/* Respect IP_DF, return a ICMP_UNREACH_NEEDFRAG. */
	if (ip->ip_off & IP_DF) {
		bridge_send_icmp_err(sc, ifp, eh, m, hassnap, &llc,
		    ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG);
		return;
	}

	error = ip_fragment(m, ifp, ifp->if_mtu);
	if (error == EMSGSIZE)
		goto dropit;

	for (; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (error == 0) {
			if (hassnap) {
				M_PREPEND(m, LLC_SNAPFRAMELEN, M_DONTWAIT);
				if (m == NULL) {
					error = ENOBUFS;
					continue;
				}
				bcopy(&llc, mtod(m, caddr_t),
				    LLC_SNAPFRAMELEN);
			}
			M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
			if (m == NULL) {
				error = ENOBUFS;
				continue;
			}
			len = m->m_pkthdr.len;
			bcopy(eh, mtod(m, caddr_t), sizeof(*eh));
			s = splimp();
			error = bridge_ifenqueue(sc, ifp, m);
			if (error) {
				splx(s);
				continue;
			}
			splx(s);
		} else
			m_freem(m);
	}
	
	if (error == 0)
		ipstat.ips_fragmented++;
	
	return;
#endif /* INET */	
 dropit:
	if (m != NULL)
		m_freem(m);
}

int
bridge_ifenqueue(sc, ifp, m)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	struct mbuf *m;
{
	int error, len;
	short mflags;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	len = m->m_pkthdr.len;
	mflags = m->m_flags;
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_etherclassify(&ifp->if_snd, m, &pktattr);
#endif
	IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
	if (error) {
		sc->sc_if.if_oerrors++;
		return (error);
	}
	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += len;
	ifp->if_obytes += len;
	if (mflags & M_MCAST)
		ifp->if_omcasts++;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);

	return (0);
}

#ifdef INET
void	
bridge_send_icmp_err(sc, ifp, eh, n, hassnap, llc, type, code)
	struct bridge_softc *sc;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *n;
	int hassnap;
	struct llc *llc;
	int type;
	int code;
{
	struct ip *ip;
	struct icmp *icp;
	struct in_addr t;
	struct mbuf *m, *n2;
	int hlen;
	u_int8_t ether_tmp[ETHER_ADDR_LEN];

	n2 = m_copym(n, 0, M_COPYALL, M_DONTWAIT);
	if (!n2)
		return;
	m = icmp_do_error(n, type, code, 0, ifp);
	if (m == NULL)
		return;

	n = n2;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
	t = ip->ip_dst;
	ip->ip_dst = ip->ip_src;
	ip->ip_src = t;

	m->m_data += hlen;
	m->m_len -= hlen;
	icp = mtod(m, struct icmp *);
	icp->icmp_cksum = 0;
	icp->icmp_cksum = in_cksum(m, ip->ip_len - hlen);
	m->m_data -= hlen;
	m->m_len += hlen;

	ip->ip_v = IPVERSION;
	ip->ip_off &= IP_DF;
	ip->ip_id = htons(ip_randomid());
	ip->ip_ttl = MAXTTL;
	HTONS(ip->ip_len);
	HTONS(ip->ip_off);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, hlen);

	/* Swap ethernet addresses */
	bcopy(&eh->ether_dhost, &ether_tmp, sizeof(ether_tmp));
	bcopy(&eh->ether_shost, &eh->ether_dhost, sizeof(ether_tmp));
	bcopy(&ether_tmp, &eh->ether_shost, sizeof(ether_tmp));

	/* Reattach SNAP header */
	if (hassnap) {
		M_PREPEND(m, LLC_SNAPFRAMELEN, M_DONTWAIT);
		if (m == NULL)
			goto dropit;
		bcopy(llc, mtod(m, caddr_t), LLC_SNAPFRAMELEN);
	}

	/* Reattach ethernet header */
	M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
	if (m == NULL)
		goto dropit;
	bcopy(eh, mtod(m, caddr_t), sizeof(*eh));

	bridge_output(ifp, m, NULL, NULL);
	m_freem(n);
	return;

 dropit:
	m_freem(n);
}
#endif
