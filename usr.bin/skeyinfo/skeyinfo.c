/*	$OpenBSD: skeyinfo.c,v 1.14 2003/06/17 21:56:26 millert Exp $	*/

/*
 * Copyright (c) 1997, 2001, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <err.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <skey.h>

extern char *__progname;

void usage(void);

int
main(int argc, char **argv)
{
	struct passwd *pw;
	struct skey key;
	char *name = NULL;
	int error, ch, verbose = 0;

	while ((ch = getopt(argc, argv, "v")) != -1)
		switch(ch) {
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (argc == 1)
		name = argv[0];
	else if (argc > 1)
		usage();

	if (name && getuid() != 0)
		errx(1, "only root may specify an alternate user");

	if (name) {
		if ((pw = getpwnam(name)) == NULL)
			errx(1, "no passwd entry for %s", name);
	} else {
		if ((pw = getpwuid(getuid())) == NULL)
			errx(1, "no passwd entry for uid %u", getuid());
	}

	if ((name = strdup(pw->pw_name)) == NULL)
		err(1, "cannot allocate memory");
	sevenbit(name);

	error = skeylookup(&key, name);
	switch (error) {
		case 0:		/* Success! */
			if (verbose)
				(void)printf("otp-%s ", skey_get_algorithm());
			(void)printf("%d %s\n", key.n - 1, key.seed);
			break;
		case -1:	/* File error */
			err(1, "cannot open %s/%s", _PATH_SKEYDIR, name);
			break;
		case 1:		/* Unknown user */
			errx(1, "%s is not listed in %s", name, _PATH_SKEYDIR);
			break;
	}
	(void)fclose(key.keyfile);

	exit(error ? 1 : 0);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-v] [user]\n", __progname);
	exit(1);
}
