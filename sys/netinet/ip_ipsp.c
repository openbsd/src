/*	$OpenBSD: ip_ipsp.c,v 1.103 2000/12/18 16:45:32 angelos Exp $	*/

/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr), 
 * Niels Provos (provos@physnet.uni-hamburg.de) and
 * Niklas Hallqvist (niklas@appli.se).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis and Niklas Hallqvist.
 *
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 1999 Niklas Hallqvist.
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
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#endif /* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#endif /* INET6 */

#include <net/pfkeyv2.h>

#include <netinet/ip_ipsp.h>

#include <crypto/xform.h>

#include <dev/rndvar.h>

#ifdef DDB
#include <ddb/db_output.h>
void tdb_hashstats(void);
#endif

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

#ifdef __GNUC__
#define INLINE static __inline
#endif

int		ipsp_kern __P((int, char **, int));
u_int8_t       	get_sa_require  __P((struct inpcb *));
void		tdb_rehash __P((void));

extern int	ipsec_auth_default_level;
extern int	ipsec_esp_trans_default_level;
extern int	ipsec_esp_network_default_level;

extern int encdebug;
int ipsec_in_use = 0;
u_int64_t ipsec_last_added = 0;
u_int32_t kernfs_epoch = 0;

struct expclusterlist_head expclusterlist =
    TAILQ_HEAD_INITIALIZER(expclusterlist);
struct explist_head explist = TAILQ_HEAD_INITIALIZER(explist);
struct ipsec_policy_head ipsec_policy_head =
    TAILQ_HEAD_INITIALIZER(ipsec_policy_head);
struct ipsec_acquire_head ipsec_acquire_head =
    TAILQ_HEAD_INITIALIZER(ipsec_acquire_head);

/*
 * This is the proper place to define the various encapsulation transforms.
 */

struct xformsw xformsw[] = {
    { XF_IP4,	         0,               "IPv4 Simple Encapsulation",
      ipe4_attach,       ipe4_init,       ipe4_zeroize,
      (int (*)(struct mbuf *, struct tdb *, int, int))ipe4_input, 
      ipip_output, },
    { XF_AH,	 XFT_AUTH,	    "IPsec AH",
      ah_attach,	ah_init,   ah_zeroize,
      ah_input,	 	ah_output, },
    { XF_ESP,	 XFT_CONF|XFT_AUTH, "IPsec ESP",
      esp_attach,	esp_init,  esp_zeroize,
      esp_input,	esp_output, },
#ifdef TCP_SIGNATURE
    { XF_TCPSIGNATURE,	 XFT_AUTH, "TCP MD5 Signature Option, RFC 2385",
      tcp_signature_tdb_attach, 	tcp_signature_tdb_init,
      tcp_signature_tdb_zeroize,	tcp_signature_tdb_input,
      tcp_signature_tdb_output, }
#endif /* TCP_SIGNATURE */
};

struct xformsw *xformswNXFORMSW = &xformsw[sizeof(xformsw)/sizeof(xformsw[0])];

unsigned char ipseczeroes[IPSEC_ZEROES_SIZE]; /* zeroes! */ 

#define TDB_HASHSIZE_INIT 32
static struct tdb **tdbh = NULL;
static struct tdb **tdbaddr = NULL;
static struct tdb **tdbsrc = NULL;
static u_int tdb_hashmask = TDB_HASHSIZE_INIT - 1;
static int tdb_count;

/*
 * Our hashing function needs to stir things with a non-zero random multiplier
 * so we cannot be DoS-attacked via choosing of the data to hash.
 */
INLINE int
tdb_hash(u_int32_t spi, union sockaddr_union *dst, u_int8_t proto)
{
    static u_int32_t mult1 = 0, mult2 = 0;
    u_int8_t *ptr = (u_int8_t *) dst;
    int i, shift;
    u_int64_t hash;
    int val32 = 0;

    while (mult1 == 0)
	mult1 = arc4random();
    while (mult2 == 0)
	mult2 = arc4random();

    hash = (spi ^ proto) * mult1;
    for (i = 0; i < SA_LEN(&dst->sa); i++)
    {
	val32 = (val32 << 8) | ptr[i];
	if (i % 4 == 3)
	{
	    hash ^= val32 * mult2;
	    val32 = 0;
	}
    }

    if (i % 4 != 0)
	hash ^= val32 * mult2;

    shift = ffs(tdb_hashmask + 1);
    while ((hash & ~tdb_hashmask) != 0)
      hash = (hash >> shift) ^ (hash & tdb_hashmask);

    return hash;
}

/*
 * Reserve an SPI; the SA is not valid yet though.  We use 0 as
 * an error return value.
 */
u_int32_t
reserve_spi(u_int32_t sspi, u_int32_t tspi, union sockaddr_union *src,
	    union sockaddr_union *dst, u_int8_t sproto, int *errval)
{
    struct tdb *tdbp;
    u_int32_t spi;
    int nums, s;

    /* Don't accept ranges only encompassing reserved SPIs.  */
    if (tspi < sspi || tspi <= SPI_RESERVED_MAX)
    {
	(*errval) = EINVAL;
	return 0;
    }

    /* Limit the range to not include reserved areas.  */
    if (sspi <= SPI_RESERVED_MAX)
      sspi = SPI_RESERVED_MAX + 1;

    if (sspi == tspi)   /* Asking for a specific SPI */
      nums = 1;
    else
      nums = 100;  /* Arbitrarily chosen */

    while (nums--)
    {
	if (sspi == tspi)  /* Specific SPI asked */
	  spi = tspi;
	else    /* Range specified */
	{
	    get_random_bytes((void *) &spi, sizeof(spi));
	    spi = sspi + (spi % (tspi - sspi));
	}
	  
	/* Don't allocate reserved SPIs.  */
	if (spi >= SPI_RESERVED_MIN && spi <= SPI_RESERVED_MAX)
	  continue;
	else
	  spi = htonl(spi);

	/* Check whether we're using this SPI already */
	s = spltdb();
	tdbp = gettdb(spi, dst, sproto);
	splx(s);

	if (tdbp != (struct tdb *) NULL)
	  continue;

	MALLOC(tdbp, struct tdb *, sizeof(struct tdb), M_TDB, M_WAITOK);
	bzero((caddr_t) tdbp, sizeof(struct tdb));
	
	tdbp->tdb_spi = spi;
	bcopy(&dst->sa, &tdbp->tdb_dst.sa, SA_LEN(&dst->sa));
	bcopy(&src->sa, &tdbp->tdb_src.sa, SA_LEN(&src->sa));
	tdbp->tdb_sproto = sproto;
	tdbp->tdb_flags |= TDBF_INVALID;       /* Mark SA as invalid for now */
	tdbp->tdb_satype = SADB_SATYPE_UNSPEC;
	tdbp->tdb_established = time.tv_sec;
	tdbp->tdb_epoch = kernfs_epoch - 1;
	puttdb(tdbp);

	/* Setup a "silent" expiration (since TDBF_INVALID's set) */
	if (ipsec_keep_invalid > 0)
	{
		tdbp->tdb_flags |= TDBF_TIMER;
		tdbp->tdb_exp_timeout = time.tv_sec + ipsec_keep_invalid;
		tdb_expiration(tdbp, TDBEXP_EARLY | TDBEXP_TIMEOUT);
	}

	return spi;
    }

    (*errval) = EEXIST;
    return 0;
}

/*
 * An IPSP SAID is really the concatenation of the SPI found in the 
 * packet, the destination address of the packet and the IPsec protocol.
 * When we receive an IPSP packet, we need to look up its tunnel descriptor
 * block, based on the SPI in the packet and the destination address (which
 * is really one of our addresses if we received the packet!
 *
 * Caller is responsible for setting at least spltdb().
 */
struct tdb *
gettdb(u_int32_t spi, union sockaddr_union *dst, u_int8_t proto)
{
    u_int32_t hashval;
    struct tdb *tdbp;

    if (tdbh == NULL)
      return (struct tdb *) NULL;

    hashval = tdb_hash(spi, dst, proto);

    for (tdbp = tdbh[hashval]; tdbp != NULL; tdbp = tdbp->tdb_hnext)
      if ((tdbp->tdb_spi == spi) && 
	  !bcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa)) &&
	  (tdbp->tdb_sproto == proto))
	break;

    return tdbp;
}

/*
 * Get an SA given the remote address, the security protocol type, and
 * the desired IDs.
 */
struct tdb *
gettdbbyaddr(union sockaddr_union *dst, u_int8_t proto, struct mbuf *m, int af)
{
    u_int32_t hashval;
    struct tdb *tdbp;

    if (tdbaddr == NULL)
      return (struct tdb *) NULL;

    hashval = tdb_hash(0, dst, proto);

    for (tdbp = tdbaddr[hashval]; tdbp != NULL; tdbp = tdbp->tdb_anext)
      if ((tdbp->tdb_sproto == proto) &&
	  ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
	  (!bcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa))))
      {
	  /*
	   * If the IDs are not set, this was probably a manually-keyed
	   * SA, so it can be used for any type of traffic.
	   */
	  if ((tdbp->tdb_srcid == NULL) && (tdbp->tdb_dstid == NULL))
	    break;

	  /* Not sure how to deal with half-set IDs...just skip the SA */
	  if ((tdbp->tdb_srcid == NULL) || (tdbp->tdb_dstid == NULL))
	    continue;

	  /* We only grok addresses */
	  if (((tdbp->tdb_srcid_type != SADB_IDENTTYPE_PREFIX) &&
	       (tdbp->tdb_dstid_type != SADB_IDENTTYPE_CONNECTION)) ||
	      ((tdbp->tdb_dstid_type != SADB_IDENTTYPE_PREFIX) &&
	       (tdbp->tdb_dstid_type != SADB_IDENTTYPE_CONNECTION)))
	    continue;

	  /* Sanity */
	  if ((m == NULL) || (af == 0))
	    continue;

	  /* XXX Check the IDs ? */
	  break;
      }

    return tdbp;
}

/*
 * Get an SA given the source address, the security protocol type, and
 * the desired IDs.
 */
struct tdb *
gettdbbysrc(union sockaddr_union *src, u_int8_t proto, struct mbuf *m, int af)
{
    u_int32_t hashval;
    struct tdb *tdbp;

    if (tdbsrc == NULL)
      return (struct tdb *) NULL;

    hashval = tdb_hash(0, src, proto);

    for (tdbp = tdbsrc[hashval]; tdbp != NULL; tdbp = tdbp->tdb_snext)
      if ((tdbp->tdb_sproto == proto) &&
	  ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
	  (!bcmp(&tdbp->tdb_src, src, SA_LEN(&src->sa))))
      {
	  /*
	   * If the IDs are not set, this was probably a manually-keyed
	   * SA, so it can be used for any type of traffic.
	   */
	  if ((tdbp->tdb_srcid == NULL) && (tdbp->tdb_dstid == NULL))
	    break;

	  /* Not sure how to deal with half-set IDs...just skip the SA */
	  if ((tdbp->tdb_srcid == NULL) || (tdbp->tdb_dstid == NULL))
	    continue;

	  /* We only grok addresses */
	  if (((tdbp->tdb_srcid_type != SADB_IDENTTYPE_PREFIX) &&
	       (tdbp->tdb_dstid_type != SADB_IDENTTYPE_CONNECTION)) ||
	      ((tdbp->tdb_dstid_type != SADB_IDENTTYPE_PREFIX) &&
	       (tdbp->tdb_dstid_type != SADB_IDENTTYPE_CONNECTION)))
	    continue;

	  /* XXX Check the IDs ? */
	  break;
      }

    return tdbp;
}

#if DDB
void
tdb_hashstats(void)
{
    int i, cnt, buckets[16];
    struct tdb *tdbp;

    if (tdbh == NULL)
    {
	db_printf("no tdb hash table\n");
	return;
    }

    bzero (buckets, sizeof(buckets));
    for (i = 0; i <= tdb_hashmask; i++)
    {
	cnt = 0;
	for (tdbp = tdbh[i]; cnt < 16 && tdbp != NULL; tdbp = tdbp->tdb_hnext)
	  cnt++;
	buckets[cnt]++;
    }

    db_printf("tdb cnt\t\tbucket cnt\n");
    for (i = 0; i < 16; i++)
      if (buckets[i] > 0)
	db_printf("%d%c\t\t%d\n", i, i == 15 ? "+" : "", buckets[i]);
}
#endif	/* DDB */

/*
 * Caller is responsible for setting at least spltdb().
 */
int
tdb_walk(int (*walker)(struct tdb *, void *), void *arg)
{
    int i, rval = 0;
    struct tdb *tdbp, *next;

    if (tdbh == NULL)
        return ENOENT;

    for (i = 0; i <= tdb_hashmask; i++)
      for (tdbp = tdbh[i]; rval == 0 && tdbp != NULL; tdbp = next)
      {
	  next = tdbp->tdb_hnext;
	  rval = walker(tdbp, (void *)arg);
      }

    return rval;
}

/*
 * Called at splsoftclock().
 */
void
handle_expirations(void *arg)
{
    struct tdb *tdb;

    for (tdb = TAILQ_FIRST(&explist);
	 tdb && tdb->tdb_timeout <= time.tv_sec;
	 tdb = TAILQ_FIRST(&explist))
    {
	/* Hard expirations first */
	if ((tdb->tdb_flags & TDBF_TIMER) &&
	    (tdb->tdb_exp_timeout <= time.tv_sec))
	{
	    /* If it's an "invalid" TDB do a silent expiration. */
	    if (!(tdb->tdb_flags & TDBF_INVALID))
	      pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	    tdb_delete(tdb, 0);
	    continue;
	}
	else
	  if ((tdb->tdb_flags & TDBF_FIRSTUSE) &&
	      (tdb->tdb_first_use + tdb->tdb_exp_first_use <= time.tv_sec))
	  {
              /* If the TDB hasn't been used, don't renew it */
              if (tdb->tdb_first_use != 0)
	        pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	      tdb_delete(tdb, 0);
	      continue;
	  }

	/* Soft expirations */
	if ((tdb->tdb_flags & TDBF_SOFT_TIMER) &&
	    (tdb->tdb_soft_timeout <= time.tv_sec))
	{
	    pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	    tdb->tdb_flags &= ~TDBF_SOFT_TIMER;
	    tdb_expiration(tdb, TDBEXP_EARLY);
	}
	else
	  if ((tdb->tdb_flags & TDBF_SOFT_FIRSTUSE) &&
	      (tdb->tdb_first_use + tdb->tdb_soft_first_use <=
	       time.tv_sec))
	  {
              /* If the TDB hasn't been used, don't renew it */
              if (tdb->tdb_first_use != 0)
	        pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	      tdb->tdb_flags &= ~TDBF_SOFT_FIRSTUSE;
	      tdb_expiration(tdb, TDBEXP_EARLY);
	  }
    }

    /* If any tdb is left on the expiration queue, set the timer.  */
    if (tdb)
      timeout(handle_expirations, (void *) NULL, 
	      hz * (tdb->tdb_timeout - time.tv_sec));
}

/*
 * Ensure the tdb is in the right place in the expiration list.
 */
void
tdb_expiration(struct tdb *tdb, int flags)
{
    struct tdb *t, *former_expirer, *next_expirer;
    int will_be_first, sole_reason, early;
    u_int64_t next_timeout = 0;
    int s = spltdb();

    /* Find the earliest expiration.  */
    if ((tdb->tdb_flags & TDBF_FIRSTUSE) && tdb->tdb_first_use != 0 &&
	(next_timeout == 0 ||
	 next_timeout > tdb->tdb_first_use + tdb->tdb_exp_first_use))
      next_timeout = tdb->tdb_first_use + tdb->tdb_exp_first_use;
    if ((tdb->tdb_flags & TDBF_SOFT_FIRSTUSE) && tdb->tdb_first_use != 0 &&
	(next_timeout == 0 ||
	 next_timeout > tdb->tdb_first_use + tdb->tdb_soft_first_use))
      next_timeout = tdb->tdb_first_use + tdb->tdb_soft_first_use;
    if ((tdb->tdb_flags & TDBF_TIMER) &&
	(next_timeout == 0 || next_timeout > tdb->tdb_exp_timeout))
      next_timeout = tdb->tdb_exp_timeout;
    if ((tdb->tdb_flags & TDBF_SOFT_TIMER) &&
	(next_timeout == 0 || next_timeout > tdb->tdb_soft_timeout))
      next_timeout = tdb->tdb_soft_timeout;

    /* No change?  */
    if (next_timeout == tdb->tdb_timeout)
    {
	splx(s);
	return;
    }

    /*
     * Find out some useful facts: Will our tdb be first to expire?
     * Was our tdb the sole reason for the old timeout?
     */
    former_expirer = TAILQ_FIRST(&expclusterlist);
    next_expirer = TAILQ_NEXT(tdb, tdb_explink);
    will_be_first = (next_timeout != 0 &&
		     (former_expirer == NULL ||
		      next_timeout < former_expirer->tdb_timeout));
    sole_reason = (tdb == former_expirer &&
		   (next_expirer == NULL ||
		    tdb->tdb_timeout != next_expirer->tdb_timeout));

    /*
     * We need to untimeout if either:
     * - there is an expiration pending and the new timeout is earlier than
     *   what already exists or
     * - the existing first expiration is due to our old timeout value solely
     */
    if ((former_expirer != NULL && will_be_first) || sole_reason)
      untimeout(handle_expirations, (void *) NULL);

    /*
     * We need to timeout if we've been asked to and if either
     * - our tdb has a timeout and no former expiration exist or
     * - the new timeout is earlier than what already exists or
     * - the existing first expiration is due to our old timeout value solely
     *   and another expiration is in the pipe.
     */
    if ((flags & TDBEXP_TIMEOUT) &&
	(will_be_first || (sole_reason && next_expirer != NULL)))
      timeout(handle_expirations, (void *) NULL,
	      hz * ((will_be_first ? next_timeout :
		     next_expirer->tdb_timeout) - time.tv_sec));

    /* Our old position, if any, is not relevant anymore.  */
    if (tdb->tdb_timeout != 0)
    {
        if (tdb->tdb_expnext.tqe_prev != NULL)
	{
	    if (next_expirer && tdb->tdb_timeout == next_expirer->tdb_timeout)
	      TAILQ_INSERT_BEFORE(tdb, next_expirer, tdb_expnext);
	    TAILQ_REMOVE(&expclusterlist, tdb, tdb_expnext);
	    tdb->tdb_expnext.tqe_prev = NULL;
	}

	TAILQ_REMOVE(&explist, tdb, tdb_explink);
    }

    tdb->tdb_timeout = next_timeout;

    if (next_timeout == 0)
    {
	splx(s);
	return;
    }

    /*
     * Search front-to-back if we believe we will end up early, otherwise
     * back-to-front.
     */
    early = will_be_first || (flags & TDBEXP_EARLY);
    for (t = (early ? TAILQ_FIRST(&expclusterlist) :
	      TAILQ_LAST(&expclusterlist, expclusterlist_head));
	 t != NULL && (early ? (t->tdb_timeout <= next_timeout) : 
		       (t->tdb_timeout > next_timeout));
	 t = (early ? TAILQ_NEXT(t, tdb_expnext) :
	      TAILQ_PREV(t, expclusterlist_head, tdb_expnext)))
      ;
    if (t == (early ? TAILQ_FIRST(&expclusterlist) : NULL))
    {
	/* We are to become the first expiration.  */
	TAILQ_INSERT_HEAD(&expclusterlist, tdb, tdb_expnext);
	TAILQ_INSERT_HEAD(&explist, tdb, tdb_explink);
    }
    else
    {
	if (early)
	  t = (t ? TAILQ_PREV(t, expclusterlist_head, tdb_expnext) :
	       TAILQ_LAST(&expclusterlist, expclusterlist_head));
	if (TAILQ_NEXT(t, tdb_expnext))
	  TAILQ_INSERT_BEFORE(TAILQ_NEXT(t, tdb_expnext), tdb, tdb_explink);
	else
	  TAILQ_INSERT_TAIL(&explist, tdb, tdb_explink);
	if (t->tdb_timeout < next_timeout)
	  TAILQ_INSERT_AFTER(&expclusterlist, t, tdb, tdb_expnext);
    }

#ifdef DIAGNOSTIC
    /*
     * Check various invariants.
     */
    if (tdb->tdb_expnext.tqe_prev != NULL)
    {
	t = TAILQ_FIRST(&expclusterlist);
	if (t != tdb && t->tdb_timeout >= tdb->tdb_timeout)
	  panic("tdb_expiration: "
		"expclusterlist first link out of order (%p, %p)",
		tdb, t);
	t = TAILQ_PREV(tdb, expclusterlist_head, tdb_expnext);
	if (t != NULL && t->tdb_timeout >= tdb->tdb_timeout)
	  panic("tdb_expiration: "
		"expclusterlist prev link out of order (%p, %p)",
		tdb, t);
	else if (t == NULL && tdb != TAILQ_FIRST(&expclusterlist))
	  panic("tdb_expiration: "
		"expclusterlist first link out of order (%p, %p)",
		tdb, TAILQ_FIRST(&expclusterlist));
	t = TAILQ_NEXT(tdb, tdb_expnext);
	if (t != NULL && t->tdb_timeout <= tdb->tdb_timeout)
	  panic("tdb_expiration: "
		"expclusterlist next link out of order (%p, %p)",
		tdb, t);
	else if (t == NULL &&
		 tdb != TAILQ_LAST(&expclusterlist, expclusterlist_head))
	  panic("tdb_expiration: "
		"expclusterlist last link out of order (%p, %p)",
		tdb, TAILQ_LAST(&expclusterlist, expclusterlist_head));
	t = TAILQ_LAST(&expclusterlist, expclusterlist_head);
	if (t != tdb && t->tdb_timeout <= tdb->tdb_timeout)
	  panic("tdb_expiration: "
		"expclusterlist last link out of order (%p, %p)",
		tdb, t);
    }

    t = TAILQ_FIRST(&explist);
    if (t != NULL && t->tdb_timeout > tdb->tdb_timeout)
      panic("tdb_expiration: explist first link out of order (%p, %p)", tdb,
	    t);

    t = TAILQ_PREV(tdb, explist_head, tdb_explink);
    if (t != NULL && t->tdb_timeout > tdb->tdb_timeout)
      panic("tdb_expiration: explist prev link out of order (%p, %p)", tdb, t);
    else if (t == NULL && tdb != TAILQ_FIRST(&explist))
      panic("tdb_expiration: explist first link out of order (%p, %p)", tdb,
	    TAILQ_FIRST(&explist));

    t = TAILQ_NEXT(tdb, tdb_explink);
    if (t != NULL && t->tdb_timeout < tdb->tdb_timeout)
      panic("tdb_expiration: explist next link out of order (%p, %p)", tdb, t);
    else if (t == NULL && tdb != TAILQ_LAST(&explist, explist_head))
      panic("tdb_expiration: explist last link out of order (%p, %p)", tdb,
	    TAILQ_LAST(&explist, explist_head));

    t = TAILQ_LAST(&explist, explist_head);
    if (t != tdb && t->tdb_timeout < tdb->tdb_timeout)
      panic("tdb_expiration: explist last link out of order (%p, %p)", tdb, t);
#endif

    splx(s);
}

/*
 * Caller is responsible for spltdb().
 */
void
tdb_rehash(void)
{
    struct tdb **new_tdbh, **new_tdbaddr, **new_srcaddr, *tdbp, *tdbnp;
    u_int i, old_hashmask = tdb_hashmask;
    u_int32_t hashval;

    tdb_hashmask = (tdb_hashmask << 1) | 1;

    MALLOC(new_tdbh, struct tdb **, sizeof(struct tdb *) * (tdb_hashmask + 1),
	   M_TDB, M_WAITOK);
    MALLOC(new_tdbaddr, struct tdb **,
	   sizeof(struct tdb *) * (tdb_hashmask + 1), M_TDB, M_WAITOK);
    MALLOC(new_srcaddr, struct tdb **,
	   sizeof(struct tdb *) * (tdb_hashmask + 1), M_TDB, M_WAITOK);

    bzero(new_tdbh, sizeof(struct tdb *) * (tdb_hashmask + 1));
    bzero(new_tdbaddr, sizeof(struct tdb *) * (tdb_hashmask + 1));
    bzero(new_srcaddr, sizeof(struct tdb *) * (tdb_hashmask + 1));

    for (i = 0; i <= old_hashmask; i++)
    {
	for (tdbp = tdbh[i]; tdbp != NULL; tdbp = tdbnp)
	{
	    tdbnp = tdbp->tdb_hnext;
	    hashval = tdb_hash(tdbp->tdb_spi, &tdbp->tdb_dst,
			       tdbp->tdb_sproto);
	    tdbp->tdb_hnext = new_tdbh[hashval];
	    new_tdbh[hashval] = tdbp;
	}

	for (tdbp = tdbaddr[i]; tdbp != NULL; tdbp = tdbnp)
	{
	    tdbnp = tdbp->tdb_anext;
	    hashval = tdb_hash(0, &tdbp->tdb_dst, tdbp->tdb_sproto);
	    tdbp->tdb_anext = new_tdbaddr[hashval];
	    new_tdbaddr[hashval] = tdbp;
	}

	for (tdbp = tdbsrc[i]; tdbp != NULL; tdbp = tdbnp)
	{
	    tdbnp = tdbp->tdb_snext;
	    hashval = tdb_hash(0, &tdbp->tdb_src, tdbp->tdb_sproto);
	    tdbp->tdb_snext = new_srcaddr[hashval];
	    new_srcaddr[hashval] = tdbp;
	}
    }

    FREE(tdbh, M_TDB);
    tdbh = new_tdbh;

    FREE(tdbaddr, M_TDB);
    tdbaddr = new_tdbaddr;

    FREE(tdbsrc, M_TDB);
    tdbsrc = new_srcaddr;
}

/*
 * Add TDB in the hash table.
 */
void
puttdb(struct tdb *tdbp)
{
    u_int32_t hashval;
    int s = spltdb();

    if (tdbh == NULL)
    {
	MALLOC(tdbh, struct tdb **, sizeof(struct tdb *) * (tdb_hashmask + 1),
	       M_TDB, M_WAITOK);
	MALLOC(tdbaddr, struct tdb **,
	       sizeof(struct tdb *) * (tdb_hashmask + 1),
	       M_TDB, M_WAITOK);
	MALLOC(tdbsrc, struct tdb **,
	       sizeof(struct tdb *) * (tdb_hashmask + 1),
	       M_TDB, M_WAITOK);

	bzero(tdbh, sizeof(struct tdb *) * (tdb_hashmask + 1));
	bzero(tdbaddr, sizeof(struct tdb *) * (tdb_hashmask + 1));
	bzero(tdbsrc, sizeof(struct tdb *) * (tdb_hashmask + 1));
    }

    hashval = tdb_hash(tdbp->tdb_spi, &tdbp->tdb_dst, tdbp->tdb_sproto);
    
    /*
     * Rehash if this tdb would cause a bucket to have more than two items
     * and if the number of tdbs exceed 10% of the bucket count.  This
     * number is arbitratily chosen and is just a measure to not keep rehashing
     * when adding and removing tdbs which happens to always end up in the
     * same bucket, which is not uncommon when doing manual keying.
     */
    if (tdbh[hashval] != NULL && tdbh[hashval]->tdb_hnext != NULL &&
	tdb_count * 10 > tdb_hashmask + 1)
    {
	tdb_rehash();
	hashval = tdb_hash(tdbp->tdb_spi, &tdbp->tdb_dst, tdbp->tdb_sproto);
    }

    tdbp->tdb_hnext = tdbh[hashval];
    tdbh[hashval] = tdbp;

    hashval = tdb_hash(0, &tdbp->tdb_dst, tdbp->tdb_sproto);
    tdbp->tdb_anext = tdbaddr[hashval];
    tdbaddr[hashval] = tdbp;

    hashval = tdb_hash(0, &tdbp->tdb_src, tdbp->tdb_sproto);
    tdbp->tdb_snext = tdbsrc[hashval];
    tdbsrc[hashval] = tdbp;

    tdb_count++;

    ipsec_last_added = time.tv_sec;

    splx(s);
}

/*
 * Caller is responsible to set at least spltdb().
 */
void
tdb_delete(struct tdb *tdbp, int expflags)
{
    struct ipsec_policy *ipo;
    struct tdb *tdbpp;
    struct inpcb *inp;
    u_int32_t hashval;
    int s;

    if (tdbh == NULL)
      return;

    hashval = tdb_hash(tdbp->tdb_spi, &tdbp->tdb_dst, tdbp->tdb_sproto);

    s = spltdb();
    if (tdbh[hashval] == tdbp)
    {
	tdbpp = tdbp;
	tdbh[hashval] = tdbp->tdb_hnext;
    }
    else
      for (tdbpp = tdbh[hashval]; tdbpp != NULL; tdbpp = tdbpp->tdb_hnext)
	if (tdbpp->tdb_hnext == tdbp)
	{
	    tdbpp->tdb_hnext = tdbp->tdb_hnext;
	    tdbpp = tdbp;
	    break;
	}

    tdbp->tdb_hnext = NULL;

    hashval = tdb_hash(0, &tdbp->tdb_dst, tdbp->tdb_sproto);

    if (tdbaddr[hashval] == tdbp)
    {
	tdbpp = tdbp;
	tdbaddr[hashval] = tdbp->tdb_anext;
    }
    else
      for (tdbpp = tdbaddr[hashval]; tdbpp != NULL; tdbpp = tdbpp->tdb_anext)
	if (tdbpp->tdb_anext == tdbp)
	{
	    tdbpp->tdb_anext = tdbp->tdb_anext;
	    tdbpp = tdbp;
	    break;
	}

    hashval = tdb_hash(0, &tdbp->tdb_src, tdbp->tdb_sproto);

    if (tdbsrc[hashval] == tdbp)
    {
	tdbpp = tdbp;
	tdbsrc[hashval] = tdbp->tdb_snext;
    }
    else
      for (tdbpp = tdbsrc[hashval]; tdbpp != NULL; tdbpp = tdbpp->tdb_snext)
	if (tdbpp->tdb_snext == tdbp)
	{
	    tdbpp->tdb_snext = tdbp->tdb_snext;
	    tdbpp = tdbp;
	    break;
	}

    tdbp->tdb_snext = NULL;

    if (tdbp->tdb_xform)
    {
      	(*(tdbp->tdb_xform->xf_zeroize))(tdbp);
	tdbp->tdb_xform = NULL;
    }

    /* Cleanup inp references */
    for (inp = TAILQ_FIRST(&tdbp->tdb_inp); inp;
	 inp = TAILQ_FIRST(&tdbp->tdb_inp))
    {
        TAILQ_REMOVE(&tdbp->tdb_inp, inp, inp_tdb_next);
	inp->inp_tdb = NULL;
    }

    /* Cleanup SPD references */
    for (ipo = TAILQ_FIRST(&tdbp->tdb_policy_head); ipo;
	 ipo = TAILQ_FIRST(&tdbp->tdb_policy_head))
    {
	TAILQ_REMOVE(&tdbp->tdb_policy_head, ipo, ipo_tdb_next);
	ipo->ipo_tdb = NULL;
    }

    /* Remove us from the expiration lists.  */
    if (tdbp->tdb_timeout != 0)
    {
        tdbp->tdb_flags &= ~(TDBF_FIRSTUSE | TDBF_SOFT_FIRSTUSE | TDBF_TIMER |
			     TDBF_SOFT_TIMER);
	tdb_expiration(tdbp, expflags);
    }

    if (tdbp->tdb_srcid)
    {
      	FREE(tdbp->tdb_srcid, M_XDATA);
	tdbp->tdb_srcid = NULL;
    }

    if (tdbp->tdb_dstid)
    {
      	FREE(tdbp->tdb_dstid, M_XDATA);
	tdbp->tdb_dstid = NULL;
    }

    if ((tdbp->tdb_onext) && (tdbp->tdb_onext->tdb_inext == tdbp))
      tdbp->tdb_onext->tdb_inext = NULL;

    if ((tdbp->tdb_inext) && (tdbp->tdb_inext->tdb_onext == tdbp))
      tdbp->tdb_inext->tdb_onext = NULL;

    FREE(tdbp, M_TDB);
    tdb_count--;

    splx(s);
}

/*
 * Initialize a TDB structure.
 */
int
tdb_init(struct tdb *tdbp, u_int16_t alg, struct ipsecinit *ii)
{
    struct xformsw *xsp;

    /* Record establishment time */
    tdbp->tdb_established = time.tv_sec;
    tdbp->tdb_epoch = kernfs_epoch - 1;

    /* Init Incoming SA-Binding Queues */
    TAILQ_INIT(&tdbp->tdb_inp);

    TAILQ_INIT(&tdbp->tdb_policy_head);

    for (xsp = xformsw; xsp < xformswNXFORMSW; xsp++)
      if (xsp->xf_type == alg)
	return (*(xsp->xf_init))(tdbp, xsp, ii);

    DPRINTF(("tdb_init(): no alg %d for spi %08x, addr %s, proto %d\n", 
	     alg, ntohl(tdbp->tdb_spi), ipsp_address(tdbp->tdb_dst),
	     tdbp->tdb_sproto));

    return EINVAL;
}

/*
 * Used by kernfs.
 */
int
ipsp_kern(int off, char **bufp, int len)
{
    static char buffer[IPSEC_KERNFS_BUFSIZE];
    struct tdb *tdb;
    int l, i, s;

    if (bufp == NULL)
      return 0;

    bzero(buffer, IPSEC_KERNFS_BUFSIZE);
    *bufp = buffer;

    if (off == 0)
    {
        kernfs_epoch++;
        l = sprintf(buffer, "Hashmask: %d, policy entries: %d\n", tdb_hashmask,
                    ipsec_in_use);
       return l;
    }
    
    if (tdbh == NULL)
      return 0;

    for (i = 0; i <= tdb_hashmask; i++)
    {
        s = spltdb();
	for (tdb = tdbh[i]; tdb; tdb = tdb->tdb_hnext)
	  if (tdb->tdb_epoch != kernfs_epoch)
	  {
	      tdb->tdb_epoch = kernfs_epoch;

	      l = sprintf(buffer,
			  "SPI = %08x, Destination = %s, Sproto = %u\n",
			  ntohl(tdb->tdb_spi),
			  ipsp_address(tdb->tdb_dst), tdb->tdb_sproto);

	      l += sprintf(buffer + l, "\tEstablished %d seconds ago\n",
			   time.tv_sec - tdb->tdb_established);

	      l += sprintf(buffer + l, "\tSource = %s",
			   ipsp_address(tdb->tdb_src));

	      if (tdb->tdb_proxy.sa.sa_family)
		l += sprintf(buffer + l, ", Proxy = %s\n",
			     ipsp_address(tdb->tdb_proxy));
	      else
		l += sprintf(buffer + l, "\n");

	      l += sprintf(buffer + l, "\tFlags (%08x) = <", tdb->tdb_flags);

	      if ((tdb->tdb_flags & ~(TDBF_TIMER | TDBF_BYTES |
				      TDBF_ALLOCATIONS | TDBF_FIRSTUSE |
				      TDBF_SOFT_TIMER | TDBF_SOFT_BYTES |
				      TDBF_SOFT_FIRSTUSE |
				      TDBF_SOFT_ALLOCATIONS)) == 0)
		l += sprintf(buffer + l, "none>\n");
	      else
	      {
		  /* We can reuse variable 'i' here, since we're not looping */
		  i = 0;

		  if (tdb->tdb_flags & TDBF_UNIQUE)
		  {
		      if (i)
			l += sprintf(buffer + l, ", ");
		      else
			i = 1;

		      l += sprintf(buffer + l, "unique");
		      i = 1;
		  }

		  if (tdb->tdb_flags & TDBF_INVALID)
		  {
		      if (i)
			l += sprintf(buffer + l, ", ");
		      else
			i = 1;

		      l += sprintf(buffer + l, "invalid");
		  }

		  if (tdb->tdb_flags & TDBF_HALFIV)
		  {
		      if (i)
			l += sprintf(buffer + l, ", ");
		      else
			i = 1;

		      l += sprintf(buffer + l, "halfiv");
		  }

		  if (tdb->tdb_flags & TDBF_PFS)
		  {
		      if (i)
			l += sprintf(buffer + l, ", ");
		      else
			i = 1;

		      l += sprintf(buffer + l, "pfs");
		  }

		  if (tdb->tdb_flags & TDBF_TUNNELING)
		  {
		      if (i)
			l += sprintf(buffer + l, ", ");
		      else
			i = 1;

		      l += sprintf(buffer + l, "tunneling");
		  }

		  if (tdb->tdb_flags & TDBF_NOREPLAY)
		  {
		      if (i)
			l += sprintf(buffer + l, ", ");
		      else
			i = 1;

		      l += sprintf(buffer + l, "noreplay");
		  }

		  if (tdb->tdb_flags & TDBF_RANDOMPADDING)
		  {
		      if (i)
			l += sprintf(buffer + l, ", ");
		      else
			i = 1;

		      l += sprintf(buffer + l, "random padding");
		  }

		  l += sprintf(buffer + l, ">\n");
	      }

	      l += sprintf(buffer + l, "\tCrypto ID: %qu\n", tdb->tdb_cryptoid);

	      if (tdb->tdb_xform)
		l += sprintf(buffer + l, "\txform = <%s>\n", 
			     tdb->tdb_xform->xf_name);

	      if (tdb->tdb_encalgxform)
		l += sprintf(buffer + l, "\t\tEncryption = <%s>\n",
			     tdb->tdb_encalgxform->name);

	      if (tdb->tdb_authalgxform)
		l += sprintf(buffer + l, "\t\tAuthentication = <%s>\n",
			     tdb->tdb_authalgxform->name);

	      if (tdb->tdb_onext)
		l += sprintf(buffer + l,
			     "\tNext SA: SPI = %08x, "
			     "Destination = %s, Sproto = %u\n",
			     ntohl(tdb->tdb_onext->tdb_spi),
			     ipsp_address(tdb->tdb_onext->tdb_dst),
			     tdb->tdb_onext->tdb_sproto);

	      if (tdb->tdb_inext)
		l += sprintf(buffer + l,
			     "\tPrevious SA: SPI = %08x, "
			     "Destination = %s, Sproto = %u\n",
			     ntohl(tdb->tdb_inext->tdb_spi),
			     ipsp_address(tdb->tdb_inext->tdb_dst),
			     tdb->tdb_inext->tdb_sproto);

	      if (tdb->tdb_interface)
		l += sprintf(buffer + l, "\tAssociated interface = <%s>\n",
			     ((struct ifnet *) tdb->tdb_interface)->if_xname);

	      l += sprintf(buffer + l, "\t%u flows have used this SA\n",
			   tdb->tdb_cur_allocations);
	    
	      l += sprintf(buffer + l, "\t%qu bytes processed by this SA\n",
			 tdb->tdb_cur_bytes);
	    
	      l += sprintf(buffer + l, "\tExpirations:\n");

	      if (tdb->tdb_flags & TDBF_TIMER)
		l += sprintf(buffer + l,
			     "\t\tHard expiration(1) in %qu seconds\n",
			     tdb->tdb_exp_timeout - time.tv_sec);
	    
	      if (tdb->tdb_flags & TDBF_SOFT_TIMER)
		l += sprintf(buffer + l,
			     "\t\tSoft expiration(1) in %qu seconds\n",
			     tdb->tdb_soft_timeout - time.tv_sec);
	    
	      if (tdb->tdb_flags & TDBF_BYTES)
		l += sprintf(buffer + l,
			     "\t\tHard expiration after %qu bytes\n",
			     tdb->tdb_exp_bytes);
	    
	      if (tdb->tdb_flags & TDBF_SOFT_BYTES)
		l += sprintf(buffer + l,
			     "\t\tSoft expiration after %qu bytes\n",
			     tdb->tdb_soft_bytes);

	      if (tdb->tdb_flags & TDBF_ALLOCATIONS)
		l += sprintf(buffer + l,
			     "\t\tHard expiration after %u flows\n",
			     tdb->tdb_exp_allocations);
	    
	      if (tdb->tdb_flags & TDBF_SOFT_ALLOCATIONS)
		l += sprintf(buffer + l,
			     "\t\tSoft expiration after %u flows\n",
			     tdb->tdb_soft_allocations);

	      if (tdb->tdb_flags & TDBF_FIRSTUSE)
	      {
		  if (tdb->tdb_first_use)
		    l += sprintf(buffer + l,
				 "\t\tHard expiration(2) in %qu seconds\n",
				 (tdb->tdb_first_use +
				  tdb->tdb_exp_first_use) - time.tv_sec);
		  else
		    l += sprintf(buffer + l,
				 "\t\tHard expiration in %qu seconds "
				 "after first use\n",
				 tdb->tdb_exp_first_use);
	      }

	      if (tdb->tdb_flags & TDBF_SOFT_FIRSTUSE)
	      {
		  if (tdb->tdb_first_use)
		    l += sprintf(buffer + l,
				 "\t\tSoft expiration(2) in %qu seconds\n",
				 (tdb->tdb_first_use +
				  tdb->tdb_soft_first_use) - time.tv_sec);
		  else
		    l += sprintf(buffer + l,
				 "\t\tSoft expiration in %qu seconds "
				 "after first use\n",
				 tdb->tdb_soft_first_use);
	      }

	      if (!(tdb->tdb_flags &
		    (TDBF_TIMER | TDBF_SOFT_TIMER | TDBF_BYTES |
		     TDBF_SOFT_ALLOCATIONS | TDBF_ALLOCATIONS |
		     TDBF_SOFT_BYTES | TDBF_FIRSTUSE | TDBF_SOFT_FIRSTUSE)))
		l += sprintf(buffer + l, "\t\t(none)\n");

	      l += sprintf(buffer + l, "\n");

	      splx(s);
	      return l;
	  }
	splx(s);
    }
    return 0;
}

/*
 * Check which transformations are required.
 */
u_int8_t
get_sa_require(struct inpcb *inp)
{
    u_int8_t sareq = 0;
       
    if (inp != NULL)
    {
	sareq |= inp->inp_seclevel[SL_AUTH] >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_AUTH : 0;
	sareq |= inp->inp_seclevel[SL_ESP_TRANS] >= IPSEC_LEVEL_USE ?
		 NOTIFY_SATYPE_CONF : 0;
	sareq |= inp->inp_seclevel[SL_ESP_NETWORK] >= IPSEC_LEVEL_USE ?
		 NOTIFY_SATYPE_TUNNEL : 0;
    }
    else
    {
	sareq |= ipsec_auth_default_level >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_AUTH : 0;
	sareq |= ipsec_esp_trans_default_level >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_CONF : 0;
	sareq |= ipsec_esp_network_default_level >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_TUNNEL : 0;
    }
    
    return (sareq);
}

/*
 * Add an inpcb to the list of inpcb which reference this tdb directly.
 */
void
tdb_add_inp(struct tdb *tdb, struct inpcb *inp)
{
    if (inp->inp_tdb)
    {
	if (inp->inp_tdb == tdb)
          return;

	TAILQ_REMOVE(&inp->inp_tdb->tdb_inp, inp, inp_tdb_next);
    }

    inp->inp_tdb = tdb;
    TAILQ_INSERT_TAIL(&tdb->tdb_inp, inp, inp_tdb_next);

    DPRINTF(("tdb_add_inp: tdb: %p, inp: %p\n", tdb, inp));
}

/* Return a printable string for the IPv4 address. */
char *
inet_ntoa4(struct in_addr ina)
{
    static char buf[4][4 * sizeof "123" + 4];
    unsigned char *ucp = (unsigned char *) &ina;
    static int i = 3;
 
    i = (i + 1) % 4;
    sprintf(buf[i], "%d.%d.%d.%d", ucp[0] & 0xff, ucp[1] & 0xff,
            ucp[2] & 0xff, ucp[3] & 0xff);
    return (buf[i]);
}

/* Return a printable string for the address. */
char *
ipsp_address(union sockaddr_union sa)
{
    switch (sa.sa.sa_family)
    {
#if INET
	case AF_INET:
	    return inet_ntoa4(sa.sin.sin_addr);
#endif /* INET */

#if INET6
	case AF_INET6:
	    return ip6_sprintf(&sa.sin6.sin6_addr);
#endif /* INET6 */

	default:
	    return "(unknown address family)";
    }
}
