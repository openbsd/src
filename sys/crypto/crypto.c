/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software. 
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
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/md5k.h>
#include <dev/rndvar.h>
#include <crypto/sha1.h>
#include <crypto/rmd160.h>
#include <crypto/cast.h>
#include <crypto/skipjack.h>
#include <crypto/blf.h>
#include <crypto/crypto.h>
#include <crypto/xform.h>

struct cryptocap *crypto_drivers = NULL;
int crypto_drivers_num = 0;

struct swcr_data **swcr_sessions = NULL;
u_int32_t swcr_sesnum = 0;
int32_t swcr_id = -1;

struct cryptop *cryptop_queue = NULL;
struct cryptodesc *cryptodesc_queue = NULL;
int crypto_queue_num = 0;
int crypto_queue_max = CRYPTO_MAX_CACHED;

u_int8_t hmac_ipad_buffer[64] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36 };

u_int8_t hmac_opad_buffer[64] = {
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C };

/*
 * Create a new session.
 */
int
crypto_newsession(u_int64_t *sid, struct cryptoini *cri)
{
    struct cryptoini *cr;
    u_int32_t hid, lid;
    int err;

    if (crypto_drivers == NULL)
      return EINVAL;

    /*
     * The algorithm we use here is pretty stupid; just use the
     * first driver that supports all the algorithms we need.
     *
     * XXX We need more smarts here (in real life too, but that's
     * XXX another story altogether).
     */

    for (hid = 0; hid < crypto_drivers_num; hid++)
    {
	/*
         * If it's not initialized or has remaining sessions referencing
         * it, skip.
         */
	if ((crypto_drivers[hid].cc_newsession == NULL) ||
	    (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP))
	  continue;

	/* See if all the algorithms are supported */
	for (cr = cri; cr; cr = cr->cri_next)
	  if (crypto_drivers[hid].cc_alg[cr->cri_alg] == 0)
	    break;

	/* Ok, all algorithms are supported */
	if (cr == NULL)
	  break;
    }

    /*
     * Can't do everything in one session.
     *
     * XXX Fix this. We need to inject a "virtual" session layer right
     * XXX about here.
     */

    if (hid == crypto_drivers_num)
      return EINVAL;

    /* Call the driver initialization routine */
    lid = hid; /* Pass the driver ID */
    err = crypto_drivers[hid].cc_newsession(&lid, cri);
    if (err == 0)
    {
	(*sid) = hid;
	(*sid) <<= 31;
	(*sid) |= (lid & 0xffffffff);
        crypto_drivers[hid].cc_sessions++;
    }

    return err;
}

/*
 * Delete an existing session (or a reserved session on an unregistered
 * driver).
 */
int
crypto_freesession(u_int64_t sid)
{
    u_int32_t hid, lid;
    int err = 0;

    if (crypto_drivers == NULL)
      return EINVAL;

    /* Determine two IDs */
    hid = (sid >> 31) & 0xffffffff;
    lid = sid & 0xffffffff;

    if (hid >= crypto_drivers_num)
      return ENOENT;

    if (crypto_drivers[hid].cc_sessions)
      crypto_drivers[hid].cc_sessions--;

    /* Call the driver cleanup routine, if available */
    if (crypto_drivers[hid].cc_freesession)
      err = crypto_drivers[hid].cc_freesession(lid);

    /*
     * If this was the last session of a driver marked as invalid, make
     * the entry available for reuse.
     */
    if ((crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP) &&
	(crypto_drivers[hid].cc_sessions == 0))
      bzero(&crypto_drivers[hid], sizeof(struct cryptocap));

    return err;
}

/*
 * Find an empty slot.
 */
int32_t
crypto_get_driverid(void)
{
    struct cryptocap *newdrv;
    int i;

    if (crypto_drivers_num == 0)
    {
        crypto_drivers_num = CRYPTO_DRIVERS_INITIAL;
	MALLOC(crypto_drivers, struct cryptocap *, 
	       crypto_drivers_num * sizeof(struct cryptocap), M_XDATA,
	       M_DONTWAIT);
	if (crypto_drivers == NULL)
	{
	    crypto_drivers_num = 0;
	    return -1;
	}

	bzero(crypto_drivers, crypto_drivers_num * sizeof(struct cryptocap));
    }

    for (i = 0; i < crypto_drivers_num; i++)
      if ((crypto_drivers[i].cc_process == NULL) &&
	  !(crypto_drivers[i].cc_flags & CRYPTOCAP_F_CLEANUP) &&
	  (crypto_drivers[i].cc_sessions == 0))
	return i;

    /* Out of entries, allocate some more */
    if (i == crypto_drivers_num)
    {
	/* Be careful about wrap-around */
	if (2 * crypto_drivers_num <= crypto_drivers_num)
	  return -1;

	MALLOC(newdrv, struct cryptocap *,
	       2 * crypto_drivers_num * sizeof(struct cryptocap),
	       M_XDATA, M_DONTWAIT);
	if (newdrv == NULL)
	  return -1;

        bcopy(crypto_drivers, newdrv,
	      crypto_drivers_num * sizeof(struct cryptocap));
	bzero(&newdrv[crypto_drivers_num],
	      crypto_drivers_num * sizeof(struct cryptocap));
	crypto_drivers_num *= 2;
	return i;
    }

    /* Shouldn't really get here... */
    return -1;
}

/*
 * Register a crypto driver. It should be called once for each algorithm
 * supported by the driver.
 */
int
crypto_register(u_int32_t driverid, int alg, void *newses, void *freeses,
		void *process)
{
    if ((driverid >= crypto_drivers_num) || (alg <= 0) ||
	(alg > CRYPTO_ALGORITHM_MAX) || (crypto_drivers == NULL))
      return EINVAL;

    /*
     * XXX Do some performance testing to determine placing.
     * XXX We probably need an auxiliary data structure that describes
     * XXX relative performances.
     */

    crypto_drivers[driverid].cc_alg[alg] = 1;

    if (crypto_drivers[driverid].cc_process == NULL)
    {
	crypto_drivers[driverid].cc_newsession =
			(int (*) (u_int32_t *, struct cryptoini *)) newses;
	crypto_drivers[driverid].cc_process =
			(int (*) (struct cryptop *)) process;
	crypto_drivers[driverid].cc_freesession =
			(int (*) (u_int32_t)) freeses;
    }

    return 0;
}

/*
 * Unregister a crypto driver. If there are pending sessions using it,
 * leave enough information around so that subsequent calls using those
 * sessions will correctly detect the driver being unregistered and reroute
 * the request.
 */
int
crypto_unregister(u_int32_t driverid, int alg)
{
    u_int32_t ses;
    int i;

    /* Sanity checks */
    if ((driverid >= crypto_drivers_num) || (alg <= 0) ||
        (alg > CRYPTO_ALGORITHM_MAX) || (crypto_drivers == NULL) ||
	(crypto_drivers[driverid].cc_alg[alg] == 0))
      return EINVAL;

    crypto_drivers[driverid].cc_alg[alg] = 0;

    /* Was this the last algorithm ? */
    for (i = 1; i <= CRYPTO_ALGORITHM_MAX; i++)
      if (crypto_drivers[driverid].cc_alg[i] != 0)
	break;

    if (i == CRYPTO_ALGORITHM_MAX + 1) 
    {
	ses = crypto_drivers[driverid].cc_sessions;
        bzero(&crypto_drivers[driverid], sizeof(struct cryptocap));

        if (ses != 0)
	{
            /* If there are pending sessions, just mark as invalid */
            crypto_drivers[driverid].cc_flags |= CRYPTOCAP_F_CLEANUP;
            crypto_drivers[driverid].cc_sessions = ses;
	}
    }

    return 0;
}

/*
 * Dispatch a crypto request to the appropriate crypto devices.
 */
int
crypto_dispatch(struct cryptop *crp)
{
    struct cryptodesc *crd;
    u_int64_t nid;
    u_int32_t hid;

    /* Sanity checks */
    if ((crp == NULL) || (crp->crp_callback == NULL))
      return EINVAL;

    if ((crp->crp_desc == NULL) || (crypto_drivers == NULL))
    {
	crp->crp_etype = EINVAL;
	return crp->crp_callback(crp);
    }

    hid = (crp->crp_sid >> 31) & 0xffffffff;

    if (hid >= crypto_drivers_num)
    {
	/* Migrate session */
	for (crd = crp->crp_desc; crd->crd_next; crd = crd->crd_next)
	  crd->CRD_INI.cri_next = &(crd->crd_next->CRD_INI);
	if (crypto_newsession(&nid, &(crp->crp_desc->CRD_INI)) == 0)
	  crp->crp_sid = nid;

	crp->crp_etype = EAGAIN;
	return crp->crp_callback(crp);
    }

    if (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP)
      crypto_freesession(crp->crp_sid);

    if (crypto_drivers[hid].cc_process == NULL)
    {
	/* Migrate session */
	for (crd = crp->crp_desc; crd->crd_next; crd = crd->crd_next)
	  crd->CRD_INI.cri_next = &(crd->crd_next->CRD_INI);
	if (crypto_newsession(&nid, &(crp->crp_desc->CRD_INI)) == 0)
	  crp->crp_sid = nid;

	crp->crp_etype = EAGAIN;
	return crp->crp_callback(crp);
    }

    return crypto_drivers[hid].cc_process(crp);
}

/*
 * Release a set of crypto descriptors.
 */
void
crypto_freereq(struct cryptop *crp)
{
    struct cryptodesc *crd;

    if (crp == NULL)
      return;

    while ((crd = crp->crp_desc) != NULL)
    {
	crp->crp_desc = crd->crd_next;

	if (crypto_queue_num + 1 > crypto_queue_max)
	  FREE(crd, M_XDATA);
	else
	{
	    crd->crd_next = cryptodesc_queue;
	    cryptodesc_queue = crd;
	    crypto_queue_num++;
	}
    }

    if (crypto_queue_num + 1 > crypto_queue_max)
      FREE(crp, M_XDATA);
    else
    {
        crp->crp_next = cryptop_queue;
        cryptop_queue = crp;
        crypto_queue_num++;
    }
}

/*
 * Acquire a set of crypto descriptors.
 */
struct cryptop *
crypto_getreq(int num)
{
    struct cryptodesc *crd;
    struct cryptop *crp;

    if (cryptop_queue == NULL)
    {
        MALLOC(crp, struct cryptop *, sizeof(struct cryptop), M_XDATA,
	       M_DONTWAIT);
        if (crp == NULL)
          return NULL;
    }
    else
    {
	crp = cryptop_queue;
	cryptop_queue = crp->crp_next;
        crypto_queue_num--;
    }

    bzero(crp, sizeof(struct cryptop));

    while (num--)
    {
        if (cryptodesc_queue == NULL)
	{
	    MALLOC(crd, struct cryptodesc *, sizeof(struct cryptodesc),
		   M_XDATA, M_DONTWAIT);
	    if (crd == NULL)
	    {
		crypto_freereq(crp);
	        return NULL;
	    }
	}
	else
	{
	    crd = cryptodesc_queue;
	    cryptodesc_queue = crd->crd_next;
	    crypto_queue_num--;
	}

	bzero(crd, sizeof(struct cryptodesc));
	crd->crd_next = crp->crp_desc;
	crp->crp_desc = crd;
    }

    return crp;
}

/*
 * Apply a symmetric encryption/decryption algorithm.
 */
int
swcr_encdec(struct cryptodesc *crd, struct swcr_data *sw, caddr_t buf,
	    int outtype)
{
    unsigned char iv[EALG_MAX_BLOCK_LEN], blk[EALG_MAX_BLOCK_LEN], *idat;
    unsigned char *ivp, piv[EALG_MAX_BLOCK_LEN];
    struct enc_xform *exf;
    int i, k, j, blks;
    struct mbuf *m;

    exf = sw->sw_exf;
    blks = exf->blocksize;

    /* Check for non-padded data */
    if (crd->crd_len % blks)
      return EINVAL;

    if (outtype == CRYPTO_BUF_CONTIG)
    {
	if (crd->crd_flags & CRD_F_ENCRYPT)
	{
	    /* Inject IV */
	    if (crd->crd_flags & CRD_F_HALFIV)
	    {
		/* "Cook" half-IV */
		for (k = 0; k < blks / 2; k++)
		  sw->sw_iv[(blks / 2) + k] = ~sw->sw_iv[k];

	        bcopy(sw->sw_iv, buf + crd->crd_inject, blks / 2);
	    }
	    else
	      bcopy(sw->sw_iv, buf + crd->crd_inject, blks);

	    for (i = crd->crd_skip;
		 i < crd->crd_skip + crd->crd_len;
		 i += blks)
	    {
		/* XOR with the IV/previous block, as appropriate. */
		if (i == crd->crd_skip)
		  for (k = 0; k < blks; k++)
		    buf[i + k] ^= sw->sw_iv[k];
		else
		  for (k = 0; k < blks; k++)
		    buf[i + k] ^= buf[i + k - blks];

		exf->encrypt(sw->sw_kschedule, buf + i);
	    }

	    /* Keep the last block */
	    bcopy(buf + crd->crd_len - blks, sw->sw_iv, blks);
	}
	else /* Decrypt */
	{
	    /* Copy the IV off the buffer */
	    bcopy(buf + crd->crd_inject, sw->sw_iv, blks);

	    /* "Cook" half-IV */
	    if (crd->crd_flags & CRD_F_HALFIV)
	      for (k = 0; k < blks / 2; k++)
		sw->sw_iv[(blks / 2) + k] = ~sw->sw_iv[k];

	    /*
	     * Start at the end, so we don't need to keep the encrypted
	     * block as the IV for the next block.
	     */
	    for (i = crd->crd_skip + crd->crd_len - blks;
		 i >= crd->crd_skip;
		 i -= blks)
	    {
		exf->decrypt(sw->sw_kschedule, buf + i);

		/* XOR with the IV/previous block, as appropriate */
		if (i == crd->crd_skip)
		  for (k = 0; k < blks; k++)
		    buf[i + k] ^= sw->sw_iv[k];
		else
		  for (k = 0; k < blks; k++)
		    buf[i + k] ^= buf[i + k - blks];
	    }
	}

	return 0; /* Done with contiguous buffer encryption/decryption */
    }
    else /* mbuf */
    {
	m = (struct mbuf *) buf;

	/* Initialize the IV */
	if (crd->crd_flags & CRD_F_ENCRYPT)
	{
	    bcopy(sw->sw_iv, iv, blks);

	    /* "Cook" half-IV */
	    if (crd->crd_flags & CRD_F_HALFIV)
	      for (k = 0; k < blks / 2; k++)
	        iv[(blks / 2) + k] = ~iv[k];

	    /* Inject IV */
	    m_copyback(m, crd->crd_inject, blks, iv);
	}
	else
	{
	    m_copydata(m, crd->crd_inject, blks, iv); /* Get IV off mbuf */

	    /* "Cook" half-IV */
	    if (crd->crd_flags & CRD_F_HALFIV)
	      for (k = 0; k < blks / 2; k++)
	        iv[(blks / 2) + k] = ~iv[k];
	}

	ivp = iv;

	/* Find beginning of data */
	m = m_getptr(m, crd->crd_skip, &k);
	if (m == NULL)
	  return EINVAL;

	i = crd->crd_len;

	while (i > 0)
	{
	    /*
	     * If there's insufficient data at the end of an mbuf, we have
	     * to do some copying.
	     */
	    if ((m->m_len < k + blks) && (m->m_len != k))
	    {
		m_copydata(m, k, blks, blk);

		/* Actual encryption/decryption */
		if (crd->crd_flags & CRD_F_ENCRYPT)
		{
		    /* XOR with previous block */
		    for (j = 0; j < blks; j++)
		      blk[j] ^= ivp[j];

		    exf->encrypt(sw->sw_kschedule, blk);

		    /* Keep encrypted block for XOR'ing with next block */
		    bcopy(blk, iv, blks);
		    ivp = iv;
		}
		else /* decrypt */
		{
		    /* Keep encrypted block for XOR'ing with next block */
		    if (ivp == iv)
		      bcopy(blk, piv, blks);
		    else
		      bcopy(blk, iv, blks);

		    exf->decrypt(sw->sw_kschedule, blk);

		    /* XOR with previous block */
		    for (j = 0; j < blks; j++)
		      blk[j] ^= ivp[j];

		    if (ivp == iv)
		      bcopy(piv, iv, blks);
		    else
		      ivp = iv;
		}

		/* Copy back decrypted block */
		m_copyback(m, k, blks, blk);

		/* Advance pointer */
		m = m_getptr(m, k + blks, &k);
		if (m == NULL)
		  return EINVAL;

		i -= blks;

		/* Could be done... */
		if (i == 0)
		  break;
	    }

	    /* Skip possibly empty mbufs */
	    if (k == m->m_len)
	    {
		for (m = m->m_next; m && m->m_len == 0; m = m->m_next)
		  ;

		k = 0;
	    }

	    /* Sanity check */
	    if (m == NULL)
	      return EINVAL;

	    /*
	     * Warning: idat may point to garbage here, but we only use it
	     * in the while() loop, only if there are indeed enough data.
	     */
	    idat = mtod(m, unsigned char *) + k;

	    while ((m->m_len >= k + blks) && (i > 0))
	    {
		if (crd->crd_flags & CRD_F_ENCRYPT)
		{
		    /* XOR with previous block/IV */
		    for (j = 0; j < blks; j++)
		      idat[j] ^= ivp[j];

		    exf->encrypt(sw->sw_kschedule, idat);
		    ivp = idat;
		}
		else /* decrypt */
		{
		    /*
		     * Keep encrypted block to be used in next block's
		     * processing.
		     */
		    if (ivp == iv)
		      bcopy(idat, piv, blks);
		    else
		      bcopy(idat, iv, blks);

		    exf->decrypt(sw->sw_kschedule, idat);

		    /* XOR with previous block/IV */
		    for (j = 0; j < blks; j++)
		      idat[j] ^= ivp[j];

		    if (ivp == iv)
		      bcopy(piv, iv, blks);
		    else
		      ivp = iv;
		}

		idat += blks;
		k += blks;
		i -= blks;
	    }
	}

	/* Keep the last block */
	if (crd->crd_flags & CRD_F_ENCRYPT)
	  bcopy(ivp, sw->sw_iv, blks);

	return 0; /* Done with mbuf encryption/decryption */
    }

    /* Unreachable */
    return EINVAL;
}

/*
 * Compute keyed-hash authenticator.
 */
int
swcr_authcompute(struct cryptodesc *crd, struct swcr_data *sw,
		 caddr_t buf, int outtype)
{
    unsigned char aalg[AALG_MAX_RESULT_LEN];
    struct auth_hash *axf;
    union authctx ctx;
    int err;

    if (sw->sw_ictx == 0)
      return EINVAL;

    axf = sw->sw_axf;

    bcopy(sw->sw_ictx, &ctx, axf->ctxsize);

    if (outtype == CRYPTO_BUF_CONTIG)
    {
	axf->Update(&ctx, buf + crd->crd_skip, crd->crd_len);
	axf->Final(aalg, &ctx);
    }
    else
    {
	err = m_apply((struct mbuf *) buf, crd->crd_skip,
		      crd->crd_len,
		      (int (*)(caddr_t, caddr_t, unsigned int)) axf->Update,
		      (caddr_t) &ctx);
	if (err)
	  return err;

	axf->Final(aalg, &ctx);
    }

    /* HMAC processing */
    switch (sw->sw_alg)
    {
	case CRYPTO_MD5_HMAC96:
	case CRYPTO_SHA1_HMAC96:
	case CRYPTO_RIPEMD160_HMAC96:
	    if (sw->sw_octx == NULL)
	      return EINVAL;

	    bcopy(sw->sw_octx, &ctx, axf->ctxsize);
	    axf->Update(&ctx, aalg, axf->hashsize);
	    axf->Final(aalg, &ctx);
	    break;
    }

    /* Inject the authentication data */
    if (outtype == CRYPTO_BUF_CONTIG)
      bcopy(aalg, buf + crd->crd_inject, axf->authsize);
    else
      m_copyback((struct mbuf *) buf, crd->crd_inject, axf->authsize, aalg);

    return 0;
}

/*
 * Generate a new software session.
 */
int
swcr_newsession(u_int32_t *sid, struct cryptoini *cri)
{
    struct swcr_data **swd;
    struct auth_hash *axf;
    struct enc_xform *txf;
    u_int32_t i;
    int k;

    if ((sid == NULL) || (cri == NULL))
      return EINVAL;

    if (swcr_sessions)
      for (i = 1; i < swcr_sesnum; i++)
	if (swcr_sessions[i] == NULL)
	  break;

    if ((swcr_sessions == NULL) || (i == swcr_sesnum))
    {
	if (swcr_sessions == NULL)
	{
	    i = 1; /* We leave swcr_sessions[0] empty */
	    swcr_sesnum = CRYPTO_SW_SESSIONS;
	}
	else
	  swcr_sesnum *= 2;

	MALLOC(swd, struct swcr_data **,
	       swcr_sesnum * sizeof(struct swcr_data *), M_XDATA, M_DONTWAIT);
	if (swd == NULL)
	{
	    /* Reset session number */
	    if (swcr_sesnum == CRYPTO_SW_SESSIONS)
	      swcr_sesnum = 0;
	    else
	      swcr_sesnum /= 2;

	    return ENOBUFS;
	}

	bzero(swd, swcr_sesnum * sizeof(struct swcr_data *));

	/* Copy existing sessions */
	if (swcr_sessions)
	{
	    bcopy(swcr_sessions, swd,
		  (swcr_sesnum / 2) * sizeof(struct swcr_data *));
	    FREE(swcr_sessions, M_XDATA);
	}

	swcr_sessions = swd;
    }

    swd = &swcr_sessions[i];
    *sid = i;

    while (cri)
    {
	MALLOC(*swd, struct swcr_data *, sizeof(struct swcr_data), M_XDATA,
	       M_DONTWAIT);
	if (*swd == NULL)
	{
	    swcr_freesession(i);
	    return ENOBUFS;
	}

	bzero(*swd, sizeof(struct swcr_data));

	switch (cri->cri_alg)
	{
	    case CRYPTO_DES_CBC:
		txf = &enc_xform_des;
		goto enccommon;

	    case CRYPTO_3DES_CBC:
		txf = &enc_xform_3des;
		goto enccommon;

	    case CRYPTO_BLF_CBC:
		txf = &enc_xform_blf;
		goto enccommon;

	    case CRYPTO_CAST_CBC:
		txf = &enc_xform_cast5;
		goto enccommon;

	    case CRYPTO_SKIPJACK_CBC:
		txf = &enc_xform_skipjack;

	enccommon:
		txf->setkey(&((*swd)->sw_kschedule), cri->cri_key,
			    cri->cri_klen / 8);
		MALLOC((*swd)->sw_iv, u_int8_t *, txf->blocksize, M_XDATA,
		       M_DONTWAIT);
		if ((*swd)->sw_iv == NULL)
		{
		    swcr_freesession(i);
		    return ENOBUFS;
		}

		(*swd)->sw_exf = txf;

		get_random_bytes((*swd)->sw_iv, txf->blocksize);
		break;

	    case CRYPTO_MD5_HMAC96:
		axf = &auth_hash_hmac_md5_96;
		goto authcommon;

	    case CRYPTO_SHA1_HMAC96:
		axf = &auth_hash_hmac_sha1_96;
		goto authcommon;
		
	    case CRYPTO_RIPEMD160_HMAC96:
		axf = &auth_hash_hmac_ripemd_160_96;

	authcommon:
		MALLOC((*swd)->sw_ictx, u_int8_t *, axf->ctxsize, M_XDATA,
		       M_DONTWAIT);
		if ((*swd)->sw_ictx == NULL)
		{
		    swcr_freesession(i);
		    return ENOBUFS;
		}

		MALLOC((*swd)->sw_octx, u_int8_t *, axf->ctxsize, M_XDATA,
		       M_DONTWAIT);
		if ((*swd)->sw_octx == NULL)
		{
		    swcr_freesession(i);
		    return ENOBUFS;
		}

		for (k = 0; k < cri->cri_klen / 8; k++)
		  cri->cri_key[k] ^= HMAC_IPAD_VAL;

		axf->Init((*swd)->sw_ictx);
		axf->Update((*swd)->sw_ictx, cri->cri_key,
			    cri->cri_klen / 8);
		axf->Update((*swd)->sw_ictx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (cri->cri_klen / 8));

		for (k = 0; k < cri->cri_klen / 8; k++)
		  cri->cri_key[k] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

		axf->Init((*swd)->sw_octx);
		axf->Update((*swd)->sw_octx, cri->cri_key,
			    cri->cri_klen / 8);
		axf->Update((*swd)->sw_octx, hmac_opad_buffer,
			    HMAC_BLOCK_LEN - (cri->cri_klen / 8));

		for (k = 0; k < cri->cri_klen / 8; k++)
		  cri->cri_key[k] ^= HMAC_OPAD_VAL;

		(*swd)->sw_axf = axf;
		break;

	    case CRYPTO_MD5_KPDK:
		axf = &auth_hash_key_md5;
		goto auth2common;

	    case CRYPTO_SHA1_KPDK:
		axf = &auth_hash_key_sha1;

	auth2common:
		MALLOC((*swd)->sw_ictx, u_int8_t *, axf->ctxsize, M_XDATA,
		       M_DONTWAIT);
		if ((*swd)->sw_ictx == NULL)
		{
		    swcr_freesession(i);
		    return ENOBUFS;
		}

		axf->Init((*swd)->sw_ictx);
		axf->Update((*swd)->sw_ictx, cri->cri_key,
			    cri->cri_klen / 8);
		axf->Final(NULL, (*swd)->sw_ictx);

		(*swd)->sw_axf = axf;
		break;

	    default:
		swcr_freesession(i);
		return EINVAL;
	}

	(*swd)->sw_alg = cri->cri_alg;
	cri = cri->cri_next;
	swd = &((*swd)->sw_next);
    }

    return 0;
}

/*
 * Free a session.
 */
int
swcr_freesession(u_int32_t sid)
{
    struct swcr_data *swd;
    struct enc_xform *txf;
    struct auth_hash *axf;

    if ((sid > swcr_sesnum) || (swcr_sessions == NULL) ||
	(swcr_sessions[sid] == NULL))
      return EINVAL;

    /* Silently accept and return */
    if (sid == 0)
      return 0;

    while ((swd = swcr_sessions[sid]) != NULL)
    {
        swcr_sessions[sid] = swd->sw_next;

	switch (swd->sw_alg)
	{
	    case CRYPTO_DES_CBC:
	    case CRYPTO_3DES_CBC:
	    case CRYPTO_BLF_CBC:
	    case CRYPTO_CAST_CBC:
	    case CRYPTO_SKIPJACK_CBC:
		txf = swd->sw_exf;

		if (swd->sw_kschedule)
		  txf->zerokey(&(swd->sw_kschedule));

		if (swd->sw_iv)
		  FREE(swd->sw_iv, M_XDATA);
		break;

	    case CRYPTO_MD5_HMAC96:
	    case CRYPTO_SHA1_HMAC96:
	    case CRYPTO_RIPEMD160_HMAC96:
	    case CRYPTO_MD5_KPDK:
	    case CRYPTO_SHA1_KPDK:
		axf = swd->sw_axf;

		if (swd->sw_ictx)
		{
		    bzero(swd->sw_ictx, axf->ctxsize);
		    FREE(swd->sw_ictx, M_XDATA);
		}

		if (swd->sw_octx)
		{
		    bzero(swd->sw_octx, axf->ctxsize);
		    FREE(swd->sw_octx, M_XDATA);
		}
		break;
	}

	FREE(swd, M_XDATA);
    }

    return 0;
}

/*
 * Process a software request.
 */
int
swcr_process(struct cryptop *crp)
{
    struct cryptodesc *crd;
    struct swcr_data *sw;
    u_int32_t lid;
    u_int64_t nid;
    int type;

    /* Some simple sanity checks */
    if ((crp == NULL) || (crp->crp_callback == NULL))
      return EINVAL;

    if ((crp->crp_desc == NULL) || (crp->crp_buf == NULL))
    {
	crp->crp_etype = EINVAL;
	goto done;
    }

    lid = crp->crp_sid & 0xffffffff;
    if ((lid >= swcr_sesnum) || (lid == 0) || (swcr_sessions[lid] == NULL))
    {
	crp->crp_etype = ENOENT;
	goto done;
    }

    if (crp->crp_flags & CRYPTO_F_IMBUF)
      type = CRYPTO_BUF_MBUF;
    else
      type = CRYPTO_BUF_CONTIG;

    /* Go through crypto descriptors, processing as we go */
    for (crd = crp->crp_desc; crd; crd = crd->crd_next)
    {
	/*
	 * Find the crypto context.
	 *
	 * XXX Note that the logic here prevents us from having
	 * XXX the same algorithm multiple times in a session
	 * XXX (or rather, we can but it won't give us the right
	 * XXX results). To do that, we'd need some way of differentiating
	 * XXX between the various instances of an algorithm (so we can
	 * XXX locate the correct crypto context).
	 */
	for (sw = swcr_sessions[lid];
	     sw && sw->sw_alg != crd->crd_alg;
	     sw = sw->sw_next)
	  ;

	/* No such context ? */
	if (sw == NULL)
	{
	    crp->crp_etype = EINVAL;
	    goto done;
	}

	switch (sw->sw_alg)
	{
	    case CRYPTO_DES_CBC:
	    case CRYPTO_3DES_CBC:
	    case CRYPTO_BLF_CBC:
	    case CRYPTO_CAST_CBC:
	    case CRYPTO_SKIPJACK_CBC:
		if ((crp->crp_etype = swcr_encdec(crd, sw, crp->crp_buf,
						  type)) != 0)
		  goto done;
		break;

	    case CRYPTO_MD5_HMAC96:
	    case CRYPTO_SHA1_HMAC96:
	    case CRYPTO_RIPEMD160_HMAC96:
	    case CRYPTO_MD5_KPDK:
	    case CRYPTO_SHA1_KPDK:
		if ((crp->crp_etype = swcr_authcompute(crd, sw, crp->crp_buf,
						       type)) != 0)
		  goto done;
		break;

	    default:  /* Unknown/unsupported algorithm */
		crp->crp_etype = EINVAL;
		goto done;
	}
    }

 done:
    if (crp->crp_etype == ENOENT)
    {
	crypto_freesession(crp->crp_sid); /* Just in case */

	/* Migrate session */
	for (crd = crp->crp_desc; crd->crd_next; crd = crd->crd_next)
	  crd->CRD_INI.cri_next = &(crd->crd_next->CRD_INI);
	if (crypto_newsession(&nid, &(crp->crp_desc->CRD_INI)) == 0)
	  crp->crp_sid = nid;
    }

    return crp->crp_callback(crp);
}

/*
 * Initialize the driver, called from the kernel main().
 */
void
swcr_init(void)
{
    swcr_id = crypto_get_driverid();
    if (swcr_id >= 0)
    {
	crypto_register(swcr_id, CRYPTO_DES_CBC, swcr_newsession,
                        swcr_freesession, swcr_process);
        crypto_register(swcr_id, CRYPTO_3DES_CBC, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_BLF_CBC, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_CAST_CBC, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_SKIPJACK_CBC, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_MD5_HMAC96, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_SHA1_HMAC96, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_RIPEMD160_HMAC96, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_MD5_KPDK, NULL, NULL, NULL);
        crypto_register(swcr_id, CRYPTO_SHA1_KPDK, NULL, NULL, NULL);
	return;
    }

    /* This should never happen */
    panic("Software crypto device cannot initialize!");
}
