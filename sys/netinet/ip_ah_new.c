/*	$OpenBSD: ip_ah_new.c,v 1.36 2000/01/09 23:42:37 angelos Exp $	*/

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

#ifdef INET6
#include <netinet6/in6.h>
#include <netinet6/ip6.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <net/pfkeyv2.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

extern struct auth_hash auth_hash_hmac_md5_96;
extern struct auth_hash auth_hash_hmac_sha1_96;
extern struct auth_hash auth_hash_hmac_ripemd_160_96;

struct auth_hash *ah_new_hash[] = {
    &auth_hash_hmac_md5_96,
    &auth_hash_hmac_sha1_96,
    &auth_hash_hmac_ripemd_160_96
};

/*
 * ah_new_attach() is called from the transformation initialization code
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

    for (i = sizeof(ah_new_hash) / sizeof(ah_new_hash[0]) - 1; i >= 0; i--)
      if (ii->ii_authalg == ah_new_hash[i]->type)
	break;

    if (i < 0) 
    {
	DPRINTF(("ah_new_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
	return EINVAL;
    }

    thash = ah_new_hash[i];

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
 * passes authentication
 */

struct mbuf *
ah_new_input(struct mbuf *m, struct tdb *tdb, int skip, int protoff)
{
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
    unsigned char calcauth[AH_MAX_HASHLEN];
    int len, count, off, roff;
    struct mbuf *m0, *m1;
    unsigned char *ptr;
    union authctx ctx;
    struct ah_new ah;
    
#ifdef INET
    struct ip ipo;
#endif /* INET */

#ifdef INET6
    struct ip6_ext *ip6e;
    struct ip6_hdr ip6;
    int last;
#endif /* INET6 */

    /* Save the AH header, we use it throughout */
    m_copydata(m, skip, sizeof(ah), (unsigned char *) &ah);

    /* Replay window checking */
    if (tdb->tdb_wnd > 0)
    {
	switch (checkreplaywindow32(ntohl(ah.ah_rpl), 0, &(tdb->tdb_rpl),
				    tdb->tdb_wnd, &(tdb->tdb_bitmap)))
	{
	    case 0: /* All's well */
		break;

	    case 1:
		DPRINTF(("ah_new_input(): replay counter wrapped for SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_wrap++;
		m_freem(m);
		return NULL;

	    case 2:
	    case 3:
		DPRINTF(("ah_new_input(): duplicate packet received in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_replay++;
		m_freem(m);
		return NULL;

	    default:
                DPRINTF(("ah_new_input(): bogus value from checkreplaywindow32() in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_replay++;
                m_freem(m);
                return NULL;
	}
    }

    /* Verify AH header length */
    if (ah.ah_hl * sizeof(u_int32_t) != AH_HMAC_HASHLEN + AH_HMAC_RPLENGTH)
    {
	DPRINTF(("ah_new_input(): bad authenticator length %d for packet in SA %s/%08x\n", ah.ah_hl * sizeof(u_int32_t), ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_badauthl++;
	m_freem(m);
	return NULL;
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += (m->m_pkthdr.len - skip -
			   ah.ah_hl * sizeof(u_int32_t));
    ahstat.ahs_ibytes += (m->m_pkthdr.len - skip -
			  ah.ah_hl * sizeof(u_int32_t));

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
      {
	  pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	  tdb_delete(tdb, 0, TDBEXP_TIMEOUT);
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

    bcopy(tdb->tdb_ictx, &ctx, ahx->ctxsize);

    switch (tdb->tdb_dst.sa.sa_family)
    {
#ifdef INET
	case AF_INET:
	    /*
	     * This is the least painful way of dealing with IPv4 header
	     * and option processing -- just make sure they're in
	     * contiguous memory.
	     */
	    m = m_pullup(m, skip);
	    if (m == NULL)
	    {
		DPRINTF(("ah_new_input(): m_pullup() failed, SA %s/%08x\n",
			 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_hdrops++;
		return NULL;
	    }

	    ptr = mtod(m, unsigned char *) + sizeof(struct ip);

	    bcopy(mtod(m, unsigned char *), (unsigned char *) &ipo,
		  sizeof(struct ip));

	    ipo.ip_tos = 0;
	    ipo.ip_len += skip;     /* adjusted in ip_intr() */
	    HTONS(ipo.ip_len);
	    HTONS(ipo.ip_id);
	    ipo.ip_off = 0;
	    ipo.ip_ttl = 0;
	    ipo.ip_sum = 0;

	    /* Include IP header in authenticator computation */
	    ahx->Update(&ctx, (unsigned char *) &ipo, sizeof(struct ip));

	    /* IPv4 option processing */
	    for (off = sizeof(struct ip); off < skip;)
	    {
		switch (ptr[off])
		{
		    case IPOPT_EOL:
			ahx->Update(&ctx, ptr + off, 1);
			off = skip;  /* End the loop */
			break;

		    case IPOPT_NOP:
			ahx->Update(&ctx, ptr + off, 1);
			off++;
			break;

		    case IPOPT_SECURITY:	/* 0x82 */
		    case 0x85:	/* Extended security */
		    case 0x86:	/* Commercial security */
		    case 0x94:	/* Router alert */
		    case 0x95:	/* RFC1770 */
			ahx->Update(&ctx, ptr + off, ptr[off + 1]);
			/* Sanity check for zero-length options */
			if (ptr[off + 1] == 0)
			{
			    DPRINTF(("ah_new_input(): illegal zero-length IPv4 option %d in SA %s/%08x\n", ptr[off], ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			    ahstat.ahs_hdrops++;
			    m_freem(m);
			    return NULL;
			}

			off += ptr[off + 1];
			break;

		    default:
			ahx->Update(&ctx, ipseczeroes, ptr[off + 1]);
			off += ptr[off + 1];
			break;
		}

		/* Sanity check */
		if (off > skip)
		{
		    DPRINTF(("ah_new_input(): malformed IPv4 options header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    ahstat.ahs_hdrops++;
		    m_freem(m);
		    return NULL;
		}
	    }

	    break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:  /* Ugly... */
	    /* Copy and "cook" (later on) the IPv6 header */
	    m_copydata(m, 0, sizeof(ip6), (unsigned char *) &ip6);

	    /* We don't do IPv6 Jumbograms */
	    if (ip6.ip6_plen == 0)
	    {
		DPRINTF(("ah_new_input(): unsupported IPv6 jumbogram in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_hdrops++;
		m_freem(m);
		return NULL;
	    }

	    ip6.ip6_flow = 0;
	    ip6.ip6_hlim = 0;
	    ip6.ip6_vfc &= ~IPV6_VERSION_MASK;
	    ip6.ip6_vfc |= IPV6_VERSION;

	    /* Include IPv6 header in authenticator computation */
	    ahx->Update(&ctx, (unsigned char *) &ip6, sizeof(ip6));

	    /* Let's deal with the remaining headers (if any) */
	    if (skip - sizeof(struct ip6_hdr) > 0)
	    {
		if (m->m_len <= skip)
		{
		    MALLOC(ptr, unsigned char *, skip - sizeof(struct ip6_hdr),
			   M_XDATA, M_WAITOK);

		    /* Copy all the protocol headers after the IPv6 header */
		    m_copydata(m, sizeof(struct ip6_hdr),
			       skip - sizeof(struct ip6_hdr), ptr);
		}
		else
		  ptr = mtod(m, unsigned char *) + sizeof(struct ip6_hdr);
	    }
	    else
	      break;

	    off = ip6.ip6_nxt & 0xff; /* Next header type */

	    for (len = 0; len < skip - sizeof(struct ip6_hdr);)
	      switch (off)
	      {
		  case IPPROTO_HOPOPTS:
		  case IPPROTO_DSTOPTS:
		      ip6e = (struct ip6_ext *) (ptr + len);

		      /*
		       * Process the mutable/immutable options -- borrows
		       * heavily from the KAME code.
		       */
		      for (last = len, count = len + sizeof(struct ip6_ext);
			   count < len + ((ip6e->ip6e_len + 1) << 3);)
		      {
			  if (ptr[count] == IP6OPT_PAD1)
			  {
			      count++;
			      continue;
			  }

			  /* Sanity check */
			  if (count + sizeof(struct ip6_ext) > len +
			      ((ip6e->ip6e_len + 1) << 3))
			  {
			      DPRINTF(("ah_new_input(): malformed IPv6 options header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			      ahstat.ahs_hdrops++;
			      m_freem(m);

			      /* Free, if we allocated */
			      if (m->m_len < skip)
			      {
				  FREE(ptr, M_XDATA);
				  ptr = NULL;
			      }
			      return NULL;
			  }

			  /*
			   * If mutable option, calculate authenticator
			   * for all immutable fields so far, then include
			   * a zeroed-out version of this option.
			   */
			  if (ptr[count] & IP6OPT_MUTABLE)
			  {
			      /* Calculate immutables */
			      ahx->Update(&ctx, ptr + last,
					  count + sizeof(struct ip6_ext) -
					  last);
			      last = count + ptr[count + 1] +
				     sizeof(struct ip6_ext);

			      /* Calculate "zeroed-out" immutables */
			      ahx->Update(&ctx, ipseczeroes, ptr[count + 1] -
					  sizeof(struct ip6_ext));
			  }
			  
			  count += ptr[count + 1] + sizeof(struct ip6_ext);

			  /* Sanity check */
			  if (count > skip - sizeof(struct ip6_hdr))
			  {
			      DPRINTF(("ah_new_input(): malformed IPv6 options header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			      ahstat.ahs_hdrops++;
			      m_freem(m);

			      /* Free, if we allocated */
			      if (m->m_len < skip)
			      {
				  FREE(ptr, M_XDATA);
				  ptr = NULL;
			      }
			      return NULL;
			  }
		      }

		      /* Include any trailing immutable options */
		      ahx->Update(&ctx, ptr + last,
				  len + ((ip6e->ip6e_len + 1) << 3) - last);

		      len += ((ip6e->ip6e_len + 1) << 3); /* Advance */
		      off = ip6e->ip6e_nxt;
		      break;

		  case IPPROTO_ROUTING:
		      ip6e = (struct ip6_ext *) (ptr + len);
		      ahx->Update(&ctx, ptr + len, (ip6e->ip6e_len + 1) << 3);
		      len += ((ip6e->ip6e_len + 1) << 3); /* Advance */
		      off = ip6e->ip6e_nxt;
		      break;

		  default:
		      DPRINTF(("ah_new_input(): unexpected IPv6 header type %d in SA %s/%08x\n", off, ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));   
		      len = skip - sizeof(struct ip6_hdr);
		      break;
	      }

	    /* Free, if we allocated */
	    if (m->m_len < skip)
	    {
		FREE(ptr, M_XDATA);
		ptr = NULL;
	    }

	    break;
#endif /* INET6 */

	default:
	    DPRINTF(("ah_new_input(): unsupported protocol family %d in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    ahstat.ahs_hdrops++;
	    m_freem(m);
	    return NULL;
    }

    /* Record the beginning of the AH header */
    for (len = 0, m1 = m; m1 && (len + m1->m_len <= skip); m1 = m1->m_next)
      len += m1->m_len;

    if (m1 == NULL)
    {
	DPRINTF(("ah_new_input(): bad mbuf chain for packet in SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_hdrops++;
	m_freem(m);
	return NULL;
    }
    else
      roff = skip - len;

    /* Skip the AH header */
    for (len = 0, m0 = m1;
	 m0 && (len + m0->m_len <= AH_NEW_FLENGTH + roff);
	 m0 = m0->m_next)
      len += m0->m_len;

    if (m0 == NULL)
    {
	DPRINTF(("ah_new_input(): bad mbuf chain for packet in SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_hdrops++;
	m_freem(m);
	return NULL;
    }
    else
      off = (AH_NEW_FLENGTH + roff) - len;

    /* Include the AH header (minus the authenticator) in the computation */
    ahx->Update(&ctx, (unsigned char *) &ah, AH_NEW_FLENGTH - AH_HMAC_HASHLEN);

    /* All-zeroes for the authenticator */
    ahx->Update(&ctx, ipseczeroes, AH_HMAC_HASHLEN);

    /* Amount of data to be verified */
    len = m->m_pkthdr.len - skip - AH_NEW_FLENGTH;

    /* Loop through the mbuf chain computing the HMAC */
    while (len > 0)
    {
	if (m0 == NULL)
	{
	    DPRINTF(("ah_new_input(): bad mbuf chain for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
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

    /* Finish HMAC computation */
    ahx->Final(calcauth, &ctx);
    bcopy(tdb->tdb_octx, &ctx, ahx->ctxsize);
    ahx->Update(&ctx, calcauth, ahx->hashsize);
    ahx->Final(calcauth, &ctx);

    /* Verify */
    if (bcmp(&(ah.ah_data), calcauth, AH_HMAC_HASHLEN))
    {
	DPRINTF(("ah_new_input(): authentication failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_badauth++;
	m_freem(m);
	return NULL;
    }

    /* Fix the Next Protocol field */
    m_copyback(m, protoff, 1, (u_char *) &(ah.ah_nh));

    /*
     * Remove the AH header from the mbuf.
     */
    if (roff == 0) 
    {
	/* The AH header was conveniently at the beginning of the mbuf */
	m_adj(m1, AH_NEW_FLENGTH);
	if (!(m1->m_flags & M_PKTHDR))
	  m->m_pkthdr.len -= AH_NEW_FLENGTH;
    }
    else
      if (roff + AH_NEW_FLENGTH >= m1->m_len)
      {
	  /*
	   * Part or all of the AH header is at the end of this mbuf, so first
	   * let's remove the remainder of the AH header from the
	   * beginning of the remainder of the mbuf chain, if any.
	   */
	  if (roff + AH_NEW_FLENGTH > m1->m_len)
	  {
	      /* Adjust the next mbuf by the remainder */
	      m_adj(m1->m_next, roff + AH_NEW_FLENGTH - m1->m_len);

	      /* The second mbuf is guaranteed not to have a pkthdr... */
	      m->m_pkthdr.len -= (roff + AH_NEW_FLENGTH - m1->m_len);
	  }

	  /* Now, let's unlink the mbuf chain for a second...*/
	  m0 = m1->m_next;
	  m1->m_next = NULL;

	  /* ...and trim the end of the first part of the chain...sick */
	  m_adj(m1, -(m1->m_len - roff));
	  if (!(m1->m_flags & M_PKTHDR))
	    m->m_pkthdr.len -= (m1->m_len - roff);

	  /* Finally, let's relink */
	  m1->m_next = m0;
      }
      else
      {
	  /* 
	   * The AH header lies in the "middle" of the mbuf...do an
	   * overlapping copy of the remainder of the mbuf over the ESP
	   * header.
	   */
	  bcopy(mtod(m1, u_char *) + roff + AH_NEW_FLENGTH,
		mtod(m1, u_char *) + roff,
		m1->m_len - (roff + AH_NEW_FLENGTH));
	  m1->m_len -= AH_NEW_FLENGTH;
	  m->m_pkthdr.len -= AH_NEW_FLENGTH;
      }

    return m;
}

int
ah_new_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
	      int protoff)
{
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
    unsigned char calcauth[AH_MAX_HASHLEN];
    int len, off, count;
    unsigned char *ptr;
    struct ah_new *ah;
    union authctx ctx;
    struct mbuf *mo;

#ifdef INET
    struct ip ipo;
#endif /* INET */

#ifdef INET6
    struct ip6_ext *ip6e;
    struct ip6_hdr ip6;
    int last;
#endif /* INET6 */

    ahstat.ahs_output++;

    /* Check for replay counter wrap-around in automatic (not manual) keying */
    if ((tdb->tdb_rpl == 0) && (tdb->tdb_wnd > 0))
    {
	DPRINTF(("ah_new_output(): SA %s/%08x should have expired\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_wrap++;
	return NULL;
    }

#ifdef INET
    if (AH_NEW_FLENGTH + m->m_pkthdr.len > IP_MAXPACKET)
    {
	DPRINTF(("ah_new_output(): packet in SA %s/%08x got too big\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_toobig++;
        return EMSGSIZE;
    }
#endif /* INET  */

    /* Update the counters */
    tdb->tdb_cur_bytes += m->m_pkthdr.len - skip;
    ahstat.ahs_obytes += m->m_pkthdr.len - skip;

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
      {
	  pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	  tdb_delete(tdb, 0, TDBEXP_TIMEOUT);
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

    /*
     * Loop through mbuf chain; if we find an M_EXT mbuf with 
     * more than one reference, replace the rest of the chain.
     * This may not be strictly necessary for AH packets, if we were
     * careful with the rest of our processing (and made a lot of
     * assumptions about the layout of the packets/mbufs).
     */
    (*mp) = m;
    while ((*mp) != NULL && 
	   (!((*mp)->m_flags & M_EXT) || 
	    ((*mp)->m_ext.ext_ref == NULL &&
	     mclrefcnt[mtocl((*mp)->m_ext.ext_buf)] <= 1)))
    {
        mo = (*mp);
        (*mp) = (*mp)->m_next;
    }
     
    if ((*mp) != NULL)
    {
        /* Replace the rest of the mbuf chain. */
        struct mbuf *n = m_copym2((*mp), 0, M_COPYALL, M_DONTWAIT);
      
        if (n == NULL)
        {
	    ahstat.ahs_hdrops++;
	    m_freem(m);
	    return ENOBUFS;
        }

        if (mo != NULL)
	  mo->m_next = n;
        else
	  m = n;

        m_freem((*mp));
	(*mp) = NULL;
    }

    bcopy(tdb->tdb_ictx, (caddr_t) &ctx, ahx->ctxsize);

    switch (tdb->tdb_dst.sa.sa_family)
    {
#ifdef INET
	case AF_INET:
	    /*
	     * This is the most painless way of dealing with IPv4 header
	     * and option processing -- just make sure they're in
	     * contiguous memory.
	     */
	    m = m_pullup(m, skip);
	    if (m == NULL)
	    {
		DPRINTF(("ah_new_output(): m_pullup() failed, SA %s/%08x\n",
			 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_hdrops++;
		return ENOBUFS;
	    }

	    ptr = mtod(m, unsigned char *) + sizeof(struct ip);

	    bcopy(mtod(m, unsigned char *), (unsigned char *) &ipo,
		  sizeof(struct ip));

	    ipo.ip_tos = 0;
	    ipo.ip_off = 0;
	    ipo.ip_ttl = 0;
	    ipo.ip_sum = 0;
	    ipo.ip_p = IPPROTO_AH;
	    ipo.ip_len = htons(ntohs(ipo.ip_len) + AH_NEW_FLENGTH);

	    /* 
	     * If we have a loose or strict routing option, we are
	     * supposed to use the last address in it as the
	     * destination address in the authenticated IPv4 header.
	     *
	     * Note that this is an issue only with the output routine;
	     * we will correctly process (in the AH input routine) incoming
	     * packets with these options without special consideration.
	     *
	     * We assume that the IP header contains the next hop's address,
	     * and that the last entry in the option is the final
	     * destination's address.
	     */
	    if (skip > sizeof(struct ip))
	    {
		for (off = sizeof(struct ip); off < skip;)
		{
		    /* First sanity check for zero-length options */
		    if ((ptr[off] != IPOPT_EOL) && (ptr[off] != IPOPT_NOP) &&
			(ptr[off + 1] == 0))
		    {
			DPRINTF(("ah_new_output(): illegal zero-length IPv4 option %d in SA %s/%08x\n", ptr[off], ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			ahstat.ahs_hdrops++;
			m_freem(m);
			return EMSGSIZE;
		    }

		    switch (ptr[off])
		    {
			case IPOPT_LSRR:
			case IPOPT_SSRR:
			    /* Sanity check for length */
			    if (ptr[off + 1] < 2 + sizeof(struct in_addr))
			    {
				DPRINTF(("ah_new_output(): malformed LSRR or SSRR IPv4 option header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
				ahstat.ahs_hdrops++;
				m_freem(m);
				return EMSGSIZE;
			    }

			    bcopy(ptr + off + ptr[off + 1] -
				  sizeof(struct in_addr),
				  &(ipo.ip_dst), sizeof(struct in_addr));
			    off = skip;
			    break;

			case IPOPT_EOL:
			    off = skip;
			    break;

			case IPOPT_NOP:
			    off++;
			    break;

			default:  /* Some other option, just skip it */
			    off += ptr[off + 1];
			    break;
		    }

		    /* Sanity check */
		    if (off > skip)
		    {
			DPRINTF(("ah_new_output(): malformed IPv4 options header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			ahstat.ahs_hdrops++;
			m_freem(m);
			return EMSGSIZE;
		    }
		}
	    }

	    /* Include IP header in authenticator computation */
	    ahx->Update(&ctx, (unsigned char *) &ipo, sizeof(struct ip));

	    /* IPv4 option processing */
	    for (off = sizeof(struct ip); off < skip;)
	    {
		switch (ptr[off])
		{
		    case IPOPT_EOL:
			ahx->Update(&ctx, ptr + off, 1);
			off = skip;  /* End the loop */
			break;

		    case IPOPT_NOP:
			ahx->Update(&ctx, ptr + off, 1);
			off++;
			break;

		    case IPOPT_SECURITY:	/* 0x82 */
		    case 0x85:	/* Extended security */
		    case 0x86:	/* Commercial security */
		    case 0x94:	/* Router alert */
		    case 0x95:	/* RFC1770 */
			ahx->Update(&ctx, ptr + off, ptr[off + 1]);
			off += ptr[off + 1];
			break;

		    default:
			ahx->Update(&ctx, ipseczeroes, ptr[off + 1]);
			off += ptr[off + 1];
			break;
		}

		/* Sanity check */
		if (off > skip)
		{
		    DPRINTF(("ah_new_output(): malformed IPv4 options header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		    ahstat.ahs_hdrops++;
		    m_freem(m);
		    return EMSGSIZE;
		}
	    }
	    break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
	    /* Copy and "cook" the IPv6 header */
	    m_copydata(m, 0, sizeof(ip6), (unsigned char *) &ip6);

	    /* We don't do IPv6 Jumbograms */
	    if (ip6.ip6_plen == 0)
	    {
		DPRINTF(("ah_new_output(): unsupported IPv6 jumbogram in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_hdrops++;
		m_freem(m);
		return EMSGSIZE;
	    }

	    ip6.ip6_flow = 0;
	    ip6.ip6_hlim = 0;
	    ip6.ip6_vfc &= ~IPV6_VERSION_MASK;
	    ip6.ip6_vfc |= IPV6_VERSION;

	    /*
	     * Note that here we assume that on output, the IPv6 header
	     * and any Type0 Routing Header present have been made to look
	     * like the will at the destination. Note that this is a
	     * different assumption than we made for IPv4 (because of 
	     * different option processing in IPv4 and IPv6, and different
	     * code paths from IPv4/IPv6 to here).
	     */

	    /* Include IPv6 header in authenticator computation */
	    ahx->Update(&ctx, (unsigned char *) &ip6, sizeof(ip6));

	    /* Let's deal with the remaining headers (if any) */
	    if (skip - sizeof(struct ip6_hdr) > 0)
	    {
		if (m->m_len <= skip)
		{
		    MALLOC(ptr, unsigned char *,
			   skip - sizeof(struct ip6_hdr), M_XDATA, M_WAITOK);

		    /* Copy all the protocol headers after the IPv6 header */
		    m_copydata(m, sizeof(struct ip6_hdr),
			       skip - sizeof(struct ip6_hdr), ptr);
		}
		else
		  ptr = mtod(m, unsigned char *) + sizeof(struct ip6_hdr);
	    }
	    else
	      break; /* Done */

	    off = ip6.ip6_nxt & 0xff; /* Next header type */
	    for (len = 0; len < skip - sizeof(struct ip6_hdr);)
	      switch (off)
	      {
		  case IPPROTO_HOPOPTS:
		  case IPPROTO_DSTOPTS:
		      ip6e = (struct ip6_ext *) (ptr + len);

		      /*
		       * Process the mutable/immutable options -- borrows
		       * heavily from the KAME code.
		       */
		      for (last = len, count = len + sizeof(struct ip6_ext);
			   count < len + ((ip6e->ip6e_len + 1) << 3);)
		      {
			  if (ptr[count] == IP6OPT_PAD1)
			  {
			      count++;
			      continue;
			  }

			  /* Sanity check */
			  if (count + sizeof(struct ip6_ext) > len +
			      ((ip6e->ip6e_len + 1) << 3))
			  {
			      DPRINTF(("ah_new_output(): malformed IPv6 options header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			      ahstat.ahs_hdrops++;
			      m_freem(m);

			      /* Free, if we allocated */
			      if (m->m_len < skip)
				FREE(ptr, M_XDATA);
			      return EMSGSIZE;
			  }

			  /*
			   * If mutable option, calculate authenticator
			   * for all immutable fields so far, then include
			   * a zeroed-out version of this option.
			   */
			  if (ptr[count] & IP6OPT_MUTABLE)
			  {
			      /* Calculate immutables */
			      ahx->Update(&ctx, ptr + last, count + 2 - last);
			      last = count + ptr[count + 1];

			      /* Calculate "zeroed-out" immutables */
			      ahx->Update(&ctx, ipseczeroes,
					  ptr[count + 1] - 2);
			  }
			  
			  count += ptr[count + 1];

			  /* Sanity check */
			  if (count > skip - sizeof(struct ip6_hdr))
			  {
			      DPRINTF(("ah_new_output(): malformed IPv6 options header in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
			      ahstat.ahs_hdrops++;
			      m_freem(m);

			      /* Free, if we allocated */
			      if (m->m_len < skip)
				FREE(ptr, M_XDATA);
			      return EMSGSIZE;
			  }
		      }

		      /* Include any trailing immutable options */
		      ahx->Update(&ctx, ptr + last,
				  len + ((ip6e->ip6e_len + 1) << 3) - last);

		      len += ((ip6e->ip6e_len + 1) << 3); /* Advance */
		      off = ip6e->ip6e_nxt;
		      break;

		  case IPPROTO_ROUTING:
		      ip6e = (struct ip6_ext *) (ptr + len);
		      ahx->Update(&ctx, ptr + len, (ip6e->ip6e_len + 1) << 3);
		      len += ((ip6e->ip6e_len + 1) << 3); /* Advance */
		      off = ip6e->ip6e_nxt;
		      break;
	      }
	    
	    /* Free, if we allocated */
	    if (m->m_len < skip)
	    {
		FREE(ptr, M_XDATA);
		ptr = NULL;
	    }

	    break;
#endif /* INET6 */

	default:
	    DPRINTF(("ah_new_output(): unsupported protocol family %d in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    ahstat.ahs_nopf++;
	    m_freem(m);
	    return EPFNOSUPPORT;
    }

    /* Inject AH header */
    (*mp) = m_inject(m, skip, AH_NEW_FLENGTH, M_WAITOK);
    if ((*mp) == NULL)
    {
	DPRINTF(("ah_new_output(): failed to inject AH header for SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_wrap++;
	return ENOBUFS;
    }

    /*
     * The AH header is guaranteed by m_inject() to be in contiguous memory,
     * at the beginning of the returned mbuf.
     */
    ah = mtod((*mp), struct ah_new *);
    
    /* Initialize the AH header */
    m_copydata(m, protoff, 1, &(ah->ah_nh)); /* Save Next Protocol field */
    ah->ah_hl = ((AH_HMAC_RPLENGTH + AH_HMAC_HASHLEN) >> 2);
    ah->ah_rv = 0;
    ah->ah_spi = tdb->tdb_spi;
    ah->ah_rpl = htonl(tdb->tdb_rpl++);

    /* Update the Next Protocol field in the IP header */
    len = IPPROTO_AH;
    m_copyback(m, protoff, 1, (unsigned char *) &len);

    /* Include the header AH in the authenticator computation */
    ahx->Update(&ctx, (unsigned char *) ah, AH_NEW_FLENGTH - AH_HMAC_HASHLEN);
    ahx->Update(&ctx, ipseczeroes, AH_HMAC_HASHLEN);

    /* Calculate the authenticator over the rest of the packet */
    len = m->m_pkthdr.len - (skip + AH_NEW_FLENGTH);
    off = AH_NEW_FLENGTH;

    while (len > 0)
    {
	if ((*mp) == 0)
	{
	    DPRINTF(("ah_new_output(): bad mbuf chain for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    ahstat.ahs_hdrops++;
	    m_freem(m);
	    (*mp) = NULL;
	    return EMSGSIZE;
	}

	count = min((*mp)->m_len - off, len);

	ahx->Update(&ctx, mtod((*mp), unsigned char *) + off, count);

	len -= count;
	off = 0;
	(*mp) = (*mp)->m_next;
    }

    /* Finish computing the authenticator */
    ahx->Final(calcauth, &ctx);
    bcopy(tdb->tdb_octx, &ctx, ahx->ctxsize);
    ahx->Update(&ctx, calcauth, ahx->hashsize);
    ahx->Final(calcauth, &ctx);

    /* Copy the authenticator */
    bcopy(calcauth, ah->ah_data, AH_HMAC_HASHLEN);

    *mp = m;
	
    return 0;
}
