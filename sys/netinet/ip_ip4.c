/*	$OpenBSD: ip_ip4.c,v 1.47 2000/01/13 06:02:31 angelos Exp $	*/

/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and 
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece, 
 * in November 1995.
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
 *	
 * Permission to use, copy, and modify this software without fee
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


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_ecn.h>

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#include <sys/socketvar.h>
#include <net/raw_cb.h>

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ip4.h>
#include <dev/rndvar.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

/*
 * We can control the acceptance of IP4 packets by altering the sysctl
 * net.inet.ip4.allow value.  Zero means drop them, all else is acceptance.
 */
int ip4_allow = 0;

struct ip4stat ip4stat;

#ifdef INET6
/*
 * Really only a wrapper for ip4_input(), for use with IPv6.
 */
int
ip4_input6(struct mbuf **m, int *offp, int proto)
{
    ip4_input(*m, *offp);
    return IPPROTO_DONE;
}
#endif /* INET6 */

/*
 * ip4_input gets called when we receive an IPv4 encapsulated packet,
 * either because we got it at a real interface, or because AH or ESP
 * were being used in tunnel mode (in which case the rcvif element will 
 * contain the address of the encX interface associated with the tunnel.
 */

void
#if __STDC__
ip4_input(struct mbuf *m, ...)
#else
ip4_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
    register struct sockaddr_in *sin;
    register struct ifnet *ifp;
    register struct ifaddr *ifa;
    struct ifqueue *ifq = NULL;
    struct ip *ipo;
#ifdef INET6
    register struct sockaddr_in6 *sin6;
    struct ip6_hdr *ip6 = NULL;
    u_int8_t itos;
#endif
    u_int8_t nxt;
    int s, iphlen;
    va_list ap;
    int isr;
    u_int8_t otos;
    u_int8_t v;
    int hlen;

    va_start(ap, m);
    iphlen = va_arg(ap, int);
    va_end(ap);

    ip4stat.ip4s_ipackets++;

    /* Bring the IP(v4) header in the first mbuf, if not there already */
    if (m->m_len < sizeof(struct ip))
    {
	if ((m = m_pullup(m, sizeof(struct ip))) == 0)
	{
	    DPRINTF(("ip4_input(): m_pullup() failed\n"));
	    ip4stat.ip4s_hdrops++;
	    m_freem(m);
	    return;
	}
    }

    ipo = mtod(m, struct ip *);

#ifdef MROUTING
    if (ipo->ip_v == IPVERSION && ipo->ip_p == IPPROTO_IPV4)
    {
	if (IN_MULTICAST(((struct ip *)((char *)ipo + iphlen))->ip_dst.s_addr))
	{
	    ipip_input (m, iphlen);
	    return;
	}
    }
#endif MROUTING

    /* keep outer ecn field */
    otos = ipo->ip_tos;

    /* If we do not accept IP4 explicitly, drop.  */
    if (!ip4_allow && (m->m_flags & (M_AUTH|M_CONF)) == 0)
    {
	DPRINTF(("ip4_input(): dropped due to policy\n"));
	ip4stat.ip4s_pdrops++;
	m_freem(m);
	return;
    }

    /* Remove outter IP header */
    m_adj(m, iphlen);

    m_copydata(m, 0, 1, &v);
    if ((v >> 4) == 4)
	hlen = sizeof(struct ip);
#ifdef INET6
    else if ((v >> 4) == 6)
	hlen = sizeof(struct ip6_hdr);
#endif
    else {
	m_freem(m);
	return /*EAFNOSUPPORT*/;
    }

    /* Bring the inner IP(v4) header in the first mbuf, if not there already */
    if (m->m_len < hlen)
    {
	if ((m = m_pullup(m, hlen)) == 0)
	{
	    DPRINTF(("ip4_input(): m_pullup() failed\n"));
	    ip4stat.ip4s_hdrops++;
	    return;
	}
    }

    ipo = mtod(m, struct ip *);
    v = ipo->ip_v;

    /*
     * RFC 1853 specifies that the inner TTL should not be touched on
     * decapsulation. There's no reason this comment should be here, but
     * this is as good as any a position.
     */

    /* Some sanity checks in the inner IPv4 header */
    switch (v) {
    case IPVERSION:
	nxt = ipo->ip_p;
	break;
#ifdef INET6
    case 6:
	ip6 = (struct ip6_hdr *)ipo;
	ipo = NULL;
	nxt = ip6->ip6_nxt;
	break;
#endif
    default:
	DPRINTF(("ip4_input(): wrong version %d on packet from %s to %s (%s->%s)\n", ipo->ip_v, inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst)));
	ip4stat.ip4s_family++;
	m_freem(m);
	return;
    }

    /*
     * If we do not accept IP4 other than as part of ESP & AH, we should
     * not accept a packet with double ip4 headers neither.
     */
 
    if (!ip4_allow && ((nxt == IPPROTO_IPIP) || (nxt == IPPROTO_IPV6)))
    {
	DPRINTF(("ip4_input(): dropped due to policy\n"));
	ip4stat.ip4s_pdrops++;
	m_freem(m);
	return;
    }

    /* update inner ecn field. */
    switch (v) {
    case IPVERSION:
	ip_ecn_egress(ECN_ALLOWED, &otos, &ipo->ip_tos);
	break;
#ifdef INET6
    case 6:
	itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
	ip_ecn_egress(ECN_ALLOWED, &otos, &itos);
	ip6->ip6_flow &= ~htonl(0xff << 20);
	ip6->ip6_flow |= htonl((u_int32_t)itos << 20);
	break;
#endif
    }

    /* Check for local address spoofing. */
    if (m->m_pkthdr.rcvif == NULL ||
	!(m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK))
    {
        for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
	{
	    for (ifa = ifp->if_addrlist.tqh_first;
		 ifa != 0;
		 ifa = ifa->ifa_list.tqe_next)
	    {
		if (ipo)
		{
		    if (ifa->ifa_addr->sa_family != AF_INET)
		      continue;

		    sin = (struct sockaddr_in *) ifa->ifa_addr;

		    if (sin->sin_addr.s_addr == ipo->ip_src.s_addr)
		    {
			DPRINTF(("ip4_input(): possible local address spoofing detected on packet from %s to %s (%s->%s)\n", inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst)));
			ip4stat.ip4s_spoof++;
			m_freem(m);
			return;
		    }
		}
#ifdef INET6
		else if (ip6)
		{
		    if (ifa->ifa_addr->sa_family != AF_INET6)
		      continue;

		    sin6 = (struct sockaddr_in6 *) ifa->ifa_addr;

		    if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr, &ip6->ip6_src))
		    {
			DPRINTF(("ip4_input(): possible local address spoofing detected on packet\n"));
			m_freem(m);
			return;
		    }

		}
#endif
	    }
	}
    }
    
    /* Statistics */
    if (ipo)
      ip4stat.ip4s_ibytes += m->m_pkthdr.len - iphlen;

    /* tdbi is only set in ESP or AH, if the next protocol is UDP or TCP */
    if (m->m_flags & (M_CONF|M_AUTH))
      m->m_pkthdr.tdbi = NULL;

    /*
     * Interface pointer stays the same; if no IPsec processing has
     * been done (or will be done), this will point to a normal 
     * interface. Otherwise, it'll point to an enc interface, which
     * will allow a packet filter to distinguish between secure and
     * untrusted packets.
     */

    if (ipo)
    {
	ifq = &ipintrq;
	isr = NETISR_IP;
    }
#ifdef INET6
    else if (ip6)
    {
	ifq = &ip6intrq;
	isr = NETISR_IPV6;
    }
#endif
    else
    {
	/* just in case */
	m_freem(m);
	return;
    }

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	m_freem(m);
	if (ipo)
	    ip4stat.ip4s_qfull++;

	splx(s);

	DPRINTF(("ip4_input(): packet dropped because of full queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, m);
    schednetisr(isr);
    splx(s);

    return;
}

int
ipe4_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
	    int protoff)
{
    u_int8_t tp, otos;

#ifdef INET
    u_int8_t itos;
    struct ip *ipo;
#endif /* INET */

#ifdef INET6    
    struct ip6_hdr *ip6o;
#endif /* INET6 */

    /* Deal with empty TDB source/destination addresses */
    /* XXX */

    m_copydata(m, 0, 1, &tp);
    tp = (tp >> 4) & 0xff;  /* Get the IP version number */

    switch (tdb->tdb_dst.sa.sa_family)
    {
#ifdef INET
	case AF_INET:
	    if ((tdb->tdb_src.sa.sa_family != AF_INET) ||
		(tdb->tdb_src.sin.sin_addr.s_addr == INADDR_ANY) ||
		(tdb->tdb_dst.sin.sin_addr.s_addr == INADDR_ANY))
	    {
		DPRINTF(("ipe4_output(): unspecified tunnel endpoind address in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ip4stat.ip4s_unspec++;
		m_freem(m);
		*mp = NULL;
		return EINVAL;
	    }

	    M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	    if (m == 0)
	    {
		DPRINTF(("ipe4_output(): M_PREPEND failed\n"));
		ip4stat.ip4s_hdrops++;
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

	    /* If the inner protocol is IP */
	    if (tp == IPVERSION)
	    {
		/* Save ECN notification */
		m_copydata(m, sizeof(struct ip) + offsetof(struct ip, ip_tos),
			   sizeof(u_int8_t), (caddr_t) &itos);

		ipo->ip_p = IPPROTO_IPIP;

		/*
		 * We should be keeping tunnel soft-state and send back ICMPs
		 * if needed.
		 */
		m_copydata(m, sizeof(struct ip) + offsetof(struct ip, ip_off),
			   sizeof(u_int16_t), (caddr_t) &ipo->ip_off);
		ipo->ip_off &= ~(IP_DF | IP_MF | IP_OFFMASK);
	    }
#ifdef INET6
	    else if (tp == (IPV6_VERSION >> 4))
	    {
		u_int32_t itos32;
		/* Save ECN notification */
		m_copydata(m, sizeof(struct ip) +
			   offsetof(struct ip6_hdr, ip6_flow),
			   sizeof(u_int32_t), (caddr_t) &itos32);
		itos = ntohl(itos32) >> 20;

		ipo->ip_p = IPPROTO_IPV6;
		ipo->ip_off = 0;
	    }
#endif /* INET6 */
	    else
	    {
		m_freem(m);
		*mp = NULL;
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
		(tdb->tdb_src.sa.sa_family != AF_INET6) ||
		IN6_IS_ADDR_UNSPECIFIED(&tdb->tdb_src.sin6.sin6_addr))
	    {
		DPRINTF(("ipe4_output(): unspecified tunnel endpoind address in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ip4stat.ip4s_unspec++;
		m_freem(m);
		*mp = NULL;
		return ENOBUFS;
	    }

	    M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
	    if (m == 0)
	    {
		DPRINTF(("ipe4_output(): M_PREPEND failed\n"));
		ip4stat.ip4s_hdrops++;
		*mp = NULL;
		return ENOBUFS;
	    }

	    /* Initialize IPv6 header */
	    ip6o = mtod(m, struct ip6_hdr *);
	    ip6o->ip6_flow = 0;
	    ip6o->ip6_vfc &= ~IPV6_VERSION_MASK;
	    ip6o->ip6_vfc |= IPV6_VERSION;
	    ip6o->ip6_plen = htons(m->m_pkthdr.len);
	    ip6o->ip6_hlim = ip_defttl;
	    ip6o->ip6_dst = tdb->tdb_dst.sin6.sin6_addr;
	    ip6o->ip6_src = tdb->tdb_src.sin6.sin6_addr;

#ifdef INET
	    if (tp == IPVERSION)
	    {
		/* Save ECN notification */
		m_copydata(m, sizeof(struct ip6_hdr) +
			   offsetof(struct ip, ip_tos), sizeof(u_int8_t),
			   (caddr_t) &itos);

		ip6o->ip6_nxt = IPPROTO_IPIP; /* This is really IPVERSION */
	    }
	    else
#endif /* INET */
	    if (tp == (IPV6_VERSION >> 4))
	    {
		u_int32_t itos32;
		/* Save ECN notification */
		m_copydata(m, sizeof(struct ip6_hdr) +
			   offsetof(struct ip6_hdr, ip6_flow),
			   sizeof(u_int32_t), (caddr_t) &itos32);
		itos = ntohl(itos32) >> 20;

		ip6o->ip6_nxt = IPPROTO_IPV6;
	    }
	    else
	    {
		m_freem(m);
		*mp = NULL;
		return EAFNOSUPPORT;
	    }

	    otos = 0;
	    ip_ecn_ingress(ECN_ALLOWED, &otos, &itos);
	    ip6o->ip6_flow |= htonl((u_int32_t) otos << 20);
	    break;
#endif /* INET6 */

	default:
	    DPRINTF(("ipe4_output(): unsupported protocol family %d\n",
		     tdb->tdb_dst.sa.sa_family));
	    m_freem(m);
	    *mp = NULL;
	    ip4stat.ip4s_family++;
	    return ENOBUFS;
    }

    ip4stat.ip4s_opackets++;

    *mp = m;

#ifdef INET
    if (tdb->tdb_dst.sa.sa_family == AF_INET)
    {
	if (tdb->tdb_xform->xf_type == XF_IP4)
	  tdb->tdb_cur_bytes += m->m_pkthdr.len - sizeof(struct ip);

	ip4stat.ip4s_obytes += m->m_pkthdr.len - sizeof(struct ip);
    }
#endif /* INET */

#ifdef INET6
    if (tdb->tdb_dst.sa.sa_family == AF_INET6)
    {
	if (tdb->tdb_xform->xf_type == XF_IP4)
	  tdb->tdb_cur_bytes += m->m_pkthdr.len - sizeof(struct ip6_hdr);

	ip4stat.ip4s_obytes += m->m_pkthdr.len - sizeof(struct ip6_hdr);
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
    /* This is a rather serious mistake, so no conditional printing */
    printf("ipe4_input(): should never be called\n");
    if (m)
      m_freem(m);
}
#endif	/* IPSEC */

int
ip4_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IP4CTL_ALLOW:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &ip4_allow));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
