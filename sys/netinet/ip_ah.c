/*	$OpenBSD: ip_ah.c,v 1.35 2000/03/17 10:25:22 angelos Exp $ */

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
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <net/pfkeyv2.h>
#include <net/if_enc.h>

#include <sys/md5k.h>
#include <crypto/sha1.h>
#include <crypto/rmd160.h>
#include <crypto/crypto.h>
#include <crypto/xform.h>

#include "bpfilter.h"

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

/*
 * ah_attach() is called from the transformation initialization code
 */
int
ah_attach()
{
    return 0;
}

/*
 * ah_init() is called when an SPI is being set up.
 */
int
ah_init(struct tdb *tdbp, struct xformsw *xsp, struct ipsecinit *ii)
{
    struct auth_hash *thash = NULL;
    struct cryptoini cria;

    /* Authentication operation */
    switch (ii->ii_authalg)
    {
	case SADB_AALG_MD5HMAC96:
	    thash = &auth_hash_hmac_md5_96;
	    break;

	case SADB_AALG_SHA1HMAC96:
	    thash = &auth_hash_hmac_sha1_96;
	    break;

	case SADB_X_AALG_RIPEMD160HMAC96:
	    thash = &auth_hash_hmac_ripemd_160_96;
	    break;

	case SADB_X_AALG_MD5:
	    thash = &auth_hash_key_md5;
	    break;

	case SADB_X_AALG_SHA1:
	    thash = &auth_hash_key_sha1;
	    break;

	default:
	    DPRINTF(("ah_init(): unsupported authentication algorithm %d specified\n", ii->ii_authalg));
	    return EINVAL;
    }

    if ((ii->ii_authkeylen != thash->keysize) && (thash->keysize != 0))
    {
	DPRINTF(("ah_init(): keylength %d doesn't match algorithm %s keysize (%d)\n", ii->ii_authkeylen, thash->name, thash->keysize));
	return EINVAL;
    }

    tdbp->tdb_xform = xsp;
    tdbp->tdb_authalgxform = thash;
    tdbp->tdb_bitmap = 0;
    tdbp->tdb_rpl = AH_HMAC_INITIAL_RPL;

    DPRINTF(("ah_init(): initialized TDB with hash algorithm %s\n",
	     thash->name));

    tdbp->tdb_amxkeylen = ii->ii_authkeylen;
    MALLOC(tdbp->tdb_amxkey, u_int8_t *, tdbp->tdb_amxkeylen, M_XDATA,
	   M_WAITOK);

    bcopy(ii->ii_authkey, tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);

    /* Initialize crypto session */
    bzero(&cria, sizeof(cria));
    cria.cri_alg = tdbp->tdb_authalgxform->type;
    cria.cri_klen = ii->ii_authkeylen * 8;
    cria.cri_key = ii->ii_authkey;

    return crypto_newsession(&tdbp->tdb_cryptoid, &cria);
}

/*
 * Paranoia.
 */
int
ah_zeroize(struct tdb *tdbp)
{
    int err;

    if (tdbp->tdb_amxkey)
    {
	bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
	FREE(tdbp->tdb_amxkey, M_XDATA);
	tdbp->tdb_amxkey = NULL;
    }

    err = crypto_freesession(tdbp->tdb_cryptoid);
    tdbp->tdb_cryptoid = 0;
    return err;
}

/*
 * Massage IPv4/IPv6 headers for AH processing.
 */
int
ah_massage_headers(struct mbuf **m0, int proto, int skip, int alg, int out)
{
    struct mbuf *m = *m0;
    unsigned char *ptr;
    int off, count;

#ifdef INET
    struct ip *ip;
#endif /* INET */

#ifdef INET6
    struct ip6_ext *ip6e;
    struct ip6_hdr ip6;
    int alloc, len, ad;
#endif /* INET6 */

    switch (proto)
    {
#ifdef INET
	case AF_INET:
	    /*
	     * This is the least painful way of dealing with IPv4 header
	     * and option processing -- just make sure they're in
	     * contiguous memory.
	     */
	    *m0 = m = m_pullup(m, skip);
	    if (m == NULL)
	    {
		DPRINTF(("ah_massage_headers(): m_pullup() failed\n"));
		ahstat.ahs_hdrops++;
		return ENOBUFS;
	    }

	    /* Fix the IP header */
	    ip = mtod(m, struct ip *);
	    ip->ip_tos = 0;
	    ip->ip_ttl = 0;
	    ip->ip_sum = 0;

	    /*
	     * On input, fix ip_len and ip_id, which have been byte-swapped
	     * at ip_intr()
	     */
	    if (!out)
	    {
		ip->ip_len += skip;
		HTONS(ip->ip_len);
		HTONS(ip->ip_id);
	    }

	    if ((alg == CRYPTO_MD5_KPDK) || (alg == CRYPTO_SHA1_KPDK))
	      ip->ip_off = htons(ip->ip_off & IP_DF);
	    else
	      ip->ip_off = 0;

	    ptr = mtod(m, unsigned char *) + sizeof(struct ip);

	    /* IPv4 option processing */
	    for (off = sizeof(struct ip); off < skip;)
	    {
		switch (ptr[off])
		{
		    case IPOPT_EOL:
			off = skip;  /* End the loop */
			break;

		    case IPOPT_NOP:
			off++;
			break;

		    case IPOPT_SECURITY:	/* 0x82 */
		    case 0x85:	/* Extended security */
		    case 0x86:	/* Commercial security */
		    case 0x94:	/* Router alert */
		    case 0x95:	/* RFC1770 */
			/* Sanity check for zero-length options */
			if (ptr[off + 1] == 0)
			{
			    DPRINTF(("ah_massage_headers(): illegal zero-length IPv4 option %d\n", ptr[off]));
			    ahstat.ahs_hdrops++;
			    m_freem(m);
			    return EINVAL;
			}

			off += ptr[off + 1];
			break;

		    case IPOPT_LSRR:
		    case IPOPT_SSRR:
			/*
			 * On output, if we have either of the source routing
			 * options, we should swap the destination address of
			 * the IP header with the last address specified in
			 * the option, as that is what the destination's
			 * IP header will look like.
			 */
			if (out)
			  bcopy(ptr + off + ptr[off + 1] -
				sizeof(struct in_addr),
				&(ip->ip_dst), sizeof(struct in_addr));

			/* Fall through */
		    default:
			/* Sanity check for zero-length options */
			if (ptr[off + 1] == 0)
			{
			    DPRINTF(("ah_massage_headers(): illegal zero-length IPv4 option %d\n", ptr[off]));
			    ahstat.ahs_hdrops++;
			    m_freem(m);
			    return EINVAL;
			}

			/* Zeroize all other options */
			count = ptr[off + 1];
			bcopy(ipseczeroes, ptr, count);
			off += count;
			break;
		}

		/* Sanity check */
		if (off > skip)
		{
		    DPRINTF(("ah_massage_headers(): malformed IPv4 options header\n"));
		    ahstat.ahs_hdrops++;
		    m_freem(m);
		    return EINVAL;
		}
	    }

	    break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:  /* Ugly... */
	    /* Copy and "cook" the IPv6 header */
	    m_copydata(m, 0, sizeof(ip6), (caddr_t) &ip6);

	    /* We don't do IPv6 Jumbograms */
	    if (ip6.ip6_plen == 0)
	    {
		DPRINTF(("ah_massage_headers(): unsupported IPv6 jumbogram"));
		ahstat.ahs_hdrops++;
		m_freem(m);
		return EMSGSIZE;
	    }

	    ip6.ip6_flow = 0;
	    ip6.ip6_hlim = 0;
	    ip6.ip6_vfc &= ~IPV6_VERSION_MASK;
	    ip6.ip6_vfc |= IPV6_VERSION;

	    /* Done with IPv6 header */
	    m_copyback(m, 0, sizeof(struct ip6_hdr), (caddr_t) &ip6);
	    
	    /* Let's deal with the remaining headers (if any) */
	    if (skip - sizeof(struct ip6_hdr) > 0)
	    {
		if (m->m_len <= skip)
		{
		    MALLOC(ptr, unsigned char *, skip - sizeof(struct ip6_hdr),
			   M_XDATA, M_DONTWAIT);
		    if (ptr == NULL)
		    {
			DPRINTF(("ah_massage_headers(): failed to allocate memory for IPv6 headers\n"));
			ahstat.ahs_hdrops++;
			m_freem(m);
			return ENOBUFS;
		    }

		    /* Copy all the protocol headers after the IPv6 header */
		    m_copydata(m, sizeof(struct ip6_hdr),
			       skip - sizeof(struct ip6_hdr), ptr);
		    alloc = 1;
		}
		else
		{
		    /* No need to allocate memory */
		    ptr = mtod(m, unsigned char *) + sizeof(struct ip6_hdr);
		    alloc = 0;
		}
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
		      for (count = len + sizeof(struct ip6_ext);
			   count < len + ((ip6e->ip6e_len + 1) << 3);)
		      {
			  if (ptr[count] == IP6OPT_PAD1)
			  {
			      count++;
			      continue; /* Skip padding */
			  }

			  /* Sanity check */
			  if (count > len + ((ip6e->ip6e_len + 1) << 3))
			  {
			      DPRINTF(("ah_massage_headers(): malformed IPv6 options header\n"));
			      ahstat.ahs_hdrops++;
			      m_freem(m);

			      /* Free, if we allocated */
			      if (alloc)
				FREE(ptr, M_XDATA);

			      return EINVAL;
			  }

			  ad = ptr[count + 1];

			  /* If mutable option, zeroize */
			  if (ptr[count] & IP6OPT_MUTABLE)
			    bcopy(ipseczeroes, ptr + count, ptr[count + 1]);

			  count += ad;

			  /* Sanity check */
			  if (count > skip - sizeof(struct ip6_hdr))
			  {
			      DPRINTF(("ah_massage_headers(): malformed IPv6 options header\n"));
			      ahstat.ahs_hdrops++;
			      m_freem(m);

			      /* Free, if we allocated */
			      if (alloc)
				FREE(ptr, M_XDATA);

			      return EINVAL;
			  }
		      }

		      len += ((ip6e->ip6e_len + 1) << 3); /* Advance */
		      off = ip6e->ip6e_nxt;
		      break;

		  case IPPROTO_ROUTING:
		      /* Always include routing headers in computation */
		      ip6e = (struct ip6_ext *) (ptr + len);
		      len += ((ip6e->ip6e_len + 1) << 3); /* Advance */
		      off = ip6e->ip6e_nxt;
		      break;

		  default:
		      DPRINTF(("ah_massage_headers(): unexpected IPv6 header type %d\n", off));
		      if (alloc)
			FREE(ptr, M_XDATA);
		      ahstat.ahs_hdrops++;
		      m_freem(m);
		      return EINVAL;
	      }

	    /* Copyback and free, if we allocated */
	    if (alloc)
	    {
		m_copyback(m, sizeof(struct ip6_hdr),
			   skip - sizeof(struct ip6_hdr), ptr);
		FREE(ptr, M_XDATA);
	    }

	    break;
#endif /* INET6 */
    }

    return 0;
}

/*
 * ah_input() gets called to verify that an input packet
 * passes authentication.
 */
int
ah_input(struct mbuf *m, struct tdb *tdb, int skip, int protoff)
{
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
    u_int32_t btsx;
    u_int8_t hl;
    int rplen;

    struct cryptodesc *crda = NULL;
    struct cryptop *crp;

    if (!(tdb->tdb_flags & TDBF_NOREPLAY))
      rplen = AH_FLENGTH + sizeof(u_int32_t);
    else
      rplen = AH_FLENGTH;

    /* Save the AH header, we use it throughout */
    m_copydata(m, skip + offsetof(struct ah, ah_hl), sizeof(u_int8_t),
	       (caddr_t) &hl);

    /* Replay window checking, if applicable */
    if ((tdb->tdb_wnd > 0) && (!(tdb->tdb_flags & TDBF_NOREPLAY)))
    {
	m_copydata(m, skip + offsetof(struct ah, ah_rpl), sizeof(u_int32_t),
		   (caddr_t) &btsx);
	btsx = ntohl(btsx);

	switch (checkreplaywindow32(btsx, 0, &(tdb->tdb_rpl),
				    tdb->tdb_wnd, &(tdb->tdb_bitmap)))
	{
	    case 0: /* All's well */
		break;

	    case 1:
		DPRINTF(("ah_input(): replay counter wrapped for SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_wrap++;
		m_freem(m);
		return ENOBUFS;

	    case 2:
	    case 3:
		DPRINTF(("ah_input(): duplicate packet received in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_replay++;
		m_freem(m);
		return ENOBUFS;

	    default:
                DPRINTF(("ah_input(): bogus value from checkreplaywindow32() in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		ahstat.ahs_replay++;
                m_freem(m);
                return ENOBUFS;
	}
    }

    /* Verify AH header length */
    if (hl * sizeof(u_int32_t) != ahx->authsize + rplen - AH_FLENGTH)
    {
	DPRINTF(("ah_input(): bad authenticator length %d for packet in SA %s/%08x\n", hl * sizeof(u_int32_t), ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_badauthl++;
	m_freem(m);
	return EACCES;
    }

    /* Update the counters */
    tdb->tdb_cur_bytes += (m->m_pkthdr.len - skip - hl * sizeof(u_int32_t));
    ahstat.ahs_ibytes += (m->m_pkthdr.len - skip - hl * sizeof(u_int32_t));

    /* Hard expiration */
    if ((tdb->tdb_flags & TDBF_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_exp_bytes))
      {
	  pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	  tdb_delete(tdb, 0, TDBEXP_TIMEOUT);
	  m_freem(m);
	  return ENXIO;
      }

    /* Notify on expiration */
    if ((tdb->tdb_flags & TDBF_SOFT_BYTES) &&
	(tdb->tdb_cur_bytes >= tdb->tdb_soft_bytes))
    {
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;     /* Turn off checking */
    }

    /* Get crypto descriptors */
    crp = crypto_getreq(1);
    if (crp == NULL)
    {
	m_freem(m);
	DPRINTF(("ah_input(): failed to acquire crypto descriptors\n"));
	ahstat.ahs_crypto++;
	return ENOBUFS;
    }

    crda = crp->crp_desc;

    crda->crd_skip = 0;
    crda->crd_len = m->m_pkthdr.len;
    crda->crd_inject = skip + rplen;

    /* Authentication operation */
    crda->crd_alg = ahx->type;
    crda->crd_key = tdb->tdb_amxkey;
    crda->crd_klen = tdb->tdb_amxkeylen * 8;

    /*
     * Save the authenticator, the skipped portion of the packet, and the
     * AH header.
     */
    MALLOC(crp->crp_opaque4, caddr_t, skip + rplen + ahx->authsize,
	   M_XDATA, M_DONTWAIT);
    if (crp->crp_opaque4 == 0)
    {
	m_freem(m);
	crypto_freereq(crp);
	DPRINTF(("ah_input(): failed to allocate auth array\n"));
	ahstat.ahs_crypto++;
	return ENOBUFS;
    }

    /* Save data */
    m_copydata(m, 0, skip + rplen + ahx->authsize, crp->crp_opaque4);

    /* Zeroize the authenticator on the packet */
    m_copyback(m, skip + rplen, ahx->authsize, ipseczeroes);

    /* "Massage" the packet headers for crypto processing */
    if ((btsx = ah_massage_headers(&m, tdb->tdb_dst.sa.sa_family,
				   skip, ahx->type, 0)) != 0)
    {
	/* mbuf will be free'd by callee */
	FREE(crp->crp_opaque4, M_XDATA);

	crypto_freereq(crp);
	return btsx;
    }

    tdb->tdb_ref++;

    /* Crypto operation descriptor */
    crp->crp_ilen = m->m_pkthdr.len; /* Total input length */
    crp->crp_flags = CRYPTO_F_IMBUF;
    crp->crp_buf = (caddr_t) m;
    crp->crp_callback = (int (*) (struct cryptop *)) ah_input_cb;
    crp->crp_sid = tdb->tdb_cryptoid;

    /* These are passed as-is to the callback */
    crp->crp_opaque1 = (caddr_t) tdb;
    crp->crp_opaque2 = (caddr_t) skip;
    crp->crp_opaque3 = (caddr_t) protoff;

    return crypto_dispatch(crp);
}

/*
 * AH input callback, called directly by the crypto driver.
 */
int
ah_input_cb(void *op)
{ 
    int roff, rplen, error, skip, protoff;
    unsigned char calc[AH_ALEN_MAX];
    struct mbuf *m1, *m0, *m;
    struct cryptodesc *crd;
    struct auth_hash *ahx;
    struct cryptop *crp;
    struct tdb *tdb;

    crp = (struct cryptop *) op;
    crd = crp->crp_desc;
    tdb = (struct tdb *) crp->crp_opaque1;
    ahx = (struct auth_hash *) tdb->tdb_authalgxform;
    skip = (int) crp->crp_opaque2;
    protoff = (int) crp->crp_opaque3;
    m = (struct mbuf *) crp->crp_buf;

    tdb->tdb_ref--;

    /* Check for crypto errors */
    if (crp->crp_etype)
    {
        if (tdb->tdb_cryptoid != 0)
	  tdb->tdb_cryptoid = crp->crp_sid;

	if (crp->crp_etype == EAGAIN)
	{
	    tdb->tdb_ref++;
	    return crypto_dispatch(crp);
	}

	ahstat.ahs_noxform++;
	DPRINTF(("ah_input_cb(): crypto error %d\n", crp->crp_etype));
	error = crp->crp_etype;
	goto baddone;
    }

    /* Shouldn't happen... */
    if (!m)
    {
	ahstat.ahs_crypto++;
	DPRINTF(("ah_input_cb(): bogus returned buffer from crypto\n"));
	error = EINVAL;
	goto baddone;
    }

    /*
     * Check that the TDB is still valid -- not really an error, but
     * we need to handle it as such. It may happen if the TDB expired
     * or was deleted while there was a pending request in the crypto
     * queue.
     */
    if (tdb->tdb_flags & TDBF_INVALID)
    {
	ahstat.ahs_invalid++;
	tdb_delete(tdb, 0, 0);
	error = ENXIO;
	DPRINTF(("ah_input_cb(): TDB expired while processing crypto\n"));
	goto baddone;
    }

    if (!(tdb->tdb_flags & TDBF_NOREPLAY))
      rplen = AH_FLENGTH + sizeof(u_int32_t);
    else
      rplen = AH_FLENGTH;

    /* Copy computed authenticator */
    m_copydata(m, skip + rplen, ahx->authsize, calc);

    /* Verify authenticator */
    if (bcmp(crp->crp_opaque4 + skip + rplen, calc, ahx->authsize))
    {
	DPRINTF(("ah_input(): authentication failed for packet in SA %s/%08x\n", ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_badauth++;
	error = EACCES;
	goto baddone;
    }

    /* Fix the Next Protocol field */
    ((u_int8_t *) crp->crp_opaque4)[protoff] =
				((u_int8_t *) crp->crp_opaque4)[skip];

    /* Copyback the saved (uncooked) network headers */
    m_copyback(m, 0, skip, crp->crp_opaque4);

    /* No longer needed */
    FREE(crp->crp_opaque4, M_XDATA);
    crypto_freereq(crp);

    /* Record the beginning of the AH header */
    m1 = m_getptr(m, skip, &roff);
    if (m1 == NULL)
    {
	DPRINTF(("ah_input(): bad mbuf chain for packet in SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	ahstat.ahs_hdrops++;
	m_freem(m);
	return EINVAL;
    }

    /* Remove the AH header from the mbuf */
    if (roff == 0) 
    {
	/* The AH header was conveniently at the beginning of the mbuf */
	m_adj(m1, rplen + ahx->authsize);
	if (!(m1->m_flags & M_PKTHDR))
	  m->m_pkthdr.len -= rplen + ahx->authsize;
    }
    else
      if (roff + rplen + ahx->authsize >= m1->m_len)
      {
	  /*
	   * Part or all of the AH header is at the end of this mbuf, so first
	   * let's remove the remainder of the AH header from the
	   * beginning of the remainder of the mbuf chain, if any.
	   */
	  if (roff + rplen + ahx->authsize > m1->m_len)
	  {
	      /* Adjust the next mbuf by the remainder */
	      m_adj(m1->m_next, roff + rplen + ahx->authsize - m1->m_len);

	      /* The second mbuf is guaranteed not to have a pkthdr... */
	      m->m_pkthdr.len -= (roff + rplen + ahx->authsize - m1->m_len);
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
	  bcopy(mtod(m1, u_char *) + roff + rplen + ahx->authsize,
		mtod(m1, u_char *) + roff,
		m1->m_len - (roff + rplen + ahx->authsize));
	  m1->m_len -= rplen + ahx->authsize;
	  m->m_pkthdr.len -= rplen + ahx->authsize;
      }

    return ipsec_common_input_cb(m, tdb, skip, protoff);

 baddone:
    if (m)
      m_freem(m);

    /* We have to free this manually */
    if (crp && crp->crp_opaque4)
      FREE(crp->crp_opaque4, M_XDATA);

    crypto_freereq(crp);

    return error;
}

/*
 * AH output routine, called by ipsp_process_packet().
 */
int
ah_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int skip,
	  int protoff)
{
    struct auth_hash *ahx = (struct auth_hash *) tdb->tdb_authalgxform;
    struct cryptodesc *crda;
    struct mbuf *mo, *mi;
    struct cryptop *crp;
    u_int16_t iplen;
    int len, rplen;
    struct ah *ah;

#if NBPFILTER > 0
    {
	struct ifnet *ifn;
	struct enchdr hdr;
	struct mbuf m1;

	bzero (&hdr, sizeof(hdr));

	hdr.af = tdb->tdb_dst.sa.sa_family;
	hdr.spi = tdb->tdb_spi;
	hdr.flags |= M_AUTH;

	m1.m_next = m;
	m1.m_len = ENC_HDRLEN;
	m1.m_data = (char *) &hdr;

	if (tdb->tdb_interface)
	  ifn = (struct ifnet *) tdb->tdb_interface;
	else
	  ifn = &(encif[0].sc_if);

	if (ifn->if_bpf)
	  bpf_mtap(ifn->if_bpf, &m1);
    }
#endif

    ahstat.ahs_output++;

    /* Check for replay counter wrap-around in automatic (not manual) keying */
    if ((tdb->tdb_rpl == 0) && (tdb->tdb_wnd > 0) &&
	(!(tdb->tdb_flags & TDBF_NOREPLAY)))
    {
	DPRINTF(("ah_output(): SA %s/%08x should have expired\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_wrap++;
	return NULL;
    }

    if (!(tdb->tdb_flags & TDBF_NOREPLAY))
      rplen = AH_FLENGTH + sizeof(u_int32_t);
    else
      rplen = AH_FLENGTH;

    switch (tdb->tdb_dst.sa.sa_family)
    {
#ifdef INET
	case AF_INET:
	    /* Check for IP maximum packet size violations */
	    if (rplen + ahx->authsize + m->m_pkthdr.len > IP_MAXPACKET)
	    {
		DPRINTF(("ah_output(): packet in SA %s/%08x got too big\n",
			 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		m_freem(m);
		ahstat.ahs_toobig++;
		return EMSGSIZE;
	    }
	    break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
	    /* Check for IPv6 maximum packet size violations */
	    if (rplen + ahx->authsize + m->m_pkthdr.len > IPV6_MAXPACKET)
	    {
		DPRINTF(("ah_output(): packet in SA %s/%08x got too big\n",
			 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
		m_freem(m);
		ahstat.ahs_toobig++;
		return EMSGSIZE;
	    }
	    break;
#endif /* INET6 */

	default:
	    DPRINTF(("ah_output(): unknown/unsupported protocol family %d, SA %s/%08x\n", tdb->tdb_dst.sa.sa_family, ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	    m_freem(m);
	    ahstat.ahs_nopf++;
	    return EPFNOSUPPORT;
    }

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
     */
    mi = m;
    while (mi != NULL && 
	   (!(mi->m_flags & M_EXT) || 
	    (mi->m_ext.ext_ref == NULL &&
	     mclrefcnt[mtocl(mi->m_ext.ext_buf)] <= 1)))
    {
        mo = mi;
        mi = mi->m_next;
    }
     
    if (mi != NULL)
    {
        /* Replace the rest of the mbuf chain. */
        struct mbuf *n = m_copym2(mi, 0, M_COPYALL, M_DONTWAIT);
      
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

        m_freem(mi);
    }

    /* Inject AH header */
    mi = m_inject(m, skip, rplen + ahx->authsize, M_DONTWAIT);
    if (mi == NULL)
    {
	DPRINTF(("ah_output(): failed to inject AH header for SA %s/%08x\n",
		 ipsp_address(tdb->tdb_dst), ntohl(tdb->tdb_spi)));
	m_freem(m);
	ahstat.ahs_wrap++;
	return ENOBUFS;
    }

    /*
     * The AH header is guaranteed by m_inject() to be in contiguous memory,
     * at the beginning of the returned mbuf.
     */
    ah = mtod(mi, struct ah *);
    
    /* Initialize the AH header */
    m_copydata(m, protoff, sizeof(u_int8_t), (caddr_t) &ah->ah_nh);
    ah->ah_hl = (rplen + ahx->authsize - AH_FLENGTH) / sizeof(u_int32_t);
    ah->ah_rv = 0;
    ah->ah_spi = tdb->tdb_spi;

    /* Zeroize authenticator */
    m_copyback(m, skip + rplen, ahx->authsize, ipseczeroes);

    if (!(tdb->tdb_flags & TDBF_NOREPLAY))
      ah->ah_rpl = htonl(tdb->tdb_rpl++);

    /* Get crypto descriptors */
    crp = crypto_getreq(1);
    if (crp == NULL)
    {
	m_freem(m);
	DPRINTF(("ah_output(): failed to acquire crypto descriptors\n"));
	ahstat.ahs_crypto++;
	return ENOBUFS;
    }

    crda = crp->crp_desc;

    crda->crd_skip = 0;
    crda->crd_inject = skip + rplen;
    crda->crd_len = m->m_pkthdr.len;

    /* Authentication operation */
    crda->crd_alg = ahx->type;
    crda->crd_key = tdb->tdb_amxkey;
    crda->crd_klen = tdb->tdb_amxkeylen * 8;

    /* Save the skipped portion of the packet */
    MALLOC(crp->crp_opaque4, caddr_t, skip, M_XDATA, M_DONTWAIT);
    if (crp->crp_opaque4 == 0)
    {
	m_freem(m);
	crypto_freereq(crp);
	DPRINTF(("ah_output(): failed to allocate auth array\n"));
	ahstat.ahs_crypto++;
	return ENOBUFS;
    }
    else
      m_copydata(m, 0, skip, crp->crp_opaque4);

    /*
     * Fix IP header length on the header used for authentication. We don't
     * need to fix the original header length as it will be fixed by our
     * caller.
     */
    switch (tdb->tdb_dst.sa.sa_family)
    {
#ifdef INET
	case AF_INET:
	    bcopy(crp->crp_opaque4 + offsetof(struct ip, ip_len),
		  (caddr_t) &iplen, sizeof(u_int16_t));
	    iplen = htons(ntohs(iplen) + rplen + ahx->authsize);
	    m_copyback(m, offsetof(struct ip, ip_len), sizeof(u_int16_t),
		       (caddr_t) &iplen);
	    break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
	    bcopy(crp->crp_opaque4 + offsetof(struct ip6_hdr, ip6_plen),
		  (caddr_t) &iplen, sizeof(u_int16_t));
	    iplen = htons(ntohs(iplen) + rplen + ahx->authsize);
	    m_copyback(m, offsetof(struct ip6_hdr, ip6_plen),
		       sizeof(u_int16_t), (caddr_t) &iplen);
	    break;
#endif /* INET6 */
    }

    tdb->tdb_ref++;

    /* Update the Next Protocol field in the IP header and the saved data */
    len = IPPROTO_AH;
    m_copyback(m, protoff, sizeof(u_int8_t), (caddr_t) &len);
    ((u_int8_t *) crp->crp_opaque4)[protoff] = IPPROTO_AH;

    /* "Massage" the packet headers for crypto processing */
    if ((len = ah_massage_headers(&m, tdb->tdb_dst.sa.sa_family,
				  skip, ahx->type, 1)) != 0)
    {
	/* mbuf will be free'd by callee */
	FREE(crp->crp_opaque4, M_XDATA);
	crypto_freereq(crp);
	return len;
    }

    /* Crypto operation descriptor */
    crp->crp_ilen = m->m_pkthdr.len; /* Total input length */
    crp->crp_flags = CRYPTO_F_IMBUF;
    crp->crp_buf = (caddr_t) m;
    crp->crp_callback = (int (*) (struct cryptop *)) ah_output_cb;
    crp->crp_sid = tdb->tdb_cryptoid;

    /* These are passed as-is to the callback */
    crp->crp_opaque1 = (caddr_t) tdb;
    crp->crp_opaque2 = (caddr_t) skip;
    crp->crp_opaque3 = (caddr_t) protoff;

    return crypto_dispatch(crp);
}

/*
 * AH output callback, called directly from the crypto handler.
 */
int
ah_output_cb(void *op)
{
    int skip, protoff, error;
    struct cryptop *crp;
    struct tdb *tdb;
    struct mbuf *m;

    crp = (struct cryptop *) op;
    tdb = (struct tdb *) crp->crp_opaque1;
    skip = (int) crp->crp_opaque2;
    protoff = (int) crp->crp_opaque3;
    m = (struct mbuf *) crp->crp_buf;

    tdb->tdb_ref--;

    /* Check for crypto errors */
    if (crp->crp_etype)
    {
        if (tdb->tdb_cryptoid != 0)
	  tdb->tdb_cryptoid = crp->crp_sid;

	if (crp->crp_etype == EAGAIN)
	{
	    tdb->tdb_ref++;
	    return crypto_dispatch(crp);
	}

	ahstat.ahs_noxform++;
	DPRINTF(("ah_output_cb(): crypto error %d\n", crp->crp_etype));
	error = crp->crp_etype;
	goto baddone;
    }

    /* Shouldn't happen... */
    if (!m)
    {
	ahstat.ahs_crypto++;
	DPRINTF(("ah_output_cb(): bogus returned buffer from crypto\n"));
	error = EINVAL;
	goto baddone;
    }

    /*
     * Check that the TDB is still valid -- not really an error, but
     * we need to handle it as such. It may happen if the TDB expired
     * or was deleted while there was a pending request in the crypto
     * queue.
     */
    if (tdb->tdb_flags & TDBF_INVALID)
    {
	ahstat.ahs_invalid++;
	tdb_delete(tdb, 0, 0);
	error = ENXIO;
	DPRINTF(("ah_output_cb(): TDB expired while processing crypto\n"));
	goto baddone;
    }

    /* Copy original headers (with the new protocol number) back in place */
    m_copyback(m, 0, skip, crp->crp_opaque4);

    /* No longer needed */
    FREE(crp->crp_opaque4, M_XDATA);
    crypto_freereq(crp);

    return ipsp_process_done(m, tdb);

 baddone:
    if (m)
      m_freem(m);

    /* We have to free this manually */
    if (crp && crp->crp_opaque4)
      FREE(crp->crp_opaque4, M_XDATA);

    crypto_freereq(crp);

    return error;
}
