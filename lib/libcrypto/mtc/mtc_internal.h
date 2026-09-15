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

#include <sys/queue.h>
#include <sys/tree.h>

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/mtc.h>
#include <openssl/x509.h>

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

/*
 * A Merkle Tree CA as configured by a relying party, per section 7.1 of
 * draft-ietf-plants-merkle-tree-certs-05.  The CA cosigner (section 5.4)
 * is identified by the CA's own ID; cosigners holds the other cosigners
 * the relying party recognises.  IDs are TrustAnchorID relative-OID bytes.
 * lock guards the fields that change after construction.
 */

struct mtc_cosigner {
	SLIST_ENTRY(mtc_cosigner) entry;
	uint8_t *id;
	size_t id_len;
	EVP_PKEY *pkey;
};

/* Revoked serials are kept as disjoint half-open ranges ordered by start. */
struct mtc_serial_range {
	RB_ENTRY(mtc_serial_range) entry;
	uint64_t start;
	uint64_t end;
};

RB_HEAD(mtc_range_tree, mtc_serial_range);

/*
 * A subtree of an issuance log covering one of its active landmarks
 * (section 6.4.1).  hash is meaningful once hashed is set.
 */
struct mtc_trusted_subtree {
	RB_ENTRY(mtc_trusted_subtree) entry;
	uint64_t landmark;
	struct mtc_subtree subtree;
	int hashed;
	uint8_t hash[EVP_MAX_MD_SIZE];
};

RB_HEAD(mtc_subtree_tree, mtc_trusted_subtree);

/* Log indices are at most 2^48 - 1 (section 5.2). */
#define MTC_MAX_TREE_SIZE	(UINT64_C(1) << 48)

/*
 * An issuance log (section 5.2) and its active landmark window: the
 * subtrees array owns the entries, tree indexes them by (start, end).
 */
struct mtc_log {
	SLIST_ENTRY(mtc_log) entry;
	uint64_t log_number;
	uint64_t last_landmark;
	struct mtc_trusted_subtree *subtrees;
	size_t subtree_count;
	struct mtc_subtree_tree tree;
};

struct mtc_ca {
	uint8_t *id;
	size_t id_len;
	const EVP_MD *hash;
	uint64_t min_serial;
	uint64_t max_serial;
	EVP_PKEY *cosigner_pkey;
	pthread_mutex_t lock;
	SLIST_HEAD(, mtc_cosigner) cosigners;
	struct mtc_range_tree revoked;
	SLIST_HEAD(, mtc_log) logs;
};

/*
 * The ID bytes are copied and a reference is taken on cosigner_pkey.  Fails
 * on an empty ID.
 */
struct mtc_ca *mtc_ca_new(const uint8_t *id, size_t id_len,
    const EVP_MD *hash, uint64_t min_serial, EVP_PKEY *cosigner_pkey);
void mtc_ca_free(struct mtc_ca *ca);

/*
 * Adds a cosigner; the ID bytes are copied and a reference is taken on
 * pkey.  Fails if id is empty, the CA's own ID or one already added.
 */
int mtc_ca_add_cosigner(struct mtc_ca *ca, const uint8_t *id, size_t id_len,
    EVP_PKEY *pkey);

const uint8_t *mtc_ca_id(const struct mtc_ca *ca, size_t *out_len);
const EVP_MD *mtc_ca_hash(const struct mtc_ca *ca);
EVP_PKEY *mtc_ca_cosigner_pkey(const struct mtc_ca *ca);

/*
 * The set of CAs a relying party trusts is a STACK_OF(OSSL_MTC_CA) ordered
 * by CA ID (shorter first, then bytewise) with mtc_ca_cmp() as its
 * comparison function.  The stack does not own the CAs.  Adding a CA whose
 * ID is already present fails.
 */
int mtc_ca_cmp(const OSSL_MTC_CA * const *a, const OSSL_MTC_CA * const *b);
int mtc_ca_stack_add(STACK_OF(OSSL_MTC_CA) *cas, struct mtc_ca *ca);
struct mtc_ca *mtc_ca_stack_lookup(STACK_OF(OSSL_MTC_CA) *cas,
    const uint8_t *id, size_t id_len);

/*
 * Converts a trust anchor ID in dotted-decimal form to the content octets
 * of its relative-OID encoding (section 4 of
 * draft-ietf-tls-trust-anchor-ids-05), in a buffer allocated with
 * malloc(3).  Components are at most UINT64_MAX.
 */
int mtc_reloid_from_text(const char *text, size_t text_len, uint8_t **out,
    size_t *out_len);

/*
 * Extracts the CA ID from a name of the form in section 5.1 of
 * draft-ietf-plants-merkle-tree-certs-05, into a buffer allocated with
 * malloc(3).
 */
int mtc_ca_id_from_name(const X509_NAME *name, uint8_t **out_id,
    size_t *out_id_len);

/*
 * Reads the PEM certificates in in (section 5.5 of
 * draft-ietf-plants-merkle-tree-certs-05), skipping other PEM blocks, and
 * adds a CA for each to cas.  The CA IDs must be distinct from each other
 * and from those already in cas.  On failure nothing is added.
 */
int mtc_ca_parse_certificates(BIO *in, STACK_OF(OSSL_MTC_CA) *cas);

/*
 * Verification of a Merkle Tree Certificate, per section 7.2 of
 * draft-ietf-plants-merkle-tree-certs-05.
 */

/* Whether cert's signature algorithm is id-alg-mtcProof. */
int mtc_is_mtc(const X509 *cert);

/*
 * The CA in cas whose ID is cert's issuer name.  *error is set to
 * X509_V_ERR_MTC_NOT_MTC when the issuer is not a CA ID and to
 * X509_V_ERR_MTC_UNTRUSTED_CA when no CA has that ID.
 */
struct mtc_ca *mtc_ca_for_cert(STACK_OF(OSSL_MTC_CA) *cas, const X509 *cert,
    int *error);

/*
 * Checks cert's proof against ca: it must lead to a trusted subtree or carry
 * a valid cosignature by the CA cosigner.  Returns 1 with *error X509_V_OK,
 * or 0 with *error an X509_V_ERR_MTC_* code.  The certificate's validity,
 * names and extensions are not checked.
 */
int mtc_verify(struct mtc_ca *ca, X509 *cert, int *error);

/* The MerkleTreeCertEntry of a TBSCertificate (5.2.1). */
int mtc_cert_entry(const EVP_MD *md, const uint8_t *tbs, size_t tbs_len,
    const CBS *extensions, uint8_t **out, size_t *out_len);

/* The CosignedMessage a cosigner signs (5.3.1). */
int mtc_cosigned_message(const uint8_t *cosigner_id, size_t cosigner_id_len,
    const uint8_t *ca_id, size_t ca_id_len, uint16_t log_number,
    uint64_t start, uint64_t end, const uint8_t *subtree_hash,
    size_t subtree_hash_len, uint8_t **out, size_t *out_len);

/*
 * Revocation by serial number, per section 7.5 of
 * draft-ietf-plants-merkle-tree-certs-05.  A serial is a log number in the
 * top 16 bits and a log index in the low 48.  Serials below min_serial and
 * above max_serial (UINT64_MAX until set) are revoked, as is any serial in
 * an added half-open range [start, end), which must be non-empty.  Ranges
 * that overlap or abut are merged as they are added.
 */
uint64_t mtc_serial(uint16_t log_number, uint64_t index);
int mtc_ca_add_revoked_range(struct mtc_ca *ca, uint64_t start, uint64_t end);
int mtc_ca_set_max_serial(struct mtc_ca *ca, uint64_t max_serial);
int mtc_ca_serial_is_revoked(struct mtc_ca *ca, uint64_t serial);

/*
 * Trusted subtrees, per sections 6.4.3 and 7.4 of
 * draft-ietf-plants-merkle-tree-certs-05.
 *
 * mtc_ca_load_landmarks() reads a log's published landmark description in
 * the section 6.4.3 format and replaces that log's active window with the
 * subtrees covering its active landmarks.  A subtree still active keeps a
 * hash already added to it.  The CA is unchanged on failure.
 *
 * mtc_ca_add_subtree_hash() records the hash of an active subtree.  It
 * fails if the subtree is not active, if hash_len is not the CA hash's
 * size, or if a different hash is already recorded; the same hash again
 * succeeds.
 *
 * mtc_ca_trusted_subtree_matches() sets *out_found when the subtree is
 * active and returns 1 when it also has a hash equal to hash.
 */
int mtc_ca_load_landmarks(struct mtc_ca *ca, uint64_t log_number, BIO *in);
int mtc_ca_add_subtree_hash(struct mtc_ca *ca, uint64_t log_number,
    struct mtc_subtree subtree, const uint8_t *hash, size_t hash_len);
int mtc_ca_trusted_subtree_matches(struct mtc_ca *ca, uint64_t log_number,
    struct mtc_subtree subtree, const uint8_t *hash, size_t hash_len,
    int *out_found);

__END_HIDDEN_DECLS

#endif /* !HEADER_MTC_INTERNAL_H */
