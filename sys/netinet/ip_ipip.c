/*	$OpenBSD: ip_ipip.c,v 1.47 2010/05/11 09:36:07 claudio Exp $ */
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 * IP-inside-IP processing
 */

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_ecn.h>

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ipip.h>

#include "bpfilter.h"

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

/*
 * We can control the acceptance of IP4 packets by altering the sysctl
 * net.inet.ipip.allow value.  Zero means drop them, all else is acceptance.
 */
int ipip_allow = 0;

struct ipipstat ipipstat;

#ifdef INET6
/*
 * Really only a wrapper for ipip_input(), for use with IPv6.
 */
int
ip4_input6(struct mbuf **m, int *offp, int proto)
{
	/* If we do not accept IP-in-IP explicitly, drop.  */
	if (!ipip_allow && ((*m)->m_flags & (M_AUTH|M_CONF)) == 0) {
		DPRINTF(("ip4_input6(): dropped due to policy\n"));
		ipipstat.ipips_pdrops++;
		m_freem(*m);
		return IPPROTO_DONE;
	}

	ipip_input(*m, *offp, NULL, proto);
	return IPPROTO_DONE;
}
#endif /* INET6 */

#ifdef INET
/*
 * Really only a wrapper for ipip_input(), for use with IPv4.
 */
void
ip4_input(struct mbuf *m, ...)
{
	struct ip *ip;
	va_list ap;
	int iphlen;

	/* If we do not accept IP-in-IP explicitly, drop.  */
	if (!ipip_allow && (m->m_flags & (M_AUTH|M_CONF)) == 0) {
		DPRINTF(("ip4_input(): dropped due to policy\n"));
		ipipstat.ipips_pdrops++;
		m_freem(m);
		return;
	}

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	ip = mtod(m, struct ip *);

	ipip_input(m, iphlen, NULL, ip->ip_p);
}
#endif /* INET */

/*
 * ipip_input gets called when we receive an IP{46} encapsulated packet,
 * either because we got it at a real interface, or because AH or ESP
 * were being used in tunnel mode (in which case the rcvif element will
 * contain the address of the encX interface associated with the tunnel.
 */

void
ipip_input(struct mbuf *m, int iphlen, struct ifnet *gifp, int proto)
{
	struct sockaddr_in *sin;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifqueue *ifq = NULL;
	struct ip *ipo;
	u_int rdomain;
#ifdef INET6
	struct sockaddr_in6 *sin6;
	struct ip6_hdr *ip6;
	u_int8_t itos;
#endif
	int isr;
	int hlen, s;
	u_int8_t otos;
	u_int8_t v;
	sa_family_t af;

	ipipstat.ipips_ipackets++;

	m_copydata(m, 0, 1, &v);

	switch (v >> 4) {
#ifdef INET
	case 4:
		hlen = sizeof(struct ip);
		break;
#endif /* INET */
#ifdef INET6
	case 6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif
	default:
		ipipstat.ipips_family++;
		m_freem(m);
		return /* EAFNOSUPPORT */;
	}

	/* Bring the IP header in the first mbuf, if not there already */
	if (m->m_len < hlen) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			DPRINTF(("ipip_input(): m_pullup() failed\n"));
			ipipstat.ipips_hdrops++;
			return;
		}
	}


	/* Keep outer ecn field. */
	switch (v >> 4) {
#ifdef INET
	case 4:
		ipo = mtod(m, struct ip *);
		otos = ipo->ip_tos;
		break;
#endif /* INET */
#ifdef INET6
	case 6:
		ip6 = mtod(m, struct ip6_hdr *);
		otos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		break;
#endif
	default:
		panic("ipip_input: should never reach here");
	}

	/* Remove outer IP header */
	m_adj(m, iphlen);

	/* Sanity check */
	if (m->m_pkthdr.len < sizeof(struct ip)) {
		ipipstat.ipips_hdrops++;
		m_freem(m);
		return;
	}

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
		hlen = sizeof(struct ip);
		break;
#endif /* INET */

#ifdef INET6
	case IPPROTO_IPV6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif
	default:
		ipipstat.ipips_family++;
		m_freem(m);
		return; /* EAFNOSUPPORT */
	}

	/*
	 * Bring the inner header into the first mbuf, if not there already.
	 */
	if (m->m_len < hlen) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			DPRINTF(("ipip_input(): m_pullup() failed\n"));
			ipipstat.ipips_hdrops++;
			return;
		}
	}

	/*
	 * RFC 1853 specifies that the inner TTL should not be touched on
	 * decapsulation. There's no reason this comment should be here, but
	 * this is as good as any a position.
	 */

	/* Some sanity checks in the inner IP header */
	switch (proto) {
#ifdef INET
    	case IPPROTO_IPV4:
		ipo = mtod(m, struct ip *);
#ifdef INET6
		ip6 = NULL;
#endif
		if (!ip_ecn_egress(ECN_ALLOWED, &otos, &ipo->ip_tos)) {
			m_freem(m);
			return;
		}
		break;
#endif /* INET */
#ifdef INET6
    	case IPPROTO_IPV6:
#ifdef INET
		ipo = NULL;
#endif
		ip6 = mtod(m, struct ip6_hdr *);
		itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		if (!ip_ecn_egress(ECN_ALLOWED, &otos, &itos)) {
			m_freem(m);
			return;
		}
		ip6->ip6_flow &= ~htonl(0xff << 20);
		ip6->ip6_flow |= htonl((u_int32_t) itos << 20);
		break;
#endif
	default:
#ifdef INET
		ipo = NULL;
#endif
#ifdef INET6
		ip6 = NULL;
#endif
	}

	/* Check for local address spoofing. */
	if ((m->m_pkthdr.rcvif == NULL ||
	    !(m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK)) &&
	    ipip_allow != 2) {
		rdomain = rtable_l2(m->m_pkthdr.rdomain);
		TAILQ_FOREACH(ifp, &ifnet, if_list) {
			if (ifp->if_rdomain != rdomain)
				continue;
			TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
#ifdef INET
				if (ipo) {
					if (ifa->ifa_addr->sa_family !=
					    AF_INET)
						continue;

					sin = (struct sockaddr_in *)
					    ifa->ifa_addr;
					if (sin->sin_addr.s_addr ==
					    ipo->ip_src.s_addr)	{
						ipipstat.ipips_spoof++;
						m_freem(m);
						return;
					}
				}
#endif /* INET */
#ifdef INET6
				if (ip6) {
					if (ifa->ifa_addr->sa_family !=
					    AF_INET6)
						continue;

					sin6 = (struct sockaddr_in6 *)
					    ifa->ifa_addr;
					if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &ip6->ip6_src)) {
						ipipstat.ipips_spoof++;
						m_freem(m);
						return;
					}

				}
#endif /* INET6 */
			}
		}
	}

	/* Statistics */
	ipipstat.ipips_ibytes += m->m_pkthdr.len - iphlen;

	/*
	 * Interface pointer stays the same; if no IPsec processing has
	 * been done (or will be done), this will point to a normal
	 * interface. Otherwise, it'll point to an enc interface, which
	 * will allow a packet filter to distinguish between secure and
	 * untrusted packets.
	 */

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
		ifq = &ipintrq;
		isr = NETISR_IP;
		af = AF_INET;
		break;
#endif
#ifdef INET6
	case IPPROTO_IPV6:
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
		af = AF_INET6;
		break;
#endif
	default:
		panic("ipip_input: should never reach here");
	}

#if NBPFILTER > 0
	if (gifp && gifp->if_bpf)
		bpf_mtap_af(gifp->if_bpf, af, m, BPF_DIRECTION_IN);
#endif
#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	s = splnet();			/* isn't it already? */
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		ipipstat.ipips_qfull++;

		splx(s);

		DPRINTF(("ipip_input(): packet dropped because of full "
		    "queue\n"));
		return;
	}

	IF_ENQUEUE(ifq, m);
	schednetisr(isr);
	splx(s);
	return;
}

int
ipip_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int dummy,
    int dummy2)
{
	u_int8_t tp, otos;

#ifdef INET
	u_int8_t itos;
	struct ip *ipo;
#endif /* INET */

#ifdef INET6
	struct ip6_hdr *ip6, *ip6o;
#endif /* INET6 */

	/* XXX Deal with empty TDB source/destination addresses. */

	m_copydata(m, 0, 1, &tp);
	tp = (tp >> 4) & 0xff;  /* Get the IP version number. */

	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		if (tdb->tdb_src.sa.sa_family != AF_INET ||
		    tdb->tdb_src.sin.sin_addr.s_addr == INADDR_ANY ||
		    tdb->tdb_dst.sin.sin_addr.s_addr == INADDR_ANY) {

			DPRINTF(("ipip_output(): unspecified tunnel endpoind "
			    "address in SA %s/%08x\n",
			    ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));

			ipipstat.ipips_unspec++;
			m_freem(m);
			*mp = NULL;
			return EINVAL;
		}

		M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
		if (m == 0) {
			DPRINTF(("ipip_output(): M_PREPEND failed\n"));
			ipipstat.ipips_hdrops++;
			*mp = NULL;
			return ENOBUFS;
		}

		ipo = mtod(m, struct ip *);

		ipo->ip_v = IPVERSION;
		ipo->ip_hl = 5;
		ipo->ip_len = htons(m->m_pkthdr.len);
		ipo->ip_ttl = ip_defttl;
		ipo->ip_sum = 0;
		ipo->ip_src = tdb->tdb_src.sin.sin_addr;
		ipo->ip_dst = tdb->tdb_dst.sin.sin_addr;

		/*
		 * We do the htons() to prevent snoopers from determining our
		 * endianness.
		 */
		ipo->ip_id = htons(ip_randomid());

		/* If the inner protocol is IP... */
		if (tp == IPVERSION) {
			/* Save ECN notification */
			m_copydata(m, sizeof(struct ip) +
			    offsetof(struct ip, ip_tos),
			    sizeof(u_int8_t), (caddr_t) &itos);

			ipo->ip_p = IPPROTO_IPIP;

			/*
			 * We should be keeping tunnel soft-state and
			 * send back ICMPs if needed.
			 */
			m_copydata(m, sizeof(struct ip) +
			    offsetof(struct ip, ip_off),
			    sizeof(u_int16_t), (caddr_t) &ipo->ip_off);
			NTOHS(ipo->ip_off);
			ipo->ip_off &= ~(IP_DF | IP_MF | IP_OFFMASK);
			HTONS(ipo->ip_off);
		}
#ifdef INET6
		else if (tp == (IPV6_VERSION >> 4)) {
			u_int32_t itos32;

			/* Save ECN notification. */
			m_copydata(m, sizeof(struct ip) +
			    offsetof(struct ip6_hdr, ip6_flow),
			    sizeof(u_int32_t), (caddr_t) &itos32);
			itos = ntohl(itos32) >> 20;
			ipo->ip_p = IPPROTO_IPV6;
			ipo->ip_off = 0;
		}
#endif /* INET6 */
		else {
			m_freem(m);
			*mp = NULL;
			ipipstat.ipips_family++;
			return EAFNOSUPPORT;
		}

		otos = 0;
		ip_ecn_ingress(ECN_ALLOWED, &otos, &itos);
		ipo->ip_tos = otos;
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&tdb->tdb_dst.sin6.sin6_addr) ||
		    tdb->tdb_src.sa.sa_family != AF_INET6 ||
		    IN6_IS_ADDR_UNSPECIFIED(&tdb->tdb_src.sin6.sin6_addr)) {

			DPRINTF(("ipip_output(): unspecified tunnel endpoind "
			    "address in SA %s/%08x\n",
			    ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));

			ipipstat.ipips_unspec++;
			m_freem(m);
			*mp = NULL;
			return ENOBUFS;
		}

		/* If the inner protocol is IPv6, clear link local scope */
		if (tp == (IPV6_VERSION >> 4)) {
			/* scoped address handling */
			ip6 = mtod(m, struct ip6_hdr *);
			if (IN6_IS_SCOPE_EMBED(&ip6->ip6_src))
				ip6->ip6_src.s6_addr16[1] = 0;
			if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst))
				ip6->ip6_dst.s6_addr16[1] = 0;
		}

		M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
		if (m == 0) {
			DPRINTF(("ipip_output(): M_PREPEND failed\n"));
			ipipstat.ipips_hdrops++;
			*mp = NULL;
			return ENOBUFS;
		}

		/* Initialize IPv6 header */
		ip6o = mtod(m, struct ip6_hdr *);
		ip6o->ip6_flow = 0;
		ip6o->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6o->ip6_vfc |= IPV6_VERSION;
		ip6o->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6o));
		ip6o->ip6_hlim = ip_defttl;
		in6_embedscope(&ip6o->ip6_src, &tdb->tdb_src.sin6, NULL, NULL);
		in6_embedscope(&ip6o->ip6_dst, &tdb->tdb_dst.sin6, NULL, NULL);

#ifdef INET
		if (tp == IPVERSION) {
			/* Save ECN notification */
			m_copydata(m, sizeof(struct ip6_hdr) +
			    offsetof(struct ip, ip_tos), sizeof(u_int8_t),
			    (caddr_t) &itos);

			/* This is really IPVERSION. */
			ip6o->ip6_nxt = IPPROTO_IPIP;
		}
		else
#endif /* INET */
			if (tp == (IPV6_VERSION >> 4)) {
				u_int32_t itos32;

				/* Save ECN notification. */
				m_copydata(m, sizeof(struct ip6_hdr) +
				    offsetof(struct ip6_hdr, ip6_flow),
				    sizeof(u_int32_t), (caddr_t) &itos32);
				itos = ntohl(itos32) >> 20;

				ip6o->ip6_nxt = IPPROTO_IPV6;
			} else {
				m_freem(m);
				*mp = NULL;
				ipipstat.ipips_family++;
				return EAFNOSUPPORT;
			}

		otos = 0;
		ip_ecn_ingress(ECN_ALLOWED, &otos, &itos);
		ip6o->ip6_flow |= htonl((u_int32_t) otos << 20);
		break;
#endif /* INET6 */

	default:
		DPRINTF(("ipip_output(): unsupported protocol family %d\n",
		    tdb->tdb_dst.sa.sa_family));
		m_freem(m);
		*mp = NULL;
		ipipstat.ipips_family++;
		return EAFNOSUPPORT;
	}

	ipipstat.ipips_opackets++;
	*mp = m;

#ifdef INET
	if (tdb->tdb_dst.sa.sa_family == AF_INET) {
		if (tdb->tdb_xform->xf_type == XF_IP4)
			tdb->tdb_cur_bytes +=
			    m->m_pkthdr.len - sizeof(struct ip);

		ipipstat.ipips_obytes += m->m_pkthdr.len - sizeof(struct ip);
	}
#endif /* INET */

#ifdef INET6
	if (tdb->tdb_dst.sa.sa_family == AF_INET6) {
		if (tdb->tdb_xform->xf_type == XF_IP4)
			tdb->tdb_cur_bytes +=
			    m->m_pkthdr.len - sizeof(struct ip6_hdr);

		ipipstat.ipips_obytes +=
		    m->m_pkthdr.len - sizeof(struct ip6_hdr);
	}
#endif /* INET6 */

	return 0;
}

#ifdef IPSEC
int
ipe4_attach()
{
	return 0;
}

int
ipe4_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
	tdbp->tdb_xform = xsp;
	return 0;
}

int
ipe4_zeroize(struct tdb *tdbp)
{
	return 0;
}

void
ipe4_input(struct mbuf *m, ...)
{
	/* This is a rather serious mistake, so no conditional printing. */
	printf("ipe4_input(): should never be called\n");
	if (m)
		m_freem(m);
}
#endif	/* IPSEC */

int
ipip_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IPIPCTL_ALLOW:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &ipip_allow));
	case IPIPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &ipipstat, sizeof(ipipstat)));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
