/*	$OpenBSD: if_vlan.c,v 1.27 2001/10/01 09:27:31 niklas Exp $ */
/*
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net/if_vlan.c,v 1.16 2000/03/26 15:21:40 charnier Exp $
 */

/*
 * if_vlan.c - pseudo-device driver for IEEE 802.1Q virtual LANs.
 * Might be extended some day to also handle IEEE 802.1p priority
 * tagging.  This is sort of sneaky in the implementation, since
 * we need to pretend to be enough of an Ethernet implementation
 * to make arp work.  The way we do this is by telling everyone
 * that we are an Ethernet, and then catch the packets that
 * ether_output() left on our output queue when it calls
 * if_start(), rewrite them for use by the real outgoing interface,
 * and ask it to send them.
 *
 * Some devices support 802.1Q tag insertion and extraction in firmware.
 * The vlan interface behavior changes when the IFCAP_VLAN_HWTAGGING
 * capability is set on the parent.  In this case, vlan_start() will not
 * modify the ethernet header.  On input, the parent can call vlan_input_tag()
 * directly in order to supply us with an incoming mbuf and the vlan
 * tag value that goes with it.
 */

#include "vlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_vlan_var.h>

struct	ifaddr	**ifnet_addrs;

struct ifvlan *ifv_softc;
int nifvlan;

extern int ifqmaxlen;

void	vlan_start (struct ifnet *ifp);
int	vlan_ioctl (struct ifnet *ifp, u_long cmd, caddr_t addr);
int	vlan_setmulti (struct ifnet *ifp);
int	vlan_unconfig (struct ifnet *ifp);
int	vlan_config (struct ifvlan *ifv, struct ifnet *p);
void	vlanattach (int count);
int	vlan_set_promisc (struct ifnet *ifp);

/*
 * Program our multicast filter. What we're actually doing is
 * programming the multicast filter of the parent. This has the
 * side effect of causing the parent interface to receive multicast
 * traffic that it doesn't really want, which ends up being discarded
 * later by the upper protocol layers. Unfortunately, there's no way
 * to avoid this: there really is only one physical interface.
 */

int vlan_setmulti(struct ifnet *ifp)
{
	struct ifreq		*ifr_p;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	struct ifvlan		*sc;
	struct vlan_mc_entry	*mc = NULL;
	int			error;

	/* Find the parent. */
	sc = ifp->if_softc;
	ifr_p = (struct ifreq *)&sc->ifv_p->if_data;

	/* First, remove any existing filter entries. */
	while(sc->vlan_mc_listhead.slh_first != NULL) {
		mc = sc->vlan_mc_listhead.slh_first;
		error = ether_delmulti(ifr_p, &sc->ifv_ac);
		if (error)
			return(error);
		SLIST_REMOVE_HEAD(&sc->vlan_mc_listhead, mc_entries);
		free(mc, M_DEVBUF);
	}

	/* Now program new ones. */
	ETHER_FIRST_MULTI(step, &sc->ifv_ac, enm);
	while (enm != NULL) {
		mc = malloc(sizeof(struct vlan_mc_entry), M_DEVBUF, M_NOWAIT);
		bcopy(enm->enm_addrlo,
		    (void *) &mc->mc_addr, ETHER_ADDR_LEN);
		SLIST_INSERT_HEAD(&sc->vlan_mc_listhead, mc, mc_entries);
		error = ether_addmulti(ifr_p, &sc->ifv_ac);
		if (error)
			return(error);
		ETHER_NEXT_MULTI(step, enm);
	}

	return(0);
}

void
vlanattach(int count)
{
	struct ifnet *ifp;
	int i;

	MALLOC(ifv_softc, struct ifvlan *, count * sizeof(struct ifvlan),
	    M_DEVBUF, M_NOWAIT);
	if (ifv_softc == NULL)
		panic("vlanattach: MALLOC failed");
	nifvlan = count;
	bzero(ifv_softc, nifvlan * sizeof(ifv_softc));

	for (i = 0; i < nifvlan; i++) {
		ifp = &ifv_softc[i].ifv_if;
		ifp->if_softc = &ifv_softc[i];
		sprintf(ifp->if_xname, "vlan%d", i);
		/* NB: flags are not set here */
		/* NB: mtu is not set here */

		ifp->if_start = vlan_start;
		ifp->if_ioctl = vlan_ioctl;
		ifp->if_output = ether_output;
		IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
		IFQ_SET_READY(&ifp->if_snd);
		if_attach(ifp);
		ether_ifattach(ifp);

		/* Now undo some of the damage... */
		ifp->if_type = IFT_8021_VLAN;
		ifp->if_hdrlen = EVL_ENCAPLEN;
	}
}

void
vlan_start(struct ifnet *ifp)
{
	struct ifvlan *ifv;
	struct ifnet *p;
	struct ether_vlan_header *evl;
	struct mbuf *m, *m0;
	int error;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	ifv = ifp->if_softc;
	p = ifv->ifv_p;

	ifp->if_flags |= IFF_OACTIVE;
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if ((p->if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING)) {
			IF_DROP(&p->if_snd);
				/* XXX stats */
			ifp->if_oerrors++;
			m_freem(m);
			continue;
		}

#ifdef ALTQ
		/*
		 * If ALTQ is enabled on the parent interface, do
		 * classification; the queueing discipline might
		 * not require classification, but might require
		 * the address family/header pointer in the pktattr.
		 */
		if (ALTQ_IS_ENABLED(&p->if_snd)) {
			switch (p->if_type) {
			case IFT_ETHER:
				altq_etherclassify(&p->if_snd, m, &pktattr);
				break;
#ifdef DIAGNOSTIC
			default:
				panic("vlan_start: impossible (altq)");
#endif
			}
		}
#endif /* ALTQ */

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/*
		 * If the IFCAP_VLAN_HWTAGGING capability is set on the parent,
		 * it can do VLAN tag insertion itself and doesn't require us
	 	 * to create a special header for it. In this case, we just pass
		 * the packet along. However, we need some way to tell the
		 * interface where the packet came from so that it knows how
		 * to find the VLAN tag to use, so we set the rcvif in the
		 * mbuf header to our ifnet.
		 *
		 * Note: we also set the M_PROTO1 flag in the mbuf to let
		 * the parent driver know that the rcvif pointer is really
		 * valid. We need to do this because sometimes mbufs will
		 * be allocated by other parts of the system that contain
		 * garbage in the rcvif pointer. Using the M_PROTO1 flag
		 * lets the driver perform a proper sanity check and avoid
		 * following potentially bogus rcvif pointers off into
		 * never-never land.
		 */
		if (p->if_capabilities & IFCAP_VLAN_HWTAGGING) {
			m->m_pkthdr.rcvif = ifp;
			m->m_flags |= M_PROTO1;
		} else {
			if (m->m_len < sizeof(struct ether_header) &&
			    (m = m_pullup(m, sizeof(struct ether_header)))
			    == NULL) {
				ifp->if_ierrors++;
				continue;
			}

			if (m->m_flags & M_PKTHDR) {
				MGETHDR(m0, MT_DATA, M_DONTWAIT);
			} else {
				MGET(m0, MT_DATA, M_DONTWAIT);
			}

			if (m0 == NULL) {
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			}

			if (m0->m_flags & M_PKTHDR)
				M_MOVE_PKTHDR(m0, m);

			m0->m_flags &= ~M_PROTO1;
			m0->m_next = m;
			m0->m_len = sizeof(struct ether_vlan_header);

			evl = mtod(m0, struct ether_vlan_header *);
			bcopy(mtod(m, char *),
			    evl, sizeof(struct ether_header));
			evl->evl_proto = evl->evl_encap_proto;
			evl->evl_encap_proto = htons(ETHERTYPE_8021Q);
			evl->evl_tag = htons(ifv->ifv_tag);

			m->m_len -= sizeof(struct ether_header);
			m->m_data += sizeof(struct ether_header);

			m = m0;
		}

		/*
		 * Send it, precisely as ether_output() would have.
		 * We are already running at splimp.
		 */
		p->if_obytes += m->m_pkthdr.len;
		if (m->m_flags & M_MCAST)
			p->if_omcasts++;
		IFQ_ENQUEUE(&p->if_snd, m, &pktattr, error);
		if (error) {
			/* mbuf is already freed */
			ifp->if_oerrors++;
			continue;
		}

		ifp->if_opackets++;
		if ((p->if_flags & IFF_OACTIVE) == 0)
			p->if_start(p);
	}
	ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

int
vlan_input_tag(struct mbuf *m, u_int16_t t)
{
	int i;
	struct ifvlan *ifv;
	struct ether_vlan_header vh;

	for (i = 0; i < nifvlan; i++) {
		ifv = &ifv_softc[i];
		if (m->m_pkthdr.rcvif == ifv->ifv_p && t == ifv->ifv_tag)
			break;
	}

	if (i >= nifvlan) {
		if (m->m_pkthdr.len < sizeof(struct ether_header))
			return (-1);
		m_copydata(m, 0, sizeof(struct ether_header), (caddr_t)&vh);
		vh.evl_proto = vh.evl_encap_proto;
		vh.evl_tag = htons(t);
		vh.evl_encap_proto = htons(ETHERTYPE_8021Q);
		M_PREPEND(m, EVL_ENCAPLEN, M_DONTWAIT);
		if (m == NULL)
			return (-1);
		m_copyback(m, 0, sizeof(struct ether_vlan_header), (caddr_t)&vh);
		ether_input_mbuf(m->m_pkthdr.rcvif, m);
		return (-1);
	}

	if ((ifv->ifv_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
	    (IFF_UP|IFF_RUNNING)) {
		m_freem(m);
		return (-1);
	}

	/*
	 * Having found a valid vlan interface corresponding to
	 * the given source interface and vlan tag, run the
	 * the real packet through ether_input().
	 */
	m->m_pkthdr.rcvif = &ifv->ifv_if;

#if NBPFILTER > 0
	if (ifv->ifv_if.if_bpf) {
		/*
		 * Do the usual BPF fakery.  Note that we don't support
		 * promiscuous mode here, since it would require the
		 * drivers to know about VLANs and we're not ready for
		 * that yet.
		 */
		bpf_mtap(ifv->ifv_if.if_bpf, m);
	}
#endif
	ifv->ifv_if.if_ipackets++;
	ether_input_mbuf(&ifv->ifv_if, m);
	return 0;
}

int
vlan_input(eh, m)
	struct ether_header *eh;
	struct mbuf *m;
{
	int i;
	struct ifvlan *ifv;
	u_int tag;

	if (m->m_len < EVL_ENCAPLEN &&
	    (m = m_pullup(m, EVL_ENCAPLEN)) == NULL) {
		m->m_pkthdr.rcvif->if_ierrors++;
		return (0);
	}

	tag = EVL_VLANOFTAG(ntohs(*mtod(m, u_int16_t *)));

	for (i = 0; i < nifvlan; i++) {
		ifv = &ifv_softc[i];
		if (m->m_pkthdr.rcvif == ifv->ifv_p && tag == ifv->ifv_tag)
			break;
	}

	if (i >= nifvlan || (ifv->ifv_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
	    (IFF_UP|IFF_RUNNING)) {
		m_freem(m);
		return -1;	/* so ether_input can take note */
	}

	/*
	 * Having found a valid vlan interface corresponding to
	 * the given source interface and vlan tag, remove the
	 * encapsulation, and run the real packet through
	 * ether_input() a second time (it had better be
	 * reentrant!).
	 */
	m->m_pkthdr.rcvif = &ifv->ifv_if;
	eh->ether_type = mtod(m, u_int16_t *)[1];
	m->m_len -= EVL_ENCAPLEN;
	m->m_data += EVL_ENCAPLEN;
	m->m_pkthdr.len -= EVL_ENCAPLEN;

#if NBPFILTER > 0
	if (ifv->ifv_if.if_bpf) {
		/*
		 * Do the usual BPF fakery.  Note that we don't support
		 * promiscuous mode here, since it would require the
		 * drivers to know about VLANs and we're not ready for
		 * that yet.
		 */
		struct mbuf m0;
		m0.m_next = m;
		m0.m_len = sizeof(struct ether_header);
		m0.m_data = (char *)eh;
		bpf_mtap(ifv->ifv_if.if_bpf, &m0);
	}
#endif
	ifv->ifv_if.if_ipackets++;
	ether_input(&ifv->ifv_if, eh, m);

	return 0;
}

int
vlan_config(struct ifvlan *ifv, struct ifnet *p)
{
	struct ifaddr *ifa1, *ifa2;
	struct sockaddr_dl *sdl1, *sdl2;

	if (p->if_type != IFT_ETHER)
		return EPROTONOSUPPORT;
	if (ifv->ifv_p)
		return EBUSY;
	ifv->ifv_p = p;

	if (p->if_capabilities & IFCAP_VLAN_MTU)
		ifv->ifv_if.if_mtu = p->if_mtu;
	else {
		/*
		 * This will be incompatible with strict
		 * 802.1Q implementations
		 */
		ifv->ifv_if.if_mtu = p->if_mtu - EVL_ENCAPLEN;
#ifdef DIAGNOSTIC
		printf("%s: initialized with non-standard mtu %d (parent %s)\n",
		    ifv->ifv_if.if_xname, ifv->ifv_if.if_mtu,
		    ifv->ifv_p->if_xname);
#endif
	}

	ifv->ifv_if.if_flags = p->if_flags &
	    (IFF_UP | IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);

	/*
	 * Inherit the if_type from the parent.  This allows us to
	 * participate in bridges of that type.
	 */
	ifv->ifv_if.if_type = p->if_type;

	/*
	 * Inherit baudrate from the parent.  An SNMP agent would use this
	 * information.
	 */
	ifv->ifv_if.if_baudrate = p->if_baudrate;

	/*
	 * If the parent interface can do hardware-assisted
	 * VLAN encapsulation, then propagate its hardware-
	 * assisted checksumming flags.
	 *
	 * If the card cannot handle hardware tagging, it cannot
	 * possibly compute the correct checksums for tagged packets.
	 *
	 * This brings up another possibility, do cards exist which
	 * have all of these capabilities but cannot utilize them together?
	 */
	if (p->if_capabilities & IFCAP_VLAN_HWTAGGING)
		ifv->ifv_if.if_capabilities = p->if_capabilities &
		    (IFCAP_CSUM_IPv4|IFCAP_CSUM_TCPv4|
		    IFCAP_CSUM_UDPv4);
		/* (IFCAP_CSUM_TCPv6|IFCAP_CSUM_UDPv6); */

	/*
	 * Set up our ``Ethernet address'' to reflect the underlying
	 * physical interface's.
	 */
	ifa1 = ifnet_addrs[ifv->ifv_if.if_index];
	ifa2 = ifnet_addrs[p->if_index];
	sdl1 = (struct sockaddr_dl *)ifa1->ifa_addr;
	sdl2 = (struct sockaddr_dl *)ifa2->ifa_addr;
	sdl1->sdl_type = IFT_ETHER;
	sdl1->sdl_alen = ETHER_ADDR_LEN;
	bcopy(LLADDR(sdl2), LLADDR(sdl1), ETHER_ADDR_LEN);
	bcopy(LLADDR(sdl2), ifv->ifv_ac.ac_enaddr, ETHER_ADDR_LEN);
	return 0;
}

int
vlan_unconfig(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct ifvlan *ifv;
	struct ifnet *p;
	struct ifreq *ifr, *ifr_p;
	struct vlan_mc_entry *mc;
	int error;

	ifv = ifp->if_softc;
	p = ifv->ifv_p;
	ifr = (struct ifreq *)&ifp->if_data;
	ifr_p = (struct ifreq *)&ifv->ifv_p->if_data;

	/*
 	 * Since the interface is being unconfigured, we need to
	 * empty the list of multicast groups that we may have joined
	 * while we were alive and remove them from the parent's list
	 * as well.
	 */
	while(ifv->vlan_mc_listhead.slh_first != NULL) {

		mc = ifv->vlan_mc_listhead.slh_first;
		error = ether_delmulti(ifr_p, &ifv->ifv_ac);
		error = ether_delmulti(ifr, &ifv->ifv_ac);
		if (error)
			return(error);
		SLIST_REMOVE_HEAD(&ifv->vlan_mc_listhead, mc_entries);
		free(mc, M_DEVBUF);
	}

	/* Disconnect from parent. */
	ifv->ifv_p = NULL;
	ifv->ifv_if.if_mtu = ETHERMTU;

	/* Clear our MAC address. */
	ifa = ifnet_addrs[ifv->ifv_if.if_index];
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ETHER_ADDR_LEN;
	bzero(LLADDR(sdl), ETHER_ADDR_LEN);
	bzero(ifv->ifv_ac.ac_enaddr, ETHER_ADDR_LEN);

	return 0;
}

int
vlan_set_promisc(struct ifnet *ifp)
{
	struct ifvlan *ifv = ifp->if_softc;
	int error = 0;

	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		if ((ifv->ifv_flags & IFVF_PROMISC) == 0) {
			error = ifpromisc(ifv->ifv_p, 1);
			if (error == 0)
				ifv->ifv_flags |= IFVF_PROMISC;
		}
	} else {
		if ((ifv->ifv_flags & IFVF_PROMISC) != 0) {
			error = ifpromisc(ifv->ifv_p, 0);
			if (error == 0)
				ifv->ifv_flags &= ~IFVF_PROMISC;
		}
	}

	return (0);
}

int
vlan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc *p = curproc;	/* XXX */
	struct ifaddr *ifa;
	struct ifnet *pr;
	struct ifreq *ifr;
	struct ifvlan *ifv;
	struct vlanreq vlr;
	int error = 0, p_mtu = 0;

	ifr = (struct ifreq *)data;
	ifa = (struct ifaddr *)data;
	ifv = ifp->if_softc;

	switch (cmd) {
	case SIOCSIFADDR:
		if (ifv->ifv_p != NULL) {
			ifp->if_flags |= IFF_UP;

			switch (ifa->ifa_addr->sa_family) {
#ifdef INET
			case AF_INET:
				arp_ifinit(&ifv->ifv_ac, ifa);
				break;
#endif
			default:
				break;
			}
		} else {
			error = EINVAL;
		}
		break;

	case SIOCGIFADDR:
		{
			struct sockaddr *sa;

			sa = (struct sockaddr *) &ifr->ifr_data;
			bcopy(((struct arpcom *)ifp->if_softc)->ac_enaddr,
			    (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
		}
		break;

	case SIOCSIFMTU:
		if (ifv->ifv_p != NULL) {
			if (ifv->ifv_p->if_capabilities & IFCAP_VLAN_MTU)
				p_mtu = ifv->ifv_p->if_mtu;
			else
				p_mtu = ifv->ifv_p->if_mtu - EVL_ENCAPLEN;
			
			if (ifr->ifr_mtu > p_mtu || ifr->ifr_mtu < ETHERMIN)
				error = EINVAL;
			else
				ifp->if_mtu = ifr->ifr_mtu;
		} else
			error = EINVAL;

		break;

	case SIOCSETVLAN:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &vlr, sizeof vlr)))
			break;
		if (vlr.vlr_parent[0] == '\0') {
			vlan_unconfig(ifp);
			if_down(ifp);
			ifp->if_flags &= ~(IFF_UP|IFF_RUNNING);
			break;
		}
		if (vlr.vlr_tag != EVL_VLANOFTAG(vlr.vlr_tag)) {
			error = EINVAL;		 /* check for valid tag */
			break;
		}
		pr = ifunit(vlr.vlr_parent);
		if (pr == NULL) {
			error = ENOENT;
			break;
		}
		error = vlan_config(ifv, pr);
		if (error)
			break;
		ifv->ifv_tag = vlr.vlr_tag;
		ifp->if_flags |= IFF_RUNNING;

		/* Update promiscuous mode, if necessary. */
		vlan_set_promisc(ifp);
		break;
		
	case SIOCGETVLAN:
		bzero(&vlr, sizeof vlr);
		if (ifv->ifv_p) {
			snprintf(vlr.vlr_parent, sizeof(vlr.vlr_parent),
			    "%s", ifv->ifv_p->if_xname);
			vlr.vlr_tag = ifv->ifv_tag;
		}
		error = copyout(&vlr, ifr->ifr_data, sizeof vlr);
		break;
		
	case SIOCSIFFLAGS:
		/*
		 * For promiscuous mode, we enable promiscuous mode on
		 * the parent if we need promiscuous on the VLAN interface.
		 */
		if (ifv->ifv_p != NULL)
			error = vlan_set_promisc(ifp);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifv->ifv_p != NULL) {
			error = vlan_setmulti(ifp);
		} else {
			error = EINVAL;
		}
		break;
	default:
		error = EINVAL;
	}
	return error;
}
