/*	$OpenBSD: grep.c,v 1.2 2003/02/16 03:46:04 cloder Exp $	*/

/*-
 * Copyright (c) 2000 Carson Harding. All rights reserved.
 * This code was written and contributed to OpenBSD by Carson Harding.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author, or the names of contributors may be 
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

#ifndef lint
static char rcsid[] = "$OpenBSD: grep.c,v 1.2 2003/02/16 03:46:04 cloder Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <fts.h>
#include <err.h>

extern	char *__progname;


void	usage(void);
void	err_regerror(int r, regex_t *rexp);
int	grep_files(int regexc, regex_t *regexv, char **files);
int	grep_tree(int regexc, regex_t *regexv, char **paths);
int	grep_file(int regexc, regex_t *rexp, char *fname);
void	arg_patt(char *s);
char	*chop_patt(char *s, size_t *len);
void	add_patt(char *s, size_t len);
void	load_patt(char *fname);
regex_t *regcomp_patt(int pattc, char *pattvp[], int cflags);


int	f_bytecount;		/* -b prepend byte count */
int	f_countonly;		/* -c return only count */
int	f_nofname;		/* -h do not prepend filenames on multiple */
int	f_fnameonly;		/* -l only print file name with match */
int	f_suppress;		/* -s suppress error messages; 1/2 -q */
int	f_lineno;		/* -n prepend with line numbers */
int	f_quiet;		/* -q no output, only status */
int	f_wmatch;		/* -w match words */
int	f_xmatch;		/* -x match line */
int	f_zerobyte;		/* -z NUL character after filename with -l */
int	f_match;		/* = REG_MATCH; else = REG_NOMATCH for -v */
int	f_multifile;		/* multiple files: prepend file names */
int	f_matchall;		/* empty pattern, matches all input */
int	f_error;		/* saw error; set exit status */

				/* default traversal flags */
int	f_ftsflags = FTS_LOGICAL|FTS_NOCHDIR|FTS_NOSTAT;

int	f_debug;		/* temporary debugging flag */

#define START_PATT_SZ	 8	/* start with room for 8 patterns */
char	**pattv;		/* array of patterns from -e and -f */
int	pattc;			/* patterns in pattern array */
int	pattn;			/* patterns we have seen, including nulls */

int
main(int argc, char **argv)
{
	int	c;
	int	ch;
	int	cflags;		/* flags to regcomp() */
	int	sawfile;	/* did we see a pattern file? */
	regex_t *regexv;	/* start of array of compiled patterns */

	int (*grepf)(int regexc, regex_t *regexv, char **argv);

	sawfile = 0;
	cflags	= REG_BASIC|REG_NEWLINE;
	grepf	= grep_files;

	if (*__progname == 'e')
		cflags |= REG_EXTENDED;
	else if (*__progname == 'f')
		cflags |= REG_NOSPEC;

	while ((ch = getopt(argc, argv, "DEFRHLPXabce:f:hilnqsvwxz")) != -1) {
		switch(ch) {
		case 'D':
			f_debug = 1;
			break;
		case 'E':
			cflags |= REG_EXTENDED;
			break;
		case 'F':
			cflags |= REG_NOSPEC;
			break;
		case 'H':
			f_ftsflags |= FTS_COMFOLLOW;
			break;
		case 'L':
			f_ftsflags |= FTS_LOGICAL;
			break;
		case 'P':
			f_ftsflags |= FTS_PHYSICAL;
			break;
		case 'R':
			grepf = grep_tree;
			/* 
			 * If walking the tree we don't know how many files
			 * we'll actually find. So assume multiple, if
			 * you don't want names, there's always -h ....
			 */
			f_multifile = 1;
			break;
		case 'X':
			f_ftsflags |= FTS_XDEV;
			break;
		case 'a':
			/* 
			 * Silently eat -a; we don't use the default
			 * behaviour it toggles off in gnugrep.
			 */
			break;
		case 'b':
			f_bytecount = 1;
			break;
		case 'c':
			f_countonly = 1;
			break;
		case 'e':
			arg_patt(optarg);
			break;
		case 'f':
			load_patt(optarg);
			sawfile = 1;
			break;
		case 'h':
			f_nofname = 1;
			break;
		case 'i':
			cflags |= REG_ICASE;
			break;
		case 'l':
			f_fnameonly = 1;
			break;
		case 'n':
			f_lineno = 1;
			break;
		case 'q':
			f_quiet = 1;
			break;
		case 's':
			f_suppress = 1;
			break;
		case 'v':
			f_match = REG_NOMATCH;
			break;
		case 'w':
			f_wmatch = 1;
			break;
		case 'x':
			f_xmatch = 1;
			break;
		case 'z':
			f_zerobyte = 1;
			break;
		default:
			usage();
			break;
		}
	}

	if ((cflags & REG_EXTENDED) && (cflags & REG_NOSPEC))
		usage();

	/*
	 * If we read one or more pattern files, and still 
	 * didn't end up with any pattern, any pattern file 
	 * we read was empty. This is different than failing
	 * to provide a pattern as an argument, and we fail
	 * on this case as if we had searched and found
	 * no matches. (At least this is what GNU grep and
	 * Solaris's grep do.)
	 */
	if (!pattn && !argv[optind]) {
		if (sawfile)
			exit(1);
		else usage();
	}

	if (!pattn) {
		arg_patt(argv[optind]);
		optind++;
	}

	/* why bother ... just do nothing sooner */
	if (f_matchall && f_match == REG_NOMATCH)
		exit(1);

	regexv = regcomp_patt(pattc, pattv, cflags);

	if (optind == argc) {
		c = grep_file(pattc, regexv, NULL);
	} else {
		if (argc - optind > 1 && !f_nofname)
			f_multifile = 1;
		c = (*grepf)(pattc, regexv, &argv[optind]);
	}

	/* XX ugh */
	if (f_error) {
		if (c && f_quiet) 
			exit(0);
		else
			exit(2);
	} else if (c) 
		exit(0);
	else
		exit(1);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-E|-F] [-abchilnqsvwx] [-RXH[-L|-P]]"
	    " {patt | -e patt | -f patt_file} [files]\n", 
	    __progname);
	exit(2);
}

/*
 * Patterns as arguments may have embedded newlines.
 * When read from file, these are detected by fgetln();
 * in arguments we have to find and cut out the segments.
 */
void
arg_patt(char *s)
{
	size_t len;
	char *sp;

	if (f_debug)
		fprintf(stderr, "arg_patt(\"%s\")\n", s);

	len = strlen(s);
	if (!len) {		     /* got "" on the command-line */
		add_patt(s, len);
		return;
	}
	for (sp = chop_patt(s, &len); sp; sp = chop_patt(NULL, &len)) {
		if (f_debug) {
			fprintf(stderr, "adding pattern \"");
			fwrite(sp, len, 1, stderr);
			fprintf(stderr, "\", length %lu\n",(unsigned long)len);
			if (pattc > 20) {
				fprintf(stderr, "too many, exiting ...\n");
				exit(2);
			}
		}
		add_patt(sp, len);
	}
}

/* 
 * Kind of like strtok; pass char *, then NULL for rest.
 * Call it memtok()... New size gets written into len.
 */
char *
chop_patt(char *s, size_t *len)
{
	char	*cp;
	static	char *save_s;
	static	int   save_n;

	if (s)
		save_n = *len;
	else
		s = save_s;

	if (save_n <= 0) {
		s = save_s = NULL;
	} else if (s) {
		if ((cp = memchr(s, '\n', save_n)) != NULL) {
			*len = cp - s;	/* returned segment */
			save_n -= *len;
			save_s = ++cp;	/* adjust past newline */
			save_n--;
		} else {
			*len = save_n;	/* else return the whole string */
			save_n = 0;
		}
	}

	return s;
}

/*
 * Start with an array for 8 patterns, and double it 
 * each time we outgrow it. If pattern is empty (0 length),
 * or if f_matchall is already set, set f_matchall and return.
 * No use adding a pattern if all input is going to match
 * anyhow.
 */
void
add_patt(char *s, size_t len)
{
	char	*p;
	static	size_t	pattmax = START_PATT_SZ;
	static size_t sumlen;

	pattn++;
	sumlen += len;

	if (!len || f_matchall) {
		f_matchall = 1;
		return;
	}

	if (!pattv) { 
		pattv = malloc(START_PATT_SZ * sizeof(char *));
		if (!pattv)
			err(2, "malloc");
		pattc = 0;
	} else if (pattc >= pattmax) {
		pattmax *= 2;
		pattv = realloc(pattv, pattmax * sizeof(char *));
		if (!pattv)
			err(2, "realloc");
	}
	p = malloc(len+1);
	if (!p) err(2, "malloc");
	memmove(p, s, len);
	p[len] = '\0';
	pattv[pattc++] = p;
}

/*
 * Load patterns from file.
 */
void
load_patt(char *fname)
{
	char	*buf;
	size_t	len;
	FILE	*fr;

	fr = fopen(fname, "r");
	if (!fr)
		err(2, "%s", fname);
	while ((buf = fgetln(fr, &len)) != NULL) {
		if (buf[len-1] == '\n')
			buf[--len] = '\0';
		add_patt(buf, len);
	}
	fclose(fr);
}

/*
 * Compile the collected pattern strings into an array
 * of regex_t.
 */
regex_t *
regcomp_patt(int lpattc, char *lpattv[], int cflags)
{
	int	i;
	int	r;
	regex_t *rxv;

	if (f_matchall)
		return NULL;

	rxv = malloc(sizeof(regex_t) * lpattc);
	if (!rxv)
		err(2, "malloc");
	for (i = 0; i < lpattc; i++) {
		if ((r = regcomp(&rxv[i], lpattv[i], cflags)) != 0)
			err_regerror(r, &rxv[i]);
	}
	return rxv;
}

/*
 * Print out regcomp error, and exit.
 */
void
err_regerror(int r, regex_t *rexp)
{
	size_t	n;
	char	*buf;

	n = regerror(r, rexp, NULL, 0);
	buf = malloc(n);
	if (!buf)
		err(2, "malloc");
	(void)regerror(r, rexp, buf, n);
	errx(2, "%s", buf);
}

/* 
 * Little wrapper so we can use function pointer above.
 */
int
grep_files(int regexc, regex_t *regexv, char **files)
{
	int	c;
	char	**fname;

	c = 0;
	for (fname = files; *fname; fname++)
		c += grep_file(regexc, regexv, *fname);

	return c;
}

/* 
 * Modified from James Howard and Dag-Erling Co?dan Sm?rgrav's grep:
 * add FTS_D to FTS_DP (especially since D was the one being used)
 * pass in regex_t array, and set fts flags above in main().
 */
int 
grep_tree(int regexc, regex_t *regexv, char **paths)
{
	int	c;
	FTS	*fts;
	FTSENT	*p;

	c = 0;

	if (!(fts = fts_open(paths, f_ftsflags, (int (*) ()) NULL)))
		err(2, "fts_open");
	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_D:
		case FTS_DP:
		case FTS_DNR:
			break;
		case FTS_ERR:
			errx(2, "%s: %s", p->fts_path, strerror(p->fts_errno));
			break;
		default:
			if (f_debug) 
				printf("%s\n", p->fts_path);
			c += grep_file(regexc, regexv, p->fts_path);
			break;
		}
	}

	return c;
}

/*
 * Open and grep the named file. If fname is NULL, read
 * from stdin. 
 */

#define isword(x) (isalnum(x) || (x) == '_')

int
grep_file(int regexc, regex_t *regexv, char *fname)
{
	int	i;
	int	c;
	int	n;
	int	r;
	int	match;
	char	*buf;
	size_t	b;
	size_t	len;
	FILE	*fr;
	regmatch_t pmatch[1];
	regoff_t   so, eo;

	b = 0;		/* byte count */
	c = 0;		/* match count */
	n = 0;		/* line count */

	if (!fname) {
		fr = stdin;
		fname = "(standard input)";
	} else {
		fr = fopen(fname, "r");
		if (!fr) {
			if (!f_suppress)
				warn("%s", fname);
			f_error = 1;
			return 0;
		}
	}

	while ((buf = fgetln(fr, &len)) != NULL) {
		n++;
		if (f_matchall)
			goto printmatch;
		match = 0;
		for (i = 0; i < regexc; i++) {
			pmatch[0].rm_so = 0;
			pmatch[0].rm_eo = len-1;
			r = regexec(&regexv[i], buf, 1, pmatch, REG_STARTEND);
			if (r == f_match) {
				/*
				 * XX gnu grep allows both -w and -x;
				 * XX but seems bizarre. sometimes -w seems
				 * XX to override, at other times, not.
				 * XX Need to figure that out.
				 * XX It seems logical to go with the most
				 * XX restrictive argument: -x, as -x is
				 * XX a boundary case of -w anyhow. 
				 */
				if (f_xmatch) {
					if (pmatch[0].rm_so != 0 ||
					    pmatch[0].rm_eo != len-1)
						continue;
				} else if (f_wmatch) {
					so = pmatch[0].rm_so;
					eo = pmatch[0].rm_eo;
					if (!((so == 0 || !isword(buf[so-1])) &&
					    (eo == len || !isword(buf[eo]))))
						continue;
				} 
				match = 1;
				break;
			}
			/* XX test for regexec() errors ?? */
		}
		if (match) {
printmatch:
			c++;
			if (f_fnameonly || f_quiet)
				break;
			if (f_countonly)
				continue;
			if (f_multifile && !f_nofname)
				printf("%s:", fname);
			if (f_lineno)
				printf("%d:", n);
			if (f_bytecount)
				printf("%lu:", (unsigned long)b);
			fwrite(buf, len, 1, stdout);
		}
		/* save position in stream before next line */
		b += len;
	}

	if (!buf && ferror(fr)) {
		warn("%s", fname);
		f_error = 1;
		/* 
		 * XX or do we spit out what result we did have?
		 */
	} else if (!f_quiet) {
		/*
		 * XX test -c and -l together: gnu grep
		 * XX allows (although ugly), do others?
		 */
		if (f_countonly) {
			if (f_multifile)
				printf("%s:", fname);
			printf("%d\n", c);
		}
		if (c && f_fnameonly) {
			fputs(fname, stdout);
			if (f_zerobyte)
				fputc('\0', stdout);
			else 
				fputc('\n', stdout);
		}
	}

	if (fr != stdin)
		fclose(fr);

	return c;
}

