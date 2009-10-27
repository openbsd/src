/*	$OpenBSD: whatis.c,v 1.13 2009/10/27 23:59:50 deraadt Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/param.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../man/config.h"
#include "../man/pathnames.h"

#define	MAXLINELEN	8192			/* max line handled */

static int *found, foundman;
extern char *__progname;

void dashtrunc(char *, char *);
int match(char *, char *);
void usage(void);
void whatis(char **, char *, int);

int
main(int argc, char *argv[])
{
	ENTRY *ep;
	TAG *tp;
	int ch, rv;
	char *beg, *conffile, **p, *p_augment, *p_path;

	conffile = NULL;
	p_augment = p_path = NULL;
	while ((ch = getopt(argc, argv, "C:M:m:P:")) != -1)
		switch (ch) {
		case 'C':
			conffile = optarg;
			break;
		case 'M':
		case 'P':		/* backward compatible */
			p_path = optarg;
			break;
		case 'm':
			p_augment = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc < 1)
		usage();

	if ((found = calloc(argc, sizeof(int))) == NULL)
		err(1, NULL);

	for (p = argv; *p; ++p)			/* trim full paths */
		if ((beg = strrchr(*p, '/')))
			*p = beg + 1;

	if (p_augment)
		whatis(argv, p_augment, 1);
	if (p_path || (p_path = getenv("MANPATH")))
		whatis(argv, p_path, 1);
	else {
		config(conffile);
		ep = (tp = getlist("_whatdb")) == NULL ?
		   NULL : TAILQ_FIRST(&tp->list);
		for (; ep != NULL; ep = TAILQ_NEXT(ep, q))
			whatis(argv, ep->s, 0);
	}

	if (!foundman) {
		fprintf(stderr, "whatis: no %s file found.\n", _PATH_WHATIS);
		exit(1);
	}
	rv = 1;
	for (p = argv; *p; ++p)
		if (found[p - argv])
			rv = 0;
		else
			printf("%s: not found\n", *p);
	exit(rv);
}

void
whatis(char **argv, char *path, int buildpath)
{
	char *end, *name, **p;
	char buf[MAXLINELEN + 1], wbuf[MAXLINELEN + 1];
	char hold[MAXPATHLEN];

	for (name = path; name; name = end) {	/* through name list */
		if ((end = strchr(name, ':')))
			*end++ = '\0';

		if (buildpath) {
			(void)snprintf(hold, sizeof hold, "%s/%s",
			    name, _PATH_WHATIS);
			name = hold;
		}

		if (!freopen(name, "r", stdin))
			continue;

		foundman = 1;

		/* for each file found */
		while (fgets(buf, sizeof(buf), stdin)) {
			dashtrunc(buf, wbuf);
			for (p = argv; *p; ++p)
				if (match(wbuf, *p)) {
					printf("%s", buf);
					found[p - argv] = 1;

					/* only print line once */
					while (*++p)
						if (match(wbuf, *p))
							found[p - argv] = 1;
					break;
				}
		}
	}
}

/*
 * match --
 *	match a full word or a full string
 */
int
match(char *bp, char *str)
{
	int len;
	char *start;

	if (!*str || !*bp)
		return(0);
	for (len = strlen(str);;) {
		/* skip leading crud */
		for (; *bp && !isalnum(*bp); ++bp)
			;
		if (!*bp)
			break;

		/* check for word match first */
		for (start = bp++; *bp == '_' || isalnum(*bp); ++bp)
			;
		if (bp - start == len) {
		    if (strncasecmp(start, str, len) == 0)
			    return(1);
		} else if (*bp && *bp != ',') {
		    /* check for full string match */
		    for (bp = start;
			*bp && *bp != ',' && *bp != '(' && !isspace(*bp); ++bp)
			    ;
		    if (bp - start == len && strncasecmp(start, str, len) == 0)
			    return(1);
		}
	}
	return(0);
}

/*
 * dashtrunc --
 *	truncate a string at " - "
 */
void
dashtrunc(char *from, char *to)
{
	int ch;

	for (; (ch = *from) && ch != '\n' &&
	    (ch != ' ' || from[1] != '-' || from[2] != ' '); ++from)
		*to++ = ch;
	*to = '\0';
}

/*
 * usage --
 *	print usage message and die
 */
void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: %s [-C file] [-M path] [-m path] command ...\n",
	    __progname);
	exit(1);
}
