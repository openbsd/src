/*	$OpenBSD: asa.c,v 1.7 2007/05/17 10:55:16 moritz Exp $	*/
/*	$NetBSD: asa.c,v 1.10 1995/04/21 03:01:41 cgd Exp $	*/

/*
 * Copyright (c) 1993,94 Winning Strategies, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: asa.c,v 1.7 2007/05/17 10:55:16 moritz Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <unistd.h>

static void asa(FILE *);
static void usage(void);

int
main(int argc, char *argv[])
{
	int ch;
	FILE *fp;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch(ch) {
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	fp = stdin;
	do {
		if (*argv) {
			if (!(fp = fopen(*argv, "r"))) {
				warn ("%s", *argv);
				continue;
			}
		}
		if (fp)
			asa (fp);
		if (fp && fp != stdin)
			(void)fclose(fp);
	} while (*argv++);

	exit (0);
}

static void
asa(FILE *f)
{
	char *buf, *lbuf = NULL;
	size_t len;

	if ((buf = fgetln (f, &len)) != NULL) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}
		/* special case the first line  */
		switch (buf[0]) {
		case '0':
			putchar ('\n');
			break;
		case '1':
			putchar ('\f');
			break;
		}

		if (buf[0] && buf[1]) {
			fputs (&buf[1], stdout);
		}

		while ((buf = fgetln(f, &len)) != NULL) {
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			else {
				if ((lbuf = malloc(len + 1)) == NULL)
					err(1, NULL);
				memcpy(lbuf, buf, len);
				lbuf[len] = '\0';
				buf = lbuf;
			}
			switch (buf[0]) {
			default:
			case ' ':
				putchar ('\n');
				break;
			case '0':
				putchar ('\n');
				putchar ('\n');
				break;
			case '1':
				putchar ('\f');
				break;
			case '+':
				putchar ('\r');
				break;
			}

			if (buf[0] && buf[1]) {
				fputs (&buf[1], stdout);
			}
		}
		free(lbuf);

		putchar ('\n');
	}
}

static void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [file ...]\n", __progname);
	exit(1);
}
