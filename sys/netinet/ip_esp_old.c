/*	$OpenBSD: ip_esp_old.c,v 1.31 1999/03/24 17:00:46 niklas Exp $	*/

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

/*
 * DES-CBC
 * Per RFCs 1829/1851 (Metzger & Simpson)
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
#include <dev/rndvar.h>

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
static void des1_decrypt(struct tdb *, u_int8_t *);
static void des3_decrypt(struct tdb *, u_int8_t *);

struct enc_xform esp_old_xform[] = {
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

int
esp_old_attach()
{
    return 0;
}

/*
 * esp_old_init() is called when an SPI is being set up.
 */

int
esp_old_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
    struct enc_xform *txform = NULL;
    int i;

    /* Check whether the encryption algorithm is supported */
    for (i = sizeof(esp_old_xform) / sizeof(struct enc_xform) - 1;
	 i >= 0; i--) 
      if (ii->ii_encalg == esp_old_xform[i].type)
	break;

    if (i < 0) 
    {
	DPRINTF(("esp_old_init(): unsupported encryption algorithm %d specified\n", ii->ii_encalg));
        return EINVAL;
    }

    txform = &esp_old_xform[i];

    if (ii->ii_enckeylen < txform->minkey)
    {
	DPRINTF(("esp_old_init(): keylength %d too small (min length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->minkey, txform->name));
	return EINVAL;
    }
    
    if (ii->ii_enckeylen > txform->maxkey)
    {
	DPRINTF(("esp_old_init(): keylength %d too large (max length is %d) for algorithm %s\n", ii->ii_enckeylen, txform->maxkey, txform->name));
	return EINVAL;
    }

    tdbp->tdb_xform = xsp;
    tdbp->tdb_encalgxform = txform;

    DPRINTF(("esp_old_init(): initialized TDB with enc algorithm %s\n",
	     txform->name));

    tdbp->tdb_ivlen = txform->ivmask;
    if (tdbp->tdb_flags & TDBF_HALFIV)
      tdbp->tdb_ivlen /= 2;

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
    }

    return 0;
}

/* Free the memory */
int
esp_old_zeroize(struct tdb *tdbp)
{
    if (tdbp->tdb_key)
    {
	if (tdbp->tdb_encalgxform)
	  switch (tdbp->tdb_encalgxform->type)
	  {
	      case SADB_EALG_DESCBC:
		  bzero(tdbp->tdb_key, 128);
		  break;

	      case SADB_EALG_3DESCBC:
		  bzero(tdbp->tdb_key, 384);
		  break;
	  }

	FREE(tdbp->tdb_key, M_XDATA);
	tdbp->tdb_key = NULL;
    }
    
    return 0;
}

/*
 * esp_old_input() gets called to decrypt an input packet
 */
struct mbuf *
esp_old_input(struct mbuf *m, struct tdb *tdb)
{
    struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
    struct ip *ip, ipo;
    u_char iv[ESP_3DES_IVS], niv[ESP_3DES_IVS], blk[ESP_3DES_BLKS], opts[40];
    u_char *idat, *odat, *ivp, *ivn, *lblk;
    struct esp_old *esp;
    int ohlen, plen, ilen, i, blks, rest;
    struct mbuf *mi, *mo;

    blks = espx->blocksize;

    if (m->m_len < sizeof(struct ip))
    {
	if ((m = m_pullup(m, sizeof(struct ip))) == NULL)
	{
	    DPRINTF(("esp_old_input(): m_pullup() failed\n"));
	    espstat.esps_hdrops++;
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ohlen = (ip->ip_hl << 2) + ESP_OLD_FLENGTH;

    /* Make sure the IP header, any IP options, and the ESP header are here */
    if (m->m_len < ohlen + blks)
    {
	if ((m = m_pullup(m, ohlen + blks)) == NULL)
	{
            DPRINTF(("esp_old_input(): m_pullup() failed\n"));
	    espstat.esps_hdrops++;
            return NULL;
	}

	ip = mtod(m, struct ip *);
    }

    esp = (struct esp_old *) ((u_int8_t *) ip + (ip->ip_hl << 2));
    ipo = *ip;

    /* Skip the IP header, IP options, SPI and IV */
    plen = m->m_pkthdr.len - (ip->ip_hl << 2) - sizeof(u_int32_t) -
	   tdb->tdb_ivlen;
    if ((plen & (blks - 1)) || (plen <= 0))
    {
	DPRINTF(("esp_old_input(): payload not a multiple of %d octets for packet from %s to %s, spi %08x\n", blks, inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += plen;
    espstat.esps_ibytes += plen;

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
      {
	  pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	  tdb_delete(tdb, 0);
	  m_freem(m);
	  return NULL;
      }
    
    /* Notify on expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
      {
	  pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	  tdb->tdb_flags &= ~TDBF_SOFT_BYTES;     /* Turn off checking */
      }

    ilen = m->m_len - (ip->ip_hl << 2) - sizeof(u_int32_t) - 4;
    idat = mtod(m, unsigned char *) + (ip->ip_hl << 2) + sizeof(u_int32_t) + 4;

    /* Get the IV */
    iv[0] = esp->esp_iv[0];
    iv[1] = esp->esp_iv[1];
    iv[2] = esp->esp_iv[2];
    iv[3] = esp->esp_iv[3];
    if (tdb->tdb_ivlen == 4)		/* Half-IV */
    {
	iv[4] = ~esp->esp_iv[0];
	iv[5] = ~esp->esp_iv[1];
	iv[6] = ~esp->esp_iv[2];
	iv[7] = ~esp->esp_iv[3];
    }
    else
    {
	iv[4] = esp->esp_iv[4];
	iv[5] = esp->esp_iv[5];
	iv[6] = esp->esp_iv[6];
	iv[7] = esp->esp_iv[7];

	/* Adjust the lengths accordingly */
	ilen -= 4;
	idat += 4;
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
		{
		    DPRINTF(("esp_old_input(): bad mbuf chain, SA %s/%08x\n",
			     ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    m_freem(m);
		    espstat.esps_hdrops++;
		    return NULL;
		}
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL)
		{
		    DPRINTF(("esp_old_input(): m_pullup() failed, SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
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

		for (i=0; i < blks; i++)
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
     * We cannot verify the decryption here (as in ip_esp_new.c), since
     * the padding may be random.
     */
    
    if (blk[blks - 2] + 2 > m->m_pkthdr.len - (ip->ip_hl << 2) -
	sizeof(u_int32_t) - tdb->tdb_ivlen)
    {
	DPRINTF(("esp_old_input(): invalid padding length %d for packet from %s to %s, spi %08x\n", blk[blks - 2], inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    m_adj(m, - blk[blks - 2] - 2);
    m_adj(m, sizeof(u_int32_t) + tdb->tdb_ivlen);

    if (m->m_len < (ipo.ip_hl << 2))
    {
	m = m_pullup(m, (ipo.ip_hl << 2));
	if (m == NULL)
	{
	    DPRINTF(("esp_old_input(): m_pullup() failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_hdrops++;
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ipo.ip_p = blk[blks - 1];
    ipo.ip_id = htons(ipo.ip_id);
    ipo.ip_off = 0;
    ipo.ip_len += (ipo.ip_hl << 2) - sizeof(u_int32_t) - tdb->tdb_ivlen -
		  blk[blks - 2] - 2;
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
esp_old_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
	      struct mbuf **mp)
{
    struct enc_xform *espx = (struct enc_xform *) tdb->tdb_encalgxform;
    struct ip *ip, ipo;
    int i, ilen, ohlen, nh, rlen, plen, padding, rest;
    u_int32_t spi;
    struct mbuf *mi, *mo;
    u_char *pad, *idat, *odat, *ivp;
    u_char iv[ESP_3DES_IVS], blk[ESP_3DES_IVS], opts[40];
    int iphlen, blks;

    blks = espx->blocksize;

    espstat.esps_output++;

    m = m_pullup(m, sizeof(struct ip));
    if (m == NULL)
    {
        DPRINTF(("esp_old_output(): m_pullup() failed for SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_hdrops++;
        return ENOBUFS;
    }

    ip = mtod(m, struct ip *);
    spi = tdb->tdb_spi;
    iphlen = (ip->ip_hl << 2);

    /*
     * If options are present, pullup the IP header and the options.
     */
    if (iphlen != sizeof(struct ip))
    {
	m = m_pullup(m, iphlen);
	if (m == NULL)
        {
	    DPRINTF(("esp_old_output(): m_pullup() failed for SA %s/%08x\n",
		     ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    espstat.esps_hdrops++;
	    return ENOBUFS;
	}

	ip = mtod(m, struct ip *);

	/* Keep the options */
	m_copydata(m, sizeof(struct ip), iphlen - sizeof(struct ip),
		   (caddr_t) opts);
    }
    
    ilen = ntohs(ip->ip_len);
    ohlen = sizeof(u_int32_t) + tdb->tdb_ivlen;

    ipo = *ip;
    nh = ipo.ip_p;

    /* Raw payload length  */
    rlen = ilen - iphlen;
    padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;
    if (iphlen + ohlen + rlen + padding > IP_MAXPACKET)
    {
	DPRINTF(("esp_old_output(): packet in SA %s/%0x8 got too big\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	espstat.esps_toobig++;
        return EMSGSIZE;
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += rlen;
    espstat.esps_obytes += rlen;

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	tdb_delete(tdb, 0);
	m_freem(m);
	return EINVAL;
    }

    /* Notify on expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;     /* Turn off checking */
    }

    pad = (u_char *) m_pad(m, padding, 1);
    if (pad == NULL)
    {
	DPRINTF(("esp_old_output(): m_pad() failed for SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
      	return ENOBUFS;
    }

    pad[padding - 2] = padding - 2;
    pad[padding - 1] = nh;

    plen = rlen + padding;
    mi = m;
    ilen = m->m_len - iphlen;
    idat = mtod(m, u_char *) + iphlen;

    /*
     * We are now ready to encrypt the payload. 
     */

    iv[0] = tdb->tdb_iv[0];
    iv[1] = tdb->tdb_iv[1];
    iv[2] = tdb->tdb_iv[2];
    iv[3] = tdb->tdb_iv[3];

    if (tdb->tdb_ivlen == 4)	/* Half-IV */
    {
	iv[4] = ~tdb->tdb_iv[0];
	iv[5] = ~tdb->tdb_iv[1];
	iv[6] = ~tdb->tdb_iv[2];
	iv[7] = ~tdb->tdb_iv[3];
    }
    else
    {
	iv[4] = tdb->tdb_iv[4];
	iv[5] = tdb->tdb_iv[5];
	iv[6] = tdb->tdb_iv[6];
	iv[7] = tdb->tdb_iv[7];
    }

    ivp = iv;
    rest = ilen % blks;
    while (plen > 0)		/* while not done */
    {
	if (ilen < blks)	/* we exhausted previous mbuf */
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
		    DPRINTF(("esp_old_output(): bad mbuf chain, SA %s/%08x\n",
			     ipsp_address(tdb->tdb_dst),
			     ntohl(tdb->tdb_spi)));
		    m_freem(m);
		    return EINVAL;
		}
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL)
		{
		    DPRINTF(("esp_old_output(): m_pullup() failed, SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
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
	    idat = (u_char *) mi->m_data;
	    if (rest)
	    {
		bcopy(idat, blk + rest, blks - rest);
		    
		for (i=0; i<blks; i++)
		    blk[i] ^= ivp[i];

		espx->encrypt(tdb, blk);

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

	    ivp = idat;
	    idat += blks;

	    ilen -= blks;
	    plen -= blks;
	}
    }

    /*
     * Done with encryption. Let's wedge in the ESP header
     * and send it out.
     */

    M_PREPEND(m, ohlen, M_DONTWAIT);
    if (m == NULL)
    {
	DPRINTF(("esp_old_output(): M_PREPEND failed, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }

    m = m_pullup(m, iphlen + ohlen);
    if (m == NULL)
    {
        DPRINTF(("esp_old_output(): m_pullup() failed, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	espstat.esps_hdrops++;
        return ENOBUFS;
    }

    ipo.ip_len = htons(iphlen + ohlen + rlen + padding);
    ipo.ip_p = IPPROTO_ESP;

    iv[0] = tdb->tdb_iv[0];
    iv[1] = tdb->tdb_iv[1];
    iv[2] = tdb->tdb_iv[2];
    iv[3] = tdb->tdb_iv[3];

    if (tdb->tdb_ivlen == 8)
    {
	iv[4] = tdb->tdb_iv[4];
	iv[5] = tdb->tdb_iv[5];
	iv[6] = tdb->tdb_iv[6];
	iv[7] = tdb->tdb_iv[7];
    }

    /* Save the last encrypted block, to be used as the next IV */
    bcopy(ivp, tdb->tdb_iv, tdb->tdb_ivlen);

    m_copyback(m, 0, sizeof(struct ip), (caddr_t) &ipo);

    /* Copy options, if existing */
    if (iphlen != sizeof(struct ip))
      m_copyback(m, sizeof(struct ip), iphlen - sizeof(struct ip),
		 (caddr_t) opts);

    m_copyback(m, iphlen, sizeof(u_int32_t), (caddr_t) &spi);
    m_copyback(m, iphlen + sizeof(u_int32_t), tdb->tdb_ivlen, (caddr_t) iv);
	
    *mp = m;

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
