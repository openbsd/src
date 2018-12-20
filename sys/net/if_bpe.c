/*	$OpenBSD: if_bpe.c,v 1.1 2018/12/20 23:00:55 dlg Exp $ */
/*
 * Copyright (c) 2018 David Gwynne <dlg@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/timeout.h>
#include <sys/pool.h>
#include <sys/tree.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/rtable.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

/* for bridge stuff */
#include <net/if_bridge.h>


#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/if_bpe.h>

#define PBB_ITAG_ISID		0x00ffffff
#define PBB_ITAG_ISID_MIN	0x00000000
#define PBB_ITAG_ISID_MAX	0x00ffffff
#define PBB_ITAG_RES2		0x03000000	/* must be zero on input */
#define PBB_ITAG_RES1		0x04000000	/* ignore on input */
#define PBB_ITAG_UCA		0x08000000
#define PBB_ITAG_DEI		0x10000000
#define PBB_ITAG_PCP_SHIFT	29
#define PBB_ITAG_PCP_MASK	(0x7U << PBB_ITAG_PCP_SHIFT)

#define BPE_BRIDGE_AGE_TMO	100 /* seconds */

struct bpe_key {
	int			k_if;
	uint32_t		k_isid;

	RBT_ENTRY(bpe_tunnel)	k_entry;
};

RBT_HEAD(bpe_tree, bpe_key);

static inline int bpe_cmp(const struct bpe_key *, const struct bpe_key *);

RBT_PROTOTYPE(bpe_tree, bpe_key, k_entry, bpe_cmp);
RBT_GENERATE(bpe_tree, bpe_key, k_entry, bpe_cmp);

struct bpe_entry {
	struct ether_addr	be_c_da; /* customer address - must be first */
	struct ether_addr	be_b_da; /* bridge address */
	unsigned int		be_type;
#define BPE_ENTRY_DYNAMIC		0
#define BPE_ENTRY_STATIC		1
	struct refcnt		be_refs;
	time_t			be_age;

	RBT_ENTRY(bpe_entry)	be_entry;
};

RBT_HEAD(bpe_map, bpe_entry);

static inline int bpe_entry_cmp(const struct bpe_entry *,
    const struct bpe_entry *);

RBT_PROTOTYPE(bpe_map, bpe_entry, be_entry, bpe_entry_cmp);
RBT_GENERATE(bpe_map, bpe_entry, be_entry, bpe_entry_cmp);

struct bpe_softc {
	struct bpe_key		sc_key; /* must be first */
	struct arpcom		sc_ac;
	struct ifmedia		sc_media;
	int			sc_txhprio;
	uint8_t			sc_group[ETHER_ADDR_LEN];

	void *			sc_lh_cookie;
	void *			sc_dh_cookie;

	struct bpe_map		sc_bridge_map;
	struct rwlock		sc_bridge_lock;
	unsigned int		sc_bridge_num;
	unsigned int		sc_bridge_max;
	int			sc_bridge_tmo; /* seconds */
	struct timeout		sc_bridge_age;
};

void		bpeattach(int);

static int	bpe_clone_create(struct if_clone *, int);
static int	bpe_clone_destroy(struct ifnet *);

static void	bpe_start(struct ifnet *);
static int	bpe_ioctl(struct ifnet *, u_long, caddr_t);
static int	bpe_media_get(struct bpe_softc *, struct ifreq *);
static int	bpe_up(struct bpe_softc *);
static int	bpe_down(struct bpe_softc *);
static int	bpe_multi(struct bpe_softc *, struct ifnet *, u_long);
static int	bpe_set_vnetid(struct bpe_softc *, const struct ifreq *);
static void	bpe_set_group(struct bpe_softc *, uint32_t);
static int	bpe_set_parent(struct bpe_softc *, const struct if_parent *);
static int	bpe_get_parent(struct bpe_softc *, struct if_parent *);
static int	bpe_del_parent(struct bpe_softc *);
static void	bpe_link_hook(void *);
static void	bpe_link_state(struct bpe_softc *, u_char, uint64_t);
static void	bpe_detach_hook(void *);

static void	bpe_input_map(struct bpe_softc *,
		    const uint8_t *, const uint8_t *);
static void	bpe_bridge_age(void *);

static struct if_clone bpe_cloner =
    IF_CLONE_INITIALIZER("bpe", bpe_clone_create, bpe_clone_destroy);

static struct bpe_tree bpe_interfaces = RBT_INITIALIZER();
static struct rwlock bpe_lock = RWLOCK_INITIALIZER("bpeifs");
static struct pool bpe_entry_pool;

#define ether_cmp(_a, _b)	memcmp((_a), (_b), ETHER_ADDR_LEN)
#define ether_is_eq(_a, _b)	(ether_cmp((_a), (_b)) == 0)
#define ether_is_bcast(_a)	ether_is_eq((_a), etherbroadcastaddr)

void
bpeattach(int count)
{
	if_clone_attach(&bpe_cloner);
}

static int
bpe_clone_create(struct if_clone *ifc, int unit)
{
	struct bpe_softc *sc;
	struct ifnet *ifp;

	if (bpe_entry_pool.pr_size == 0) {
		pool_init(&bpe_entry_pool, sizeof(struct bpe_entry), 0,
		    IPL_NONE, 0, "bpepl", NULL);
	}

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = &sc->sc_ac.ac_if;

	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d",
	    ifc->ifc_name, unit);

	sc->sc_key.k_if = 0;
	sc->sc_key.k_isid = 0;
	bpe_set_group(sc, 0);

	sc->sc_txhprio = IF_HDRPRIO_PACKET;

	rw_init(&sc->sc_bridge_lock, "bpebr");
	RBT_INIT(bpe_map, &sc->sc_bridge_map);
	sc->sc_bridge_num = 0;
	sc->sc_bridge_max = 100; /* XXX */
	sc->sc_bridge_tmo = 240;
	timeout_set_proc(&sc->sc_bridge_age, bpe_bridge_age, sc);

	ifp->if_softc = sc;
	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	ifp->if_ioctl = bpe_ioctl;
	ifp->if_start = bpe_start;
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ether_fakeaddr(ifp);

	if_attach(ifp);
	ether_ifattach(ifp);

	return (0);
}

static int
bpe_clone_destroy(struct ifnet *ifp)
{
	struct bpe_softc *sc = ifp->if_softc;

	NET_LOCK();
	if (ISSET(ifp->if_flags, IFF_RUNNING))
		bpe_down(sc);
	NET_UNLOCK();

	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

static inline int
bpe_entry_valid(struct bpe_softc *sc, const struct bpe_entry *be)
{
	time_t diff;

	if (be == NULL)
		return (0);

	if (be->be_type == BPE_ENTRY_STATIC)
		return (1);

	diff = time_uptime - be->be_age;
	if (diff < sc->sc_bridge_tmo)
		return (1);

	return (0);
}

static void
bpe_start(struct ifnet *ifp)
{
	struct bpe_softc *sc = ifp->if_softc;
	struct ifnet *ifp0;
	struct mbuf *m0, *m;
	struct ether_header *ceh;
	struct ether_header *beh;
	uint32_t itag, *itagp;
	int hlen = sizeof(*beh) + sizeof(*itagp);
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif
	int txprio;
	uint8_t prio;

	ifp0 = if_get(sc->sc_key.k_if);
	if (ifp0 == NULL || !ISSET(ifp0->if_flags, IFF_RUNNING)) {
		ifq_purge(&ifp->if_snd);
		goto done;
	}

	txprio = sc->sc_txhprio;

	while ((m0 = ifq_dequeue(&ifp->if_snd)) != NULL) {
#if NBPFILTER > 0
		if_bpf = ifp->if_bpf;
		if (if_bpf)
			bpf_mtap_ether(if_bpf, m0, BPF_DIRECTION_OUT);
#endif

		ceh = mtod(m0, struct ether_header *);

		/* force prepend of a whole mbuf because of alignment */
		m = m_get(M_DONTWAIT, m0->m_type);
		if (m == NULL) {
			m_freem(m0);
			continue;
		}

		M_MOVE_PKTHDR(m, m0);
		m->m_next = m0;

		m_align(m, 0);
		m->m_len = 0;

		m = m_prepend(m, hlen, M_DONTWAIT);
		if (m == NULL)
			continue;

		beh = mtod(m, struct ether_header *);

		if (ether_is_bcast(ceh->ether_dhost)) {
			memcpy(beh->ether_dhost, sc->sc_group,
			    sizeof(beh->ether_dhost));
		} else {
			struct bpe_entry *be;

			rw_enter_read(&sc->sc_bridge_lock);
			be = RBT_FIND(bpe_map, &sc->sc_bridge_map,
			    (struct bpe_entry *)ceh->ether_dhost);
			if (bpe_entry_valid(sc, be)) {
				memcpy(beh->ether_dhost, &be->be_b_da,
				    sizeof(beh->ether_dhost));
			} else {
				/* "flood" to unknown hosts */
				memcpy(beh->ether_dhost, sc->sc_group,
				    sizeof(beh->ether_dhost));
			}
			rw_exit_read(&sc->sc_bridge_lock);
		}

		memcpy(beh->ether_shost, ((struct arpcom *)ifp0)->ac_enaddr,
		    sizeof(beh->ether_shost));
		beh->ether_type = htons(ETHERTYPE_PBB);

		prio = (txprio == IF_HDRPRIO_PACKET) ?
		    m->m_pkthdr.pf.prio : txprio;

		itag = sc->sc_key.k_isid;
		itag |= prio << PBB_ITAG_PCP_SHIFT;
		itagp = (uint32_t *)(beh + 1);

		htobem32(itagp, itag);

		if_enqueue(ifp0, m);
	}

done:
	if_put(ifp0);
}

static void
bpe_bridge_age(void *arg)
{
	struct bpe_softc *sc = arg;
	struct bpe_entry *be, *nbe;
	time_t diff;

	timeout_add_sec(&sc->sc_bridge_age, BPE_BRIDGE_AGE_TMO);

	rw_enter_write(&sc->sc_bridge_lock);
	RBT_FOREACH_SAFE(be, bpe_map, &sc->sc_bridge_map, nbe) {
		if (be->be_type != BPE_ENTRY_DYNAMIC)
			continue;

		diff = time_uptime - be->be_age;
		if (diff < sc->sc_bridge_tmo)
			continue;

		sc->sc_bridge_num--;
		RBT_REMOVE(bpe_map, &sc->sc_bridge_map, be);
		if (refcnt_rele(&be->be_refs))
			pool_put(&bpe_entry_pool, be);
	}
	rw_exit_write(&sc->sc_bridge_lock);
}

static int
bpe_rtfind(struct bpe_softc *sc, struct ifbaconf *baconf)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct bpe_entry *be;
	struct ifbareq bareq;
	caddr_t uaddr, end;
	int error;
	time_t age;
	struct sockaddr_dl *sdl;

	if (baconf->ifbac_len == 0) {
		/* single read is atomic */
		baconf->ifbac_len = sc->sc_bridge_num * sizeof(bareq);
		return (0);
	}

	uaddr = baconf->ifbac_buf;
	end = uaddr + baconf->ifbac_len;

	rw_enter_read(&sc->sc_bridge_lock);
	RBT_FOREACH(be, bpe_map, &sc->sc_bridge_map) {
		if (uaddr >= end)
			break;

		memcpy(bareq.ifba_name, ifp->if_xname,
		    sizeof(bareq.ifba_name));
		memcpy(bareq.ifba_ifsname, ifp->if_xname,
		    sizeof(bareq.ifba_ifsname));
		memcpy(&bareq.ifba_dst, &be->be_c_da,
		    sizeof(bareq.ifba_dst));

		memset(&bareq.ifba_dstsa, 0, sizeof(bareq.ifba_dstsa));

		bzero(&bareq.ifba_dstsa, sizeof(bareq.ifba_dstsa));
		sdl = (struct sockaddr_dl *)&bareq.ifba_dstsa;
		sdl->sdl_len = sizeof(sdl);
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = 0;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_nlen = 0;
		sdl->sdl_alen = sizeof(be->be_b_da);
		CTASSERT(sizeof(sdl->sdl_data) >= sizeof(be->be_b_da));
		memcpy(sdl->sdl_data, &be->be_b_da, sizeof(be->be_b_da));

		switch (be->be_type) {
		case BPE_ENTRY_DYNAMIC:
			age = time_uptime - be->be_age;
			bareq.ifba_age = MIN(age, 0xff);
			bareq.ifba_flags = IFBAF_DYNAMIC;
			break;
		case BPE_ENTRY_STATIC:
			bareq.ifba_age = 0;
			bareq.ifba_flags = IFBAF_STATIC;
			break;
		}

		error = copyout(&bareq, uaddr, sizeof(bareq));
		if (error != 0) {
			rw_exit_read(&sc->sc_bridge_lock);
			return (error);
		}

		uaddr += sizeof(bareq);
	}
	baconf->ifbac_len = sc->sc_bridge_num * sizeof(bareq);
	rw_exit_read(&sc->sc_bridge_lock);

	return (0);
}

static void
bpe_flush_map(struct bpe_softc *sc, uint32_t flags)
{
	struct bpe_entry *be, *nbe;

	rw_enter_write(&sc->sc_bridge_lock);
	RBT_FOREACH_SAFE(be, bpe_map, &sc->sc_bridge_map, nbe) {
		if (flags == IFBF_FLUSHDYN &&
		    be->be_type != BPE_ENTRY_DYNAMIC)
			continue;

		RBT_REMOVE(bpe_map, &sc->sc_bridge_map, be);
		if (refcnt_rele(&be->be_refs))
			pool_put(&bpe_entry_pool, be);
	}
	rw_exit_write(&sc->sc_bridge_lock);
}

static int
bpe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bpe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifbrparam *bparam = (struct ifbrparam *)data;
	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = bpe_up(sc);
			else
				error = 0;
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = bpe_down(sc);
		}
		break;

	case SIOCSVNETID:
		error = bpe_set_vnetid(sc, ifr);
	case SIOCGVNETID:
		ifr->ifr_vnetid = sc->sc_key.k_isid;
		break;

	case SIOCSIFPARENT:
		error = bpe_set_parent(sc, (struct if_parent *)data);
		break;
	case SIOCGIFPARENT:
		error = bpe_get_parent(sc, (struct if_parent *)data);
		break;
	case SIOCDIFPARENT:
		error = bpe_del_parent(sc);
		break;

	case SIOCSTXHPRIO:
		if (ifr->ifr_hdrprio == IF_HDRPRIO_PACKET) /* use mbuf prio */
			;
		else if (ifr->ifr_hdrprio < IF_HDRPRIO_MIN ||
		    ifr->ifr_hdrprio > IF_HDRPRIO_MAX) {
			error = EINVAL;
			break;
		}

		sc->sc_txhprio = ifr->ifr_hdrprio;
		break;
	case SIOCGTXHPRIO:
		ifr->ifr_hdrprio = sc->sc_txhprio;
		break;

	case SIOCGIFMEDIA:
		error = bpe_media_get(sc, ifr);
		break;

	case SIOCBRDGSCACHE:
		error = suser(curproc);
		if (error != 0)
			break;

		if (bparam->ifbrp_csize < 1) {
			error = EINVAL;
			break;
		}

		/* commit */
		sc->sc_bridge_max = bparam->ifbrp_csize;
		break;
	case SIOCBRDGGCACHE:
		bparam->ifbrp_csize = sc->sc_bridge_max;
		break;

	case SIOCBRDGSTO:
		error = suser(curproc);
		if (error != 0)
			break;

		if (bparam->ifbrp_ctime < 8 ||
		    bparam->ifbrp_ctime > 3600) {
			error = EINVAL;
			break;
		}
		sc->sc_bridge_tmo = bparam->ifbrp_ctime;
		break;
	case SIOCBRDGGTO:
		bparam->ifbrp_ctime = sc->sc_bridge_tmo;
		break;

	case SIOCBRDGRTS:
		error = bpe_rtfind(sc, (struct ifbaconf *)data);
		break;
	case SIOCBRDGFLUSH:
		error = suser(curproc);
		if (error != 0)
			break;

		bpe_flush_map(sc,
		    ((struct ifbreq *)data)->ifbr_ifsflags);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	return (error);
}

static int
bpe_media_get(struct bpe_softc *sc, struct ifreq *ifr)
{
	struct ifnet *ifp0;
	int error;

	ifp0 = if_get(sc->sc_key.k_if);
	if (ifp0 != NULL)
		error = (*ifp0->if_ioctl)(ifp0, SIOCGIFMEDIA, (caddr_t)ifr);
	else
		error = ENOTTY;
	if_put(ifp0);

	return (error);
}

static int
bpe_up(struct bpe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ifnet *ifp0;
	struct bpe_softc *osc;
	int error = 0;
	u_int hardmtu;
	u_int hlen = sizeof(struct ether_header) + sizeof(uint32_t);

	KASSERT(!ISSET(ifp->if_flags, IFF_RUNNING));
	NET_ASSERT_LOCKED();

	ifp0 = if_get(sc->sc_key.k_if);
	if (ifp0 == NULL)
		return (ENXIO);

	/* check again if bpe will work on top of the parent */
	if (ifp0->if_type != IFT_ETHER) {
		error = EPROTONOSUPPORT;
		goto put;
	}

	hardmtu = ifp0->if_hardmtu;
	if (hardmtu < hlen) {
		error = ENOBUFS;
		goto put;
	}
	hardmtu -= hlen;
	if (ifp->if_mtu > hardmtu) {
		error = ENOBUFS;
		goto put;
	}

	/* parent is fine, let's prepare the bpe to handle packets */
	ifp->if_hardmtu = hardmtu;
	SET(ifp->if_flags, ifp0->if_flags & IFF_SIMPLEX);

	/* commit the interface */
	error = rw_enter(&bpe_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		goto scrub;

	osc = (struct bpe_softc *)RBT_INSERT(bpe_tree, &bpe_interfaces,
	    (struct bpe_key *)sc);
	rw_exit(&bpe_lock);

	if (osc != NULL) {
		error = EADDRINUSE;
		goto scrub;
	}

	if (bpe_multi(sc, ifp0, SIOCADDMULTI) != 0) {
		error = ENOTCONN;
		goto remove;
	}

	/* Register callback for physical link state changes */
	sc->sc_lh_cookie = hook_establish(ifp0->if_linkstatehooks, 1,
	    bpe_link_hook, sc);

	/* Register callback if parent wants to unregister */
	sc->sc_dh_cookie = hook_establish(ifp0->if_detachhooks, 0,
	    bpe_detach_hook, sc);

	/* we're running now */
	SET(ifp->if_flags, IFF_RUNNING);
	bpe_link_state(sc, ifp0->if_link_state, ifp0->if_baudrate);

	if_put(ifp0);

	timeout_add_sec(&sc->sc_bridge_age, BPE_BRIDGE_AGE_TMO);

	return (0);

remove:
	rw_enter(&bpe_lock, RW_WRITE);
	RBT_REMOVE(bpe_tree, &bpe_interfaces, (struct bpe_key *)sc);
	rw_exit(&bpe_lock);
scrub:
	CLR(ifp->if_flags, IFF_SIMPLEX);
	ifp->if_hardmtu = 0xffff;
put:
	if_put(ifp0);

	return (error);
}

static int
bpe_down(struct bpe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ifnet *ifp0;

	NET_ASSERT_LOCKED();

	CLR(ifp->if_flags, IFF_RUNNING);

	ifp0 = if_get(sc->sc_key.k_if);
	if (ifp0 != NULL) {
		hook_disestablish(ifp0->if_detachhooks, sc->sc_dh_cookie);
		hook_disestablish(ifp0->if_linkstatehooks, sc->sc_lh_cookie);
		bpe_multi(sc, ifp0, SIOCDELMULTI);
	}
	if_put(ifp0);

	rw_enter(&bpe_lock, RW_WRITE);
	RBT_REMOVE(bpe_tree, &bpe_interfaces, (struct bpe_key *)sc);
	rw_exit(&bpe_lock);

	CLR(ifp->if_flags, IFF_SIMPLEX);
	ifp->if_hardmtu = 0xffff;

	return (0);
}

static int
bpe_multi(struct bpe_softc *sc, struct ifnet *ifp0, u_long cmd)
{
	struct ifreq ifr;
	struct sockaddr *sa;

	/* make it convincing */
	CTASSERT(sizeof(ifr.ifr_name) == sizeof(ifp0->if_xname));
	memcpy(ifr.ifr_name, ifp0->if_xname, sizeof(ifr.ifr_name));

	sa = &ifr.ifr_addr;
	CTASSERT(sizeof(sa->sa_data) >= sizeof(sc->sc_group));

	sa->sa_family = AF_UNSPEC;
	memcpy(sa->sa_data, sc->sc_group, sizeof(sc->sc_group));

	return ((*ifp0->if_ioctl)(ifp0, cmd, (caddr_t)&ifr));
}

static void
bpe_set_group(struct bpe_softc *sc, uint32_t isid)
{
	uint8_t *group = sc->sc_group;

	group[0] = 0x01;
	group[1] = 0x1e;
	group[2] = 0x83;
	group[3] = isid >> 16;
	group[4] = isid >> 8;
	group[5] = isid >> 0;
}

static int
bpe_set_vnetid(struct bpe_softc *sc, const struct ifreq *ifr)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t isid;

	if (ifr->ifr_vnetid < PBB_ITAG_ISID_MIN ||
	    ifr->ifr_vnetid > PBB_ITAG_ISID_MAX)
		return (EINVAL);

	isid = ifr->ifr_vnetid;
	if (isid == sc->sc_key.k_isid)
		return (0);

	if (ISSET(ifp->if_flags, IFF_RUNNING))
		return (EBUSY);

	/* commit */
	sc->sc_key.k_isid = isid;
	bpe_set_group(sc, isid);
	bpe_flush_map(sc, IFBF_FLUSHALL);

	return (0);
}

static int
bpe_set_parent(struct bpe_softc *sc, const struct if_parent *p)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ifnet *ifp0;

	ifp0 = ifunit(p->ifp_parent); /* doesn't need an if_put */
	if (ifp0 == NULL)
		return (ENXIO);

	if (ifp0->if_type != IFT_ETHER)
		return (ENXIO);

	if (ifp0->if_index == sc->sc_key.k_if)
		return (0);

	if (ISSET(ifp->if_flags, IFF_RUNNING))
		return (EBUSY);

	/* commit */
	sc->sc_key.k_if = ifp0->if_index;
	bpe_flush_map(sc, IFBF_FLUSHALL);

	return (0);
}

static int
bpe_get_parent(struct bpe_softc *sc, struct if_parent *p)
{
	struct ifnet *ifp0;
	int error = 0;

	ifp0 = if_get(sc->sc_key.k_if);
	if (ifp0 == NULL)
		error = EADDRNOTAVAIL;
	else
		memcpy(p->ifp_parent, ifp0->if_xname, sizeof(p->ifp_parent));
	if_put(ifp0);

	return (error);
}

static int
bpe_del_parent(struct bpe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (ISSET(ifp->if_flags, IFF_RUNNING))
		return (EBUSY);

	/* commit */
	sc->sc_key.k_if = 0;
	bpe_flush_map(sc, IFBF_FLUSHALL);

	return (0);
}

static inline struct bpe_softc *
bpe_find(struct ifnet *ifp0, uint32_t isid)
{
	struct bpe_key k = { .k_if = ifp0->if_index, .k_isid = isid };
	struct bpe_softc *sc;

	rw_enter_read(&bpe_lock);
	sc = (struct bpe_softc *)RBT_FIND(bpe_tree, &bpe_interfaces, &k);
	rw_exit_read(&bpe_lock);

	return (sc);
}

static void
bpe_input_map(struct bpe_softc *sc, const uint8_t *ba, const uint8_t *ca)
{
	struct bpe_entry *be;
	int new = 0;

	if (ETHER_IS_MULTICAST(ca))
		return;

	/* remember where it came from */
	rw_enter_read(&sc->sc_bridge_lock);
	be = RBT_FIND(bpe_map, &sc->sc_bridge_map, (struct bpe_entry *)ca);
	if (be == NULL)
		new = 1;
	else {
		be->be_age = time_uptime; /* only a little bit racy */

		if (be->be_type != BPE_ENTRY_DYNAMIC ||
		    ether_is_eq(ba, &be->be_b_da))
			be = NULL;
		else
			refcnt_take(&be->be_refs);
	}
	rw_exit_read(&sc->sc_bridge_lock);

	if (new) {
		struct bpe_entry *obe;
		unsigned int num;

		be = pool_get(&bpe_entry_pool, PR_NOWAIT);
		if (be == NULL) {
			/* oh well */
			return;
		}

		memcpy(&be->be_c_da, ca, sizeof(be->be_c_da));
		memcpy(&be->be_b_da, ba, sizeof(be->be_b_da));
		be->be_type = BPE_ENTRY_DYNAMIC;
		refcnt_init(&be->be_refs);
		be->be_age = time_uptime;

		rw_enter_write(&sc->sc_bridge_lock);
		num = sc->sc_bridge_num;
		if (++num > sc->sc_bridge_max)
			obe = be;
		else {
			/* try and give the ref to the map */
			obe = RBT_INSERT(bpe_map, &sc->sc_bridge_map, be);
			if (obe == NULL) {
				/* count the insert */
				sc->sc_bridge_num = num;
			}
		}
		rw_exit_write(&sc->sc_bridge_lock);

		if (obe != NULL)
			pool_put(&bpe_entry_pool, obe);
	} else if (be != NULL) {
		rw_enter_write(&sc->sc_bridge_lock);
		memcpy(&be->be_b_da, ba, sizeof(be->be_b_da));
		rw_exit_write(&sc->sc_bridge_lock);

		if (refcnt_rele(&be->be_refs)) {
			/* ioctl may have deleted the entry */
			pool_put(&bpe_entry_pool, be);
		}
	}
}

void
bpe_input(struct ifnet *ifp0, struct mbuf *m)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct bpe_softc *sc;
	struct ifnet *ifp;
	struct ether_header *beh, *ceh;
	uint32_t *itagp, itag;
	unsigned int hlen = sizeof(*beh) + sizeof(*itagp) + sizeof(*ceh);
	struct mbuf *n;
	int off;

	if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (m == NULL) {
			/* pbb short ++ */
			return;
		}
	}

	beh = mtod(m, struct ether_header *);
	itagp = (uint32_t *)(beh + 1);
	itag = bemtoh32(itagp);

	if (itag & PBB_ITAG_RES2) {
		/* dropped by res2 ++ */
		goto drop;
	}

	sc = bpe_find(ifp0, itag & PBB_ITAG_ISID);
	if (sc == NULL) {
		/* no interface found */
		goto drop;
	}

	ceh = (struct ether_header *)(itagp + 1);

	bpe_input_map(sc, beh->ether_shost, ceh->ether_shost);

	m_adj(m, sizeof(*beh) + sizeof(*itagp));

	n = m_getptr(m, sizeof(*ceh), &off);
	if (n == NULL) {
		/* no data ++ */
		goto drop;
	}

	if (!ALIGNED_POINTER(mtod(n, caddr_t) + off, uint32_t)) {
		/* unaligned ++ */
		n = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
		m_freem(m);
		if (n == NULL)
			return;

		m = n;
	}

	ifp = &sc->sc_ac.ac_if;

	m->m_flags &= ~(M_BCAST|M_MCAST);
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);
	return;

drop:
	m_freem(m);
}

void
bpe_detach_hook(void *arg)
{
	struct bpe_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (ISSET(ifp->if_flags, IFF_RUNNING)) {
		bpe_down(sc);
		CLR(ifp->if_flags, IFF_UP);
	}

	sc->sc_key.k_if = 0;
}

static void
bpe_link_hook(void *arg)
{
	struct bpe_softc *sc = arg;
	struct ifnet *ifp0;
	u_char link = LINK_STATE_DOWN;
	uint64_t baud = 0;

	ifp0 = if_get(sc->sc_key.k_if);
	if (ifp0 != NULL) {
		link = ifp0->if_link_state;
		baud = ifp0->if_baudrate;
	}
	if_put(ifp0);

	bpe_link_state(sc, link, baud);
}

void
bpe_link_state(struct bpe_softc *sc, u_char link, uint64_t baud)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (ifp->if_link_state == link)
		return;

	ifp->if_link_state = link;
	ifp->if_baudrate = baud;

	if_link_state_change(ifp);
}

static inline int
bpe_cmp(const struct bpe_key *a, const struct bpe_key *b)
{
	if (a->k_if > b->k_if)
		return (1);
	if (a->k_if < b->k_if)
		return (-1);
	if (a->k_isid > b->k_isid)
		return (1);
	if (a->k_isid < b->k_isid)
		return (-1);

	return (0);
}

static inline int
bpe_entry_cmp(const struct bpe_entry *a, const struct bpe_entry *b)
{
	return memcmp(&a->be_c_da, &b->be_c_da, sizeof(a->be_c_da));
}
