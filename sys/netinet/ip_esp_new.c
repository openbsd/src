/*	$OpenBSD: ip_esp_new.c,v 1.23 1998/07/30 08:41:20 provos Exp $	*/

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
#include <net/encap.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ah.h>
#include <sys/syslog.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

extern void encap_sendnotify(int, struct tdb *, void *);
extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);
extern void des_set_key(caddr_t, caddr_t);

static void des1_encrypt(void *, u_int8_t *);
static void des3_encrypt(void *, u_int8_t *);
static void blf_encrypt(void *, u_int8_t *);
static void cast5_encrypt(void *, u_int8_t *);
static void des1_decrypt(void *, u_int8_t *);
static void des3_decrypt(void *, u_int8_t *);
static void blf_decrypt(void *, u_int8_t *);
static void cast5_decrypt(void *, u_int8_t *);

struct esp_hash esp_new_hash[] = {
     { ALG_AUTH_MD5, "HMAC-MD5-96", 
       AH_MD5_ALEN,
       sizeof(MD5_CTX),
       (void (*) (void *)) MD5Init, 
       (void (*) (void *, u_int8_t *, u_int16_t)) MD5Update, 
       (void (*) (u_int8_t *, void *)) MD5Final 
     },
     { ALG_AUTH_SHA1, "HMAC-SHA1-96",
       AH_SHA1_ALEN,
       sizeof(SHA1_CTX),
       (void (*) (void *)) SHA1Init, 
       (void (*) (void *, u_int8_t *, u_int16_t)) SHA1Update, 
       (void (*) (u_int8_t *, void *)) SHA1Final 
     },
     { ALG_AUTH_RMD160, "HMAC-RIPEMD-160-96",
       AH_RMD160_ALEN,
       sizeof(RMD160_CTX),
       (void (*)(void *)) RMD160Init, 
       (void (*)(void *, u_int8_t *, u_int16_t)) RMD160Update, 
       (void (*)(u_int8_t *, void *)) RMD160Final 
     }
};

struct esp_xform esp_new_xform[] = {
     { ALG_ENC_DES, "Data Encryption Standard (DES)",
       ESP_DES_BLKS, ESP_DES_IVS,
       8, 8, 8 | 1,
       des1_encrypt,
       des1_decrypt 
     },
     { ALG_ENC_3DES, "Tripple DES (3DES)",
       ESP_3DES_BLKS, ESP_3DES_IVS,
       24, 24, 8 | 1,
       des3_encrypt,
       des3_decrypt 
     },
     { ALG_ENC_BLF, "Blowfish",
       ESP_BLF_BLKS, ESP_BLF_IVS,
       5, BLF_MAXKEYLEN, 8 | 1,
       blf_encrypt,
       blf_decrypt 
     },
     { ALG_ENC_CAST, "CAST",
       ESP_CAST_BLKS, ESP_CAST_IVS,
       5, 16, 8 | 1,
       cast5_encrypt,
       cast5_decrypt 
     }
};

static void
des1_encrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     des_ecb_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]), 1);
}

static void
des1_decrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     des_ecb_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]), 0);
}

static void
des3_encrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     des_ecb3_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]),
		      (caddr_t) (xd->edx_eks[1]),
		      (caddr_t) (xd->edx_eks[2]), 1);
}

static void
des3_decrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     des_ecb3_encrypt(blk, blk, (caddr_t) (xd->edx_eks[2]),
		      (caddr_t) (xd->edx_eks[1]),
		      (caddr_t) (xd->edx_eks[0]), 0);
}

static void
blf_encrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     Blowfish_encipher(&xd->edx_bks, (u_int32_t *)blk,
		       (u_int32_t *) (blk + 4));
}

static void
blf_decrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     Blowfish_decipher(&xd->edx_bks, (u_int32_t *)blk,
		       (u_int32_t *) (blk + 4));
}

static void
cast5_encrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     cast_encrypt(&xd->edx_cks, blk, blk);
}

static void
cast5_decrypt(void *pxd, u_int8_t *blk)
{
     struct esp_new_xdata *xd = pxd;
     cast_decrypt(&xd->edx_cks, blk, blk);
}

/*
 * esp_new_attach() is called from the transformation initialization code.
 * It just returns.
 */

int
esp_new_attach()
{
    DPRINTF(("esp_new_attach(): setting up\n"));
    return 0;
}

/*
 * esp_new_init() is called when an SPI is being set up. It interprets the
 * encap_msghdr present in m, and sets up the transformation data, in
 * this case, the encryption and decryption key schedules
 */

int
esp_new_init(struct tdb *tdbp, struct xformsw *xsp, struct mbuf *m)
{
    struct esp_new_xdata *xd;
    struct esp_new_xencap txd;
    struct encap_msghdr *em;
    struct esp_xform *txform;
    struct esp_hash *thash;
    caddr_t buffer = NULL;
    u_int32_t rk[14];
    int blocklen, i;

    if (m->m_len < ENCAP_MSG_FIXED_LEN)
    {
    	if ((m = m_pullup(m, ENCAP_MSG_FIXED_LEN)) == NULL)
    	{
	    DPRINTF(("esp_new_init(): m_pullup failed\n"));
	    return ENOBUFS;
	}
    }

    em = mtod(m, struct encap_msghdr *);
    if (em->em_msglen - EMT_SETSPI_FLEN <= ESP_NEW_XENCAP_LEN)
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_new_init(): initialization failed\n");
	return EINVAL;
    }

    /* Just copy the standard fields */
    m_copydata(m, EMT_SETSPI_FLEN, ESP_NEW_XENCAP_LEN, (caddr_t) &txd);

    /* Check whether the encryption algorithm is supported */
    for (i = sizeof(esp_new_xform) / sizeof(struct esp_xform) - 1; i >= 0; i--) 
	if (txd.edx_enc_algorithm == esp_new_xform[i].type)
	      break;
    if (i < 0) 
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_new_init(): unsupported encryption algorithm %d specified\n", txd.edx_enc_algorithm);
        return EINVAL;
    }

    txform = &esp_new_xform[i];
    DPRINTF(("esp_new_init(): initialized TDB with enc algorithm %d: %s\n",
	     txd.edx_enc_algorithm, esp_new_xform[i].name));

    /* Check whether the authentication algorithm is supported */
    if (txd.edx_flags & ESP_NEW_FLAG_AUTH) 
    {
        for (i = sizeof(esp_new_hash) / sizeof(struct esp_hash) - 1; i >= 0;
	     i--) 
	    if (txd.edx_hash_algorithm == esp_new_hash[i].type)
	      break;
	if (i < 0) 
	{
            if (encdebug)
                log(LOG_WARNING, "esp_new_init(): unsupported authentication algorithm %d specified\n", txd.edx_hash_algorithm);
            return EINVAL;
	}

	DPRINTF(("esp_new_init(): initialized TDB with hash algorithm %d: %s\n",
		 txd.edx_hash_algorithm, esp_new_hash[i].name));
        blocklen = HMAC_BLOCK_LEN;
	thash = &esp_new_hash[i];
      }

    if (txd.edx_ivlen + txd.edx_confkeylen + txd.edx_authkeylen + 
	EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN != em->em_msglen)
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_new_init(): message length (%d) doesn't match\n", em->em_msglen);
	return EINVAL;
    }

    /* Check the IV length */
    if (((txd.edx_ivlen == 0) && !(txform->ivmask&1)) ||
	((txd.edx_ivlen != 0) && (
	     !(txd.edx_ivlen & txform->ivmask) ||
	     (txd.edx_ivlen & (txd.edx_ivlen - 1)))))
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_new_init(): unsupported IV length %d\n",
	      txd.edx_ivlen);
	return EINVAL;
    }

    /* Check the key length */
    if (txd.edx_confkeylen < txform->minkey || 
	txd.edx_confkeylen > txform->maxkey)
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_new_init(): bad key length %d\n",
	      txd.edx_confkeylen);
	return EINVAL;
    }

    MALLOC(tdbp->tdb_xdata, caddr_t, sizeof(struct esp_new_xdata),
	   M_XDATA, M_WAITOK);
    if (tdbp->tdb_xdata == NULL)
    {
        DPRINTF(("esp_new_init(): MALLOC() failed\n"));
        return ENOBUFS;
    }

    bzero(tdbp->tdb_xdata, sizeof(struct esp_new_xdata));
    xd = (struct esp_new_xdata *) tdbp->tdb_xdata;

    /* Pointer to the transform */
    tdbp->tdb_xform = xsp;

    xd->edx_ivlen = txd.edx_ivlen;
    xd->edx_enc_algorithm = txd.edx_enc_algorithm;
    xd->edx_wnd = txd.edx_wnd;
    xd->edx_flags = txd.edx_flags;
    xd->edx_hash_algorithm = txd.edx_hash_algorithm;
    xd->edx_bitmap = 0;
    xd->edx_xform = txform;

    /* Pass name of enc algorithm for kernfs */
    tdbp->tdb_confname = xd->edx_xform->name;

    /* Replay counters are mandatory, even without auth */
    xd->edx_rpl = AH_HMAC_INITIAL_RPL;

    /* Copy the IV */
    m_copydata(m, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN, xd->edx_ivlen,
	       (caddr_t) xd->edx_iv);

    /* Copy the key material */
    m_copydata(m, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + xd->edx_ivlen,
	       txd.edx_confkeylen, (caddr_t) rk);
    switch (xd->edx_enc_algorithm)
    {
	case ALG_ENC_DES:
	    des_set_key((caddr_t) rk, (caddr_t) (xd->edx_eks[0]));
	    break;

	case ALG_ENC_3DES:
	    des_set_key((caddr_t) rk, (caddr_t) (xd->edx_eks[0]));
	    des_set_key((caddr_t) (rk + 2), (caddr_t) (xd->edx_eks[1]));
	    des_set_key((caddr_t) (rk + 4), (caddr_t) (xd->edx_eks[2]));
	    break;
        case ALG_ENC_BLF:
	    blf_key(&xd->edx_bks, (caddr_t) rk, txd.edx_confkeylen);
	    break;
        case ALG_ENC_CAST:
	    cast_setkey(&xd->edx_cks, (caddr_t) rk, txd.edx_confkeylen);
	    break;
    }

    if (txd.edx_flags & ESP_NEW_FLAG_AUTH)
    {
	xd->edx_hash = thash;

	/* Pass name of auth algorithm for kernfs */
	tdbp->tdb_authname = xd->edx_hash->name;

	DPRINTF(("esp_new_init(): using %d bytes of authentication key\n",
		 txd.edx_authkeylen));

	MALLOC(buffer, caddr_t, 
	       txd.edx_authkeylen < blocklen ? blocklen : txd.edx_authkeylen,
	       M_TEMP, M_WAITOK);
	if (buffer == NULL)
	{
	    DPRINTF(("esp_new_init(): MALLOC() failed\n"));
	    free(tdbp->tdb_xdata, M_XDATA);
	    return ENOBUFS;
	}

	bzero(buffer, txd.edx_authkeylen < blocklen ? 
	      blocklen : txd.edx_authkeylen);

	/* Copy the key to the buffer */
	m_copydata(m, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + xd->edx_ivlen +
		   txd.edx_confkeylen, txd.edx_authkeylen, buffer);

	/* Shorten the key if necessary */
	if (txd.edx_authkeylen > blocklen)
	{
	    xd->edx_hash->Init(&(xd->edx_ictx));
	    xd->edx_hash->Update(&(xd->edx_ictx), buffer, txd.edx_authkeylen);
	    bzero(buffer, txd.edx_authkeylen < blocklen ? 
		  blocklen : txd.edx_authkeylen);
	    xd->edx_hash->Final(buffer, &(xd->edx_ictx));
	}

	/* Precompute the I and O pads of the HMAC */
	for (i = 0; i < blocklen; i++)
	  buffer[i] ^= HMAC_IPAD_VAL;

	xd->edx_hash->Init(&(xd->edx_ictx));
	xd->edx_hash->Update(&(xd->edx_ictx), buffer, blocklen);
	 
	for (i = 0; i < blocklen; i++)
	  buffer[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

	xd->edx_hash->Init(&(xd->edx_octx));
	xd->edx_hash->Update(&(xd->edx_octx), buffer, blocklen);

	bzero(buffer, blocklen);
	free(buffer, M_TEMP);
    }

    bzero(rk, 14 * sizeof(u_int32_t));		/* paranoid */
    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */

    return 0;
}

int
esp_new_zeroize(struct tdb *tdbp)
{
    DPRINTF(("esp_new_zeroize(): freeing memory\n"));
    if (tdbp->tdb_xdata)
    {
      	FREE(tdbp->tdb_xdata, M_XDATA);
	tdbp->tdb_xdata = NULL;
    }
    return 0;
}


struct mbuf *
esp_new_input(struct mbuf *m, struct tdb *tdb)
{
    u_char iv[ESP_MAX_IVS], niv[ESP_MAX_IVS];
    u_char blk[ESP_MAX_BLKS], *lblk, opts[40];
    int ohlen, oplen, plen, alen, ilen, i, blks, rest;
    struct esp_new_xdata *xd;
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

    xd = (struct esp_new_xdata *) tdb->tdb_xdata;

    blks = xd->edx_xform->blocksize;

    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
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

    esp = (struct esp_new *) ((u_int8_t *) ip + (ip->ip_hl << 2));

    ipo = *ip;

    /* Replay window checking */
    if (xd->edx_wnd >= 0)
    {
	btsx = ntohl(esp->esp_rpl);
	if ((errc = checkreplaywindow32(btsx, 0, &(xd->edx_rpl), xd->edx_wnd,
					&(xd->edx_bitmap))) != 0)
	{
	    switch(errc)
	    {
		case 1:
		    if (encdebug)
		      log(LOG_ERR, "esp_new_input(): replay counter wrapped for packets from %x to %x, spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(esp->esp_spi));
		    espstat.esps_wrap++;
		    break;

		case 2:
	        case 3:
		    if (encdebug) 
		      log(LOG_WARNING, "esp_new_input(): duplicate packet received, %x->%x spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(esp->esp_spi));
		    espstat.esps_replay++;
		    break;
	    }

	    m_freem(m);
	    return NULL;
	}
    }

    /* Skip the IP header, IP options, SPI, SN and IV and minus Auth Data */
    plen = m->m_pkthdr.len - (ip->ip_hl << 2) - 2 * sizeof(u_int32_t) - 
	   xd->edx_ivlen - alen;

    if ((plen & (blks - 1)) || (plen <= 0))
    {
	DPRINTF(("esp_new_input(): payload not a multiple of %d octets for packet from %x to %x, spi %08x\n", blks, ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    if (xd->edx_flags & ESP_NEW_FLAG_AUTH) 
    {
	bcopy(&(xd->edx_ictx), &ctx, xd->edx_hash->ctxsize);

	/* Auth covers SPI + SN + IV*/
	oplen = plen + 2 * sizeof(u_int32_t) + xd->edx_ivlen; 
	off = (ip->ip_hl << 2);

	mo = m;
	while (oplen > 0)
	{
	    if (mo == 0)
	      panic("esp_new_input(): m_copydata (copy)");

	    count = min(mo->m_len - off, oplen);

	    xd->edx_hash->Update(&ctx, mtod(mo, unsigned char *) + off, count);
	    oplen -= count;
	    if (oplen == 0) 
	    {
		/* Get the authentication data */
		if (mo->m_len - off - count >= alen)
		  bcopy(mtod(mo, unsigned char *) + off + count, buf, alen);
		else 
		{
		    int olen = alen, tmp = 0;
		      
		    mi = mo;
		    off += count;
		      
		    while (mi != NULL && olen > 0) 
		    {
			count = min(mi->m_len - off, olen);
			bcopy(mtod(mi, unsigned char *) + off, buf + tmp,
			      count);
			  
			off = 0;
			tmp += count;
			olen -= count;
			mi = mi->m_next;
		    }
		}
	    }
		   
	    off = 0;
	    mo = mo->m_next;
	}

	xd->edx_hash->Final(buf2, &ctx);
	bcopy(&(xd->edx_octx), &ctx, xd->edx_hash->ctxsize);
	xd->edx_hash->Update(&ctx, buf2, xd->edx_hash->hashsize);
	xd->edx_hash->Final(buf2, &ctx);

	if (bcmp(buf2, buf, AH_HMAC_HASHLEN))
	{
	    if (encdebug)
	      log(LOG_ALERT, "esp_new_input(): authentication failed for packet from %x to %x, spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(esp->esp_spi));
	    espstat.esps_badauth++;
	    m_freem(m);
	    return NULL;
	}
    }

    oplen = plen;
    ilen = m->m_len - (ip->ip_hl << 2) - 2 * sizeof(u_int32_t);
    idat = mtod(m, unsigned char *) + (ip->ip_hl << 2) + 2 * sizeof(u_int32_t);

    if (xd->edx_ivlen == 0)		/* Derived IV in use */
    {
	bcopy((u_char *) &esp->esp_rpl, iv, sizeof(esp->esp_rpl));
	iv[4] = ~iv[0];
	iv[5] = ~iv[1];
	iv[6] = ~iv[2];
	iv[7] = ~iv[3];
    }
    else
    {
	bcopy(idat, iv, xd->edx_ivlen);
	ilen -= xd->edx_ivlen;
	idat += xd->edx_ivlen;
    }
    
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
		    panic("esp_new_input(): bad chain (i)\n");
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL) 
		{
		    DPRINTF(("esp_new_input(): m_pullup() failed, SA %x/%08x\n",
			       tdb->tdb_dst, ntohl(tdb->tdb_spi)));
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
		    
		xd->edx_xform->decrypt(xd, blk);

		for (i=0; i<blks; i++)
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

	    xd->edx_xform->decrypt(xd, idat);

	    for (i=0; i<blks; i++)
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
     * blk[7] contains the next protocol, and blk[6] contains the
     * amount of padding the original chain had. Chop off the
     * appropriate parts of the chain, and return.
     * Verify correct decryption by checking the last padding bytes.
     */

    if ((xd->edx_flags & ESP_NEW_FLAG_NPADDING) == 0)
    {
        if (blk[6] + 2 + alen > m->m_pkthdr.len - (ip->ip_hl << 2) - 2 * sizeof(u_int32_t) - xd->edx_ivlen)
        {
	    DPRINTF(("esp_new_input(): invalid padding length %d for packet from %x to %x, SA %x/%08x\n", blk[6], ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi)));
	    espstat.esps_badilen++;
	    m_freem(m);
	    return NULL;
	}
        if ((blk[6] != blk[5]) && (blk[6] != 0))
	{
	    if (encdebug)
	      log(LOG_ALERT, "esp_new_input(): decryption failed for packet from %x to %x, SA %x/%08x\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
	    m_freem(m);
	    return NULL;
	} 

      	m_adj(m, - blk[6] - 2 - alen);		/* Old type padding */
    }
    else
    {
        if (blk[6] + 1 + alen > m->m_pkthdr.len - (ip->ip_hl << 2) - 2 * sizeof(u_int32_t) - xd->edx_ivlen)
        {
	    DPRINTF(("esp_new_input(): invalid padding length %d for packet from %x to %x, SA %x/%08x\n", blk[6], ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi)));
	    espstat.esps_badilen++;
	    m_freem(m);
	    return NULL;
	}
	if (blk[6] == 0)
	{
	    if (encdebug)
	      log(LOG_ALERT, "esp_new_input(): decryption failed for packet from %x to %x, SA %x/%08x -- peer is probably using old style padding\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
	    m_freem(m);
	    return NULL;
	}
	else
	  if (blk[6] != blk[5] + 1)
          {
	      if (encdebug)
                log(LOG_ALERT, "esp_new_input(): decryption failed for packet from %x to %x, SA %x/%08x\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
              m_freem(m);
              return NULL;
          }

      	m_adj(m, - blk[6] - 1 - alen);
    }

    m_adj(m, 2 * sizeof(u_int32_t) + xd->edx_ivlen);

    if (m->m_len < (ipo.ip_hl << 2))
    {
	m = m_pullup(m, (ipo.ip_hl << 2));
	if (m == NULL)
	{
	    DPRINTF(("esp_new_input(): m_pullup() failed for packet from %x to %x, SA %x/%08x\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi)));
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ipo.ip_p = blk[7];
    ipo.ip_id = htons(ipo.ip_id);
    ipo.ip_off = 0;
    ipo.ip_len += (ipo.ip_hl << 2) -  2 * sizeof(u_int32_t) - xd->edx_ivlen -
		  blk[6] - 1 - alen;

    if ((xd->edx_flags & ESP_NEW_FLAG_NPADDING) == 0)
      ipo.ip_len -= 1;

    ipo.ip_len = htons(ipo.ip_len);
    ipo.ip_sum = 0;
    *ip = ipo;

    /* Copy the options back */
    m_copyback(m, sizeof(struct ip), (ipo.ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    ip->ip_sum = in_cksum(m, (ip->ip_hl << 2));

    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) + 
	                  blk[6] + 1 + alen;
    espstat.esps_ibytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) + 
                           blk[6] + 1 + alen;

    if ((xd->edx_flags & ESP_NEW_FLAG_NPADDING) == 0)
    {
	tdb->tdb_cur_bytes++;
	espstat.esps_ibytes++;
    }

    /* Notify on expiration */
    if (tdb->tdb_flags & TDBF_SOFT_PACKETS)
    {
      if (tdb->tdb_cur_packets >= tdb->tdb_soft_packets)
      {
	  encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
	  tdb->tdb_flags &= ~TDBF_SOFT_PACKETS;
      }
      else
	if (tdb->tdb_flags & TDBF_SOFT_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)
	  {
	      encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
	      tdb->tdb_flags &= ~TDBF_SOFT_BYTES;
	  }
    }

    if (tdb->tdb_flags & TDBF_PACKETS)
    {
      if (tdb->tdb_cur_packets >= tdb->tdb_exp_packets)
      {
	  encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
	  tdb_delete(tdb, 0);
      }
      else
	if (tdb->tdb_flags & TDBF_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes)
	  {
	      encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
	      tdb_delete(tdb, 0);
	  }
    }

    return m;
}

int
esp_new_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
	       struct mbuf **mp)
{
    struct esp_new_xdata *xd;
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
    
    xd = (struct esp_new_xdata *) tdb->tdb_xdata;

    blks = xd->edx_xform->blocksize;

    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
    {
      alen = AH_HMAC_HASHLEN;
      DPRINTF(("esp_new_output(): using hash algorithm: %s\n", xd->edx_hash->name));
    } 
    else
      alen = 0;

    espstat.esps_output++;

    m = m_pullup(m, sizeof (struct ip));   /* Get IP header in one mbuf */
    if (m == NULL)
    {
        DPRINTF(("esp_new_output(): m_pullup() failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi)));
	return ENOBUFS;
    }

    if (xd->edx_rpl == 0)
    {
	if (encdebug)
          log(LOG_ALERT, "esp_new_output(): SA %x/%0x8 should have expired\n",
	      tdb->tdb_dst, ntohl(tdb->tdb_spi));
	m_freem(m);
	espstat.esps_wrap++;
	return NULL;
    }

    espo.esp_spi = tdb->tdb_spi;
    espo.esp_rpl = htonl(xd->edx_rpl++);

    ip = mtod(m, struct ip *);
    iphlen = (ip->ip_hl << 2);
    
    /*
     * If options are present, pullup the IP header, the options.
     */
    if (iphlen != sizeof(struct ip))
    {
	m = m_pullup(m, iphlen + 8);
	if (m == NULL)
	{
	    DPRINTF(("esp_new_input(): m_pullup() failed for SA %x/%08x\n",
		     tdb->tdb_dst, ntohl(tdb->tdb_spi)));
	    return ENOBUFS;
	}

	ip = mtod(m, struct ip *);

	/* Keep the options */
	m_copydata(m, sizeof(struct ip), iphlen - sizeof(struct ip), 
		   (caddr_t) opts);
    }

    ilen = ntohs(ip->ip_len);    /* Size of the packet */
    ohlen = 2 * sizeof(u_int32_t) + xd->edx_ivlen;

    ipo = *ip;
    nh = ipo.ip_p;

    /* Raw payload length */
    rlen = ilen - iphlen; 
    padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;

    pad = (u_char *) m_pad(m, padding + alen);
    if (pad == NULL)
    {
        DPRINTF(("esp_new_output(): m_pad() failed for SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi)));
      	return ENOBUFS;
    }

    /* Self describing padding */
    for (i = 0; i < padding - 2; i++)
      pad[i] = i + 1;

    if (xd->edx_flags & ESP_NEW_FLAG_NPADDING)
      pad[padding - 2] = padding - 1;
    else
      pad[padding - 2] = padding - 2;

    pad[padding - 1] = nh;

    mi = m;
    plen = rlen + padding;
    ilen = m->m_len - iphlen;
    idat = mtod(m, u_char *) + iphlen;

    if (xd->edx_ivlen == 0)
    {
	bcopy((u_char *) &espo.esp_rpl, iv, 4);
	iv[4] = ~iv[0];
	iv[5] = ~iv[1];
	iv[6] = ~iv[2];
	iv[7] = ~iv[3];
    } 
    else
    {
	bcopy(xd->edx_iv, iv, xd->edx_ivlen);
	bcopy(xd->edx_iv, espo.esp_iv, xd->edx_ivlen);
    }

    /* Authenticate the esp header */
    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
    {
	bcopy(&(xd->edx_ictx), &ctx, xd->edx_hash->ctxsize);
	xd->edx_hash->Update(&ctx, (unsigned char *) &espo, 
		  2 * sizeof(u_int32_t) + xd->edx_ivlen);
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
		    panic("esp_new_output(): bad chain (i)\n");
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL)
		{
		    DPRINTF(("esp_new_output(): m_pullup() failed, SA %x/%08x\n",
			       tdb->tdb_dst, ntohl(tdb->tdb_spi)));
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
		    
		for (i=0; i<blks; i++)
		    blk[i] ^= ivp[i];

		xd->edx_xform->encrypt(xd, blk);

		if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
		    xd->edx_hash->Update(&ctx, blk, blks);

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
	    for (i=0; i<blks; i++)
		idat[i] ^= ivp[i];

	    xd->edx_xform->encrypt(xd, idat);

	    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
		xd->edx_hash->Update(&ctx, idat, blks);

	    ivp = idat;
	    idat += blks;

	    ilen -= blks;
	    plen -= blks;
	}
    }

    /* Put in authentication data */
    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
    {
	xd->edx_hash->Final(auth, &ctx);
	bcopy(&(xd->edx_octx), &ctx, xd->edx_hash->ctxsize);
	xd->edx_hash->Update(&ctx, auth, xd->edx_hash->hashsize);
	xd->edx_hash->Final(auth, &ctx);

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
        DPRINTF(("esp_new_output(): M_PREPEND failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }

    m = m_pullup(m, iphlen + ohlen);
    if (m == NULL)
    {
        DPRINTF(("esp_new_output(): m_pullup() failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }

    /* Fix the length and the next protocol, copy back and off we go */
    ipo.ip_len = htons(iphlen + ohlen + rlen + padding + alen);
    ipo.ip_p = IPPROTO_ESP;

    /* Save the last encrypted block, to be used as the next IV */
    bcopy(ivp, xd->edx_iv, xd->edx_ivlen);

    m_copyback(m, 0, sizeof(struct ip), (caddr_t) &ipo);

    /* Copy options, if existing */
    if (iphlen != sizeof(struct ip))
      m_copyback(m, sizeof(struct ip), iphlen - sizeof(struct ip),
		 (caddr_t) opts);

    /* Copy in the esp header */
    m_copyback(m, iphlen, ohlen, (caddr_t) &espo);
	
    *mp = m;

    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += rlen + padding;
    espstat.esps_obytes += rlen + padding;

    /* Notify on expiration */
    if (tdb->tdb_flags & TDBF_SOFT_PACKETS)
    {
      if (tdb->tdb_cur_packets >= tdb->tdb_soft_packets)
      {
	  encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
	  tdb->tdb_flags &= ~TDBF_SOFT_PACKETS;
      }
      else
	if (tdb->tdb_flags & TDBF_SOFT_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)
	  {
	      encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
	      tdb->tdb_flags &= ~TDBF_SOFT_BYTES;
	  }
    }

    if (tdb->tdb_flags & TDBF_PACKETS)
    {
      if (tdb->tdb_cur_packets >= tdb->tdb_exp_packets)
      {
	  encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
	  tdb_delete(tdb, 0);
      }
      else
	if (tdb->tdb_flags & TDBF_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes)
	  {
	      encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
	      tdb_delete(tdb, 0);
	  }
    }

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
