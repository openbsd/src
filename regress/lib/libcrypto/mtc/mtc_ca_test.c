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

#include <openssl/bio.h>
#include <openssl/evp.h>

#include "mtc_internal.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

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

int
main(void)
{
	int failed = 0;

	failed |= test_ca_roundtrip();
	failed |= test_ca_add_cosigners();
	failed |= test_ca_add_cosigner_duplicate();
	failed |= test_ca_revoked_serials();
	failed |= test_serial();
	failed |= test_ca_landmarks();
	failed |= test_ca_landmarks_bad();
	failed |= test_ca_stack();
	failed |= test_reloid_from_text();
	mtc_ca_free(NULL);

	return failed;
}
