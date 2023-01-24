/*	$OpenBSD: rde_aspa.c,v 1.3 2023/01/24 11:28:41 claudio Exp $ */

/*
 * Copyright (c) 2022 Claudio Jeker <claudio@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

#define	UNKNOWN		0x0
#define	NOT_PROVIDER	0x1
#define	PROVIDER	0x2

#define	CP(x, y)	(x | (y << 4))
#define	CP_GET(x, i)	((x >> (i * 4)) & 0xf)

struct rde_aspa_set {
	uint32_t		 as;
	uint32_t		 num;
	uint32_t		*pas;
	uint32_t		*pas_aid;
	int			 next;
};

/*
 * Power of 2 hash table
 * The nodes are stored in the sets array.
 * Additonal data for the rde_aspa_set are stored in data.
 * For lookups only table and mask need to be accessed.
 */
struct rde_aspa {
	struct rde_aspa_set	**table;
	uint32_t		  mask;
	uint32_t		  maxset;
	struct rde_aspa_set	 *sets;
	uint32_t		 *data;
	size_t			  maxdata;
	size_t			  curdata;
	uint32_t		  curset;
	time_t			  lastchange;
};

struct aspa_state {
	int	nhops;
	int	nup_p;
	int	nup_u;
	int	nup_np;
	int	ndown_p;
	int	ndown_u;
	int	ndown_np;
};

/*
 * Use fmix32() the finalization mix of MurmurHash3 as a 32bit hash function.
 */
static inline uint32_t
hash(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

/*
 * Lookup an asnum in the aspa hash table.
 */
static struct rde_aspa_set *
aspa_lookup(struct rde_aspa *ra, uint32_t asnum)
{
	struct rde_aspa_set *aspa;
	uint32_t h;

	h = hash(asnum) & ra->mask;
	aspa = ra->table[h];
	if (aspa == NULL)
		return NULL;

	while (aspa->as < asnum) {
		if (!aspa->next)
			break;
		aspa++;
	}

	if (aspa->as == asnum)
		return aspa;
	return NULL;
}

/*
 * Lookup if there is a customer - provider relation between cas and pas.
 * Returns UNKNOWN if cas is not in the ra table or the aid is out of range.
 * Returns PROVIDER if pas is registered for cas for the specified aid.
 * Retruns NOT_PROVIDER otherwise.
 * The returned value includes the result for both IPv4 and IPv6 and needs
 * to be looked at with CP_GET.
 * This function is called very frequently and needs to be fast.
 */
static uint8_t
aspa_cp_lookup(struct rde_aspa *ra, uint32_t cas, uint32_t pas)
{
	struct rde_aspa_set *aspa;
	uint32_t i;

	aspa = aspa_lookup(ra, cas);
	if (aspa == NULL)
		return CP(UNKNOWN, UNKNOWN);

	if (aspa->num < 16) {
		for (i = 0; i < aspa->num; i++) {
			if (aspa->pas[i] == pas)
				break;
			if (aspa->pas[i] > pas)
				return CP(NOT_PROVIDER, NOT_PROVIDER);
		}
		if (i == aspa->num)
			return CP(NOT_PROVIDER, NOT_PROVIDER);
	} else {
		uint32_t lim, x;
		for (i = 0, lim = aspa->num; lim != 0; lim /= 2) {
			x = lim / 2;
			i += x;
			if (aspa->pas[i] == pas) {
				break;
			} else if (aspa->pas[i] < pas) {
				/* move right */
				i++;
				lim--;
			} else {
				/* move left */
				i -= x;
			}
		}
		if (lim == 0)
			return CP(NOT_PROVIDER, NOT_PROVIDER);
	}

	if (aspa->pas_aid == NULL)
		return CP(PROVIDER, PROVIDER);
	switch (aspa->pas_aid[i / 16] >> ((i % 16) * 2) & 0x3) {
	case 0x1:
		return CP(PROVIDER, NOT_PROVIDER);
	case 0x2:
		return CP(NOT_PROVIDER, PROVIDER);
	case 0x3:
		return CP(PROVIDER, PROVIDER);
	default:
		fatalx("impossible state in aspa_cp_lookup");
	}
}

/*
 * Calculate the various indexes of an aspath.
 * The up-ramp starts at the source-as which is the right-most / last element
 * in the aspath. The down-ramp starts at index 1.
 * nhops: number of unique hops in the path
 * nup_p: smallest index after which all aspath hops are provider valid
 * nup_u: The biggest index of an unknown aspath hop.
 * nup_np: The biggest index of a not-provider aspath hop.
 * ndown_p: biggest index before which all aspath hops are provider valid
 * ndown_u: The smallest index of an unknown aspath hop.
 * ndown_np: The smallest index of a not-provider aspath hop.
 * Returns 0 on success and -1 if a AS_SET is encountered.
 */
static int
aspa_check_aspath(struct rde_aspa *ra, struct aspath *a, struct aspa_state *s)
{
	uint8_t		*seg;
	int		 afi;
	uint32_t	 as, prevas = 0;
	uint16_t	 len, seg_size;
	uint8_t		 i, r, seg_type, seg_len;

	/* the neighbor-as itself is by definition valid */
	s[0].ndown_p = 1;
	s[1].ndown_p = 1;

	/*
	 * Walk aspath and validate if necessary both up- and down-ramp.
	 * If an AS_SET is found return -1 to indicate failure.
	 */
	seg = aspath_dump(a);
	len = aspath_length(a);
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		if (seg_type != AS_SEQUENCE)
			return -1;

		for (i = 0; i < seg_len; i++) {
			as = aspath_extract(seg, i);

			if (as == prevas)
				continue; /* skip prepends */

			s[0].nhops++;
			s[1].nhops++;
			if (prevas == 0) {
				prevas = as; /* skip left-most AS */
				continue;
			}

			/*
			 * down-ramp check, remember the
			 * left-most unknown or not-provider
			 * node and the right-most provider node
			 * for which all nodes before are valid.
			 */
			r = aspa_cp_lookup(ra, prevas, as);
			for (afi = 0; afi < 2; afi++) {
				switch (CP_GET(r, afi)) {
				case UNKNOWN:
					if (s[afi].ndown_u == 0)
						s[afi].ndown_u = s[afi].nhops;
					break;
				case PROVIDER:
					if (s[afi].ndown_p + 1 == s[afi].nhops)
						s[afi].ndown_p = s[afi].nhops;
					break;
				case NOT_PROVIDER:
					if (s[afi].ndown_np == 0)
						s[afi].ndown_np = s[afi].nhops;
					break;
				}
			}

			/*
			 * up-ramp check, remember the right-most
			 * unknown and not-provider node and the
			 * left-most provider node for which all nodes
			 * after are valid.
			 * We recorde the nhops value of prevas,
			 * that's why the use of nhops - 1.
			 */
			r = aspa_cp_lookup(ra, as, prevas);
			for (afi = 0; afi < 2; afi++) {
				switch (CP_GET(r, afi)) {
				case UNKNOWN:
					s[afi].nup_p = 0;
					s[afi].nup_u = s[afi].nhops - 1;
					break;
				case PROVIDER:
					if (s[afi].nup_p == 0)
						s[afi].nup_p = s[afi].nhops - 1;
					break;
				case NOT_PROVIDER:
					s[afi].nup_p = 0;
					s[afi].nup_np = s[afi].nhops - 1;
					break;
				}
			}
			prevas = as;
		}
	}

	/* the source-as itself is by definition valid */
	if (s[0].nup_p == 0)
		s[0].nup_p = s[0].nhops;
	if (s[1].nup_p == 0)
		s[1].nup_p = s[1].nhops;
	return 0;
}

/*
 * Set the two possible aspa outcomes for up-ramp only and up/down ramp
 * in the vstate array.
 */
static void
aspa_check_finalize(struct aspa_state *state, uint8_t *onlyup, uint8_t *downup)
{
	/*
	 * Just an up-ramp:
	 * if a check returned NOT_PROVIDER then the result is invalid.
	 * if a check returned UNKNOWN then the result is unknown.
	 * else path is valid.
	 */
	if (state->nup_np != 0)
		*onlyup = ASPA_INVALID;
	else if (state->nup_u != 0)
		*onlyup = ASPA_UNKNOWN;
	else
		*onlyup = ASPA_VALID;

	/*
	 * Both up-ramp and down-ramp:
	 * if nhops <= 2 the result is valid.
	 * if there is less than one AS hop between up-ramp and
	 *   down-ramp then the result is valid.
	 * if not-provider nodes for both ramps exist and they
	 *   do not overlap the path is invalid.
	 * else the path is unknown.
	 */
	if (state->nhops <= 2)
		*downup = ASPA_VALID;
	else if (state->nup_p - state->ndown_p <= 1)
		*downup = ASPA_VALID;
	else if (state->nup_np != 0 && state->ndown_np != 0 &&
	    state->nup_np - state->ndown_np >= 0)
		*downup = ASPA_INVALID;
	else
		*downup = ASPA_UNKNOWN;
}

/*
 * Validate an aspath against the aspa_set *ra.
 * Returns ASPA_VALID if the aspath is valid, ASPA_UNKNOWN if the
 * aspath contains hops with unknown relation and invalid for
 * empty aspaths, aspath with AS_SET and aspaths that fail validation.
 */
void
aspa_validation(struct rde_aspa *ra, struct aspath *a,
    struct rde_aspa_state *vstate)
{
	struct aspa_state state[2] = { 0 };

	/* no aspa table, evrything is unknown */
	if (ra == NULL) {
		memset(vstate, ASPA_UNKNOWN, 4);
		return;
	}

	/* empty ASPATHs are always invalid */
	if (aspath_length(a) == 0) {
		memset(vstate, ASPA_INVALID, 4);
		return;
	}

	if (aspa_check_aspath(ra, a, state) == -1) {
		memset(vstate, ASPA_INVALID, 4);
		return;
	}

	aspa_check_finalize(state, &vstate->onlyup_v4, &vstate->downup_v4);
	aspa_check_finalize(state + 1, &vstate->onlyup_v6, &vstate->downup_v6);
}

/*
 * Preallocate all data structures needed for the aspa table.
 * There are entries number of rde_aspa_sets with data_size bytes of
 * extra data (used to store SPAS and optional AFI bitmasks).
 */
struct rde_aspa *
aspa_table_prep(uint32_t entries, size_t datasize)
{
	struct rde_aspa *ra;
	uint32_t hsize = 1024;

	if (entries == 0)
		return NULL;
	if (entries > UINT32_MAX / 2)
		fatalx("aspa_table_prep overflow");

	while (hsize < entries)
		hsize *= 2;

	if ((ra = calloc(1, sizeof(*ra))) == NULL)
		fatal("aspa table prep");

	if ((ra->table = calloc(hsize, sizeof(ra->table[0]))) == NULL)
		fatal("aspa table prep");

	if ((ra->sets = calloc(entries, sizeof(ra->sets[0]))) == NULL)
		fatal("aspa table prep");

	if ((ra->data = malloc(datasize)) == NULL)
		fatal("aspa table prep");

	ra->mask = hsize - 1;
	ra->maxset = entries;
	ra->maxdata = datasize / sizeof(ra->data[0]);
	ra->lastchange = getmonotime();

	return ra;
}

/*
 * Insert an aspa customer/provider set into the hash table.
 * For hash conflict resulution insertion must happen in reverse order (biggest
 * customer asnum first). On conflict objects in the sets array are moved
 * around so that conflicting elements are direct neighbors.
 * The per AID information is (if required) stored as 2bits per provider.
 */
void
aspa_add_set(struct rde_aspa *ra, uint32_t cas, const uint32_t *pas,
    uint32_t pascnt, const uint32_t *pas_aid)
{
	struct rde_aspa_set *aspa;
	uint32_t h, i;

	if (ra->curset >= ra->maxset)
		fatalx("aspa set overflow");

	h = hash(cas) & ra->mask;
	aspa = ra->table[h];
	if (aspa == NULL) {
		aspa = &ra->sets[ra->curset++];
	} else {
		if (aspa->as <= cas)
			fatalx("%s: bad order of adds", __func__);

		/* insert before aspa */
		memmove(aspa + 1, aspa,
		    (ra->sets + ra->curset - aspa) * sizeof(*aspa));
		ra->curset++;
		memset(aspa, 0, sizeof(*aspa));
		aspa->next = 1;

		/* adjust hashtable after shift of elements */
		for (i = 0; i <= ra->mask; i++) {
			if (ra->table[i] > aspa)
				ra->table[i]++;
		}
	}
	aspa->as = cas;
	ra->table[h] = aspa;

	if (ra->maxdata - ra->curdata < pascnt)
		fatalx("aspa set data overflow");

	aspa->num = pascnt;
	aspa->pas = ra->data + ra->curdata;
	for (i = 0; i < pascnt; i++)
		ra->data[ra->curdata++] = pas[i];

	/* nobody in their right mind has per afi specific data */
	if (pas_aid != NULL) {
		/* 2 bits per entry rounded to next uint32_t */
		if (ra->maxdata - ra->curdata < (pascnt * 2 + 31) / 32)
			fatalx("aspa set data overflow");

		aspa->pas_aid = ra->data + ra->curdata;
		for (i = 0; i < (pascnt * 2 + 31) / 32; i++)
			ra->data[ra->curdata++] = pas_aid[i];
	}
}

void
aspa_table_free(struct rde_aspa *ra)
{
	if (ra == NULL)
		return;
	free(ra->table);
	free(ra->sets);
	free(ra->data);
	free(ra);
}

void
aspa_table_stats(const struct rde_aspa *ra, struct ctl_show_set *cset)
{
	if (ra == NULL)
		return;
	cset->lastchange = ra->lastchange;
	cset->as_cnt = ra->maxset;
}

/*
 * Return true if the two rde_aspa tables are contain the same data.
 */
int
aspa_table_equal(const struct rde_aspa *ra, const struct rde_aspa *rb)
{
	uint32_t i;

	/* allow NULL pointers to be passed */
	if (ra == NULL && rb == NULL)
		return 1;
	if (ra == NULL || rb == NULL)
		return 0;

	if (ra->maxset != rb->maxset ||
	    ra->maxdata != rb->maxdata)
		return 0;
	for (i = 0; i < ra->maxset; i++)
		if (ra->sets[i].as != rb->sets[i].as)
			return 0;
	if (memcmp(ra->data, rb->data, ra->maxdata * sizeof(ra->data[0])) != 0)
		return 0;

	return 1;
}

void
aspa_table_unchanged(struct rde_aspa *ra, const struct rde_aspa *old)
{
	if (ra == NULL || old == NULL)
		return;
	ra->lastchange = old->lastchange;
}
