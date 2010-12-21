/*	$OpenBSD: ipsec_input.c,v 1.99 2010/12/21 19:16:15 markus Exp $	*/
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

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/protosw.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/bpf.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6protosw.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_ipcomp.h>

#include <net/if_enc.h>

#include "bpfilter.h"

void *ipsec_common_ctlinput(u_int, int, struct sockaddr *, void *, int);

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

/* sysctl variables */
int esp_enable = 1;
int ah_enable = 1;
int ipcomp_enable = 0;

int *espctl_vars[ESPCTL_MAXID] = ESPCTL_VARS;
int *ahctl_vars[AHCTL_MAXID] = AHCTL_VARS;
int *ipcompctl_vars[IPCOMPCTL_MAXID] = IPCOMPCTL_VARS;

#ifdef INET6
extern struct ip6protosw inet6sw[];
extern u_char ip6_protox[];
#endif

/*
 * ipsec_common_input() gets called when we receive an IPsec-protected packet
 * in IPv4 or IPv6. All it does is find the right TDB and call the appropriate
 * transform. The callback takes care of further processing (like ingress
 * filtering).
 */
int
ipsec_common_input(struct mbuf *m, int skip, int protoff, int af, int sproto,
    int udpencap)
{
#define IPSEC_ISTAT(x,y,z) (sproto == IPPROTO_ESP ? (x)++ : \
			    sproto == IPPROTO_AH ? (y)++ : (z)++)

	union sockaddr_union dst_address;
	struct timeval tv;
	struct tdb *tdbp;
	struct ifnet *encif;
	u_int32_t spi;
	u_int16_t cpi;
	int s, error;

	IPSEC_ISTAT(espstat.esps_input, ahstat.ahs_input,
	    ipcompstat.ipcomps_input);

	if (m == 0) {
		DPRINTF(("ipsec_common_input(): NULL packet received\n"));
		IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops,
		    ipcompstat.ipcomps_hdrops);
		return EINVAL;
	}

	if ((sproto == IPPROTO_ESP && !esp_enable) ||
	    (sproto == IPPROTO_AH && !ah_enable) ||
	    (sproto == IPPROTO_IPCOMP && !ipcomp_enable)) {
		rip_input(m, skip, sproto);
		return 0;
	}

	if (m->m_pkthdr.len - skip < 2 * sizeof(u_int32_t)) {
		m_freem(m);
		IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops,
		    ipcompstat.ipcomps_hdrops);
		DPRINTF(("ipsec_common_input(): packet too small\n"));
		return EINVAL;
	}

	/* Retrieve the SPI from the relevant IPsec header */
	if (sproto == IPPROTO_ESP)
		m_copydata(m, skip, sizeof(u_int32_t), (caddr_t) &spi);
	else if (sproto == IPPROTO_AH)
		m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		    (caddr_t) &spi);
	else if (sproto == IPPROTO_IPCOMP) {
		m_copydata(m, skip + sizeof(u_int16_t), sizeof(u_int16_t),
		    (caddr_t) &cpi);
		spi = ntohl(htons(cpi));
	}

	/*
	 * Find tunnel control block and (indirectly) call the appropriate
	 * kernel crypto routine. The resulting mbuf chain is a valid
	 * IP packet ready to go through input processing.
	 */

	bzero(&dst_address, sizeof(dst_address));
	dst_address.sa.sa_family = af;

	switch (af) {
#ifdef INET
	case AF_INET:
		dst_address.sin.sin_len = sizeof(struct sockaddr_in);
		m_copydata(m, offsetof(struct ip, ip_dst),
		    sizeof(struct in_addr),
		    (caddr_t) &(dst_address.sin.sin_addr));
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		dst_address.sin6.sin6_len = sizeof(struct sockaddr_in6);
		m_copydata(m, offsetof(struct ip6_hdr, ip6_dst),
		    sizeof(struct in6_addr),
		    (caddr_t) &(dst_address.sin6.sin6_addr));
		in6_recoverscope(&dst_address.sin6, &dst_address.sin6.sin6_addr,
		    NULL);
		break;
#endif /* INET6 */

	default:
		DPRINTF(("ipsec_common_input(): unsupported protocol "
		    "family %d\n", af));
		m_freem(m);
		IPSEC_ISTAT(espstat.esps_nopf, ahstat.ahs_nopf,
		    ipcompstat.ipcomps_nopf);
		return EPFNOSUPPORT;
	}

	s = spltdb();
	tdbp = gettdb(rtable_l2(m->m_pkthdr.rdomain),
	    spi, &dst_address, sproto);
	if (tdbp == NULL) {
		splx(s);
		DPRINTF(("ipsec_common_input(): could not find SA for "
		    "packet to %s, spi %08x\n",
		    ipsp_address(dst_address), ntohl(spi)));
		m_freem(m);
		IPSEC_ISTAT(espstat.esps_notdb, ahstat.ahs_notdb,
		    ipcompstat.ipcomps_notdb);
		return ENOENT;
	}

	if (tdbp->tdb_flags & TDBF_INVALID) {
		splx(s);
		DPRINTF(("ipsec_common_input(): attempted to use invalid SA %s/%08x/%u\n", ipsp_address(dst_address), ntohl(spi), tdbp->tdb_sproto));
		m_freem(m);
		IPSEC_ISTAT(espstat.esps_invalid, ahstat.ahs_invalid,
		    ipcompstat.ipcomps_invalid);
		return EINVAL;
	}

	if (udpencap && !(tdbp->tdb_flags & TDBF_UDPENCAP)) {
		splx(s);
		DPRINTF(("ipsec_common_input(): attempted to use non-udpencap SA %s/%08x/%u\n", ipsp_address(dst_address), ntohl(spi), tdbp->tdb_sproto));
		m_freem(m);
		espstat.esps_udpinval++;
		return EINVAL;
	}

	if (tdbp->tdb_xform == NULL) {
		splx(s);
		DPRINTF(("ipsec_common_input(): attempted to use uninitialized SA %s/%08x/%u\n", ipsp_address(dst_address), ntohl(spi), tdbp->tdb_sproto));
		m_freem(m);
		IPSEC_ISTAT(espstat.esps_noxform, ahstat.ahs_noxform,
		    ipcompstat.ipcomps_noxform);
		return ENXIO;
	}

	if (sproto != IPPROTO_IPCOMP) {
		if ((encif = enc_getif(tdbp->tdb_rdomain,
		    tdbp->tdb_tap)) == NULL) {
			splx(s);
			DPRINTF(("ipsec_common_input(): "
			    "no enc%u interface for SA %s/%08x/%u\n",
			    tdbp->tdb_tap, ipsp_address(dst_address),
			    ntohl(spi), tdbp->tdb_sproto));
			m_freem(m);

			IPSEC_ISTAT(espstat.esps_pdrops,
			    ahstat.ahs_pdrops,
			    ipcompstat.ipcomps_pdrops);
			return EACCES;
		}

		/* XXX This conflicts with the scoped nature of IPv6 */
		m->m_pkthdr.rcvif = encif;
	}

	/* Register first use, setup expiration timer. */
	if (tdbp->tdb_first_use == 0) {
		tdbp->tdb_first_use = time_second;

		tv.tv_usec = 0;

		tv.tv_sec = tdbp->tdb_exp_first_use + tdbp->tdb_first_use;
		if (tdbp->tdb_flags & TDBF_FIRSTUSE)
			timeout_add(&tdbp->tdb_first_tmo, hzto(&tv));

		tv.tv_sec = tdbp->tdb_first_use + tdbp->tdb_soft_first_use;
		if (tdbp->tdb_flags & TDBF_SOFT_FIRSTUSE)
			timeout_add(&tdbp->tdb_sfirst_tmo, hzto(&tv));
	}

	/*
	 * Call appropriate transform and return -- callback takes care of
	 * everything else.
	 */
	error = (*(tdbp->tdb_xform->xf_input))(m, tdbp, skip, protoff);
	splx(s);
	return error;
}

/*
 * IPsec input callback, called by the transform callback. Takes care of
 * filtering and other sanity checks on the processed packet.
 */
int
ipsec_common_input_cb(struct mbuf *m, struct tdb *tdbp, int skip, int protoff,
    struct m_tag *mt)
{
	int af, sproto;
	u_char prot;

#if NBPFILTER > 0
	struct ifnet *encif;
#endif

#ifdef INET
	struct ip *ip, ipn;
#endif /* INET */

#ifdef INET6
	struct ip6_hdr *ip6, ip6n;
#endif /* INET6 */
	struct m_tag *mtag;
	struct tdb_ident *tdbi;

	af = tdbp->tdb_dst.sa.sa_family;
	sproto = tdbp->tdb_sproto;

	tdbp->tdb_last_used = time_second;

	/* Sanity check */
	if (m == NULL) {
		/* The called routine will print a message if necessary */
		IPSEC_ISTAT(espstat.esps_badkcr, ahstat.ahs_badkcr,
		    ipcompstat.ipcomps_badkcr);
		return EINVAL;
	}

#ifdef INET
	/* Fix IPv4 header */
	if (af == AF_INET) {
		if ((m->m_len < skip) && ((m = m_pullup(m, skip)) == NULL)) {
			DPRINTF(("ipsec_common_input_cb(): processing failed "
			    "for SA %s/%08x\n", ipsp_address(tdbp->tdb_dst),
			    ntohl(tdbp->tdb_spi)));
			IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			return ENOBUFS;
		}

		ip = mtod(m, struct ip *);
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
		prot = ip->ip_p;

		/* IP-in-IP encapsulation */
		if (prot == IPPROTO_IPIP) {
			if (m->m_pkthdr.len - skip < sizeof(struct ip)) {
				m_freem(m);
				IPSEC_ISTAT(espstat.esps_hdrops,
				    ahstat.ahs_hdrops,
				    ipcompstat.ipcomps_hdrops);
				return EINVAL;
			}
			/* ipn will now contain the inner IPv4 header */
			m_copydata(m, skip, sizeof(struct ip),
			    (caddr_t) &ipn);

			/*
			 * Check that the inner source address is the same as
			 * the proxy address, if available.
			 */
			if ((tdbp->tdb_proxy.sa.sa_family == AF_INET &&
			    tdbp->tdb_proxy.sin.sin_addr.s_addr !=
			    INADDR_ANY &&
			    ipn.ip_src.s_addr !=
			    tdbp->tdb_proxy.sin.sin_addr.s_addr) ||
			    (tdbp->tdb_proxy.sa.sa_family != AF_INET &&
				tdbp->tdb_proxy.sa.sa_family != 0)) {

				DPRINTF(("ipsec_common_input_cb(): inner "
				    "source address %s doesn't correspond to "
				    "expected proxy source %s, SA %s/%08x\n",
				    inet_ntoa4(ipn.ip_src),
				    ipsp_address(tdbp->tdb_proxy),
				    ipsp_address(tdbp->tdb_dst),
				    ntohl(tdbp->tdb_spi)));

				m_freem(m);
				IPSEC_ISTAT(espstat.esps_pdrops,
				    ahstat.ahs_pdrops,
				    ipcompstat.ipcomps_pdrops);
				return EACCES;
			}
		}

#ifdef INET6
		/* IPv6-in-IP encapsulation. */
		if (prot == IPPROTO_IPV6) {
			if (m->m_pkthdr.len - skip < sizeof(struct ip6_hdr)) {
				m_freem(m);
				IPSEC_ISTAT(espstat.esps_hdrops,
				    ahstat.ahs_hdrops,
				    ipcompstat.ipcomps_hdrops);
				return EINVAL;
			}
			/* ip6n will now contain the inner IPv6 header. */
			m_copydata(m, skip, sizeof(struct ip6_hdr),
			    (caddr_t) &ip6n);

			/*
			 * Check that the inner source address is the same as
			 * the proxy address, if available.
			 */
			if ((tdbp->tdb_proxy.sa.sa_family == AF_INET6 &&
			    !IN6_IS_ADDR_UNSPECIFIED(&tdbp->tdb_proxy.sin6.sin6_addr) &&
			    !IN6_ARE_ADDR_EQUAL(&ip6n.ip6_src,
				&tdbp->tdb_proxy.sin6.sin6_addr)) ||
			    (tdbp->tdb_proxy.sa.sa_family != AF_INET6 &&
				tdbp->tdb_proxy.sa.sa_family != 0)) {

				DPRINTF(("ipsec_common_input_cb(): inner "
				    "source address %s doesn't correspond to "
				    "expected proxy source %s, SA %s/%08x\n",
				    ip6_sprintf(&ip6n.ip6_src),
				    ipsp_address(tdbp->tdb_proxy),
				    ipsp_address(tdbp->tdb_dst),
				    ntohl(tdbp->tdb_spi)));

				m_freem(m);
				IPSEC_ISTAT(espstat.esps_pdrops,
				    ahstat.ahs_pdrops,
				    ipcompstat.ipcomps_pdrops);
				return EACCES;
			}
		}
#endif /* INET6 */
	}
#endif /* INET */

#ifdef INET6
	/* Fix IPv6 header */
	if (af == AF_INET6)
	{
		if (m->m_len < sizeof(struct ip6_hdr) &&
		    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {

			DPRINTF(("ipsec_common_input_cb(): processing failed "
			    "for SA %s/%08x\n", ipsp_address(tdbp->tdb_dst),
			    ntohl(tdbp->tdb_spi)));

			IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			return EACCES;
		}

		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_plen = htons(m->m_pkthdr.len - skip);

		/* Save protocol */
		m_copydata(m, protoff, 1, (caddr_t) &prot);

#ifdef INET
		/* IP-in-IP encapsulation */
		if (prot == IPPROTO_IPIP) {
			if (m->m_pkthdr.len - skip < sizeof(struct ip)) {
				m_freem(m);
				IPSEC_ISTAT(espstat.esps_hdrops,
				    ahstat.ahs_hdrops,
				    ipcompstat.ipcomps_hdrops);
				return EINVAL;
			}
			/* ipn will now contain the inner IPv4 header */
			m_copydata(m, skip, sizeof(struct ip), (caddr_t) &ipn);

			/*
			 * Check that the inner source address is the same as
			 * the proxy address, if available.
			 */
			if ((tdbp->tdb_proxy.sa.sa_family == AF_INET &&
			    tdbp->tdb_proxy.sin.sin_addr.s_addr !=
			    INADDR_ANY &&
			    ipn.ip_src.s_addr !=
				tdbp->tdb_proxy.sin.sin_addr.s_addr) ||
			    (tdbp->tdb_proxy.sa.sa_family != AF_INET &&
				tdbp->tdb_proxy.sa.sa_family != 0)) {

				DPRINTF(("ipsec_common_input_cb(): inner "
				    "source address %s doesn't correspond to "
				    "expected proxy source %s, SA %s/%08x\n",
				    inet_ntoa4(ipn.ip_src),
				    ipsp_address(tdbp->tdb_proxy),
				    ipsp_address(tdbp->tdb_dst),
				    ntohl(tdbp->tdb_spi)));

				m_freem(m);
				IPSEC_ISTAT(espstat.esps_pdrops,
				    ahstat.ahs_pdrops,
				    ipcompstat.ipcomps_pdrops);
				return EACCES;
			}
		}
#endif /* INET */

		/* IPv6-in-IP encapsulation */
		if (prot == IPPROTO_IPV6) {
			if (m->m_pkthdr.len - skip < sizeof(struct ip6_hdr)) {
				m_freem(m);
				IPSEC_ISTAT(espstat.esps_hdrops,
				    ahstat.ahs_hdrops,
				    ipcompstat.ipcomps_hdrops);
				return EINVAL;
			}
			/* ip6n will now contain the inner IPv6 header. */
			m_copydata(m, skip, sizeof(struct ip6_hdr),
			    (caddr_t) &ip6n);

			/*
			 * Check that the inner source address is the same as
			 * the proxy address, if available.
			 */
			if ((tdbp->tdb_proxy.sa.sa_family == AF_INET6 &&
			    !IN6_IS_ADDR_UNSPECIFIED(&tdbp->tdb_proxy.sin6.sin6_addr) &&
			    !IN6_ARE_ADDR_EQUAL(&ip6n.ip6_src,
				&tdbp->tdb_proxy.sin6.sin6_addr)) ||
			    (tdbp->tdb_proxy.sa.sa_family != AF_INET6 &&
				tdbp->tdb_proxy.sa.sa_family != 0)) {

				DPRINTF(("ipsec_common_input_cb(): inner "
				    "source address %s doesn't correspond to "
				    "expected proxy source %s, SA %s/%08x\n",
				    ip6_sprintf(&ip6n.ip6_src),
				    ipsp_address(tdbp->tdb_proxy),
				    ipsp_address(tdbp->tdb_dst),
				    ntohl(tdbp->tdb_spi)));

				m_freem(m);
				IPSEC_ISTAT(espstat.esps_pdrops,
				    ahstat.ahs_pdrops,
				    ipcompstat.ipcomps_pdrops);
				return EACCES;
			}
		}
	}
#endif /* INET6 */

	/*
	 * Record what we've done to the packet (under what SA it was
	 * processed). If we've been passed an mtag, it means the packet
	 * was already processed by an ethernet/crypto combo card and
	 * thus has a tag attached with all the right information, but
	 * with a PACKET_TAG_IPSEC_IN_CRYPTO_DONE as opposed to
	 * PACKET_TAG_IPSEC_IN_DONE type; in that case, just change the type.
	 */
	if (tdbp->tdb_sproto != IPPROTO_IPCOMP) {
		mtag = m_tag_get(PACKET_TAG_IPSEC_IN_DONE,
		    sizeof(struct tdb_ident), M_NOWAIT);
		if (mtag == NULL) {
			m_freem(m);
			DPRINTF(("ipsec_common_input_cb(): failed to "
			    "get tag\n"));
			IPSEC_ISTAT(espstat.esps_hdrops, ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			return ENOMEM;
		}

		tdbi = (struct tdb_ident *)(mtag + 1);
		bcopy(&tdbp->tdb_dst, &tdbi->dst,
		    sizeof(union sockaddr_union));
		tdbi->proto = tdbp->tdb_sproto;
		tdbi->spi = tdbp->tdb_spi;
		tdbi->rdomain = tdbp->tdb_rdomain;

		m_tag_prepend(m, mtag);
	}

	if (sproto == IPPROTO_ESP) {
		/* Packet is confidential ? */
		if (tdbp->tdb_encalgxform)
			m->m_flags |= M_CONF;

		/* Check if we had authenticated ESP. */
		if (tdbp->tdb_authalgxform)
			m->m_flags |= M_AUTH;
	} else if (sproto == IPPROTO_AH)
		m->m_flags |= M_AUTH | M_AUTH_AH;

#if NPF > 0
	/* Add pf tag if requested. */
	if (pf_tag_packet(m, tdbp->tdb_tag, -1))
		DPRINTF(("failed to tag ipsec packet\n"));
	pf_pkt_addr_changed(m);
#endif

	if (tdbp->tdb_flags & TDBF_TUNNELING)
		m->m_flags |= M_TUNNEL;

#if NBPFILTER > 0
	if ((encif = enc_getif(tdbp->tdb_rdomain, tdbp->tdb_tap)) != NULL) {
		encif->if_ipackets++;
		encif->if_ibytes += m->m_pkthdr.len;

		if (encif->if_bpf) {
			struct enchdr hdr;

			hdr.af = af;
			hdr.spi = tdbp->tdb_spi;
			hdr.flags = m->m_flags & (M_AUTH|M_CONF|M_AUTH_AH);

			bpf_mtap_hdr(encif->if_bpf, (char *)&hdr,
			    ENC_HDRLEN, m, BPF_DIRECTION_IN);
		}
	}
#endif

	/* Call the appropriate IPsec transform callback. */
	switch (af) {
#ifdef INET
	case AF_INET:
		switch (sproto)
		{
		case IPPROTO_ESP:
			return esp4_input_cb(m);

		case IPPROTO_AH:
			return ah4_input_cb(m);

		case IPPROTO_IPCOMP:
			return ipcomp4_input_cb(m);

		default:
			DPRINTF(("ipsec_common_input_cb(): unknown/unsupported"
			    " security protocol %d\n", sproto));
			m_freem(m);
			return EPFNOSUPPORT;
		}
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		switch (sproto) {
		case IPPROTO_ESP:
			return esp6_input_cb(m, skip, protoff);

		case IPPROTO_AH:
			return ah6_input_cb(m, skip, protoff);

		case IPPROTO_IPCOMP:
			return ipcomp6_input_cb(m, skip, protoff);

		default:
			DPRINTF(("ipsec_common_input_cb(): unknown/unsupported"
			    " security protocol %d\n", sproto));
			m_freem(m);
			return EPFNOSUPPORT;
		}
		break;
#endif /* INET6 */

	default:
		DPRINTF(("ipsec_common_input_cb(): unknown/unsupported "
		    "protocol family %d\n", af));
		m_freem(m);
		return EPFNOSUPPORT;
	}
#undef IPSEC_ISTAT
}

int
esp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case ESPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &espstat, sizeof(espstat)));
	default:
		if (name[0] < ESPCTL_MAXID)
			return (sysctl_int_arr(espctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (ENOPROTOOPT);
	}
}

int
ah_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case AHCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &ahstat, sizeof(ahstat)));
	default:
		if (name[0] < AHCTL_MAXID)
			return (sysctl_int_arr(ahctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (ENOPROTOOPT);
	}
}

int
ipcomp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IPCOMPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &ipcompstat, sizeof(ipcompstat)));
	default:
		if (name[0] < IPCOMPCTL_MAXID)
			return (sysctl_int_arr(ipcompctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (ENOPROTOOPT);
	}
}

#ifdef INET
/* IPv4 AH wrapper. */
void
ah4_input(struct mbuf *m, ...)
{
	int skip;

	va_list ap;
	va_start(ap, m);
	skip = va_arg(ap, int);
	va_end(ap);

	ipsec_common_input(m, skip, offsetof(struct ip, ip_p), AF_INET,
	    IPPROTO_AH, 0);
	return;
}

/* IPv4 AH callback. */
int
ah4_input_cb(struct mbuf *m, ...)
{
	struct ifqueue *ifq = &ipintrq;
	int s = splnet();

	/*
	 * Interface pointer is already in first mbuf; chop off the
	 * `outer' header and reschedule.
	 */

	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		ahstat.ahs_qfull++;
		splx(s);

		m_freem(m);
		DPRINTF(("ah4_input_cb(): dropped packet because of full "
		    "IP queue\n"));
		return ENOBUFS;
	}

	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_IP);
	splx(s);
	return 0;
}


/* XXX rdomain */
void *
ah4_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return (NULL);

	return (ipsec_common_ctlinput(rdomain, cmd, sa, v, IPPROTO_AH));
}

/* IPv4 ESP wrapper. */
void
esp4_input(struct mbuf *m, ...)
{
	int skip;

	va_list ap;
	va_start(ap, m);
	skip = va_arg(ap, int);
	va_end(ap);

	ipsec_common_input(m, skip, offsetof(struct ip, ip_p), AF_INET,
	    IPPROTO_ESP, 0);
}

/* IPv4 ESP callback. */
int
esp4_input_cb(struct mbuf *m, ...)
{
	struct ifqueue *ifq = &ipintrq;
	int s = splnet();

	/*
	 * Interface pointer is already in first mbuf; chop off the
	 * `outer' header and reschedule.
	 */
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		espstat.esps_qfull++;
		splx(s);

		m_freem(m);
		DPRINTF(("esp4_input_cb(): dropped packet because of full "
		    "IP queue\n"));
		return ENOBUFS;
	}

	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_IP);
	splx(s);
	return 0;
}

/* IPv4 IPCOMP wrapper */
void
ipcomp4_input(struct mbuf *m, ...)
{
	int skip;
	va_list ap;
	va_start(ap, m);
	skip = va_arg(ap, int);
	va_end(ap);

	ipsec_common_input(m, skip, offsetof(struct ip, ip_p), AF_INET,
	    IPPROTO_IPCOMP, 0);
}

/* IPv4 IPCOMP callback */
int
ipcomp4_input_cb(struct mbuf *m, ...)
{
	struct ifqueue *ifq = &ipintrq;
	int s = splnet();

	/*
	 * Interface pointer is already in first mbuf; chop off the
	 * `outer' header and reschedule.
	 */
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		ipcompstat.ipcomps_qfull++;
		splx(s);

		m_freem(m);
		DPRINTF(("ipcomp4_input_cb(): dropped packet because of full IP queue\n"));
		return ENOBUFS;
	}

	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_IP);
	splx(s);

	return 0;
}

void *
ipsec_common_ctlinput(u_int rdomain, int cmd, struct sockaddr *sa,
    void *v, int proto)
{
	extern u_int ip_mtudisc_timeout;
	struct ip *ip = v;
	int s;

	if (cmd == PRC_MSGSIZE && ip && ip_mtudisc && ip->ip_v == 4) {
		struct tdb *tdbp;
		struct sockaddr_in dst;
		struct icmp *icp;
		int hlen = ip->ip_hl << 2;
		u_int32_t spi, mtu;
		ssize_t adjust;

		/* Find the right MTU. */
		icp = (struct icmp *)((caddr_t) ip -
		    offsetof(struct icmp, icmp_ip));
		mtu = ntohs(icp->icmp_nextmtu);

		/*
		 * Ignore the packet, if we do not receive a MTU
		 * or the MTU is too small to be acceptable.
		 */
		if (mtu < 296)
			return (NULL);

		bzero(&dst, sizeof(struct sockaddr_in));
		dst.sin_family = AF_INET;
		dst.sin_len = sizeof(struct sockaddr_in);
		dst.sin_addr.s_addr = ip->ip_dst.s_addr;

		bcopy((caddr_t)ip + hlen, &spi, sizeof(u_int32_t));

		s = spltdb();
		tdbp = gettdb(rdomain, spi, (union sockaddr_union *)&dst,
		    proto);
		if (tdbp == NULL || tdbp->tdb_flags & TDBF_INVALID) {
			splx(s);
			return (NULL);
		}

		/* Walk the chain backswards to the first tdb */
		for (; tdbp; tdbp = tdbp->tdb_inext) {
			if (tdbp->tdb_flags & TDBF_INVALID ||
			    (adjust = ipsec_hdrsz(tdbp)) == -1) {
				splx(s);
				return (NULL);
			}

			mtu -= adjust;

			/* Store adjusted MTU in tdb */
			tdbp->tdb_mtu = mtu;
			tdbp->tdb_mtutimeout = time_second +
			    ip_mtudisc_timeout;
			DPRINTF(("ipsec_common_ctlinput: "
			    "spi %08x mtu %d adjust %d\n",
			    ntohl(tdbp->tdb_spi), tdbp->tdb_mtu,
			    adjust));
		}
		splx(s);
		return (NULL);
	}
	return (NULL);
}

/* XXX rdomain */
void *
udpencap_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	struct ip *ip = v;
	struct tdb *tdbp;
	struct icmp *icp;
	u_int32_t mtu;
	ssize_t adjust;
	struct sockaddr_in dst, src;
	union sockaddr_union *su_dst, *su_src;
	int s;

	icp = (struct icmp *)((caddr_t) ip - offsetof(struct icmp, icmp_ip));
	mtu = ntohs(icp->icmp_nextmtu);

	/*
	 * Ignore the packet, if we do not receive a MTU
	 * or the MTU is too small to be acceptable.
	 */
	if (mtu < 296)
		return (NULL);

	bzero(&dst, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);
	dst.sin_addr.s_addr = ip->ip_dst.s_addr;
	su_dst = (union sockaddr_union *)&dst;
	bzero(&src, sizeof(src));
	src.sin_family = AF_INET;
	src.sin_len = sizeof(struct sockaddr_in);
	src.sin_addr.s_addr = ip->ip_src.s_addr;
	su_src = (union sockaddr_union *)&src;

	s = spltdb();
	tdbp = gettdbbysrcdst(rdomain, 0, su_src, su_dst, IPPROTO_ESP);

	for (; tdbp != NULL; tdbp = tdbp->tdb_snext) {
		if (tdbp->tdb_sproto == IPPROTO_ESP &&
		    ((tdbp->tdb_flags & (TDBF_INVALID|TDBF_UDPENCAP))
		    == TDBF_UDPENCAP) &&
		    !bcmp(&tdbp->tdb_dst, &dst, SA_LEN(&su_dst->sa)) &&
		    !bcmp(&tdbp->tdb_src, &src, SA_LEN(&su_src->sa))) {
			if ((adjust = ipsec_hdrsz(tdbp)) != -1) {
				/* Store adjusted MTU in tdb */
				tdbp->tdb_mtu = mtu - adjust;
				tdbp->tdb_mtutimeout = time_second +
				    ip_mtudisc_timeout;
				DPRINTF(("udpencap_ctlinput: "
				    "spi %08x mtu %d adjust %d\n",
				    ntohl(tdbp->tdb_spi), tdbp->tdb_mtu,
				    adjust));
			}
		}
	}
	splx(s);
	return (NULL);
}

/* XXX rdomain */
void *
esp4_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return (NULL);

	return (ipsec_common_ctlinput(rdomain, cmd, sa, v, IPPROTO_ESP));
}
#endif /* INET */

#ifdef INET6
/* IPv6 AH wrapper. */
int
ah6_input(struct mbuf **mp, int *offp, int proto)
{
	int l = 0;
	int protoff, nxt;
	struct ip6_ext ip6e;

	if (*offp < sizeof(struct ip6_hdr)) {
		DPRINTF(("ah6_input(): bad offset\n"));
		ahstat.ahs_hdrops++;
		m_freem(*mp);
		*mp = NULL;
		return IPPROTO_DONE;
	} else if (*offp == sizeof(struct ip6_hdr)) {
		protoff = offsetof(struct ip6_hdr, ip6_nxt);
	} else {
		/* Chase down the header chain... */
		protoff = sizeof(struct ip6_hdr);
		nxt = (mtod(*mp, struct ip6_hdr *))->ip6_nxt;

		do {
			protoff += l;
			m_copydata(*mp, protoff, sizeof(ip6e),
			    (caddr_t) &ip6e);

			if (nxt == IPPROTO_AH)
				l = (ip6e.ip6e_len + 2) << 2;
			else
				l = (ip6e.ip6e_len + 1) << 3;
#ifdef DIAGNOSTIC
			if (l <= 0)
				panic("ah6_input: l went zero or negative");
#endif

			nxt = ip6e.ip6e_nxt;
		} while (protoff + l < *offp);

		/* Malformed packet check */
		if (protoff + l != *offp) {
			DPRINTF(("ah6_input(): bad packet header chain\n"));
			ahstat.ahs_hdrops++;
			m_freem(*mp);
			*mp = NULL;
			return IPPROTO_DONE;
		}
		protoff += offsetof(struct ip6_ext, ip6e_nxt);
	}
	ipsec_common_input(*mp, *offp, protoff, AF_INET6, proto, 0);
	return IPPROTO_DONE;
}

/* IPv6 AH callback. */
int
ah6_input_cb(struct mbuf *m, int off, int protoff)
{
	int nxt;
	u_int8_t nxt8;
	int nest = 0;

	/* Retrieve new protocol */
	m_copydata(m, protoff, sizeof(u_int8_t), (caddr_t) &nxt8);
	nxt = nxt8;

	/*
	 * see the end of ip6_input for this logic.
	 * IPPROTO_IPV[46] case will be processed just like other ones
	 */
	while (nxt != IPPROTO_DONE) {
		if (ip6_hdrnestlimit && (++nest > ip6_hdrnestlimit)) {
			ip6stat.ip6s_toomanyhdr++;
			goto bad;
		}

		/*
		 * Protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			ip6stat.ip6s_tooshort++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
			goto bad;
		}
		nxt = (*inet6sw[ip6_protox[nxt]].pr_input)(&m, &off, nxt);
	}
	return 0;

 bad:
	m_freem(m);
	return EINVAL;
}

/* IPv6 ESP wrapper. */
int
esp6_input(struct mbuf **mp, int *offp, int proto)
{
	int l = 0;
	int protoff, nxt;
	struct ip6_ext ip6e;

	if (*offp < sizeof(struct ip6_hdr)) {
		DPRINTF(("esp6_input(): bad offset\n"));
		espstat.esps_hdrops++;
		m_freem(*mp);
		*mp = NULL;
		return IPPROTO_DONE;
	} else if (*offp == sizeof(struct ip6_hdr)) {
		protoff = offsetof(struct ip6_hdr, ip6_nxt);
	} else {
		/* Chase down the header chain... */
		protoff = sizeof(struct ip6_hdr);
		nxt = (mtod(*mp, struct ip6_hdr *))->ip6_nxt;

		do {
			protoff += l;
			m_copydata(*mp, protoff, sizeof(ip6e),
			    (caddr_t) &ip6e);

			if (nxt == IPPROTO_AH)
				l = (ip6e.ip6e_len + 2) << 2;
			else
				l = (ip6e.ip6e_len + 1) << 3;
#ifdef DIAGNOSTIC
			if (l <= 0)
				panic("esp6_input: l went zero or negative");
#endif

			nxt = ip6e.ip6e_nxt;
		} while (protoff + l < *offp);

		/* Malformed packet check */
		if (protoff + l != *offp) {
			DPRINTF(("esp6_input(): bad packet header chain\n"));
			espstat.esps_hdrops++;
			m_freem(*mp);
			*mp = NULL;
			return IPPROTO_DONE;
		}
		protoff += offsetof(struct ip6_ext, ip6e_nxt);
	}
	ipsec_common_input(*mp, *offp, protoff, AF_INET6, proto, 0);
	return IPPROTO_DONE;

}

/* IPv6 ESP callback */
int
esp6_input_cb(struct mbuf *m, int skip, int protoff)
{
	return ah6_input_cb(m, skip, protoff);
}

/* IPv6 IPcomp wrapper */
int
ipcomp6_input(struct mbuf **mp, int *offp, int proto)
{
	int l = 0;
	int protoff, nxt;
	struct ip6_ext ip6e;

	if (*offp < sizeof(struct ip6_hdr)) {
		DPRINTF(("ipcomp6_input(): bad offset\n"));
		ipcompstat.ipcomps_hdrops++;
		m_freem(*mp);
		*mp = NULL;
		return IPPROTO_DONE;
	} else if (*offp == sizeof(struct ip6_hdr)) {
		protoff = offsetof(struct ip6_hdr, ip6_nxt);
	} else {
		/* Chase down the header chain... */
		protoff = sizeof(struct ip6_hdr);
		nxt = (mtod(*mp, struct ip6_hdr *))->ip6_nxt;

		do {
			protoff += l;
			m_copydata(*mp, protoff, sizeof(ip6e),
			    (caddr_t) &ip6e);
			if (nxt == IPPROTO_AH)
				l = (ip6e.ip6e_len + 2) << 2;
			else
				l = (ip6e.ip6e_len + 1) << 3;
#ifdef DIAGNOSTIC
			if (l <= 0)
				panic("ipcomp6_input: l went zero or negative");
#endif

			nxt = ip6e.ip6e_nxt;
		} while (protoff + l < *offp);

		/* Malformed packet check */
		if (protoff + l != *offp) {
			DPRINTF(("ipcomp6_input(): bad packet header chain\n"));
			ipcompstat.ipcomps_hdrops++;
			m_freem(*mp);
			*mp = NULL;
			return IPPROTO_DONE;
		}

		protoff += offsetof(struct ip6_ext, ip6e_nxt);
	}
	ipsec_common_input(*mp, *offp, protoff, AF_INET6, proto, 0);
	return IPPROTO_DONE;
}

/* IPv6 IPcomp callback */
int
ipcomp6_input_cb(struct mbuf *m, int skip, int protoff)
{
	return ah6_input_cb(m, skip, protoff);
}

#endif /* INET6 */
