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

struct cryptop *cryptop_queue = NULL;
struct cryptodesc *cryptodesc_queue = NULL;
int crypto_queue_num = 0;
int crypto_queue_max = CRYPTO_MAX_CACHED;

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
