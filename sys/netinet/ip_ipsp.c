/*	$OpenBSD: ip_ipsp.c,v 1.213 2015/04/17 11:04:01 mikeb Exp $	*/
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
 * Copyright (c) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
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

#include "pf.h"
#include "pfsync.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif

#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>

#ifdef DDB
#include <ddb/db_output.h>
void tdb_hashstats(void);
#endif

#ifdef ENCDEBUG
#define	DPRINTF(x)	if (encdebug) printf x
#else
#define	DPRINTF(x)
#endif

void		tdb_rehash(void);
void		tdb_timeout(void *v);
void		tdb_firstuse(void *v);
void		tdb_soft_timeout(void *v);
void		tdb_soft_firstuse(void *v);
int		tdb_hash(u_int, u_int32_t, union sockaddr_union *, u_int8_t);

int ipsec_in_use = 0;
u_int64_t ipsec_last_added = 0;

struct ipsec_policy_head ipsec_policy_head =
    TAILQ_HEAD_INITIALIZER(ipsec_policy_head);
struct ipsec_acquire_head ipsec_acquire_head =
    TAILQ_HEAD_INITIALIZER(ipsec_acquire_head);

/*
 * This is the proper place to define the various encapsulation transforms.
 */

struct xformsw xformsw[] = {
#ifdef IPSEC
	{ XF_IP4,	     0,               "IPv4 Simple Encapsulation",
	  ipe4_attach,       ipe4_init,       ipe4_zeroize,
	  (int (*)(struct mbuf *, struct tdb *, int, int))ipe4_input,
	  ipip_output, },
	{ XF_AH,	 XFT_AUTH,	    "IPsec AH",
	  ah_attach,	ah_init,   ah_zeroize,
	  ah_input,	 	ah_output, },
	{ XF_ESP,	 XFT_CONF|XFT_AUTH, "IPsec ESP",
	  esp_attach,	esp_init,  esp_zeroize,
	  esp_input,	esp_output, },
	{ XF_IPCOMP,	XFT_COMP, "IPcomp",
	  ipcomp_attach,    ipcomp_init, ipcomp_zeroize,
	  ipcomp_input,     ipcomp_output, },
#endif /* IPSEC */
#ifdef TCP_SIGNATURE
	{ XF_TCPSIGNATURE,	 XFT_AUTH, "TCP MD5 Signature Option, RFC 2385",
	  tcp_signature_tdb_attach, 	tcp_signature_tdb_init,
	  tcp_signature_tdb_zeroize,	tcp_signature_tdb_input,
	  tcp_signature_tdb_output, }
#endif /* TCP_SIGNATURE */
};

struct xformsw *xformswNXFORMSW = &xformsw[nitems(xformsw)];

#define	TDB_HASHSIZE_INIT	32

static SIPHASH_KEY tdbkey;
static struct tdb **tdbh = NULL;
static struct tdb **tdbdst = NULL;
static struct tdb **tdbsrc = NULL;
static u_int tdb_hashmask = TDB_HASHSIZE_INIT - 1;
static int tdb_count;

/*
 * Our hashing function needs to stir things with a non-zero random multiplier
 * so we cannot be DoS-attacked via choosing of the data to hash.
 */
int
tdb_hash(u_int rdomain, u_int32_t spi, union sockaddr_union *dst,
    u_int8_t proto)
{
	SIPHASH_CTX ctx;

	SipHash24_Init(&ctx, &tdbkey);
	SipHash24_Update(&ctx, &rdomain, sizeof(rdomain));
	SipHash24_Update(&ctx, &spi, sizeof(spi));
	SipHash24_Update(&ctx, &proto, sizeof(proto));
	SipHash24_Update(&ctx, dst, SA_LEN(&dst->sa));

	return (SipHash24_End(&ctx) & tdb_hashmask);
}

/*
 * Reserve an SPI; the SA is not valid yet though.  We use 0 as
 * an error return value.
 */
u_int32_t
reserve_spi(u_int rdomain, u_int32_t sspi, u_int32_t tspi,
    union sockaddr_union *src, union sockaddr_union *dst,
    u_int8_t sproto, int *errval)
{
	struct tdb *tdbp, *exists;
	u_int32_t spi;
	int nums, s;

	/* Don't accept ranges only encompassing reserved SPIs. */
	if (sproto != IPPROTO_IPCOMP &&
	    (tspi < sspi || tspi <= SPI_RESERVED_MAX)) {
		(*errval) = EINVAL;
		return 0;
	}
	if (sproto == IPPROTO_IPCOMP && (tspi < sspi ||
	    tspi <= CPI_RESERVED_MAX ||
	    tspi >= CPI_PRIVATE_MIN)) {
		(*errval) = EINVAL;
		return 0;
	}

	/* Limit the range to not include reserved areas. */
	if (sspi <= SPI_RESERVED_MAX)
		sspi = SPI_RESERVED_MAX + 1;

	/* For IPCOMP the CPI is only 16 bits long, what a good idea.... */

	if (sproto == IPPROTO_IPCOMP) {
		u_int32_t t;
		if (sspi >= 0x10000)
			sspi = 0xffff;
		if (tspi >= 0x10000)
			tspi = 0xffff;
		if (sspi > tspi) {
			t = sspi; sspi = tspi; tspi = t;
		}
	}

	if (sspi == tspi)   /* Asking for a specific SPI. */
		nums = 1;
	else
		nums = 100;  /* Arbitrarily chosen */

	/* allocate ahead of time to avoid potential sleeping race in loop */
	tdbp = tdb_alloc(rdomain);

	while (nums--) {
		if (sspi == tspi)  /* Specific SPI asked. */
			spi = tspi;
		else    /* Range specified */
			spi = sspi + arc4random_uniform(tspi - sspi);

		/* Don't allocate reserved SPIs.  */
		if (spi >= SPI_RESERVED_MIN && spi <= SPI_RESERVED_MAX)
			continue;
		else
			spi = htonl(spi);

		/* Check whether we're using this SPI already. */
		s = splsoftnet();
		exists = gettdb(rdomain, spi, dst, sproto);
		splx(s);

		if (exists)
			continue;


		tdbp->tdb_spi = spi;
		bcopy(&dst->sa, &tdbp->tdb_dst.sa, SA_LEN(&dst->sa));
		bcopy(&src->sa, &tdbp->tdb_src.sa, SA_LEN(&src->sa));
		tdbp->tdb_sproto = sproto;
		tdbp->tdb_flags |= TDBF_INVALID; /* Mark SA invalid for now. */
		tdbp->tdb_satype = SADB_SATYPE_UNSPEC;
		puttdb(tdbp);

		/* Setup a "silent" expiration (since TDBF_INVALID's set). */
		if (ipsec_keep_invalid > 0) {
			tdbp->tdb_flags |= TDBF_TIMER;
			tdbp->tdb_exp_timeout = ipsec_keep_invalid;
			timeout_add_sec(&tdbp->tdb_timer_tmo,
			    ipsec_keep_invalid);
		}

		return spi;
	}

	(*errval) = EEXIST;
	tdb_free(tdbp);
	return 0;
}

/*
 * An IPSP SAID is really the concatenation of the SPI found in the
 * packet, the destination address of the packet and the IPsec protocol.
 * When we receive an IPSP packet, we need to look up its tunnel descriptor
 * block, based on the SPI in the packet and the destination address (which
 * is really one of our addresses if we received the packet!
 *
 * Caller is responsible for setting at least splsoftnet().
 */
struct tdb *
gettdb(u_int rdomain, u_int32_t spi, union sockaddr_union *dst, u_int8_t proto)
{
	u_int32_t hashval;
	struct tdb *tdbp;

	if (tdbh == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, spi, dst, proto);

	for (tdbp = tdbh[hashval]; tdbp != NULL; tdbp = tdbp->tdb_hnext)
		if ((tdbp->tdb_spi == spi) && (tdbp->tdb_sproto == proto) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    !memcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa)))
			break;

	return tdbp;
}

/*
 * Same as gettdb() but compare SRC as well, so we
 * use the tdbsrc[] hash table.  Setting spi to 0
 * matches all SPIs.
 */
struct tdb *
gettdbbysrcdst(u_int rdomain, u_int32_t spi, union sockaddr_union *src,
    union sockaddr_union *dst, u_int8_t proto)
{
	u_int32_t hashval;
	struct tdb *tdbp;
	union sockaddr_union su_null;

	if (tdbsrc == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, 0, src, proto);

	for (tdbp = tdbsrc[hashval]; tdbp != NULL; tdbp = tdbp->tdb_snext)
		if (tdbp->tdb_sproto == proto &&
		    (spi == 0 || tdbp->tdb_spi == spi) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (tdbp->tdb_dst.sa.sa_family == AF_UNSPEC ||
		    !memcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa))) &&
		    !memcmp(&tdbp->tdb_src, src, SA_LEN(&src->sa)))
			break;

	if (tdbp != NULL)
		return (tdbp);

	memset(&su_null, 0, sizeof(su_null));
	su_null.sa.sa_len = sizeof(struct sockaddr);
	hashval = tdb_hash(rdomain, 0, &su_null, proto);

	for (tdbp = tdbsrc[hashval]; tdbp != NULL; tdbp = tdbp->tdb_snext)
		if (tdbp->tdb_sproto == proto &&
		    (spi == 0 || tdbp->tdb_spi == spi) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (tdbp->tdb_dst.sa.sa_family == AF_UNSPEC ||
		    !memcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa))) &&
		    tdbp->tdb_src.sa.sa_family == AF_UNSPEC)
			break;

	return (tdbp);
}

/*
 * Check that IDs match. Return true if so. The t* range of
 * arguments contains information from TDBs; the p* range of
 * arguments contains information from policies or already
 * established TDBs.
 */
int
ipsp_aux_match(struct tdb *tdb,
    struct ipsec_ref *psrcid,
    struct ipsec_ref *pdstid,
    struct sockaddr_encap *pfilter,
    struct sockaddr_encap *pfiltermask)
{
	if (psrcid != NULL)
		if (tdb->tdb_srcid == NULL ||
		    !ipsp_ref_match(tdb->tdb_srcid, psrcid))
			return 0;

	if (pdstid != NULL)
		if (tdb->tdb_dstid == NULL ||
		    !ipsp_ref_match(tdb->tdb_dstid, pdstid))
			return 0;

	/* Check for filter matches. */
	if (pfilter != NULL && pfiltermask != NULL &&
	    tdb->tdb_filter.sen_type) {
		/*
		 * XXX We should really be doing a subnet-check (see
		 * whether the TDB-associated filter is a subset
		 * of the policy's. For now, an exact match will solve
		 * most problems (all this will do is make every
		 * policy get its own SAs).
		 */
		if (memcmp(&tdb->tdb_filter, pfilter,
		    sizeof(struct sockaddr_encap)) ||
		    memcmp(&tdb->tdb_filtermask, pfiltermask,
		    sizeof(struct sockaddr_encap)))
			return 0;
	}

	return 1;
}

/*
 * Get an SA given the remote address, the security protocol type, and
 * the desired IDs.
 */
struct tdb *
gettdbbydst(u_int rdomain, union sockaddr_union *dst, u_int8_t sproto,
    struct ipsec_ref *srcid, struct ipsec_ref *dstid,
    struct sockaddr_encap *filter, struct sockaddr_encap *filtermask)
{
	u_int32_t hashval;
	struct tdb *tdbp;

	if (tdbdst == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, 0, dst, sproto);

	for (tdbp = tdbdst[hashval]; tdbp != NULL; tdbp = tdbp->tdb_dnext)
		if ((tdbp->tdb_sproto == sproto) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (!memcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa)))) {
			/* Do IDs match ? */
			if (!ipsp_aux_match(tdbp, srcid, dstid, filter,
			    filtermask))
				continue;
			break;
		}

	return tdbp;
}

/*
 * Get an SA given the source address, the security protocol type, and
 * the desired IDs.
 */
struct tdb *
gettdbbysrc(u_int rdomain, union sockaddr_union *src, u_int8_t sproto,
    struct ipsec_ref *srcid, struct ipsec_ref *dstid,
    struct sockaddr_encap *filter, struct sockaddr_encap *filtermask)
{
	u_int32_t hashval;
	struct tdb *tdbp;

	if (tdbsrc == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, 0, src, sproto);

	for (tdbp = tdbsrc[hashval]; tdbp != NULL; tdbp = tdbp->tdb_snext)
		if ((tdbp->tdb_sproto == sproto) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (!memcmp(&tdbp->tdb_src, src, SA_LEN(&src->sa)))) {
			/* Check whether IDs match */
			if (!ipsp_aux_match(tdbp, dstid, srcid, filter,
			    filtermask))
				continue;
			break;
		}

	return tdbp;
}

#if DDB

#define NBUCKETS 16
void
tdb_hashstats(void)
{
	int i, cnt, buckets[NBUCKETS];
	struct tdb *tdbp;

	if (tdbh == NULL) {
		db_printf("no tdb hash table\n");
		return;
	}

	memset(buckets, 0, sizeof(buckets));
	for (i = 0; i <= tdb_hashmask; i++) {
		cnt = 0;
		for (tdbp = tdbh[i]; cnt < NBUCKETS - 1 && tdbp != NULL;
		    tdbp = tdbp->tdb_hnext)
			cnt++;
		buckets[cnt]++;
	}

	db_printf("tdb cnt\t\tbucket cnt\n");
	for (i = 0; i < NBUCKETS; i++)
		if (buckets[i] > 0)
			db_printf("%d%s\t\t%d\n", i, i == NBUCKETS - 1 ?
			    "+" : "", buckets[i]);
}
#endif	/* DDB */

/*
 * Caller is responsible for setting at least splsoftnet().
 */
int
tdb_walk(u_int rdomain, int (*walker)(struct tdb *, void *, int), void *arg)
{
	int i, rval = 0;
	struct tdb *tdbp, *next;

	if (tdbh == NULL)
		return ENOENT;

	for (i = 0; i <= tdb_hashmask; i++)
		for (tdbp = tdbh[i]; rval == 0 && tdbp != NULL; tdbp = next) {
			next = tdbp->tdb_hnext;

			if (rdomain != tdbp->tdb_rdomain)
				continue;

			if (i == tdb_hashmask && next == NULL)
				rval = walker(tdbp, (void *)arg, 1);
			else
				rval = walker(tdbp, (void *)arg, 0);
		}

	return rval;
}

/*
 * Called at splsoftclock().
 */
void
tdb_timeout(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_TIMER))
		return;

	/* If it's an "invalid" TDB do a silent expiration. */
	if (!(tdb->tdb_flags & TDBF_INVALID))
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	tdb_delete(tdb);
}

void
tdb_firstuse(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_SOFT_FIRSTUSE))
		return;

	/* If the TDB hasn't been used, don't renew it. */
	if (tdb->tdb_first_use != 0)
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	tdb_delete(tdb);
}

void
tdb_soft_timeout(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_SOFT_TIMER))
		return;

	/* Soft expirations. */
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_TIMER;
}

void
tdb_soft_firstuse(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_SOFT_FIRSTUSE))
		return;

	/* If the TDB hasn't been used, don't renew it. */
	if (tdb->tdb_first_use != 0)
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_FIRSTUSE;
}

/*
 * Caller is responsible for splsoftnet().
 */
void
tdb_rehash(void)
{
	struct tdb **new_tdbh, **new_tdbdst, **new_srcaddr, *tdbp, *tdbnp;
	u_int i, old_hashmask = tdb_hashmask;
	u_int32_t hashval;

	tdb_hashmask = (tdb_hashmask << 1) | 1;

	arc4random_buf(&tdbkey, sizeof(tdbkey));
	new_tdbh = mallocarray(tdb_hashmask + 1, sizeof(struct tdb *), M_TDB,
	    M_WAITOK | M_ZERO);
	new_tdbdst = mallocarray(tdb_hashmask + 1, sizeof(struct tdb *), M_TDB,
	    M_WAITOK | M_ZERO);
	new_srcaddr = mallocarray(tdb_hashmask + 1, sizeof(struct tdb *), M_TDB,
	    M_WAITOK | M_ZERO);

	for (i = 0; i <= old_hashmask; i++) {
		for (tdbp = tdbh[i]; tdbp != NULL; tdbp = tdbnp) {
			tdbnp = tdbp->tdb_hnext;
			hashval = tdb_hash(tdbp->tdb_rdomain,
			    tdbp->tdb_spi, &tdbp->tdb_dst,
			    tdbp->tdb_sproto);
			tdbp->tdb_hnext = new_tdbh[hashval];
			new_tdbh[hashval] = tdbp;
		}

		for (tdbp = tdbdst[i]; tdbp != NULL; tdbp = tdbnp) {
			tdbnp = tdbp->tdb_dnext;
			hashval = tdb_hash(tdbp->tdb_rdomain,
			    0, &tdbp->tdb_dst,
			    tdbp->tdb_sproto);
			tdbp->tdb_dnext = new_tdbdst[hashval];
			new_tdbdst[hashval] = tdbp;
		}

		for (tdbp = tdbsrc[i]; tdbp != NULL; tdbp = tdbnp) {
			tdbnp = tdbp->tdb_snext;
			hashval = tdb_hash(tdbp->tdb_rdomain,
			    0, &tdbp->tdb_src,
			    tdbp->tdb_sproto);
			tdbp->tdb_snext = new_srcaddr[hashval];
			new_srcaddr[hashval] = tdbp;
		}
	}

	free(tdbh, M_TDB, 0);
	tdbh = new_tdbh;

	free(tdbdst, M_TDB, 0);
	tdbdst = new_tdbdst;

	free(tdbsrc, M_TDB, 0);
	tdbsrc = new_srcaddr;
}

/*
 * Add TDB in the hash table.
 */
void
puttdb(struct tdb *tdbp)
{
	u_int32_t hashval;
	int s = splsoftnet();

	if (tdbh == NULL) {
		arc4random_buf(&tdbkey, sizeof(tdbkey));
		tdbh = mallocarray(tdb_hashmask + 1, sizeof(struct tdb *),
		    M_TDB, M_WAITOK | M_ZERO);
		tdbdst = mallocarray(tdb_hashmask + 1, sizeof(struct tdb *),
		    M_TDB, M_WAITOK | M_ZERO);
		tdbsrc = mallocarray(tdb_hashmask + 1, sizeof(struct tdb *),
		    M_TDB, M_WAITOK | M_ZERO);
	}

	hashval = tdb_hash(tdbp->tdb_rdomain, tdbp->tdb_spi,
	    &tdbp->tdb_dst, tdbp->tdb_sproto);

	/*
	 * Rehash if this tdb would cause a bucket to have more than
	 * two items and if the number of tdbs exceed 10% of the
	 * bucket count.  This number is arbitratily chosen and is
	 * just a measure to not keep rehashing when adding and
	 * removing tdbs which happens to always end up in the same
	 * bucket, which is not uncommon when doing manual keying.
	 */
	if (tdbh[hashval] != NULL && tdbh[hashval]->tdb_hnext != NULL &&
	    tdb_count * 10 > tdb_hashmask + 1) {
		tdb_rehash();
		hashval = tdb_hash(tdbp->tdb_rdomain, tdbp->tdb_spi,
		    &tdbp->tdb_dst, tdbp->tdb_sproto);
	}

	tdbp->tdb_hnext = tdbh[hashval];
	tdbh[hashval] = tdbp;

	hashval = tdb_hash(tdbp->tdb_rdomain, 0, &tdbp->tdb_dst,
	    tdbp->tdb_sproto);
	tdbp->tdb_dnext = tdbdst[hashval];
	tdbdst[hashval] = tdbp;

	hashval = tdb_hash(tdbp->tdb_rdomain, 0, &tdbp->tdb_src,
	    tdbp->tdb_sproto);
	tdbp->tdb_snext = tdbsrc[hashval];
	tdbsrc[hashval] = tdbp;

	tdb_count++;

	ipsec_last_added = time_second;

	splx(s);
}

/*
 * Caller is responsible to set at least splsoftnet().
 */
void
tdb_delete(struct tdb *tdbp)
{
	struct tdb *tdbpp;
	u_int32_t hashval;
	int s;

	if (tdbh == NULL)
		return;

	s = splsoftnet();

	hashval = tdb_hash(tdbp->tdb_rdomain, tdbp->tdb_spi,
	    &tdbp->tdb_dst, tdbp->tdb_sproto);

	if (tdbh[hashval] == tdbp) {
		tdbh[hashval] = tdbp->tdb_hnext;
	} else {
		for (tdbpp = tdbh[hashval]; tdbpp != NULL;
		    tdbpp = tdbpp->tdb_hnext) {
			if (tdbpp->tdb_hnext == tdbp) {
				tdbpp->tdb_hnext = tdbp->tdb_hnext;
				break;
			}
		}
	}

	tdbp->tdb_hnext = NULL;

	hashval = tdb_hash(tdbp->tdb_rdomain, 0, &tdbp->tdb_dst,
	    tdbp->tdb_sproto);

	if (tdbdst[hashval] == tdbp) {
		tdbdst[hashval] = tdbp->tdb_dnext;
	} else {
		for (tdbpp = tdbdst[hashval]; tdbpp != NULL;
		    tdbpp = tdbpp->tdb_dnext) {
			if (tdbpp->tdb_dnext == tdbp) {
				tdbpp->tdb_dnext = tdbp->tdb_dnext;
				break;
			}
		}
	}

	tdbp->tdb_dnext = NULL;

	hashval = tdb_hash(tdbp->tdb_rdomain, 0, &tdbp->tdb_src,
	    tdbp->tdb_sproto);

	if (tdbsrc[hashval] == tdbp) {
		tdbsrc[hashval] = tdbp->tdb_snext;
	}
	else {
		for (tdbpp = tdbsrc[hashval]; tdbpp != NULL;
		    tdbpp = tdbpp->tdb_snext) {
			if (tdbpp->tdb_snext == tdbp) {
				tdbpp->tdb_snext = tdbp->tdb_snext;
				break;
			}
		}
	}

	tdbp->tdb_snext = NULL;
	tdb_free(tdbp);
	tdb_count--;

	splx(s);
}

/*
 * Allocate a TDB and initialize a few basic fields.
 */
struct tdb *
tdb_alloc(u_int rdomain)
{
	struct tdb *tdbp;

	tdbp = malloc(sizeof(*tdbp), M_TDB, M_WAITOK | M_ZERO);

	TAILQ_INIT(&tdbp->tdb_policy_head);

	/* Record establishment time. */
	tdbp->tdb_established = time_second;

	/* Save routing domain */
	tdbp->tdb_rdomain = rdomain;

	/* Initialize timeouts. */
	timeout_set(&tdbp->tdb_timer_tmo, tdb_timeout, tdbp);
	timeout_set(&tdbp->tdb_first_tmo, tdb_firstuse, tdbp);
	timeout_set(&tdbp->tdb_stimer_tmo, tdb_soft_timeout, tdbp);
	timeout_set(&tdbp->tdb_sfirst_tmo, tdb_soft_firstuse, tdbp);

	return tdbp;
}

void
tdb_free(struct tdb *tdbp)
{
	struct ipsec_policy *ipo;

	if (tdbp->tdb_xform) {
		(*(tdbp->tdb_xform->xf_zeroize))(tdbp);
		tdbp->tdb_xform = NULL;
	}

#if NPFSYNC > 0
	/* Cleanup pfsync references */
	pfsync_delete_tdb(tdbp);
#endif

	/* Cleanup SPD references. */
	for (ipo = TAILQ_FIRST(&tdbp->tdb_policy_head); ipo;
	    ipo = TAILQ_FIRST(&tdbp->tdb_policy_head))	{
		TAILQ_REMOVE(&tdbp->tdb_policy_head, ipo, ipo_tdb_next);
		ipo->ipo_tdb = NULL;
		ipo->ipo_last_searched = 0; /* Force a re-search. */
	}

	/* Remove expiration timeouts. */
	tdbp->tdb_flags &= ~(TDBF_FIRSTUSE | TDBF_SOFT_FIRSTUSE | TDBF_TIMER |
	    TDBF_SOFT_TIMER);
	timeout_del(&tdbp->tdb_timer_tmo);
	timeout_del(&tdbp->tdb_first_tmo);
	timeout_del(&tdbp->tdb_stimer_tmo);
	timeout_del(&tdbp->tdb_sfirst_tmo);

	if (tdbp->tdb_srcid) {
		ipsp_reffree(tdbp->tdb_srcid);
		tdbp->tdb_srcid = NULL;
	}

	if (tdbp->tdb_dstid) {
		ipsp_reffree(tdbp->tdb_dstid);
		tdbp->tdb_dstid = NULL;
	}

#if NPF > 0
	if (tdbp->tdb_tag) {
		pf_tag_unref(tdbp->tdb_tag);
		tdbp->tdb_tag = 0;
	}
#endif

	if ((tdbp->tdb_onext) && (tdbp->tdb_onext->tdb_inext == tdbp))
		tdbp->tdb_onext->tdb_inext = NULL;

	if ((tdbp->tdb_inext) && (tdbp->tdb_inext->tdb_onext == tdbp))
		tdbp->tdb_inext->tdb_onext = NULL;

	free(tdbp, M_TDB, 0);
}

/*
 * Do further initializations of a TDB.
 */
int
tdb_init(struct tdb *tdbp, u_int16_t alg, struct ipsecinit *ii)
{
	struct xformsw *xsp;
	int err;
#ifdef ENCDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif

	for (xsp = xformsw; xsp < xformswNXFORMSW; xsp++) {
		if (xsp->xf_type == alg) {
			err = (*(xsp->xf_init))(tdbp, xsp, ii);
			return err;
		}
	}

	DPRINTF(("tdb_init(): no alg %d for spi %08x, addr %s, proto %d\n",
	    alg, ntohl(tdbp->tdb_spi), ipsp_address(&tdbp->tdb_dst, buf,
	    sizeof(buf)), tdbp->tdb_sproto));

	return EINVAL;
}

#ifdef ENCDEBUG
/* Return a printable string for the address. */
const char *
ipsp_address(union sockaddr_union *sa, char *buf, socklen_t size)
{
	switch (sa->sa.sa_family) {
	case AF_INET:
		return inet_ntop(AF_INET, &sa->sin.sin_addr,
		    buf, (size_t)size);

#ifdef INET6
	case AF_INET6:
		return inet_ntop(AF_INET6, &sa->sin6.sin6_addr,
		    buf, (size_t)size);
#endif /* INET6 */

	default:
		return "(unknown address family)";
	}
}
#endif /* ENCDEBUG */

/* Check whether an IP{4,6} address is unspecified. */
int
ipsp_is_unspecified(union sockaddr_union addr)
{
	switch (addr.sa.sa_family) {
	case AF_INET:
		if (addr.sin.sin_addr.s_addr == INADDR_ANY)
			return 1;
		else
			return 0;

#ifdef INET6
	case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&addr.sin6.sin6_addr))
			return 1;
		else
			return 0;
#endif /* INET6 */

	case 0: /* No family set. */
	default:
		return 1;
	}
}

/* Free reference-counted structure. */
void
ipsp_reffree(struct ipsec_ref *ipr)
{
#ifdef DIAGNOSTIC
	if (ipr->ref_count <= 0)
		printf("ipsp_reffree: illegal reference count %d for "
		    "object %p (len = %d, malloctype = %d)\n",
		    ipr->ref_count, ipr, ipr->ref_len, ipr->ref_malloctype);
#endif
	if (--ipr->ref_count <= 0)
		free(ipr, ipr->ref_malloctype, 0);
}

/* Return true if the two structures match. */
int
ipsp_ref_match(struct ipsec_ref *ref1, struct ipsec_ref *ref2)
{
	if (ref1->ref_type != ref2->ref_type ||
	    ref1->ref_len != ref2->ref_len ||
	    memcmp(ref1 + 1, ref2 + 1, ref1->ref_len))
		return 0;

	return 1;
}
