/*	$OpenBSD: skeyinfo.c,v 1.10 2002/05/16 17:26:58 millert Exp $	*/

/*
 * Copyright (c) 1997, 2001, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
usage()
{
	(void)fprintf(stderr, "usage: %s [-v] [user]\n", __progname);
	exit(1);
}
