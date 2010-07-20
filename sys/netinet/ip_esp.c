/*	$OpenBSD: ip_esp.c,v 1.111 2010/07/20 15:36:03 matthew Exp $ */
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
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
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
#include <net/bpf.h>

#include <dev/rndvar.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#endif /* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
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

struct espstat espstat;

/*
 * esp_attach() is called from the transformation initialization code.
 */
int
esp_attach()
{
	return 0;
}

/*
 * esp_init() is called when an SPI is being set up.
 */
int
esp_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
	struct enc_xform *txform = NULL;
	struct auth_hash *thash = NULL;
	struct cryptoini cria, crie;

	if (!ii->ii_encalg && !ii->ii_authalg) {
		DPRINTF(("esp_init(): neither authentication nor encryption "
		    "algorithm given"));
		return EINVAL;
	}

	if (ii->ii_encalg) {
		switch (ii->ii_encalg) {
		case SADB_EALG_NULL:
			txform = &enc_xform_null;
			break;

		case SADB_EALG_DESCBC:
			txform = &enc_xform_des;
			break;

		case SADB_EALG_3DESCBC:
			txform = &enc_xform_3des;
			break;

		case SADB_X_EALG_AES:
			txform = &enc_xform_rijndael128;
			break;

		case SADB_X_EALG_AESCTR:
			txform = &enc_xform_aes_ctr;
			break;

		case SADB_X_EALG_BLF:
			txform = &enc_xform_blf;
			break;

		case SADB_X_EALG_CAST:
			txform = &enc_xform_cast5;
			break;

		case SADB_X_EALG_SKIPJACK:
			txform = &enc_xform_skipjack;
			break;

		default:
			DPRINTF(("esp_init(): unsupported encryption algorithm %d specified\n", ii->ii_encalg));
			return EINVAL;
		}

		if (ii->ii_enckeylen < txform->minkey) {
			DPRINTF(("esp_init(): keylength %d too small (min length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->minkey, txform->name));
			return EINVAL;
		}

		if (ii->ii_enckeylen > txform->maxkey) {
			DPRINTF(("esp_init(): keylength %d too large (max length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->maxkey, txform->name));
			return EINVAL;
		}

		tdbp->tdb_encalgxform = txform;

		DPRINTF(("esp_init(): initialized TDB with enc algorithm %s\n",
		    txform->name));

		tdbp->tdb_ivlen = txform->ivsize;
		if (tdbp->tdb_flags & TDBF_HALFIV)
			tdbp->tdb_ivlen /= 2;
	}

	if (ii->ii_authalg) {
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
			DPRINTF(("esp_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
			return EINVAL;
		}

		if (ii->ii_authkeylen != thash->keysize) {
			DPRINTF(("esp_init(): keylength %d doesn't match algorithm %s keysize (%d)\n", ii->ii_authkeylen, thash->name, thash->keysize));
			return EINVAL;
		}

		tdbp->tdb_authalgxform = thash;

		DPRINTF(("esp_init(): initialized TDB with hash algorithm %s\n",
		    thash->name));
	}

	tdbp->tdb_xform = xsp;
	tdbp->tdb_bitmap = 0;
	tdbp->tdb_rpl = AH_HMAC_INITIAL_RPL;

	/* Initialize crypto session */
	if (tdbp->tdb_encalgxform) {
		/* Save the raw keys */
		tdbp->tdb_emxkeylen = ii->ii_enckeylen;
		tdbp->tdb_emxkey = malloc(tdbp->tdb_emxkeylen, M_XDATA,
		    M_WAITOK);
		bcopy(ii->ii_enckey, tdbp->tdb_emxkey, tdbp->tdb_emxkeylen);

		bzero(&crie, sizeof(crie));

		crie.cri_alg = tdbp->tdb_encalgxform->type;

		if (tdbp->tdb_authalgxform)
			crie.cri_next = &cria;
		else
			crie.cri_next = NULL;

		crie.cri_klen = ii->ii_enckeylen * 8;
		crie.cri_key = ii->ii_enckey;
		/* XXX Rounds ? */
	}

	if (tdbp->tdb_authalgxform) {
		/* Save the raw keys */
		tdbp->tdb_amxkeylen = ii->ii_authkeylen;
		tdbp->tdb_amxkey = malloc(tdbp->tdb_amxkeylen, M_XDATA,
		    M_WAITOK);
		bcopy(ii->ii_authkey, tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);

		bzero(&cria, sizeof(cria));

		cria.cri_alg = tdbp->tdb_authalgxform->type;
		cria.cri_next = NULL;
		cria.cri_klen = ii->ii_authkeylen * 8;
		cria.cri_key = ii->ii_authkey;
	}

	return crypto_newsession(&tdbp->tdb_cryptoid,
	    (tdbp->tdb_encalgxform ? &crie : &cria), 0);
}

/*
 * Paranoia.
 */
int
esp_zeroize(struct tdb *tdbp)
{
	int err;

	if (tdbp->tdb_amxkey) {
		bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
		free(tdbp->tdb_amxkey, M_XDATA);
		tdbp->tdb_amxkey = NULL;
	}

	if (tdbp->tdb_emxkey) {
		bzero(tdbp->tdb_emxkey, tdbp->tdb_emxkeylen);
		free(tdbp->tdb_emxkey, M_XDATA);
		tdbp->tdb_emxkey = NULL;
	}

	err = crypto_freesession(tdbp->tdb_cryptoid);
	tdbp->tdb_cryptoid = 0;
	return err;
}

#define MAXBUFSIZ (AH_ALEN_MAX > ESP_MAX_IVS ? AH_ALEN_MAX : ESP_MAX_IVS)

/*
 * ESP input processing, called (eventually) through the protocol switch.
 */
int
esp_input(struct mbuf *m, struct tdb *tdb, int skip, int protoff)
{
	struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
	struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
	struct tdb_crypto *tc;
	int plen, alen, hlen;
	struct m_tag *mtag;
	u_int32_t btsx;

	struct cryptodesc *crde = NULL, *crda = NULL;
	struct cryptop *crp;

	/* Determine the ESP header length */
	if (tdb->tdb_flags & TDBF_NOREPLAY)
		hlen = sizeof(u_int32_t) + tdb->tdb_ivlen; /* "old" ESP */
	else
		hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen; /* "new" ESP */

	alen = esph ? esph->authsize : 0;
	plen = m->m_pkthdr.len - (skip + hlen + alen);
	if (plen <= 0) {
		DPRINTF(("esp_input: invalid payload length\n"));
		espstat.esps_badilen++;
		m_freem(m);
		return EINVAL;
	}

	if (espx) {
		/*
		 * Verify payload length is multiple of encryption algorithm
		 * block size.
		 */
		if (plen & (espx->blocksize - 1)) {
			DPRINTF(("esp_input(): payload of %d octets not a multiple of %d octets, SA %s/%08x\n", plen, espx->blocksize, ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			espstat.esps_badilen++;
			m_freem(m);
			return EINVAL;
		}
	}

	/* Replay window checking, if appropriate -- no value commitment. */
	if ((tdb->tdb_wnd > 0) && (!(tdb->tdb_flags & TDBF_NOREPLAY))) {
		m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		    (unsigned char *) &btsx);
		btsx = ntohl(btsx);

		switch (checkreplaywindow32(btsx, 0, &(tdb->tdb_rpl),
		    tdb->tdb_wnd, &(tdb->tdb_bitmap), 0)) {
		case 0: /* All's well */
			break;

		case 1:
			m_freem(m);
			DPRINTF(("esp_input(): replay counter wrapped for SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			espstat.esps_wrap++;
			return EACCES;

		case 2:
		case 3:
			DPRINTF(("esp_input(): duplicate packet received in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			m_freem(m);
			return EACCES;

		default:
			m_freem(m);
			DPRINTF(("esp_input(): bogus value from checkreplaywindow32() in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			espstat.esps_replay++;
			return EACCES;
		}
	}

	/* Update the counters */
	tdb->tdb_cur_bytes += m->m_pkthdr.len - skip - hlen - alen;
	espstat.esps_ibytes += m->m_pkthdr.len - skip - hlen - alen;

	/* Hard expiration */
	if ((tdb->tdb_flags & TDBF_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))	{
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		m_freem(m);
		return ENXIO;
	}

	/* Notify on soft expiration */
	if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	    (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES;       /* Turn off checking */
	}

#ifdef notyet
	/* Find out if we've already done crypto */
	for (mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_CRYPTO_DONE, NULL);
	     mtag != NULL;
	     mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_CRYPTO_DONE, mtag)) {
		struct tdb_ident *tdbi;

		tdbi = (struct tdb_ident *) (mtag + 1);
		if (tdbi->proto == tdb->tdb_sproto && tdbi->spi == tdb->tdb_spi &&
		    tdbi->rdomain == tdb->tdb_rdomain && !bcmp(&tdbi->dst,
		    &tdb->tdb_dst, sizeof(union sockaddr_union)))
			break;
	}
#else
	mtag = NULL;
#endif

	/* Get crypto descriptors */
	crp = crypto_getreq(esph && espx ? 2 : 1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("esp_input(): failed to acquire crypto descriptors\n"));
		espstat.esps_crypto++;
		return ENOBUFS;
	}

	/* Get IPsec-specific opaque pointer */
	if (esph == NULL || mtag != NULL)
		tc = malloc(sizeof(*tc), M_XDATA, M_NOWAIT | M_ZERO);
	else
		tc = malloc(sizeof(*tc) + alen, M_XDATA, M_NOWAIT | M_ZERO);
	if (tc == NULL)	{
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("esp_input(): failed to allocate tdb_crypto\n"));
		espstat.esps_crypto++;
		return ENOBUFS;
	}

	tc->tc_ptr = (caddr_t) mtag;

	if (esph) {
		crda = crp->crp_desc;
		crde = crda->crd_next;

		/* Authentication descriptor */
		crda->crd_skip = skip;
		crda->crd_len = m->m_pkthdr.len - (skip + alen);
		crda->crd_inject = m->m_pkthdr.len - alen;

		crda->crd_alg = esph->type;
		crda->crd_key = tdb->tdb_amxkey;
		crda->crd_klen = tdb->tdb_amxkeylen * 8;

		/* Copy the authenticator */
		if (mtag == NULL)
			m_copydata(m, m->m_pkthdr.len - alen, alen, (caddr_t) (tc + 1));
	} else
		crde = crp->crp_desc;

	/* Crypto operation descriptor */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = (int (*) (struct cryptop *)) esp_input_cb;
	crp->crp_sid = tdb->tdb_cryptoid;
	crp->crp_opaque = (caddr_t) tc;

	/* These are passed as-is to the callback */
	tc->tc_skip = skip;
	tc->tc_protoff = protoff;
	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = tdb->tdb_sproto;
	tc->tc_rdomain = tdb->tdb_rdomain;
	bcopy(&tdb->tdb_dst, &tc->tc_dst, sizeof(union sockaddr_union));

	/* Decryption descriptor */
	if (espx) {
		crde->crd_skip = skip + hlen;
		crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
		crde->crd_inject = skip + hlen - tdb->tdb_ivlen;

		if (tdb->tdb_flags & TDBF_HALFIV) {
			/* Copy half-IV from packet */
			m_copydata(m, crde->crd_inject, tdb->tdb_ivlen, crde->crd_iv);

			/* Cook IV */
			for (btsx = 0; btsx < tdb->tdb_ivlen; btsx++)
				crde->crd_iv[tdb->tdb_ivlen + btsx] = ~crde->crd_iv[btsx];

			crde->crd_flags |= CRD_F_IV_EXPLICIT;
		}

		crde->crd_alg = espx->type;
		crde->crd_key = tdb->tdb_emxkey;
		crde->crd_klen = tdb->tdb_emxkeylen * 8;
		/* XXX Rounds ? */
	}

	if (mtag == NULL)
		return crypto_dispatch(crp);
	else
		return esp_input_cb(crp);
}

/*
 * ESP input callback, called directly by the crypto driver.
 */
int
esp_input_cb(void *op)
{
	u_int8_t lastthree[3], aalg[AH_HMAC_MAX_HASHLEN];
	int s, hlen, roff, skip, protoff, error;
	struct mbuf *m1, *mo, *m;
	struct auth_hash *esph;
	struct tdb_crypto *tc;
	struct cryptop *crp;
	struct m_tag *mtag;
	struct tdb *tdb;
	u_int32_t btsx;
	caddr_t ptr;

	crp = (struct cryptop *) op;

	tc = (struct tdb_crypto *) crp->crp_opaque;
	skip = tc->tc_skip;
	protoff = tc->tc_protoff;
	mtag = (struct m_tag *) tc->tc_ptr;

	m = (struct mbuf *) crp->crp_buf;
	if (m == NULL) {
		/* Shouldn't happen... */
		free(tc, M_XDATA);
		crypto_freereq(crp);
		espstat.esps_crypto++;
		DPRINTF(("esp_input_cb(): bogus returned buffer from crypto\n"));
		return (EINVAL);
	}

	s = spltdb();

	tdb = gettdb(tc->tc_rdomain, tc->tc_spi, &tc->tc_dst, tc->tc_proto);
	if (tdb == NULL) {
		free(tc, M_XDATA);
		espstat.esps_notdb++;
		DPRINTF(("esp_input_cb(): TDB is expired while in crypto"));
		error = EPERM;
		goto baddone;
	}

	esph = (struct auth_hash *) tdb->tdb_authalgxform;

	/* Check for crypto errors */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (tdb->tdb_cryptoid != 0)
				tdb->tdb_cryptoid = crp->crp_sid;
			splx(s);
			return crypto_dispatch(crp);
		}
		free(tc, M_XDATA);
		espstat.esps_noxform++;
		DPRINTF(("esp_input_cb(): crypto error %d\n", crp->crp_etype));
		error = crp->crp_etype;
		goto baddone;
	}

	/* If authentication was performed, check now. */
	if (esph != NULL) {
		/*
		 * If we have a tag, it means an IPsec-aware NIC did the verification
		 * for us.
		 */
		if (mtag == NULL) {
			/* Copy the authenticator from the packet */
			m_copydata(m, m->m_pkthdr.len - esph->authsize,
			    esph->authsize, aalg);

			ptr = (caddr_t) (tc + 1);

			/* Verify authenticator */
			if (timingsafe_bcmp(ptr, aalg, esph->authsize)) {
				free(tc, M_XDATA);
				DPRINTF(("esp_input_cb(): authentication failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
				espstat.esps_badauth++;
				error = EACCES;
				goto baddone;
			}
		}

		/* Remove trailing authenticator */
		m_adj(m, -(esph->authsize));
	}
	free(tc, M_XDATA);

	/* Replay window checking, if appropriate */
	if ((tdb->tdb_wnd > 0) && (!(tdb->tdb_flags & TDBF_NOREPLAY))) {
		m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		    (unsigned char *) &btsx);
		btsx = ntohl(btsx);

		switch (checkreplaywindow32(btsx, 0, &(tdb->tdb_rpl),
		    tdb->tdb_wnd, &(tdb->tdb_bitmap), 1)) {
		case 0: /* All's well */
#if NPFSYNC > 0
			pfsync_update_tdb(tdb,0);
#endif
			break;

		case 1:
			DPRINTF(("esp_input_cb(): replay counter wrapped for SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			espstat.esps_wrap++;
			error = EACCES;
			goto baddone;

		case 2:
		case 3:
			DPRINTF(("esp_input_cb(): duplicate packet received in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			error = EACCES;
			goto baddone;

		default:
			DPRINTF(("esp_input_cb(): bogus value from checkreplaywindow32() in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			espstat.esps_replay++;
			error = EACCES;
			goto baddone;
		}
	}

	/* Release the crypto descriptors */
	crypto_freereq(crp);

	/* Determine the ESP header length */
	if (tdb->tdb_flags & TDBF_NOREPLAY)
		hlen = sizeof(u_int32_t) + tdb->tdb_ivlen; /* "old" ESP */
	else
		hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen; /* "new" ESP */

	/* Find beginning of ESP header */
	m1 = m_getptr(m, skip, &roff);
	if (m1 == NULL)	{
		espstat.esps_hdrops++;
		splx(s);
		DPRINTF(("esp_input_cb(): bad mbuf chain, SA %s/%08x\n",
		    ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		m_freem(m);
		return EINVAL;
	}

	/* Remove the ESP header and IV from the mbuf. */
	if (roff == 0) {
		/* The ESP header was conveniently at the beginning of the mbuf */
		m_adj(m1, hlen);
		if (!(m1->m_flags & M_PKTHDR))
			m->m_pkthdr.len -= hlen;
	} else if (roff + hlen >= m1->m_len) {
		/*
		 * Part or all of the ESP header is at the end of this mbuf, so
		 * first let's remove the remainder of the ESP header from the
		 * beginning of the remainder of the mbuf chain, if any.
		 */
		if (roff + hlen > m1->m_len) {
			/* Adjust the next mbuf by the remainder */
			m_adj(m1->m_next, roff + hlen - m1->m_len);

			/* The second mbuf is guaranteed not to have a pkthdr... */
			m->m_pkthdr.len -= (roff + hlen - m1->m_len);
		}

		/* Now, let's unlink the mbuf chain for a second...*/
		mo = m1->m_next;
		m1->m_next = NULL;

		/* ...and trim the end of the first part of the chain...sick */
		m_adj(m1, -(m1->m_len - roff));
		if (!(m1->m_flags & M_PKTHDR))
			m->m_pkthdr.len -= (m1->m_len - roff);

		/* Finally, let's relink */
		m1->m_next = mo;
	} else {
		/*
		 * The ESP header lies in the "middle" of the mbuf...do an
		 * overlapping copy of the remainder of the mbuf over the ESP
		 * header.
		 */
		bcopy(mtod(m1, u_char *) + roff + hlen,
		    mtod(m1, u_char *) + roff, m1->m_len - (roff + hlen));
		m1->m_len -= hlen;
		m->m_pkthdr.len -= hlen;
	}

	/* Save the last three bytes of decrypted data */
	m_copydata(m, m->m_pkthdr.len - 3, 3, lastthree);

	/* Verify pad length */
	if (lastthree[1] + 2 > m->m_pkthdr.len - skip) {
		espstat.esps_badilen++;
		splx(s);
		DPRINTF(("esp_input_cb(): invalid padding length %d for packet in SA %s/%08x\n", lastthree[1], ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		m_freem(m);
		return EINVAL;
	}

	/* Verify correct decryption by checking the last padding bytes */
	if (!(tdb->tdb_flags & TDBF_RANDOMPADDING)) {
		if ((lastthree[1] != lastthree[0]) && (lastthree[1] != 0)) {
			espstat.esps_badenc++;
			splx(s);
			DPRINTF(("esp_input(): decryption failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			m_freem(m);
			return EINVAL;
		}
	}

	/* Trim the mbuf chain to remove the trailing authenticator and padding */
	m_adj(m, -(lastthree[1] + 2));

	/* Restore the Next Protocol field */
	m_copyback(m, protoff, sizeof(u_int8_t), lastthree + 2, M_NOWAIT);

	/* Back to generic IPsec input processing */
	error = ipsec_common_input_cb(m, tdb, skip, protoff, mtag);
	splx(s);
	return (error);

 baddone:
	splx(s);

	if (m != NULL)
		m_freem(m);

	crypto_freereq(crp);

	return (error);
}

/*
 * ESP output routine, called by ipsp_process_packet().
 */
int
esp_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
    int protoff)
{
	struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
	struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
	int ilen, hlen, rlen, padding, blks, alen;
	struct mbuf *mi, *mo = (struct mbuf *) NULL;
	struct tdb_crypto *tc;
	unsigned char *pad;
	u_int8_t prot;

	struct cryptodesc *crde = NULL, *crda = NULL;
	struct cryptop *crp;
#if NBPFILTER > 0
	struct ifnet *encif;

	if ((encif = enc_getif(tdb->tdb_rdomain, tdb->tdb_tap)) != NULL) {
		encif->if_opackets++;
		encif->if_obytes += m->m_pkthdr.len;

		if (encif->if_bpf) {
			struct enchdr hdr;

			bzero (&hdr, sizeof(hdr));

			hdr.af = tdb->tdb_dst.sa.sa_family;
			hdr.spi = tdb->tdb_spi;
			if (espx)
				hdr.flags |= M_CONF;
			if (esph)
				hdr.flags |= M_AUTH;

			bpf_mtap_hdr(encif->if_bpf, (char *)&hdr,
			    ENC_HDRLEN, m, BPF_DIRECTION_OUT);
		}
	}
#endif

	if (tdb->tdb_flags & TDBF_NOREPLAY)
		hlen = sizeof(u_int32_t) + tdb->tdb_ivlen;
	else
		hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen;

	rlen = m->m_pkthdr.len - skip; /* Raw payload length. */
	if (espx)
		blks = espx->blocksize;
	else
		blks = 4; /* If no encryption, we have to be 4-byte aligned. */

	padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;

	alen = esph ? esph->authsize : 0;
	espstat.esps_output++;

	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		/* Check for IP maximum packet size violations. */
		if (skip + hlen + rlen + padding + alen > IP_MAXPACKET)	{
			DPRINTF(("esp_output(): packet in SA %s/%08x got "
			    "too big\n", ipsp_address(tdb->tdb_dst),
			    ntohl(tdb->tdb_spi)));
			m_freem(m);
			espstat.esps_toobig++;
			return EMSGSIZE;
		}
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		/* Check for IPv6 maximum packet size violations. */
		if (skip + hlen + rlen + padding + alen > IPV6_MAXPACKET) {
			DPRINTF(("esp_output(): packet in SA %s/%08x got too "
			    "big\n", ipsp_address(tdb->tdb_dst),
			    ntohl(tdb->tdb_spi)));
			m_freem(m);
			espstat.esps_toobig++;
			return EMSGSIZE;
		}
		break;
#endif /* INET6 */

	default:
		DPRINTF(("esp_output(): unknown/unsupported protocol "
		    "family %d, SA %s/%08x\n", tdb->tdb_dst.sa.sa_family
		    , ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		m_freem(m);
		espstat.esps_nopf++;
		return EPFNOSUPPORT;
	}

	/* Update the counters. */
	tdb->tdb_cur_bytes += m->m_pkthdr.len - skip;
	espstat.esps_obytes += m->m_pkthdr.len - skip;

	/* Hard byte expiration. */
	if (tdb->tdb_flags & TDBF_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
		tdb_delete(tdb);
		m_freem(m);
		return EINVAL;
	}

	/* Soft byte expiration. */
	if (tdb->tdb_flags & TDBF_SOFT_BYTES &&
	    tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes) {
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
		tdb->tdb_flags &= ~TDBF_SOFT_BYTES;    /* Turn off checking. */
	}

	/*
	 * Loop through mbuf chain; if we find a readonly mbuf,
	 * replace the rest of the chain.
	 */
	mo = NULL;
	mi = m;
	while (mi != NULL && !M_READONLY(mi)) {
		mo = mi;
		mi = mi->m_next;
	}

	if (mi != NULL)	{
		/* Replace the rest of the mbuf chain. */
		struct mbuf *n = m_copym2(mi, 0, M_COPYALL, M_DONTWAIT);

		if (n == NULL) {
			DPRINTF(("esp_output(): bad mbuf chain, SA %s/%08x\n",
			    ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			espstat.esps_hdrops++;
			m_freem(m);
			return ENOBUFS;
		}

		if (mo != NULL)
			mo->m_next = n;
		else
			m = n;

		m_freem(mi);
	}

	/* Inject ESP header. */
	mo = m_inject(m, skip, hlen, M_DONTWAIT);
	if (mo == NULL) {
		DPRINTF(("esp_output(): failed to inject ESP header for "
		    "SA %s/%08x\n", ipsp_address(tdb->tdb_dst),
		    ntohl(tdb->tdb_spi)));
		m_freem(m);
		espstat.esps_hdrops++;
		return ENOBUFS;
	}

	/* Initialize ESP header. */
	bcopy((caddr_t) &tdb->tdb_spi, mtod(mo, caddr_t), sizeof(u_int32_t));
	if (!(tdb->tdb_flags & TDBF_NOREPLAY)) {
		u_int32_t replay = htonl(tdb->tdb_rpl++);
		bcopy((caddr_t) &replay, mtod(mo, caddr_t) + sizeof(u_int32_t),
		    sizeof(u_int32_t));
#if NPFSYNC > 0
		pfsync_update_tdb(tdb,1);
#endif
	}

	/*
	 * Add padding -- better to do it ourselves than use the crypto engine,
	 * although if/when we support compression, we'd have to do that.
	 */
	pad = (u_char *) m_pad(m, padding + alen);
	if (pad == NULL) {
		DPRINTF(("esp_output(): m_pad() failed for SA %s/%08x\n",
		    ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		return ENOBUFS;
	}

	/* Self-describing or random padding ? */
	if (!(tdb->tdb_flags & TDBF_RANDOMPADDING))
		for (ilen = 0; ilen < padding - 2; ilen++)
			pad[ilen] = ilen + 1;
	else
		arc4random_buf((void *) pad, padding - 2);

	/* Fix padding length and Next Protocol in padding itself. */
	pad[padding - 2] = padding - 2;
	m_copydata(m, protoff, sizeof(u_int8_t), pad + padding - 1);

	/* Fix Next Protocol in IPv4/IPv6 header. */
	prot = IPPROTO_ESP;
	m_copyback(m, protoff, sizeof(u_int8_t), &prot, M_NOWAIT);

	/* Get crypto descriptors. */
	crp = crypto_getreq(esph && espx ? 2 : 1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("esp_output(): failed to acquire crypto "
		    "descriptors\n"));
		espstat.esps_crypto++;
		return ENOBUFS;
	}

	if (espx) {
		crde = crp->crp_desc;
		crda = crde->crd_next;

		/* Encryption descriptor. */
		crde->crd_skip = skip + hlen;
		crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
		crde->crd_flags = CRD_F_ENCRYPT;
		crde->crd_inject = skip + hlen - tdb->tdb_ivlen;

		if (tdb->tdb_flags & TDBF_HALFIV) {
			/* Copy half-iv in the packet. */
			m_copyback(m, crde->crd_inject, tdb->tdb_ivlen,
			    tdb->tdb_iv, M_NOWAIT);

			/* Cook half-iv. */
			bcopy(tdb->tdb_iv, crde->crd_iv, tdb->tdb_ivlen);
			for (ilen = 0; ilen < tdb->tdb_ivlen; ilen++)
				crde->crd_iv[tdb->tdb_ivlen + ilen] =
				    ~crde->crd_iv[ilen];

			crde->crd_flags |=
			    CRD_F_IV_PRESENT | CRD_F_IV_EXPLICIT;
		}

		/* Encryption operation. */
		crde->crd_alg = espx->type;
		crde->crd_key = tdb->tdb_emxkey;
		crde->crd_klen = tdb->tdb_emxkeylen * 8;
		/* XXX Rounds ? */
	} else
		crda = crp->crp_desc;

	/* IPsec-specific opaque crypto info. */
	tc = malloc(sizeof(*tc), M_XDATA, M_NOWAIT | M_ZERO);
	if (tc == NULL) {
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("esp_output(): failed to allocate tdb_crypto\n"));
		espstat.esps_crypto++;
		return ENOBUFS;
	}

	tc->tc_spi = tdb->tdb_spi;
	tc->tc_proto = tdb->tdb_sproto;
	tc->tc_rdomain = tdb->tdb_rdomain;
	bcopy(&tdb->tdb_dst, &tc->tc_dst, sizeof(union sockaddr_union));

	/* Crypto operation descriptor. */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length. */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = (int (*) (struct cryptop *)) esp_output_cb;
	crp->crp_opaque = (caddr_t) tc;
	crp->crp_sid = tdb->tdb_cryptoid;

	if (esph) {
		/* Authentication descriptor. */
		crda->crd_skip = skip;
		crda->crd_len = m->m_pkthdr.len - (skip + alen);
		crda->crd_inject = m->m_pkthdr.len - alen;

		/* Authentication operation. */
		crda->crd_alg = esph->type;
		crda->crd_key = tdb->tdb_amxkey;
		crda->crd_klen = tdb->tdb_amxkeylen * 8;
	}

	if ((tdb->tdb_flags & TDBF_SKIPCRYPTO) == 0)
		return crypto_dispatch(crp);
	else
		return esp_output_cb(crp);
}

/*
 * ESP output callback, called directly by the crypto driver.
 */
int
esp_output_cb(void *op)
{
	struct cryptop *crp = (struct cryptop *) op;
	struct tdb_crypto *tc;
	struct tdb *tdb;
	struct mbuf *m;
	int error, s;

	tc = (struct tdb_crypto *) crp->crp_opaque;

	m = (struct mbuf *) crp->crp_buf;
	if (m == NULL) {
		/* Shouldn't happen... */
		free(tc, M_XDATA);
		crypto_freereq(crp);
		espstat.esps_crypto++;
		DPRINTF(("esp_output_cb(): bogus returned buffer from "
		    "crypto\n"));
		return (EINVAL);
	}


	s = spltdb();

	tdb = gettdb(tc->tc_rdomain, tc->tc_spi, &tc->tc_dst, tc->tc_proto);
	if (tdb == NULL) {
		free(tc, M_XDATA);
		espstat.esps_notdb++;
		DPRINTF(("esp_output_cb(): TDB is expired while in crypto\n"));
		error = EPERM;
		goto baddone;
	}

	/* Check for crypto errors. */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (tdb->tdb_cryptoid != 0)
				tdb->tdb_cryptoid = crp->crp_sid;
			splx(s);
			return crypto_dispatch(crp);
		}
		free(tc, M_XDATA);
		espstat.esps_noxform++;
		DPRINTF(("esp_output_cb(): crypto error %d\n",
		    crp->crp_etype));
		error = crp->crp_etype;
		goto baddone;
	}
	free(tc, M_XDATA);

	/* Release crypto descriptors. */
	crypto_freereq(crp);

	/*
	 * If we're doing half-iv, keep a copy of the last few bytes of the
	 * encrypted part, for use as the next IV. Note that HALF-IV is only
	 * supposed to be used without authentication (the old ESP specs).
	 */
	if (tdb->tdb_flags & TDBF_HALFIV)
		m_copydata(m, m->m_pkthdr.len - tdb->tdb_ivlen, tdb->tdb_ivlen,
		    tdb->tdb_iv);

	/* Call the IPsec input callback. */
	error = ipsp_process_done(m, tdb);
	splx(s);
	return error;

 baddone:
	splx(s);

	if (m != NULL)
		m_freem(m);

	crypto_freereq(crp);

	return error;
}

/*
 * return 0 on success
 * return 1 for counter == 0
 * return 2 for very old packet
 * return 3 for packet within current window but already received
 */
int
checkreplaywindow32(u_int32_t seq, u_int32_t initial, u_int32_t *lastseq,
    u_int32_t window, u_int32_t *bitmap, int commit)
{
	u_int32_t diff, llseq, lbitmap;

	/* Just do the checking, without "committing" any changes. */
	if (commit == 0) {
		llseq = *lastseq;
		lbitmap = *bitmap;

		lastseq = &llseq;
		bitmap = &lbitmap;
	}

	seq -= initial;

	if (seq == 0)
		return 1;

	if (seq > *lastseq - initial) {
		diff = seq - (*lastseq - initial);
		if (diff < window)
			*bitmap = ((*bitmap) << diff) | 1;
		else
			*bitmap = 1;
		*lastseq = seq + initial;
		return 0;
	}

	diff = *lastseq - initial - seq;
	if (diff >= window) {
		espstat.esps_wrap++;
		return 2;
	}

	if ((*bitmap) & (((u_int32_t) 1) << diff)) {
		espstat.esps_replay++;
		return 3;
	}

	*bitmap |= (((u_int32_t) 1) << diff);
	return 0;
}

/*
 * m_pad(m, n) pads <m> with <n> bytes at the end. The packet header
 * length is updated, and a pointer to the first byte of the padding
 * (which is guaranteed to be all in one mbuf) is returned.
 */

caddr_t
m_pad(struct mbuf *m, int n)
{
	struct mbuf *m0, *m1;
	int len, pad;
	caddr_t retval;

	if (n <= 0) {  /* No stupid arguments. */
		DPRINTF(("m_pad(): pad length invalid (%d)\n", n));
		m_freem(m);
		return NULL;
	}

	len = m->m_pkthdr.len;
	pad = n;
	m0 = m;

	while (m0->m_len < len) {
		len -= m0->m_len;
		m0 = m0->m_next;
	}

	if (m0->m_len != len) {
		DPRINTF(("m_pad(): length mismatch (should be %d instead of "
		    "%d)\n", m->m_pkthdr.len,
		    m->m_pkthdr.len + m0->m_len - len));

		m_freem(m);
		return NULL;
	}

	/* Check for zero-length trailing mbufs, and find the last one. */
	for (m1 = m0; m1->m_next; m1 = m1->m_next) {
		if (m1->m_next->m_len != 0) {
			DPRINTF(("m_pad(): length mismatch (should be %d "
			    "instead of %d)\n", m->m_pkthdr.len,
			    m->m_pkthdr.len + m1->m_next->m_len));

			m_freem(m);
			return NULL;
		}

		m0 = m1->m_next;
	}

	if ((m0->m_flags & M_EXT) ||
	    m0->m_data + m0->m_len + pad >= &(m0->m_dat[MLEN])) {
		/* Add an mbuf to the chain. */
		MGET(m1, M_DONTWAIT, MT_DATA);
		if (m1 == 0) {
			m_freem(m0);
			DPRINTF(("m_pad(): cannot append\n"));
			return NULL;
		}

		m0->m_next = m1;
		m0 = m1;
		m0->m_len = 0;
	}

	retval = m0->m_data + m0->m_len;
	m0->m_len += pad;
	m->m_pkthdr.len += pad;

	return retval;
}
