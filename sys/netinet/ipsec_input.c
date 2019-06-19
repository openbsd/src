/*	$OpenBSD: ipsec_input.c,v 1.168 2018/11/09 13:26:12 claudio Exp $	*/
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
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/bpf.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6protosw.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_ipcomp.h>

#include <net/if_enc.h>

#include <crypto/cryptodev.h>
#include <crypto/xform.h>

#include "bpfilter.h"

void ipsec_common_ctlinput(u_int, int, struct sockaddr *, void *, int);

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

/* sysctl variables */
int encdebug = 0;
int ipsec_keep_invalid = IPSEC_DEFAULT_EMBRYONIC_SA_TIMEOUT;
int ipsec_require_pfs = IPSEC_DEFAULT_PFS;
int ipsec_soft_allocations = IPSEC_DEFAULT_SOFT_ALLOCATIONS;
int ipsec_exp_allocations = IPSEC_DEFAULT_EXP_ALLOCATIONS;
int ipsec_soft_bytes = IPSEC_DEFAULT_SOFT_BYTES;
int ipsec_exp_bytes = IPSEC_DEFAULT_EXP_BYTES;
int ipsec_soft_timeout = IPSEC_DEFAULT_SOFT_TIMEOUT;
int ipsec_exp_timeout = IPSEC_DEFAULT_EXP_TIMEOUT;
int ipsec_soft_first_use = IPSEC_DEFAULT_SOFT_FIRST_USE;
int ipsec_exp_first_use = IPSEC_DEFAULT_EXP_FIRST_USE;
int ipsec_expire_acquire = IPSEC_DEFAULT_EXPIRE_ACQUIRE;

int esp_enable = 1;
int ah_enable = 1;
int ipcomp_enable = 0;

int *espctl_vars[ESPCTL_MAXID] = ESPCTL_VARS;
int *ahctl_vars[AHCTL_MAXID] = AHCTL_VARS;
int *ipcompctl_vars[IPCOMPCTL_MAXID] = IPCOMPCTL_VARS;

struct cpumem *espcounters;
struct cpumem *ahcounters;
struct cpumem *ipcompcounters;
struct cpumem *ipseccounters;

char ipsec_def_enc[20];
char ipsec_def_auth[20];
char ipsec_def_comp[20];

int *ipsecctl_vars[IPSEC_MAXID] = IPSECCTL_VARS;

int esp_sysctl_espstat(void *, size_t *, void *);
int ah_sysctl_ahstat(void *, size_t *, void *);
int ipcomp_sysctl_ipcompstat(void *, size_t *, void *);
int ipsec_sysctl_ipsecstat(void *, size_t *, void *);

void
ipsec_init(void)
{
	espcounters = counters_alloc(esps_ncounters);
	ahcounters = counters_alloc(ahs_ncounters);
	ipcompcounters = counters_alloc(ipcomps_ncounters);
	ipseccounters = counters_alloc(ipsec_ncounters);

	strlcpy(ipsec_def_enc, IPSEC_DEFAULT_DEF_ENC, sizeof(ipsec_def_enc));
	strlcpy(ipsec_def_auth, IPSEC_DEFAULT_DEF_AUTH, sizeof(ipsec_def_auth));
	strlcpy(ipsec_def_comp, IPSEC_DEFAULT_DEF_COMP, sizeof(ipsec_def_comp));

}

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
#define IPSEC_ISTAT(x,y,z) do {			\
	if (sproto == IPPROTO_ESP)		\
		espstat_inc(x);			\
	else if (sproto == IPPROTO_AH)		\
		ahstat_inc(y);			\
	else					\
		ipcompstat_inc(z);		\
} while (0)

	union sockaddr_union dst_address;
	struct tdb *tdbp = NULL;
	struct ifnet *encif;
	u_int32_t spi;
	u_int16_t cpi;
	int error;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	NET_ASSERT_LOCKED();

	ipsecstat_inc(ipsec_ipackets);
	ipsecstat_add(ipsec_ibytes, m->m_pkthdr.len);
	IPSEC_ISTAT(esps_input, ahs_input, ipcomps_input);

	if (m == NULL) {
		DPRINTF(("%s: NULL packet received\n", __func__));
		IPSEC_ISTAT(esps_hdrops, ahs_hdrops, ipcomps_hdrops);
		return EINVAL;
	}

	if ((sproto == IPPROTO_IPCOMP) && (m->m_flags & M_COMP)) {
		DPRINTF(("%s: repeated decompression\n", __func__));
		ipcompstat_inc(ipcomps_pdrops);
		error = EINVAL;
		goto drop;
	}

	if (m->m_pkthdr.len - skip < 2 * sizeof(u_int32_t)) {
		DPRINTF(("%s: packet too small\n", __func__));
		IPSEC_ISTAT(esps_hdrops, ahs_hdrops, ipcomps_hdrops);
		error = EINVAL;
		goto drop;
	}

	/* Retrieve the SPI from the relevant IPsec header */
	switch (sproto) {
	case IPPROTO_ESP:
		m_copydata(m, skip, sizeof(u_int32_t), (caddr_t) &spi);
		break;
	case IPPROTO_AH:
		m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		    (caddr_t) &spi);
		break;
	case IPPROTO_IPCOMP:
		m_copydata(m, skip + sizeof(u_int16_t), sizeof(u_int16_t),
		    (caddr_t) &cpi);
		spi = ntohl(htons(cpi));
		break;
	default:
		panic("%s: unknown/unsupported security protocol %d",
		    __func__, sproto);
	}

	/*
	 * Find tunnel control block and (indirectly) call the appropriate
	 * kernel crypto routine. The resulting mbuf chain is a valid
	 * IP packet ready to go through input processing.
	 */

	memset(&dst_address, 0, sizeof(dst_address));
	dst_address.sa.sa_family = af;

	switch (af) {
	case AF_INET:
		dst_address.sin.sin_len = sizeof(struct sockaddr_in);
		m_copydata(m, offsetof(struct ip, ip_dst),
		    sizeof(struct in_addr),
		    (caddr_t) &(dst_address.sin.sin_addr));
		break;

#ifdef INET6
	case AF_INET6:
		dst_address.sin6.sin6_len = sizeof(struct sockaddr_in6);
		m_copydata(m, offsetof(struct ip6_hdr, ip6_dst),
		    sizeof(struct in6_addr),
		    (caddr_t) &(dst_address.sin6.sin6_addr));
		in6_recoverscope(&dst_address.sin6,
		    &dst_address.sin6.sin6_addr);
		break;
#endif /* INET6 */

	default:
		DPRINTF(("%s: unsupported protocol family %d\n", __func__, af));
		IPSEC_ISTAT(esps_nopf, ahs_nopf, ipcomps_nopf);
		error = EPFNOSUPPORT;
		goto drop;
	}

	tdbp = gettdb(rtable_l2(m->m_pkthdr.ph_rtableid),
	    spi, &dst_address, sproto);
	if (tdbp == NULL) {
		DPRINTF(("%s: could not find SA for packet to %s, spi %08x\n",
		    __func__,
		    ipsp_address(&dst_address, buf, sizeof(buf)), ntohl(spi)));
		IPSEC_ISTAT(esps_notdb, ahs_notdb, ipcomps_notdb);
		error = ENOENT;
		goto drop;
	}

	if (tdbp->tdb_flags & TDBF_INVALID) {
		DPRINTF(("%s: attempted to use invalid SA %s/%08x/%u\n",
		    __func__, ipsp_address(&dst_address, buf,
		    sizeof(buf)), ntohl(spi), tdbp->tdb_sproto));
		IPSEC_ISTAT(esps_invalid, ahs_invalid, ipcomps_invalid);
		error = EINVAL;
		goto drop;
	}

	if (udpencap && !(tdbp->tdb_flags & TDBF_UDPENCAP)) {
		DPRINTF(("%s: attempted to use non-udpencap SA %s/%08x/%u\n",
		    __func__, ipsp_address(&dst_address, buf,
		    sizeof(buf)), ntohl(spi), tdbp->tdb_sproto));
		espstat_inc(esps_udpinval);
		error = EINVAL;
		goto drop;
	}

	if (!udpencap && (tdbp->tdb_flags & TDBF_UDPENCAP)) {
		DPRINTF(("%s: attempted to use udpencap SA %s/%08x/%u\n",
		    __func__, ipsp_address(&dst_address, buf,
		    sizeof(buf)), ntohl(spi), tdbp->tdb_sproto));
		espstat_inc(esps_udpneeded);
		error = EINVAL;
		goto drop;
	}

	if (tdbp->tdb_xform == NULL) {
		DPRINTF(("%s: attempted to use uninitialized SA %s/%08x/%u\n",
		    __func__, ipsp_address(&dst_address, buf,
		    sizeof(buf)), ntohl(spi), tdbp->tdb_sproto));
		IPSEC_ISTAT(esps_noxform, ahs_noxform, ipcomps_noxform);
		error = ENXIO;
		goto drop;
	}

	if (sproto != IPPROTO_IPCOMP) {
		if ((encif = enc_getif(tdbp->tdb_rdomain,
		    tdbp->tdb_tap)) == NULL) {
			DPRINTF(("%s: no enc%u interface for SA %s/%08x/%u\n",
			    __func__,
			    tdbp->tdb_tap, ipsp_address(&dst_address, buf,
			    sizeof(buf)), ntohl(spi), tdbp->tdb_sproto));
			IPSEC_ISTAT(esps_pdrops, ahs_pdrops, ipcomps_pdrops);
			error = EACCES;
			goto drop;
		}

		/* XXX This conflicts with the scoped nature of IPv6 */
		m->m_pkthdr.ph_ifidx = encif->if_index;
	}

	/* Register first use, setup expiration timer. */
	if (tdbp->tdb_first_use == 0) {
		tdbp->tdb_first_use = time_second;
		if (tdbp->tdb_flags & TDBF_FIRSTUSE)
			timeout_add_sec(&tdbp->tdb_first_tmo,
			    tdbp->tdb_exp_first_use);
		if (tdbp->tdb_flags & TDBF_SOFT_FIRSTUSE)
			timeout_add_sec(&tdbp->tdb_sfirst_tmo,
			    tdbp->tdb_soft_first_use);
	}

	tdbp->tdb_ipackets++;
	tdbp->tdb_ibytes += m->m_pkthdr.len;

	/*
	 * Call appropriate transform and return -- callback takes care of
	 * everything else.
	 */
	error = (*(tdbp->tdb_xform->xf_input))(m, tdbp, skip, protoff);
	if (error) {
		ipsecstat_inc(ipsec_idrops);
		tdbp->tdb_idrops++;
	}
	return error;

 drop:
	ipsecstat_inc(ipsec_idrops);
	if (tdbp != NULL)
		tdbp->tdb_idrops++;
	m_freem(m);
	return error;
}

void
ipsec_input_cb(struct cryptop *crp)
{
	struct tdb_crypto *tc = (struct tdb_crypto *) crp->crp_opaque;
	struct mbuf *m = (struct mbuf *) crp->crp_buf;
	struct tdb *tdb = NULL;
	int clen, error;

	if (m == NULL) {
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		ipsecstat_inc(ipsec_crypto);
		goto droponly;
	}


	NET_LOCK();
	tdb = gettdb(tc->tc_rdomain, tc->tc_spi, &tc->tc_dst, tc->tc_proto);
	if (tdb == NULL) {
		DPRINTF(("%s: TDB is expired while in crypto", __func__));
		ipsecstat_inc(ipsec_notdb);
		goto baddone;
	}

	/* Check for crypto errors */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (tdb->tdb_cryptoid != 0)
				tdb->tdb_cryptoid = crp->crp_sid;
			NET_UNLOCK();
			crypto_dispatch(crp);
			return;
		}
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		ipsecstat_inc(ipsec_noxform);
		goto baddone;
	}

	/* Length of data after processing */
	clen = crp->crp_olen;

	/* Release the crypto descriptors */
	crypto_freereq(crp);

	switch (tdb->tdb_sproto) {
	case IPPROTO_ESP:
		error = esp_input_cb(tdb, tc, m, clen);
		break;
	case IPPROTO_AH:
		error = ah_input_cb(tdb, tc, m, clen);
		break;
	case IPPROTO_IPCOMP:
		error = ipcomp_input_cb(tdb, tc, m, clen);
		break;
	default:
		panic("%s: unknown/unsupported security protocol %d",
		    __func__, tdb->tdb_sproto);
	}

	NET_UNLOCK();
	if (error) {
		ipsecstat_inc(ipsec_idrops);
		tdb->tdb_idrops++;
	}
	return;

 baddone:
	NET_UNLOCK();
 droponly:
	ipsecstat_inc(ipsec_idrops);
	if (tdb != NULL)
		tdb->tdb_idrops++;
	free(tc, M_XDATA, 0);
	m_freem(m);
	crypto_freereq(crp);
}

/*
 * IPsec input callback, called by the transform callback. Takes care of
 * filtering and other sanity checks on the processed packet.
 */
int
ipsec_common_input_cb(struct mbuf *m, struct tdb *tdbp, int skip, int protoff)
{
	int af, sproto;
	u_int8_t prot;

#if NBPFILTER > 0
	struct ifnet *encif;
#endif

	struct ip *ip, ipn;

#ifdef INET6
	struct ip6_hdr *ip6, ip6n;
#endif /* INET6 */
	struct m_tag *mtag;
	struct tdb_ident *tdbi;

#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	af = tdbp->tdb_dst.sa.sa_family;
	sproto = tdbp->tdb_sproto;

	tdbp->tdb_last_used = time_second;

	/* Sanity check */
	if (m == NULL) {
		/* The called routine will print a message if necessary */
		IPSEC_ISTAT(esps_badkcr, ahs_badkcr, ipcomps_badkcr);
		return -1;
	}

	/* Fix IPv4 header */
	if (af == AF_INET) {
		if ((m->m_len < skip) && ((m = m_pullup(m, skip)) == NULL)) {
			DPRINTF(("%s: processing failed for SA %s/%08x\n",
			    __func__, ipsp_address(&tdbp->tdb_dst,
			    buf, sizeof(buf)), ntohl(tdbp->tdb_spi)));
			IPSEC_ISTAT(esps_hdrops, ahs_hdrops, ipcomps_hdrops);
			return -1;
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
				IPSEC_ISTAT(esps_hdrops, ahs_hdrops,
				    ipcomps_hdrops);
				return -1;
			}
			/* ipn will now contain the inner IPv4 header */
			m_copydata(m, skip, sizeof(struct ip),
			    (caddr_t) &ipn);
		}

#ifdef INET6
		/* IPv6-in-IP encapsulation. */
		if (prot == IPPROTO_IPV6) {
			if (m->m_pkthdr.len - skip < sizeof(struct ip6_hdr)) {
				m_freem(m);
				IPSEC_ISTAT(esps_hdrops, ahs_hdrops,
				    ipcomps_hdrops);
				return -1;
			}
			/* ip6n will now contain the inner IPv6 header. */
			m_copydata(m, skip, sizeof(struct ip6_hdr),
			    (caddr_t) &ip6n);
		}
#endif /* INET6 */
	}

#ifdef INET6
	/* Fix IPv6 header */
	if (af == AF_INET6)
	{
		if (m->m_len < sizeof(struct ip6_hdr) &&
		    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {

			DPRINTF(("%s: processing failed for SA %s/%08x\n",
			    __func__, ipsp_address(&tdbp->tdb_dst,
			    buf, sizeof(buf)), ntohl(tdbp->tdb_spi)));

			IPSEC_ISTAT(esps_hdrops, ahs_hdrops, ipcomps_hdrops);
			return -1;
		}

		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_plen = htons(m->m_pkthdr.len - skip);

		/* Save protocol */
		m_copydata(m, protoff, 1, (caddr_t) &prot);

		/* IP-in-IP encapsulation */
		if (prot == IPPROTO_IPIP) {
			if (m->m_pkthdr.len - skip < sizeof(struct ip)) {
				m_freem(m);
				IPSEC_ISTAT(esps_hdrops, ahs_hdrops,
				    ipcomps_hdrops);
				return -1;
			}
			/* ipn will now contain the inner IPv4 header */
			m_copydata(m, skip, sizeof(struct ip), (caddr_t) &ipn);
		}

		/* IPv6-in-IP encapsulation */
		if (prot == IPPROTO_IPV6) {
			if (m->m_pkthdr.len - skip < sizeof(struct ip6_hdr)) {
				m_freem(m);
				IPSEC_ISTAT(esps_hdrops, ahs_hdrops,
				    ipcomps_hdrops);
				return -1;
			}
			/* ip6n will now contain the inner IPv6 header. */
			m_copydata(m, skip, sizeof(struct ip6_hdr),
			    (caddr_t) &ip6n);
		}
	}
#endif /* INET6 */

	/*
	 * Fix TCP/UDP checksum of UDP encapsulated transport mode ESP packet.
	 * (RFC3948 3.1.2)
	 */
	if ((af == AF_INET || af == AF_INET6) &&
	    (tdbp->tdb_flags & TDBF_UDPENCAP) &&
	    (tdbp->tdb_flags & TDBF_TUNNELING) == 0) {
		u_int16_t cksum;

		switch (prot) {
		case IPPROTO_UDP:
			if (m->m_pkthdr.len < skip + sizeof(struct udphdr)) {
				m_freem(m);
				IPSEC_ISTAT(esps_hdrops, ahs_hdrops,
				    ipcomps_hdrops);
				return -1;
			}
			cksum = 0;
			m_copyback(m, skip + offsetof(struct udphdr, uh_sum),
			    sizeof(cksum), &cksum, M_NOWAIT);
#ifdef INET6
			if (af == AF_INET6) {
				cksum = in6_cksum(m, IPPROTO_UDP, skip,
				    m->m_pkthdr.len - skip);
				m_copyback(m, skip + offsetof(struct udphdr,
				    uh_sum), sizeof(cksum), &cksum, M_NOWAIT);
			}
#endif
			break;
		case IPPROTO_TCP:
			if (m->m_pkthdr.len < skip + sizeof(struct tcphdr)) {
				m_freem(m);
				IPSEC_ISTAT(esps_hdrops, ahs_hdrops,
				    ipcomps_hdrops);
				return -1;
			}
			cksum = 0;
			m_copyback(m, skip + offsetof(struct tcphdr, th_sum),
			    sizeof(cksum), &cksum, M_NOWAIT);
			if (af == AF_INET)
				cksum = in4_cksum(m, IPPROTO_TCP, skip,
				    m->m_pkthdr.len - skip);
#ifdef INET6
			else if (af == AF_INET6)
				cksum = in6_cksum(m, IPPROTO_TCP, skip,
				    m->m_pkthdr.len - skip);
#endif
			m_copyback(m, skip + offsetof(struct tcphdr, th_sum),
			    sizeof(cksum), &cksum, M_NOWAIT);
			break;
		}
	}

	/*
	 * Record what we've done to the packet (under what SA it was
	 * processed).
	 */
	if (tdbp->tdb_sproto != IPPROTO_IPCOMP) {
		mtag = m_tag_get(PACKET_TAG_IPSEC_IN_DONE,
		    sizeof(struct tdb_ident), M_NOWAIT);
		if (mtag == NULL) {
			m_freem(m);
			DPRINTF(("%s: failed to get tag\n", __func__));
			IPSEC_ISTAT(esps_hdrops, ahs_hdrops, ipcomps_hdrops);
			return -1;
		}

		tdbi = (struct tdb_ident *)(mtag + 1);
		tdbi->dst = tdbp->tdb_dst;
		tdbi->proto = tdbp->tdb_sproto;
		tdbi->spi = tdbp->tdb_spi;
		tdbi->rdomain = tdbp->tdb_rdomain;

		m_tag_prepend(m, mtag);
	}

	switch (sproto) {
	case IPPROTO_ESP:
		/* Packet is confidential ? */
		if (tdbp->tdb_encalgxform)
			m->m_flags |= M_CONF;

		/* Check if we had authenticated ESP. */
		if (tdbp->tdb_authalgxform)
			m->m_flags |= M_AUTH;
		break;
	case IPPROTO_AH:
		m->m_flags |= M_AUTH;
		break;
	case IPPROTO_IPCOMP:
		m->m_flags |= M_COMP;
		break;
	default:
		panic("%s: unknown/unsupported security protocol %d",
		    __func__, sproto);
	}

#if NPF > 0
	/* Add pf tag if requested. */
	pf_tag_packet(m, tdbp->tdb_tag, -1);
	pf_pkt_addr_changed(m);
#endif

	if (tdbp->tdb_flags & TDBF_TUNNELING)
		m->m_flags |= M_TUNNEL;

	ipsecstat_add(ipsec_idecompbytes, m->m_pkthdr.len);
	tdbp->tdb_idecompbytes += m->m_pkthdr.len;

#if NBPFILTER > 0
	if ((encif = enc_getif(tdbp->tdb_rdomain, tdbp->tdb_tap)) != NULL) {
		encif->if_ipackets++;
		encif->if_ibytes += m->m_pkthdr.len;

		if (encif->if_bpf) {
			struct enchdr hdr;

			hdr.af = af;
			hdr.spi = tdbp->tdb_spi;
			hdr.flags = m->m_flags & (M_AUTH|M_CONF);

			bpf_mtap_hdr(encif->if_bpf, (char *)&hdr,
			    ENC_HDRLEN, m, BPF_DIRECTION_IN, NULL);
		}
	}
#endif

#if NPF > 0
	/*
	 * The ip_deliver() shortcut avoids running through ip_input() with the
	 * same IP header twice.  Packets in transport mode have to be be
	 * passed to pf explicitly.  In tunnel mode the inner IP header will
	 * run through ip_input() and pf anyway.
	 */
	if ((tdbp->tdb_flags & TDBF_TUNNELING) == 0) {
		struct ifnet *ifp;

		/* This is the enc0 interface unless for ipcomp. */
		if ((ifp = if_get(m->m_pkthdr.ph_ifidx)) == NULL) {
			m_freem(m);
			return -1;
		}
		if (pf_test(af, PF_IN, ifp, &m) != PF_PASS) {
			if_put(ifp);
			m_freem(m);
			return -1;
		}
		if_put(ifp);
		if (m == NULL)
			return -1;
	}
#endif
	/* Call the appropriate IPsec transform callback. */
	ip_deliver(&m, &skip, prot, af);
	return 0;
#undef IPSEC_ISTAT
}

int
ipsec_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error;

	switch (name[0]) {
	case IPCTL_IPSEC_ENC_ALGORITHM:
		NET_LOCK();
		error = sysctl_tstring(oldp, oldlenp, newp, newlen,
		    ipsec_def_enc, sizeof(ipsec_def_enc));
		NET_UNLOCK();
		return (error);
	case IPCTL_IPSEC_AUTH_ALGORITHM:
		NET_LOCK();
		error = sysctl_tstring(oldp, oldlenp, newp, newlen,
		    ipsec_def_auth, sizeof(ipsec_def_auth));
		NET_UNLOCK();
		return (error);
	case IPCTL_IPSEC_IPCOMP_ALGORITHM:
		NET_LOCK();
		error = sysctl_tstring(oldp, oldlenp, newp, newlen,
		    ipsec_def_comp, sizeof(ipsec_def_comp));
		NET_UNLOCK();
		return (error);
	case IPCTL_IPSEC_STATS:
		return (ipsec_sysctl_ipsecstat(oldp, oldlenp, newp));
	default:
		if (name[0] < IPSEC_MAXID) {
			NET_LOCK();
			error = sysctl_int_arr(ipsecctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);
			NET_UNLOCK();
			return (error);
		}
		return (EOPNOTSUPP);
	}
}

int
esp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case ESPCTL_STATS:
		return (esp_sysctl_espstat(oldp, oldlenp, newp));
	default:
		if (name[0] < ESPCTL_MAXID) {
			NET_LOCK();
			error = sysctl_int_arr(espctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);
			NET_UNLOCK();
			return (error);
		}
		return (ENOPROTOOPT);
	}
}

int
esp_sysctl_espstat(void *oldp, size_t *oldlenp, void *newp)
{
	struct espstat espstat;

	CTASSERT(sizeof(espstat) == (esps_ncounters * sizeof(uint64_t)));
	memset(&espstat, 0, sizeof espstat);
	counters_read(espcounters, (uint64_t *)&espstat, esps_ncounters);
	return (sysctl_rdstruct(oldp, oldlenp, newp, &espstat,
	    sizeof(espstat)));
}

int
ah_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case AHCTL_STATS:
		return ah_sysctl_ahstat(oldp, oldlenp, newp);
	default:
		if (name[0] < AHCTL_MAXID) {
			NET_LOCK();
			error = sysctl_int_arr(ahctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);
			NET_UNLOCK();
			return (error);
		}
		return (ENOPROTOOPT);
	}
}

int
ah_sysctl_ahstat(void *oldp, size_t *oldlenp, void *newp)
{
	struct ahstat ahstat;

	CTASSERT(sizeof(ahstat) == (ahs_ncounters * sizeof(uint64_t)));
	memset(&ahstat, 0, sizeof ahstat);
	counters_read(ahcounters, (uint64_t *)&ahstat, ahs_ncounters);
	return (sysctl_rdstruct(oldp, oldlenp, newp, &ahstat, sizeof(ahstat)));
}

int
ipcomp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IPCOMPCTL_STATS:
		return ipcomp_sysctl_ipcompstat(oldp, oldlenp, newp);
	default:
		if (name[0] < IPCOMPCTL_MAXID) {
			NET_LOCK();
			error = sysctl_int_arr(ipcompctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);
			NET_UNLOCK();
			return (error);
		}
		return (ENOPROTOOPT);
	}
}

int
ipcomp_sysctl_ipcompstat(void *oldp, size_t *oldlenp, void *newp)
{
	struct ipcompstat ipcompstat;

	CTASSERT(sizeof(ipcompstat) == (ipcomps_ncounters * sizeof(uint64_t)));
	memset(&ipcompstat, 0, sizeof ipcompstat);
	counters_read(ipcompcounters, (uint64_t *)&ipcompstat,
	    ipcomps_ncounters);
	return (sysctl_rdstruct(oldp, oldlenp, newp, &ipcompstat,
	    sizeof(ipcompstat)));
}

int
ipsec_sysctl_ipsecstat(void *oldp, size_t *oldlenp, void *newp)
{
	struct ipsecstat ipsecstat;

	CTASSERT(sizeof(ipsecstat) == (ipsec_ncounters * sizeof(uint64_t)));
	memset(&ipsecstat, 0, sizeof ipsecstat);
	counters_read(ipseccounters, (uint64_t *)&ipsecstat, ipsec_ncounters);
	return (sysctl_rdstruct(oldp, oldlenp, newp, &ipsecstat,
	    sizeof(ipsecstat)));
}

/* IPv4 AH wrapper. */
int
ah4_input(struct mbuf **mp, int *offp, int proto, int af)
{
	if (
#if NPF > 0
	    ((*mp)->m_pkthdr.pf.flags & PF_TAG_DIVERTED) ||
#endif
	    !ah_enable)
		return rip_input(mp, offp, proto, af);

	ipsec_common_input(*mp, *offp, offsetof(struct ip, ip_p), AF_INET,
	    proto, 0);
	return IPPROTO_DONE;
}

void
ah4_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return;

	ipsec_common_ctlinput(rdomain, cmd, sa, v, IPPROTO_AH);
}

/* IPv4 ESP wrapper. */
int
esp4_input(struct mbuf **mp, int *offp, int proto, int af)
{
	if (
#if NPF > 0
	    ((*mp)->m_pkthdr.pf.flags & PF_TAG_DIVERTED) ||
#endif
	    !esp_enable)
		return rip_input(mp, offp, proto, af);

	ipsec_common_input(*mp, *offp, offsetof(struct ip, ip_p), AF_INET,
	    proto, 0);
	return IPPROTO_DONE;
}

/* IPv4 IPCOMP wrapper */
int
ipcomp4_input(struct mbuf **mp, int *offp, int proto, int af)
{
	if (
#if NPF > 0
	    ((*mp)->m_pkthdr.pf.flags & PF_TAG_DIVERTED) ||
#endif
	    !ipcomp_enable)
		return rip_input(mp, offp, proto, af);

	ipsec_common_input(*mp, *offp, offsetof(struct ip, ip_p), AF_INET,
	    proto, 0);
	return IPPROTO_DONE;
}

void
ipsec_common_ctlinput(u_int rdomain, int cmd, struct sockaddr *sa,
    void *v, int proto)
{
	struct ip *ip = v;

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
			return;

		memset(&dst, 0, sizeof(struct sockaddr_in));
		dst.sin_family = AF_INET;
		dst.sin_len = sizeof(struct sockaddr_in);
		dst.sin_addr.s_addr = ip->ip_dst.s_addr;

		memcpy(&spi, (caddr_t)ip + hlen, sizeof(u_int32_t));

		tdbp = gettdb(rdomain, spi, (union sockaddr_union *)&dst,
		    proto);
		if (tdbp == NULL || tdbp->tdb_flags & TDBF_INVALID)
			return;

		/* Walk the chain backwards to the first tdb */
		NET_ASSERT_LOCKED();
		for (; tdbp; tdbp = tdbp->tdb_inext) {
			if (tdbp->tdb_flags & TDBF_INVALID ||
			    (adjust = ipsec_hdrsz(tdbp)) == -1)
				return;

			mtu -= adjust;

			/* Store adjusted MTU in tdb */
			tdbp->tdb_mtu = mtu;
			tdbp->tdb_mtutimeout = time_second +
			    ip_mtudisc_timeout;
			DPRINTF(("%s: spi %08x mtu %d adjust %ld\n", __func__,
			    ntohl(tdbp->tdb_spi), tdbp->tdb_mtu,
			    adjust));
		}
	}
}

void
udpencap_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	struct ip *ip = v;
	struct tdb *tdbp;
	struct icmp *icp;
	u_int32_t mtu;
	ssize_t adjust;
	struct sockaddr_in dst, src;
	union sockaddr_union *su_dst, *su_src;

	NET_ASSERT_LOCKED();

	icp = (struct icmp *)((caddr_t) ip - offsetof(struct icmp, icmp_ip));
	mtu = ntohs(icp->icmp_nextmtu);

	/*
	 * Ignore the packet, if we do not receive a MTU
	 * or the MTU is too small to be acceptable.
	 */
	if (mtu < 296)
		return;

	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);
	dst.sin_addr.s_addr = ip->ip_dst.s_addr;
	su_dst = (union sockaddr_union *)&dst;
	memset(&src, 0, sizeof(src));
	src.sin_family = AF_INET;
	src.sin_len = sizeof(struct sockaddr_in);
	src.sin_addr.s_addr = ip->ip_src.s_addr;
	su_src = (union sockaddr_union *)&src;

	tdbp = gettdbbysrcdst(rdomain, 0, su_src, su_dst, IPPROTO_ESP);

	for (; tdbp != NULL; tdbp = tdbp->tdb_snext) {
		if (tdbp->tdb_sproto == IPPROTO_ESP &&
		    ((tdbp->tdb_flags & (TDBF_INVALID|TDBF_UDPENCAP)) ==
		    TDBF_UDPENCAP) &&
		    !memcmp(&tdbp->tdb_dst, &dst, su_dst->sa.sa_len) &&
		    !memcmp(&tdbp->tdb_src, &src, su_src->sa.sa_len)) {
			if ((adjust = ipsec_hdrsz(tdbp)) != -1) {
				/* Store adjusted MTU in tdb */
				tdbp->tdb_mtu = mtu - adjust;
				tdbp->tdb_mtutimeout = time_second +
				    ip_mtudisc_timeout;
				DPRINTF(("%s: spi %08x mtu %d adjust %ld\n",
				    __func__,
				    ntohl(tdbp->tdb_spi), tdbp->tdb_mtu,
				    adjust));
			}
		}
	}
}

void
esp4_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return;

	ipsec_common_ctlinput(rdomain, cmd, sa, v, IPPROTO_ESP);
}

#ifdef INET6
/* IPv6 AH wrapper. */
int
ah6_input(struct mbuf **mp, int *offp, int proto, int af)
{
	int l = 0;
	int protoff, nxt;
	struct ip6_ext ip6e;

	if (
#if NPF > 0
	    ((*mp)->m_pkthdr.pf.flags & PF_TAG_DIVERTED) ||
#endif
	    !ah_enable)
		return rip6_input(mp, offp, proto, af);

	if (*offp < sizeof(struct ip6_hdr)) {
		DPRINTF(("%s: bad offset\n", __func__));
		ahstat_inc(ahs_hdrops);
		m_freemp(mp);
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
			DPRINTF(("%s: bad packet header chain\n", __func__));
			ahstat_inc(ahs_hdrops);
			m_freemp(mp);
			return IPPROTO_DONE;
		}
		protoff += offsetof(struct ip6_ext, ip6e_nxt);
	}
	ipsec_common_input(*mp, *offp, protoff, AF_INET6, proto, 0);
	return IPPROTO_DONE;
}

/* IPv6 ESP wrapper. */
int
esp6_input(struct mbuf **mp, int *offp, int proto, int af)
{
	int l = 0;
	int protoff, nxt;
	struct ip6_ext ip6e;

	if (
#if NPF > 0
	    ((*mp)->m_pkthdr.pf.flags & PF_TAG_DIVERTED) ||
#endif
	    !esp_enable)
		return rip6_input(mp, offp, proto, af);

	if (*offp < sizeof(struct ip6_hdr)) {
		DPRINTF(("%s: bad offset\n", __func__));
		espstat_inc(esps_hdrops);
		m_freemp(mp);
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
			DPRINTF(("%s: bad packet header chain\n", __func__));
			espstat_inc(esps_hdrops);
			m_freemp(mp);
			return IPPROTO_DONE;
		}
		protoff += offsetof(struct ip6_ext, ip6e_nxt);
	}
	ipsec_common_input(*mp, *offp, protoff, AF_INET6, proto, 0);
	return IPPROTO_DONE;

}

/* IPv6 IPcomp wrapper */
int
ipcomp6_input(struct mbuf **mp, int *offp, int proto, int af)
{
	int l = 0;
	int protoff, nxt;
	struct ip6_ext ip6e;

	if (
#if NPF > 0
	    ((*mp)->m_pkthdr.pf.flags & PF_TAG_DIVERTED) ||
#endif
	    !ipcomp_enable)
		return rip6_input(mp, offp, proto, af);

	if (*offp < sizeof(struct ip6_hdr)) {
		DPRINTF(("%s: bad offset\n", __func__));
		ipcompstat_inc(ipcomps_hdrops);
		m_freemp(mp);
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
				panic("l went zero or negative");
#endif

			nxt = ip6e.ip6e_nxt;
		} while (protoff + l < *offp);

		/* Malformed packet check */
		if (protoff + l != *offp) {
			DPRINTF(("%s: bad packet header chain\n", __func__));
			ipcompstat_inc(ipcomps_hdrops);
			m_freemp(mp);
			return IPPROTO_DONE;
		}

		protoff += offsetof(struct ip6_ext, ip6e_nxt);
	}
	ipsec_common_input(*mp, *offp, protoff, AF_INET6, proto, 0);
	return IPPROTO_DONE;
}
#endif /* INET6 */

int
ipsec_forward_check(struct mbuf *m, int hlen, int af)
{
	struct tdb *tdb;
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	int error = 0;

	/*
	 * IPsec policy check for forwarded packets. Look at
	 * inner-most IPsec SA used.
	 */
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdb = gettdb(tdbi->rdomain, tdbi->spi, &tdbi->dst, tdbi->proto);
	} else
		tdb = NULL;
	ipsp_spd_lookup(m, af, hlen, &error, IPSP_DIRECTION_IN, tdb, NULL, 0);

	return error;
}

int
ipsec_local_check(struct mbuf *m, int hlen, int proto, int af)
{
	struct tdb *tdb;
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	int error = 0;

	/*
	 * If it's a protected packet for us, skip the policy check.
	 * That's because we really only care about the properties of
	 * the protected packet, and not the intermediate versions.
	 * While this is not the most paranoid setting, it allows
	 * some flexibility in handling nested tunnels (in setting up
	 * the policies).
	 */
	if ((proto == IPPROTO_ESP) || (proto == IPPROTO_AH) ||
	    (proto == IPPROTO_IPCOMP))
		return 0;

	/*
	 * If the protected packet was tunneled, then we need to
	 * verify the protected packet's information, not the
	 * external headers. Thus, skip the policy lookup for the
	 * external packet, and keep the IPsec information linked on
	 * the packet header (the encapsulation routines know how
	 * to deal with that).
	 */
	if ((proto == IPPROTO_IPV4) || (proto == IPPROTO_IPV6))
		return 0;

	/*
	 * When processing IPv6 header chains, do not look at the
	 * outer header.  The inner protocol is relevant and will
	 * be checked by the local delivery loop later.
	 */
	if ((af == AF_INET6) && ((proto == IPPROTO_DSTOPTS) ||
	    (proto == IPPROTO_ROUTING) || (proto == IPPROTO_FRAGMENT)))
		return 0;

	/*
	 * If the protected packet is TCP or UDP, we'll do the
	 * policy check in the respective input routine, so we can
	 * check for bypass sockets.
	 */
	if ((proto == IPPROTO_TCP) || (proto == IPPROTO_UDP))
		return 0;

	/*
	 * IPsec policy check for local-delivery packets. Look at the
	 * inner-most SA that protected the packet. This is in fact
	 * a bit too restrictive (it could end up causing packets to
	 * be dropped that semantically follow the policy, e.g., in
	 * certain SA-bundle configurations); but the alternative is
	 * very complicated (and requires keeping track of what
	 * kinds of tunneling headers have been seen in-between the
	 * IPsec headers), and I don't think we lose much functionality
	 * that's needed in the real world (who uses bundles anyway ?).
	 */
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
	if (mtag) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdb = gettdb(tdbi->rdomain, tdbi->spi, &tdbi->dst,
		    tdbi->proto);
	} else
		tdb = NULL;
	ipsp_spd_lookup(m, af, hlen, &error, IPSP_DIRECTION_IN,
	    tdb, NULL, 0);

	return error;
}
