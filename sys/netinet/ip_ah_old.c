/*	$OpenBSD: ip_ah_old.c,v 1.21 1999/02/25 20:14:38 angelos Exp $	*/

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

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <net/pfkeyv2.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

struct auth_hash ah_old_hash[] = {
     { SADB_AALG_X_MD5, "Keyed MD5", 
       0, AH_MD5_ALEN,
       sizeof(MD5_CTX),
       (void (*)(void *))MD5Init, 
       (void (*)(void *, u_int8_t *, u_int16_t))MD5Update, 
       (void (*)(u_int8_t *, void *))MD5Final 
     },
     { SADB_AALG_X_SHA1, "Keyed SHA1",
       0, AH_SHA1_ALEN,
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
    return 0;
}

/*
 * ah_old_init() is called when an SPI is being set up.
 */

int
ah_old_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
    struct auth_hash *thash = NULL;
    int i;

    /* Check whether the hash algorithm is supported */
    for (i = sizeof(ah_old_hash) / sizeof(struct auth_hash) - 1; i >= 0; i--) 
      if (ii->ii_authalg == ah_old_hash[i].type)
	      break;

    if (i < 0) 
    {
	DPRINTF(("ah_old_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
	return EINVAL;
    }

    thash = &ah_old_hash[i];

    DPRINTF(("ah_old_init(): initalized TDB with hash algorithm %s\n",
	     thash->name));

    tdbp->tdb_xform = xsp;
    tdbp->tdb_authalgxform = thash;

    tdbp->tdb_amxkeylen = ii->ii_authkeylen;
    MALLOC(tdbp->tdb_amxkey, u_int8_t *, tdbp->tdb_amxkeylen,
	   M_XDATA, M_WAITOK);

    bcopy(ii->ii_authkey, tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);

    MALLOC(tdbp->tdb_ictx, u_int8_t *, thash->ctxsize, M_XDATA, M_WAITOK);
    bzero(tdbp->tdb_ictx, thash->ctxsize);
    thash->Init(tdbp->tdb_ictx);
    thash->Update(tdbp->tdb_ictx, tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
    thash->Final(NULL, tdbp->tdb_ictx);

    bzero(ipseczeroes, IPSEC_ZEROES_SIZE);	/* paranoid */

    return 0;
}

/*
 * Free memory
 */

int
ah_old_zeroize(struct tdb *tdbp)
{
    if (tdbp->tdb_amxkey)
    {
	bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
	FREE(tdbp->tdb_amxkey, M_XDATA);
	tdbp->tdb_amxkey = NULL;
    }
    
    if (tdbp->tdb_ictx)
    {
	if (tdbp->tdb_authalgxform)
	  bzero(tdbp->tdb_ictx, tdbp->tdb_authalgxform->ctxsize);

    	FREE(tdbp->tdb_ictx, M_XDATA);
	tdbp->tdb_ictx = NULL;
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
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
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
    alen = ahx->hashsize;
    ohlen = sizeof(struct ip) + AH_OLD_FLENGTH + alen;

    if (m->m_len < ohlen)
    {
	if ((m = m_pullup(m, ohlen)) == NULL)
	{
	    DPRINTF(("ah_old_input(): m_pullup() failed\n"));
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
	    DPRINTF(("ah_old_input(): m_pullup() failed\n"));
	    ahstat.ahs_hdrops++;
	    return NULL;
	}
	
	ip = mtod(m, struct ip *);
	ah = (struct ah_old *)((u_int8_t *) ip + (ip->ip_hl << 2));
	ohlen += ((ip->ip_hl << 2) - sizeof(struct ip));
    }
    else
      ah = (struct ah_old *) (ip + 1);

    /* Update the counters */
    tdb->tdb_cur_bytes += ip->ip_len - (ip->ip_hl << 2);
    ahstat.ahs_ibytes += ip->ip_len - (ip->ip_hl << 2);

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
      {
/* XXX
   encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
*/
	  m_freem(m);
	  tdb_delete(tdb, 0);
	  return NULL;
      }

    /* Notify on expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
      {
/* XXX
   encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
*/
	  tdb->tdb_flags &= ~TDBF_SOFT_BYTES;   /* Turn off checking */
      }

    ipo = *ip;
    ipo.ip_tos = 0;
    ipo.ip_len += (ip->ip_hl << 2);	/* adjusted in ip_intr() */
    HTONS(ipo.ip_len);
    HTONS(ipo.ip_id);
    ipo.ip_off = htons(ipo.ip_off & IP_DF);	/* XXX -- and the C bit? */
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
    
    
    ahx->Update(&ctx, (unsigned char *) ah, AH_OLD_FLENGTH);
    ahx->Update(&ctx, ipseczeroes, alen);

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
	    DPRINTF(("ah_old_input(): bad mbuf chain for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
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
	    DPRINTF(("ah_old_input(): bad mbuf chain for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
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

    ahx->Update(&ctx, (unsigned char *) tdb->tdb_amxkey, tdb->tdb_amxkeylen);
    ahx->Final((unsigned char *) (aho->ah_data), &ctx);

    if (bcmp(aho->ah_data, ah->ah_data, alen))
    {
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

    return m;
}

int
ah_old_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb,
  	      struct mbuf **mp)
{
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
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

    alen = ahx->hashsize;

    ahstat.ahs_output++;
    m = m_pullup(m, sizeof(struct ip));
    if (m == NULL)
    {
	DPRINTF(("ah_old_output(): m_pullup() failed, SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_hdrops++;
      	return ENOBUFS;
    }

    ip = mtod(m, struct ip *);
	
    /* Update the counters */
    tdb->tdb_cur_bytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) -
			  AH_OLD_FLENGTH - alen;
    ahstat.ahs_obytes += ntohs(ip->ip_len) - (ip->ip_hl << 2) -
			 AH_OLD_FLENGTH - alen;

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

    if ((ip->ip_hl << 2) > sizeof(struct ip))
    {
	if ((m = m_pullup(m, ip->ip_hl << 2)) == NULL)
	{
	    DPRINTF(("ah_old_output(): m_pullup() failed, SA %s/%08x\n",
		     ipsp_address(tdb->tdb_dst),
		     ntohl(tdb->tdb_spi)));
	    ahstat.ahs_hdrops++;
	    return ENOBUFS;
	}
	
	ip = mtod(m, struct ip *);
    }

    /* Save the options */
    m_copydata(m, sizeof(struct ip), (ip->ip_hl << 2) - sizeof(struct ip),
	       (caddr_t) opts);

    ilen = ntohs(ip->ip_len);

    ohlen = AH_OLD_FLENGTH + alen;
    if (ohlen + ilen > IP_MAXPACKET)
    {
	DPRINTF(("ah_old_output(): packet in SA %s/%0x8 got too big\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_toobig++;
        return EMSGSIZE;
    }
	
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

    ahx->Update(&ctx, (unsigned char *) &aho, AH_OLD_FLENGTH);
    ahx->Update(&ctx, ipseczeroes, alen);

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
	{
	    DPRINTF(("ah_old_output(): M_PREPEND() failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	    m_freem(m);
	    return NULL;
	}
	
	count = min(m0->m_len - off, len);

	ahx->Update(&ctx, mtod(m0, unsigned char *) + off, count);

	len -= count;
	off = 0;
	m0 = m0->m_next;
    }

    ahx->Update(&ctx, (unsigned char *) tdb->tdb_amxkey, tdb->tdb_amxkeylen);

    ipo.ip_tos = ip->ip_tos;
    ipo.ip_id = ip->ip_id;
    ipo.ip_off = ip->ip_off;
    ipo.ip_ttl = ip->ip_ttl;
/*  ipo.ip_len = ntohs(ipo.ip_len); */
	
    M_PREPEND(m, ohlen, M_DONTWAIT);
    if (m == NULL)
    {
	DPRINTF(("ah_old_output(): M_PREPEND() failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
        return ENOBUFS;
    }

    m = m_pullup(m, ohlen + (ipo.ip_hl << 2));
    if (m == NULL)
    {
	DPRINTF(("ah_old_output(): m_pullup() failed for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo.ip_src), inet_ntoa4(ipo.ip_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_hdrops++;
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

    ahx->Final(ah->ah_data, &ctx);

    *mp = m;

    return 0;
}
