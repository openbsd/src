/*	$Id: test-roa.c,v 1.4 2020/11/01 22:28:24 job Exp $ */
/*
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

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include "extern.h"

int verbose;

static void
pem_print(X509 **xp)
{
	BIO     *bio = NULL;
	char    *pem = NULL;

	bio = BIO_new(BIO_s_mem());
	assert(bio != NULL);

	if (0 == PEM_write_bio_X509(bio, *xp)) {
		BIO_free(bio);
		errx(1, "PEM_write_bio_X509: unable to write cert");
	}

	if ((pem = (char *) malloc(bio->num_write + 1)) == NULL)
		err(1, NULL);

	memset(pem, 0, bio->num_write + 1);
	BIO_read(bio, pem, bio->num_write);
	BIO_free(bio);
	printf("%s", pem);
	free(pem);
}

static void
roa_print(const struct roa *p)
{
	char	 buf[128];
	size_t	 i;

	assert(p != NULL);

	printf("Subject key identifier: %s\n", p->ski);
	printf("Authority key identifier: %s\n", p->aki);
	printf("asID: %" PRIu32 "\n", p->asid);
	for (i = 0; i < p->ipsz; i++) {
		ip_addr_print(&p->ips[i].addr,
			p->ips[i].afi, buf, sizeof(buf));
		printf("%5zu: %s (max: %zu)\n", i + 1,
			buf, p->ips[i].maxlength);
	}
}

int
main(int argc, char *argv[])
{
	int		 c, i, ppem, verb = 0;
	X509		*xp = NULL;
	struct roa	*p;


	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	while ((c = getopt(argc, argv, "pv")) != -1)
		switch (c) {
		case 'p':
			ppem++;
			break;
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
		if ((p = roa_parse(&xp, argv[i], NULL)) == NULL)
			break;
		if (verb)
			roa_print(p);
		if (ppem)
			pem_print(&xp);
		roa_free(p);
		X509_free(xp);
	}

	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state(0);
	ERR_free_strings();

	if (i < argc)
		errx(1, "test failed for %s", argv[i]);

	printf("OK\n");
	return 0;
}
