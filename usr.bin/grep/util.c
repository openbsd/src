/*	$OpenBSD: util.c,v 1.58 2017/12/09 18:38:37 pirofti Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Co�dan Sm�rgrav
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <regex.h>
#include <stdbool.h>
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
static int	procline(str_t *l, int);
static int	grep_search(fastgrep_t *, char *, size_t, regmatch_t *pmatch, int);
#ifndef SMALL
static bool	grep_cmp(const char *, const char *, size_t);
static void	grep_revstr(unsigned char *, int);
#endif

int
grep_tree(char **argv)
{
	FTS	*fts;
	FTSENT	*p;
	int	c, fts_flags;

	c = 0;

	fts_flags = FTS_PHYSICAL | FTS_NOSTAT | FTS_NOCHDIR;

	if (!(fts = fts_open(argv, fts_flags, NULL)))
		err(2, NULL);
	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DNR:
			break;
		case FTS_ERR:
			file_err = 1;
			if(!sflag)
				warnc(p->fts_errno, "%s", p->fts_path);
			break;
		case FTS_DP:
			break;
		default:
			c += procfile(p->fts_path);
			break;
		}
	}
	if (errno)
		err(2, "fts_read");
	fts_close(fts);
	return c;
}

int
procfile(char *fn)
{
	str_t ln;
	file_t *f;
	int c, t, z, nottext;

	mcount = mlimit;

	if (fn == NULL) {
		fn = "(standard input)";
		f = grep_fdopen(STDIN_FILENO, "r");
	} else {
		f = grep_open(fn, "r");
	}
	if (f == NULL) {
		file_err = 1;
		if (!sflag)
			warn("%s", fn);
		return 0;
	}

	nottext = grep_bin_file(f);
	if (nottext && binbehave == BIN_FILE_SKIP) {
		grep_close(f);
		return 0;
	}

	ln.file = fn;
	ln.line_no = 0;
	ln.len = 0;
	linesqueued = 0;
	tail = 0;
	ln.off = -1;

	if (Bflag > 0)
		initqueue();
	for (c = 0;  c == 0 || !(lflag || qflag); ) {
		if (mflag && mlimit == 0)
			break;
		ln.off += ln.len + 1;
		if ((ln.dat = grep_fgetln(f, &ln.len)) == NULL)
			break;
		if (ln.len > 0 && ln.dat[ln.len - 1] == '\n')
			--ln.len;
		ln.line_no++;

		z = tail;

		if ((t = procline(&ln, nottext)) == 0 && Bflag > 0 && z == 0) {
			enqueue(&ln);
			linesqueued++;
		}
		c += t;
		if (mflag && mcount <= 0)
			break;
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
	if (c && !cflag && !lflag && !Lflag &&
	    binbehave == BIN_FILE_BIN && nottext && !qflag)
		printf("Binary file %s matches\n", fn);

	return c;
}


/*
 * Process an individual line in a file. Return non-zero if it matches.
 */

#define isword(x) (isalnum((unsigned char)x) || (x) == '_')

static int
procline(str_t *l, int nottext)
{
	regmatch_t	pmatch = { 0 };
	int		c, i, r;
	regoff_t	offset;

	/* size_t will be converted to regoff_t. ssize_t is guaranteed to fit
	 * into regoff_t */
	if (l->len > SSIZE_MAX) {
		errx(2, "Line is too big to process");
	}

	c = 0;
	i = 0;
	if (matchall) {
		c = 1;
		goto print;
	}

	for (i = 0; i < patterns; i++) {
		offset = 0;
redo:
		if (fg_pattern[i].pattern) {
			int flags = 0;
			if (offset)
				flags |= REG_NOTBOL;
			r = grep_search(&fg_pattern[i], l->dat + offset,
			    l->len - offset, &pmatch, flags);
			pmatch.rm_so += offset;
			pmatch.rm_eo += offset;
		} else {
			int flags = eflags;
			if (offset)
				flags |= REG_NOTBOL;
			pmatch.rm_so = offset;
			pmatch.rm_eo = l->len;
			r = regexec(&r_pattern[i], l->dat, 1, &pmatch, flags);
		}
		if (r == 0 && xflag) {
			if (pmatch.rm_so != 0 || pmatch.rm_eo != l->len)
				r = REG_NOMATCH;
		}
		if (r == 0) {
			c = 1;
			if (oflag && pmatch.rm_so != pmatch.rm_eo)
				goto print;
			break;
		}
	}
	if (oflag)
		return c;
print:
	if (vflag)
		c = !c;

	/* Count the matches if we have a match limit */
	if (mflag)
		mcount -= c;

	if (c && binbehave == BIN_FILE_BIN && nottext)
		return c; /* Binary file */

	if ((tail > 0 || c) && !cflag && !qflag) {
		if (c) {
			if (first > 0 && tail == 0 && (Bflag < linesqueued) &&
			    (Aflag || Bflag))
				printf("--\n");
			first = 1;
			tail = Aflag;
			if (Bflag > 0)
				printqueue();
			linesqueued = 0;
			printline(l, ':', oflag ? &pmatch : NULL);
		} else {
			printline(l, '-', oflag ? &pmatch : NULL);
			tail--;
		}
	}
	if (oflag && !matchall) {
		offset = pmatch.rm_eo;
		goto redo;
	}
	return c;
}

#ifndef SMALL
void
fgrepcomp(fastgrep_t *fg, const unsigned char *pattern)
{
	int i;

	/* Initialize. */
	fg->patternLen = strlen(pattern);
	fg->bol = 0;
	fg->eol = 0;
	fg->wmatch = wflag;
	fg->reversedSearch = 0;

	/*
	 * Make a copy and upper case it for later if in -i mode,
	 * else just copy the pointer.
	 */
	if (iflag) {
		fg->pattern = grep_malloc(fg->patternLen + 1);
		for (i = 0; i < fg->patternLen; i++)
			fg->pattern[i] = toupper(pattern[i]);
		fg->pattern[fg->patternLen] = '\0';
	} else
		fg->pattern = (unsigned char *)pattern;	/* really const */

	/* Preprocess pattern. */
	for (i = 0; i <= UCHAR_MAX; i++)
		fg->qsBc[i] = fg->patternLen;
	for (i = 1; i < fg->patternLen; i++) {
		fg->qsBc[fg->pattern[i]] = fg->patternLen - i;
		/*
		 * If case is ignored, make the jump apply to both upper and
		 * lower cased characters.  As the pattern is stored in upper
		 * case, apply the same to the lower case equivalents.
		 */
		if (iflag)
			fg->qsBc[tolower(fg->pattern[i])] = fg->patternLen - i;
	}
}
#endif

/*
 * Returns: -1 on failure, 0 on success
 */
int
fastcomp(fastgrep_t *fg, const char *pattern)
{
#ifdef SMALL
	return -1;
#else
	int i;
	int bol = 0;
	int eol = 0;
	int shiftPatternLen;
	int hasDot = 0;
	int firstHalfDot = -1;
	int firstLastHalfDot = -1;
	int lastHalfDot = 0;

	/* Initialize. */
	fg->patternLen = strlen(pattern);
	fg->bol = 0;
	fg->eol = 0;
	fg->wmatch = 0;
	fg->reversedSearch = 0;

	/* Remove end-of-line character ('$'). */
	if (fg->patternLen > 0 && pattern[fg->patternLen - 1] == '$') {
		eol++;
		fg->eol = 1;
		fg->patternLen--;
	}

	/* Remove beginning-of-line character ('^'). */
	if (pattern[0] == '^') {
		bol++;
		fg->bol = 1;
		fg->patternLen--;
	}

	/* Remove enclosing [[:<:]] and [[:>:]] (word match). */
	if (wflag) {
		/* basic re's use \( \), extended re's ( ) */
		int extra = Eflag ? 1 : 2;
		fg->patternLen -= 14 + 2 * extra;
		fg->wmatch = 7 + extra;
	} else if (fg->patternLen >= 14 &&
	    strncmp(pattern + fg->bol, "[[:<:]]", 7) == 0 &&
	    strncmp(pattern + fg->bol + fg->patternLen - 7, "[[:>:]]", 7) == 0) {
		fg->patternLen -= 14;
		fg->wmatch = 7;
	}

	/*
	 * Copy pattern minus '^' and '$' characters as well as word
	 * match character classes at the beginning and ending of the
	 * string respectively.
	 */
	fg->pattern = grep_malloc(fg->patternLen + 1);
	memcpy(fg->pattern, pattern + bol + fg->wmatch, fg->patternLen);
	fg->pattern[fg->patternLen] = '\0';

	/* Look for ways to cheat...er...avoid the full regex engine. */
	for (i = 0; i < fg->patternLen; i++)
	{
		switch (fg->pattern[i]) {
		case '.':
			hasDot = i;
			if (i < fg->patternLen / 2) {
				if (firstHalfDot < 0)
					/* Closest dot to the beginning */
					firstHalfDot = i;
			} else {
				/* Closest dot to the end of the pattern. */
				lastHalfDot = i;
				if (firstLastHalfDot < 0)
					firstLastHalfDot = i;
			}
			break;
		case '(': case ')':
		case '{': case '}':
			/* Special in BRE if preceded by '\\' */
		case '?':
		case '+':
		case '|':
			/* Not special in BRE. */
			if (!Eflag)
				goto nonspecial;
		case '\\':
		case '*':
		case '[': case ']':
			/* Free memory and let others know this is empty. */
			free(fg->pattern);
			fg->pattern = NULL;
			return (-1);
		default:
nonspecial:
			if (iflag)
				fg->pattern[i] = toupper(fg->pattern[i]);
			break;
		}
	}

	/*
	 * Determine if a reverse search would be faster based on the placement
	 * of the dots.
	 */
	if ((!(lflag || cflag || oflag)) && ((!(bol || eol)) &&
	    ((lastHalfDot) && ((firstHalfDot < 0) ||
	    ((fg->patternLen - (lastHalfDot + 1)) < firstHalfDot))))) {
		fg->reversedSearch = 1;
		hasDot = fg->patternLen - (firstHalfDot < 0 ?
		    firstLastHalfDot : firstHalfDot) - 1;
		grep_revstr(fg->pattern, fg->patternLen);
	}

	/*
	 * Normal Quick Search would require a shift based on the position the
	 * next character after the comparison is within the pattern.  With
	 * wildcards, the position of the last dot effects the maximum shift
	 * distance.
	 * The closer to the end the wild card is the slower the search.  A
	 * reverse version of this algorithm would be useful for wildcards near
	 * the end of the string.
	 *
	 * Examples:
	 * Pattern	Max shift
	 * -------	---------
	 * this		5
	 * .his		4
	 * t.is		3
	 * th.s		2
	 * thi.		1
	 */

	/* Adjust the shift based on location of the last dot ('.'). */
	shiftPatternLen = fg->patternLen - hasDot;

	/* Preprocess pattern. */
	for (i = 0; i <= UCHAR_MAX; i++)
		fg->qsBc[i] = shiftPatternLen;
	for (i = hasDot + 1; i < fg->patternLen; i++) {
		fg->qsBc[fg->pattern[i]] = fg->patternLen - i;
		/*
		 * If case is ignored, make the jump apply to both upper and
		 * lower cased characters.  As the pattern is stored in upper
		 * case, apply the same to the lower case equivalents.
		 */
		if (iflag)
			fg->qsBc[tolower(fg->pattern[i])] = fg->patternLen - i;
	}

	/*
	 * Put pattern back to normal after pre-processing to allow for easy
	 * comparisons later.
	 */
	if (fg->reversedSearch)
		grep_revstr(fg->pattern, fg->patternLen);

	return (0);
#endif
}

/*
 * Word boundaries using regular expressions are defined as the point
 * of transition from a non-word char to a word char, or vice versa.
 * This means that grep -w +a and grep -w a+ never match anything,
 * because they lack a starting or ending transition, but grep -w a+b
 * does match a line containing a+b.
 */
#define wmatch(d, l, s, e)	\
	((s == 0 || !isword(d[s-1])) && (e == l || !isword(d[e])) && \
	  e > s && isword(d[s]) && isword(d[e-1]))

static int
grep_search(fastgrep_t *fg, char *data, size_t dataLen, regmatch_t *pmatch,
    int flags)
{
#ifdef SMALL
	return 0;
#else
	regoff_t j;
	int rtrnVal = REG_NOMATCH;

	pmatch->rm_so = -1;
	pmatch->rm_eo = -1;

	/* No point in going farther if we do not have enough data. */
	if (dataLen < fg->patternLen)
		return (rtrnVal);

	/* Only try once at the beginning or ending of the line. */
	if (fg->bol || fg->eol) {
		if (fg->bol && (flags & REG_NOTBOL))
			return 0;
		/* Simple text comparison. */
		/* Verify data is >= pattern length before searching on it. */
		if (dataLen >= fg->patternLen) {
			/* Determine where in data to start search at. */
			if (fg->eol)
				j = dataLen - fg->patternLen;
			else
				j = 0;
			if (!((fg->bol && fg->eol) && (dataLen != fg->patternLen)))
				if (grep_cmp(fg->pattern, data + j,
				    fg->patternLen)) {
					pmatch->rm_so = j;
					pmatch->rm_eo = j + fg->patternLen;
					if (!fg->wmatch || wmatch(data, dataLen,
					    pmatch->rm_so, pmatch->rm_eo))
						rtrnVal = 0;
				}
		}
	} else if (fg->reversedSearch) {
		/* Quick Search algorithm. */
		j = dataLen;
		do {
			if (grep_cmp(fg->pattern, data + j - fg->patternLen,
			    fg->patternLen)) {
				pmatch->rm_so = j - fg->patternLen;
				pmatch->rm_eo = j;
				if (!fg->wmatch || wmatch(data, dataLen,
				    pmatch->rm_so, pmatch->rm_eo)) {
					rtrnVal = 0;
					break;
				}
			}
			/* Shift if within bounds, otherwise, we are done. */
			if (j == fg->patternLen)
				break;
			j -= fg->qsBc[(unsigned char)data[j - fg->patternLen - 1]];
		} while (j >= fg->patternLen);
	} else {
		/* Quick Search algorithm. */
		j = 0;
		do {
			if (grep_cmp(fg->pattern, data + j, fg->patternLen)) {
				pmatch->rm_so = j;
				pmatch->rm_eo = j + fg->patternLen;
				if (fg->patternLen == 0 || !fg->wmatch ||
				    wmatch(data, dataLen, pmatch->rm_so,
				    pmatch->rm_eo)) {
					rtrnVal = 0;
					break;
				}
			}

			/* Shift if within bounds, otherwise, we are done. */
			if (j + fg->patternLen == dataLen)
				break;
			else
				j += fg->qsBc[(unsigned char)data[j + fg->patternLen]];
		} while (j <= (dataLen - fg->patternLen));
	}

	return (rtrnVal);
#endif
}


void *
grep_malloc(size_t size)
{
	void	*ptr;

	if ((ptr = malloc(size)) == NULL)
		err(2, "malloc");
	return ptr;
}

void *
grep_calloc(size_t nmemb, size_t size)
{
	void	*ptr;

	if ((ptr = calloc(nmemb, size)) == NULL)
		err(2, "calloc");
	return ptr;
}

void *
grep_realloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL)
		err(2, "realloc");
	return ptr;
}

void *
grep_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if ((ptr = reallocarray(ptr, nmemb, size)) == NULL)
		err(2, "reallocarray");
	return ptr;
}

#ifndef SMALL
/*
 * Returns:	true on success, false on failure
 */
static bool
grep_cmp(const char *pattern, const char *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (((pattern[i] == data[i]) || (!Fflag && pattern[i] == '.'))
		    || (iflag && pattern[i] == toupper((unsigned char)data[i])))
			continue;
		return false;
	}

	return true;
}

static void
grep_revstr(unsigned char *str, int len)
{
	int i;
	char c;

	for (i = 0; i < len / 2; i++) {
		c = str[i];
		str[i] = str[len - i - 1];
		str[len - i - 1] = c;
	}
}
#endif

void
printline(str_t *line, int sep, regmatch_t *pmatch)
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
		printf("%lld", line->line_no);
		++n;
	}
	if (bflag) {
		if (n)
			putchar(sep);
		printf("%lld", (long long)line->off);
		++n;
	}
	if (n)
		putchar(sep);
	if (pmatch)
		fwrite(line->dat + pmatch->rm_so,
		    pmatch->rm_eo - pmatch->rm_so, 1, stdout);
	else
		fwrite(line->dat, line->len, 1, stdout);
	putchar('\n');
}
