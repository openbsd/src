/*	$OpenBSD: chash.c,v 1.7 2025/12/13 07:55:34 jsg Exp $	*/
/*
 * Copyright (c) 2025 Claudio Jeker <claudio@openbsd.org>
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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "chash.h"

/*
 * CH hash table: a cache optimized hash table
 *
 * The CH hash table is split into an extendible hashing part and fixed
 * size sub tables. The sub tables are split into CH_H2_SIZE groups holding
 * 7 elements each.
 * The sub tables are open addressed hash tables similar to swiss tables
 * but the groups are only using 7 fields and not 16. Also the meta data
 * is part of each group. On 64bit archs a group uses 64 bytes.
 *
 * The hash is split in three parts:
 * o H1: variable size hash used by the extendible hashing table
 * o H2: fixed size hash to select a group in a sub table.
 * o H3: 1 byte hash to select element in a group.
 *
 * Unlike other unordered sets this hash table does not allow a key to
 * be inserted more than once.
 */ 

#define CH_MAX_LOAD	875	/* in per-mille */

#define CH_H3_BITS	8
#define CH_H3_SIZE	(1 << CH_H3_BITS)
#define CH_H3_MASK	(CH_H3_SIZE - 1)

#define CH_H2_SHIFT	CH_H3_BITS
#define CH_H2_BITS	9
#define CH_H2_SIZE	(1 << CH_H2_BITS)
#define CH_H2_MASK	(CH_H2_SIZE - 1)

#define CH_H1_BITS	(64 - CH_H2_BITS - CH_H3_BITS)

#define CH_H1(h, l)		((l) == 0 ? 0 : h >> (64 - (l)))
#define CH_H2(h)		((h >> CH_H2_SHIFT) & CH_H2_MASK)
#define CH_H3(h)		(h & CH_H3_MASK)

struct ch_group {
	uint64_t	 cg_meta;
	/*
	 * cg_meta is actually split like this:
	 *	uint8_t	 cg_flags;
	 *	uint8_t  cg_hashes[7];
	 */
	void		*cg_data[7];
};
#define CH_EVER_FULL	0x80
#define CH_SLOT_MASK	0x7f
#define CH_FLAGS_SHIFT	56

struct ch_meta {
	uint32_t	cs_num_elm;
	uint32_t	cs_num_tomb;
	uint32_t	cs_num_ever_full;
	uint32_t	cs_local_level;
};

/*
 * API to work with the cg_meta field of a ch_group:
 *   cg_meta_set_flags: Set bits in flags section, return true if not yet set.
 *   cg_meta_clear_flags: Clear bits in flags section, return true if cleared.
 *   cg_meta_get_flags: Return flags section as uint8_t.
 *   cg_meta_check_flags: Return true if flag is set in flags section.
 *   cg_meta_set_hash: Set one of the 7 hash bytes according to slot and hash.
 *   ch_meta_locate: find possible canidates.
 */
static inline int
cg_meta_set_flags(struct ch_group *g, uint8_t flag)
{
	uint64_t f = flag, oldf;
	oldf = g->cg_meta & (f << CH_FLAGS_SHIFT);
	g->cg_meta |= f << CH_FLAGS_SHIFT;
	return oldf != 0;
}

static inline int
cg_meta_clear_flags(struct ch_group *g, uint8_t flag)
{
	uint64_t f = flag, oldf;
	oldf = g->cg_meta & (f << CH_FLAGS_SHIFT);
	g->cg_meta &= ~(f << CH_FLAGS_SHIFT);
	return oldf != 0;
}

static inline uint8_t
cg_meta_get_flags(const struct ch_group *g)
{
	return g->cg_meta >> CH_FLAGS_SHIFT;
}

static inline int
cg_meta_check_flags(const struct ch_group *g, uint8_t flag)
{
	uint64_t f = flag;
	return (g->cg_meta & (f << CH_FLAGS_SHIFT)) != 0;
}

static inline void
cg_meta_set_hash(struct ch_group *g, int slot, uint64_t hash)
{
	uint64_t newval;

	newval = g->cg_meta & ~(0xffULL << (slot * 8));
	newval |= hash << (slot * 8);
	g->cg_meta = newval;
}

/*
 * Find possible candidates in the group where both the mask matches with
 * the hash of the slot and where the flag bit for the slot is set.
 * Additionally set CH_EVER_FULL in the return value if it is set in
 * the group metadata.
 */
static uint8_t
ch_meta_locate(struct ch_group *g, uint64_t mask)
{
	uint64_t lookup;
	uint8_t flags, i, hits;

	lookup = g->cg_meta ^ mask;
	flags = cg_meta_get_flags(g);
	hits = flags & CH_EVER_FULL;

	for (i = 0; i < 7; i++) {
		if (((lookup >> i * 8) & CH_H3_MASK) == 0)
			hits |= flags & (1 << i);
	}
	return hits;
}

/*
 * API to work with fixed size sub tables.
 * Each sub table is an array of struct ch_group with CH_H2_SIZE elements.
 *
 * To find an element the group metadata is used to see if there is a
 * hit. If there was no hit but the CH_EVER_FULL flag is set the lookup
 * will continue and look at the next element until the needle is found
 * or there is a group where CH_EVER_FULL is unset.
 *
 * On insert if a group overflows CH_EVER_FULL is set. The flag will not be
 * cleared again.
 * On remove a data slot in the group is freed but depending on the
 * CH_EVER_FULL flag the slot is considered a tombstone (if set) or considered
 * free (not set).
 * 
 * Sub tables are automatically split in two when the maximum loadfactor is
 * reached. If the fill factor drops below a threashold then buddy tables
 * may be joined back together.
 */

/* Return load factor (including tombstones) of a sub table in per mille. */
static int
ch_sub_loadfactor(struct ch_meta *m)
{
	uint32_t max = CH_H2_SIZE * 7;
	uint32_t used = m->cs_num_elm + m->cs_num_tomb;

	return used * 1000 / max;
}

/* Return fill factor (only active elements) of a sub table in per mille. */
static int
ch_sub_fillfactor(struct ch_meta *m)
{
	uint32_t max = CH_H2_SIZE * 7;
	uint32_t used = m->cs_num_elm;

	return used * 1000 / max;
}

/*
 * Insert element into the hash table. If an equal element is already present
 * in the table return the pointer to that element else return NULL.
 */
static void *
ch_sub_insert(const struct ch_type *type, struct ch_group *table,
    struct ch_meta *meta, uint64_t h, void *elm)
{
	uint64_t mask;
	uint32_t bucket = CH_H2(h);
	int i;
	uint8_t empties, hits, ins_i;
	struct ch_group *g = &table[bucket], *ins_g = NULL;

	memset(&mask, CH_H3(h), sizeof(mask));
	while (1) {
		/* first check if object already present */
		hits = ch_meta_locate(g, mask);
		for (i = 0; i < 7; i++) {
			if (hits & (1 << i)) {
				if (type->t_equal(g->cg_data[i], elm))
					return g->cg_data[i];
			}
		}
		/* at the same time remember the first empty spot */
		empties = ~cg_meta_get_flags(g) & CH_SLOT_MASK;
		if (empties == 0) {
			/* overflow, mark group as full */
			if (cg_meta_set_flags(g, CH_EVER_FULL) == 0)
				meta->cs_num_ever_full++;
		} else {
			if (ins_g == NULL) {
				ins_g = g;
				ins_i = ffs(empties) - 1;
			}
			/* check if lookup is finished and obj is not present */
			if (cg_meta_check_flags(g, CH_EVER_FULL) == 0)
				break;
		}
		g = &table[++bucket & CH_H2_MASK];
	}

	/*
	 * insert new object and adjust accounting.
	 * If CH_EVER_FULL is set then it was a tombstone and not an empty slot.
	 */
	ins_g->cg_data[ins_i] = elm;
	cg_meta_set_hash(ins_g, ins_i, h & CH_H3_MASK);
	cg_meta_set_flags(ins_g, 1 << ins_i);
	if (cg_meta_check_flags(ins_g, CH_EVER_FULL))
		meta->cs_num_tomb--;
	meta->cs_num_elm++;

	return NULL;
}

/*
 * Look up needle in the sub table and if found, remove the entry and
 * return its pointer else return NULL.
 */
static void *
ch_sub_remove(const struct ch_type *type, struct ch_group *table,
    struct ch_meta *meta, uint64_t h, const void *needle)
{
	uint64_t mask;
	uint32_t bucket = CH_H2(h);
	int i;
	uint8_t hits;
	struct ch_group *g = &table[bucket];

	memset(&mask, CH_H3(h), sizeof(mask));
	while (1) {
		hits = ch_meta_locate(g, mask);
		for (i = 0; i < 7; i++) {
			if (hits & (1 << i)) {
				/* most porbably a hit */
				if (type->t_equal(g->cg_data[i], needle)) {
					void *elm = g->cg_data[i];
					g->cg_data[i] = NULL;
					cg_meta_set_hash(g, i, 0);
					cg_meta_clear_flags(g, 1 << i);
					if (hits & CH_EVER_FULL)
						meta->cs_num_tomb++;
					meta->cs_num_elm--;
					return elm;
				}
			}
		}
		if ((hits & CH_EVER_FULL) == 0)
			return NULL;
		g = &table[++bucket & CH_H2_MASK];
	}
}

/*
 * Look up needle in the sub table and if found return its pointer
 * else return NULL.
 */
static void *
ch_sub_find(const struct ch_type *type, struct ch_group *table, uint64_t h,
    const void *needle)
{
	uint64_t mask;
	uint32_t bucket = CH_H2(h);
	int i;
	uint8_t hits;
	struct ch_group *g = &table[bucket];

	memset(&mask, CH_H3(h), sizeof(mask));
	while (1) {
		hits = ch_meta_locate(g, mask);
		for (i = 0; i < 7; i++) {
			if (hits & (1 << i)) {
				/* most porbably a hit */
				if (type->t_equal(g->cg_data[i], needle))
					return g->cg_data[i];
			}
		}
		if ((hits & CH_EVER_FULL) == 0)
			return NULL;
		g = &table[++bucket & CH_H2_MASK];
	}
}

/*
 * Look up hash in the sub table and if possible match is found pass it to
 * cmp callback to verify.
 */
static void *
ch_sub_locate(const struct ch_type *type, struct ch_group *table, uint64_t h,
    int (*cmp)(const void *, void *), void *arg)
{
	uint64_t mask;
	uint32_t bucket = CH_H2(h);
	int i;
	uint8_t hits;
	struct ch_group *g = &table[bucket];

	memset(&mask, CH_H3(h), sizeof(mask));
	while (1) {
		hits = ch_meta_locate(g, mask);
		for (i = 0; i < 7; i++) {
			if (hits & (1 << i)) {
				/* most porbably a hit */
				if (cmp(g->cg_data[i], arg))
					return g->cg_data[i];
			}
		}
		if ((hits & CH_EVER_FULL) == 0)
			return NULL;
		g = &table[++bucket & CH_H2_MASK];
	}
}

/*
 * Start of sub table iterator, reset the set and grp indices and locate
 * first element in sub table. Return element or NULL if table is empty.
 */
static void *
ch_sub_first(const struct ch_type *type, struct ch_group *table,
    struct ch_iter *iter)
{
	struct ch_group *g;
	uint32_t n;
	uint8_t elms;

	for (n = 0; n < CH_H2_SIZE; n++, g++) {
		g = &table[n];
		elms = cg_meta_get_flags(g) & CH_SLOT_MASK;
		if (elms == 0)
			continue;
		iter->ci_set_idx = n;
		iter->ci_grp_idx = ffs(elms);
		return g->cg_data[iter->ci_grp_idx - 1];
	}
	iter->ci_set_idx = n;
	return NULL;
}

/*
 * Get next element of a sub table based on the iterator indices.
 * Return element or NULL if the rest of the table is empty.
 */
static void *
ch_sub_next(const struct ch_type *type, struct ch_group *table,
    struct ch_iter *iter)
{
	struct ch_group *g;
	uint32_t n;
	uint8_t elms;

	for (n = iter->ci_set_idx; n < CH_H2_SIZE; n++, g++) {
		g = &table[n];
		elms = cg_meta_get_flags(g) & CH_SLOT_MASK;
		elms &= CH_SLOT_MASK << iter->ci_grp_idx;
		if (elms == 0) {
			iter->ci_grp_idx = 0;
			continue;
		}
		iter->ci_set_idx = n;
		iter->ci_grp_idx = ffs(elms);
		return g->cg_data[iter->ci_grp_idx - 1];
	}
	iter->ci_set_idx = n;
	return NULL;
}

/*
 * Split table from into two new tables low and high. Update the meta data
 * accordingly. The cs_local_level of low and high will be 1 higher then that
 * of the from table.
 */
static int
ch_sub_split(const struct ch_type *type, struct ch_group *from,
    struct ch_group *low, struct ch_group *high, struct ch_meta *frommeta,
    struct ch_meta *lowmeta, struct ch_meta *highmeta)
{
	struct ch_group *g = &from[0];
	uint32_t n;
	uint8_t elms, i;

	lowmeta->cs_local_level = frommeta->cs_local_level + 1;
	highmeta->cs_local_level = frommeta->cs_local_level + 1;

	for (n = 0; n < CH_H2_SIZE; n++, g++) {
		elms = cg_meta_get_flags(g) & CH_SLOT_MASK;
		if (elms == 0)
			continue;
		for (i = 0; i < 7; i++) {
			if (elms & (1 << i)) {
				void *v;
				uint64_t h = type->t_hash(g->cg_data[i]);
				if (CH_H1(h, lowmeta->cs_local_level) & 1)
					v = ch_sub_insert(type, high, highmeta,
					    h, g->cg_data[i]);
				else
					v = ch_sub_insert(type, low, lowmeta,
					    h, g->cg_data[i]);
				if (v != NULL) {
					errno = EINVAL;
					return -1;
				}
			}
		}
	}

	return 0;
}

/*
 * Merge all active elements of one sub group into the table table.
 * Return 0 on success, -1 on failure.
 */
static int
ch_sub_merge_one(const struct ch_type *type, struct ch_group *table,
    struct ch_meta *meta, const struct ch_group *from)
{
	uint8_t elms, i;

	elms = cg_meta_get_flags(from) & CH_SLOT_MASK;
	if (elms == 0)
		return 0;
	for (i = 0; i < 7; i++) {
		if (elms & (1 << i)) {
			void *v;
			uint64_t h = type->t_hash(from->cg_data[i]);
			v = ch_sub_insert(type, table, meta, h,
			    from->cg_data[i]);
			if (v != NULL)		/* how should there be a dup? */
				return -1;
		}
	}
	return 0;
}

/*
 * Merge sub tables from and buddy into the new table to.
 * Returns 0 on success and -1 on failure keeping from and buddy as is.
 */
static int
ch_sub_merge(const struct ch_type *type, struct ch_group *to,
    struct ch_group *from, struct ch_group *buddy, struct ch_meta *tometa,
    struct ch_meta *frommeta, struct ch_meta *buddymeta)
{
	struct ch_group *g = &from[0];
	struct ch_group *b = &buddy[0];
	uint32_t n;

	tometa->cs_local_level = frommeta->cs_local_level - 1;

	for (n = 0; n < CH_H2_SIZE; n++, g++, b++) {
		if (ch_sub_merge_one(type, to, tometa, g) == -1)
			return -1;
		if (ch_sub_merge_one(type, to, tometa, b) == -1)
			return -1;
	}
	return 0;
}

static int
ch_sub_alloc(struct ch_group **table, struct ch_meta **meta)
{
	if ((*table = calloc(CH_H2_SIZE, sizeof(**table))) == NULL)
		return -1;
	if ((*meta = calloc(1, sizeof(**meta))) == NULL) {
		free(*table);
		*table = NULL;
		return -1;
	}
	return 0;
}

static void
ch_sub_free(struct ch_group *table, struct ch_meta *meta)
{
	free(table);
	free(meta);
}

/*
 * Resize extendible hash table by 2. Updating all pointers accordingly.
 * Return 0 on success, -1 on failure and set errno.
 */
static int
ch_table_resize(struct ch_table *t)
{
	struct ch_group **tables;
	struct ch_meta **metas;
	uint64_t oldsize = 1ULL << t->ch_level;
	uint64_t newsize = 1ULL << (t->ch_level + 1);
	int64_t idx;

	if (t->ch_level + 1 >= CH_H1_BITS) {
		errno = E2BIG;
		return -1;
	}

	if (t->ch_tables == NULL) {
		oldsize = 0;
		newsize = 1;
	}

	tables = reallocarray(t->ch_tables, newsize, sizeof(*tables));
	if (tables == NULL)
		return -1;
	metas = reallocarray(t->ch_metas, newsize, sizeof(*metas));
	if (metas == NULL) {
		free(tables);
		return -1;
	}

	for (idx = oldsize - 1; idx >= 0; idx--) {
		tables[idx * 2] = tables[idx];
		tables[idx * 2 + 1] = tables[idx];
		metas[idx * 2] = metas[idx];
		metas[idx * 2 + 1] = metas[idx];
	}

	if (t->ch_tables != NULL)
		t->ch_level++;
	t->ch_tables = tables;
	t->ch_metas = metas;
	return 0;
}

/*
 * Set table pointers from idx to the required number of elements
 * depending on the table ch_level and the sub table cs_local_level.
 * Idx only includes the bits covered by the sub table cs_local_level.
 */
static void
ch_table_fill(struct ch_table *t, uint64_t idx, struct ch_group *table,
    struct ch_meta *meta)
{
	uint64_t cnt, i;

	idx <<= (t->ch_level - meta->cs_local_level);
	cnt = 1ULL << (t->ch_level - meta->cs_local_level);

	for (i = 0; i < cnt; i++) {
		t->ch_tables[idx + i] = table;
		t->ch_metas[idx + i] = meta;
	}
}

/*
 * Return the buddy sub group for the table with idx and local_level.
 * The buddy page must have the same local level to be a buddy.
 */
static struct ch_group *
ch_table_buddy(struct ch_table *t, uint64_t idx, uint32_t local_level,
    struct ch_meta **meta)
{
	struct ch_meta *m;

	/* the single root table can't be merged */
	if (local_level == 0)
		return NULL;

	idx ^= 1ULL << (t->ch_level - local_level);

	m = t->ch_metas[idx];
	/* can only merge buddies at same level */
	if (m->cs_local_level == local_level) {
		*meta = m;
		return t->ch_tables[idx];
	}
	return NULL;
}

/*
 * Grow the hash table by spliting a sub group and if needed by doubling
 * the extendible hash of the primary table.
 * Return 0 on success, -1 on failure and set errno.
 */
static int
ch_table_grow(const struct ch_type *type, struct ch_table *t, uint64_t h,
    struct ch_group *table, struct ch_meta *meta)
{
	struct ch_group *left = NULL, *right = NULL;
	struct ch_meta *leftmeta = NULL, *rightmeta = NULL;
	uint64_t idx;

	/* check if the extendible hashing table needs to grow */
	if (meta->cs_local_level == t->ch_level) {
		if (ch_table_resize(t) == -1)
			goto fail;
	}

	/* allocate new sub tables */
	if (ch_sub_alloc(&left, &leftmeta) == -1)
		goto fail;
	if (ch_sub_alloc(&right, &rightmeta) == -1)
		goto fail;

	/* split up the old table into the two new ones */
	if (ch_sub_split(type, table, left, right,
	    meta, leftmeta, rightmeta) == -1)
		goto fail;

	/*
	 * Insert new tables into the extendible hash table.
	 * Calculate index based on the H1 hash of the old sub table
	 * but shift it by one bit since the new tables use one more bit.
	 */
	idx = CH_H1(h, meta->cs_local_level) << 1;
	ch_table_fill(t, idx, left, leftmeta);
	ch_table_fill(t, idx | 1, right, rightmeta);
	ch_sub_free(table, meta);

	return 0;

 fail:
	ch_sub_free(right, rightmeta);
	ch_sub_free(left, leftmeta);
	return -1;
}

/*
 * Try to compact two sub groups back together and adjusting the extendible
 * hash. Compaction only happens if there is a buddy page (page with same
 * local level on the alternate slot) and the result of the merge is below
 * 2 / 3 of the maximum load factor.
 * Return 0 on success, -1 if no compaction is possible.
 */
static int
ch_table_compact(const struct ch_type *type, struct ch_table *t, uint64_t h,
    struct ch_group *table, struct ch_meta *meta)
{
	struct ch_group *buddy, *to = NULL;
	struct ch_meta *buddymeta, *tometa = NULL;
	uint64_t idx;

	idx = CH_H1(h, t->ch_level);
	buddy = ch_table_buddy(t, idx, meta->cs_local_level, &buddymeta);
	if (buddy == NULL || ch_sub_fillfactor(meta) +
	    ch_sub_fillfactor(buddymeta) > CH_MAX_LOAD * 2 / 3)
		return -1;

	/* allocate new sub table */
	if (ch_sub_alloc(&to, &tometa) == -1)
		goto fail;

	/* merge the table and buddy into to. */
	if (ch_sub_merge(type, to, table, buddy, tometa, meta, buddymeta) ==
	    -1)
		goto fail;

	/*
	 * Update table in the extendible hash table, which overwrites
	 * all entries of the buddy.
	 */
	idx = CH_H1(h, tometa->cs_local_level);
	ch_table_fill(t, idx, to, tometa);
	ch_sub_free(buddy, buddymeta);
	ch_sub_free(table, meta);

	return 0;

 fail:
	ch_sub_free(to, tometa);
	return -1;
}

/*
 * Public API used by the macros defined in chash.h.
 */
int
_ch_init(const struct ch_type *type, struct ch_table *t)
{
	struct ch_group *table = NULL;
	struct ch_meta *meta = NULL;

	t->ch_level = 0;
	t->ch_num_elm = 0;
	if (ch_sub_alloc(&table, &meta) == -1)
		goto fail;

	if (ch_table_resize(t) == -1)
		goto fail;

	ch_table_fill(t, 0, table, meta);

	return 0;

 fail:
	ch_sub_free(table, meta);
	return -1;
}

void
_ch_destroy(const struct ch_type *type, struct ch_table *t)
{
	uint64_t idx, max = 1ULL << t->ch_level;
	struct ch_group *table = NULL;

	for (idx = 0; idx < max; idx++) {
		if (table == t->ch_tables[idx])
			continue;

		table = t->ch_tables[idx];
		ch_sub_free(t->ch_tables[idx], t->ch_metas[idx]);
	}
	free(t->ch_tables);
	free(t->ch_metas);
	memset(t, 0, sizeof(*t));
}

void *
_ch_insert(const struct ch_type *type, struct ch_table *t, uint64_t h,
    void *elm)
{
	struct ch_group *table;
	struct ch_meta *meta;
	void *v;
	uint64_t idx;

	if (t->ch_tables == NULL)
		if (_ch_init(type, t) == -1)
			return CH_INS_FAILED;

	idx = CH_H1(h, t->ch_level);
	table = t->ch_tables[idx];
	meta = t->ch_metas[idx];

	if (ch_sub_loadfactor(meta) >= CH_MAX_LOAD) {
		if (ch_table_grow(type, t, h, table, meta) == -1)
			return CH_INS_FAILED;
		/* refetch data after resize */
		idx = CH_H1(h, t->ch_level);
		table = t->ch_tables[idx];
		meta = t->ch_metas[idx];
	}

	v = ch_sub_insert(type, table, meta, h, elm);
	if (v == NULL)
		t->ch_num_elm++;
	return v;
}

void *
_ch_remove(const struct ch_type *type, struct ch_table *t, uint64_t h,
    const void *needle)
{
	struct ch_group *table;
	struct ch_meta *meta;
	void *v;
	uint64_t idx;

	if (t->ch_tables == NULL)
		return NULL;

	idx = CH_H1(h, t->ch_level);
	table = t->ch_tables[idx];
	meta = t->ch_metas[idx];

	v = ch_sub_remove(type, table, meta, h, needle);
	if (v != NULL) {
		t->ch_num_elm--;

		while (ch_sub_fillfactor(meta) <= CH_MAX_LOAD / 4) {
			if (ch_table_compact(type, t, h, table, meta) == -1)
				break;

			/* refetch data after compaction */
			table = t->ch_tables[idx];
			meta = t->ch_metas[idx];
		}
	}
	return v;
}

void *
_ch_find(const struct ch_type *type, struct ch_table *t, uint64_t h,
    const void *needle)
{
	struct ch_group *table;
	uint64_t idx;

	if (t->ch_tables == NULL)
		return NULL;

	idx = CH_H1(h, t->ch_level);
	table = t->ch_tables[idx];

	return ch_sub_find(type, table, h, needle);
}

void *
_ch_locate(const struct ch_type *type, struct ch_table *t, uint64_t h,
    int (*cmp)(const void *, void *), void *arg)
{
	struct ch_group *table;
	uint64_t idx;

	if (t->ch_tables == NULL)
		return NULL;

	idx = CH_H1(h, t->ch_level);
	table = t->ch_tables[idx];

	return ch_sub_locate(type, table, h, cmp, arg);
}

void *
_ch_first(const struct ch_type *type, struct ch_table *t, struct ch_iter *it)
{
	struct ch_group *table;
	uint64_t idx;

	if (t->ch_tables == NULL)
		return NULL;

	idx = it->ci_ext_idx = 0;
	table = t->ch_tables[idx];

	return ch_sub_first(type, table, it);
}

void *
_ch_next(const struct ch_type *type, struct ch_table *t, struct ch_iter *it)
{
	struct ch_group *table;
	uint64_t idx, max;
	void *v;

	if (t->ch_tables == NULL)
		return NULL;

	max = 1ULL << t->ch_level;
	idx = it->ci_ext_idx;
	if (idx >= max)
		return NULL;

	table = t->ch_tables[idx];
	v = ch_sub_next(type, table, it);
	if (v != NULL)
		return v;

	/* find next sub table */
	for (idx++; idx < max; idx++) {
		if (table != t->ch_tables[idx])
			break;
	}
	if (idx >= max)
		return NULL;
	/* start next sub table */
	it->ci_ext_idx = idx;
	table = t->ch_tables[idx];
	return ch_sub_first(type, table, it);
}

/*
 * Implementation of a fast non-cryptographic hash function for small inputs
 * Based on HashLen0to16() from CityHash64.
 */

/* A prime between 2^63 and 2^64 */
static const uint64_t prime = 0x9ae16a3b2f90404fULL;

static inline uint64_t
ch_rotate(uint64_t val, int shift)
{
	return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

static inline uint64_t
ch_mix(uint64_t u, uint64_t v, uint64_t mul)
{
	uint64_t a, b;

	a = (u ^ v) * mul;
	a ^= (a >> 47);
	b = (v ^ a) * mul;
	b ^= (b >> 47);
	return b * mul;
}

uint64_t
ch_qhash64(uint64_t hash, uint64_t value)
{
	uint64_t mul = prime + 2 * sizeof(value);
	uint64_t a = value + prime;
	uint64_t b = hash;
	uint64_t c, d;

	c = ch_rotate(b, 37) * mul + a;
	d = (ch_rotate(a, 25) + b) * mul;
	return ch_mix(c, d, mul);
}
