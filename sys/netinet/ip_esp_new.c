/*	$OpenBSD: ip_esp_new.c,v 1.34 1999/02/24 22:33:02 angelos Exp $	*/

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
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
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
 * Based on draft-ietf-ipsec-esp-v2-00.txt and
 * draft-ietf-ipsec-ciph-{des,3des}-{derived,expiv}-00.txt
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

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <net/pfkeyv2.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);
extern void des_set_key(caddr_t, caddr_t);

static void des1_encrypt(struct tdb *, u_int8_t *);
static void des3_encrypt(struct tdb *, u_int8_t *);
static void blf_encrypt(struct tdb *, u_int8_t *);
static void cast5_encrypt(struct tdb *, u_int8_t *);
static void des1_decrypt(struct tdb *, u_int8_t *);
static void des3_decrypt(struct tdb *, u_int8_t *);
static void blf_decrypt(struct tdb *, u_int8_t *);
static void cast5_decrypt(struct tdb *, u_int8_t *);

struct auth_hash esp_new_hash[] = {
     { SADB_AALG_MD5HMAC96, "HMAC-MD5-96", 
       MD5HMAC96_KEYSIZE, AH_MD5_ALEN,
       sizeof(MD5_CTX),
       (void (*) (void *)) MD5Init, 
       (void (*) (void *, u_int8_t *, u_int16_t)) MD5Update, 
       (void (*) (u_int8_t *, void *)) MD5Final 
     },
     { SADB_AALG_SHA1HMAC96, "HMAC-SHA1-96",
       SHA1HMAC96_KEYSIZE, AH_SHA1_ALEN,
       sizeof(SHA1_CTX),
       (void (*) (void *)) SHA1Init, 
       (void (*) (void *, u_int8_t *, u_int16_t)) SHA1Update, 
       (void (*) (u_int8_t *, void *)) SHA1Final 
     },
     { SADB_AALG_X_RIPEMD160HMAC96, "HMAC-RIPEMD-160-96",
       RIPEMD160HMAC96_KEYSIZE, AH_RMD160_ALEN,
       sizeof(RMD160_CTX),
       (void (*)(void *)) RMD160Init, 
       (void (*)(void *, u_int8_t *, u_int16_t)) RMD160Update, 
       (void (*)(u_int8_t *, void *)) RMD160Final 
     }
};

struct enc_xform esp_new_xform[] = {
     { SADB_EALG_DESCBC, "Data Encryption Standard (DES)",
       ESP_DES_BLKS, ESP_DES_IVS,
       8, 8, 8,
       des1_encrypt,
       des1_decrypt 
     },
     { SADB_EALG_3DESCBC, "Triple DES (3DES)",
       ESP_3DES_BLKS, ESP_3DES_IVS,
       24, 24, 8,
       des3_encrypt,
       des3_decrypt 
     },
     { SADB_EALG_X_BLF, "Blowfish",
       ESP_BLF_BLKS, ESP_BLF_IVS,
       5, BLF_MAXKEYLEN, 8,
       blf_encrypt,
       blf_decrypt 
     },
     { SADB_EALG_X_CAST, "CAST",
       ESP_CAST_BLKS, ESP_CAST_IVS,
       5, 16, 8,
       cast5_encrypt,
       cast5_decrypt 
     }
};

static void
des1_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb_encrypt(blk, blk, tdb->tdb_key, 1);
}

static void
des1_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb_encrypt(blk, blk, tdb->tdb_key, 0);
}

static void
des3_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb3_encrypt(blk, blk, tdb->tdb_key, tdb->tdb_key + 128,
		     tdb->tdb_key + 256, 1);
}

static void
des3_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb3_encrypt(blk, blk, tdb->tdb_key + 256, tdb->tdb_key + 128,
		     tdb->tdb_key, 0);
}

static void
blf_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    blf_ecb_encrypt((blf_ctx *) tdb->tdb_key, blk, 8);
}

static void
blf_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    blf_ecb_decrypt((blf_ctx *) tdb->tdb_key, blk, 8);
}

static void
cast5_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    cast_encrypt((cast_key *) tdb->tdb_key, blk, blk);
}

static void
cast5_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    cast_decrypt((cast_key *) tdb->tdb_key, blk, blk);
}

/*
 * esp_new_attach() is called from the transformation initialization code.
 */

int
esp_new_attach()
{
    return 0;
}

/*
 * esp_new_init() is called when an SPI is being set up.
 */

int
esp_new_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
    struct enc_xform *txform = NULL;
    struct auth_hash *thash = NULL;
    int i;

    /* Check whether the encryption algorithm is supported */
    for (i = sizeof(esp_new_xform) / sizeof(struct enc_xform) - 1;
	 i >= 0; i--) 
      if (ii->ii_encalg == esp_new_xform[i].type)
	break;

    if (i < 0) 
    {
	DPRINTF(("esp_new_init(): unsupported encryption algorithm %d specified\n", ii->ii_encalg));
        return EINVAL;
    }

    txform = &esp_new_xform[i];

    if (ii->ii_enckeylen < txform->minkey)
    {
	DPRINTF(("esp_new_init(): keylength %d too small (min length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->minkey, txform->name));
	return EINVAL;
    }
    
    if (ii->ii_enckeylen > txform->maxkey)
    {
	DPRINTF(("esp_new_init(): keylength %d too large (max length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->maxkey, txform->name));
	return EINVAL;
    }

    if (ii->ii_authalg)
    {
	for (i = sizeof(esp_new_hash) / sizeof(struct auth_hash) - 1;
	     i >= 0; i--) 
	  if (ii->ii_authalg == esp_new_hash[i].type)
	    break;

	if (i < 0) 
	{
	    DPRINTF(("esp_new_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
	    return EINVAL;
	}

	thash = &esp_new_hash[i];

	if (ii->ii_authkeylen != thash->keysize)
	{
	    DPRINTF(("esp_new_init(): keylength %d doesn't match algorithm %s keysize (%d)\n", ii->ii_authkeylen, thash->name, thash->keysize));
	    return EINVAL;
	}
    
    	tdbp->tdb_authalgxform = thash;

	DPRINTF(("esp_new_init(): initialized TDB with hash algorithm %s\n",
		 thash->name));
    }
    
    tdbp->tdb_xform = xsp;
    tdbp->tdb_encalgxform = txform;
    tdbp->tdb_bitmap = 0;
    tdbp->tdb_rpl = AH_HMAC_INITIAL_RPL;

    DPRINTF(("esp_new_init(): initialized TDB with enc algorithm %s\n",
	     txform->name));

    tdbp->tdb_ivlen = txform->ivmask;

    /* Initialize the IV */
    get_random_bytes(tdbp->tdb_iv, tdbp->tdb_ivlen);

    switch (ii->ii_encalg)
    {
	case SADB_EALG_DESCBC:
	    MALLOC(tdbp->tdb_key, u_int8_t *, 128, M_XDATA, M_WAITOK);
	    bzero(tdbp->tdb_key, 128);
	    des_set_key(ii->ii_enckey, tdbp->tdb_key);
	    break;

	case SADB_EALG_3DESCBC:
	    MALLOC(tdbp->tdb_key, u_int8_t *, 384, M_XDATA, M_WAITOK);
	    bzero(tdbp->tdb_key, 384);
	    des_set_key(ii->ii_enckey, tdbp->tdb_key);
	    des_set_key(ii->ii_enckey + 8, tdbp->tdb_key + 128);
	    des_set_key(ii->ii_enckey + 16, tdbp->tdb_key + 256);
	    break;

        case SADB_EALG_X_BLF:
	    MALLOC(tdbp->tdb_key, u_int8_t *, sizeof(blf_ctx),
		   M_XDATA, M_WAITOK);
	    bzero(tdbp->tdb_key, sizeof(blf_ctx));
	    blf_key((blf_ctx *) tdbp->tdb_key, ii->ii_enckey,
		    ii->ii_enckeylen);
	    break;

        case SADB_EALG_X_CAST:
	    MALLOC(tdbp->tdb_key, u_int8_t *, sizeof(cast_key),
		   M_XDATA, M_WAITOK);
	    bzero(tdbp->tdb_key, sizeof(cast_key));
	    cast_setkey((cast_key *) tdbp->tdb_key, ii->ii_enckey,
			ii->ii_enckeylen);
	    break;
    }

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
esp_new_zeroize(struct tdb *tdbp)
{
    if (tdbp->tdb_key)
    {
	FREE(tdbp->tdb_key, M_XDATA);
	tdbp->tdb_key = NULL;
    }

    if (tdbp->tdb_ictx)
    {
	FREE(tdbp->tdb_ictx, M_XDATA);
	tdbp->tdb_ictx = NULL;
    }

    if (tdbp->tdb_octx)
    {
	FREE(tdbp->tdb_octx, M_XDATA);
	tdbp->tdb_octx = NULL;
    }

    return 0;
}


struct mbuf *
esp_new_input(struct mbuf *m, struct tdb *tdb)
{
    struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
    struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
    u_char iv[ESP_MAX_IVS], niv[ESP_MAX_IVS];
    u_char blk[ESP_MAX_BLKS], *lblk, opts[40];
    int ohlen, oplen, plen, alen, ilen, i, blks, rest;
    int count, off, errc;
    struct mbuf *mi, *mo;
    u_char *idat, *odat, *ivp, *ivn;
    struct esp_new *esp;
    struct ip *ip, ipo;
    u_int32_t btsx;
    union {
	 MD5_CTX md5ctx;
	 SHA1_CTX sha1ctx;
	 RMD160_CTX rmd160ctx;
    } ctx;
    u_char buf[AH_ALEN_MAX], buf2[AH_ALEN_MAX];

    blks = espx->blocksize;

    if (esph)
      alen = AH_HMAC_HASHLEN;
    else
      alen = 0;

    if (m->m_len < sizeof(struct ip))
    {
	if ((m = m_pullup(m, sizeof(struct ip))) == NULL)
	{
	    DPRINTF(("esp_new_input(): (possibly too short) packet dropped\n"));
	    espstat.esps_hdrops++;
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ohlen = (ip->ip_hl << 2) + ESP_NEW_FLENGTH;

    /* Make sure the IP header, any IP options, and the ESP header are here */
    if (m->m_len < ohlen + blks)
    {
	if ((m = m_pullup(m, ohlen + blks)) == NULL)
	{
            DPRINTF(("esp_new_input(): m_pullup() failed\n"));
	    espstat.esps_hdrops++;
            return NULL;
	}

	ip = mtod(m, struct ip *);
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += ip->ip_len - ohlen - alen;
    espstat.esps_ibytes += ip->ip_len - ohlen - alen;

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
    {
/* XXX
   encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
*/
	tdb_delete(tdb, 0);
	m_freem(m);
	return NULL;
    }

    /* Notify on expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
    {
/* XXX
   encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
*/
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;       /* Turn off checking */
    }

    esp = (struct esp_new *) ((u_int8_t *) ip + (ip->ip_hl << 2));
    ipo = *ip;

    /* Replay window checking */
    if (tdb->tdb_wnd > 0)
    {
	btsx = ntohl(esp->esp_rpl);
	if ((errc = checkreplaywindow32(btsx, 0, &(tdb->tdb_rpl), tdb->tdb_wnd,
					&(tdb->tdb_bitmap))) != 0)
	{
	    switch(errc)
	    {
		case 1:
		    DPRINTF(("esp_new_input(): replay counter wrapped for packets from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(esp->esp_spi)));
		    espstat.esps_wrap++;
		    break;

		case 2:
	        case 3:
		    DPRINTF(("esp_new_input(): duplicate packet received from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(esp->esp_spi)));
		    espstat.esps_replay++;
		    break;
	    }

	    m_freem(m);
	    return NULL;
	}
    }

    /* Skip the IP header, IP options, SPI, SN and IV and minus Auth Data */
    plen = m->m_pkthdr.len - (ip->ip_hl << 2) - 2 * sizeof(u_int32_t) - 
	   tdb->tdb_ivlen - alen;

    if ((plen & (blks - 1)) || (plen <= 0))
    {
	DPRINTF(("esp_new_input(): payload not a multiple of %d octets for packet from %s to %s, spi %08x\n", blks, inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    if (esph)
    {
	bcopy(tdb->tdb_ictx, &ctx, esph->ctxsize);

	/* Auth covers SPI + SN + IV */
	oplen = plen + 2 * sizeof(u_int32_t) + tdb->tdb_ivlen; 
	off = (ip->ip_hl << 2);

	/* Copy the authentication data */
	m_copydata(m, m->m_pkthdr.len - alen, alen, buf);

	mo = m;

	while (oplen > 0)
	{
	    if (mo == 0)
	    {
		DPRINTF(("esp_new_input(): bad mbuf chain for packet from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(esp->esp_spi)));
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

	esph->Final(buf2, &ctx);
	bcopy(tdb->tdb_octx, &ctx, esph->ctxsize);
	esph->Update(&ctx, buf2, esph->hashsize);
	esph->Final(buf2, &ctx);

	if (bcmp(buf2, buf, AH_HMAC_HASHLEN))
	{
	    DPRINTF(("esp_new_input(): authentication failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(esp->esp_spi)));
	    espstat.esps_badauth++;
	    m_freem(m);
	    return NULL;
	}
    }

    oplen = plen;
    ilen = m->m_len - (ip->ip_hl << 2) - 2 * sizeof(u_int32_t);
    idat = mtod(m, unsigned char *) + (ip->ip_hl << 2) + 2 * sizeof(u_int32_t);

    bcopy(idat, iv, tdb->tdb_ivlen);
    ilen -= tdb->tdb_ivlen;
    idat += tdb->tdb_ivlen;

    mi = m;

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
		    DPRINTF(("esp_new_input(): bad mbuf chain, SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    espstat.esps_hdrops++;
		    m_freem(m);
		    return NULL;
		}
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL) 
		{
		    DPRINTF(("esp_new_input(): m_pullup() failed, SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
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

    /* Save the options */
    m_copydata(m, sizeof(struct ip), (ipo.ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    if (lblk != blk)
      bcopy(lblk, blk, blks);

    /*
     * Now, the entire chain has been decrypted. As a side effect,
     * blk[blks - 1] contains the next protocol, and blk[blks - 2] contains
     * the amount of padding the original chain had. Chop off the
     * appropriate parts of the chain, and return.
     * Verify correct decryption by checking the last padding bytes.
     */

    if (blk[blks - 2] + 2 + alen > m->m_pkthdr.len - (ip->ip_hl << 2) - 2 * sizeof(u_int32_t) - tdb->tdb_ivlen)
    {
	DPRINTF(("esp_new_input(): invalid padding length %d for packet from %s to %s, spi %08x\n", blk[blks - 2], inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    if ((blk[blks - 2] != blk[blks - 3]) && (blk[blks - 2] != 0))
    {
	DPRINTF(("esp_new_input(): decryption failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	return NULL;
    } 

    m_adj(m, - blk[blks - 2] - 2 - alen);		/* Old type padding */
    m_adj(m, 2 * sizeof(u_int32_t) + tdb->tdb_ivlen);

    if (m->m_len < (ipo.ip_hl << 2))
    {
	m = m_pullup(m, (ipo.ip_hl << 2));
	if (m == NULL)
	{
	    DPRINTF(("esp_new_input(): m_pullup() failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_hdrops++;
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ipo.ip_p = blk[blks - 1];
    ipo.ip_id = htons(ipo.ip_id);
    ipo.ip_off = 0;
    ipo.ip_len += (ipo.ip_hl << 2) -  2 * sizeof(u_int32_t) - tdb->tdb_ivlen -
		  blk[blks - 2] - 2 - alen;

    ipo.ip_len = htons(ipo.ip_len);
    ipo.ip_sum = 0;
    *ip = ipo;

    /* Copy the options back */
    m_copyback(m, sizeof(struct ip), (ipo.ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    ip->ip_sum = in_cksum(m, (ip->ip_hl << 2));

    return m;
}

int
esp_new_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
	       struct mbuf **mp)
{
    struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
    struct auth_hash *esph = (struct auth_hash *) tdb->tdb_authalgxform;
    struct ip *ip, ipo;
    int i, ilen, ohlen, nh, rlen, plen, padding, rest;
    struct esp_new espo;
    struct mbuf *mi, *mo;
    u_char *pad, *idat, *odat, *ivp;
    u_char iv[ESP_MAX_IVS], blk[ESP_MAX_BLKS], auth[AH_ALEN_MAX], opts[40];
    union {
	 MD5_CTX md5ctx;
	 SHA1_CTX sha1ctx;
	 RMD160_CTX rmd160ctx;
    } ctx;
    int iphlen, blks, alen;
    
    blks = espx->blocksize;

    if (esph)
    {
      alen = AH_HMAC_HASHLEN;
      DPRINTF(("esp_new_output(): using hash algorithm: %s\n", esph->name));
    } 
    else
      alen = 0;

    espstat.esps_output++;

    m = m_pullup(m, sizeof (struct ip));   /* Get IP header in one mbuf */
    if (m == NULL)
    {
        DPRINTF(("esp_new_output(): m_pullup() failed, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_hdrops++;
	return ENOBUFS;
    }

    if (tdb->tdb_rpl == 0)
    {
	DPRINTF(("esp_new_output(): SA %s/%0x8 should have expired\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	espstat.esps_wrap++;
	return ENOBUFS;
    }

    espo.esp_spi = tdb->tdb_spi;
    espo.esp_rpl = htonl(tdb->tdb_rpl++);

    ip = mtod(m, struct ip *);
    iphlen = (ip->ip_hl << 2);
    
    /* Update the counters */
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2);
    espstat.esps_obytes += ntohs(ip->ip_len) - (ip->ip_hl << 2);

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
    {
/* XXX
   encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
*/
	tdb_delete(tdb, 0);
	m_freem(m);
	return EINVAL;
    }

    /* Notify on expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
    {
/* XXX
        encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
*/
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;     /* Turn off checking */
    }

    /*
     * If options are present, pullup the IP header, the options.
     */
    if (iphlen != sizeof(struct ip))
    {
	m = m_pullup(m, iphlen + 8);
	if (m == NULL)
	{
	    DPRINTF(("esp_new_input(): m_pullup() failed for SA %s/%08x\n",
		     ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_hdrops++;
	    return ENOBUFS;
	}

	ip = mtod(m, struct ip *);

	/* Keep the options */
	m_copydata(m, sizeof(struct ip), iphlen - sizeof(struct ip), 
		   (caddr_t) opts);
    }

    ilen = ntohs(ip->ip_len);    /* Size of the packet */
    ohlen = 2 * sizeof(u_int32_t) + tdb->tdb_ivlen;

    ipo = *ip;
    nh = ipo.ip_p;

    /* Raw payload length */
    rlen = ilen - iphlen; 
    padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;
    if (iphlen + ohlen + rlen + padding + alen > IP_MAXPACKET)
    {
	DPRINTF(("esp_new_output(): packet in SA %s/%0x8 got too big\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	espstat.esps_toobig++;
        return EMSGSIZE;
    }

    pad = (u_char *) m_pad(m, padding + alen, 0);
    if (pad == NULL)
    {
        DPRINTF(("esp_new_output(): m_pad() failed for SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
      	return ENOBUFS;
    }

    /* Self describing padding */
    for (i = 0; i < padding - 2; i++)
      pad[i] = i + 1;

    pad[padding - 2] = padding - 2;
    pad[padding - 1] = nh;

    mi = m;
    plen = rlen + padding;
    ilen = m->m_len - iphlen;
    idat = mtod(m, u_char *) + iphlen;

    bcopy(tdb->tdb_iv, iv, tdb->tdb_ivlen);
    bcopy(tdb->tdb_iv, espo.esp_iv, tdb->tdb_ivlen);

    /* Authenticate the esp header */
    if (esph)
    {
	bcopy(tdb->tdb_ictx, &ctx, esph->ctxsize);
	esph->Update(&ctx, (unsigned char *) &espo, 
		  2 * sizeof(u_int32_t) + tdb->tdb_ivlen);
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
		    DPRINTF(("esp_new_output(): bad mbuf chain, SA %s/%08x\n",
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
		    DPRINTF(("esp_new_output(): m_pullup() failed, SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
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

	/* Copy the final authenticator */
	bcopy(auth, pad + padding, alen);
    }

    /*
     * Done with encryption. Let's wedge in the ESP header
     * and send it out.
     */

    M_PREPEND(m, ohlen, M_DONTWAIT);
    if (m == NULL)
    {
        DPRINTF(("esp_new_output(): M_PREPEND failed, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }

    m = m_pullup(m, iphlen + ohlen);
    if (m == NULL)
    {
        DPRINTF(("esp_new_output(): m_pullup() failed, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_hdrops++;
        return ENOBUFS;
    }

    /* Fix the length and the next protocol, copy back and off we go */
    ipo.ip_len = htons(iphlen + ohlen + rlen + padding + alen);
    ipo.ip_p = IPPROTO_ESP;

    /* Save the last encrypted block, to be used as the next IV */
    bcopy(ivp, tdb->tdb_iv, tdb->tdb_ivlen);

    m_copyback(m, 0, sizeof(struct ip), (caddr_t) &ipo);

    /* Copy options, if existing */
    if (iphlen != sizeof(struct ip))
      m_copyback(m, sizeof(struct ip), iphlen - sizeof(struct ip),
		 (caddr_t) opts);

    /* Copy in the esp header */
    m_copyback(m, iphlen, ohlen, (caddr_t) &espo);
	
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

