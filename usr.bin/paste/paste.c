/*	$OpenBSD: paste.c,v 1.26 2018/08/04 19:19:37 schwarze Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *delim;
int delimcnt;

int	tr(char *);
__dead void usage(void);
void	parallel(char **);
void	sequential(char **);

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch, seq;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	seq = 0;
	while ((ch = getopt(argc, argv, "d:s")) != -1) {
		switch (ch) {
		case 'd':
			delimcnt = tr(delim = optarg);
			break;
		case 's':
			seq = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if (delim == NULL) {
		delimcnt = 1;
		delim = "\t";
	}

	if (seq)
		sequential(argv);
	else
		parallel(argv);
	return 0;
}

struct list {
	SIMPLEQ_ENTRY(list) entries;
	FILE *fp;
	int cnt;
	char *name;
};

void
parallel(char **argv)
{
	SIMPLEQ_HEAD(, list) head = SIMPLEQ_HEAD_INITIALIZER(head);
	struct list *lp;
	char *line, *p;
	size_t linesize;
	ssize_t len;
	int cnt;
	int opencnt, output;
	char ch;

	for (cnt = 0; (p = *argv) != NULL; ++argv, ++cnt) {
		if ((lp = malloc(sizeof(*lp))) == NULL)
			err(1, NULL);

		if (p[0] == '-' && p[1] == '\0')
			lp->fp = stdin;
		else if ((lp->fp = fopen(p, "r")) == NULL)
			err(1, "%s", p);
		lp->cnt = cnt;
		lp->name = p;
		SIMPLEQ_INSERT_TAIL(&head, lp, entries);
	}

	line = NULL;
	linesize = 0;

	for (opencnt = cnt; opencnt;) {
		output = 0;
		SIMPLEQ_FOREACH(lp, &head, entries) {
			if (lp->fp == NULL) {
				if (output && lp->cnt &&
				    (ch = delim[(lp->cnt - 1) % delimcnt]))
					putchar(ch);
				continue;
			}
			if ((len = getline(&line, &linesize, lp->fp)) == -1) {
				if (ferror(lp->fp))
					err(1, "%s", lp->fp == stdin ?
					    "getline" : lp->name);
				if (--opencnt == 0)
					break;
				if (lp->fp != stdin)
					fclose(lp->fp);
				lp->fp = NULL;
				if (output && lp->cnt &&
				    (ch = delim[(lp->cnt - 1) % delimcnt]))
					putchar(ch);
				continue;
			}
			if (line[len - 1] == '\n')
				line[len - 1] = '\0';
			/*
			 * make sure that we don't print any delimiters
			 * unless there's a non-empty file.
			 */
			if (!output) {
				output = 1;
				for (cnt = 0; cnt < lp->cnt; ++cnt)
					if ((ch = delim[cnt % delimcnt]))
						putchar(ch);
			} else if ((ch = delim[(lp->cnt - 1) % delimcnt]))
				putchar(ch);
			fputs(line, stdout);
		}
		if (output)
			putchar('\n');
	}
	free(line);
}

void
sequential(char **argv)
{
	FILE *fp;
	char *line, *p;
	size_t linesize;
	ssize_t len;
	int cnt;

	line = NULL;
	linesize = 0;
	for (; (p = *argv) != NULL; ++argv) {
		if (p[0] == '-' && p[1] == '\0')
			fp = stdin;
		else if ((fp = fopen(p, "r")) == NULL) {
			warn("%s", p);
			continue;
		}
		cnt = -1;
		while ((len = getline(&line, &linesize, fp)) != -1) {
			if (line[len - 1] == '\n')
				line[len - 1] = '\0';
			if (cnt >= 0)
				putchar(delim[cnt]);
			if (++cnt == delimcnt)
				cnt = 0;
			fputs(line, stdout);
		}
		if (ferror(fp))
			err(1, "%s", fp == stdin ? "getline" : p);
		if (cnt >= 0)
			putchar('\n');
		if (fp != stdin)
			fclose(fp);
	}
	free(line);
}

int
tr(char *arg)
{
	int cnt;
	char ch, *p;

	for (p = arg, cnt = 0; (ch = *p++) != '\0'; ++arg, ++cnt) {
		if (ch == '\\') {
			switch (ch = *p++) {
			case 'n':
				*arg = '\n';
				break;
			case 't':
				*arg = '\t';
				break;
			case '0':
				*arg = '\0';
				break;
			default:
				*arg = ch;
				break;
			}
		} else
			*arg = ch;
	}

	if (cnt == 0)
		errx(1, "no delimiters specified");
	return cnt;
}

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-s] [-d list] file ...\n", __progname);
	exit(1);
}
