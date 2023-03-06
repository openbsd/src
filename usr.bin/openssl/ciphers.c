/* $OpenBSD: ciphers.c,v 1.18 2023/03/06 14:32:05 tb Exp $ */
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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

#include <stdio.h>
#include <stdlib.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "apps.h"
#include "progs.h"

static struct {
	int usage;
	int use_supported;
	int verbose;
	int version;
} cfg;

static const struct option ciphers_options[] = {
	{
		.name = "h",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.usage,
	},
	{
		.name = "?",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.usage,
	},
	{
		.name = "s",
		.desc = "Only list ciphers that are supported by the TLS method",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.use_supported,
	},
	{
		.name = "tls1",
		.desc = "Use TLS protocol version 1",
		.type = OPTION_VALUE,
		.opt.value = &cfg.version,
		.value = TLS1_VERSION,
	},
	{
		.name = "tls1_1",
		.desc = "Use TLS protocol version 1.1",
		.type = OPTION_VALUE,
		.opt.value = &cfg.version,
		.value = TLS1_1_VERSION,
	},
	{
		.name = "tls1_2",
		.desc = "Use TLS protocol version 1.2",
		.type = OPTION_VALUE,
		.opt.value = &cfg.version,
		.value = TLS1_2_VERSION,
	},
	{
		.name = "tls1_3",
		.desc = "Use TLS protocol version 1.3",
		.type = OPTION_VALUE,
		.opt.value = &cfg.version,
		.value = TLS1_3_VERSION,
	},
	{
		.name = "v",
		.desc = "Provide cipher listing",
		.type = OPTION_VALUE,
		.opt.value = &cfg.verbose,
		.value = 1,
	},
	{
		.name = "V",
		.desc = "Provide cipher listing with cipher suite values",
		.type = OPTION_VALUE,
		.opt.value = &cfg.verbose,
		.value = 2,
	},
	{ NULL },
};

static void
ciphers_usage(void)
{
	fprintf(stderr, "usage: ciphers [-hsVv] [-tls1] [-tls1_1] [-tls1_2] "
	    "[-tls1_3] [cipherlist]\n");
	options_usage(ciphers_options);
}

int
ciphers_main(int argc, char **argv)
{
	char *cipherlist = NULL;
	STACK_OF(SSL_CIPHER) *ciphers;
	STACK_OF(SSL_CIPHER) *supported_ciphers = NULL;
	const SSL_CIPHER *cipher;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	uint16_t value;
	int i, rv = 0;
	char *desc;

	if (pledge("stdio rpath", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));

	if (options_parse(argc, argv, ciphers_options, &cipherlist,
	    NULL) != 0) {
		ciphers_usage();
		return (1);
	}

	if (cfg.usage) {
		ciphers_usage();
		return (1);
	}

	if ((ssl_ctx = SSL_CTX_new(TLS_method())) == NULL)
		goto err;

	if (cfg.version != 0) {
		if (!SSL_CTX_set_min_proto_version(ssl_ctx,
		    cfg.version))
			goto err;
		if (!SSL_CTX_set_max_proto_version(ssl_ctx,
		    cfg.version))
			goto err;
	}

	if (cipherlist != NULL) {
		if (SSL_CTX_set_cipher_list(ssl_ctx, cipherlist) == 0)
			goto err;
	}

	if ((ssl = SSL_new(ssl_ctx)) == NULL)
		goto err;

	if (cfg.use_supported) {
		if ((supported_ciphers =
		    SSL_get1_supported_ciphers(ssl)) == NULL)
			goto err;
		ciphers = supported_ciphers;
	} else {
		if ((ciphers = SSL_get_ciphers(ssl)) == NULL)
			goto err;
	}

	for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
		cipher = sk_SSL_CIPHER_value(ciphers, i);
		if (cfg.verbose == 0) {
			fprintf(stdout, "%s%s", (i ? ":" : ""),
			    SSL_CIPHER_get_name(cipher));
			continue;
		}
		if (cfg.verbose > 1) {
			value = SSL_CIPHER_get_value(cipher);
			fprintf(stdout, "%-*s0x%02X,0x%02X - ", 10, "",
			    ((value >> 8) & 0xff), (value & 0xff));
		}
		desc = SSL_CIPHER_description(cipher, NULL, 0);
		if (strcmp(desc, "OPENSSL_malloc Error") == 0) {
			fprintf(stderr, "out of memory\n");
			goto err;
		}
		fprintf(stdout, "%s", desc);
		free(desc);
	}
	if (cfg.verbose == 0)
		fprintf(stdout, "\n");

	goto done;

 err:
	ERR_print_errors_fp(stderr);
	rv = 1;

 done:
	sk_SSL_CIPHER_free(supported_ciphers);
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (rv);
}
