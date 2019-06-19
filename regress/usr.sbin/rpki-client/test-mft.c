/*	$Id: test-mft.c,v 1.1 2019/06/18 12:09:07 claudio Exp $ */
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "extern.h"

static void
mft_print(const struct mft *p)
{
	size_t	 i;

	assert(p != NULL);

	printf("Subject key identifier: %s\n", p->ski);
	printf("Authority key identifier: %s\n", p->aki);
	for (i = 0; i < p->filesz; i++)
		printf("%5zu: %s\n", i + 1, p->files[i].file);
}


int
main(int argc, char *argv[])
{
	int		 c, i, verb = 0, force = 0;
	struct mft	*p;
	X509		*xp = NULL;

	SSL_library_init();
	SSL_load_error_strings();

	while (-1 != (c = getopt(argc, argv, "fv")))
		switch (c) {
		case 'f':
			force = 1;
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
		if ((p = mft_parse(&xp, argv[i], force)) == NULL)
			break;
		if (verb)
			mft_print(p);
		mft_free(p);
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
