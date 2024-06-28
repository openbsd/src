/*	$OpenBSD: ssl_set_alpn_protos.c,v 1.3 2024/06/28 14:50:37 tb Exp $ */
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

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	if (buf == NULL) {
		fprintf(stderr, "(null), len %zu\n", len);
		return;
	}
	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");
	if (len % 8)
		fprintf(stderr, "\n");
}

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

static const struct select_next_proto_test {
	const unsigned char *server_list;
	size_t server_list_len;
	const unsigned char *client_list;
	size_t client_list_len;
	int want_ret;
	const unsigned char *want_out;
	unsigned char want_out_len; /* yes, unsigned char */
} select_next_proto_tests[] = {
	{
		.server_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.server_list_len = 6,
		.client_list = "\x01" "a",
		.client_list_len = 2,
		.want_ret = OPENSSL_NPN_NEGOTIATED,
		.want_out = "a",
		.want_out_len = 1,
	},
	{
		.server_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.server_list_len = 6,
		.client_list = "\x02" "aa" "\x01" "b" "\x01" "c",
		.client_list_len = 7,
		.want_ret = OPENSSL_NPN_NEGOTIATED,
		.want_out = "b",
		.want_out_len = 1,
	},
	{
		/* Use server preference. */
		.server_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.server_list_len = 6,
		.client_list = "\x01" "c" "\x01" "b" "\x01" "a",
		.client_list_len = 6,
		.want_ret = OPENSSL_NPN_NEGOTIATED,
		.want_out = "a",
		.want_out_len = 1,
	},
	{
		/* Again server preference wins. */
		.server_list = "\x01" "a" "\x03" "bbb" "\x02" "cc",
		.server_list_len = 9,
		.client_list = "\x01" "z" "\x02" "cc" "\x03" "bbb",
		.client_list_len = 9,
		.want_ret = OPENSSL_NPN_NEGOTIATED,
		.want_out = "bbb",
		.want_out_len = 3,
	},
	{
		/* No overlap fails with first client protocol. */
		.server_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.server_list_len = 6,
		.client_list = "\x01" "z" "\x01" "y",
		.client_list_len = 4,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
		.want_out = "z",
		.want_out_len = 1,
	},
	{
		/*
		 * No server protocols is a misconfiguration, but should fail
		 * cleanly.
		 */
		.server_list = "",
		.server_list_len = 0,
		.client_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.client_list_len = 6,
		.want_out = "a",
		.want_out_len = 1,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/*
		 * NULL server protocols is a programming error that fails
		 * cleanly.
		 */
		.server_list = NULL,
		.server_list_len = 0,
		.client_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.client_list_len = 6,
		.want_out = "a",
		.want_out_len = 1,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/*
		 * Malformed server protocols is a misconfiguration, but it
		 * should fail cleanly.
		 */
		.server_list = "\x00",
		.server_list_len = 1,
		.client_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.client_list_len = 6,
		.want_out = "a",
		.want_out_len = 1,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/*
		 * Malformed server protocols is a misconfiguration, but it
		 * should fail cleanly.
		 */
		.server_list = "\x01" "a" "\x03" "bb",
		.server_list_len = 5,
		.client_list = "\x01" "a" "\x01" "b" "\x01" "c",
		.client_list_len = 6,
		.want_out = "a",
		.want_out_len = 1,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/*
		 * Empty client protocols is not reachable from the ALPN
		 * callback. It fails cleanly with NULL protocol and 0 length.
		 */
		.server_list = "\x01" "a",
		.server_list_len = 2,
		.client_list = "",
		.client_list_len = 0,
		.want_out = NULL,
		.want_out_len = 0,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/*
		 * NULL client protocols is not reachable from the ALPN
		 * callback. It fails cleanly with NULL protocol and 0 length.
		 */
		.server_list = "\x01" "a",
		.server_list_len = 2,
		.client_list = NULL,
		.client_list_len = 0,
		.want_out = NULL,
		.want_out_len = 0,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/*
		 * Malformed client list fails cleanly with NULL protocol and
		 * 0 length.
		 */
		.server_list = "\x01" "a",
		.server_list_len = 2,
		.client_list = "\x01" "a" "\x02" "bb" "\x03" "cc" "\x04" "ddd",
		.client_list_len = 12,
		.want_out = NULL,
		.want_out_len = 0,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/*
		 * Malformed client list fails cleanly with NULL protocol and
		 * 0 length.
		 */
		.server_list = "\x01" "a",
		.server_list_len = 2,
		.client_list = "\x01" "a" "\x02" "bb" "\x00" "\x03" "ddd",
		.client_list_len = 10,
		.want_out = NULL,
		.want_out_len = 0,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},

	/*
	 * Some non-toy examples.
	 */

	{
		.server_list = "\x08" "http/1.1" "\x06" "spdy/1",
		.server_list_len = 16,
		.client_list = "\x08" "http/2.0" "\x08" "http/1.1",
		.client_list_len = 18,
		.want_out = "http/1.1",
		.want_out_len = 8,
		.want_ret = OPENSSL_NPN_NEGOTIATED,
	},
	{
		.server_list = "\x08" "http/2.0" "\x06" "spdy/1",
		.server_list_len = 16,
		.client_list = "\x08" "http/1.0" "\x08" "http/1.1",
		.client_list_len = 18,
		.want_out = "http/1.0",
		.want_out_len = 8,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		.server_list = "\x08" "http/1.1" "\x08" "http/1.0",
		.server_list_len = 18,
		.client_list = "\x08" "http/1.0" "\x08" "http/1.1",
		.client_list_len = 18,
		.want_out = "http/1.1",
		.want_out_len = 8,
		.want_ret = OPENSSL_NPN_NEGOTIATED,
	},
	{
		/* Server malformed. */
		.server_list = "\x08" "http/1.1" "\x07" "http/1.0",
		.server_list_len = 18,
		.client_list = "\x08" "http/1.0" "\x08" "http/1.1",
		.client_list_len = 18,
		.want_out = "http/1.0",
		.want_out_len = 8,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/* Server malformed. */
		.server_list = "\x07" "http/1.1" "\x08" "http/1.0",
		.server_list_len = 18,
		.client_list = "\x08" "http/1.0" "\x08" "http/1.1",
		.client_list_len = 18,
		.want_out = "http/1.0",
		.want_out_len = 8,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
	{
		/* Client has trailing bytes. */
		.server_list = "\x08" "http/1.1" "\x08" "http/1.0",
		.server_list_len = 18,
		.client_list = "\x08" "http/1.0" "\x07" "http/1.1",
		.client_list_len = 18,
		.want_out = NULL,
		.want_out_len = 0,
		.want_ret = OPENSSL_NPN_NO_OVERLAP,
	},
};

#define N_SELECT_NEXT_PROTO_TESTS \
    (sizeof(select_next_proto_tests) / sizeof(select_next_proto_tests[0]))

static int
select_next_proto_testcase(const struct select_next_proto_test *test)
{
	unsigned char *out;
	unsigned char out_len;
	int ret;
	int failed = 0;

	ret = SSL_select_next_proto(&out, &out_len, test->server_list,
	    test->server_list_len, test->client_list, test->client_list_len);

	if (ret != test->want_ret || out_len != test->want_out_len ||
	    (out == NULL && test->want_out != NULL) ||
	    (out != NULL && test->want_out == NULL) ||
	    (out != NULL && test->want_out != NULL &&
	     memcmp(out, test->want_out, out_len) != 0)) {
		fprintf(stderr, "FAIL: ret: %u (want %u), out_len: %u (want %u)\n",
		    ret, test->want_ret, out_len, test->want_out_len);
		fprintf(stderr, "\ngot:\n");
		hexdump(out, out_len);
		fprintf(stderr, "\nwant:\n");
		hexdump(test->want_out, test->want_out_len);
		fprintf(stderr, "\nserver:\n");
		hexdump(test->server_list, test->server_list_len);
		fprintf(stderr, "\nclient:\n");
		hexdump(test->client_list, test->client_list_len);
		fprintf(stderr, "\n");
		failed = 1;
	}

	return failed;
}

static int
test_ssl_select_next_proto(void)
{
	size_t i;
	int failed = 0;

	for (i = 0; i < N_SELECT_NEXT_PROTO_TESTS; i++)
		failed |= select_next_proto_testcase(&select_next_proto_tests[i]);

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

	failed |= test_ssl_select_next_proto();

	if (!failed)
		printf("PASS %s\n", __FILE__);

	return failed;
}
