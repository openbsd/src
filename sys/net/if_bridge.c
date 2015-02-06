/*	$OpenBSD: if_bridge.c,v 1.232 2015/02/06 22:10:43 benno Exp $	*/

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
#include "carp.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/kernel.h>

#include <crypto/siphash.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_llc.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_icmp.h>

#ifdef IPSEC
#include <netinet/ip_ipsp.h>
#include <net/if_enc.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
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

#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif

#include <net/if_bridge.h>

/*
 * Maximum number of addresses to cache
 */
#ifndef	BRIDGE_RTABLE_MAX
#define	BRIDGE_RTABLE_MAX	100
#endif

/*
 * Timeout (in seconds) for entries learned dynamically
 */
#ifndef	BRIDGE_RTABLE_TIMEOUT
#define	BRIDGE_RTABLE_TIMEOUT	240
#endif

void	bridgeattach(int);
int	bridge_ioctl(struct ifnet *, u_long, caddr_t);
void	bridge_start(struct ifnet *);
void	bridgeintr_frame(struct bridge_softc *, struct mbuf *);
void	bridge_broadcast(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *);
void	bridge_localbroadcast(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *);
void	bridge_span(struct bridge_softc *, struct ether_header *,
    struct mbuf *);
void	bridge_stop(struct bridge_softc *);
void	bridge_init(struct bridge_softc *);
int	bridge_bifconf(struct bridge_softc *, struct ifbifconf *);

void	bridge_timer(void *);
int	bridge_rtfind(struct bridge_softc *, struct ifbaconf *);
void	bridge_rtage(struct bridge_softc *);
int	bridge_rtdaddr(struct bridge_softc *, struct ether_addr *);
void	bridge_rtflush(struct bridge_softc *, int);
struct ifnet *bridge_rtupdate(struct bridge_softc *,
    struct ether_addr *, struct ifnet *ifp, int, u_int8_t, struct mbuf *);
struct bridge_rtnode *bridge_rtlookup(struct bridge_softc *,
    struct ether_addr *);
u_int32_t	bridge_hash(struct bridge_softc *, struct ether_addr *);
int bridge_blocknonip(struct ether_header *, struct mbuf *);
int		bridge_addrule(struct bridge_iflist *,
    struct ifbrlreq *, int out);
void	bridge_flushrule(struct bridge_iflist *);
int	bridge_brlconf(struct bridge_softc *, struct ifbrlconf *);
u_int8_t bridge_filterrule(struct brl_head *, struct ether_header *,
    struct mbuf *);
struct mbuf *bridge_ip(struct bridge_softc *, int, struct ifnet *,
    struct ether_header *, struct mbuf *m);
int	bridge_ifenqueue(struct bridge_softc *, struct ifnet *, struct mbuf *);
void	bridge_fragment(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *);
void	bridge_send_icmp_err(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *, int, struct llc *, int, int, int);
#ifdef IPSEC
int bridge_ipsec(struct bridge_softc *, struct ifnet *,
    struct ether_header *, int, struct llc *,
    int, int, int, struct mbuf *);
#define ICMP_DEFLEN MHLEN
#endif
int     bridge_clone_create(struct if_clone *, int);
int	bridge_clone_destroy(struct ifnet *ifp);
int	bridge_delete(struct bridge_softc *, struct bridge_iflist *);
void	bridge_copyaddr(struct sockaddr *, struct sockaddr *);

#define	ETHERADDR_IS_IP_MCAST(a) \
	/* struct etheraddr *a;	*/				\
	((a)->ether_addr_octet[0] == 0x01 &&			\
	 (a)->ether_addr_octet[1] == 0x00 &&			\
	 (a)->ether_addr_octet[2] == 0x5e)

LIST_HEAD(, bridge_softc) bridge_list;

struct if_clone bridge_cloner =
    IF_CLONE_INITIALIZER("bridge", bridge_clone_create, bridge_clone_destroy);

/* ARGSUSED */
void
bridgeattach(int n)
{
	LIST_INIT(&bridge_list);
	if_clone_attach(&bridge_cloner);
}

int
bridge_clone_create(struct if_clone *ifc, int unit)
{
	struct bridge_softc *sc;
	struct ifnet *ifp;
	int i, s;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!sc)
		return (ENOMEM);

	sc->sc_stp = bstp_create(&sc->sc_if);
	if (!sc->sc_stp) {
		free(sc, M_DEVBUF, 0);
		return (ENOMEM);
	}

	sc->sc_brtmax = BRIDGE_RTABLE_MAX;
	sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;
	timeout_set(&sc->sc_brtimeout, bridge_timer, sc);
	TAILQ_INIT(&sc->sc_iflist);
	TAILQ_INIT(&sc->sc_spanlist);
	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++)
		LIST_INIT(&sc->sc_rts[i]);
	arc4random_buf(&sc->sc_hashkey, sizeof(sc->sc_hashkey));
	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name,
	    unit);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_ioctl = bridge_ioctl;
	ifp->if_output = bridge_output;
	ifp->if_start = bridge_start;
	ifp->if_type = IFT_BRIDGE;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&sc->sc_if.if_bpf, ifp,
	    DLT_EN10MB, ETHER_HDR_LEN);
#endif

	s = splnet();
	LIST_INSERT_HEAD(&bridge_list, sc, sc_list);
	splx(s);

	return (0);
}

int
bridge_clone_destroy(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct bridge_iflist *bif;
	int s;

	bridge_stop(sc);
	bridge_rtflush(sc, IFBF_FLUSHALL);
	while ((bif = TAILQ_FIRST(&sc->sc_iflist)) != NULL)
		bridge_delete(sc, bif);
	while ((bif = TAILQ_FIRST(&sc->sc_spanlist)) != NULL) {
		TAILQ_REMOVE(&sc->sc_spanlist, bif, next);
		free(bif, M_DEVBUF, 0);
	}

	s = splnet();
	LIST_REMOVE(sc, sc_list);
	splx(s);

	bstp_destroy(sc->sc_stp);
	if_detach(ifp);

	free(sc, M_DEVBUF, 0);
	return (0);
}

int
bridge_delete(struct bridge_softc *sc, struct bridge_iflist *p)
{
	int error;

	if (p->bif_flags & IFBIF_STP)
		bstp_delete(p->bif_stp);

	p->ifp->if_bridgeport = NULL;
	error = ifpromisc(p->ifp, 0);

	TAILQ_REMOVE(&sc->sc_iflist, p, next);
	bridge_rtdelete(sc, p->ifp, 0);
	bridge_flushrule(p);
	free(p, M_DEVBUF, 0);

	return (error);
}

int
bridge_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bridge_softc *sc = (struct bridge_softc *)ifp->if_softc;
	struct ifbreq *req = (struct ifbreq *)data;
	struct ifbareq *bareq = (struct ifbareq *)data;
	struct ifbrparam *bparam = (struct ifbrparam *)data;
	struct ifbrlreq *brlreq = (struct ifbrlreq *)data;
	struct ifbropreq *brop = (struct ifbropreq *)data;
	struct ifnet *ifs;
	struct bridge_iflist *p;
	struct bstp_port *bp;
	struct bstp_state *bs = sc->sc_stp;
	int error = 0, s;

	s = splnet();
	switch (cmd) {
	case SIOCBRDGADD:
		if ((error = suser(curproc, 0)) != 0)
			break;

		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}
		if (ifs->if_bridgeport != NULL) {
			p = (struct bridge_iflist *)ifs->if_bridgeport;
			if (p->bridge_sc == sc)
				error = EEXIST;
			else
				error = EBUSY;
			break;
		}

		/* If it's in the span list, it can't be a member. */
		TAILQ_FOREACH(p, &sc->sc_spanlist, next)
			if (p->ifp == ifs)
				break;
		if (p != NULL) {
			error = EBUSY;
			break;
		}

		if (ifs->if_type == IFT_ETHER) {
			if ((ifs->if_flags & IFF_UP) == 0) {
				struct ifreq ifreq;

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

		p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (p == NULL) {
			if (ifs->if_type == IFT_ETHER)
				ifpromisc(ifs, 0);
			error = ENOMEM;
			break;
		}

		p->bridge_sc = sc;
		p->ifp = ifs;
		p->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
		SIMPLEQ_INIT(&p->bif_brlin);
		SIMPLEQ_INIT(&p->bif_brlout);
		ifs->if_bridgeport = (caddr_t)p;
		TAILQ_INSERT_TAIL(&sc->sc_iflist, p, next);
		break;
	case SIOCBRDGDEL:
		if ((error = suser(curproc, 0)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		error = bridge_delete(sc, p);
		break;
	case SIOCBRDGIFS:
		error = bridge_bifconf(sc, (struct ifbifconf *)data);
		break;
	case SIOCBRDGADDS:
		if ((error = suser(curproc, 0)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}
		if (ifs->if_bridgeport != NULL) {
			error = EBUSY;
			break;
		}
		TAILQ_FOREACH(p, &sc->sc_spanlist, next) {
			if (p->ifp == ifs)
				break;
		}
		if (p != NULL) {
			error = EEXIST;
			break;
		}
		p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (p == NULL) {
			error = ENOMEM;
			break;
		}
		p->ifp = ifs;
		p->bif_flags = IFBIF_SPAN;
		SIMPLEQ_INIT(&p->bif_brlin);
		SIMPLEQ_INIT(&p->bif_brlout);
		TAILQ_INSERT_TAIL(&sc->sc_spanlist, p, next);
		break;
	case SIOCBRDGDELS:
		if ((error = suser(curproc, 0)) != 0)
			break;
		TAILQ_FOREACH(p, &sc->sc_spanlist, next) {
			if (strncmp(p->ifp->if_xname, req->ifbr_ifsname,
			    sizeof(p->ifp->if_xname)) == 0) {
				TAILQ_REMOVE(&sc->sc_spanlist, p, next);
				free(p, M_DEVBUF, 0);
				break;
			}
		}
		if (p == NULL) {
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
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		req->ifbr_ifsflags = p->bif_flags;
		req->ifbr_portno = p->ifp->if_index & 0xfff;
		if (p->bif_flags & IFBIF_STP) {
			bp = p->bif_stp;
			req->ifbr_state = bstp_getstate(bs, bp);
			req->ifbr_priority = bp->bp_priority;
			req->ifbr_path_cost = bp->bp_path_cost;
			req->ifbr_proto = bp->bp_protover;
			req->ifbr_role = bp->bp_role;
			req->ifbr_stpflags = bp->bp_flags;
			req->ifbr_fwd_trans = bp->bp_forward_transitions;
			req->ifbr_desg_bridge = bp->bp_desg_pv.pv_dbridge_id;
			req->ifbr_desg_port = bp->bp_desg_pv.pv_dport_id;
			req->ifbr_root_bridge = bp->bp_desg_pv.pv_root_id;
			req->ifbr_root_cost = bp->bp_desg_pv.pv_cost;
			req->ifbr_root_port = bp->bp_desg_pv.pv_port_id;

			/* Copy STP state options as flags */
			if (bp->bp_operedge)
				req->ifbr_ifsflags |= IFBIF_BSTP_EDGE;
			if (bp->bp_flags & BSTP_PORT_AUTOEDGE)
				req->ifbr_ifsflags |= IFBIF_BSTP_AUTOEDGE;
			if (bp->bp_ptp_link)
				req->ifbr_ifsflags |= IFBIF_BSTP_PTP;
			if (bp->bp_flags & BSTP_PORT_AUTOPTP)
				req->ifbr_ifsflags |= IFBIF_BSTP_AUTOPTP;
		}
		break;
	case SIOCBRDGSIFFLGS:
		if ((error = suser(curproc, 0)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		if (req->ifbr_ifsflags & IFBIF_RO_MASK) {
			error = EINVAL;
			break;
		}
		if (req->ifbr_ifsflags & IFBIF_STP) {
			if ((p->bif_flags & IFBIF_STP) == 0) {
				/* Enable STP */
				if ((p->bif_stp = bstp_add(sc->sc_stp,
				    p->ifp)) == NULL) {
					error = ENOMEM;
					break;
				}
			} else {
				/* Update STP flags */
				bstp_ifsflags(p->bif_stp, req->ifbr_ifsflags);
			}
		} else if (p->bif_flags & IFBIF_STP) {
			bstp_delete(p->bif_stp);
			p->bif_stp = NULL;
		}
		p->bif_flags = req->ifbr_ifsflags;
		break;
	case SIOCBRDGRTS:
		error = bridge_rtfind(sc, (struct ifbaconf *)data);
		break;
	case SIOCBRDGFLUSH:
		if ((error = suser(curproc, 0)) != 0)
			break;

		bridge_rtflush(sc, req->ifbr_ifsflags);
		break;
	case SIOCBRDGSADDR:
		if ((error = suser(curproc, 0)) != 0)
			break;
		ifs = ifunit(bareq->ifba_ifsname);
		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}

		ifs = bridge_rtupdate(sc, &bareq->ifba_dst, ifs, 1,
		    bareq->ifba_flags, NULL);
		if (ifs == NULL)
			error = ENOMEM;
		break;
	case SIOCBRDGDADDR:
		if ((error = suser(curproc, 0)) != 0)
			break;
		error = bridge_rtdaddr(sc, &bareq->ifba_dst);
		break;
	case SIOCBRDGGCACHE:
		bparam->ifbrp_csize = sc->sc_brtmax;
		break;
	case SIOCBRDGSCACHE:
		if ((error = suser(curproc, 0)) != 0)
			break;
		sc->sc_brtmax = bparam->ifbrp_csize;
		break;
	case SIOCBRDGSTO:
		if ((error = suser(curproc, 0)) != 0)
			break;
		if (bparam->ifbrp_ctime < 0 ||
		    bparam->ifbrp_ctime > INT_MAX / hz) {
			error = EINVAL;
			break;
		}
		sc->sc_brttimeout = bparam->ifbrp_ctime;
		if (bparam->ifbrp_ctime != 0)
			timeout_add_sec(&sc->sc_brtimeout, sc->sc_brttimeout);
		else
			timeout_del(&sc->sc_brtimeout);
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
		if ((error = suser(curproc, 0)) != 0)
			break;
		ifs = ifunit(brlreq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
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
		if ((error = suser(curproc, 0)) != 0)
			break;
		ifs = ifunit(brlreq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		bridge_flushrule(p);
		break;
	case SIOCBRDGGRL:
		error = bridge_brlconf(sc, (struct ifbrlconf *)data);
		break;
	case SIOCBRDGGPARAM:
		if ((bp = bs->bs_root_port) == NULL)
			brop->ifbop_root_port = 0;
		else
			brop->ifbop_root_port = bp->bp_ifp->if_index;
		brop->ifbop_maxage = bs->bs_bridge_max_age >> 8;
		brop->ifbop_hellotime = bs->bs_bridge_htime >> 8;
		brop->ifbop_fwddelay = bs->bs_bridge_fdelay >> 8;
		brop->ifbop_holdcount = bs->bs_txholdcount;
		brop->ifbop_priority = bs->bs_bridge_priority;
		brop->ifbop_protocol = bs->bs_protover;
		brop->ifbop_root_bridge = bs->bs_root_pv.pv_root_id;
		brop->ifbop_root_path_cost = bs->bs_root_pv.pv_cost;
		brop->ifbop_root_port = bs->bs_root_pv.pv_port_id;
		brop->ifbop_desg_bridge = bs->bs_root_pv.pv_dbridge_id;
		brop->ifbop_last_tc_time.tv_sec = bs->bs_last_tc_time.tv_sec;
		brop->ifbop_last_tc_time.tv_usec = bs->bs_last_tc_time.tv_usec;
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
	case SIOCBRDGSTXHC:
	case SIOCBRDGSPROTO:
	case SIOCBRDGSIFPRIO:
	case SIOCBRDGSIFCOST:
		error = suser(curproc, 0);
		break;
	default:
		error = ENOTTY;
		break;
	}

	if (!error)
		error = bstp_ioctl(ifp, cmd, data);

	splx(s);
	return (error);
}

/* Detach an interface from a bridge.  */
void
bridge_ifdetach(struct ifnet *ifp)
{
	struct bridge_softc *sc;
	struct bridge_iflist *bif;

	bif = (struct bridge_iflist *)ifp->if_bridgeport;
	sc = bif->bridge_sc;

	bridge_delete(sc, bif);
}

void
bridge_update(struct ifnet *ifp, struct ether_addr *ea, int delete)
{
	struct bridge_softc *sc;
	struct bridge_iflist *bif;
	u_int8_t *addr;

	addr = (u_int8_t *)ea;

	bif = (struct bridge_iflist *)ifp->if_bridgeport;
	sc = bif->bridge_sc;

	/*
	 * Update the bridge interface if it is in
	 * the learning state.
	 */
	if ((bif->bif_flags & IFBIF_LEARNING) &&
	    (ETHER_IS_MULTICAST(addr) == 0) &&
	    !(addr[0] == 0 && addr[1] == 0 && addr[2] == 0 &&
	    addr[3] == 0 && addr[4] == 0 && addr[5] == 0)) {
		/* Care must be taken with spanning tree */
		if ((bif->bif_flags & IFBIF_STP) &&
		    (bif->bif_state == BSTP_IFSTATE_DISCARDING))
			return;

		/* Delete the address from the bridge */
		bridge_rtdaddr(sc, ea);

		if (!delete) {
			/* Update the bridge table */
			bridge_rtupdate(sc, ea, ifp, 0, IFBAF_DYNAMIC, NULL);
		}
	}
}

int
bridge_bifconf(struct bridge_softc *sc, struct ifbifconf *bifc)
{
	struct bridge_iflist *p;
	struct bstp_port *bp;
	struct bstp_state *bs = sc->sc_stp;
	u_int32_t total = 0, i = 0;
	int error = 0;
	struct ifbreq *breq = NULL;

	TAILQ_FOREACH(p, &sc->sc_iflist, next)
		total++;

	TAILQ_FOREACH(p, &sc->sc_spanlist, next)
		total++;

	if (bifc->ifbic_len == 0) {
		i = total;
		goto done;
	}

	if ((breq = (struct ifbreq *)
	    malloc(sizeof(*breq), M_DEVBUF, M_NOWAIT)) == NULL)
		goto done;

	TAILQ_FOREACH(p, &sc->sc_iflist, next) {
		bzero(breq, sizeof(*breq));
		if (bifc->ifbic_len < sizeof(*breq))
			break;
		strlcpy(breq->ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(breq->ifbr_ifsname, p->ifp->if_xname, IFNAMSIZ);
		breq->ifbr_ifsflags = p->bif_flags;
		breq->ifbr_portno = p->ifp->if_index & 0xfff;
		if (p->bif_flags & IFBIF_STP) {
			bp = p->bif_stp;
			breq->ifbr_state = bstp_getstate(sc->sc_stp, bp);
			breq->ifbr_priority = bp->bp_priority;
			breq->ifbr_path_cost = bp->bp_path_cost;
			breq->ifbr_proto = bp->bp_protover;
			breq->ifbr_role = bp->bp_role;
			breq->ifbr_stpflags = bp->bp_flags;
			breq->ifbr_fwd_trans = bp->bp_forward_transitions;
			breq->ifbr_root_bridge = bs->bs_root_pv.pv_root_id;
			breq->ifbr_root_cost = bs->bs_root_pv.pv_cost;
			breq->ifbr_root_port = bs->bs_root_pv.pv_port_id;
			breq->ifbr_desg_bridge = bs->bs_root_pv.pv_dbridge_id;
			breq->ifbr_desg_port = bs->bs_root_pv.pv_dport_id;

			/* Copy STP state options as flags */
			if (bp->bp_operedge)
				breq->ifbr_ifsflags |= IFBIF_BSTP_EDGE;
			if (bp->bp_flags & BSTP_PORT_AUTOEDGE)
				breq->ifbr_ifsflags |= IFBIF_BSTP_AUTOEDGE;
			if (bp->bp_ptp_link)
				breq->ifbr_ifsflags |= IFBIF_BSTP_PTP;
			if (bp->bp_flags & BSTP_PORT_AUTOPTP)
				breq->ifbr_ifsflags |= IFBIF_BSTP_AUTOPTP;
		}
		error = copyout((caddr_t)breq,
		    (caddr_t)(bifc->ifbic_req + i), sizeof(*breq));
		if (error)
			goto done;
		i++;
		bifc->ifbic_len -= sizeof(*breq);
	}
	TAILQ_FOREACH(p, &sc->sc_spanlist, next) {
		bzero(breq, sizeof(*breq));
		if (bifc->ifbic_len < sizeof(*breq))
			break;
		strlcpy(breq->ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(breq->ifbr_ifsname, p->ifp->if_xname, IFNAMSIZ);
		breq->ifbr_ifsflags = p->bif_flags | IFBIF_SPAN;
		breq->ifbr_portno = p->ifp->if_index & 0xfff;
		error = copyout((caddr_t)breq,
		    (caddr_t)(bifc->ifbic_req + i), sizeof(*breq));
		if (error)
			goto done;
		i++;
		bifc->ifbic_len -= sizeof(*breq);
	}

done:
	if (breq != NULL)
		free(breq, M_DEVBUF, 0);
	bifc->ifbic_len = i * sizeof(*breq);
	return (error);
}

int
bridge_brlconf(struct bridge_softc *sc, struct ifbrlconf *bc)
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
	ifl = (struct bridge_iflist *)ifp->if_bridgeport;
	if (ifl == NULL || ifl->bridge_sc != sc)
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
		bzero(&req, sizeof req);
		if (bc->ifbrl_len < sizeof(req))
			goto done;
		strlcpy(req.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(req.ifbr_ifsname, ifl->ifp->if_xname, IFNAMSIZ);
		req.ifbr_action = n->brl_action;
		req.ifbr_flags = n->brl_flags;
		req.ifbr_src = n->brl_src;
		req.ifbr_dst = n->brl_dst;
#if NPF > 0
		req.ifbr_tagname[0] = '\0';
		if (n->brl_tag)
			pf_tag2tagname(n->brl_tag, req.ifbr_tagname);
#endif
		error = copyout((caddr_t)&req,
		    (caddr_t)(bc->ifbrl_buf + (i * sizeof(req))), sizeof(req));
		if (error)
			goto done;
		i++;
		bc->ifbrl_len -= sizeof(req);
	}

	SIMPLEQ_FOREACH(n, &ifl->bif_brlout, brl_next) {
		bzero(&req, sizeof req);
		if (bc->ifbrl_len < sizeof(req))
			goto done;
		strlcpy(req.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(req.ifbr_ifsname, ifl->ifp->if_xname, IFNAMSIZ);
		req.ifbr_action = n->brl_action;
		req.ifbr_flags = n->brl_flags;
		req.ifbr_src = n->brl_src;
		req.ifbr_dst = n->brl_dst;
#if NPF > 0
		req.ifbr_tagname[0] = '\0';
		if (n->brl_tag)
			pf_tag2tagname(n->brl_tag, req.ifbr_tagname);
#endif
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
bridge_init(struct bridge_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;

	if ((ifp->if_flags & IFF_RUNNING) == IFF_RUNNING)
		return;

	ifp->if_flags |= IFF_RUNNING;
	bstp_initialization(sc->sc_stp);

	if (sc->sc_brttimeout != 0)
		timeout_add_sec(&sc->sc_brtimeout, sc->sc_brttimeout);
}

/*
 * Stop the bridge and deallocate the routing table.
 */
void
bridge_stop(struct bridge_softc *sc)
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
bridge_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	struct ether_header *eh;
	struct ifnet *dst_if = NULL;
	struct bridge_rtnode *dst_p = NULL;
	struct ether_addr *dst;
	struct bridge_softc *sc;
	int s, error, len;
#ifdef IPSEC
	struct m_tag *mtag;
#endif /* IPSEC */

	/* ifp must be a member interface of the bridge. */ 
	if (ifp->if_bridgeport == NULL) {
		m_freem(m);
		return (EINVAL);
	}
	sc = ((struct bridge_iflist *)ifp->if_bridgeport)->bridge_sc;

	if (m->m_len < sizeof(*eh)) {
		m = m_pullup(m, sizeof(*eh));
		if (m == NULL)
			return (ENOBUFS);
	}
	eh = mtod(m, struct ether_header *);
	dst = (struct ether_addr *)&eh->ether_dhost[0];

	/*
	 * If bridge is down, but original output interface is up,
	 * go ahead and send out that interface.  Otherwise the packet
	 * is dropped below.
	 */
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		dst_if = ifp;
		goto sendunicast;
	}

#if NBPFILTER > 0
	if (sc->sc_if.if_bpf)
		bpf_mtap(sc->sc_if.if_bpf, m, BPF_DIRECTION_OUT);
#endif
	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/*
	 * If the packet is a broadcast or we don't know a better way to
	 * get there, send to all interfaces.
	 */
	if ((dst_p = bridge_rtlookup(sc, dst)) != NULL)
		dst_if = dst_p->brt_if;
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
			return (0);
		}
#endif /* IPSEC */
		bridge_span(sc, NULL, m);

		TAILQ_FOREACH(p, &sc->sc_iflist, next) {
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
			    (p->bif_state == BSTP_IFSTATE_DISCARDING))
				continue;

			if ((p->bif_flags & IFBIF_DISCOVER) == 0 &&
			    (m->m_flags & (M_BCAST | M_MCAST)) == 0)
				continue;

			if (IF_QFULL(&dst_if->if_snd)) {
				IF_DROP(&dst_if->if_snd);
				sc->sc_if.if_oerrors++;
				continue;
			}
			if (TAILQ_NEXT(p, next) == NULL) {
				used = 1;
				mc = m;
			} else {
				struct mbuf *m1, *m2, *mx;

				m1 = m_copym2(m, 0, ETHER_HDR_LEN,
				    M_DONTWAIT);
				if (m1 == NULL) {
					sc->sc_if.if_oerrors++;
					continue;
				}
				m2 = m_copym2(m, ETHER_HDR_LEN,
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

			s = splnet();
			error = bridge_ifenqueue(sc, dst_if, mc);
			splx(s);
			if (error)
				continue;
		}
		if (!used)
			m_freem(m);
		return (0);
	}

sendunicast:
	if (dst_p != NULL && dst_p->brt_tunnel.sa.sa_family != AF_UNSPEC &&
	    (sa = bridge_tunneltag(m, dst_p->brt_tunnel.sa.sa_family)) != NULL)
		memcpy(sa, &dst_p->brt_tunnel.sa, dst_p->brt_tunnel.sa.sa_len);

	bridge_span(sc, NULL, m);
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		return (ENETDOWN);
	}
	s = splnet();
	bridge_ifenqueue(sc, dst_if, m);
	splx(s);
	return (0);
}

/*
 * Start output on the bridge.  This function should never be called.
 */
void
bridge_start(struct ifnet *ifp)
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
	int s;

	LIST_FOREACH(sc, &bridge_list, sc_list) {
		for (;;) {
			s = splnet();
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
bridgeintr_frame(struct bridge_softc *sc, struct mbuf *m)
{
	int s, len;
	struct ifnet *src_if, *dst_if;
	struct bridge_iflist *ifl;
	struct bridge_rtnode *dst_p;
	struct ether_addr *dst, *src;
	struct ether_header eh;

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		return;
	}

	src_if = m->m_pkthdr.rcvif;

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	ifl = (struct bridge_iflist *)src_if->if_bridgeport;
	if (ifl == NULL) {
		m_freem(m);
		return;
	}

	if ((ifl->bif_flags & IFBIF_STP) &&
	    (ifl->bif_state == BSTP_IFSTATE_DISCARDING)) {
		m_freem(m);
		return;
	}

	if (m->m_pkthdr.len < sizeof(eh)) {
		m_freem(m);
		return;
	}
	m_copydata(m, 0, ETHER_HDR_LEN, (caddr_t)&eh);
	dst = (struct ether_addr *)&eh.ether_dhost[0];
	src = (struct ether_addr *)&eh.ether_shost[0];

	/*
	 * If interface is learning, and if source address
	 * is not broadcast or multicast, record its address.
	 */
	if ((ifl->bif_flags & IFBIF_LEARNING) &&
	    (eh.ether_shost[0] & 1) == 0 &&
	    !(eh.ether_shost[0] == 0 && eh.ether_shost[1] == 0 &&
	    eh.ether_shost[2] == 0 && eh.ether_shost[3] == 0 &&
	    eh.ether_shost[4] == 0 && eh.ether_shost[5] == 0))
		bridge_rtupdate(sc, src, src_if, 0, IFBAF_DYNAMIC, m);

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
		if ((dst_p = bridge_rtlookup(sc, dst)) != NULL)
			dst_if = dst_p->brt_if;
		else
			dst_if = NULL;
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

	if (bridge_filterrule(&ifl->bif_brlin, &eh, m) == BRL_ACTION_BLOCK) {
		m_freem(m);
		return;
	}
	m = bridge_ip(sc, BRIDGE_IN, src_if, &eh, m);
	if (m == NULL)
		return;
	/*
	 * If the packet is a multicast or broadcast OR if we don't
	 * know any better, forward it to all interfaces.
	 */
	if ((m->m_flags & (M_BCAST | M_MCAST)) || dst_if == NULL) {
		sc->sc_if.if_imcasts++;
		bridge_broadcast(sc, src_if, &eh, m);
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
	ifl = (struct bridge_iflist *)dst_if->if_bridgeport;
	if ((ifl->bif_flags & IFBIF_STP) &&
	    (ifl->bif_state == BSTP_IFSTATE_DISCARDING)) {
		m_freem(m);
		return;
	}
	if (bridge_filterrule(&ifl->bif_brlout, &eh, m) == BRL_ACTION_BLOCK) {
		m_freem(m);
		return;
	}
	m = bridge_ip(sc, BRIDGE_OUT, dst_if, &eh, m);
	if (m == NULL)
		return;

	len = m->m_pkthdr.len;
#if NVLAN > 0
	if ((m->m_flags & M_VLANTAG) &&
	    (dst_if->if_capabilities & IFCAP_VLAN_HWTAGGING) == 0)
		len += ETHER_VLAN_ENCAP_LEN;
#endif
	if ((len - ETHER_HDR_LEN) > dst_if->if_mtu)
		bridge_fragment(sc, dst_if, &eh, m);
	else {
		s = splnet();
		bridge_ifenqueue(sc, dst_if, m);
		splx(s);
	}
}

/*
 * Receive input from an interface.  Queue the packet for bridging if its
 * not for us, and schedule an interrupt.
 */
struct mbuf *
bridge_input(struct ifnet *ifp, struct ether_header *eh, struct mbuf *m)
{
	struct bridge_softc *sc;
	int s;
	struct bridge_iflist *ifl, *srcifl;
	struct arpcom *ac;
	struct mbuf *mc;

	/*
	 * Make sure this interface is a bridge member.
	 */
	if (ifp == NULL || ifp->if_bridgeport == NULL || m == NULL)
		return (m);

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("bridge_input(): no HDR");

	m->m_flags &= ~M_PROTO1;	/* Loop prevention */

	ifl = (struct bridge_iflist *)ifp->if_bridgeport;
	sc = ifl->bridge_sc;
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return (m);

#if NBPFILTER > 0
	if (sc->sc_if.if_bpf)
		bpf_mtap_hdr(sc->sc_if.if_bpf, (caddr_t)eh,
		    ETHER_HDR_LEN, m, BPF_DIRECTION_IN, NULL);
#endif

	bridge_span(sc, eh, m);

	if (m->m_flags & (M_BCAST | M_MCAST)) {
		/*
	 	 * Reserved destination MAC addresses (01:80:C2:00:00:0x)
		 * should not be forwarded to bridge members according to
		 * section 7.12.6 of the 802.1D-2004 specification.  The
		 * STP destination address (as stored in bstp_etheraddr)
		 * is the first of these.
	 	 */
		if (bcmp(eh->ether_dhost, bstp_etheraddr, ETHER_ADDR_LEN - 1)
		    == 0) {
			if (eh->ether_dhost[ETHER_ADDR_LEN - 1] == 0) {
				/* STP traffic */
				if ((m = bstp_input(sc->sc_stp, ifl->bif_stp,
				    eh, m)) == NULL)
					return (NULL);
			} else if (eh->ether_dhost[ETHER_ADDR_LEN - 1] <= 0xf) {
				m_freem(m);
				return (NULL);
			}
		}

		/*
		 * No need to queue frames for ifs in the discarding state
		 */
		if ((ifl->bif_flags & IFBIF_STP) &&
		    (ifl->bif_state == BSTP_IFSTATE_DISCARDING))
			return (m);

		/*
		 * make a copy of 'm' with 'eh' tacked on to the
		 * beginning.  Return 'm' for local processing
		 * and enqueue the copy.  Schedule netisr.
		 */
		mc = m_copym2(m, 0, M_COPYALL, M_NOWAIT);
		if (mc == NULL)
			return (m);
		M_PREPEND(mc, ETHER_HDR_LEN, M_DONTWAIT);
		if (mc == NULL)
			return (m);
		bcopy(eh, mtod(mc, caddr_t), ETHER_HDR_LEN);
		s = splnet();
		if (IF_QFULL(&sc->sc_if.if_snd)) {
			m_freem(mc);
			splx(s);
			return (m);
		}
		IF_ENQUEUE(&sc->sc_if.if_snd, mc);
		splx(s);
		schednetisr(NETISR_BRIDGE);
		if (ifp->if_type == IFT_GIF) {
			TAILQ_FOREACH(ifl, &sc->sc_iflist, next) {
				if (ifl->ifp->if_type == IFT_ETHER)
					break;
			}
			if (ifl != NULL) {
				m->m_pkthdr.rcvif = ifl->ifp;
				m->m_pkthdr.ph_rtableid = ifl->ifp->if_rdomain;
#if NBPFILTER > 0
				if (ifl->ifp->if_bpf)
					bpf_mtap(ifl->ifp->if_bpf, m,
					    BPF_DIRECTION_IN);
#endif
				m->m_flags |= M_PROTO1;
				ether_input(ifl->ifp, eh, m);
				ifl->ifp->if_ipackets++;
				m = NULL;
			}
		}
		return (m);
	}

	/*
	 * No need to queue frames for ifs in the discarding state
	 */
	if ((ifl->bif_flags & IFBIF_STP) &&
	    (ifl->bif_state == BSTP_IFSTATE_DISCARDING))
		return (m);

	/*
	 * Unicast, make sure it's not for us.
	 */
	srcifl = ifl;
	TAILQ_FOREACH(ifl, &sc->sc_iflist, next) {
		if (ifl->ifp->if_type != IFT_ETHER)
			continue;
		ac = (struct arpcom *)ifl->ifp;
		if (bcmp(ac->ac_enaddr, eh->ether_dhost, ETHER_ADDR_LEN) == 0
#if NCARP > 0
		    || (ifl->ifp->if_carp && carp_ourether(ifl->ifp->if_carp,
			(u_int8_t *)&eh->ether_dhost) != NULL)
#endif
		    ) {
			if (srcifl->bif_flags & IFBIF_LEARNING)
				bridge_rtupdate(sc,
				    (struct ether_addr *)&eh->ether_shost,
				    ifp, 0, IFBAF_DYNAMIC, m);
			if (bridge_filterrule(&srcifl->bif_brlin, eh, m) ==
			    BRL_ACTION_BLOCK) {
				m_freem(m);
				return (NULL);
			}

			/* Make sure the real incoming interface
			 * is aware */
#if NBPFILTER > 0
			if (ifl->ifp->if_bpf)
				bpf_mtap_hdr(ifl->ifp->if_bpf, (caddr_t)eh,
				    ETHER_HDR_LEN, m, BPF_DIRECTION_IN, NULL);
#endif
			/* Count for the interface we are going to */
			ifl->ifp->if_ipackets++;

			/* Count for the bridge */
			sc->sc_if.if_ipackets++;
			sc->sc_if.if_ibytes += ETHER_HDR_LEN + m->m_pkthdr.len;

			m->m_pkthdr.rcvif = ifl->ifp;
			m->m_pkthdr.ph_rtableid = ifl->ifp->if_rdomain;
			if (ifp->if_type == IFT_GIF) {
				m->m_flags |= M_PROTO1;
				ether_input(ifl->ifp, eh, m);
				m = NULL;
			}
			return (m);
		}
		if (bcmp(ac->ac_enaddr, eh->ether_shost, ETHER_ADDR_LEN) == 0
#if NCARP > 0
		    || (ifl->ifp->if_carp && carp_ourether(ifl->ifp->if_carp,
			(u_int8_t *)&eh->ether_shost) != NULL)
#endif
		    ) {
			m_freem(m);
			return (NULL);
		}
	}
	M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
	if (m == NULL)
		return (NULL);
	bcopy(eh, mtod(m, caddr_t), ETHER_HDR_LEN);
	s = splnet();
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
 * (except the one it came in on).
 */
void
bridge_broadcast(struct bridge_softc *sc, struct ifnet *ifp,
    struct ether_header *eh, struct mbuf *m)
{
	struct bridge_iflist *p;
	struct mbuf *mc;
	struct ifnet *dst_if;
	int len, s, used = 0;

	TAILQ_FOREACH(p, &sc->sc_iflist, next) {
		/*
		 * Don't retransmit out of the same interface where
		 * the packet was received from.
		 */
		dst_if = p->ifp;
		if (dst_if->if_index == ifp->if_index)
			continue;

		if ((p->bif_flags & IFBIF_STP) &&
		    (p->bif_state == BSTP_IFSTATE_DISCARDING))
			continue;

		if ((p->bif_flags & IFBIF_DISCOVER) == 0 &&
		    (m->m_flags & (M_BCAST | M_MCAST)) == 0)
			continue;

		if ((dst_if->if_flags & IFF_RUNNING) == 0)
			continue;

		if (IF_QFULL(&dst_if->if_snd)) {
			IF_DROP(&dst_if->if_snd);
			sc->sc_if.if_oerrors++;
			continue;
		}

		/* Drop non-IP frames if the appropriate flag is set. */
		if (p->bif_flags & IFBIF_BLOCKNONIP &&
		    bridge_blocknonip(eh, m))
			continue;

		if (bridge_filterrule(&p->bif_brlout, eh, m) == BRL_ACTION_BLOCK)
			continue;

		bridge_localbroadcast(sc, dst_if, eh, m);

		/* If last one, reuse the passed-in mbuf */
		if (TAILQ_NEXT(p, next) == NULL) {
			mc = m;
			used = 1;
		} else {
			struct mbuf *m1, *m2, *mx;

			m1 = m_copym2(m, 0, ETHER_HDR_LEN,
			    M_DONTWAIT);
			if (m1 == NULL) {
				sc->sc_if.if_oerrors++;
				continue;
			}
			m2 = m_copym2(m, ETHER_HDR_LEN,
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

		mc = bridge_ip(sc, BRIDGE_OUT, dst_if, eh, mc);
		if (mc == NULL)
			continue;

		len = mc->m_pkthdr.len;
#if NVLAN > 0
		if ((mc->m_flags & M_VLANTAG) &&
		    (dst_if->if_capabilities & IFCAP_VLAN_HWTAGGING) == 0)
			len += ETHER_VLAN_ENCAP_LEN;
#endif
		if ((len - ETHER_HDR_LEN) > dst_if->if_mtu)
			bridge_fragment(sc, dst_if, eh, mc);
		else {
			s = splnet();
			bridge_ifenqueue(sc, dst_if, mc);
			splx(s);
		}
	}

	if (!used)
		m_freem(m);
}

void
bridge_localbroadcast(struct bridge_softc *sc, struct ifnet *ifp,
    struct ether_header *eh, struct mbuf *m)
{
	struct mbuf *m1;
	u_int16_t etype;

	/*
	 * quick optimisation, don't send packets up the stack if no
	 * corresponding address has been specified.
	 */
	etype = ntohs(eh->ether_type);
	if (!(m->m_flags & M_VLANTAG) && etype == ETHERTYPE_IP) {
		struct ifaddr *ifa;
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == AF_INET)
				break;
		}
		if (ifa == NULL)
			return;
	}

	m1 = m_copym2(m, 0, M_COPYALL, M_DONTWAIT);
	if (m1 == NULL) {
		sc->sc_if.if_oerrors++;
		return;
	}
	/* fixup header a bit */
	m1->m_pkthdr.rcvif = ifp;
	m1->m_pkthdr.ph_rtableid = ifp->if_rdomain;
	m1->m_flags |= M_PROTO1;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m1,
		    BPF_DIRECTION_IN);
#endif

	ether_input(ifp, NULL, m1);
	ifp->if_ipackets++;
}

void
bridge_span(struct bridge_softc *sc, struct ether_header *eh,
    struct mbuf *morig)
{
	struct bridge_iflist *p;
	struct ifnet *ifp;
	struct mbuf *mc, *m;
	int s, error;

	if (TAILQ_EMPTY(&sc->sc_spanlist))
		return;

	m = m_copym2(morig, 0, M_COPYALL, M_NOWAIT);
	if (m == NULL)
		return;
	if (eh != NULL) {
		M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
		if (m == NULL)
			return;
		bcopy(eh, mtod(m, caddr_t), ETHER_HDR_LEN);
	}

	TAILQ_FOREACH(p, &sc->sc_spanlist, next) {
		ifp = p->ifp;

		if ((ifp->if_flags & IFF_RUNNING) == 0)
			continue;

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

		s = splnet();
		error = bridge_ifenqueue(sc, ifp, mc);
		splx(s);
		if (error)
			continue;
	}
	m_freem(m);
}

struct ifnet *
bridge_rtupdate(struct bridge_softc *sc, struct ether_addr *ea,
    struct ifnet *ifp, int setflags, u_int8_t flags, struct mbuf *m)
{
	struct bridge_rtnode *p, *q;
	struct sockaddr *sa = NULL;
	u_int32_t h;
	int dir;

	if (m != NULL) {
		/* Check if the mbuf was tagged with a tunnel endpoint addr */
		sa = bridge_tunnel(m);
	}

	h = bridge_hash(sc, ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	if (p == NULL) {
		if (sc->sc_brtcnt >= sc->sc_brtmax)
			goto done;
		p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT);
		if (p == NULL)
			goto done;

		bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
		p->brt_if = ifp;
		p->brt_age = 1;
		bridge_copyaddr(sa, (struct sockaddr *)&p->brt_tunnel);

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
			} else if (!(q->brt_flags & IFBAF_STATIC))
				q->brt_if = ifp;

			if (q->brt_if == ifp)
				q->brt_age = 1;
			ifp = q->brt_if;
			bridge_copyaddr(sa,
			     (struct sockaddr *)&q->brt_tunnel);

			goto want;
		}

		if (dir > 0) {
			if (sc->sc_brtcnt >= sc->sc_brtmax)
				goto done;
			p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT);
			if (p == NULL)
				goto done;

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;
			bridge_copyaddr(sa,
			    (struct sockaddr *)&p->brt_tunnel);

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
			p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT);
			if (p == NULL)
				goto done;

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;
			bridge_copyaddr(sa,
			    (struct sockaddr *)&p->brt_tunnel);

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
	return (ifp);
}

struct bridge_rtnode *
bridge_rtlookup(struct bridge_softc *sc, struct ether_addr *ea)
{
	struct bridge_rtnode *p;
	u_int32_t h;
	int dir;

	h = bridge_hash(sc, ea);
	LIST_FOREACH(p, &sc->sc_rts[h], brt_next) {
		dir = memcmp(ea, &p->brt_addr, sizeof(p->brt_addr));
		if (dir == 0)
			return (p);
		if (dir > 0)
			goto fail;
	}
fail:
	return (NULL);
}

u_int32_t
bridge_hash(struct bridge_softc *sc, struct ether_addr *addr)
{
	return SipHash24((SIPHASH_KEY *)sc->sc_hashkey, addr, ETHER_ADDR_LEN) &
	    BRIDGE_RTABLE_MASK;
}

void
bridge_timer(void *vsc)
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
bridge_rtage(struct bridge_softc *sc)
{
	struct bridge_rtnode *n, *p;
	int i;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
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
				free(n, M_DEVBUF, 0);
				n = p;
			}
		}
	}

	if (sc->sc_brttimeout != 0)
		timeout_add_sec(&sc->sc_brtimeout, sc->sc_brttimeout);
}

void
bridge_rtagenode(struct ifnet *ifp, int age)
{
	struct bridge_softc *sc;
	struct bridge_rtnode *n;
	int i;

	sc = ((struct bridge_iflist *)ifp->if_bridgeport)->bridge_sc;
	if (sc == NULL)
		return;

	/*
	 * If the age is zero then flush, otherwise set all the expiry times to
	 * age for the interface
	 */
	if (age == 0)
		bridge_rtdelete(sc, ifp, 1);
	else {
		for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
			LIST_FOREACH(n, &sc->sc_rts[i], brt_next) {
				/* Cap the expiry time to 'age' */
				if (n->brt_if == ifp &&
				    n->brt_age > time_uptime + age &&
				    (n->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
					n->brt_age = time_uptime + age;
			}
		}
	}
}



/*
 * Remove all dynamic addresses from the cache
 */
void
bridge_rtflush(struct bridge_softc *sc, int full)
{
	int i;
	struct bridge_rtnode *p, *n;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (full ||
			    (n->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF, 0);
				n = p;
			} else
				n = LIST_NEXT(n, brt_next);
		}
	}
}

/*
 * Remove an address from the cache
 */
int
bridge_rtdaddr(struct bridge_softc *sc, struct ether_addr *ea)
{
	int h;
	struct bridge_rtnode *p;

	h = bridge_hash(sc, ea);
	LIST_FOREACH(p, &sc->sc_rts[h], brt_next) {
		if (bcmp(ea, &p->brt_addr, sizeof(p->brt_addr)) == 0) {
			LIST_REMOVE(p, brt_next);
			sc->sc_brtcnt--;
			free(p, M_DEVBUF, 0);
			return (0);
		}
	}

	return (ENOENT);
}
/*
 * Delete routes to a specific interface member.
 */
void
bridge_rtdelete(struct bridge_softc *sc, struct ifnet *ifp, int dynonly)
{
	int i;
	struct bridge_rtnode *n, *p;

	/*
	 * Loop through all of the hash buckets and traverse each
	 * chain looking for routes to this interface.
	 */
	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
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
			free(n, M_DEVBUF, 0);
			n = p;
		}
	}
}

/*
 * Gather all of the routes for this interface.
 */
int
bridge_rtfind(struct bridge_softc *sc, struct ifbaconf *baconf)
{
	int i, error = 0, onlycnt = 0;
	u_int32_t cnt = 0;
	struct bridge_rtnode *n;
	struct ifbareq bareq;

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
				bridge_copyaddr(&n->brt_tunnel.sa,
				    (struct sockaddr *)&bareq.ifba_dstsa);
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
bridge_blocknonip(struct ether_header *eh, struct mbuf *m)
{
	struct llc llc;
	u_int16_t etype;

	if (m->m_pkthdr.len < ETHER_HDR_LEN)
		return (1);

#if NVLAN > 0
	if (m->m_flags & M_VLANTAG)
		return (1);
#endif

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
	    (ETHER_HDR_LEN + LLC_SNAPFRAMELEN))
		return (1);

	m_copydata(m, ETHER_HDR_LEN, LLC_SNAPFRAMELEN,
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
bridge_filterrule(struct brl_head *h, struct ether_header *eh, struct mbuf *m)
{
	struct brl_node *n;
	u_int8_t flags;

	SIMPLEQ_FOREACH(n, h, brl_next) {
		flags = n->brl_flags & (BRL_FLAG_SRCVALID|BRL_FLAG_DSTVALID);
		if (flags == 0)
			goto return_action;
		if (flags == (BRL_FLAG_SRCVALID|BRL_FLAG_DSTVALID)) {
			if (bcmp(eh->ether_shost, &n->brl_src, ETHER_ADDR_LEN))
				continue;
			if (bcmp(eh->ether_dhost, &n->brl_dst, ETHER_ADDR_LEN))
				continue;
			goto return_action;
		}
		if (flags == BRL_FLAG_SRCVALID) {
			if (bcmp(eh->ether_shost, &n->brl_src, ETHER_ADDR_LEN))
				continue;
			goto return_action;
		}
		if (flags == BRL_FLAG_DSTVALID) {
			if (bcmp(eh->ether_dhost, &n->brl_dst, ETHER_ADDR_LEN))
				continue;
			goto return_action;
		}
	}
	return (BRL_ACTION_PASS);

return_action:
#if NPF > 0
	pf_tag_packet(m, n->brl_tag, -1);
#endif
	return (n->brl_action);
}

int
bridge_addrule(struct bridge_iflist *bif, struct ifbrlreq *req, int out)
{
	struct brl_node *n;

	n = malloc(sizeof(*n), M_DEVBUF, M_NOWAIT);
	if (n == NULL)
		return (ENOMEM);
	bcopy(&req->ifbr_src, &n->brl_src, sizeof(struct ether_addr));
	bcopy(&req->ifbr_dst, &n->brl_dst, sizeof(struct ether_addr));
	n->brl_action = req->ifbr_action;
	n->brl_flags = req->ifbr_flags;
#if NPF > 0
	if (req->ifbr_tagname[0])
		n->brl_tag = pf_tagname2tag(req->ifbr_tagname, 1);
	else
		n->brl_tag = 0;
#endif
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

void
bridge_flushrule(struct bridge_iflist *bif)
{
	struct brl_node *p;

	while (!SIMPLEQ_EMPTY(&bif->bif_brlin)) {
		p = SIMPLEQ_FIRST(&bif->bif_brlin);
		SIMPLEQ_REMOVE_HEAD(&bif->bif_brlin, brl_next);
#if NPF > 0
		pf_tag_unref(p->brl_tag);
#endif
		free(p, M_DEVBUF, 0);
	}
	while (!SIMPLEQ_EMPTY(&bif->bif_brlout)) {
		p = SIMPLEQ_FIRST(&bif->bif_brlout);
		SIMPLEQ_REMOVE_HEAD(&bif->bif_brlout, brl_next);
#if NPF > 0
		pf_tag_unref(p->brl_tag);
#endif
		free(p, M_DEVBUF, 0);
	}
}

#ifdef IPSEC
int
bridge_ipsec(struct bridge_softc *sc, struct ifnet *ifp,
    struct ether_header *eh, int hassnap, struct llc *llc,
    int dir, int af, int hlen, struct mbuf *m)
{
	union sockaddr_union dst;
	struct timeval tv;
	struct tdb *tdb;
	u_int32_t spi;
	u_int16_t cpi;
	int error, off, s;
	u_int8_t proto = 0;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif /* INET6 */
#if NPF > 0
	struct ifnet *encif;
#endif

	if (dir == BRIDGE_IN) {
		switch (af) {
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

		s = splsoftnet();

		tdb = gettdb(ifp->if_rdomain, spi, &dst, proto);
		if (tdb != NULL && (tdb->tdb_flags & TDBF_INVALID) == 0 &&
		    tdb->tdb_xform != NULL) {
			if (tdb->tdb_first_use == 0) {
				tdb->tdb_first_use = time_second;

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
			splx(s);
			return (1);
		} else {
			splx(s);
 skiplookup:
			/* XXX do an input policy lookup */
			return (0);
		}
	} else { /* Outgoing from the bridge. */
		tdb = ipsp_spd_lookup(m, af, hlen, &error,
		    IPSP_DIRECTION_OUT, NULL, NULL, 0);
		if (tdb != NULL) {
			/*
			 * We don't need to do loop detection, the
			 * bridge will do that for us.
			 */
#if NPF > 0
			if ((encif = enc_getif(tdb->tdb_rdomain,
			    tdb->tdb_tap)) == NULL ||
			    pf_test(af, dir, encif,
			    &m, NULL) != PF_PASS) {
				m_freem(m);
				return (1);
			}
			if (m == NULL)
				return (1);
			else if (af == AF_INET)
				in_proto_cksum_out(m, encif);
#ifdef INET6
			else if (af == AF_INET6)
				in6_proto_cksum_out(m, encif);
#endif /* INET6 */
#endif /* NPF */

			ip = mtod(m, struct ip *);
			if ((af == AF_INET) &&
			    ip_mtudisc && (ip->ip_off & htons(IP_DF)) &&
			    tdb->tdb_mtu && ntohs(ip->ip_len) > tdb->tdb_mtu &&
			    tdb->tdb_mtutimeout > time_second)
				bridge_send_icmp_err(sc, ifp, eh, m,
				    hassnap, llc, tdb->tdb_mtu,
				    ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG);
			else
				error = ipsp_process_packet(m, tdb, af, 0);
			return (1);
		} else
			return (0);
	}

	return (0);
}
#endif /* IPSEC */

/*
 * Filter IP packets by peeking into the ethernet frame.  This violates
 * the ISO model, but allows us to act as a IP filter at the data link
 * layer.  As a result, most of this code will look familiar to those
 * who've read net/if_ethersubr.c and netinet/ip_input.c
 */
struct mbuf *
bridge_ip(struct bridge_softc *sc, int dir, struct ifnet *ifp,
    struct ether_header *eh, struct mbuf *m)
{
	struct llc llc;
	int hassnap = 0;
	struct ip *ip;
	int hlen;
	u_int16_t etype;

#if NVLAN > 0
	if (m->m_flags & M_VLANTAG)
		return (m);
#endif

	etype = ntohs(eh->ether_type);

	if (etype != ETHERTYPE_IP && etype != ETHERTYPE_IPV6) {
		if (etype > ETHERMTU ||
		    m->m_pkthdr.len < (LLC_SNAPFRAMELEN +
		    ETHER_HDR_LEN))
			return (m);

		m_copydata(m, ETHER_HDR_LEN,
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

	m_adj(m, ETHER_HDR_LEN);
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

		if ((m->m_pkthdr.csum_flags & M_IPV4_CSUM_IN_OK) == 0) {
			if (m->m_pkthdr.csum_flags & M_IPV4_CSUM_IN_BAD) {
				ipstat.ips_badsum++;
				goto dropit;
			}

			ipstat.ips_inswcsum++;
			if (in_cksum(m, hlen) != 0) {
				ipstat.ips_badsum++;
				goto dropit;
			}
		}

		if (ntohs(ip->ip_len) < hlen)
			goto dropit;

		if (m->m_pkthdr.len < ntohs(ip->ip_len))
			goto dropit;
		if (m->m_pkthdr.len > ntohs(ip->ip_len)) {
			if (m->m_len == m->m_pkthdr.len) {
				m->m_len = ntohs(ip->ip_len);
				m->m_pkthdr.len = ntohs(ip->ip_len);
			} else
				m_adj(m, ntohs(ip->ip_len) - m->m_pkthdr.len);
		}

#ifdef IPSEC
		if ((sc->sc_if.if_flags & IFF_LINK2) == IFF_LINK2 &&
		    bridge_ipsec(sc, ifp, eh, hassnap, &llc,
		    dir, AF_INET, hlen, m))
			return (NULL);
#endif /* IPSEC */
#if NPF > 0
		/* Finally, we get to filter the packet! */
		if (pf_test(AF_INET, dir, ifp, &m, eh) != PF_PASS)
			goto dropit;
		if (m == NULL)
			goto dropit;
#endif /* NPF > 0 */

		/* Rebuild the IP header */
		if (m->m_len < hlen && ((m = m_pullup(m, hlen)) == NULL))
			return (NULL);
		if (m->m_len < sizeof(struct ip))
			goto dropit;
		in_proto_cksum_out(m, ifp);
		ip = mtod(m, struct ip *);
		ip->ip_sum = 0;
		if (0 && (ifp->if_capabilities & IFCAP_CSUM_IPv4))
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
		else {
			ipstat.ips_outswcsum++;
			ip->ip_sum = in_cksum(m, hlen);
		}

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
		    bridge_ipsec(sc, ifp, eh, hassnap, &llc,
		    dir, AF_INET6, hlen, m))
			return (NULL);
#endif /* IPSEC */

#if NPF > 0
		if (pf_test(AF_INET6, dir, ifp, &m, eh) != PF_PASS)
			goto dropit;
		if (m == NULL)
			return (NULL);
#endif /* NPF > 0 */
		in6_proto_cksum_out(m, ifp);

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

void
bridge_fragment(struct bridge_softc *sc, struct ifnet *ifp,
    struct ether_header *eh, struct mbuf *m)
{
	struct llc llc;
	struct mbuf *m0;
	int s, error = 0;
	int hassnap = 0;
	u_int16_t etype;
	struct ip *ip;

	etype = ntohs(eh->ether_type);
#if NVLAN > 0
	if ((m->m_flags & M_VLANTAG) || etype == ETHERTYPE_VLAN ||
	    etype == ETHERTYPE_QINQ) {
		int len = m->m_pkthdr.len;

		if (m->m_flags & M_VLANTAG)
			len += ETHER_VLAN_ENCAP_LEN;
		if ((ifp->if_capabilities & IFCAP_VLAN_MTU) &&
		    (len - sizeof(struct ether_vlan_header) <= ifp->if_mtu)) {
			s = splnet();
			bridge_ifenqueue(sc, ifp, m);
			splx(s);
			return;
		}
		goto dropit;
	}
#endif
	if (etype != ETHERTYPE_IP) {
		if (etype > ETHERMTU ||
		    m->m_pkthdr.len < (LLC_SNAPFRAMELEN +
		    ETHER_HDR_LEN))
			goto dropit;

		m_copydata(m, ETHER_HDR_LEN,
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

	m_adj(m, ETHER_HDR_LEN);
	if (hassnap)
		m_adj(m, LLC_SNAPFRAMELEN);

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		goto dropit;
	ip = mtod(m, struct ip *);

	/* Respect IP_DF, return a ICMP_UNREACH_NEEDFRAG. */
	if (ip->ip_off & htons(IP_DF)) {
		bridge_send_icmp_err(sc, ifp, eh, m, hassnap, &llc,
		    ifp->if_mtu, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG);
		return;
	}

	error = ip_fragment(m, ifp, ifp->if_mtu);
	if (error) {
		m = NULL;
		goto dropit;
	}

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
			bcopy(eh, mtod(m, caddr_t), sizeof(*eh));
			s = splnet();
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
 dropit:
	if (m != NULL)
		m_freem(m);
}

#if NVLAN > 0
extern int vlan_output(struct ifnet *, struct mbuf *, struct sockaddr *,
    struct rtentry *);
#endif

int
bridge_ifenqueue(struct bridge_softc *sc, struct ifnet *ifp, struct mbuf *m)
{
	int error, len;
	short mflags;

#if NGIF > 0
	/* Packet needs etherip encapsulation. */
	if (ifp->if_type == IFT_GIF) {
		m->m_flags |= M_PROTO1;

		/* Count packets input into the gif from outside */
		ifp->if_ipackets++;
		ifp->if_ibytes += m->m_pkthdr.len;
	}
#endif
#if NVLAN > 0
	/*
	 * If the underlying interface cannot do VLAN tag insertion itself,
	 * create an encapsulation header.
	 */
	if (ifp->if_output == vlan_output) {
		struct ifvlan	*ifv = ifp->if_softc;
		struct ifnet	*p = ifv->ifv_p;
		u_int8_t        prio = m->m_pkthdr.pf.prio;

		/* IEEE 802.1p has prio 0 and 1 swapped */
		if (prio <= 1)
			prio = !prio;

		/* should we use the tx tagging hw offload at all? */
		if ((p->if_capabilities & IFCAP_VLAN_HWTAGGING) &&
		    (ifv->ifv_type == ETHERTYPE_VLAN)) {
			m->m_pkthdr.ether_vtag = ifv->ifv_tag +
			    (prio << EVL_PRIO_BITS);
			m->m_flags |= M_VLANTAG;
		} else {
			struct ether_vlan_header evh;

			m_copydata(m, 0, ETHER_HDR_LEN, (caddr_t)&evh);
			evh.evl_proto = evh.evl_encap_proto;
			evh.evl_encap_proto = htons(ifv->ifv_type);
			evh.evl_tag = htons(ifv->ifv_tag +
			    (prio << EVL_PRIO_BITS));
			m_adj(m, ETHER_HDR_LEN);
			M_PREPEND(m, sizeof(evh), M_DONTWAIT);
			if (m == NULL) {
				sc->sc_if.if_oerrors++;
				return (ENOBUFS);
			}
			m_copyback(m, 0, sizeof(evh), &evh, M_NOWAIT);
			m->m_flags &= ~M_VLANTAG;
		}
	}
#endif
	len = m->m_pkthdr.len;
	mflags = m->m_flags;
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error) {
		sc->sc_if.if_oerrors++;
		return (error);
	}
	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += len;
	ifp->if_obytes += len;
	if (mflags & M_MCAST)
		ifp->if_omcasts++;
	if_start(ifp);

	return (0);
}

void
bridge_send_icmp_err(struct bridge_softc *sc, struct ifnet *ifp,
    struct ether_header *eh, struct mbuf *n, int hassnap, struct llc *llc,
    int mtu, int type, int code)
{
	struct ip *ip;
	struct icmp *icp;
	struct in_addr t;
	struct mbuf *m, *n2;
	int hlen;
	u_int8_t ether_tmp[ETHER_ADDR_LEN];

	n2 = m_copym(n, 0, M_COPYALL, M_DONTWAIT);
	if (!n2) {
		m_freem(n);
		return;
	}
	m = icmp_do_error(n, type, code, 0, mtu);
	if (m == NULL) {
		m_freem(n2);
		return;
	}

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
	icp->icmp_cksum = in_cksum(m, ntohs(ip->ip_len) - hlen);
	m->m_data -= hlen;
	m->m_len += hlen;

	ip->ip_v = IPVERSION;
	ip->ip_off &= htons(IP_DF);
	ip->ip_id = htons(ip_randomid());
	ip->ip_ttl = MAXTTL;
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

struct sockaddr *
bridge_tunnel(struct mbuf *m)
{
	struct m_tag    *mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_TUNNEL, NULL)) == NULL)
		return (NULL);

	return ((struct sockaddr *)(mtag + 1));
}

struct sockaddr *
bridge_tunneltag(struct mbuf *m, int af)
{
	struct m_tag    *mtag;
	size_t		 len;
	struct sockaddr	*sa;

	if ((mtag = m_tag_find(m, PACKET_TAG_TUNNEL, NULL)) != NULL) {
		sa = (struct sockaddr *)(mtag + 1);
		if (sa->sa_family != af) {
			m_tag_delete(m, mtag);
			mtag = NULL;
		}
	}
	if (mtag == NULL) {
		if (af == AF_INET)
			len = sizeof(struct sockaddr_in);
		else if (af == AF_INET6)
			len = sizeof(struct sockaddr_in6);
		else
			return (NULL);
		mtag = m_tag_get(PACKET_TAG_TUNNEL, len, M_NOWAIT);
		if (mtag == NULL)
			return (NULL);
		bzero(mtag + 1, len);
		sa = (struct sockaddr *)(mtag + 1);
		sa->sa_family = af;
		sa->sa_len = len;
		m_tag_prepend(m, mtag);
	}

	return ((struct sockaddr *)(mtag + 1));
}

void
bridge_tunneluntag(struct mbuf *m)
{
	struct m_tag    *mtag;
	if ((mtag = m_tag_find(m, PACKET_TAG_TUNNEL, NULL)) != NULL)
		m_tag_delete(m, mtag);
}

void
bridge_copyaddr(struct sockaddr *src, struct sockaddr *dst)
{
	if (src != NULL && src->sa_family != AF_UNSPEC)
		memcpy(dst, src, src->sa_len);
	else
		dst->sa_family = AF_UNSPEC;
}
