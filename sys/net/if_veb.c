/*	$OpenBSD: if_veb.c,v 1.68 2025/12/11 06:02:11 dlg Exp $ */

/*
 * Copyright (c) 2021 David Gwynne <dlg@openbsd.org>
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
#include "kstat.h"
#include "pf.h"
#include "vlan.h"

#include <sys/param.h>
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
#include <sys/kstat.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/if_bridge.h>
#include <net/if_etherbridge.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif

/* there are (basically) 4096 vids (vlan tags) */
#define VEB_VID_COUNT		4096
#define VEB_VID_BYTES		(VEB_VID_COUNT / 8)
#define VEB_VID_WORDS		(VEB_VID_BYTES / sizeof(uint32_t))

/* SIOCBRDGIFFLGS, SIOCBRDGIFFLGS */
#define VEB_IFBIF_FLAGS	\
	(IFBIF_PVLAN_PTAGS|IFBIF_LOCKED|IFBIF_LEARNING|IFBIF_DISCOVER|IFBIF_BLOCKNONIP)

struct veb_rule {
	TAILQ_ENTRY(veb_rule)		vr_entry;
	SMR_TAILQ_ENTRY(veb_rule)	vr_lentry[2];

	uint16_t			vr_flags;
#define VEB_R_F_IN				(1U << 0)
#define VEB_R_F_OUT				(1U << 1)
#define VEB_R_F_SRC				(1U << 2)
#define VEB_R_F_DST				(1U << 3)

#define VEB_R_F_ARP				(1U << 4)
#define VEB_R_F_RARP				(1U << 5)
#define VEB_R_F_SHA				(1U << 6)
#define VEB_R_F_SPA				(1U << 7)
#define VEB_R_F_THA				(1U << 8)
#define VEB_R_F_TPA				(1U << 9)
	uint16_t			 vr_arp_op;

	uint64_t			 vr_src;
	uint64_t			 vr_dst;
	struct ether_addr		 vr_arp_sha;
	struct ether_addr		 vr_arp_tha;
	struct in_addr			 vr_arp_spa;
	struct in_addr			 vr_arp_tpa;

	unsigned int			 vr_action;
#define VEB_R_MATCH				0
#define VEB_R_PASS				1
#define VEB_R_BLOCK				2

	int				 vr_pftag;
};

TAILQ_HEAD(veb_rules, veb_rule);
SMR_TAILQ_HEAD(veb_rule_list, veb_rule);

struct veb_softc;

enum veb_port_counters {
	veb_c_double_tag,
	veb_c_tagged_filter_in,
	veb_c_untagged_none,
	veb_c_pvptags_in,
	veb_c_locked,
	veb_c_bpfilter,
	veb_c_blocknonip_in,
	veb_c_svlan,
	veb_c_rule_in,

	veb_c_hairpin,
	veb_c_protected,
	veb_c_pvlan,
	veb_c_pvptags_out,
	veb_c_tagged_filter_out,
	veb_c_rule_out,
	veb_c_blocknonip_out,

	veb_c_ncounters
};

struct veb_port_cpu {
	struct refcnt			 c_refs;
	struct pc_lock			 c_lock;
	uint64_t			 c_counters[veb_c_ncounters];
};

struct veb_port {
	struct ifnet			*p_ifp0;
	struct refcnt			 p_refs;
	struct cpumem			*p_percpu;
	struct kstat			*p_kstat;

	int (*p_enqueue)(struct ifnet *, struct mbuf *, struct netstack *);

	int (*p_ioctl)(struct ifnet *, u_long, caddr_t);
	int (*p_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);

	struct task			 p_ltask;
	struct task			 p_dtask;

	struct veb_softc		*p_veb;

	struct ether_port		 p_brport;

	unsigned int			 p_link_state;
	unsigned int			 p_bif_flags;
	uint32_t			 p_protected;
	uint16_t			 p_pvid;
	uint32_t			*p_vid_map;

	struct veb_rules		 p_vrl;
	unsigned int			 p_nvrl;
	struct veb_rule_list		 p_vr_list[2];
#define VEB_RULE_LIST_OUT			0
#define VEB_RULE_LIST_IN			1
};

static inline void
veb_p_take(struct veb_port *p)
{
	refcnt_take(&p->p_refs);
}

static inline void
veb_p_rele(struct veb_port *p)
{
	refcnt_rele_wake(&p->p_refs);
}

struct veb_ports {
	struct refcnt			 m_refs;
	unsigned int			 m_count;

	/* followed by an array of veb_port pointers */
};

struct veb_pvlan {
	RBT_ENTRY(veb_pvlan)		 v_entry;
	uint16_t			 v_primary;
	uint16_t			 v_secondary;
#define v_isolated			 v_secondary
	unsigned int			 v_type;
};

RBT_HEAD(veb_pvlan_vp, veb_pvlan);
RBT_HEAD(veb_pvlan_vs, veb_pvlan);

struct veb_softc {
	struct ifnet			 sc_if;
	unsigned int			 sc_dead;

	struct etherbridge		 sc_eb;
	int				 sc_dflt_pvid;
	int				 sc_txprio;
	int				 sc_rxprio;

	struct rwlock			 sc_rule_lock;
	struct veb_ports		*sc_ports;
	struct veb_ports		*sc_spans;

	/*
	 * pvlan topology is stored twice:
	 *
	 * once in an array hanging off sc_pvlans for the forwarding path.
	 * entries in sc_pvlans are indexed by the secondary vid (Vs), and
	 * stores the primary vid (Vp) the Vs is associated with and the
	 * type of relationship Vs has with Vp.
	 *
	 * primary vids have an entry filled with their own vid to indicate
	 * that the vid is in use.
	 *
	 * vids without pvlan configuration have 0 in their sc_pvlans entry.
	 */
	uint16_t			*sc_pvlans;
#define VEB_PVLAN_V_MASK	EVL_VLID_MASK
#define VEB_PVLAN_T_PRIMARY	(0 << 12)
#define VEB_PVLAN_T_ISOLATED	(1 << 12)
#define VEB_PVLAN_T_COMMUNITY	(2 << 12)
#define VEB_PVLAN_T_MASK	(3 << 12)

	/*
	 * the pvlan topology is stored again in trees for the
	 * ioctls. technically the ioctl code could brute force through
	 * the sc_pvlans above, but this seemed like a good idea at
	 * the time.
	 *
	 * primary vids are stored in their own sc_pvlans_vp tree.
	 * there can only be one isolaved vid (Vi) per pvlan, which
	 * is managed using the v_isolated (v_secondary) id member
	 * in the primary veb_vplan struct here.
	 *
	 * secondary vids are stored in the sc_pvlans_vs tree.
	 * they're ordered by Vp, type, and Vs to make it easy to
	 * find pvlans for userland.
	 */
	struct veb_pvlan_vp		 sc_pvlans_vp;
	struct veb_pvlan_vs		 sc_pvlans_vs;

	/*
	 * this is incremented when the pvlan topology changes, and
	 * copied into the FINDPV and NFINDPV ioctl results so userland
	 * can tell if a change has happened across multiple queries.
	 */
	unsigned int			 sc_pvlans_gen;
};

#define DPRINTF(_sc, fmt...)    do { \
	if (ISSET((_sc)->sc_if.if_flags, IFF_DEBUG)) \
		printf(fmt); \
} while (0)

static int	veb_clone_create(struct if_clone *, int);
static int	veb_clone_destroy(struct ifnet *);

static int	veb_ioctl(struct ifnet *, u_long, caddr_t);
static void	veb_input(struct ifnet *, struct mbuf *, struct netstack *);
static int	veb_enqueue(struct ifnet *, struct mbuf *);
static int	veb_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
static void	veb_start(struct ifqueue *);

static int	veb_up(struct veb_softc *);
static int	veb_down(struct veb_softc *);
static int	veb_iff(struct veb_softc *);

static void	veb_p_linkch(void *);
static void	veb_p_detach(void *);
static int	veb_p_ioctl(struct ifnet *, u_long, caddr_t);
static int	veb_p_output(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);

static inline size_t
veb_ports_size(unsigned int n)
{
	/* use of _ALIGN is inspired by CMSGs */
	return _ALIGN(sizeof(struct veb_ports)) +
	    n * sizeof(struct veb_port *);
}

static inline struct veb_port **
veb_ports_array(struct veb_ports *m)
{
	return (struct veb_port **)((caddr_t)m + _ALIGN(sizeof(*m)));
}

static inline void veb_ports_free(struct veb_ports *);

static void	veb_p_unlink(struct veb_softc *, struct veb_port *);
static void	veb_p_fini(struct veb_port *);
static void	veb_p_dtor(struct veb_softc *, struct veb_port *);
static int	veb_add_port(struct veb_softc *,
		    const struct ifbreq *, unsigned int);
static int	veb_del_port(struct veb_softc *,
		    const struct ifbreq *, unsigned int);
static int	veb_port_list(struct veb_softc *, struct ifbifconf *);
static int	veb_port_set_flags(struct veb_softc *, struct ifbreq *);
static int	veb_port_get_flags(struct veb_softc *, struct ifbreq *);
static int	veb_port_set_protected(struct veb_softc *,
		    const struct ifbreq *);
static int	veb_port_set_pvid(struct veb_softc *,
		    const struct ifbreq *);
static int	veb_add_addr(struct veb_softc *, const struct ifbareq *);
static int	veb_add_vid_addr(struct veb_softc *, const struct ifbvareq *);
static int	veb_del_addr(struct veb_softc *, const struct ifbareq *);
static int	veb_del_vid_addr(struct veb_softc *, const struct ifbvareq *);

static int	veb_get_vid_map(struct veb_softc *, struct ifbrvidmap *);
static int	veb_set_vid_map(struct veb_softc *, const struct ifbrvidmap *);

static int	veb_add_pvlan(struct veb_softc *, const struct ifbrpvlan *);
static int	veb_del_pvlan(struct veb_softc *, const struct ifbrpvlan *);
static int	veb_find_pvlan(struct veb_softc *, struct ifbrpvlan *);
static int	veb_nfind_pvlan(struct veb_softc *, struct ifbrpvlan *);
static uint16_t	veb_pvlan(struct veb_softc *, uint16_t);

static int	veb_rule_add(struct veb_softc *, const struct ifbrlreq *);
static int	veb_rule_list_flush(struct veb_softc *,
		    const struct ifbrlreq *);
static void	veb_rule_list_free(struct veb_rule *);
static int	veb_rule_list_get(struct veb_softc *, struct ifbrlconf *);

static int	 veb_eb_port_cmp(void *, void *, void *);
static void	*veb_eb_port_take(void *, void *);
static void	 veb_eb_port_rele(void *, void *);
static size_t	 veb_eb_port_ifname(void *, char *, size_t, void *);
static void	 veb_eb_port_sa(void *, struct sockaddr_storage *, void *);

static void	*veb_ep_brport_take(void *);
static void	 veb_ep_brport_rele(void *, void *);

#if NKSTAT > 0
static void	 veb_port_kstat_attach(struct veb_port *);
static void	 veb_port_kstat_detach(struct veb_port *);
#endif

static const struct etherbridge_ops veb_etherbridge_ops = {
	veb_eb_port_cmp,
	veb_eb_port_take,
	veb_eb_port_rele,
	veb_eb_port_ifname,
	veb_eb_port_sa,
};

static inline int
veb_pvlan_vp_cmp(const struct veb_pvlan *a, const struct veb_pvlan *b)
{
	if (a->v_primary < b->v_primary)
		return (-1);
	if (a->v_primary > b->v_primary)
		return (1);
	return (0);
}

RBT_PROTOTYPE(veb_pvlan_vp, veb_pvlan, v_entry, veb_pvlan_vp_cmp);

static inline int
veb_pvlan_vs_cmp(const struct veb_pvlan *a, const struct veb_pvlan *b)
{
	int rv;

	rv = veb_pvlan_vp_cmp(a, b);
	if (rv != 0)
		return (rv);

	if (a->v_type < b->v_type)
		return (-1);
	if (a->v_type > b->v_type)
		return (1);

	if (a->v_secondary < b->v_secondary)
		return (-1);
	if (a->v_secondary > b->v_secondary)
		return (1);

	return (0);
}

RBT_PROTOTYPE(veb_pvlan_vs, veb_pvlan, v_entry, veb_pvlan_vs_cmp);

static struct if_clone veb_cloner =
    IF_CLONE_INITIALIZER("veb", veb_clone_create, veb_clone_destroy);

static struct pool veb_rule_pool;

static int	vport_clone_create(struct if_clone *, int);
static int	vport_clone_destroy(struct ifnet *);

struct vport_softc {
	struct arpcom		 sc_ac;
	unsigned int		 sc_dead;
};

static int	vport_if_enqueue(struct ifnet *, struct mbuf *,
		    struct netstack *);

static int	vport_ioctl(struct ifnet *, u_long, caddr_t);
static int	vport_enqueue(struct ifnet *, struct mbuf *);
static void	vport_start(struct ifqueue *);

static int	vport_up(struct vport_softc *);
static int	vport_down(struct vport_softc *);
static int	vport_iff(struct vport_softc *);

static struct if_clone vport_cloner =
    IF_CLONE_INITIALIZER("vport", vport_clone_create, vport_clone_destroy);

void
vebattach(int count)
{
	if_clone_attach(&veb_cloner);
	if_clone_attach(&vport_cloner);
}

static int
veb_clone_create(struct if_clone *ifc, int unit)
{
	struct veb_softc *sc;
	struct ifnet *ifp;
	int error;

	if (veb_rule_pool.pr_size == 0) {
		pool_init(&veb_rule_pool, sizeof(struct veb_rule),
		    0, IPL_SOFTNET, 0, "vebrpl", NULL);
	}

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO|M_CANFAIL);
	if (sc == NULL)
		return (ENOMEM);

	rw_init(&sc->sc_rule_lock, "vebrlk");
	sc->sc_ports = NULL;
	sc->sc_spans = NULL;
	RBT_INIT(veb_pvlan_vp, &sc->sc_pvlans_vp);
	RBT_INIT(veb_pvlan_vs, &sc->sc_pvlans_vs);

	ifp = &sc->sc_if;

	snprintf(ifp->if_xname, IFNAMSIZ, "%s%d", ifc->ifc_name, unit);

	error = etherbridge_init(&sc->sc_eb, ifp->if_xname,
	    &veb_etherbridge_ops, sc);
	if (error != 0) {
		free(sc, M_DEVBUF, sizeof(*sc));
		return (error);
	}

	sc->sc_dflt_pvid = 1;
	sc->sc_txprio = IF_HDRPRIO_PACKET;
	sc->sc_rxprio = IF_HDRPRIO_OUTER;

	ifp->if_softc = sc;
	ifp->if_type = IFT_BRIDGE;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	ifp->if_ioctl = veb_ioctl;
	ifp->if_input = veb_input;
	ifp->if_output = veb_output;
	ifp->if_enqueue = veb_enqueue;
	ifp->if_qstart = veb_start;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED | IFXF_MPSAFE;

	if_counters_alloc(ifp);
	if_attach(ifp);

	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, ETHER_HDR_LEN);
#endif

	return (0);
}

static int
veb_clone_destroy(struct ifnet *ifp)
{
	struct veb_softc *sc = ifp->if_softc;
	struct veb_ports *mp, *ms;
	struct veb_port **ps;
	struct veb_port *p;
	struct veb_pvlan *v, *nv;
	unsigned int i;

	NET_LOCK();
	sc->sc_dead = 1;

	if (ISSET(ifp->if_flags, IFF_RUNNING))
		veb_down(sc);
	NET_UNLOCK();

	if_detach(ifp);

	NET_LOCK();

	/*
	 * this is an upside down version of veb_p_dtor() and
	 * veb_ports_destroy() to avoid a lot of malloc/free and
	 * smr_barrier calls if we remove ports one by one.
	 */

	mp = SMR_PTR_GET_LOCKED(&sc->sc_ports);
	SMR_PTR_SET_LOCKED(&sc->sc_ports, NULL);
	if (mp != NULL) {
		ps = veb_ports_array(mp);
		for (i = 0; i < mp->m_count; i++)
			veb_p_unlink(sc, ps[i]);
	}

	ms = SMR_PTR_GET_LOCKED(&sc->sc_spans);
	SMR_PTR_SET_LOCKED(&sc->sc_spans, NULL);
	if (ms != NULL) {
		ps = veb_ports_array(ms);
		for (i = 0; i < ms->m_count; i++)
			veb_p_unlink(sc, ps[i]);
	}

	smr_barrier(); /* everything everywhere all at once */
	if (mp != NULL || ms != NULL) {

		if (mp != NULL) {
			refcnt_finalize(&mp->m_refs, "vebdtor");

			ps = veb_ports_array(mp);
			for (i = 0; i < mp->m_count; i++) {
				p = ps[i];
				/* the ports map holds a port ref */
				veb_p_rele(p);
				/* now we can finalize the port */
				veb_p_fini(p);
			}

			veb_ports_free(mp);
		}
		if (ms != NULL) {
			refcnt_finalize(&ms->m_refs, "vebdtor");

			ps = veb_ports_array(ms);
			for (i = 0; i < ms->m_count; i++) {
				p = ps[i];
				/* the ports map holds a port ref */
				veb_p_rele(p);
				/* now we can finalize the port */
				veb_p_fini(p);
			}

			veb_ports_free(ms);
		}
	}
	NET_UNLOCK();

	etherbridge_destroy(&sc->sc_eb);

	RBT_FOREACH_SAFE(v, veb_pvlan_vp, &sc->sc_pvlans_vp, nv) {
		RBT_REMOVE(veb_pvlan_vp, &sc->sc_pvlans_vp, v);
		free(v, M_IFADDR, sizeof(*v));
	}
	RBT_FOREACH_SAFE(v, veb_pvlan_vs, &sc->sc_pvlans_vs, nv) {
		RBT_REMOVE(veb_pvlan_vs, &sc->sc_pvlans_vs, v);
		free(v, M_IFADDR, sizeof(*v));
	}
	free(sc->sc_pvlans, M_IFADDR, VEB_VID_COUNT * sizeof(*sc->sc_pvlans));

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

static struct mbuf *
veb_span_input(struct ifnet *ifp0, struct mbuf *m, uint64_t dst, void *brport,
    struct netstack *ns)
{
	m_freem(m);
	return (NULL);
}

static void
veb_span(struct veb_softc *sc, struct mbuf *m0)
{
	struct veb_ports *sm;
	struct veb_port **ps;
	struct veb_port *p;
	struct ifnet *ifp0;
	struct mbuf *m;
	unsigned int i;

	smr_read_enter();
	sm = SMR_PTR_GET(&sc->sc_spans);
	if (sm != NULL)
		refcnt_take(&sm->m_refs);
	smr_read_leave();
	if (sm == NULL)
		return;

	ps = veb_ports_array(sm);
	for (i = 0; i < sm->m_count; i++) {
		p = ps[i];

		ifp0 = p->p_ifp0;
		if (!ISSET(ifp0->if_flags, IFF_RUNNING))
			continue;

		m = m_dup_pkt(m0, max_linkhdr + ETHER_ALIGN, M_NOWAIT);
		if (m == NULL) {
			/* XXX count error */
			continue;
		}

		m = ether_offload_ifcap(ifp0, m);
		if (m == NULL) {
			counters_inc(sc->sc_if.if_counters, ifc_oerrors);
			continue;
		}

		if_enqueue(ifp0, m); /* XXX count error */
	}
	refcnt_rele_wake(&sm->m_refs);
}

static int
veb_ip_filter(const struct mbuf *m)
{
	const struct ether_header *eh;

	eh = mtod(m, struct ether_header *);
	switch (ntohs(eh->ether_type)) {
	case ETHERTYPE_IP:
	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
	case ETHERTYPE_IPV6:
		return (0);
	default:
		break;
	}

	return (1);
}

static int
veb_svlan_filter(const struct mbuf *m)
{
	const struct ether_header *eh;

	eh = mtod(m, struct ether_header *);

	return (eh->ether_type == htons(ETHERTYPE_QINQ));
}

static int
veb_vid_map_filter(struct veb_port *p, uint16_t vid)
{
	uint32_t *map;
	int drop = 1;

	smr_read_enter();
	map = SMR_PTR_GET(&p->p_vid_map);
	if (map != NULL) {
		unsigned int off = vid / 32;
		unsigned int bit = vid % 32;

		drop = !ISSET(map[off], 1U << bit);
	}
	smr_read_leave();

	return (drop);
}

static int
veb_rule_arp_match(const struct veb_rule *vr, struct mbuf *m)
{
	struct ether_header *eh;
	struct ether_arp ea;

	eh = mtod(m, struct ether_header *);

	if (eh->ether_type != htons(ETHERTYPE_ARP))
		return (0);
	if (m->m_pkthdr.len < sizeof(*eh) + sizeof(ea))
		return (0);

	m_copydata(m, sizeof(*eh), sizeof(ea), (caddr_t)&ea);

	if (ea.arp_hrd != htons(ARPHRD_ETHER) ||
	    ea.arp_pro != htons(ETHERTYPE_IP) ||
	    ea.arp_hln != ETHER_ADDR_LEN ||
	    ea.arp_pln != sizeof(struct in_addr))
		return (0);

	if (ISSET(vr->vr_flags, VEB_R_F_ARP)) {
		if (ea.arp_op != htons(ARPOP_REQUEST) &&
		    ea.arp_op != htons(ARPOP_REPLY))
			return (0);
	}
	if (ISSET(vr->vr_flags, VEB_R_F_RARP)) {
		if (ea.arp_op != htons(ARPOP_REVREQUEST) &&
		    ea.arp_op != htons(ARPOP_REVREPLY))
			return (0);
	}

	if (vr->vr_arp_op != htons(0) && vr->vr_arp_op != ea.arp_op)
		return (0);

	if (ISSET(vr->vr_flags, VEB_R_F_SHA) &&
	    !ETHER_IS_EQ(&vr->vr_arp_sha, ea.arp_sha))
		return (0);
	if (ISSET(vr->vr_flags, VEB_R_F_THA) &&
	    !ETHER_IS_EQ(&vr->vr_arp_tha, ea.arp_tha))
		return (0);
	if (ISSET(vr->vr_flags, VEB_R_F_SPA) &&
	    memcmp(&vr->vr_arp_spa, ea.arp_spa, sizeof(vr->vr_arp_spa)) != 0)
		return (0);
	if (ISSET(vr->vr_flags, VEB_R_F_TPA) &&
	    memcmp(&vr->vr_arp_tpa, ea.arp_tpa, sizeof(vr->vr_arp_tpa)) != 0)
		return (0);

	return (1);
}

static int
veb_rule_list_test(struct veb_rule *vr, int dir, struct mbuf *m,
    uint64_t src, uint64_t dst, uint16_t vid)
{
	SMR_ASSERT_CRITICAL();

	do {
		/* XXX check vid */

		if (ISSET(vr->vr_flags, VEB_R_F_ARP|VEB_R_F_RARP) &&
		    !veb_rule_arp_match(vr, m))
			continue;

		if (ISSET(vr->vr_flags, VEB_R_F_SRC) &&
		    vr->vr_src != src)
			continue;
		if (ISSET(vr->vr_flags, VEB_R_F_DST) &&
		    vr->vr_dst != dst)
			continue;

		if (vr->vr_action == VEB_R_BLOCK)
			return (VEB_R_BLOCK);
#if NPF > 0
		pf_tag_packet(m, vr->vr_pftag, -1);
#endif
		if (vr->vr_action == VEB_R_PASS)
			return (VEB_R_PASS);
	} while ((vr = SMR_TAILQ_NEXT(vr, vr_lentry[dir])) != NULL);

	return (VEB_R_PASS);
}

static inline int
veb_rule_filter(struct veb_port *p, int dir, struct mbuf *m,
    uint64_t src, uint64_t dst, uint16_t vid)
{
	struct veb_rule *vr;
	int filter = VEB_R_PASS;

	smr_read_enter();
	vr = SMR_TAILQ_FIRST(&p->p_vr_list[dir]);
	if (vr != NULL)
		filter = veb_rule_list_test(vr, dir, m, src, dst, vid);
	smr_read_leave();

	return (filter == VEB_R_BLOCK);
}

#if NPF > 0
struct veb_pf_ip_family {
	sa_family_t	   af;
	struct mbuf	*(*ip_check)(struct ifnet *, struct mbuf *);
	void		 (*ip_input)(struct ifnet *, struct mbuf *,
			    struct netstack *);
};

static const struct veb_pf_ip_family veb_pf_ipv4 = {
	.af		= AF_INET,
	.ip_check	= ipv4_check,
	.ip_input	= ipv4_input,
};

#ifdef INET6
static const struct veb_pf_ip_family veb_pf_ipv6 = {
	.af		= AF_INET6,
	.ip_check	= ipv6_check,
	.ip_input	= ipv6_input,
};
#endif

static struct mbuf *
veb_pf(struct ifnet *ifp0, int dir, struct mbuf *m, struct netstack *ns)
{
	struct ether_header *eh, copy;
	const struct veb_pf_ip_family *fam;
	int hlen;

	/*
	 * pf runs on vport interfaces when they enter or leave the
	 * l3 stack, so don't confuse things (even more) by running
	 * pf again here. note that because of this exception the
	 * pf direction on vport interfaces is reversed compared to
	 * other veb ports.
	 */
	if (ifp0->if_enqueue == vport_enqueue)
		return (m);

	eh = mtod(m, struct ether_header *);
	switch (ntohs(eh->ether_type)) {
	case ETHERTYPE_IP:
		fam = &veb_pf_ipv4;
		break;
#ifdef INET6
	case ETHERTYPE_IPV6:
		fam = &veb_pf_ipv6;
		break;
#endif
	default:
		return (m);
	}

	copy = *eh;
	m_adj(m, sizeof(*eh));

	m = (*fam->ip_check)(ifp0, m);
	if (m == NULL)
		return (NULL);

	if (pf_test(fam->af, dir, ifp0, &m) != PF_PASS) {
		m_freem(m);
		return (NULL);
	}
	if (m == NULL)
		return (NULL);

	if (dir == PF_IN && ISSET(m->m_pkthdr.pf.flags, PF_TAG_DIVERTED)) {
		pf_mbuf_unlink_state_key(m);
		pf_mbuf_unlink_inpcb(m);
		if_input_proto(ifp0, m, fam->ip_input, ns);
		return (NULL);
	}

	hlen = roundup(sizeof(*eh), sizeof(long));
	m = m_prepend(m, hlen, M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	/* checksum? */

	m_adj(m, hlen - sizeof(*eh));
	eh = mtod(m, struct ether_header *);
	*eh = copy;

	return (m);
}
#endif /* NPF > 0 */

struct veb_ctx {
	struct netstack		*ns;
	struct veb_port		*p;
	uint64_t		 src;
	uint64_t		 dst;
	uint16_t		 vp;	/* primary vlan */
	uint16_t		 vs;	/* secondary vlan */
	uint16_t		 vt;	/* secondary vlan type */
};

static int
veb_pvlan_filter(struct veb_softc *sc, const struct veb_ctx *ctx, uint16_t vs)
{
	uint16_t pvlan;

	smr_read_enter();
	pvlan = veb_pvlan(sc, vs);
	smr_read_leave();

	/* are we in the same pvlan? */
	if (ctx->vp != (pvlan & VEB_PVLAN_V_MASK))
		return (1);

	switch (ctx->vt) {
	case VEB_PVLAN_T_PRIMARY:
		/* primary ports are permitted to send to anything */
		break;

	case VEB_PVLAN_T_COMMUNITY:
		/* same communities are permitted */
		if (ctx->vs == vs)
			break;

		/* FALLTHROUGH */
	case VEB_PVLAN_T_ISOLATED:
		/* isolated (or community) can only send to a primary port */
		if (ctx->vp == vs)
			break;

		return (1);
	}

	return (0);
}

static int
veb_if_enqueue(struct ifnet *ifp, struct mbuf *m, struct netstack *ns)
{
	return (if_enqueue(ifp, m));
}

static void
veb_port_count(struct veb_port *p, enum veb_port_counters counter)
{
	struct veb_port_cpu *c;
	unsigned int gen;

	c = cpumem_enter(p->p_percpu);
	gen = pc_sprod_enter(&c->c_lock);
	c->c_counters[counter]++;
	pc_sprod_leave(&c->c_lock, gen);
	cpumem_leave(p->p_percpu, c);
}

static void
veb_broadcast(struct veb_softc *sc, struct veb_ctx *ctx, struct mbuf *m0)
{
	struct ifnet *ifp = &sc->sc_if;
	struct veb_ports *pm;
	struct veb_port **ps;
	struct ifnet *ifp0;
	struct mbuf *m;
	unsigned int i;

	if (ctx->p->p_pvid == ctx->vs) { /* XXX which vlan is the right one? */
#if NPF > 0
		/*
		 * we couldn't find a specific port to send this packet to,
		 * but pf should still have a chance to apply policy to it.
		 * let pf look at it, but use the veb interface as a proxy.
		 */
		if (ISSET(ifp->if_flags, IFF_LINK1) &&
		    (m0 = veb_pf(ifp, PF_FWD, m0, ctx->ns)) == NULL)
			return;
#endif
	}

	counters_pkt(ifp->if_counters, ifc_opackets, ifc_obytes,
	    m0->m_pkthdr.len);

	smr_read_enter();
	pm = SMR_PTR_GET(&sc->sc_ports);
	if (__predict_true(pm != NULL))
		refcnt_take(&pm->m_refs);
	smr_read_leave();
	if (__predict_false(pm == NULL))
		goto done;

	ps = veb_ports_array(pm);
	for (i = 0; i < pm->m_count; i++) {
		struct veb_port *tp = ps[i];
		uint16_t pvid, vid;
		unsigned int bif_flags;

		if (ctx->p == tp || (ctx->p->p_protected & tp->p_protected)) {
			/*
			 * don't let Ethernet packets hairpin or
			 * move between ports in the same protected
			 * domain(s).
			 */
			continue;
		}

		ifp0 = tp->p_ifp0;
		if (!ISSET(ifp0->if_flags, IFF_RUNNING)) {
			/* don't waste time */
			continue;
		}

		bif_flags = READ_ONCE(tp->p_bif_flags);

		if (!ISSET(bif_flags, IFBIF_DISCOVER) &&
		    !ISSET(m0->m_flags, M_BCAST | M_MCAST)) {
			/* don't flood unknown unicast */
			continue;
		}

		pvid = tp->p_pvid;
		if (pvid < IFBR_PVID_MIN || pvid > IFBR_PVID_MAX ||
		    veb_pvlan_filter(sc, ctx, pvid)) {
			if (ISSET(bif_flags, IFBIF_PVLAN_PTAGS)) {
				/*
				 * port is attached to something that is
				 * vlan aware but pvlan unaware. only flood
				 * to the primary vid.
				 */
				vid = ctx->vp;
			} else {
				/*
				 * this must be an inter switch
				 * trunk, so use the original vid.
				 */
				vid = ctx->vs;
			}

			if (veb_vid_map_filter(tp, vid))
				continue;
		} else
			vid = pvid;

		if (veb_rule_filter(tp, VEB_RULE_LIST_OUT, m0,
		    ctx->src, ctx->dst, vid)) {
			veb_port_count(tp, veb_c_rule_out);
			continue;
		}

		if (ISSET(bif_flags, IFBIF_BLOCKNONIP) &&
		    veb_ip_filter(m0))
			continue;

		m = m_dup_pkt(m0, max_linkhdr + ETHER_ALIGN, M_NOWAIT);
		if (m == NULL) {
			/* XXX count error? */
			continue;
		}

		if (pvid != vid)
			m->m_pkthdr.ether_vtag |= vid;
		else
			CLR(m->m_flags, M_VLANTAG);

		m = ether_offload_ifcap(ifp0, m);
		if (m == NULL) {
			counters_inc(ifp->if_counters, ifc_oerrors);
			continue;
		}

		(*tp->p_enqueue)(ifp0, m, ctx->ns); /* XXX count error */
	}
	refcnt_rele_wake(&pm->m_refs);

done:
	m_freem(m0);
}

static struct mbuf *
veb_transmit(struct veb_softc *sc, struct veb_ctx *ctx, struct mbuf *m,
    struct veb_port *tp, uint16_t tvs)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ifnet *ifp0;
	uint16_t pvid, vid = tvs;
	unsigned int bif_flags = READ_ONCE(tp->p_bif_flags);
	enum veb_port_counters c;

	/*
	 * don't let Ethernet packets hairpin or move between
	 * ports in the same protected domain(s).
	 */

	if (ctx->p == tp) {
		c = veb_c_hairpin;
		goto drop;
	}
	if (ctx->p->p_protected & tp->p_protected) {
		c = veb_c_protected;
		goto drop;
	}

	if (veb_pvlan_filter(sc, ctx, tvs)) {
		c = veb_c_pvlan;
		goto drop;
	}

	/* address entries are still subject to tagged config */
	pvid = tp->p_pvid;
	if (tvs != pvid) {
		if (ISSET(bif_flags, IFBIF_PVLAN_PTAGS)) {
			/*
			 * this port is vlan aware but pvlan unaware,
			 * so it only understands the primary vlan.
			 */
			if (tvs != ctx->vp) {
				c = veb_c_pvptags_out;
				goto drop;
			}
		} else {
			/*
			 * this must be an inter switch trunk, so use the
			 * original vid.
			 */
			vid = ctx->vs;
		}

		if (veb_vid_map_filter(tp, vid)) {
			c = veb_c_tagged_filter_out;
			goto drop;
		}
	}

	if (veb_rule_filter(tp, VEB_RULE_LIST_OUT, m,
	    ctx->src, ctx->dst, vid)) {
		c = veb_c_rule_out;
		goto drop;
	}

	if (ISSET(bif_flags, IFBIF_BLOCKNONIP) &&
	    veb_ip_filter(m)) {
		c = veb_c_blocknonip_out;
		goto drop;
	}

	ifp0 = tp->p_ifp0;

	if (tvs != pvid)
		m->m_pkthdr.ether_vtag |= vid;
	else {
#if NPF > 0
		if (ISSET(ifp->if_flags, IFF_LINK1) &&
		    (m = veb_pf(ifp0, PF_FWD, m, ctx->ns)) == NULL)
			return (NULL);
#endif

		CLR(m->m_flags, M_VLANTAG);
	}

	counters_pkt(ifp->if_counters, ifc_opackets, ifc_obytes,
	    m->m_pkthdr.len);

	m = ether_offload_ifcap(ifp0, m);
	if (m == NULL) {
		counters_inc(ifp->if_counters, ifc_oerrors);
		return (NULL);
	}

	(*tp->p_enqueue)(ifp0, m, ctx->ns); /* XXX count error */

	return (NULL);
drop:
	veb_port_count(tp, c);
	m_freem(m);
	return (NULL);
}

static struct mbuf *
veb_vport_input(struct ifnet *ifp0, struct mbuf *m, uint64_t dst, void *brport,
    struct netstack *ns)
{
	return (m);
}

static uint16_t
veb_pvlan(struct veb_softc *sc, uint16_t vid)
{
	uint16_t *pvlans;
	uint16_t pvlan;

	/*
	 * a normal non-pvlan vlan operates like the primary vid in a pvlan,
	 * or visa versa. when doing a lookup we pretend that a non-pvlan vid
	 * is the primary vid in a pvlan.
	 */

	pvlans = SMR_PTR_GET(&sc->sc_pvlans);
	if (pvlans == NULL)
		return (VEB_PVLAN_T_PRIMARY | vid);

	pvlan = pvlans[vid];
	if (pvlan == 0)
		return (VEB_PVLAN_T_PRIMARY | vid);

	return (pvlan);
}

static struct mbuf *
veb_port_input(struct ifnet *ifp0, struct mbuf *m, uint64_t dst, void *brport,
    struct netstack *ns)
{
	struct veb_port *p = brport;
	struct veb_softc *sc = p->p_veb;
	struct veb_ctx ctx = {
		.ns = ns,
		.p = p,
		.dst = dst,
		.vs = p->p_pvid,
	};
	struct ifnet *ifp = &sc->sc_if;
	struct ether_header *eh;
	unsigned int bif_flags;
	enum veb_port_counters c;
	uint16_t pvlan;
	int prio;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return (m);

	/* Is this a MAC Bridge component Reserved address? */
	if (ETH64_IS_8021_RSVD(dst)) {
		if (!ISSET(ifp->if_flags, IFF_LINK0)) {
			/*
			 * letting vlans through implies this is
			 * an s-vlan component.
			 */
			return (m);
		}

		/* look at the last nibble of the 802.1 reserved address */
		switch (dst & 0xf) {
		case 0x0: /* Nearest Customer Bridge Group Address */
		case 0xb: /* EDE-SS PEP (IEEE Std 802.1AEcg) */
		case 0xc: /* reserved */
		case 0xd: /* Provider Bridge MVRP Address */
		case 0xf: /* reserved */
			break;
		default:
			return (m);
		}
	}

	eh = mtod(m, struct ether_header *);
	if (!ISSET(m->m_flags, M_VLANTAG) &&
	    eh->ether_type == htons(ETHERTYPE_VLAN)) {
		struct ether_vlan_header *evl;

		evl = mtod(m, struct ether_vlan_header *);
		m->m_pkthdr.ether_vtag = ntohs(evl->evl_tag);
		SET(m->m_flags, M_VLANTAG);

		memmove((caddr_t)evl + EVL_ENCAPLEN, evl,
		    offsetof(struct ether_vlan_header, evl_encap_proto));
		m_adj(m, EVL_ENCAPLEN);

		eh = mtod(m, struct ether_header *);
	}

	if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
		/* don't allow double tagging as it enables vlan hopping */
		c = veb_c_double_tag;
		goto drop;
	}

	if (ISSET(m->m_flags, M_VLANTAG)) {
		uint16_t tvid = EVL_VLANOFTAG(m->m_pkthdr.ether_vtag);

		if (tvid == EVL_VLID_NULL) {
			/* this preserves PRIOFTAG for BPF */
			CLR(m->m_flags, M_VLANTAG);
		} else if (veb_vid_map_filter(p, tvid)) {
			c = veb_c_tagged_filter_in;
			goto drop;
		} else
			ctx.vs = tvid;

		prio = sc->sc_rxprio;
		switch (prio) {
		case IF_HDRPRIO_PACKET:
			break;
		case IF_HDRPRIO_OUTER:
			prio = EVL_PRIOFTAG(m->m_pkthdr.ether_vtag);
			/* IEEE 802.1p has prio 0 and 1 swapped */
			if (prio <= 1)
				prio = !prio;
			/* FALLTHROUGH */
		default:
			m->m_pkthdr.pf.prio = prio;
			break;
		}
	} else {
		/* prepare for BPF */
		m->m_pkthdr.ether_vtag = 0;
	}

	if (ctx.vs == IFBR_PVID_DECLINE)
		return (m);
	if (ctx.vs == IFBR_PVID_NONE) {
		c = veb_c_untagged_none;
		goto drop;
	}

#ifdef DIAGNOSTIC
	if (ctx.vs < IFBR_PVID_MIN ||
	    ctx.vs > IFBR_PVID_MAX) {
		panic("%s: %s vid %u is outside valid range", __func__,
		    ifp0->if_xname, ctx.vs);
	}
#endif

	smr_read_enter();
	pvlan = veb_pvlan(sc, ctx.vs);
	smr_read_leave();

	ctx.vp = pvlan & VEB_PVLAN_V_MASK;
	ctx.vt = pvlan & VEB_PVLAN_T_MASK;
	ctx.src = ether_addr_to_e64((struct ether_addr *)eh->ether_shost);

	bif_flags = READ_ONCE(p->p_bif_flags);

	if (ISSET(bif_flags, IFBIF_PVLAN_PTAGS) &&
	    ISSET(m->m_flags, M_VLANTAG) &&
	    ctx.vt != VEB_PVLAN_T_PRIMARY) {
		c = veb_c_pvptags_in;
		goto drop;
	}

	if (ISSET(bif_flags, IFBIF_LOCKED)) {
		struct eb_entry *ebe;
		struct veb_port *rp = NULL;

		smr_read_enter();
		ebe = etherbridge_resolve_entry(&sc->sc_eb, ctx.vp, ctx.src);
		if (ebe != NULL && ctx.vs == etherbridge_vs(ebe))
			rp = etherbridge_port(ebe);
		smr_read_leave();

		if (rp != p) {
			c = veb_c_locked;
			goto drop;
		}
	}

	counters_pkt(ifp->if_counters, ifc_ipackets, ifc_ibytes,
	    m->m_pkthdr.len);

	if (!ISSET(m->m_flags, M_VLANTAG)) {
		SET(m->m_flags, M_VLANTAG); /* for BPF */
		m->m_pkthdr.ether_vtag |= ctx.vs;
	}

	/* force packets into the one routing domain for pf */
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NBPFILTER > 0
	if_bpf = READ_ONCE(ifp->if_bpf);
	if (if_bpf != NULL) {
		if (bpf_mtap_ether(if_bpf, m, 0) != 0) {
			c = veb_c_bpfilter;
			goto drop;
		}
	}
#endif

	veb_span(sc, m);

	if (ISSET(bif_flags, IFBIF_BLOCKNONIP) &&
	    veb_ip_filter(m)) {
		c = veb_c_blocknonip_in;
		goto drop;
	}

	if (!ISSET(ifp->if_flags, IFF_LINK0) &&
	    veb_svlan_filter(m)) {
		c = veb_c_svlan;
		goto drop;
	}

	if (veb_rule_filter(p, VEB_RULE_LIST_IN, m,
	    ctx.src, ctx.dst, ctx.vs)) {
		c = veb_c_rule_in;
		goto drop;
	}

#if NPF > 0
	if (ISSET(ifp->if_flags, IFF_LINK1) && p->p_pvid == ctx.vs &&
	    (m = veb_pf(ifp0, PF_IN, m, ctx.ns)) == NULL)
		return (NULL);
#endif

	eh = mtod(m, struct ether_header *);

	if (ISSET(bif_flags, IFBIF_LEARNING))
		etherbridge_map(&sc->sc_eb, ctx.p, ctx.vp, ctx.vs, ctx.src);

	prio = sc->sc_txprio;
	prio = (prio == IF_HDRPRIO_PACKET) ? m->m_pkthdr.pf.prio : prio;
	/* IEEE 802.1p has prio 0 and 1 swapped */
	if (prio <= 1)
		prio = !prio;
	m->m_pkthdr.ether_vtag = (prio << EVL_PRIO_BITS);

	CLR(m->m_flags, M_BCAST|M_MCAST);

	if (!ETH64_IS_MULTICAST(ctx.dst)) {
		struct eb_entry *ebe;
		struct veb_port *tp = NULL;
		struct veb_port_cpu *tc;
		uint16_t tvs = 0;

		smr_read_enter();
		ebe = etherbridge_resolve_entry(&sc->sc_eb, ctx.vp, ctx.dst);
		if (ebe != NULL) {
			tp = etherbridge_port(ebe);
			tc = veb_ep_brport_take(tp);
			tvs = etherbridge_vs(ebe);
		}
		smr_read_leave();
		if (tp != NULL) {
			m = veb_transmit(sc, &ctx, m, tp, tvs);
			veb_ep_brport_rele(tc, tp);
		}

		if (m == NULL)
			return (NULL);

		/* unknown unicast address */
	} else {
		SET(m->m_flags,
		    ETH64_IS_BROADCAST(ctx.dst) ? M_BCAST : M_MCAST);
	}

	veb_broadcast(sc, &ctx, m);
	return (NULL);

drop:
	veb_port_count(p, c);
	m_freem(m);
	return (NULL);
}

static void
veb_input(struct ifnet *ifp, struct mbuf *m, struct netstack *ns)
{
	m_freem(m);
}

static int
veb_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	m_freem(m);
	return (ENODEV);
}

static int
veb_enqueue(struct ifnet *ifp, struct mbuf *m)
{
	m_freem(m);
	return (ENODEV);
}

static void
veb_start(struct ifqueue *ifq)
{
	ifq_purge(ifq);
}

static int
veb_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct veb_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifbrparam *bparam = (struct ifbrparam *)data;
	int error = 0;

	if (sc->sc_dead)
		return (ENXIO);

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = veb_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = veb_down(sc);
		}
		break;

	case SIOCSVNETID:
		if (ifr->ifr_vnetid < IFBR_PVID_MIN ||
		    ifr->ifr_vnetid > IFBR_PVID_MAX) {
			error = EINVAL;
			break;
		}

		sc->sc_dflt_pvid = ifr->ifr_vnetid;
		break;
	case SIOCGVNETID:
		if (sc->sc_dflt_pvid == IFBR_PVID_NONE)
			error = EADDRNOTAVAIL;
		else
			ifr->ifr_vnetid = (int64_t)sc->sc_dflt_pvid;
		break;
	case SIOCDVNETID:
		sc->sc_dflt_pvid = IFBR_PVID_NONE;
		break;
	case SIOCSTXHPRIO:
		error = if_txhprio_l2_check(ifr->ifr_hdrprio);
		if (error != 0)
			break;

		sc->sc_txprio = ifr->ifr_hdrprio;
		break;
	case SIOCGTXHPRIO:
		ifr->ifr_hdrprio = sc->sc_txprio;
		break;

	case SIOCSRXHPRIO:
		error = if_rxhprio_l2_check(ifr->ifr_hdrprio);
		if (error != 0)
			break;

		sc->sc_rxprio = ifr->ifr_hdrprio;
		break;
	case SIOCGRXHPRIO:
		ifr->ifr_hdrprio = sc->sc_rxprio;
		break;

	case SIOCBRDGADDPV:
		error = veb_add_pvlan(sc, (const struct ifbrpvlan *)data);
		break;
	case SIOCBRDGDELPV:
		error = veb_del_pvlan(sc, (const struct ifbrpvlan *)data);
		break;
	case SIOCBRDGFINDPV:
		error = veb_find_pvlan(sc, (struct ifbrpvlan *)data);
		break;
	case SIOCBRDGNFINDPV:
		error = veb_nfind_pvlan(sc, (struct ifbrpvlan *)data);
		break;

	case SIOCBRDGADD:
		error = suser(curproc);
		if (error != 0)
			break;

		error = veb_add_port(sc, (struct ifbreq *)data, 0);
		break;
	case SIOCBRDGADDS:
		error = suser(curproc);
		if (error != 0)
			break;

		error = veb_add_port(sc, (struct ifbreq *)data, 1);
		break;
	case SIOCBRDGDEL:
		error = suser(curproc);
		if (error != 0)
			break;

		error = veb_del_port(sc, (struct ifbreq *)data, 0);
		break;
	case SIOCBRDGDELS:
		error = suser(curproc);
		if (error != 0)
			break;

		error = veb_del_port(sc, (struct ifbreq *)data, 1);
		break;

	case SIOCBRDGSCACHE:
		error = suser(curproc);
		if (error != 0)
			break;

		error = etherbridge_set_max(&sc->sc_eb, bparam);
		break;
	case SIOCBRDGGCACHE:
		error = etherbridge_get_max(&sc->sc_eb, bparam);
		break;

	case SIOCBRDGSTO:
		error = suser(curproc);
		if (error != 0)
			break;

		error = etherbridge_set_tmo(&sc->sc_eb, bparam);
		break;
	case SIOCBRDGGTO:
		error = etherbridge_get_tmo(&sc->sc_eb, bparam);
		break;

	case SIOCBRDGRTS:
		error = etherbridge_rtfind(&sc->sc_eb, (struct ifbaconf *)data);
		break;
	case SIOCBRDGVRTS:
		error = etherbridge_vareq(&sc->sc_eb, (struct ifbaconf *)data);
		break;
	case SIOCBRDGIFS:
		error = veb_port_list(sc, (struct ifbifconf *)data);
		break;
	case SIOCBRDGFLUSH:
		etherbridge_flush(&sc->sc_eb,
		    ((struct ifbreq *)data)->ifbr_ifsflags);
		break;
	case SIOCBRDGSADDR:
		error = veb_add_addr(sc, (struct ifbareq *)data);
		break;
	case SIOCBRDGDADDR:
		error = veb_del_addr(sc, (struct ifbareq *)data);
		break;
	case SIOCBRDGSVADDR:
		error = veb_add_vid_addr(sc, (struct ifbvareq *)data);
		break;
	case SIOCBRDGDVADDR:
		error = veb_del_vid_addr(sc, (struct ifbvareq *)data);
		break;

	case SIOCBRDGSIFPROT:
		error = veb_port_set_protected(sc, (struct ifbreq *)data);
		break;
	case SIOCBRDGSPVID:
		error = veb_port_set_pvid(sc, (struct ifbreq *)data);
		break;

	case SIOCBRDGSVMAP:
		error = veb_set_vid_map(sc, (const struct ifbrvidmap *)data);
		break;
	case SIOCBRDGGVMAP:
		error = veb_get_vid_map(sc, (struct ifbrvidmap *)data);
		break;

	case SIOCBRDGSIFFLGS:
		error = veb_port_set_flags(sc, (struct ifbreq *)data);
		break;
	case SIOCBRDGGIFFLGS:
		error = veb_port_get_flags(sc, (struct ifbreq *)data);
		break;

	case SIOCBRDGARL:
		error = veb_rule_add(sc, (struct ifbrlreq *)data);
		break;
	case SIOCBRDGFRL:
		error = veb_rule_list_flush(sc, (struct ifbrlreq *)data);
		break;
	case SIOCBRDGGRL:
		error = veb_rule_list_get(sc, (struct ifbrlconf *)data);
		break;

	default:
		error = ENOTTY;
		break;
	}

	if (error == ENETRESET)
		error = veb_iff(sc);

	return (error);
}

static struct veb_ports *
veb_ports_insert(struct veb_ports *om, struct veb_port *p)
{
	struct veb_ports *nm;
	struct veb_port **nps, **ops;
	unsigned int ocount = om != NULL ? om->m_count : 0;
	unsigned int ncount = ocount + 1;
	unsigned int i;

	nm = malloc(veb_ports_size(ncount), M_DEVBUF, M_WAITOK|M_ZERO);

	refcnt_init(&nm->m_refs);
	nm->m_count = ncount;

	nps = veb_ports_array(nm);

	if (om != NULL) {
		ops = veb_ports_array(om);
		for (i = 0; i < ocount; i++) {
			struct veb_port *op = ops[i];
			veb_p_take(op);
			nps[i] = op;
		}
	} else
		i = 0;

	veb_p_take(p);
	nps[i] = p;

	return (nm);
}

static struct veb_ports *
veb_ports_remove(struct veb_ports *om, struct veb_port *p)
{
	struct veb_ports *nm;
	struct veb_port **nps, **ops;
	unsigned int ocount = om->m_count;
	unsigned int ncount = ocount - 1;
	unsigned int i, j;

	if (ncount == 0)
		return (NULL);

	nm = malloc(veb_ports_size(ncount), M_DEVBUF, M_WAITOK|M_ZERO);

	refcnt_init(&nm->m_refs);
	nm->m_count = ncount;

	nps = veb_ports_array(nm);
	j = 0;

	ops = veb_ports_array(om);
	for (i = 0; i < ocount; i++) {
		struct veb_port *op = ops[i];
		if (op == p)
			continue;

		veb_p_take(op);
		nps[j++] = op;
	}
	KASSERT(j == ncount);

	return (nm);
}

static inline void
veb_ports_free(struct veb_ports *m)
{
	free(m, M_DEVBUF, veb_ports_size(m->m_count));
}

static void
veb_ports_destroy(struct veb_ports *m)
{
	struct veb_port **ps = veb_ports_array(m);
	unsigned int i;

	for (i = 0; i < m->m_count; i++) {
		struct veb_port *p = ps[i];
		veb_p_rele(p);
	}

	veb_ports_free(m);
}

static int
veb_add_port(struct veb_softc *sc, const struct ifbreq *req, unsigned int span)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ifnet *ifp0;
	struct veb_ports **ports_ptr;
	struct veb_ports *om, *nm;
	struct veb_port *p;
	struct veb_port_cpu *c;
	struct cpumem_iter cmi;
	int isvport;
	int error;

	NET_ASSERT_LOCKED();

	ifp0 = if_unit(req->ifbr_ifsname);
	if (ifp0 == NULL)
		return (EINVAL);

	if (ifp0->if_type != IFT_ETHER) {
		error = EPROTONOSUPPORT;
		goto put;
	}

	if (ifp0 == ifp) {
		error = EPROTONOSUPPORT;
		goto put;
	}

	isvport = (ifp0->if_enqueue == vport_enqueue);

	error = ether_brport_isset(ifp0);
	if (error != 0)
		goto put;

	/* let's try */

	p = malloc(sizeof(*p), M_DEVBUF, M_WAITOK|M_ZERO|M_CANFAIL);
	if (p == NULL) {
		error = ENOMEM;
		goto put;
	}

	ifsetlro(ifp0, 0);

	p->p_ifp0 = ifp0;
	p->p_veb = sc;
	p->p_pvid = sc->sc_dflt_pvid;

	refcnt_init(&p->p_refs);
	TAILQ_INIT(&p->p_vrl);
	SMR_TAILQ_INIT(&p->p_vr_list[0]);
	SMR_TAILQ_INIT(&p->p_vr_list[1]);

	p->p_percpu = cpumem_malloc(sizeof(*c), M_DEVBUF);
	CPUMEM_FOREACH(c, &cmi, p->p_percpu) {
		/* use a per cpu refcnt as a proxy to the port refcnt */
		veb_p_take(p);
		refcnt_init(&c->c_refs);

		pc_lock_init(&c->c_lock);
	}

	p->p_enqueue = isvport ? vport_if_enqueue : veb_if_enqueue;
	p->p_ioctl = ifp0->if_ioctl;
	p->p_output = ifp0->if_output;

	p->p_brport.ep_port = p;
	p->p_brport.ep_port_take = veb_ep_brport_take;
	p->p_brport.ep_port_rele = veb_ep_brport_rele;

	if (span) {
		ports_ptr = &sc->sc_spans;

		if (isvport) {
			error = EPROTONOSUPPORT;
			goto free;
		}

		p->p_brport.ep_input = veb_span_input;
		p->p_bif_flags = IFBIF_SPAN;
	} else {
		ports_ptr = &sc->sc_ports;

		error = ifpromisc(ifp0, 1);
		if (error != 0)
			goto free;

		p->p_bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
		p->p_brport.ep_input = isvport ?
		    veb_vport_input : veb_port_input;
	}

	om = SMR_PTR_GET_LOCKED(ports_ptr);
	nm = veb_ports_insert(om, p);

	/* this might have changed if we slept for malloc or ifpromisc */
	error = ether_brport_isset(ifp0);
	if (error != 0)
		goto unpromisc;

	task_set(&p->p_ltask, veb_p_linkch, p);
	if_linkstatehook_add(ifp0, &p->p_ltask);

	task_set(&p->p_dtask, veb_p_detach, p);
	if_detachhook_add(ifp0, &p->p_dtask);

	/* commit */
	SMR_PTR_SET_LOCKED(ports_ptr, nm);

	ether_brport_set(ifp0, &p->p_brport);
	if (!isvport) { /* vport is special */
		ifp0->if_ioctl = veb_p_ioctl;
		ifp0->if_output = veb_p_output;
	}

	veb_p_linkch(p);

	/* clean up the old veb_ports map */
	smr_barrier();
	if (om != NULL) {
		refcnt_finalize(&om->m_refs, "vebports");
		veb_ports_destroy(om);
	}

#if NKSTAT > 0
	veb_port_kstat_attach(p);
#endif

	return (0);

unpromisc:
	if (!span)
		ifpromisc(ifp0, 0);
free:
	cpumem_free(p->p_percpu, M_DEVBUF, sizeof(*c));
	free(p, M_DEVBUF, sizeof(*p));
put:
	if_put(ifp0);
	return (error);
}

static struct veb_port *
veb_trunkport(struct veb_softc *sc, const char *name, unsigned int span)
{
	struct veb_ports *m;
	struct veb_port **ps;
	struct veb_port *p;
	unsigned int i;

	m = SMR_PTR_GET_LOCKED(span ? &sc->sc_spans : &sc->sc_ports);
	if (m == NULL)
		return (NULL);

	ps = veb_ports_array(m);
	for (i = 0; i < m->m_count; i++) {
		p = ps[i];

		if (strncmp(p->p_ifp0->if_xname, name, IFNAMSIZ) == 0)
			return (p);
	}

	return (NULL);
}

static int
veb_del_port(struct veb_softc *sc, const struct ifbreq *req, unsigned int span)
{
	struct veb_port *p;

	NET_ASSERT_LOCKED();
	p = veb_trunkport(sc, req->ifbr_ifsname, span);
	if (p == NULL)
		return (EINVAL);

	veb_p_dtor(sc, p);

	return (0);
}

static struct veb_port *
veb_port_get(struct veb_softc *sc, const char *name)
{
	struct veb_ports *m;
	struct veb_port **ps;
	struct veb_port *p;
	unsigned int i;

	NET_ASSERT_LOCKED();

	m = SMR_PTR_GET_LOCKED(&sc->sc_ports);
	if (m == NULL)
		return (NULL);

	ps = veb_ports_array(m);
	for (i = 0; i < m->m_count; i++) {
		p = ps[i];

		if (strncmp(p->p_ifp0->if_xname, name, IFNAMSIZ) == 0) {
			veb_p_take(p);
			return (p);
		}
	}

	return (NULL);
}

static void
veb_port_put(struct veb_softc *sc, struct veb_port *p)
{
	veb_p_rele(p);
}

static int
veb_port_set_protected(struct veb_softc *sc, const struct ifbreq *ifbr)
{
	struct veb_port *p;

	p = veb_port_get(sc, ifbr->ifbr_ifsname);
	if (p == NULL)
		return (ESRCH);

	p->p_protected = ifbr->ifbr_protected;
	veb_port_put(sc, p);

	return (0);
}

static int
veb_port_set_pvid(struct veb_softc *sc, const struct ifbreq *ifbr)
{
	struct veb_port *p;
	uint16_t pvid;
	int error = 0;

	switch (ifbr->ifbr_pvid) {
	case EVL_VLID_NULL:
		pvid = sc->sc_dflt_pvid;
		break;
	default:
		if (ifbr->ifbr_pvid < EVL_VLID_MIN ||
		    ifbr->ifbr_pvid > EVL_VLID_MAX)
			return (EINVAL);

		/* FALLTHROUGH */
	case IFBR_PVID_NONE:
	case IFBR_PVID_DECLINE:
		pvid = ifbr->ifbr_pvid;
		break;
	}

	p = veb_port_get(sc, ifbr->ifbr_ifsname);
	if (p == NULL)
		return (ESRCH);

	if ((pvid < EVL_VLID_MIN || pvid > EVL_VLID_MAX) &&
	    p->p_ifp0->if_enqueue == vport_enqueue) {
		error = EOPNOTSUPP;
		goto put;
	}
	p->p_pvid = pvid;

put:
	veb_port_put(sc, p);

	return (error);
}

static int
veb_get_vid_map(struct veb_softc *sc, struct ifbrvidmap *ifbrvm)
{
	struct veb_port *p;
	uint32_t *map;
	int anybits = 0;
	int error = 0;

	p = veb_port_get(sc, ifbrvm->ifbrvm_ifsname);
	if (p == NULL)
		return (ESRCH);

	smr_read_enter();
	map = p->p_vid_map;
	if (map == NULL)
		memset(ifbrvm->ifbrvm_map, 0, sizeof(ifbrvm->ifbrvm_map));
	else {
		size_t w;

		for (w = 0; w < VEB_VID_WORDS; w++) {
			uint32_t e = map[w];
			size_t t = w * sizeof(e);
			size_t b;

			for (b = 0; b < sizeof(e); b++)
				ifbrvm->ifbrvm_map[t + b] = e >> (b * 8);

			anybits |= e;
		}
	}
	smr_read_leave();

	if (p->p_ifp0->if_enqueue == vport_enqueue && !anybits)
		error = ENOENT;

	veb_port_put(sc, p);

	return (error);
}

static int
veb_chk_vid_map(const struct ifbrvidmap *ifbrvm)
{
	size_t off;
	size_t bit;

	/*
	 * vlan 0 and 4095 are not valid vlan tags
	 */

	off = 0 / 8;
	bit = 0 % 8;
	if (ISSET(ifbrvm->ifbrvm_map[off], 1U << bit))
		return (EINVAL);

	off = 4095 / 8;
	bit = 4095 % 8;
	if (ISSET(ifbrvm->ifbrvm_map[off], 1U << bit))
		return (EINVAL);

	return (0);
}

static uint32_t *
veb_new_vid_map(const struct ifbrvidmap *ifbrvm)
{
	uint32_t *map;
	size_t w;

	map = mallocarray(VEB_VID_WORDS, sizeof(*map), M_IFADDR,
	    M_WAITOK|M_CANFAIL);
	if (map == NULL)
		return (NULL);

	for (w = 0; w < VEB_VID_WORDS; w++) {
		uint32_t e = 0;
		size_t t = w * sizeof(e);
		size_t b;

		for (b = 0; b < sizeof(e); b++)
			e |= (uint32_t)ifbrvm->ifbrvm_map[t + b] << (b * 8);

		map[w] = e;
	}

	return (map);
}

static inline void
veb_free_vid_map(uint32_t *map)
{
	free(map, M_IFADDR, VEB_VID_BYTES);
}

struct veb_vid_map_dtor {
	struct smr_entry	 smr;
	uint32_t		*map;
};

static void
veb_dtor_vid_map(void *arg)
{
	struct veb_vid_map_dtor *dtor = arg;
	veb_free_vid_map(dtor->map);
	free(dtor, M_TEMP, sizeof(*dtor));
}

static void
veb_destroy_vid_map(uint32_t *map)
{
	struct veb_vid_map_dtor *dtor;

	dtor = malloc(sizeof(*dtor), M_TEMP, M_NOWAIT);
	if (dtor == NULL) {
		/* oh well, the proc can sleep instead */
		smr_barrier();
		veb_free_vid_map(map);
		return;
	}

	smr_init(&dtor->smr);
	dtor->map = map;
	smr_call(&dtor->smr, veb_dtor_vid_map, dtor);
}

static void
veb_set_vid_map_set(uint32_t *nmap, const uint32_t *omap)
{
	/* nop - nmap replaces (sets) the vid map */
}

static void
veb_set_vid_map_or(uint32_t *nmap, const uint32_t *omap)
{
	size_t w;

	if (omap == NULL)
		return;

	for (w = 0; w < VEB_VID_WORDS; w++)
		nmap[w] |= omap[w];
}

static void
veb_set_vid_map_andnot(uint32_t *nmap, const uint32_t *omap)
{
	size_t w;

	if (omap == NULL) {
		/* empty set, clear everything */
		for (w = 0; w < VEB_VID_WORDS; w++)
			nmap[w] = 0;
		return;
	}

	for (w = 0; w < VEB_VID_WORDS; w++) {
		uint32_t e = nmap[w];
		nmap[w] = omap[w] & ~e;
	}
}

static int
veb_set_vid_map(struct veb_softc *sc, const struct ifbrvidmap *ifbrvm)
{
	void (*apply)(uint32_t *, const uint32_t *);
	struct veb_port *p;
	uint32_t *nmap = NULL, *omap = NULL;
	int error = 0;

	switch (ifbrvm->ifbrvm_op) {
	case IFBRVM_OP_SET:
		apply = veb_set_vid_map_set;
		break;
	case IFBRVM_OP_OR:
		apply = veb_set_vid_map_or;
		break;
	case IFBRVM_OP_ANDNOT:
		apply = veb_set_vid_map_andnot;
		break;
	default:
		return (EINVAL);
	}

	error = veb_chk_vid_map(ifbrvm);
	if (error != 0)
		return (error);

	p = veb_port_get(sc, ifbrvm->ifbrvm_ifsname);
	if (p == NULL)
		return (ESRCH);

	nmap = veb_new_vid_map(ifbrvm);
	if (nmap == NULL) {
		error = ENOMEM;
		goto put;
	}

	error = rw_enter(&sc->sc_rule_lock, RW_WRITE|RW_INTR);
	if (error != 0)
		goto put;

	omap = SMR_PTR_GET_LOCKED(&p->p_vid_map);
	apply(nmap, omap);
	SMR_PTR_SET_LOCKED(&p->p_vid_map, nmap);
	rw_exit(&sc->sc_rule_lock);
	nmap = NULL;

put:
	veb_port_put(sc, p);
	if (omap != NULL)
		veb_destroy_vid_map(omap);
	if (nmap != NULL)
		veb_free_vid_map(nmap);
	return (error);
}

static int
veb_vid_inuse(struct veb_softc *sc, uint16_t vid)
{
	struct veb_ports *pm;
	struct veb_port **ps;
	unsigned int off = vid / 32;
	unsigned int bit = vid % 32;
	unsigned int i;

	/* must be holding sc->sc_rule_lock */

	pm = SMR_PTR_GET_LOCKED(&sc->sc_ports);
	ps = veb_ports_array(pm);
	for (i = 0; i < pm->m_count; i++) {
		struct veb_port *p = ps[i];
		uint32_t *map;

		if (p->p_pvid == vid)
			return (1);

		map = SMR_PTR_GET_LOCKED(&p->p_vid_map);
		if (map != NULL && ISSET(map[off], 1U << bit))
			return (1);
	}

	return (0);
}

static int
veb_add_pvlan(struct veb_softc *sc, const struct ifbrpvlan *ifbrpv)
{
	struct veb_pvlan *v;
	uint16_t *pvlans = NULL;
	int error;

	if (ifbrpv->ifbrpv_primary < EVL_VLID_MIN ||
	    ifbrpv->ifbrpv_primary > EVL_VLID_MAX)
		return (EINVAL);

	switch (ifbrpv->ifbrpv_type) {
	case IFBRPV_T_PRIMARY:
		if (ifbrpv->ifbrpv_secondary != 0)
			return (EINVAL);
		break;
	case IFBRPV_T_ISOLATED:
	case IFBRPV_T_COMMUNITY:
		if (ifbrpv->ifbrpv_secondary < EVL_VLID_MIN ||
		    ifbrpv->ifbrpv_secondary > EVL_VLID_MAX)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	if (sc->sc_pvlans == NULL) {
		pvlans = mallocarray(VEB_VID_COUNT, sizeof(*pvlans),
		    M_IFADDR, M_WAITOK|M_CANFAIL|M_ZERO);
		if (pvlans == NULL)
			return (ENOMEM);
	}

	v = malloc(sizeof(*v), M_IFADDR, M_WAITOK|M_CANFAIL);
	if (v == NULL) {
		error = ENOMEM;
		goto freepvlans;
	}

	v->v_primary = ifbrpv->ifbrpv_primary;
	v->v_secondary = ifbrpv->ifbrpv_secondary;
	v->v_type = ifbrpv->ifbrpv_type;

	error = rw_enter(&sc->sc_rule_lock, RW_WRITE|RW_INTR);
	if (error != 0)
		goto free;

	if (sc->sc_pvlans == NULL) {
		KASSERT(pvlans != NULL);
		SMR_PTR_SET_LOCKED(&sc->sc_pvlans, pvlans);
		pvlans = NULL;
	}

	if (ifbrpv->ifbrpv_type == IFBRPV_T_PRIMARY) {
		struct veb_pvlan *ovp;

		if (sc->sc_pvlans[v->v_primary] != 0) {
			error = EBUSY;
			goto err;
		}

		ovp = RBT_INSERT(veb_pvlan_vp, &sc->sc_pvlans_vp, v);
		if (ovp != NULL) {
			panic("%s: %s %p pvlans and pvlans_vp inconsistency\n",
			    __func__, sc->sc_if.if_xname, sc);
		}

		sc->sc_pvlans[v->v_primary] = VEB_PVLAN_T_PRIMARY |
		    v->v_primary;
	} else { /* secondary */
		struct veb_pvlan *vp, *ovs;
		uint16_t pve = v->v_primary;

		if (sc->sc_pvlans[v->v_secondary] != 0) {
			error = EBUSY;
			goto err;
		}

		if (sc->sc_pvlans[v->v_primary] != v->v_primary) {
			error = ENETUNREACH; /* XXX */
			goto err;
		}

		vp = RBT_FIND(veb_pvlan_vp, &sc->sc_pvlans_vp, v);
		if (vp == NULL) {
			panic("%s: %s %p pvlans and pvlans_vp inconsistency\n",
			    __func__, sc->sc_if.if_xname, sc);
		}

		if (veb_vid_inuse(sc, v->v_secondary)) {
			error = EADDRINUSE;
			goto err;
		}

		if (ifbrpv->ifbrpv_type == IFBRPV_T_ISOLATED) {
			if (vp->v_isolated != 0) {
				error = EADDRNOTAVAIL;
				goto err;
			}
			vp->v_isolated = v->v_secondary;
			pve |= VEB_PVLAN_T_ISOLATED;
		} else { /* IFBRPV_T_COMMUNITY */
			pve |= VEB_PVLAN_T_COMMUNITY;
		}

		ovs = RBT_INSERT(veb_pvlan_vs, &sc->sc_pvlans_vs, v);
		if (ovs != NULL) {
			panic("%s: %s %p pvlans and pvlans_vs inconsistency\n",
			    __func__, sc->sc_if.if_xname, sc);
		}

		sc->sc_pvlans[v->v_secondary] = pve;
	}
	sc->sc_pvlans_gen++;
	v = NULL;

err:
	rw_exit(&sc->sc_rule_lock);
free:
	free(v, M_IFADDR, sizeof(*v));
freepvlans:
	free(pvlans, M_IFADDR, VEB_VID_COUNT * sizeof(*pvlans));
	return (error);
}

static int
veb_dev_pvlan_filter(struct etherbridge *eb, struct eb_entry *ebe,
    void *cookie)
{
	struct veb_pvlan *vs = cookie;

	return (etherbridge_vs(ebe) == vs->v_secondary);
}

static int
veb_del_pvlan(struct veb_softc *sc, const struct ifbrpvlan *ifbrpv)
{
	struct veb_pvlan key;
	struct veb_pvlan *v = NULL;
	struct veb_pvlan *vp, *vs;
	uint16_t *pvlans;
	uint16_t pve;
	int error;

	if (ifbrpv->ifbrpv_primary < EVL_VLID_MIN ||
	    ifbrpv->ifbrpv_primary > EVL_VLID_MAX)
		return (EINVAL);

	switch (ifbrpv->ifbrpv_type) {
	case IFBRPV_T_PRIMARY:
		if (ifbrpv->ifbrpv_secondary != 0)
			return (EINVAL);
		break;
	case IFBRPV_T_ISOLATED:
	case IFBRPV_T_COMMUNITY:
		if (ifbrpv->ifbrpv_secondary < EVL_VLID_MIN ||
		    ifbrpv->ifbrpv_secondary > EVL_VLID_MAX)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	key.v_primary = ifbrpv->ifbrpv_primary;
	key.v_secondary = ifbrpv->ifbrpv_secondary;
	key.v_type = ifbrpv->ifbrpv_type;

	error = rw_enter(&sc->sc_rule_lock, RW_WRITE|RW_INTR);
	if (error != 0)
		return (error);

	pvlans = sc->sc_pvlans;
	if (pvlans == NULL) {
		error = ESRCH;
		goto err;
	}

	vp = RBT_FIND(veb_pvlan_vp, &sc->sc_pvlans_vp, &key);
	if (vp == NULL) {
		error = ESRCH;
		goto err;
	}

	if (ifbrpv->ifbrpv_type == IFBRPV_T_PRIMARY) {
		vs = RBT_NFIND(veb_pvlan_vs, &sc->sc_pvlans_vs, &key);
		if (vs != NULL && vs->v_primary == vp->v_primary) {
			error = EBUSY;
			goto err;
		}

		v = vp;
		KASSERT(v->v_isolated == 0); /* vs NFIND should found this */

		pve = VEB_PVLAN_T_PRIMARY | v->v_primary;
		if (sc->sc_pvlans[v->v_primary] != pve) {
			panic("%s: %s %p pvlans and pvlans_vp inconsistency\n",
			    __func__, sc->sc_if.if_xname, sc);
		}

		RBT_REMOVE(veb_pvlan_vp, &sc->sc_pvlans_vp, v);
		sc->sc_pvlans[v->v_primary] = 0;
	} else { /* secondary */
		uint16_t pve;

		vs = RBT_FIND(veb_pvlan_vs, &sc->sc_pvlans_vs, &key);
		if (vs == NULL || vs->v_type != key.v_type) {
			error = ESRCH;
			goto err;
		}

		if (veb_vid_inuse(sc, vs->v_secondary)) {
			error = EBUSY;
			goto err;
		}

		v = vs;
		pve = v->v_primary;
		if (ifbrpv->ifbrpv_type == IFBRPV_T_ISOLATED) {
			KASSERT(vp->v_isolated == v->v_secondary);
			vp->v_isolated = 0;

			pve |= VEB_PVLAN_T_ISOLATED;
		} else { /* community */
			pve |= VEB_PVLAN_T_COMMUNITY;
		}

		if (sc->sc_pvlans[v->v_secondary] != pve) {
			panic("%s: %s %p pvlans and pvlans_vs inconsistency\n",
			    __func__, sc->sc_if.if_xname, sc);
		}

		RBT_REMOVE(veb_pvlan_vs, &sc->sc_pvlans_vs, v);
		sc->sc_pvlans[v->v_secondary] = 0;
		/* XXX smr_barrier for sc_pvlans entry use to end? */
		etherbridge_filter(&sc->sc_eb, veb_dev_pvlan_filter, v);
	}
	sc->sc_pvlans_gen++;

err:
	rw_exit(&sc->sc_rule_lock);
	free(v, M_IFADDR, sizeof(*v));
	return (error);
}

static int
veb_find_pvlan(struct veb_softc *sc, struct ifbrpvlan *ifbrpv)
{
	return (ENOTTY);
}

static int
veb_nfind_pvlan_primary(struct veb_softc *sc, struct ifbrpvlan *ifbrpv)
{
	struct veb_pvlan key;
	struct veb_pvlan *vp;
	int error;

	if (ifbrpv->ifbrpv_secondary != 0)
		return (EINVAL);

	key.v_primary = ifbrpv->ifbrpv_primary;

	error = rw_enter(&sc->sc_rule_lock, RW_READ|RW_INTR);
	if (error != 0)
		return (error);

	vp = RBT_NFIND(veb_pvlan_vp, &sc->sc_pvlans_vp, &key);
	if (vp == NULL) {
		error = ENOENT;
		goto err;
	}

	ifbrpv->ifbrpv_primary = vp->v_primary;
	ifbrpv->ifbrpv_secondary = vp->v_isolated;
	ifbrpv->ifbrpv_gen = sc->sc_pvlans_gen;

err:
	rw_exit(&sc->sc_rule_lock);
	return (error);
}

static int
veb_nfind_pvlan(struct veb_softc *sc, struct ifbrpvlan *ifbrpv)
{
	struct veb_pvlan key;
	struct veb_pvlan *vs;
	int error;

	if (ifbrpv->ifbrpv_type == IFBRPV_T_PRIMARY)
		return (veb_nfind_pvlan_primary(sc, ifbrpv));

	if (ifbrpv->ifbrpv_primary < EVL_VLID_MIN ||
	    ifbrpv->ifbrpv_primary > EVL_VLID_MAX)
		return (EINVAL);

	key.v_primary = ifbrpv->ifbrpv_primary;
	key.v_secondary = ifbrpv->ifbrpv_secondary;
	key.v_type = ifbrpv->ifbrpv_type;

	error = rw_enter(&sc->sc_rule_lock, RW_READ|RW_INTR);
	if (error != 0)
		return (error);

	vs = RBT_NFIND(veb_pvlan_vs, &sc->sc_pvlans_vs, &key);
	if (vs == NULL ||
	    vs->v_primary != ifbrpv->ifbrpv_primary ||
	    vs->v_type != ifbrpv->ifbrpv_type) {
		error = ENOENT;
		goto err;
	}

	ifbrpv->ifbrpv_secondary = vs->v_secondary;
	ifbrpv->ifbrpv_gen = sc->sc_pvlans_gen;

err:
	rw_exit(&sc->sc_rule_lock);
	return (error);
}

static int
veb_rule_add(struct veb_softc *sc, const struct ifbrlreq *ifbr)
{
	const struct ifbrarpf *brla = &ifbr->ifbr_arpf;
	struct veb_rule vr, *vrp;
	struct veb_port *p;
	int error;

	memset(&vr, 0, sizeof(vr));

	switch (ifbr->ifbr_action) {
	case BRL_ACTION_BLOCK:
		vr.vr_action = VEB_R_BLOCK;
		break;
	case BRL_ACTION_PASS:
		vr.vr_action = VEB_R_PASS;
		break;
	/* XXX VEB_R_MATCH */
	default:
		return (EINVAL);
	}

	if (!ISSET(ifbr->ifbr_flags, BRL_FLAG_IN|BRL_FLAG_OUT))
		return (EINVAL);
	if (ISSET(ifbr->ifbr_flags, BRL_FLAG_IN))
		SET(vr.vr_flags, VEB_R_F_IN);
	if (ISSET(ifbr->ifbr_flags, BRL_FLAG_OUT))
		SET(vr.vr_flags, VEB_R_F_OUT);

	if (ISSET(ifbr->ifbr_flags, BRL_FLAG_SRCVALID)) {
		SET(vr.vr_flags, VEB_R_F_SRC);
		vr.vr_src = ether_addr_to_e64(&ifbr->ifbr_src);
	}
	if (ISSET(ifbr->ifbr_flags, BRL_FLAG_DSTVALID)) {
		SET(vr.vr_flags, VEB_R_F_DST);
		vr.vr_dst = ether_addr_to_e64(&ifbr->ifbr_dst);
	}

	/* ARP rule */
	if (ISSET(brla->brla_flags, BRLA_ARP|BRLA_RARP)) {
		if (ISSET(brla->brla_flags, BRLA_ARP))
			SET(vr.vr_flags, VEB_R_F_ARP);
		if (ISSET(brla->brla_flags, BRLA_RARP))
			SET(vr.vr_flags, VEB_R_F_RARP);

		if (ISSET(brla->brla_flags, BRLA_SHA)) {
			SET(vr.vr_flags, VEB_R_F_SHA);
			vr.vr_arp_sha = brla->brla_sha;
		}
		if (ISSET(brla->brla_flags, BRLA_THA)) {
			SET(vr.vr_flags, VEB_R_F_THA);
			vr.vr_arp_tha = brla->brla_tha;
		}
		if (ISSET(brla->brla_flags, BRLA_SPA)) {
			SET(vr.vr_flags, VEB_R_F_SPA);
			vr.vr_arp_spa = brla->brla_spa;
		}
		if (ISSET(brla->brla_flags, BRLA_TPA)) {
			SET(vr.vr_flags, VEB_R_F_TPA);
			vr.vr_arp_tpa = brla->brla_tpa;
		}
		vr.vr_arp_op = htons(brla->brla_op);
	}

	if (ifbr->ifbr_tagname[0] != '\0') {
#if NPF > 0
		vr.vr_pftag = pf_tagname2tag((char *)ifbr->ifbr_tagname, 1);
		if (vr.vr_pftag == 0)
			return (ENOMEM);
#else
		return (EINVAL);
#endif
	}

	p = veb_port_get(sc, ifbr->ifbr_ifsname);
	if (p == NULL) {
		error = ESRCH;
		goto error;
	}

	vrp = pool_get(&veb_rule_pool, PR_WAITOK|PR_LIMITFAIL|PR_ZERO);
	if (vrp == NULL) {
		error = ENOMEM;
		goto port_put;
	}

	*vrp = vr;

	/* there's one big lock on a veb for all ports */
	error = rw_enter(&sc->sc_rule_lock, RW_WRITE|RW_INTR);
	if (error != 0)
		goto rule_put;

	TAILQ_INSERT_TAIL(&p->p_vrl, vrp, vr_entry);
	p->p_nvrl++;
	if (ISSET(vr.vr_flags, VEB_R_F_OUT)) {
		SMR_TAILQ_INSERT_TAIL_LOCKED(&p->p_vr_list[0],
		    vrp, vr_lentry[0]);
	}
	if (ISSET(vr.vr_flags, VEB_R_F_IN)) {
		SMR_TAILQ_INSERT_TAIL_LOCKED(&p->p_vr_list[1],
		    vrp, vr_lentry[1]);
	}

	rw_exit(&sc->sc_rule_lock);
	veb_port_put(sc, p);

	return (0);

rule_put:
	pool_put(&veb_rule_pool, vrp);
port_put:
	veb_port_put(sc, p);
error:
#if NPF > 0
	pf_tag_unref(vr.vr_pftag);
#endif
	return (error);
}

static void
veb_rule_list_free(struct veb_rule *nvr)
{
	struct veb_rule *vr;

	while ((vr = nvr) != NULL) {
		nvr = TAILQ_NEXT(vr, vr_entry);
		pool_put(&veb_rule_pool, vr);
	}
}

static int
veb_rule_list_flush(struct veb_softc *sc, const struct ifbrlreq *ifbr)
{
	struct veb_port *p;
	struct veb_rule *vr;
	int error;

	p = veb_port_get(sc, ifbr->ifbr_ifsname);
	if (p == NULL)
		return (ESRCH);

	error = rw_enter(&sc->sc_rule_lock, RW_WRITE|RW_INTR);
	if (error != 0) {
		veb_port_put(sc, p);
		return (error);
	}

	/* take all the rules away */
	vr = TAILQ_FIRST(&p->p_vrl);

	/* reset the lists and counts of rules */
	TAILQ_INIT(&p->p_vrl);
	p->p_nvrl = 0;
	SMR_TAILQ_INIT(&p->p_vr_list[0]);
	SMR_TAILQ_INIT(&p->p_vr_list[1]);

	rw_exit(&sc->sc_rule_lock);
	veb_port_put(sc, p);

	smr_barrier();
	veb_rule_list_free(vr);

	return (0);
}

static void
veb_rule2ifbr(struct ifbrlreq *ifbr, const struct veb_rule *vr)
{
	switch (vr->vr_action) {
	case VEB_R_PASS:
		ifbr->ifbr_action = BRL_ACTION_PASS;
		break;
	case VEB_R_BLOCK:
		ifbr->ifbr_action = BRL_ACTION_BLOCK;
		break;
	}

	if (ISSET(vr->vr_flags, VEB_R_F_IN))
		SET(ifbr->ifbr_flags, BRL_FLAG_IN);
	if (ISSET(vr->vr_flags, VEB_R_F_OUT))
		SET(ifbr->ifbr_flags, BRL_FLAG_OUT);

	if (ISSET(vr->vr_flags, VEB_R_F_SRC)) {
		SET(ifbr->ifbr_flags, BRL_FLAG_SRCVALID);
		ether_e64_to_addr(&ifbr->ifbr_src, vr->vr_src);
	}
	if (ISSET(vr->vr_flags, VEB_R_F_DST)) {
		SET(ifbr->ifbr_flags, BRL_FLAG_DSTVALID);
		ether_e64_to_addr(&ifbr->ifbr_dst, vr->vr_dst);
	}

	/* ARP rule */
	if (ISSET(vr->vr_flags, VEB_R_F_ARP|VEB_R_F_RARP)) {
		struct ifbrarpf *brla = &ifbr->ifbr_arpf;

		if (ISSET(vr->vr_flags, VEB_R_F_ARP))
			SET(brla->brla_flags, BRLA_ARP);
		if (ISSET(vr->vr_flags, VEB_R_F_RARP))
			SET(brla->brla_flags, BRLA_RARP);

		if (ISSET(vr->vr_flags, VEB_R_F_SHA)) {
			SET(brla->brla_flags, BRLA_SHA);
			brla->brla_sha = vr->vr_arp_sha;
		}
		if (ISSET(vr->vr_flags, VEB_R_F_THA)) {
			SET(brla->brla_flags, BRLA_THA);
			brla->brla_tha = vr->vr_arp_tha;
		}

		if (ISSET(vr->vr_flags, VEB_R_F_SPA)) {
			SET(brla->brla_flags, BRLA_SPA);
			brla->brla_spa = vr->vr_arp_spa;
		}
		if (ISSET(vr->vr_flags, VEB_R_F_TPA)) {
			SET(brla->brla_flags, BRLA_TPA);
			brla->brla_tpa = vr->vr_arp_tpa;
		}

		brla->brla_op = ntohs(vr->vr_arp_op);
	}

#if NPF > 0
	if (vr->vr_pftag != 0)
		pf_tag2tagname(vr->vr_pftag, ifbr->ifbr_tagname);
#endif
}

static int
veb_rule_list_get(struct veb_softc *sc, struct ifbrlconf *ifbrl)
{
	struct veb_port *p;
	struct veb_rule *vr;
	struct ifbrlreq *ifbr, *ifbrs;
	int error = 0;
	size_t len;

	p = veb_port_get(sc, ifbrl->ifbrl_ifsname);
	if (p == NULL)
		return (ESRCH);

	len = p->p_nvrl; /* estimate */
	if (ifbrl->ifbrl_len == 0 || len == 0) {
		ifbrl->ifbrl_len = len * sizeof(*ifbrs);
		goto port_put;
	}

	error = rw_enter(&sc->sc_rule_lock, RW_READ|RW_INTR);
	if (error != 0)
		goto port_put;

	ifbrs = mallocarray(p->p_nvrl, sizeof(*ifbrs), M_TEMP,
	    M_WAITOK|M_CANFAIL|M_ZERO);
	if (ifbrs == NULL) {
		rw_exit(&sc->sc_rule_lock);
		goto port_put;
	}
	len = p->p_nvrl * sizeof(*ifbrs);

	ifbr = ifbrs;
	TAILQ_FOREACH(vr, &p->p_vrl, vr_entry) {
		strlcpy(ifbr->ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(ifbr->ifbr_ifsname, p->p_ifp0->if_xname, IFNAMSIZ);
		veb_rule2ifbr(ifbr, vr);

		ifbr++;
	}

	rw_exit(&sc->sc_rule_lock);

	error = copyout(ifbrs, ifbrl->ifbrl_buf, min(len, ifbrl->ifbrl_len));
	if (error == 0)
		ifbrl->ifbrl_len = len;
	free(ifbrs, M_TEMP, len);

port_put:
	veb_port_put(sc, p);
	return (error);
}

static int
veb_port_list(struct veb_softc *sc, struct ifbifconf *bifc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct veb_ports *m;
	struct veb_port **ps;
	struct veb_port *p;
	struct ifnet *ifp0;
	struct ifbreq breq;
	int n = 0, error = 0;
	unsigned int i;

	NET_ASSERT_LOCKED();

	if (bifc->ifbic_len == 0) {
		m = SMR_PTR_GET_LOCKED(&sc->sc_ports);
		if (m != NULL)
			n += m->m_count;
		m = SMR_PTR_GET_LOCKED(&sc->sc_spans);
		if (m != NULL)
			n += m->m_count;
		goto done;
	}

	m = SMR_PTR_GET_LOCKED(&sc->sc_ports);
	if (m != NULL) {
		ps = veb_ports_array(m);
		for (i = 0; i < m->m_count; i++) {
			if (bifc->ifbic_len < sizeof(breq))
				break;

			p = ps[i];

			memset(&breq, 0, sizeof(breq));

			ifp0 = p->p_ifp0;

			strlcpy(breq.ifbr_name, ifp->if_xname, IFNAMSIZ);
			strlcpy(breq.ifbr_ifsname, ifp0->if_xname, IFNAMSIZ);

			breq.ifbr_ifsflags = p->p_bif_flags;
			breq.ifbr_portno = ifp0->if_index;
			breq.ifbr_protected = p->p_protected;
			breq.ifbr_pvid = p->p_pvid;
			if ((error = copyout(&breq, bifc->ifbic_req + n,
			    sizeof(breq))) != 0)
				goto done;

			bifc->ifbic_len -= sizeof(breq);
			n++;
		}
	}

	m = SMR_PTR_GET_LOCKED(&sc->sc_spans);
	if (m != NULL) {
		ps = veb_ports_array(m);
		for (i = 0; i < m->m_count; i++) {
			if (bifc->ifbic_len < sizeof(breq))
				break;

			p = ps[i];

			memset(&breq, 0, sizeof(breq));

			strlcpy(breq.ifbr_name, ifp->if_xname, IFNAMSIZ);
			strlcpy(breq.ifbr_ifsname, p->p_ifp0->if_xname,
			    IFNAMSIZ);

			breq.ifbr_ifsflags = p->p_bif_flags;
			if ((error = copyout(&breq, bifc->ifbic_req + n,
			    sizeof(breq))) != 0)
				goto done;

			bifc->ifbic_len -= sizeof(breq);
			n++;
		}
	}

done:
	bifc->ifbic_len = n * sizeof(breq);
	return (error);
}

static int
veb_port_set_flags(struct veb_softc *sc, struct ifbreq *ifbr)
{
	struct veb_port *p;

	if (ISSET(ifbr->ifbr_ifsflags, ~VEB_IFBIF_FLAGS))
		return (EINVAL);
	if (ISSET(ifbr->ifbr_ifsflags, IFBIF_LOCKED) &&
	    ISSET(ifbr->ifbr_ifsflags, IFBIF_LEARNING|IFBIF_DISCOVER))
		return (EINVAL);

	p = veb_port_get(sc, ifbr->ifbr_ifsname);
	if (p == NULL)
		return (ESRCH);

	p->p_bif_flags = ifbr->ifbr_ifsflags;

	veb_port_put(sc, p);
	return (0);
}

static int
veb_port_get_flags(struct veb_softc *sc, struct ifbreq *ifbr)
{
	struct veb_port *p;

	p = veb_port_get(sc, ifbr->ifbr_ifsname);
	if (p == NULL)
		return (ESRCH);

	ifbr->ifbr_ifsflags = p->p_bif_flags;
	ifbr->ifbr_portno = p->p_ifp0->if_index;
	ifbr->ifbr_protected = p->p_protected;

	veb_port_put(sc, p);
	return (0);
}

static int
veb_add_addr(struct veb_softc *sc, const struct ifbareq *ifba)
{
	struct veb_port *p;
	int error = 0;
	unsigned int type;
	uint16_t vp, vs;

	if (ISSET(ifba->ifba_flags, ~IFBAF_TYPEMASK))
		return (EINVAL);
	switch (ifba->ifba_flags & IFBAF_TYPEMASK) {
	case IFBAF_DYNAMIC:
		type = EBE_DYNAMIC;
		break;
	case IFBAF_STATIC:
		type = EBE_STATIC;
		break;
	default:
		return (EINVAL);
	}

	if (ifba->ifba_dstsa.ss_family != AF_UNSPEC)
		return (EAFNOSUPPORT);

	p = veb_port_get(sc, ifba->ifba_ifsname);
	if (p == NULL)
		return (ESRCH);

	vs = p->p_pvid;
	if (vs < IFBR_PVID_MIN ||
	    vs > IFBR_PVID_MAX) {
		error = EADDRNOTAVAIL;
		goto put;
	}

	smr_read_enter();
	vp = veb_pvlan(sc, vs);
	smr_read_leave();

	vp &= VEB_PVLAN_V_MASK;

	error = etherbridge_add_addr(&sc->sc_eb, p,
	    vp, vs, &ifba->ifba_dst, type);
put:
	veb_port_put(sc, p);

	return (error);
}

static int
veb_add_vid_addr(struct veb_softc *sc, const struct ifbvareq *ifbva)
{
	struct veb_port *p;
	int error = 0;
	unsigned int type;
	uint16_t vp, vs;

	if (ISSET(ifbva->ifbva_flags, ~IFBAF_TYPEMASK))
		return (EINVAL);
	switch (ifbva->ifbva_flags & IFBAF_TYPEMASK) {
	case IFBAF_DYNAMIC:
		type = EBE_DYNAMIC;
		break;
	case IFBAF_STATIC:
		type = EBE_STATIC;
		break;
	default:
		return (EINVAL);
	}

	if (ifbva->ifbva_dstsa.ss_family != AF_UNSPEC)
		return (EAFNOSUPPORT);

	if (ifbva->ifbva_vid != EVL_VLID_NULL) {
		if (ifbva->ifbva_vid < EVL_VLID_MIN ||
		    ifbva->ifbva_vid > EVL_VLID_MAX)
			return (EINVAL);
	}

	p = veb_port_get(sc, ifbva->ifbva_ifsname);
	if (p == NULL)
		return (ESRCH);

	vs = ifbva->ifbva_vid;
	if (vs == EVL_VLID_NULL) {
		vs = p->p_pvid;
		if (vs < IFBR_PVID_MIN ||
		    vs > IFBR_PVID_MAX) {
			error = EADDRNOTAVAIL;
			goto put;
		}
	}

	smr_read_enter();
	vp = veb_pvlan(sc, vs);
	smr_read_leave();

	vp &= VEB_PVLAN_V_MASK;

	error = etherbridge_add_addr(&sc->sc_eb, p,
	    vp, vs, &ifbva->ifbva_dst, type);

put:
	veb_port_put(sc, p);

	return (error);
}

static int
veb_del_addr(struct veb_softc *sc, const struct ifbareq *ifba)
{
	uint16_t vp, vs;

	vs = sc->sc_dflt_pvid;
	if (vs == IFBR_PVID_NONE)
		return (ESRCH);

	smr_read_enter();
	vp = veb_pvlan(sc, vs);
	smr_read_leave();

	vp &= VEB_PVLAN_V_MASK;

	return (etherbridge_del_addr(&sc->sc_eb, vp, &ifba->ifba_dst));
}

static int
veb_del_vid_addr(struct veb_softc *sc, const struct ifbvareq *ifbva)
{
	uint16_t vp, vs;

	vs = ifbva->ifbva_vid;

	if (vs < EVL_VLID_MIN ||
	    vs > EVL_VLID_MAX)
		return (EINVAL);

	smr_read_enter();
	vp = veb_pvlan(sc, vs);
	smr_read_leave();

	vp &= VEB_PVLAN_V_MASK;

	return (etherbridge_del_addr(&sc->sc_eb, vp, &ifbva->ifbva_dst));
}

static int
veb_p_ioctl(struct ifnet *ifp0, u_long cmd, caddr_t data)
{
	const struct ether_port *ep = ether_brport_get_locked(ifp0);
	struct veb_port *p;
	int error = 0;

	KASSERTMSG(ep != NULL,
	    "%s: %s called without an ether_brport set",
	    ifp0->if_xname, __func__);
	KASSERTMSG((ep->ep_input == veb_port_input) ||
	    (ep->ep_input == veb_span_input),
	    "%s called %s, but ep_input (%p) seems wrong",
	    ifp0->if_xname, __func__, ep->ep_input);

	p = ep->ep_port;

	switch (cmd) {
	case SIOCSIFADDR:
		error = EBUSY;
		break;

	default:
		error = (*p->p_ioctl)(ifp0, cmd, data);
		break;
	}

	return (error);
}

static int
veb_p_output(struct ifnet *ifp0, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	int (*p_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *) = NULL;
	const struct ether_port *ep;

	/* restrict transmission to bpf only */
	if ((m_tag_find(m, PACKET_TAG_DLT, NULL) == NULL)) {
		m_freem(m);
		return (EBUSY);
	}

	smr_read_enter();
	ep = ether_brport_get(ifp0);
	if (ep != NULL && ep->ep_input == veb_port_input) {
		struct veb_port *p = ep->ep_port;
		p_output = p->p_output; /* code doesn't go away */
	}
	smr_read_leave();

	if (p_output == NULL) {
		m_freem(m);
		return (ENXIO);
	}

	return ((*p_output)(ifp0, m, dst, rt));
}

/*
 * there must be an smr_barrier after ether_brport_clr() and before
 * veb_port is freed in veb_p_fini()
 */

static void
veb_p_unlink(struct veb_softc *sc, struct veb_port *p)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ifnet *ifp0 = p->p_ifp0;

	ifp0->if_ioctl = p->p_ioctl;
	ifp0->if_output = p->p_output;

	ether_brport_clr(ifp0); /* needs an smr_barrier */

	if_detachhook_del(ifp0, &p->p_dtask);
	if_linkstatehook_del(ifp0, &p->p_ltask);

	if (!ISSET(p->p_bif_flags, IFBIF_SPAN)) {
		if (ifpromisc(ifp0, 0) != 0) {
			log(LOG_WARNING, "%s %s: unable to disable promisc\n",
			    ifp->if_xname, ifp0->if_xname);
		}

		etherbridge_detach_port(&sc->sc_eb, p);
	}
}

static void
veb_p_fini(struct veb_port *p)
{
	struct ifnet *ifp0 = p->p_ifp0;
	struct veb_port_cpu *c;
	struct cpumem_iter cmi;

	CPUMEM_FOREACH(c, &cmi, p->p_percpu)
		veb_ep_brport_rele(c, p);
	refcnt_finalize(&p->p_refs, "vebpdtor");
	veb_rule_list_free(TAILQ_FIRST(&p->p_vrl));

	if_put(ifp0);
	veb_free_vid_map(p->p_vid_map);
#if NKSTAT > 0
	veb_port_kstat_detach(p);
#endif
	cpumem_free(p->p_percpu, M_DEVBUF, sizeof(*c));
	free(p, M_DEVBUF, sizeof(*p)); /* hope you didn't forget smr_barrier */
}

static void
veb_p_dtor(struct veb_softc *sc, struct veb_port *p)
{
	struct veb_ports **ports_ptr;
	struct veb_ports *om, *nm;

	ports_ptr = ISSET(p->p_bif_flags, IFBIF_SPAN) ?
	    &sc->sc_spans : &sc->sc_ports;

	om = SMR_PTR_GET_LOCKED(ports_ptr);
	nm = veb_ports_remove(om, p);
	SMR_PTR_SET_LOCKED(ports_ptr, nm);

	veb_p_unlink(sc, p);

	smr_barrier();
	refcnt_finalize(&om->m_refs, "vebports");
	veb_ports_destroy(om);

	veb_p_fini(p);
}

static void
veb_p_detach(void *arg)
{
	struct veb_port *p = arg;
	struct veb_softc *sc = p->p_veb;

	NET_ASSERT_LOCKED();

	veb_p_dtor(sc, p);
}

static int
veb_p_active(struct veb_port *p)
{
	struct ifnet *ifp0 = p->p_ifp0;

	return (ISSET(ifp0->if_flags, IFF_RUNNING) &&
	    LINK_STATE_IS_UP(ifp0->if_link_state));
}

static void
veb_p_linkch(void *arg)
{
	struct veb_port *p = arg;
	u_char link_state = LINK_STATE_FULL_DUPLEX;

	NET_ASSERT_LOCKED();

	if (!veb_p_active(p))
		link_state = LINK_STATE_DOWN;

	p->p_link_state = link_state;
}

static int
veb_up(struct veb_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	int error;

	error = etherbridge_up(&sc->sc_eb);
	if (error != 0)
		return (error);

	NET_ASSERT_LOCKED();
	SET(ifp->if_flags, IFF_RUNNING);

	return (0);
}

static int
veb_iff(struct veb_softc *sc)
{
	return (0);
}

static int
veb_down(struct veb_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	int error;

	error = etherbridge_down(&sc->sc_eb);
	if (error != 0)
		return (0);

	NET_ASSERT_LOCKED();
	CLR(ifp->if_flags, IFF_RUNNING);

	return (0);
}

static int
veb_eb_port_cmp(void *arg, void *a, void *b)
{
	struct veb_port *pa = a, *pb = b;
	return (pa == pb);
}

static void *
veb_eb_port_take(void *arg, void *port)
{
	struct veb_port *p = port;

	veb_p_take(p);

	return (p);
}

static void
veb_eb_port_rele(void *arg, void *port)
{
	struct veb_port *p = port;

	veb_p_rele(p);
}

static void *
veb_ep_brport_take(void *port)
{
	struct veb_port *p = port;
	struct veb_port_cpu *c;

	c = cpumem_enter(p->p_percpu);
	refcnt_take(&c->c_refs);
	cpumem_leave(p->p_percpu, c);

	return (c);
}

static void
veb_ep_brport_rele(void *cpu, void *port)
{
	struct veb_port_cpu *c = cpu;

	if (refcnt_rele(&c->c_refs))
		veb_p_rele(port);
}

static size_t
veb_eb_port_ifname(void *arg, char *dst, size_t len, void *port)
{
	struct veb_port *p = port;

	return (strlcpy(dst, p->p_ifp0->if_xname, len));
}

static void
veb_eb_port_sa(void *arg, struct sockaddr_storage *ss, void *port)
{
	ss->ss_family = AF_UNSPEC;
}

RBT_GENERATE(veb_pvlan_vp, veb_pvlan, v_entry, veb_pvlan_vp_cmp);
RBT_GENERATE(veb_pvlan_vs, veb_pvlan, v_entry, veb_pvlan_vs_cmp);

#if NKSTAT > 0
static const char * const veb_port_counter_names[veb_c_ncounters] = {
	[veb_c_double_tag]		= "double-tagged",
	[veb_c_tagged_filter_in]	= "tagged-in",
	[veb_c_untagged_none]		= "untagged",
	[veb_c_pvptags_in]		= "pvptags-in",
	[veb_c_locked]			= "locked",
	[veb_c_bpfilter]		= "bpfilter",
	[veb_c_blocknonip_in]		= "blocknonip-in",
	[veb_c_svlan]			= "svlan",
	[veb_c_rule_in]			= "rules-in",

	[veb_c_hairpin]			= "hairpin",
	[veb_c_protected]		= "protected",
	[veb_c_pvlan]			= "pvlan",
	[veb_c_pvptags_out]		= "pvptags-out",
	[veb_c_tagged_filter_out]	= "tagged-out",
	[veb_c_rule_out]		= "rules-out",
	[veb_c_blocknonip_out]		= "blocknonip-out",
};

struct veb_port_kstat {
	struct kstat_kv		interface;
	struct kstat_kv		counters[veb_c_ncounters];
};

static int
veb_port_kstat_read(struct kstat *ks)
{
	struct veb_port *p = ks->ks_softc;
	struct veb_port_kstat *kvs = ks->ks_data;
	struct veb_port_cpu *c;
	uint64_t counters[veb_c_ncounters];
	struct cpumem_iter cmi;
	unsigned int gen, i;

	for (i = 0; i < veb_c_ncounters; i++)
		kstat_kv_u64(&kvs->counters[i]) = 0;

	CPUMEM_FOREACH(c, &cmi, p->p_percpu) {
		pc_cons_enter(&c->c_lock, &gen);
		do {
			for (i = 0; i < veb_c_ncounters; i++)
				counters[i] = c->c_counters[i];
		} while (pc_cons_leave(&c->c_lock, &gen) != 0);

		for (i = 0; i < veb_c_ncounters; i++)
			kstat_kv_u64(&kvs->counters[i]) += counters[i];
	}

	nanouptime(&ks->ks_updated);

	return (0);
}


static void
veb_port_kstat_attach(struct veb_port *p)
{
	static const char veb_port_kstat_name[] = "veb-port";
	struct veb_softc *sc = p->p_veb;
	struct ifnet *ifp = &sc->sc_if;
	struct ifnet *ifp0 = p->p_ifp0;
	struct kstat *ks;
	struct veb_port_kstat *kvs;
	unsigned int i;

	kvs = malloc(sizeof(*kvs), M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (kvs == NULL) {
		log(LOG_WARNING, "%s %s: unable to allocate %s kstat\n",
		    ifp->if_xname, ifp0->if_xname, veb_port_kstat_name);
		return;
	}

	ks = kstat_create(ifp->if_xname, 0,
	    veb_port_kstat_name, ifp0->if_index,
	    KSTAT_T_KV, 0);
	if (ks == NULL) {
		log(LOG_WARNING, "%s %s: unable to create %s kstat\n",
		    ifp->if_xname, ifp0->if_xname, veb_port_kstat_name);
		free(kvs, M_DEVBUF, sizeof(*kvs));
		return;
	}

	kstat_kv_init(&kvs->interface, "interface", KSTAT_KV_T_ISTR);
	strlcpy(kstat_kv_istr(&kvs->interface), ifp0->if_xname,
	    sizeof(kstat_kv_istr(&kvs->interface)));

	for (i = 0; i < veb_c_ncounters; i++) {
		kstat_kv_unit_init(&kvs->counters[i],
		    veb_port_counter_names[i],
		    KSTAT_KV_T_COUNTER64, KSTAT_KV_U_PACKETS);
	}

	ks->ks_softc = p;
	ks->ks_data = kvs;
	ks->ks_datalen = sizeof(*kvs);
	ks->ks_read = veb_port_kstat_read;

	kstat_install(ks);

	p->p_kstat = ks;
}

static void
veb_port_kstat_detach(struct veb_port *p)
{
	struct kstat *ks = p->p_kstat;
	struct veb_port_kstat *kvs;

	if (ks == NULL)
		return;

	p->p_kstat = NULL;

	kstat_remove(ks);
	kvs = ks->ks_data;
	kstat_destroy(ks);

	free(kvs, M_DEVBUF, sizeof(*kvs));
}
#endif /* NKSTAT > 0 */

/*
 * virtual ethernet bridge port
 */

static int
vport_clone_create(struct if_clone *ifc, int unit)
{
	struct vport_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO|M_CANFAIL);
	if (sc == NULL)
		return (ENOMEM);

	ifp = &sc->sc_ac.ac_if;

	snprintf(ifp->if_xname, IFNAMSIZ, "%s%d", ifc->ifc_name, unit);

	ifp->if_softc = sc;
	ifp->if_type = IFT_ETHER;
	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	ifp->if_ioctl = vport_ioctl;
	ifp->if_enqueue = vport_enqueue;
	ifp->if_qstart = vport_start;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED | IFXF_MPSAFE;

	ifp->if_capabilities = 0;
#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif
	ifp->if_capabilities |= IFCAP_CSUM_IPv4;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv6 | IFCAP_CSUM_UDPv6;

	ether_fakeaddr(ifp);

	if_counters_alloc(ifp);
	if_attach(ifp);
	if_attach_iqueues(ifp, softnet_count());
	ether_ifattach(ifp);

	return (0);
}

static int
vport_clone_destroy(struct ifnet *ifp)
{
	struct vport_softc *sc = ifp->if_softc;

	NET_LOCK();
	sc->sc_dead = 1;

	if (ISSET(ifp->if_flags, IFF_RUNNING))
		vport_down(sc);
	NET_UNLOCK();

	ether_ifdetach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

static int
vport_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vport_softc *sc = ifp->if_softc;
	int error = 0;

	if (sc->sc_dead)
		return (ENXIO);

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = vport_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = vport_down(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	if (error == ENETRESET)
		error = vport_iff(sc);

	return (error);
}

static int
vport_up(struct vport_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	NET_ASSERT_LOCKED();
	SET(ifp->if_flags, IFF_RUNNING);

	return (0);
}

static int
vport_iff(struct vport_softc *sc)
{
	return (0);
}

static int
vport_down(struct vport_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	NET_ASSERT_LOCKED();
	CLR(ifp->if_flags, IFF_RUNNING);

	return (0);
}

static int
vport_if_enqueue(struct ifnet *ifp, struct mbuf *m, struct netstack *ns)
{
	uint16_t csum;
	int rv = 0;

	/*
	 * switching an l2 packet toward a vport means pushing it
	 * into the network stack. this function exists to make
	 * if_vinput compat with veb calling if_enqueue.
	 */

	/* handle packets coming from a different vport into this one */
	csum = m->m_pkthdr.csum_flags;
	if (ISSET(csum, M_IPV4_CSUM_OUT))
		SET(csum, M_IPV4_CSUM_IN_OK);
	if (ISSET(csum, M_TCP_CSUM_OUT))
		SET(csum, M_TCP_CSUM_IN_OK);
	if (ISSET(csum, M_UDP_CSUM_OUT))
		SET(csum, M_UDP_CSUM_IN_OK);
	if (ISSET(csum, M_ICMP_CSUM_OUT))
		SET(csum, M_ICMP_CSUM_IN_OK);
	m->m_pkthdr.csum_flags = csum;

	if (ns != NULL) {
		/* this is already running in a softnet context */
		if_vinput(ifp, m, ns);
	} else {
		/* move the packet to a softnet context for processing */
		struct ifiqueue *ifiq;
		unsigned int flow = 0;

		if (ISSET(m->m_pkthdr.csum_flags, M_FLOWID))
			flow = m->m_pkthdr.ph_flowid;

		ifiq = ifp->if_iqs[flow % ifp->if_niqs];
		rv = ifiq_enqueue_qlim(ifiq, m, 8192);
	}

	return (rv);
}

static int
vport_enqueue(struct ifnet *ifp, struct mbuf *m)
{
	struct arpcom *ac;
	const struct ether_port *ep;
	void *ref;
	int error = ENETDOWN;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	/*
	 * a packet sent from the l3 stack out a vport goes into
	 * veb for switching out another port.
	 */

#if NPF > 0
	/*
	 * there's no relationship between pf states in the l3 stack
	 * and the l2 bridge.
	 */
	pf_pkt_addr_changed(m);
#endif

	ac = (struct arpcom *)ifp;

	smr_read_enter();
	ep = SMR_PTR_GET(&ac->ac_brport);
	if (ep != NULL)
		ref = ep->ep_port_take(ep->ep_port);
	smr_read_leave();
	if (ep != NULL) {
		struct mbuf *(*input)(struct ifnet *, struct mbuf *,
		    uint64_t, void *, struct netstack *) = ep->ep_input;
		struct ether_header *eh;
		uint64_t dst;

		counters_pkt(ifp->if_counters, ifc_opackets, ifc_obytes,
		    m->m_pkthdr.len);

#if NBPFILTER > 0
		if_bpf = READ_ONCE(ifp->if_bpf);
		if (if_bpf != NULL)
			bpf_mtap_ether(if_bpf, m, BPF_DIRECTION_OUT);
#endif

		eh = mtod(m, struct ether_header *);
		dst = ether_addr_to_e64((struct ether_addr *)eh->ether_dhost);

		if (input == veb_vport_input)
			input = veb_port_input;
		m = (*input)(ifp, m, dst, ep->ep_port, NULL);

		error = 0;

		ep->ep_port_rele(ref, ep->ep_port);
	}

	m_freem(m);

	return (error);
}

static void
vport_start(struct ifqueue *ifq)
{
	ifq_purge(ifq);
}
