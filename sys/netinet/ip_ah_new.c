/*	$OpenBSD: ip_ah_new.c,v 1.19 1998/11/25 02:01:27 niklas Exp $	*/

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
 * Based on RFC 2085.
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
#include <sys/socketvar.h>

#include <machine/cpu.h>
#include <machine/endian.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <net/encap.h>

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <sys/syslog.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

extern void encap_sendnotify(int, struct tdb *, void *);

struct ah_hash ah_new_hash[] = {
     { ALG_AUTH_MD5, "HMAC-MD5-96", 
       AH_MD5_ALEN,
       sizeof(MD5_CTX),
       (void (*)(void *)) MD5Init, 
       (void (*)(void *, u_int8_t *, u_int16_t)) MD5Update, 
       (void (*)(u_int8_t *, void *)) MD5Final 
     },
     { ALG_AUTH_SHA1, "HMAC-SHA1-96",
       AH_SHA1_ALEN,
       sizeof(SHA1_CTX),
       (void (*)(void *)) SHA1Init, 
       (void (*)(void *, u_int8_t *, u_int16_t)) SHA1Update, 
       (void (*)(u_int8_t *, void *)) SHA1Final 
     },
     { ALG_AUTH_RMD160, "HMAC-RIPEMD-160-96",
       AH_RMD160_ALEN,
       sizeof(RMD160_CTX),
       (void (*)(void *)) RMD160Init, 
       (void (*)(void *, u_int8_t *, u_int16_t)) RMD160Update, 
       (void (*)(u_int8_t *, void *)) RMD160Final 
     }
};

/*
 * ah_new_attach() is called from the transformation initialization code.
 * It just returns.
 */

int
ah_new_attach()
{
    DPRINTF(("ah_new_attach(): setting up\n"));
    return 0;
}

/*
 * ah_new_init() is called when an SPI is being set up. It interprets the
 * encap_msghdr present in m, and sets up the transformation data.
 */

int
ah_new_init(struct tdb *tdbp, struct xformsw *xsp, struct mbuf *m)
{
    struct ah_new_xdata *xd;
    struct ah_new_xencap txd;
    struct encap_msghdr *em;
    struct ah_hash *thash;
    caddr_t buffer = NULL;
    int blocklen, i;

    if (m->m_len < ENCAP_MSG_FIXED_LEN)
    {
        if ((m = m_pullup(m, ENCAP_MSG_FIXED_LEN)) == NULL)
        {
	    DPRINTF(("ah_new_init(): m_pullup failed\n"));
            return ENOBUFS;
        }
    }

    em = mtod(m, struct encap_msghdr *);
    if (em->em_msglen - EMT_SETSPI_FLEN <= AH_NEW_XENCAP_LEN)
    {
	if (encdebug)
	  log(LOG_WARNING, "ah_new_init() initialization failed\n");
	return EINVAL;
    }

    /* Just copy the standard fields */
    m_copydata(m, EMT_SETSPI_FLEN, AH_NEW_XENCAP_LEN, (caddr_t) &txd);

    /* Check whether the hash algorithm is supported */
    for (i = sizeof(ah_new_hash) / sizeof(struct ah_hash) - 1; i >= 0; i--) 
	if (txd.amx_hash_algorithm == ah_new_hash[i].type)
	      break;
    if (i < 0) 
    {
	if (encdebug)
	  log(LOG_WARNING, "ah_new_init(): unsupported authentication algorithm %d specified\n", txd.amx_hash_algorithm);
	return EINVAL;
    }
    DPRINTF(("ah_new_init(): initalized TDB with hash algorithm %d: %s\n",
	     txd.amx_hash_algorithm, ah_new_hash[i].name));
    thash = &ah_new_hash[i];
    blocklen = HMAC_BLOCK_LEN;

    if (txd.amx_keylen + EMT_SETSPI_FLEN + AH_NEW_XENCAP_LEN != em->em_msglen)
    {
	if (encdebug)
	  log(LOG_WARNING, "ah_new_init(): message length (%d) doesn't match\n",
	      em->em_msglen);
	return EINVAL;
    }

    MALLOC(tdbp->tdb_xdata, caddr_t, sizeof(struct ah_new_xdata),
	   M_XDATA, M_WAITOK);
    if (tdbp->tdb_xdata == NULL)
    {
	DPRINTF(("ah_new_init(): MALLOC failed\n"));
      	return ENOBUFS;
    }

    MALLOC(buffer, caddr_t,
	   (txd.amx_keylen < blocklen ? blocklen : txd.amx_keylen),
	   M_TEMP, M_WAITOK);
    if (buffer == NULL)
    {
        DPRINTF(("ah_new_init(): MALLOC failed\n"));
	free(tdbp->tdb_xdata, M_XDATA);
        return ENOBUFS;
    }

    bzero(buffer, (txd.amx_keylen < blocklen ? blocklen : txd.amx_keylen));
    bzero(tdbp->tdb_xdata, sizeof(struct ah_new_xdata));
    xd = (struct ah_new_xdata *) tdbp->tdb_xdata;

    /* Copy the key to the buffer */
    m_copydata(m, EMT_SETSPI_FLEN + AH_NEW_XENCAP_LEN, txd.amx_keylen, buffer);

    xd->amx_hash = thash;
    /* Shorten the key if necessary */
    if (txd.amx_keylen > blocklen)
    {
	xd->amx_hash->Init(&(xd->amx_ictx));
	xd->amx_hash->Update(&(xd->amx_ictx), buffer, txd.amx_keylen);
	bzero(buffer,
	      (txd.amx_keylen < blocklen ? blocklen : txd.amx_keylen));
	xd->amx_hash->Final(buffer, &(xd->amx_ictx));
    }

    /* Pointer to the transform */
    tdbp->tdb_xform = xsp;

    /* Pass name of auth algorithm for kernfs */
    tdbp->tdb_authname = xd->amx_hash->name;

    xd->amx_hash_algorithm = txd.amx_hash_algorithm;
    xd->amx_rpl = AH_HMAC_INITIAL_RPL;
    xd->amx_wnd = txd.amx_wnd;
    xd->amx_bitmap = 0;

    /* Precompute the I and O pads of the HMAC */
    for (i = 0; i < blocklen; i++)
      buffer[i] ^= HMAC_IPAD_VAL;

    xd->amx_hash->Init(&(xd->amx_ictx));
    xd->amx_hash->Update(&(xd->amx_ictx), buffer, blocklen);

    for (i = 0; i < blocklen; i++)
      buffer[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

    xd->amx_hash->Init(&(xd->amx_octx));
    xd->amx_hash->Update(&(xd->amx_octx), buffer, blocklen);

    bzero(buffer, blocklen);			/* paranoid */
    free(buffer, M_TEMP);

    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */

    return 0;
}

/* Free memory */
int
ah_new_zeroize(struct tdb *tdbp)
{
    DPRINTF(("ah_new_zeroize(): freeing memory\n"));
    if (tdbp->tdb_xdata)
    {
    	FREE(tdbp->tdb_xdata, M_XDATA);
	tdbp->tdb_xdata = NULL;
    }
    return 0;
}

/*
 * ah_new_input() gets called to verify that an input packet
 * passes authentication.
 */

struct mbuf *
ah_new_input(struct mbuf *m, struct tdb *tdb)
{
    struct ah_new_xdata *xd;
    struct ip *ip, ipo;
    struct ah_new *aho, *ah;
    int ohlen, len, count, off, errc;
    u_int32_t btsx;
    struct mbuf *m0;
    union {
	 MD5_CTX md5ctx; 
	 SHA1_CTX sha1ctx;
	 RMD160_CTX rmd160ctx;
    } ctx;
    u_int8_t optval;
    u_char buffer[40];

    aho = (struct ah_new *) buffer;

    xd = (struct ah_new_xdata *) tdb->tdb_xdata;

    ohlen = sizeof(struct ip) + AH_NEW_FLENGTH;

    if (m->m_len < ohlen)
    {
	if ((m = m_pullup(m, ohlen)) == NULL)
	{
	    ahstat.ahs_hdrops++;
	    DPRINTF(("ah_new_input(): (possibly too short) packet dropped\n"));
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);

    /* Adjust, if options are present */
    if ((ip->ip_hl << 2) > sizeof(struct ip))
    {
	if ((m = m_pullup(m, ohlen - sizeof (struct ip) +
			  (ip->ip_hl << 2))) == NULL)
	{
	    DPRINTF(("ah_new_input(): m_pullup() failed\n"));
	    ahstat.ahs_hdrops++;
	    return NULL;
	}
	
	ip = mtod(m, struct ip *);
	ah = (struct ah_new *) ((u_int8_t *) ip + (ip->ip_hl << 2));
	ohlen += ((ip->ip_hl << 2) - sizeof(struct ip));
    }
    else
      ah = (struct ah_new *) (ip + 1);

    if (ah->ah_hl * sizeof(u_int32_t) != AH_HMAC_HASHLEN + AH_HMAC_RPLENGTH)
    {
	DPRINTF(("ah_new_input(): bad authenticator length for packet from %x to %x, spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(ah->ah_spi)));
	ahstat.ahs_badauthl++;
	m_freem(m);
	return NULL;
    }

    /* Replay window checking */
    if (xd->amx_wnd >= 0)
    {
	btsx = ntohl(ah->ah_rpl);
	if ((errc = checkreplaywindow32(btsx, 0, &(xd->amx_rpl), xd->amx_wnd,
					&(xd->amx_bitmap))) != 0)
	{
	    switch(errc)
	    {
		case 1:
		    if (encdebug)
		      log(LOG_ERR, "ah_new_input(): replay counter wrapped for packets from %x to %x, spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(ah->ah_spi));
		    ahstat.ahs_wrap++;
		    break;

		case 2:
	        case 3:
		    if (encdebug)
		      log(LOG_WARNING, "ah_new_input(): duplicate packet received, %x->%x spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(ah->ah_spi));
		    ahstat.ahs_replay++;
		    break;
	    }

	    m_freem(m);
	    return NULL;
	}
    }

    ipo = *ip;
    ipo.ip_tos = 0;
    ipo.ip_len += (ip->ip_hl << 2);     /* adjusted in ip_intr() */
    HTONS(ipo.ip_len);
    HTONS(ipo.ip_id);
    ipo.ip_off = 0;
    ipo.ip_ttl = 0;
    ipo.ip_sum = 0;

    bcopy(&(xd->amx_ictx), &ctx, xd->amx_hash->ctxsize);
    xd->amx_hash->Update(&ctx, (unsigned char *) &ipo, sizeof(struct ip));

    /* Options */
    if ((ip->ip_hl << 2) > sizeof(struct ip))
      for (off = sizeof(struct ip); off < (ip->ip_hl << 2);)
      {
	  optval = ((u_int8_t *) ip)[off];
	  switch (optval)
	  {
	      case IPOPT_EOL:
		  xd->amx_hash->Update(&ctx, ipseczeroes, 1);

		  off = ip->ip_hl << 2;
		  break;

	      case IPOPT_NOP:
		  xd->amx_hash->Update(&ctx, ipseczeroes, 1);

		  off++;
		  break;

	      case IPOPT_SECURITY:
	      case 133:
	      case 134:
		  optval = ((u_int8_t *) ip)[off + 1];

		  xd->amx_hash->Update(&ctx, (u_int8_t *) ip + off, optval);

		  off += optval;
		  break;

	      default:
		  optval = ((u_int8_t *) ip)[off + 1];

		  xd->amx_hash->Update(&ctx, ipseczeroes, optval);

		  off += optval;
		  break;
	  }
      }

    xd->amx_hash->Update(&ctx, (unsigned char *) ah, AH_NEW_FLENGTH -
			 AH_HMAC_HASHLEN);
    xd->amx_hash->Update(&ctx, ipseczeroes, AH_HMAC_HASHLEN);

    /*
     * Code shamelessly stolen from m_copydata
     */
    off = ohlen;
    len = m->m_pkthdr.len - off;
    m0 = m;
	
    while (off > 0)
    {
	if (m0 == 0)
	  panic("ah_new_input(): m_copydata (off)");

	if (off < m0->m_len)
	  break;

	off -= m0->m_len;
	m0 = m0->m_next;
    }

    while (len > 0)
    {
	if (m0 == 0)
	  panic("ah_new_input(): m_copydata (copy)");

	count = min(m0->m_len - off, len);

	xd->amx_hash->Update(&ctx, mtod(m0, unsigned char *) + off, count);

	len -= count;
	off = 0;
	m0 = m0->m_next;
    }

    xd->amx_hash->Final((unsigned char *) (aho->ah_data), &ctx);
    bcopy(&(xd->amx_octx), &ctx, xd->amx_hash->ctxsize);
    xd->amx_hash->Update(&ctx, (unsigned char *) (aho->ah_data),
			 xd->amx_hash->hashsize);
    xd->amx_hash->Final((unsigned char *) (aho->ah_data), &ctx);

    if (bcmp(aho->ah_data, ah->ah_data, AH_HMAC_HASHLEN))
    {
	if (encdebug)
	  log(LOG_ALERT, "ah_new_input(): authentication failed for packet from %x to %x, spi %08x\n", ip->ip_src, ip->ip_dst, ntohl(ah->ah_spi));
#ifdef ENCDEBUG
	if (encdebug)
	{
	    printf("Received authenticator: ");
	    for (off = 0; off < AH_HMAC_HASHLEN; off++)
	      printf("%02x ", ah->ah_data[off]);
	    printf("\n");

	    printf("Computed authenticator: ");
	    for (off = 0; off < AH_HMAC_HASHLEN; off++)
	      printf("%02x ", aho->ah_data[off]);
	    printf("\n");
	}
#endif
	ahstat.ahs_badauth++;
	m_freem(m);
	return NULL;
    }
	
    ipo = *ip;
    ipo.ip_p = ah->ah_nh;

    /* Save options */
    m_copydata(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) buffer);

    m->m_len -= AH_NEW_FLENGTH;
    m->m_data += AH_NEW_FLENGTH;
    m->m_pkthdr.len -= AH_NEW_FLENGTH;

    ip = mtod(m, struct ip *);
    *ip = ipo;
    ip->ip_len = htons(ip->ip_len - AH_NEW_FLENGTH + (ip->ip_hl << 2));
    HTONS(ip->ip_id);
    HTONS(ip->ip_off);
    ip->ip_sum = 0;

    /* Copy the options back */
    m_copyback(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) buffer);

    ip->ip_sum = in_cksum(m, (ip->ip_hl << 2));

    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2);
    ahstat.ahs_ibytes += ntohs(ip->ip_len) - (ip->ip_hl << 2);

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
ah_new_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb, 
	      struct mbuf **mp)
{
    struct ah_new_xdata *xd;
    struct ip *ip, ipo;
    struct ah_new aho, *ah;
    register int len, off, count;
    register struct mbuf *m0;
    union {
	 MD5_CTX md5ctx;
	 SHA1_CTX sha1ctx;
	 RMD160_CTX rmd160ctx;
    } ctx;
    int ilen, ohlen;
    u_int8_t optval;
    u_char buffer[AH_ALEN_MAX], opts[40];

    ahstat.ahs_output++;
    m = m_pullup(m, sizeof(struct ip));
    if (m == NULL)
    {
	DPRINTF(("ah_new_output(): m_pullup() failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi)));
      	return ENOBUFS;
    }
	
    ip = mtod(m, struct ip *);
	
    xd = (struct ah_new_xdata *) tdb->tdb_xdata;

    if ((ip->ip_hl << 2) > sizeof(struct ip))
    {
        if ((m = m_pullup(m, ip->ip_hl << 2)) == NULL)
        {
            DPRINTF(("ah_new_output(): m_pullup() failed, SA &x/%08x\n",
		     tdb->tdb_dst, ntohl(tdb->tdb_spi)));
            ahstat.ahs_hdrops++;
            return ENOBUFS;
        }

        ip = mtod(m, struct ip *);
    }

    /* Save options */
    m_copydata(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    DPRINTF(("ah_new_output(): using hash algorithm %s\n", xd->amx_hash->name));

    ilen = ntohs(ip->ip_len);

    ohlen = AH_NEW_FLENGTH;
    if (ohlen + ilen > IP_MAXPACKET) {
	if (encdebug)
            log(LOG_ALERT,
		"ah_new_output(): packet in SA %x/%0x8 got too big\n",
		tdb->tdb_dst, ntohl(tdb->tdb_spi));
	m_freem(m);
	ahstat.ahs_toobig++;
        return ENOBUFS;
    }

    ipo.ip_v = IPVERSION;
    ipo.ip_hl = ip->ip_hl;
    ipo.ip_tos = 0;
    ipo.ip_len = htons(ohlen + ilen);
    ipo.ip_id = ip->ip_id;
    ipo.ip_off = 0;
    ipo.ip_ttl = 0;
    ipo.ip_p = IPPROTO_AH;
    ipo.ip_sum = 0;
    ipo.ip_src = ip->ip_src;
    ipo.ip_dst = ip->ip_dst;

    bzero(&aho, sizeof(struct ah_new));

    aho.ah_nh = ip->ip_p;
    aho.ah_hl = ((AH_HMAC_RPLENGTH + AH_HMAC_HASHLEN) >> 2);
    aho.ah_rv = 0;
    aho.ah_spi = tdb->tdb_spi;

    if (xd->amx_rpl == 0)
    {
	if (encdebug)
          log(LOG_ALERT, "ah_new_output(): SA %x/%0x8 should have expired\n",
	      tdb->tdb_dst, ntohl(tdb->tdb_spi));
	m_freem(m);
	ahstat.ahs_wrap++;
	return NULL;
    }

    aho.ah_rpl = htonl(xd->amx_rpl++);

    bcopy((caddr_t)&(xd->amx_ictx), (caddr_t)&ctx, xd->amx_hash->ctxsize);
    xd->amx_hash->Update(&ctx, (unsigned char *) &ipo, sizeof(struct ip));

    /* Options */
    if ((ip->ip_hl << 2) > sizeof(struct ip))
      for (off = sizeof(struct ip); off < (ip->ip_hl << 2);)
      {
          optval = ((u_int8_t *) ip)[off];
          switch (optval)
          {
              case IPOPT_EOL:
		  xd->amx_hash->Update(&ctx, ipseczeroes, 1);

                  off = ip->ip_hl << 2;
                  break;

              case IPOPT_NOP:
                  xd->amx_hash->Update(&ctx, ipseczeroes, 1);

                  off++;
                  break;

              case IPOPT_SECURITY:
              case 133:
              case 134:
                  optval = ((u_int8_t *) ip)[off + 1];

		  xd->amx_hash->Update(&ctx, (u_int8_t *) ip + off, optval);

                  off += optval;
                  break;

              default:
                  optval = ((u_int8_t *) ip)[off + 1];

		  xd->amx_hash->Update(&ctx, ipseczeroes, optval);

                  off += optval;
                  break;
          }
      }

    xd->amx_hash->Update(&ctx, (unsigned char *) &aho, AH_NEW_FLENGTH);

    off = ip->ip_hl << 2;

    /*
     * Code shamelessly stolen from m_copydata
     */
    len = m->m_pkthdr.len - off;
	
    m0 = m;

    while (len > 0)
    {
	if (m0 == 0)
	  panic("ah_new_output(): m_copydata");
	count = min(m0->m_len - off, len);

	xd->amx_hash->Update(&ctx, mtod(m0, unsigned char *) + off, count);

	len -= count;
	off = 0;
	m0 = m0->m_next;
    }

    ipo.ip_tos = ip->ip_tos;
    ipo.ip_id = ip->ip_id;
    ipo.ip_off = ip->ip_off;
    ipo.ip_ttl = ip->ip_ttl;
/*  ipo.ip_len = ntohs(ipo.ip_len); */
	
    M_PREPEND(m, ohlen, M_DONTWAIT);
    if (m == NULL)
    {
        DPRINTF(("ah_new_output(): M_PREPEND() failed for packet from %x to %x, spi %08x\n", ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }

    m = m_pullup(m, ohlen + (ipo.ip_hl << 2));
    if (m == NULL)
    {
	DPRINTF(("ah_new_output(): m_pullup() failed for packet from %x to %x, spi %08x\n", ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }
	
    ip = mtod(m, struct ip *);
    ah = (struct ah_new *) ((u_int8_t *) ip + (ipo.ip_hl << 2));
    *ip = ipo;
    ah->ah_nh = aho.ah_nh;
    ah->ah_hl = aho.ah_hl;
    ah->ah_rv = aho.ah_rv;
    ah->ah_spi = aho.ah_spi;
    ah->ah_rpl = aho.ah_rpl;

    xd->amx_hash->Final(buffer, &ctx);
    bcopy(&(xd->amx_octx), &ctx, xd->amx_hash->ctxsize);
    xd->amx_hash->Update(&ctx, buffer, xd->amx_hash->hashsize);
    xd->amx_hash->Final(buffer, &ctx);

    /* Restore the options */
    m_copyback(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    /* Copy the authenticator */
    bcopy(buffer, ah->ah_data, AH_HMAC_HASHLEN);

    *mp = m;
	
    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) - 
			  AH_NEW_FLENGTH;
    ahstat.ahs_obytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) - AH_NEW_FLENGTH;

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
