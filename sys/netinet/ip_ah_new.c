/*	$OpenBSD: ip_ah_new.c,v 1.25 1999/03/24 17:00:44 niklas Exp $	*/

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

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <net/pfkeyv2.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

struct auth_hash ah_new_hash[] = {
     { SADB_AALG_MD5HMAC96, "HMAC-MD5-96", 
       MD5HMAC96_KEYSIZE, AH_MD5_ALEN,
       sizeof(MD5_CTX),
       (void (*)(void *)) MD5Init, 
       (void (*)(void *, u_int8_t *, u_int16_t)) MD5Update, 
       (void (*)(u_int8_t *, void *)) MD5Final 
     },
     { SADB_AALG_SHA1HMAC96, "HMAC-SHA1-96",
       SHA1HMAC96_KEYSIZE, AH_SHA1_ALEN,
       sizeof(SHA1_CTX),
       (void (*)(void *)) SHA1Init, 
       (void (*)(void *, u_int8_t *, u_int16_t)) SHA1Update, 
       (void (*)(u_int8_t *, void *)) SHA1Final 
     },
     { SADB_AALG_X_RIPEMD160HMAC96, "HMAC-RIPEMD-160-96",
       RIPEMD160HMAC96_KEYSIZE, AH_RMD160_ALEN,
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
    return 0;
}

/*
 * ah_new_init() is called when an SPI is being set up.
 */

int
ah_new_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
    struct auth_hash *thash = NULL;
    int i;

    for (i = sizeof(ah_new_hash) / sizeof(struct auth_hash) - 1;
	 i >= 0; i--) 
      if (ii->ii_authalg == ah_new_hash[i].type)
	break;

    if (i < 0) 
    {
	DPRINTF(("ah_new_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
	return EINVAL;
    }

    thash = &ah_new_hash[i];

    if (ii->ii_authkeylen != thash->keysize)
    {
	DPRINTF(("ah_new_init(): keylength %d doesn't match algorithm %s keysize (%d)\n", ii->ii_authkeylen, thash->name, thash->keysize));
	return EINVAL;
    }

    tdbp->tdb_xform = xsp;
    tdbp->tdb_authalgxform = thash;
    tdbp->tdb_bitmap = 0;
    tdbp->tdb_rpl = AH_HMAC_INITIAL_RPL;

    DPRINTF(("ah_new_init(): initialized TDB with hash algorithm %s\n",
	     thash->name));

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

    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */

    return 0;
}

/* Free memory */
int
ah_new_zeroize(struct tdb *tdbp)
{
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

/*
 * ah_new_input() gets called to verify that an input packet
 * passes authentication.
 */

struct mbuf *
ah_new_input(struct mbuf *m, struct tdb *tdb)
{
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
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
	DPRINTF(("ah_new_input(): bad authenticator length for packet from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(ah->ah_spi)));
	ahstat.ahs_badauthl++;
	m_freem(m);
	return NULL;
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += ip->ip_len - (ip->ip_hl << 2) -
			  ah->ah_hl * sizeof(u_int32_t);
    ahstat.ahs_ibytes += ip->ip_len - (ip->ip_hl << 2) -
			 ah->ah_hl * sizeof(u_int32_t);

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

    /* Replay window checking */
    if (tdb->tdb_wnd > 0)
    {
	btsx = ntohl(ah->ah_rpl);
	if ((errc = checkreplaywindow32(btsx, 0, &(tdb->tdb_rpl), tdb->tdb_wnd,
					&(tdb->tdb_bitmap))) != 0)
	{
	    switch(errc)
	    {
		case 1:
		    DPRINTF(("ah_new_input(): replay counter wrapped for packets from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(ah->ah_spi)));
		    ahstat.ahs_wrap++;
		    break;

		case 2:
	        case 3:
		    DPRINTF(("ah_new_input(): duplicate packet received from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(ah->ah_spi)));
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

    bcopy(tdb->tdb_ictx, &ctx, ahx->ctxsize);
    ahx->Update(&ctx, (unsigned char *) &ipo, sizeof(struct ip));

    /* Options */
    if ((ip->ip_hl << 2) > sizeof(struct ip))
      for (off = sizeof(struct ip); off < (ip->ip_hl << 2);)
      {
	  optval = ((u_int8_t *) ip)[off];
	  switch (optval)
	  {
	      case IPOPT_EOL:
		  ahx->Update(&ctx, ipseczeroes, 1);

		  off = ip->ip_hl << 2;
		  break;

	      case IPOPT_NOP:
		  ahx->Update(&ctx, ipseczeroes, 1);

		  off++;
		  break;

	      case IPOPT_SECURITY:
	      case 133:
	      case 134:
		  optval = ((u_int8_t *) ip)[off + 1];

		  ahx->Update(&ctx, (u_int8_t *) ip + off, optval);

		  off += optval;
		  break;

	      default:
		  optval = ((u_int8_t *) ip)[off + 1];

		  ahx->Update(&ctx, ipseczeroes, optval);

		  off += optval;
		  break;
	  }
      }

    ahx->Update(&ctx, (unsigned char *) ah, AH_NEW_FLENGTH - AH_HMAC_HASHLEN);
    ahx->Update(&ctx, ipseczeroes, AH_HMAC_HASHLEN);

    /*
     * Code shamelessly stolen from m_copydata
     */
    off = ohlen;
    len = m->m_pkthdr.len - off;
    m0 = m;
	
    while (off > 0)
    {
	if (m0 == 0)
	{
	    DPRINTF(("ah_new_input(): bad mbuf chain for packet from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(ah->ah_spi)));
	    ahstat.ahs_hdrops++;
	    m_freem(m);
	    return NULL;
	}

	if (off < m0->m_len)
	  break;

	off -= m0->m_len;
	m0 = m0->m_next;
    }

    while (len > 0)
    {
	if (m0 == 0)
	{
	    DPRINTF(("ah_new_input(): bad mbuf chain for packet from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(ah->ah_spi)));
	    ahstat.ahs_hdrops++;
	    m_freem(m);
	    return NULL;
	}

	count = min(m0->m_len - off, len);

	ahx->Update(&ctx, mtod(m0, unsigned char *) + off, count);

	len -= count;
	off = 0;
	m0 = m0->m_next;
    }

    ahx->Final((unsigned char *) (aho->ah_data), &ctx);
    bcopy(tdb->tdb_octx, &ctx, ahx->ctxsize);
    ahx->Update(&ctx, (unsigned char *) (aho->ah_data), ahx->hashsize);
    ahx->Final((unsigned char *) (aho->ah_data), &ctx);

    if (bcmp(aho->ah_data, ah->ah_data, AH_HMAC_HASHLEN))
    {
	DPRINTF(("ah_new_input(): authentication failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ip->ip_src), inet_ntoa4(ip->ip_dst), ntohl(ah->ah_spi)));
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

    return m;
}

int
ah_new_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb, 
	      struct mbuf **mp)
{
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
    struct ip *ip, ipo;
    struct ah_new aho, *ah;
    register int len, off, count, ilen;
    union {
	 MD5_CTX md5ctx;
	 SHA1_CTX sha1ctx;
	 RMD160_CTX rmd160ctx;
    } ctx;
    u_int8_t optval;
    u_char opts[40];

    ahstat.ahs_output++;
    m = m_pullup(m, sizeof(struct ip));
    if (m == NULL)
    {
	DPRINTF(("ah_new_output(): m_pullup() failed, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_hdrops++;
      	return ENOBUFS;
    }
	
    ip = mtod(m, struct ip *);
	
    if ((ip->ip_hl << 2) > sizeof(struct ip))
    {
        if ((m = m_pullup(m, ip->ip_hl << 2)) == NULL)
        {
            DPRINTF(("ah_new_output(): m_pullup() failed, SA %s/%08x\n",
		     ipsp_address(tdb->tdb_dst),
		     ntohl(tdb->tdb_spi)));
            ahstat.ahs_hdrops++;
            return ENOBUFS;
        }

        ip = mtod(m, struct ip *);
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2);
    ahstat.ahs_obytes += ntohs(ip->ip_len) - (ip->ip_hl << 2);

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
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;      /* Turn off checking */
    }

    /* Save options */
    m_copydata(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    ilen = ntohs(ip->ip_len);

    if (AH_NEW_FLENGTH + ilen > IP_MAXPACKET)
    {
	DPRINTF(("ah_new_output(): packet in SA %s/%08x got too big\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_toobig++;
        return EMSGSIZE;
    }

    ipo.ip_v = IPVERSION;
    ipo.ip_hl = ip->ip_hl;
    ipo.ip_tos = 0;
    ipo.ip_len = htons(AH_NEW_FLENGTH + ilen);
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

    if (tdb->tdb_rpl == 0)
    {
	DPRINTF(("ah_new_output(): SA %s/%08x should have expired\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_wrap++;
	return NULL;
    }

    aho.ah_rpl = htonl(tdb->tdb_rpl++);

    bcopy(tdb->tdb_ictx, (caddr_t)&ctx, ahx->ctxsize);
    ahx->Update(&ctx, (unsigned char *) &ipo, sizeof(struct ip));

    /* Options */
    if ((ip->ip_hl << 2) > sizeof(struct ip))
      for (off = sizeof(struct ip); off < (ip->ip_hl << 2);)
      {
          optval = ((u_int8_t *) ip)[off];
          switch (optval)
          {
              case IPOPT_EOL:
		  ahx->Update(&ctx, ipseczeroes, 1);

                  off = ip->ip_hl << 2;
                  break;

              case IPOPT_NOP:
                  ahx->Update(&ctx, ipseczeroes, 1);

                  off++;
                  break;

              case IPOPT_SECURITY:
              case 133:
              case 134:
                  optval = ((u_int8_t *) ip)[off + 1];

		  ahx->Update(&ctx, (u_int8_t *) ip + off, optval);

                  off += optval;
                  break;

              default:
                  optval = ((u_int8_t *) ip)[off + 1];

		  ahx->Update(&ctx, ipseczeroes, optval);

                  off += optval;
                  break;
          }
      }

    ahx->Update(&ctx, (unsigned char *) &aho, AH_NEW_FLENGTH);

    off = ip->ip_hl << 2;

    /*
     * Code shamelessly stolen from m_copydata
     */
    len = m->m_pkthdr.len - off;
	
    *mp = m;

    while (len > 0)
    {
	if ((*mp) == 0)
	{
	    DPRINTF(("ah_new_output(): bad mbuf chain for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	    ahstat.ahs_hdrops++;
	    m_freem(m);
	    return EMSGSIZE;
	}

	count = min((*mp)->m_len - off, len);

	ahx->Update(&ctx, mtod(*mp, unsigned char *) + off, count);

	len -= count;
	off = 0;
	*mp = (*mp)->m_next;
    }

    *mp = NULL;

    ipo.ip_tos = ip->ip_tos;
    ipo.ip_id = ip->ip_id;
    ipo.ip_off = ip->ip_off;
    ipo.ip_ttl = ip->ip_ttl;
/*  ipo.ip_len = ntohs(ipo.ip_len); */
	
    M_PREPEND(m, AH_NEW_FLENGTH, M_DONTWAIT);
    if (m == NULL)
    {
        DPRINTF(("ah_new_output(): M_PREPEND() failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }

    m = m_pullup(m, AH_NEW_FLENGTH + (ipo.ip_hl << 2));
    if (m == NULL)
    {
	DPRINTF(("ah_new_output(): m_pullup() failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_hdrops++;
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

    /* Restore the options */
    bcopy(opts, (caddr_t) (ip + 1), (ip->ip_hl << 2) - sizeof(struct ip));

    /* Finish computing the authenticator */
    ahx->Final(opts, &ctx);
    bcopy(tdb->tdb_octx, &ctx, ahx->ctxsize);
    ahx->Update(&ctx, opts, ahx->hashsize);
    ahx->Final(opts, &ctx);

    /* Copy the authenticator */
    bcopy(opts, ah->ah_data, AH_HMAC_HASHLEN);

    *mp = m;
	
    return 0;
}
