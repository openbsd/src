/*	$OpenBSD: ipsec_output.c,v 1.26 2003/02/19 19:15:13 jason Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * Copyright (c) 2000-2001 Angelos D. Keromytis.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#endif /* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ipcomp.h>
#include <crypto/xform.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

/*
 * Loop over a tdb chain, taking into consideration protocol tunneling. The
 * fourth argument is set if the first encapsulation header is already in
 * place.
 */
int
ipsp_process_packet(struct mbuf *m, struct tdb *tdb, int af, int tunalready)
{
	struct timeval tv;
	int i, off, error;
	struct mbuf *mp;

#ifdef INET
	int setdf = 0;
	struct ip *ip;
#endif /* INET */
#ifdef INET6
	struct ip6_hdr *ip6;
#endif /* INET6 */

	/* Check that the transform is allowed by the administrator. */
	if ((tdb->tdb_sproto == IPPROTO_ESP && !esp_enable) ||
	    (tdb->tdb_sproto == IPPROTO_AH && !ah_enable) ||
	    (tdb->tdb_sproto == IPPROTO_IPCOMP && !ipcomp_enable)) {
		DPRINTF(("ipsp_process_packet(): IPsec outbound packet "
		    "dropped due to policy (check your sysctls)\n"));
		m_freem(m);
		return EHOSTUNREACH;
	}

	/* Sanity check. */
	if (!tdb->tdb_xform) {
		DPRINTF(("ipsp_process_packet(): uninitialized TDB\n"));
		m_freem(m);
		return EHOSTUNREACH;
	}

	/* Check if the SPI is invalid. */
	if (tdb->tdb_flags & TDBF_INVALID) {
		DPRINTF(("ipsp_process_packet(): attempt to use invalid "
		    "SA %s/%08x/%u\n", ipsp_address(tdb->tdb_dst),
		    ntohl(tdb->tdb_spi), tdb->tdb_sproto));
		m_freem(m);
		return ENXIO;
	}

	/* Check that the network protocol is supported */
	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		break;
#endif /* INET6 */

	default:
		DPRINTF(("ipsp_process_packet(): attempt to use "
		    "SA %s/%08x/%u for protocol family %d\n",
		    ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi),
		    tdb->tdb_sproto, tdb->tdb_dst.sa.sa_family));
		m_freem(m);
		return ENXIO;
	}

	/*
	 * Register first use if applicable, setup relevant expiration timer.
	 */
	if (tdb->tdb_first_use == 0) {
		int pri;

		pri = splhigh();
		tdb->tdb_first_use = time.tv_sec;
		splx(pri);

		tv.tv_usec = 0;

		tv.tv_sec = tdb->tdb_first_use + tdb->tdb_exp_first_use;
		if (tdb->tdb_flags & TDBF_FIRSTUSE)
			timeout_add(&tdb->tdb_first_tmo,
			    hzto(&tv));

		tv.tv_sec = tdb->tdb_first_use + tdb->tdb_soft_first_use;
		if (tdb->tdb_flags & TDBF_SOFT_FIRSTUSE)
			timeout_add(&tdb->tdb_sfirst_tmo,
			    hzto(&tv));
	}

	/*
	 * Check for tunneling if we don't have the first header in place.
	 * When doing Ethernet-over-IP, we are handed an already-encapsulated
	 * frame, so we don't need to re-encapsulate.
	 */
	if (tunalready == 0) {
		/*
		 * If the target protocol family is different, we know we'll be
		 * doing tunneling.
		 */
		if (af == tdb->tdb_dst.sa.sa_family) {
#ifdef INET
			if (af == AF_INET)
				i = sizeof(struct ip);
#endif /* INET */

#ifdef INET6
			if (af == AF_INET6)
				i = sizeof(struct ip6_hdr);
#endif /* INET6 */

			/* Bring the network header in the first mbuf. */
			if (m->m_len < i) {
				if ((m = m_pullup(m, i)) == NULL)
					return ENOBUFS;
			}

#ifdef INET
			ip = mtod(m, struct ip *);

			/*
			 * This is not a bridge packet, remember if we
			 * had IP_DF.
			 */
			setdf = ntohs(ip->ip_off) & IP_DF;
#endif /* INET */

#ifdef INET6
			ip6 = mtod(m, struct ip6_hdr *);
#endif /* INET6 */
		}

		/* Do the appropriate encapsulation, if necessary. */
		if ((tdb->tdb_dst.sa.sa_family != af) || /* PF mismatch */
		    (tdb->tdb_flags & TDBF_TUNNELING) || /* Tunneling needed */
		    (tdb->tdb_xform->xf_type == XF_IP4) || /* ditto */
#ifdef INET
		    ((tdb->tdb_dst.sa.sa_family == AF_INET) &&
			(tdb->tdb_dst.sin.sin_addr.s_addr != INADDR_ANY) &&
			(tdb->tdb_dst.sin.sin_addr.s_addr != ip->ip_dst.s_addr)) ||
#endif /* INET */
#ifdef INET6
		    ((tdb->tdb_dst.sa.sa_family == AF_INET6) &&
			(!IN6_IS_ADDR_UNSPECIFIED(&tdb->tdb_dst.sin6.sin6_addr)) &&
			(!IN6_ARE_ADDR_EQUAL(&tdb->tdb_dst.sin6.sin6_addr,
			    &ip6->ip6_dst))) ||
#endif /* INET6 */
		    0) {
#ifdef INET
			/* Fix IPv4 header checksum and length. */
			if (af == AF_INET) {
				if (m->m_len < sizeof(struct ip))
					if ((m = m_pullup(m,
					    sizeof(struct ip))) == NULL)
						return ENOBUFS;

				ip = mtod(m, struct ip *);
				ip->ip_len = htons(m->m_pkthdr.len);
				ip->ip_sum = 0;
				ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
			}
#endif /* INET */

#ifdef INET6
			/* Fix IPv6 header payload length. */
			if (af == AF_INET6) {
				if (m->m_len < sizeof(struct ip6_hdr))
					if ((m = m_pullup(m,
					    sizeof(struct ip6_hdr))) == NULL)
						return ENOBUFS;

				if (m->m_pkthdr.len - sizeof(*ip6) >
				    IPV6_MAXPACKET) {
					/* No jumbogram support. */
					m_freem(m);
					return ENXIO;	/*?*/
				}
				ip6 = mtod(m, struct ip6_hdr *);
				ip6->ip6_plen = htons(m->m_pkthdr.len
				    - sizeof(*ip6));
			}
#endif /* INET6 */

			/* Encapsulate -- the last two arguments are unused. */
			error = ipip_output(m, tdb, &mp, 0, 0);
			if ((mp == NULL) && (!error))
				error = EFAULT;
			if (error) {
				if (mp)	{
					m_freem(mp);
					mp = NULL;
				}
				return error;
			}

			m = mp;
			mp = NULL;

#ifdef INET
			if (tdb->tdb_dst.sa.sa_family == AF_INET && setdf) {
				if (m->m_len < sizeof(struct ip))
					if ((m = m_pullup(m,
					    sizeof(struct ip))) == NULL)
						return ENOBUFS;

				ip = mtod(m, struct ip *);
				NTOHS(ip->ip_off);
				ip->ip_off |= IP_DF;
				HTONS(ip->ip_off);
			}

			/* Remember that we appended a tunnel header. */
			tdb->tdb_flags |= TDBF_USEDTUNNEL;
#endif
		}

		/* We may be done with this TDB */
		if (tdb->tdb_xform->xf_type == XF_IP4)
			return ipsp_process_done(m, tdb);
	} else {
		/*
		 * If this is just an IP-IP TDB and we're told there's
		 * already an encapsulation header, move on.
		 */
		if (tdb->tdb_xform->xf_type == XF_IP4)
			return ipsp_process_done(m, tdb);
	}

	/* Extract some information off the headers. */
	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		ip = mtod(m, struct ip *);
		i = ip->ip_hl << 2;
		off = offsetof(struct ip, ip_p);
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
		i = sizeof(struct ip6_hdr);
		off = offsetof(struct ip6_hdr, ip6_nxt);
		break;
#endif /* INET6 */
	}

	/* Non expansion policy for IPCOMP */
	if (tdb->tdb_sproto == IPPROTO_IPCOMP) {
		if ((m->m_pkthdr.len - i) < tdb->tdb_compalgxform->minlen) {
			extern struct ipcompstat ipcompstat;

			/* No need to compress, leave the packet untouched */
			ipcompstat.ipcomps_minlen++;
			return ipsp_process_done(m, tdb);
		}
	}

	/* Invoke the IPsec transform. */
	return (*(tdb->tdb_xform->xf_output))(m, tdb, NULL, i, off);
}

/*
 * Called by the IPsec output transform callbacks, to transmit the packet
 * or do further processing, as necessary.
 */
int
ipsp_process_done(struct mbuf *m, struct tdb *tdb)
{
#ifdef INET
	struct ip *ip;
#endif /* INET */

#ifdef INET6
	struct ip6_hdr *ip6;
#endif /* INET6 */

	struct tdb_ident *tdbi;
	struct m_tag *mtag;

	tdb->tdb_last_used = time.tv_sec;

	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		/* Fix the header length, for AH processing. */
		ip = mtod(m, struct ip *);
		ip->ip_len = htons(m->m_pkthdr.len);
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		/* Fix the header length, for AH processing. */
		if (m->m_pkthdr.len < sizeof(*ip6)) {
			m_freem(m);
			return ENXIO;
		}
		if (m->m_pkthdr.len - sizeof(*ip6) > IPV6_MAXPACKET) {
			/* No jumbogram support. */
			m_freem(m);
			return ENXIO;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6));
		break;
#endif /* INET6 */

	default:
		m_freem(m);
		DPRINTF(("ipsp_process_done(): unknown protocol family (%d)\n",
		    tdb->tdb_dst.sa.sa_family));
		return ENXIO;
	}

	/*
	 * Add a record of what we've done or what needs to be done to the
	 * packet.
	 */
	if ((tdb->tdb_flags & TDBF_SKIPCRYPTO) == 0)
		mtag = m_tag_get(PACKET_TAG_IPSEC_OUT_DONE,
		    sizeof(struct tdb_ident),
		    M_NOWAIT);
	else
		mtag = m_tag_get(PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED,
		    sizeof(struct tdb_ident), M_NOWAIT);

	if (mtag == NULL) {
		m_freem(m);
		DPRINTF(("ipsp_process_done(): could not allocate packet "
		    "tag\n"));
		return ENOMEM;
	}

	tdbi = (struct tdb_ident *)(mtag + 1);
	bcopy(&tdb->tdb_dst, &tdbi->dst, sizeof(union sockaddr_union));
	tdbi->proto = tdb->tdb_sproto;
	tdbi->spi = tdb->tdb_spi;

	m_tag_prepend(m, mtag);

	/* If there's another (bundled) TDB to apply, do so. */
	if (tdb->tdb_onext)
		return ipsp_process_packet(m, tdb->tdb_onext,
		    tdb->tdb_dst.sa.sa_family, 0);

	/*
	 * We're done with IPsec processing, transmit the packet using the
	 * appropriate network protocol (IP or IPv6). SPD lookup will be
	 * performed again there.
	 */
	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		NTOHS(ip->ip_len);
		NTOHS(ip->ip_off);

		return ip_output(m, (void *)NULL, (void *)NULL, IP_RAWOUTPUT, (void *)NULL, (void *)NULL);
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		/*
		 * We don't need massage, IPv6 header fields are always in
		 * net endian.
		 */
		return ip6_output(m, NULL, NULL, 0, NULL, NULL);
#endif /* INET6 */
	}
	return EINVAL; /* Not reached. */
}

ssize_t
ipsec_hdrsz(struct tdb *tdbp)
{
	ssize_t adjust;

	switch (tdbp->tdb_sproto) {
	case IPPROTO_ESP:
		if (tdbp->tdb_encalgxform == NULL)
			return (-1);

		/* Header length */
		if (tdbp->tdb_flags & TDBF_NOREPLAY)
			adjust = sizeof(u_int32_t) + tdbp->tdb_ivlen;
		else
			adjust = 2 * sizeof(u_int32_t) + tdbp->tdb_ivlen;
		/* Authenticator */
		if (tdbp->tdb_authalgxform != NULL)
			adjust += AH_HMAC_HASHLEN;
		/* Padding */
		adjust += tdbp->tdb_encalgxform->blocksize;
		break;

	case IPPROTO_AH:
		if (tdbp->tdb_authalgxform == NULL)
			return (-1);

		if (!(tdbp->tdb_flags & TDBF_NOREPLAY))
			adjust = AH_FLENGTH + sizeof(u_int32_t);
		else
			adjust = AH_FLENGTH;
		adjust += tdbp->tdb_authalgxform->authsize;
		break;

	default:
		return (-1);
	}

	if (!(tdbp->tdb_flags & TDBF_TUNNELING) &&
	    !(tdbp->tdb_flags & TDBF_USEDTUNNEL))
		return (adjust);

	switch (tdbp->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		adjust += sizeof(struct ip);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		adjust += sizeof(struct ip6_hdr);
		break;
#endif /* INET6 */
	}

	return (adjust);
}

void
ipsec_adjust_mtu(struct mbuf *m, u_int32_t mtu)
{
	struct tdb_ident *tdbi;
	struct tdb *tdbp;
	struct m_tag *mtag;
	ssize_t adjust;
	int s;

	s = spltdb();

	for (mtag = m_tag_find(m, PACKET_TAG_IPSEC_OUT_DONE, NULL); mtag;
	     mtag = m_tag_find(m, PACKET_TAG_IPSEC_OUT_DONE, mtag)) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdbp = gettdb(tdbi->spi, &tdbi->dst, tdbi->proto);
		if (tdbp == NULL)
			break;

		if ((adjust = ipsec_hdrsz(tdbp)) == -1)
			break;

		mtu -= adjust;
		tdbp->tdb_mtu = mtu;
		tdbp->tdb_mtutimeout = time.tv_sec + ip_mtudisc_timeout;
	}

	splx(s);
}
