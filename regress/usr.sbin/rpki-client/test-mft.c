/*	$Id: test-mft.c,v 1.33 2025/10/23 05:35:46 tb Exp $ */
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

#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include <err.h>
#include <resolv.h>	/* b64_ntop */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>

#include "extern.h"

int outformats;
int verbose;
int filemode = 1;
int experimental;

int
main(int argc, char *argv[])
{
	int		 c, i, verb = 0;
	struct mft	*p;
	struct cert	*cert = NULL;
	unsigned char	*buf;
	size_t		 len;

	x509_init_oid();

	while (-1 != (c = getopt(argc, argv, "pv")))
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
		buf = load_file(argv[i], &len);
		if ((p = mft_parse(&cert, argv[i], -1, buf, len)) == NULL) {
			free(buf);
			break;
		}
		if (verb)
			mft_print(cert, p);
		free(buf);
		mft_free(p);
		cert_free(cert);
		cert = NULL;
	}

	if (i < argc)
		errx(1, "test failed for %s", argv[i]);

	printf("OK\n");
	return 0;
}

time_t
get_current_time(void)
{
	return time(NULL);
}
