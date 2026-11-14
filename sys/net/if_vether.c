/* $OpenBSD: if_vether.c,v 1.40 2026/11/14 23:41:31 dlg Exp $ */

/*
 * Copyright (c) 2009 Theo de Raadt
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "vlan.h"
#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

void	vetherattach(int);
int	vetherioctl(struct ifnet *, u_long, caddr_t);
void	vetherqstart(struct ifqueue *);
int	vether_clone_create(struct if_clone *, int);
int	vether_clone_destroy(struct ifnet *);

struct vether_softc {
	struct arpcom		sc_ac;
};

struct if_clone	vether_cloner =
    IF_CLONE_INITIALIZER("vether", vether_clone_create, vether_clone_destroy);

void
vetherattach(int nvether)
{
	if_clone_attach(&vether_cloner);
}

int
vether_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet		*ifp;
	struct vether_softc	*sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = &sc->sc_ac.ac_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "vether%d", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ether_fakeaddr(ifp);

	ifp->if_softc = sc;
	ifp->if_ioctl = vetherioctl;
	ifp->if_qstart = vetherqstart;

	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif
	ifp->if_capabilities |= IFCAP_CSUM_IPv4;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
	ifp->if_capabilities |= IFCAP_CSUM_TCPv6 | IFCAP_CSUM_UDPv6;
	ifp->if_xflags = IFXF_CLONED | IFXF_MPSAFE;

	if_attach(ifp);
	ether_ifattach(ifp);
	return (0);
}

int
vether_clone_destroy(struct ifnet *ifp)
{
	struct vether_softc	*sc = ifp->if_softc;

	ether_ifdetach(ifp);
	if_detach(ifp);
	free(sc, M_DEVBUF, sizeof(*sc));
	return (0);
}

/*
 * The bridge has magically already done all the work for us,
 * and we only need to discard the packets.
 */
void
vetherqstart(struct ifqueue *ifq)
{
	struct mbuf		*m;

	while ((m = ifq_dequeue(ifq)) != NULL) {
#if NBPFILTER > 0
		struct ifnet	*ifp = ifq->ifq_if;

		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif /* NBPFILTER > 0 */

		m_freem(m);
	}
}

int
vetherioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vether_softc	*sc = (struct vether_softc *)ifp->if_softc;
	int			 error = 0, link_state;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			ifp->if_flags |= IFF_RUNNING;
			link_state = LINK_STATE_UP;
		} else {
			ifp->if_flags &= ~IFF_RUNNING;
			link_state = LINK_STATE_DOWN;
		}
		if (ifp->if_link_state != link_state) {
			ifp->if_link_state = link_state;
			if_link_state_change(ifp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}
	return (error);
}
