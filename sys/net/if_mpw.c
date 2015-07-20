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
	struct		ifnet sc_if;

	struct		ifaddr sc_ifa;
	struct		sockaddr_mpls sc_smpls; /* Local label */

	uint32_t	sc_flags;
	uint32_t	sc_type;
	struct		shim_hdr sc_rshim;
	struct		sockaddr_storage sc_nexthop;
};

void	mpwattach(int);
int	mpw_clone_create(struct if_clone *, int);
int	mpw_clone_destroy(struct ifnet *);
int	mpw_ioctl(struct ifnet *, u_long, caddr_t);
int	mpw_output(struct ifnet *, struct mbuf *, struct sockaddr *,
    struct rtentry *);
void	mpw_start(struct ifnet *);
int	mpw_input(struct ifnet *, struct mbuf *);
#if NVLAN > 0
struct	mbuf *mpw_vlan_handle(struct mbuf *, struct mpw_softc *);
#endif /* NVLAN */

struct if_clone mpw_cloner =
    IF_CLONE_INITIALIZER("mpw", mpw_clone_create, mpw_clone_destroy);

/* ARGSUSED */
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
	struct ifih *ifih;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc == NULL)
		return (ENOMEM);

	ifih = malloc(sizeof(*ifih), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ifih == NULL) {
		free(sc, M_DEVBUF, sizeof(*sc));
		return (ENOMEM);
	}

	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "mpw%d", unit);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_ioctl = mpw_ioctl;
	ifp->if_output = mpw_output;
	ifp->if_start = mpw_start;
	ifp->if_type = IFT_MPLSTUNNEL;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	if_alloc_sadl(ifp);

	sc->sc_ifa.ifa_ifp = ifp;
	sc->sc_ifa.ifa_rtrequest = link_rtrequest;
	sc->sc_ifa.ifa_addr = (struct sockaddr *) ifp->if_sadl;
	sc->sc_smpls.smpls_len = sizeof(sc->sc_smpls);
	sc->sc_smpls.smpls_family = AF_MPLS;

	ifih->ifih_input = mpw_input;
	SLIST_INSERT_HEAD(&ifp->if_inputs, ifih, ifih_next);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, ETHER_HDR_LEN);
#endif /* NBFILTER */

	return (0);
}

int
mpw_clone_destroy(struct ifnet *ifp)
{
	struct mpw_softc *sc = ifp->if_softc;
	struct ifih *ifih = SLIST_FIRST(&ifp->if_inputs);
	int s;

	ifp->if_flags &= ~IFF_RUNNING;

	if (sc->sc_smpls.smpls_label) {
		s = splsoftnet();
		rt_ifa_del(&sc->sc_ifa, RTF_MPLS | RTF_UP,
		    smplstosa(&sc->sc_smpls));
		splx(s);
	}

	SLIST_REMOVE(&ifp->if_inputs, ifih, ifih, ifih_next);
	free(ifih, M_DEVBUF, sizeof(*ifih));

	if_detach(ifp);
	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

int
mpw_input(struct ifnet *ifp, struct mbuf *m)
{
	/* Don't have local broadcast. */
	m_freem(m);
	return (1);
}

int
mpw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *) data;
	struct mpw_softc *sc = ifp->if_softc;
	struct sockaddr_in *sin;
	struct sockaddr_in *sin_nexthop;
	int error = 0;
	int s;
	struct ifmpwreq imr;

	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < MPE_MTU_MIN ||
		    ifr->ifr_mtu > MPE_MTU_MAX)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP))
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;

	case SIOCSETMPWCFG:
		error = suser(curproc, 0);
		if (error != 0)
			break;

		error = copyin(ifr->ifr_data, &imr, sizeof(imr));
		if (error != 0)
			break;

		/* Teardown all configuration if got no nexthop */
		sin = (struct sockaddr_in *) &imr.imr_nexthop;
		if (sin->sin_addr.s_addr == 0) {
			s = splsoftnet();
			if (rt_ifa_del(&sc->sc_ifa, RTF_MPLS | RTF_UP,
			    smplstosa(&sc->sc_smpls)) == 0)
				sc->sc_smpls.smpls_label = 0;
			splx(s);

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
		    htonl(imr.imr_lshim.shim_label << MPLS_LABEL_OFFSET);
		imr.imr_rshim.shim_label =
		    htonl(imr.imr_rshim.shim_label << MPLS_LABEL_OFFSET);

		if (sc->sc_smpls.smpls_label != imr.imr_lshim.shim_label) {
			s = splsoftnet();
			if (sc->sc_smpls.smpls_label)
				rt_ifa_del(&sc->sc_ifa, RTF_MPLS | RTF_UP,
				    smplstosa(&sc->sc_smpls));

			sc->sc_smpls.smpls_label = imr.imr_lshim.shim_label;
			error = rt_ifa_add(&sc->sc_ifa, RTF_MPLS | RTF_UP,
			    smplstosa(&sc->sc_smpls));
			splx(s);
			if (error != 0) {
				sc->sc_smpls.smpls_label = 0;
				break;
			}
		}

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
		    ((ntohl(sc->sc_smpls.smpls_label & MPLS_LABEL_MASK)) >>
			MPLS_LABEL_OFFSET);
		imr.imr_rshim.shim_label =
		    ((ntohl(sc->sc_rshim.shim_label & MPLS_LABEL_MASK)) >>
			MPLS_LABEL_OFFSET);
		memcpy(&imr.imr_nexthop, &sc->sc_nexthop,
		    sizeof(imr.imr_nexthop));

		error = copyout(&imr, ifr->ifr_data, sizeof(imr));
		break;

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

int
mpw_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	struct mpw_softc *sc = ifp->if_softc;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct ether_header *eh, ehc;
	struct shim_hdr *shim;
	int s;

	if (sc->sc_type == IMR_TYPE_NONE) {
		m_freem(m);
		return (EHOSTUNREACH);
	}

	if (sc->sc_flags & IMR_FLAG_CONTROLWORD) {
		shim = mtod(m, struct shim_hdr *);
		m_adj(m, MPLS_HDRLEN);

		/*
		 * The first 4 bits identifies that this packet is a
		 * control word. If the control word is configured and
		 * we received an IP datagram we shall drop it.
		 */
		if (shim->shim_label & CW_ZERO_MASK) {
			ifp->if_ierrors++;
			m_freem(m);
			return (EINVAL);
		}

		/* We don't support fragmentation just yet. */
		if (shim->shim_label & CW_FRAG_MASK) {
			ifp->if_ierrors++;
			m_freem(m);
			return (EINVAL);
		}
	}

	if (sc->sc_type == IMR_TYPE_ETHERNET_TAGGED) {
		m_copydata(m, 0, sizeof(ehc), (caddr_t) &ehc);
		m_adj(m, ETHER_HDR_LEN);

		/* Ethernet tagged expects at least 2 VLANs */
		if (ntohs(ehc.ether_type) != ETHERTYPE_QINQ) {
			ifp->if_ierrors++;
			m_freem(m);
			return (EINVAL);
		}

		/* Remove dummy VLAN and update ethertype */
		if (EVL_VLANOFTAG(*mtod(m, uint16_t *)) == 0) {
			m_adj(m, EVL_ENCAPLEN);
			ehc.ether_type = htons(ETHERTYPE_VLAN);
		}

		M_PREPEND(m, sizeof(*eh), M_NOWAIT);
		if (m == NULL)
			return (ENOMEM);

		eh = mtod(m, struct ether_header *);
		memcpy(eh, &ehc, sizeof(*eh));
	}

	ml_enqueue(&ml, m);

	s = splnet();
	if_input(ifp, &ml);
	splx(s);

	return (0);
}

#if NVLAN > 0
extern void vlan_start(struct ifnet *ifp);

/*
 * This routine handles VLAN tag reinsertion in packets flowing through
 * the pseudowire. Also it does the necessary modifications to the VLANs
 * to respect the RFC.
 */
struct mbuf *
mpw_vlan_handle(struct mbuf *m, struct mpw_softc *sc)
{
	int needsdummy = 0;
	int fakeifv = 0;
	struct ifvlan *ifv = NULL;
	struct ether_vlan_header *evh;
	struct ifnet *ifp, *ifp0;
	int nvlan, moff;
	struct ether_header eh;
	struct ifvlan fifv;
	struct vlan_shim {
		uint16_t	vs_tpid;
		uint16_t	vs_tci;
	} vs;

	ifp0 = ifp = if_get(m->m_pkthdr.ph_ifidx);
	KASSERT(ifp != NULL);
	if (ifp->if_start == vlan_start)
		ifv = ifp->if_softc;

	/* If we were relying on VLAN HW support, fake an ifv */
	if (ifv == NULL && (m->m_flags & M_VLANTAG) == M_VLANTAG) {
		memset(&fifv, 0, sizeof(fifv));
		fifv.ifv_tag = EVL_VLANOFTAG(m->m_pkthdr.ether_vtag);
		fifv.ifv_prio = EVL_PRIOFTAG(m->m_pkthdr.ether_vtag);
		ifv = &fifv;
		fakeifv = 1;
	}

	/*
	 * Always remove VLAN flag as we are inserting them here. Also we
	 * might get a tagged packet with no VLAN interface, in this case
	 * we can't do anything.
	 */
	m->m_flags &= ~M_VLANTAG;

	/*
	 * Do VLAN managing.
	 *
	 * Case ethernet (raw):
	 *  No VLAN: just pass it.
	 *  One or more VLANs: insert VLAN tag back.
	 *
	 * NOTE: In case of raw access mode, the if_vlan will do the job
	 * of dropping non tagged packets for us.
	 */
	if (sc->sc_type == IMR_TYPE_ETHERNET && ifv == NULL)
		return (m);

	/*
	 * Case ethernet-tagged:
	 *  0 VLAN: Drop packet
	 *  1 VLAN: Tag packet with dummy VLAN
	 *  >1 VLAN: Nothing
	 */
	if (sc->sc_type == IMR_TYPE_ETHERNET_TAGGED && ifv == NULL) {
		m_freem(m);
		return (NULL);
	}

	/* Copy and remove ethernet header */
	m_copydata(m, 0, sizeof(eh), (caddr_t) &eh);
	if (ntohs(eh.ether_type) == ETHERTYPE_VLAN ||
	    ntohs(eh.ether_type) == ETHERTYPE_QINQ)
		m_adj(m, sizeof(*evh));
	else
		m_adj(m, sizeof(eh));

	/* Count VLAN stack size */
	nvlan = 0;
	while ((ifp = ifv->ifv_p) != NULL && ifp->if_start == vlan_start) {
		ifv = ifp->if_softc;
		nvlan++;
	}
	moff = sizeof(*evh) + (nvlan * EVL_ENCAPLEN);

	/* The mode ethernet tagged always need at least 2 VLANs */
	if (sc->sc_type == IMR_TYPE_ETHERNET_TAGGED && nvlan == 0) {
		needsdummy = 1;
		moff += EVL_ENCAPLEN;
	}

	/* Add VLAN to the beginning of the packet */
	M_PREPEND(m, moff, M_NOWAIT);
	if (m == NULL)
		return (NULL);

	/* Copy original ethernet type */
	moff -= sizeof(eh.ether_type);
	m_copyback(m, moff, sizeof(eh.ether_type), &eh.ether_type, M_NOWAIT);

	/* Fill inner VLAN values */
	ifv = ifp0->if_softc;
	while (nvlan-- > 0) {
		vs.vs_tci = htons((ifv->ifv_prio << EVL_PRIO_BITS) +
		    ifv->ifv_tag);
		vs.vs_tpid = htons(ifv->ifv_type);

		moff -= sizeof(vs);
		m_copyback(m, moff, sizeof(vs), &vs, M_NOWAIT);

		ifp = ifv->ifv_p;
		ifv = ifp->if_softc;
	}

	/* Copy ethernet header back */
	evh = mtod(m, struct ether_vlan_header *);
	memcpy(evh->evl_dhost, eh.ether_dhost, sizeof(evh->evl_dhost));
	memcpy(evh->evl_shost, eh.ether_shost, sizeof(evh->evl_shost));

	if (fakeifv)
		ifv = &fifv;

	/* Insert the last VLAN and optionally a dummy VLAN */
	if (needsdummy) {
		evh->evl_encap_proto = ntohs(ETHERTYPE_QINQ);
		evh->evl_tag = 0;

		vs.vs_tci = ntohs((m->m_pkthdr.pf.prio << EVL_PRIO_BITS) +
		    ifv->ifv_tag);
		vs.vs_tpid = ntohs(ETHERTYPE_VLAN);
		m_copyback(m, moff, sizeof(vs), &vs, M_NOWAIT);
	} else {
		evh->evl_encap_proto = (nvlan > 0) ?
		    ntohs(ETHERTYPE_QINQ) : ntohs(ETHERTYPE_VLAN);
		evh->evl_tag = ntohs((m->m_pkthdr.pf.prio << EVL_PRIO_BITS) +
		    ifv->ifv_tag);
	}

	return (m);
}
#endif /* NVLAN */

void
mpw_start(struct ifnet *ifp)
{
	struct mpw_softc *sc = ifp->if_softc;
	struct mbuf *m;
	struct rtentry *rt;
	struct shim_hdr *shim;
	struct sockaddr_storage ss;

	rt = rtalloc((struct sockaddr *) &sc->sc_nexthop,
	    RT_REPORT | RT_RESOLVE, 0);
	if (rt == NULL)
		return;

	/*
	 * XXX: lie about being MPLS, so mpls_output() get the TTL from
	 * the right place.
	 */
	memcpy(&ss, &sc->sc_nexthop, sizeof(sc->sc_nexthop));
	((struct sockaddr *) &ss)->sa_family = AF_MPLS;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if ((ifp->if_flags & IFF_RUNNING) == 0 ||
		    sc->sc_rshim.shim_label == 0 ||
		    sc->sc_type == IMR_TYPE_NONE) {
			m_freem(m);
			continue;
		}

#if NVLAN > 0
		m = mpw_vlan_handle(m, sc);
		if (m == NULL)
			continue;
#else
		/* Ethernet tagged doesn't work without VLANs'*/
		if (sc->sc_type == IMR_TYPE_ETHERNET_TAGGED) {
			m_freem(m);
			continue;
		}
#endif /* NVLAN */

#if NBPFILTER > 0
		if (sc->sc_if.if_bpf)
			bpf_mtap(sc->sc_if.if_bpf, m, BPF_DIRECTION_OUT);
#endif /* NBPFILTER */

		if (sc->sc_flags & IMR_FLAG_CONTROLWORD) {
			M_PREPEND(m, sizeof(*shim), M_NOWAIT);
			if (m == NULL)
				continue;

			shim = mtod(m, struct shim_hdr *);
			memset(shim, 0, sizeof(*shim));
		}

		M_PREPEND(m, sizeof(*shim), M_NOWAIT);
		if (m == NULL)
			continue;

		shim = mtod(m, struct shim_hdr *);
		shim->shim_label = htonl(mpls_defttl) & MPLS_TTL_MASK;
		shim->shim_label |= sc->sc_rshim.shim_label;

		/* XXX: MPLS only uses domain 0 */
		m->m_pkthdr.ph_rtableid = 0;

		mpls_output(rt->rt_ifp, m, (struct sockaddr *) &ss, rt);
	}

	rtfree(rt);
}
