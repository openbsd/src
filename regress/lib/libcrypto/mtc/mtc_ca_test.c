/*	$OpenBSD$ */
/*
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include "mtc_internal.h"
#include "x509_local.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

/*
 * mtc-leaf-cosigned.pem and mtc-leaf-landmark.pem are the same Ed25519
 * leaf (key mtc-leaf-key.pem) at index 6 of an eight-entry log 1 of the CA
 * in mtc-ca-cert.pem, whose other entries are 32 bytes of the entry index.
 * The first carries a proof into [0, 8) cosigned with mtc-ca-key.pem, the
 * second a proof into [6, 8) with no cosignatures; its -landmarks.txt and
 * -subtrees.txt hold the landmark description and the [6, 8) hash that
 * make it trusted.  mtc-landmark.pem and its two .txt files are
 * BoringSSL's pki/testdata/path_builder_unittest/mtc_plants04 vector of
 * the same shape (ML-DSA-44 leaf, proof into [0, 4) of an eight-entry log),
 * as are the mtc-leaf-standalone*.pem certificates, which are cosigned
 * by an ML-DSA-44 key.
 */

static const char *certs_dir;

/* The CA ID 32473.1 and two more cosigner IDs, 32473.0 and 32473.2. */
static const uint8_t ca_id[] = { 0x81, 0xfd, 0x59, 0x01 };
static const uint8_t cosigner0_id[] = { 0x81, 0xfd, 0x59, 0x00 };
static const uint8_t cosigner2_id[] = { 0x81, 0xfd, 0x59, 0x02 };

static EVP_PKEY *
gen_key(void)
{
	EVP_PKEY_CTX *ctx;
	EVP_PKEY *pkey = NULL;

	if ((ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL)) == NULL)
		errx(1, "EVP_PKEY_CTX_new_id");
	if (EVP_PKEY_keygen_init(ctx) <= 0)
		errx(1, "EVP_PKEY_keygen_init");
	if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
		errx(1, "EVP_PKEY_keygen");
	EVP_PKEY_CTX_free(ctx);

	return pkey;
}

static struct mtc_ca *
new_ca(EVP_PKEY *pkey, uint64_t min_serial)
{
	struct mtc_ca *ca;

	if ((ca = mtc_ca_new(ca_id, sizeof(ca_id), EVP_sha256(), min_serial,
	    pkey)) == NULL)
		errx(1, "mtc_ca_new");

	return ca;
}

/*
 * Each configured field is retrievable, and the cosigner key survives the
 * caller dropping its reference.
 */
static int
test_ca_roundtrip(void)
{
	struct mtc_ca *ca;
	EVP_PKEY *pkey;
	const uint8_t *id;
	size_t id_len;
	int failed = 0;

	pkey = gen_key();
	ca = new_ca(pkey, 5);
	EVP_PKEY_free(pkey);

	id = mtc_ca_id(ca, &id_len);
	if (id_len != sizeof(ca_id) || memcmp(id, ca_id, id_len) != 0) {
		warnx("CA ID mismatch");
		failed = 1;
	}
	if (mtc_ca_hash(ca) != EVP_sha256()) {
		warnx("hash mismatch");
		failed = 1;
	}
	if (EVP_PKEY_id(mtc_ca_cosigner_pkey(ca)) != EVP_PKEY_ED25519) {
		warnx("cosigner key type mismatch");
		failed = 1;
	}
	if (ca->min_serial != 5) {
		warnx("min_serial %llu, want 5", ca->min_serial);
		failed = 1;
	}
	if (mtc_ca_new(ca_id, 0, EVP_sha256(), 0, mtc_ca_cosigner_pkey(ca)) !=
	    NULL) {
		warnx("empty CA ID accepted");
		failed = 1;
	}

	mtc_ca_free(ca);

	return failed;
}

static size_t
cosigner_count(const struct mtc_ca *ca)
{
	struct mtc_cosigner *c;
	size_t n = 0;

	SLIST_FOREACH(c, &ca->cosigners, entry)
		n++;

	return n;
}

static const struct mtc_cosigner *
find_cosigner(const struct mtc_ca *ca, const uint8_t *id, size_t id_len)
{
	struct mtc_cosigner *c;

	SLIST_FOREACH(c, &ca->cosigners, entry) {
		if (c->id_len == id_len && memcmp(c->id, id, id_len) == 0)
			return c;
	}

	return NULL;
}

/* Two distinct cosigners are stored with their IDs and keys. */
static int
test_ca_add_cosigners(void)
{
	struct mtc_ca *ca;
	const struct mtc_cosigner *c;
	EVP_PKEY *ca_key, *k0, *k2;
	int failed = 0;

	ca_key = gen_key();
	k0 = gen_key();
	k2 = gen_key();
	ca = new_ca(ca_key, 0);

	if (!mtc_ca_add_cosigner(ca, cosigner0_id, sizeof(cosigner0_id), k0)) {
		warnx("add cosigner 0 failed");
		failed = 1;
	}
	if (!mtc_ca_add_cosigner(ca, cosigner2_id, sizeof(cosigner2_id), k2)) {
		warnx("add cosigner 2 failed");
		failed = 1;
	}
	if (cosigner_count(ca) != 2) {
		warnx("cosigner count %zu, want 2", cosigner_count(ca));
		failed = 1;
	}
	if ((c = find_cosigner(ca, cosigner0_id, sizeof(cosigner0_id))) ==
	    NULL || c->pkey != k0) {
		warnx("cosigner 0 stored wrongly");
		failed = 1;
	}
	if ((c = find_cosigner(ca, cosigner2_id, sizeof(cosigner2_id))) ==
	    NULL || c->pkey != k2) {
		warnx("cosigner 2 stored wrongly");
		failed = 1;
	}

	mtc_ca_free(ca);
	EVP_PKEY_free(ca_key);
	EVP_PKEY_free(k0);
	EVP_PKEY_free(k2);

	return failed;
}

/* A repeated, empty or CA-owned ID is rejected, leaving the list as is. */
static int
test_ca_add_cosigner_duplicate(void)
{
	struct mtc_ca *ca;
	EVP_PKEY *ca_key, *k0;
	int failed = 0;

	ca_key = gen_key();
	k0 = gen_key();
	ca = new_ca(ca_key, 0);

	if (!mtc_ca_add_cosigner(ca, cosigner0_id, sizeof(cosigner0_id), k0)) {
		warnx("add cosigner 0 failed");
		failed = 1;
	}
	if (mtc_ca_add_cosigner(ca, cosigner0_id, sizeof(cosigner0_id), k0)) {
		warnx("duplicate cosigner accepted");
		failed = 1;
	}
	if (mtc_ca_add_cosigner(ca, ca_id, sizeof(ca_id), k0)) {
		warnx("CA ID accepted as cosigner");
		failed = 1;
	}
	if (mtc_ca_add_cosigner(ca, ca_id, 0, k0)) {
		warnx("empty cosigner ID accepted");
		failed = 1;
	}
	if (cosigner_count(ca) != 1) {
		warnx("cosigner count %zu, want 1", cosigner_count(ca));
		failed = 1;
	}

	mtc_ca_free(ca);
	EVP_PKEY_free(ca_key);
	EVP_PKEY_free(k0);

	return failed;
}

static int
check_revoked(struct mtc_ca *ca, uint64_t serial, int want)
{
	int got;

	if ((got = mtc_ca_serial_is_revoked(ca, serial)) != want) {
		warnx("serial %llu revoked %d, want %d", serial, got, want);
		return 1;
	}

	return 0;
}

/*
 * Serials below min_serial are revoked, added half-open ranges are honoured
 * at both boundaries whether they overlap, abut or contain earlier ones,
 * empty and inverted ranges are rejected, and setting max_serial revokes
 * everything above it.
 */
static int
test_ca_revoked_serials(void)
{
	struct mtc_ca *ca;
	EVP_PKEY *ca_key;
	int failed = 0;

	ca_key = gen_key();
	ca = new_ca(ca_key, 5);

	failed |= check_revoked(ca, 0, 1);
	failed |= check_revoked(ca, 4, 1);
	failed |= check_revoked(ca, 5, 0);
	failed |= check_revoked(ca, UINT64_MAX, 0);

	if (!mtc_ca_add_revoked_range(ca, 10, 20)) {
		warnx("add revoked range [10, 20) failed");
		failed = 1;
	}
	failed |= check_revoked(ca, 9, 0);
	failed |= check_revoked(ca, 10, 1);
	failed |= check_revoked(ca, 19, 1);
	failed |= check_revoked(ca, 20, 0);

	if (!mtc_ca_add_revoked_range(ca, 100, 101)) {
		warnx("add revoked range [100, 101) failed");
		failed = 1;
	}
	failed |= check_revoked(ca, 100, 1);
	failed |= check_revoked(ca, 101, 0);

	if (mtc_ca_add_revoked_range(ca, 30, 30)) {
		warnx("empty revoked range accepted");
		failed = 1;
	}
	if (mtc_ca_add_revoked_range(ca, 40, 30)) {
		warnx("inverted revoked range accepted");
		failed = 1;
	}
	failed |= check_revoked(ca, 30, 0);
	failed |= check_revoked(ca, 35, 0);

	if (!mtc_ca_add_revoked_range(ca, 15, 25)) {
		warnx("add revoked range [15, 25) failed");
		failed = 1;
	}
	if (!mtc_ca_add_revoked_range(ca, 25, 30)) {
		warnx("add revoked range [25, 30) failed");
		failed = 1;
	}
	if (!mtc_ca_add_revoked_range(ca, 5, 10)) {
		warnx("add revoked range [5, 10) failed");
		failed = 1;
	}
	failed |= check_revoked(ca, 5, 1);
	failed |= check_revoked(ca, 9, 1);
	failed |= check_revoked(ca, 24, 1);
	failed |= check_revoked(ca, 25, 1);
	failed |= check_revoked(ca, 29, 1);
	failed |= check_revoked(ca, 30, 0);
	failed |= check_revoked(ca, 99, 0);
	failed |= check_revoked(ca, 100, 1);

	if (!mtc_ca_add_revoked_range(ca, 50, 60)) {
		warnx("add revoked range [50, 60) failed");
		failed = 1;
	}
	if (!mtc_ca_add_revoked_range(ca, 70, 80)) {
		warnx("add revoked range [70, 80) failed");
		failed = 1;
	}
	if (!mtc_ca_add_revoked_range(ca, 40, 150)) {
		warnx("add revoked range [40, 150) failed");
		failed = 1;
	}
	failed |= check_revoked(ca, 39, 0);
	failed |= check_revoked(ca, 40, 1);
	failed |= check_revoked(ca, 65, 1);
	failed |= check_revoked(ca, 149, 1);
	failed |= check_revoked(ca, 150, 0);

	failed |= check_revoked(ca, mtc_serial(1, 200), 0);
	if (!mtc_ca_set_max_serial(ca, mtc_serial(1, 200))) {
		warnx("set max_serial failed");
		failed = 1;
	}
	failed |= check_revoked(ca, mtc_serial(1, 200), 0);
	failed |= check_revoked(ca, mtc_serial(1, 201), 1);
	failed |= check_revoked(ca, mtc_serial(2, 0), 1);

	mtc_ca_free(ca);
	EVP_PKEY_free(ca_key);

	return failed;
}

static int
test_serial(void)
{
	int failed = 0;

	if (mtc_serial(0, 0) != 0 || mtc_serial(0, 7) != 7 ||
	    mtc_serial(1, 0) != (UINT64_C(1) << 48) ||
	    mtc_serial(0xffff, 0xffffffffffff) != UINT64_MAX) {
		warnx("mtc_serial");
		failed = 1;
	}

	return failed;
}

static struct mtc_subtree
subtree(uint64_t start, uint64_t end)
{
	struct mtc_subtree s = { start, end };

	return s;
}

static int
load_landmarks(struct mtc_ca *ca, uint64_t log_number, const char *text)
{
	BIO *bio;
	int ret;

	if ((bio = BIO_new_mem_buf(text, -1)) == NULL)
		errx(1, "BIO_new_mem_buf");
	ret = mtc_ca_load_landmarks(ca, log_number, bio);
	BIO_free(bio);

	return ret;
}

static int
check_match(struct mtc_ca *ca, uint64_t log_number, struct mtc_subtree s,
    const uint8_t *hash, int want_found, int want_match)
{
	int found, match;

	match = mtc_ca_trusted_subtree_matches(ca, log_number, s, hash, 32,
	    &found);
	if (found != want_found || match != want_match) {
		warnx("subtree [%llu, %llu): found %d match %d, "
		    "want %d %d", s.start, s.end, found, match, want_found,
		    want_match);
		return 1;
	}

	return 0;
}

/*
 * A landmark description installs the subtrees covering its active
 * landmarks; adding a hash makes one usable; a later description keeps the
 * hash of a subtree still active and drops the ones that aged out.
 */
static int
test_ca_landmarks(void)
{
	const uint8_t hash[32] = { 0x5a };
	const uint8_t other[32] = { 0xa5 };
	struct mtc_ca *ca;
	EVP_PKEY *ca_key;
	int failed = 0;

	ca_key = gen_key();
	ca = new_ca(ca_key, 0);

	/*
	 * Landmarks 3 and 2 active with tree sizes 8, 6, 3: landmark 3 covers
	 * [6, 8) and landmark 2 covers [3, 6), giving the subtrees [6, 7),
	 * [7, 8), [3, 4) and [4, 6).
	 */
	if (!load_landmarks(ca, 1, "3 2\n8\n6\n3\n")) {
		warnx("load_landmarks failed");
		failed = 1;
		goto done;
	}
	failed |= check_match(ca, 1, subtree(7, 8), hash, 1, 0);
	failed |= check_match(ca, 1, subtree(4, 6), hash, 1, 0);
	failed |= check_match(ca, 1, subtree(0, 2), hash, 0, 0);
	failed |= check_match(ca, 2, subtree(7, 8), hash, 0, 0);

	if (mtc_ca_add_subtree_hash(ca, 1, subtree(0, 2), hash, 32)) {
		warnx("hash added to inactive subtree");
		failed = 1;
	}
	if (mtc_ca_add_subtree_hash(ca, 1, subtree(7, 8), hash, 16)) {
		warnx("hash of wrong length added");
		failed = 1;
	}
	if (!mtc_ca_add_subtree_hash(ca, 1, subtree(7, 8), hash, 32)) {
		warnx("add_subtree_hash failed");
		failed = 1;
	}
	failed |= check_match(ca, 1, subtree(7, 8), hash, 1, 1);
	failed |= check_match(ca, 1, subtree(7, 8), other, 1, 0);

	if (!mtc_ca_add_subtree_hash(ca, 1, subtree(7, 8), hash, 32)) {
		warnx("re-adding the same hash failed");
		failed = 1;
	}
	if (mtc_ca_add_subtree_hash(ca, 1, subtree(7, 8), other, 32)) {
		warnx("a different hash replaced the recorded one");
		failed = 1;
	}

	/*
	 * Landmarks 4 and 3 active with tree sizes 10, 8, 6: [7, 8) stays and
	 * keeps its hash, [3, 4) and [4, 6) age out.
	 */
	if (!load_landmarks(ca, 1, "4 2\n10\n8\n6\n")) {
		warnx("second load_landmarks failed");
		failed = 1;
		goto done;
	}
	failed |= check_match(ca, 1, subtree(7, 8), hash, 1, 1);
	failed |= check_match(ca, 1, subtree(9, 10), hash, 1, 0);
	failed |= check_match(ca, 1, subtree(4, 6), hash, 0, 0);
	if (mtc_ca_add_subtree_hash(ca, 1, subtree(4, 6), hash, 32)) {
		warnx("hash added to aged-out subtree");
		failed = 1;
	}

	if (!load_landmarks(ca, 2, "18446744073709551615 1\n"
	    "281474976710656\n281474976710655\n")) {
		warnx("load_landmarks with maximal values failed");
		failed = 1;
		goto done;
	}
	failed |= check_match(ca, 2, subtree(MTC_MAX_TREE_SIZE - 1,
	    MTC_MAX_TREE_SIZE), hash, 1, 0);

 done:
	mtc_ca_free(ca);
	EVP_PKEY_free(ca_key);

	return failed;
}

/* Malformed landmark descriptions are rejected and install nothing. */
static int
test_ca_landmarks_bad(void)
{
	const char *bad[] = {
		"garbage\n",
		"3\n",
		"2 3\n8\n6\n3\n",
		"3 2\n8\n6\n",
		"3 2\n8\n6\n6\n",
		"3 2\n8\n6\n3\nextra\n",
		"3 2\n8\n6\n3",
		"-3 2\n8\n6\n3\n",
		"+3 2\n8\n6\n3\n",
		" 3 2\n8\n6\n3\n",
		"3 2\n8\n6\n3 \n",
		"18446744073709551616 2\n8\n6\n3\n",
		"3 2\n281474976710657\n6\n3\n",
	};
	const uint8_t hash[32] = { 0 };
	struct mtc_ca *ca;
	EVP_PKEY *ca_key;
	size_t i;
	int failed = 0;

	ca_key = gen_key();
	ca = new_ca(ca_key, 0);

	for (i = 0; i < nitems(bad); i++) {
		if (load_landmarks(ca, 1, bad[i])) {
			warnx("bad landmarks %zu accepted", i);
			failed = 1;
		}
	}
	failed |= check_match(ca, 1, subtree(7, 8), hash, 0, 0);

	mtc_ca_free(ca);
	EVP_PKEY_free(ca_key);

	return failed;
}

/*
 * A stack of CAs orders them by ID, shorter first, finds each by its ID,
 * rejects a second CA with the same ID and finds nothing for an unknown ID.
 */
static int
test_ca_stack(void)
{
	const uint8_t long_id[] = { 0x81, 0xfd, 0x59, 0x00, 0x01 };
	const uint8_t unknown_id[] = { 0x81, 0xfd, 0x59, 0x03 };
	const uint8_t *ids[] = { ca_id, cosigner0_id, cosigner2_id, long_id };
	const size_t id_lens[] = { sizeof(ca_id), sizeof(cosigner0_id),
	    sizeof(cosigner2_id), sizeof(long_id) };
	const size_t want_order[] = { 1, 0, 2, 3 };
	STACK_OF(OSSL_MTC_CA) *cas;
	struct mtc_ca *ca[nitems(ids)], *dup;
	EVP_PKEY *key;
	size_t i;
	int failed = 0;

	key = gen_key();
	for (i = 0; i < nitems(ids); i++) {
		if ((ca[i] = mtc_ca_new(ids[i], id_lens[i], EVP_sha256(), 0,
		    key)) == NULL)
			errx(1, "mtc_ca_new");
	}
	if ((dup = mtc_ca_new(ca_id, sizeof(ca_id), EVP_sha256(), 0, key)) ==
	    NULL)
		errx(1, "mtc_ca_new");
	if ((cas = sk_OSSL_MTC_CA_new(mtc_ca_cmp)) == NULL)
		errx(1, "sk_OSSL_MTC_CA_new");

	if (mtc_ca_stack_lookup(cas, ca_id, sizeof(ca_id)) != NULL) {
		warnx("lookup in empty stack found a CA");
		failed = 1;
	}
	for (i = 0; i < nitems(ids); i++) {
		if (!mtc_ca_stack_add(cas, ca[i])) {
			warnx("adding CA %zu failed", i);
			failed = 1;
		}
	}
	if (mtc_ca_stack_add(cas, dup)) {
		warnx("duplicate CA ID accepted");
		failed = 1;
	}
	if ((size_t)sk_OSSL_MTC_CA_num(cas) != nitems(ids)) {
		warnx("stack holds %d CAs, want %zu", sk_OSSL_MTC_CA_num(cas),
		    nitems(ids));
		failed = 1;
		goto done;
	}
	sk_OSSL_MTC_CA_sort(cas);
	for (i = 0; i < nitems(ids); i++) {
		if (sk_OSSL_MTC_CA_value(cas, i) != ca[want_order[i]]) {
			warnx("CA at position %zu out of order", i);
			failed = 1;
		}
	}
	for (i = 0; i < nitems(ids); i++) {
		if (mtc_ca_stack_lookup(cas, ids[i], id_lens[i]) != ca[i]) {
			warnx("lookup of CA %zu failed", i);
			failed = 1;
		}
	}
	if (mtc_ca_stack_lookup(cas, unknown_id, sizeof(unknown_id)) != NULL) {
		warnx("lookup of unknown ID found a CA");
		failed = 1;
	}

 done:
	sk_OSSL_MTC_CA_free(cas);
	for (i = 0; i < nitems(ids); i++)
		mtc_ca_free(ca[i]);
	mtc_ca_free(dup);
	EVP_PKEY_free(key);

	return failed;
}

/*
 * Dotted-decimal IDs encode to relative-OID content octets; empty
 * strings, empty components, non-digits and components above UINT64_MAX
 * are rejected.
 */
static int
test_reloid_from_text(void)
{
	const struct {
		const char *text;
		const uint8_t *want;
		size_t want_len;
	} good[] = {
		{ "32473.1", ca_id, sizeof(ca_id) },
		{ "0", (const uint8_t *)"\x00", 1 },
		{ "127", (const uint8_t *)"\x7f", 1 },
		{ "128", (const uint8_t *)"\x81\x00", 2 },
		{ "0.0.0", (const uint8_t *)"\x00\x00\x00", 3 },
		{ "18446744073709551615",
		    (const uint8_t *)"\x81\xff\xff\xff\xff\xff\xff\xff\xff\x7f",
		    10 },
	};
	const char *bad[] = {
		"", ".", "1.", ".1", "1..2", "1 2", "1a", "-1", "+1",
		"18446744073709551616",
	};
	uint8_t *out;
	size_t out_len, i;
	int failed = 0;

	for (i = 0; i < nitems(good); i++) {
		if (!mtc_reloid_from_text(good[i].text, strlen(good[i].text),
		    &out, &out_len)) {
			warnx("reloid_from_text(\"%s\") failed", good[i].text);
			failed = 1;
			continue;
		}
		if (out_len != good[i].want_len ||
		    memcmp(out, good[i].want, out_len) != 0) {
			warnx("reloid_from_text(\"%s\") encoded wrongly",
			    good[i].text);
			failed = 1;
		}
		free(out);
	}
	for (i = 0; i < nitems(bad); i++) {
		if (mtc_reloid_from_text(bad[i], strlen(bad[i]), &out,
		    &out_len)) {
			warnx("reloid_from_text(\"%s\") accepted", bad[i]);
			failed = 1;
			free(out);
		}
	}

	return failed;
}

/*
 * The CAs a store trusts are those given to X509_STORE_trust_mtc_ca(), and
 * configuring a CA through the caller's pointer is seen through the store.
 */
static int
test_store_mtc_cas(void)
{
	const uint8_t hash[32] = { 0x5a };
	X509_STORE *store;
	STACK_OF(OSSL_MTC_CA) *cas;
	struct mtc_ca *ca, *trusted;
	EVP_PKEY *key;
	int failed = 0;

	key = gen_key();
	if ((ca = mtc_ca_new(ca_id, sizeof(ca_id), EVP_sha256(), 0, key)) ==
	    NULL)
		errx(1, "mtc_ca_new");
	if ((store = X509_STORE_new()) == NULL)
		errx(1, "X509_STORE_new");

	if (x509_store_get0_mtc_cas(store) != NULL) {
		warnx("new store trusts MTC CAs");
		failed = 1;
	}
	if (!X509_STORE_trust_mtc_ca(store, ca)) {
		warnx("X509_STORE_trust_mtc_ca failed");
		failed = 1;
	}
	if ((cas = x509_store_get0_mtc_cas(store)) == NULL ||
	    sk_OSSL_MTC_CA_num(cas) != 1 ||
	    (trusted = mtc_ca_stack_lookup(cas, ca_id, sizeof(ca_id))) !=
	    ca) {
		warnx("trusted CA not in the store's stack");
		failed = 1;
		goto done;
	}

	if (!load_landmarks(ca, 1, "3 2\n8\n6\n3\n") ||
	    !mtc_ca_add_subtree_hash(ca, 1, subtree(6, 7), hash,
	    sizeof(hash))) {
		warnx("configuring the trusted CA failed");
		failed = 1;
	}
	failed |= check_match(trusted, 1, subtree(6, 7), hash, 1, 1);
	failed |= check_match(trusted, 1, subtree(0, 8), hash, 0, 0);

 done:
	X509_STORE_free(store);
	mtc_ca_free(ca);
	EVP_PKEY_free(key);

	return failed;
}

static FILE *
open_fixture(const char *name)
{
	char *path;
	FILE *fp;

	if (asprintf(&path, "%s/%s", certs_dir, name) == -1)
		err(1, "asprintf");
	if ((fp = fopen(path, "r")) == NULL)
		err(1, "%s", path);
	free(path);

	return fp;
}

static X509 *
load_cert(const char *name)
{
	X509 *cert;
	FILE *fp;

	fp = open_fixture(name);
	if ((cert = PEM_read_X509(fp, NULL, NULL, NULL)) == NULL)
		errx(1, "%s: PEM_read_X509", name);
	fclose(fp);

	return cert;
}

/* The CA of mtc-ca-cert.pem, alone on a new stack. */
static STACK_OF(OSSL_MTC_CA) *
load_ca(void)
{
	STACK_OF(OSSL_MTC_CA) *cas;
	BIO *bio;
	FILE *fp;

	if ((cas = sk_OSSL_MTC_CA_new(mtc_ca_cmp)) == NULL)
		errx(1, "sk_OSSL_MTC_CA_new");
	fp = open_fixture("mtc-ca-cert.pem");
	if ((bio = BIO_new_fp(fp, BIO_CLOSE)) == NULL)
		errx(1, "BIO_new_fp");
	if (!mtc_ca_parse_certificates(bio, cas))
		errx(1, "mtc-ca-cert.pem: mtc_ca_parse_certificates");
	BIO_free(bio);
	if (sk_OSSL_MTC_CA_num(cas) != 1)
		errx(1, "mtc-ca-cert.pem: %d CAs", sk_OSSL_MTC_CA_num(cas));

	return cas;
}

static void
free_cas(STACK_OF(OSSL_MTC_CA) *cas)
{
	sk_OSSL_MTC_CA_pop_free(cas, mtc_ca_free);
}

static void
load_landmark_file(struct mtc_ca *ca, uint64_t log_number, const char *name)
{
	BIO *bio;
	FILE *fp;

	fp = open_fixture(name);
	if ((bio = BIO_new_fp(fp, BIO_CLOSE)) == NULL)
		errx(1, "BIO_new_fp");
	if (!mtc_ca_load_landmarks(ca, log_number, bio))
		errx(1, "%s: mtc_ca_load_landmarks", name);
	BIO_free(bio);
}

/*
 * Reads the one line of a subtrees file, "<ca> <log> <start> <end>
 * <base64 hash>", returning the log number and subtree and the hash.
 */
static uint64_t
read_subtree_file(const char *name, struct mtc_subtree *out_subtree,
    uint8_t out_hash[32])
{
	char line[256], ca[64], b64[64];
	uint8_t decoded[48];
	unsigned long long log_number, start, end;
	FILE *fp;

	fp = open_fixture(name);
	if (fgets(line, sizeof(line), fp) == NULL)
		err(1, "%s", name);
	fclose(fp);
	if (sscanf(line, "%63s %llu %llu %llu %63s", ca, &log_number, &start,
	    &end, b64) != 5)
		errx(1, "%s: malformed", name);
	if (strlen(b64) != 44 ||
	    EVP_DecodeBlock(decoded, (const unsigned char *)b64, 44) != 33)
		errx(1, "%s: bad hash", name);
	memcpy(out_hash, decoded, 32);
	out_subtree->start = start;
	out_subtree->end = end;

	return log_number;
}

static void
trust_subtree_file(struct mtc_ca *ca, const char *name)
{
	struct mtc_subtree s;
	uint8_t hash[32];
	uint64_t log_number;

	log_number = read_subtree_file(name, &s, hash);
	if (!mtc_ca_add_subtree_hash(ca, log_number, s, hash, sizeof(hash)))
		errx(1, "%s: mtc_ca_add_subtree_hash", name);
}

static int
check_verify(const char *what, struct mtc_ca *ca, X509 *cert, int want_ret,
    int want_error)
{
	int error = -1, ret;

	ret = mtc_verify(ca, cert, &error);
	if (ret != want_ret || error != want_error) {
		warnx("%s: verify %d error %d (%s), want %d %d", what, ret,
		    error, X509_verify_cert_error_string(error), want_ret,
		    want_error);
		return 1;
	}

	return 0;
}

static int
check_verify_file(const char *name, struct mtc_ca *ca, int want_ret,
    int want_error)
{
	X509 *cert;
	int failed;

	cert = load_cert(name);
	failed = check_verify(name, ca, cert, want_ret, want_error);
	X509_free(cert);

	return failed;
}

/* An MTC is recognised by its signature algorithm alone. */
static int
test_is_mtc(void)
{
	X509 *cert;
	int failed = 0;

	cert = load_cert("mtc-leaf-cosigned.pem");
	if (!mtc_is_mtc(cert)) {
		warnx("mtc-leaf-cosigned.pem not recognised as an MTC");
		failed = 1;
	}
	X509_free(cert);

	cert = load_cert("mtc-ca-cert.pem");
	if (mtc_is_mtc(cert)) {
		warnx("mtc-ca-cert.pem recognised as an MTC");
		failed = 1;
	}
	X509_free(cert);

	return failed;
}

/*
 * The CA for a certificate is the trusted one whose ID is the issuer's
 * trust anchor ID; an issuer that is not one is not an MTC.
 */
static int
test_ca_for_cert(void)
{
	STACK_OF(OSSL_MTC_CA) *cas;
	struct mtc_ca *ca, *other;
	X509_NAME *name;
	X509 *cert;
	EVP_PKEY *key;
	int error, failed = 0;

	cas = load_ca();
	ca = sk_OSSL_MTC_CA_value(cas, 0);

	cert = load_cert("mtc-leaf-cosigned.pem");
	error = -1;
	if (mtc_ca_for_cert(cas, cert, &error) != ca || error != -1) {
		warnx("CA not found for mtc-leaf-cosigned.pem");
		failed = 1;
	}

	key = gen_key();
	if ((other = mtc_ca_new(cosigner2_id, sizeof(cosigner2_id),
	    EVP_sha256(), 0, key)) == NULL)
		errx(1, "mtc_ca_new");
	if (!mtc_ca_stack_add(cas, other))
		errx(1, "mtc_ca_stack_add");
	if (mtc_ca_for_cert(cas, cert, &error) != ca) {
		warnx("CA not found among two");
		failed = 1;
	}
	free_cas(cas);
	if ((cas = sk_OSSL_MTC_CA_new(mtc_ca_cmp)) == NULL)
		errx(1, "sk_OSSL_MTC_CA_new");
	if ((other = mtc_ca_new(cosigner2_id, sizeof(cosigner2_id),
	    EVP_sha256(), 0, key)) == NULL)
		errx(1, "mtc_ca_new");
	if (!mtc_ca_stack_add(cas, other))
		errx(1, "mtc_ca_stack_add");
	error = -1;
	if (mtc_ca_for_cert(cas, cert, &error) != NULL ||
	    error != X509_V_ERR_MTC_UNTRUSTED_CA) {
		warnx("issuer 32473.1 found among CA 32473.2: error %d", error);
		failed = 1;
	}
	X509_free(cert);

	if ((cert = X509_new()) == NULL || (name = X509_NAME_new()) == NULL)
		errx(1, "X509_new");
	if (!X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
	    (const unsigned char *)"Not an MTC", -1, -1, 0))
		errx(1, "X509_NAME_add_entry_by_txt");
	if (!X509_set_issuer_name(cert, name))
		errx(1, "X509_set_issuer_name");
	X509_NAME_free(name);
	error = -1;
	if (mtc_ca_for_cert(cas, cert, &error) != NULL ||
	    error != X509_V_ERR_MTC_NOT_MTC) {
		warnx("CN issuer taken for a trust anchor ID: error %d", error);
		failed = 1;
	}
	X509_free(cert);

	free_cas(cas);
	EVP_PKEY_free(key);

	return failed;
}

/*
 * A cosigned MTC verifies against the CA whose cosigner key signed it and
 * against no other; a revoked serial, an inclusion proof that does not
 * reconstruct the cosigned hash, one that does not fit its subtree and a
 * malformed proof each fail with their own error.
 */
static int
test_verify_cosigned(void)
{
	STACK_OF(OSSL_MTC_CA) *cas;
	struct mtc_ca *ca, *other;
	X509 *cert;
	EVP_PKEY *key;
	int failed = 0;

	cas = load_ca();
	ca = sk_OSSL_MTC_CA_value(cas, 0);
	cert = load_cert("mtc-leaf-cosigned.pem");

	failed |= check_verify("cosigned", ca, cert, 1, X509_V_OK);

	key = gen_key();
	other = new_ca(key, 0);
	failed |= check_verify("cosigned, other cosigner key", other, cert, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	mtc_ca_free(other);
	EVP_PKEY_free(key);

	if (!mtc_ca_add_revoked_range(ca, mtc_serial(1, 6), mtc_serial(1, 7)))
		errx(1, "mtc_ca_add_revoked_range");
	failed |= check_verify("cosigned, revoked", ca, cert, 0,
	    X509_V_ERR_MTC_REVOKED);
	free_cas(cas);
	cas = load_ca();
	ca = sk_OSSL_MTC_CA_value(cas, 0);

	/*
	 * The proof starts with an empty extensions list (2 bytes), start and
	 * end (6 bytes each) and the inclusion_proof length (2 bytes).
	 */
	if (cert->signature->length < 17)
		errx(1, "mtc-leaf-cosigned.pem: short proof");
	cert->signature->data[16] ^= 0x01;
	failed |= check_verify("cosigned, damaged proof hash", ca, cert, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	cert->signature->data[16] ^= 0x01;
	failed |= check_verify("cosigned, proof restored", ca, cert, 1,
	    X509_V_OK);
	if (cert->signature->data[13] != 8)
		errx(1, "mtc-leaf-cosigned.pem: end is not 8");
	cert->signature->data[13] = 16;
	failed |= check_verify("cosigned, proof too short for [0, 16)", ca,
	    cert, 0, X509_V_ERR_MTC_INCLUSION_FAILED);
	cert->signature->data[13] = 8;
	X509_free(cert);

	failed |= check_verify_file("mtc-leaf-standalone-truncated.pem", ca, 0,
	    X509_V_ERR_MTC_BAD_PROOF);
	failed |= check_verify_file("mtc-leaf-standalone-trailing.pem", ca, 0,
	    X509_V_ERR_MTC_BAD_PROOF);

	free_cas(cas);

	return failed;
}

/*
 * A signatureless MTC verifies only through a trusted subtree with the
 * hash its proof reconstructs; a cosigned one still verifies when its
 * subtree is not a trusted one.
 */
static int
test_verify_landmark(void)
{
	const uint8_t wrong_hash[32] = { 0x01 };
	STACK_OF(OSSL_MTC_CA) *cas;
	struct mtc_ca *ca;
	int failed = 0;

	cas = load_ca();
	ca = sk_OSSL_MTC_CA_value(cas, 0);

	failed |= check_verify_file("mtc-leaf-landmark.pem", ca, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	load_landmark_file(ca, 1, "mtc-leaf-landmark-landmarks.txt");
	failed |= check_verify_file("mtc-leaf-landmark.pem", ca, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	trust_subtree_file(ca, "mtc-leaf-landmark-subtrees.txt");
	failed |= check_verify_file("mtc-leaf-landmark.pem", ca, 1, X509_V_OK);
	failed |= check_verify_file("mtc-leaf-cosigned.pem", ca, 1, X509_V_OK);
	free_cas(cas);

	cas = load_ca();
	ca = sk_OSSL_MTC_CA_value(cas, 0);
	load_landmark_file(ca, 1, "mtc-leaf-landmark-landmarks.txt");
	if (!mtc_ca_add_subtree_hash(ca, 1, subtree(6, 8), wrong_hash,
	    sizeof(wrong_hash)))
		errx(1, "mtc_ca_add_subtree_hash");
	failed |= check_verify_file("mtc-leaf-landmark.pem", ca, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	free_cas(cas);

	return failed;
}

/*
 * The BoringSSL vectors: the landmark leaf verifies through its trusted
 * subtree; the standalone leaves are cosigned by a key this CA does not
 * have, so only their cosignature lists are judged.
 */
static int
test_verify_boringssl(void)
{
	STACK_OF(OSSL_MTC_CA) *cas;
	struct mtc_ca *ca;
	int failed = 0;

	cas = load_ca();
	ca = sk_OSSL_MTC_CA_value(cas, 0);

	failed |= check_verify_file("mtc-landmark.pem", ca, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	load_landmark_file(ca, 1, "mtc-landmark-landmarks.txt");
	trust_subtree_file(ca, "mtc-landmark-subtrees.txt");
	failed |= check_verify_file("mtc-landmark.pem", ca, 1, X509_V_OK);

	failed |= check_verify_file("mtc-leaf-standalone.pem", ca, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	failed |= check_verify_file("mtc-leaf-standalone-3cosigners.pem", ca, 0,
	    X509_V_ERR_MTC_NOT_TRUSTED);
	failed |= check_verify_file("mtc-leaf-standalone-no_ca_signer.pem", ca,
	    0, X509_V_ERR_MTC_NOT_TRUSTED);
	failed |= check_verify_file(
	    "mtc-leaf-standalone-duplicate_ca_signer.pem", ca, 0,
	    X509_V_ERR_MTC_BAD_PROOF);
	failed |= check_verify_file(
	    "mtc-leaf-standalone-cosigner_wrong_order.pem", ca, 0,
	    X509_V_ERR_MTC_BAD_PROOF);

	free_cas(cas);

	return failed;
}

/* Times within, before and after the leaves' validity, 2020 to 2035. */
static const time_t valid_time = 1609459200;	/* 2021-01-01T00:00:00Z */
static const time_t early_time = 1262304000;	/* 2010-01-01T00:00:00Z */
static const time_t late_time = 4102444800;	/* 2100-01-01T00:00:00Z */

/*
 * Verifies cert in a context over store at time when, with host and purpose
 * applied when given, reporting the result and ctx->error.
 */
static int
store_verify(X509_STORE *store, X509 *cert, const char *host, time_t when,
    int purpose, int *error)
{
	X509_STORE_CTX *ctx;
	X509_VERIFY_PARAM *param;
	int ret;

	if ((ctx = X509_STORE_CTX_new()) == NULL)
		errx(1, "X509_STORE_CTX_new");
	if (!X509_STORE_CTX_init(ctx, store, cert, NULL))
		errx(1, "X509_STORE_CTX_init");
	param = X509_STORE_CTX_get0_param(ctx);
	X509_VERIFY_PARAM_set_time(param, when);
	if (host != NULL && !X509_VERIFY_PARAM_set1_host(param, host, 0))
		errx(1, "X509_VERIFY_PARAM_set1_host");
	if (purpose != 0 && !X509_VERIFY_PARAM_set_purpose(param, purpose))
		errx(1, "X509_VERIFY_PARAM_set_purpose");

	ret = x509_verify_mtc(ctx);
	*error = X509_STORE_CTX_get_error(ctx);
	if (ret) {
		if (sk_X509_num(X509_STORE_CTX_get0_chain(ctx)) != 1 ||
		    sk_X509_value(X509_STORE_CTX_get0_chain(ctx), 0) != cert) {
			warnx("verified chain is not the leaf alone");
			ret = -1;
		}
		if (X509_STORE_CTX_get_current_cert(ctx) != cert ||
		    X509_STORE_CTX_get_error_depth(ctx) != 0) {
			warnx("current cert or depth not the leaf");
			ret = -1;
		}
	}
	X509_STORE_CTX_free(ctx);

	return ret;
}

static int
check_store_verify(const char *what, X509_STORE *store, X509 *cert,
    const char *host, time_t when, int purpose, int want_ret, int want_error)
{
	int error, ret;

	ret = store_verify(store, cert, host, when, purpose, &error);
	if (ret != want_ret || error != want_error) {
		warnx("%s: x509_verify_mtc %d error %d (%s), want %d %d", what,
		    ret, error, X509_verify_cert_error_string(error), want_ret,
		    want_error);
		return 1;
	}

	return 0;
}

/*
 * An MTC verifies in a store context when its proof verifies against a CA
 * the store trusts and it passes the leaf checks for the parameters given;
 * the verified chain is the leaf alone.
 */
static int
test_x509_verify_mtc(void)
{
	STACK_OF(OSSL_MTC_CA) *cas;
	struct mtc_ca *ca;
	X509_STORE *store;
	X509_STORE_CTX *ctx;
	X509 *cert, *landmark;
	int failed = 0;

	cas = load_ca();
	ca = sk_OSSL_MTC_CA_value(cas, 0);
	if ((store = X509_STORE_new()) == NULL)
		errx(1, "X509_STORE_new");
	cert = load_cert("mtc-leaf-cosigned.pem");
	landmark = load_cert("mtc-landmark.pem");

	failed |= check_store_verify("untrusting store", store, cert, NULL,
	    valid_time, 0, 0, X509_V_ERR_MTC_UNTRUSTED_CA);
	failed |= check_store_verify("no store", NULL, cert, NULL, valid_time,
	    0, 0, X509_V_ERR_MTC_UNTRUSTED_CA);

	if (!X509_STORE_trust_mtc_ca(store, ca))
		errx(1, "X509_STORE_trust_mtc_ca");

	failed |= check_store_verify("cosigned", store, cert, NULL, valid_time,
	    0, 1, X509_V_OK);
	failed |= check_store_verify("cosigned, host", store, cert, "a.example",
	    valid_time, 0, 1, X509_V_OK);
	failed |= check_store_verify("cosigned, other host", store, cert,
	    "b.example", valid_time, 0, 0, X509_V_ERR_HOSTNAME_MISMATCH);
	failed |= check_store_verify("cosigned, not yet valid", store, cert,
	    NULL, early_time, 0, 0, X509_V_ERR_CERT_NOT_YET_VALID);
	failed |= check_store_verify("cosigned, expired", store, cert, NULL,
	    late_time, 0, 0, X509_V_ERR_CERT_HAS_EXPIRED);
	failed |= check_store_verify("cosigned, server", store, cert, NULL,
	    valid_time, X509_PURPOSE_SSL_SERVER, 1, X509_V_OK);
	failed |= check_store_verify("landmark, untrusted", store, landmark,
	    NULL, valid_time, 0, 0, X509_V_ERR_MTC_NOT_TRUSTED);

	load_landmark_file(ca, 1, "mtc-landmark-landmarks.txt");
	trust_subtree_file(ca, "mtc-landmark-subtrees.txt");
	failed |= check_store_verify("landmark", store, landmark, NULL,
	    valid_time, 0, 1, X509_V_OK);
	failed |= check_store_verify("landmark, server", store, landmark, NULL,
	    valid_time, X509_PURPOSE_SSL_SERVER, 1, X509_V_OK);
	failed |= check_store_verify("landmark, client", store, landmark, NULL,
	    valid_time, X509_PURPOSE_SSL_CLIENT, 0, X509_V_ERR_INVALID_PURPOSE);

	if ((ctx = X509_STORE_CTX_new()) == NULL)
		errx(1, "X509_STORE_CTX_new");
	if (!X509_STORE_CTX_init(ctx, store, cert, NULL))
		errx(1, "X509_STORE_CTX_init");
	X509_VERIFY_PARAM_set_time(X509_STORE_CTX_get0_param(ctx), valid_time);
	if (x509_verify_mtc(ctx) != 1 || x509_verify_mtc(ctx) != 0 ||
	    X509_STORE_CTX_get_error(ctx) != X509_V_ERR_INVALID_CALL) {
		warnx("second verification in one context not refused");
		failed = 1;
	}
	X509_STORE_CTX_free(ctx);

	X509_free(landmark);
	X509_free(cert);
	X509_STORE_free(store);
	free_cas(cas);

	return failed;
}

/* Runs the leaf checks alone over cert at valid_time with flags. */
static int
leaf_checks(X509 *cert, unsigned long flags, int *error)
{
	X509_STORE_CTX *ctx;
	X509_VERIFY_PARAM *param;
	int ret;

	if ((ctx = X509_STORE_CTX_new()) == NULL)
		errx(1, "X509_STORE_CTX_new");
	if (!X509_STORE_CTX_init(ctx, NULL, cert, NULL))
		errx(1, "X509_STORE_CTX_init");
	param = X509_STORE_CTX_get0_param(ctx);
	X509_VERIFY_PARAM_set_time(param, valid_time);
	if (!X509_VERIFY_PARAM_set_flags(param, flags))
		errx(1, "X509_VERIFY_PARAM_set_flags");

	ret = mtc_leaf_checks(ctx);
	*error = X509_STORE_CTX_get_error(ctx);
	X509_STORE_CTX_free(ctx);

	return ret;
}

/* The leaf checks leave ctx->error alone when they pass. */
static int
check_leaf(const char *what, X509 *cert, unsigned long flags, int want_ret,
    int want_error)
{
	int error, ret;

	ret = leaf_checks(cert, flags, &error);
	if (ret != want_ret || (ret == 0 && error != want_error)) {
		warnx("%s: leaf checks %d error %d (%s), want %d %d", what, ret,
		    error, X509_verify_cert_error_string(error), want_ret,
		    want_error);
		return 1;
	}

	return 0;
}

/* Adds an extension under 1.3.6.1.4.1.32473.99 (an RFC 5612 example arc). */
static void
add_example_ext(X509 *cert, int critical)
{
	ASN1_OBJECT *obj;
	ASN1_OCTET_STRING *value;
	X509_EXTENSION *ext;

	if ((obj = OBJ_txt2obj("1.3.6.1.4.1.32473.99", 1)) == NULL)
		errx(1, "OBJ_txt2obj");
	if ((value = ASN1_OCTET_STRING_new()) == NULL ||
	    !ASN1_OCTET_STRING_set(value, (const unsigned char *)"!", 1))
		errx(1, "ASN1_OCTET_STRING_set");
	if ((ext = X509_EXTENSION_create_by_OBJ(NULL, obj, critical, value)) ==
	    NULL)
		errx(1, "X509_EXTENSION_create_by_OBJ");
	if (!X509_add_ext(cert, ext, -1))
		errx(1, "X509_add_ext");
	X509_EXTENSION_free(ext);
	ASN1_OCTET_STRING_free(value);
	ASN1_OBJECT_free(obj);
}

/*
 * The leaf checks reject a duplicate extension, an unhandled critical
 * extension unless asked to ignore it, an empty subject alternative name
 * and a TBSCertificate signature algorithm other than the outer one.
 */
static int
test_leaf_checks(void)
{
	GENERAL_NAMES *names;
	X509 *cert;
	int loc, failed = 0;

	cert = load_cert("mtc-leaf-cosigned.pem");
	failed |= check_leaf("as issued", cert, 0, 1, X509_V_OK);
	X509_free(cert);

	cert = load_cert("mtc-leaf-cosigned.pem");
	add_example_ext(cert, 0);
	add_example_ext(cert, 0);
	failed |= check_leaf("duplicate extension", cert, 0, 0,
	    X509_V_ERR_UNSPECIFIED);
	X509_free(cert);

	cert = load_cert("mtc-leaf-cosigned.pem");
	add_example_ext(cert, 1);
	failed |= check_leaf("unhandled critical extension", cert, 0, 0,
	    X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION);
	failed |= check_leaf("ignored critical extension", cert,
	    X509_V_FLAG_IGNORE_CRITICAL, 1, X509_V_OK);
	X509_free(cert);

	cert = load_cert("mtc-leaf-cosigned.pem");
	if ((loc = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1)) < 0)
		errx(1, "mtc-leaf-cosigned.pem: no subject alternative name");
	X509_EXTENSION_free(X509_delete_ext(cert, loc));
	if ((names = sk_GENERAL_NAME_new_null()) == NULL)
		errx(1, "sk_GENERAL_NAME_new_null");
	if (!X509_add1_ext_i2d(cert, NID_subject_alt_name, names, 0,
	    X509V3_ADD_DEFAULT))
		errx(1, "X509_add1_ext_i2d");
	sk_GENERAL_NAME_free(names);
	failed |= check_leaf("empty subject alternative name", cert, 0, 0,
	    X509_V_ERR_INVALID_EXTENSION);
	X509_free(cert);

	cert = load_cert("mtc-leaf-cosigned.pem");
	if (!X509_ALGOR_set0(cert->cert_info->signature,
	    OBJ_nid2obj(NID_ED25519), V_ASN1_UNDEF, NULL))
		errx(1, "X509_ALGOR_set0");
	failed |= check_leaf("inner signature algorithm", cert, 0, 0,
	    X509_V_ERR_MTC_BAD_PROOF);
	X509_free(cert);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: %s certs-dir\n", argv[0]);
		return 1;
	}
	certs_dir = argv[1];

	failed |= test_ca_roundtrip();
	failed |= test_ca_add_cosigners();
	failed |= test_ca_add_cosigner_duplicate();
	failed |= test_ca_revoked_serials();
	failed |= test_serial();
	failed |= test_ca_landmarks();
	failed |= test_ca_landmarks_bad();
	failed |= test_ca_stack();
	failed |= test_reloid_from_text();
	failed |= test_store_mtc_cas();
	failed |= test_is_mtc();
	failed |= test_ca_for_cert();
	failed |= test_verify_cosigned();
	failed |= test_verify_landmark();
	failed |= test_verify_boringssl();
	failed |= test_x509_verify_mtc();
	failed |= test_leaf_checks();
	mtc_ca_free(NULL);

	return failed;
}
