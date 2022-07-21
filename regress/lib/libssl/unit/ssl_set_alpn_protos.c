/*	$OpenBSD: ssl_set_alpn_protos.c,v 1.2 2022/07/21 03:59:04 tb Exp $ */
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

struct alpn_test {
	const char *description;
	const uint8_t protocols[24];
	size_t protocols_len;
	int ret;
};

static const struct alpn_test alpn_tests[] = {
	{
		.description = "valid protocol list",
		.protocols = {
			6, 's', 'p', 'd', 'y', '/', '1',
			8, 'h', 't', 't', 'p', '/', '1', '.', '1',
		},
		.protocols_len = 16,
		.ret = 0,
	},
	{
		.description = "zero length protocol",
		.protocols = {
			0,
		},
		.protocols_len = 1,
		.ret = 1,
	},
	{
		.description = "zero length protocol at start",
		.protocols = {
			0,
			8, 'h', 't', 't', 'p', '/', '1', '.', '1',
			6, 's', 'p', 'd', 'y', '/', '1',
		},
		.protocols_len = 17,
		.ret = 1,
	},
	{
		.description = "zero length protocol embedded",
		.protocols = {
			8, 'h', 't', 't', 'p', '/', '1', '.', '1',
			0,
			6, 's', 'p', 'd', 'y', '/', '1',
		},
		.protocols_len = 17,
		.ret = 1,
	},
	{
		.description = "zero length protocol at end",
		.protocols = {
			8, 'h', 't', 't', 'p', '/', '1', '.', '1',
			6, 's', 'p', 'd', 'y', '/', '1',
			0,
		},
		.protocols_len = 17,
		.ret = 1,
	},
	{
		.description = "protocol length too short",
		.protocols = {
			6, 'h', 't', 't', 'p', '/', '1', '.', '1',
		},
		.protocols_len = 9,
		.ret = 1,
	},
	{
		.description = "protocol length too long",
		.protocols = {
			8, 's', 'p', 'd', 'y', '/', '1',
		},
		.protocols_len = 7,
		.ret = 1,
	},
};

static const size_t N_ALPN_TESTS = sizeof(alpn_tests) / sizeof(alpn_tests[0]);

static int
test_ssl_set_alpn_protos(const struct alpn_test *tc)
{
	SSL_CTX *ctx;
	SSL *ssl;
	int ret;
	int failed = 0;

	if ((ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		errx(1, "SSL_CTX_new");

	ret = SSL_CTX_set_alpn_protos(ctx, tc->protocols, tc->protocols_len);
	if (ret != tc->ret) {
		warnx("%s: setting on SSL_CTX: want %d, got %d",
		    tc->description, tc->ret, ret);
		failed = 1;
	}

	if ((ssl = SSL_new(ctx)) == NULL)
		errx(1, "SSL_new");

	ret = SSL_set_alpn_protos(ssl, tc->protocols, tc->protocols_len);
	if (ret != tc->ret) {
		warnx("%s: setting on SSL: want %d, got %d",
		    tc->description, tc->ret, ret);
		failed = 1;
	}

	SSL_CTX_free(ctx);
	SSL_free(ssl);

	return failed;
}

static int
test_ssl_set_alpn_protos_edge_cases(void)
{
	SSL_CTX *ctx;
	SSL *ssl;
	const uint8_t valid[] = {
		6, 's', 'p', 'd', 'y', '/', '3',
		8, 'h', 't', 't', 'p', '/', '1', '.', '1',
	};
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
		warnx("setting 'NULL, 43' on SSL_CTX failed");
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
		warnx("setting 'NULL, 43' on SSL failed");
		failed = 1;
	}

	SSL_CTX_free(ctx);
	SSL_free(ssl);

	return failed;
}

int
main(void)
{
	size_t i;
	int failed = 0;

	for (i = 0; i < N_ALPN_TESTS; i++)
		failed |= test_ssl_set_alpn_protos(&alpn_tests[i]);

	failed |= test_ssl_set_alpn_protos_edge_cases();

	if (!failed)
		printf("PASS %s\n", __FILE__);

	return failed;
}
