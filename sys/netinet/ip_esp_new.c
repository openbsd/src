/*	$OpenBSD: ip_esp_new.c,v 1.6 1997/09/28 22:57:47 deraadt Exp $	*/

/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis and Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
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

extern void encap_sendnotify(int, struct tdb *);
extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);
extern void des_set_key(caddr_t, caddr_t);

/*
 * esp_new_attach() is called from the transformation initialization code.
 * It just returns.
 */

int
esp_new_attach()
{
#ifdef ENCDEBUG
    if (encdebug)
      printf("esp_new_attach(): setting up\n");
#endif /* ENCDEBUG */
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
    caddr_t buffer = NULL;
    int blocklen, i, enc_keylen, auth_keylen;
    u_int32_t rk[6];

    if (m->m_len < ENCAP_MSG_FIXED_LEN)
    {
    	if ((m = m_pullup(m, ENCAP_MSG_FIXED_LEN)) == NULL)
    	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_new_init(): m_pullup failed\n");
#endif /* ENCDEBUG */
	    return ENOBUFS;
	}
    }

    em = mtod(m, struct encap_msghdr *);
    if (em->em_msglen - EMT_SETSPI_FLEN <= ESP_NEW_XENCAP_LEN)
    {
	log(LOG_WARNING, "esp_new_init(): initialization failed\n");
	return EINVAL;
    }

    /* Just copy the standard fields */
    m_copydata(m, EMT_SETSPI_FLEN, ESP_NEW_XENCAP_LEN, (caddr_t) &txd);

    /* Check whether the encryption algorithm is supported */
    switch (txd.edx_enc_algorithm)
    {
        case ALG_ENC_DES:
        case ALG_ENC_3DES:
#ifdef ENCDEBUG
            if (encdebug)
              printf("esp_new_init(): initialized TDB with enc algorithm %d\n",
                     txd.edx_enc_algorithm);
#endif /* ENCDEBUG */
            break;

        default:
            log(LOG_WARNING, "esp_new_init(): unsupported encryption algorithm %d specified\n", txd.edx_enc_algorithm);
            return EINVAL;
    }

    /* Check whether the authentication algorithm is supported */
    if (txd.edx_flags & ESP_NEW_FLAG_AUTH)
      switch (txd.edx_hash_algorithm)
      {
          case ALG_AUTH_MD5:
          case ALG_AUTH_SHA1:
#ifdef ENCDEBUG
              if (encdebug)
                printf("esp_new_init(): initialized TDB with hash algorithm %d\n", txd.edx_hash_algorithm);
#endif /* ENCDEBUG */
	      blocklen = HMAC_BLOCK_LEN;
              break;

          default:
              log(LOG_WARNING, "esp_new_init(): unsupported authentication algorithm %d specified\n", txd.edx_enc_algorithm);
              return EINVAL;
      }

    if (txd.edx_ivlen + txd.edx_keylen + EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN
	!= em->em_msglen)
    {
	log(LOG_WARNING, "esp_new_init(): message length (%d) doesn't match\n",
	    em->em_msglen);
	return EINVAL;
    }

    switch (txd.edx_enc_algorithm)
    {
	case ALG_ENC_DES:
	    if ((txd.edx_ivlen != 0) && (txd.edx_ivlen != 8))
	    {
	       	log(LOG_WARNING, "esp_new_init(): unsupported IV length %d\n",
		    txd.edx_ivlen);
		return EINVAL;
	    }

	    if (txd.edx_keylen < 8)
	    {
		log(LOG_WARNING, "esp_new_init(): bad key length\n",
		    txd.edx_keylen);
		return EINVAL;
	    }

	    enc_keylen = 8;
	    break;

	case ALG_ENC_3DES:
            if ((txd.edx_ivlen != 0) && (txd.edx_ivlen != 8))
            {
                log(LOG_WARNING, "esp_new_init(): unsupported IV length %d\n",
                    txd.edx_ivlen);
                return EINVAL;
            }

            if (txd.edx_keylen < 24)
            {
                log(LOG_WARNING, "esp_new_init(): bad key length\n",
                    txd.edx_keylen);
                return EINVAL;
            }

	    enc_keylen = 24;
            break;
    }

    MALLOC(tdbp->tdb_xdata, caddr_t, sizeof(struct esp_new_xdata),
	   M_XDATA, M_WAITOK);
    if (tdbp->tdb_xdata == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_init(): MALLOC() failed\n");
#endif /* ENCDEBUG */
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

    /* Replay counters are mandatory, even without auth */
    xd->edx_rpl = AH_HMAC_INITIAL_RPL;

    /* Copy the IV */
    m_copydata(m, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN, xd->edx_ivlen,
	       (caddr_t) xd->edx_iv);

    switch (xd->edx_enc_algorithm)
    {
	case ALG_ENC_DES:
	    /* Copy the key material */
	    m_copydata(m, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + xd->edx_ivlen,
		       enc_keylen, (caddr_t) rk);

	    des_set_key((caddr_t) rk, (caddr_t) (xd->edx_eks[0]));
	    break;

	case ALG_ENC_3DES:
	    /* Copy the key material */
	    m_copydata(m, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + xd->edx_ivlen,
		       enc_keylen, (caddr_t) rk);

	    des_set_key((caddr_t) rk, (caddr_t) (xd->edx_eks[0]));
	    des_set_key((caddr_t) rk + 2, (caddr_t) (xd->edx_eks[1]));
	    des_set_key((caddr_t) rk + 4, (caddr_t) (xd->edx_eks[2]));
	    break;
    }

    if (txd.edx_flags & ESP_NEW_FLAG_AUTH)
    {
	auth_keylen = txd.edx_keylen - enc_keylen;

#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_init(): using %d bytes of authentication key\n",
		 auth_keylen);
#endif

	MALLOC(buffer, caddr_t, 
	       auth_keylen < blocklen ? blocklen : auth_keylen,
	       M_TEMP, M_WAITOK);
	if (buffer == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_new_init(): MALLOC() failed\n");
#endif /* ENCDEBUG */
	    free(tdbp->tdb_xdata, M_XDATA);
	    return ENOBUFS;
	}

	bzero(buffer, auth_keylen < blocklen ? blocklen : auth_keylen);

	/* Copy the key to the buffer */
	m_copydata(m, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + xd->edx_ivlen +
		   enc_keylen, auth_keylen, buffer);

	/* Shorten the key if necessary */
	if (auth_keylen > blocklen)
	{
	    switch (xd->edx_hash_algorithm)
	    {
		case ALG_AUTH_MD5:
		    MD5Init(&(xd->edx_md5_ictx));
		    MD5Update(&(xd->edx_md5_ictx), buffer, auth_keylen);
		    bzero(buffer, 
			  auth_keylen < blocklen ? blocklen : auth_keylen);
		    MD5Final(buffer, &(xd->edx_md5_ictx));
		    break;

		case ALG_AUTH_SHA1:
		    SHA1Init(&(xd->edx_sha1_ictx));
		    SHA1Update(&(xd->edx_sha1_ictx), buffer, auth_keylen);
		    bzero(buffer,
			  auth_keylen < blocklen ? blocklen : auth_keylen);
		    SHA1Final(buffer, &(xd->edx_sha1_ictx));
		    break;
	    }
	}

	/* Precompute the I and O pads of the HMAC */
	for (i = 0; i < blocklen; i++)
	  buffer[i] ^= HMAC_IPAD_VAL;

	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
		MD5Init(&(xd->edx_md5_ictx));
		MD5Update(&(xd->edx_md5_ictx), buffer, blocklen);
		break;

	    case ALG_AUTH_SHA1:
		SHA1Init(&(xd->edx_sha1_ictx));
		SHA1Update(&(xd->edx_sha1_ictx), buffer, blocklen);
		break;
	}
	 
	for (i = 0; i < blocklen; i++)
	  buffer[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
		MD5Init(&(xd->edx_md5_octx));
		MD5Update(&(xd->edx_md5_octx), buffer, blocklen);
		break;

	    case ALG_AUTH_SHA1:
		SHA1Init(&(xd->edx_sha1_octx));
		SHA1Update(&(xd->edx_sha1_octx), buffer, blocklen);
		break;
	}

	bzero(buffer, blocklen);
	free(buffer, M_TEMP);
    }

    bzero(rk, 6 * sizeof(u_int32_t));		/* paranoid */
    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */

    return 0;
}

int
esp_new_zeroize(struct tdb *tdbp)
{
#ifdef ENCDEBUG
    if (encdebug)
      printf("esp_new_zeroize(): freeing memory\n");
#endif ENCDEBUG
    FREE(tdbp->tdb_xdata, M_XDATA);
    return 0;
}


struct mbuf *
esp_new_input(struct mbuf *m, struct tdb *tdb)
{
    struct esp_new_xdata *xd;
    struct ip *ip, ipo;
    u_char iv[ESP_3DES_IVS], niv[ESP_3DES_IVS], blk[ESP_3DES_BLKS], opts[40];
    u_char *idat, *odat;
    struct esp_new *esp;
    struct ifnet *rcvif;
    int ohlen, oplen, plen, alen, ilen, olen, i, blks;
    int count, off, errc;
    u_int32_t btsx;
    struct mbuf *mi, *mo;
    MD5_CTX md5ctx;
    SHA1_CTX sha1ctx;
    u_char buf[AH_ALEN_MAX], buf2[AH_ALEN_MAX];

    xd = (struct esp_new_xdata *)tdb->tdb_xdata;

    switch (xd->edx_enc_algorithm)
    {
	case ALG_ENC_DES:
	    blks = ESP_DES_BLKS;
	    break;

	case ALG_ENC_3DES:
	    blks = ESP_3DES_BLKS;
	    break;

	default:
            log(LOG_ALERT,
                "esp_new_input(): unsupported algorithm %d in SA %x/%08x\n",
                xd->edx_enc_algorithm, tdb->tdb_dst, ntohl(tdb->tdb_spi));
            m_freem(m);
            return NULL;
    }

    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
    { 
	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
	    case ALG_AUTH_SHA1:
		alen = AH_HMAC_HASHLEN;
		break;

	    default:
		log(LOG_ALERT, "esp_new_input(): unsupported algorithm %d in SA %x/%08x\n", xd->edx_hash_algorithm, tdb->tdb_dst, 
		    ntohl(tdb->tdb_spi));
		m_freem(m);
		return NULL;
	}
    } 
    else
      alen = 0;

    rcvif = m->m_pkthdr.rcvif;
    if (rcvif == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_input(): receive interface is NULL!!!\n");
#endif /* ENCDEBUG */
	rcvif = &enc_softc;
    }

    if (m->m_len < sizeof(struct ip))
    {
	if ((m = m_pullup(m, sizeof(struct ip))) == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_new_input(): (possibly too short) packet dropped\n");
#endif /* ENCDEBUG */
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
#ifdef ENCDEBUG
            if (encdebug)
              printf("esp_old_input(): m_pullup() failed\n");
#endif /* ENCDEBUG */
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
		    log(LOG_ERR, "esp_new_input(): replay counter wrapped for packets from %x to %x, spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(esp->esp_spi));
		    espstat.esps_wrap++;
		    break;

		case 2:
	        case 3:
		    log(LOG_WARNING, "esp_new_input(): duplicate packet received, %x->%x spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(esp->esp_spi));
		    espstat.esps_replay++;
		    break;
	    }

	    m_freem(m);
	    return NULL;
	}
    }

    /* Skip the IP header, IP options, SPI, SN and IV and minus Auth Data*/
    plen = m->m_pkthdr.len - (ip->ip_hl << 2) - 2 * sizeof (u_int32_t) - 
	   xd->edx_ivlen - alen;
    if (plen & (blks - 1))
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_input(): payload not a multiple of %d octets for packet from %x to %x, spi %08x\n", blks, ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    if (xd->edx_flags & ESP_NEW_FLAG_AUTH) 
    {
	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
		md5ctx = xd->edx_md5_ictx;
		break;
	      
	    case ALG_AUTH_SHA1:
		sha1ctx = xd->edx_sha1_ictx;
		break;
	}

	/* Auth covers SPI + SN + IV*/
	oplen = plen + 2 * sizeof(u_int32_t) + xd->edx_ivlen; 
	off = (ip->ip_hl << 2);

	mo = m;
	while (oplen > 0)
	{
	    if (mo == 0)
	      panic("esp_new_input(): m_copydata (copy)");

	    count = min(mo->m_len - off, oplen);

	    switch (xd->edx_hash_algorithm)
	    {
		case ALG_AUTH_MD5:
		    MD5Update(&md5ctx, mtod(mo, unsigned char *) + off, 
			      count);
		    break;

		case ALG_AUTH_SHA1:
		    SHA1Update(&sha1ctx, mtod(mo, unsigned char *) + off, 
			       count);
		    break;
	    }

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

	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
		MD5Final(buf2, &md5ctx);
		md5ctx = xd->edx_md5_octx;
		MD5Update(&md5ctx, buf2, AH_MD5_ALEN);
		MD5Final(buf2, &md5ctx);
		break;

	    case ALG_AUTH_SHA1:
		SHA1Final(buf2, &sha1ctx);
		sha1ctx = xd->edx_sha1_octx;
		SHA1Update(&sha1ctx, buf2, AH_SHA1_ALEN);
		SHA1Final(buf2, &sha1ctx);
		break;
	}

	if (bcmp(buf2, buf, AH_HMAC_HASHLEN))
	{
	    log(LOG_ALERT,
		"esp_new_input(): authentication failed for packet from %x to %x, spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(esp->esp_spi));
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
    
    olen = ilen;
    odat = idat;
    mi = mo = m;
    i = 0;

    /*
     * At this point:
     *   plen is # of encapsulated payload octets
     *   ilen is # of octets left in this mbuf
     *   idat is first encapsulated payload octed in this mbuf
     *   same for olen and odat
     *   iv contains the IV.
     *   mi and mo point to the first mbuf
     *
     * From now on until the end of the mbuf chain:
     *   . move the next eight octets of the chain into blk[]
     *     (ilen, idat, and mi are adjusted accordingly)
     *     and save it back into iv[]
     *   . decrypt blk[], xor with iv[], put back into chain
     *     (olen, odat, amd mo are adjusted accordingly)
     *   . repeat
     */

    while (plen > 0)		/* while not done */
    {
	while (ilen == 0)	/* we exhausted previous mbuf */
	{
	    mi = mi->m_next;
	    if (mi == NULL)
	      panic("esp_new_input(): bad chain (i)\n");

	    ilen = mi->m_len;
	    idat = (u_char *) mi->m_data;
	}

	blk[i] = niv[i] = *idat++;
	i++;
	ilen--;

	if (i == blks)
	{
	    switch (xd->edx_enc_algorithm)
	    {
		case ALG_ENC_DES:
	    	    des_ecb_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]), 0);
		    break;

		case ALG_ENC_3DES:
		    des_ecb3_encrypt(blk, blk, (caddr_t) (xd->edx_eks[2]),
                             	     (caddr_t) (xd->edx_eks[1]),
                             	     (caddr_t) (xd->edx_eks[0]), 0);
		    break;
	    }

	    for (i=0; i<8; i++)
	    {
		while (olen == 0)
		{
		    mo = mo->m_next;
		    if (mo == NULL)
		      panic("esp_new_input(): bad chain (o)\n");

		    olen = mo->m_len;
		    odat = (u_char *)mo->m_data;
		}

		*odat = blk[i] ^ iv[i];
		iv[i] = niv[i];
		blk[i] = *odat++; /* needed elsewhere */
		olen--;
	    }

	    i = 0;
	}

	plen--;
    }

    /* Save the options */
    m_copydata(m, sizeof(struct ip), (ipo.ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    /*
     * Now, the entire chain has been decrypted. As a side effect,
     * blk[7] contains the next protocol, and blk[6] contains the
     * amount of padding the original chain had. Chop off the
     * appropriate parts of the chain, and return.
     * Verify correct decryption by checking the last padding bytes.
     */

    if (xd->edx_flags & ESP_NEW_FLAG_OPADDING)
    {
        if (blk[6] != blk[5])
	{
	    log(LOG_ALERT, "esp_new_input(): decryption failed for packet from %x to %x, SA %x/%08x\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
	    m_freem(m);
	    return NULL;
	} 

      	m_adj(m, - blk[6] - 2 - alen);		/* Old type padding */
    }
    else
    {
	if (blk[6] == 0)
	{
	    log(LOG_ALERT, "esp_new_input(): decryption failed for packet from %x to %x, SA %x/%08x -- peer is probably using old style padding\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
	    m_freem(m);
	    return NULL;
	}
	else
	  if ((blk[6] == 0) || (blk[6] != blk[5] + 1))
          {
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
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_new_input(): m_pullup() failed for packet from %x to %x, SA %x/%08x\n", ipo.ip_src, ipo.ip_dst, tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ipo.ip_p = blk[7];
    ipo.ip_id = htons(ipo.ip_id);
    ipo.ip_off = 0;
    ipo.ip_len += (ipo.ip_hl << 2) -  2 * sizeof(u_int32_t) - xd->edx_ivlen -
		  blk[6] - 1 - alen;

    if (xd->edx_flags & ESP_NEW_FLAG_OPADDING)
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

    if (xd->edx_flags & ESP_NEW_FLAG_OPADDING)
    {
	tdb->tdb_cur_bytes++;
	espstat.esps_ibytes++;
    }

    /* Notify on expiration */
    if (tdb->tdb_flags & TDBF_SOFT_PACKETS)
      if (tdb->tdb_cur_packets >= tdb->tdb_soft_packets)
      {
	  encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb);
	  tdb->tdb_flags &= ~TDBF_SOFT_PACKETS;
      }
      else
	if (tdb->tdb_flags & TDBF_SOFT_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)
	  {
	      encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb);
	      tdb->tdb_flags &= ~TDBF_SOFT_BYTES;
	  }
    
    if (tdb->tdb_flags & TDBF_PACKETS)
      if (tdb->tdb_cur_packets >= tdb->tdb_exp_packets)
      {
	  encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb);
	  tdb_delete(tdb, 0);
      }
      else
	if (tdb->tdb_flags & TDBF_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes)
	  {
	      encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb);
	      tdb_delete(tdb, 0);
	  }

    return m;
}

int
esp_new_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
	       struct mbuf **mp)
{
    struct esp_new_xdata *xd;
    struct ip *ip, ipo;
    int i, ilen, olen, ohlen, nh, rlen, plen, padding;
    struct esp_new espo;
    struct mbuf *mi, *mo;
    u_char *pad, *idat, *odat;
    u_char iv[ESP_3DES_IVS], blk[ESP_3DES_IVS], auth[AH_ALEN_MAX], opts[40];
    MD5_CTX md5ctx;
    SHA1_CTX sha1ctx;
    int iphlen, blks, alen;
    
    xd = (struct esp_new_xdata *) tdb->tdb_xdata;

    switch (xd->edx_enc_algorithm)
    {
        case ALG_ENC_DES:
            blks = ESP_DES_BLKS;
            break;

        case ALG_ENC_3DES:
            blks = ESP_3DES_BLKS;
            break;

        default:
            log(LOG_ALERT,
                "esp_new_output(): unsupported algorithm %d in SA %x/%08x\n",
                xd->edx_enc_algorithm, tdb->tdb_dst, ntohl(tdb->tdb_spi));
            m_freem(m);
            return NULL;
    }

    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
    {
	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
	    case ALG_AUTH_SHA1:
		alen = AH_HMAC_HASHLEN;
#ifdef ENCDEBUG
		if (encdebug)
		  printf("esp_new_output(): using hash algorithm %d\n",
			 xd->edx_hash_algorithm);
#endif /* ENCDEBUG */
		break;

	    default:
		log(LOG_ALERT, "esp_new_output(): unsupported algorithm %d in SA %x/%08x\n", xd->edx_hash_algorithm, tdb->tdb_dst, ntohl(tdb->tdb_spi));
		m_freem(m);
		return NULL;
	}
    } 
    else
      alen = 0;

    espstat.esps_output++;

    m = m_pullup(m, sizeof (struct ip));   /* Get IP header in one mbuf */
    if (m == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_output(): m_pullup() failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	return ENOBUFS;
    }

    if (xd->edx_rpl == 0)
    {
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
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("esp_new_input(): m_pullup() failed for SA %x/%08x\n",
		     tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
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
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_output(): m_pad() failed for SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
      	return ENOBUFS;
    }

    /* Self describing padding */
    for (i = 0; i < padding - 2; i++)
      pad[i] = i + 1;

    if (xd->edx_flags & ESP_NEW_FLAG_OPADDING)
      pad[padding - 2] = padding - 2;
    else
      pad[padding - 2] = padding - 1;

    pad[padding - 1] = nh;

    mi = mo = m;
    plen = rlen + padding;
    ilen = olen = m->m_len - iphlen;
    idat = odat = mtod(m, u_char *) + iphlen;
    i = 0;

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
	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
		md5ctx = xd->edx_md5_ictx;
		MD5Update(&md5ctx, (unsigned char *) &espo, 
			  2 * sizeof(u_int32_t) + xd->edx_ivlen);
		break;

	    case ALG_AUTH_SHA1:
		sha1ctx = xd->edx_sha1_ictx;
		SHA1Update(&sha1ctx, (unsigned char *) &espo, 
			   2 * sizeof(u_int32_t) + xd->edx_ivlen);
		break;
	}
    }

    /* Encrypt the payload */

    while (plen > 0)		/* while not done */
    {
	while (ilen == 0)	/* we exhausted previous mbuf */
	{
	    mi = mi->m_next;
	    if (mi == NULL)
	      panic("esp_new_output(): bad chain (i)\n");

	    ilen = mi->m_len;
	    idat = (u_char *) mi->m_data;
	}

	blk[i] = *idat++ ^ iv[i];
		
	i++;
	ilen--;

	if (i == blks)
	{
	    switch (xd->edx_enc_algorithm)
	    {
		case ALG_ENC_DES:
	    	    des_ecb_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]), 1);
		    break;

		case ALG_ENC_3DES:
                    des_ecb3_encrypt(blk, blk, (caddr_t) (xd->edx_eks[0]),
                            	     (caddr_t) (xd->edx_eks[1]),
                             	     (caddr_t) (xd->edx_eks[2]), 1);
		    break;
	    }

	    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
	      switch (xd->edx_hash_algorithm)
	      {
		  case ALG_AUTH_MD5:
		      MD5Update(&md5ctx, blk, blks);
		      break;

		  case ALG_AUTH_SHA1:
		      SHA1Update(&sha1ctx, blk, blks);
		      break;
	      }

	    for (i = 0; i < blks; i++)
	    {
		while (olen == 0)
		{
		    mo = mo->m_next;
		    if (mo == NULL)
		      panic("esp_new_output(): bad chain (o)\n");

		    olen = mo->m_len;
		    odat = (u_char *)mo->m_data;
		}

		*odat++ = blk[i];
		iv[i] = blk[i];
		olen--;
	    }
	    i = 0;
	}

	plen--;
    }

    /* Put in authentication data */
    if (xd->edx_flags & ESP_NEW_FLAG_AUTH)
    {
	switch (xd->edx_hash_algorithm)
	{
	    case ALG_AUTH_MD5:
		MD5Final(auth, &md5ctx);
		md5ctx = xd->edx_md5_octx;
		MD5Update(&md5ctx, auth, AH_MD5_ALEN);
		MD5Final(auth, &md5ctx);
		break;

	    case ALG_AUTH_SHA1:
		SHA1Final(auth, &sha1ctx);
		sha1ctx = xd->edx_sha1_octx;
		SHA1Update(&sha1ctx, auth, AH_SHA1_ALEN);
		SHA1Final(auth, &sha1ctx);
		break;
	}

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
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_output(): M_PREPEND failed, SA %x/%08x\n",
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

    /* Fix the length and the next protocol, copy back and off we go */
    ipo.ip_len = htons(iphlen + ohlen + rlen + padding + alen);
    ipo.ip_p = IPPROTO_ESP;

    /* Save the last encrypted block, to be used as the next IV */
    bcopy(blk, xd->edx_iv, xd->edx_ivlen);

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
      if (tdb->tdb_cur_packets >= tdb->tdb_soft_packets)
      {
	  encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb);
	  tdb->tdb_flags &= ~TDBF_SOFT_PACKETS;
      }
      else
	if (tdb->tdb_flags & TDBF_SOFT_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes)
	  {
	      encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb);
	      tdb->tdb_flags &= ~TDBF_SOFT_BYTES;
	  }
    
    if (tdb->tdb_flags & TDBF_PACKETS)
      if (tdb->tdb_cur_packets >= tdb->tdb_exp_packets)
      {
	  encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb);
	  tdb_delete(tdb, 0);
      }
      else
	if (tdb->tdb_flags & TDBF_BYTES)
	  if (tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes)
	  {
	      encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb);
	      tdb_delete(tdb, 0);
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
