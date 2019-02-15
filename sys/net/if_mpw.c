/*	$OpenBSD: if_mpw.c,v 1.38 2019/02/15 02:01:44 dlg Exp $ */

/*
 * Copyright (c) 2015 Rafael Zalamena <rzalamena@openbsd.org>
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
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netinet/if_ether.h>
#include <netmpls/mpls.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif /* NBPFILTER */

#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif

struct mpw_softc {
	struct arpcom		sc_ac;
#define sc_if			sc_ac.ac_if

	unsigned int		sc_rdomain;
	struct ifaddr		sc_ifa;
	struct sockaddr_mpls	sc_smpls; /* Local label */

	uint32_t		sc_flags;
	uint32_t		sc_type;
	struct shim_hdr		sc_rshim;
	struct sockaddr_storage	sc_nexthop;

	struct rwlock		sc_lock;
	unsigned int		sc_dead;
};

void	mpwattach(int);
int	mpw_clone_create(struct if_clone *, int);
int	mpw_clone_destroy(struct ifnet *);
int	mpw_ioctl(struct ifnet *, u_long, caddr_t);
int	mpw_output(struct ifnet *, struct mbuf *, struct sockaddr *,
    struct rtentry *);
void	mpw_start(struct ifnet *);
#if NVLAN > 0
struct	mbuf *mpw_vlan_handle(struct mbuf *, struct mpw_softc *);
#endif /* NVLAN */

struct if_clone mpw_cloner =
    IF_CLONE_INITIALIZER("mpw", mpw_clone_create, mpw_clone_destroy);

void
mpwattach(int n)
{
	if_clone_attach(&mpw_cloner);
}

int
mpw_clone_create(struct if_clone *ifc, int unit)
{
	struct mpw_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (sc == NULL)
		return (ENOMEM);

	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d",
	    ifc->ifc_name, unit);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED;
	ifp->if_ioctl = mpw_ioctl;
	ifp->if_output = mpw_output;
	ifp->if_start = mpw_start;
	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ether_fakeaddr(ifp);

	rw_init(&sc->sc_lock, ifp->if_xname);
	sc->sc_dead = 0;

	if_attach(ifp);
	ether_ifattach(ifp);

	sc->sc_rdomain = 0;
	sc->sc_ifa.ifa_ifp = ifp;
	sc->sc_ifa.ifa_addr = sdltosa(ifp->if_sadl);
	sc->sc_smpls.smpls_len = sizeof(sc->sc_smpls);
	sc->sc_smpls.smpls_family = AF_MPLS;

	return (0);
}

int
mpw_clone_destroy(struct ifnet *ifp)
{
	struct mpw_softc *sc = ifp->if_softc;

	ifp->if_flags &= ~IFF_RUNNING;

	rw_enter_write(&sc->sc_lock);
	sc->sc_dead = 1;

	if (sc->sc_smpls.smpls_label) {
		rt_ifa_del(&sc->sc_ifa, RTF_MPLS|RTF_LOCAL,
		    smplstosa(&sc->sc_smpls), sc->sc_rdomain);
	}
	rw_exit_write(&sc->sc_lock);

	ether_ifdetach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

int
mpw_set_label(struct mpw_softc *sc, uint32_t label, unsigned int rdomain)
{
	int error;

	rw_assert_wrlock(&sc->sc_lock);
	if (sc->sc_dead)
		return (ENXIO);

	if (sc->sc_smpls.smpls_label) {
		rt_ifa_del(&sc->sc_ifa, RTF_MPLS|RTF_LOCAL,
		    smplstosa(&sc->sc_smpls), sc->sc_rdomain);
	}

	sc->sc_smpls.smpls_label = label;
	sc->sc_rdomain = rdomain;

	error = rt_ifa_add(&sc->sc_ifa, RTF_MPLS|RTF_LOCAL,
	    smplstosa(&sc->sc_smpls), sc->sc_rdomain);
	if (error != 0)
		sc->sc_smpls.smpls_label = 0;

	return (error);
}

int
mpw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *) data;
	struct mpw_softc *sc = ifp->if_softc;
	struct sockaddr_in *sin;
	struct sockaddr_in *sin_nexthop;
	int error = 0;
	struct ifmpwreq imr;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP))
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;

	case SIOCGPWE3:
		ifr->ifr_pwe3 = IF_PWE3_ETHERNET;
		break;

	case SIOCSETMPWCFG:
		error = suser(curproc);
		if (error != 0)
			break;

		error = copyin(ifr->ifr_data, &imr, sizeof(imr));
		if (error != 0)
			break;

		/* Teardown all configuration if got no nexthop */
		sin = (struct sockaddr_in *) &imr.imr_nexthop;
		if (sin->sin_addr.s_addr == 0) {
			if (rt_ifa_del(&sc->sc_ifa, RTF_MPLS|RTF_LOCAL,
			    smplstosa(&sc->sc_smpls), 0) == 0)
				sc->sc_smpls.smpls_label = 0;

			memset(&sc->sc_rshim, 0, sizeof(sc->sc_rshim));
			memset(&sc->sc_nexthop, 0, sizeof(sc->sc_nexthop));
			sc->sc_flags = 0;
			sc->sc_type = 0;
			break;
		}

		/* Validate input */
		if (sin->sin_family != AF_INET ||
		    imr.imr_lshim.shim_label > MPLS_LABEL_MAX ||
		    imr.imr_lshim.shim_label <= MPLS_LABEL_RESERVED_MAX ||
		    imr.imr_rshim.shim_label > MPLS_LABEL_MAX ||
		    imr.imr_rshim.shim_label <= MPLS_LABEL_RESERVED_MAX) {
			error = EINVAL;
			break;
		}

		/* Setup labels and create inbound route */
		imr.imr_lshim.shim_label =
		    MPLS_LABEL2SHIM(imr.imr_lshim.shim_label);
		imr.imr_rshim.shim_label =
		    MPLS_LABEL2SHIM(imr.imr_rshim.shim_label);

		rw_enter_write(&sc->sc_lock);
		if (sc->sc_smpls.smpls_label != imr.imr_lshim.shim_label) {
			error = mpw_set_label(sc, imr.imr_lshim.shim_label,
			    sc->sc_rdomain);
		}
		rw_exit_write(&sc->sc_lock);
		if (error != 0)
			break;

		/* Apply configuration */
		sc->sc_flags = imr.imr_flags;
		sc->sc_type = imr.imr_type;
		sc->sc_rshim.shim_label = imr.imr_rshim.shim_label;
		sc->sc_rshim.shim_label |= MPLS_BOS_MASK;

		memset(&sc->sc_nexthop, 0, sizeof(sc->sc_nexthop));
		sin_nexthop = (struct sockaddr_in *) &sc->sc_nexthop;
		sin_nexthop->sin_family = sin->sin_family;
		sin_nexthop->sin_len = sizeof(struct sockaddr_in);
		sin_nexthop->sin_addr.s_addr = sin->sin_addr.s_addr;
		break;

	case SIOCGETMPWCFG:
		imr.imr_flags = sc->sc_flags;
		imr.imr_type = sc->sc_type;
		imr.imr_lshim.shim_label =
		    MPLS_SHIM2LABEL(sc->sc_smpls.smpls_label);
		imr.imr_rshim.shim_label =
		    MPLS_SHIM2LABEL(sc->sc_rshim.shim_label);
		memcpy(&imr.imr_nexthop, &sc->sc_nexthop,
		    sizeof(imr.imr_nexthop));

		error = copyout(&imr, ifr->ifr_data, sizeof(imr));
		break;

	case SIOCSLIFPHYRTABLE:
		if (ifr->ifr_rdomainid < 0 ||
		    ifr->ifr_rdomainid > RT_TABLEID_MAX ||
		    !rtable_exists(ifr->ifr_rdomainid) ||
		    ifr->ifr_rdomainid != rtable_l2(ifr->ifr_rdomainid)) {
			error = EINVAL;
			break;
		}
		rw_enter_write(&sc->sc_lock);
		if (sc->sc_rdomain != ifr->ifr_rdomainid) {
			error = mpw_set_label(sc, sc->sc_smpls.smpls_label,
			    ifr->ifr_rdomainid);
		}
		rw_exit_write(&sc->sc_lock);
		break;
	case SIOCGLIFPHYRTABLE:
		ifr->ifr_rdomainid = sc->sc_rdomain;
		break;


	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	return (error);
}

static void
mpw_input(struct mpw_softc *sc, struct mbuf *m)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct ifnet *ifp = &sc->sc_if;
	struct shim_hdr *shim;
	struct mbuf *n;
	int off;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		goto drop;

	if (sc->sc_type == IMR_TYPE_NONE)
		goto drop;

	shim = mtod(m, struct shim_hdr *);
	if (!MPLS_BOS_ISSET(shim->shim_label)) {
		/* don't have RFC 6391: Flow-Aware Transport of Pseudowires */
		goto drop;
	}
	m_adj(m, sizeof(*shim));

	if (sc->sc_flags & IMR_FLAG_CONTROLWORD) {
		if (m->m_len < sizeof(*shim)) {
			m = m_pullup(m, sizeof(*shim));
			if (m == NULL)
				return;
		}
		shim = mtod(m, struct shim_hdr *);

		/*
		 * The first 4 bits identifies that this packet is a
		 * control word. If the control word is configured and
		 * we received an IP datagram we shall drop it.
		 */
		if (shim->shim_label & CW_ZERO_MASK) {
			ifp->if_ierrors++;
			goto drop;
		}

		/* We don't support fragmentation just yet. */
		if (shim->shim_label & CW_FRAG_MASK) {
			ifp->if_ierrors++;
			goto drop;
		}

		m_adj(m, MPLS_HDRLEN);
	}

	if (m->m_len < sizeof(struct ether_header)) {
		m = m_pullup(m, sizeof(struct ether_header));
		if (m == NULL)
			return;
	}

	n = m_getptr(m, sizeof(struct ether_header), &off);
	if (n == NULL) {
		ifp->if_ierrors++;
		goto drop;
	}
	if (!ALIGNED_POINTER(mtod(n, caddr_t) + off, uint32_t)) {
		n = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
		/* Dispose of the original mbuf chain */
		m_freem(m);
		if (n == NULL)
			return;
		m = n;
	}

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);
	return;
drop:
	m_freem(m);
}

int
mpw_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	struct mpw_softc *sc = ifp->if_softc;

	if (dst->sa_family == AF_LINK &&
	    rt != NULL && ISSET(rt->rt_flags, RTF_LOCAL)) {
		mpw_input(sc, m);
		return (0);
	}

	return (ether_output(ifp, m, dst, rt));
}

void
mpw_start(struct ifnet *ifp)
{
	struct mpw_softc *sc = ifp->if_softc;
	struct rtentry *rt;
	struct ifnet *ifp0;
	struct mbuf *m, *m0;
	struct shim_hdr *shim;
	struct sockaddr_mpls smpls = {
		.smpls_len = sizeof(smpls),
		.smpls_family = AF_MPLS,
	};

	if (!ISSET(ifp->if_flags, IFF_RUNNING) ||
	    sc->sc_rshim.shim_label == 0 ||
	    sc->sc_type == IMR_TYPE_NONE) {
		IFQ_PURGE(&ifp->if_snd);
		return;
	}

	rt = rtalloc(sstosa(&sc->sc_nexthop), RT_RESOLVE, sc->sc_rdomain);
	if (!rtisvalid(rt)) {
		IFQ_PURGE(&ifp->if_snd);
		goto rtfree;
	}

	ifp0 = if_get(rt->rt_ifidx);
	if (ifp0 == NULL) {
		IFQ_PURGE(&ifp->if_snd);
		goto rtfree;
	}

	while ((m = ifq_dequeue(&ifp->if_snd)) != NULL) {
#if NBPFILTER > 0
		if (sc->sc_if.if_bpf)
			bpf_mtap(sc->sc_if.if_bpf, m, BPF_DIRECTION_OUT);
#endif /* NBPFILTER */

		m0 = m_get(M_DONTWAIT, m->m_type);
		if (m0 == NULL) {
			m_freem(m);
			continue;
		}

		M_MOVE_PKTHDR(m0, m);
		m0->m_next = m;
		m_align(m0, 0);
		m0->m_len = 0;

		if (sc->sc_flags & IMR_FLAG_CONTROLWORD) {
			m0 = m_prepend(m0, sizeof(*shim), M_NOWAIT);
			if (m0 == NULL)
				continue;

			shim = mtod(m0, struct shim_hdr *);
			memset(shim, 0, sizeof(*shim));
		}

		m0 = m_prepend(m0, sizeof(*shim), M_NOWAIT);
		if (m0 == NULL)
			continue;

		shim = mtod(m0, struct shim_hdr *);
		shim->shim_label = htonl(mpls_defttl) & MPLS_TTL_MASK;
		shim->shim_label |= sc->sc_rshim.shim_label;

		m0->m_pkthdr.ph_rtableid = ifp->if_rdomain;

		mpls_output(ifp0, m0, (struct sockaddr *)&smpls, rt);
	}

	if_put(ifp0);
rtfree:
	rtfree(rt);
}
