/*	$OpenBSD: if_bridge.c,v 1.315 2018/12/12 14:19:15 mpi Exp $	*/

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
#include "mpw.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>

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
void	bridge_ifdetach(void *);
void	bridge_spandetach(void *);
int	bridge_input(struct ifnet *, struct mbuf *, void *);
void	bridge_process(struct ifnet *, struct mbuf *);
void	bridgeintr_frame(struct bridge_softc *, struct ifnet *, struct mbuf *);
void	bridge_broadcast(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *);
int	bridge_localbroadcast(struct ifnet *, struct ether_header *,
    struct mbuf *);
void	bridge_span(struct bridge_softc *, struct mbuf *);
void	bridge_stop(struct bridge_softc *);
void	bridge_init(struct bridge_softc *);
int	bridge_bifconf(struct bridge_softc *, struct ifbifconf *);
int bridge_blocknonip(struct ether_header *, struct mbuf *);
void	bridge_ifinput(struct ifnet *, struct mbuf *);
int	bridge_dummy_output(struct ifnet *, struct mbuf *, struct sockaddr *,
    struct rtentry *);
void	bridge_send_icmp_err(struct ifnet *, struct ether_header *,
    struct mbuf *, int, struct llc *, int, int, int);
int	bridge_ifenqueue(struct ifnet *, struct ifnet *, struct mbuf *);
struct mbuf *bridge_ip(struct ifnet *, int, struct ifnet *,
    struct ether_header *, struct mbuf *);
#ifdef IPSEC
int bridge_ipsec(struct ifnet *, struct ether_header *, int, struct llc *,
    int, int, int, struct mbuf *);
#endif
int     bridge_clone_create(struct if_clone *, int);
int	bridge_clone_destroy(struct ifnet *);
int	bridge_delete(struct bridge_softc *, struct bridge_iflist *);

#define	ETHERADDR_IS_IP_MCAST(a) \
	/* struct etheraddr *a;	*/				\
	((a)->ether_addr_octet[0] == 0x01 &&			\
	 (a)->ether_addr_octet[1] == 0x00 &&			\
	 (a)->ether_addr_octet[2] == 0x5e)

struct niqueue bridgeintrq = NIQUEUE_INITIALIZER(1024, NETISR_BRIDGE);

struct if_clone bridge_cloner =
    IF_CLONE_INITIALIZER("bridge", bridge_clone_create, bridge_clone_destroy);

void
bridgeattach(int n)
{
	if_clone_attach(&bridge_cloner);
}

int
bridge_clone_create(struct if_clone *ifc, int unit)
{
	struct bridge_softc *sc;
	struct ifnet *ifp;
	int i;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	sc->sc_stp = bstp_create(&sc->sc_if);
	if (!sc->sc_stp) {
		free(sc, M_DEVBUF, sizeof *sc);
		return (ENOMEM);
	}

	sc->sc_brtmax = BRIDGE_RTABLE_MAX;
	sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;
	timeout_set(&sc->sc_brtimeout, bridge_rtage, sc);
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
	ifp->if_output = bridge_dummy_output;
	ifp->if_xflags = IFXF_CLONED;
	ifp->if_start = NULL;
	ifp->if_type = IFT_BRIDGE;
	ifp->if_hdrlen = ETHER_HDR_LEN;

	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&sc->sc_if.if_bpf, ifp,
	    DLT_EN10MB, ETHER_HDR_LEN);
#endif

	if_ih_insert(ifp, ether_input, NULL);

	return (0);
}

int
bridge_dummy_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	m_freem(m);
	return (EAFNOSUPPORT);
}

int
bridge_clone_destroy(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct bridge_iflist *bif;

	bridge_stop(sc);
	bridge_rtflush(sc, IFBF_FLUSHALL);
	while ((bif = TAILQ_FIRST(&sc->sc_iflist)) != NULL)
		bridge_delete(sc, bif);
	while ((bif = TAILQ_FIRST(&sc->sc_spanlist)) != NULL)
		bridge_spandetach(bif);

	bstp_destroy(sc->sc_stp);

	/* Undo pseudo-driver changes. */
	if_deactivate(ifp);

	if_ih_remove(ifp, ether_input, NULL);

	KASSERT(SRPL_EMPTY_LOCKED(&ifp->if_inputs));

	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof *sc);
	return (0);
}

int
bridge_delete(struct bridge_softc *sc, struct bridge_iflist *bif)
{
	int error;

	if (bif->bif_flags & IFBIF_STP)
		bstp_delete(bif->bif_stp);

	bif->ifp->if_bridgeport = NULL;
	error = ifpromisc(bif->ifp, 0);
	hook_disestablish(bif->ifp->if_detachhooks, bif->bif_dhcookie);

	if_ih_remove(bif->ifp, bridge_input, NULL);
	TAILQ_REMOVE(&sc->sc_iflist, bif, next);
	bridge_rtdelete(sc, bif->ifp, 0);
	bridge_flushrule(bif);
	free(bif, M_DEVBUF, sizeof *bif);

	return (error);
}

int
bridge_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bridge_softc *sc = (struct bridge_softc *)ifp->if_softc;
	struct ifbreq *req = (struct ifbreq *)data;
	struct ifbropreq *brop = (struct ifbropreq *)data;
	struct ifnet *ifs;
	struct bridge_iflist *bif;
	struct bstp_port *bp;
	struct bstp_state *bs = sc->sc_stp;
	int error = 0;

	switch (cmd) {
	case SIOCBRDGADD:
	/* bridge(4) does not distinguish between routing/forwarding ports */
	case SIOCBRDGADDL:
		if ((error = suser(curproc)) != 0)
			break;

		ifs = ifunit(req->ifbr_ifsname);

		/* try to create the interface if it does't exist */
		if (ifs == NULL && if_clone_create(req->ifbr_ifsname, 0) == 0)
			ifs = ifunit(req->ifbr_ifsname);

		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}

		if (ifs->if_bridgeport != NULL) {
			bif = (struct bridge_iflist *)ifs->if_bridgeport;
			if (bif->bridge_sc == sc)
				error = EEXIST;
			else
				error = EBUSY;
			break;
		}

		/* If it's in the span list, it can't be a member. */
		TAILQ_FOREACH(bif, &sc->sc_spanlist, next) {
			if (bif->ifp == ifs)
				break;
		}
		if (bif != NULL) {
			error = EBUSY;
			break;
		}

		if (ifs->if_type == IFT_ETHER) {
			error = ifpromisc(ifs, 1);
			if (error != 0)
				break;
		}
#if NMPW > 0
		else if (ifs->if_type == IFT_MPLSTUNNEL) {
			/* Nothing needed */
		}
#endif /* NMPW */
		else {
			error = EINVAL;
			break;
		}

		bif = malloc(sizeof(*bif), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (bif == NULL) {
			if (ifs->if_type == IFT_ETHER)
				ifpromisc(ifs, 0);
			error = ENOMEM;
			break;
		}

		bif->bridge_sc = sc;
		bif->ifp = ifs;
		bif->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
		SIMPLEQ_INIT(&bif->bif_brlin);
		SIMPLEQ_INIT(&bif->bif_brlout);
		ifs->if_bridgeport = (caddr_t)bif;
		bif->bif_dhcookie = hook_establish(ifs->if_detachhooks, 0,
		    bridge_ifdetach, bif);
		if_ih_insert(bif->ifp, bridge_input, NULL);
		TAILQ_INSERT_TAIL(&sc->sc_iflist, bif, next);
		break;
	case SIOCBRDGDEL:
		if ((error = suser(curproc)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		bif = (struct bridge_iflist *)ifs->if_bridgeport;
		if (bif == NULL || bif->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		error = bridge_delete(sc, bif);
		break;
	case SIOCBRDGIFS:
		error = bridge_bifconf(sc, (struct ifbifconf *)data);
		break;
	case SIOCBRDGADDS:
		if ((error = suser(curproc)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}
		if (ifs->if_type != IFT_ETHER &&
		    ifs->if_type != IFT_MPLSTUNNEL) {
			error = EINVAL;
			break;
		}
		if (ifs->if_bridgeport != NULL) {
			error = EBUSY;
			break;
		}
		TAILQ_FOREACH(bif, &sc->sc_spanlist, next) {
			if (bif->ifp == ifs)
				break;
		}
		if (bif != NULL) {
			error = EEXIST;
			break;
		}
		bif = malloc(sizeof(*bif), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (bif == NULL) {
			error = ENOMEM;
			break;
		}
		bif->ifp = ifs;
		bif->bif_flags = IFBIF_SPAN;
		bif->bridge_sc = sc;
		bif->bif_dhcookie = hook_establish(ifs->if_detachhooks, 0,
		    bridge_spandetach, bif);
		SIMPLEQ_INIT(&bif->bif_brlin);
		SIMPLEQ_INIT(&bif->bif_brlout);
		TAILQ_INSERT_TAIL(&sc->sc_spanlist, bif, next);
		break;
	case SIOCBRDGDELS:
		if ((error = suser(curproc)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		TAILQ_FOREACH(bif, &sc->sc_spanlist, next) {
			if (bif->ifp == ifs)
				break;
		}
		if (bif == NULL) {
			error = ENOENT;
			break;
		}
		bridge_spandetach(bif);
		break;
	case SIOCBRDGGIFFLGS:
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		bif = (struct bridge_iflist *)ifs->if_bridgeport;
		if (bif == NULL || bif->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		req->ifbr_ifsflags = bif->bif_flags;
		req->ifbr_portno = bif->ifp->if_index & 0xfff;
		req->ifbr_protected = bif->bif_protected;
		if (bif->bif_flags & IFBIF_STP) {
			bp = bif->bif_stp;
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
		if ((error = suser(curproc)) != 0)
			break;
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		bif = (struct bridge_iflist *)ifs->if_bridgeport;
		if (bif == NULL || bif->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		if (req->ifbr_ifsflags & IFBIF_RO_MASK) {
			error = EINVAL;
			break;
		}
		if (req->ifbr_ifsflags & IFBIF_STP) {
			if ((bif->bif_flags & IFBIF_STP) == 0) {
				/* Enable STP */
				if ((bif->bif_stp = bstp_add(sc->sc_stp,
				    bif->ifp)) == NULL) {
					error = ENOMEM;
					break;
				}
			} else {
				/* Update STP flags */
				bstp_ifsflags(bif->bif_stp, req->ifbr_ifsflags);
			}
		} else if (bif->bif_flags & IFBIF_STP) {
			bstp_delete(bif->bif_stp);
			bif->bif_stp = NULL;
		}
		bif->bif_flags = req->ifbr_ifsflags;
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == IFF_UP)
			bridge_init(sc);

		if ((ifp->if_flags & IFF_UP) == 0)
			bridge_stop(sc);

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
	case SIOCBRDGSIFPROT:
		ifs = ifunit(req->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		bif = (struct bridge_iflist *)ifs->if_bridgeport;
		if (bif == NULL || bif->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		bif->bif_protected = req->ifbr_protected;
		break;
	case SIOCBRDGRTS:
	case SIOCBRDGGCACHE:
	case SIOCBRDGGPRI:
	case SIOCBRDGGMA:
	case SIOCBRDGGHT:
	case SIOCBRDGGFD:
	case SIOCBRDGGTO:
	case SIOCBRDGGRL:
		break;
	case SIOCBRDGFLUSH:
	case SIOCBRDGSADDR:
	case SIOCBRDGDADDR:
	case SIOCBRDGSCACHE:
	case SIOCBRDGSTO:
	case SIOCBRDGARL:
	case SIOCBRDGFRL:
	case SIOCBRDGSPRI:
	case SIOCBRDGSFD:
	case SIOCBRDGSMA:
	case SIOCBRDGSHT:
	case SIOCBRDGSTXHC:
	case SIOCBRDGSPROTO:
	case SIOCBRDGSIFPRIO:
	case SIOCBRDGSIFCOST:
		error = suser(curproc);
		break;
	default:
		error = ENOTTY;
		break;
	}

	if (!error)
		error = bridgectl_ioctl(ifp, cmd, data);

	if (!error)
		error = bstp_ioctl(ifp, cmd, data);

	return (error);
}

/* Detach an interface from a bridge.  */
void
bridge_ifdetach(void *xbif)
{
	struct bridge_iflist *bif = xbif;
	struct bridge_softc *sc = bif->bridge_sc;

	bridge_delete(sc, bif);
}

void
bridge_spandetach(void *xbif)
{
	struct bridge_iflist *bif = xbif;
	struct bridge_softc *sc = bif->bridge_sc;

	hook_disestablish(bif->ifp->if_detachhooks, bif->bif_dhcookie);
	TAILQ_REMOVE(&sc->sc_spanlist, bif, next);
	free(bif, M_DEVBUF, sizeof(*bif));
}

int
bridge_bifconf(struct bridge_softc *sc, struct ifbifconf *bifc)
{
	struct bridge_iflist *bif;
	struct bstp_port *bp;
	struct bstp_state *bs = sc->sc_stp;
	u_int32_t total = 0, i = 0;
	int error = 0;
	struct ifbreq *breq, *breqs = NULL;

	TAILQ_FOREACH(bif, &sc->sc_iflist, next)
		total++;

	TAILQ_FOREACH(bif, &sc->sc_spanlist, next)
		total++;

	if (bifc->ifbic_len == 0) {
		i = total;
		goto done;
	}

	breqs = mallocarray(total, sizeof(*breqs), M_TEMP, M_NOWAIT|M_ZERO);
	if (breqs == NULL)
		goto done;

	TAILQ_FOREACH(bif, &sc->sc_iflist, next) {
		if (bifc->ifbic_len < (i + 1) * sizeof(*breqs))
			break;
		breq = &breqs[i];
		strlcpy(breq->ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(breq->ifbr_ifsname, bif->ifp->if_xname, IFNAMSIZ);
		breq->ifbr_ifsflags = bif->bif_flags;
		breq->ifbr_portno = bif->ifp->if_index & 0xfff;
		breq->ifbr_protected = bif->bif_protected;
		if (bif->bif_flags & IFBIF_STP) {
			bp = bif->bif_stp;
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
		i++;
	}
	TAILQ_FOREACH(bif, &sc->sc_spanlist, next) {
		if (bifc->ifbic_len < (i + 1) * sizeof(*breqs))
			break;
		breq = &breqs[i];
		strlcpy(breq->ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(breq->ifbr_ifsname, bif->ifp->if_xname, IFNAMSIZ);
		breq->ifbr_ifsflags = bif->bif_flags | IFBIF_SPAN;
		breq->ifbr_portno = bif->ifp->if_index & 0xfff;
		i++;
	}

	error = copyout(breqs, bifc->ifbic_req, i * sizeof(*breqs));
done:
	free(breqs, M_TEMP, total * sizeof(*breq));
	bifc->ifbic_len = i * sizeof(*breq);
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
	struct bridge_iflist *bif;
	struct ether_header *eh;
	struct ifnet *dst_if = NULL;
	struct bridge_rtnode *dst_p = NULL;
	struct ether_addr *dst;
	struct bridge_softc *sc;
	struct bridge_tunneltag *brtag;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif
	int error;

	KERNEL_ASSERT_LOCKED();

	/* ifp must be a member interface of the bridge. */
	bif = (struct bridge_iflist *)ifp->if_bridgeport;
	if (bif == NULL) {
		m_freem(m);
		return (EINVAL);
	}
	sc = bif->bridge_sc;

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
	if_bpf = sc->sc_if.if_bpf;
	if (if_bpf)
		bpf_mtap(if_bpf, m, BPF_DIRECTION_OUT);
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
		struct bridge_iflist *bif, *bif0;
		struct mbuf *mc;
		int used = 0;

		bridge_span(sc, m);

		TAILQ_FOREACH(bif, &sc->sc_iflist, next) {
			dst_if = bif->ifp;
			if ((dst_if->if_flags & IFF_RUNNING) == 0)
				continue;

			/*
			 * If this is not the original output interface,
			 * and the interface is participating in spanning
			 * tree, make sure the port is in a state that
			 * allows forwarding.
			 */
			if (dst_if != ifp &&
			    (bif->bif_flags & IFBIF_STP) &&
			    (bif->bif_state == BSTP_IFSTATE_DISCARDING))
				continue;
#if NMPW > 0
			/*
			 * Split horizon: avoid broadcasting messages from
			 * wire to another wire.
			 */
			if (ifp->if_type == IFT_MPLSTUNNEL &&
			    dst_if->if_type == IFT_MPLSTUNNEL)
				continue;
#endif /* NMPW */
			if ((bif->bif_flags & IFBIF_DISCOVER) == 0 &&
			    (m->m_flags & (M_BCAST | M_MCAST)) == 0)
				continue;

			if (TAILQ_NEXT(bif, next) == NULL) {
				used = 1;
				mc = m;
			} else {
				mc = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
				if (mc == NULL) {
					sc->sc_if.if_oerrors++;
					continue;
				}
			}

			bif0 = (struct bridge_iflist *)dst_if->if_bridgeport;
			KASSERT(bif0 != NULL);
			if (bridge_filterrule(&bif0->bif_brlout, eh, mc) ==
			    BRL_ACTION_BLOCK)
				continue;

			error = bridge_ifenqueue(&sc->sc_if, dst_if, mc);
			if (error)
				continue;
		}
		if (!used)
			m_freem(m);
		return (0);
	}

sendunicast:
	if ((dst_p != NULL) &&
	    (dst_p->brt_tunnel.brtag_peer.sa.sa_family != AF_UNSPEC) &&
	    ((brtag = bridge_tunneltag(m)) != NULL))
		bridge_copytag(&dst_p->brt_tunnel, brtag);

	bridge_span(sc, m);
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		return (ENETDOWN);
	}
	bridge_ifenqueue(&sc->sc_if, dst_if, m);
	return (0);
}

/*
 * Loop through each bridge interface and process their input queues.
 */
void
bridgeintr(void)
{
	struct mbuf_list ml;
	struct mbuf *m;
	struct ifnet *ifp;

	niq_delist(&bridgeintrq, &ml);
	if (ml_empty(&ml))
		return;

	KERNEL_LOCK();
	while ((m = ml_dequeue(&ml)) != NULL) {

		ifp = if_get(m->m_pkthdr.ph_ifidx);
		if (ifp == NULL) {
			m_freem(m);
			continue;
		}

		bridge_process(ifp, m);

		if_put(ifp);
	}
	KERNEL_UNLOCK();
}

/*
 * Process a single frame.  Frame must be freed or queued before returning.
 */
void
bridgeintr_frame(struct bridge_softc *sc, struct ifnet *src_if, struct mbuf *m)
{
	struct ifnet *dst_if;
	struct bridge_iflist *bif;
	struct bridge_rtnode *dst_p;
	struct ether_addr *dst, *src;
	struct ether_header eh;
	u_int32_t protected;
	int len;


	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	bif = (struct bridge_iflist *)src_if->if_bridgeport;
	KASSERT(bif != NULL);

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
	if ((bif->bif_flags & IFBIF_LEARNING) &&
	    (eh.ether_shost[0] & 1) == 0 &&
	    !(eh.ether_shost[0] == 0 && eh.ether_shost[1] == 0 &&
	    eh.ether_shost[2] == 0 && eh.ether_shost[3] == 0 &&
	    eh.ether_shost[4] == 0 && eh.ether_shost[5] == 0))
		bridge_rtupdate(sc, src, src_if, 0, IFBAF_DYNAMIC, m);

	if ((bif->bif_flags & IFBIF_STP) &&
	    (bif->bif_state == BSTP_IFSTATE_LEARNING)) {
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
	if (!ETHER_IS_MULTICAST(eh.ether_dhost)) {
		if ((dst_p = bridge_rtlookup(sc, dst)) != NULL)
			dst_if = dst_p->brt_if;
		else
			dst_if = NULL;
		if (dst_if == src_if) {
			m_freem(m);
			return;
		}
	} else {
		if (memcmp(etherbroadcastaddr, eh.ether_dhost,
		    sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
		dst_if = NULL;
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

	if (bif->bif_flags & IFBIF_BLOCKNONIP && bridge_blocknonip(&eh, m)) {
		m_freem(m);
		return;
	}

	if (bridge_filterrule(&bif->bif_brlin, &eh, m) == BRL_ACTION_BLOCK) {
		m_freem(m);
		return;
	}
	m = bridge_ip(&sc->sc_if, BRIDGE_IN, src_if, &eh, m);
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
	protected = bif->bif_protected;

	/*
	 * At this point, we're dealing with a unicast frame going to a
	 * different interface
	 */
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		return;
	}
	bif = (struct bridge_iflist *)dst_if->if_bridgeport;
	if ((bif->bif_flags & IFBIF_STP) &&
	    (bif->bif_state == BSTP_IFSTATE_DISCARDING)) {
		m_freem(m);
		return;
	}
	/*
	 * Do not transmit if both ports are part of the same protected
	 * domain.
	 */
	if (protected != 0 && (protected & bif->bif_protected)) {
		m_freem(m);
		return;
	}
	if (bridge_filterrule(&bif->bif_brlout, &eh, m) == BRL_ACTION_BLOCK) {
		m_freem(m);
		return;
	}
	m = bridge_ip(&sc->sc_if, BRIDGE_OUT, dst_if, &eh, m);
	if (m == NULL)
		return;

	len = m->m_pkthdr.len;
#if NVLAN > 0
	if ((m->m_flags & M_VLANTAG) &&
	    (dst_if->if_capabilities & IFCAP_VLAN_HWTAGGING) == 0)
		len += ETHER_VLAN_ENCAP_LEN;
#endif
	if ((len - ETHER_HDR_LEN) > dst_if->if_mtu)
		bridge_fragment(&sc->sc_if, dst_if, &eh, m);
	else {
		bridge_ifenqueue(&sc->sc_if, dst_if, m);
	}
}

/*
 * Return 1 if `ena' belongs to `bif', 0 otherwise.
 */
int
bridge_ourether(struct bridge_iflist *bif, uint8_t *ena)
{
	struct arpcom *ac = (struct arpcom *)bif->ifp;

	if (memcmp(ac->ac_enaddr, ena, ETHER_ADDR_LEN) == 0)
		return (1);

#if NCARP > 0
	if (carp_ourether(bif->ifp, ena))
		return (1);
#endif

	return (0);
}

/*
 * Receive input from an interface.  Queue the packet for bridging if its
 * not for us, and schedule an interrupt.
 */
int
bridge_input(struct ifnet *ifp, struct mbuf *m, void *cookie)
{
	KASSERT(m->m_flags & M_PKTHDR);

	if (m->m_flags & M_PROTO1) {
		m->m_flags &= ~M_PROTO1;
		return (0);
	}

	niq_enqueue(&bridgeintrq, m);

	return (1);
}

void
bridge_process(struct ifnet *ifp, struct mbuf *m)
{
	struct bridge_softc *sc;
	struct bridge_iflist *bif, *bif0;
	struct ether_header *eh;
	struct mbuf *mc;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	KERNEL_ASSERT_LOCKED();

	bif = (struct bridge_iflist *)ifp->if_bridgeport;
	if (bif == NULL)
		goto reenqueue;

	sc = bif->bridge_sc;
	if ((sc->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		goto reenqueue;

#if NVLAN > 0
	/*
	 * If the underlying interface removed the VLAN header itself,
	 * add it back.
	 */
	if (ISSET(m->m_flags, M_VLANTAG)) {
		m = vlan_inject(m, ETHERTYPE_VLAN, m->m_pkthdr.ether_vtag);
		if (m == NULL)
			return;
	}
#endif

#if NBPFILTER > 0
	if_bpf = sc->sc_if.if_bpf;
	if (if_bpf)
		bpf_mtap_ether(if_bpf, m, BPF_DIRECTION_IN);
#endif

	bridge_span(sc, m);

	eh = mtod(m, struct ether_header *);
	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		/*
		 * Reserved destination MAC addresses (01:80:C2:00:00:0x)
		 * should not be forwarded to bridge members according to
		 * section 7.12.6 of the 802.1D-2004 specification.  The
		 * STP destination address (as stored in bstp_etheraddr)
		 * is the first of these.
		 */
		if (memcmp(eh->ether_dhost, bstp_etheraddr,
		    ETHER_ADDR_LEN - 1) == 0) {
			if (eh->ether_dhost[ETHER_ADDR_LEN - 1] == 0) {
				/* STP traffic */
				if ((m = bstp_input(sc->sc_stp, bif->bif_stp,
				    eh, m)) == NULL)
					return;
			} else if (eh->ether_dhost[ETHER_ADDR_LEN - 1] <= 0xf) {
				m_freem(m);
				return;
			}
		}

		/*
		 * No need to process frames for ifs in the discarding state
		 */
		if ((bif->bif_flags & IFBIF_STP) &&
		    (bif->bif_state == BSTP_IFSTATE_DISCARDING))
			goto reenqueue;

		mc = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
		if (mc == NULL)
			goto reenqueue;

		bridge_ifinput(ifp, mc);

		bridgeintr_frame(sc, ifp, m);
		return;
	}

	/*
	 * No need to queue frames for ifs in the discarding state
	 */
	if ((bif->bif_flags & IFBIF_STP) &&
	    (bif->bif_state == BSTP_IFSTATE_DISCARDING))
		goto reenqueue;

	/*
	 * Unicast, make sure it's not for us.
	 */
	bif0 = bif;
	TAILQ_FOREACH(bif, &sc->sc_iflist, next) {
		if (bif->ifp->if_type != IFT_ETHER)
			continue;
		if (bridge_ourether(bif, eh->ether_dhost)) {
			if (bif0->bif_flags & IFBIF_LEARNING)
				bridge_rtupdate(sc,
				    (struct ether_addr *)&eh->ether_shost,
				    ifp, 0, IFBAF_DYNAMIC, m);
			if (bridge_filterrule(&bif0->bif_brlin, eh, m) ==
			    BRL_ACTION_BLOCK) {
				m_freem(m);
				return;
			}

			/* Count for the bridge */
			sc->sc_if.if_ipackets++;
			sc->sc_if.if_ibytes += m->m_pkthdr.len;

			bridge_ifinput(bif->ifp, m);
			return;
		}
		if (bridge_ourether(bif, eh->ether_shost)) {
			m_freem(m);
			return;
		}
	}

	bridgeintr_frame(sc, ifp, m);
	return;

reenqueue:
	bridge_ifinput(ifp, m);
}

/*
 * Send a frame to all interfaces that are members of the bridge
 * (except the one it came in on).
 */
void
bridge_broadcast(struct bridge_softc *sc, struct ifnet *ifp,
    struct ether_header *eh, struct mbuf *m)
{
	struct bridge_iflist *bif;
	struct mbuf *mc;
	struct ifnet *dst_if;
	int len, used = 0;
	u_int32_t protected;

	bif = (struct bridge_iflist *)ifp->if_bridgeport;
	protected = bif->bif_protected;

	TAILQ_FOREACH(bif, &sc->sc_iflist, next) {
		dst_if = bif->ifp;

		if ((dst_if->if_flags & IFF_RUNNING) == 0)
			continue;

		if ((bif->bif_flags & IFBIF_STP) &&
		    (bif->bif_state == BSTP_IFSTATE_DISCARDING))
			continue;

		if ((bif->bif_flags & IFBIF_DISCOVER) == 0 &&
		    (m->m_flags & (M_BCAST | M_MCAST)) == 0)
			continue;

		/* Drop non-IP frames if the appropriate flag is set. */
		if (bif->bif_flags & IFBIF_BLOCKNONIP &&
		    bridge_blocknonip(eh, m))
			continue;

		/*
		 * Do not transmit if both ports are part of the same
		 * protected domain.
		 */
		if (protected != 0 && (protected & bif->bif_protected))
			continue;

		if (bridge_filterrule(&bif->bif_brlout, eh, m) == BRL_ACTION_BLOCK)
			continue;

		/*
		 * Don't retransmit out of the same interface where
		 * the packet was received from.
		 */
		if (dst_if->if_index == ifp->if_index)
			continue;

		if (bridge_localbroadcast(dst_if, eh, m))
			sc->sc_if.if_oerrors++;

#if NMPW > 0
		/*
		 * Split horizon: avoid broadcasting messages from wire to
		 * another wire.
		 */
		if (ifp->if_type == IFT_MPLSTUNNEL &&
		    dst_if->if_type == IFT_MPLSTUNNEL)
			continue;
#endif /* NMPW */

		/* If last one, reuse the passed-in mbuf */
		if (TAILQ_NEXT(bif, next) == NULL) {
			mc = m;
			used = 1;
		} else {
			mc = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
			if (mc == NULL) {
				sc->sc_if.if_oerrors++;
				continue;
			}
		}

		mc = bridge_ip(&sc->sc_if, BRIDGE_OUT, dst_if, eh, mc);
		if (mc == NULL)
			continue;

		len = mc->m_pkthdr.len;
#if NVLAN > 0
		if ((mc->m_flags & M_VLANTAG) &&
		    (dst_if->if_capabilities & IFCAP_VLAN_HWTAGGING) == 0)
			len += ETHER_VLAN_ENCAP_LEN;
#endif
		if ((len - ETHER_HDR_LEN) > dst_if->if_mtu)
			bridge_fragment(&sc->sc_if, dst_if, eh, mc);
		else {
			bridge_ifenqueue(&sc->sc_if, dst_if, mc);
		}
	}

	if (!used)
		m_freem(m);
}

int
bridge_localbroadcast(struct ifnet *ifp, struct ether_header *eh,
    struct mbuf *m)
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
			return (0);
	}

	m1 = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
	if (m1 == NULL)
		return (1);

#if NPF > 0
	pf_pkt_addr_changed(m1);
#endif	/* NPF */

	bridge_ifinput(ifp, m1);

	return (0);
}

void
bridge_span(struct bridge_softc *sc, struct mbuf *m)
{
	struct bridge_iflist *bif;
	struct ifnet *ifp;
	struct mbuf *mc;
	int error;

	TAILQ_FOREACH(bif, &sc->sc_spanlist, next) {
		ifp = bif->ifp;

		if ((ifp->if_flags & IFF_RUNNING) == 0)
			continue;

		mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
		if (mc == NULL) {
			sc->sc_if.if_oerrors++;
			continue;
		}

		error = bridge_ifenqueue(&sc->sc_if, ifp, mc);
		if (error)
			continue;
	}
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

#ifdef IPSEC
int
bridge_ipsec(struct ifnet *ifp, struct ether_header *eh, int hassnap,
    struct llc *llc, int dir, int af, int hlen, struct mbuf *m)
{
	union sockaddr_union dst;
	struct tdb *tdb;
	u_int32_t spi;
	u_int16_t cpi;
	int error, off;
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
				goto skiplookup;

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

			break;
#ifdef INET6
		case AF_INET6:
			if (m->m_pkthdr.len - hlen < 2 * sizeof(u_int32_t))
				goto skiplookup;

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

			break;
#endif /* INET6 */
		default:
			return (0);
		}

		switch (proto) {
		case IPPROTO_ESP:
			m_copydata(m, hlen, sizeof(u_int32_t), (caddr_t)&spi);
			break;
		case IPPROTO_AH:
			m_copydata(m, hlen + sizeof(u_int32_t),
			    sizeof(u_int32_t), (caddr_t)&spi);
			break;
		case IPPROTO_IPCOMP:
			m_copydata(m, hlen + sizeof(u_int16_t),
			    sizeof(u_int16_t), (caddr_t)&cpi);
			spi = ntohl(htons(cpi));
			break;
		}

		NET_ASSERT_LOCKED();

		tdb = gettdb(ifp->if_rdomain, spi, &dst, proto);
		if (tdb != NULL && (tdb->tdb_flags & TDBF_INVALID) == 0 &&
		    tdb->tdb_xform != NULL) {
			if (tdb->tdb_first_use == 0) {
				tdb->tdb_first_use = time_second;
				if (tdb->tdb_flags & TDBF_FIRSTUSE)
					timeout_add_sec(&tdb->tdb_first_tmo,
					    tdb->tdb_exp_first_use);
				if (tdb->tdb_flags & TDBF_SOFT_FIRSTUSE)
					timeout_add_sec(&tdb->tdb_sfirst_tmo,
					    tdb->tdb_soft_first_use);
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
		    IPSP_DIRECTION_OUT, NULL, NULL, 0);
		if (tdb != NULL) {
			/*
			 * We don't need to do loop detection, the
			 * bridge will do that for us.
			 */
#if NPF > 0
			if ((encif = enc_getif(tdb->tdb_rdomain,
			    tdb->tdb_tap)) == NULL ||
			    pf_test(af, dir, encif, &m) != PF_PASS) {
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
				bridge_send_icmp_err(ifp, eh, m,
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
bridge_ip(struct ifnet *brifp, int dir, struct ifnet *ifp,
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
			ipstat_inc(ips_toosmall);
			return (NULL);
		}
		ip = mtod(m, struct ip *);

		if (ip->ip_v != IPVERSION) {
			ipstat_inc(ips_badvers);
			goto dropit;
		}

		hlen = ip->ip_hl << 2;	/* get whole header length */
		if (hlen < sizeof(struct ip)) {
			ipstat_inc(ips_badhlen);
			goto dropit;
		}

		if (hlen > m->m_len) {
			if ((m = m_pullup(m, hlen)) == NULL) {
				ipstat_inc(ips_badhlen);
				return (NULL);
			}
			ip = mtod(m, struct ip *);
		}

		if ((m->m_pkthdr.csum_flags & M_IPV4_CSUM_IN_OK) == 0) {
			if (m->m_pkthdr.csum_flags & M_IPV4_CSUM_IN_BAD) {
				ipstat_inc(ips_badsum);
				goto dropit;
			}

			ipstat_inc(ips_inswcsum);
			if (in_cksum(m, hlen) != 0) {
				ipstat_inc(ips_badsum);
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
		if ((brifp->if_flags & IFF_LINK2) == IFF_LINK2 &&
		    bridge_ipsec(ifp, eh, hassnap, &llc, dir, AF_INET, hlen, m))
			return (NULL);
#endif /* IPSEC */
#if NPF > 0
		/* Finally, we get to filter the packet! */
		if (pf_test(AF_INET, dir, ifp, &m) != PF_PASS)
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
			ipstat_inc(ips_outswcsum);
			ip->ip_sum = in_cksum(m, hlen);
		}

		break;

#ifdef INET6
	case ETHERTYPE_IPV6: {
		struct ip6_hdr *ip6;

		if (m->m_len < sizeof(struct ip6_hdr)) {
			if ((m = m_pullup(m, sizeof(struct ip6_hdr)))
			    == NULL) {
				ip6stat_inc(ip6s_toosmall);
				return (NULL);
			}
		}

		ip6 = mtod(m, struct ip6_hdr *);

		if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
			ip6stat_inc(ip6s_badvers);
			goto dropit;
		}

#ifdef IPSEC
		hlen = sizeof(struct ip6_hdr);

		if ((brifp->if_flags & IFF_LINK2) == IFF_LINK2 &&
		    bridge_ipsec(ifp, eh, hassnap, &llc, dir, AF_INET6, hlen,
		    m))
			return (NULL);
#endif /* IPSEC */

#if NPF > 0
		if (pf_test(AF_INET6, dir, ifp, &m) != PF_PASS)
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
	m_freem(m);
	return (NULL);
}

void
bridge_fragment(struct ifnet *brifp, struct ifnet *ifp, struct ether_header *eh,
    struct mbuf *m)
{
	struct llc llc;
	struct mbuf *m0;
	int error = 0;
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
			bridge_ifenqueue(brifp, ifp, m);
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
		bridge_send_icmp_err(ifp, eh, m, hassnap, &llc,
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
			error = bridge_ifenqueue(brifp, ifp, m);
			if (error) {
				continue;
			}
		} else
			m_freem(m);
	}

	if (error == 0)
		ipstat_inc(ips_fragmented);

	return;
 dropit:
	m_freem(m);
}

int
bridge_ifenqueue(struct ifnet *brifp, struct ifnet *ifp, struct mbuf *m)
{
	int error, len;

	/* Loop prevention. */
	m->m_flags |= M_PROTO1;

	len = m->m_pkthdr.len;

	error = if_enqueue(ifp, m);
	if (error) {
		brifp->if_oerrors++;
		return (error);
	}

	brifp->if_opackets++;
	brifp->if_obytes += len;

	return (0);
}

void
bridge_ifinput(struct ifnet *ifp, struct mbuf *m)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();

	m->m_flags |= M_PROTO1;

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);
}

void
bridge_send_icmp_err(struct ifnet *ifp,
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

struct bridge_tunneltag *
bridge_tunnel(struct mbuf *m)
{
	struct m_tag    *mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_TUNNEL, NULL)) == NULL)
		return (NULL);

	return ((struct bridge_tunneltag *)(mtag + 1));
}

struct bridge_tunneltag *
bridge_tunneltag(struct mbuf *m)
{
	struct m_tag	*mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_TUNNEL, NULL)) == NULL) {
		mtag = m_tag_get(PACKET_TAG_TUNNEL,
		    sizeof(struct bridge_tunneltag), M_NOWAIT);
		if (mtag == NULL)
			return (NULL);
		bzero(mtag + 1, sizeof(struct bridge_tunneltag));
		m_tag_prepend(m, mtag);
	}

	return ((struct bridge_tunneltag *)(mtag + 1));
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
	else {
		dst->sa_family = AF_UNSPEC;
		dst->sa_len = 0;
	}
}

void
bridge_copytag(struct bridge_tunneltag *src, struct bridge_tunneltag *dst)
{
	if (src == NULL) {
		memset(dst, 0, sizeof(*dst));
	} else {
		bridge_copyaddr(&src->brtag_peer.sa, &dst->brtag_peer.sa);
		bridge_copyaddr(&src->brtag_local.sa, &dst->brtag_local.sa);
		dst->brtag_id = src->brtag_id;
	}
}
