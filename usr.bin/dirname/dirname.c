/*	$OpenBSD: dirname.c,v 1.17 2019/01/25 00:19:26 millert Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <millert@openbsd.org>
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
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void __dead usage(void);

int
main(int argc, char *argv[])
{
	int ch;
	char *dir;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if ((dir = dirname(argv[0])) == NULL)
		err(1, "%s", argv[0]);
	puts(dir);
	return 0;
}

extern char *__progname;

static void __dead
usage(void)
{
	(void)fprintf(stderr, "usage: %s pathname\n", __progname);
	exit(1);
}
