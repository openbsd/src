/*	$OpenBSD: pkcs7test.c,v 1.8 2026/08/30 16:55:41 tb Exp $	*/
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2026 Theo Buehler <tb@openbsd.org>
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

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>

const char certificate[] = "\
-----BEGIN CERTIFICATE----- \n\
MIIDpTCCAo2gAwIBAgIJAPYm3GvOr5eTMA0GCSqGSIb3DQEBBQUAMHAxCzAJBgNV \n\
BAYTAlVLMRYwFAYDVQQKDA1PcGVuU1NMIEdyb3VwMSIwIAYDVQQLDBlGT1IgVEVT \n\
VElORyBQVVJQT1NFUyBPTkxZMSUwIwYDVQQDDBxPcGVuU1NMIFRlc3QgSW50ZXJt \n\
ZWRpYXRlIENBMB4XDTE0MDUyNDE0NDUxMVoXDTI0MDQwMTE0NDUxMVowZDELMAkG \n\
A1UEBhMCVUsxFjAUBgNVBAoMDU9wZW5TU0wgR3JvdXAxIjAgBgNVBAsMGUZPUiBU \n\
RVNUSU5HIFBVUlBPU0VTIE9OTFkxGTAXBgNVBAMMEFRlc3QgQ2xpZW50IENlcnQw \n\
ggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC0ranbHRLcLVqN+0BzcZpY \n\
+yOLqxzDWT1LD9eW1stC4NzXX9/DCtSIVyN7YIHdGLrIPr64IDdXXaMRzgZ2rOKs \n\
lmHCAiFpO/ja99gGCJRxH0xwQatqAULfJVHeUhs7OEGOZc2nWifjqKvGfNTilP7D \n\
nwi69ipQFq9oS19FmhwVHk2wg7KZGHI1qDyG04UrfCZMRitvS9+UVhPpIPjuiBi2 \n\
x3/FZIpL5gXJvvFK6xHY63oq2asyzBATntBgnP4qJFWWcvRx24wF1PnZabxuVoL2 \n\
bPnQ/KvONDrw3IdqkKhYNTul7jEcu3OlcZIMw+7DiaKJLAzKb/bBF5gm/pwW6As9 \n\
AgMBAAGjTjBMMAwGA1UdEwEB/wQCMAAwDgYDVR0PAQH/BAQDAgXgMCwGCWCGSAGG \n\
+EIBDQQfFh1PcGVuU1NMIEdlbmVyYXRlZCBDZXJ0aWZpY2F0ZTANBgkqhkiG9w0B \n\
AQUFAAOCAQEAJzA4KTjkjXGSC4He63yX9Br0DneGBzjAwc1H6f72uqnCs8m7jgkE \n\
PQJFdTzQUKh97QPUuayZ2gl8XHagg+iWGy60Kw37gQ0+lumCN2sllvifhHU9R03H \n\
bWtS4kue+yQjMbrzf3zWygMDgwvFOUAIgBpH9qGc+CdNu97INTYd0Mvz51vLlxRn \n\
sC5aBYCWaZFnw3lWYxf9eVFRy9U+DkYFqX0LpmbDtcKP7AZGE6ZwSzaim+Cnoz1u \n\
Cgn+QmpFXgJKMFIZ82iSZISn+JkCCGxctZX1lMvai4Wi8Y0HxW9FTFZ6KBNwwE4B \n\
zjbN/ehBkgLlW/DWfi44DvwUHmuU6QP3cw== \n\
-----END CERTIFICATE----- \n\
";

const char private_key[] = "\
-----BEGIN RSA PRIVATE KEY----- \n\
MIIEpQIBAAKCAQEAtK2p2x0S3C1ajftAc3GaWPsji6scw1k9Sw/XltbLQuDc11/f \n\
wwrUiFcje2CB3Ri6yD6+uCA3V12jEc4GdqzirJZhwgIhaTv42vfYBgiUcR9McEGr \n\
agFC3yVR3lIbOzhBjmXNp1on46irxnzU4pT+w58IuvYqUBavaEtfRZocFR5NsIOy \n\
mRhyNag8htOFK3wmTEYrb0vflFYT6SD47ogYtsd/xWSKS+YFyb7xSusR2Ot6Ktmr \n\
MswQE57QYJz+KiRVlnL0cduMBdT52Wm8blaC9mz50PyrzjQ68NyHapCoWDU7pe4x \n\
HLtzpXGSDMPuw4miiSwMym/2wReYJv6cFugLPQIDAQABAoIBAAZOyc9MhIwLSU4L \n\
p4RgQvM4UVVe8/Id+3XTZ8NsXExJbWxXfIhiqGjaIfL8u4vsgRjcl+v1s/jo2/iT \n\
KMab4o4D8gXD7UavQVDjtjb/ta79WL3SjRl2Uc9YjjMkyq6WmDNQeo2NKDdafCTB \n\
1uzSJtLNipB8Z53ELPuHJhxX9QMHrMnuha49riQgXZ7buP9iQrHJFhImBjSzbxJx \n\
L+TI6rkyLSf9Wi0Pd3L27Ob3QWNfNRYNSeTE+08eSRChkur5W0RuXAcuAICdQlCl \n\
LBvWO/LmmvbzCqiDcgy/TliSb6CGGwgiNG7LJZmlkYNj8laGwalNlYZs3UrVv6NO \n\
Br2loAECgYEA2kvCvPGj0Dg/6g7WhXDvAkEbcaL1tSeCxBbNH+6HS2UWMWvyTtCn \n\
/bbD519QIdkvayy1QjEf32GV/UjUVmlULMLBcDy0DGjtL3+XpIhLKWDNxN1v1/ai \n\
1oz23ZJCOgnk6K4qtFtlRS1XtynjA+rBetvYvLP9SKeFrnpzCgaA2r0CgYEA0+KX \n\
1ACXDTNH5ySX3kMjSS9xdINf+OOw4CvPHFwbtc9aqk2HePlEsBTz5I/W3rKwXva3 \n\
NqZ/bRqVVeZB/hHKFywgdUQk2Uc5z/S7Lw70/w1HubNTXGU06Ngb6zOFAo/o/TwZ \n\
zTP1BMIKSOB6PAZPS3l+aLO4FRIRotfFhgRHOoECgYEAmiZbqt8cJaJDB/5YYDzC \n\
mp3tSk6gIb936Q6M5VqkMYp9pIKsxhk0N8aDCnTU+kIK6SzWBpr3/d9Ecmqmfyq7 \n\
5SvWO3KyVf0WWK9KH0abhOm2BKm2HBQvI0DB5u8sUx2/hsvOnjPYDISbZ11t0MtK \n\
u35Zy89yMYcSsIYJjG/ROCUCgYEAgI2P9G5PNxEP5OtMwOsW84Y3Xat/hPAQFlI+ \n\
HES+AzbFGWJkeT8zL2nm95tVkFP1sggZ7Kxjz3w7cpx7GX0NkbWSE9O+T51pNASV \n\
tN1sQ3p5M+/a+cnlqgfEGJVvc7iAcXQPa3LEi5h2yPR49QYXAgG6cifn3dDSpmwn \n\
SUI7PQECgYEApGCIIpSRPLAEHTGmP87RBL1smurhwmy2s/pghkvUkWehtxg0sGHh \n\
kuaqDWcskogv+QC0sVdytiLSz8G0DwcEcsHK1Fkyb8A+ayiw6jWJDo2m9+IF4Fww \n\
1Te6jFPYDESnbhq7+TLGgHGhtwcu5cnb4vSuYXGXKupZGzoLOBbv1Zw= \n\
-----END RSA PRIVATE KEY----- \n\
";

const char message[] = "\
Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do \r\n\
eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut   \r\n\
enim ad minim veniam, quis nostrud exercitation ullamco laboris  \r\n\
nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor   \r\n\
in reprehenderit in voluptate velit esse cillum dolore eu fugiat \r\n\
nulla pariatur. Excepteur sint occaecat cupidatat non proident,  \r\n\
sunt in culpa qui officia deserunt mollit anim id est laborum.   \r\n\
";

static int
x509_store_callback(int ok, X509_STORE_CTX *ctx)
{
	/* Pretend the certificate issuer is valid... */
	return 1;
}

static void
fatal(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vwarnx("%s", ap);
	va_end(ap);
	ERR_print_errors_fp(stderr);
	exit(1);
}

static void
message_compare(const char *out, size_t len)
{
	if (len != sizeof(message)) {
		fprintf(stderr, "FAILURE: length mismatch (%zu != %zu)\n",
		    len, sizeof(message));
		exit(1);
	}
	if (memcmp(out, message, len) != 0) {
		fprintf(stderr, "FAILURE: message mismatch\n");
		fprintf(stderr, "Got:\n%s\n", out);
		fprintf(stderr, "Want:\n%s\n", message);
		exit(1);
	}
}

static int
pkcs7_basics(void)
{
	BIO *bio_in, *bio_content, *bio_out, *bio_cert, *bio_pkey;
	STACK_OF(X509) *certs;
	const EVP_CIPHER *cipher;
	EVP_PKEY *pkey;
	X509_STORE *store;
	X509 *cert;
	PKCS7 *p7;
	size_t len;
	char *out;
	int flags;

	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();

	/*
	 * A bunch of setup...
	 */
	cipher = EVP_aes_256_cbc();
	if (cipher == NULL)
		fatal("cipher");

	certs = sk_X509_new_null();
	if (certs == NULL)
		fatal("sk_X509_new_null");

	bio_cert = BIO_new_mem_buf((char *)certificate, sizeof(certificate));
	if (bio_cert == NULL)
		fatal("BIO_new_mem_buf certificate");

	cert = PEM_read_bio_X509_AUX(bio_cert, NULL, NULL, NULL);
	if (cert == NULL)
		fatal("PEM_read_bio_X509_AUX");
	sk_X509_push(certs, cert);

	store = X509_STORE_new();
	if (store == NULL)
		fatal("X509_STORE_new");
	X509_STORE_set_verify_cb(store, x509_store_callback);

	bio_pkey = BIO_new_mem_buf((char *)private_key, sizeof(private_key));
	if (bio_pkey == NULL)
		fatal("BIO_new_mem_buf private_key");

	pkey = PEM_read_bio_PrivateKey(bio_pkey, NULL, NULL, NULL);
	if (pkey == NULL)
		fatal("PEM_read_bio_PrivateKey");

	bio_content = BIO_new_mem_buf((char *)message, sizeof(message));
	if (bio_content == NULL)
		fatal("BIO_new_mem_buf message");

	/*
	 * Encrypt and then decrypt.
	 */
	if (BIO_reset(bio_content) != 1)
		fatal("BIO_reset");
	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL)
		fatal("BIO_new");

	p7 = PKCS7_encrypt(certs, bio_content, cipher, 0);
	if (p7 == NULL)
		fatal("PKCS7_encrypt");
	if (PEM_write_bio_PKCS7(bio_out, p7) != 1)
		fatal("PEM_write_bio_PKCS7");
	PKCS7_free(p7);

	bio_in = bio_out;
	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL)
		fatal("BIO_new");

	p7 = PEM_read_bio_PKCS7(bio_in, NULL, NULL, NULL);
	if (p7 == NULL)
		fatal("PEM_read_bio_PKCS7");
	if (PKCS7_decrypt(p7, pkey, cert, bio_out, 0) != 1)
		fatal("PKCS7_decrypt");
	PKCS7_free(p7);

	len = BIO_get_mem_data(bio_out, &out);
	message_compare(out, len);

	BIO_free(bio_in);
	BIO_free(bio_out);

	/*
	 * Sign and then verify.
	 */
	if (BIO_reset(bio_content) != 1)
		fatal("BIO_reset");
	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL)
		fatal("BIO_new");

	p7 = PKCS7_sign(cert, pkey, certs, bio_content, 0);
	if (p7 == NULL)
		fatal("PKCS7_sign");
	if (PEM_write_bio_PKCS7(bio_out, p7) != 1)
		fatal("PEM_write_bio_PKCS7");
	PKCS7_free(p7);

	bio_in = bio_out;
	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL)
		fatal("BIO_new");

	p7 = PEM_read_bio_PKCS7(bio_in, NULL, NULL, NULL);
	if (p7 == NULL)
		fatal("PEM_read_bio_PKCS7");
	if (PKCS7_verify(p7, certs, store, NULL, bio_out, 0) != 1)
		fatal("PKCS7_verify");
	PKCS7_free(p7);

	len = BIO_get_mem_data(bio_out, &out);
	message_compare(out, len);

	BIO_free(bio_in);
	BIO_free(bio_out);

	/*
	 * Sign and then verify with a detached signature.
	 */
	if (BIO_reset(bio_content) != 1)
		fatal("BIO_reset");
	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL)
		fatal("BIO_new");

	flags = PKCS7_DETACHED|PKCS7_PARTIAL;
	p7 = PKCS7_sign(NULL, NULL, NULL, bio_content, flags);
	if (p7 == NULL)
		fatal("PKCS7_sign");
	if (PKCS7_sign_add_signer(p7, cert, pkey, NULL, flags) == NULL)
		fatal("PKCS7_sign_add_signer");
	if (PKCS7_final(p7, bio_content, flags) != 1)
		fatal("PKCS7_final");
	if (PEM_write_bio_PKCS7(bio_out, p7) != 1)
		fatal("PEM_write_bio_PKCS7");
	PKCS7_free(p7);

	/* bio_out contains only the detached signature. */
	bio_in = bio_out;
	if (BIO_reset(bio_content) != 1)
		fatal("BIO_reset");

	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL)
		fatal("BIO_new");

	p7 = PEM_read_bio_PKCS7(bio_in, NULL, NULL, NULL);
	if (p7 == NULL)
		fatal("PEM_read_bio_PKCS7");
	if (PKCS7_verify(p7, certs, store, bio_content, bio_out, flags) != 1)
		fatal("PKCS7_verify");
	PKCS7_free(p7);

	len = BIO_get_mem_data(bio_out, &out);
	message_compare(out, len);

	BIO_free(bio_in);
	BIO_free(bio_out);
	BIO_free(bio_content);
	BIO_free(bio_cert);
	BIO_free(bio_pkey);

	EVP_PKEY_free(pkey);

	X509_free(cert);
	X509_STORE_free(store);
	sk_X509_free(certs);

	return 0;
}

static int
pkcs7_stream_missing_content_nid(int nid)
{
	PKCS7 *p7 = NULL;
	const unsigned char *p, *name;
	unsigned char **boundary = NULL;
	unsigned char *der = NULL;
	int der_len = 0;
	int ret;
	int failed = 1;

	name = OBJ_nid2sn(nid);

	/*
	 * Create PKCS7 object with Content Type corresponding to nid
	 * and omit the optional content.
	 */

	if ((p7 = PKCS7_new()) == NULL)
		fatal("PKCS7_new NID %d (%s)", nid, name);
	ASN1_OBJECT_free(p7->type);
	if ((p7->type = OBJ_nid2obj(nid)) == NULL)
		fatal("OBJ_nid2obj NID %d (%s)", nid, name);

	/*
	 * Round trip this through DER.
	 */

	if ((der_len = i2d_PKCS7(p7, &der)) <= 0)
		fatal("i2d_PKCS7 NID %d (%s)", nid, name);

	PKCS7_free(p7);
	p7 = NULL;

	p = der;
	if ((p7 = d2i_PKCS7(NULL, &p, der_len)) == NULL)
		fatal("d2i_PKCS7 NID %d (%s)", nid, name);

	/*
	 * It deserialized, so we can safely stream it, right?
	 */

	if ((ret = PKCS7_stream(&boundary, p7)) != 0) {
		fprintf(stderr, "FAILURE: PKCS7_stream for NID %d (%s) "
		    "want 0, got %d\n", nid, name, ret);
		goto out;
	}

	failed = 0;

 out:
	PKCS7_free(p7);
	freezero(der, der_len);

	return failed;
}

/*
 * For each Content Type OID (RFC 2315, section 14), create a PKCS7 object that
 * d2i_PKCS7() accepts. For x in [1..6] we use an object that encodes to
 *
 * SEQUENCE {
 *   OBJECT_IDENTIFIER { 1.2.840.113549.1.7.x }
 * }
 *
 * This works because RFC 2315 section 7 marks the content optional:
 *
 *  ContentInfo ::= SEQUENCE {
 *    contentType ContentType,
 *    content
 *      [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL }
 *
 * reflected in the ASN1_TFLG_OPTIONAL in pk7_asn1.c's p7default_tt.
 */

static int
pkcs7_stream_missing_content(void)
{
	int failed = 0;

	/* NID naming consistency is king. */
	failed |= pkcs7_stream_missing_content_nid(NID_pkcs7_data);
	failed |= pkcs7_stream_missing_content_nid(NID_pkcs7_signed);
	failed |= pkcs7_stream_missing_content_nid(NID_pkcs7_enveloped);
	failed |= pkcs7_stream_missing_content_nid(NID_pkcs7_signedAndEnveloped);
	failed |= pkcs7_stream_missing_content_nid(NID_pkcs7_digest);
	failed |= pkcs7_stream_missing_content_nid(NID_pkcs7_encrypted);

	return failed;
}

/*
 * SEQUENCE {
 *   # signedData
 *   OBJECT_IDENTIFIER { 1.2.840.113549.1.7.2 }
 *   [0] {
 *     SEQUENCE {
 *       INTEGER { 1 }
 *       SET {}
 *       SEQUENCE {
 *         # id-ct-TSTInfo
 *         OBJECT_IDENTIFIER { 1.2.840.113549.1.9.16.1.4 }
 *         [0] {
 *           SEQUENCE {
 *             INTEGER { 1 }
 *             OCTET_STRING { `deadbeef` }
 *           }
 *         }
 *       }
 *       SET {}
 *     }
 *   }
 * }
 */
static const uint8_t pkcs7_malformed_der[] = {
	 0x30, 0x32, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
	 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0, 0x25, 0x30,
	 0x23, 0x02, 0x01, 0x01, 0x31, 0x00, 0x30, 0x1a,
	 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
	 0x01, 0x09, 0x10, 0x01, 0x04, 0xa0, 0x0b, 0x30,
	 0x09, 0x02, 0x01, 0x01, 0x04, 0x04, 0xde, 0xad,
	 0xbe, 0xef, 0x31, 0x00,
};
static int pkcs7_malformed_der_len = sizeof(pkcs7_malformed_der);

static int
pkcs7_stream_signedData_oob(void)
{
	PKCS7 *p7 = NULL;
	const unsigned char *p;
	unsigned char **boundary = NULL;
	int ret;
	int failed = 1;

	p = pkcs7_malformed_der;
	if ((p7 = d2i_PKCS7(NULL, &p, pkcs7_malformed_der_len)) == NULL)
		fatal("d2i_PKCS7 malformed");

	if ((ret = PKCS7_stream(&boundary, p7)) != 0) {
		fprintf(stderr, "FAILURE: PKCS7_stream want 0, got %d\n", ret);
		goto out;
	}

	failed = 0;
 out:
	PKCS7_free(p7);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= pkcs7_basics();
	failed |= pkcs7_stream_missing_content();
	failed |= pkcs7_stream_signedData_oob();

	return failed;
}
