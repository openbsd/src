/*	$OpenBSD: ipsec_input.c,v 1.16 2000/01/15 20:03:05 angelos Exp $	*/

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
 * Authentication Header Processing
 * Per RFC1826 (Atkinson, 1995)
 *
 * Encapsulation Security Payload Processing
 * Per RFC1827 (Atkinson, 1995)
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
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#ifdef INET6
#include <netinet6/in6.h>
#include <netinet6/ip6.h>
#endif/ * INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ah.h>

#include <net/if_enc.h>

#include "bpfilter.h"

int ipsec_common_input(struct mbuf **, int, int, int, int);

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

/* sysctl variables */
int esp_enable = 0;
int ah_enable = 0;

/*
 * ipsec_common_input() gets called when we receive an IPsec-protected packet
 * in IPv4 or IPv6.
 */

int
ipsec_common_input(struct mbuf **m0, int skip, int protoff, int af, int sproto)
{
#define IPSEC_ISTAT(y,z) (sproto == IPPROTO_ESP ? (y)++ : (z)++)
#define IPSEC_NAME (sproto == IPPROTO_ESP ? (af == AF_INET ? "esp_input()" :\
					                     "esp6_input()") :\
                                            (af == AF_INET ? "ah_input()" :\
                                                             "ah6_input()"))
    union sockaddr_union src_address, dst_address, src2, dst2;
    caddr_t sport = 0, dport = 0;
    struct flow *flow;
    struct tdb *tdbp;
    struct mbuf *m;
    u_int32_t spi;
    u_int8_t prot;
    int s;

#ifdef INET
    struct ip *ip, ipn;
#endif /* INET */

#ifdef INET6
    struct ip6_hdr *ip6, ip6n;
#endif /* INET6 */

    IPSEC_ISTAT(espstat.esps_input, ahstat.ahs_input);

    if (m0 == 0)
    {
	DPRINTF(("%s: NULL packet received\n"));
        IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops);
        return EINVAL;
    }
    else
      m = *m0;

    if ((sproto == IPPROTO_ESP && !esp_enable) ||
	(sproto == IPPROTO_AH && !ah_enable))
    {
        m_freem(m);
	*m0 = NULL;
        IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
        return EOPNOTSUPP;
    }

    /* Retrieve the SPI from the relevant IPsec header */
    if (sproto == IPPROTO_ESP)
      m_copydata(m, skip, sizeof(u_int32_t), (caddr_t) &spi);
    else
      m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		 (caddr_t) &spi);

    /*
     * Find tunnel control block and (indirectly) call the appropriate
     * kernel crypto routine. The resulting mbuf chain is a valid
     * IP packet ready to go through input processing.
     */

    bzero(&dst_address, sizeof(dst_address));
    dst_address.sa.sa_family = af;

    switch (af)
    {
#ifdef INET
	case AF_INET:
	    dst_address.sin.sin_len = sizeof(struct sockaddr_in);
	    sport = (caddr_t) &src_address.sin.sin_port;
	    dport = (caddr_t) &dst_address.sin.sin_port;
	    m_copydata(m, offsetof(struct ip, ip_dst), sizeof(struct in_addr),
		       (caddr_t) &(dst_address.sin.sin_addr));
	    break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
	    dst_address.sin6.sin6_len = sizeof(struct sockaddr_in6);
	    sport = (caddr_t) &src_address.sin6.sin6_port;
	    dport = (caddr_t) &dst_address.sin6.sin6_port;
	    m_copydata(m, offsetof(struct ip6_hdr, ip6_dst),
		       sizeof(struct in6_addr),
		       (caddr_t) &(dst_address.sin6.sin6_addr));
	    break;
#endif /* INET6 */

	default:
	    DPRINTF(("%s: unsupported protocol family %d\n", IPSEC_NAME, af));
	    m_freem(m);
	    *m0 = NULL;
	    IPSEC_ISTAT(espstat.esps_nopf, ahstat.ahs_nopf);
	    return EPFNOSUPPORT;
    }

    s = spltdb();
    tdbp = gettdb(spi, &dst_address, sproto);
    if (tdbp == NULL)
    {
	splx(s);
	DPRINTF(("%s: could not find SA for packet to %s, spi %08x\n",
		 IPSEC_NAME, ipsp_address(dst_address), ntohl(spi)));
	m_freem(m);
	*m0 = NULL;
	IPSEC_ISTAT(espstat.esps_notdb, ahstat.ahs_notdb);
	return ENOENT;
    }
	
    if (tdbp->tdb_flags & TDBF_INVALID)
    {
	splx(s);
	DPRINTF(("%s: attempted to use invalid SA %s/%08x\n",
		 IPSEC_NAME, ipsp_address(dst_address), ntohl(spi)));
	m_freem(m);
	*m0 = NULL;
	IPSEC_ISTAT(espstat.esps_invalid, ahstat.ahs_invalid);
	return EINVAL;
    }

    if (tdbp->tdb_xform == NULL)
    {
	splx(s);
	DPRINTF(("%s: attempted to use uninitialized SA %s/%08x\n",
		 IPSEC_NAME, ipsp_address(dst_address), ntohl(spi)));
	m_freem(m);
	*m0 = NULL;
	IPSEC_ISTAT(espstat.esps_noxform, ahstat.ahs_noxform);
	return ENXIO;
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

    /* If we do ingress filtering and the list is empty, quick drop */
    if (ipsec_acl && (tdbp->tdb_access == NULL))
    {
	DPRINTF(("%s: packet from %s dropped due to empty policy list, SA %s/%08x\n", IPSEC_NAME, ipsp_address(src_address), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
	splx(s);
	m_freem(m);
	*m0 = NULL;
	IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
	return EACCES;
    }

    m = (*(tdbp->tdb_xform->xf_input))(m, tdbp, skip, protoff);
    if (m == NULL)
    {
	/* The called routine will print a message if necessary */
	splx(s);
	IPSEC_ISTAT(espstat.esps_badkcr, ahstat.ahs_badkcr);
	return EINVAL;
    }

#ifdef INET
    /* Fix IPv4 header */
    if (af == AF_INET)
    {
        if ((m = m_pullup(m, skip)) == 0)
        {
	    DPRINTF(("%s: processing failed for SA %s/%08x\n",
		     IPSEC_NAME, ipsp_address(tdbp->tdb_dst), ntohl(spi)));
	    splx(s);
            IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops);
	    *m0 = NULL;
            return ENOMEM;
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
		((tdbp->tdb_proxy.sa.sa_family != AF_INET) &&
		 (tdbp->tdb_proxy.sa.sa_family != 0)))
	    {
		DPRINTF(("%s: inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", IPSEC_NAME, inet_ntoa4(ipn.ip_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		splx(s);
		m_freem(m);
	    	*m0 = NULL;
                IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
		return EACCES;
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
		DPRINTF(("%s: inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", IPSEC_NAME, inet6_ntoa4(ip6n.ip6_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		splx(s);
		m_freem(m);
	    	*m0 = NULL;
		IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
		return EACCES;
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
	    DPRINTF(("%s: source address %s doesn't correspond to expected source %s, SA %s/%08x\n", IPSEC_NAME, inet_ntoa4(ip->ip_src), ipsp_address(tdbp->tdb_src), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
	    splx(s);
	    m_freem(m);
	    IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
	    *m0 = NULL;
	    return EACCES;
	}
    }
#endif /* INET */

#ifdef INET6
    /* Fix IPv6 header */
    if (af == INET6)
    {
        if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == 0)
        {
	    DPRINTF(("%s: processing failed for SA %s/%08x\n",
		     IPSEC_NAME, ipsp_address(tdbp->tdb_dst), ntohl(spi)));
	    splx(s);
            IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops);
	    *m0 = NULL;
            return ENOMEM;
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
		DPRINTF(("%s: inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", IPSEC_NAME, inet_ntoa4(ipn.ip_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		splx(s);
		m_freem(m);
		IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
	    	*m0 = NULL;
		return EACCES;
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
		DPRINTF(("%s: inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", IPSEC_NAME, inet6_ntoa4(ip6n.ip6_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		splx(s);
		m_freem(m);
	    	*m0 = NULL;
		IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
		return EACCES;
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
	    DPRINTF(("%s: packet %s to %s does not match any ACL entries, SA %s/%08x\n", IPSEC_NAME, ipsp_address(src_address), ipsp_address(dst_address), ipsp_address(tdbp->tdb_src), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
	    splx(s);
	    m_freem(m);
	    *m0 = NULL;
	    IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
	    return EACCES;
	}
    }
#endif /* INET6 */

    /* Access control */
    if (ipsec_acl)
    {
	bzero(&src_address, sizeof(dst_address));
	src_address.sa.sa_family = af;
	src_address.sa.sa_len = dst_address.sa.sa_len;

#ifdef INET
	if (af == AF_INET)
	  m_copydata(m, offsetof(struct ip, ip_src), sizeof(struct in_addr),
		     (caddr_t) &(src_address.sin.sin_addr));
#endif /* INET */

#ifdef INET6
	if (af == AF_INET6)
	  m_copydata(m, offsetof(struct ip6_hdr, ip6_src),
		     sizeof(struct in6_addr),
		     (caddr_t) &(src_address.sin6.sin6_addr));
#endif /* INET6 */

	/* Save transport layer source/destination ports, if any */
	switch (prot)
	{
	    case IPPROTO_TCP:
		m_copydata(m, skip + offsetof(struct tcphdr, th_sport),
			   sizeof(u_int16_t), (caddr_t) sport);
		m_copydata(m, skip + offsetof(struct tcphdr, th_dport),
			   sizeof(u_int16_t), (caddr_t) dport);
		break;

	    case IPPROTO_UDP:
		m_copydata(m, skip + offsetof(struct udphdr, uh_sport),
			   sizeof(u_int16_t), (caddr_t) sport);
		m_copydata(m, skip + offsetof(struct udphdr, uh_dport),
			   sizeof(u_int16_t), (caddr_t) dport);
		break;

	    default:
		/* Nothing needed */
	}

	for (flow = tdbp->tdb_access; flow; flow = flow->flow_next)
	{
	    /* Match for address family */
	    if (flow->flow_src.sa.sa_family != af)
	      continue;

	    /* Match for transport protocol */
	    if (flow->flow_proto && flow->flow_proto != prot)
	      continue;

	    /* Netmask handling */
	    rt_maskedcopy(&src_address.sa, &src2.sa, &flow->flow_srcmask.sa);
	    rt_maskedcopy(&dst_address.sa, &dst2.sa, &flow->flow_dstmask.sa);

	    /* Check addresses */
	    if (bcmp(&src2, &flow->flow_src, src2.sa.sa_len) ||
		bcmp(&dst2, &flow->flow_dst, dst2.sa.sa_len))
	      continue;

	    break; /* success! */
	}

	if (flow == NULL)
	{
	    /* Failed to match any entry in the ACL */
		DPRINTF(("%s: packet from %s to %s dropped due to policy, SA %s/%08x\n", IPSEC_NAME, ipsp_address(src_address), ipsp_address(dst_address), ipsp_address(tdbp->tdb_dst), ntohl(spi)));
		splx(s);
		m_freem(m);
	    	*m0 = NULL;
		IPSEC_ISTAT(espstat.esps_pdrops, ahstat.ahs_pdrops);
		return EACCES;
	}
    }

    if (prot == IPPROTO_TCP || prot == IPPROTO_UDP)
    {
	if (tdbp->tdb_bind_out)
	{
	    if (!(m->m_flags & M_PKTHDR))
	      DPRINTF(("%s: mbuf is not a packet header!\n", IPSEC_NAME));

	    MALLOC(m->m_pkthdr.tdbi, struct tdb_ident *,
		   sizeof(struct tdb_ident), M_TEMP, M_NOWAIT);

	    if (m->m_pkthdr.tdbi == NULL)
	    {
		((struct tdb_ident *) m->m_pkthdr.tdbi)->spi =
						tdbp->tdb_bind_out->tdb_spi;
		((struct tdb_ident *) m->m_pkthdr.tdbi)->dst =
						tdbp->tdb_bind_out->tdb_dst;
		((struct tdb_ident *) m->m_pkthdr.tdbi)->proto =
						tdbp->tdb_bind_out->tdb_sproto;
	    }
	}
	else
	  m->m_pkthdr.tdbi = NULL;
    }
    else
      m->m_pkthdr.tdbi = NULL;

    if (sproto == IPPROTO_ESP)
    {
	/* Packet is confidental */
	m->m_flags |= M_CONF;

	/* Check if we had authenticated ESP */
	if (tdbp->tdb_authalgxform)
	  m->m_flags |= M_AUTH;
    }
    else
      m->m_flags |= M_AUTH;

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
        struct mbuf m1;
        struct enchdr hdr;

	hdr.af = af;
	hdr.spi = tdbp->tdb_spi;
	hdr.flags = m->m_flags & (M_AUTH|M_CONF);

        m1.m_next = m;
        m1.m_len = ENC_HDRLEN;
        m1.m_data = (char *) &hdr;
        
        bpf_mtap(m->m_pkthdr.rcvif->if_bpf, &m1);
    }
#endif
    splx(s);

    *m0 = m;
    return 0;
#undef IPSEC_NAME
#undef IPSEC_ISTAT
}

int
esp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlen, void *newp,
	   size_t newlen)
{
    /* All sysctl names at this level are terminal. */
    if (namelen != 1)
      return ENOTDIR;

    switch (name[0])
    {
	case ESPCTL_ENABLE:
	    return sysctl_int(oldp, oldlen, newp, newlen, &esp_enable);
	default:
	    return ENOPROTOOPT;
    }
    /* NOTREACHED */
}

int
ah_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlen, void *newp,
	  size_t newlen)
{
    /* All sysctl names at this level are terminal. */
    if (namelen != 1)
      return ENOTDIR;

    switch (name[0])
    {
	case AHCTL_ENABLE:
	    return sysctl_int(oldp, oldlen, newp, newlen, &ah_enable);
	default:
	    return ENOPROTOOPT;
    }
    /* NOTREACHED */
}

#ifdef INET
/* IPv4 AH wrapper */
void
ah_input(struct mbuf *m, ...)
{
    struct ifqueue *ifq = &ipintrq;
    struct mbuf *mp = m;
    int skip, s;

    va_list ap;
    va_start(ap, m);
    skip = va_arg(ap, int);
    va_end(ap);

    if (ipsec_common_input(&mp, skip, offsetof(struct ip, ip_p), AF_INET,
			   IPPROTO_AH) != 0)
      return;

    /*
     * Interface pointer is already in first mbuf; chop off the 
     * `outer' header and reschedule.
     */

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	if (mp->m_pkthdr.tdbi)
	  free(mp->m_pkthdr.tdbi, M_TEMP);
	m_freem(mp);
	ahstat.ahs_qfull++;
	splx(s);
	DPRINTF(("ah_input(): dropped packet because of full IP queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, mp);
    schednetisr(NETISR_IP);
    splx(s);
}

/* IPv4 ESP wrapper */
void
esp_input(struct mbuf *m, ...)
{
    struct ifqueue *ifq = &ipintrq;
    struct mbuf *mp = m;
    int skip, s;

    va_list ap;
    va_start(ap, m);
    skip = va_arg(ap, int);
    va_end(ap);

    if (ipsec_common_input(&mp, skip, offsetof(struct ip, ip_p), AF_INET,
			   IPPROTO_ESP) != 0)
      return;

    /*
     * Interface pointer is already in first mbuf; chop off the 
     * `outer' header and reschedule.
     */

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	if (mp->m_pkthdr.tdbi)
	  free(mp->m_pkthdr.tdbi, M_TEMP);
	m_freem(mp);
	espstat.esps_qfull++;
	splx(s);
	DPRINTF(("esp_input(): dropped packet because of full IP queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, mp);
    schednetisr(NETISR_IP);
    splx(s);
}
#endif /* INET */

#ifdef INET6
/* IPv6 AH wrapper */
int
ah6_input(struct mbuf **mp, int *offp, int proto)
{
    struct mbuf *m = *mp;
    u_int8_t nxt = 0;
    int protoff;

    if (*offp == sizeof(struct ip6_hdr))
      protoff = offsetof(struct ip6_hdr, ip6_nxt);
    else
    {
	/* Chase the header chain... */

	protoff = sizeof(struct ip6_hdr);

	do
	{
	    protoff += nxt;
	    m_copydata(m, protoff + offsetof(struct ip6_ext, ip6e_len),
		       sizeof(u_int8_t), (caddr_t) &nxt);
	    nxt = (nxt + 1) * 8;
	} while (protoff + nxt < *offp);

	/* Malformed packet check */
	if (protoff + nxt != *offp)
	{
	    DPRINTF(("ah6_input(): bad packet header chain\n"));
	    ahstat.ahs_hdrops++;
	    m_freem(m);
	    *mp = NULL;
	    return IPPROTO_DONE;
	}

	protoff += offsetof(struct ip6_ext, ip6e_nxt);
    }

    if (ipsec_common_input(&m, *offp, protoff, AF_INET6, proto) != 0)
    {
	*mp = NULL;
	return IPPROTO_DONE;
    }

    /* Retrieve new protocol */
    m_copydata(m, protoff, sizeof(u_int8_t), (caddr_t) &nxt);
    return nxt;
}

/* IPv6 ESP wrapper */
int
esp6_input(struct mbuf **mp, int *offp, int proto)
{
    struct mbuf *m = *mp;
    u_int8_t nxt = 0;
    int protoff;

    if (*offp == sizeof(struct ip6_hdr))
      protoff = offsetof(struct ip6_hdr, ip6_nxt);
    else
    {
	/* Chase the header chain... */

	protoff = sizeof(struct ip6_hdr);

	do
	{
	    protoff += nxt;
	    m_copydata(m, protoff + offsetof(struct ip6_ext, ip6e_len),
		       sizeof(u_int8_t), (caddr_t) &nxt);
	    nxt = (nxt + 1) * 8;
	} while (protoff + nxt < *offp);

	/* Malformed packet check */
	if (protoff + nxt != *offp)
	{
	    DPRINTF(("esp6_input(): bad packet header chain\n"));
	    espstat.esps_hdrops++;
	    m_freem(m);
	    *mp = NULL;
	    return IPPROTO_DONE;
	}

	protoff += offsetof(struct ip6_ext, ip6e_nxt);
    }

    protoff = offsetof(struct ip6_hdr, ip6_nxt);
    if (ipsec_common_input(&m, *offp, protoff, AF_INET6, proto) != 0)
    {
	*mp = NULL;
	return IPPROTO_DONE;
    }

    /* Retrieve new protocol */
    m_copydata(m, protoff, sizeof(u_int8_t), (caddr_t) &nxt);
    return nxt;
}
#endif /* INET6 */
