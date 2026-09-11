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

#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "mtc_internal.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

/* Number of leaves in the exhaustive round-trip test. */
#define MTC_TEST_LIMIT 65

/* Length of the leaves build_tree() appends. */
#define MTC_TEST_ENTRY_LEN (5 + 8)

static const EVP_MD *md;
static size_t hash_len;

static struct mtc_subtree
subtree(uint64_t start, uint64_t end)
{
	struct mtc_subtree s = { start, end };

	return s;
}

/* Leaf i is "label" followed by the 8 bytes of i, most significant first. */
static void
make_entry(uint64_t i, uint8_t entry[MTC_TEST_ENTRY_LEN])
{
	size_t j;

	memcpy(entry, "label", 5);
	for (j = 0; j < 8; j++)
		entry[5 + j] = (uint8_t)(i >> (56 - j * 8));
}

static struct mtc_tree *
build_tree(uint64_t n)
{
	struct mtc_tree *tree;
	uint8_t entry[MTC_TEST_ENTRY_LEN];
	uint64_t i;

	if ((tree = mtc_tree_new(md)) == NULL)
		errx(1, "mtc_tree_new");
	for (i = 0; i < n; i++) {
		make_entry(i, entry);
		if (!mtc_tree_append(tree, entry, sizeof(entry)))
			errx(1, "mtc_tree_append %llu", i);
	}

	return tree;
}

static int
leaf_hash(const struct mtc_tree *tree, uint64_t index,
    uint8_t out[EVP_MAX_MD_SIZE])
{
	return mtc_tree_subtree_hash(tree, subtree(index, index + 1), out);
}

static int
check_valid(uint64_t start, uint64_t end, int want)
{
	int got;

	if ((got = mtc_subtree_is_valid(subtree(start, end))) != want) {
		warnx("is_valid([%llu, %llu)) = %d, want %d", start, end, got,
		    want);
		return 1;
	}

	return 0;
}

static int
test_subtree_is_valid(void)
{
	int failed = 0;

	failed |= check_valid(0, 0, 0);
	failed |= check_valid(1, 0, 0);
	failed |= check_valid(0, UINT64_MAX, 1);
	failed |= check_valid(4, 8, 1);
	failed |= check_valid(4, 9, 0);
	failed |= check_valid(4, 6, 1);
	failed |= check_valid(0, 6, 1);

	return failed;
}

static int
check_split(uint64_t start, uint64_t end, uint64_t want)
{
	uint64_t got;

	if ((got = mtc_subtree_split(subtree(start, end))) != want) {
		warnx("split([%llu, %llu)) = %llu, want %llu", start, end, got,
		    want);
		return 1;
	}

	return 0;
}

static int
test_subtree_split(void)
{
	int failed = 0;

	failed |= check_split(24601, 24601, 24601);
	failed |= check_split(1336, 1337, 1337);
	failed |= check_split(42, 44, 43);
	failed |= check_split(0, 31, 16);
	failed |= check_split(64, 128, 96);
	failed |= check_split(0, 257, 256);
	failed |= check_split(0, UINT64_MAX, UINT64_C(1) << 63);
	failed |= check_split(UINT64_MAX - 3, UINT64_MAX, UINT64_MAX - 1);

	return failed;
}

/*
 * Generate the inclusion proof for index within s and check that it
 * evaluates to the hash of s.
 */
static int
check_inclusion(const struct mtc_tree *tree, uint64_t index,
    struct mtc_subtree s, const char *name)
{
	uint8_t entry_hash[EVP_MAX_MD_SIZE];
	uint8_t want[EVP_MAX_MD_SIZE];
	uint8_t got[EVP_MAX_MD_SIZE];
	uint8_t *proof = NULL;
	size_t proof_len = 0;
	int failed = 1;

	if (!leaf_hash(tree, index, entry_hash)) {
		warnx("%s: leaf_hash", name);
		goto err;
	}
	if (!mtc_tree_subtree_hash(tree, s, want)) {
		warnx("%s: subtree_hash", name);
		goto err;
	}
	if (!mtc_tree_inclusion_proof(tree, index, s, &proof, &proof_len)) {
		warnx("%s: inclusion_proof", name);
		goto err;
	}
	if (!mtc_eval_subtree_inclusion_proof(md, proof, proof_len, index,
	    entry_hash, s, got)) {
		warnx("%s: eval_inclusion_proof", name);
		goto err;
	}
	if (memcmp(got, want, hash_len) != 0) {
		warnx("%s: inclusion proof gives the wrong hash", name);
		goto err;
	}

	failed = 0;

 err:
	free(proof);

	return failed;
}

/*
 * Generate the consistency proof for s within the tree of n leaves and check
 * that it evaluates to the hash of that tree.
 */
static int
check_consistency(const struct mtc_tree *tree, struct mtc_subtree s,
    uint64_t n, const char *name)
{
	uint8_t node_hash[EVP_MAX_MD_SIZE];
	uint8_t want[EVP_MAX_MD_SIZE];
	uint8_t got[EVP_MAX_MD_SIZE];
	uint8_t *proof = NULL;
	size_t proof_len = 0;
	int failed = 1;

	if (!mtc_tree_subtree_hash(tree, s, node_hash)) {
		warnx("%s: subtree_hash", name);
		goto err;
	}
	if (!mtc_tree_subtree_hash(tree, subtree(0, n), want)) {
		warnx("%s: tree hash", name);
		goto err;
	}
	if (!mtc_tree_consistency_proof(tree, s, subtree(0, n), &proof,
	    &proof_len)) {
		warnx("%s: consistency_proof", name);
		goto err;
	}
	if (!mtc_eval_subtree_consistency_proof(md, n, s, proof, proof_len,
	    node_hash, got)) {
		warnx("%s: eval_consistency_proof", name);
		goto err;
	}
	if (memcmp(got, want, hash_len) != 0) {
		warnx("%s: consistency proof gives the wrong hash", name);
		goto err;
	}

	failed = 0;

 err:
	free(proof);

	return failed;
}

static int
test_inclusion_roundtrip(void)
{
	struct mtc_tree *tree;
	int failed = 0;

	tree = build_tree(847);

	failed |= check_inclusion(tree, 0, subtree(0, 16), "leaf 0 of [0, 16)");
	failed |= check_inclusion(tree, 845, subtree(840, 847),
	    "leaf 845 of [840, 847)");

	mtc_tree_free(tree);

	return failed;
}

static int
test_inclusion_invalid_args(void)
{
	struct mtc_tree *tree;
	struct mtc_subtree s;
	uint8_t node_hash[EVP_MAX_MD_SIZE];
	uint8_t wrong_hash[EVP_MAX_MD_SIZE];
	uint8_t want[EVP_MAX_MD_SIZE];
	uint8_t got[EVP_MAX_MD_SIZE];
	uint8_t *proof = NULL;
	size_t proof_len = 0;
	int failed = 0;

	tree = build_tree(847);
	s = subtree(840, 847);

	if (!leaf_hash(tree, 845, node_hash) ||
	    !mtc_tree_subtree_hash(tree, s, want) ||
	    !mtc_tree_inclusion_proof(tree, 845, s, &proof, &proof_len))
		errx(1, "inclusion proof setup");

	/* A wrong leaf hash evaluates, to the wrong subtree hash. */
	if (!leaf_hash(tree, 846, wrong_hash))
		errx(1, "leaf_hash");
	if (!mtc_eval_subtree_inclusion_proof(md, proof, proof_len, 845,
	    wrong_hash, s, got)) {
		warnx("wrong leaf hash: evaluation failed");
		failed = 1;
	} else if (memcmp(got, want, hash_len) == 0) {
		warnx("wrong leaf hash gives the right subtree hash");
		failed = 1;
	}

	if (mtc_eval_subtree_inclusion_proof(md, proof, proof_len, 845,
	    node_hash, subtree(840, 849), got)) {
		warnx("invalid subtree accepted");
		failed = 1;
	}

	if (mtc_eval_subtree_inclusion_proof(md, proof, proof_len, 848,
	    node_hash, s, got)) {
		warnx("index outside the subtree accepted");
		failed = 1;
	}

	free(proof);
	mtc_tree_free(tree);

	return failed;
}

/*
 * A tree of one leaf: its root is that leaf's hash, and the proofs for the
 * whole of it are empty.
 */
static int
test_one_leaf(void)
{
	struct mtc_tree *tree;
	struct mtc_subtree full;
	uint8_t entry[MTC_TEST_ENTRY_LEN];
	uint8_t want[EVP_MAX_MD_SIZE];
	uint8_t got[EVP_MAX_MD_SIZE];
	uint8_t *proof = NULL;
	size_t proof_len = 0;
	int failed = 0;

	tree = build_tree(1);
	full = subtree(0, 1);

	if (mtc_tree_leaf_count(tree) != 1) {
		warnx("leaf count %llu, want 1", mtc_tree_leaf_count(tree));
		failed = 1;
	}

	make_entry(0, entry);
	if (!mtc_hash_leaf(md, entry, sizeof(entry), want) ||
	    !mtc_tree_subtree_hash(tree, full, got))
		errx(1, "one leaf hashes");
	if (memcmp(got, want, hash_len) != 0) {
		warnx("root of a one-leaf tree is not the leaf hash");
		failed = 1;
	}

	if (!mtc_tree_inclusion_proof(tree, 0, full, &proof, &proof_len))
		errx(1, "inclusion_proof");
	if (proof_len != 0) {
		warnx("one-leaf inclusion proof has length %zu", proof_len);
		failed = 1;
	}
	free(proof);
	proof = NULL;

	if (!mtc_tree_consistency_proof(tree, full, full, &proof, &proof_len))
		errx(1, "consistency_proof");
	if (proof_len != 0) {
		warnx("one-leaf consistency proof has length %zu", proof_len);
		failed = 1;
	}
	free(proof);

	failed |= check_inclusion(tree, 0, full, "one leaf");
	failed |= check_consistency(tree, full, 1, "one leaf");

	mtc_tree_free(tree);

	return failed;
}

/*
 * A subtree spanning the whole tree: its consistency proof is empty and
 * every leaf's inclusion proof evaluates to the root.
 */
static int
test_whole_tree_subtree(void)
{
	struct mtc_tree *tree;
	struct mtc_subtree full;
	uint8_t *proof = NULL;
	size_t proof_len = 0;
	uint64_t index;
	int failed = 0;

	tree = build_tree(13);
	full = subtree(0, 13);

	if (!mtc_tree_consistency_proof(tree, full, full, &proof, &proof_len))
		errx(1, "consistency_proof");
	if (proof_len != 0) {
		warnx("whole-tree consistency proof has length %zu", proof_len);
		failed = 1;
	}
	free(proof);

	failed |= check_consistency(tree, full, 13, "whole tree");
	for (index = 0; index < 13; index++)
		failed |= check_inclusion(tree, index, full, "whole tree");

	mtc_tree_free(tree);

	return failed;
}

/*
 * Check that proof is the concatenation of the hashes of parts, computed
 * over tree.
 */
static int
check_proof_parts(const uint8_t *proof, size_t proof_len,
    const struct mtc_tree *tree, const struct mtc_subtree *parts,
    size_t nparts, const char *name)
{
	uint8_t want[8 * EVP_MAX_MD_SIZE];
	size_t i;

	if (nparts > sizeof(want) / hash_len)
		errx(1, "%s: too many parts", name);
	for (i = 0; i < nparts; i++) {
		if (!mtc_tree_subtree_hash(tree, parts[i], want + i * hash_len))
			errx(1, "%s: subtree_hash", name);
	}
	if (proof_len != nparts * hash_len ||
	    memcmp(proof, want, proof_len) != 0) {
		warnx("%s: proof differs from expected parts", name);
		return 1;
	}

	return 0;
}

static int
check_inclusion_parts(const struct mtc_tree *tree, uint64_t index,
    struct mtc_subtree s, const struct mtc_subtree *parts, size_t nparts,
    const char *name)
{
	uint8_t *proof = NULL;
	size_t proof_len = 0;
	int failed;

	if (!mtc_tree_inclusion_proof(tree, index, s, &proof, &proof_len))
		errx(1, "%s: inclusion_proof", name);
	failed = check_proof_parts(proof, proof_len, tree, parts, nparts, name);
	free(proof);

	return failed;
}

static int
check_consistency_parts(const struct mtc_tree *tree, struct mtc_subtree s,
    struct mtc_subtree range, const struct mtc_subtree *parts, size_t nparts,
    const char *name)
{
	uint8_t *proof = NULL;
	size_t proof_len = 0;
	int failed;

	if (!mtc_tree_consistency_proof(tree, s, range, &proof, &proof_len))
		errx(1, "%s: consistency_proof", name);
	failed = check_proof_parts(proof, proof_len, tree, parts, nparts, name);
	free(proof);

	return failed;
}

/*
 * The structural examples of RFC 9162 section 2.1.5, over a tree of 7
 * leaves d0..d6 with nodes b = d1, c = d2, d = d3, f = d5, g = [0, 2),
 * h = [2, 4), i = [4, 6), j = d6, k = [0, 4), l = [4, 7).
 */
static int
test_rfc9162_structure(void)
{
	struct mtc_tree *tree;
	struct mtc_subtree full;
	const struct mtc_subtree d0_parts[] = {
		{ 1, 2 }, { 2, 4 }, { 4, 7 },
	};
	const struct mtc_subtree d3_parts[] = {
		{ 2, 3 }, { 0, 2 }, { 4, 7 },
	};
	const struct mtc_subtree d6_parts[] = {
		{ 4, 6 }, { 0, 4 },
	};
	const struct mtc_subtree hash0_parts[] = {
		{ 2, 3 }, { 3, 4 }, { 0, 2 }, { 4, 7 },
	};
	const struct mtc_subtree hash2_parts[] = {
		{ 4, 6 }, { 6, 7 }, { 0, 4 },
	};
	int failed = 0;

	tree = build_tree(7);
	full = subtree(0, 7);

	/* Inclusion proof for d0 is [b, h, l]. */
	failed |= check_inclusion_parts(tree, 0, full, d0_parts,
	    nitems(d0_parts), "RFC 9162 d0");
	/* Inclusion proof for d3 is [c, g, l]. */
	failed |= check_inclusion_parts(tree, 3, full, d3_parts,
	    nitems(d3_parts), "RFC 9162 d3");
	/* Inclusion proof for d6 is [i, k]. */
	failed |= check_inclusion_parts(tree, 6, full, d6_parts,
	    nitems(d6_parts), "RFC 9162 d6");
	/* Consistency proof for hash0 = [0, 3) is [c, d, g, l]. */
	failed |= check_consistency_parts(tree, subtree(0, 3), full,
	    hash0_parts, nitems(hash0_parts), "RFC 9162 hash0");
	/* Consistency proof for hash2 = [0, 6) is [i, j, k]. */
	failed |= check_consistency_parts(tree, subtree(0, 6), full,
	    hash2_parts, nitems(hash2_parts), "RFC 9162 hash2");

	mtc_tree_free(tree);

	return failed;
}

/*
 * The worked examples of section 4 of draft-ietf-plants-merkle-tree-certs-05:
 * the full subtree [4, 8) and the partial subtree [8, 13) (Figures 3-5), the
 * inclusion proof of Figure 6 and the consistency proofs of Figures 7 and 8.
 */
static int
test_plants_section4_examples(void)
{
	struct mtc_tree *t13, *t14;
	struct mtc_subtree full13, full14, s48, s813;
	const struct mtc_subtree fig6_parts[] = {
		{ 11, 12 }, { 8, 10 }, { 12, 13 },
	};
	const struct mtc_subtree fig7_parts[] = {
		{ 0, 4 }, { 8, 14 },
	};
	const struct mtc_subtree fig8_parts[] = {
		{ 12, 13 }, { 13, 14 }, { 8, 12 }, { 0, 8 },
	};
	int failed = 0;

	t13 = build_tree(13);
	t14 = build_tree(14);
	full13 = subtree(0, 13);
	full14 = subtree(0, 14);
	s48 = subtree(4, 8);
	s813 = subtree(8, 13);

	if (!mtc_subtree_is_valid(s48) || mtc_subtree_leaf_count(s48) != 4 ||
	    !mtc_subtree_is_valid(s813) || mtc_subtree_leaf_count(s813) != 5 ||
	    !mtc_subtree_contains_subtree(full13, s48) ||
	    !mtc_subtree_contains_subtree(full13, s813)) {
		warnx("section 4.1 subtrees");
		failed = 1;
	}

	/*
	 * Figure 6: entry 10 of [8, 13) is
	 * [MTH(d11), MTH(D[8:10]), MTH(d12)].
	 */
	failed |= check_inclusion_parts(t13, 10, s813, fig6_parts,
	    nitems(fig6_parts), "Figure 6");
	failed |= check_inclusion(t13, 10, s813, "Figure 6");

	/* Figure 7: [4, 8) in a size-14 tree is [MTH(D[0:4]), MTH(D[8:14])]. */
	failed |= check_consistency_parts(t14, s48, full14, fig7_parts,
	    nitems(fig7_parts), "Figure 7");
	failed |= check_consistency(t14, s48, 14, "Figure 7");

	/*
	 * Figure 8: [8, 13) in a size-14 tree is
	 * [MTH(d12), MTH(d13), MTH(D[8:12]), MTH(D[0:8])].
	 */
	failed |= check_consistency_parts(t14, s813, full14, fig8_parts,
	    nitems(fig8_parts), "Figure 8");
	failed |= check_consistency(t14, s813, 14, "Figure 8");

	mtc_tree_free(t13);
	mtc_tree_free(t14);

	return failed;
}

static int
is_power_of_two(uint64_t x)
{
	return x != 0 && (x & (x - 1)) == 0;
}

static int
check_find_subtrees(uint64_t start, uint64_t end, size_t want_count,
    const struct mtc_subtree *want)
{
	struct mtc_subtree out[2];
	size_t count, i;

	count = mtc_find_subtrees(subtree(start, end), out);
	if (count != want_count) {
		warnx("find_subtrees([%llu, %llu)) returned %zu, want %zu",
		    start, end, count, want_count);
		return 1;
	}
	for (i = 0; i < count; i++) {
		if (out[i].start != want[i].start ||
		    out[i].end != want[i].end) {
			warnx("find_subtrees([%llu, %llu))[%zu] = [%llu, %llu),"
			    " want [%llu, %llu)", start, end, i, out[i].start,
			    out[i].end, want[i].start, want[i].end);
			return 1;
		}
	}

	return 0;
}

/*
 * Section 4.5: the worked examples of Figures 9 and 10, then the covering
 * properties for every small interval.
 */
static int
test_find_subtrees(void)
{
	const struct mtc_subtree fig9[] = { { 4, 8 }, { 8, 13 } };
	const struct mtc_subtree fig10[] = { { 7, 8 }, { 8, 9 } };
	const struct mtc_subtree one[] = { { 5, 6 } };
	struct mtc_subtree out[2];
	uint64_t start, end, len;
	size_t count;
	int failed = 0;

	failed |= check_find_subtrees(5, 13, 2, fig9);
	failed |= check_find_subtrees(7, 9, 2, fig10);
	failed |= check_find_subtrees(5, 6, 1, one);

	for (end = 1; end <= 64; end++) {
		for (start = 0; start < end; start++) {
			len = end - start;
			count = mtc_find_subtrees(subtree(start, end), out);

			if (count == 1) {
				if (!mtc_subtree_is_valid(out[0]) ||
				    out[0].start != start ||
				    out[0].end != end) {
					warnx("[%llu, %llu): bad single cover",
					    start, end);
					failed = 1;
				}
				continue;
			}

			if (count != 2 ||
			    !mtc_subtree_is_valid(out[0]) ||
			    !mtc_subtree_is_valid(out[1]) ||
			    out[0].end != out[1].start ||
			    out[0].start > start ||
			    out[1].end != end ||
			    !is_power_of_two(mtc_subtree_leaf_count(out[0])) ||
			    mtc_subtree_leaf_count(out[0]) >= 2 * len ||
			    mtc_subtree_leaf_count(out[1]) > len) {
				warnx("[%llu, %llu): bad two-subtree cover",
				    start, end);
				failed = 1;
			}
		}
	}

	return failed;
}

/*
 * Generate and verify consistency proofs for every valid subtree of every
 * tree size up to MTC_TEST_LIMIT, and inclusion proofs for every leaf of
 * every valid subtree.
 */
static int
test_exhaustive(void)
{
	struct mtc_tree *tree;
	struct mtc_subtree s;
	uint64_t n, start, end, index;
	int failed = 0;

	tree = build_tree(MTC_TEST_LIMIT);

	for (n = 1; n < MTC_TEST_LIMIT; n++) {
		for (end = 1; end <= n; end++) {
			for (start = 0; start < end; start++) {
				s = subtree(start, end);
				if (!mtc_subtree_is_valid(s))
					continue;
				failed |= check_consistency(tree, s, n,
				    "exhaustive");
			}
		}
	}

	for (end = 1; end <= MTC_TEST_LIMIT; end++) {
		for (start = 0; start < end; start++) {
			s = subtree(start, end);
			if (!mtc_subtree_is_valid(s))
				continue;
			for (index = start; index < end; index++)
				failed |= check_inclusion(tree, index, s,
				    "exhaustive");
		}
	}

	mtc_tree_free(tree);

	return failed;
}

/*
 * The accumulated test vectors of Appendix C of
 * draft-ietf-plants-merkle-tree-certs-05: over a tree whose leaf i is the
 * single byte i, every output of each section 4 algorithm for tree sizes up
 * to 130 is formatted as a text line and fed to one SHA-256.
 */

#define VECTOR_LEAVES 130

static const char *vector_subtree_hashes =
    "94a95384a8c69acea9b50d035a58285b3a777cb7a724005faa5e1f1e1190007f";
static const char *vector_inclusion_proofs =
    "ac2a8f989e44d99e399db448050ff5f19757df53cfb716aa81015d3955d8163f";
static const char *vector_consistency_proofs =
    "c586ebbb73a5621baf2140095d87dde934e3b6503a562a1a5215b8209edd083d";
static const char *vector_covering_subtrees =
    "e0aecb912a10c57d753b6ecc64db73217f9bc4ed10fcb4e9062be3b6fbe1ebfd";

static struct mtc_tree *
build_byte_tree(void)
{
	struct mtc_tree *tree;
	uint8_t leaf;
	unsigned int i;

	if ((tree = mtc_tree_new(md)) == NULL)
		errx(1, "mtc_tree_new");
	for (i = 0; i < VECTOR_LEAVES; i++) {
		leaf = i;
		if (!mtc_tree_append(tree, &leaf, 1))
			errx(1, "mtc_tree_append %u", i);
	}

	return tree;
}

static void
accumulate(EVP_MD_CTX *acc, const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (len < 0 || (size_t)len >= sizeof(buf))
		errx(1, "accumulate: line too long");
	if (!EVP_DigestUpdate(acc, buf, len))
		errx(1, "EVP_DigestUpdate");
}

/* Feed " " followed by the hex of each hash_len-byte hash in proof. */
static void
accumulate_hashes(EVP_MD_CTX *acc, const uint8_t *proof, size_t proof_len)
{
	size_t i, j;

	for (i = 0; i < proof_len; i += hash_len) {
		accumulate(acc, " ");
		for (j = 0; j < hash_len; j++)
			accumulate(acc, "%02x", proof[i + j]);
	}
}

static int
check_vector(EVP_MD_CTX *acc, const char *want, const char *name)
{
	uint8_t digest[EVP_MAX_MD_SIZE];
	char hex[2 * EVP_MAX_MD_SIZE + 1];
	unsigned int digest_len, i;

	if (!EVP_DigestFinal_ex(acc, digest, &digest_len))
		errx(1, "EVP_DigestFinal_ex");
	for (i = 0; i < digest_len; i++)
		snprintf(hex + 2 * i, 3, "%02x", digest[i]);
	if (strcmp(hex, want) != 0) {
		warnx("%s: accumulated %s, want %s", name, hex, want);
		return 1;
	}

	return 0;
}

static int
test_vectors(void)
{
	struct mtc_tree *tree;
	struct mtc_subtree s, out[2];
	EVP_MD_CTX *acc;
	uint8_t hash[EVP_MAX_MD_SIZE];
	uint8_t *proof = NULL;
	size_t proof_len = 0, j;
	uint64_t n, start, end, index;
	int failed = 0;

	tree = build_byte_tree();
	if ((acc = EVP_MD_CTX_new()) == NULL)
		errx(1, "EVP_MD_CTX_new");

	/* C.1 */
	if (!EVP_DigestInit_ex(acc, EVP_sha256(), NULL))
		errx(1, "EVP_DigestInit_ex");
	for (end = 1; end <= VECTOR_LEAVES; end++) {
		for (start = 0; start < end; start++) {
			s = subtree(start, end);
			if (!mtc_subtree_is_valid(s))
				continue;
			if (!mtc_tree_subtree_hash(tree, s, hash))
				errx(1, "subtree_hash");
			accumulate(acc, "[%llu, %llu)", start, end);
			accumulate_hashes(acc, hash, hash_len);
			accumulate(acc, "\n");
		}
	}
	failed |= check_vector(acc, vector_subtree_hashes, "C.1");

	/* C.2 */
	if (!EVP_DigestInit_ex(acc, EVP_sha256(), NULL))
		errx(1, "EVP_DigestInit_ex");
	for (end = 1; end <= VECTOR_LEAVES; end++) {
		for (start = 0; start < end; start++) {
			s = subtree(start, end);
			if (!mtc_subtree_is_valid(s))
				continue;
			for (index = start; index < end; index++) {
				if (!mtc_tree_inclusion_proof(tree, index, s,
				    &proof, &proof_len))
					errx(1, "inclusion_proof");
				accumulate(acc, "%llu [%llu, %llu)", index,
				    start, end);
				accumulate_hashes(acc, proof, proof_len);
				accumulate(acc, "\n");
				free(proof);
				proof = NULL;
			}
		}
	}
	failed |= check_vector(acc, vector_inclusion_proofs, "C.2");

	/* C.3 */
	if (!EVP_DigestInit_ex(acc, EVP_sha256(), NULL))
		errx(1, "EVP_DigestInit_ex");
	for (n = 0; n <= VECTOR_LEAVES; n++) {
		for (end = 1; end <= n; end++) {
			for (start = 0; start < end; start++) {
				s = subtree(start, end);
				if (!mtc_subtree_is_valid(s))
					continue;
				if (!mtc_tree_consistency_proof(tree, s,
				    subtree(0, n), &proof, &proof_len))
					errx(1, "consistency_proof");
				accumulate(acc, "[%llu, %llu) %llu", start, end,
				    n);
				accumulate_hashes(acc, proof, proof_len);
				accumulate(acc, "\n");
				free(proof);
				proof = NULL;
			}
		}
	}
	failed |= check_vector(acc, vector_consistency_proofs, "C.3");

	/* C.4 */
	if (!EVP_DigestInit_ex(acc, EVP_sha256(), NULL))
		errx(1, "EVP_DigestInit_ex");
	for (end = 1; end <= VECTOR_LEAVES; end++) {
		for (start = 0; start < end; start++) {
			s = subtree(start, end);
			if (mtc_subtree_is_valid(s)) {
				accumulate(acc, "[%llu, %llu)\n", start, end);
				continue;
			}
			if (mtc_find_subtrees(s, out) != 2)
				errx(1, "find_subtrees");
			for (j = 0; j < 2; j++)
				accumulate(acc, "[%llu, %llu)%s", out[j].start,
				    out[j].end, j == 0 ? " " : "\n");
		}
	}
	failed |= check_vector(acc, vector_covering_subtrees, "C.4");

	EVP_MD_CTX_free(acc);
	mtc_tree_free(tree);

	return failed;
}

int
main(void)
{
	int failed = 0;

	md = EVP_sha256();
	hash_len = EVP_MD_size(md);

	failed |= test_subtree_is_valid();
	failed |= test_subtree_split();
	failed |= test_one_leaf();
	failed |= test_whole_tree_subtree();
	failed |= test_inclusion_roundtrip();
	failed |= test_inclusion_invalid_args();
	failed |= test_rfc9162_structure();
	failed |= test_plants_section4_examples();
	failed |= test_find_subtrees();
	failed |= test_exhaustive();
	failed |= test_vectors();

	return failed;
}
