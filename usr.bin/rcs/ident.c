/*	$OpenBSD: ident.c,v 1.11 2006/01/20 14:35:02 xsa Exp $	*/
/*
 * Copyright (c) 2005 Xavier Santolaria <xsa@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include "rcsprog.h"

#define KEYDELIM	'$'	/* keywords delimiter */
#define VALDELIM	':'	/* values delimiter */

static int found = 0;

static int	ident_file(const char *, FILE *);
static int	ident_line(FILE *);

int
ident_main(int argc, char **argv)
{
	int i, ch;
	FILE *fp;

	while ((ch = rcs_getopt(argc, argv, "qV")) != -1) {
		switch(ch) {
		case 'q':
			verbose = 0;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0)
		ident_file(NULL, stdin);
	else {
		for (i = 0; i < argc; i++) {
			if ((fp = fopen(argv[i], "r")) == NULL) {
				cvs_log(LP_ERRNO, "%s", argv[i]);
				continue;
			}

			ident_file(argv[i], fp);
			fclose(fp);
		}
	}

	return (0);
}


static int
ident_file(const char *filename, FILE *fp)
{
	int c;

	if (filename != NULL)
		printf("%s:\n", filename);
	else
		filename = "standard output";

	for (c = 0; c != EOF; (c = getc(fp))) {
		if ((feof(fp)) || (ferror(fp)))
			break;
		if (c == KEYDELIM)
			ident_line(fp);
	}

	if ((found == 0) && (verbose == 1))
		fprintf(stderr, "ident warning: no id keywords in %s\n",
	 	    filename);

	found = 0;

	return (0);
}

static int
ident_line(FILE *fp)
{
	int c;
	char *p, linebuf[1024];

	p = linebuf;

	while ((c = getc(fp)) != VALDELIM) {
		if ((c == EOF) && (feof(fp) | ferror(fp)))
			return (0);

		if (isalpha(c))
			*(p++) = c;
		else
			return (0);
	}

	*(p++) = VALDELIM;

	while ((c = getc(fp)) != KEYDELIM) {
		if ((c == EOF) && (feof(fp) | ferror(fp)))
			return (0);

		if (c == '\n')
			return (0);

		*(p++) = c;
	}

	if (p[-1] != ' ')
		return (0);

	/* append trailing KEYDELIM */
	*(p++) = c;
	*p = '\0';

	found++;
	printf("     %c%s\n", KEYDELIM, linebuf);

	return (0);
}

void
ident_usage(void)
{
	fprintf(stderr, "usage: ident [-qV] [file ...]\n");
}
