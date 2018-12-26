/*	$OpenBSD: if_ethersubr.c,v 1.257 2018/12/26 18:32:38 denis Exp $	*/
/*	$NetBSD: if_ethersubr.c,v 1.19 1996/05/07 02:40:30 thorpej Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 */

/*
%%% portions-copyright-nrl-95
Portions of this software are Copyright 1995-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.
*/

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_ipsp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include "pppoe.h"
#if NPPPOE > 0
#include <net/if_pppoe.h>
#endif

#include "bpe.h"
#if NBPE > 0
#include <net/if_bpe.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif

#ifdef PIPEX
#include <net/pipex.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#endif /* MPLS */

u_int8_t etherbroadcastaddr[ETHER_ADDR_LEN] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
u_int8_t etheranyaddr[ETHER_ADDR_LEN] =
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#define senderr(e) { error = (e); goto bad;}

int
ether_ioctl(struct ifnet *ifp, struct arpcom *arp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ifp->if_hardmtu)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_MULTICAST) {
			error = (cmd == SIOCADDMULTI) ?
			    ether_addmulti(ifr, arp) :
			    ether_delmulti(ifr, arp);
		} else
			error = ENOTTY;
		break;

	default:
		error = ENOTTY;
	}

	return (error);
}


void
ether_rtrequest(struct ifnet *ifp, int req, struct rtentry *rt)
{
	switch (rt_key(rt)->sa_family) {
	case AF_INET:
		arp_rtrequest(ifp, req, rt);
		break;
#ifdef INET6
	case AF_INET6:
		nd6_rtrequest(ifp, req, rt);
		break;
#endif
	default:
		break;
	}
}

int
ether_resolve(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt, struct ether_header *eh)
{
	struct arpcom *ac = (struct arpcom *)ifp;
	sa_family_t af = dst->sa_family;
	int error = 0;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		senderr(ENETDOWN);

	KASSERT(rt != NULL || ISSET(m->m_flags, M_MCAST|M_BCAST) ||
		af == AF_UNSPEC || af == pseudo_AF_HDRCMPLT);

#ifdef DIAGNOSTIC
	if (ifp->if_rdomain != rtable_l2(m->m_pkthdr.ph_rtableid)) {
		printf("%s: trying to send packet on wrong domain. "
		    "if %d vs. mbuf %d\n", ifp->if_xname,
		    ifp->if_rdomain, rtable_l2(m->m_pkthdr.ph_rtableid));
	}
#endif

	switch (af) {
	case AF_INET:
		error = arpresolve(ifp, rt, m, dst, eh->ether_dhost);
		if (error)
			return (error);
		eh->ether_type = htons(ETHERTYPE_IP);

		/* If broadcasting on a simplex interface, loopback a copy */
		if (ISSET(m->m_flags, M_BCAST) &&
		    ISSET(ifp->if_flags, IFF_SIMPLEX) &&
		    !m->m_pkthdr.pf.routed) {
			struct mbuf *mcopy;

			/* XXX Should we input an unencrypted IPsec packet? */
			mcopy = m_copym(m, 0, M_COPYALL, M_NOWAIT);
			if (mcopy != NULL)
				if_input_local(ifp, mcopy, af);
		}
		break;
#ifdef INET6
	case AF_INET6:
		error = nd6_resolve(ifp, rt, m, dst, eh->ether_dhost);
		if (error)
			return (error);
		eh->ether_type = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef MPLS
	case AF_MPLS:
		if (rt)
			dst = rt_key(rt);
		else
			senderr(EHOSTUNREACH);

		if (!ISSET(ifp->if_xflags, IFXF_MPLS))
			senderr(ENETUNREACH);

		af = dst->sa_family;
		if (af == AF_MPLS)
			af = rt->rt_gateway->sa_family;         

		switch (af) {
		case AF_LINK:
			if (satosdl(dst)->sdl_alen < sizeof(eh->ether_dhost))
				senderr(EHOSTUNREACH);
			memcpy(eh->ether_dhost, LLADDR(satosdl(dst)),
			    sizeof(eh->ether_dhost));
			break;
#ifdef INET6
		case AF_INET6:
			error = nd6_resolve(ifp, rt, m, dst, eh->ether_dhost);
			if (error)
				return (error);
			break;
#endif
		case AF_INET:
			error = arpresolve(ifp, rt, m, dst, eh->ether_dhost);
			if (error)
				return (error);
			break;
		default:
			senderr(EHOSTUNREACH);
		}
		/* XXX handling for simplex devices in case of M/BCAST ?? */
		if (m->m_flags & (M_BCAST | M_MCAST))
			eh->ether_type = htons(ETHERTYPE_MPLS_MCAST);
		else
			eh->ether_type = htons(ETHERTYPE_MPLS);
		break;
#endif /* MPLS */
	case pseudo_AF_HDRCMPLT:
		/* take the whole header from the sa */
		memcpy(eh, dst->sa_data, sizeof(*eh));
		return (0);

	case AF_UNSPEC:
		/* take the dst and type from the sa, but get src below */
		memcpy(eh, dst->sa_data, sizeof(*eh));
		break;

	default:
		printf("%s: can't handle af%d\n", ifp->if_xname, af);
		senderr(EAFNOSUPPORT);
	}

	memcpy(eh->ether_shost, ac->ac_enaddr, sizeof(eh->ether_shost));

	return (0);

bad:
	m_freem(m);
	return (error);
}

struct mbuf*
ether_encap(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt, int *errorp)
{
	struct ether_header eh;
	int error;

	error = ether_resolve(ifp, m, dst, rt, &eh);
	switch (error) {
	case 0:
		break;
	case EAGAIN:
		error = 0;
	default:
		*errorp = error;
		return (NULL);
	}

	m = m_prepend(m, ETHER_ALIGN + sizeof(eh), M_DONTWAIT);
	if (m == NULL) {
		*errorp = ENOBUFS;
		return (NULL);
	}

	m_adj(m, ETHER_ALIGN);
	memcpy(mtod(m, struct ether_header *), &eh, sizeof(eh));

	return (m);
}

int
ether_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	int error;

	m = ether_encap(ifp, m, dst, rt, &error);
	if (m == NULL)
		return (error);

	return (if_enqueue(ifp, m));
}

/*
 * Process a received Ethernet packet;
 * the packet is in the mbuf chain m without
 * the ether header, which is provided separately.
 */
int
ether_input(struct ifnet *ifp, struct mbuf *m, void *cookie)
{
	struct ether_header *eh;
	void (*input)(struct ifnet *, struct mbuf *);
	u_int16_t etype;
	struct arpcom *ac;

	/* Drop short frames */
	if (m->m_len < ETHER_HDR_LEN)
		goto dropanyway;

	ac = (struct arpcom *)ifp;
	eh = mtod(m, struct ether_header *);

	/* Is the packet for us? */
	if (memcmp(ac->ac_enaddr, eh->ether_dhost, ETHER_ADDR_LEN) != 0) {

		/* If not, it must be multicast or broadcast to go further */
		if (!ETHER_IS_MULTICAST(eh->ether_dhost))
			goto dropanyway;

		/*
		 * If this is not a simplex interface, drop the packet
		 * if it came from us.
		 */
		if ((ifp->if_flags & IFF_SIMPLEX) == 0) {
			if (memcmp(ac->ac_enaddr, eh->ether_shost,
			    ETHER_ADDR_LEN) == 0)
				goto dropanyway;
		}

		if (memcmp(etherbroadcastaddr, eh->ether_dhost,
		    ETHER_ADDR_LEN) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
		ifp->if_imcasts++;
	}

	/*
	 * HW vlan tagged packets that were not collected by vlan(4) must
	 * be dropped now.
	 */
	if (m->m_flags & M_VLANTAG)
		goto dropanyway;

	etype = ntohs(eh->ether_type);

	switch (etype) {
	case ETHERTYPE_IP:
		input = ipv4_input;
		break;

	case ETHERTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP)
			goto dropanyway;
		input = arpinput;
		break;

	case ETHERTYPE_REVARP:
		if (ifp->if_flags & IFF_NOARP)
			goto dropanyway;
		input = revarpinput;
		break;

#ifdef INET6
	/*
	 * Schedule IPv6 software interrupt for incoming IPv6 packet.
	 */
	case ETHERTYPE_IPV6:
		input = ipv6_input;
		break;
#endif /* INET6 */
#if NPPPOE > 0 || defined(PIPEX)
	case ETHERTYPE_PPPOEDISC:
	case ETHERTYPE_PPPOE:
		if (m->m_flags & (M_MCAST | M_BCAST))
			goto dropanyway;
#ifdef PIPEX
		if (pipex_enable) {
			struct pipex_session *session;

			if ((session = pipex_pppoe_lookup_session(m)) != NULL) {
				pipex_pppoe_input(m, session);
				return (1);
			}
		}
#endif
		if (etype == ETHERTYPE_PPPOEDISC)
			niq_enqueue(&pppoediscinq, m);
		else
			niq_enqueue(&pppoeinq, m);
		return (1);
#endif
#ifdef MPLS
	case ETHERTYPE_MPLS:
	case ETHERTYPE_MPLS_MCAST:
		input = mpls_input;
		break;
#endif
#if NBPE > 0
	case ETHERTYPE_PBB:
		bpe_input(ifp, m);
		return (1);
#endif
	default:
		goto dropanyway;
	}

	m_adj(m, sizeof(*eh));
	(*input)(ifp, m);
	return (1);
dropanyway:
	m_freem(m);
	return (1);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
char *
ether_sprintf(u_char *ap)
{
	int i;
	static char etherbuf[ETHER_ADDR_LEN * 3];
	char *cp = etherbuf;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}

/*
 * Generate a (hopefully) acceptable MAC address, if asked.
 */
void
ether_fakeaddr(struct ifnet *ifp)
{
	static int unit;
	int rng = arc4random();

	/* Non-multicast; locally administered address */
	((struct arpcom *)ifp)->ac_enaddr[0] = 0xfe;
	((struct arpcom *)ifp)->ac_enaddr[1] = 0xe1;
	((struct arpcom *)ifp)->ac_enaddr[2] = 0xba;
	((struct arpcom *)ifp)->ac_enaddr[3] = 0xd0 | (unit++ & 0xf);
	((struct arpcom *)ifp)->ac_enaddr[4] = rng;
	((struct arpcom *)ifp)->ac_enaddr[5] = rng >> 8;
}

/*
 * Perform common duties while attaching to interface list
 */
void
ether_ifattach(struct ifnet *ifp)
{
	struct arpcom *ac = (struct arpcom *)ifp;

	/*
	 * Any interface which provides a MAC address which is obviously
	 * invalid gets whacked, so that users will notice.
	 */
	if (ETHER_IS_MULTICAST(((struct arpcom *)ifp)->ac_enaddr))
		ether_fakeaddr(ifp);

	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	ifp->if_mtu = ETHERMTU;
	if (ifp->if_output == NULL)
		ifp->if_output = ether_output;
	ifp->if_rtrequest = ether_rtrequest;

	if_ih_insert(ifp, ether_input, NULL);

	if (ifp->if_hardmtu == 0)
		ifp->if_hardmtu = ETHERMTU;

	if_alloc_sadl(ifp);
	memcpy(LLADDR(ifp->if_sadl), ac->ac_enaddr, ifp->if_addrlen);
	LIST_INIT(&ac->ac_multiaddrs);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, ETHER_HDR_LEN);
#endif
}

void
ether_ifdetach(struct ifnet *ifp)
{
	struct arpcom *ac = (struct arpcom *)ifp;
	struct ether_multi *enm;

	/* Undo pseudo-driver changes. */
	if_deactivate(ifp);

	if_ih_remove(ifp, ether_input, NULL);

	KASSERT(SRPL_EMPTY_LOCKED(&ifp->if_inputs));

	for (enm = LIST_FIRST(&ac->ac_multiaddrs);
	    enm != NULL;
	    enm = LIST_FIRST(&ac->ac_multiaddrs)) {
		LIST_REMOVE(enm, enm_list);
		free(enm, M_IFMADDR, sizeof *enm);
	}
}

#if 0
/*
 * This is for reference.  We have table-driven versions of the
 * crc32 generators, which are faster than the double-loop.
 */
u_int32_t __pure
ether_crc32_le_update(u_int_32_t crc, const u_int8_t *buf, size_t len)
{
	u_int32_t c, carry;
	size_t i, j;

	for (i = 0; i < len; i++) {
		c = buf[i];
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x01) ? 1 : 0) ^ (c & 0x01);
			crc >>= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_LE);
		}
	}

	return (crc);
}

u_int32_t __pure
ether_crc32_be_update(u_int_32_t crc, const u_int8_t *buf, size_t len)
{
	u_int32_t c, carry;
	size_t i, j;

	for (i = 0; i < len; i++) {
		c = buf[i];
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x80000000U) ? 1 : 0) ^ (c & 0x01);
			crc <<= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_BE) | carry;
		}
	}

	return (crc);
}
#else
u_int32_t __pure
ether_crc32_le_update(u_int32_t crc, const u_int8_t *buf, size_t len)
{
	static const u_int32_t crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	size_t i;

	for (i = 0; i < len; i++) {
		crc ^= buf[i];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}

	return (crc);
}

u_int32_t __pure
ether_crc32_be_update(u_int32_t crc, const u_int8_t *buf, size_t len)
{
	static const u_int8_t rev[] = {
		0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
		0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
	};
	static const u_int32_t crctab[] = {
		0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
		0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
		0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
		0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd
	};
	size_t i;
	u_int8_t data;

	for (i = 0; i < len; i++) {
		data = buf[i];
		crc = (crc << 4) ^ crctab[(crc >> 28) ^ rev[data & 0xf]];
		crc = (crc << 4) ^ crctab[(crc >> 28) ^ rev[data >> 4]];
	}

	return (crc);
}
#endif

u_int32_t
ether_crc32_le(const u_int8_t *buf, size_t len)
{
	return ether_crc32_le_update(0xffffffff, buf, len);
}

u_int32_t
ether_crc32_be(const u_int8_t *buf, size_t len)
{
	return ether_crc32_be_update(0xffffffff, buf, len);
}

u_char	ether_ipmulticast_min[ETHER_ADDR_LEN] =
    { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x00 };
u_char	ether_ipmulticast_max[ETHER_ADDR_LEN] =
    { 0x01, 0x00, 0x5e, 0x7f, 0xff, 0xff };

#ifdef INET6
u_char	ether_ip6multicast_min[ETHER_ADDR_LEN] =
    { 0x33, 0x33, 0x00, 0x00, 0x00, 0x00 };
u_char	ether_ip6multicast_max[ETHER_ADDR_LEN] =
    { 0x33, 0x33, 0xff, 0xff, 0xff, 0xff };
#endif

/*
 * Convert a sockaddr into an Ethernet address or range of Ethernet
 * addresses.
 */
int
ether_multiaddr(struct sockaddr *sa, u_int8_t addrlo[ETHER_ADDR_LEN],
    u_int8_t addrhi[ETHER_ADDR_LEN])
{
	struct sockaddr_in *sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif /* INET6 */

	switch (sa->sa_family) {

	case AF_UNSPEC:
		memcpy(addrlo, sa->sa_data, ETHER_ADDR_LEN);
		memcpy(addrhi, addrlo, ETHER_ADDR_LEN);
		break;

	case AF_INET:
		sin = satosin(sa);
		if (sin->sin_addr.s_addr == INADDR_ANY) {
			/*
			 * An IP address of INADDR_ANY means listen to
			 * or stop listening to all of the Ethernet
			 * multicast addresses used for IP.
			 * (This is for the sake of IP multicast routers.)
			 */
			memcpy(addrlo, ether_ipmulticast_min, ETHER_ADDR_LEN);
			memcpy(addrhi, ether_ipmulticast_max, ETHER_ADDR_LEN);
		} else {
			ETHER_MAP_IP_MULTICAST(&sin->sin_addr, addrlo);
			memcpy(addrhi, addrlo, ETHER_ADDR_LEN);
		}
		break;
#ifdef INET6
	case AF_INET6:
		sin6 = satosin6(sa);
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			/*
			 * An IP6 address of 0 means listen to or stop
			 * listening to all of the Ethernet multicast
			 * address used for IP6.
			 *
			 * (This might not be healthy, given IPv6's reliance on
			 * multicast for things like neighbor discovery.
			 * Perhaps initializing all-nodes, solicited nodes, and
			 * possibly all-routers for this interface afterwards
			 * is not a bad idea.)
			 */

			memcpy(addrlo, ether_ip6multicast_min, ETHER_ADDR_LEN);
			memcpy(addrhi, ether_ip6multicast_max, ETHER_ADDR_LEN);
		} else {
			ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, addrlo);
			memcpy(addrhi, addrlo, ETHER_ADDR_LEN);
		}
		break;
#endif

	default:
		return (EAFNOSUPPORT);
	}
	return (0);
}

/*
 * Add an Ethernet multicast address or range of addresses to the list for a
 * given interface.
 */
int
ether_addmulti(struct ifreq *ifr, struct arpcom *ac)
{
	struct ether_multi *enm;
	u_char addrlo[ETHER_ADDR_LEN];
	u_char addrhi[ETHER_ADDR_LEN];
	int s = splnet(), error;

	error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
	if (error != 0) {
		splx(s);
		return (error);
	}

	/*
	 * Verify that we have valid Ethernet multicast addresses.
	 */
	if ((addrlo[0] & 0x01) != 1 || (addrhi[0] & 0x01) != 1) {
		splx(s);
		return (EINVAL);
	}
	/*
	 * See if the address range is already in the list.
	 */
	ETHER_LOOKUP_MULTI(addrlo, addrhi, ac, enm);
	if (enm != NULL) {
		/*
		 * Found it; just increment the reference count.
		 */
		++enm->enm_refcount;
		splx(s);
		return (0);
	}
	/*
	 * New address or range; malloc a new multicast record
	 * and link it into the interface's multicast list.
	 */
	enm = malloc(sizeof(*enm), M_IFMADDR, M_NOWAIT);
	if (enm == NULL) {
		splx(s);
		return (ENOBUFS);
	}
	memcpy(enm->enm_addrlo, addrlo, ETHER_ADDR_LEN);
	memcpy(enm->enm_addrhi, addrhi, ETHER_ADDR_LEN);
	enm->enm_refcount = 1;
	LIST_INSERT_HEAD(&ac->ac_multiaddrs, enm, enm_list);
	ac->ac_multicnt++;
	if (memcmp(addrlo, addrhi, ETHER_ADDR_LEN) != 0)
		ac->ac_multirangecnt++;
	splx(s);
	/*
	 * Return ENETRESET to inform the driver that the list has changed
	 * and its reception filter should be adjusted accordingly.
	 */
	return (ENETRESET);
}

/*
 * Delete a multicast address record.
 */
int
ether_delmulti(struct ifreq *ifr, struct arpcom *ac)
{
	struct ether_multi *enm;
	u_char addrlo[ETHER_ADDR_LEN];
	u_char addrhi[ETHER_ADDR_LEN];
	int s = splnet(), error;

	error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
	if (error != 0) {
		splx(s);
		return (error);
	}

	/*
	 * Look up the address in our list.
	 */
	ETHER_LOOKUP_MULTI(addrlo, addrhi, ac, enm);
	if (enm == NULL) {
		splx(s);
		return (ENXIO);
	}
	if (--enm->enm_refcount != 0) {
		/*
		 * Still some claims to this record.
		 */
		splx(s);
		return (0);
	}
	/*
	 * No remaining claims to this record; unlink and free it.
	 */
	LIST_REMOVE(enm, enm_list);
	free(enm, M_IFMADDR, sizeof *enm);
	ac->ac_multicnt--;
	if (memcmp(addrlo, addrhi, ETHER_ADDR_LEN) != 0)
		ac->ac_multirangecnt--;
	splx(s);
	/*
	 * Return ENETRESET to inform the driver that the list has changed
	 * and its reception filter should be adjusted accordingly.
	 */
	return (ENETRESET);
}
