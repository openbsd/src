/*	$OpenBSD: ip_ip4.c,v 1.37 1999/12/09 04:00:07 angelos Exp $	*/

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

/*
 * We can control the acceptance of IP4 packets by altering the sysctl
 * net.inet.ip4.allow value.  Zero means drop them, all else is acceptance.
 */
int ip4_allow = 0;

struct ip4stat ip4stat;

/*
 * ip4_input gets called when we receive an encapsulated packet,
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
    int s, iphlen;
    va_list ap;

    va_start(ap, m);
    iphlen = va_arg(ap, int);
    va_end(ap);

    ip4stat.ip4s_ipackets++;

#ifdef MROUTING
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

    if (ipo->ip_v == IPVERSION)
    {
	if (IN_MULTICAST(((struct ip *)((char *)ipo + iphlen))->ip_dst.s_addr))
	{
	    ipip_input (m, iphlen);
	    return;
	}
    }
#endif MROUTING

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

    /* Bring the inner IP(v4) header in the first mbuf, if not there already */
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

    /*
     * RFC 1853 specifies that the inner TTL should not be touched on
     * decapsulation. There's no reason this comment should be here, but
     * this is as good as any a position.
     */

    /* Some sanity checks in the inner IPv4 header */
    if (ipo->ip_v != IPVERSION)
    {
	DPRINTF(("ip4_input(): wrong version %d on packet from %s to %s (%s->%s)\n", ipo->ip_v, inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst)));
	ip4stat.ip4s_notip4++;
	m_freem(m);
	return;
    }

    /*
     * If we do not accept IP4 other than as part of ESP & AH, we should
     * not accept a packet with double ip4 headers neither.
     */
 
    if (!ip4_allow && ((ipo->ip_p == IPPROTO_IPIP) ||
	 (ipo->ip_p == IPPROTO_IPV6)))
    {
	DPRINTF(("ip4_input(): dropped due to policy\n"));
	ip4stat.ip4s_pdrops++;
	m_freem(m);
	return;
    }

    /* Check for local address spoofing. */
    if (m->m_pkthdr.rcvif == NULL ||
	!(m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK))
    {
        for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
	  for (ifa = ifp->if_addrlist.tqh_first;
	       ifa != 0;
	       ifa = ifa->ifa_list.tqe_next)
	  {
	      if (ifa->ifa_addr->sa_family != AF_INET)
		continue;

	      sin = (struct sockaddr_in *) ifa->ifa_addr;

	      if (sin->sin_addr.s_addr == ipo->ip_src.s_addr)
	      {
		  DPRINTF(("ip_input(): possible local address spoofing detected on packet from %s to %s (%s->%s)\n", inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst)));
		  ip4stat.ip4s_spoof++;
		  m_freem(m);
		  return;
	      }
	  }
    }
    
    /* Statistics */
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

    ifq = &ipintrq;

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	m_freem(m);
	ip4stat.ip4s_qfull++;
	splx(s);

	DPRINTF(("ip4_input(): packet dropped because of full queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, m);
    schednetisr(NETISR_IP);
    splx(s);

    return;
}

#ifdef IPSEC
int
ipe4_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
	    int protoff)
{
    struct ip *ipo, *ipi;
    ushort ilen;

    /* Check that the source address, if present, is from AF_INET */
    if ((tdb->tdb_src.sa.sa_family != 0) &&
	(tdb->tdb_src.sa.sa_family != AF_INET))
    {
	DPRINTF(("ipe4_output(): IP in protocol-family <%d> attempted, aborting", tdb->tdb_src.sa.sa_family));
	m_freem(m);
	return EINVAL;
    }

    /* Check that the destination address are AF_INET */
    if (tdb->tdb_dst.sa.sa_family != AF_INET)
    {
	DPRINTF(("ipe4_output(): IP in protocol-family <%d> attempted, aborting", tdb->tdb_dst.sa.sa_family));
	m_freem(m);
	return EINVAL;
    }

    ip4stat.ip4s_opackets++;
    ipi = mtod(m, struct ip *);
    ilen = ntohs(ipi->ip_len);

    M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
    if (m == 0)
    {
	DPRINTF(("ipe4_output(): M_PREPEND failed\n"));
      	return ENOBUFS;
    }

    ipo = mtod(m, struct ip *);

    ipo->ip_v = IPVERSION;
    ipo->ip_hl = 5;
    ipo->ip_tos = ipi->ip_tos;
    ipo->ip_len = htons(ilen + sizeof(struct ip));
    ipo->ip_ttl = ip_defttl;
    ipo->ip_p = IPPROTO_IPIP;
    ipo->ip_id = ip_randomid();
    HTONS(ipo->ip_id);

    /* We should be keeping tunnel soft-state and send back ICMPs if needed. */
    ipo->ip_off = ipi->ip_off & ~(IP_DF | IP_MF | IP_OFFMASK);

    ipo->ip_sum = 0;

    ipo->ip_src = tdb->tdb_src.sin.sin_addr;
    ipo->ip_dst = tdb->tdb_dst.sin.sin_addr;

    *mp = m;

    /* Update the counters */
    if (tdb->tdb_xform->xf_type == XF_IP4)
      tdb->tdb_cur_bytes += ntohs(ipo->ip_len) - (ipo->ip_hl << 2);

    ip4stat.ip4s_obytes += ntohs(ipo->ip_len) - (ipo->ip_hl << 2);
    return 0;
}

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
