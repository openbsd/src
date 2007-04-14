/*	$OpenBSD: fsck_vnd.c,v 1.1 2007/04/14 11:54:00 grunk Exp $ */

/*
 * Copyright (c) 2007 Alexander von Gernler <grunk@pestilenz.org>
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
#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>

__dead void	 usage(void);


__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-fnpy] image ...\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		 ch, i;
	struct stat	 sb;

	while ((ch = getopt(argc, argv, "fnpy")) != -1) {
		switch (ch) {
		case 'f':
		case 'n':
		case 'p':
		case 'y':
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	/* the only check we can do on vnd images */
	for (i=0; i<argc; i++)
		if (stat(argv[i], &sb) == -1)
			err(1, "stat");

	return (0);
}
