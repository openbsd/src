/*	$OpenBSD: grep.c,v 1.34 2006/02/09 09:54:46 otto Exp $	*/

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
#include <sys/limits.h>
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
int	 Gflag;		/* -G: interpret pattern as basic regexp */
int	 Hflag;		/* -H: if -R, follow explicitly listed symlinks */
int	 Lflag;		/* -L: only show names of files with no matches */
int	 Pflag;		/* -P: if -R, no symlinks are followed */
int	 Rflag;		/* -R: recursively search directory trees */
int	 Sflag;		/* -S: if -R, follow all symlinks */
#ifndef NOZ
int	 Zflag;		/* -Z: decompress input before processing */
#endif
int	 bflag;		/* -b: show block numbers for each match */
int	 cflag;		/* -c: only show a count of matching lines */
int	 hflag;		/* -h: don't print filename headers */
int	 iflag;		/* -i: ignore case */
int	 lflag;		/* -l: only show names of files with matches */
int	 nflag;		/* -n: show line numbers in front of matching lines */
int	 oflag;		/* -o: always print file name */
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
	    "usage: %s [-abcEFGHhIiLlnoPqRSsUVvwx] [-A num] [-B num] [-C[num]]\n"
#else
	    "usage: %s [-abcEFGHhIiLlnoPqRSsUVvwxZ] [-A num] [-B num] [-C[num]]\n"
#endif
	    "\t[-e pattern] [-f file] [--binary-files=value] [--context[=num]]\n"
	    "\t[--line-buffered] [pattern] [file ...]\n", __progname);
	exit(2);
}

#ifdef NOZ
static char *optstr = "0123456789A:B:CEFGHILPSRUVabce:f:hilnoqrsuvwxy";
#else
static char *optstr = "0123456789A:B:CEFGHILPSRUVZabce:f:hilnoqrsuvwxy";
#endif

struct option long_options[] =
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
	if (len == 0 || matchall) {
		matchall = 1;
		return;
	}
	if (patterns == pattern_sz) {
		pattern_sz *= 2;
		pattern = grep_realloc(pattern, ++pattern_sz * sizeof(*pattern));
	}
	if (pat[len - 1] == '\n')
		--len;
	/* pat may not be NUL-terminated */
	if (wflag && !Fflag) {
		int bol = 0, eol = 0, extra;
		if (pat[0] == '^')
			bol = 1;
		if (pat[len - 1] == '$')
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
read_patterns(const char *fn)
{
	FILE *f;
	char *line;
	size_t len;
	int nl;

	if ((f = fopen(fn, "r")) == NULL)
		err(2, "%s", fn);
	nl = 0;
	while ((line = fgetln(f, &len)) != NULL) {
		if (*line == '\n') {
			++nl;
			continue;
		}
		if (nl) {
			matchall = 1;
			break;
		}
		nl = 0;
		add_pattern(line, len);
	}
	if (ferror(f))
		err(2, "%s", fn);
	fclose(f);
}

int
main(int argc, char *argv[])
{
	int c, lastc, prevoptind, newarg, i;
	struct patfile *patfile, *pf_next;
	long l;
	char *ep;

	SLIST_INIT(&patfilelh);
	switch (__progname[0]) {
	case 'e':
		Eflag++;
		break;
	case 'f':
		Fflag++;
		break;
	case 'g':
		Gflag++;
		break;
#ifndef NOZ
	case 'z':
		Zflag++;
		switch(__progname[1]) {
		case 'e':
			Eflag++;
			break;
		case 'f':
			Fflag++;
			break;
		case 'g':
			Gflag++;
			break;
		}
		break;
#endif
	}

	lastc = '\0';
	newarg = 1;
	prevoptind = 1;
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
			Fflag = Gflag = 0;
			Eflag++;
			break;
		case 'F':
			Eflag = Gflag = 0;
			Fflag++;
			break;
		case 'G':
			Eflag = Fflag = 0;
			Gflag++;
			break;
		case 'H':
			Hflag++;
			break;
		case 'I':
			binbehave = BIN_FILE_SKIP;
			break;
		case 'L':
			lflag = 0;
			Lflag = qflag = 1;
			break;
		case 'P':
			Pflag++;
			break;
		case 'S':
			Sflag++;
			break;
		case 'R':
		case 'r':
			Rflag++;
			oflag++;
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
			Zflag++;
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
			add_pattern(optarg, strlen(optarg));
			break;
		case 'f':
			patfile = grep_malloc(sizeof(*patfile));
			patfile->pf_file = optarg;
			SLIST_INSERT_HEAD(&patfilelh, patfile, pf_next);
			break;
		case 'h':
			oflag = 0;
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
			hflag = 0;
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

	for (patfile = SLIST_FIRST(&patfilelh); patfile != NULL;
	    patfile = pf_next) {
		pf_next = SLIST_NEXT(patfile, pf_next);
		read_patterns(patfile->pf_file);
		free(patfile);
	}

	if (argc == 0 && patterns == 0)
		usage();

	if (patterns == 0) {
		add_pattern(*argv, strlen(*argv));
		--argc;
		++argv;
	}

	if (Eflag)
		cflags |= REG_EXTENDED;
	fg_pattern = grep_malloc(patterns * sizeof(*fg_pattern));
	r_pattern = grep_malloc(patterns * sizeof(*r_pattern));
	for (i = 0; i < patterns; ++i) {
		/* Check if cheating is allowed (always is for fgrep). */
		if (Fflag) {
			fgrepcomp(&fg_pattern[i], pattern[i]);
		} else {
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
		setlinebuf(stdout);

	if ((argc == 0 || argc == 1) && !oflag)
		hflag = 1;

	if (argc == 0)
		exit(!procfile(NULL));

	if (Rflag)
		c = grep_tree(argv);
	else
		for (c = 0; argc--; ++argv)
			c += procfile(*argv);

	exit(!c);
}
