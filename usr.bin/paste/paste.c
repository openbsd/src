/*	$OpenBSD: paste.c,v 1.16 2007/08/06 19:16:06 sobrado Exp $	*/

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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)paste.c	5.7 (Berkeley) 10/30/90";*/
static char rcsid[] = "$OpenBSD: paste.c,v 1.16 2007/08/06 19:16:06 sobrado Exp $";
#endif /* not lint */

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
void	usage(void);
void	parallel(char **);
void	sequential(char **);

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch, seq;

	seq = 0;
	while ((ch = getopt(argc, argv, "d:s")) != -1)
		switch(ch) {
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
	argc -= optind;
	argv += optind;

	if (!delim) {
		delimcnt = 1;
		delim = "\t";
	}

	if (seq)
		sequential(argv);
	else
		parallel(argv);
	exit(0);
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
	int cnt;
	char ch, *p;
	int opencnt, output;
	char *buf, *lbuf;
	size_t len;

	for (cnt = 0; (p = *argv); ++argv, ++cnt) {
		if (!(lp = malloc(sizeof(struct list))))
			err(1, "malloc");

		if (p[0] == '-' && !p[1])
			lp->fp = stdin;
		else if (!(lp->fp = fopen(p, "r")))
			err(1, "%s", p);
		lp->cnt = cnt;
		lp->name = p;
		SIMPLEQ_INSERT_TAIL(&head, lp, entries);
	}

	for (opencnt = cnt; opencnt;) {
		output = 0;
		SIMPLEQ_FOREACH(lp, &head, entries) {
			lbuf = NULL;
			if (!lp->fp) {
				if (output && lp->cnt &&
				    (ch = delim[(lp->cnt - 1) % delimcnt]))
					putchar(ch);
				continue;
			}
			if (!(buf = fgetln(lp->fp, &len))) {
				if (!--opencnt)
					break;
				lp->fp = NULL;
				if (output && lp->cnt &&
				    (ch = delim[(lp->cnt - 1) % delimcnt]))
					putchar(ch);
				continue;
			}
			if (*(buf + len - 1) == '\n')
				*(buf + len - 1) = '\0';
			else {
				if ((lbuf = malloc(len + 1)) == NULL)
					err(1, "malloc");
				memcpy(lbuf, buf, len);
				lbuf[len] = '\0';
				buf = lbuf;
			}
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
			(void)printf("%s", buf);
			if (lbuf)
				free(lbuf);
		}
		if (output)
			putchar('\n');
	}
}

void
sequential(char **argv)
{
	FILE *fp;
	int cnt;
	char ch, *p, *dp;
	char *buf, *lbuf;
	size_t len;

	for (; (p = *argv); ++argv) {
		lbuf = NULL;
		if (p[0] == '-' && !p[1])
			fp = stdin;
		else if (!(fp = fopen(p, "r"))) {
			warn("%s", p);
			continue;
		}
		if ((buf = fgetln(fp, &len))) {
			for (cnt = 0, dp = delim;;) {
				if (*(buf + len - 1) == '\n')
					*(buf + len - 1) = '\0';
				else {
					if ((lbuf = malloc(len + 1)) == NULL)
						err(1, "malloc");
					memcpy(lbuf, buf, len);
					lbuf[len] = '\0';
					buf = lbuf;
				}
				(void)printf("%s", buf);
				if (!(buf = fgetln(fp, &len)))
					break;
				if ((ch = *dp++))
					putchar(ch);
				if (++cnt == delimcnt) {
					dp = delim;
					cnt = 0;
				}
			}
			putchar('\n');
		}
		if (fp != stdin)
			(void)fclose(fp);
		if (lbuf)
			free(lbuf);
	}
}

int
tr(char *arg)
{
	int cnt;
	char ch, *p;

	for (p = arg, cnt = 0; (ch = *p++); ++arg, ++cnt)
		if (ch == '\\')
			switch(ch = *p++) {
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
		} else
			*arg = ch;

	if (!cnt)
		errx(1, "no delimiters specified");
	return(cnt);
}

void
usage(void)
{
	extern char *__progname;
	(void)fprintf(stderr, "usage: %s [-s] [-d list] file ...\n",
	    __progname);
	exit(1);
}
