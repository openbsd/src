/*	$OpenBSD: mktemp.c,v 1.7 2001/10/11 00:05:55 millert Exp $	*/

/*
 * Copyright (c) 1996, 1997, 2001 Todd C. Miller <Todd.Miller@courtesan.com>
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

#ifndef lint                                                              
static const char rcsid[] = "$OpenBSD: mktemp.c,v 1.7 2001/10/11 00:05:55 millert Exp $";
#endif /* not lint */                                                        

#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

__dead void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, fd, uflag = 0, quiet = 0, tflag = 0, makedir = 0;
	char *cp, *template, *tempfile, *prefix = _PATH_TMP;
	size_t plen;

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
				warnx("template must not contain directory separators in -t mode");
			exit(1);
		}

		cp = getenv("TMPDIR");
		if (cp != NULL && *cp != '\0')
			prefix = cp;
		plen = strlen(prefix);
		while (plen != 0 && prefix[plen - 1] == '/')
			plen--;

		tempfile = (char *)malloc(plen + 1 + strlen(template) + 1);
		if (tempfile == NULL) {
			if (!quiet)
				warnx("cannot allocate memory");
			exit(1);
		}
		(void)memcpy(tempfile, prefix, plen);
		tempfile[plen] = '/';
		(void)strcpy(tempfile + plen + 1, template);	/* SAFE */
	} else {
		if ((tempfile = strdup(template)) == NULL) {
			if (!quiet)
				warnx("cannot allocate memory");
			exit(1);
		}
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
usage()
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "Usage: %s [-dqtu] [-p prefix] [template]\n", __progname);
	exit(1);
}
