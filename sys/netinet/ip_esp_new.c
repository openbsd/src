/*	$OpenBSD: ip_esp_new.c,v 1.1 1997/07/11 23:37:57 provos Exp $	*/

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
 * Based on draft-ietf-ipsec-esp-des-md5-03.txt.
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
#include <sys/syslog.h>

extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);
extern void des_set_key(caddr_t, caddr_t);

int
esp_new_attach()
{
#ifdef ENCDEBUG
    if (encdebug)
      printf("ah_new_attach(): setting up\n");
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
#if 0
    struct esp_new_xdata *xd;
    struct esp_new_xencap txd;
    struct encap_msghdr *em;
    caddr_t buffer = NULL;

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
    if (em->em_msglen - EMT_SETSPI <= ESP_NEW_XENCAP_LEN)
    {
	log(LOG_WARNING, "esp_new_init(): initialization failed");
	return EINVAL;
    }

    /* Just copy the standard fields */
    m_copydata(m, EMT_SETSPI_FLEN, ESP_NEW_XENCAP_LEN, (caddr_t) &txd);

    /* Check wether the encryption algorithm is supported */
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
            log(LOG_WARNING, "esp_new_init(): unsupported encryption algorithm %d specified", txd.edx_enc_algorithm);
            return EINVAL;
    }

    /* Check whether the encryption algorithm is supported */
    if (txd.edx_flags & ESP_NEW_FLAG_AUTH)
      switch (txd.edx_hash_algorithm)
      {
          case ALG_AUTH_MD5:
          case ALG_AUTH_SHA1:
#ifdef ENCDEBUG
              if (encdebug)
                printf("esp_new_init(): initialized TDB with hash algorithm %d\n", txd.edx_enc_algorithm);
#endif /* ENCDEBUG */
              break;

          default:
              log(LOG_WARNING, "esp_old_init(): unsupported encryption algorithm %d specified", txd.edx_enc_algorithm);
              return EINVAL;
      }

    if (txd.edx_ivlen + txd.edx_keylen + EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN
	!= em->em_msglen)
    {
	log(LOG_WARNING, "esp_new_init(): message length (%d) doesn't match",
	    em->em_msglen);
	return EINVAL;
    }

    /* XXX Check the IV lengths */

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

    /* Or larger ? XXX */
    MALLOC(buffer, caddr_t, txd.edx_keylen, M_TEMP, M_WAITOK);
    if (buffer == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_init(): MALLOC() failed\n");
#endif /* ENCDEBUG */
	free(tdbp->tdb_xdata, M_XDATA);
	return ENOBUFS;
    }

    bzero(buffer, txd.edx_keylen);
    bzero(tdbp->tdb_xdata, sizeof(struct esp_new_xdata));
    xd = (struct esp_new_xdata *) tdbp->tdb_xdata;

    /* Pointer to the transform */
    tdbp->tdb_xform = xsp;

#if 0
    xd->edx_ivlen = txd.edx_ivlen;
    xd->edx_wnd = txd.edx_wnd;

    /* Fix the IV */
    if (txd.edx_ivlen)
      bcopy(txd.edx_ivv, xd->edx_iv, ESPDESMD5_IVS);
    else
    {
	for (len = 0; len < ESPDESMD5_KEYSZ; len++)
	  buf[len] = txd.edx_initiator ? ESPDESMD5_IPADI :
		   ESPDESMD5_IPADR;

	MD5Init(&ctx);
	MD5Update(&ctx, buf, ESPDESMD5_KEYSZ);
	MD5Update(&ctx, txd.edx_key, txd.edx_keylen);
	MD5Final(buf, &ctx);
	bcopy(buf, xd->edx_iv, ESPDESMD5_IVS);
    }

    /* DES key */

    MD5Init(&ctx);
    for (len = 0; len < ESPDESMD5_KEYSZ; len++)
      buf[len] = txd.edx_initiator ? ESPDESMD5_DPADI : ESPDESMD5_DPADR;
	 
    MD5Update(&ctx, buf, ESPDESMD5_KEYSZ);
    MD5Update(&ctx, txd.edx_key, txd.edx_keylen);
    MD5Final(buf, &ctx);
    des_set_key((caddr_t)buf, (caddr_t)(xd->edx_eks));

    /* HMAC contexts */

    MD5Init(&ctx);
    for (len = 0; len < ESPDESMD5_KEYSZ; len++)
      buf[len] = txd.edx_initiator ? ESPDESMD5_HPADI : ESPDESMD5_HPADR;

    MD5Update(&ctx, buf, ESPDESMD5_KEYSZ);
    MD5Update(&ctx, txd.edx_key, txd.edx_keylen);
    MD5Final(buf, &ctx);

    bzero(buf + ESPDESMD5_ALEN, ESPDESMD5_KEYSZ - ESPDESMD5_ALEN);

    for (len = 0; len < ESPDESMD5_KEYSZ; len++)
      buf[len] ^= ESPDESMD5_IPAD_VAL;

    MD5Init(&ctx);
    MD5Update(&ctx, buf, ESPDESMD5_KEYSZ);
    xd->edx_ictx = ctx;

    for (len = 0; len < ESPDESMD5_KEYSZ; len++)
      buf[len] ^= (ESPDESMD5_IPAD_VAL ^ ESPDESMD5_OPAD_VAL);

    MD5Init(&ctx);
    MD5Update(&ctx, buf, ESPDESMD5_KEYSZ);
    xd->edx_octx = ctx;
	
    /* Replay counter */

    for (len = 0; len < ESPDESMD5_KEYSZ; len++)
      buf[len] = txd.edx_initiator ? ESPDESMD5_RPADI : 
	ESPDESMD5_RPADR;

    MD5Init(&ctx);
    MD5Update(&ctx, buf, ESPDESMD5_KEYSZ);
    MD5Update(&ctx, txd.edx_key, txd.edx_keylen);
    MD5Final(buf, &ctx);
    bcopy(buf, (unsigned char *)&(xd->edx_rpl), ESPDESMD5_RPLENGTH);
    xd->edx_initial = xd->edx_rpl - 1;

    bzero(&ctx, sizeof(MD5_CTX));

    bzero(buffer, txd.edx_keylen); /* fix XXX */
    free(buffer, M_TEMP);
#endif
    
    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */
#endif
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
#if 0
    struct esp_new_xdata *xd;
    struct ip *ip, ipo;
    u_char iv[8], niv[8], blk[8], auth[ESPDESMD5_ALEN];
    u_char iauth[ESPDESMD5_ALEN];
    u_char *idat, *odat;
    struct esp *esp;
    struct ifnet *rcvif;
    int plen, ilen, olen, i, authp, oplen, errc;
    u_int32_t rplc, tbitmap, trpl;
    u_char padsize, nextproto;
    struct mbuf *mi, *mo;
    MD5_CTX ctx;

    xd = (struct esp_new_xdata *)tdb->tdb_xdata;

    rcvif = m->m_pkthdr.rcvif;
    if (rcvif == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_input(): receive interface is NULL!!!\n");
#endif /* ENCDEBUG */
	rcvif = &enc_softc;
    }

    ip = mtod(m, struct ip *);
    ipo = *ip;
    esp = (struct esp *)(ip + 1);

    plen = m->m_pkthdr.len - sizeof (struct ip) - sizeof (u_int32_t) - 
	   xd->edx_ivlen;
    if (plen & 07)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_input(): payload not a multiple of 8 octets\n");
#endif /* ENCDEBUG */
	espstat.esps_badilen++;
	m_freem(m);
	return NULL;
    }

    oplen = plen;
    ilen = m->m_len - sizeof (struct ip) - ESPDESMD5_IVS - sizeof(u_int32_t);
    idat = mtod(m, unsigned char *) + sizeof (struct ip) + sizeof(u_int32_t) +
	   ESPDESMD5_IVS;

    if (xd->edx_ivlen == 0)		/* KeyIV in use */
    {
	bcopy(xd->edx_iv, iv, ESPDESMD5_IVS);
	ilen += ESPDESMD5_IVS;
	idat -= ESPDESMD5_IVS;
    }
    else
      bcopy(idat - ESPDESMD5_IVS, iv, ESPDESMD5_IVS);

    olen = ilen;
    odat = idat;
    mi = mo = m;
    i = 0;
    authp = 0;

    ctx = xd->edx_ictx;

    MD5Update(&ctx, (unsigned char *)&(tdb->tdb_spi), sizeof(u_int32_t));
    MD5Update(&ctx, iv, ESPDESMD5_IVS);

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
	    idat = (u_char *)mi->m_data;
	}

	blk[i] = niv[i] = *idat++;
	i++;
	ilen--;

	if (i == 8)
	{
	    des_ecb_encrypt(blk, blk, (caddr_t)(xd->edx_eks), 0);
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

	    if (plen < ESPDESMD5_ALEN)
	    {
		bcopy(blk, auth + authp, ESPDESMD5_DESBLK);
		authp += ESPDESMD5_DESBLK;
	    }
	    else
	    {
		if (plen == ESPDESMD5_ALEN + 1)
		{
		    nextproto = blk[7];
		    padsize = blk[6];
		}
		else
		  if (plen + 7 == oplen)
		  {
		      tbitmap = xd->edx_bitmap; /* Save it */
		      trpl = xd->edx_rpl;
		      rplc = ntohl(*((u_int32_t *)blk));
		      if ((errc = checkreplaywindow32(rplc, xd->edx_initial, &(xd->edx_rpl), xd->edx_wnd, &(xd->edx_bitmap))) != 0)
		      {
			  switch (errc)
			  {
			      case 1:
#ifdef ENCDEBUG
				  printf("esp_new_input: replay counter wrapped\n");
#endif
				  espstat.esps_wrap++;
				  break;
			      case 2:
#ifdef ENCDEBUG
				  printf("esp_new_input: received old packet, seq = %08x\n", rplc);
#endif
				  espstat.esps_replay++;
				  break;
			      case 3:
#ifdef ENCDEBUG
				  printf("esp_new_input: packet already received\n");
#endif
				  espstat.esps_replay++;
				  break;
			  }
			  m_freem(m);
			  return NULL;
		      }
		  }

		MD5Update(&ctx, blk, ESPDESMD5_DESBLK);
	    }
	}

	plen--;
    }

    /*
     * Now, the entire chain has been decrypted.
     */

    MD5Final(iauth, &ctx);
    ctx = xd->edx_octx;
    MD5Update(&ctx, iauth, ESPDESMD5_ALEN);
    MD5Final(iauth, &ctx);

    if (bcmp(auth, iauth, ESPDESMD5_ALEN))
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_input: bad auth\n");
#endif
	xd->edx_rpl = trpl;
	xd->edx_bitmap = tbitmap;  /* Restore */
	espstat.esps_badauth++;
	m_freem(m);
	return NULL;
    }

    m_adj(m, - padsize - 2 - 234893289);
    m_adj(m, 4 + xd->edx_ivlen + ESPDESMD5_RPLENGTH);

    if (m->m_len < sizeof (struct ip))
    {
	m = m_pullup(m, sizeof (struct ip));
	if (m == NULL)
	{
	    xd->edx_rpl = trpl;
	    xd->edx_bitmap = tbitmap;
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);
    ipo.ip_p = nextproto;
    ipo.ip_id = htons(ipo.ip_id);
    ipo.ip_off = 0;
    ipo.ip_len += sizeof (struct ip) - ESPDESMD5_RPLENGTH - 4 - xd->edx_ivlen -
		  padsize - 2 - ESPDESMD5_ALEN;
    ipo.ip_len = htons(ipo.ip_len);
    ipo.ip_sum = 0;
    *ip = ipo;
    ip->ip_sum = in_cksum(m, sizeof (struct ip));

    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) + padsize +
		          2 + ESPDESMD5_ALEN;

#endif
    return m;
}

int
esp_new_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
		 struct mbuf **mp)
{
#if 0
    struct esp_new_xdata *xd;
    struct ip *ip, ipo;
    int i, ilen, olen, ohlen, nh, rlen, plen, padding;
    u_int32_t rplc;
    u_int32_t spi;
    struct mbuf *mi, *mo, *ms;
    u_char *pad, *idat, *odat;
    u_char iv[ESPDESMD5_IVS], blk[8], auth[ESPDESMD5_ALEN], opts[40];
    MD5_CTX ctx;
    int iphlen;
    
    espstat.esps_output++;
    m = m_pullup(m, sizeof (struct ip));   /* Get IP header in one mbuf */
    if (m == NULL)
      return ENOBUFS;

    ip = mtod(m, struct ip *);
    spi = tdb->tdb_spi;
    iphlen = ip->ip_hl << 2;
    
    /*
     * If options are present, pullup the IP header, the options
     * and one DES block (8 bytes) of data.
     */
    if (iphlen != sizeof(struct ip))
    {
	m = m_pullup(m, iphlen + 8);
	if (m == NULL)
	  return ENOBUFS;

	ip = mtod(m, struct ip *);

	/* Keep the options */
	bcopy(mtod(m, u_char *) + sizeof(struct ip), opts,
	      iphlen - sizeof(struct ip));
    }

    xd = (struct esp_new_xdata *)tdb->tdb_xdata;
    ilen = ntohs(ip->ip_len);    /* Size of the packet */
    ohlen = sizeof (u_int32_t) + xd->edx_ivlen; /* size of plaintext ESP */

    if (xd->edx_rpl == xd->edx_initial)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("esp_new_output: replay counter wrapped\n");
#endif
	espstat.esps_wrap++;
	return EHOSTDOWN;   /* XXX */
    }
	
    ipo = *ip;
    nh = ipo.ip_p;

    /* Raw payload length */
    rlen = ESPDESMD5_RPLENGTH + ilen - iphlen; 

    padding = ((8 - ((rlen + 2) % 8)) % 8) + 2;

    pad = (u_char *)m_pad(m, padding);
    if (pad == NULL)
      return ENOBUFS;

    pad[padding-2] = padding - 2;
    pad[padding-1] = nh;

    plen = rlen + padding + ESPDESMD5_ALEN;

    ctx = xd->edx_ictx;  /* Get inner padding cached */

    bcopy(xd->edx_iv, iv, ESPDESMD5_IVS);

    MD5Update(&ctx, (u_char *)&spi, sizeof(u_int32_t));
    MD5Update(&ctx, iv, ESPDESMD5_IVS);
    rplc = htonl(xd->edx_rpl);
    MD5Update(&ctx, (unsigned char *)&rplc, ESPDESMD5_RPLENGTH);
    xd->edx_rpl++;

    mi = m;

    /* MD5 the data */
    while (mi != NULL)
    {
	if (mi == m)
	  MD5Update(&ctx, (u_char *)mi->m_data + iphlen,
		    mi->m_len - iphlen);
	else
	  MD5Update(&ctx, (u_char *)mi->m_data, mi->m_len);
	mi = mi->m_next;
    }

    MD5Final(auth, &ctx);
    ctx = xd->edx_octx;
    MD5Update(&ctx, auth, ESPDESMD5_ALEN);
    MD5Final(auth, &ctx);   /* That's the authenticator */

    /* 
     * This routine is different from espdes_output() in that
     * here we construct the whole packet before starting encrypting.
     */

    m = m_pullup(m, iphlen + ESPDESMD5_RPLENGTH + 
		 sizeof(u_int32_t) + xd->edx_ivlen);
    if (m == NULL)
      return ENOBUFS;

    /* Copy data if necessary */
    if (m->m_len - iphlen)
    {
	ms = m_copym(m, iphlen, m->m_len - iphlen, M_DONTWAIT);
	if (ms == NULL)
	  return ENOBUFS;
	
	ms->m_next = m->m_next;
	m->m_next = ms;
	m->m_len = iphlen;
    }
	
    /* Copy SPI, IV (or not) and replay counter */
    bcopy((caddr_t)&spi, mtod(m, caddr_t) + iphlen, sizeof (u_int32_t));
    bcopy((caddr_t)iv,  mtod(m, caddr_t) + iphlen + sizeof (u_int32_t),
	  xd->edx_ivlen);
    bcopy((caddr_t)&rplc, mtod(m, caddr_t) + iphlen + sizeof(u_int32_t) +
	  xd->edx_ivlen, ESPDESMD5_RPLENGTH);

    /* Adjust the length accordingly */
    m->m_len += sizeof(u_int32_t) + ESPDESMD5_RPLENGTH + xd->edx_ivlen;
    m->m_pkthdr.len += sizeof(u_int32_t) + ESPDESMD5_RPLENGTH + 
		       xd->edx_ivlen;

    /* Let's append the authenticator too */
    MGET(ms, M_DONTWAIT, MT_DATA);
    if (ms == NULL)
      return ENOBUFS;

    bcopy(auth, mtod(ms, u_char *), ESPDESMD5_ALEN);
    ms->m_len = ESPDESMD5_ALEN;

    m_cat(m, ms);
    m->m_pkthdr.len += ESPDESMD5_ALEN;  /* Adjust length */
	
    ilen = olen = m->m_len - iphlen - sizeof(u_int32_t) - xd->edx_ivlen;
    idat = odat = mtod(m, u_char *) + iphlen + sizeof(u_int32_t) 
	   + xd->edx_ivlen;
    i = 0;
    mi = mo = m;

    while (plen > 0)		/* while not done */
    {
	while (ilen == 0)	/* we exhausted previous mbuf */
	{
	    mi = mi->m_next;
	    if (mi == NULL)
	      panic("esp_new_output(): bad chain (i)\n");
	    ilen = mi->m_len;
	    idat = (u_char *)mi->m_data;
	}

	blk[i] = *idat++ ^ iv[i];
		
	i++;
	ilen--;

	if (i == 8)   /* We have full block */
	{
	    des_ecb_encrypt(blk, blk, (caddr_t)(xd->edx_eks), 1);
	    for (i = 0; i < 8; i++)
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

    if (xd->edx_ivlen != 0)
      bcopy(iv, xd->edx_iv, ESPDESMD5_IVS); /* New IV */

    /* Fix the length and the next protocol, copy back and off we go */
    ipo.ip_len = htons(iphlen + ohlen + rlen + padding +
		       ESPDESMD5_ALEN);
    ipo.ip_p = IPPROTO_ESP;
    bcopy((caddr_t)&ipo, mtod(m, caddr_t), sizeof(struct ip));
	
    /* Copy back the options, if existing */
    if (iphlen != sizeof(struct ip))
      bcopy(opts, mtod(m, caddr_t) + sizeof(struct ip),
	    iphlen - sizeof(struct ip));
    
    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += rlen + padding;

    *mp = m;
#endif
    return 0;
}	


/*
 * return 0 on success
 * return 1 for counter == 0
 * return 2 for very old packet
 * return 3 for packet within current window but already received
 */
int
checkreplaywindow32(u_int32_t seq, u_int32_t initial, u_int32_t *lastseq, u_int32_t window, u_int32_t *bitmap)
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
