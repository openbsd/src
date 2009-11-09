/* $OpenBSD: if_vether.c,v 1.1 2009/11/09 03:16:05 deraadt Exp $ */

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
#include "vether.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_var.h>
#endif

#include <dev/rndvar.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef MPLS_DEBUG
#define DPRINTF(x)    do { if (vetherdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

void	vetherattach(int);
int	vetherioctl(struct ifnet *, u_long, caddr_t);
void	vetherstart(struct ifnet *);
int	vether_clone_create(struct if_clone *, int);
int	vether_clone_destroy(struct ifnet *);
int	vether_media_change(struct ifnet *);
void	vether_media_status(struct ifnet *, struct ifmediareq *);

struct vether_softc {
	struct arpcom		sc_ac;
	struct ifmedia		sc_media;
	LIST_ENTRY(vether_softc) sc_list;
};

LIST_HEAD(, vether_softc)	vether_list;
struct if_clone	vether_cloner =
    IF_CLONE_INITIALIZER("vether", vether_clone_create, vether_clone_destroy);


int
vether_media_change(struct ifnet *ifp)
{
	return (0);
}

void
vether_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID | IFM_ACTIVE;
}

void
vetherattach(int nvether)
{
	LIST_INIT(&vether_list);
	if_clone_attach(&vether_cloner);
}

int
vether_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet 		*ifp;
	struct vether_softc	*sc;
	u_int32_t		 macaddr_rnd;
	int 			 s;

	if ((sc = malloc(sizeof(*sc),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	/* from if_tun.c: generate fake MAC address: 00 bd xx xx xx unit_no */
	sc->sc_ac.ac_enaddr[0] = 0x00;
	sc->sc_ac.ac_enaddr[1] = 0xbd;
	/*
	 * This no longer happens pre-scheduler so let's use the real
	 * random subsystem instead of random().
	 */
	macaddr_rnd = arc4random();
	bcopy(&macaddr_rnd, &sc->sc_ac.ac_enaddr[2], sizeof(u_int32_t));
	sc->sc_ac.ac_enaddr[5] = (u_char)unit + 1;

	ifp = &sc->sc_ac.ac_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "vether%d", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_softc = sc;
	ifp->if_ioctl = vetherioctl;
	ifp->if_start = vetherstart;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);

	ifmedia_init(&sc->sc_media, 0, vether_media_change,
	    vether_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	s = splnet();
	LIST_INSERT_HEAD(&vether_list, sc, sc_list);
	splx(s);
	return (0);
}

int
vether_clone_destroy(struct ifnet *ifp)
{
	struct vether_softc	*sc = ifp->if_softc;
	int			 s;

	s = splnet();
	LIST_REMOVE(sc, sc_list);
	splx(s);

	ether_ifdetach(ifp);
	if_detach(ifp);
	free(sc, M_DEVBUF);
	return (0);
}

/*
 * Start output on the vether interface.
 */
void
vetherstart(struct ifnet *ifp)
{
	struct mbuf 		*m;
	int			 s, inout;

	for (;;) {
		s = splnet();
		IFQ_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;

		inout = (m->m_flags & M_PROTO1) ?
		    BPF_DIRECTION_IN : BPF_DIRECTION_OUT;
		m->m_flags &= ~M_PROTO1;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, inout);
#endif

		if (inout == BPF_DIRECTION_IN)
			ether_input_mbuf(ifp, m);
		else
			m_freem(m);
	}
}

/* ARGSUSED */
int
vetherioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vether_softc	*sc = (struct vether_softc *)ifp->if_softc;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
	int			 error = 0, link_state;

	ifr = (struct ifreq *)data;
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif
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

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}
	return (error);
}
