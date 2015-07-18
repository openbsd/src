/*	$OpenBSD: if_vxlan.c,v 1.26 2015/07/18 22:15:14 goda Exp $	*/

/*
 * Copyright (c) 2013 Reyk Floeter <reyk@openbsd.org>
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
#include "vxlan.h"
#include "vlan.h"
#include "pf.h"
#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/route.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#include <net/if_vxlan.h>

void	 vxlanattach(int);
int	 vxlanioctl(struct ifnet *, u_long, caddr_t);
void	 vxlanstart(struct ifnet *);
int	 vxlan_clone_create(struct if_clone *, int);
int	 vxlan_clone_destroy(struct ifnet *);
void	 vxlan_multicast_cleanup(struct ifnet *);
int	 vxlan_multicast_join(struct ifnet *, struct sockaddr_in *,
	    struct sockaddr_in *);
int	 vxlan_media_change(struct ifnet *);
void	 vxlan_media_status(struct ifnet *, struct ifmediareq *);
int	 vxlan_config(struct ifnet *, struct sockaddr *, struct sockaddr *);
int	 vxlan_output(struct ifnet *, struct mbuf *);
void	 vxlan_addr_change(void *);
void	 vxlan_if_change(void *);
void	 vxlan_link_change(void *);

struct if_clone	vxlan_cloner =
    IF_CLONE_INITIALIZER("vxlan", vxlan_clone_create, vxlan_clone_destroy);

int	 vxlan_enable = 0;
u_long	 vxlan_tagmask;

#define VXLAN_TAGHASHSIZE		 32
#define VXLAN_TAGHASH(tag)		 (tag & vxlan_tagmask)
LIST_HEAD(vxlan_taghash, vxlan_softc)	*vxlan_tagh;

void
vxlanattach(int count)
{
	if ((vxlan_tagh = hashinit(VXLAN_TAGHASHSIZE, M_DEVBUF, M_NOWAIT,
	    &vxlan_tagmask)) == NULL)
		panic("vxlanattach: hashinit");

	if_clone_attach(&vxlan_cloner);
}

int
vxlan_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet		*ifp;
	struct vxlan_softc	*sc;

	if ((sc = malloc(sizeof(*sc),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	sc->sc_imo.imo_membership = malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK|M_ZERO);
	sc->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;
	sc->sc_dstport = htons(VXLAN_PORT);
	sc->sc_vnetid = 0;

	ifp = &sc->sc_ac.ac_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "vxlan%d", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ether_fakeaddr(ifp);

	ifp->if_softc = sc;
	ifp->if_ioctl = vxlanioctl;
	ifp->if_start = vxlanstart;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_hardmtu = 0xffff;
	ifp->if_capabilities = IFCAP_VLAN_MTU;

	ifmedia_init(&sc->sc_media, 0, vxlan_media_change,
	    vxlan_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

#if 0
	/*
	 * Instead of using a decreased MTU of 1450 bytes, prefer
	 * to use the default Ethernet-size MTU of 1500 bytes and to
	 * increase the MTU of the outer transport interfaces to
	 * at least 1550 bytes. The following is disabled by default.
	 */
	ifp->if_mtu = ETHERMTU - sizeof(struct ether_header);
	ifp->if_mtu -= sizeof(struct vxlanudpiphdr);
#endif

	LIST_INSERT_HEAD(&vxlan_tagh[VXLAN_TAGHASH(0)], sc, sc_entry);
	vxlan_enable++;

	return (0);
}

int
vxlan_clone_destroy(struct ifnet *ifp)
{
	struct vxlan_softc	*sc = ifp->if_softc;
	int			 s;

	s = splnet();
	vxlan_multicast_cleanup(ifp);
	splx(s);

	vxlan_enable--;
	LIST_REMOVE(sc, sc_entry);

	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);
	free(sc->sc_imo.imo_membership, M_IPMOPTS, 0);
	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

void
vxlan_multicast_cleanup(struct ifnet *ifp)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct ip_moptions	*imo = &sc->sc_imo;
	struct ifnet		*mifp;

	mifp = if_get(imo->imo_ifidx);
	if (mifp != NULL) {
		if (sc->sc_ahcookie != NULL) {
			hook_disestablish(mifp->if_addrhooks, sc->sc_ahcookie);
			sc->sc_ahcookie = NULL;
		}
		if (sc->sc_lhcookie != NULL) {
			hook_disestablish(mifp->if_linkstatehooks,
			    sc->sc_lhcookie);
			sc->sc_lhcookie = NULL;
		}
		if (sc->sc_dhcookie != NULL) {
			hook_disestablish(mifp->if_detachhooks,
			    sc->sc_dhcookie);
			sc->sc_dhcookie = NULL;
		}
	}

	if (imo->imo_num_memberships > 0) {
		in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
		imo->imo_ifidx = 0;
	}
}

int
vxlan_multicast_join(struct ifnet *ifp, struct sockaddr_in *src,
    struct sockaddr_in *dst)
{
	struct vxlan_softc	*sc = ifp->if_softc;
	struct ip_moptions	*imo = &sc->sc_imo;
	struct ifaddr		*ifa;
	struct ifnet		*mifp;

	if (!IN_MULTICAST(dst->sin_addr.s_addr))
		return (0);

	if (src->sin_addr.s_addr == INADDR_ANY ||
	    IN_MULTICAST(src->sin_addr.s_addr))
		return (EINVAL);
	if ((ifa = ifa_ifwithaddr(sintosa(src), sc->sc_rdomain)) == NULL ||
	    (mifp = ifa->ifa_ifp) == NULL ||
	    (mifp->if_flags & IFF_MULTICAST) == 0)
		return (EADDRNOTAVAIL);

	if ((imo->imo_membership[0] =
	    in_addmulti(&dst->sin_addr, mifp)) == NULL)
		return (ENOBUFS);

	imo->imo_num_memberships++;
	imo->imo_ifidx = mifp->if_index;
	if (sc->sc_ttl > 0)
		imo->imo_ttl = sc->sc_ttl;
	else
		imo->imo_ttl = IP_DEFAULT_MULTICAST_TTL;
	imo->imo_loop = 0;

	/*
	 * Use interface hooks to track any changes on the interface
	 * that is used to send out the tunnel traffic as multicast.
	 */
	if ((sc->sc_ahcookie = hook_establish(mifp->if_addrhooks,
	    0, vxlan_addr_change, sc)) == NULL ||
	    (sc->sc_lhcookie = hook_establish(mifp->if_linkstatehooks,
	    0, vxlan_link_change, sc)) == NULL ||
	    (sc->sc_dhcookie = hook_establish(mifp->if_detachhooks,
	    0, vxlan_if_change, sc)) == NULL)
		panic("%s: cannot allocate interface hook",
		    mifp->if_xname);

	return (0);
}

void
vxlanstart(struct ifnet *ifp)
{
	struct mbuf		*m;
	int			 s;

	for (;;) {
		s = splnet();
		IFQ_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;
		ifp->if_opackets++;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		vxlan_output(ifp, m);
	}
}

int
vxlan_config(struct ifnet *ifp, struct sockaddr *src, struct sockaddr *dst)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct sockaddr_in	*src4, *dst4;
	int			 reset = 0, error;

	if (src != NULL && dst != NULL) {
		/* XXX inet6 is not supported */
		if (src->sa_family != AF_INET || dst->sa_family != AF_INET)
			return (EAFNOSUPPORT);
	} else {
		/* Reset current configuration */
		src = (struct sockaddr *)&sc->sc_src;
		dst = (struct sockaddr *)&sc->sc_dst;
		reset = 1;
	}

	src4 = satosin(src);
	dst4 = satosin(dst);

	if (src4->sin_len != sizeof(*src4) || dst4->sin_len != sizeof(*dst4))
		return (EINVAL);

	vxlan_multicast_cleanup(ifp);

	if (IN_MULTICAST(dst4->sin_addr.s_addr)) {
		if ((error = vxlan_multicast_join(ifp, src4, dst4)) != 0)
			return (error);
	}
	if (dst4->sin_port)
		sc->sc_dstport = dst4->sin_port;

	if (!reset) {
		bzero(&sc->sc_src, sizeof(sc->sc_src));
		bzero(&sc->sc_dst, sizeof(sc->sc_dst));
		memcpy(&sc->sc_src, src, src->sa_len);
		memcpy(&sc->sc_dst, dst, dst->sa_len);
	}

	LIST_REMOVE(sc, sc_entry);
	LIST_INSERT_HEAD(&vxlan_tagh[VXLAN_TAGHASH(sc->sc_vnetid)],
	    sc, sc_entry);

	return (0);
}

/* ARGSUSED */
int
vxlanioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct if_laddrreq	*lifr = (struct if_laddrreq *)data;
	struct proc		*p = curproc;
	int			 error = 0, s;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			ifp->if_flags |= IFF_RUNNING;
		} else {
			ifp->if_flags &= ~IFF_RUNNING;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCSLIFPHYADDR:
		if ((error = suser(p, 0)) != 0)
			break;
		s = splnet();
		error = vxlan_config(ifp,
		    (struct sockaddr *)&lifr->addr,
		    (struct sockaddr *)&lifr->dstaddr);
		splx(s);
		break;

	case SIOCDIFPHYADDR:
		if ((error = suser(p, 0)) != 0)
			break;
		s = splnet();
		vxlan_multicast_cleanup(ifp);
		bzero(&sc->sc_src, sizeof(sc->sc_src));
		bzero(&sc->sc_dst, sizeof(sc->sc_dst));
		sc->sc_dstport = htons(VXLAN_PORT);
		splx(s);
		break;

	case SIOCGLIFPHYADDR:
		if (sc->sc_dst.ss_family == AF_UNSPEC) {
			error = EADDRNOTAVAIL;
			break;
		}
		bzero(&lifr->addr, sizeof(lifr->addr));
		bzero(&lifr->dstaddr, sizeof(lifr->dstaddr));
		memcpy(&lifr->addr, &sc->sc_src, sc->sc_src.ss_len);
		memcpy(&lifr->dstaddr, &sc->sc_dst, sc->sc_dst.ss_len);
		break;

	case SIOCSLIFPHYRTABLE:
		if ((error = suser(p, 0)) != 0)
			break;
		if (ifr->ifr_rdomainid < 0 ||
		    ifr->ifr_rdomainid > RT_TABLEID_MAX ||
		    !rtable_exists(ifr->ifr_rdomainid)) {
			error = EINVAL;
			break;
		}
		s = splnet();
		sc->sc_rdomain = ifr->ifr_rdomainid;
		(void)vxlan_config(ifp, NULL, NULL);
		splx(s);
		break;

	case SIOCGLIFPHYRTABLE:
		ifr->ifr_rdomainid = sc->sc_rdomain;
		break;

	case SIOCSLIFPHYTTL:
		if ((error = suser(p, 0)) != 0)
			break;
		if (ifr->ifr_ttl < 0 || ifr->ifr_ttl > 0xff) {
			error = EINVAL;
			break;
		}
		if (sc->sc_ttl == (u_int8_t)ifr->ifr_ttl)
			break;
		s = splnet();
		sc->sc_ttl = (u_int8_t)(ifr->ifr_ttl);
		(void)vxlan_config(ifp, NULL, NULL);
		splx(s);
		break;

	case SIOCGLIFPHYTTL:
		ifr->ifr_ttl = (int)sc->sc_ttl;
		break;

	case SIOCSVNETID:
		if ((error = suser(p, 0)) != 0)
			break;
		if (ifr->ifr_vnetid < 0 || ifr->ifr_vnetid > 0x00ffffff) {
			error = EINVAL;
			break;
		}
		s = splnet();
		sc->sc_vnetid = (u_int32_t)ifr->ifr_vnetid;
		(void)vxlan_config(ifp, NULL, NULL);
		splx(s);
		break;

	case SIOCGVNETID:
		ifr->ifr_vnetid = (int)sc->sc_vnetid;
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	return (error);
}

int
vxlan_media_change(struct ifnet *ifp)
{
	return (0);
}

void
vxlan_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID | IFM_ACTIVE;
}

int
vxlan_lookup(struct mbuf *m, struct udphdr *uh, int iphlen,
    struct sockaddr *srcsa)
{
	struct mbuf_list	 ml = MBUF_LIST_INITIALIZER();
	struct vxlan_softc	*sc = NULL;
	struct vxlan_header	 v;
	u_int32_t		 vni;
	struct ifnet		*ifp;
	int			 skip;
	struct ether_header	*eh;
#if NBRIDGE > 0
	struct sockaddr		*sa;
#endif
	int			 s;

	/* XXX Should verify the UDP port first before copying the packet */
	skip = iphlen + sizeof(*uh);
	if (m->m_pkthdr.len - skip < sizeof(v))
		return (0);
	m_copydata(m, skip, sizeof(v), (caddr_t)&v);
	skip += sizeof(v);

	vni = ntohl(v.vxlan_id);

	/* Validate header */
	if ((vni == 0) || (vni & VXLAN_RESERVED2) ||
	    (ntohl(v.vxlan_flags) != VXLAN_FLAGS_VNI))
		return (0);

	vni >>= VXLAN_VNI_S;
	LIST_FOREACH(sc, &vxlan_tagh[VXLAN_TAGHASH(vni)], sc_entry) {
		if ((uh->uh_dport == sc->sc_dstport) &&
		    vni == sc->sc_vnetid &&
		    sc->sc_rdomain == rtable_l2(m->m_pkthdr.ph_rtableid))
			goto found;
	}

	/* not found */
	return (0);

 found:
	m_adj(m, skip);
	ifp = &sc->sc_ac.ac_if;

	if ((eh = mtod(m, struct ether_header *)) == NULL)
		return (EINVAL);

#if NBRIDGE > 0
	/* Store the peer IP address for the bridge */
	if (ifp->if_bridgeport != NULL &&
	    srcsa->sa_family != AF_UNSPEC &&
	    (sa = bridge_tunneltag(m, srcsa->sa_family)) != NULL)
		memcpy(sa, srcsa, sa->sa_len);
#endif

	/* Clear multicast flag from the outer packet */
	if (sc->sc_imo.imo_num_memberships > 0 &&
	    m->m_flags & (M_MCAST) &&
	    !ETHER_IS_MULTICAST(eh->ether_dhost))
		m->m_flags &= ~M_MCAST;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ml_enqueue(&ml, m);
	s = splnet();
	if_input(ifp, &ml);
	splx(s);

	/* success */
	return (1);
}

int
vxlan_output(struct ifnet *ifp, struct mbuf *m)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct udpiphdr		*ui;
	struct vxlanudpiphdr	*vi;
	u_int16_t		 len = m->m_pkthdr.len;
	struct ip		*ip;
#if NBRIDGE > 0
	struct sockaddr_in	*sin;
#endif
	int			 error;

	/* VXLAN header */
	M_PREPEND(m, sizeof(*vi), M_DONTWAIT);
	if (m == NULL) {
		ifp->if_oerrors++;
		return (ENOBUFS);
	}

	len += sizeof(struct vxlan_header);

	ui = mtod(m, struct udpiphdr *);
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_src = ((struct sockaddr_in *)&sc->sc_src)->sin_addr;
	ui->ui_dst = ((struct sockaddr_in *)&sc->sc_dst)->sin_addr;
	ui->ui_sport = sc->sc_dstport;
	ui->ui_dport = sc->sc_dstport;
	ui->ui_ulen = htons(sizeof(struct udphdr) + len);

	ip = (struct ip *)ui;
	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_id = htons(ip_randomid());
	ip->ip_off = 0; /* htons(IP_DF); XXX should we disallow IP fragments? */
	ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_len = htons(sizeof(struct udpiphdr) + len);
	if (sc->sc_ttl > 0)
		ip->ip_ttl = sc->sc_ttl;
	else
		ip->ip_ttl = IPDEFTTL;

#if NBRIDGE > 0
	if ((sin = satosin(bridge_tunnel(m))) != NULL &&
	    sin->sin_family == AF_INET) {
		ui->ui_dst = sin->sin_addr;

		/*
		 * If the LINK0 flag is set, send the packet back to
		 * the original source port of the endport, otherwise use
		 * the configured VXLAN port.
		 */
		if (ifp->if_flags & IFF_LINK0)
			ui->ui_dport = sin->sin_port;
	}
	if (sin != NULL)
		bridge_tunneluntag(m);
#endif

	vi = (struct vxlanudpiphdr *)ui;
	vi->ui_v.vxlan_flags = htonl(VXLAN_FLAGS_VNI);
	vi->ui_v.vxlan_id = htonl(sc->sc_vnetid << VXLAN_VNI_S);

	/* UDP checksum should be 0 */
	ui->ui_sum = 0;

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	m->m_pkthdr.ph_rtableid = sc->sc_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	if ((error =
	    ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL, 0))) {
		ifp->if_oerrors++;
	}

	return (error);
}

void
vxlan_addr_change(void *arg)
{
	struct vxlan_softc	*sc = arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	int			 s, error;

	/*
	 * Reset the configuration after resume or any possible address
	 * configuration changes.
	 */
	s = splnet();
	if ((error = vxlan_config(ifp, NULL, NULL))) {
		/*
		 * The source address of the tunnel can temporarily disappear,
		 * after a link state change when running the DHCP client,
		 * so keep it configured.
		 */
	}
	splx(s);
}

void
vxlan_if_change(void *arg)
{
	struct vxlan_softc	*sc = arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	int			 s, error;

	/*
	 * Reset the configuration after the parent interface disappeared.
	 */
	s = splnet();
	if ((error = vxlan_config(ifp, NULL, NULL)) != 0) {
		/* The configured tunnel addresses are invalid, remove them */
		bzero(&sc->sc_src, sizeof(sc->sc_src));
		bzero(&sc->sc_dst, sizeof(sc->sc_dst));
	}
	splx(s);
}

void
vxlan_link_change(void *arg)
{
	struct vxlan_softc	*sc = arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	int			 s;

	/*
	 * The machine might have lost its multicast associations after
	 * link state changes.  This fixes a problem with VMware after
	 * suspend/resume of the host or guest.
	 */
	s = splnet();
	(void)vxlan_config(ifp, NULL, NULL);
	splx(s);
}
