/*	$OpenBSD: ip_ip4.c,v 1.28 1999/04/09 23:28:45 niklas Exp $	*/

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
 * net.inet.ip4.allow value.  Zero means drop them, all ilse is acceptance.
 */
int ip4_allow = 0;
struct ip4stat ip4stat;

/*
 * ip4_input gets called when we receive an encapsulated packet,
 * either because we got it at a real interface, or because AH or ESP
 * were being used in tunnel mode (in which case the rcvif element will 
 * contain the address of the encapX interface associated with the tunnel.
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
    register struct ifnet *ifp;
    register struct ifaddr *ifa;
    register struct sockaddr_in *sin;
    int iphlen;
    struct ip *ipo, *ipi;
    struct ifqueue *ifq = NULL;
    int s;
    va_list ap;

    va_start(ap, m);
    iphlen = va_arg(ap, int);
    va_end(ap);

    ip4stat.ip4s_ipackets++;

    /* If we do not accept IP4 explicitly, drop.  */
    if (!ip4_allow && (m->m_flags & (M_AUTH|M_CONF)) == 0)
    {
	DPRINTF(("ip4_input(): dropped due to policy\n"));
	ip4stat.ip4s_pdrops++;
	m_freem(m);
	return;
    }

    /*
     * Strip IP options, if any.
     */
    if (iphlen > sizeof(struct ip))
    {
	ip_stripoptions(m, (struct mbuf *) 0);
	iphlen = sizeof(struct ip);
    }
	
    /*
     * Make sure next IP header is in the first mbuf.
     *
     * Careful here! we are receiving the packet from ipintr;
     * this means that the ip_len field has been adjusted to
     * not count the ip header, and is also in host order.
     */

    ipo = mtod(m, struct ip *);

    if (m->m_len < iphlen + sizeof(struct ip))
    {
	if ((m = m_pullup(m, iphlen + sizeof(struct ip))) == 0)
	{
	    DPRINTF(("ip4_input(): m_pullup() failed\n"));
	    ip4stat.ip4s_hdrops++;
	    m_freem(m);
	    return;
	}

	ipo = mtod(m, struct ip *);
    }

    ipi = (struct ip *) ((caddr_t) ipo + iphlen);

    /*
     * RFC 1853 specifies that the inner TTL should not be touched on
     * decapsulation. There's no reason this comment should be here, but
     * this is as good as any a position.
     */

    if (ipi->ip_v != IPVERSION)
    {
	DPRINTF(("ip4_input(): wrong version %d on packet from %s to %s (%s->%s)\n", ipi->ip_v, inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipi->ip_src), inet_ntoa4(ipi->ip_dst)));
	ip4stat.ip4s_notip4++;
	m_freem(m);
	return;
    }

     /*
      * If we do not accept IP4 other than part of ESP & AH, we should
      * not accept a packet with double ip4 headers neither.
      */
 
     if (!ip4_allow && ipi->ip_p == IPPROTO_IPIP)
     {
 	DPRINTF(("ip4_input(): dropped due to policy\n"));
 	ip4stat.ip4s_pdrops++;
 	m_freem(m);
 	return;
     }
 
    /*
     * Check for local address spoofing.
     */
    for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
      for (ifa = ifp->if_addrlist.tqh_first;
	   ifa != 0;
	   ifa = ifa->ifa_list.tqe_next)
      {
	  if (ifa->ifa_addr->sa_family != AF_INET)
	    continue;

	  sin = (struct sockaddr_in *) ifa->ifa_addr;

	  if (sin->sin_addr.s_addr == ipi->ip_src.s_addr)
	  {
	      DPRINTF(("ip_input(): possible local address spoofing detected on packet from %s to %s (%s->%s)\n", inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipi->ip_src), inet_ntoa4(ipi->ip_dst)));
 	      ip4stat.ip4s_spoof++;
	      m_freem(m);
	      return;
	  }
      }
    
    /* Statistics */
    ip4stat.ip4s_ibytes += ntohs(ipi->ip_len);

    /*
     * Interface pointer is already in first mbuf; chop off the 
     * `outer' header and reschedule.
     */

    m->m_len -= iphlen;
    m->m_pkthdr.len -= iphlen;
    m->m_data += iphlen;

    /* tdbi is only set in esp or ah, if next protocol is udp or tcp */
    if (m->m_flags & (M_CONF|M_AUTH))
	m->m_pkthdr.tdbi = NULL;

    /*
     * Interface pointer stays the same; if no IPsec processing has
     * been done (or will be done), this will point to a normal 
     * interface. Otherwise, it'll point to an encap interface, which
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

int
ipe4_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb, 
	    struct mbuf **mp)
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

    /* Check that the destination address is AF_INET */
    if (tdb->tdb_src.sa.sa_family != AF_INET)
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

    /*
     * XXX We should be keeping tunnel soft-state and send back ICMPs
     * if needed
     */
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
