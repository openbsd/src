/*	$OpenBSD: bio_asn1.c,v 1.1 2023/03/26 19:14:11 tb Exp $ */

/*
 * Copyright (c) 2023 Theo Buehler <tb@openbsd.org>
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
#include <stdio.h>
#include <stdlib.h>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/pkcs7.h>
#include <openssl/objects.h>

/*
 * test_prefix_leak() leaks before asn/bio_asn1.c r1.19.
 */

static long
read_leak_cb(BIO *bio, int cmd, const char *argp, int argi, long argl, long ret)
{
	int read_return = BIO_CB_READ | BIO_CB_RETURN;
	char *set_me;

	if ((cmd & read_return) != read_return)
		return ret;

	set_me = BIO_get_callback_arg(bio);
	*set_me = 1;

	return 0;
}

static int
test_prefix_leak(void)
{
	const char *data = "some data\n";
	BIO *bio_in = NULL, *bio_out = NULL;
	PKCS7 *pkcs7 = NULL;
	char set_me = 0;
	int failed = 1;

	if ((bio_in = BIO_new_mem_buf(data, -1)) == NULL)
		goto err;

	BIO_set_callback(bio_in, read_leak_cb);
	BIO_set_callback_arg(bio_in, &set_me);

	if ((pkcs7 = PKCS7_new()) == NULL)
		goto err;
	if (!PKCS7_set_type(pkcs7, NID_pkcs7_data))
		goto err;

	if ((bio_out = BIO_new(BIO_s_mem())) == NULL)
		goto err;

	if (!i2d_ASN1_bio_stream(bio_out, (ASN1_VALUE *)pkcs7, bio_in,
	    SMIME_STREAM | SMIME_BINARY, &PKCS7_it))
		goto err;

	if (set_me != 1) {
		fprintf(stderr, "%s: read_leak_cb didn't set set_me", __func__);
		goto err;
	}

	failed = 0;

 err:
	BIO_free(bio_in);
	BIO_free(bio_out);
	PKCS7_free(pkcs7);

	return failed;
}

/*
 * test_infinite_loop() would hang before asn/bio_asn1.c r1.18.
 */

#define SENTINEL (-57)

static long
inf_loop_cb(BIO *bio, int cmd, const char *argp, int argi, long argl, long ret)
{
	int write_return = BIO_CB_WRITE | BIO_CB_RETURN;
	char *set_me;

	if ((cmd & write_return) != write_return)
		return ret;

	set_me = BIO_get_callback_arg(bio);

	/* First time around: ASN1_STATE_HEADER_COPY - succeed. */
	if (*set_me == 0) {
		*set_me = 1;
		return ret;
	}

	/* Second time around: ASN1_STATE_DATA_COPY - return sentinel value. */
	if (*set_me == 1) {
		*set_me = 2;
		return SENTINEL;
	}

	/* Everything else is unexpected: return EOF. */
	*set_me = 3;

	return 0;

}

static int
test_infinite_loop(void)
{
	BIO *asn_bio = NULL, *bio = NULL;
	char set_me = 0;
	int failed = 1;
	int write_ret;

	if ((asn_bio = BIO_new(BIO_f_asn1())) == NULL)
		goto err;

	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto err;

	BIO_set_callback(bio, inf_loop_cb);
	BIO_set_callback_arg(bio, &set_me);

	if (BIO_push(asn_bio, bio) == NULL) {
		BIO_free(bio);
		goto err;
	}

	if ((write_ret = BIO_write(asn_bio, "foo", 3)) != SENTINEL) {
		fprintf(stderr, "%s: BIO_write: want %d, got %d", __func__,
		    SENTINEL, write_ret);
		goto err;
	}

	if (set_me != 2) {
		fprintf(stderr, "%s: set_me: %d != 2", __func__, set_me);
		goto err;
	}

	failed = 0;
 err:
	BIO_free_all(asn_bio);

	return failed;
}

int
main(void)
{
	int failed = 0;

	failed |= test_prefix_leak();
	failed |= test_infinite_loop();

	return failed;
}
