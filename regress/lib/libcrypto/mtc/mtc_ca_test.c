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

#include <openssl/evp.h>

#include "mtc_internal.h"

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

int
main(void)
{
	int failed = 0;

	failed |= test_ca_roundtrip();
	failed |= test_ca_add_cosigners();
	failed |= test_ca_add_cosigner_duplicate();
	failed |= test_ca_revoked_serials();
	failed |= test_serial();
	mtc_ca_free(NULL);

	return failed;
}
