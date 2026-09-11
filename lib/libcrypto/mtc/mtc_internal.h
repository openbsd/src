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

#ifndef HEADER_MTC_INTERNAL_H
#define HEADER_MTC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <openssl/evp.h>

#include "bytestring.h"

__BEGIN_HIDDEN_DECLS

/*
 * Merkle tree subtrees, per section 4 of
 * draft-ietf-plants-merkle-tree-certs-05.
 *
 * A subtree is the half-open interval [start, end) of leaf indices.  It is
 * valid when it is non-empty and start is a multiple of the smallest power
 * of two that is not less than end - start.  Whole trees are the subtrees
 * starting at zero.
 *
 * Every node hash is EVP_MD_size() bytes of the CA's digest.  Buffers that
 * receive one are EVP_MAX_MD_SIZE bytes.  A proof is a concatenation of
 * node hashes.
 *
 * These functions do not check for NULL pointers.  mtc_tree_free() accepts
 * a NULL tree.
 */

struct mtc_subtree {
	uint64_t start;
	uint64_t end;
};

int mtc_subtree_is_valid(struct mtc_subtree subtree);
uint64_t mtc_subtree_leaf_count(struct mtc_subtree subtree);
uint64_t mtc_subtree_split(struct mtc_subtree subtree);
struct mtc_subtree mtc_subtree_left(struct mtc_subtree subtree);
struct mtc_subtree mtc_subtree_right(struct mtc_subtree subtree);
int mtc_subtree_contains_index(struct mtc_subtree subtree, uint64_t index);
int mtc_subtree_contains_subtree(struct mtc_subtree outer,
    struct mtc_subtree inner);

/*
 * Covers the interval [start, end), start < end, with one or two valid
 * subtrees written to out (section 4.5).  A one-leaf interval is covered by
 * itself.  Otherwise out[0] is a full subtree ending where out[1] starts,
 * out[0].start <= start and out[1].end == end.  Returns the number written.
 */
size_t mtc_find_subtrees(struct mtc_subtree interval,
    struct mtc_subtree out[2]);

/* HASH(0x00 || entry) and HASH(0x01 || left || right).  out may alias. */
int mtc_hash_leaf(const EVP_MD *md, const uint8_t *entry, size_t entry_len,
    uint8_t out[EVP_MAX_MD_SIZE]);
int mtc_hash_node(const EVP_MD *md, const uint8_t *left, const uint8_t *right,
    uint8_t out[EVP_MAX_MD_SIZE]);

/*
 * Recomputes the root hash of a tree of n leaves from the hash of one of its
 * subtrees and a consistency proof (section 4.4.3).  Returns 1 and writes
 * out_root_hash when the proof is well-formed and consistent with node_hash.
 * The caller compares out_root_hash against the root it trusts.
 */
int mtc_eval_subtree_consistency_proof(const EVP_MD *md, uint64_t n,
    struct mtc_subtree subtree, const uint8_t *proof, size_t proof_len,
    const uint8_t *node_hash, uint8_t out_root_hash[EVP_MAX_MD_SIZE]);

/*
 * Recomputes the hash of subtree from the hash of the leaf at index (in
 * whole-tree coordinates) and an inclusion proof (section 4.3.2).  Returns
 * 1 and writes out_root_hash when the proof is well-formed.  The caller
 * compares out_root_hash against the subtree hash it trusts.
 */
int mtc_eval_subtree_inclusion_proof(const EVP_MD *md, const uint8_t *proof,
    size_t proof_len, uint64_t index, const uint8_t *entry_hash,
    struct mtc_subtree subtree, uint8_t out_root_hash[EVP_MAX_MD_SIZE]);

/*
 * An in-memory Merkle tree that produces the proofs the evaluators above
 * consume.  md must outlive the tree.
 */
struct mtc_tree;

struct mtc_tree *mtc_tree_new(const EVP_MD *md);
void mtc_tree_free(struct mtc_tree *tree);
int mtc_tree_append(struct mtc_tree *tree, const uint8_t *entry,
    size_t entry_len);
uint64_t mtc_tree_leaf_count(const struct mtc_tree *tree);

/* subtree must be valid with end <= mtc_tree_leaf_count(). */
int mtc_tree_subtree_hash(const struct mtc_tree *tree,
    struct mtc_subtree subtree, uint8_t out[EVP_MAX_MD_SIZE]);

/*
 * Proofs are returned in a buffer allocated with malloc(3), which the caller
 * frees.  Neither output is written on failure.  index must lie in subtree;
 * subtree must lie in tree_range; both must be valid with end <=
 * mtc_tree_leaf_count().
 */
int mtc_tree_inclusion_proof(const struct mtc_tree *tree, uint64_t index,
    struct mtc_subtree subtree, uint8_t **out_proof, size_t *out_proof_len);
int mtc_tree_consistency_proof(const struct mtc_tree *tree,
    struct mtc_subtree subtree, struct mtc_subtree tree_range,
    uint8_t **out_proof, size_t *out_proof_len);

/*
 * The MTCProof carried in the signatureValue of a Merkle Tree Certificate,
 * per section 6.2 of draft-ietf-plants-merkle-tree-certs-05.  Every CBS
 * references the input the proof was parsed from, which must outlive it.
 */

struct mtc_cosignature {
	CBS cosigner_id;
	CBS signature;
};

struct mtc_proof {
	CBS extensions;
	uint64_t start;
	uint64_t end;
	CBS inclusion_proof;
	CBS signatures;
};

/*
 * Parses the MTCProof in [in, in_len).  Fails on trailing bytes, on
 * start >= end, and on a cosignature list that is not strictly ascending by
 * cosigner_id.  proof is untouched on failure.
 */
int mtc_proof_parse(const uint8_t *in, size_t in_len, struct mtc_proof *proof);

/*
 * Extracts the cosignatures of a parsed proof.  With out NULL only *count is
 * set.  With out non-NULL, fails if more than max are present.  Nothing is
 * written on failure.
 */
int mtc_proof_get_cosignatures(const struct mtc_proof *proof,
    struct mtc_cosignature *out, size_t max, size_t *count);

__END_HIDDEN_DECLS

#endif /* !HEADER_MTC_INTERNAL_H */
