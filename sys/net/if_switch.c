/*	$OpenBSD: if_switch.c,v 1.25 2018/12/28 14:32:47 bluhm Exp $	*/

/*
 * Copyright (c) 2016 Kazuya GODA <goda@openbsd.org>
 * Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>
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
#include "pf.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/selinfo.h>
#include <sys/pool.h>
#include <sys/syslog.h>

#include <net/if_types.h>
#include <net/netisr.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/ethertypes.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/ofp.h>
#include <net/if_bridge.h>
#include <net/if_switch.h>

int	 switch_clone_create(struct if_clone *, int);
int	 switch_clone_destroy(struct ifnet *);
void	 switch_process(struct ifnet *, struct mbuf *);
int	 switch_port_set_local(struct switch_softc *, struct switch_port *);
int	 switch_port_unset_local(struct switch_softc *, struct switch_port *);
int	 switch_ioctl(struct ifnet *, unsigned long, caddr_t);
int	 switch_port_add(struct switch_softc *, struct ifbreq *);
void	 switch_port_detach(void *);
int	 switch_port_del(struct switch_softc *, struct ifbreq *);
int	 switch_port_list(struct switch_softc *, struct ifbifconf *);
int	 switch_input(struct ifnet *, struct mbuf *, void *);
struct mbuf
	*switch_port_ingress(struct switch_softc *, struct ifnet *,
	    struct mbuf *);
int	 switch_ifenqueue(struct switch_softc *, struct ifnet *,
	    struct mbuf *, int);
void	 switch_port_ifb_start(struct ifnet *);

struct mbuf
	*switch_flow_classifier_udp(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_tcp(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_icmpv4(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_nd6(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_icmpv6(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_ipv4(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_ipv6(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_arp(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_ether(struct mbuf *, int *,
	    struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier_tunnel(struct mbuf *, int *,
	    struct switch_flow_classify *);
void	 switch_flow_classifier_dump(struct switch_softc *,
	    struct switch_flow_classify *);
void	 switchattach(int);

struct if_clone switch_cloner =
    IF_CLONE_INITIALIZER("switch", switch_clone_create, switch_clone_destroy);

LIST_HEAD(, switch_softc) switch_list;
struct niqueue switchintrq = NIQUEUE_INITIALIZER(1024, NETISR_SWITCH);
struct rwlock switch_ifs_lk = RWLOCK_INITIALIZER("switchifs");

struct pool swfcl_pool;

void
switchattach(int n)
{
	pool_init(&swfcl_pool, sizeof(union switch_field), 0, 0, 0,
	    "swfcl", NULL);
	swofp_attach();
	LIST_INIT(&switch_list);
	if_clone_attach(&switch_cloner);
}

struct switch_softc *
switch_lookup(int unit)
{
	struct switch_softc	*sc;

	/* must hold switch_ifs_lk */
	LIST_FOREACH(sc, &switch_list, sc_switch_next) {
		if (sc->sc_unit == unit)
			return (sc);
	}

	return (NULL);
}

int
switch_clone_create(struct if_clone *ifc, int unit)
{
	struct switch_softc	*sc;
	struct ifnet		*ifp;

	sc = malloc(sizeof(struct switch_softc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "switch%d", unit);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_ioctl = switch_ioctl;
	ifp->if_output = NULL;
	ifp->if_start = NULL;
	ifp->if_type = IFT_BRIDGE;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	TAILQ_INIT(&sc->sc_swpo_list);

	sc->sc_unit = unit;
	sc->sc_stp = bstp_create(&sc->sc_if);
	if (!sc->sc_stp) {
		free(sc, M_DEVBUF, sizeof(*sc));
		return (ENOMEM);
	}

	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, ETHER_HDR_LEN);
#endif

	swofp_create(sc);

	LIST_INSERT_HEAD(&switch_list, sc, sc_switch_next);

	return (0);
}

int
switch_clone_destroy(struct ifnet *ifp)
{
	struct switch_softc	*sc = ifp->if_softc;
	struct switch_port	*swpo, *tp;
	struct ifnet		*ifs;

	TAILQ_FOREACH_SAFE(swpo, &sc->sc_swpo_list, swpo_list_next, tp) {
		if ((ifs = if_get(swpo->swpo_ifindex)) != NULL) {
			switch_port_detach(ifs);
			if_put(ifs);
		} else
			log(LOG_ERR, "failed to cleanup on ifindex(%d)\n",
			    swpo->swpo_ifindex);
	}
	LIST_REMOVE(sc, sc_switch_next);
	bstp_destroy(sc->sc_stp);
	swofp_destroy(sc);
	switch_dev_destroy(sc);
	if_deactivate(ifp);
	if_detach(ifp);
	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}


void
switchintr(void)
{
	struct mbuf_list	 ml;
	struct mbuf		*m;
	struct ifnet		*ifp;

	niq_delist(&switchintrq, &ml);
	if (ml_empty(&ml))
		return;

	while ((m = ml_dequeue(&ml)) != NULL) {
		KASSERT(m->m_flags & M_PKTHDR);
		ifp = if_get(m->m_pkthdr.ph_ifidx);
		if (ifp == NULL) {
			m_freem(m);
			continue;
		}
		switch_process(ifp, m);
		if_put(ifp);
	}

}

void
switch_process(struct ifnet *ifp, struct mbuf *m)
{
	struct switch_softc		*sc = NULL;
	struct switch_port		*swpo;
	struct switch_flow_classify	 swfcl = { 0 };

	swpo = (struct switch_port *)ifp->if_switchport;
	if (swpo == NULL)
		goto discard;
	sc = swpo->swpo_switch;
	if ((sc->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		goto discard;

#if NBPFILTER > 0
	if (sc->sc_if.if_bpf)
		bpf_mtap_ether(sc->sc_if.if_bpf, m, BPF_DIRECTION_IN);
#endif

	if (m->m_pkthdr.len < sizeof(struct ether_header))
		goto discard;

	if ((m = switch_port_ingress(sc, ifp, m)) == NULL)
		return; /* m was freed in switch_process_ingress */

	if ((m = switch_flow_classifier(m, swpo->swpo_port_no,
	    &swfcl)) == NULL) {
		switch_swfcl_free(&swfcl);
		return;  /* m was freed in switch_flow_classifier */
	}

	if (sc->sc_if.if_flags & IFF_DEBUG)
		switch_flow_classifier_dump(sc, &swfcl);

	if (!sc->switch_process_forward)
		goto discard;

	(sc->switch_process_forward)(sc, &swfcl, m);

	switch_swfcl_free(&swfcl);
	return;

discard:
	m_freem(m);
	switch_swfcl_free(&swfcl);
	if (sc)
		sc->sc_if.if_oerrors++;
}

int
switch_port_set_local(struct switch_softc *sc, struct switch_port *swpo)
{
	struct switch_port	*tswpo;
	struct ifreq		 ifreq;
	struct ifnet		*ifs;
	int			error = 0, re_up = 0;

	/*
	 * Only one local interface can exist per switch device.
	 */
	TAILQ_FOREACH(tswpo, &sc->sc_swpo_list, swpo_list_next) {
		if (tswpo->swpo_flags & IFBIF_LOCAL)
			return (EEXIST);
	}

	ifs = if_get(swpo->swpo_ifindex);
	if (ifs == NULL)
		return (ENOENT);

	if (ifs->if_flags & IFF_UP) {
		re_up = 1;
		memset(&ifreq, 0, sizeof(ifreq));
		strlcpy(ifreq.ifr_name, ifs->if_xname, IFNAMSIZ);
		ifs->if_flags &= ~IFF_UP;
		ifreq.ifr_flags = ifs->if_flags;
		error = (*ifs->if_ioctl)(ifs, SIOCSIFFLAGS, (caddr_t)&ifreq);
		if (error)
			goto error;
	}

	swpo->swpo_flags |= IFBIF_LOCAL;
	swpo->swpo_port_no = OFP_PORT_LOCAL;
	swpo->swop_bk_start = ifs->if_start;
	ifs->if_start = switch_port_ifb_start;

	if (re_up) {
		memset(&ifreq, 0, sizeof(ifreq));
		strlcpy(ifreq.ifr_name, ifs->if_xname, IFNAMSIZ);
		ifs->if_flags &= IFF_UP;
		ifreq.ifr_flags = ifs->if_flags;
		error = (*ifs->if_ioctl)(ifs, SIOCSIFFLAGS, (caddr_t)&ifreq);
		if (error)
			goto error;
	}

 error:
	if_put(ifs);
	return (error);
}

int
switch_port_unset_local(struct switch_softc *sc, struct switch_port *swpo)
{
	struct ifreq	ifreq;
	struct ifnet	*ifs;
	int		error = 0, re_up = 0;

	ifs = if_get(swpo->swpo_ifindex);
	if (ifs == NULL)
		return (ENOENT);

	if (ifs->if_flags & IFF_UP) {
		re_up = 1;
		memset(&ifreq, 0, sizeof(ifreq));
		strlcpy(ifreq.ifr_name, ifs->if_xname, IFNAMSIZ);
		ifs->if_flags &= ~IFF_UP;
		ifreq.ifr_flags = ifs->if_flags;
		error = (*ifs->if_ioctl)(ifs, SIOCSIFFLAGS, (caddr_t)&ifreq);
		if (error)
			goto error;
	}

	swpo->swpo_flags &= ~IFBIF_LOCAL;
	swpo->swpo_port_no = swofp_assign_portno(sc, ifs->if_index);
	ifs->if_start = swpo->swop_bk_start;
	swpo->swop_bk_start = NULL;

	if (re_up) {
		memset(&ifreq, 0, sizeof(ifreq));
		strlcpy(ifreq.ifr_name, ifs->if_xname, IFNAMSIZ);
		ifs->if_flags &= IFF_UP;
		ifreq.ifr_flags = ifs->if_flags;
		error = (*ifs->if_ioctl)(ifs, SIOCSIFFLAGS, (caddr_t)&ifreq);
		if (error)
			goto error;
	}

 error:
	if_put(ifs);
	return (error);
}

int
switch_ioctl(struct ifnet *ifp, unsigned long cmd, caddr_t data)
{
	struct ifbaconf		*baconf = (struct ifbaconf *)data;
	struct ifbropreq	*brop = (struct ifbropreq *)data;
	struct ifbrlconf	*bc = (struct ifbrlconf *)data;
	struct ifbreq		*breq = (struct ifbreq *)data;
	struct switch_softc	*sc = ifp->if_softc;
	struct bstp_state	*bs = sc->sc_stp;
	struct bstp_port	*bp;
	struct ifnet		*ifs;
	struct switch_port	*swpo;
	int			 error = 0;

	switch (cmd) {
	case SIOCBRDGADD:
		if ((error = suser(curproc)) != 0)
			break;
		error = switch_port_add(sc, (struct ifbreq *)data);
		break;
	case SIOCBRDGDEL:
		if ((error = suser(curproc)) != 0)
			break;
		error = switch_port_del(sc, (struct ifbreq *)data);
		break;
	case SIOCBRDGIFS:
		error = switch_port_list(sc, (struct ifbifconf *)data);
		break;
	case SIOCBRDGADDL:
		if ((error = suser(curproc)) != 0)
			break;
		error = switch_port_add(sc, (struct ifbreq *)data);
		if (error && error != EEXIST)
			break;
		ifs = ifunit(breq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		swpo = (struct switch_port *)ifs->if_switchport;
		if (swpo == NULL || swpo->swpo_switch != sc) {
			error = ESRCH;
			break;
		}
		error = switch_port_set_local(sc, swpo);
		break;
	case SIOCBRDGGIFFLGS:
		ifs = ifunit(breq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		swpo = (struct switch_port *)ifs->if_switchport;
		if (swpo == NULL || swpo->swpo_switch != sc) {
			error = ESRCH;
			break;
		}
		breq->ifbr_ifsflags = swpo->swpo_flags;
		breq->ifbr_portno = swpo->swpo_port_no;
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == IFF_UP)
			ifp->if_flags |= IFF_RUNNING;

		if ((ifp->if_flags & IFF_UP) == 0)
			ifp->if_flags &= ~IFF_RUNNING;

		break;
	case SIOCBRDGRTS:
		baconf->ifbac_len = 0;
		break;
	case SIOCBRDGGRL:
		bc->ifbrl_len = 0;
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
	case SIOCSWGDPID:
	case SIOCSWSDPID:
	case SIOCSWGMAXFLOW:
	case SIOCSWGMAXGROUP:
	case SIOCSWSPORTNO:
		error = swofp_ioctl(ifp, cmd, data);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

int
switch_port_add(struct switch_softc *sc, struct ifbreq *req)
{
	struct ifnet		*ifs;
	struct switch_port	*swpo;
	int			 error;

	if ((ifs = ifunit(req->ifbr_ifsname)) == NULL)
		return (ENOENT);

	if (ifs->if_bridgeport != NULL)
		return (EBUSY);

	if (ifs->if_switchport != NULL) {
		swpo = (struct switch_port *)ifs->if_switchport;
		if (swpo->swpo_switch == sc)
			return (EEXIST);
		else
			return (EBUSY);
	}

	if (ifs->if_type == IFT_ETHER) {
		if ((error = ifpromisc(ifs, 1)) != 0)
			return (error);
	}

	swpo = malloc(sizeof(*swpo), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (swpo == NULL) {
		if (ifs->if_type == IFT_ETHER)
			ifpromisc(ifs, 0);
		return (ENOMEM);
	}
	swpo->swpo_switch = sc;
	swpo->swpo_ifindex = ifs->if_index;
	ifs->if_switchport = (caddr_t)swpo;
	if_ih_insert(ifs, switch_input, NULL);
	swpo->swpo_port_no = swofp_assign_portno(sc, ifs->if_index);
	swpo->swpo_dhcookie = hook_establish(ifs->if_detachhooks, 0,
	    switch_port_detach, ifs);

	nanouptime(&swpo->swpo_appended);

	TAILQ_INSERT_TAIL(&sc->sc_swpo_list, swpo, swpo_list_next);

	return (0);
}

int
switch_port_list(struct switch_softc *sc, struct ifbifconf *bifc)
{
	struct switch_port	*swpo;
	struct ifnet		*ifs;
	struct ifbreq		 breq;
	int			 total = 0, n = 0, error = 0;

	TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next)
		total++;

	if (bifc->ifbic_len == 0)
		goto done;

	TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
		memset(&breq, 0, sizeof(breq));

		if (bifc->ifbic_len < sizeof(breq))
			break;

		ifs = if_get(swpo->swpo_ifindex);
		if (ifs == NULL) {
			error = ENOENT;
			goto done;
		}
		strlcpy(breq.ifbr_ifsname, ifs->if_xname, IFNAMSIZ);
		if_put(ifs);

		strlcpy(breq.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		breq.ifbr_ifsflags = swpo->swpo_flags;
		breq.ifbr_portno = swpo->swpo_port_no;

		if ((error = copyout((caddr_t)&breq,
		    (caddr_t)(bifc->ifbic_req + n), sizeof(breq))) != 0)
			goto done;

		bifc->ifbic_len -= sizeof(breq);
		n++;
	}

done:
	bifc->ifbic_len = n * sizeof(breq);
	return (error);
}

void
switch_port_detach(void *arg)
{
	struct ifnet		*ifp = (struct ifnet *)arg;
	struct switch_softc	*sc;
	struct switch_port	*swpo;

	swpo = (struct switch_port *)ifp->if_switchport;
	sc = swpo->swpo_switch;
	if (swpo->swpo_flags & IFBIF_LOCAL)
		switch_port_unset_local(sc, swpo);

	ifp->if_switchport = NULL;
	hook_disestablish(ifp->if_detachhooks, swpo->swpo_dhcookie);
	ifpromisc(ifp, 0);
	if_ih_remove(ifp, switch_input, NULL);
	TAILQ_REMOVE(&sc->sc_swpo_list, swpo, swpo_list_next);
	free(swpo, M_DEVBUF, sizeof(*swpo));
}

int
switch_port_del(struct switch_softc *sc, struct ifbreq *req)
{
	struct switch_port	*swpo;
	struct ifnet		*ifs;
	int			 error = 0;

	TAILQ_FOREACH(swpo, &sc->sc_swpo_list, swpo_list_next) {
		if ((ifs = if_get(swpo->swpo_ifindex)) == NULL)
			continue;
		if (strncmp(ifs->if_xname, req->ifbr_ifsname, IFNAMSIZ) == 0)
			break;
		if_put(ifs);
	}

	if (swpo) {
		switch_port_detach(ifs);
		if_put(ifs);
		error = 0;
	} else
		error = ENOENT;

	return (error);
}

int
switch_input(struct ifnet *ifp, struct mbuf *m, void *cookie)
{
	KASSERT(m->m_flags & M_PKTHDR);

	if (m->m_flags & M_PROTO1) {
		m->m_flags &= ~M_PROTO1;
		return (0);
	}

	niq_enqueue(&switchintrq, m);

	return (1);
}


struct mbuf *
switch_port_ingress(struct switch_softc *sc, struct ifnet *src_if,
    struct mbuf *m)
{
	struct switch_port	*swpo;
	struct ether_header	 eh;

	swpo = (struct switch_port *)src_if->if_switchport;

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	m_copydata(m, 0, ETHER_HDR_LEN, (caddr_t)&eh);
#if 0
	/* It's the "#if 0" because it doesn't test switch(4) with pf(4)
	 * or with ipsec(4).
	 */
	if ((m = bridge_ip((struct bridge_softc *)sc,
	    PF_IN, src_if, &eh, m)) == NULL) {
		sc->sc_if.if_ierrors++;
		return (NULL);
	}
#endif /* NPF */

	return (m);
}

void
switch_port_egress(struct switch_softc *sc, struct switch_fwdp_queue *fwdp_q,
    struct mbuf *m)
{
	struct switch_port	*swpo;
	struct ifnet		*dst_if;
	struct mbuf		*mc;
	struct ether_header	 eh;
	int			 len, used = 0;

#if NBPFILTER > 0
	if (sc->sc_if.if_bpf)
		bpf_mtap(sc->sc_if.if_bpf, m, BPF_DIRECTION_OUT);
#endif

	m_copydata(m, 0, ETHER_HDR_LEN, (caddr_t)&eh);
	TAILQ_FOREACH(swpo, fwdp_q, swpo_fwdp_next) {

		if ((dst_if = if_get(swpo->swpo_ifindex)) == NULL)
			continue;

		if ((dst_if->if_flags & IFF_RUNNING) == 0)
			goto out;

		if (TAILQ_NEXT(swpo, swpo_fwdp_next) == NULL) {
			mc = m;
			used = 1;
		} else {
			mc = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
			if (mc == NULL)
				goto out;
		}

#if 0
		/* It's the "#if 0" because it doesn't test switch(4) with pf(4)
		 * or with ipsec(4).
		 */
		if ((mc = bridge_ip((struct bridge_softc *)sc,
		    PF_OUT, dst_if, &eh, mc)) == NULL) {
			sc->sc_if.if_ierrors++;
			goto out;
		}
#endif

		len = mc->m_pkthdr.len;
#if NVLAN > 0
		if ((mc->m_flags & M_VLANTAG) &&
		    (dst_if->if_capabilities & IFCAP_VLAN_HWTAGGING) == 0)
			len += ETHER_VLAN_ENCAP_LEN;
#endif

		/*
		 * Only if egress port has local port capabilities, it doesn't
		 * need fragment because a frame sends up local network stack.
		 */
		if (!(swpo->swpo_flags & IFBIF_LOCAL) &&
		    ((len - ETHER_HDR_LEN) > dst_if->if_mtu))
			bridge_fragment(&sc->sc_if, dst_if, &eh, mc);
		else
			switch_ifenqueue(sc, dst_if, mc,
			    (swpo->swpo_flags & IFBIF_LOCAL));
 out:

		if_put(dst_if);
	}

	if (!used)
		m_freem(m);
}

int
switch_ifenqueue(struct switch_softc *sc, struct ifnet *ifp,
    struct mbuf *m, int local)
{
	struct mbuf_list	 ml = MBUF_LIST_INITIALIZER();
	int			 error, len;

	/* Loop prevention. */
	m->m_flags |= M_PROTO1;

	KASSERT(m->m_flags & M_PKTHDR);
	len = m->m_pkthdr.len;

	if (local) {
		ml_enqueue(&ml, m);
		if_input(ifp, &ml);
	} else {
		error = if_enqueue(ifp, m);
		if (error) {
			sc->sc_if.if_oerrors++;
			return (error);
		}
		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += len;
	}

	return (0);
}

void
switch_port_ifb_start(struct ifnet *ifp)
{
	struct mbuf		*m;
	struct mbuf_list	 ml = MBUF_LIST_INITIALIZER();

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			return;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif /* NBPFILTER > 0 */

		ml_enqueue(&ml, m);
		if_input(ifp, &ml);
	}
}

/*
 * Flow Classifier
 */

int
switch_swfcl_dup(struct switch_flow_classify *from,
    struct switch_flow_classify *to)
{
	memset(to, 0, sizeof(*to));

	to->swfcl_flow_hash = from->swfcl_flow_hash;
	to->swfcl_metadata = from->swfcl_metadata;
	to->swfcl_cookie = from->swfcl_cookie;
	to->swfcl_table_id = from->swfcl_table_id;
	to->swfcl_in_port = from->swfcl_in_port;

	if (from->swfcl_tunnel) {
		to->swfcl_tunnel = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_tunnel == NULL)
			goto failed;
		memcpy(to->swfcl_tunnel, from->swfcl_tunnel,
		    sizeof(*from->swfcl_tunnel));
	}
	if (from->swfcl_ether) {
		to->swfcl_ether = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_ether == NULL)
			goto failed;
		memcpy(to->swfcl_ether, from->swfcl_ether,
		    sizeof(*from->swfcl_ether));
	}
	if (from->swfcl_vlan) {
		to->swfcl_vlan = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_vlan == NULL)
			goto failed;
		memcpy(to->swfcl_vlan, from->swfcl_vlan,
		    sizeof(*from->swfcl_vlan));
	}
	if (from->swfcl_ipv4) {
		to->swfcl_ipv4 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_ipv4 == NULL)
			goto failed;
		memcpy(to->swfcl_ipv4, from->swfcl_ipv4,
		    sizeof(*from->swfcl_ipv4));
	}
	if (from->swfcl_ipv6) {
		to->swfcl_ipv6 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_ipv6 == NULL)
			goto failed;
		memcpy(to->swfcl_ipv6, from->swfcl_ipv6,
		    sizeof(*from->swfcl_ipv6));
	}
	if (from->swfcl_arp) {
		to->swfcl_arp = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_arp == NULL)
			goto failed;
		memcpy(to->swfcl_arp, from->swfcl_arp,
		    sizeof(*from->swfcl_arp));

	}
	if (from->swfcl_nd6) {
		to->swfcl_nd6 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_nd6 == NULL)
			goto failed;
		memcpy(to->swfcl_nd6, from->swfcl_nd6,
		    sizeof(*from->swfcl_nd6));
	}
	if (from->swfcl_icmpv4) {
		to->swfcl_icmpv4 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_icmpv4 == NULL)
			goto failed;
		memcpy(to->swfcl_icmpv4, from->swfcl_icmpv4,
		    sizeof(*from->swfcl_icmpv4));
	}
	if (from->swfcl_icmpv6) {
		to->swfcl_icmpv6 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_icmpv6 == NULL)
			goto failed;
		memcpy(to->swfcl_icmpv6, from->swfcl_icmpv6,
		    sizeof(*from->swfcl_icmpv6));
	}
	if (from->swfcl_tcp) {
		to->swfcl_tcp = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_tcp == NULL)
			goto failed;
		memcpy(to->swfcl_tcp, from->swfcl_tcp,
		    sizeof(*from->swfcl_tcp));
	}
	if (from->swfcl_udp) {
		to->swfcl_udp = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_udp == NULL)
			goto failed;
		memcpy(to->swfcl_udp, from->swfcl_udp,
		    sizeof(*from->swfcl_udp));
	}
	if (from->swfcl_sctp) {
		to->swfcl_sctp = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (to->swfcl_sctp == NULL)
			goto failed;
		memcpy(to->swfcl_sctp, from->swfcl_sctp,
		    sizeof(*from->swfcl_sctp));
	}

	return (0);
 failed:
	switch_swfcl_free(to);
	return (ENOBUFS);
}

void
switch_swfcl_free(struct switch_flow_classify *swfcl)
{
	if (swfcl->swfcl_tunnel)
		pool_put(&swfcl_pool, swfcl->swfcl_tunnel);
	if (swfcl->swfcl_ether)
		pool_put(&swfcl_pool, swfcl->swfcl_ether);
	if (swfcl->swfcl_vlan)
		pool_put(&swfcl_pool, swfcl->swfcl_vlan);
	if (swfcl->swfcl_ipv4)
		pool_put(&swfcl_pool, swfcl->swfcl_ipv4);
	if (swfcl->swfcl_ipv6)
		pool_put(&swfcl_pool, swfcl->swfcl_ipv6);
	if (swfcl->swfcl_arp)
		pool_put(&swfcl_pool, swfcl->swfcl_arp);
	if (swfcl->swfcl_nd6)
		pool_put(&swfcl_pool, swfcl->swfcl_nd6);
	if (swfcl->swfcl_icmpv4)
		pool_put(&swfcl_pool, swfcl->swfcl_icmpv4);
	if (swfcl->swfcl_icmpv6)
		pool_put(&swfcl_pool, swfcl->swfcl_icmpv6);
	if (swfcl->swfcl_tcp)
		pool_put(&swfcl_pool, swfcl->swfcl_tcp);
	if (swfcl->swfcl_udp)
		pool_put(&swfcl_pool, swfcl->swfcl_udp);
	if (swfcl->swfcl_sctp)
		pool_put(&swfcl_pool, swfcl->swfcl_sctp);

	memset(swfcl, 0, sizeof(*swfcl));
}

struct mbuf *
switch_flow_classifier_udp(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct udphdr	*uh;

	swfcl->swfcl_udp = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_udp == NULL) {
		m_freem(m);
		return (NULL);
	}

	if (m->m_len < (*offset + sizeof(*uh)) &&
	    (m = m_pullup(m, *offset + sizeof(*uh))) == NULL)
		return (NULL);

	uh = (struct udphdr *)((m)->m_data + *offset);

	swfcl->swfcl_udp->udp_src = uh->uh_sport;
	swfcl->swfcl_udp->udp_dst = uh->uh_dport;

	return (m);
}

struct mbuf *
switch_flow_classifier_tcp(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct tcphdr	*th;

	swfcl->swfcl_tcp = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_tcp == NULL) {
		m_freem(m);
		return (NULL);
	}

	if (m->m_len < (*offset + sizeof(*th)) &&
	    (m = m_pullup(m, *offset + sizeof(*th))) == NULL)
		return (NULL);

	th = (struct tcphdr *)((m)->m_data + *offset);

	swfcl->swfcl_tcp->tcp_src = th->th_sport;
	swfcl->swfcl_tcp->tcp_dst = th->th_dport;
	swfcl->swfcl_tcp->tcp_flags = th->th_flags;

	return (m);
}

struct mbuf *
switch_flow_classifier_icmpv4(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct icmp	*icmp;

	swfcl->swfcl_icmpv4 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_icmpv4 == NULL) {
		m_freem(m);
		return (NULL);
	}

	if (m->m_len < (*offset + ICMP_MINLEN) &&
	    (m = m_pullup(m, (*offset + ICMP_MINLEN))) == NULL)
		return (NULL);

	icmp = (struct icmp *)((m)->m_data + *offset);

	swfcl->swfcl_icmpv4->icmpv4_type = icmp->icmp_type;
	swfcl->swfcl_icmpv4->icmpv4_code = icmp->icmp_code;

	return (m);
}

#ifdef INET6
struct mbuf *
switch_flow_classifier_nd6(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct icmp6_hdr		*icmp6;
	struct nd_neighbor_advert	*nd_na;
	struct nd_neighbor_solicit	*nd_ns;
	union nd_opts			 ndopts;
	uint8_t				*lladdr;
	int				 lladdrlen;
	int				 icmp6len = m->m_pkthdr.len - *offset;

	IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, *offset, sizeof(*icmp6));
	if (icmp6 == NULL)
		goto failed;

	switch (icmp6->icmp6_type) {
	case ND_NEIGHBOR_ADVERT:
		if (icmp6len < sizeof(struct nd_neighbor_advert))
			goto failed;
		break;
	case ND_NEIGHBOR_SOLICIT:
		if (icmp6len < sizeof(struct nd_neighbor_solicit))
			goto failed;
		break;
	}

	swfcl->swfcl_nd6 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_nd6 == NULL)
		goto failed;

	switch (icmp6->icmp6_type) {
	case ND_NEIGHBOR_ADVERT:
		IP6_EXTHDR_GET(nd_na, struct nd_neighbor_advert *, m,
		    *offset, icmp6len);

		if (nd_na == NULL)
			goto failed;

		swfcl->swfcl_nd6->nd6_target = nd_na->nd_na_target;
		icmp6len -= sizeof(*nd_na);
		nd6_option_init(nd_na + 1, icmp6len, &ndopts);
		if (nd6_options(&ndopts) < 0)
			goto failed;

		if (!ndopts.nd_opts_tgt_lladdr)
			goto failed;

		lladdr = (char *)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = (ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3) - 2;

		/* switch(4) only supports Ethernet interfaces */
		if (lladdrlen != ETHER_ADDR_LEN)
			goto failed;
		memcpy(swfcl->swfcl_nd6->nd6_lladdr, lladdr, ETHER_ADDR_LEN);
		break;
	case ND_NEIGHBOR_SOLICIT:
		IP6_EXTHDR_GET(nd_ns, struct nd_neighbor_solicit *, m,
		    *offset, icmp6len);
		if (nd_ns == NULL)
			goto failed;
		swfcl->swfcl_nd6->nd6_target = nd_ns->nd_ns_target;
		icmp6len -= sizeof(*nd_ns);

		nd6_option_init(nd_ns + 1, icmp6len, &ndopts);
		if (nd6_options(&ndopts) < 0)
			goto failed;

		if (!ndopts.nd_opts_src_lladdr)
			goto failed;
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = (ndopts.nd_opts_src_lladdr->nd_opt_len << 3) - 2;

		/* switch(4) only supports Ethernet interfaces */
		if (lladdrlen != ETHER_ADDR_LEN)
			goto failed;
		memcpy(swfcl->swfcl_nd6->nd6_lladdr, lladdr, ETHER_ADDR_LEN);

		break;
	}

	return (m);

 failed:
	m_freem(m);
	return (NULL);
}

struct mbuf *
switch_flow_classifier_icmpv6(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct icmp6_hdr	*icmp6;

	swfcl->swfcl_icmpv6 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_icmpv6 == NULL) {
		m_freem(m);
		return (NULL);
	}

	IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, *offset, sizeof(*icmp6));
	if (icmp6 == NULL)
		return (NULL); /* m was already freed */

	swfcl->swfcl_icmpv6->icmpv6_type = icmp6->icmp6_type;
	swfcl->swfcl_icmpv6->icmpv6_code = icmp6->icmp6_code;

	switch (icmp6->icmp6_type) {
	case ND_NEIGHBOR_SOLICIT:
	case ND_NEIGHBOR_ADVERT:
		return switch_flow_classifier_nd6(m, offset, swfcl);
	}

	return (m);
}
#endif /* INET6 */

struct mbuf *
switch_flow_classifier_ipv4(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct ip	*ip;

	swfcl->swfcl_ipv4 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_ipv4 == NULL) {
		m_freem(m);
		return (NULL);
	}

	if (m->m_len < (*offset + sizeof(*ip)) &&
	    (m = m_pullup(m, *offset + sizeof(*ip))) == NULL)
		return (NULL);

	ip = (struct ip *)((m)->m_data + *offset);

	swfcl->swfcl_ipv4->ipv4_tos = ip->ip_tos;
	swfcl->swfcl_ipv4->ipv4_ttl = ip->ip_ttl;
	swfcl->swfcl_ipv4->ipv4_proto = ip->ip_p;

	memcpy(&swfcl->swfcl_ipv4->ipv4_src, &ip->ip_src.s_addr,
	    sizeof(uint32_t));
	memcpy(&swfcl->swfcl_ipv4->ipv4_dst, &ip->ip_dst.s_addr,
	    sizeof(uint32_t));

	*offset += (ip->ip_hl << 2);

	switch (ip->ip_p) {
	case IPPROTO_UDP:
		return switch_flow_classifier_udp(m, offset, swfcl);
	case IPPROTO_TCP:
		return switch_flow_classifier_tcp(m, offset, swfcl);
	case IPPROTO_ICMP:
		return switch_flow_classifier_icmpv4(m, offset, swfcl);
	}

	return (m);
}

#ifdef INET6
struct mbuf *
switch_flow_classifier_ipv6(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct ip6_hdr	*ip6;

	swfcl->swfcl_ipv6 = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_ipv6 == NULL) {
		m_freem(m);
		return (NULL);
	}

	if (m->m_len < (*offset + sizeof(*ip6)) &&
	    (m = m_pullup(m, *offset + sizeof(*ip6))) == NULL)
		return (NULL);

	ip6 = (struct ip6_hdr *)((m)->m_data + *offset);

	swfcl->swfcl_ipv6->ipv6_src = ip6->ip6_src;
	swfcl->swfcl_ipv6->ipv6_dst = ip6->ip6_dst;
	swfcl->swfcl_ipv6->ipv6_flow_label =
	    (ip6->ip6_flow & IPV6_FLOWLABEL_MASK);
	swfcl->swfcl_ipv6->ipv6_tclass = (ntohl(ip6->ip6_flow) >> 20);
	swfcl->swfcl_ipv6->ipv6_hlimit = ip6->ip6_hlim;
	swfcl->swfcl_ipv6->ipv6_nxt = ip6->ip6_nxt;

	*offset += sizeof(*ip6);

	switch (ip6->ip6_nxt) {
	case IPPROTO_UDP:
		return switch_flow_classifier_udp(m, offset, swfcl);
	case IPPROTO_TCP:
		return switch_flow_classifier_tcp(m, offset, swfcl);
	case IPPROTO_ICMPV6:
		return switch_flow_classifier_icmpv6(m, offset, swfcl);
	}

	return (m);
}
#endif /* INET6 */

struct mbuf *
switch_flow_classifier_arp(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct ether_arp	*ea;

	swfcl->swfcl_arp = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_arp == NULL) {
		m_freem(m);
		return (NULL);
	}

	if (m->m_len < (*offset + sizeof(*ea)) &&
	    (m = m_pullup(m, *offset + sizeof(*ea))) == NULL)
		return (NULL);

	ea = (struct ether_arp *)((m)->m_data + *offset);

	swfcl->swfcl_arp->_arp_op = ea->arp_op;

	memcpy(swfcl->swfcl_arp->arp_sha, &ea->arp_sha, ETHER_ADDR_LEN);
	memcpy(swfcl->swfcl_arp->arp_tha, &ea->arp_tha, ETHER_ADDR_LEN);
	memcpy(&swfcl->swfcl_arp->arp_sip, &ea->arp_spa, sizeof(uint32_t));
	memcpy(&swfcl->swfcl_arp->arp_tip, &ea->arp_tpa, sizeof(uint32_t));

	return (m);
}

struct mbuf *
switch_flow_classifier_ether(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct ether_header		*eh;
	struct ether_vlan_header	*evl;
	uint16_t			 ether_type;

	swfcl->swfcl_ether = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_ether == NULL) {
		m_freem(m);
		return (NULL);
	}

	if (m->m_len < sizeof(*eh) && (m = m_pullup(m, sizeof(*eh))) == NULL)
		return (NULL);
	eh = mtod(m, struct ether_header *);

	memcpy(swfcl->swfcl_ether->eth_src, eh->ether_shost, ETHER_ADDR_LEN);
	memcpy(swfcl->swfcl_ether->eth_dst, eh->ether_dhost, ETHER_ADDR_LEN);

	if ((m->m_flags & M_VLANTAG) ||
	    (ntohs(eh->ether_type) == ETHERTYPE_VLAN) ||
	    (ntohs(eh->ether_type) == ETHERTYPE_QINQ)) {
		swfcl->swfcl_vlan = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
		if (swfcl->swfcl_vlan == NULL) {
			m_freem(m);
			return (NULL);
		}
	}

	if (m->m_flags & M_VLANTAG) {
		/*
		 * Hardware VLAN tagging is only supported for 801.1Q VLAN,
		 * but not for 802.1ad QinQ.
		 */
		swfcl->swfcl_vlan->vlan_tpid = htons(ETHERTYPE_VLAN);
		swfcl->swfcl_vlan->vlan_vid =
		    htons(EVL_VLANOFTAG(m->m_pkthdr.ether_vtag));
		swfcl->swfcl_vlan->vlan_pcp =
		    EVL_PRIOFTAG(m->m_pkthdr.ether_vtag);
		ether_type = eh->ether_type;
		*offset += sizeof(*eh);
	} else if (ntohs(eh->ether_type) == ETHERTYPE_VLAN) {
		if (m->m_len < sizeof(*evl) &&
		    (m = m_pullup(m, sizeof(*evl))) == NULL)
			return (NULL);
		evl = mtod(m, struct ether_vlan_header *);

		/*
		 * Software VLAN tagging is currently only supported for
		 * 801.1Q VLAN, but not for 802.1ad QinQ.
		 */
		swfcl->swfcl_vlan->vlan_tpid = htons(ETHERTYPE_VLAN);
		swfcl->swfcl_vlan->vlan_vid =
		    (evl->evl_tag & htons(EVL_VLID_MASK));
		swfcl->swfcl_vlan->vlan_pcp =
		    EVL_PRIOFTAG(ntohs(evl->evl_tag));
		ether_type = evl->evl_proto;
		*offset += sizeof(*evl);
	} else {
		ether_type = eh->ether_type;
		*offset += sizeof(*eh);
	}

	swfcl->swfcl_ether->eth_type = ether_type;

	ether_type = ntohs(ether_type);
	switch (ether_type) {
	case ETHERTYPE_ARP:
		return switch_flow_classifier_arp(m, offset, swfcl);
	case ETHERTYPE_IP:
		return switch_flow_classifier_ipv4(m, offset, swfcl);
#ifdef INET6
	case ETHERTYPE_IPV6:
		return switch_flow_classifier_ipv6(m, offset, swfcl);
#endif /* INET6 */
	case ETHERTYPE_MPLS:
		/* unsupported yet */
		break;
	}

	return (m);
}

struct mbuf *
switch_flow_classifier_tunnel(struct mbuf *m, int *offset,
    struct switch_flow_classify *swfcl)
{
	struct bridge_tunneltag	*brtag;

	if ((brtag = bridge_tunnel(m)) == NULL)
		goto out;

	if ((brtag->brtag_peer.sa.sa_family != AF_INET) &&
	    (brtag->brtag_peer.sa.sa_family != AF_INET6))
		goto out;

	swfcl->swfcl_tunnel = pool_get(&swfcl_pool, PR_NOWAIT|PR_ZERO);
	if (swfcl->swfcl_tunnel == NULL) {
		m_freem(m);
		return (NULL);
	}

	swfcl->swfcl_tunnel->tun_af = brtag->brtag_peer.sa.sa_family;
	swfcl->swfcl_tunnel->tun_key = htobe64(brtag->brtag_id);
	if (swfcl->swfcl_tunnel->tun_af == AF_INET) {
		swfcl->swfcl_tunnel->tun_ipv4_src =
		    brtag->brtag_local.sin.sin_addr;
		swfcl->swfcl_tunnel->tun_ipv4_dst =
		    brtag->brtag_peer.sin.sin_addr;
	} else {
		swfcl->swfcl_tunnel->tun_ipv6_src =
		    brtag->brtag_local.sin6.sin6_addr;
		swfcl->swfcl_tunnel->tun_ipv6_dst =
		    brtag->brtag_peer.sin6.sin6_addr;
	}
	bridge_tunneluntag(m);
 out:
	return switch_flow_classifier_ether(m, offset, swfcl);
}

struct mbuf *
switch_flow_classifier(struct mbuf *m, uint32_t in_port,
    struct switch_flow_classify *swfcl)
{
	int	 offset = 0;

	memset(swfcl, 0, sizeof(*swfcl));
	swfcl->swfcl_in_port = in_port;

	return switch_flow_classifier_tunnel(m, &offset, swfcl);
}

void
switch_flow_classifier_dump(struct switch_softc *sc,
    struct switch_flow_classify *swfcl)
{
	char	saddr[INET6_ADDRSTRLEN], daddr[INET6_ADDRSTRLEN];

	log(LOG_DEBUG, "%s: ", sc->sc_if.if_xname);
	addlog("in_port(%u),", swfcl->swfcl_in_port);

	if (swfcl->swfcl_tunnel) {
		if (swfcl->swfcl_tunnel->tun_af == AF_INET) {
			inet_ntop(AF_INET,
			    (void *)&swfcl->swfcl_tunnel->tun_ipv4_src,
			    saddr, sizeof(saddr));
			inet_ntop(AF_INET,
			    (void *)&swfcl->swfcl_tunnel->tun_ipv4_dst,
			    daddr, sizeof(daddr));
			addlog("tun_ipv4_src(%s),tun_ipv4_dst(%s),"
			    "tun_id(%llu),", saddr, daddr,
			    be64toh(swfcl->swfcl_tunnel->tun_key));
		} else if (swfcl->swfcl_tunnel->tun_af == AF_INET6) {
			inet_ntop(AF_INET6,
			    (void *)&swfcl->swfcl_tunnel->tun_ipv6_src,
			    saddr, sizeof(saddr));
			inet_ntop(AF_INET6,
			    (void *)&swfcl->swfcl_tunnel->tun_ipv6_dst,
			    daddr, sizeof(daddr));
			addlog("tun_ipv6_src(%s) tun_ipv6_dst(%s),"
			    "tun_id(%llu),", saddr, daddr,
			    be64toh(swfcl->swfcl_tunnel->tun_key));
		}
	}

	if (swfcl->swfcl_vlan) {
		addlog("vlan_tpid(0x%0x4x),vlan_pcp(%u),vlan_vid(%u),",
		    ntohs(swfcl->swfcl_vlan->vlan_tpid),
		    swfcl->swfcl_vlan->vlan_pcp,
		    ntohs(swfcl->swfcl_vlan->vlan_vid));
	}

	if (swfcl->swfcl_ether) {
		addlog("eth_dst(%s),eth_src(%s),eth_type(0x%04x)",
		    ether_sprintf(swfcl->swfcl_ether->eth_dst),
		    ether_sprintf(swfcl->swfcl_ether->eth_src),
		    ntohs(swfcl->swfcl_ether->eth_type));
	}

	if (swfcl->swfcl_arp) {
		inet_ntop(AF_INET, (void *)&swfcl->swfcl_arp->arp_sip,
		    saddr, sizeof(saddr));
		inet_ntop(AF_INET, (void *)&swfcl->swfcl_arp->arp_tip,
		    daddr, sizeof(daddr));
		addlog("arp_op(%x),arp_tha(%s),arp_sha(%s),arp_sip(%s),"
		    "arp_tip(%s),", swfcl->swfcl_arp->_arp_op,
		    ether_sprintf(swfcl->swfcl_arp->arp_tha),
		    ether_sprintf(swfcl->swfcl_arp->arp_sha), saddr, daddr);
	}

	if (swfcl->swfcl_ipv4) {
		inet_ntop(AF_INET, (void *)&swfcl->swfcl_ipv4->ipv4_src,
		    saddr, sizeof(saddr));
		inet_ntop(AF_INET, (void *)&swfcl->swfcl_ipv4->ipv4_dst,
		    daddr, sizeof(daddr));
		addlog("ip_proto(%u),ip_tos(%u),ip_ttl(%u),ip_src(%s),"
		    "ip_dst(%s),", swfcl->swfcl_ipv4->ipv4_proto,
		    swfcl->swfcl_ipv4->ipv4_tos, swfcl->swfcl_ipv4->ipv4_ttl,
		    saddr, daddr);
	}

	if (swfcl->swfcl_ipv6) {
		inet_ntop(AF_INET6, (void *)&swfcl->swfcl_ipv6->ipv6_src,
		    saddr, sizeof(saddr));
		inet_ntop(AF_INET6, (void *)&swfcl->swfcl_ipv6->ipv6_dst,
		    daddr, sizeof(daddr));
		addlog("ip6_nxt(%u),ip6_flow_label(%u),ip6_tclass(%d),"
		    "ip6_hlimit(%u),ip6_src(%s),ip6_dst(%s),",
		    swfcl->swfcl_ipv6->ipv6_nxt,
		    ntohl(swfcl->swfcl_ipv6->ipv6_flow_label),
		    swfcl->swfcl_ipv6->ipv6_tclass,
		    swfcl->swfcl_ipv6->ipv6_hlimit, saddr, daddr);
	}

	if (swfcl->swfcl_icmpv4) {
		addlog("icmp_type(%u),icmp_code(%u),",
		    swfcl->swfcl_icmpv4->icmpv4_type,
		    swfcl->swfcl_icmpv4->icmpv4_code);
	}

	if (swfcl->swfcl_icmpv6) {
		addlog("icmp6_type(%u),icmp6_code(%u),",
		    swfcl->swfcl_icmpv6->icmpv6_type,
		    swfcl->swfcl_icmpv6->icmpv6_code);
	}

	if (swfcl->swfcl_nd6) {
		inet_ntop(AF_INET6, (void *)&swfcl->swfcl_nd6->nd6_target,
		    saddr, sizeof(saddr));
		addlog("nd_target(%s),nd_lladdr(%s),", saddr,
		    ether_sprintf(swfcl->swfcl_nd6->nd6_lladdr));
	}

	if (swfcl->swfcl_tcp) {
		addlog("tcp_src(%u),tcp_dst(%u),tcp_flags(%x),",
		    ntohs(swfcl->swfcl_tcp->tcp_src),
		    ntohs(swfcl->swfcl_tcp->tcp_dst),
		    swfcl->swfcl_tcp->tcp_flags);
	}

	if (swfcl->swfcl_udp) {
		addlog("udp_src(%u),udp_dst(%u),",
		    ntohs(swfcl->swfcl_udp->udp_src),
		    ntohs(swfcl->swfcl_udp->udp_dst));
	}

	addlog("\n");
}

int
switch_mtap(caddr_t arg, struct mbuf *m, int dir, uint64_t datapath_id)
{
	struct dlt_openflow_hdr	 of;

	of.of_datapath_id = htobe64(datapath_id);
	of.of_direction = htonl(dir == BPF_DIRECTION_IN ?
	    DLT_OPENFLOW_TO_SWITCH : DLT_OPENFLOW_TO_CONTROLLER);

	return (bpf_mtap_hdr(arg, (caddr_t)&of, sizeof(of), m, dir, NULL));
}

int
ofp_split_mbuf(struct mbuf *m, struct mbuf **mtail)
{
	uint16_t		 ohlen;

	*mtail = NULL;

 again:
	/* We need more data. */
	KASSERT(m->m_flags & M_PKTHDR);
	if (m->m_pkthdr.len < sizeof(struct ofp_header))
		return (-1);

	m_copydata(m, offsetof(struct ofp_header, oh_length), sizeof(ohlen),
	    (caddr_t)&ohlen);
	ohlen = ntohs(ohlen);

	/* We got an invalid packet header, skip it. */
	if (ohlen < sizeof(struct ofp_header)) {
		m_adj(m, sizeof(struct ofp_header));
		goto again;
	}

	/* Nothing to split. */
	if (m->m_pkthdr.len == ohlen)
		return (0);
	else if (m->m_pkthdr.len < ohlen)
		return (-1);

	*mtail = m_split(m, ohlen, M_NOWAIT);
	/* No memory, try again later. */
	if (*mtail == NULL)
		return (-1);

	return (0);
}
