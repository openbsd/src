/*	$OpenBSD: ip_ipsp.c,v 1.182 2011/03/31 10:36:42 jasper Exp $	*/
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#endif /* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>
#include <crypto/xform.h>
#include <dev/rndvar.h>

#ifdef DDB
#include <ddb/db_output.h>
void tdb_hashstats(void);
#endif

#ifdef ENCDEBUG
#define	DPRINTF(x)	if (encdebug) printf x
#else
#define	DPRINTF(x)
#endif

u_int8_t	get_sa_require(struct inpcb *);
void		tdb_rehash(void);
void		tdb_timeout(void *v);
void		tdb_firstuse(void *v);
void		tdb_soft_timeout(void *v);
void		tdb_soft_firstuse(void *v);
int		tdb_hash(u_int, u_int32_t, union sockaddr_union *, u_int8_t);

extern int	ipsec_auth_default_level;
extern int	ipsec_esp_trans_default_level;
extern int	ipsec_esp_network_default_level;
extern int	ipsec_ipcomp_default_level;

extern int encdebug;
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

unsigned char ipseczeroes[IPSEC_ZEROES_SIZE]; /* zeroes! */

#define	TDB_HASHSIZE_INIT	32

static struct tdb **tdbh = NULL;
static struct tdb **tdbaddr = NULL;
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
	static u_int32_t mult1 = 0, mult2 = 0;
	u_int8_t *ptr = (u_int8_t *) dst;
	int i, shift;
	u_int64_t hash;
	int val32 = 0;

	while (mult1 == 0)
		mult1 = arc4random();
	while (mult2 == 0)
		mult2 = arc4random();

	hash = (spi ^ proto ^ rdomain) * mult1;
	for (i = 0; i < SA_LEN(&dst->sa); i++) {
		val32 = (val32 << 8) | ptr[i];
		if (i % 4 == 3) {
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
reserve_spi(u_int rdomain, u_int32_t sspi, u_int32_t tspi,
    union sockaddr_union *src, union sockaddr_union *dst,
    u_int8_t sproto, int *errval)
{
	struct tdb *tdbp;
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
		s = spltdb();
		tdbp = gettdb(rdomain, spi, dst, sproto);
		splx(s);

		if (tdbp != (struct tdb *) NULL)
			continue;

		tdbp = tdb_alloc(rdomain);

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
		    !bcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa)))
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
		    !bcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa))) &&
		    !bcmp(&tdbp->tdb_src, src, SA_LEN(&src->sa)))
			break;

	if (tdbp != NULL)
		return (tdbp);

	bzero(&su_null, sizeof(su_null));
	su_null.sa.sa_len = sizeof(struct sockaddr);
	hashval = tdb_hash(rdomain, 0, &su_null, proto);

	for (tdbp = tdbsrc[hashval]; tdbp != NULL; tdbp = tdbp->tdb_snext)
		if (tdbp->tdb_sproto == proto &&
		    (spi == 0 || tdbp->tdb_spi == spi) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (tdbp->tdb_dst.sa.sa_family == AF_UNSPEC ||
		    !bcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa))) &&
		    tdbp->tdb_src.sa.sa_family == AF_UNSPEC)
			break;

	return (tdbp);
}

/*
 * Check that credentials and IDs match. Return true if so. The t*
 * range of arguments contains information from TDBs; the p*
 * range of arguments contains information from policies or
 * already established TDBs.
 */
int
ipsp_aux_match(struct tdb *tdb,
    struct ipsec_ref *psrcid,
    struct ipsec_ref *pdstid,
    struct ipsec_ref *plcred,
    struct ipsec_ref *prcred,
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

	if (plcred != NULL)
		if (tdb->tdb_local_cred == NULL ||
		   !ipsp_ref_match(tdb->tdb_local_cred, plcred))
			return 0;

	if (prcred != NULL)
		if (tdb->tdb_remote_cred == NULL ||
		    !ipsp_ref_match(tdb->tdb_remote_cred, prcred))
			return 0;

	/* Check for filter matches. */
	if (tdb->tdb_filter.sen_type) {
		/*
		 * XXX We should really be doing a subnet-check (see
		 * whether the TDB-associated filter is a subset
		 * of the policy's. For now, an exact match will solve
		 * most problems (all this will do is make every
		 * policy get its own SAs).
		 */
		if (bcmp(&tdb->tdb_filter, pfilter,
		    sizeof(struct sockaddr_encap)) ||
		    bcmp(&tdb->tdb_filtermask, pfiltermask,
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
gettdbbyaddr(u_int rdomain, union sockaddr_union *dst, u_int8_t sproto,
    struct ipsec_ref *srcid, struct ipsec_ref *dstid,
    struct ipsec_ref *local_cred, struct mbuf *m, int af,
    struct sockaddr_encap *filter, struct sockaddr_encap *filtermask)
{
	u_int32_t hashval;
	struct tdb *tdbp;

	if (tdbaddr == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, 0, dst, sproto);

	for (tdbp = tdbaddr[hashval]; tdbp != NULL; tdbp = tdbp->tdb_anext)
		if ((tdbp->tdb_sproto == sproto) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (!bcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa)))) {
			/* Do IDs and local credentials match ? */
			if (!ipsp_aux_match(tdbp, srcid, dstid,
			    local_cred, NULL, filter, filtermask))
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
    struct mbuf *m, int af, struct sockaddr_encap *filter,
    struct sockaddr_encap *filtermask)
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
		    (!bcmp(&tdbp->tdb_src, src, SA_LEN(&src->sa)))) {
			/* Check whether IDs match */
			if (!ipsp_aux_match(tdbp, dstid, srcid, NULL, NULL,
			    filter, filtermask))
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

	bzero (buckets, sizeof(buckets));
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
 * Caller is responsible for setting at least spltdb().
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
 * Caller is responsible for spltdb().
 */
void
tdb_rehash(void)
{
	struct tdb **new_tdbh, **new_tdbaddr, **new_srcaddr, *tdbp, *tdbnp;
	u_int i, old_hashmask = tdb_hashmask;
	u_int32_t hashval;

	tdb_hashmask = (tdb_hashmask << 1) | 1;

	new_tdbh = malloc(sizeof(struct tdb *) * (tdb_hashmask + 1), M_TDB,
	    M_WAITOK | M_ZERO);
	new_tdbaddr = malloc(sizeof(struct tdb *) * (tdb_hashmask + 1), M_TDB,
	    M_WAITOK | M_ZERO);
	new_srcaddr = malloc(sizeof(struct tdb *) * (tdb_hashmask + 1), M_TDB,
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

		for (tdbp = tdbaddr[i]; tdbp != NULL; tdbp = tdbnp) {
			tdbnp = tdbp->tdb_anext;
			hashval = tdb_hash(tdbp->tdb_rdomain,
			    0, &tdbp->tdb_dst,
			    tdbp->tdb_sproto);
			tdbp->tdb_anext = new_tdbaddr[hashval];
			new_tdbaddr[hashval] = tdbp;
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

	free(tdbh, M_TDB);
	tdbh = new_tdbh;

	free(tdbaddr, M_TDB);
	tdbaddr = new_tdbaddr;

	free(tdbsrc, M_TDB);
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

	if (tdbh == NULL) {
		tdbh = malloc(sizeof(struct tdb *) * (tdb_hashmask + 1), M_TDB,
		    M_WAITOK | M_ZERO);
		tdbaddr = malloc(sizeof(struct tdb *) * (tdb_hashmask + 1),
		    M_TDB, M_WAITOK | M_ZERO);
		tdbsrc = malloc(sizeof(struct tdb *) * (tdb_hashmask + 1),
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
	tdbp->tdb_anext = tdbaddr[hashval];
	tdbaddr[hashval] = tdbp;

	hashval = tdb_hash(tdbp->tdb_rdomain, 0, &tdbp->tdb_src,
	    tdbp->tdb_sproto);
	tdbp->tdb_snext = tdbsrc[hashval];
	tdbsrc[hashval] = tdbp;

	tdb_count++;

	ipsec_last_added = time_second;

	splx(s);
}

/*
 * Caller is responsible to set at least spltdb().
 */
void
tdb_delete(struct tdb *tdbp)
{
	struct tdb *tdbpp;
	u_int32_t hashval;
	int s;

	if (tdbh == NULL)
		return;

	hashval = tdb_hash(tdbp->tdb_rdomain, tdbp->tdb_spi,
	    &tdbp->tdb_dst, tdbp->tdb_sproto);

	s = spltdb();
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

	if (tdbaddr[hashval] == tdbp) {
		tdbaddr[hashval] = tdbp->tdb_anext;
	} else {
		for (tdbpp = tdbaddr[hashval]; tdbpp != NULL;
		    tdbpp = tdbpp->tdb_anext) {
			if (tdbpp->tdb_anext == tdbp) {
				tdbpp->tdb_anext = tdbp->tdb_anext;
				break;
			}
		}
	}

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

	/* Init Incoming SA-Binding Queues. */
	TAILQ_INIT(&tdbp->tdb_inp_out);
	TAILQ_INIT(&tdbp->tdb_inp_in);

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
	struct inpcb *inp;

	if (tdbp->tdb_xform) {
		(*(tdbp->tdb_xform->xf_zeroize))(tdbp);
		tdbp->tdb_xform = NULL;
	}

#if NPFSYNC > 0
	/* Cleanup pfsync references */
	pfsync_delete_tdb(tdbp);
#endif

	/* Cleanup inp references. */
	for (inp = TAILQ_FIRST(&tdbp->tdb_inp_in); inp;
	    inp = TAILQ_FIRST(&tdbp->tdb_inp_in)) {
		TAILQ_REMOVE(&tdbp->tdb_inp_in, inp, inp_tdb_in_next);
		inp->inp_tdb_in = NULL;
	}

	for (inp = TAILQ_FIRST(&tdbp->tdb_inp_out); inp;
	    inp = TAILQ_FIRST(&tdbp->tdb_inp_out)) {
		TAILQ_REMOVE(&tdbp->tdb_inp_out, inp, inp_tdb_out_next);
		inp->inp_tdb_out = NULL;
	}

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

	if (tdbp->tdb_local_auth) {
		ipsp_reffree(tdbp->tdb_local_auth);
		tdbp->tdb_local_auth = NULL;
	}

	if (tdbp->tdb_remote_auth) {
		ipsp_reffree(tdbp->tdb_remote_auth);
		tdbp->tdb_remote_auth = NULL;
	}

	if (tdbp->tdb_srcid) {
		ipsp_reffree(tdbp->tdb_srcid);
		tdbp->tdb_srcid = NULL;
	}

	if (tdbp->tdb_dstid) {
		ipsp_reffree(tdbp->tdb_dstid);
		tdbp->tdb_dstid = NULL;
	}

	if (tdbp->tdb_local_cred) {
		ipsp_reffree(tdbp->tdb_local_cred);
		tdbp->tdb_local_cred = NULL;
	}

	if (tdbp->tdb_remote_cred) {
		ipsp_reffree(tdbp->tdb_remote_cred);
		tdbp->tdb_remote_cred = NULL;
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

	free(tdbp, M_TDB);
}

/*
 * Do further initializations of a TDB.
 */
int
tdb_init(struct tdb *tdbp, u_int16_t alg, struct ipsecinit *ii)
{
	struct xformsw *xsp;
	int err;

	for (xsp = xformsw; xsp < xformswNXFORMSW; xsp++) {
		if (xsp->xf_type == alg) {
			err = (*(xsp->xf_init))(tdbp, xsp, ii);
			return err;
		}
	}

	DPRINTF(("tdb_init(): no alg %d for spi %08x, addr %s, proto %d\n",
	    alg, ntohl(tdbp->tdb_spi), ipsp_address(tdbp->tdb_dst),
	    tdbp->tdb_sproto));

	return EINVAL;
}

/*
 * Check which transformations are required.
 */
u_int8_t
get_sa_require(struct inpcb *inp)
{
	u_int8_t sareq = 0;

	if (inp != NULL) {
		sareq |= inp->inp_seclevel[SL_AUTH] >= IPSEC_LEVEL_USE ?
		    NOTIFY_SATYPE_AUTH : 0;
		sareq |= inp->inp_seclevel[SL_ESP_TRANS] >= IPSEC_LEVEL_USE ?
		    NOTIFY_SATYPE_CONF : 0;
		sareq |= inp->inp_seclevel[SL_ESP_NETWORK] >= IPSEC_LEVEL_USE ?
		    NOTIFY_SATYPE_TUNNEL : 0;
	} else {
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
tdb_add_inp(struct tdb *tdb, struct inpcb *inp, int inout)
{
	if (inout) {
		if (inp->inp_tdb_in) {
			if (inp->inp_tdb_in == tdb)
				return;

			TAILQ_REMOVE(&inp->inp_tdb_in->tdb_inp_in, inp,
			    inp_tdb_in_next);
		}

		inp->inp_tdb_in = tdb;
		TAILQ_INSERT_TAIL(&tdb->tdb_inp_in, inp, inp_tdb_in_next);
	} else {
		if (inp->inp_tdb_out) {
			if (inp->inp_tdb_out == tdb)
				return;

			TAILQ_REMOVE(&inp->inp_tdb_out->tdb_inp_out, inp,
			    inp_tdb_out_next);
		}

		inp->inp_tdb_out = tdb;
		TAILQ_INSERT_TAIL(&tdb->tdb_inp_out, inp, inp_tdb_out_next);
	}
}

/* Return a printable string for the IPv4 address. */
char *
inet_ntoa4(struct in_addr ina)
{
	static char buf[4][4 * sizeof "123" + 4];
	unsigned char *ucp = (unsigned char *) &ina;
	static int i = 3;

	i = (i + 1) % 4;
	snprintf(buf[i], sizeof buf[0], "%d.%d.%d.%d",
	    ucp[0] & 0xff, ucp[1] & 0xff,
	    ucp[2] & 0xff, ucp[3] & 0xff);
	return (buf[i]);
}

/* Return a printable string for the address. */
char *
ipsp_address(union sockaddr_union sa)
{
	switch (sa.sa.sa_family) {
#ifdef INET
	case AF_INET:
		return inet_ntoa4(sa.sin.sin_addr);
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		return ip6_sprintf(&sa.sin6.sin6_addr);
#endif /* INET6 */

	default:
		return "(unknown address family)";
	}
}

/* Check whether an IP{4,6} address is unspecified. */
int
ipsp_is_unspecified(union sockaddr_union addr)
{
	switch (addr.sa.sa_family) {
#ifdef INET
	case AF_INET:
		if (addr.sin.sin_addr.s_addr == INADDR_ANY)
			return 1;
		else
			return 0;
#endif /* INET */

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
		free(ipr, ipr->ref_malloctype);
}

/* Mark a TDB as TDBF_SKIPCRYPTO. */
void
ipsp_skipcrypto_mark(struct tdb_ident *tdbi)
{
	struct tdb *tdb;
	int s = spltdb();

	tdb = gettdb(tdbi->rdomain, tdbi->spi, &tdbi->dst, tdbi->proto);
	if (tdb != NULL) {
		tdb->tdb_flags |= TDBF_SKIPCRYPTO;
		tdb->tdb_last_marked = time_second;
	}
	splx(s);
}

/* Unmark a TDB as TDBF_SKIPCRYPTO. */
void
ipsp_skipcrypto_unmark(struct tdb_ident *tdbi)
{
	struct tdb *tdb;
	int s = spltdb();

	tdb = gettdb(tdbi->rdomain, tdbi->spi, &tdbi->dst, tdbi->proto);
	if (tdb != NULL) {
		tdb->tdb_flags &= ~TDBF_SKIPCRYPTO;
		tdb->tdb_last_marked = time_second;
	}
	splx(s);
}

/* Return true if the two structures match. */
int
ipsp_ref_match(struct ipsec_ref *ref1, struct ipsec_ref *ref2)
{
	if (ref1->ref_type != ref2->ref_type ||
	    ref1->ref_len != ref2->ref_len ||
	    bcmp(ref1 + 1, ref2 + 1, ref1->ref_len))
		return 0;

	return 1;
}

#ifdef notyet
/*
 * Go down a chain of IPv4/IPv6/ESP/AH/IPiP chains creating an tag for each
 * IPsec header encountered. The offset where the first header, as well
 * as its type are given to us.
 */
struct m_tag *
ipsp_parse_headers(struct mbuf *m, int off, u_int8_t proto)
{
	int ipv4sa = 0, s, esphlen = 0, trail = 0, i;
	SLIST_HEAD(packet_tags, m_tag) tags;
	unsigned char lasteight[8];
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	struct tdb *tdb;

#ifdef INET
	struct ip iph;
#endif /* INET */

#ifdef INET6
	struct in6_addr ip6_dst;
#endif /* INET6 */

	/* We have to start with a known network protocol. */
	if (proto != IPPROTO_IPV4 && proto != IPPROTO_IPV6)
		return NULL;

	SLIST_INIT(&tags);

	while (1) {
		switch (proto) {
#ifdef INET
		case IPPROTO_IPV4: /* Also IPPROTO_IPIP */
		{
			/*
			 * Save the IP header (we need both the
			 * address and ip_hl).
			 */
			m_copydata(m, off, sizeof(struct ip), (caddr_t) &iph);
			ipv4sa = 1;
			proto = iph.ip_p;
			off += iph.ip_hl << 2;
			break;
		}
#endif /* INET */

#ifdef INET6
		case IPPROTO_IPV6:
		{
			int nxtp, l;

			/* Copy the IPv6 address. */
			m_copydata(m, off + offsetof(struct ip6_hdr, ip6_dst),
			    sizeof(struct ip6_hdr), (caddr_t) &ip6_dst);
			ipv4sa = 0;

			/*
			 * Go down the chain of headers until we encounter a
			 * non-option.
			 */
			for (l = ip6_nexthdr(m, off, proto, &nxtp); l != -1;
			    l = ip6_nexthdr(m, off, proto, &nxtp)) {
				off += l;
				proto = nxtp;

				/* Construct a tag. */
				if (nxtp == IPPROTO_AH)	{
					mtag = m_tag_get(PACKET_TAG_IPSEC_IN_CRYPTO_DONE,
					    sizeof(struct tdb_ident),
					    M_NOWAIT);

					if (mtag == NULL)
						return SLIST_FIRST(&tags);

					tdbi = (struct tdb_ident *) (mtag + 1);
					bzero(tdbi, sizeof(struct tdb_ident));

					m_copydata(m, off + sizeof(u_int32_t),
					    sizeof(u_int32_t),
					    (caddr_t) &tdbi->spi);

					tdbi->proto = IPPROTO_AH;
					tdbi->dst.sin6.sin6_family = AF_INET6;
					tdbi->dst.sin6.sin6_len =
					    sizeof(struct sockaddr_in6);
					tdbi->dst.sin6.sin6_addr = ip6_dst;
					tdbi->rdomain =
					    rtable_l2(m->m_pkthdr.rdomain);
					SLIST_INSERT_HEAD(&tags,
					    mtag, m_tag_link);
				}
				else
					if (nxtp == IPPROTO_IPV6)
						m_copydata(m, off +
						    offsetof(struct ip6_hdr,
							ip6_dst),
						    sizeof(struct ip6_hdr),
						    (caddr_t) &ip6_dst);
			}
			break;
		}
#endif /* INET6 */

		case IPPROTO_ESP:
		/* Verify that this has been decrypted. */
		{
			union sockaddr_union su;
			u_int32_t spi;

			m_copydata(m, off, sizeof(u_int32_t), (caddr_t) &spi);
			bzero(&su, sizeof(union sockaddr_union));

			s = spltdb();

#ifdef INET
			if (ipv4sa) {
				su.sin.sin_family = AF_INET;
				su.sin.sin_len = sizeof(struct sockaddr_in);
				su.sin.sin_addr = iph.ip_dst;
			}
#endif /* INET */

#ifdef INET6
			if (!ipv4sa) {
				su.sin6.sin6_family = AF_INET6;
				su.sin6.sin6_len = sizeof(struct sockaddr_in6);
				su.sin6.sin6_addr = ip6_dst;
			}
#endif /* INET6 */

			tdb = gettdb(spi, &su, IPPROTO_ESP);
			if (tdb == NULL) {
				splx(s);
				return SLIST_FIRST(&tags);
			}

			/* How large is the ESP header ? We use this later. */
			if (tdb->tdb_flags & TDBF_NOREPLAY)
				esphlen = sizeof(u_int32_t) + tdb->tdb_ivlen;
			else
				esphlen = 2 * sizeof(u_int32_t) +
				    tdb->tdb_ivlen;

			/*
			 * Verify decryption. If the SA is using
			 * random padding (as the "old" ESP SAs were
			 * bound to do, there's nothing we can do to
			 * see if the payload has been decrypted.
			 */
			if (tdb->tdb_flags & TDBF_RANDOMPADDING) {
				splx(s);
				return SLIST_FIRST(&tags);
			}

			/* Update the length of trailing ESP authenticators. */
			if (tdb->tdb_authalgxform)
				trail += tdb->tdb_authalgxform->authsize;

			splx(s);

			/* Copy the last 10 bytes. */
			m_copydata(m, m->m_pkthdr.len - trail - 8, 8,
			    lasteight);

			/* Verify the self-describing padding values. */
			if (lasteight[6] != 0) {
				if (lasteight[6] != lasteight[5])
					return SLIST_FIRST(&tags);

				for (i = 4; lasteight[i + 1] != 1 && i >= 0;
				    i--)
					if (lasteight[i + 1] !=
					    lasteight[i] + 1)
						return SLIST_FIRST(&tags);
			}
		}
		/* FALLTHROUGH */
		case IPPROTO_AH:
			mtag = m_tag_get(PACKET_TAG_IPSEC_IN_CRYPTO_DONE,
			    sizeof(struct tdb_ident), M_NOWAIT);
			if (mtag == NULL)
				return SLIST_FIRST(&tags);

			tdbi = (struct tdb_ident *) (mtag + 1);
			bzero(tdbi, sizeof(struct tdb_ident));

			/* Get SPI off the relevant header. */
			if (proto == IPPROTO_AH)
				m_copydata(m, off + sizeof(u_int32_t),
				    sizeof(u_int32_t), (caddr_t) &tdbi->spi);
			else /* IPPROTO_ESP */
				m_copydata(m, off, sizeof(u_int32_t),
				    (caddr_t) &tdbi->spi);

			tdbi->proto = proto; /* AH or ESP */
			tdbi->rdomain = rtable_l2(m->m_pkthdr.rdomain);

#ifdef INET
			/* Last network header was IPv4. */
			if (ipv4sa) {
				tdbi->dst.sin.sin_family = AF_INET;
				tdbi->dst.sin.sin_len =
				    sizeof(struct sockaddr_in);
				tdbi->dst.sin.sin_addr = iph.ip_dst;
			}
#endif /* INET */

#ifdef INET6
			/* Last network header was IPv6. */
			if (!ipv4sa) {
				tdbi->dst.sin6.sin6_family = AF_INET6;
				tdbi->dst.sin6.sin6_len =
				    sizeof(struct sockaddr_in6);
				tdbi->dst.sin6.sin6_addr = ip6_dst;
			}
#endif /* INET6 */

			SLIST_INSERT_HEAD(&tags, mtag, m_tag_link);

			/* Update next protocol/header and header offset. */
			if (proto == IPPROTO_AH) {
				u_int8_t foo[2];

				m_copydata(m, off, 2 * sizeof(u_int8_t), foo);
				proto = foo[0];
				off += (foo[1] + 2) << 2;
			} else {/* IPPROTO_ESP */
				/* Initialized in IPPROTO_ESP case. */
				off += esphlen;
				proto = lasteight[7];
			}
			break;

		default:
			return SLIST_FIRST(&tags); /* We're done. */
		}
	}
}
#endif /* notyet */
