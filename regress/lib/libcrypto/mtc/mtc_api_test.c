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
#include <string.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/mtc.h>

/* The CA ID 32473.1 and a cosigner ID 32473.0. */
static const uint8_t ca_id[] = { 0x81, 0xfd, 0x59, 0x01 };
static const uint8_t cosigner_id[] = { 0x81, 0xfd, 0x59, 0x00 };

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

static int
load_landmarks(OSSL_MTC_CA *ca, uint64_t log_number, const char *desc)
{
	BIO *bio;
	int ret;

	if ((bio = BIO_new_mem_buf(desc, -1)) == NULL)
		errx(1, "BIO_new_mem_buf");
	ret = OSSL_MTC_CA_load_landmarks(ca, log_number, bio);
	BIO_free(bio);

	return ret;
}

/*
 * A CA is created and its ID read back; a cosigner is added once; revoked
 * ranges and max_serial are set; landmarks are loaded and a subtree hash
 * is recorded for an active subtree only.
 */
static int
test_ca_api(void)
{
	const uint8_t hash[32] = { 0x5a };
	OSSL_MTC_CA *ca;
	EVP_PKEY *ca_key, *cosigner_key;
	const uint8_t *id;
	size_t id_len;
	int failed = 0;

	ca_key = gen_key();
	cosigner_key = gen_key();

	if (OSSL_MTC_CA_new(ca_id, 0, EVP_sha256(), 0, ca_key) != NULL) {
		warnx("empty CA ID accepted");
		failed = 1;
	}
	if ((ca = OSSL_MTC_CA_new(ca_id, sizeof(ca_id), EVP_sha256(), 5,
	    ca_key)) == NULL)
		errx(1, "OSSL_MTC_CA_new");

	if (!OSSL_MTC_CA_get0_id(ca, &id, &id_len)) {
		warnx("OSSL_MTC_CA_get0_id failed");
		failed = 1;
	} else if (id_len != sizeof(ca_id) ||
	    memcmp(id, ca_id, sizeof(ca_id)) != 0) {
		warnx("CA ID mismatch");
		failed = 1;
	}

	if (!OSSL_MTC_CA_add1_cosigner(ca, cosigner_id, sizeof(cosigner_id),
	    "Ed25519", cosigner_key)) {
		warnx("add1_cosigner failed");
		failed = 1;
	}
	if (OSSL_MTC_CA_add1_cosigner(ca, cosigner_id, sizeof(cosigner_id),
	    "Ed25519", cosigner_key)) {
		warnx("duplicate cosigner accepted");
		failed = 1;
	}
	if (OSSL_MTC_CA_add1_cosigner(ca, ca_id, sizeof(ca_id), "Ed25519",
	    cosigner_key)) {
		warnx("CA ID accepted as cosigner");
		failed = 1;
	}

	if (!OSSL_MTC_CA_add_revoked_range(ca, 10, 20)) {
		warnx("add_revoked_range failed");
		failed = 1;
	}
	if (OSSL_MTC_CA_add_revoked_range(ca, 20, 20)) {
		warnx("empty revoked range accepted");
		failed = 1;
	}
	if (!OSSL_MTC_CA_set_max_serial(ca, OSSL_MTC_serial(1, 200))) {
		warnx("set_max_serial failed");
		failed = 1;
	}

	if (!load_landmarks(ca, 1, "3 2\n8\n6\n3\n")) {
		warnx("load_landmarks failed");
		failed = 1;
	}
	if (load_landmarks(ca, 1, "3 2\n8\n6\n")) {
		warnx("truncated landmarks accepted");
		failed = 1;
	}
	if (!OSSL_MTC_CA_add_subtree_hash(ca, 1, 6, 7, hash, sizeof(hash))) {
		warnx("add_subtree_hash to active subtree failed");
		failed = 1;
	}
	if (OSSL_MTC_CA_add_subtree_hash(ca, 1, 0, 8, hash, sizeof(hash))) {
		warnx("add_subtree_hash to inactive subtree accepted");
		failed = 1;
	}
	if (OSSL_MTC_CA_add_subtree_hash(ca, 1, 6, 7, hash, 20)) {
		warnx("add_subtree_hash with wrong length accepted");
		failed = 1;
	}

	OSSL_MTC_CA_free(ca);
	OSSL_MTC_CA_free(NULL);
	EVP_PKEY_free(ca_key);
	EVP_PKEY_free(cosigner_key);

	return failed;
}

/* CAs compare by ID, shorter first, then bytewise; a serial packs its parts. */
static int
test_cmp_and_serial(void)
{
	const uint8_t long_id[] = { 0x81, 0xfd, 0x59, 0x00, 0x00 };
	OSSL_MTC_CA *a, *b, *c;
	const OSSL_MTC_CA *pa, *pb, *pc;
	EVP_PKEY *key;
	int failed = 0;

	key = gen_key();
	if ((a = OSSL_MTC_CA_new(cosigner_id, sizeof(cosigner_id),
	    EVP_sha256(), 0, key)) == NULL ||
	    (b = OSSL_MTC_CA_new(ca_id, sizeof(ca_id), EVP_sha256(), 0,
	    key)) == NULL ||
	    (c = OSSL_MTC_CA_new(long_id, sizeof(long_id), EVP_sha256(), 0,
	    key)) == NULL)
		errx(1, "OSSL_MTC_CA_new");
	pa = a;
	pb = b;
	pc = c;

	if (OSSL_MTC_CA_cmp(&pa, &pb) >= 0 || OSSL_MTC_CA_cmp(&pb, &pc) >= 0 ||
	    OSSL_MTC_CA_cmp(&pa, &pa) != 0) {
		warnx("OSSL_MTC_CA_cmp ordering wrong");
		failed = 1;
	}

	if (OSSL_MTC_serial(0, 0) != 0 ||
	    OSSL_MTC_serial(1, 0) != (UINT64_C(1) << 48) ||
	    OSSL_MTC_serial(0xffff, 0xffffffffffff) != UINT64_MAX) {
		warnx("OSSL_MTC_serial wrong");
		failed = 1;
	}

	OSSL_MTC_CA_free(a);
	OSSL_MTC_CA_free(b);
	OSSL_MTC_CA_free(c);
	EVP_PKEY_free(key);

	return failed;
}

int
main(void)
{
	int failed = 0;

	failed |= test_ca_api();
	failed |= test_cmp_and_serial();

	return failed;
}
