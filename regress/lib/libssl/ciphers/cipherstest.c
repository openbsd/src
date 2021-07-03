/*
 * Copyright (c) 2015, 2020 Joel Sing <jsing@openbsd.org>
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

#include <err.h>
#include <stdio.h>
#include <string.h>

int ssl3_num_ciphers(void);
const SSL_CIPHER *ssl3_get_cipher(unsigned int u);

int ssl_parse_ciphersuites(STACK_OF(SSL_CIPHER) **out_ciphers, const char *str);

static inline int
ssl_aes_is_accelerated(void)
{
#if defined(__i386__) || defined(__x86_64__)
	return ((OPENSSL_cpu_caps() & (1ULL << 57)) != 0);
#else
	return (0);
#endif
}

static int
check_cipher_order(void)
{
	unsigned long id, prev_id = 0;
	const SSL_CIPHER *cipher;
	int num_ciphers;
	int i;

	num_ciphers = ssl3_num_ciphers();

	for (i = 1; i <= num_ciphers; i++) {
		/*
		 * For some reason, ssl3_get_cipher() returns ciphers in
		 * reverse order.
		 */
		if ((cipher = ssl3_get_cipher(num_ciphers - i)) == NULL) {
			fprintf(stderr, "FAIL: ssl3_get_cipher(%d) returned "
			    "NULL\n", i);
			return 1;
		}
		if ((id = SSL_CIPHER_get_id(cipher)) <= prev_id) {
			fprintf(stderr, "FAIL: ssl3_ciphers is not sorted by "
			    "id - cipher %d (%lx) <= cipher %d (%lx)\n",
			    i, id, i - 1, prev_id);
			return 1;
		}
		prev_id = id;
	}

	return 0;
}

static int
cipher_find_test(void)
{
	STACK_OF(SSL_CIPHER) *ciphers;
	const SSL_CIPHER *cipher;
	unsigned char buf[2];
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int ret = 1;
	int i;

	if ((ssl_ctx = SSL_CTX_new(TLS_method())) == NULL) {
		fprintf(stderr, "SSL_CTX_new() returned NULL\n");
		goto failure;
	}
	if ((ssl = SSL_new(ssl_ctx)) == NULL) {
		fprintf(stderr, "SSL_new() returned NULL\n");
		goto failure;
	}
	if (!SSL_set_cipher_list(ssl, "ALL")) {
		fprintf(stderr, "SSL_set_cipher_list failed\n");
		goto failure;
	}

	if ((ciphers = SSL_get_ciphers(ssl)) == NULL) {
		fprintf(stderr, "no ciphers\n");
		goto failure;
	}

	for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
		uint16_t cipher_value;

		cipher = sk_SSL_CIPHER_value(ciphers, i);
		cipher_value = SSL_CIPHER_get_value(cipher);

		buf[0] = cipher_value >> 8;
		buf[1] = cipher_value & 0xff;

		if ((cipher = SSL_CIPHER_find(ssl, buf)) == NULL) {
			fprintf(stderr,
			    "SSL_CIPHER_find() returned NULL for %s\n",
			    SSL_CIPHER_get_name(cipher));
			goto failure;
		}

		if (SSL_CIPHER_get_value(cipher) != cipher_value) {
			fprintf(stderr,
			    "got cipher with value 0x%x, want 0x%x\n",
			    SSL_CIPHER_get_value(cipher), cipher_value);
			goto failure;
		}
	}

	ret = 0;

 failure:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (ret);
}

static int
cipher_get_by_value_tests(void)
{
	STACK_OF(SSL_CIPHER) *ciphers;
	const SSL_CIPHER *cipher;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	unsigned long id;
	uint16_t value;
	int ret = 1;
	int i;

	if ((ssl_ctx = SSL_CTX_new(SSLv23_method())) == NULL) {
		fprintf(stderr, "SSL_CTX_new() returned NULL\n");
		goto failure;
	}
	if ((ssl = SSL_new(ssl_ctx)) == NULL) {
		fprintf(stderr, "SSL_new() returned NULL\n");
		goto failure;
	}

	if ((ciphers = SSL_get_ciphers(ssl)) == NULL) {
		fprintf(stderr, "no ciphers\n");
		goto failure;
	}

	for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
		cipher = sk_SSL_CIPHER_value(ciphers, i);

		id = SSL_CIPHER_get_id(cipher);
		if (SSL_CIPHER_get_by_id(id) == NULL) {
			fprintf(stderr, "SSL_CIPHER_get_by_id() failed "
			    "for %s (0x%lx)\n", SSL_CIPHER_get_name(cipher),
			    id);
			goto failure;
		}

		value = SSL_CIPHER_get_value(cipher);
		if (SSL_CIPHER_get_by_value(value) == NULL) {
			fprintf(stderr, "SSL_CIPHER_get_by_value() failed "
			    "for %s (0x%04hx)\n", SSL_CIPHER_get_name(cipher),
			    value);
			goto failure;
		}
	}

	ret = 0;

 failure:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (ret);
}

struct parse_ciphersuites_test {
	const char *str;
	const int want;
	const unsigned long cids[32];
};

struct parse_ciphersuites_test parse_ciphersuites_tests[] = {
	{
		/* LibreSSL names. */
		.str = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256:AEAD-AES128-GCM-SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
		},
	},
	{
		/* OpenSSL names. */
		.str = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
		},
	},
	{
		/* Different priority order. */
		.str = "AEAD-AES128-GCM-SHA256:AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_128_GCM_SHA256,
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
		},
	},
	{
		/* Known but unsupported names. */
		.str = "AEAD-AES256-GCM-SHA384:AEAD-AES128-CCM-SHA256:AEAD-AES128-CCM-8-SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
		},
	},
	{
		/* Empty string means no TLSv1.3 ciphersuites. */
		.str = "",
		.want = 1,
		.cids = { 0 },
	},
	{
		.str = "TLS_CHACHA20_POLY1305_SHA256:TLS_NOT_A_CIPHERSUITE",
		.want = 0,
	},
	{
		.str = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256,TLS_AES_128_GCM_SHA256",
		.want = 0,
	},
};

#define N_PARSE_CIPHERSUITES_TESTS \
    (sizeof(parse_ciphersuites_tests) / sizeof(*parse_ciphersuites_tests))

static int
parse_ciphersuites_test(void)
{
	struct parse_ciphersuites_test *pct;
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	SSL_CIPHER *cipher;
	int failed = 1;
	int j, ret;
	size_t i;

	for (i = 0; i < N_PARSE_CIPHERSUITES_TESTS; i++) {
		pct = &parse_ciphersuites_tests[i];

		ret = ssl_parse_ciphersuites(&ciphers, pct->str);
		if (ret != pct->want) {
			fprintf(stderr, "FAIL: test %zu - "
			    "ssl_parse_ciphersuites returned %d, want %d\n",
			    i, ret, pct->want);
			goto failed;
		}
		if (ret == 0)
			continue;

		for (j = 0; j < sk_SSL_CIPHER_num(ciphers); j++) {
			cipher = sk_SSL_CIPHER_value(ciphers, j);
			if (SSL_CIPHER_get_id(cipher) == pct->cids[j])
				continue;
			fprintf(stderr, "FAIL: test %zu - got cipher %d with "
			    "id %lx, want %lx\n", i, j,
			    SSL_CIPHER_get_id(cipher), pct->cids[j]);
			goto failed;
		}
		if (pct->cids[j] != 0) {
			fprintf(stderr, "FAIL: test %zu - got %d ciphers, "
			    "expected more", i, sk_SSL_CIPHER_num(ciphers));
			goto failed;
		}
	}

	failed = 0;

 failed:
	sk_SSL_CIPHER_free(ciphers);

	return failed;
}

struct cipher_set_test {
	int ctx_ciphersuites_first;
	const char *ctx_ciphersuites;
	const char *ctx_rulestr;
	int ssl_ciphersuites_first;
	const char *ssl_ciphersuites;
	const char *ssl_rulestr;
	int cids_aes_accel_fixup;
	unsigned long cids[32];
};

struct cipher_set_test cipher_set_tests[] = {
	{
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids_aes_accel_fixup = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids_aes_accel_fixup = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_ciphersuites_first = 1,
		.ctx_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_ciphersuites_first = 1,
		.ssl_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_ciphersuites_first = 0,
		.ctx_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_ciphersuites_first = 0,
		.ssl_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_ciphersuites_first = 1,
		.ssl_ciphersuites = "",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_ciphersuites_first = 0,
		.ssl_ciphersuites = "",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.ssl_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
};

#define N_CIPHER_SET_TESTS \
    (sizeof(cipher_set_tests) / sizeof(*cipher_set_tests))

static int
cipher_set_test(void)
{
	struct cipher_set_test *cst;
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	SSL_CIPHER *cipher;
	SSL_CTX *ctx = NULL;
	SSL *ssl = NULL;
	int failed = 0;
	size_t i;
	int j;

	for (i = 0; i < N_CIPHER_SET_TESTS; i++) {
		cst = &cipher_set_tests[i];

		if (!ssl_aes_is_accelerated() && cst->cids_aes_accel_fixup) {
			cst->cids[0] = TLS1_3_CK_CHACHA20_POLY1305_SHA256;
			cst->cids[1] = TLS1_3_CK_AES_256_GCM_SHA384;
		}

		if ((ctx = SSL_CTX_new(TLS_method())) == NULL)
			errx(1, "SSL_CTX_new");

		if (cst->ctx_ciphersuites_first && cst->ctx_ciphersuites != NULL) {
			if (!SSL_CTX_set_ciphersuites(ctx, cst->ctx_ciphersuites))
				errx(1, "SSL_CTX_set_ciphersuites");
		}
		if (cst->ctx_rulestr != NULL) {
			if (!SSL_CTX_set_cipher_list(ctx, cst->ctx_rulestr))
				errx(1, "SSL_CTX_set_cipher_list");
		}
		if (!cst->ctx_ciphersuites_first && cst->ctx_ciphersuites != NULL) {
			if (!SSL_CTX_set_ciphersuites(ctx, cst->ctx_ciphersuites))
				errx(1, "SSL_CTX_set_ciphersuites");
		}

		/* XXX - check SSL_CTX_get_ciphers(ctx) */

		if ((ssl = SSL_new(ctx)) == NULL)
			errx(1, "SSL_new");

		if (cst->ssl_ciphersuites_first && cst->ssl_ciphersuites != NULL) {
			if (!SSL_set_ciphersuites(ssl, cst->ssl_ciphersuites))
				errx(1, "SSL_set_ciphersuites");
		}
		if (cst->ssl_rulestr != NULL) {
			if (!SSL_set_cipher_list(ssl, cst->ssl_rulestr))
				errx(1, "SSL_set_cipher_list");
		}
		if (!cst->ssl_ciphersuites_first && cst->ssl_ciphersuites != NULL) {
			if (!SSL_set_ciphersuites(ssl, cst->ssl_ciphersuites))
				errx(1, "SSL_set_ciphersuites");
		}

		ciphers = SSL_get_ciphers(ssl);

		for (j = 0; j < sk_SSL_CIPHER_num(ciphers); j++) {
			cipher = sk_SSL_CIPHER_value(ciphers, j);
			if (SSL_CIPHER_get_id(cipher) == cst->cids[j])
				continue;
			fprintf(stderr, "FAIL: test %zu - got cipher %d with "
			    "id %lx, want %lx\n", i, j,
			    SSL_CIPHER_get_id(cipher), cst->cids[j]);
			failed |= 1;
		}
		if (cst->cids[j] != 0) {
			fprintf(stderr, "FAIL: test %zu - got %d ciphers, "
			    "expected more", i, sk_SSL_CIPHER_num(ciphers));
			failed |= 1;
		}

		SSL_CTX_free(ctx);
		SSL_free(ssl);
	}

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= check_cipher_order();

	failed |= cipher_find_test();
	failed |= cipher_get_by_value_tests();

	failed |= parse_ciphersuites_test();
	failed |= cipher_set_test();

	return (failed);
}
