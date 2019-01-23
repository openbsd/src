/*	$OpenBSD: if_mpw.c,v 1.25 2019/01/23 23:08:38 dlg Exp $ */

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

	struct ifaddr		sc_ifa;
	struct sockaddr_mpls	sc_smpls; /* Local label */

	uint32_t		sc_flags;
	uint32_t		sc_type;
	struct shim_hdr		sc_rshim;
	struct sockaddr_storage	sc_nexthop;
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

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d",
	    ifc->ifc_name, unit);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED;
	ifp->if_ioctl = mpw_ioctl;
	ifp->if_output = mpw_output;
	ifp->if_start = mpw_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ether_fakeaddr(ifp);

	if_attach(ifp);
	ether_ifattach(ifp);

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

	if (sc->sc_smpls.smpls_label) {
		rt_ifa_del(&sc->sc_ifa, RTF_MPLS,
		    smplstosa(&sc->sc_smpls));
	}

	ether_ifdetach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
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
			if (rt_ifa_del(&sc->sc_ifa, RTF_MPLS,
			    smplstosa(&sc->sc_smpls)) == 0)
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
		    htonl(imr.imr_lshim.shim_label << MPLS_LABEL_OFFSET);
		imr.imr_rshim.shim_label =
		    htonl(imr.imr_rshim.shim_label << MPLS_LABEL_OFFSET);

		if (sc->sc_smpls.smpls_label != imr.imr_lshim.shim_label) {
			if (sc->sc_smpls.smpls_label)
				rt_ifa_del(&sc->sc_ifa, RTF_MPLS,
				    smplstosa(&sc->sc_smpls));

			sc->sc_smpls.smpls_label = imr.imr_lshim.shim_label;
			error = rt_ifa_add(&sc->sc_ifa, RTF_MPLS|RTF_LOCAL,
			    smplstosa(&sc->sc_smpls));
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

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		goto drop;

	if (sc->sc_type == IMR_TYPE_NONE)
		goto drop;

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
			goto drop;
		}

		/* We don't support fragmentation just yet. */
		if (shim->shim_label & CW_FRAG_MASK) {
			ifp->if_ierrors++;
			goto drop;
		}
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
	struct ifnet *p;
	struct mbuf *m;
	struct shim_hdr *shim;
	struct sockaddr_storage ss;

	if (!ISSET(ifp->if_flags, IFF_RUNNING) ||
	    sc->sc_rshim.shim_label == 0 ||
	    sc->sc_type == IMR_TYPE_NONE) {
		IFQ_PURGE(&ifp->if_snd);
		return;
	}

	rt = rtalloc(sstosa(&sc->sc_nexthop), RT_RESOLVE, ifp->if_rdomain);
	if (!rtisvalid(rt)) {
		IFQ_PURGE(&ifp->if_snd);
		goto rtfree;
	}

	p = if_get(rt->rt_ifidx);
	if (p == NULL) {
		IFQ_PURGE(&ifp->if_snd);
		goto rtfree;
	}

	/*
	 * XXX: lie about being MPLS, so mpls_output() get the TTL from
	 * the right place.
	 */
	memcpy(&ss, &sc->sc_nexthop, sizeof(sc->sc_nexthop));
	ss.ss_family = AF_MPLS;

	while ((m = ifq_dequeue(&ifp->if_snd)) != NULL) {
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

		m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

		mpls_output(p, m, sstosa(&ss), rt);
	}

	if_put(p);
rtfree:
	rtfree(rt);
}
