/*	$OpenBSD: mlinks.c,v 1.3 2016/11/05 16:43:50 schwarze Exp $ */
/*
 * Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
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
 *
 * Some operating systems need MLINKS for pages with more than one name.
 * Extract these in a format suitable for portable LibreSSL.
 */
#include <err.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dbm_map.h"
#include "dbm.h"

int
main(int argc, char *argv[])
{
	const int32_t	*pp;  /* Page record in the pages table. */
	const char	*np;  /* Names of the page. */
	const char	*fp;  /* Primary filename of the page. */
	const char	*ep;  /* Filname extension including the dot. */
	size_t		 flen, nlen;
	int32_t		 i, npages;

	if (argc != 2)
		errx(1, "usage: mlinks filename");

	if (dbm_open(argv[1]) == -1)
		err(1, "%s", argv[1]);

	pp = dbm_getint(4);
	npages = be32toh(*pp++);
	if (npages <= 0)
		errx(1, "database empty or corrupt: %d pages", npages);

	for (i = 0; i < npages; i++, pp += 5) {
		np = dbm_get(pp[0]);
		if (np == NULL)
			errx(1, "database corrupt: bad name pointer");

		/* Skip files with just one name. */
		if (strchr(np, '\0')[1] == '\0')
			continue;

		fp = dbm_get(pp[4]);
		if (fp == NULL)
			errx(1, "database corrupt: bad file pointer");

		/* Skip the file type byte. */
		fp++;

		/* Skip directory parts of filenames. */
		ep = strrchr(fp, '/');
		if (ep != NULL)
			fp = ep + 1;

		ep = strrchr(fp, '.');
		if (ep == NULL)
			errx(1, "no filename extension: %s", fp);
		flen = ep - fp;

		while (*np != '\0') {

			/* Skip the name type byte. */
			np++;

			/* Skip the primary filename. */
			nlen = strlen(np);
			if (nlen == flen && strncmp(fp, np, nlen) == 0) {
				np = strchr(np, '\0') + 1;
				continue;
			}

			/* Describe the desired mlink. */
			printf("%s,", fp);
			while (*np != '\0')
				putchar(*np++);
			np++;
			puts(ep);
		}
	}
	dbm_close();
	return 0;
}
