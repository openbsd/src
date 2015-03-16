/*	$OpenBSD: grep.c,v 1.50 2015/03/16 13:27:59 millert Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "grep.h"

/* Flags passed to regcomp() and regexec() */
int	 cflags;
int	 eflags = REG_STARTEND;

int	 matchall;	/* shortcut */
int	 patterns, pattern_sz;
char   **pattern;
regex_t	*r_pattern;
fastgrep_t *fg_pattern;

/* For regex errors  */
char	 re_error[RE_ERROR_BUF + 1];

/* Command-line flags */
int	 Aflag;		/* -A x: print x lines trailing each match */
int	 Bflag;		/* -B x: print x lines leading each match */
int	 Eflag;		/* -E: interpret pattern as extended regexp */
int	 Fflag;		/* -F: interpret pattern as list of fixed strings */
int	 Hflag;		/* -H: always print filename header */
int	 Lflag;		/* -L: only show names of files with no matches */
int	 Rflag;		/* -R: recursively search directory trees */
#ifndef NOZ
int	 Zflag;		/* -Z: decompress input before processing */
#endif
int	 bflag;		/* -b: show block numbers for each match */
int	 cflag;		/* -c: only show a count of matching lines */
int	 hflag;		/* -h: don't print filename headers */
int	 iflag;		/* -i: ignore case */
int	 lflag;		/* -l: only show names of files with matches */
int	 nflag;		/* -n: show line numbers in front of matching lines */
int	 oflag;		/* -o: print each match */
int	 qflag;		/* -q: quiet mode (don't output anything) */
int	 sflag;		/* -s: silent mode (ignore errors) */
int	 vflag;		/* -v: only show non-matching lines */
int	 wflag;		/* -w: pattern must start and end on word boundaries */
int	 xflag;		/* -x: pattern must match entire line */
int	 lbflag;	/* --line-buffered */

int binbehave = BIN_FILE_BIN;

enum {
	BIN_OPT = CHAR_MAX + 1,
	HELP_OPT,
	MMAP_OPT,
	LINEBUF_OPT
};

/* Housekeeping */
int	 first;		/* flag whether or not this is our first match */
int	 tail;		/* lines left to print */
int	 file_err;	/* file reading error */

struct patfile {
	const char		*pf_file;
	SLIST_ENTRY(patfile)	 pf_next;
};
SLIST_HEAD(, patfile)		 patfilelh;

extern char *__progname;

static void
usage(void)
{
	fprintf(stderr,
#ifdef NOZ
	    "usage: %s [-abcEFGHhIiLlnoqRsUVvwx] [-A num] [-B num] [-C[num]]\n"
#else
	    "usage: %s [-abcEFGHhIiLlnoqRsUVvwxZ] [-A num] [-B num] [-C[num]]\n"
#endif
	    "\t[-e pattern] [-f file] [--binary-files=value] [--context[=num]]\n"
	    "\t[--line-buffered] [pattern] [file ...]\n", __progname);
	exit(2);
}

#ifdef NOZ
static const char optstr[] = "0123456789A:B:CEFGHILRUVabce:f:hilnoqrsuvwxy";
#else
static const char optstr[] = "0123456789A:B:CEFGHILRUVZabce:f:hilnoqrsuvwxy";
#endif

static const struct option long_options[] =
{
	{"binary-files",	required_argument,	NULL, BIN_OPT},
	{"help",		no_argument,		NULL, HELP_OPT},
	{"mmap",		no_argument,		NULL, MMAP_OPT},
	{"line-buffered",	no_argument,		NULL, LINEBUF_OPT},
	{"after-context",	required_argument,	NULL, 'A'},
	{"before-context",	required_argument,	NULL, 'B'},
	{"context",		optional_argument,	NULL, 'C'},
	{"devices",		required_argument,	NULL, 'D'},
	{"extended-regexp",	no_argument,		NULL, 'E'},
	{"fixed-strings",	no_argument,		NULL, 'F'},
	{"basic-regexp",	no_argument,		NULL, 'G'},
	{"with-filename",	no_argument,		NULL, 'H'},
	{"binary",		no_argument,		NULL, 'U'},
	{"version",		no_argument,		NULL, 'V'},
	{"text",		no_argument,		NULL, 'a'},
	{"byte-offset",		no_argument,		NULL, 'b'},
	{"count",		no_argument,		NULL, 'c'},
	{"regexp",		required_argument,	NULL, 'e'},
	{"file",		required_argument,	NULL, 'f'},
	{"no-filename",		no_argument,		NULL, 'h'},
	{"ignore-case",		no_argument,		NULL, 'i'},
	{"files-without-match",	no_argument,		NULL, 'L'},
	{"files-with-matches",	no_argument,		NULL, 'l'},
	{"line-number",		no_argument,		NULL, 'n'},
	{"quiet",		no_argument,		NULL, 'q'},
	{"silent",		no_argument,		NULL, 'q'},
	{"recursive",		no_argument,		NULL, 'r'},
	{"no-messages",		no_argument,		NULL, 's'},
	{"revert-match",	no_argument,		NULL, 'v'},
	{"word-regexp",		no_argument,		NULL, 'w'},
	{"line-regexp",		no_argument,		NULL, 'x'},
	{"unix-byte-offsets",	no_argument,		NULL, 'u'},
#ifndef NOZ
	{"decompress",		no_argument,		NULL, 'Z'},
#endif
	{NULL,			no_argument,		NULL, 0}
};


static void
add_pattern(char *pat, size_t len)
{
	if (!xflag && (len == 0 || matchall)) {
		matchall = 1;
		return;
	}
	if (patterns == pattern_sz) {
		pattern_sz *= 2;
		pattern = grep_reallocarray(pattern, ++pattern_sz, sizeof(*pattern));
	}
	if (len > 0 && pat[len - 1] == '\n')
		--len;
	/* pat may not be NUL-terminated */
	if (wflag && !Fflag) {
		int bol = 0, eol = 0, extra;
		if (pat[0] == '^')
			bol = 1;
		if (len > 0 && pat[len - 1] == '$')
			eol = 1;
		extra = Eflag ? 2 : 4;
		pattern[patterns] = grep_malloc(len + 15 + extra);
		snprintf(pattern[patterns], len + 15 + extra,
		   "%s[[:<:]]%s%.*s%s[[:>:]]%s",
		    bol ? "^" : "",
		    Eflag ? "(" : "\\(",
		    (int)len - bol - eol, pat + bol,
		    Eflag ? ")" : "\\)",
		    eol ? "$" : "");
		len += 14 + extra;
	} else {
		pattern[patterns] = grep_malloc(len + 1);
		memcpy(pattern[patterns], pat, len);
		pattern[patterns][len] = '\0';
	}
	++patterns;
}

static void
add_patterns(char *pats)
{
	char *nl;

	while ((nl = strchr(pats, '\n')) != NULL) {
		add_pattern(pats, nl - pats);
		pats = nl + 1;
	}
	add_pattern(pats, strlen(pats));
}

static void
read_patterns(const char *fn)
{
	FILE *f;
	char *line;
	size_t len;

	if ((f = fopen(fn, "r")) == NULL)
		err(2, "%s", fn);
	while ((line = fgetln(f, &len)) != NULL)
		add_pattern(line, *line == '\n' ? 0 : len);
	if (ferror(f))
		err(2, "%s", fn);
	fclose(f);
}

int
main(int argc, char *argv[])
{
	int c, lastc, prevoptind, newarg, i, needpattern, exprs, expr_sz;
	struct patfile *patfile, *pf_next;
	long l;
	char *ep, **expr;

	SLIST_INIT(&patfilelh);
	switch (__progname[0]) {
	case 'e':
		Eflag = 1;
		break;
	case 'f':
		Fflag = 1;
		break;
#ifndef NOZ
	case 'z':
		Zflag = 1;
		switch(__progname[1]) {
		case 'e':
			Eflag = 1;
			break;
		case 'f':
			Fflag = 1;
			break;
		}
		break;
#endif
	}

	lastc = '\0';
	newarg = 1;
	prevoptind = 1;
	needpattern = 1;
	expr_sz = exprs = 0;
	expr = NULL;
	while ((c = getopt_long(argc, argv, optstr,
				long_options, NULL)) != -1) {
		switch (c) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (newarg || !isdigit(lastc))
				Aflag = 0;
			else if (Aflag > INT_MAX / 10)
				errx(2, "context out of range");
			Aflag = Bflag = (Aflag * 10) + (c - '0');
			break;
		case 'A':
		case 'B':
			l = strtol(optarg, &ep, 10);
			if (ep == optarg || *ep != '\0' ||
			    l <= 0 || l >= INT_MAX)
				errx(2, "context out of range");
			if (c == 'A')
				Aflag = (int)l;
			else
				Bflag = (int)l;
			break;
		case 'C':
			if (optarg == NULL)
				Aflag = Bflag = 2;
			else {
				l = strtol(optarg, &ep, 10);
				if (ep == optarg || *ep != '\0' ||
				    l <= 0 || l >= INT_MAX)
					errx(2, "context out of range");
				Aflag = Bflag = (int)l;
			}
			break;
		case 'E':
			Fflag = 0;
			Eflag = 1;
			break;
		case 'F':
			Eflag = 0;
			Fflag = 1;
			break;
		case 'G':
			Eflag = Fflag = 0;
			break;
		case 'H':
			Hflag = 1;
			break;
		case 'I':
			binbehave = BIN_FILE_SKIP;
			break;
		case 'L':
			lflag = 0;
			Lflag = qflag = 1;
			break;
		case 'R':
		case 'r':
			Rflag = 1;
			break;
		case 'U':
			binbehave = BIN_FILE_BIN;
			break;
		case 'V':
			fprintf(stderr, "grep version %u.%u\n", VER_MAJ, VER_MIN);
			exit(0);
			break;
#ifndef NOZ
		case 'Z':
			Zflag = 1;
			break;
#endif
		case 'a':
			binbehave = BIN_FILE_TEXT;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'e':
			/* defer adding of expressions until all arguments are parsed */
			if (exprs == expr_sz) {
				expr_sz *= 2;
				expr = grep_reallocarray(expr, ++expr_sz,
				    sizeof(*expr));
			}
			needpattern = 0;
			expr[exprs] = optarg;
			++exprs;
			break;
		case 'f':
			patfile = grep_malloc(sizeof(*patfile));
			patfile->pf_file = optarg;
			SLIST_INSERT_HEAD(&patfilelh, patfile, pf_next);
			needpattern = 0;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'i':
		case 'y':
			iflag = 1;
			cflags |= REG_ICASE;
			break;
		case 'l':
			Lflag = 0;
			lflag = qflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case BIN_OPT:
			if (strcmp("binary", optarg) == 0)
				binbehave = BIN_FILE_BIN;
			else if (strcmp("without-match", optarg) == 0)
				binbehave = BIN_FILE_SKIP;
			else if (strcmp("text", optarg) == 0)
				binbehave = BIN_FILE_TEXT;
			else
				errx(2, "Unknown binary-files option");
			break;
		case 'u':
		case MMAP_OPT:
			/* default, compatibility */
			break;
		case LINEBUF_OPT:
			lbflag = 1;
			break;
		case HELP_OPT:
		default:
			usage();
		}
		lastc = c;
		newarg = optind != prevoptind;
		prevoptind = optind;
	}
	argc -= optind;
	argv += optind;

	for (i = 0; i < exprs; i++)
		add_patterns(expr[i]);
	free(expr);
	expr = NULL;

	for (patfile = SLIST_FIRST(&patfilelh); patfile != NULL;
	    patfile = pf_next) {
		pf_next = SLIST_NEXT(patfile, pf_next);
		read_patterns(patfile->pf_file);
		free(patfile);
	}

	if (argc == 0 && needpattern)
		usage();

	if (argc != 0 && needpattern) {
		add_patterns(*argv);
		--argc;
		++argv;
	}

	if (Eflag)
		cflags |= REG_EXTENDED;
	if (Fflag)
		cflags |= REG_NOSPEC;
#ifdef SMALL
	/* Sorry, this won't work */
	if (Fflag && wflag)
		errx(1, "Can't use small fgrep with -w");
#endif
	fg_pattern = grep_calloc(patterns, sizeof(*fg_pattern));
	r_pattern = grep_calloc(patterns, sizeof(*r_pattern));
	for (i = 0; i < patterns; ++i) {
		/* Check if cheating is allowed (always is for fgrep). */
#ifndef SMALL
		if (Fflag) {
			fgrepcomp(&fg_pattern[i], pattern[i]);
		} else
#endif
		{
			if (fastcomp(&fg_pattern[i], pattern[i])) {
				/* Fall back to full regex library */
				c = regcomp(&r_pattern[i], pattern[i], cflags);
				if (c != 0) {
					regerror(c, &r_pattern[i], re_error,
					    RE_ERROR_BUF);
					errx(2, "%s", re_error);
				}
			}
		}
	}

	if (lbflag)
		setvbuf(stdout, NULL, _IOLBF, 0);

	if ((argc == 0 || argc == 1) && !Rflag && !Hflag)
		hflag = 1;

	if (argc == 0)
		exit(!procfile(NULL));

	if (Rflag)
		c = grep_tree(argv);
	else
		for (c = 0; argc--; ++argv)
			c += procfile(*argv);

	exit(c ? (file_err ? (qflag ? 0 : 2) : 0) : (file_err ? 2 : 1));
}
