/*	$OpenBSD: mktemp.c,v 1.21 2015/10/07 06:43:15 deraadt Exp $	*/

/*
 * Copyright (c) 1996, 1997, 2001-2003, 2013
 *	Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__dead void usage(void);
__dead void fatal(const char *, ...) __attribute__((__format__(printf, 1, 2)));
__dead void fatalx(const char *, ...) __attribute__((__format__(printf, 1, 2)));

static int quiet;

int
main(int argc, char *argv[])
{
	int ch, fd, uflag = 0, tflag = 0, makedir = 0;
	char *cp, *template, *tempfile, *prefix = _PATH_TMP;
	size_t len;

	if (tame("stdio rpath wpath cpath", NULL) == -1)
		err(1, "tame");

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

	len = strlen(template);
	if (len < 6 || strcmp(&template[len - 6], "XXXXXX")) {
		fatalx("insufficient number of Xs in template `%s'",
		    template);
	}
	if (tflag) {
		if (strchr(template, '/')) {
			fatalx("template must not contain directory "
			    "separators in -t mode");
		}

		cp = getenv("TMPDIR");
		if (cp != NULL && *cp != '\0')
			prefix = cp;
		len = strlen(prefix);
		while (len != 0 && prefix[len - 1] == '/')
			len--;

		if (asprintf(&tempfile, "%.*s/%s", (int)len, prefix, template) < 0)
			tempfile = NULL;
	} else
		tempfile = strdup(template);

	if (tempfile == NULL)
		fatalx("cannot allocate memory");

	if (makedir) {
		if (mkdtemp(tempfile) == NULL)
			fatal("cannot make temp dir %s", tempfile);
		if (uflag)
			(void)rmdir(tempfile);
	} else {
		if ((fd = mkstemp(tempfile)) < 0)
			fatal("cannot make temp file %s", tempfile);
		(void)close(fd);
		if (uflag)
			(void)unlink(tempfile);
	}

	(void)puts(tempfile);
	free(tempfile);

	exit(EXIT_SUCCESS);
}

__dead void
fatal(const char *fmt, ...)
{
	if (!quiet) {
		va_list ap;

		va_start(ap, fmt);
		vwarn(fmt, ap);
		va_end(ap);
	}
	exit(EXIT_FAILURE);
}

__dead void
fatalx(const char *fmt, ...)
{
	if (!quiet) {
		va_list ap;

		va_start(ap, fmt);
		vwarnx(fmt, ap);
		va_end(ap);
	}
	exit(EXIT_FAILURE);
}

__dead void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-dqtu] [-p directory] [template]\n", __progname);
	exit(EXIT_FAILURE);
}
