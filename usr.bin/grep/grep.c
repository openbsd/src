/*	$OpenBSD: grep.c,v 1.8 2003/06/22 23:51:22 tedu Exp $	*/

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
int	 Vflag;		/* -V: display version information */
#ifndef NOZ
int	 Zflag;		/* -Z: decompress input before processing */
#endif
int	 aflag;		/* -a: only search ascii files */
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

/* Housekeeping */
int	 first;		/* flag whether or not this is our fist match */
int	 tail;		/* lines left to print */
int	 lead;		/* number of lines in leading context queue */

extern char *__progname;

static void
usage(void)
{
	fprintf(stderr,
#ifdef NOZ
	    "usage: %s [-[AB] num] [-CEFGHLPRSVabchilnoqsvwx]"
#else
	    "usage: %s [-[AB] num] [-CEFGHLPRSVZabchilnoqsvwx]"
#endif
	    " [-e pattern] [-f file] [file ...]\n", __progname);
	exit(2);
}

#ifdef NOZ
static char *optstr = "0123456789A:B:CEFGHLPSRUVabce:f:hilnoqrsuvwxy";
#else
static char *optstr = "0123456789A:B:CEFGHLPSRUVZabce:f:hilnoqrsuvwxy";
#endif

struct option long_options[] =
{
	{"basic-regexp",        no_argument,       NULL, 'G'},
	{"extended-regexp",     no_argument,       NULL, 'E'},
	{"fixed-strings",       no_argument,       NULL, 'F'},
	{"after-context",       required_argument, NULL, 'A'},
	{"before-context",      required_argument, NULL, 'B'},
	{"context",             optional_argument, NULL, 'C'},
	{"version",             no_argument,       NULL, 'V'},
	{"byte-offset",         no_argument,       NULL, 'b'},
	{"count",               no_argument,       NULL, 'c'},
	{"regexp",              required_argument, NULL, 'e'},
	{"file",                required_argument, NULL, 'f'},
	{"no-filename",         no_argument,       NULL, 'h'},
	{"ignore-case",         no_argument,       NULL, 'i'},
	{"files-without-match", no_argument,       NULL, 'L'},
	{"files-with-matches",  no_argument,       NULL, 'l'},
	{"line-number",         no_argument,       NULL, 'n'},
	{"quiet",               no_argument,       NULL, 'q'},
	{"silent",              no_argument,       NULL, 'q'},
	{"recursive",           no_argument,       NULL, 'r'},
	{"no-messages",         no_argument,       NULL, 's'},
	{"text",                no_argument,       NULL, 'a'},
	{"revert-match",        no_argument,       NULL, 'v'},
	{"word-regexp",         no_argument,       NULL, 'w'},
	{"line-regexp",         no_argument,       NULL, 'x'},
	{"binary",              no_argument,       NULL, 'U'},
	{"unix-byte-offsets",   no_argument,       NULL, 'u'},
#ifndef NOZ
	{"decompress",          no_argument,       NULL, 'Z'},
#endif
	{NULL,                  no_argument,       NULL, 0}
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
		pattern = grep_realloc(pattern, ++pattern_sz);
	}
	if (pat[len-1] == '\n')
		--len;
	pattern[patterns] = grep_malloc(len+1);
	strncpy(pattern[patterns], pat, len);
	pattern[patterns][len] = '\0';
	++patterns;
}

static void
read_patterns(char *fn)
{
	FILE *f;
	char *line;
	size_t len;
	int nl;

	if ((f = fopen(fn, "r")) == NULL)
		err(1, "%s", fn);
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
		err(1, "%s", fn);
	fclose(f);
}

int
main(int argc, char *argv[])
{
	char *tmp;
	int c, i;

	while ((c = getopt_long(argc, argv, optstr,
				long_options, (int *)NULL)) != -1) {
		switch (c) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			tmp = argv[optind - 1];
			if (tmp[0] == '-' && tmp[1] == c && !tmp[2])
				Aflag = Bflag = strtol(++tmp, (char **)NULL, 10);
			else
				Aflag = Bflag = strtol(argv[optind] + 1, (char **)NULL, 10);
			break;
		case 'A':
			Aflag = strtol(optarg, (char **)NULL, 10);
			break;
		case 'B':
			Bflag = strtol(optarg, (char **)NULL, 10);
			break;
		case 'C':
			if (optarg == NULL)
				Aflag = Bflag = 2;
			else
				Aflag = Bflag = strtol(optarg, (char **)NULL, 10);
			break;
		case 'E':
			Eflag++;
			break;
		case 'F':
			Fflag++;
			break;
		case 'G':
			Gflag++;
			break;
		case 'H':
			Hflag++;
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
		case 'u':
			/* these are here for compatability */
			break;
		case 'V':
			fprintf(stderr, "grep version %u.%u\n", VER_MAJ, VER_MIN);
			fprintf(stderr, argv[0]);
			usage();
			break;
#ifndef NOZ
		case 'Z':
			Zflag++;
			break;
#endif
		case 'a':
			aflag = 1;
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
			read_patterns(optarg);
			break;
		case 'h':
			oflag = 0;
			hflag = 1;
			break;
		case 'i':
		case 'y':
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
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0 && patterns == 0)
		usage();

	if (patterns == 0) {
		add_pattern(*argv, strlen(*argv));
		--argc;
		++argv;
	}

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

	cflags |= Eflag ? REG_EXTENDED : REG_BASIC;
	r_pattern = grep_malloc(patterns * sizeof(regex_t));
	for (i = 0; i < patterns; ++i) {
		if ((c = regcomp(&r_pattern[i], pattern[i], cflags))) {
			regerror(c, &r_pattern[i], re_error, RE_ERROR_BUF);
			errx(1, "%s", re_error);
		}
	}

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
