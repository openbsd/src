/*	$OpenBSD: dc.c,v 1.1 2003/09/19 17:58:25 otto Exp $	*/

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
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

#ifndef lint
static const char rcsid[] = "$OpenBSD: dc.c,v 1.1 2003/09/19 17:58:25 otto Exp $";
#endif /* not lint */

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

static __dead void	usage(void);

extern char 		*__progname;

static __dead void
usage(void)
{
	fprintf(stderr, "usage: %s [file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	FILE		*file;
	struct source	src;

	if (argc > 2)
		usage();


	init_bmachine();
	setlinebuf(stdout);
	setlinebuf(stderr);
	if (argc == 2 && strcmp(argv[1], "-") != 0) {
		file = fopen(argv[1], "r");
		if (file == NULL)
			err(1, "cannot open file %s", argv[1]);
		src_setstream(&src, file);
		reset_bmachine(&src);
		eval();
		fclose(file);
	}
	/*
	 * BSD dc and Solaris dc continue with stdin after processing
	 * the file given as the argument.
	 */
	src_setstream(&src, stdin);
	reset_bmachine(&src);
	eval();

	return 0;
}
