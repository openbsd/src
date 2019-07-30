/*	$OpenBSD: ip_ipsp.c,v 1.229 2017/11/06 15:12:43 mpi Exp $	*/
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
#include <netinet/ip_ipip.h>

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
int ipsec_ids_idle = 100;		/* keep free ids for 100s */

/* Protected by the NET_LOCK(). */
u_int32_t ipsec_ids_next_flow = 1;	/* may not be zero */
struct ipsec_ids_tree ipsec_ids_tree;
struct ipsec_ids_flows ipsec_ids_flows;
struct ipsec_policy_head ipsec_policy_head =
    TAILQ_HEAD_INITIALIZER(ipsec_policy_head);

void ipsp_ids_timeout(void *);
static inline int ipsp_ids_cmp(const struct ipsec_ids *,
    const struct ipsec_ids *);
static inline int ipsp_ids_flow_cmp(const struct ipsec_ids *,
    const struct ipsec_ids *);
RBT_PROTOTYPE(ipsec_ids_tree, ipsec_ids, id_node_flow, ipsp_ids_cmp);
RBT_PROTOTYPE(ipsec_ids_flows, ipsec_ids, id_node_id, ipsp_ids_flow_cmp);
RBT_GENERATE(ipsec_ids_tree, ipsec_ids, id_node_flow, ipsp_ids_cmp);
RBT_GENERATE(ipsec_ids_flows, ipsec_ids, id_node_id, ipsp_ids_flow_cmp);

/*
 * This is the proper place to define the various encapsulation transforms.
 */

struct xformsw xformsw[] = {
#ifdef IPSEC
{
  .xf_type	= XF_IP4,
  .xf_flags	= 0,
  .xf_name	= "IPv4 Simple Encapsulation",
  .xf_attach	= ipe4_attach,
  .xf_init	= ipe4_init,
  .xf_zeroize	= ipe4_zeroize,
  .xf_input	= ipe4_input,
  .xf_output	= ipip_output,
},
{
  .xf_type	= XF_AH,
  .xf_flags	= XFT_AUTH,
  .xf_name	= "IPsec AH",
  .xf_attach	= ah_attach,
  .xf_init	= ah_init,
  .xf_zeroize	= ah_zeroize,
  .xf_input	= ah_input,
  .xf_output	= ah_output,
},
{
  .xf_type	= XF_ESP,
  .xf_flags	= XFT_CONF|XFT_AUTH,
  .xf_name	= "IPsec ESP",
  .xf_attach	= esp_attach,
  .xf_init	= esp_init,
  .xf_zeroize	= esp_zeroize,
  .xf_input	= esp_input,
  .xf_output	= esp_output,
},
{
  .xf_type	= XF_IPCOMP,
  .xf_flags	= XFT_COMP,
  .xf_name	= "IPcomp",
  .xf_attach	= ipcomp_attach,
  .xf_init	= ipcomp_init,
  .xf_zeroize	= ipcomp_zeroize,
  .xf_input	= ipcomp_input,
  .xf_output	= ipcomp_output,
},
#endif /* IPSEC */
#ifdef TCP_SIGNATURE
{
  .xf_type	= XF_TCPSIGNATURE,
  .xf_flags	= XFT_AUTH,
  .xf_name	= "TCP MD5 Signature Option, RFC 2385",
  .xf_attach	= tcp_signature_tdb_attach,
  .xf_init	= tcp_signature_tdb_init,
  .xf_zeroize	= tcp_signature_tdb_zeroize,
  .xf_input	= tcp_signature_tdb_input,
  .xf_output	= tcp_signature_tdb_output,
}
#endif /* TCP_SIGNATURE */
};

struct xformsw *xformswNXFORMSW = &xformsw[nitems(xformsw)];

#define	TDB_HASHSIZE_INIT	32

/* Protected by the NET_LOCK(). */
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

	NET_ASSERT_LOCKED();

	SipHash24_Init(&ctx, &tdbkey);
	SipHash24_Update(&ctx, &rdomain, sizeof(rdomain));
	SipHash24_Update(&ctx, &spi, sizeof(spi));
	SipHash24_Update(&ctx, &proto, sizeof(proto));
	SipHash24_Update(&ctx, dst, dst->sa.sa_len);

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
	int nums;

	NET_ASSERT_LOCKED();

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
		exists = gettdb(rdomain, spi, dst, sproto);
		if (exists)
			continue;


		tdbp->tdb_spi = spi;
		memcpy(&tdbp->tdb_dst.sa, &dst->sa, dst->sa.sa_len);
		memcpy(&tdbp->tdb_src.sa, &src->sa, src->sa.sa_len);
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
 */
struct tdb *
gettdb(u_int rdomain, u_int32_t spi, union sockaddr_union *dst, u_int8_t proto)
{
	u_int32_t hashval;
	struct tdb *tdbp;

	NET_ASSERT_LOCKED();

	if (tdbh == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, spi, dst, proto);

	for (tdbp = tdbh[hashval]; tdbp != NULL; tdbp = tdbp->tdb_hnext)
		if ((tdbp->tdb_spi == spi) && (tdbp->tdb_sproto == proto) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    !memcmp(&tdbp->tdb_dst, dst, dst->sa.sa_len))
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

	NET_ASSERT_LOCKED();

	if (tdbsrc == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, 0, src, proto);

	for (tdbp = tdbsrc[hashval]; tdbp != NULL; tdbp = tdbp->tdb_snext)
		if (tdbp->tdb_sproto == proto &&
		    (spi == 0 || tdbp->tdb_spi == spi) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (tdbp->tdb_dst.sa.sa_family == AF_UNSPEC ||
		    !memcmp(&tdbp->tdb_dst, dst, dst->sa.sa_len)) &&
		    !memcmp(&tdbp->tdb_src, src, src->sa.sa_len))
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
		    !memcmp(&tdbp->tdb_dst, dst, dst->sa.sa_len)) &&
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
    struct ipsec_ids *ids,
    struct sockaddr_encap *pfilter,
    struct sockaddr_encap *pfiltermask)
{
	if (ids != NULL)
		if (tdb->tdb_ids == NULL ||
		    !ipsp_ids_match(tdb->tdb_ids, ids))
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
    struct ipsec_ids *ids,
    struct sockaddr_encap *filter, struct sockaddr_encap *filtermask)
{
	u_int32_t hashval;
	struct tdb *tdbp;

	NET_ASSERT_LOCKED();

	if (tdbdst == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, 0, dst, sproto);

	for (tdbp = tdbdst[hashval]; tdbp != NULL; tdbp = tdbp->tdb_dnext)
		if ((tdbp->tdb_sproto == sproto) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (!memcmp(&tdbp->tdb_dst, dst, dst->sa.sa_len))) {
			/* Do IDs match ? */
			if (!ipsp_aux_match(tdbp, ids, filter, filtermask))
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
    struct ipsec_ids *ids,
    struct sockaddr_encap *filter, struct sockaddr_encap *filtermask)
{
	u_int32_t hashval;
	struct tdb *tdbp;

	NET_ASSERT_LOCKED();

	if (tdbsrc == NULL)
		return (struct tdb *) NULL;

	hashval = tdb_hash(rdomain, 0, src, sproto);

	for (tdbp = tdbsrc[hashval]; tdbp != NULL; tdbp = tdbp->tdb_snext)
		if ((tdbp->tdb_sproto == sproto) &&
		    (tdbp->tdb_rdomain == rdomain) &&
		    ((tdbp->tdb_flags & TDBF_INVALID) == 0) &&
		    (!memcmp(&tdbp->tdb_src, src, src->sa.sa_len))) {
			/* Check whether IDs match */
			if (!ipsp_aux_match(tdbp, ids, filter,
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

int
tdb_walk(u_int rdomain, int (*walker)(struct tdb *, void *, int), void *arg)
{
	int i, rval = 0;
	struct tdb *tdbp, *next;

	NET_ASSERT_LOCKED();

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

void
tdb_timeout(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_TIMER))
		return;

	NET_LOCK();
	/* If it's an "invalid" TDB do a silent expiration. */
	if (!(tdb->tdb_flags & TDBF_INVALID))
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	tdb_delete(tdb);
	NET_UNLOCK();
}

void
tdb_firstuse(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_SOFT_FIRSTUSE))
		return;

	NET_LOCK();
	/* If the TDB hasn't been used, don't renew it. */
	if (tdb->tdb_first_use != 0)
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_HARD);
	tdb_delete(tdb);
	NET_UNLOCK();
}

void
tdb_soft_timeout(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_SOFT_TIMER))
		return;

	NET_LOCK();
	/* Soft expirations. */
	pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_TIMER;
	NET_UNLOCK();
}

void
tdb_soft_firstuse(void *v)
{
	struct tdb *tdb = v;

	if (!(tdb->tdb_flags & TDBF_SOFT_FIRSTUSE))
		return;

	NET_LOCK();
	/* If the TDB hasn't been used, don't renew it. */
	if (tdb->tdb_first_use != 0)
		pfkeyv2_expire(tdb, SADB_EXT_LIFETIME_SOFT);
	tdb->tdb_flags &= ~TDBF_SOFT_FIRSTUSE;
	NET_UNLOCK();
}

void
tdb_rehash(void)
{
	struct tdb **new_tdbh, **new_tdbdst, **new_srcaddr, *tdbp, *tdbnp;
	u_int i, old_hashmask = tdb_hashmask;
	u_int32_t hashval;

	NET_ASSERT_LOCKED();

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

	NET_ASSERT_LOCKED();

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
}

void
tdb_unlink(struct tdb *tdbp)
{
	struct tdb *tdbpp;
	u_int32_t hashval;

	NET_ASSERT_LOCKED();

	if (tdbh == NULL)
		return;

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
	tdb_count--;
}

void
tdb_delete(struct tdb *tdbp)
{
	NET_ASSERT_LOCKED();

	tdb_unlink(tdbp);
	tdb_free(tdbp);
}

/*
 * Allocate a TDB and initialize a few basic fields.
 */
struct tdb *
tdb_alloc(u_int rdomain)
{
	struct tdb *tdbp;

	NET_ASSERT_LOCKED();

	tdbp = malloc(sizeof(*tdbp), M_TDB, M_WAITOK | M_ZERO);

	TAILQ_INIT(&tdbp->tdb_policy_head);

	/* Record establishment time. */
	tdbp->tdb_established = time_second;

	/* Save routing domain */
	tdbp->tdb_rdomain = rdomain;

	/* Initialize timeouts. */
	timeout_set_proc(&tdbp->tdb_timer_tmo, tdb_timeout, tdbp);
	timeout_set_proc(&tdbp->tdb_first_tmo, tdb_firstuse, tdbp);
	timeout_set_proc(&tdbp->tdb_stimer_tmo, tdb_soft_timeout, tdbp);
	timeout_set_proc(&tdbp->tdb_sfirst_tmo, tdb_soft_firstuse, tdbp);

	return tdbp;
}

void
tdb_free(struct tdb *tdbp)
{
	struct ipsec_policy *ipo;

	NET_ASSERT_LOCKED();

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

	if (tdbp->tdb_ids) {
		ipsp_ids_free(tdbp->tdb_ids);
		tdbp->tdb_ids = NULL;
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

	DPRINTF(("%s: no alg %d for spi %08x, addr %s, proto %d\n", __func__,
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

int
ipsp_ids_match(struct ipsec_ids *a, struct ipsec_ids *b)
{
	return a == b;
}

struct ipsec_ids *
ipsp_ids_insert(struct ipsec_ids *ids)
{
	struct ipsec_ids *found;
	u_int32_t start_flow;

	NET_ASSERT_LOCKED();

	found = RBT_INSERT(ipsec_ids_tree, &ipsec_ids_tree, ids);
	if (found) {
		/* if refcount was zero, then timeout is running */
		if (found->id_refcount++ == 0)
			timeout_del(&found->id_timeout);
		DPRINTF(("%s: ids %p count %d\n", __func__,
		    found, found->id_refcount));
		return found;
	}
	ids->id_flow = start_flow = ipsec_ids_next_flow;
	if (++ipsec_ids_next_flow == 0)
		ipsec_ids_next_flow = 1;
	while (RBT_INSERT(ipsec_ids_flows, &ipsec_ids_flows, ids) != NULL) {
		ids->id_flow = ipsec_ids_next_flow;
		if (++ipsec_ids_next_flow == 0)
			ipsec_ids_next_flow = 1;
		if (ipsec_ids_next_flow == start_flow) {
			DPRINTF(("ipsec_ids_next_flow exhausted %u\n",
			    ipsec_ids_next_flow));
			return NULL;
		}
	}
	ids->id_refcount = 1;
	DPRINTF(("%s: new ids %p flow %u\n", __func__, ids, ids->id_flow));
	timeout_set_proc(&ids->id_timeout, ipsp_ids_timeout, ids);
	return ids;
}

struct ipsec_ids *
ipsp_ids_lookup(u_int32_t ipsecflowinfo)
{
	struct ipsec_ids	key;

	NET_ASSERT_LOCKED();

	key.id_flow = ipsecflowinfo;
	return RBT_FIND(ipsec_ids_flows, &ipsec_ids_flows, &key);
}

/* free ids only from delayed timeout */
void
ipsp_ids_timeout(void *arg)
{
	struct ipsec_ids *ids = arg;

	DPRINTF(("%s: ids %p count %d\n", __func__, ids, ids->id_refcount));
	KASSERT(ids->id_refcount == 0);

	NET_LOCK();
	RBT_REMOVE(ipsec_ids_tree, &ipsec_ids_tree, ids);
	RBT_REMOVE(ipsec_ids_flows, &ipsec_ids_flows, ids);
	free(ids->id_local, M_CREDENTIALS, 0);
	free(ids->id_remote, M_CREDENTIALS, 0);
	free(ids, M_CREDENTIALS, 0);
	NET_UNLOCK();
}

/* decrements refcount, actual free happens in timeout */
void
ipsp_ids_free(struct ipsec_ids *ids)
{
	/*
	 * If the refcount becomes zero, then a timeout is started. This
	 * timeout must be cancelled if refcount is increased from zero.
	 */
	DPRINTF(("%s: ids %p count %d\n", __func__, ids, ids->id_refcount));
	KASSERT(ids->id_refcount > 0);
	if (--ids->id_refcount == 0)
		timeout_add_sec(&ids->id_timeout, ipsec_ids_idle);
}

static int
ipsp_id_cmp(struct ipsec_id *a, struct ipsec_id *b)
{
	if (a->type > b->type)
		return 1;
	if (a->type < b->type)
		return -1;
	if (a->len > b->len)
		return 1;
	if (a->len < b->len)
		return -1;
	return memcmp(a + 1, b + 1, a->len);
}

static inline int
ipsp_ids_cmp(const struct ipsec_ids *a, const struct ipsec_ids *b)
{
	int ret;

	ret = ipsp_id_cmp(a->id_remote, b->id_remote);
	if (ret != 0)
		return ret;
	return ipsp_id_cmp(a->id_local, b->id_local);
}

static inline int
ipsp_ids_flow_cmp(const struct ipsec_ids *a, const struct ipsec_ids *b)
{
	if (a->id_flow > b->id_flow)
		return 1;
	if (a->id_flow < b->id_flow)
		return -1;
	return 0;
}
