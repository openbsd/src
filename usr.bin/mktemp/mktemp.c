/*	$OpenBSD: mktemp.c,v 1.15 2009/10/27 23:59:40 deraadt Exp $	*/

/*
 * Copyright (c) 1996, 1997, 2001 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

__dead void usage(void);

int
main(int argc, char *argv[])
{
	int ch, fd, uflag = 0, quiet = 0, tflag = 0, makedir = 0;
	char *cp, *template, *tempfile, *prefix = _PATH_TMP;
	int plen;

	while ((ch = getopt(argc, argv, "dp:qtu")) != -1)
		switch(ch) {
		case 'd':
			makedir = 1;
			break;
		case 'p':
			prefix = optarg;
			tflag = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		default:
			usage();
	}

	/* If no template specified use a default one (implies -t mode) */
	switch (argc - optind) {
	case 1:
		template = argv[optind];
		break;
	case 0:
		template = "tmp.XXXXXXXXXX";
		tflag = 1;
		break;
	default:
		usage();
	}

	if (tflag) {
		if (strchr(template, '/')) {
			if (!quiet)
				warnx("template must not contain directory "
				    "separators in -t mode");
			exit(1);
		}

		cp = getenv("TMPDIR");
		if (cp != NULL && *cp != '\0')
			prefix = cp;
		plen = strlen(prefix);
		while (plen != 0 && prefix[plen - 1] == '/')
			plen--;

		if (asprintf(&tempfile, "%.*s/%s", plen, prefix, template) < 0)
			tempfile = NULL;
	} else
		tempfile = strdup(template);
	if (tempfile == NULL) {
		if (!quiet)
			warnx("cannot allocate memory");
		exit(1);
	}

	if (makedir) {
		if (mkdtemp(tempfile) == NULL) {
			if (!quiet)
				warn("cannot make temp dir %s", tempfile);
			exit(1);
		}

		if (uflag)
			(void)rmdir(tempfile);
	} else {
		if ((fd = mkstemp(tempfile)) < 0) {
			if (!quiet)
				warn("cannot make temp file %s", tempfile);
			exit(1);
		}
		(void)close(fd);

		if (uflag)
			(void)unlink(tempfile);
	}

	(void)puts(tempfile);
	free(tempfile);

	exit(0);
}

__dead void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-dqtu] [-p directory] [template]\n", __progname);
	exit(1);
}
