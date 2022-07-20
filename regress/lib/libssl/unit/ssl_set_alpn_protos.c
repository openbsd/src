/*	$OpenBSD: ssl_set_alpn_protos.c,v 1.1 2022/07/20 14:50:03 tb Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
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

#include <openssl/ssl.h>

static const uint8_t valid[] = {
	6, 's', 'p', 'd', 'y', '/', '1',
	8, 'h', 't', 't', 'p', '/', '1', '.', '1',
};

static const uint8_t invalid_len1[] = {
	0,
};

static const uint8_t invalid_contains_len0_proto[] = {
	8, 'h', 't', 't', 'p', '/', '1', '.', '1',
	0,
	6, 's', 'p', 'd', 'y', '/', '1',
};

static const uint8_t invalid_proto_len_too_short[] = {
	6, 'h', 't', 't', 'p', '/', '1', '.', '1',
};

static const uint8_t invalid_proto_len_too_long[] = {
	8, 's', 'p', 'd', 'y', '/', '1',
};

static int
test_ssl_set_alpn_protos(void)
{
	SSL_CTX *ctx;
	SSL *ssl = NULL;
	int failed = 0;

	if ((ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "SSL_CTX_new");

	if (SSL_CTX_set_alpn_protos(ctx, valid, sizeof(valid)) != 0) {
		warnx("setting valid protocols on SSL_CTX failed");
		failed = 1;
	}

	if (SSL_CTX_set_alpn_protos(ctx, NULL, 0) != 0) {
		warnx("setting 'NULL, 0' on SSL_CTX failed");
		failed = 1;
	}
	if (SSL_CTX_set_alpn_protos(ctx, valid, 0) != 0) {
		warnx("setting 'valid, 0' on SSL_CTX failed");
		failed = 1;
	}
	if (SSL_CTX_set_alpn_protos(ctx, NULL, 43) != 0) {
		warnx("setting 'valid, 43' on SSL_CTX failed");
		failed = 1;
	}

	if (SSL_CTX_set_alpn_protos(ctx, invalid_len1, sizeof(invalid_len1))
	    != 1) {
		warnx("setting invalid_len1 on SSL_CTX succeeded");
		failed = 1;
	}
	if (SSL_CTX_set_alpn_protos(ctx, invalid_contains_len0_proto,
	    sizeof(invalid_contains_len0_proto)) != 1) {
		warnx("setting invalid_contains_len0_proto on SSL_CTX "
		    "succeeded");
		failed = 1;
	}
	if (SSL_CTX_set_alpn_protos(ctx, invalid_proto_len_too_short,
	    sizeof(invalid_proto_len_too_short)) != 1) {
		warnx("setting invalid_proto_len_too_short on SSL_CTX "
		    "succeeded");
		failed = 1;
	}
	if (SSL_CTX_set_alpn_protos(ctx, invalid_proto_len_too_long,
	    sizeof(invalid_proto_len_too_long)) != 1) {
		warnx("setting invalid_proto_len_too_long on SSL_CTX "
		    "succeeded");
		failed = 1;
	}

	if ((ssl = SSL_new(ctx)) == NULL)
		errx(1, "SSL_new");

	if (SSL_set_alpn_protos(ssl, valid, sizeof(valid)) != 0) {
		warnx("setting valid protocols on SSL failed");
		failed = 1;
	}
	if (SSL_set_alpn_protos(ssl, NULL, 0) != 0) {
		warnx("setting 'NULL, 0' on SSL failed");
		failed = 1;
	}
	if (SSL_set_alpn_protos(ssl, valid, 0) != 0) {
		warnx("setting 'valid, 0' on SSL failed");
		failed = 1;
	}
	if (SSL_set_alpn_protos(ssl, NULL, 43) != 0) {
		warnx("setting 'valid, 43' on SSL failed");
		failed = 1;
	}

	if (SSL_set_alpn_protos(ssl, invalid_len1, sizeof(invalid_len1))
	    != 1) {
		warnx("setting invalid_len1 on SSL succeeded");
		failed = 1;
	}
	if (SSL_set_alpn_protos(ssl, invalid_contains_len0_proto,
	    sizeof(invalid_contains_len0_proto)) != 1) {
		warnx("setting invalid_contains_len0_proto on SSL succeeded");
		failed = 1;
	}
	if (SSL_set_alpn_protos(ssl, invalid_proto_len_too_short,
	    sizeof(invalid_proto_len_too_short)) != 1) {
		warnx("setting invalid_proto_len_too_short on SSL succeeded");
		failed = 1;
	}
	if (SSL_set_alpn_protos(ssl, invalid_proto_len_too_long,
	    sizeof(invalid_proto_len_too_long)) != 1) {
		warnx("setting invalid_proto_len_too_long on SSL succeeded");
		failed = 1;
	}

	SSL_CTX_free(ctx);
	SSL_free(ssl);

	return failed;
}

int
main(void)
{
	int failed;

	failed = test_ssl_set_alpn_protos();

	if (!failed)
		printf("PASS %s\n", __FILE__);

	return failed;
}
