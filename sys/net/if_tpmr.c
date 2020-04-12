/*	$OpenBSD: if_tpmr.c,v 1.10 2020/04/12 06:56:37 dlg Exp $ */

/*
 * Copyright (c) 2019 The University of Queensland
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

/*
 * This code was written by David Gwynne <dlg@uq.edu.au> as part
 * of the Information Technology Infrastructure Group (ITIG) in the
 * Faculty of Engineering, Architecture and Information Technology
 * (EAIT).
 */

#include "bpfilter.h"
#include "pf.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/rwlock.h>
#include <sys/percpu.h>
#include <sys/smr.h>
#include <sys/task.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/if_media.h> /* if_trunk.h uses ifmedia bits */
#include <crypto/siphash.h> /* if_trunk.h uses siphash bits */
#include <net/if_trunk.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif

static const uint8_t	ether_8021_prefix[ETHER_ADDR_LEN - 1] =
    { 0x01, 0x80, 0xc2, 0x00, 0x00 };

#define ETHER_IS_8021_PREFIX(_m) \
    (memcmp((_m), ether_8021_prefix, sizeof(ether_8021_prefix)) == 0)

/*
 * tpmr interface
 */

#define TPMR_NUM_PORTS		2
#define TPMR_TRUNK_PROTO	TRUNK_PROTO_NONE

struct tpmr_softc;

struct tpmr_port {
	struct ifnet		*p_ifp0;

	int (*p_ioctl)(struct ifnet *, u_long, caddr_t);
	int (*p_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);

	struct task		 p_ltask;
	struct task		 p_dtask;

	struct tpmr_softc	*p_tpmr;
	unsigned int		 p_slot;
};

struct tpmr_softc {
	struct ifnet		 sc_if;
	unsigned int		 sc_dead;

	struct tpmr_port	*sc_ports[TPMR_NUM_PORTS];
	unsigned int		 sc_nports;
};

#define DPRINTF(_sc, fmt...)	do { \
	if (ISSET((_sc)->sc_if.if_flags, IFF_DEBUG)) \
		printf(fmt); \
} while (0)

static int	tpmr_clone_create(struct if_clone *, int);
static int	tpmr_clone_destroy(struct ifnet *);

static int	tpmr_ioctl(struct ifnet *, u_long, caddr_t);
static int	tpmr_enqueue(struct ifnet *, struct mbuf *);
static int	tpmr_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
static void	tpmr_start(struct ifqueue *);

static int	tpmr_up(struct tpmr_softc *);
static int	tpmr_down(struct tpmr_softc *);
static int	tpmr_iff(struct tpmr_softc *);

static void	tpmr_p_linkch(void *);
static void	tpmr_p_detach(void *);
static int	tpmr_p_ioctl(struct ifnet *, u_long, caddr_t);
static int	tpmr_p_output(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);

static int	tpmr_get_trunk(struct tpmr_softc *, struct trunk_reqall *);
static void	tpmr_p_dtor(struct tpmr_softc *, struct tpmr_port *,
		    const char *);
static int	tpmr_add_port(struct tpmr_softc *,
		    const struct trunk_reqport *);
static int	tpmr_get_port(struct tpmr_softc *, struct trunk_reqport *);
static int	tpmr_del_port(struct tpmr_softc *,
		    const struct trunk_reqport *);

static struct if_clone tpmr_cloner =
    IF_CLONE_INITIALIZER("tpmr", tpmr_clone_create, tpmr_clone_destroy);

void
tpmrattach(int count)
{
	if_clone_attach(&tpmr_cloner);
}

static int
tpmr_clone_create(struct if_clone *ifc, int unit)
{
	struct tpmr_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO|M_CANFAIL);
	if (sc == NULL)
		return (ENOMEM);

	ifp = &sc->sc_if;

	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d",
	    ifc->ifc_name, unit);

	ifp->if_softc = sc;
	ifp->if_type = IFT_BRIDGE;
	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	ifp->if_mtu = 0;
	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	ifp->if_ioctl = tpmr_ioctl;
	ifp->if_output = tpmr_output;
	ifp->if_enqueue = tpmr_enqueue;
	ifp->if_qstart = tpmr_start;
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_xflags = IFXF_CLONED | IFXF_MPSAFE;
	ifp->if_link_state = LINK_STATE_DOWN;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);

	if_counters_alloc(ifp);
	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, ETHER_HDR_LEN);
#endif

	ifp->if_llprio = IFQ_MAXPRIO;

	return (0);
}

static int
tpmr_clone_destroy(struct ifnet *ifp)
{
	struct tpmr_softc *sc = ifp->if_softc;
	unsigned int i;

	NET_LOCK();
	sc->sc_dead = 1;

	if (ISSET(ifp->if_flags, IFF_RUNNING))
		tpmr_down(sc);
	NET_UNLOCK();

	if_detach(ifp);

	NET_LOCK();
	for (i = 0; i < nitems(sc->sc_ports); i++) {
		struct tpmr_port *p = SMR_PTR_GET_LOCKED(&sc->sc_ports[i]);
		if (p == NULL)
			continue;
		tpmr_p_dtor(sc, p, "destroy");
	}
	NET_UNLOCK();

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

static int
tpmr_8021q_filter(const struct mbuf *m)
{
	const struct ether_header *eh;

	if (m->m_len < sizeof(*eh))
		return (1);

	eh = mtod(m, struct ether_header *);
	if (ETHER_IS_8021_PREFIX(eh->ether_dhost)) {
		switch (eh->ether_dhost[5]) {
		case 0x01: /* IEEE MAC-specific Control Protocols */
		case 0x02: /* IEEE 802.3 Slow Protocols */
		case 0x04: /* IEEE MAC-specific Control Protocols */
		case 0x0e: /* Individual LAN Scope, Nearest Bridge */
			return (1);
		default:
			break;
		}
	}

	return (0);
}

#if NPF > 0
static struct mbuf *
tpmr_pf(struct ifnet *ifp0, int dir, struct mbuf *m)
{
	struct ether_header *eh, copy;
	sa_family_t af = AF_UNSPEC;

	eh = mtod(m, struct ether_header *);
	switch (ntohs(eh->ether_type)) {
	case ETHERTYPE_IP:
		af = AF_INET;
		break;
	case ETHERTYPE_IPV6:
		af = AF_INET6;
		break;
	default:
		return (m);
	}

	copy = *eh;
	m_adj(m, sizeof(*eh));

	if (pf_test(af, dir, ifp0, &m) != PF_PASS) {
		m_freem(m);
		return (NULL);
	}
	if (m == NULL)
		return (NULL);

	m = m_prepend(m, sizeof(*eh), M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	/* checksum? */

	eh = mtod(m, struct ether_header *);
	*eh = copy;

	return (m);
}
#endif /* NPF > 0 */

static int
tpmr_input(struct ifnet *ifp0, struct mbuf *m, void *cookie)
{
	struct tpmr_port *p = cookie;
	struct tpmr_softc *sc = p->p_tpmr;
	struct ifnet *ifp = &sc->sc_if;
	struct tpmr_port *pn;
	int len;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		goto drop;

#if NVLAN > 0
	/*
	 * If the underlying interface removed the VLAN header itself,
	 * add it back.
	 */
	if (ISSET(m->m_flags, M_VLANTAG)) {
		m = vlan_inject(m, ETHERTYPE_VLAN, m->m_pkthdr.ether_vtag);
		if (m == NULL) {
			counters_inc(ifp->if_counters, ifc_ierrors);
			goto drop;
		}
	}
#endif

	if (!ISSET(ifp->if_flags, IFF_LINK0) &&
	    tpmr_8021q_filter(m))
		goto drop;

#if NPF > 0
	if (!ISSET(ifp->if_flags, IFF_LINK1) &&
	    (m = tpmr_pf(ifp0, PF_IN, m)) == NULL)
		return (1);
#endif

	len = m->m_pkthdr.len;
	counters_pkt(ifp->if_counters, ifc_ipackets, ifc_ibytes, len);

#if NBPFILTER > 0
	if_bpf = ifp->if_bpf;
	if (if_bpf) {
		if (bpf_mtap(if_bpf, m, 0))
			goto drop;
	}
#endif

	smr_read_enter();
	pn = SMR_PTR_GET(&sc->sc_ports[!p->p_slot]);
	if (pn == NULL)
		m_freem(m);
	else {
		struct ifnet *ifpn = pn->p_ifp0;

#if NPF > 0
		if (!ISSET(ifp->if_flags, IFF_LINK1) &&
		    (m = tpmr_pf(ifpn, PF_OUT, m)) == NULL)
			;
		else
#endif
		if ((*ifpn->if_enqueue)(ifpn, m))
			counters_inc(ifp->if_counters, ifc_oerrors);
		else {
			counters_pkt(ifp->if_counters,
			    ifc_opackets, ifc_obytes, len);
		}
	}
	smr_read_leave();

	return (1);

drop:
	m_freem(m);
	return (1);
}

static int
tpmr_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	m_freem(m);
	return (ENODEV);
}

static int
tpmr_enqueue(struct ifnet *ifp, struct mbuf *m)
{
	m_freem(m);
	return (ENODEV);
}

static void
tpmr_start(struct ifqueue *ifq)
{
	ifq_purge(ifq);
}

static int
tpmr_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct tpmr_softc *sc = ifp->if_softc;
	int error = 0;

	if (sc->sc_dead)
		return (ENXIO);

	switch (cmd) {
	case SIOCSIFADDR:
		error = EAFNOSUPPORT;
		break;

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = tpmr_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = tpmr_down(sc);
		}
		break;

	case SIOCSTRUNK:
		error = suser(curproc);
		if (error != 0)
			break;

		if (((struct trunk_reqall *)data)->ra_proto !=
		    TRUNK_PROTO_LACP) {
			error = EPROTONOSUPPORT;
			break;
		}

		/* nop */
		break;
	case SIOCGTRUNK:
		error = tpmr_get_trunk(sc, (struct trunk_reqall *)data);
		break;

	case SIOCSTRUNKOPTS:
		error = suser(curproc);
		if (error != 0)
			break;

		error = EPROTONOSUPPORT;
		break;

	case SIOCGTRUNKOPTS:
		break;

	case SIOCGTRUNKPORT:
		error = tpmr_get_port(sc, (struct trunk_reqport *)data);
		break;
	case SIOCSTRUNKPORT:
		error = suser(curproc);
		if (error != 0)
			break;

		error = tpmr_add_port(sc, (struct trunk_reqport *)data);
		break;
	case SIOCSTRUNKDELPORT:
		error = suser(curproc);
		if (error != 0)
			break;

		error = tpmr_del_port(sc, (struct trunk_reqport *)data);
		break;

	default:
		error = ENOTTY;
		break;
	}

	if (error == ENETRESET)
		error = tpmr_iff(sc);

	return (error);
}

static int
tpmr_get_trunk(struct tpmr_softc *sc, struct trunk_reqall *ra)
{
	struct ifnet *ifp = &sc->sc_if;
	size_t size = ra->ra_size;
	caddr_t ubuf = (caddr_t)ra->ra_port;
	int error = 0;
	int i;

	ra->ra_proto = TPMR_TRUNK_PROTO;
	memset(&ra->ra_psc, 0, sizeof(ra->ra_psc));

	ra->ra_ports = sc->sc_nports;
	for (i = 0; i < nitems(sc->sc_ports); i++) {
		struct trunk_reqport rp;
		struct ifnet *ifp0;
		struct tpmr_port *p = SMR_PTR_GET_LOCKED(&sc->sc_ports[i]);
		if (p == NULL)
			continue;

		if (size < sizeof(rp))
			break;

		ifp0 = p->p_ifp0;

		CTASSERT(sizeof(rp.rp_ifname) == sizeof(ifp->if_xname));
		CTASSERT(sizeof(rp.rp_portname) == sizeof(ifp0->if_xname));

		memset(&rp, 0, sizeof(rp));
		memcpy(rp.rp_ifname, ifp->if_xname, sizeof(rp.rp_ifname));
		memcpy(rp.rp_portname, ifp0->if_xname, sizeof(rp.rp_portname));

		if (!ISSET(ifp0->if_flags, IFF_RUNNING))
			SET(rp.rp_flags, TRUNK_PORT_DISABLED);
		else {
			SET(rp.rp_flags, TRUNK_PORT_ACTIVE);
			if (LINK_STATE_IS_UP(ifp0->if_link_state)) {
				SET(rp.rp_flags, TRUNK_PORT_COLLECTING |
				    TRUNK_PORT_DISTRIBUTING);
			}
		}

		error = copyout(&rp, ubuf, sizeof(rp));
		if (error != 0)
			break;

		ubuf += sizeof(rp);
		size -= sizeof(rp);
	}

	return (error);
}

static int
tpmr_add_port(struct tpmr_softc *sc, const struct trunk_reqport *rp)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ifnet *ifp0;
	struct arpcom *ac0;
	struct tpmr_port **pp;
	struct tpmr_port *p;
	int i;
	int error;

	NET_ASSERT_LOCKED();
	if (sc->sc_nports >= nitems(sc->sc_ports))
		return (ENOSPC);

	ifp0 = ifunit(rp->rp_portname);
	if (ifp0 == NULL)
		return (EINVAL);

	if (ifp0->if_type != IFT_ETHER)
		return (EPROTONOSUPPORT);

	ac0 = (struct arpcom *)ifp0;
	if (ac0->ac_trunkport != NULL)
		return (EBUSY);

	/* let's try */

	ifp0 = if_get(ifp0->if_index); /* get an actual reference */
	if (ifp0 == NULL) {
		/* XXX this should never happen */
		return (EINVAL);
	}

	p = malloc(sizeof(*p), M_DEVBUF, M_WAITOK|M_ZERO|M_CANFAIL);
	if (p == NULL) {
		error = ENOMEM;
		goto put;
	}

	p->p_ifp0 = ifp0;
	p->p_tpmr = sc;

	p->p_ioctl = ifp0->if_ioctl;
	p->p_output = ifp0->if_output;

	error = ifpromisc(ifp0, 1);
	if (error != 0)
		goto free;

	task_set(&p->p_ltask, tpmr_p_linkch, p);
	if_linkstatehook_add(ifp0, &p->p_ltask);

	task_set(&p->p_dtask, tpmr_p_detach, p);
	if_detachhook_add(ifp0, &p->p_dtask);

	/* commit */
	DPRINTF(sc, "%s %s trunkport: creating port\n",
	    ifp->if_xname, ifp0->if_xname);

	for (i = 0; i < nitems(sc->sc_ports); i++) {
		pp = &sc->sc_ports[i];
		if (SMR_PTR_GET_LOCKED(pp) == NULL)
			break;
	}
	sc->sc_nports++;

	p->p_slot = i;

	ac0->ac_trunkport = p;
	/* make sure p is visible before handlers can run */
	membar_producer();
	ifp0->if_ioctl = tpmr_p_ioctl;
	ifp0->if_output = tpmr_p_output;
	if_ih_insert(ifp0, tpmr_input, p);

	SMR_PTR_SET_LOCKED(pp, p);

	tpmr_p_linkch(p);

	return (0);

free:
	free(p, M_DEVBUF, sizeof(*p));
put:
	if_put(ifp0);
	return (error);
}

static struct tpmr_port *
tpmr_trunkport(struct tpmr_softc *sc, const char *name)
{
	unsigned int i;

	for (i = 0; i < nitems(sc->sc_ports); i++) {
		struct tpmr_port *p = SMR_PTR_GET_LOCKED(&sc->sc_ports[i]);
		if (p == NULL)
			continue;

		if (strcmp(p->p_ifp0->if_xname, name) == 0)
			return (p);
	}

	return (NULL);
}

static int
tpmr_get_port(struct tpmr_softc *sc, struct trunk_reqport *rp)
{
	struct tpmr_port *p;

	NET_ASSERT_LOCKED();
	p = tpmr_trunkport(sc, rp->rp_portname);
	if (p == NULL)
		return (EINVAL);

	/* XXX */

	return (0);
}

static int
tpmr_del_port(struct tpmr_softc *sc, const struct trunk_reqport *rp)
{
	struct tpmr_port *p;

	NET_ASSERT_LOCKED();
	p = tpmr_trunkport(sc, rp->rp_portname);
	if (p == NULL)
		return (EINVAL);

	tpmr_p_dtor(sc, p, "del");

	return (0);
}

static int
tpmr_p_ioctl(struct ifnet *ifp0, u_long cmd, caddr_t data)
{
	struct arpcom *ac0 = (struct arpcom *)ifp0;
	struct tpmr_port *p = ac0->ac_trunkport;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		error = EBUSY;
		break;

	case SIOCGTRUNKPORT: {
		struct trunk_reqport *rp = (struct trunk_reqport *)data;
		struct tpmr_softc *sc = p->p_tpmr;
		struct ifnet *ifp = &sc->sc_if;

		if (strncmp(rp->rp_ifname, rp->rp_portname,
		    sizeof(rp->rp_ifname)) != 0)
			return (EINVAL);

		CTASSERT(sizeof(rp->rp_ifname) == sizeof(ifp->if_xname));
		memcpy(rp->rp_ifname, ifp->if_xname, sizeof(rp->rp_ifname));
		break;
	}

	default:
		error = (*p->p_ioctl)(ifp0, cmd, data);
		break;
	}

	return (error);
}

static int
tpmr_p_output(struct ifnet *ifp0, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	struct arpcom *ac0 = (struct arpcom *)ifp0;
	struct tpmr_port *p = ac0->ac_trunkport;

	/* restrict transmission to bpf only */
	if ((m_tag_find(m, PACKET_TAG_DLT, NULL) == NULL)) {
		m_freem(m);
		return (EBUSY);
	}

	return ((*p->p_output)(ifp0, m, dst, rt));
}

static void
tpmr_p_dtor(struct tpmr_softc *sc, struct tpmr_port *p, const char *op)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ifnet *ifp0 = p->p_ifp0;
	struct arpcom *ac0 = (struct arpcom *)ifp0;

	DPRINTF(sc, "%s %s: destroying port\n",
	    ifp->if_xname, ifp0->if_xname);

	if_ih_remove(ifp0, tpmr_input, p);

	ifp0->if_ioctl = p->p_ioctl;
	ifp0->if_output = p->p_output;
	membar_producer();

	ac0->ac_trunkport = NULL;

	sc->sc_nports--;
	SMR_PTR_SET_LOCKED(&sc->sc_ports[p->p_slot], NULL);

	if (ifpromisc(ifp0, 0) != 0) {
		log(LOG_WARNING, "%s %s: unable to disable promisc\n",
		    ifp->if_xname, ifp0->if_xname);
	}

	if_detachhook_del(ifp0, &p->p_dtask);
	if_linkstatehook_del(ifp0, &p->p_ltask);

	smr_barrier();

	if_put(ifp0);
	free(p, M_DEVBUF, sizeof(*p));

	if (ifp->if_link_state != LINK_STATE_DOWN) {
		ifp->if_link_state = LINK_STATE_DOWN;
		if_link_state_change(ifp);
	}
}

static void
tpmr_p_detach(void *arg)
{
	struct tpmr_port *p = arg;
	struct tpmr_softc *sc = p->p_tpmr;

	tpmr_p_dtor(sc, p, "detach");

	NET_ASSERT_LOCKED();
}

static int
tpmr_p_active(struct tpmr_port *p)
{
	struct ifnet *ifp0 = p->p_ifp0;

	return (ISSET(ifp0->if_flags, IFF_RUNNING) &&
	    LINK_STATE_IS_UP(ifp0->if_link_state));
}

static void
tpmr_p_linkch(void *arg)
{
	struct tpmr_port *p = arg;
	struct tpmr_softc *sc = p->p_tpmr;
	struct ifnet *ifp = &sc->sc_if;
	struct tpmr_port *np;
	u_char link_state = LINK_STATE_FULL_DUPLEX;

	NET_ASSERT_LOCKED();

	if (!tpmr_p_active(p))
		link_state = LINK_STATE_DOWN;

	np = SMR_PTR_GET_LOCKED(&sc->sc_ports[!p->p_slot]);
	if (np == NULL || !tpmr_p_active(np))
		link_state = LINK_STATE_DOWN;

	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}

static int
tpmr_up(struct tpmr_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;

	NET_ASSERT_LOCKED();
	SET(ifp->if_flags, IFF_RUNNING);

	return (0);
}

static int
tpmr_iff(struct tpmr_softc *sc)
{
	return (0);
}

static int
tpmr_down(struct tpmr_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;

	NET_ASSERT_LOCKED();
	CLR(ifp->if_flags, IFF_RUNNING);

	return (0);
}
