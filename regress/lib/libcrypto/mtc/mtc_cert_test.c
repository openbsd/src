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
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "bytestring.h"
#include "mtc_internal.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

/*
 * The mtc-*.pem fixtures are BoringSSL's
 * pki/testdata/path_builder_unittest/mtc_plants04 certificates; the
 * -trailing and -truncated ones are mtc-leaf-standalone.pem with one byte
 * added to or removed from its MTCProof.
 */

/* The experimentation OID for id-alg-mtcProof used by the fixtures. */
#define MTC_ALG_OID "1.3.6.1.4.1.44363.47.0"

static const char *certs_dir;

/*
 * Load the named certificate, check that its signatureAlgorithm is
 * id-alg-mtcProof, and return it.  The MTCProof is its signatureValue.
 */
static X509 *
load_mtc_cert(const char *name)
{
	const ASN1_BIT_STRING *sig;
	const X509_ALGOR *alg;
	const ASN1_OBJECT *alg_obj;
	ASN1_OBJECT *mtc_obj;
	X509 *cert;
	char *path;
	FILE *fp;

	if (asprintf(&path, "%s/%s", certs_dir, name) == -1)
		err(1, "asprintf");
	if ((fp = fopen(path, "r")) == NULL)
		err(1, "%s", path);
	if ((cert = PEM_read_X509(fp, NULL, NULL, NULL)) == NULL)
		errx(1, "%s: PEM_read_X509", name);
	fclose(fp);
	free(path);

	X509_get0_signature(&sig, &alg, cert);
	X509_ALGOR_get0(&alg_obj, NULL, NULL, alg);
	if ((mtc_obj = OBJ_txt2obj(MTC_ALG_OID, 1)) == NULL)
		errx(1, "OBJ_txt2obj");
	if (OBJ_cmp(alg_obj, mtc_obj) != 0)
		errx(1, "%s: signatureAlgorithm is not id-alg-mtcProof", name);
	ASN1_OBJECT_free(mtc_obj);

	return cert;
}

static void
get_proof_bytes(const X509 *cert, const uint8_t **out, size_t *out_len)
{
	const ASN1_BIT_STRING *sig;

	X509_get0_signature(&sig, NULL, cert);
	*out = ASN1_STRING_get0_data(sig);
	*out_len = ASN1_STRING_length(sig);
}

struct cosig_expect {
	const uint8_t *id;
	size_t id_len;
	size_t sig_len;
};

/*
 * A certificate whose MTCProof is well-formed: check the decoded fields and
 * that the cosignatures are exactly those expected.
 */
static int
check_valid(const char *name, uint64_t start, uint64_t end, size_t ext_len,
    size_t incl_len, const struct cosig_expect *exp, size_t nexp)
{
	struct mtc_cosignature cosig[8];
	struct mtc_proof proof;
	const uint8_t *data;
	size_t len, count, i;
	X509 *cert;
	int failed = 1;

	cert = load_mtc_cert(name);
	get_proof_bytes(cert, &data, &len);

	if (!mtc_proof_parse(data, len, &proof)) {
		warnx("%s: parse failed", name);
		goto err;
	}
	if (proof.start != start || proof.end != end) {
		warnx("%s: subtree [%llu, %llu), want [%llu, %llu)", name,
		    proof.start, proof.end, start, end);
		goto err;
	}
	if (CBS_len(&proof.extensions) != ext_len) {
		warnx("%s: extensions length %zu, want %zu", name,
		    CBS_len(&proof.extensions), ext_len);
		goto err;
	}
	if (CBS_len(&proof.inclusion_proof) != incl_len) {
		warnx("%s: inclusion proof length %zu, want %zu", name,
		    CBS_len(&proof.inclusion_proof), incl_len);
		goto err;
	}

	if (!mtc_proof_get_cosignatures(&proof, cosig, nitems(cosig),
	    &count)) {
		warnx("%s: get_cosignatures failed", name);
		goto err;
	}
	if (count != nexp) {
		warnx("%s: %zu cosignatures, want %zu", name, count, nexp);
		goto err;
	}
	for (i = 0; i < nexp; i++) {
		if (!CBS_mem_equal(&cosig[i].cosigner_id, exp[i].id,
		    exp[i].id_len)) {
			warnx("%s: cosigner %zu id mismatch", name, i);
			goto err;
		}
		if (CBS_len(&cosig[i].signature) != exp[i].sig_len) {
			warnx("%s: cosigner %zu signature length %zu, "
			    "want %zu", name, i, CBS_len(&cosig[i].signature),
			    exp[i].sig_len);
			goto err;
		}
	}

	failed = 0;

 err:
	X509_free(cert);

	return failed;
}

/* A certificate whose MTCProof is malformed: parsing must fail. */
static int
check_parse_fails(const char *name)
{
	struct mtc_proof proof;
	const uint8_t *data;
	size_t len;
	X509 *cert;
	int failed = 0;

	cert = load_mtc_cert(name);
	get_proof_bytes(cert, &data, &len);

	if (mtc_proof_parse(data, len, &proof)) {
		warnx("%s: parse succeeded", name);
		failed = 1;
	}

	X509_free(cert);

	return failed;
}

/* The cosigner IDs the fixtures use: 32473.0, 32473.1 and 32473.2. */
static const uint8_t cosigner_0[] = { 0x81, 0xfd, 0x59, 0x00 };
static const uint8_t cosigner_1[] = { 0x81, 0xfd, 0x59, 0x01 };
static const uint8_t cosigner_2[] = { 0x81, 0xfd, 0x59, 0x02 };

static int
test_fixtures(void)
{
	const struct cosig_expect one[] = {
		{ cosigner_1, sizeof(cosigner_1), 2420 },
	};
	const struct cosig_expect three[] = {
		{ cosigner_0, sizeof(cosigner_0), 2420 },
		{ cosigner_1, sizeof(cosigner_1), 2420 },
		{ cosigner_2, sizeof(cosigner_2), 2420 },
	};
	const struct cosig_expect not_ca[] = {
		{ cosigner_0, sizeof(cosigner_0), 2420 },
	};
	int failed = 0;

	failed |= check_valid("mtc-leaf.pem", 0, 10, 0, 128, NULL, 0);
	failed |= check_valid("mtc-ica.pem", 0, 10, 0, 128, NULL, 0);
	failed |= check_valid("mtc-leaf-standalone.pem", 0, 10, 0, 128, one,
	    nitems(one));
	failed |= check_valid("mtc-leaf-standalone-3cosigners.pem", 0, 10, 0,
	    128, three, nitems(three));
	failed |= check_valid("mtc-leaf-standalone-no_ca_signer.pem", 0, 10, 0,
	    128, not_ca, nitems(not_ca));

	failed |= check_parse_fails(
	    "mtc-leaf-standalone-cosigner_wrong_order.pem");
	failed |= check_parse_fails(
	    "mtc-leaf-standalone-duplicate_ca_signer.pem");
	failed |= check_parse_fails("mtc-leaf-standalone-trailing.pem");
	failed |= check_parse_fails("mtc-leaf-standalone-truncated.pem");

	return failed;
}

/*
 * Hand-assembled proofs.  A well-formed proof is
 *	00 00			extensions, empty
 *	00 00 00 00 00 00	start = 0
 *	00 00 00 00 00 0a	end = 10
 *	00 00			inclusion_proof, empty
 *	00 00			signatures, empty
 * and a cosignature is a one-byte-prefixed cosigner_id followed by a
 * two-byte-prefixed signature, so 01 01 00 00 is cosigner 0x01 with an
 * empty signature.
 */

static const uint8_t proof_minimal[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x00,
};

static const uint8_t proof_trailing[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x00,
	0x00,
};

static const uint8_t proof_empty_subtree[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x00,
};

static const uint8_t proof_inverted_subtree[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
};

static const uint8_t proof_extensions_overrun[] = {
	0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x00,
};

static const uint8_t proof_signatures_overrun[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x08, 0x01, 0x01, 0x00, 0x00,
};

static const uint8_t proof_empty_cosigner_id[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00,
};

static const uint8_t proof_two_cosigners[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
};

static const uint8_t proof_duplicate_cosigners[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
};

static const uint8_t proof_unordered_cosigners[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x08, 0x01, 0x02, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
};

/* Cosigner 0x01 then 0x01 0x00: the shorter id sorts first. */
static const uint8_t proof_length_ordered_cosigners[] = {
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00,
	0x00, 0x09, 0x01, 0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
};

static const struct {
	const char *desc;
	const uint8_t *proof;
	size_t len;
	int want;
} synthetic[] = {
	{ "minimal", proof_minimal, sizeof(proof_minimal), 1 },
	{ "empty input", proof_minimal, 0, 0 },
	{ "truncated", proof_minimal, sizeof(proof_minimal) - 1, 0 },
	{ "trailing byte", proof_trailing, sizeof(proof_trailing), 0 },
	{ "empty subtree", proof_empty_subtree, sizeof(proof_empty_subtree),
	    0 },
	{ "inverted subtree", proof_inverted_subtree,
	    sizeof(proof_inverted_subtree), 0 },
	{ "extensions overrun", proof_extensions_overrun,
	    sizeof(proof_extensions_overrun), 0 },
	{ "signatures overrun", proof_signatures_overrun,
	    sizeof(proof_signatures_overrun), 0 },
	{ "empty cosigner_id", proof_empty_cosigner_id,
	    sizeof(proof_empty_cosigner_id), 0 },
	{ "two cosigners", proof_two_cosigners, sizeof(proof_two_cosigners),
	    1 },
	{ "duplicate cosigners", proof_duplicate_cosigners,
	    sizeof(proof_duplicate_cosigners), 0 },
	{ "unordered cosigners", proof_unordered_cosigners,
	    sizeof(proof_unordered_cosigners), 0 },
	{ "length-ordered cosigners", proof_length_ordered_cosigners,
	    sizeof(proof_length_ordered_cosigners), 1 },
};

static int
test_synthetic(void)
{
	struct mtc_proof proof;
	size_t i;
	int got, failed = 0;

	for (i = 0; i < nitems(synthetic); i++) {
		got = mtc_proof_parse(synthetic[i].proof, synthetic[i].len,
		    &proof);
		if (got != synthetic[i].want) {
			warnx("%s: parse returned %d, want %d",
			    synthetic[i].desc, got, synthetic[i].want);
			failed = 1;
		}
	}

	return failed;
}

/* The two-pass contract of mtc_proof_get_cosignatures(). */
static int
test_cosignature_passes(void)
{
	const uint8_t first[] = { 0x01 }, second[] = { 0x02 };
	struct mtc_cosignature cosig[2];
	struct mtc_proof proof;
	size_t count = 0;
	int failed = 0;

	if (!mtc_proof_parse(proof_two_cosigners, sizeof(proof_two_cosigners),
	    &proof))
		errx(1, "two cosigners: parse failed");

	if (!mtc_proof_get_cosignatures(&proof, NULL, 0, &count) ||
	    count != 2) {
		warnx("count pass: got %zu, want 2", count);
		failed = 1;
	}

	count = 0;
	if (mtc_proof_get_cosignatures(&proof, cosig, 1, &count) ||
	    count != 0) {
		warnx("short array accepted or count written");
		failed = 1;
	}

	if (!mtc_proof_get_cosignatures(&proof, cosig, nitems(cosig),
	    &count) || count != 2 ||
	    !CBS_mem_equal(&cosig[0].cosigner_id, first, sizeof(first)) ||
	    !CBS_mem_equal(&cosig[1].cosigner_id, second, sizeof(second)) ||
	    CBS_len(&cosig[0].signature) != 0) {
		warnx("fill pass: wrong cosignatures");
		failed = 1;
	}

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	if (argc != 2)
		errx(1, "usage: %s certs-dir", argv[0]);
	certs_dir = argv[1];

	failed |= test_fixtures();
	failed |= test_synthetic();
	failed |= test_cosignature_passes();

	return failed;
}
