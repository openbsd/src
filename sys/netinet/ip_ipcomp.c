/* $OpenBSD: ip_ipcomp.c,v 1.59 2017/11/08 16:29:20 visa Exp $ */

/*
 * Copyright (c) 2001 Jean-Jacques Bernard-Gundol (jj@wabbitt.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* IP payload compression protocol (IPComp), see RFC 2393 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif				/* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ipcomp.h>
#include <net/pfkeyv2.h>
#include <net/if_enc.h>

#include <crypto/cryptodev.h>
#include <crypto/xform.h>

#include "bpfilter.h"

void ipcomp_output_cb(struct cryptop *);
void ipcomp_input_cb(struct cryptop *);

#ifdef ENCDEBUG
#define DPRINTF(x)      if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

/*
 * ipcomp_attach() is called from the transformation code
 */
int
ipcomp_attach(void)
{
	return 0;
}

/*
 * ipcomp_init() is called when an CPI is being set up.
 */
int
ipcomp_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
	struct comp_algo *tcomp = NULL;
	struct cryptoini cric;

	switch (ii->ii_compalg) {
	case SADB_X_CALG_DEFLATE:
		tcomp = &comp_algo_deflate;
		break;
	case SADB_X_CALG_LZS:
		tcomp = &comp_algo_lzs;
		break;

	default:
		DPRINTF(("%s: unsupported compression algorithm %d specified\n",
		    __func__, ii->ii_compalg));
		return EINVAL;
	}

	tdbp->tdb_compalgxform = tcomp;

	DPRINTF(("%s: initialized TDB with ipcomp algorithm %s\n", __func__,
	    tcomp->name));

	tdbp->tdb_xform = xsp;

	/* Initialize crypto session */
	memset(&cric, 0, sizeof(cric));
	cric.cri_alg = tdbp->tdb_compalgxform->type;

	return crypto_newsession(&tdbp->tdb_cryptoid, &cric, 0);
}

/*
 * ipcomp_zeroize() used when IPCA is deleted
 */
int
ipcomp_zeroize(struct tdb *tdbp)
{
	int err;

	err = crypto_freesession(tdbp->tdb_cryptoid);
	tdbp->tdb_cryptoid = 0;
	return err;
}

/*
 * ipcomp_input() gets called to uncompress an input packet
 */
int
ipcomp_input(struct mbuf *m, struct tdb *tdb, int skip, int protoff)
{
	struct comp_algo *ipcompx = (struct comp_algo *) tdb->tdb_compalgxform;
	struct tdb_crypto *tc;
	int hlen;

	struct cryptodesc *crdc = NULL;
	struct cryptop *crp;

	hlen = IPCOMP_HLENGTH;

	/* Get crypto descriptors */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("%s: failed to acquire crypto descriptors\n", __func__));
		ipcompstat_inc(ipcomps_crypto);
		return ENOBUFS;
	}
	/* Get IPsec-specific opaque pointer */
	tc = malloc(sizeof(*tc), M_XDATA, M_NOWAIT | M_ZERO);
	if (tc == NULL) {
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		ipcompstat_inc(ipcomps_crypto);
		return ENOBUFS;
	}
	crdc = &crp->crp_desc[0];

	crdc->crd_skip = skip + hlen;
	crdc->crd_len = m->m_pkthdr.len - (skip + hlen);
	crdc->crd_inject = skip;

	/* Decompression operation */
	crdc->crd_alg = ipcompx->type;

	/* Crypto operation descriptor */
	crp->crp_ilen = m->m_pkthdr.len - (skip + hlen);
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t)m;
	crp->crp_callback = ipcomp_input_cb;
	crp->crp_sid = tdb->tdb_cryptoid;
	crp->crp_opaque = (caddr_t)tc;

	/* These are passed as-is to the callback */
	tc->tc_skip = skip;
	tc->tc_protoff = protoff;
	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = IPPROTO_IPCOMP;
	tc->tc_rdomain = tdb->tdb_rdomain;
	tc->tc_dst = tdb->tdb_dst;

	return crypto_dispatch(crp);
}

/*
 * IPComp input callback, called directly by the crypto driver
 */
void
ipcomp_input_cb(struct cryptop *crp)
{
	int skip, protoff, roff, hlen = IPCOMP_HLENGTH, clen;
	u_int8_t nproto;
	struct mbuf *m, *m1, *mo;
	struct tdb_crypto *tc;
	struct tdb *tdb;
	struct ipcomp  *ipcomp;
	caddr_t addr;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	tc = (struct tdb_crypto *) crp->crp_opaque;
	skip = tc->tc_skip;
	protoff = tc->tc_protoff;

	m = (struct mbuf *) crp->crp_buf;
	if (m == NULL) {
		/* Shouldn't happen... */
		free(tc, M_XDATA, 0);
		crypto_freereq(crp);
		ipcompstat_inc(ipcomps_crypto);
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		return;
	}

	NET_LOCK();

	tdb = gettdb(tc->tc_rdomain, tc->tc_spi, &tc->tc_dst, tc->tc_proto);
	if (tdb == NULL) {
		free(tc, M_XDATA, 0);
		ipcompstat_inc(ipcomps_notdb);
		DPRINTF(("%s: TDB expired while in crypto", __func__));
		goto baddone;
	}

	/* update the counters */
	tdb->tdb_cur_bytes += m->m_pkthdr.len - (skip + hlen);
	ipcompstat_add(ipcomps_ibytes, m->m_pkthdr.len - (skip + hlen));

	/* Hard expiration */
	if ((tdb->tdb_flags & TDBF_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes)) {
		free(tc, M_XDATA, 0);
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		goto baddone;
	}
	/* Notify on soft expiration */
	if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES;	/* Turn off checking */
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
		free(tc, M_XDATA, 0);
		ipcompstat_inc(ipcomps_noxform);
		DPRINTF(("%s: crypto error %d\n", __func__,
		    crp->crp_etype));
		goto baddone;
	}
	free(tc, M_XDATA, 0);

	/* Length of data after processing */
	clen = crp->crp_olen;

	/* In case it's not done already, adjust the size of the mbuf chain */
	m->m_pkthdr.len = clen + hlen + skip;

	if ((m->m_len < skip + hlen) && (m = m_pullup(m, skip + hlen)) == 0) {
		goto baddone;
	}

	/* Find the beginning of the IPCOMP header */
	m1 = m_getptr(m, skip, &roff);
	if (m1 == NULL) {
		ipcompstat_inc(ipcomps_hdrops);
		DPRINTF(("%s: bad mbuf chain, IPCA %s/%08x\n", __func__,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		goto baddone;
	}
	/* Keep the next protocol field */
	addr = (caddr_t) mtod(m, struct ip *) + skip;
	ipcomp = (struct ipcomp *) addr;
	nproto = ipcomp->ipcomp_nh;

	/* Remove the IPCOMP header from the mbuf */
	if (roff == 0) {
		/* The IPCOMP header is at the beginning of m1 */
		m_adj(m1, hlen);
		if (!(m1->m_flags & M_PKTHDR))
			m->m_pkthdr.len -= hlen;
	} else if (roff + hlen >= m1->m_len) {
		if (roff + hlen > m1->m_len) {
			/* Adjust the next mbuf by the remainder */
			m_adj(m1->m_next, roff + hlen - m1->m_len);

			/*
			 * The second mbuf is guaranteed not to have a
			 * pkthdr...
			 */
			m->m_pkthdr.len -= (roff + hlen - m1->m_len);
		}
		/* Now, let's unlink the mbuf chain for a second... */
		mo = m1->m_next;
		m1->m_next = NULL;

		/* ...and trim the end of the first part of the chain...sick */
		m_adj(m1, -(m1->m_len - roff));
		if (!(m1->m_flags & M_PKTHDR))
			m->m_pkthdr.len -= (m1->m_len - roff);

		/* Finally, let's relink */
		m1->m_next = mo;
	} else {
		memmove(mtod(m1, u_char *) + roff,
		    mtod(m1, u_char *) + roff + hlen,
		    m1->m_len - (roff + hlen));
		m1->m_len -= hlen;
		m->m_pkthdr.len -= hlen;
	}

	/* Release the crypto descriptors */
	crypto_freereq(crp);

	/* Restore the Next Protocol field */
	m_copyback(m, protoff, sizeof(u_int8_t), &nproto, M_NOWAIT);

	/* Back to generic IPsec input processing */
	ipsec_common_input_cb(m, tdb, skip, protoff);
	NET_UNLOCK();
	return;

baddone:
	NET_UNLOCK();

	m_freem(m);

	crypto_freereq(crp);
}

/*
 * IPComp output routine, called by ipsp_process_packet()
 */
int
ipcomp_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
    int protoff)
{
	struct comp_algo *ipcompx = (struct comp_algo *) tdb->tdb_compalgxform;
	int             hlen;
	struct cryptodesc *crdc = NULL;
	struct cryptop *crp;
	struct tdb_crypto *tc;
	struct mbuf    *mi;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif
#if NBPFILTER > 0
	struct ifnet *encif;

	if ((encif = enc_getif(0, tdb->tdb_tap)) != NULL) {
		encif->if_opackets++;
		encif->if_obytes += m->m_pkthdr.len;

		if (encif->if_bpf) {
			struct enchdr hdr;

			memset(&hdr, 0, sizeof(hdr));

			hdr.af = tdb->tdb_dst.sa.sa_family;
			hdr.spi = tdb->tdb_spi;

			bpf_mtap_hdr(encif->if_bpf, (char *)&hdr,
			    ENC_HDRLEN, m, BPF_DIRECTION_OUT, NULL);
		}
	}
#endif
	hlen = IPCOMP_HLENGTH;

	ipcompstat_inc(ipcomps_output);

	switch (tdb->tdb_dst.sa.sa_family) {
	case AF_INET:
		/* Check for IPv4 maximum packet size violations */
		/*
		 * Since compression is going to reduce the size, no need to
		 * worry
		 */
		if (m->m_pkthdr.len + hlen > IP_MAXPACKET) {
			DPRINTF(("%s: packet in IPCA %s/%08x got too big\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			m_freem(m);
			ipcompstat_inc(ipcomps_toobig);
			return EMSGSIZE;
		}
		break;

#ifdef INET6
	case AF_INET6:
		/* Check for IPv6 maximum packet size violations */
		if (m->m_pkthdr.len + hlen > IPV6_MAXPACKET) {
			DPRINTF(("%s: packet in IPCA %s/%08x got too big\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			m_freem(m);
			ipcompstat_inc(ipcomps_toobig);
			return EMSGSIZE;
		}
		break;
#endif /* INET6 */

	default:
		DPRINTF(("%s: unknown/unsupported protocol family %d, "
		    "IPCA %s/%08x\n", __func__, tdb->tdb_dst.sa.sa_family,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		m_freem(m);
		ipcompstat_inc(ipcomps_nopf);
		return EPFNOSUPPORT;
	}

	/* Update the counters */

	tdb->tdb_cur_bytes += m->m_pkthdr.len - skip;
	ipcompstat_add(ipcomps_obytes, m->m_pkthdr.len - skip);

	/* Hard byte expiration */
	if ((tdb->tdb_flags & TDBF_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes)) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		m_freem(m);
		return EINVAL;
	}
	/* Soft byte expiration */
	if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES;	/* Turn off checking */
	}
	/*
	 * Loop through mbuf chain; if we find a readonly mbuf,
	 * copy the packet.
	 */
	mi = m;
	while (mi != NULL && !M_READONLY(mi))
		mi = mi->m_next;

	if (mi != NULL) {
		struct mbuf *n = m_dup_pkt(m, 0, M_DONTWAIT);

		if (n == NULL) {
			DPRINTF(("%s: bad mbuf chain, IPCA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			ipcompstat_inc(ipcomps_hdrops);
			m_freem(m);
			return ENOBUFS;
		}

		m_freem(m);
		m = n;
	}
	/* Ok now, we can pass to the crypto processing */

	/* Get crypto descriptors */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("%s: failed to acquire crypto descriptors\n", __func__));
		ipcompstat_inc(ipcomps_crypto);
		return ENOBUFS;
	}
	crdc = &crp->crp_desc[0];

	/* Compression descriptor */
	crdc->crd_skip = skip;
	crdc->crd_len = m->m_pkthdr.len - skip;
	crdc->crd_flags = CRD_F_COMP;
	crdc->crd_inject = skip;

	/* Compression operation */
	crdc->crd_alg = ipcompx->type;

	/* IPsec-specific opaque crypto info */
	tc = malloc(sizeof(*tc), M_XDATA, M_NOWAIT | M_ZERO);
	if (tc == NULL) {
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		ipcompstat_inc(ipcomps_crypto);
		return ENOBUFS;
	}

	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = tdb->tdb_sproto;
	tc->tc_skip = skip;
	tc->tc_rdomain = tdb->tdb_rdomain;
	tc->tc_dst = tdb->tdb_dst;

	/* Crypto operation descriptor */
	crp->crp_ilen = m->m_pkthdr.len;	/* Total input length */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t)m;
	crp->crp_callback = ipcomp_output_cb;
	crp->crp_opaque = (caddr_t)tc;
	crp->crp_sid = tdb->tdb_cryptoid;

	return crypto_dispatch(crp);
}

/*
 * IPComp output callback, called directly from the crypto driver
 */
void
ipcomp_output_cb(struct cryptop *crp)
{
	struct tdb_crypto *tc;
	struct tdb *tdb;
	struct mbuf *m, *mo;
	int skip, rlen, roff;
	u_int16_t cpi;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct ipcomp  *ipcomp;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	tc = (struct tdb_crypto *) crp->crp_opaque;
	skip = tc->tc_skip;
	rlen = crp->crp_ilen - skip;

	m = (struct mbuf *) crp->crp_buf;
	if (m == NULL) {
		/* Shouldn't happen... */
		free(tc, M_XDATA, 0);
		crypto_freereq(crp);
		ipcompstat_inc(ipcomps_crypto);
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		return;
	}

	NET_LOCK();

	tdb = gettdb(tc->tc_rdomain, tc->tc_spi, &tc->tc_dst, tc->tc_proto);
	if (tdb == NULL) {
		free(tc, M_XDATA, 0);
		ipcompstat_inc(ipcomps_notdb);
		DPRINTF(("%s: TDB expired while in crypto\n", __func__));
		goto baddone;
	}

	/* Check for crypto errors. */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (tdb->tdb_cryptoid != 0)
				tdb->tdb_cryptoid = crp->crp_sid;
			NET_UNLOCK();
			crypto_dispatch(crp);
			return;
		}
		free(tc, M_XDATA, 0);
		ipcompstat_inc(ipcomps_noxform);
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		goto baddone;
	}
	free(tc, M_XDATA, 0);

	/* Check sizes. */
	if (rlen < crp->crp_olen) {
		/* Compression was useless, we have lost time. */
		crypto_freereq(crp);
		if (ipsp_process_done(m, tdb))
			ipcompstat_inc(ipcomps_outfail);
		NET_UNLOCK();
		return;
	}

	/* Inject IPCOMP header */
	mo = m_makespace(m, skip, IPCOMP_HLENGTH, &roff);
	if (mo == NULL) {
		DPRINTF(("%s: failed to inject IPCOMP header for "
		    "IPCA %s/%08x\n", __func__, ipsp_address(&tdb->tdb_dst, buf,
		     sizeof(buf)), ntohl(tdb->tdb_spi)));
		ipcompstat_inc(ipcomps_wrap);
		goto baddone;
	}

	/* Initialize the IPCOMP header */
	ipcomp = (struct ipcomp *)(mtod(mo, caddr_t) + roff);
	memset(ipcomp, 0, sizeof(struct ipcomp));
	cpi = (u_int16_t) ntohl(tdb->tdb_spi);
	ipcomp->ipcomp_cpi = htons(cpi);

	/* m_pullup before ? */
	switch (tdb->tdb_dst.sa.sa_family) {
	case AF_INET:
		ip = mtod(m, struct ip *);
		ipcomp->ipcomp_nh = ip->ip_p;
		ip->ip_p = IPPROTO_IPCOMP;
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
		ipcomp->ipcomp_nh = ip6->ip6_nxt;
		ip6->ip6_nxt = IPPROTO_IPCOMP;
		break;
#endif
	default:
		DPRINTF(("%s: unsupported protocol family %d, IPCA %s/%08x\n",
		    __func__, tdb->tdb_dst.sa.sa_family,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		ipcompstat_inc(ipcomps_nopf);
		goto baddone;
		break;
	}

	/* Release the crypto descriptor. */
	crypto_freereq(crp);

	if (ipsp_process_done(m, tdb))
		ipcompstat_inc(ipcomps_outfail);
	NET_UNLOCK();
	return;

baddone:
	NET_UNLOCK();

	m_freem(m);

	crypto_freereq(crp);
}
