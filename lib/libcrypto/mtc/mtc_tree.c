/*	$OpenBSD$ */
/*
 * Copyright (c) 2025, Google Inc.
 * Copyright (c) 2026, Bob Beck <beck@obtuse.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Merkle tree subtree operations of section 4 of
 * draft-ietf-plants-merkle-tree-certs-05, built on the RFC 9162 Merkle tree.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "bytestring.h"
#include "mtc_internal.h"

/* The largest power of two strictly smaller than n, for n >= 2. */
static uint64_t
pow2_smaller(uint64_t n)
{
	assert(n >= 2);

	n -= 1;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;

	return (n >> 1) + 1;
}

int
mtc_subtree_is_valid(struct mtc_subtree subtree)
{
	uint64_t n, k;

	if (subtree.start >= subtree.end)
		return 0;

	/*
	 * k is the largest power of two dividing start; the size may not
	 * exceed it.
	 */
	n = subtree.end - subtree.start;
	k = subtree.start & (~subtree.start + 1);

	return subtree.start == 0 || n <= k;
}

uint64_t
mtc_subtree_leaf_count(struct mtc_subtree subtree)
{
	return subtree.end - subtree.start;
}

uint64_t
mtc_subtree_split(struct mtc_subtree subtree)
{
	uint64_t n;

	n = mtc_subtree_leaf_count(subtree);
	if (n < 2)
		return subtree.end;

	return subtree.start + pow2_smaller(n);
}

struct mtc_subtree
mtc_subtree_left(struct mtc_subtree subtree)
{
	struct mtc_subtree left;

	left.start = subtree.start;
	left.end = mtc_subtree_split(subtree);

	return left;
}

struct mtc_subtree
mtc_subtree_right(struct mtc_subtree subtree)
{
	struct mtc_subtree right;

	right.start = mtc_subtree_split(subtree);
	right.end = subtree.end;

	return right;
}

int
mtc_subtree_contains_index(struct mtc_subtree subtree, uint64_t index)
{
	return subtree.start <= index && index < subtree.end;
}

int
mtc_subtree_contains_subtree(struct mtc_subtree outer,
    struct mtc_subtree inner)
{
	return outer.start <= inner.start && inner.end <= outer.end;
}

/* The number of bits needed to represent n; 0 for n == 0. */
static int
u64_bit_length(uint64_t n)
{
	int len = 0;

	while (n != 0) {
		n >>= 1;
		len++;
	}

	return len;
}

size_t
mtc_find_subtrees(struct mtc_subtree interval, struct mtc_subtree out[2])
{
	uint64_t start, end, last, mask, mid, left_start;
	int split, left_split;

	start = interval.start;
	end = interval.end;

	assert(start < end);

	if (end - start == 1) {
		out[0] = interval;
		return 1;
	}

	/*
	 * split is the height at which the paths from the root to start and
	 * to the last leaf diverge.  mid is the leftmost leaf under the
	 * right branch at that height; the two subtrees meet there.
	 */
	last = end - 1;
	split = u64_bit_length(start ^ last) - 1;
	mask = (UINT64_C(1) << split) - 1;
	mid = last & ~mask;

	/*
	 * The left subtree is the largest full subtree ending at mid that
	 * begins at or before start: its height is that of the most
	 * significant zero bit of start below split.
	 */
	left_split = u64_bit_length(~start & mask);
	left_start = start & ~((UINT64_C(1) << left_split) - 1);

	out[0].start = left_start;
	out[0].end = mid;
	out[1].start = mid;
	out[1].end = end;

	return 2;
}

/* The node hash length of md, or 0 if md is unusable. */
static size_t
mtc_hash_len(const EVP_MD *md)
{
	int len;

	if ((len = EVP_MD_size(md)) <= 0 || len > EVP_MAX_MD_SIZE)
		return 0;

	return len;
}

/*
 * HASH(header || a || b).  out may alias a or b.  A zero-length operand is
 * skipped.
 */
static int
mtc_hash(EVP_MD_CTX *ctx, const EVP_MD *md, uint8_t header,
    const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len,
    uint8_t out[EVP_MAX_MD_SIZE])
{
	if (!EVP_DigestInit_ex(ctx, md, NULL))
		return 0;
	if (!EVP_DigestUpdate(ctx, &header, 1))
		return 0;
	if (a_len > 0 && !EVP_DigestUpdate(ctx, a, a_len))
		return 0;
	if (b_len > 0 && !EVP_DigestUpdate(ctx, b, b_len))
		return 0;
	if (!EVP_DigestFinal_ex(ctx, out, NULL))
		return 0;

	return 1;
}

int
mtc_hash_leaf(const EVP_MD *md, const uint8_t *entry, size_t entry_len,
    uint8_t out[EVP_MAX_MD_SIZE])
{
	EVP_MD_CTX *ctx;
	int ret = 0;

	if ((ctx = EVP_MD_CTX_new()) == NULL)
		goto err;
	if (!mtc_hash(ctx, md, 0x00, entry, entry_len, NULL, 0, out))
		goto err;

	ret = 1;

 err:
	EVP_MD_CTX_free(ctx);

	return ret;
}

int
mtc_hash_node(const EVP_MD *md, const uint8_t *left, const uint8_t *right,
    uint8_t out[EVP_MAX_MD_SIZE])
{
	EVP_MD_CTX *ctx = NULL;
	size_t hash_len;
	int ret = 0;

	if ((hash_len = mtc_hash_len(md)) == 0)
		goto err;
	if ((ctx = EVP_MD_CTX_new()) == NULL)
		goto err;
	if (!mtc_hash(ctx, md, 0x01, left, hash_len, right, hash_len, out))
		goto err;

	ret = 1;

 err:
	EVP_MD_CTX_free(ctx);

	return ret;
}

/*
 * Section 4.4.3.  fn, sn and tn are the draft's "first", "second" and
 * "third" numbers: the paths from the root to the subtree's first
 * leaf, to its last leaf, and to the tree's last leaf.  Shifting each
 * right by one moves the three cursors up one level.  node and
 * out_root_hash are the draft's fr and sr. The final comparison of
 * out_root_hash to a known trusted hash (the last step of the proof)
 * is expected to be done by the caller.
 */
static int
eval_consistency_proof(EVP_MD_CTX *ctx, const EVP_MD *md, size_t hash_len,
    uint64_t n, struct mtc_subtree subtree, CBS *proof,
    const uint8_t *node_hash, uint8_t out_root_hash[EVP_MAX_MD_SIZE])
{
	uint64_t fn, sn, tn;
	uint8_t node[EVP_MAX_MD_SIZE];
	uint8_t root[EVP_MAX_MD_SIZE];
	CBS hash;

	if (!mtc_subtree_is_valid(subtree) || n < subtree.end)
		return 0;

	fn = subtree.start;
	sn = subtree.end - 1;
	tn = n - 1;

	if (sn == tn) {
		/* The subtree ends the tree: rise to its root. */
		while (fn != sn) {
			fn >>= 1;
			sn >>= 1;
			tn >>= 1;
		}
	} else {
		/*
		 * Rise while the last leaf is a right child, to the largest
		 * full subtree sharing its right edge.
		 */
		while (fn != sn && (sn & 1) == 1) {
			fn >>= 1;
			sn >>= 1;
			tn >>= 1;
		}
	}

	/*
	 * A subtree whose hash is a node of the tree is omitted from the
	 * proof and node_hash seeds both cursors; otherwise the first proof
	 * element does.
	 */
	if (fn == sn) {
		memcpy(node, node_hash, hash_len);
		memcpy(root, node_hash, hash_len);
	} else {
		if (!CBS_get_bytes(proof, &hash, hash_len))
			return 0;
		memcpy(node, CBS_data(&hash), hash_len);
		memcpy(root, CBS_data(&hash), hash_len);
	}

	while (CBS_len(proof) > 0) {
		if (!CBS_get_bytes(proof, &hash, hash_len))
			return 0;
		if (tn == 0)
			return 0;

		if ((sn & 1) == 1 || sn == tn) {
			/* node stops updating at the subtree root. */
			if (fn < sn && !mtc_hash(ctx, md, 0x01, CBS_data(&hash),
			    hash_len, node, hash_len, node))
				return 0;
			if (!mtc_hash(ctx, md, 0x01, CBS_data(&hash), hash_len,
			    root, hash_len, root))
				return 0;
			while ((sn & 1) == 0) {
				fn >>= 1;
				sn >>= 1;
				tn >>= 1;
			}
		} else {
			if (!mtc_hash(ctx, md, 0x01, root, hash_len,
			    CBS_data(&hash), hash_len, root))
				return 0;
		}
		fn >>= 1;
		sn >>= 1;
		tn >>= 1;
	}

	if (tn != 0)
		return 0;
	if (memcmp(node, node_hash, hash_len) != 0)
		return 0;

	memcpy(out_root_hash, root, hash_len);

	return 1;
}

int
mtc_eval_subtree_consistency_proof(const EVP_MD *md, uint64_t n,
    struct mtc_subtree subtree, const uint8_t *proof, size_t proof_len,
    const uint8_t *node_hash, uint8_t out_root_hash[EVP_MAX_MD_SIZE])
{
	EVP_MD_CTX *ctx = NULL;
	size_t hash_len;
	CBS cbs;
	int ret = 0;

	if ((hash_len = mtc_hash_len(md)) == 0)
		goto err;
	if ((ctx = EVP_MD_CTX_new()) == NULL)
		goto err;

	CBS_init(&cbs, proof, proof_len);
	if (!eval_consistency_proof(ctx, md, hash_len, n, subtree, &cbs,
	    node_hash, out_root_hash))
		goto err;

	ret = 1;

 err:
	EVP_MD_CTX_free(ctx);

	return ret;
}

int
mtc_eval_subtree_inclusion_proof(const EVP_MD *md, const uint8_t *proof,
    size_t proof_len, uint64_t index, const uint8_t *entry_hash,
    struct mtc_subtree subtree, uint8_t out_root_hash[EVP_MAX_MD_SIZE])
{
	struct mtc_subtree leaf;

	if (!mtc_subtree_is_valid(subtree))
		return 0;
	if (!mtc_subtree_contains_index(subtree, index))
		return 0;

	/*
	 * An inclusion proof is the consistency proof of the single-leaf
	 * subtree, in coordinates relative to subtree.
	 */
	leaf.start = index - subtree.start;
	leaf.end = leaf.start + 1;

	return mtc_eval_subtree_consistency_proof(md,
	    mtc_subtree_leaf_count(subtree), leaf, proof, proof_len,
	    entry_hash, out_root_hash);
}

/*
 * An in-memory tree stores every node.  levels[l] holds the hashes of the
 * full subtrees of 2^l leaves, so levels[0] is the leaves.  Hashes of
 * partial subtrees on the right edge are computed on demand.
 */
struct mtc_level {
	uint8_t *nodes;
	size_t count;
	size_t cap;
};

struct mtc_tree {
	const EVP_MD *md;
	EVP_MD_CTX *ctx;
	size_t hash_len;
	struct mtc_level *levels;
	size_t num_levels;
	size_t cap;
};

static int
level_push(struct mtc_level *level, const uint8_t *hash, size_t hash_len)
{
	uint8_t *nodes;
	size_t cap;

	if (level->count == level->cap) {
		cap = level->cap == 0 ? 16 : level->cap * 2;
		if ((nodes = reallocarray(level->nodes, cap, hash_len)) == NULL)
			return 0;
		level->nodes = nodes;
		level->cap = cap;
	}
	memcpy(level->nodes + level->count * hash_len, hash, hash_len);
	level->count++;

	return 1;
}

static const uint8_t *
tree_node(const struct mtc_tree *tree, size_t level, uint64_t index)
{
	assert(level < tree->num_levels);
	assert(index < tree->levels[level].count);

	return tree->levels[level].nodes + index * tree->hash_len;
}

/* Keep the first n leaves and the interior nodes they support. */
static void
tree_truncate(struct mtc_tree *tree, uint64_t n)
{
	size_t level;

	tree->levels[0].count = n;
	for (level = 1; level < tree->num_levels; level++) {
		n /= 2;
		if (tree->levels[level].count > n)
			tree->levels[level].count = n;
	}
}

static int
tree_add_level(struct mtc_tree *tree)
{
	struct mtc_level *levels;
	size_t cap;

	if (tree->num_levels == tree->cap) {
		cap = tree->cap == 0 ? 8 : tree->cap * 2;
		if ((levels = reallocarray(tree->levels, cap,
		    sizeof(*levels))) == NULL)
			return 0;
		tree->levels = levels;
		tree->cap = cap;
	}
	memset(&tree->levels[tree->num_levels], 0, sizeof(*tree->levels));
	tree->num_levels++;

	return 1;
}

/* Complete the interior levels for the leaves present. */
static int
tree_update_levels(struct mtc_tree *tree)
{
	struct mtc_level *level;
	uint8_t hash[EVP_MAX_MD_SIZE];
	size_t l, i;
	uint64_t n;

	n = mtc_tree_leaf_count(tree) / 2;
	for (l = 1; n != 0; l++, n /= 2) {
		if (l == tree->num_levels && !tree_add_level(tree))
			return 0;
		level = &tree->levels[l];
		while (level->count < n) {
			i = level->count;
			if (!mtc_hash(tree->ctx, tree->md, 0x01,
			    tree_node(tree, l - 1, 2 * i), tree->hash_len,
			    tree_node(tree, l - 1, 2 * i + 1), tree->hash_len,
			    hash))
				return 0;
			if (!level_push(level, hash, tree->hash_len))
				return 0;
		}
	}

	return 1;
}

struct mtc_tree *
mtc_tree_new(const EVP_MD *md)
{
	struct mtc_tree *tree = NULL;
	size_t hash_len;

	if ((hash_len = mtc_hash_len(md)) == 0)
		goto err;
	if ((tree = calloc(1, sizeof(*tree))) == NULL)
		goto err;
	tree->md = md;
	tree->hash_len = hash_len;
	if ((tree->ctx = EVP_MD_CTX_new()) == NULL)
		goto err;
	if (!tree_add_level(tree))
		goto err;

	return tree;

 err:
	mtc_tree_free(tree);

	return NULL;
}

void
mtc_tree_free(struct mtc_tree *tree)
{
	size_t i;

	if (tree == NULL)
		return;

	for (i = 0; i < tree->num_levels; i++)
		free(tree->levels[i].nodes);
	free(tree->levels);
	EVP_MD_CTX_free(tree->ctx);
	free(tree);
}

uint64_t
mtc_tree_leaf_count(const struct mtc_tree *tree)
{
	return tree->levels[0].count;
}

int
mtc_tree_append(struct mtc_tree *tree, const uint8_t *entry, size_t entry_len)
{
	uint8_t hash[EVP_MAX_MD_SIZE];
	uint64_t n;

	n = mtc_tree_leaf_count(tree);

	if (!mtc_hash(tree->ctx, tree->md, 0x00, entry, entry_len, NULL, 0,
	    hash))
		return 0;
	if (!level_push(&tree->levels[0], hash, tree->hash_len))
		return 0;
	if (!tree_update_levels(tree)) {
		tree_truncate(tree, n);
		return 0;
	}

	return 1;
}

static size_t
trailing_ones(uint64_t n)
{
	size_t count = 0;

	while ((n & 1) != 0) {
		n >>= 1;
		count++;
	}

	return count;
}

int
mtc_tree_subtree_hash(const struct mtc_tree *tree, struct mtc_subtree subtree,
    uint8_t out[EVP_MAX_MD_SIZE])
{
	uint64_t start, last;
	size_t level;

	assert(mtc_subtree_is_valid(subtree));
	assert(subtree.end <= mtc_tree_leaf_count(tree));

	/*
	 * Start from the largest full subtree on the right edge, then fold
	 * in left neighbours while rising to the subtree root.  out holds
	 * the hash of the node at (last << level) throughout.
	 */
	start = subtree.start;
	last = subtree.end - 1;
	level = trailing_ones(last - start);
	start >>= level;
	last >>= level;
	memcpy(out, tree_node(tree, level, last), tree->hash_len);

	while (start < last) {
		if ((last & 1) != 0 && !mtc_hash(tree->ctx, tree->md, 0x01,
		    tree_node(tree, level, last - 1), tree->hash_len, out,
		    tree->hash_len, out))
			return 0;
		level++;
		start >>= 1;
		last >>= 1;
	}

	return 1;
}

int
mtc_tree_inclusion_proof(const struct mtc_tree *tree, uint64_t index,
    struct mtc_subtree subtree, uint8_t **out_proof, size_t *out_proof_len)
{
	struct mtc_subtree edge;
	uint8_t hash[EVP_MAX_MD_SIZE];
	uint64_t start, last, neighbour;
	size_t level = 0;
	uint8_t *proof = NULL;
	size_t proof_len;
	CBB cbb;
	int ret = 0;

	assert(mtc_subtree_is_valid(subtree));
	assert(subtree.end <= mtc_tree_leaf_count(tree));
	assert(mtc_subtree_contains_index(subtree, index));

	if (!CBB_init(&cbb, 0))
		goto err;

	/* The proof is the sibling of index's ancestor at each level. */
	start = subtree.start;
	last = subtree.end - 1;
	while (start < last) {
		neighbour = index ^ 1;
		if (neighbour < last) {
			if (!CBB_add_bytes(&cbb, tree_node(tree, level,
			    neighbour), tree->hash_len))
				goto err;
		} else if (neighbour == last) {
			/* The right edge node, which may be partial. */
			edge.start = last << level;
			edge.end = subtree.end;
			if (!mtc_tree_subtree_hash(tree, edge, hash))
				goto err;
			if (!CBB_add_bytes(&cbb, hash, tree->hash_len))
				goto err;
		}
		level++;
		start >>= 1;
		index >>= 1;
		last >>= 1;
	}

	if (!CBB_finish(&cbb, &proof, &proof_len))
		goto err;

	*out_proof = proof;
	*out_proof_len = proof_len;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

/*
 * SUBTREE_PROOF of section 4.4.1.  known_hash is set while the caller
 * holds the hash of subtree, so that hash is omitted from the proof.
 */
static int
consistency_proof(const struct mtc_tree *tree, struct mtc_subtree subtree,
    struct mtc_subtree range, int known_hash, CBB *cbb)
{
	struct mtc_subtree subproof_range, hash_range;
	uint8_t hash[EVP_MAX_MD_SIZE];
	uint64_t k;

	if (subtree.start == range.start && subtree.end == range.end) {
		if (known_hash)
			return 1;
		if (!mtc_tree_subtree_hash(tree, subtree, hash))
			return 0;
		return CBB_add_bytes(cbb, hash, tree->hash_len);
	}

	k = mtc_subtree_split(range);
	if (subtree.end <= k) {
		subproof_range = mtc_subtree_left(range);
		hash_range = mtc_subtree_right(range);
	} else if (subtree.start >= k) {
		hash_range = mtc_subtree_left(range);
		subproof_range = mtc_subtree_right(range);
	} else {
		subtree.start = k;
		hash_range = mtc_subtree_left(range);
		subproof_range = mtc_subtree_right(range);
		known_hash = 0;
	}

	if (!consistency_proof(tree, subtree, subproof_range, known_hash, cbb))
		return 0;
	if (!mtc_tree_subtree_hash(tree, hash_range, hash))
		return 0;

	return CBB_add_bytes(cbb, hash, tree->hash_len);
}

int
mtc_tree_consistency_proof(const struct mtc_tree *tree,
    struct mtc_subtree subtree, struct mtc_subtree tree_range,
    uint8_t **out_proof, size_t *out_proof_len)
{
	uint8_t *proof = NULL;
	size_t proof_len;
	CBB cbb;
	int ret = 0;

	assert(mtc_subtree_is_valid(subtree));
	assert(mtc_subtree_is_valid(tree_range));
	assert(mtc_subtree_contains_subtree(tree_range, subtree));
	assert(tree_range.end <= mtc_tree_leaf_count(tree));

	if (!CBB_init(&cbb, 0))
		goto err;
	if (!consistency_proof(tree, subtree, tree_range, 1, &cbb))
		goto err;
	if (!CBB_finish(&cbb, &proof, &proof_len))
		goto err;

	*out_proof = proof;
	*out_proof_len = proof_len;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}
