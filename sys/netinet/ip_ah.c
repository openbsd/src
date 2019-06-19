/*	$OpenBSD: ip_ah.c,v 1.143 2018/08/28 15:15:02 mpi Exp $ */
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis and Niklas Hallqvist.
 *
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 1999 Niklas Hallqvist.
 * Copyright (c) 2001 Angelos D. Keromytis.
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

#include "pfsync.h"

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
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <net/pfkeyv2.h>
#include <net/if_enc.h>

#if NPFSYNC > 0
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#include <crypto/cryptodev.h>
#include <crypto/xform.h>

#include "bpfilter.h"

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

int	ah_massage_headers(struct mbuf **, int, int, int, int);

const unsigned char ipseczeroes[IPSEC_ZEROES_SIZE]; /* zeroes! */


/*
 * ah_attach() is called from the transformation initialization code.
 */
int
ah_attach(void)
{
	return 0;
}

/*
 * ah_init() is called when an SPI is being set up.
 */
int
ah_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
	struct auth_hash *thash = NULL;
	struct cryptoini cria, crin;

	/* Authentication operation. */
	switch (ii->ii_authalg) {
	case SADB_AALG_MD5HMAC:
		thash = &auth_hash_hmac_md5_96;
		break;

	case SADB_AALG_SHA1HMAC:
		thash = &auth_hash_hmac_sha1_96;
		break;

	case SADB_X_AALG_RIPEMD160HMAC:
		thash = &auth_hash_hmac_ripemd_160_96;
		break;

	case SADB_X_AALG_SHA2_256:
		thash = &auth_hash_hmac_sha2_256_128;
		break;

	case SADB_X_AALG_SHA2_384:
		thash = &auth_hash_hmac_sha2_384_192;
		break;

	case SADB_X_AALG_SHA2_512:
		thash = &auth_hash_hmac_sha2_512_256;
		break;

	default:
		DPRINTF(("%s: unsupported authentication algorithm %d"
		    " specified\n", __func__, ii->ii_authalg));
		return EINVAL;
	}

	if (ii->ii_authkeylen != thash->keysize && thash->keysize != 0) {
		DPRINTF(("ah_init(): keylength %d doesn't match algorithm "
		    "%s keysize (%d)\n", ii->ii_authkeylen, thash->name,
		    thash->keysize));
		return EINVAL;
	}

	tdbp->tdb_xform = xsp;
	tdbp->tdb_authalgxform = thash;
	tdbp->tdb_rpl = AH_HMAC_INITIAL_RPL;

	DPRINTF(("%s: initialized TDB with hash algorithm %s\n", __func__,
	    thash->name));

	tdbp->tdb_amxkeylen = ii->ii_authkeylen;
	tdbp->tdb_amxkey = malloc(tdbp->tdb_amxkeylen, M_XDATA, M_WAITOK);

	memcpy(tdbp->tdb_amxkey, ii->ii_authkey, tdbp->tdb_amxkeylen);

	/* Initialize crypto session. */
	memset(&cria, 0, sizeof(cria));
	cria.cri_alg = tdbp->tdb_authalgxform->type;
	cria.cri_klen = ii->ii_authkeylen * 8;
	cria.cri_key = ii->ii_authkey;

	if ((tdbp->tdb_wnd > 0) && (tdbp->tdb_flags & TDBF_ESN)) {
		memset(&crin, 0, sizeof(crin));
		crin.cri_alg = CRYPTO_ESN;
		cria.cri_next = &crin;
	}

	return crypto_newsession(&tdbp->tdb_cryptoid, &cria, 0);
}

/*
 * Paranoia.
 */
int
ah_zeroize(struct tdb *tdbp)
{
	int err;

	if (tdbp->tdb_amxkey) {
		explicit_bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
		free(tdbp->tdb_amxkey, M_XDATA, tdbp->tdb_amxkeylen);
		tdbp->tdb_amxkey = NULL;
	}

	err = crypto_freesession(tdbp->tdb_cryptoid);
	tdbp->tdb_cryptoid = 0;
	return err;
}

/*
 * Massage IPv4/IPv6 headers for AH processing.
 */
int
ah_massage_headers(struct mbuf **m0, int af, int skip, int alg, int out)
{
	struct mbuf *m = *m0;
	unsigned char *ptr;
	int off, count;
	struct ip *ip;
#ifdef INET6
	struct ip6_ext *ip6e;
	struct ip6_hdr ip6;
	int ad, alloc, nxt, noff, error;
#endif /* INET6 */

	switch (af) {
	case AF_INET:
		/*
		 * This is the least painful way of dealing with IPv4 header
		 * and option processing -- just make sure they're in
		 * contiguous memory.
		 */
		*m0 = m = m_pullup(m, skip);
		if (m == NULL) {
			DPRINTF(("%s: m_pullup() failed\n", __func__));
			ahstat_inc(ahs_hdrops);
			return ENOBUFS;
		}

		/* Fix the IP header */
		ip = mtod(m, struct ip *);
		ip->ip_tos = 0;
		ip->ip_ttl = 0;
		ip->ip_sum = 0;
		ip->ip_off = 0;

		ptr = mtod(m, unsigned char *);

		/* IPv4 option processing */
		for (off = sizeof(struct ip); off < skip;) {
			if (ptr[off] != IPOPT_EOL && ptr[off] != IPOPT_NOP &&
			    off + 1 >= skip) {
				DPRINTF(("%s: illegal IPv4 option length for"
				    " option %d\n", __func__, ptr[off]));

				ahstat_inc(ahs_hdrops);
				m_freem(m);
				return EINVAL;
			}

			switch (ptr[off]) {
			case IPOPT_EOL:
				off = skip;  /* End the loop. */
				break;

			case IPOPT_NOP:
				off++;
				break;

			case IPOPT_SECURITY:	/* 0x82 */
			case 0x85:	/* Extended security. */
			case 0x86:	/* Commercial security. */
			case 0x94:	/* Router alert */
			case 0x95:	/* RFC1770 */
				/* Sanity check for option length. */
				if (ptr[off + 1] < 2) {
					DPRINTF(("%s: illegal IPv4 option"
					    " length for option %d\n", __func__,
					    ptr[off]));

					ahstat_inc(ahs_hdrops);
					m_freem(m);
					return EINVAL;
				}

				off += ptr[off + 1];
				break;

			case IPOPT_LSRR:
			case IPOPT_SSRR:
				/* Sanity check for option length. */
				if (ptr[off + 1] < 2) {
					DPRINTF(("%s: illegal IPv4 option"
					    " length for option %d\n", __func__,
					    ptr[off]));

					ahstat_inc(ahs_hdrops);
					m_freem(m);
					return EINVAL;
				}

				/*
				 * On output, if we have either of the
				 * source routing options, we should
				 * swap the destination address of the
				 * IP header with the last address
				 * specified in the option, as that is
				 * what the destination's IP header
				 * will look like.
				 */
				if (out &&
				    ptr[off + 1] >= 2 + sizeof(struct in_addr))
					memcpy(&ip->ip_dst,
					    ptr + off + ptr[off + 1] -
					    sizeof(struct in_addr),
					    sizeof(struct in_addr));

				/* FALLTHROUGH */
			default:
				/* Sanity check for option length. */
				if (ptr[off + 1] < 2) {
					DPRINTF(("%s: illegal IPv4 option"
					    " length for option %d\n", __func__,
					    ptr[off]));
					ahstat_inc(ahs_hdrops);
					m_freem(m);
					return EINVAL;
				}

				/* Zeroize all other options. */
				count = ptr[off + 1];
				memset(ptr + off, 0, count);
				off += count;
				break;
			}

			/* Sanity check. */
			if (off > skip)	{
				DPRINTF(("%s: malformed IPv4 options header\n",
				    __func__));

				ahstat_inc(ahs_hdrops);
				m_freem(m);
				return EINVAL;
			}
		}

		break;

#ifdef INET6
	case AF_INET6:  /* Ugly... */
		/* Copy and "cook" the IPv6 header. */
		m_copydata(m, 0, sizeof(ip6), (caddr_t) &ip6);

		/* We don't do IPv6 Jumbograms. */
		if (ip6.ip6_plen == 0) {
			DPRINTF(("%s: unsupported IPv6 jumbogram", __func__));
			ahstat_inc(ahs_hdrops);
			m_freem(m);
			return EMSGSIZE;
		}

		ip6.ip6_flow = 0;
		ip6.ip6_hlim = 0;
		ip6.ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6.ip6_vfc |= IPV6_VERSION;

		/* Scoped address handling. */
		if (IN6_IS_SCOPE_EMBED(&ip6.ip6_src))
			ip6.ip6_src.s6_addr16[1] = 0;
		if (IN6_IS_SCOPE_EMBED(&ip6.ip6_dst))
			ip6.ip6_dst.s6_addr16[1] = 0;

		/* Done with IPv6 header. */
		error = m_copyback(m, 0, sizeof(struct ip6_hdr), &ip6,
		    M_NOWAIT);
		if (error) {
			DPRINTF(("%s: m_copyback no memory", __func__));
			ahstat_inc(ahs_hdrops);
			m_freem(m);
			return error;
		}

		/* Let's deal with the remaining headers (if any). */
		if (skip - sizeof(struct ip6_hdr) > 0) {
			if (m->m_len <= skip) {
				ptr = malloc(skip - sizeof(struct ip6_hdr),
				    M_XDATA, M_NOWAIT);
				if (ptr == NULL) {
					DPRINTF(("%s: failed to allocate memory"
					    " for IPv6 headers\n", __func__));
					ahstat_inc(ahs_hdrops);
					m_freem(m);
					return ENOBUFS;
				}

				/*
				 * Copy all the protocol headers after
				 * the IPv6 header.
				 */
				m_copydata(m, sizeof(struct ip6_hdr),
				    skip - sizeof(struct ip6_hdr), ptr);
				alloc = 1;
			} else {
				/* No need to allocate memory. */
				ptr = mtod(m, unsigned char *) +
				    sizeof(struct ip6_hdr);
				alloc = 0;
			}
		} else
			break;

		nxt = ip6.ip6_nxt;  /* Next header type. */

		for (off = 0; off < skip - sizeof(struct ip6_hdr);) {
			if (off + sizeof(struct ip6_ext) >
			    skip - sizeof(struct ip6_hdr))
				goto error6;
			ip6e = (struct ip6_ext *)(ptr + off);

			switch (nxt) {
			case IPPROTO_HOPOPTS:
			case IPPROTO_DSTOPTS:
				noff = off + ((ip6e->ip6e_len + 1) << 3);

				/* Sanity check. */
				if (noff > skip - sizeof(struct ip6_hdr))
					goto error6;

				/*
				 * Zero out mutable options.
				 */
				for (count = off + sizeof(struct ip6_ext);
				     count < noff;) {
					if (ptr[count] == IP6OPT_PAD1) {
						count++;
						continue; /* Skip padding. */
					}

					if (count + 2 > noff)
						goto error6;
					ad = ptr[count + 1] + 2;
					if (count + ad > noff)
						goto error6;

					/* If mutable option, zeroize. */
					if (ptr[count] & IP6OPT_MUTABLE)
						memset(ptr + count, 0, ad);

					count += ad;
				}

				if (count != noff)
					goto error6;
				break;

			case IPPROTO_ROUTING:
				/*
				 * Always include routing headers in
				 * computation.
				 */
			    {
				struct ip6_rthdr *rh;

				rh = (struct ip6_rthdr *)(ptr + off);
				/*
				 * must adjust content to make it look like
				 * its final form (as seen at the final
				 * destination).
				 * we only know how to massage type 0 routing
				 * header.
				 */
				if (out && rh->ip6r_type == IPV6_RTHDR_TYPE_0) {
					struct ip6_rthdr0 *rh0;
					struct in6_addr *addr, finaldst;
					int i;

					rh0 = (struct ip6_rthdr0 *)rh;
					addr = (struct in6_addr *)(rh0 + 1);

					for (i = 0; i < rh0->ip6r0_segleft; i++)
						if (IN6_IS_SCOPE_EMBED(&addr[i]))
							addr[i].s6_addr16[1] = 0;

					finaldst = addr[rh0->ip6r0_segleft - 1];
					memmove(&addr[1], &addr[0],
					    sizeof(struct in6_addr) *
					    (rh0->ip6r0_segleft - 1));

					m_copydata(m, 0, sizeof(ip6),
					    (caddr_t)&ip6);
					addr[0] = ip6.ip6_dst;
					ip6.ip6_dst = finaldst;
					error = m_copyback(m, 0, sizeof(ip6),
					    &ip6, M_NOWAIT);
					if (error) {
						if (alloc)
							free(ptr, M_XDATA, 0);
						ahstat_inc(ahs_hdrops);
						m_freem(m);
						return error;
					}
					rh0->ip6r0_segleft = 0;
				}
				break;
			    }

			default:
				DPRINTF(("%s: unexpected IPv6 header type %d\n",
				    __func__, off));
error6:
				if (alloc)
					free(ptr, M_XDATA, 0);
				ahstat_inc(ahs_hdrops);
				m_freem(m);
				return EINVAL;
			}

			/* Advance. */
			off += ((ip6e->ip6e_len + 1) << 3);
			nxt = ip6e->ip6e_nxt;
		}

		/* Copyback and free, if we allocated. */
		if (alloc) {
			error = m_copyback(m, sizeof(struct ip6_hdr),
			    skip - sizeof(struct ip6_hdr), ptr, M_NOWAIT);
			free(ptr, M_XDATA, 0);
			if (error) {
				ahstat_inc(ahs_hdrops);
				m_freem(m);
				return error;
			}
		}

		break;
#endif /* INET6 */
	}

	return 0;
}

/*
 * ah_input() gets called to verify that an input packet
 * passes authentication.
 */
int
ah_input(struct mbuf *m, struct tdb *tdb, int skip, int protoff)
{
	struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
	struct tdb_crypto *tc = NULL;
	u_int32_t btsx, esn;
	u_int8_t hl;
	int error, rplen;
	u_int64_t ibytes;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif
	struct cryptodesc *crda = NULL;
	struct cryptop *crp = NULL;

	rplen = AH_FLENGTH + sizeof(u_int32_t);

	/* Save the AH header, we use it throughout. */
	m_copydata(m, skip + offsetof(struct ah, ah_hl), sizeof(u_int8_t),
	    (caddr_t) &hl);

	/* Replay window checking, if applicable. */
	if (tdb->tdb_wnd > 0) {
		m_copydata(m, skip + offsetof(struct ah, ah_rpl),
		    sizeof(u_int32_t), (caddr_t) &btsx);
		btsx = ntohl(btsx);

		switch (checkreplaywindow(tdb, btsx, &esn, 0)) {
		case 0: /* All's well. */
			break;
		case 1:
			DPRINTF(("%s: replay counter wrapped for SA %s/%08x\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_wrap);
			error = ENOBUFS;
			goto drop;
		case 2:
			DPRINTF(("%s: old packet received in SA %s/%08x\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_replay);
			error = ENOBUFS;
			goto drop;
		case 3:
			DPRINTF(("%s: duplicate packet received in SA "
			    "%s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_replay);
			error = ENOBUFS;
			goto drop;
		default:
			DPRINTF(("%s: bogus value from "
			    "checkreplaywindow() in SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_replay);
			error = ENOBUFS;
			goto drop;
		}
	}

	/* Verify AH header length. */
	if (hl * sizeof(u_int32_t) != ahx->authsize + rplen - AH_FLENGTH) {
		DPRINTF(("%s: bad authenticator length %ld for packet "
		    "in SA %s/%08x\n", __func__, hl * sizeof(u_int32_t),
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		ahstat_inc(ahs_badauthl);
		error = EACCES;
		goto drop;
	}
	if (skip + ahx->authsize + rplen > m->m_pkthdr.len) {
		DPRINTF(("%s: bad mbuf length %d (expecting %d) "
		    "for packet in SA %s/%08x\n", __func__,
		    m->m_pkthdr.len, skip + ahx->authsize + rplen,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		ahstat_inc(ahs_badauthl);
		error = EACCES;
		goto drop;
	}

	/* Update the counters. */
	ibytes = (m->m_pkthdr.len - skip - hl * sizeof(u_int32_t));
	tdb->tdb_cur_bytes += ibytes;
	tdb->tdb_ibytes += ibytes;
	ahstat_add(ahs_ibytes, ibytes);

	/* Hard expiration. */
	if (tdb->tdb_flags & TDBF_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		error = ENXIO;
		goto drop;
	}

	/* Notify on expiration. */
	if (tdb->tdb_flags & TDBF_SOFT_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES;  /* Turn off checking. */
	}

	/* Get crypto descriptors. */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		DPRINTF(("%s: failed to acquire crypto descriptors\n",
		    __func__));
		ahstat_inc(ahs_crypto);
		error = ENOBUFS;
		goto drop;
	}

	crda = &crp->crp_desc[0];

	crda->crd_skip = 0;
	crda->crd_len = m->m_pkthdr.len;
	crda->crd_inject = skip + rplen;

	/* Authentication operation. */
	crda->crd_alg = ahx->type;
	crda->crd_key = tdb->tdb_amxkey;
	crda->crd_klen = tdb->tdb_amxkeylen * 8;

	if ((tdb->tdb_wnd > 0) && (tdb->tdb_flags & TDBF_ESN)) {
		esn = htonl(esn);
		memcpy(crda->crd_esn, &esn, 4);
		crda->crd_flags |= CRD_F_ESN;
	}

	/* Allocate IPsec-specific opaque crypto info. */
	tc = malloc(sizeof(*tc) + skip + rplen + ahx->authsize, M_XDATA,
	    M_NOWAIT | M_ZERO);
	if (tc == NULL) {
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		ahstat_inc(ahs_crypto);
		error = ENOBUFS;
		goto drop;
	}

	/*
	 * Save the authenticator, the skipped portion of the packet,
	 * and the AH header.
	 */
	m_copydata(m, 0, skip + rplen + ahx->authsize, (caddr_t) (tc + 1));

	/* Zeroize the authenticator on the packet. */
	m_copyback(m, skip + rplen, ahx->authsize, ipseczeroes, M_NOWAIT);

	/* "Massage" the packet headers for crypto processing. */
	error = ah_massage_headers(&m, tdb->tdb_dst.sa.sa_family, skip,
	    ahx->type, 0);
	if (error) {
		/* mbuf was freed by callee. */
		m = NULL;
		goto drop;
	}

	/* Crypto operation descriptor. */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length. */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t)m;
	crp->crp_callback = ipsec_input_cb;
	crp->crp_sid = tdb->tdb_cryptoid;
	crp->crp_opaque = (caddr_t)tc;

	/* These are passed as-is to the callback. */
	tc->tc_skip = skip;
	tc->tc_protoff = protoff;
	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = tdb->tdb_sproto;
	tc->tc_rdomain = tdb->tdb_rdomain;
	memcpy(&tc->tc_dst, &tdb->tdb_dst, sizeof(union sockaddr_union));

	return crypto_dispatch(crp);

 drop:
	m_freem(m);
	crypto_freereq(crp);
	free(tc, M_XDATA, 0);
	return error;
}

int
ah_input_cb(struct tdb *tdb, struct tdb_crypto *tc, struct mbuf *m, int clen)
{
	struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
	int roff, rplen, skip, protoff;
	u_int32_t btsx, esn;
	caddr_t ptr;
	unsigned char calc[AH_ALEN_MAX];
	struct mbuf *m1, *m0;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	NET_ASSERT_LOCKED();

	skip = tc->tc_skip;
	protoff = tc->tc_protoff;

	rplen = AH_FLENGTH + sizeof(u_int32_t);

	/* Copy authenticator off the packet. */
	m_copydata(m, skip + rplen, ahx->authsize, calc);

	ptr = (caddr_t) (tc + 1);

	/* Verify authenticator. */
	if (timingsafe_bcmp(ptr + skip + rplen, calc, ahx->authsize)) {
		DPRINTF(("%s: authentication failed for packet in SA %s/%08x\n",
		    __func__, ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));

		ahstat_inc(ahs_badauth);
		goto baddone;
	}

	/* Fix the Next Protocol field. */
	((u_int8_t *) ptr)[protoff] = ((u_int8_t *) ptr)[skip];

	/* Copyback the saved (uncooked) network headers. */
	m_copyback(m, 0, skip, ptr, M_NOWAIT);

	/* Replay window checking, if applicable. */
	if (tdb->tdb_wnd > 0) {
		m_copydata(m, skip + offsetof(struct ah, ah_rpl),
		    sizeof(u_int32_t), (caddr_t) &btsx);
		btsx = ntohl(btsx);

		switch (checkreplaywindow(tdb, btsx, &esn, 1)) {
		case 0: /* All's well. */
#if NPFSYNC > 0
			pfsync_update_tdb(tdb,0);
#endif
			break;
		case 1:
			DPRINTF(("%s: replay counter wrapped for SA %s/%08x\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_wrap);
			goto baddone;
		case 2:
			DPRINTF(("%s: old packet received in SA %s/%08x\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_replay);
			goto baddone;
		case 3:
			DPRINTF(("%s): duplicate packet received in "
			    "SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_replay);
			goto baddone;
		default:
			DPRINTF(("%s: bogus value from "
			    "checkreplaywindow() in SA %s/%08x\n", __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_replay);
			goto baddone;
		}
	}

	/* Record the beginning of the AH header. */
	m1 = m_getptr(m, skip, &roff);
	if (m1 == NULL) {
		DPRINTF(("%s: bad mbuf chain for packet in SA %s/%08x\n",
		    __func__, ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		ahstat_inc(ahs_hdrops);
		goto baddone;
	}

	/* Remove the AH header from the mbuf. */
	if (roff == 0) {
		/*
		 * The AH header was conveniently at the beginning of
		 * the mbuf.
		 */
		m_adj(m1, rplen + ahx->authsize);
		/*
		 * If m1 is the first mbuf, it has set M_PKTHDR and m_adj()
		 * has already adjusted the packet header length for us.
		 */
		if (m1 != m)
			m->m_pkthdr.len -= rplen + ahx->authsize;
	} else
		if (roff + rplen + ahx->authsize >= m1->m_len) {
			int adjlen;

			/*
			 * Part or all of the AH header is at the end
			 * of this mbuf, so first let's remove the
			 * remainder of the AH header from the
			 * beginning of the remainder of the mbuf
			 * chain, if any.
			 */
			if (roff + rplen + ahx->authsize > m1->m_len) {
				adjlen = roff + rplen + ahx->authsize -
				    m1->m_len;
				/* Adjust the next mbuf by the remainder. */
				m_adj(m1->m_next, adjlen);

				/*
				 * The second mbuf is guaranteed not
				 * to have a pkthdr...
				 */
				m->m_pkthdr.len -= adjlen;
			}

			/* Now, let's unlink the mbuf chain for a second... */
			m0 = m1->m_next;
			m1->m_next = NULL;

			/*
			 * ...and trim the end of the first part of
			 * the chain...sick
			 */
			adjlen = m1->m_len - roff;
			m_adj(m1, -adjlen);
			/*
			 * If m1 is the first mbuf, it has set M_PKTHDR and
			 * m_adj() has already adjusted the packet header len.
			 */
			if (m1 != m)
				m->m_pkthdr.len -= adjlen;

			/* Finally, let's relink. */
			m1->m_next = m0;
		} else {
			/*
			 * The AH header lies in the "middle" of the
			 * mbuf...do an overlapping copy of the
			 * remainder of the mbuf over the ESP header.
			 */
			bcopy(mtod(m1, u_char *) + roff + rplen +
			    ahx->authsize, mtod(m1, u_char *) + roff,
			    m1->m_len - (roff + rplen + ahx->authsize));
			m1->m_len -= rplen + ahx->authsize;
			m->m_pkthdr.len -= rplen + ahx->authsize;
		}

	free(tc, M_XDATA, 0);

	return ipsec_common_input_cb(m, tdb, skip, protoff);

 baddone:
	m_freem(m);
	free(tc, M_XDATA, 0);
	return -1;
}

/*
 * AH output routine, called by ipsp_process_packet().
 */
int
ah_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
    int protoff)
{
	struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
	struct cryptodesc *crda;
	struct tdb_crypto *tc = NULL;
	struct mbuf *mi;
	struct cryptop *crp = NULL;
	u_int16_t iplen;
	int error, rplen, roff;
	u_int8_t prot;
	struct ah *ah;
#if NBPFILTER > 0
	struct ifnet *encif;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	if ((encif = enc_getif(tdb->tdb_rdomain, tdb->tdb_tap)) != NULL) {
		encif->if_opackets++;
		encif->if_obytes += m->m_pkthdr.len;

		if (encif->if_bpf) {
			struct enchdr hdr;

			memset(&hdr, 0, sizeof(hdr));

			hdr.af = tdb->tdb_dst.sa.sa_family;
			hdr.spi = tdb->tdb_spi;
			hdr.flags |= M_AUTH;

			bpf_mtap_hdr(encif->if_bpf, (char *)&hdr,
			    ENC_HDRLEN, m, BPF_DIRECTION_OUT, NULL);
		}
	}
#endif

	ahstat_inc(ahs_output);

	/*
	 * Check for replay counter wrap-around in automatic (not
	 * manual) keying.
	 */
	if ((tdb->tdb_rpl == 0) && (tdb->tdb_wnd > 0)) {
		DPRINTF(("%s: SA %s/%08x should have expired\n", __func__,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		ahstat_inc(ahs_wrap);
		error = EINVAL;
		goto drop;
	}

	rplen = AH_FLENGTH + sizeof(u_int32_t);

	switch (tdb->tdb_dst.sa.sa_family) {
	case AF_INET:
		/* Check for IP maximum packet size violations. */
		if (rplen + ahx->authsize + m->m_pkthdr.len > IP_MAXPACKET) {
			DPRINTF(("%s: packet in SA %s/%08x got too big\n",
			    __func__,
			    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
			    ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_toobig);
			error = EMSGSIZE;
			goto drop;
		}
		break;

#ifdef INET6
	case AF_INET6:
		/* Check for IPv6 maximum packet size violations. */
		if (rplen + ahx->authsize + m->m_pkthdr.len > IPV6_MAXPACKET) {
			DPRINTF(("%s: packet in SA %s/%08x got too big\n",
			    __func__, ipsp_address(&tdb->tdb_dst, buf,
			    sizeof(buf)), ntohl(tdb->tdb_spi)));
			ahstat_inc(ahs_toobig);
			error = EMSGSIZE;
			goto drop;
		}
		break;
#endif /* INET6 */

	default:
		DPRINTF(("%s: unknown/unsupported protocol family %d, "
		    "SA %s/%08x\n", __func__, tdb->tdb_dst.sa.sa_family,
		    ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		ahstat_inc(ahs_nopf);
		error = EPFNOSUPPORT;
		goto drop;
	}

	/* Update the counters. */
	tdb->tdb_cur_bytes += m->m_pkthdr.len - skip;
	ahstat_add(ahs_obytes, m->m_pkthdr.len - skip);

	/* Hard expiration. */
	if (tdb->tdb_flags & TDBF_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		error = EINVAL;
		goto drop;
	}

	/* Notify on expiration. */
	if (tdb->tdb_flags & TDBF_SOFT_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES; /* Turn off checking */
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
			ahstat_inc(ahs_hdrops);
			error = ENOBUFS;
			goto drop;
		}

		m_freem(m);
		m = n;
	}

	/* Inject AH header. */
	mi = m_makespace(m, skip, rplen + ahx->authsize, &roff);
	if (mi == NULL) {
		DPRINTF(("%s: failed to inject AH header for SA %s/%08x\n",
		    __func__, ipsp_address(&tdb->tdb_dst, buf, sizeof(buf)),
		    ntohl(tdb->tdb_spi)));
		ahstat_inc(ahs_hdrops);
		error = ENOBUFS;
		goto drop;
	}

	/*
	 * The AH header is guaranteed by m_makespace() to be in
	 * contiguous memory, at 'roff' of the returned mbuf.
	 */
	ah = (struct ah *)(mtod(mi, caddr_t) + roff);

	/* Initialize the AH header. */
	m_copydata(m, protoff, sizeof(u_int8_t), (caddr_t) &ah->ah_nh);
	ah->ah_hl = (rplen + ahx->authsize - AH_FLENGTH) / sizeof(u_int32_t);
	ah->ah_rv = 0;
	ah->ah_spi = tdb->tdb_spi;

	/* Zeroize authenticator. */
	m_copyback(m, skip + rplen, ahx->authsize, ipseczeroes, M_NOWAIT);

	tdb->tdb_rpl++;
	ah->ah_rpl = htonl((u_int32_t)(tdb->tdb_rpl & 0xffffffff));
#if NPFSYNC > 0
	pfsync_update_tdb(tdb,1);
#endif

	/* Get crypto descriptors. */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		DPRINTF(("%s: failed to acquire crypto descriptors\n",
		    __func__));
		ahstat_inc(ahs_crypto);
		error = ENOBUFS;
		goto drop;
	}

	crda = &crp->crp_desc[0];

	crda->crd_skip = 0;
	crda->crd_inject = skip + rplen;
	crda->crd_len = m->m_pkthdr.len;

	/* Authentication operation. */
	crda->crd_alg = ahx->type;
	crda->crd_key = tdb->tdb_amxkey;
	crda->crd_klen = tdb->tdb_amxkeylen * 8;

	if ((tdb->tdb_wnd > 0) && (tdb->tdb_flags & TDBF_ESN)) {
		u_int32_t esn;

		esn = htonl((u_int32_t)(tdb->tdb_rpl >> 32));
		memcpy(crda->crd_esn, &esn, 4);
		crda->crd_flags |= CRD_F_ESN;
	}

	/* Allocate IPsec-specific opaque crypto info. */
	tc = malloc(sizeof(*tc) + skip, M_XDATA, M_NOWAIT | M_ZERO);
	if (tc == NULL) {
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		ahstat_inc(ahs_crypto);
		error = ENOBUFS;
		goto drop;
	}

	/* Save the skipped portion of the packet. */
	m_copydata(m, 0, skip, (caddr_t) (tc + 1));

	/*
	 * Fix IP header length on the header used for
	 * authentication. We don't need to fix the original
	 * header length as it will be fixed by our caller.
	 */
	switch (tdb->tdb_dst.sa.sa_family) {
	case AF_INET:
		memcpy((caddr_t) &iplen, ((caddr_t)(tc + 1)) +
		    offsetof(struct ip, ip_len), sizeof(u_int16_t));
		iplen = htons(ntohs(iplen) + rplen + ahx->authsize);
		m_copyback(m, offsetof(struct ip, ip_len),
		    sizeof(u_int16_t), &iplen, M_NOWAIT);
		break;

#ifdef INET6
	case AF_INET6:
		memcpy((caddr_t) &iplen, ((caddr_t)(tc + 1)) +
		    offsetof(struct ip6_hdr, ip6_plen), sizeof(u_int16_t));
		iplen = htons(ntohs(iplen) + rplen + ahx->authsize);
		m_copyback(m, offsetof(struct ip6_hdr, ip6_plen),
		    sizeof(u_int16_t), &iplen, M_NOWAIT);
		break;
#endif /* INET6 */
	}

	/* Fix the Next Header field in saved header. */
	((u_int8_t *) (tc + 1))[protoff] = IPPROTO_AH;

	/* Update the Next Protocol field in the IP header. */
	prot = IPPROTO_AH;
	m_copyback(m, protoff, sizeof(u_int8_t), &prot, M_NOWAIT);

	/* "Massage" the packet headers for crypto processing. */
	error = ah_massage_headers(&m, tdb->tdb_dst.sa.sa_family, skip,
	    ahx->type, 1);
	if (error) {
		/* mbuf was freed by callee. */
		m = NULL;
		goto drop;
	}

	/* Crypto operation descriptor. */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length. */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t)m;
	crp->crp_callback = ipsec_output_cb;
	crp->crp_sid = tdb->tdb_cryptoid;
	crp->crp_opaque = (caddr_t)tc;

	/* These are passed as-is to the callback. */
	tc->tc_skip = skip;
	tc->tc_protoff = protoff;
	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = tdb->tdb_sproto;
	tc->tc_rdomain = tdb->tdb_rdomain;
	memcpy(&tc->tc_dst, &tdb->tdb_dst, sizeof(union sockaddr_union));

	return crypto_dispatch(crp);

 drop:
	m_freem(m);
	crypto_freereq(crp);
	free(tc, M_XDATA, 0);
	return error;
}

/*
 * AH output callback.
 */
int
ah_output_cb(struct tdb *tdb, struct tdb_crypto *tc, struct mbuf *m, int ilen,
    int olen)
{
	int skip = tc->tc_skip;
	caddr_t ptr = (caddr_t) (tc + 1);

	/*
	 * Copy original headers (with the new protocol number) back
	 * in place.
	 */
	m_copyback(m, 0, skip, ptr, M_NOWAIT);

	/* No longer needed. */
	free(tc, M_XDATA, 0);

	/* Call the IPsec input callback. */
	if (ipsp_process_done(m, tdb)) {
		ahstat_inc(ahs_outfail);
		return -1;
	}

	return 0;
}
