/*	$OpenBSD: if_trunk.c,v 1.4 2005/07/31 03:52:18 pascoe Exp $	*/

/*
 * Copyright (c) 2005 Reyk Floeter <reyk@vantronix.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bpfilter.h"
#include "trunk.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_trunk.h>

SLIST_HEAD(__trhead, trunk_softc) trunk_list;	/* list of trunks */

extern struct ifaddr **ifnet_addrs;
extern int ifqmaxlen;

void	 trunkattach(int);
int	 trunk_clone_create(struct if_clone *, int);
int	 trunk_clone_destroy(struct ifnet *);
void	 trunk_lladdr(struct trunk_softc *, u_int8_t *);
int	 trunk_port_create(struct trunk_softc *, struct ifnet *);
int	 trunk_port_destroy(struct trunk_port *);
void	 trunk_port_watchdog(struct ifnet *);
int	 trunk_port_ioctl(struct ifnet *, u_long, caddr_t);
struct trunk_port *trunk_port_get(struct trunk_softc *, struct ifnet *);
int	 trunk_port_checkstacking(struct trunk_softc *);
void	 trunk_port2req(struct trunk_port *, struct trunk_reqport *);
int	 trunk_ioctl(struct ifnet *, u_long, caddr_t);
void	 trunk_start(struct ifnet *);
void	 trunk_watchdog(struct ifnet *);
int	 trunk_media_change(struct ifnet *);
void	 trunk_media_status(struct ifnet *, struct ifmediareq *);
struct trunk_port *trunk_link_active(struct trunk_softc *,
	    struct trunk_port *);

struct if_clone trunk_cloner =
    IF_CLONE_INITIALIZER("trunk", trunk_clone_create, trunk_clone_destroy);

/* Simple round robin */
int	 trunk_rr_attach(struct trunk_softc *);
int	 trunk_rr_detach(struct trunk_softc *);
int	 trunk_rr_start(struct trunk_softc *, struct mbuf *);
int	 trunk_rr_watchdog(struct trunk_softc *);
int	 trunk_rr_input(struct trunk_softc *, struct trunk_port *,
	    struct ether_header *, struct mbuf *);

/* Trunk protocol table */
static const struct {
	enum trunk_proto	ti_proto;
	int			(*ti_attach)(struct trunk_softc *);
} trunk_protos[] = {
	{ TRUNK_PROTO_ROUNDROBIN, trunk_rr_attach },
	{ TRUNK_PROTO_NONE, }
};

void
trunkattach(int count)
{
	SLIST_INIT(&trunk_list);
	if_clone_attach(&trunk_cloner);
}

int
trunk_clone_create(struct if_clone *ifc, int unit)
{
	struct trunk_softc *tr;
	struct ifnet *ifp;
	int i, error = 0;

	if ((tr = malloc(sizeof(struct trunk_softc),
	    M_DEVBUF, M_NOWAIT)) == NULL)
		return (ENOMEM);

	bzero(tr, sizeof(struct trunk_softc));

	tr->tr_unit = unit;
	tr->tr_proto = TRUNK_PROTO_NONE;
	for (i = 0; trunk_protos[i].ti_proto != TRUNK_PROTO_NONE; i++) {
		if (trunk_protos[i].ti_proto == TRUNK_PROTO_DEFAULT) {
			tr->tr_proto = trunk_protos[i].ti_proto;
			if ((error = trunk_protos[i].ti_attach(tr)) != 0) {
				free(tr, M_DEVBUF);
				return (error);
			}
			break;
		}
	}
	SLIST_INIT(&tr->tr_ports);

	/* Initialise pseudo media types */
	ifmedia_init(&tr->tr_media, 0, trunk_media_change,
	    trunk_media_status);
	ifmedia_add(&tr->tr_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&tr->tr_media, IFM_ETHER | IFM_AUTO);

	ifp = &tr->tr_ac.ac_if;
	ifp->if_carp = NULL;
	ifp->if_type = IFT_ETHER;
	ifp->if_softc = tr;
	ifp->if_start = trunk_start;
	ifp->if_watchdog = trunk_watchdog;
	ifp->if_ioctl = trunk_ioctl;
	ifp->if_output = ether_output;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;

	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);

	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d",
	    ifc->ifc_name, unit);

	/*
	 * Attach as an ordinary ethernet device, childs will be attached
	 * as special device IFT_IEEE8023ADLAG.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Insert into the global list of trunks */
	SLIST_INSERT_HEAD(&trunk_list, tr, tr_entries);

	return (0);
}

int
trunk_clone_destroy(struct ifnet *ifp)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct trunk_port *tp;
	int error, s;

	s = splnet();

	/* Shutdown and remove trunk ports, return on error */
	while ((tp = SLIST_FIRST(&tr->tr_ports)) != NULL) {
		if ((error = trunk_port_destroy(tp)) != 0) {
			splx(s);
			return (error);
		}
	}

	ifmedia_delete_instance(&tr->tr_media, IFM_INST_ANY);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ether_ifdetach(ifp);
	if_detach(ifp);

	SLIST_REMOVE(&trunk_list, tr, trunk_softc, tr_entries);
	free(tr, M_DEVBUF);

	splx(s);

	return (0);
}

void
trunk_lladdr(struct trunk_softc *tr, u_int8_t *lladdr)
{
	struct ifnet *ifp = &tr->tr_ac.ac_if;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifa = ifnet_addrs[ifp->if_index];
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ETHER_ADDR_LEN;
	bcopy(lladdr, LLADDR(sdl), ETHER_ADDR_LEN);
	bcopy(lladdr, tr->tr_ac.ac_enaddr, ETHER_ADDR_LEN);
}

int
trunk_port_create(struct trunk_softc *tr, struct ifnet *ifp)
{
	struct trunk_softc *tr_ptr;
	struct trunk_port *tp;
	int error = 0;

	/* Limit the maximal number of trunk ports */
	if (tr->tr_count >= TRUNK_MAX_PORTS)
		return (ENOSPC);

	/* New trunk port has to be in an idle state */
	if (ifp->if_flags & IFF_OACTIVE)
		return (EBUSY);

	/* Check if port has already been associated to a trunk */
	if (trunk_port_get(NULL, ifp) != NULL)
		return (EBUSY);

	/* XXX Disallow non-ethernet interfaces (this should be any of 802) */
	if (ifp->if_type != IFT_ETHER)
		return (EPROTONOSUPPORT);

	if ((error = ifpromisc(ifp, 1)) != 0)
		return (error);

	if ((tp = malloc(sizeof(struct trunk_port),
	    M_DEVBUF, M_NOWAIT)) == NULL)
		return (ENOMEM);

	bzero(tp, sizeof(struct trunk_port));

	/* Check if port is a stacked trunk */
	SLIST_FOREACH(tr_ptr, &trunk_list, tr_entries) {
		if (ifp == &tr_ptr->tr_ac.ac_if) {
			tp->tp_flags |= TRUNK_PORT_STACK;
			if (trunk_port_checkstacking(tr_ptr) >=
			    TRUNK_MAX_STACKING) {
				free(tp, M_DEVBUF);
				return (E2BIG);
			}
		}
	}

	/* Change the interface type */
	tp->tp_iftype = ifp->if_type;
	ifp->if_type = IFT_IEEE8023ADLAG;
	ifp->if_tp = (caddr_t)tp;
	tp->tp_watchdog = ifp->if_watchdog;
	ifp->if_watchdog = trunk_port_watchdog;
	tp->tp_ioctl = ifp->if_ioctl;
	ifp->if_ioctl = trunk_port_ioctl;

	tp->tp_if = ifp;
	tp->tp_trunk = (caddr_t)tr;

	if (SLIST_EMPTY(&tr->tr_ports)) {
		tr->tr_primary = tp;
		tp->tp_flags |= TRUNK_PORT_MASTER;
		trunk_lladdr(tr, ((struct arpcom *)ifp)->ac_enaddr);
	}

	/* Insert into the global list of trunks */
	SLIST_INSERT_HEAD(&tr->tr_ports, tp, tp_entries);
	tr->tr_count++;

	return (0);
}

int
trunk_port_checkstacking(struct trunk_softc *tr)
{
	struct trunk_softc *tr_ptr;
	struct trunk_port *tp;
	int m = 0;
	
	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries) {
		if (tp->tp_flags & TRUNK_PORT_STACK) {
			tr_ptr = (struct trunk_softc *)tp->tp_if->if_softc;
			m = MAX(m, trunk_port_checkstacking(tr_ptr));
		}
	}

	return (m + 1);
}

int
trunk_port_destroy(struct trunk_port *tp)
{
	struct trunk_softc *tr = (struct trunk_softc *)tp->tp_trunk;
	struct trunk_port *tp_ptr;
	struct ifnet *ifp = tp->tp_if;

	/* Port has to be down */
	if (ifp->if_flags & IFF_UP)
		if_down(ifp);

	ifpromisc(ifp, 0);

	/* Restore interface */
	ifp->if_type = tp->tp_iftype;
	ifp->if_watchdog = tp->tp_watchdog;
	ifp->if_ioctl = tp->tp_ioctl;
	ifp->if_tp = NULL;

	/* Finally, remove the port from the trunk */
	SLIST_REMOVE(&tr->tr_ports, tp, trunk_port, tp_entries);
	tr->tr_count--;

	/* Update the primary interface */
	if (tp == tr->tr_primary) {
		if ((tp_ptr = SLIST_FIRST(&tr->tr_ports)) == NULL) {
			u_int8_t lladdr[ETHER_ADDR_LEN];
			bzero(&lladdr, ETHER_ADDR_LEN);
			trunk_lladdr(tr, lladdr);
		} else {
			trunk_lladdr(tr,
			    ((struct arpcom *)tp_ptr->tp_if)->ac_enaddr);
			tp_ptr->tp_flags = TRUNK_PORT_MASTER;
		}
		tr->tr_primary = tp_ptr;
	}

	free(tp, M_DEVBUF);

	return (0);
}

void
trunk_port_watchdog(struct ifnet *ifp)
{
	struct trunk_softc *tr;
	struct trunk_port *tp;

	/* Should be checked by the caller */
	if (ifp->if_type != IFT_IEEE8023ADLAG)
		return;
	if ((tp = (struct trunk_port *)ifp->if_tp) == NULL ||
	    (tr = (struct trunk_softc *)tp->tp_trunk) == NULL)
		return;

	if (tr->tr_ifflags & IFF_DEBUG)
		printf("%s\n", __func__);

	if (tp->tp_watchdog != NULL)
		(*tp->tp_watchdog)(ifp);
}


int
trunk_port_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct trunk_reqport *rp = (struct trunk_reqport *)data;
	struct trunk_softc *tr;
	struct trunk_port *tp;
	int s, error = 0;

	s = splimp();

	/* Should be checked by the caller */
	if (ifp->if_type != IFT_IEEE8023ADLAG ||
	    (tp = (struct trunk_port *)ifp->if_tp) == NULL ||
	    (tr = (struct trunk_softc *)tp->tp_trunk) == NULL)
		goto fallback;

	switch (cmd) {
	case SIOCGTRUNKPORT:
		if (rp->rp_portname[0] == '\0' ||
		    ifunit(rp->rp_portname) != ifp) {
			error = EINVAL;
			break;
		}

		/* Search in all trunks if the global flag is set */
		if ((tp = trunk_port_get(rp->rp_flags & TRUNK_PORT_GLOBAL ?
		    NULL : tr, ifp)) == NULL) {
			error = ENOENT;
			break;
		}

		trunk_port2req(tp, rp);
		break;
	default:
		goto fallback;
	}

	splx(s);
	return (error);

 fallback:
	splx(s);
	return ((*tp->tp_ioctl)(ifp, cmd, data));
}

void
trunk_port_ifdetach(struct ifnet *ifp)
{
	struct trunk_port *tp;

	if ((tp = (struct trunk_port *)ifp->if_tp) == NULL)
		return;

	trunk_port_destroy(tp);
}

struct trunk_port *
trunk_port_get(struct trunk_softc *tr, struct ifnet *ifp)
{
	struct trunk_port *tp;
	struct trunk_softc *tr_ptr;

	if (tr != NULL) {
		/* Search port in specified trunk */
		SLIST_FOREACH(tp, &tr->tr_ports, tp_entries) {
			if (tp->tp_if == ifp)
				return (tp);
		}
	} else {
		/* Search all trunks for the selected port */
		SLIST_FOREACH(tr_ptr, &trunk_list, tr_entries) {
			SLIST_FOREACH(tp, &tr_ptr->tr_ports, tp_entries) {
				if (tp->tp_if == ifp)
					return (tp);
			}
		}
	}

	return (NULL);
}

void
trunk_port2req(struct trunk_port *tp, struct trunk_reqport *rp)
{
	struct trunk_softc *tr = (struct trunk_softc *)tp->tp_trunk;
	strlcpy(rp->rp_ifname, tr->tr_ifname, sizeof(rp->rp_ifname));
	strlcpy(rp->rp_portname, tp->tp_if->if_xname, sizeof(rp->rp_portname));
	rp->rp_flags = tp->tp_flags;
}

int
trunk_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct trunk_reqall *ra = (struct trunk_reqall *)data;
	struct trunk_reqport *rp = (struct trunk_reqport *)data, rpbuf;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct trunk_port *tp;
	struct ifnet *tpif;
	int s, i, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &tr->tr_ac, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	bzero(&rpbuf, sizeof(rpbuf));

	switch (cmd) {
	case SIOCGTRUNK:
		ra->ra_proto = tr->tr_proto;
		ra->ra_ports = i = 0;
		tp = SLIST_FIRST(&tr->tr_ports);
		while (tp && ra->ra_size >=
		    i + sizeof(struct trunk_reqport)) {
			trunk_port2req(tp, &rpbuf);
			error = copyout(&rpbuf, (caddr_t)ra->ra_port + i,
			    sizeof(struct trunk_reqport));
			if (error)
				break;
			i += sizeof(struct trunk_reqport);
			ra->ra_ports++;
			tp = SLIST_NEXT(tp, tp_entries);
		}
		break;
	case SIOCSTRUNK:
		if ((error = suser(curproc, 0)) != 0) {
			error = EPERM;
			break;
		}
		if (ra->ra_proto >= TRUNK_PROTO_MAX) {
			error = EPROTONOSUPPORT;
			break;
		}
		if (tr->tr_proto != TRUNK_PROTO_NONE)
			error = tr->tr_detach(tr);
		if (error != 0)
			break;
		for (i = 0; i < (sizeof(trunk_protos) /
		    sizeof(trunk_protos[0])); i++) {
			if (trunk_protos[i].ti_proto == ra->ra_proto) {
				if (tr->tr_ifflags & IFF_DEBUG)
					printf("%s: using proto %u\n",
					    tr->tr_ifname,
					    trunk_protos[i].ti_proto);
				tr->tr_proto = trunk_protos[i].ti_proto;
				if (tr->tr_proto != TRUNK_PROTO_NONE)
					error = trunk_protos[i].ti_attach(tr);
				goto out;
			}
		}
		error = EPROTONOSUPPORT;
		break;
	case SIOCGTRUNKPORT:
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		/* Search in all trunks if the global flag is set */
		if ((tp = trunk_port_get(rp->rp_flags & TRUNK_PORT_GLOBAL ?
		    NULL : tr, tpif)) == NULL) {
			error = ENOENT;
			break;
		}

		trunk_port2req(tp, rp);
		break;
	case SIOCSTRUNKPORT:
		if ((error = suser(curproc, 0)) != 0) {
			error = EPERM;
			break;
		}
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}
		error = trunk_port_create(tr, tpif);
		break;
	case SIOCSTRUNKDELPORT:
		if ((error = suser(curproc, 0)) != 0) {
			error = EPERM;
			break;
		}
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		/* Search in all trunks if the global flag is set */
		if ((tp = trunk_port_get(rp->rp_flags & TRUNK_PORT_GLOBAL ?
		    NULL : tr, tpif)) == NULL) {
			error = ENOENT;
			break;
		}

		error = trunk_port_destroy(tp);
		break;
	case SIOCSIFADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET) {
			arp_ifinit(&tr->tr_ac, ifa);
		}
#endif /* INET */
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	case SIOCADDMULTI:
		error = ether_addmulti(ifr, &tr->tr_ac);
		break;
	case SIOCDELMULTI:
		error = ether_delmulti(ifr, &tr->tr_ac);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &tr->tr_media, cmd);
		break;
	default:
		error = EINVAL;
		break;
	}

 out:
	splx(s);

	return (error);
}

void
trunk_start(struct ifnet *ifp)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct mbuf *m;
	int error = 0;

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: start\n", ifp->if_xname);

	for (;; error = 0) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		if (tr->tr_proto != TRUNK_PROTO_NONE)
			error = (*tr->tr_start)(tr, m);
		else
			m_free(m);
		if (error == 0)
			ifp->if_opackets++;
		else
			ifp->if_oerrors++;
	}

	return;
}

void
trunk_watchdog(struct ifnet *ifp)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;

	if (tr->tr_proto != TRUNK_PROTO_NONE &&
	    (*tr->tr_watchdog)(tr) != 0) {
		ifp->if_oerrors++;
	}

}

int
trunk_input(struct ifnet *ifp, struct ether_header *eh, struct mbuf *m)
{
	struct trunk_softc *tr;
	struct trunk_port *tp;
	struct ifnet *trifp;
	int error = 0;

	/* Should be checked by the caller */
	if (ifp->if_type != IFT_IEEE8023ADLAG) {
		error = EPROTONOSUPPORT;
		goto bad;
	}
	if ((tp = (struct trunk_port *)ifp->if_tp) == NULL ||
	    (tr = (struct trunk_softc *)tp->tp_trunk) == NULL) {
		error = ENOENT;
		goto bad;
	}
	if (tr->tr_proto == TRUNK_PROTO_NONE)
		goto bad;
	trifp = &tr->tr_ac.ac_if;

#if NBPFILTER > 0
	if (trifp->if_bpf)
		bpf_mtap_hdr(trifp->if_bpf, (char *)eh, ETHER_HDR_LEN, m);
#endif

	error = (*tr->tr_input)(tr, tp, eh, m);
	if (error != 0)
		goto bad;

	trifp->if_ipackets++;

	return (0);

 bad:
	trifp->if_ierrors++;
	return (error);
}

int
trunk_media_change(struct ifnet *ifp)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;

	if (tr->tr_ifflags & IFF_DEBUG)
		printf("%s\n", __func__);

	/* Ignore */
	return (0);
}

void
trunk_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct trunk_port *tp;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_ETHER | IFM_AUTO;

	tp = tr->tr_primary;
	if (tp != NULL && tp->tp_if->if_flags & IFF_UP)
		imr->ifm_status |= IFM_ACTIVE;
}

struct trunk_port *
trunk_link_active(struct trunk_softc *tr, struct trunk_port *tp)
{
	struct trunk_port *tp_next;

	/*
	 * Search a port which reports an active link state.
	 * Normally, this should be LINK_STATE_UP but not all
	 * drivers seem to report this correctly so we assume
	 * that LINK_STATE_DOWN is the opposite from
	 * LINK_STATE_UNKNOWN and LINK_STATE_UP.
	 */

	if (tp == NULL)
		goto search;
	if (tp->tp_link_state != LINK_STATE_DOWN)
		return (tp);
	if ((tp_next = SLIST_NEXT(tp, tp_entries)) != NULL &&
	    tp_next->tp_link_state != LINK_STATE_DOWN)
		return (tp_next);

 search:
	SLIST_FOREACH(tp_next, &tr->tr_ports, tp_entries) {
		if (tp_next->tp_link_state != LINK_STATE_DOWN)
			return (tp_next);
	}

	return (NULL);
}

/*
 * Simple round robin trunking
 */

int
trunk_rr_attach(struct trunk_softc *tr)
{
	struct trunk_port *tp;

	tr->tr_detach = trunk_rr_detach;
	tr->tr_start = trunk_rr_start;
	tr->tr_input = trunk_rr_input;

	tp = SLIST_FIRST(&tr->tr_ports);
	tr->tr_psc = (caddr_t)tp;

	return (0);
}

int
trunk_rr_detach(struct trunk_softc *tr)
{
	tr->tr_psc = NULL;
	return (0);
}

int
trunk_rr_start(struct trunk_softc *tr, struct mbuf *m)
{
	struct trunk_port *tp = (struct trunk_port *)tr->tr_psc, *tp_next;
	struct ifnet *ifp;
	int error = 0;

	if (tp == NULL && (tp = trunk_link_active(tr, NULL)) == NULL)
		return (ENOENT);

	/* Send mbuf */
	ifp = tp->tp_if;
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error)
		return (error);
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);

	ifp->if_obytes += m->m_pkthdr.len;
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;

	/* Get next active port */
	tp_next = trunk_link_active(tr, SLIST_NEXT(tp, tp_entries));
	tr->tr_psc = (caddr_t)tp_next;

	return (error);
}

int
trunk_rr_watchdog(struct trunk_softc *tr)
{
	if (tr->tr_ifflags & IFF_DEBUG)
		printf("%s\n", __func__);

	return (0);
}

int
trunk_rr_input(struct trunk_softc *tr, struct trunk_port *tp,
    struct ether_header *eh, struct mbuf *m)
{
	struct ifnet *ifp = &tr->tr_ac.ac_if;

	/* Just pass in the packet to our trunk device */
	m->m_pkthdr.rcvif = ifp;

	return (0);
}
