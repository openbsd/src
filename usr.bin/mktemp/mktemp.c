/*	$OpenBSD: mktemp.c,v 1.6 2001/10/01 17:08:30 millert Exp $	*/

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
static const char rcsid[] = "$OpenBSD: mktemp.c,v 1.6 2001/10/01 17:08:30 millert Exp $";
#endif /* not lint */                                                        

#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

void
usage()
{
	extern char *__progname;

	(void) fprintf(stderr,
	    "Usage: %s [-dqtu] [-p prefix] template\n", __progname);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, uflag = 0, qflag = 0, tflag = 0, makedir = 0;
	char *cp, *template, *prefix = _PATH_TMP;
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
			qflag = 1;
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

	if (argc - optind != 1)
		usage();

	if (tflag) {
		if (strchr(argv[optind], '/')) {
			if (qflag)
				exit(1);
			else
				errx(1, "template must not contain directory separators in -t mode");
		}

		cp = getenv("TMPDIR");
		if (cp != NULL && *cp != '\0')
			prefix = cp;
		plen = strlen(prefix);
		while (plen != 0 && prefix[plen - 1] == '/')
			plen--;

		template = (char *)malloc(plen + 1 + strlen(argv[optind]) + 1);
		if (template == NULL) {
			if (qflag)
				exit(1);
			else
				errx(1, "Cannot allocate memory");
		}
		memcpy(template, prefix, plen);
		template[plen] = '/';
		strcpy(template + plen + 1, argv[optind]);	/* SAFE */
	} else {
		if ((template = strdup(argv[optind])) == NULL) {
			if (qflag)
				exit(1);
			else
				errx(1, "Cannot allocate memory");
		}
	}

	if (makedir) {
		if (mkdtemp(template) == NULL) {
			if (qflag)
				exit(1);
			else
				err(1, "Cannot make temp dir %s", template);
		}

		if (uflag)
			(void) rmdir(template);
	} else {
		if (mkstemp(template) < 0) {
			if (qflag)
				exit(1);
			else
				err(1, "Cannot create temp file %s", template);
		}

		if (uflag)
			(void) unlink(template);
	}

	(void) puts(template);
	free(template);

	exit(0);
}
