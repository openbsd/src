/*	$OpenBSD: ip_esp.c,v 1.29 1999/12/09 03:46:03 angelos Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

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

#ifdef INET6
#include <netinet6/in6.h>
#include <netinet6/ip6.h>
#endif/ * INET6 */

#include <sys/socketvar.h>
#include <net/raw_cb.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>

#include <net/if_enc.h>

#include "bpfilter.h"

extern struct enc_softc encif[];

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

int esp_enable = 0;

/*
 * esp_common_input() gets called when we receive an ESP-protected packet
 * in IPv4 or IPv6.
 */

static void
esp_common_input(struct mbuf *m, int skip, int protoff, int af)
{
    union sockaddr_union sunion;
    struct ifqueue *ifq = NULL;
    struct tdb *tdbp;
    u_int32_t spi;
    u_int8_t prot;
    int s;

#ifdef INET
    struct ip *ip, ipn;
#endif /* INET */

#ifdef INET6
    struct ip6_hdr *ip6, ip6n;
#endif /* INET6 */

    espstat.esps_input++;

    if (!esp_enable)
    {
        m_freem(m);
        espstat.esps_pdrops++;
        return;
    }

    /* Retrieve the SPI from the ESP header */
    m_copydata(m, skip , sizeof(u_int32_t), (unsigned char *) &spi);

    /*
     * Find tunnel control block and (indirectly) call the appropriate
     * kernel crypto routine. The resulting mbuf chain is a valid
     * IP packet ready to go through input processing.
     */

    bzero(&sunion, sizeof(sunion));
    sunion.sin.sin_family = af;

#ifdef INET
    if (af == AF_INET)
    {
	sunion.sin.sin_len = sizeof(struct sockaddr_in);
	m_copydata(m, offsetof(struct ip, ip_dst), sizeof(struct in_addr),
		   (unsigned char *) &(sunion.sin.sin_addr));
    }
#endif /* INET */

#ifdef INET6
    if (af == AF_INET6)
    {
	sunion.sin6.sin6_len = sizeof(struct sockaddr_in6);
	m_copydata(m, offsetof(struct ip6_hdr, ip6_dst),
		   sizeof(struct in6_addr),
		   (unsigned char *) &(sunion.sin6.sin6_addr));
    }
#endif /* INET6 */

    s = spltdb();
    tdbp = gettdb(spi, &sunion, IPPROTO_ESP);
    if (tdbp == NULL)
    {
	DPRINTF(("esp_input(): could not find SA for packet to %s, spi %08x\n", ipsp_address(sunion), ntohl(spi)));
	m_freem(m);
	espstat.esps_notdb++;
	return;
    }
	
    if (tdbp->tdb_flags & TDBF_INVALID)
    {
	DPRINTF(("esp_input(): attempted to use invalid SA %s/%08x\n",
		 ipsp_address(sunion), ntohl(spi)));
	m_freem(m);
	espstat.esps_invalid++;
	return;
    }

    if (tdbp->tdb_xform == NULL)
    {
	DPRINTF(("esp_input(): attempted to use uninitialized SA %s/%08x\n",
		 ipsp_address(sunion), ntohl(spi)));
	m_freem(m);
	espstat.esps_noxform++;
	return;
    }

    if (tdbp->tdb_interface)
      m->m_pkthdr.rcvif = (struct ifnet *) tdbp->tdb_interface;
    else
      m->m_pkthdr.rcvif = &encif[0].sc_if;

    /* Register first use, setup expiration timer */
    if (tdbp->tdb_first_use == 0)
    {
	tdbp->tdb_first_use = time.tv_sec;
	tdb_expiration(tdbp, TDBEXP_TIMEOUT);
    }

    m = (*(tdbp->tdb_xform->xf_input))(m, tdbp, skip, protoff);
    if (m == NULL)
    {
	/* esp_xxx_input() will print a message if necessary */
	espstat.esps_badkcr++;
	return;
    }

#ifdef INET
    /* Fix IPv4 header */
    if (af == AF_INET)
    {
        if ((m = m_pullup(m, skip)) == 0)
        {
	    DPRINTF(("esp_input(): processing failed for SA %s/%08x\n",
		     ipsp_address(tdbp->tdb_dst), ntohl(spi)));
            espstat.esps_hdrops++;
            return;
        }

	ip = mtod(m, struct ip *);
	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
	prot = ip->ip_p;

	/* IP-in-IP encapsulation */
	if (prot == IPPROTO_IPIP)
	{
	    /* ipn will now contain the inner IPv4 header */
	    m_copydata(m, ip->ip_hl << 2, sizeof(struct ip), (caddr_t) &ipn);

	    /*
	     * Check that the inner source address is the same as
	     * the proxy address, if available.
	     */
	    if (((tdbp->tdb_proxy.sa.sa_family == AF_INET) &&
		 (tdbp->tdb_proxy.sin.sin_addr.s_addr != INADDR_ANY) &&
		 (ipn.ip_src.s_addr != tdbp->tdb_proxy.sin.sin_addr.s_addr)) ||
		((tdbp->tdb_proxy.sa.sa_family != AF_INET6) &&
		 (tdbp->tdb_proxy.sa.sa_family != 0)))
	    {
		DPRINTF(("esp_input(): inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", inet_ntoa4(ipn.ip_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		m_free(m);
		espstat.esps_hdrops++;
		return;
	    }
	}

#if INET6
	/* IPv6-in-IP encapsulation */
	if (prot == IPPROTO_IPV6)
	{
	    /* ip6n will now contain the inner IPv6 header */
	    m_copydata(m, ip->ip_hl << 2, sizeof(struct ip6_hdr),
		       (caddr_t) &ip6n);

	    /*
	     * Check that the inner source address is the same as
	     * the proxy address, if available.
	     */
	    if (((tdbp->tdb_proxy.sa.sa_family == AF_INET6) &&
		 !IN6_IS_ADDR_UNSPECIFIED(&tdbp->tdb_proxy.sin6.sin6_addr) &&
		 !IN6_ARE_ADDR_EQUAL(&ip6n.ip6_src,
				     &tdbp->tdb_proxy.sin6.sin6_addr)) ||
		((tdbp->tdb_proxy.sa.sa_family != AF_INET6) &&
		 (tdbp->tdb_proxy.sa.sa_family != 0)))
	    {
		DPRINTF(("esp_input(): inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", inet6_ntoa4(ip6n.ip6_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		m_free(m);
		espstat.esps_hdrops++;
		return;
	    }
	}
#endif /* INET6 */

	/* 
	 * Check that the source address is an expected one, if we know what
	 * it's supposed to be. This avoids source address spoofing.
	 */
	if (((tdbp->tdb_src.sa.sa_family == AF_INET) &&
	     (tdbp->tdb_src.sin.sin_addr.s_addr != INADDR_ANY) &&
	     (ip->ip_src.s_addr != tdbp->tdb_src.sin.sin_addr.s_addr)) ||
	    ((tdbp->tdb_src.sa.sa_family != AF_INET) &&
	     (tdbp->tdb_src.sa.sa_family != 0)))
	{
	    DPRINTF(("esp_input(): source address %s doesn't correspond to expected source %s, SA %s/%08x\n", inet_ntoa4(ip->ip_src), ipsp_address(tdbp->tdb_src), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
	    m_free(m);
	    espstat.esps_hdrops++;
	    return;
	}
    }
#endif /* INET */

#ifdef INET6
    /* Fix IPv6 header */
    if (af == INET6)
    {
        if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == 0)
        {
	    DPRINTF(("esp_input(): processing failed for SA %s/%08x\n",
		     ipsp_address(tdbp->tdb_dst), ntohl(spi)));
            espstat.esps_hdrops++;
            return;
        }

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons(m->m_pkthdr.len);

	/* Save protocol */
	m_copydata(m, protoff, 1, (unsigned char *) &prot);

#ifdef INET
	/* IP-in-IP encapsulation */
	if (prot == IPPROTO_IPIP)
	{
	    /* ipn will now contain the inner IPv4 header */
	    m_copydata(m, skip, sizeof(struct ip), (caddr_t) &ipn);

	    /*
	     * Check that the inner source address is the same as
	     * the proxy address, if available.
	     */
	    if (((tdbp->tdb_proxy.sa.sa_family == AF_INET) &&
		 (tdbp->tdb_proxy.sin.sin_addr.s_addr != INADDR_ANY) &&
		 (ipn.ip_src.s_addr != tdbp->tdb_proxy.sin.sin_addr.s_addr)) ||
		((tdbp->tdb_proxy.sa.sa_family != AF_INET) &&
		 (tdbp->tdb_proxy.sa.sa_family != 0)))
	    {
		DPRINTF(("esp_input(): inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", inet_ntoa4(ipn.ip_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		m_free(m);
		espstat.esps_hdrops++;
		return;
	    }
	}
#endif /* INET */

	/* IPv6-in-IP encapsulation */
	if (prot == IPPROTO_IPV6)
	{
	    /* ip6n will now contain the inner IPv6 header */
	    m_copydata(m, skip, sizeof(struct ip6_hdr), (caddr_t) &ip6n);

	    /*
	     * Check that the inner source address is the same as
	     * the proxy address, if available.
	     */
	    if (((tdbp->tdb_proxy.sa.sa_family == AF_INET6) &&
		 !IN6_IS_ADDR_UNSPECIFIED(&tdbp->tdb_proxy.sin6.sin6_addr) &&
		 !IN6_ARE_ADDR_EQUAL(&ip6n.ip6_src,
				     &tdbp->tdb_proxy.sin6.sin6_addr)) ||
		((tdbp->tdb_proxy.sa.sa_family != AF_INET6) &&
		 (tdbp->tdb_proxy.sa.sa_family != 0)))
	    {
		DPRINTF(("esp_input(): inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", inet6_ntoa4(ip6n.ip6_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		m_free(m);
		espstat.esps_hdrops++;
		return;
	    }
	}

	/* 
	 * Check that the source address is an expected one, if we know what
	 * it's supposed to be. This avoids source address spoofing.
	 */
	if (((tdbp->tdb_src.sa.sa_family == AF_INET6) &&
	     !IN6_IS_ADDR_UNSPECIFIED(&tdbp->tdb_src.sin6.sin6_addr) &&
	     !IN6_ARE_ADDR_EQUAL(&ip6->ip6_src,
				 &tdbp->tdb_src.sin6.sin6_addr)) ||
	    ((tdbp->tdb_src.sa.sa_family != AF_INET6) &&
	     (tdbp->tdb_src.sa.sa_family != 0)))
	{
	    DPRINTF(("esp_input(): source address %s doesn't correspond to expected source %s, SA %s/%08x\n", inet6_ntoa4(ip6->ip6_src), ipsp_address(tdbp->tdb_src), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
	    m_free(m);
	    espstat.esps_hdrops++;
	    return;
	}
    }
#endif /* INET6 */

    if (prot == IPPROTO_TCP || prot == IPPROTO_UDP)
    {
	struct tdb_ident *tdbi = NULL;

	if (tdbp->tdb_bind_out)
	{
	    tdbi = m->m_pkthdr.tdbi;
	    if (!(m->m_flags & M_PKTHDR))
	      DPRINTF(("esp_input(): mbuf is not a packet header!\n"));

	    MALLOC(tdbi, struct tdb_ident *, sizeof(struct tdb_ident),
	           M_TEMP, M_NOWAIT);

	    if (tdbi == NULL)
	      m->m_pkthdr.tdbi = NULL;
	    else
	    {
		tdbi->spi = tdbp->tdb_bind_out->tdb_spi;
		tdbi->dst = tdbp->tdb_bind_out->tdb_dst;
		tdbi->proto = tdbp->tdb_bind_out->tdb_sproto;
	    }
	}
    }
    else
      m->m_pkthdr.tdbi = NULL;

    /* Packet is confidental */
    m->m_flags |= M_CONF;

#if NBPFILTER > 0
    if (m->m_pkthdr.rcvif->if_bpf) 
    {
        /*
         * We need to prepend the address family as
         * a four byte field.  Cons up a dummy header
         * to pacify bpf.  This is safe because bpf
         * will only read from the mbuf (i.e., it won't
         * try to free it or keep a pointer a to it).
         */
        struct mbuf m0;
        struct enchdr hdr;

	hdr.af = af;
	hdr.spi = tdbp->tdb_spi;
	hdr.flags = m->m_flags & (M_AUTH|M_CONF);

        m0.m_next = m;
        m0.m_len = ENC_HDRLEN;
        m0.m_data = (char *) &hdr;
        
        bpf_mtap(m->m_pkthdr.rcvif->if_bpf, &m0);
    }
#endif
    splx(s);

    /*
     * Interface pointer is already in first mbuf; chop off the 
     * `outer' header and reschedule.
     */

#ifdef INET
    if (af == AF_INET)
      ifq = &ipintrq;
#endif /* INET */

#ifdef INET6
    if (af == AF_INET6)
      ifq = &ip6intrq;
#endif /* INET6 */

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	if (m->m_pkthdr.tdbi)
	  free(m->m_pkthdr.tdbi, M_TEMP);
	m_freem(m);
	espstat.esps_qfull++;
	splx(s);
	DPRINTF(("esp_input(): dropped packet because of full IP queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, m);

#ifdef INET
    if (af == AF_INET)
      schednetisr(NETISR_IP);
#endif /* INET */

#ifdef INET6
    if (af == AF_INET6)
      schednetisr(NETISR_IPV6);
#endif /* INET6 */

    splx(s);
    return;
}

int
esp_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
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
	case ESPCTL_ENABLE:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &esp_enable));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}

#ifdef INET
/* IPv4 ESP wrapper */
void
esp_input(struct mbuf *m, ...)
{
    int skip;

    va_list ap;
    va_start(ap, m);
    skip = va_arg(ap, int);
    va_end(ap);

    esp_common_input(m, skip, offsetof(struct ip, ip_p), AF_INET);
}
#endif /* INET */

#ifdef INET6
/* IPv6 ESP wrapper */
void
esp6_input(struct mbuf *m, ...)
{
    int skip, protoff;

    va_list ap;
	
    va_start(ap, m);
    skip = va_arg(ap, int);
    protoff = va_arg(ap, int);
    va_end(ap);

    esp_common_input(m, skip, protoff, AF_INET6);
}
#endif /* INET6 */
