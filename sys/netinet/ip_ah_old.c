/*	$OpenBSD: ip_ah_old.c,v 1.13 1998/03/18 10:16:27 provos Exp $	*/

/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Additional transforms and features in 1997 by Angelos D. Keromytis and
 * Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
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
 * Authentication Header Processing
 * Per RFCs 1828/1852 (Metzger & Simpson)
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
#include <netinet/ip_icmp.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>
#include <net/encap.h>

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <sys/syslog.h>

extern void encap_sendnotify(int, struct tdb *);

struct ah_hash ah_old_hash[] = {
     { ALG_AUTH_MD5, "Keyed MD5", 
       AH_MD5_ALEN,
       sizeof(MD5_CTX),
       (void (*)(void *))MD5Init, 
       (void (*)(void *, u_int8_t *, u_int16_t))MD5Update, 
       (void (*)(u_int8_t *, void *))MD5Final 
     },
     { ALG_AUTH_SHA1, "Keyed SHA1",
       AH_SHA1_ALEN,
       sizeof(SHA1_CTX),
       (void (*)(void *))SHA1Init, 
       (void (*)(void *, u_int8_t *, u_int16_t))SHA1Update, 
       (void (*)(u_int8_t *, void *))SHA1Final 
     }
};

/*
 * ah_old_attach() is called from the transformation initialization code.
 */

int
ah_old_attach()
{
#ifdef ENCDEBUG
    if (encdebug)
      printf("ah_old_attach(): setting up\n");
#endif /* ENCDEBUG */
    return 0;
}

/*
 * ah_old_init() is called when an SPI is being set up. It interprets the
 * encap_msghdr present in m, and sets up the transformation data.
 */

int
ah_old_init(struct tdb *tdbp, struct xformsw *xsp, struct mbuf *m)
{
    struct ah_old_xencap xenc;
    struct ah_old_xdata *xd;
    struct encap_msghdr *em;
    struct ah_hash *thash;
    int i;

    if (m->m_len < ENCAP_MSG_FIXED_LEN)
    {
        if ((m = m_pullup(m, ENCAP_MSG_FIXED_LEN)) == NULL)
        {
#ifdef ENCDEBUG
            if (encdebug)
              printf("ah_old_init(): m_pullup failed\n");
#endif /* ENCDEBUG */
            return ENOBUFS;
        }
    }

    em = mtod(m, struct encap_msghdr *);
    if (em->em_msglen - EMT_SETSPI_FLEN <= AH_OLD_XENCAP_LEN)
    {
	if (encdebug)
	  log(LOG_WARNING, "ah_old_init(): initialization failed\n");
	return EINVAL;
    }

    /* Just copy the standard fields */
    m_copydata(m, EMT_SETSPI_FLEN, AH_OLD_XENCAP_LEN, (caddr_t) &xenc);

    /* Check whether the hash algorithm is supported */
    for (i=sizeof(ah_old_hash)/sizeof(struct ah_hash)-1; i >= 0; i--) 
	if (xenc.amx_hash_algorithm == ah_old_hash[i].type)
	      break;
    if (i < 0) 
    {
	if (encdebug)
	  log(LOG_WARNING, "ah_old_init(): unsupported authentication algorithm %d specified\n",
	      xenc.amx_hash_algorithm);
	m_freem(m);
	return EINVAL;
    }
#ifdef ENCDEBUG
    if (encdebug)
      printf("ah_old_init(): initalized TDB with hash algorithm %d: %s\n",
	     xenc.amx_hash_algorithm, ah_old_hash[i].name);
#endif /* ENCDEBUG */
    thash = &ah_old_hash[i];

    if (xenc.amx_keylen + EMT_SETSPI_FLEN + AH_OLD_XENCAP_LEN != em->em_msglen)
    {
	if (encdebug)
	  log(LOG_WARNING, "ah_old_init(): message length (%d) doesn't match\n",
	    em->em_msglen);
	return EINVAL;
    }

    MALLOC(tdbp->tdb_xdata, caddr_t, sizeof(struct ah_old_xdata) +
	   xenc.amx_keylen, M_XDATA, M_WAITOK);
    if (tdbp->tdb_xdata == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("ah_old_init(): MALLOC() failed\n");
#endif /* ENCDEBUG */
      	return ENOBUFS;
    }

    bzero(tdbp->tdb_xdata, sizeof(struct ah_old_xdata) + xenc.amx_keylen);
    xd = (struct ah_old_xdata *) tdbp->tdb_xdata;

    /* Pointer to the transform */
    tdbp->tdb_xform = xsp;

    xd->amx_keylen = xenc.amx_keylen;
    xd->amx_hash_algorithm = xenc.amx_hash_algorithm;
    xd->amx_hash = thash;

    /* Pass name of auth algorithm for kernfs */
    tdbp->tdb_authname = xd->amx_hash->name;

    /* Copy the key material */
    m_copydata(m, EMT_SETSPI_FLEN + AH_OLD_XENCAP_LEN, xd->amx_keylen,
	       (caddr_t) xd->amx_key);

    xd->amx_hash->Init(&(xd->amx_ctx));
    xd->amx_hash->Update(&(xd->amx_ctx), xd->amx_key, xd->amx_keylen);
    xd->amx_hash->Final(NULL, &(xd->amx_ctx));

    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */

    return 0;
}

/*
 * Free memory
 */

int
ah_old_zeroize(struct tdb *tdbp)
{
#ifdef ENCDEBUG
    if (encdebug)
      printf("ah_old_zeroize(): freeing memory\n");
#endif /* ENCDEBUG */
    if (tdbp->tdb_xdata)
    {
    	FREE(tdbp->tdb_xdata, M_XDATA);
	tdbp->tdb_xdata = NULL;
    }
    return 0;
}

/*
 * ah_old_input() gets called to verify that an input packet
 * passes authentication.
 */

struct mbuf *
ah_old_input(struct mbuf *m, struct tdb *tdb)
{
    struct ah_old_xdata *xd;
    struct ip *ip, ipo;
    struct ah_old *ah, *aho;
    int ohlen, len, count, off, alen;
    struct mbuf *m0;
    union {
	 MD5_CTX md5ctx; 
	 SHA1_CTX sha1ctx;
    } ctx;
    u_int8_t optval;
    u_char buffer[40];

    aho = (struct ah_old *) buffer;

    xd = (struct ah_old_xdata *) tdb->tdb_xdata;

    alen = xd->amx_hash->hashsize;

    ohlen = sizeof(struct ip) + AH_OLD_FLENGTH + alen;

    if (m->m_len < ohlen)
    {
	if ((m = m_pullup(m, ohlen)) == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("ah_old_input(): m_pullup() failed\n");
#endif /* ENCDEBUG */
	    ahstat.ahs_hdrops++;
	    return NULL;
	}
    }

    ip = mtod(m, struct ip *);

    if ((ip->ip_hl << 2) > sizeof(struct ip))
    {
	if ((m = m_pullup(m, ohlen - sizeof (struct ip) +
			  (ip->ip_hl << 2))) == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("ah_old_input(): m_pullup() failed\n");
#endif /* ENCDEBUG */
	    ahstat.ahs_hdrops++;
	    return NULL;
	}
	
	ip = mtod(m, struct ip *);
	ah = (struct ah_old *)((u_int8_t *) ip + (ip->ip_hl << 2));
	ohlen += ((ip->ip_hl << 2) - sizeof(struct ip));
    }
    else
      ah = (struct ah_old *) (ip + 1);

    ipo = *ip;
    ipo.ip_tos = 0;
    ipo.ip_len += (ip->ip_hl << 2);	/* adjusted in ip_intr() */
    HTONS(ipo.ip_len);
    HTONS(ipo.ip_id);
    ipo.ip_off = htons(ipo.ip_off & IP_DF);	/* XXX -- and the C bit? */
    ipo.ip_ttl = 0;
    ipo.ip_sum = 0;

    bcopy(&(xd->amx_ctx), &ctx, xd->amx_hash->ctxsize);
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
    
    
    xd->amx_hash->Update(&ctx, (unsigned char *) ah, AH_OLD_FLENGTH);
    xd->amx_hash->Update(&ctx, ipseczeroes, alen);

    /*
     * Code shamelessly stolen from m_copydata
     */
    off = ohlen;
    len = m->m_pkthdr.len - off;
    m0 = m;
	
    while (off > 0)
    {
	if (m0 == 0)
	  panic("ah_old_input(): m_copydata (off)");

	if (off < m0->m_len)
	  break;

	off -= m0->m_len;
	m0 = m0->m_next;
    }

    while (len > 0)
    {
	if (m0 == 0)
	  panic("ah_old_input(): m_copydata (copy)");

	count = min(m0->m_len - off, len);

	xd->amx_hash->Update(&ctx, mtod(m0, unsigned char *) + off, count);

	len -= count;
	off = 0;
	m0 = m0->m_next;
    }

    xd->amx_hash->Update(&ctx, (unsigned char *) xd->amx_key, xd->amx_keylen);
    xd->amx_hash->Final((unsigned char *) (aho->ah_data), &ctx);

    if (bcmp(aho->ah_data, ah->ah_data, alen))
    {
	if (encdebug)
	  log(LOG_ALERT, "ah_old_input(): authentication failed for packet from %x to %x, spi %08x\n", ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi));
	ahstat.ahs_badauth++;
	m_freem(m);
	return NULL;
    }
	
    ipo = *ip;
    ipo.ip_p = ah->ah_nh;

    /* Save options */
    m_copydata(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) buffer);

    m->m_len -= (AH_OLD_FLENGTH + alen);
    m->m_data += (AH_OLD_FLENGTH + alen);
    m->m_pkthdr.len -= (AH_OLD_FLENGTH + alen);

    ip = mtod(m, struct ip *);
    *ip = ipo;
    ip->ip_len = htons(ip->ip_len - AH_OLD_FLENGTH - alen + (ip->ip_hl << 2));
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
    }

    if (tdb->tdb_flags & TDBF_PACKETS)
    {
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
    }

    return m;
}

int
ah_old_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
  	      struct mbuf **mp)
{
    struct ah_old_xdata *xd;
    struct ip *ip, ipo;
    struct ah_old *ah, aho;
    register int len, off, count;
    register struct mbuf *m0;
    union {
	 MD5_CTX md5ctx;
	 SHA1_CTX sha1ctx;
    } ctx;
    int ilen, ohlen, alen;
    u_int8_t optval;
    u_char opts[40];

    ahstat.ahs_output++;
    m = m_pullup(m, sizeof(struct ip));
    if (m == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("ah_old_output(): m_pullup() failed, SA %x/%08x\n",
		 tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
      	return ENOBUFS;
    }

    ip = mtod(m, struct ip *);
	
    xd = (struct ah_old_xdata *) tdb->tdb_xdata;

    if ((ip->ip_hl << 2) > sizeof(struct ip))
    {
	if ((m = m_pullup(m, ip->ip_hl << 2)) == NULL)
	{
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("ah_old_output(): m_pullup() failed, SA &x/%08x\n",
		     tdb->tdb_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
	    ahstat.ahs_hdrops++;
	    return NULL;
	}
	
	ip = mtod(m, struct ip *);
    }

    alen = xd->amx_hash->hashsize;

    /* Save the options */
    m_copydata(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    ilen = ntohs(ip->ip_len);

    ohlen = AH_OLD_FLENGTH + alen;
	
    ipo.ip_v = IPVERSION;
    ipo.ip_hl = ip->ip_hl;
    ipo.ip_tos = 0;
    ipo.ip_len = htons(ohlen + ilen);
    ipo.ip_id = ip->ip_id;
    ipo.ip_off = htons(ntohs(ip->ip_off) & IP_DF);
    ipo.ip_ttl = 0;
    ipo.ip_p = IPPROTO_AH;
    ipo.ip_sum = 0;
    ipo.ip_src = ip->ip_src;
    ipo.ip_dst = ip->ip_dst;

    aho.ah_nh = ip->ip_p;
    aho.ah_hl = alen >> 2;
    aho.ah_rv = 0;
    aho.ah_spi = tdb->tdb_spi;

    bcopy(&(xd->amx_ctx), &ctx, xd->amx_hash->ctxsize);
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

    xd->amx_hash->Update(&ctx, (unsigned char *) &aho, AH_OLD_FLENGTH);
    xd->amx_hash->Update(&ctx, ipseczeroes, alen);

    /* Skip the IP header and any options */
    off = ip->ip_hl << 2;

    /*
     * Code shamelessly stolen from m_copydata
     */
    len = m->m_pkthdr.len - off;
	
    m0 = m;

    while (len > 0)
    {
	if (m0 == 0)
	  panic("ah_old_output(): m_copydata()");
	count = min(m0->m_len - off, len);

	xd->amx_hash->Update(&ctx, mtod(m0, unsigned char *) + off, count);

	len -= count;
	off = 0;
	m0 = m0->m_next;
    }

    xd->amx_hash->Update(&ctx, (unsigned char *) xd->amx_key, xd->amx_keylen);

    ipo.ip_tos = ip->ip_tos;
    ipo.ip_id = ip->ip_id;
    ipo.ip_off = ip->ip_off;
    ipo.ip_ttl = ip->ip_ttl;
/*  ipo.ip_len = ntohs(ipo.ip_len); */
	
    M_PREPEND(m, ohlen, M_DONTWAIT);
    if (m == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("ah_old_output(): M_PREPEND() failed for packet from %x to %x, spi %08x\n", ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
        return ENOBUFS;
    }

    m = m_pullup(m, ohlen + (ipo.ip_hl << 2));
    if (m == NULL)
    {
#ifdef ENCDEBUG
	if (encdebug)
	  printf("ah_old_output(): m_pullup() failed for packet from %x to %x, spi %08x\n", ipo.ip_src, ipo.ip_dst, ntohl(tdb->tdb_spi));
#endif /* ENCDEBUG */
        return ENOBUFS;
    }

    ip = mtod(m, struct ip *);
    ah = (struct ah_old *) ((u_int8_t *) ip + (ipo.ip_hl << 2));
    *ip = ipo;
    ah->ah_nh = aho.ah_nh;
    ah->ah_hl = aho.ah_hl;
    ah->ah_rv = aho.ah_rv;
    ah->ah_spi = aho.ah_spi;

    /* Restore the options */
    m_copyback(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    xd->amx_hash->Final(ah->ah_data, &ctx);

    *mp = m;

    /* Update the counters */
    tdb->tdb_cur_packets++;
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) -
			  AH_OLD_FLENGTH - alen;
    ahstat.ahs_obytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) -
			 AH_OLD_FLENGTH - alen;

    /* Notify on expiration */
    if (tdb->tdb_flags & TDBF_SOFT_PACKETS)
    {
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
    }

    if (tdb->tdb_flags & TDBF_PACKETS)
    {
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
    }

    return 0;
}
