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
 *
 *	$Id: util.c,v 1.1 2003/06/22 22:20:07 deraadt Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "grep.h"

/*
 * Process a file line by line...
 */

static int	linesqueued;
static int	procline(str_t *l);

int 
grep_tree(char **argv)
{
	FTS	       *fts;
	FTSENT	       *p;
	int		c, fts_flags;

	c = fts_flags = 0;

	if (Hflag)
		fts_flags = FTS_COMFOLLOW;
	if (Pflag)
		fts_flags = FTS_PHYSICAL;
	if (Sflag)
		fts_flags = FTS_LOGICAL;

	fts_flags |= FTS_NOSTAT | FTS_NOCHDIR;

	if (!(fts = fts_open(argv, fts_flags, (int (*) ()) NULL)))
		err(1, NULL);
	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DNR:
			break;
		case FTS_ERR:
			errx(1, "%s: %s", p->fts_path, strerror(p->fts_errno));
			break;
		case FTS_DP:
			break;
		default:
			c += procfile(p->fts_path);
			break;
		}
	}

	return c;
}

int
procfile(char *fn)
{
	str_t ln;
	file_t *f;
	int c, t, z;

	if (fn == NULL) {
		fn = "(standard input)";
		f = grep_fdopen(STDIN_FILENO, "r");
	} else {
		f = grep_open(fn, "r");
	}
	if (f == NULL) {
		if (!sflag)
			warn("%s", fn);
		return 0;
	}
	if (aflag && grep_bin_file(f)) {
		grep_close(f);
		return 0;
	}

	ln.file = fn;
	ln.line_no = 0;
	linesqueued = 0;
	ln.off = -1;

	if (Bflag > 0)
		initqueue();
	for (c = 0; !(lflag && c);) {
		ln.off += ln.len + 1;
		if ((ln.dat = grep_fgetln(f, &ln.len)) == NULL)
			break;
		if (ln.len > 0 && ln.dat[ln.len - 1] == '\n')
			--ln.len;
		ln.line_no++;

		z = tail;
		
		if ((t = procline(&ln)) == 0 && Bflag > 0 && z == 0) {
			enqueue(&ln);
			linesqueued++;
		}
		c += t;
	}
	if (Bflag > 0)
		clearqueue();
	grep_close(f);

	if (cflag) {
		if (!hflag)
			printf("%s:", ln.file);
		printf("%u\n", c);
	}
	if (lflag && c != 0)
		printf("%s\n", fn);
	if (Lflag && c == 0)
		printf("%s\n", fn);
	return c;
}


/*
 * Process an individual line in a file. Return non-zero if it matches.
 */

#define isword(x) (isalnum(x) || (x) == '_')

static int
procline(str_t *l)
{
	regmatch_t	pmatch;
	int		c, i, r, t;

	if (matchall) {
		c = !vflag;
		goto print;
	}
	
	t = vflag ? REG_NOMATCH : 0;
	pmatch.rm_so = 0;
	pmatch.rm_eo = l->len;
	for (c = i = 0; i < patterns; i++) {
		r = regexec(&r_pattern[i], l->dat, 0, &pmatch,  eflags);
		if (r == REG_NOMATCH && t == 0)
			continue;
		if (r == 0) {
			if (wflag) {
				if ((pmatch.rm_so != 0 && isword(l->dat[pmatch.rm_so - 1]))
				    || (pmatch.rm_eo != l->len && isword(l->dat[pmatch.rm_eo])))
					r = REG_NOMATCH;
			}
			if (xflag) {
				if (pmatch.rm_so != 0 || pmatch.rm_eo != l->len)
					r = REG_NOMATCH;
			}
		}
		if (r == t) {
			c++;
			break;
		}
	}
	
print:
	if ((tail > 0 || c) && !cflag && !qflag) {
		if (c) {
			if (first > 0 && tail == 0 && (Bflag < linesqueued) && (Aflag || Bflag))
				printf("--\n");
			first = 1;
			tail = Aflag;
			if (Bflag > 0)
				printqueue();
			linesqueued = 0;
			printline(l, ':');
		} else {
			printline(l, '-');
			tail--;
		}
	}
	return c;
}

void *
grep_malloc(size_t size)
{
	void	       *ptr;

	if ((ptr = malloc(size)) == NULL)
		err(1, "malloc");
	return ptr;
}

void *
grep_realloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL)
		err(1, "realloc");
	return ptr;
}

void
printline(str_t *line, int sep)
{
	int n;
	
	n = 0;
	if (!hflag) {
		fputs(line->file, stdout);
		++n;
	}
	if (nflag) {
		if (n)
			putchar(sep);
		printf("%d", line->line_no);
		++n;
	}
	if (bflag) {
		if (n)
			putchar(sep);
		printf("%lu", (unsigned long)line->off);
	}
	if (n)
		putchar(sep);
	fwrite(line->dat, line->len, 1, stdout);
	putchar('\n');
}
