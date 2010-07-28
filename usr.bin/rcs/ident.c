/*	$OpenBSD: ident.c,v 1.28 2010/07/28 09:07:11 ray Exp $	*/
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

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rcsprog.h"

#define KEYDELIM	'$'	/* keywords delimiter */
#define VALDELIM	':'	/* values delimiter */

static int found = 0;
static int flags = 0;

static void	ident_file(const char *, FILE *);
static void	ident_line(FILE *);

int
ident_main(int argc, char **argv)
{
	int i, ch, status;
	FILE *fp;

	status = 0;

	while ((ch = rcs_getopt(argc, argv, "qV")) != -1) {
		switch(ch) {
		case 'q':
			flags |= QUIET;
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
				warn("%s", argv[i]);
				status = 1;
				continue;
			}

			ident_file(argv[i], fp);
			(void)fclose(fp);
			if (i != argc - 1)
				printf("\n");
		}
	}

	return (status);
}


static void
ident_file(const char *filename, FILE *fp)
{
	int c;

	if (filename != NULL)
		printf("%s:\n", filename);
	else
		filename = "standard input";

	for (c = 0; c != EOF; c = getc(fp)) {
		if (feof(fp) || ferror(fp))
			break;
		if (c == KEYDELIM)
			ident_line(fp);
	}

	if (found == 0 && !(flags & QUIET))
		fprintf(stderr, "ident warning: no id keywords in %s\n",
		    filename);

	found = 0;
}

static void
ident_line(FILE *fp)
{
	int c;
	BUF *bp;
	size_t len;

	bp = buf_alloc(512);

	while ((c = getc(fp)) != VALDELIM) {
		if (c == EOF)
			goto out;

		if (isalpha(c))
			buf_putc(bp, c);
		else
			goto out;
	}

	buf_putc(bp, VALDELIM);

	while ((c = getc(fp)) != KEYDELIM) {
		if (c == EOF)
			goto out;

		if (c == '\n')
			goto out;

		buf_putc(bp, c);
	}

	len = buf_len(bp);
	if (buf_getc(bp, len - 1) != ' ')
		goto out;

	/* append trailing KEYDELIM */
	buf_putc(bp, c);

	/* Append newline for printing. */
	buf_putc(bp, '\n');
	printf("     %c", KEYDELIM);
	fflush(stdout);
	buf_write_fd(bp, STDOUT_FILENO);

	found++;
out:
	if (bp != NULL)
		buf_free(bp);
}

void
ident_usage(void)
{
	fprintf(stderr, "usage: ident [-qV] [file ...]\n");
}
