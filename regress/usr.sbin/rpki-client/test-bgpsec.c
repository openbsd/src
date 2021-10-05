/*	$Id: test-bgpsec.c,v 1.1 2021/10/05 11:23:16 job Exp $ */
/*
 * Copyright (c) 2021 Job Snijders <job@sobornost.net>
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/socket.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>

#include "extern.h"

#include "test-common.c"

int verbose;

static void
cert_print(const struct cert *p)
{
	size_t	 i;
	char	 buf1[64], buf2[64];
	int	 sockt;

	assert(p != NULL);

	if (p->crl != NULL)
		printf("Revocation list: %s\n", p->crl);
	printf("Subject key identifier: %s\n", pretty_key_id(p->ski));
	if (p->aki != NULL)
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
	if (p->aia != NULL)
		printf("Authority info access: %s\n", p->aia);

	for (i = 0; i < p->asz; i++)
		switch (p->as[i].type) {
		case CERT_AS_ID:
			printf("%5zu: AS: %"
				PRIu32 "\n", i + 1, p->as[i].id);
			break;
		case CERT_AS_RANGE:
			printf("%5zu: AS: %"
				PRIu32 "--%" PRIu32 "\n", i + 1,
				p->as[i].range.min, p->as[i].range.max);
			break;
		default:
			printf("%5zu: AS: invalid element", i + 1);
		}
}

int
main(int argc, char *argv[])
{
	int		 c, i, verb = 0;
	X509		*xp = NULL;
	struct cert	*p;

	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	while ((c = getopt(argc, argv, "v")) != -1)
		switch (c) {
		case 'v':
			verb++;
			break;
		default:
			errx(1, "bad argument %c", c);
		}

	argv += optind;
	argc -= optind;

	if (argc == 0)
		errx(1, "argument missing");

	for (i = 0; i < argc; i++) {
		p = cert_parse(&xp, argv[i]);
		if (p == NULL)
			break;
		if (verb)
			cert_print(p);
		cert_free(p);
		X509_free(xp);
	}

	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();

	if (i < argc)
		errx(1, "test failed for %s", argv[i]);

	printf("OK\n");
	return 0;
}
