/* $OpenBSD: ssl_versions.c,v 1.2 2017/01/03 16:58:10 jsing Exp $ */
/*
 * Copyright (c) 2016 Joel Sing <jsing@openbsd.org>
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

#include <openssl/ssl.h>

int ssl_enabled_version_range(SSL *s, uint16_t *min_ver, uint16_t *max_ver);
int ssl_max_shared_version(SSL *s, uint16_t peer_ver, uint16_t *max_ver);

struct version_range_test {
	const long options;
	const uint16_t minver;
	const uint16_t maxver;
};

static struct version_range_test version_range_tests[] = {
	{
		.options = 0,
		.minver = TLS1_VERSION,
		.maxver = TLS1_2_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1,
		.minver = TLS1_1_VERSION,
		.maxver = TLS1_2_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1_2,
		.minver = TLS1_VERSION,
		.maxver = TLS1_1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1_1,
		.minver = TLS1_VERSION,
		.maxver = TLS1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1,
		.minver = TLS1_2_VERSION,
		.maxver = TLS1_2_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2,
		.minver = TLS1_VERSION,
		.maxver = TLS1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_2,
		.minver = TLS1_1_VERSION,
		.maxver = TLS1_1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2,
		.minver = 0,
		.maxver = 0,
	},
};

#define N_VERSION_RANGE_TESTS \
    (sizeof(version_range_tests) / sizeof(*version_range_tests))

static int
test_ssl_enabled_version_range(void)
{
	struct version_range_test *vrt;
	uint16_t minver, maxver;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int failed = 1;
	size_t i;

	if ((ssl_ctx = SSL_CTX_new(TLS_method())) == NULL) { 
		fprintf(stderr, "SSL_CTX_new() returned NULL\n");
		goto failure;
	}
	if ((ssl = SSL_new(ssl_ctx)) == NULL) {
		fprintf(stderr, "SSL_new() returned NULL\n");
		goto failure;
	}

	failed = 0;

	for (i = 0; i < N_VERSION_RANGE_TESTS; i++) {
		vrt = &version_range_tests[i];

		SSL_clear_options(ssl, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 |
		    SSL_OP_NO_TLSv1_2);
		SSL_set_options(ssl, vrt->options);

		minver = maxver = 0xffff;

		if (ssl_enabled_version_range(ssl, &minver, &maxver) != 1) {
			if (vrt->minver != 0 || vrt->maxver != 0) {
				fprintf(stderr, "FAIL: test %zu - failed but "
				    "wanted non-zero versions\n", i);
				failed++;
			}
			continue;
		}
		if (minver != vrt->minver) {
			fprintf(stderr, "FAIL: test %zu - got minver %x, "
			    "want %x\n", i, minver, vrt->minver);
			failed++;
		}
		if (maxver != vrt->maxver) {
			fprintf(stderr, "FAIL: test %zu - got maxver %x, "
			    "want %x\n", i, maxver, vrt->maxver);
			failed++;
		}
	}

 failure:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (failed);
}

struct shared_version_test {
	const long options;
	const uint16_t peerver;
	const uint16_t maxver;
};

static struct shared_version_test shared_version_tests[] = {
	{
		.options = 0,
		.peerver = SSL2_VERSION,
		.maxver = 0,
	},
	{
		.options = 0,
		.peerver = SSL3_VERSION,
		.maxver = 0,
	},
	{
		.options = 0,
		.peerver = TLS1_VERSION,
		.maxver = TLS1_VERSION,
	},
	{
		.options = 0,
		.peerver = TLS1_1_VERSION,
		.maxver = TLS1_1_VERSION,
	},
	{
		.options = 0,
		.peerver = TLS1_2_VERSION,
		.maxver = TLS1_2_VERSION,
	},
	{
		.options = 0,
		.peerver = 0x7f12,
		.maxver = TLS1_2_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1_2,
		.peerver = TLS1_2_VERSION,
		.maxver = TLS1_1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2,
		.peerver = TLS1_2_VERSION,
		.maxver = TLS1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2,
		.peerver = TLS1_2_VERSION,
		.maxver = 0,
	},
	{
		.options = SSL_OP_NO_TLSv1,
		.peerver = TLS1_1_VERSION,
		.maxver = TLS1_1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1,
		.peerver = TLS1_1_VERSION,
		.maxver = 0,
	},
	{
		.options = SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2,
		.peerver = TLS1_1_VERSION,
		.maxver = TLS1_VERSION,
	},
	{
		.options = SSL_OP_NO_TLSv1,
		.peerver = TLS1_VERSION,
		.maxver = 0,
	},
};

#define N_SHARED_VERSION_TESTS \
    (sizeof(shared_version_tests) / sizeof(*shared_version_tests))

static int
test_ssl_max_shared_version(void)
{
	struct shared_version_test *srt;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	uint16_t maxver;
	int failed = 1;
	size_t i;

	if ((ssl_ctx = SSL_CTX_new(TLS_method())) == NULL) { 
		fprintf(stderr, "SSL_CTX_new() returned NULL\n");
		goto failure;
	}
	if ((ssl = SSL_new(ssl_ctx)) == NULL) {
		fprintf(stderr, "SSL_new() returned NULL\n");
		goto failure;
	}

	failed = 0;

	for (i = 0; i < N_SHARED_VERSION_TESTS; i++) {
		srt = &shared_version_tests[i];

		SSL_clear_options(ssl, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 |
		    SSL_OP_NO_TLSv1_2);
		SSL_set_options(ssl, srt->options);

		maxver = 0;

		if (ssl_max_shared_version(ssl, srt->peerver, &maxver) != 1) {
			if (srt->maxver != 0) {
				fprintf(stderr, "FAIL: test %zu - failed but "
				    "wanted non-zero shared version\n", i);
				failed++;
			}
			continue;
		}
		if (maxver != srt->maxver) {
			fprintf(stderr, "FAIL: test %zu - got shared "
			    "version %x, want %x\n", i, maxver, srt->maxver);
			failed++;
		}
	}

 failure:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (failed);
}

int
main(int argc, char **argv)
{
	int failed = 0;

	SSL_library_init();

	failed |= test_ssl_enabled_version_range();
	failed |= test_ssl_max_shared_version();

	if (failed == 0)
		printf("PASS %s\n", __FILE__);

        return (failed);
}
