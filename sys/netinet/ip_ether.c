/*	$OpenBSD: ip_ether.c,v 1.1 1999/10/28 03:08:34 angelos Exp $  */

/*
 * The author of this code is Angelos D. Keromytis (kermit@adk.gr)
 *
 * This code was written by Angelos D. Keromytis for OpenBSD in October 1999.
 *
 * Copyright (C) 1999 by Angelos D. Keromytis.
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
 * Ethernet-inside-IP processing
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
#include <netinet/ip_ether.h>
#include <netinet/if_ether.h>
#include <dev/rndvar.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

/*
 * We can control the acceptance of EtherIP packets by altering the sysctl
 * net.inet.etherip.allow value. Zero means drop them, all else is acceptance.
 */
int etherip_allow = 0;

struct etheripstat etheripstat;

/*
 * etherip_input gets called when we receive an encapsulated packet,
 * either because we got it at a real interface, or because AH or ESP
 * were being used in tunnel mode (in which case the rcvif element will 
 * contain the address of the encapX interface associated with the tunnel.
 */

void
#if __STDC__
etherip_input(struct mbuf *m, ...)
#else
etherip_input(m, va_alist)
struct mbuf *m;
va_dcl
#endif
{
    struct ifqueue *ifq = NULL;
    int iphlen, s;
    va_list ap;

    va_start(ap, m);
    iphlen = va_arg(ap, int);
    va_end(ap);

    etheripstat.etherip_ipackets++;

    /* If we do not accept EtherIP explicitly, drop. */
    if (!etherip_allow && (m->m_flags & (M_AUTH|M_CONF)) == 0)
    {
	DPRINTF(("etherip_input(): dropped due to policy\n"));
	etheripstat.etherip_pdrops++;
	m_freem(m);
	return;
    }

    /*
     * Remove the outer IP header, make sure there's at least an
     * ethernet header's worth of data in there. 
     */

    if (m->m_pkthdr.len < iphlen + sizeof(struct ether_header))
    {
	DPRINTF(("etherip_input(): encapsulated packet too short\n"));
	etheripstat.etherip_hdrops++;
	m_freem(m);
	return;
    }

    /* Make sure the ethernet header at least is in the first mbuf. */
    if (m->m_len < iphlen + sizeof(struct ether_header))
    {
	if ((m = m_pullup(m, iphlen + sizeof(struct ether_header))) == 0)
	{
	    DPRINTF(("etherip_input(): m_pullup() failed\n"));
	    etheripstat.etherip_adrops++;
	    m_freem(m);
	    return;
	}
    }

    /*
     * Interface pointer is already in first mbuf; chop off the 
     * `outer' header and reschedule.
     */

    m->m_len -= iphlen;
    m->m_pkthdr.len -= iphlen;
    m->m_data += iphlen;

    /* Statistics */
    etheripstat.etherip_ibytes += m->m_pkthdr.len;


    /* tdbi is only set in ESP or ESP, if next protocol is udp or tcp */
    if (m->m_flags & (M_CONF|M_AUTH))
	m->m_pkthdr.tdbi = NULL;

    /*
     * Interface pointer stays the same; if no IPsec processing has
     * been done (or will be done), this will point to a normal 
     * interface. Otherwise, it'll point to an encap interface, which
     * will allow a packet filter to distinguish between secure and
     * untrusted packets.
     */

    /* XXX Queue on the right bridge interface. */
    ifq = &ipintrq;

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	m_freem(m);
	etheripstat.etherip_qfull++;
	splx(s);

	DPRINTF(("etherip_input(): packet dropped because of full queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, m);
    schednetisr(NETISR_IP);
    splx(s);
    return;
}

#ifdef IPSEC
int
etherip_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb, 
	       struct mbuf **mp)
{
    struct ip *ipo;
    ushort ilen;

    /* Check that the destination address is AF_INET */
    if (tdb->tdb_dst.sa.sa_family != AF_INET)
    {
	DPRINTF(("etherip_output(): IP in protocol-family <%d> attempted, aborting", tdb->tdb_dst.sa.sa_family));
	etheripstat.etherip_adrops++;
	m_freem(m);
	return EINVAL;
    }

    etheripstat.etherip_opackets++;
    ilen = m->m_pkthdr.len;

    M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
    if (m == 0)
    {
	DPRINTF(("etherip_output(): M_PREPEND failed\n"));
	etheripstat.etherip_adrops++;
      	return ENOBUFS;
    }

    ipo = mtod(m, struct ip *);
	
    ipo->ip_v = IPVERSION;
    ipo->ip_hl = 5;
    ipo->ip_len = htons(ilen + sizeof(struct ip));
    ipo->ip_ttl = ip_defttl;
    ipo->ip_p = IPPROTO_ETHERIP;
    ipo->ip_tos = 0;
    ipo->ip_off = 0;
    ipo->ip_sum = 0;
    ipo->ip_id = ip_randomid();
    HTONS(ipo->ip_id);

    /*
     * We should be keeping tunnel soft-state and send back ICMPs if needed.
     */

    ipo->ip_src = tdb->tdb_src.sin.sin_addr;
    ipo->ip_dst = tdb->tdb_dst.sin.sin_addr;

    *mp = m;

    etheripstat.etherip_obytes += ntohs(ipo->ip_len) - (ipo->ip_hl << 2);
    return 0;
}
#endif /* IPSEC */

int
etherip_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
	       void *newp, size_t newlen)
{
    /* All sysctl names at this level are terminal. */
    if (namelen != 1)
      return (ENOTDIR);

    switch (name[0]) 
    {
	case ETHERIPCTL_ALLOW:
	    return (sysctl_int(oldp, oldlenp, newp, newlen, &etherip_allow));
	default:
	    return (ENOPROTOOPT);
    }
    /* NOTREACHED */
}
