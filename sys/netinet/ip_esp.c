/*	$OpenBSD: ip_esp.c,v 1.37 2000/03/29 07:09:57 angelos Exp $ */

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
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/bpf.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
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

#include <sys/md5k.h>
#include <crypto/sha1.h>
#include <crypto/rmd160.h>
#include <crypto/crypto.h>
#include <crypto/xform.h>

#include "bpfilter.h"

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

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

    if (ii->ii_encalg)
    {
	switch (ii->ii_encalg)
	{
	    case SADB_EALG_DESCBC:
		txform = &enc_xform_des;
		break;

	    case SADB_EALG_3DESCBC:
		txform = &enc_xform_3des;
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

	if (ii->ii_enckeylen < txform->minkey)
	{
	    DPRINTF(("esp_init(): keylength %d too small (min length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->minkey, txform->name));
	    return EINVAL;
	}
    
	if (ii->ii_enckeylen > txform->maxkey)
	{
	    DPRINTF(("esp_init(): keylength %d too large (max length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->maxkey, txform->name));
	    return EINVAL;
	}
    }

    if (ii->ii_authalg)
    {
	switch (ii->ii_authalg)
	{
	    case SADB_AALG_MD5HMAC96:
		thash = &auth_hash_hmac_md5_96;
		break;

	    case SADB_AALG_SHA1HMAC96:
		thash = &auth_hash_hmac_sha1_96;
		break;

	    case SADB_X_AALG_RIPEMD160HMAC96:
		thash = &auth_hash_hmac_ripemd_160_96;
		break;

	    default:
		DPRINTF(("esp_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
		return EINVAL;
	}

	if (ii->ii_authkeylen != thash->keysize)
	{
	    DPRINTF(("esp_init(): keylength %d doesn't match algorithm %s keysize (%d)\n", ii->ii_authkeylen, thash->name, thash->keysize));
	    return EINVAL;
	}
    
    	tdbp->tdb_authalgxform = thash;

	DPRINTF(("esp_init(): initialized TDB with hash algorithm %s\n",
		 thash->name));
    }
    
    tdbp->tdb_xform = xsp;
    tdbp->tdb_encalgxform = txform;
    tdbp->tdb_bitmap = 0;
    tdbp->tdb_rpl = AH_HMAC_INITIAL_RPL;

    DPRINTF(("esp_init(): initialized TDB with enc algorithm %s\n",
	     txform->name));

    tdbp->tdb_ivlen = txform->ivmask;
    if (tdbp->tdb_flags & TDBF_HALFIV)
      tdbp->tdb_ivlen /= 2;

    /* Save the raw keys */
    if (tdbp->tdb_authalgxform)
    {
	tdbp->tdb_amxkeylen = ii->ii_authkeylen;
	MALLOC(tdbp->tdb_amxkey, u_int8_t *, tdbp->tdb_amxkeylen, M_XDATA,
	       M_WAITOK);
	bcopy(ii->ii_authkey, tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
    }

    if (tdbp->tdb_encalgxform)
    {
	tdbp->tdb_emxkeylen = ii->ii_enckeylen;
	MALLOC(tdbp->tdb_emxkey, u_int8_t *, tdbp->tdb_emxkeylen, M_XDATA,
	       M_WAITOK);
	bcopy(ii->ii_enckey, tdbp->tdb_emxkey, tdbp->tdb_emxkeylen);
    }

    /* Initialize crypto session */
    if (tdbp->tdb_encalgxform)
    {
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

    if (tdbp->tdb_authalgxform)
    {
	bzero(&cria, sizeof(cria));

	cria.cri_alg = tdbp->tdb_authalgxform->type;
	cria.cri_next = NULL;
	cria.cri_klen = ii->ii_authkeylen * 8;
	cria.cri_key = ii->ii_authkey;
    }

    return crypto_newsession(&tdbp->tdb_cryptoid,
			     (tdbp->tdb_encalgxform ? &crie : &cria));
}

/*
 * Paranoia.
 */
int
esp_zeroize(struct tdb *tdbp)
{
    int err;

    if (tdbp->tdb_amxkey)
    {
	bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
	FREE(tdbp->tdb_amxkey, M_XDATA);
	tdbp->tdb_amxkey = NULL;
    }

    if (tdbp->tdb_emxkey)
    {
	bzero(tdbp->tdb_emxkey, tdbp->tdb_emxkeylen);
	FREE(tdbp->tdb_emxkey, M_XDATA);
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
    int plen, alen, hlen;
    u_int32_t btsx;

    struct cryptodesc *crde = NULL, *crda = NULL;
    struct cryptop *crp;

    /* Determine the ESP header length */
    if (tdb->tdb_flags & TDBF_NOREPLAY)
      hlen = sizeof(u_int32_t) + tdb->tdb_ivlen; /* "old" ESP */
    else
      hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen; /* "new" ESP */

    if (esph)
      alen = AH_HMAC_HASHLEN;
    else
      alen = 0;

    if (espx)
    {
	/*
	 * Verify payload length is multiple of encryption algorithm
	 * block size.
	 */
	plen = m->m_pkthdr.len - (skip + hlen + alen);
	if ((plen & (espx->blocksize - 1)) || (plen <= 0))
	{
	    DPRINTF(("esp_input(): payload not a multiple of %d octets, SA %s/%08x\n", espx->blocksize, ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_badilen++;
	    m_freem(m);
	    return EINVAL;
	}
    }

    /* Replay window checking, if appropriate */
    if ((tdb->tdb_wnd > 0) && (!(tdb->tdb_flags & TDBF_NOREPLAY)))
    {
	m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		   (unsigned char *) &btsx);
	btsx = ntohl(btsx);

	switch (checkreplaywindow32(btsx, 0, &(tdb->tdb_rpl), tdb->tdb_wnd,
				    &(tdb->tdb_bitmap)))
	{
	    case 0: /* All's well */
		break;

	    case 1:
		DPRINTF(("esp_input(): replay counter wrapped for SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		espstat.esps_wrap++;
		m_freem(m);
		return EACCES;

	    case 2:
	    case 3:
		DPRINTF(("esp_input(): duplicate packet received in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		espstat.esps_replay++;
		m_freem(m);
		return EACCES;

	    default:
		DPRINTF(("esp_input(): bogus value from checkreplaywindow32() in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		espstat.esps_replay++;
		m_freem(m);
		return EACCES;
	}
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += m->m_pkthdr.len - skip - hlen - alen;
    espstat.esps_ibytes += m->m_pkthdr.len - skip - hlen - alen;

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	tdb_delete(tdb, 0, TDBEXP_TIMEOUT);
	m_freem(m);
	return ENXIO;
    }

    /* Notify on soft expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;       /* Turn off checking */
    }

    /* Get crypto descriptors */
    crp = crypto_getreq(esph && espx ? 2 : 1);
    if (crp == NULL)
    {
	m_freem(m);
	DPRINTF(("esp_input(): failed to acquire crypto descriptors\n"));
	espstat.esps_crypto++;
	return ENOBUFS;
    }

    if (esph)
    {
	crda = crp->crp_desc;
	crde = crda->crd_next;

	/* Authentication descriptor */
	crda->crd_skip = skip;
	crda->crd_len = m->m_pkthdr.len - (skip + alen);
	crda->crd_inject = m->m_pkthdr.len - alen;

	crda->crd_alg = esph->type;
	crda->crd_key = tdb->tdb_amxkey;
	crda->crd_klen = tdb->tdb_amxkeylen * 8;
    }
    else
      crde = crp->crp_desc;

    tdb->tdb_ref++;

    /* Crypto operation descriptor */
    crp->crp_ilen = m->m_pkthdr.len; /* Total input length */
    crp->crp_flags = CRYPTO_F_IMBUF;
    crp->crp_buf = (caddr_t) m;
    crp->crp_callback = (int (*) (struct cryptop *)) esp_input_cb;
    crp->crp_sid = tdb->tdb_cryptoid;

    /* These are passed as-is to the callback */
    crp->crp_opaque1 = (caddr_t) tdb;
    (long) crp->crp_opaque2 = skip;
    (long) crp->crp_opaque3 = protoff;

    /* Decryption descriptor */
    if (espx)
    {
	crde->crd_skip = skip + hlen;
	crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
	crde->crd_inject = skip + hlen - tdb->tdb_ivlen;

        if (tdb->tdb_flags & TDBF_HALFIV)
	{
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

    MALLOC(crp->crp_opaque4, caddr_t, alen, M_XDATA, M_DONTWAIT);
    if (crp->crp_opaque4 == 0)
    {
	m_freem(m);
	crypto_freereq(crp);
	DPRINTF(("esp_input(): failed to allocate auth array\n"));
	espstat.esps_crypto++;
	return ENOBUFS;
    }

    /* Copy the authenticator */
    m_copydata(m, m->m_pkthdr.len - alen, alen, crp->crp_opaque4);

    return crypto_dispatch(crp);
}

/*
 * ESP input callback, called directly by the crypto driver.
 */
int
esp_input_cb(void *op)
{
    u_int8_t lastthree[3], aalg[AH_HMAC_HASHLEN];
    int hlen, roff, skip, protoff, error;
    struct mbuf *m1, *mo, *m;
    struct cryptodesc *crd;
    struct auth_hash *esph;
    struct enc_xform *espx;
    struct cryptop *crp;
    struct tdb *tdb;

    crp = (struct cryptop *) op;
    crd = crp->crp_desc;
    tdb = (struct tdb *) crp->crp_opaque1;
    esph = (struct auth_hash *) tdb->tdb_authalgxform;
    espx = (struct enc_xform *) tdb->tdb_encalgxform;
    skip = (long) crp->crp_opaque2;
    protoff = (long) crp->crp_opaque3;
    m = (struct mbuf *) crp->crp_buf;

    tdb->tdb_ref--;

    /* Check for crypto errors */
    if (crp->crp_etype)
    {
	/* Reset the session ID */
	if (tdb->tdb_cryptoid != 0)
	  tdb->tdb_cryptoid = crp->crp_sid;

	if (crp->crp_etype == EAGAIN)
	{
	    tdb->tdb_ref++;
	    return crypto_dispatch(crp);
	}

	espstat.esps_noxform++;
	DPRINTF(("esp_input_cb(): crypto error %d\n", crp->crp_etype));
	error = crp->crp_etype;
	goto baddone;
    }

    /* Shouldn't happen... */
    if (!m)
    {
	espstat.esps_crypto++;
	DPRINTF(("esp_input_cb(): bogus returned buffer from crypto\n"));
	error = EINVAL;
	goto baddone;
    }

    /*
     * Check that the TDB is still valid -- not really an error, but
     * we need to handle it as such. It may happen if the TDB expired
     * or was deleted while there was a pending request in the crypto
     * queue.
     */
    if (tdb->tdb_flags & TDBF_INVALID)
    {
	espstat.esps_invalid++;
	tdb_delete(tdb, 0, 0);
	error = ENXIO;
	DPRINTF(("esp_input_cb(): TDB expired while processing crypto\n"));
	goto baddone;
    }

    /* If authentication was performed, check now */
    if (esph)
    {
	/* Copy the authenticator from the packet */
	m_copydata(m, m->m_pkthdr.len - esph->authsize,
		   esph->authsize, aalg);

	/* Verify authenticator */
	if (bcmp(crp->crp_opaque4, aalg, esph->authsize))
	{
	    DPRINTF(("esp_input_cb(): authentication failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_badauth++;
	    error = EACCES;
	    goto baddone;
	}

	/* Remove trailing authenticator */
	m_adj(m, -(esph->authsize));

	/* We have to manually free this */
	FREE(crp->crp_opaque4, M_XDATA);
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
    if (m1 == NULL)
    {
	DPRINTF(("esp_input_cb(): bad mbuf chain, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_hdrops++;
	m_freem(m);
	return EINVAL;
    }

    /* Remove the ESP header and IV from the mbuf. */
    if (roff == 0) 
    {
	/* The ESP header was conveniently at the beginning of the mbuf */
	m_adj(m1, hlen);
	if (!(m1->m_flags & M_PKTHDR))
	  m->m_pkthdr.len -= hlen;
    }
    else
      if (roff + hlen >= m1->m_len)
      {
	  /*
	   * Part or all of the ESP header is at the end of this mbuf, so
	   * first let's remove the remainder of the ESP header from the
	   * beginning of the remainder of the mbuf chain, if any.
	   */
	  if (roff + hlen > m1->m_len)
	  {
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
      }
      else
      {
	  /* 
	   * The ESP header lies in the "middle" of the mbuf...do an
	   * overlapping copy of the remainder of the mbuf over the ESP
	   * header.
	   */
	  bcopy(mtod(m1, u_char *) + roff + hlen, mtod(m1, u_char *) + roff,
		m1->m_len - (roff + hlen));
	  m1->m_len -= hlen;
	  m->m_pkthdr.len -= hlen;
      }

    /* Save the last three bytes of decrypted data */
    m_copydata(m, m->m_pkthdr.len - 3, 3, lastthree);

    /* Verify pad length */
    if (lastthree[1] + 2 > m->m_pkthdr.len - skip - hlen)
    {
	DPRINTF(("esp_input_cb(): invalid padding length %d for packet in SA %s/%08x\n", lastthree[2], ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return EINVAL;
    }

    /* Verify correct decryption by checking the last padding bytes */
    if (!(tdb->tdb_flags & TDBF_RANDOMPADDING))
    {
	if ((lastthree[1] != lastthree[0]) && (lastthree[1] != 0))
	{
	    DPRINTF(("esp_input(): decryption failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_badenc++;
	    m_freem(m);
	    return EINVAL;
	}
    }

    /* Trim the mbuf chain to remove the trailing authenticator and padding */
    m_adj(m, -(lastthree[1] + 2));

    /* Restore the Next Protocol field */
    m_copyback(m, protoff, sizeof(u_int8_t), lastthree + 2);

    /* Back to generic IPsec input processing */
    return ipsec_common_input_cb(m, tdb, skip, protoff);

 baddone:
    if (m)
      m_freem(m);

    /* We have to manually free this */
    if (crp && crp->crp_opaque4)
      FREE(crp->crp_opaque4, M_XDATA);

    crypto_freereq(crp);

    return error;
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
    int ilen, hlen, rlen, plen, padding, blks, alen;
    struct mbuf *mi, *mo = (struct mbuf *) NULL;
    unsigned char *pad;

    struct cryptodesc *crde = NULL, *crda = NULL;
    struct cryptop *crp;

#if NBPFILTER > 0
    {
	struct ifnet *ifn;
	struct enchdr hdr;
	struct mbuf m1;

	bzero (&hdr, sizeof(hdr));

	hdr.af = tdb->tdb_dst.sa.sa_family;
	hdr.spi = tdb->tdb_spi;
	if (espx)
	  hdr.flags |= M_CONF;
	if (esph)
	  hdr.flags |= M_AUTH;

	m1.m_next = m;
	m1.m_len = ENC_HDRLEN;
	m1.m_data = (char *) &hdr;

	if (tdb->tdb_interface)
	  ifn = (struct ifnet *) tdb->tdb_interface;
	else
	  ifn = &(encif[0].sc_if);

	if (ifn->if_bpf)
	  bpf_mtap(ifn->if_bpf, &m1);
    }
#endif

    if (tdb->tdb_flags & TDBF_NOREPLAY)
      hlen = sizeof(u_int32_t) + tdb->tdb_ivlen;
    else
      hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen;

    rlen = m->m_pkthdr.len - skip; /* Raw payload length */
    if (espx)
      blks = espx->blocksize;
    else
      blks = 4; /* If no encryption is used, we have to be 4-byte aligned */

    padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;
    plen = rlen + padding; /* Padded payload length */

    if (esph)
      alen = AH_HMAC_HASHLEN;
    else
      alen = 0;

    espstat.esps_output++;

    /* Check for replay counter wrap-around in automatic (not manual) keying */
    if ((!(tdb->tdb_flags & TDBF_NOREPLAY)) &&
	(tdb->tdb_rpl == 0) && (tdb->tdb_wnd > 0))
    {
	DPRINTF(("esp_output(): SA %s/%08x should have expired\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	espstat.esps_wrap++;
	return EACCES;
    }

    switch (tdb->tdb_dst.sa.sa_family)
    {
#ifdef INET
	case AF_INET:
	    /* Check for IP maximum packet size violations */
	    if (skip + hlen + rlen + padding + alen > IP_MAXPACKET)
	    {
		DPRINTF(("esp_output(): packet in SA %s/%08x got too big\n",
			 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		m_freem(m);
		espstat.esps_toobig++;
		return EMSGSIZE;
	    }
	    break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
	    /* Check for IPv6 maximum packet size violations */
	    if (skip + hlen + rlen + padding + alen > IPV6_MAXPACKET)
	    {
		DPRINTF(("esp_output(): packet in SA %s/%08x got too big\n",
			 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		m_freem(m);
		espstat.esps_toobig++;
		return EMSGSIZE;
	    }
	    break;
#endif /* INET6 */

	default:
	    DPRINTF(("esp_output(): unknown/unsupported protocol family %d, SA %s/%08x\n", tdb->tdb_dst.sa.sa_family, ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    m_freem(m);
	    espstat.esps_nopf++;
	    return EPFNOSUPPORT;
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += m->m_pkthdr.len - skip;
    espstat.esps_obytes += m->m_pkthdr.len - skip;

    /* Hard byte expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	tdb_delete(tdb, 0, TDBEXP_TIMEOUT);
	m_freem(m);
	return EINVAL;
    }

    /* Soft byte expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;     /* Turn off checking */
    }

    /*
     * Loop through mbuf chain; if we find an M_EXT mbuf with 
     * more than one reference, replace the rest of the chain.
     */
    mi = m;
    while (mi != NULL && 
	   (!(mi->m_flags & M_EXT) || 
	    (mi->m_ext.ext_ref == NULL &&
	     mclrefcnt[mtocl(mi->m_ext.ext_buf)] <= 1)))
    {
        mo = mi;
        mi = mi->m_next;
    }
     
    if (mi != NULL)
    {
        /* Replace the rest of the mbuf chain. */
        struct mbuf *n = m_copym2(mi, 0, M_COPYALL, M_DONTWAIT);
      
        if (n == NULL)
        {
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

    /* Inject ESP header */
    mo = m_inject(m, skip, hlen, M_DONTWAIT);
    if (mo == NULL)
    {
	DPRINTF(("esp_output(): failed to inject ESP header for SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	espstat.esps_wrap++;
	return ENOBUFS;
    }

    /* Initialize ESP header */
    bcopy((caddr_t) &tdb->tdb_spi, mtod(mo, caddr_t), sizeof(u_int32_t));
    if (!(tdb->tdb_flags & TDBF_NOREPLAY))
    {
	u_int32_t replay = htonl(tdb->tdb_rpl++);
	bcopy((caddr_t) &replay, mtod(mo, caddr_t) + sizeof(u_int32_t),
	      sizeof(u_int32_t));
    }

    /*
     * Add padding -- better to do it ourselves than use the crypto engine,
     * although if/when we support compression, we'd have to do that.
     */
    pad = (u_char *) m_pad(m, padding + alen,
			   tdb->tdb_flags & TDBF_RANDOMPADDING);
    if (pad == NULL)
    {
        DPRINTF(("esp_output(): m_pad() failed for SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
      	return ENOBUFS;
    }

    /* Self-describing padding ? */
    if (!(tdb->tdb_flags & TDBF_RANDOMPADDING))
    {
	for (ilen = 0; ilen < padding - 2; ilen++)
	  pad[ilen] = ilen + 1;
    }

    /* Fix padding length and Next Protocol in padding itself */
    pad[padding - 2] = padding - 2;
    m_copydata(m, protoff, sizeof(u_int8_t), pad + padding - 1);

    /* Fix Next Protocol in IPv4/IPv6 header */
    ilen = IPPROTO_ESP;
    m_copyback(m, protoff, sizeof(u_int8_t), (u_char *) &ilen);

    /* Get crypto descriptors */
    crp = crypto_getreq(esph && espx ? 2 : 1);
    if (crp == NULL)
    {
	m_freem(m);
	DPRINTF(("esp_output(): failed to acquire crypto descriptors\n"));
	espstat.esps_crypto++;
	return ENOBUFS;
    }

    if (espx)
    {
	crde = crp->crp_desc;
	crda = crde->crd_next;

	/* Encryption descriptor */
	crde->crd_skip = skip + hlen;
	crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
	crde->crd_flags = CRD_F_ENCRYPT;
	crde->crd_inject = skip + hlen - tdb->tdb_ivlen;

        if (tdb->tdb_flags & TDBF_HALFIV)
	{
	    /* Copy half-iv in the packet */
	    m_copyback(m, crde->crd_inject, tdb->tdb_ivlen, tdb->tdb_iv);

	    /* Cook half-iv */
	    bcopy(tdb->tdb_iv, crde->crd_iv, tdb->tdb_ivlen);
	    for (ilen = 0; ilen < tdb->tdb_ivlen; ilen++)
	      crde->crd_iv[tdb->tdb_ivlen + ilen] = ~crde->crd_iv[ilen];

	    crde->crd_flags |= CRD_F_IV_PRESENT | CRD_F_IV_EXPLICIT;
	}

	/* Encryption operation */
	crde->crd_alg = espx->type;
	crde->crd_key = tdb->tdb_emxkey;
	crde->crd_klen = tdb->tdb_emxkeylen * 8;
	/* XXX Rounds ? */
    }
    else
      crda = crp->crp_desc;

    tdb->tdb_ref++;

    /* Crypto operation descriptor */
    crp->crp_ilen = m->m_pkthdr.len; /* Total input length */
    crp->crp_flags = CRYPTO_F_IMBUF;
    crp->crp_buf = (caddr_t) m;
    crp->crp_callback = (int (*) (struct cryptop *)) esp_output_cb;
    crp->crp_opaque1 = (caddr_t) tdb;
    crp->crp_sid = tdb->tdb_cryptoid;

    if (esph)
    {
	/* Authentication descriptor */
	crda->crd_skip = skip;
	crda->crd_len = m->m_pkthdr.len - (skip + alen);
	crda->crd_inject = m->m_pkthdr.len - alen;

	/* Authentication operation */
	crda->crd_alg = esph->type;
	crda->crd_key = tdb->tdb_amxkey;
	crda->crd_klen = tdb->tdb_amxkeylen * 8;
    }

    return crypto_dispatch(crp);
}

/*
 * ESP output callback, called directly by the crypto driver.
 */
int
esp_output_cb(void *op)
{
    struct cryptop *crp = (struct cryptop *) op;
    struct tdb *tdb;
    struct mbuf *m;
    int error;

    tdb = (struct tdb *) crp->crp_opaque1;
    m = (struct mbuf *) crp->crp_buf;

    tdb->tdb_ref--;

    /* Check for crypto errors */
    if (crp->crp_etype)
    {
	/* Reset session ID */
	if (tdb->tdb_cryptoid != 0)
	  tdb->tdb_cryptoid = crp->crp_sid;

	if (crp->crp_etype == EAGAIN)
	{
	    tdb->tdb_ref++;
	    return crypto_dispatch(crp);
	}

	espstat.esps_noxform++;
	DPRINTF(("esp_output_cb(): crypto error %d\n", crp->crp_etype));
	error = crp->crp_etype;
	goto baddone;
    }

    /* Shouldn't happen... */
    if (!m)
    {
	espstat.esps_crypto++;
	DPRINTF(("esp_output_cb(): bogus returned buffer from crypto\n"));
	error = EINVAL;
	goto baddone;
    }

    /*
     * Check that the TDB is still valid -- not really an error, but
     * we need to handle it as such. It may happen if the TDB expired
     * or was deleted while there was a pending request in the crypto
     * queue.
     */
    if (tdb->tdb_flags & TDBF_INVALID)
    {
	espstat.esps_invalid++;
	tdb_delete(tdb, 0, 0);
	error = ENXIO;
	DPRINTF(("esp_output_cb(): TDB expired while processing crypto\n"));
	goto baddone;
    }

    /* Release crypto descriptors */
    crypto_freereq(crp);

    /*
     * If we're doing half-iv, keep a copy of the last few bytes of the
     * encrypted part, for use as the next IV. Note that HALF-IV is only
     * supposed to be used without authentication (the old ESP specs).
     */
    if (tdb->tdb_flags & TDBF_HALFIV)
      m_copydata(m, m->m_pkthdr.len - tdb->tdb_ivlen, tdb->tdb_ivlen,
		 tdb->tdb_iv);

    /* Call the IPsec input callback */
    return ipsp_process_done(m, tdb);

 baddone:
    if (m)
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
		    u_int32_t window, u_int32_t *bitmap)
{
    u_int32_t diff;

    seq -= initial;

    if (seq == 0)
      return 1;

    if (seq > *lastseq - initial)
    {
	diff = seq - (*lastseq - initial);
	if (diff < window)
	  *bitmap = ((*bitmap) << diff) | 1;
	else
	  *bitmap = 1;
	*lastseq = seq + initial;
	return 0;
    }

    diff = *lastseq - initial - seq;
    if (diff >= window)
    {
	espstat.esps_wrap++;
	return 2;
    }

    if ((*bitmap) & (((u_int32_t) 1) << diff))
    {
	espstat.esps_replay++;
	return 3;
    }

    *bitmap |= (((u_int32_t) 1) << diff);
    return 0;
}

/*
 * m_pad(m, n) pads <m> with <n> bytes at the end. The packet header
 * length is updated, and a pointer to the first byte of the padding
 * (which is guaranteed to be all in one mbuf) is returned. The third
 * argument specifies whether we need randompadding or not.
 */

caddr_t
m_pad(struct mbuf *m, int n, int randompadding)
{
    register struct mbuf *m0, *m1;
    register int len, pad;
    caddr_t retval;
    u_int8_t dat;
	
    if (n <= 0)			/* no stupid arguments */
    {
	DPRINTF(("m_pad(): pad length invalid (%d)\n", n));
        return NULL;
    }

    len = m->m_pkthdr.len;
    pad = n;

    m0 = m;

    while (m0->m_len < len)
    {
	len -= m0->m_len;
	m0 = m0->m_next;
    }

    if (m0->m_len != len)
    {
	DPRINTF(("m_pad(): length mismatch (should be %d instead of %d)\n",
		 m->m_pkthdr.len, m->m_pkthdr.len + m0->m_len - len));
	m_freem(m);
	return NULL;
    }

    if ((m0->m_flags & M_EXT) ||
	(m0->m_data + m0->m_len + pad >= &(m0->m_dat[MLEN])))
    {
	/*
	 * Add an mbuf to the chain
	 */

	MGET(m1, M_DONTWAIT, MT_DATA);
	if (m1 == 0)
	{
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

    if (randompadding)
      for (len = 0; len < n; len++)
      {
	  get_random_bytes((void *) &dat, sizeof(u_int8_t));
	  retval[len] = len + dat;
      }

    return retval;
}
