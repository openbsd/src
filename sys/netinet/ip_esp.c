/*	$OpenBSD: ip_esp.c,v 1.31 2000/01/27 08:09:09 angelos Exp $ */

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
#include <netinet6/in6.h>
#include <netinet6/ip6.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <net/pfkeyv2.h>
#include <net/if_enc.h>

#include "bpfilter.h"

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

extern struct auth_hash auth_hash_hmac_md5_96;
extern struct auth_hash auth_hash_hmac_sha1_96;
extern struct auth_hash auth_hash_hmac_ripemd_160_96;

struct auth_hash *esp_hash[] = {
    &auth_hash_hmac_md5_96,
    &auth_hash_hmac_sha1_96,
    &auth_hash_hmac_ripemd_160_96
};

extern struct enc_xform enc_xform_des;
extern struct enc_xform enc_xform_3des;
extern struct enc_xform enc_xform_blf;
extern struct enc_xform enc_xform_cast5;
extern struct enc_xform enc_xform_skipjack;

struct enc_xform *esp_xform[] = {
    &enc_xform_des,
    &enc_xform_3des,
    &enc_xform_blf,
    &enc_xform_cast5,
    &enc_xform_skipjack,
};

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
    int i;

    /* Check whether the encryption algorithm is supported */
    for (i = sizeof(esp_xform) / sizeof(esp_xform[0]) - 1;
	 i >= 0; i--) 
      if (ii->ii_encalg == esp_xform[i]->type)
	break;

    if (i < 0) 
    {
	DPRINTF(("esp_init(): unsupported encryption algorithm %d specified\n", ii->ii_encalg));
        return EINVAL;
    }

    txform = esp_xform[i];

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

    if (ii->ii_authalg)
    {
	for (i = sizeof(esp_hash) / sizeof(esp_hash[0]) - 1;
	     i >= 0; i--) 
	  if (ii->ii_authalg == esp_hash[i]->type)
	    break;

	if (i < 0) 
	{
	    DPRINTF(("esp_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
	    return EINVAL;
	}

	thash = esp_hash[i];

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

    /* Initialize the IV */
    get_random_bytes(tdbp->tdb_iv, tdbp->tdb_ivlen);

    /* Save the raw keys */
    tdbp->tdb_amxkeylen = ii->ii_authkeylen;
    MALLOC(tdbp->tdb_amxkey, u_int8_t *, tdbp->tdb_amxkeylen, M_XDATA,
	   M_WAITOK);
    bcopy(ii->ii_authkey, tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);

    tdbp->tdb_emxkeylen = ii->ii_enckeylen;
    MALLOC(tdbp->tdb_emxkey, u_int8_t *, tdbp->tdb_emxkeylen, M_XDATA,
	   M_WAITOK);
    bcopy(ii->ii_enckey, tdbp->tdb_emxkey, tdbp->tdb_emxkeylen);

    if (txform->setkey)
	txform->setkey(&tdbp->tdb_key, ii->ii_enckey, ii->ii_enckeylen);

    if (thash)
    {
	/* Precompute the I and O pads of the HMAC */
	for (i = 0; i < ii->ii_authkeylen; i++)
	  ii->ii_authkey[i] ^= HMAC_IPAD_VAL;

	MALLOC(tdbp->tdb_ictx, u_int8_t *, thash->ctxsize, M_XDATA, M_WAITOK);
	bzero(tdbp->tdb_ictx, thash->ctxsize);
	thash->Init(tdbp->tdb_ictx);
	thash->Update(tdbp->tdb_ictx, ii->ii_authkey, ii->ii_authkeylen);
	thash->Update(tdbp->tdb_ictx, hmac_ipad_buffer,
		      HMAC_BLOCK_LEN - ii->ii_authkeylen);
	 
	for (i = 0; i < ii->ii_authkeylen; i++)
	  ii->ii_authkey[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

	MALLOC(tdbp->tdb_octx, u_int8_t *, thash->ctxsize, M_XDATA, M_WAITOK);
	bzero(tdbp->tdb_octx, thash->ctxsize);
	thash->Init(tdbp->tdb_octx);
	thash->Update(tdbp->tdb_octx, ii->ii_authkey, ii->ii_authkeylen);
	thash->Update(tdbp->tdb_octx, hmac_opad_buffer,
		      HMAC_BLOCK_LEN - ii->ii_authkeylen);
    }

    return 0;
}

int
esp_zeroize(struct tdb *tdbp)
{
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

    if (tdbp->tdb_key && tdbp->tdb_encalgxform &&
        tdbp->tdb_encalgxform->zerokey)
      tdbp->tdb_encalgxform->zerokey(&tdbp->tdb_key);

    if (tdbp->tdb_ictx)
    {
	if (tdbp->tdb_authalgxform)
	  bzero(tdbp->tdb_ictx, tdbp->tdb_authalgxform->ctxsize);
	FREE(tdbp->tdb_ictx, M_XDATA);
	tdbp->tdb_ictx = NULL;
    }

    if (tdbp->tdb_octx)
    {
	if (tdbp->tdb_authalgxform)
	  bzero(tdbp->tdb_octx, tdbp->tdb_authalgxform->ctxsize);
	FREE(tdbp->tdb_octx, M_XDATA);
	tdbp->tdb_octx = NULL;
    }

    return 0;
}

#define MAXBUFSIZ (AH_ALEN_MAX > ESP_MAX_IVS ? AH_ALEN_MAX : ESP_MAX_IVS)

struct mbuf *
esp_input(struct mbuf *m, struct tdb *tdb, int skip, int protoff)
{
    struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
    struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
    int oplen, plen, alen, ilen, i, blks, rest, count, off, roff, hlen;
    u_char iv[MAXBUFSIZ], niv[MAXBUFSIZ], blk[ESP_MAX_BLKS], *lblk;
    u_char *idat, *odat, *ivp, *ivn;
    struct mbuf *mi, *mo, *m1;
    union authctx ctx;
    u_int32_t btsx;

    /* Determine the ESP header length */
    if (tdb->tdb_flags & TDBF_NOREPLAY)
      hlen = sizeof(u_int32_t) + tdb->tdb_ivlen; /* "old" ESP */
    else
      hlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen; /* "new" ESP */

    blks = espx->blocksize;

    if (esph)
      alen = AH_HMAC_HASHLEN;
    else
      alen = 0;

    /* Skip the IP header, IP options, SPI, IV, and any Replay and Auth Data */
    plen = m->m_pkthdr.len - (skip + hlen + alen);
    if ((plen & (blks - 1)) || (plen <= 0))
    {
	DPRINTF(("esp_input(): payload not a multiple of %d octets, SA %s/%08x\n", blks, ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    /* Auth covers SPI + SN + IV */
    oplen = plen + hlen;

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
		return NULL;

	    case 2:
	    case 3:
		DPRINTF(("esp_input(): duplicate packet received in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		espstat.esps_replay++;
		m_freem(m);
		return NULL;

	    default:
		DPRINTF(("esp_input(): bogus value from checkreplaywindow32() in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		espstat.esps_replay++;
		m_freem(m);
		return NULL;
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
	return NULL;
    }

    /* Notify on expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;       /* Turn off checking */
    }

    /* 
     * Skip forward to the beginning of the ESP header. If we run out
     * of mbufs in the process, the check inside the following while()
     * loop will catch it.
     */
    for (mo = m, i = 0; mo && i + mo->m_len <= skip; mo = mo->m_next)
      i += mo->m_len;

    off = skip - i;

    /* Preserve these for later processing */
    roff = off;
    m1 = mo;

    /* Verify the authenticator, if applicable */
    if (esph)
    {
	bcopy(tdb->tdb_ictx, &ctx, esph->ctxsize);

	/* Copy the authentication data */
	m_copydata(m, m->m_pkthdr.len - alen, alen, iv);

	/* Compute authenticator over the mbuf chain */
	while (oplen > 0)
	{
	    if (mo == NULL)
	    {
		DPRINTF(("esp_input(): bad mbuf chain, SA %s/%08x\n",
			 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		espstat.esps_hdrops++;
		m_freem(m);
		return NULL;
	    }

	    count = min(mo->m_len - off, oplen);
	    esph->Update(&ctx, mtod(mo, unsigned char *) + off, count);
	    oplen -= count;
	    off = 0;
	    mo = mo->m_next;
	}

	esph->Final(niv, &ctx);
	bcopy(tdb->tdb_octx, &ctx, esph->ctxsize);
	esph->Update(&ctx, niv, esph->hashsize);
	esph->Final(niv, &ctx);

	/* Verify */
	if (bcmp(niv, iv, AH_HMAC_HASHLEN))
	{
	    DPRINTF(("esp_input(): authentication failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_badauth++;
	    m_freem(m);
	    return NULL;
	}
    }

    oplen = plen;

    /* Find beginning of encrypted data (actually, the IV) */
    mi = m1;
    ilen = mi->m_len - roff - sizeof(u_int32_t);
    if (!(tdb->tdb_flags & TDBF_NOREPLAY))
      ilen -= sizeof(u_int32_t);
    while (ilen <= 0)
    {
	mi = mi->m_next;
	if (mi == NULL)
	{
	    DPRINTF(("esp_input(): bad mbuf chain, SA %s/%08x\n",
		     ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_hdrops++;
	    m_freem(m);
	    return NULL;
	}

	ilen += mi->m_len;
    }

    idat = mtod(mi, unsigned char *) + (mi->m_len - ilen);
    m_copydata(mi, mi->m_len - ilen, tdb->tdb_ivlen, iv);

    /* If we're doing half-IV, generate full IV */
    if (tdb->tdb_flags & TDBF_HALFIV)
    {
	for (i = 0; i < tdb->tdb_ivlen; i++)
	  iv[tdb->tdb_ivlen + i] = ~iv[i];
    }

    /* Now skip over the IV */
    ilen -= tdb->tdb_ivlen;
    while (ilen <= 0)
    {
	mi = mi->m_next;
	if (mi == NULL)
	{
	    DPRINTF(("esp_input(): bad mbuf chain, SA %s/%08x\n",
		     ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_hdrops++;
	    m_freem(m);
	    return NULL;
	}

	ilen += mi->m_len;
    }

    /*
     * Remove the ESP header and IV from the mbuf.
     */
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
	   * Part or all of the ESP header is at the end of this mbuf, so first
	   * let's remove the remainder of the ESP header from the
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

    /* Point to the encrypted data */
    idat = mtod(mi, unsigned char *) + (mi->m_len - ilen);

    /*
     * At this point:
     *   plen is # of encapsulated payload octets
     *   ilen is # of octets left in this mbuf
     *   idat is first encapsulated payload octed in this mbuf
     *   same for olen and odat
     *   ivp points to the IV, ivn buffers the next IV.
     *   mi points to the first mbuf
     *
     * From now on until the end of the mbuf chain:
     *   . move the next eight octets of the chain into ivn
     *   . decrypt idat and xor with ivp
     *   . swap ivp and ivn.
     *   . repeat
     */

    ivp = iv;
    ivn = niv;
    rest = ilen % blks;
    while (plen > 0)		/* while not done */
    {
	if (ilen < blks) 
	{
	    if (rest)
	    {
		bcopy(idat, blk, rest);
		odat = idat;
	    }

	    do {
		mi = (mo = mi)->m_next;
		if (mi == NULL)
		{
		    DPRINTF(("esp_input(): bad mbuf chain, SA %s/%08x\n",
			     ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    espstat.esps_hdrops++;
		    m_freem(m);
		    return NULL;
		}
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL) 
		{
		    DPRINTF(("esp_input(): m_pullup() failed, SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    m_freem(m);
		    espstat.esps_hdrops++;
		    return NULL;
		}
		/* 
		 * m_pullup was not called at the beginning of the chain
		 * but might return a new mbuf, link it into the chain.
		 */
		mo->m_next = mi;
	    }
		    
	    ilen = mi->m_len;
	    idat = mtod(mi, u_char *);

	    if (rest)
	    {
		bcopy(idat, blk + rest, blks - rest);
		bcopy(blk, ivn, blks);
		    
		espx->decrypt(tdb, blk);

		for (i = 0; i < blks; i++)
		    blk[i] ^= ivp[i];

		ivp = ivn;
		ivn = (ivp == iv) ? niv : iv;

		bcopy(blk, odat, rest);
		bcopy(blk + rest, idat, blks - rest);

		lblk = blk;   /* last block touched */
		
		idat += blks - rest;
		ilen -= blks - rest;
		plen -= blks;
	    }

	    rest = ilen % blks;
	}

	while (ilen >= blks && plen > 0)
	{
	    bcopy(idat, ivn, blks);

	    espx->decrypt(tdb, idat);

	    for (i = 0; i < blks; i++)
		idat[i] ^= ivp[i];

	    ivp = ivn;
	    ivn = (ivp == iv) ? niv : iv;

	    lblk = idat;   /* last block touched */
	    idat += blks;

	    ilen -= blks;
	    plen -= blks;
	}
    }

    /* Save last block (end of padding), if it was in-place decrypted */
    if (lblk != blk)
      bcopy(lblk, blk, blks);

    /*
     * Now, the entire chain has been decrypted. As a side effect,
     * blk[blks - 1] contains the next protocol, and blk[blks - 2] contains
     * the amount of padding the original chain had. Chop off the
     * appropriate parts of the chain, and return.
     */

    if (blk[blks - 2] + 2 + alen > m->m_pkthdr.len - skip - hlen)
    {
	DPRINTF(("esp_input(): invalid padding length %d for packet in SA %s/%08x\n", blk[blks - 2], ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    /* Verify correct decryption by checking the last padding bytes. */
    if (!(tdb->tdb_flags & TDBF_RANDOMPADDING))
    {
	if ((blk[blks - 2] != blk[blks - 3]) && (blk[blks - 2] != 0))
	{
	    DPRINTF(("esp_input(): decryption failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_badenc++;
	    m_freem(m);
	    return NULL;
	}
    }

    /* Trim the mbuf chain to remove the trailing authenticator */
    m_adj(m, - blk[blks - 2] - 2 - alen);

    /* Restore the Next Protocol field */
    m_copyback(m, protoff, 1, &blk[blks - 1]);

    return m;
}

int
esp_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
	       int protoff)
{
    struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
    struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
    u_char iv[ESP_MAX_IVS], blk[ESP_MAX_BLKS], auth[AH_ALEN_MAX];
    int i, ilen, hlen, rlen, plen, padding, rest, blks, alen;
    struct mbuf *mi, *mo = (struct mbuf *) NULL;
    u_char *pad, *idat, *odat, *ivp;
    union authctx ctx;

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

    blks = espx->blocksize;
    rlen = m->m_pkthdr.len - skip; /* Raw payload length */
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
	DPRINTF(("esp_output(): SA %s/%0x8 should have expired\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	espstat.esps_wrap++;
	return ENOBUFS;
    }

#ifdef INET
    /* Check for IPv6 maximum packet size violations */
    if (tdb->tdb_dst.sa.sa_family == AF_INET)
      if (skip + hlen + rlen + padding + alen > IP_MAXPACKET)
      {
	  DPRINTF(("esp_output(): packet in SA %s/%0x8 got too big\n",
		   ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	  m_freem(m);
	  espstat.esps_toobig++;
	  return EMSGSIZE;
      }
#endif /* INET */

#ifdef INET6
    /* Check for IPv6 maximum packet size violations */
    if (tdb->tdb_dst.sa.sa_family == AF_INET6)
      if (skip + hlen + rlen + padding + alen > IPV6_MAXPACKET)
      {
	  DPRINTF(("esp_output(): packet in SA %s/%0x8 got too big\n",
		   ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	  m_freem(m);
	  espstat.esps_toobig++;
	  return EMSGSIZE;
      }
#endif /* INET6 */

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
    mo = m_inject(m, skip, hlen, M_WAITOK);
    if (mo == NULL)
    {
	DPRINTF(("esp_output(): failed to inject ESP header for SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	espstat.esps_wrap++;
	return ENOBUFS;
    }

    /* Initialize ESP header */
    m_copyback(mo, 0, sizeof(u_int32_t), (caddr_t) &tdb->tdb_spi);
    if (!(tdb->tdb_flags & TDBF_NOREPLAY))
    {
	u_int32_t replay = htonl(tdb->tdb_rpl++);
	m_copyback(mo, sizeof(u_int32_t), sizeof(u_int32_t),
		   (caddr_t) &replay);
    }

    /* Initialize IV (cook the half-IV if applicable) */
    bcopy(tdb->tdb_iv, iv, tdb->tdb_ivlen);
    if (tdb->tdb_flags & TDBF_HALFIV)
    {
	for (i = 0; i < tdb->tdb_ivlen; i++)
	  iv[i + tdb->tdb_ivlen] = ~iv[i];
    }

    /* Copy IV in ESP header */
    m_copyback(mo, hlen - tdb->tdb_ivlen, tdb->tdb_ivlen,
	       (caddr_t) &tdb->tdb_iv);

    /* Add padding */
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
    m_copydata(m, protoff, 1, &pad[padding - 1]);

    /* Fix Next Protocol in IPv4/IPv6 header */
    ilen = IPPROTO_ESP;
    m_copyback(m, protoff, sizeof(u_int8_t), (u_char *) &ilen);

    mi = mo;

    /* If it's just the ESP header, just skip to the next mbuf */
    if (mi->m_len == hlen)
    {
	mi = mi->m_next;
	ilen = mi->m_len;
	idat = mtod(mi, u_char *);
    }
    else
    {  /* There's data at the end of this mbuf, skip over ESP header */
	ilen = mi->m_len - hlen;
	idat = mtod(mi, u_char *) + hlen;
    }

    /* Authenticate the ESP header if applicable */
    if (esph)
    {
	bcopy(tdb->tdb_ictx, &ctx, esph->ctxsize);
	esph->Update(&ctx, mtod(mo, unsigned char *), hlen);
    }

    /* Encrypt the payload */
    ivp = iv;
    rest = ilen % blks;
    while (plen > 0)		/* while not done */
    {
	if (ilen < blks) 
	{
	    if (rest)
	    {
	        if (ivp == blk)
		{
		    bcopy(blk, iv, blks);
		    ivp = iv;
		}

		bcopy(idat, blk, rest);
		odat = idat;
	    }

	    do {
		mi = (mo = mi)->m_next;
		if (mi == NULL)
		{
		    DPRINTF(("esp_output(): bad mbuf chain, SA %s/%08x\n",
			     ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    espstat.esps_hdrops++;
		    m_freem(m);
		    return EINVAL;
		}
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL)
		{
		    DPRINTF(("esp_output(): m_pullup() failed, SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    m_freem(m);
		    espstat.esps_hdrops++;
		    return ENOBUFS;
		}
		/* 
		 * m_pullup was not called at the beginning of the chain
		 * but might return a new mbuf, link it into the chain.
		 */
		mo->m_next = mi;
	    }
		    
	    ilen = mi->m_len;
	    idat = mtod(mi, u_char *);

	    if (rest)
	    {
		bcopy(idat, blk + rest, blks - rest);
		    
		for (i = 0; i < blks; i++)
		  blk[i] ^= ivp[i];

		espx->encrypt(tdb, blk);

		if (esph)
		  esph->Update(&ctx, blk, blks);

		ivp = blk;

		bcopy(blk, odat, rest);
		bcopy(blk + rest, idat, blks - rest);
		
		idat += blks - rest;
		ilen -= blks - rest;
		plen -= blks;
	    }

	    rest = ilen % blks;
	}

	while (ilen >= blks && plen > 0)
	{
	    for (i = 0; i < blks; i++)
	      idat[i] ^= ivp[i];

	    espx->encrypt(tdb, idat);

	    if (esph)
	      esph->Update(&ctx, idat, blks);

	    ivp = idat;
	    idat += blks;

	    ilen -= blks;
	    plen -= blks;
	}
    }

    /* Put in authentication data */
    if (esph)
    {
	esph->Final(auth, &ctx);
	bcopy(tdb->tdb_octx, &ctx, esph->ctxsize);
	esph->Update(&ctx, auth, esph->hashsize);
	esph->Final(auth, &ctx);

	/* Copy the final authenticator -- cheat and use bcopy() again */
	bcopy(auth, pad + padding, alen);
    }
    
    /* Save the last encrypted block, to be used as the next IV */
    bcopy(ivp, tdb->tdb_iv, tdb->tdb_ivlen);

    *mp = m;

    return 0;
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
 *
 *
 * m_pad(m, n) pads <m> with <n> bytes at the end. The packet header
 * length is updated, and a pointer to the first byte of the padding
 * (which is guaranteed to be all in one mbuf) is returned. The third
 * argument specifies whether we need randompadding or not.
 *
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
