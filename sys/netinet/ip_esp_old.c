/*	$OpenBSD: ip_esp_old.c,v 1.17 1998/05/18 21:10:45 provos Exp $	*/

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
#include <net/encap.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <dev/rndvar.h>
#include <sys/syslog.h>

extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);
extern void des_set_key(caddr_t, caddr_t);

extern int encap_sendnotify(int, struct tdb *, void *);

static void des1_encrypt(void *, u_int8_t *);
static void des3_encrypt(void *, u_int8_t *);
static void des1_decrypt(void *, u_int8_t *);
static void des3_decrypt(void *, u_int8_t *);

struct esp_xform esp_old_xform[] = {
     { ALG_ENC_DES, "Data Encryption Standard (DES)",
       ESP_DES_BLKS, ESP_DES_IVS,
       8, 8, 8 | 4,
       des1_encrypt,
       des1_decrypt 
     },
     { ALG_ENC_3DES, "Tripple DES (3DES)",
       ESP_3DES_BLKS, ESP_3DES_IVS,
       24, 24, 8 | 4,
       des3_encrypt,
       des3_decrypt 
     }
};

static void
des1_encrypt(void *pxd, u_int8_t *blk)
{
     struct esp_old_xdata *xd = pxd;
     des_ecb_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]), 1);
}

static void
des1_decrypt(void *pxd, u_int8_t *blk)
{
     struct esp_old_xdata *xd = pxd;
     des_ecb_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]), 0);
}

static void
des3_encrypt(void *pxd, u_int8_t *blk)
{
     struct esp_old_xdata *xd = pxd;
     des_ecb3_encrypt(blk, blk, (caddr_t) (xd->edx_eks[2]),
		      (caddr_t) (xd->edx_eks[1]),
		      (caddr_t) (xd->edx_eks[0]), 1);
}

static void
des3_decrypt(void *pxd, u_int8_t *blk)
{
     struct esp_old_xdata *xd = pxd;
     des_ecb3_encrypt(blk, blk, (caddr_t) (xd->edx_eks[2]),
		      (caddr_t) (xd->edx_eks[1]),
		      (caddr_t) (xd->edx_eks[0]), 0);
}

int
esp_old_attach()
{
#ifdef ENCDEBUG
    if (encdebug)
      printf("esp_old_attach(): setting up\n");
#endif /* ENCDEBUG */
    return 0;
}

/*
 * esp_old_init() is called when an SPI is being set up. It interprets the
 * encap_msghdr present in m, and sets up the transformation data, in
 * this case, the encryption and decryption key schedules
 */

int
esp_old_init(struct tdb *tdbp, struct xformsw *xsp, struct mbuf *m)
{
    struct esp_old_xdata *xd;
    struct esp_old_xencap xenc;
    struct encap_msghdr *em;
    struct esp_xform *txform;
    u_int32_t rk[6];
    int i;

    if (m->m_len < ENCAP_MSG_FIXED_LEN)
    {
	if ((m = m_pullup(m, ENCAP_MSG_FIXED_LEN)) == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_old_init(): m_pullup failed\n");
#endif /* ENCDEBUG */
	    return ENOBUFS;
	}
    }

    em = mtod(m, struct encap_msghdr *);
    if (em->em_msglen - EMT_SETSPI_FLEN <= ESP_OLD_XENCAP_LEN)
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_old_init(): initialization failed\n");
	return EINVAL;
    }

    /* Just copy the standard fields */
    m_copydata(m, EMT_SETSPI_FLEN, ESP_OLD_XENCAP_LEN, (caddr_t) &xenc);

    /* Check whether the encryption algorithm is supported */
    for (i=sizeof(esp_old_xform)/sizeof(struct esp_xform)-1; i >= 0; i--) 
	if (xenc.edx_enc_algorithm == esp_old_xform[i].type)
	      break;
    if (i < 0) 
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_old_init(): unsupported encryption algorithm %d specified\n", xenc.edx_enc_algorithm);
        return EINVAL;
    }

    txform = &esp_old_xform[i];
#ifdef ENCDEBUG
    if (encdebug)
      printf("esp_old_init(): initialized TDB with enc algorithm %d: %s\n",
	     xenc.edx_enc_algorithm, esp_old_xform[i].name);
#endif /* ENCDEBUG */

    if (xenc.edx_ivlen + xenc.edx_keylen + EMT_SETSPI_FLEN +
	ESP_OLD_XENCAP_LEN != em->em_msglen)
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_old_init(): message length (%d) doesn't match\n", em->em_msglen);
	return EINVAL;
    }

    /* Check the IV length */
    if (((xenc.edx_ivlen == 0) && !(txform->ivmask&1)) ||
	((xenc.edx_ivlen != 0) && (
	     !(xenc.edx_ivlen & txform->ivmask) ||
	     (xenc.edx_ivlen & (xenc.edx_ivlen-1)))))
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_old_init(): unsupported IV length %d\n",
	      xenc.edx_ivlen);
	return EINVAL;
    }

    /* Check the key length */
    if (xenc.edx_keylen < txform->minkey || xenc.edx_keylen > txform->maxkey)
    {
	if (encdebug)
	  log(LOG_WARNING, "esp_old_init(): bad key length %d\n",
	      xenc.edx_keylen);
	return EINVAL;
    }

    MALLOC(tdbp->tdb_xdata, caddr_t, sizeof(struct esp_old_xdata),
	   M_XDATA, M_WAITOK);
    if (tdbp->tdb_xdata == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_old_init(): MALLOC() failed\n");
#endif /* ENCDEBUG */
      	return ENOBUFS;
    }

    bzero(tdbp->tdb_xdata, sizeof(struct esp_old_xdata));
    xd = (struct esp_old_xdata *) tdbp->tdb_xdata;

    /* Pointer to the transform */
    tdbp->tdb_xform = xsp;

    xd->edx_ivlen = xenc.edx_ivlen;
    xd->edx_xform = txform;
    xd->edx_enc_algorithm = xenc.edx_enc_algorithm;

    /* Pass name of enc algorithm for kernfs */
    tdbp->tdb_confname = xd->edx_xform->name;

    /* Copy the IV */
    m_copydata(m, EMT_SETSPI_FLEN + ESP_OLD_XENCAP_LEN, xd->edx_ivlen,
	       (caddr_t) xd->edx_iv);

    /* Copy the key material */
    m_copydata(m, EMT_SETSPI_FLEN + ESP_OLD_XENCAP_LEN + xd->edx_ivlen,
	       xenc.edx_keylen, (caddr_t) rk);

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
    }

    bzero(rk, 6 * sizeof(u_int32_t));		/* paranoid */

    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */

    return 0;
}

/* Free the memory */
int
esp_old_zeroize(struct tdb *tdbp)
{
#ifdef ENCDEBUG
    if (encdebug)
      printf("esp_old_zeroize(): freeing memory\n");
#endif /* ENCDEBUG */
    if (tdbp->tdb_xdata)
    {
       	FREE(tdbp->tdb_xdata, M_XDATA);
	tdbp->tdb_xdata = NULL;
    }
    return 0;
}

/*
 * esp_old_input() gets called to decrypt an input packet
 */
struct mbuf *
esp_old_input(struct mbuf *m, struct tdb *tdb)
{
    struct esp_old_xdata *xd;
    struct ip *ip, ipo;
    u_char iv[ESP_3DES_IVS], niv[ESP_3DES_IVS], blk[ESP_3DES_BLKS], opts[40];
    u_char *idat, *odat, *ivp, *ivn, *lblk;
    struct esp_old *esp;
    int ohlen, plen, ilen, i, blks, rest;
    struct mbuf *mi;

    xd = (struct esp_old_xdata *) tdb->tdb_xdata;

    blks = xd->edx_xform->blocksize;

    if (m->m_len < sizeof(struct ip))
    {
	if ((m = m_pullup(m, sizeof(struct ip))) == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_old_input(): m_pullup() failed\n");
#endif /* ENCDEBUG */
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
#ifdef ENCDEBUG
            if (encdebug)
              printf("esp_old_input(): m_pullup() failed\n");
#endif /* ENCDEBUG */
            espstat.esps_hdrops++;
            return NULL;
	}

	ip = mtod(m, struct ip *);
    }

    esp = (struct esp_old *) ((u_int8_t *) ip + (ip->ip_hl << 2));

    ipo = *ip;

    /* Skip the IP header, IP options, SPI and IV */
    plen = m->m_pkthdr.len - (ip->ip_hl << 2) - sizeof(u_int32_t) -
	   xd->edx_ivlen;
    if ((plen & (blks - 1)) || (plen <= 0))
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_old_input(): payload not a multiple of %d octets for packet from %x to %x, spi %08x\n", blks, ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    ilen = m->m_len - (ip->ip_hl << 2) - sizeof(u_int32_t) - 4;
    idat = mtod(m, unsigned char *) + (ip->ip_hl << 2) + sizeof(u_int32_t) + 4;

    /* Get the IV */
    iv[0] = esp->esp_iv[0];
    iv[1] = esp->esp_iv[1];
    iv[2] = esp->esp_iv[2];
    iv[3] = esp->esp_iv[3];
    if (xd->edx_ivlen == 4)		/* Half-IV */
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
		mi = mi->m_next;
		if (mi == NULL)
		    panic("esp_old_output(): bad chain (i)\n");
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL)
		{
#ifdef ENCDEBUG
		    if (encdebug)
			printf("esp_old_input(): m_pullup() failed, SA %x/%08x\n",
			       tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
		    espstat.esps_hdrops++;
		    return NULL;
		}
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
     * We cannot verify the decryption here (as in ip_esp_new.c), since
     * the padding may be random.
     */
    
    if (blk[6] + 2 > m->m_pkthdr.len - (ip->ip_hl << 2) - sizeof(u_int32_t) -
	xd->edx_ivlen)
    {
#ifdef ENCDEBUG
        if (encdebug)
	  printf("esp_old_input(): invalid padding length %d for packet from %x to %x, SA %x/%08x\n", blk[6], ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    m_adj(m, -blk[6] - 2);
    m_adj(m, 4 + xd->edx_ivlen);

    if (m->m_len < (ipo.ip_hl << 2))
    {
	m = m_pullup(m, (ipo.ip_hl << 2));
	if (m == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_old_input(): m_pullup() failed for packet from %x to %x, SA %x/%08x\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ipo.ip_p = blk[7];
    ipo.ip_id = htons(ipo.ip_id);
    ipo.ip_off = 0;
    ipo.ip_len += (ipo.ip_hl << 2) - sizeof(u_int32_t) - xd->edx_ivlen -
		  blk[6] - 2;
    ipo.ip_len = htons(ipo.ip_len);
    ipo.ip_sum = 0;
    *ip = ipo;

    /* Copy the options back */
    m_copyback(m, sizeof(struct ip), (ipo.ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    ip->ip_sum = in_cksum(m, (ip->ip_hl << 2));

    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) + blk[6] + 2;
    espstat.esps_ibytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) + blk[6] + 2;

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
esp_old_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
	      struct mbuf **mp)
{
    struct esp_old_xdata *xd;
    struct ip *ip, ipo;
    int i, ilen, ohlen, nh, rlen, plen, padding, rest;
    u_int32_t spi;
    struct mbuf *mi;
    u_char *pad, *idat, *odat, *ivp;
    u_char iv[ESP_3DES_IVS], blk[ESP_3DES_IVS], opts[40];
    int iphlen, blks;

    xd = (struct esp_old_xdata *) tdb->tdb_xdata;

    blks = xd->edx_xform->blocksize;

    espstat.esps_output++;

    m = m_pullup(m, sizeof(struct ip));
    if (m == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_old_output(): m_pullup() failed for SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
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
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_old_output(): m_pullup() failed for SA %x/%08x\n",
		     tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	    return ENOBUFS;
	}

	ip = mtod(m, struct ip *);

	/* Keep the options */
	m_copydata(m, sizeof(struct ip), iphlen - sizeof(struct ip),
		   (caddr_t) opts);
    }
    
    ilen = ntohs(ip->ip_len);
    ohlen = sizeof(u_int32_t) + xd->edx_ivlen;

    ipo = *ip;
    nh = ipo.ip_p;

    rlen = ilen - iphlen; /* raw payload length  */
    padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;

    pad = (u_char *) m_pad(m, padding);
    if (pad == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_old_output(): m_pad() failed for SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
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

    iv[0] = xd->edx_iv[0];
    iv[1] = xd->edx_iv[1];
    iv[2] = xd->edx_iv[2];
    iv[3] = xd->edx_iv[3];

    if (xd->edx_ivlen == 4)	/* Half-IV */
    {
	iv[4] = ~xd->edx_iv[0];
	iv[5] = ~xd->edx_iv[1];
	iv[6] = ~xd->edx_iv[2];
	iv[7] = ~xd->edx_iv[3];
    }
    else
    {
	iv[4] = xd->edx_iv[4];
	iv[5] = xd->edx_iv[5];
	iv[6] = xd->edx_iv[6];
	iv[7] = xd->edx_iv[7];
    }

    ivp = iv;
    rest = ilen % blks;
    while (plen > 0)		/* while not done */
    {
	if (ilen < blks)	/* we exhausted previous mbuf */
	{
	    if (rest)
	    {
		bcopy(idat, blk, rest);
		odat = idat;
	    }

	    do {
		mi = mi->m_next;
		if (mi == NULL)
		    panic("esp_old_output(): bad chain (i)\n");
	    } while (mi->m_len == 0);

	    if (mi->m_len < blks - rest)
	    {
		if ((mi = m_pullup(mi, blks - rest)) == NULL)
		{
#ifdef ENCDEBUG
		    if (encdebug)
			printf("esp_old_output(): m_pullup() failed, SA %x/%08x\n",
			       tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
		    return ENOBUFS;
		}
	    }
		    
	    ilen = mi->m_len;
	    idat = (u_char *) mi->m_data;
	    if (rest)
	    {
		bcopy(idat, blk + rest, blks - rest);
		    
		for (i=0; i<blks; i++)
		    blk[i] ^= ivp[i];

		xd->edx_xform->encrypt(xd, blk);

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
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_old_output(): M_PREPEND failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
        return ENOBUFS;
    }

    m = m_pullup(m, iphlen + ohlen);
    if (m == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_old_output(): m_pullup() failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
        return ENOBUFS;
    }

    ipo.ip_len = htons(iphlen + ohlen + rlen + padding);
    ipo.ip_p = IPPROTO_ESP;

    iv[0] = xd->edx_iv[0];
    iv[1] = xd->edx_iv[1];
    iv[2] = xd->edx_iv[2];
    iv[3] = xd->edx_iv[3];

    if (xd->edx_ivlen == 8)
    {
	iv[4] = xd->edx_iv[4];
	iv[5] = xd->edx_iv[5];
	iv[6] = xd->edx_iv[6];
	iv[7] = xd->edx_iv[7];
    }

    /* Save the last encrypted block, to be used as the next IV */
    bcopy(ivp, xd->edx_iv, xd->edx_ivlen);

    m_copyback(m, 0, sizeof(struct ip), (caddr_t) &ipo);

    /* Copy options, if existing */
    if (iphlen != sizeof(struct ip))
      m_copyback(m, sizeof(struct ip), iphlen - sizeof(struct ip),
		 (caddr_t) opts);

    m_copyback(m, iphlen, sizeof(u_int32_t), (caddr_t) &spi);
    m_copyback(m, iphlen + sizeof(u_int32_t), xd->edx_ivlen, (caddr_t) iv);
	
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
 *
 *
 * m_pad(m, n) pads <m> with <n> bytes at the end. The packet header
 * length is updated, and a pointer to the first byte of the padding
 * (which is guaranteed to be all in one mbuf) is returned.
 *
 */

caddr_t
m_pad(struct mbuf *m, int n)
{
    register struct mbuf *m0, *m1;
    register int len, pad;
    caddr_t retval;
    u_int8_t dat;
	
    if (n <= 0)			/* no stupid arguments */
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("m_pad(): pad length invalid (%d)\n", n);
#endif /* ENCDEBUG */
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
#ifdef ENCDEBUG
	if (encdebug)
	  printf("m_pad(): length mismatch (should be %d instead of %d)\n",
		 m->m_pkthdr.len, m->m_pkthdr.len + m0->m_len - len);
#endif /* ENCDEBUG */
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
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("m_pad(): cannot append\n");
#endif /* ENCDEBUG */
	    return NULL;
	}

	m0->m_next = m1;
	m0 = m1;
	m0->m_len = 0;
    }

    retval = m0->m_data + m0->m_len;
    m0->m_len += pad;
    m->m_pkthdr.len += pad;

    for (len = 0; len < n; len++)
    {
	get_random_bytes((void *) &dat, sizeof(u_int8_t));
	retval[len] = len + dat;
    }

    return retval;
}
