/*	$Id: term.c,v 1.5 2009/06/23 23:53:43 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term.h"
#include "man.h"
#include "mdoc.h"

extern	int		  man_run(struct termp *, 
				const struct man *);
extern	int		  mdoc_run(struct termp *, 
				const struct mdoc *);

static	struct termp	 *term_alloc(enum termenc);
static	void		  term_free(struct termp *);
static	void		  term_pword(struct termp *, const char *, int);
static	void		  term_pescape(struct termp *, 
				const char *, int *, int);
static	void		  term_nescape(struct termp *,
				const char *, size_t);
static	void		  term_chara(struct termp *, char);
static	void		  term_encodea(struct termp *, char);
static	int		  term_isopendelim(const char *, int);
static	int		  term_isclosedelim(const char *, int);


void *
ascii_alloc(void)
{

	return(term_alloc(TERMENC_ASCII));
}


int
terminal_man(void *arg, const struct man *man)
{
	struct termp	*p;

	p = (struct termp *)arg;
	if (NULL == p->symtab)
		p->symtab = term_ascii2htab();

	return(man_run(p, man));
}


int
terminal_mdoc(void *arg, const struct mdoc *mdoc)
{
	struct termp	*p;

	p = (struct termp *)arg;
	if (NULL == p->symtab)
		p->symtab = term_ascii2htab();

	return(mdoc_run(p, mdoc));
}


void
terminal_free(void *arg)
{

	term_free((struct termp *)arg);
}


static void
term_free(struct termp *p)
{

	if (p->buf)
		free(p->buf);
	if (TERMENC_ASCII == p->enc && p->symtab)
		term_asciifree(p->symtab);

	free(p);
}


static struct termp *
term_alloc(enum termenc enc)
{
	struct termp *p;

	if (NULL == (p = malloc(sizeof(struct termp))))
		err(1, "malloc");
	bzero(p, sizeof(struct termp));
	p->maxrmargin = 78;
	p->enc = enc;
	return(p);
}


static int
term_isclosedelim(const char *p, int len)
{

	if (1 != len)
		return(0);

	switch (*p) {
	case('.'):
		/* FALLTHROUGH */
	case(','):
		/* FALLTHROUGH */
	case(';'):
		/* FALLTHROUGH */
	case(':'):
		/* FALLTHROUGH */
	case('?'):
		/* FALLTHROUGH */
	case('!'):
		/* FALLTHROUGH */
	case(')'):
		/* FALLTHROUGH */
	case(']'):
		/* FALLTHROUGH */
	case('}'):
		return(1);
	default:
		break;
	}

	return(0);
}


static int
term_isopendelim(const char *p, int len)
{

	if (1 != len)
		return(0);

	switch (*p) {
	case('('):
		/* FALLTHROUGH */
	case('['):
		/* FALLTHROUGH */
	case('{'):
		return(1);
	default:
		break;
	}

	return(0);
}


/*
 * Flush a line of text.  A "line" is loosely defined as being something
 * that should be followed by a newline, regardless of whether it's
 * broken apart by newlines getting there.  A line can also be a
 * fragment of a columnar list.
 *
 * Specifically, a line is whatever's in p->buf of length p->col, which
 * is zeroed after this function returns.
 *
 * The variables TERMP_NOLPAD, TERMP_LITERAL and TERMP_NOBREAK are of
 * critical importance here.  Their behaviour follows:
 *
 *  - TERMP_NOLPAD: when beginning to write the line, don't left-pad the
 *    offset value.  This is useful when doing columnar lists where the
 *    prior column has right-padded.
 *
 *  - TERMP_NOBREAK: this is the most important and is used when making
 *    columns.  In short: don't print a newline and instead pad to the
 *    right margin.  Used in conjunction with TERMP_NOLPAD.
 *
 *  - TERMP_NONOBREAK: don't newline when TERMP_NOBREAK is specified.
 *
 *  In-line line breaking:
 *
 *  If TERMP_NOBREAK is specified and the line overruns the right
 *  margin, it will break and pad-right to the right margin after
 *  writing.  If maxrmargin is violated, it will break and continue
 *  writing from the right-margin, which will lead to the above
 *  scenario upon exit.
 *
 *  Otherwise, the line will break at the right margin.  Extremely long
 *  lines will cause the system to emit a warning (TODO: hyphenate, if
 *  possible).
 *
 *  FIXME: newline breaks occur (in groff) also occur when a single
 *  space follows a NOBREAK!
 */
void
term_flushln(struct termp *p)
{
	int		 i, j;
	size_t		 vbl, vsz, vis, maxvis, mmax, bp;

	/*
	 * First, establish the maximum columns of "visible" content.
	 * This is usually the difference between the right-margin and
	 * an indentation, but can be, for tagged lists or columns, a
	 * small set of values.
	 */

	assert(p->offset < p->rmargin);
	maxvis = p->rmargin - p->offset;
	mmax = p->maxrmargin - p->offset;
	bp = TERMP_NOBREAK & p->flags ? mmax : maxvis;
	vis = 0;

	/*
	 * If in the standard case (left-justified), then begin with our
	 * indentation, otherwise (columns, etc.) just start spitting
	 * out text.
	 */

	if ( ! (p->flags & TERMP_NOLPAD))
		/* LINTED */
		for (j = 0; j < (int)p->offset; j++)
			putchar(' ');

	for (i = 0; i < (int)p->col; i++) {
		/*
		 * Count up visible word characters.  Control sequences
		 * (starting with the CSI) aren't counted.  A space
		 * generates a non-printing word, which is valid (the
		 * space is printed according to regular spacing rules).
		 */

		/* LINTED */
		for (j = i, vsz = 0; j < (int)p->col; j++) {
			if (' ' == p->buf[j])
				break;
			else if (8 == p->buf[j])
				j += 1;
			else
				vsz++;
		}

		/*
		 * Choose the number of blanks to prepend: no blank at the
		 * beginning of a line, one between words -- but do not
		 * actually write them yet.
		 */
		vbl = (size_t)(0 == vis ? 0 : 1);

		/*
		 * Find out whether we would exceed the right margin.
		 * If so, break to the next line.  (TODO: hyphenate)
		 * Otherwise, write the chosen number of blanks now.
		 */
		if (vis && vis + vbl + vsz > bp) {
			putchar('\n');
			if (TERMP_NOBREAK & p->flags) {
				for (j = 0; j < (int)p->rmargin; j++)
					putchar(' ');
				vis = p->rmargin - p->offset;
			} else {
				for (j = 0; j < (int)p->offset; j++)
					putchar(' ');
				vis = 0;
			}
		} else {
			for (j = 0; j < (int)vbl; j++)
				putchar(' ');
			vis += vbl;
		}

		/*
		 * Finally, write out the word.
		 */
		for ( ; i < (int)p->col; i++) {
			if (' ' == p->buf[i])
				break;
			putchar(p->buf[i]);
		}
		vis += vsz;
	}

	/*
	 * If we've overstepped our maximum visible no-break space, then
	 * cause a newline and offset at the right margin.
	 */

	if ((TERMP_NOBREAK & p->flags) && vis >= maxvis) {
		if ( ! (TERMP_NONOBREAK & p->flags)) {
			putchar('\n');
			for (i = 0; i < (int)p->rmargin; i++)
				putchar(' ');
		}
		p->col = 0;
		return;
	}

	/*
	 * If we're not to right-marginalise it (newline), then instead
	 * pad to the right margin and stay off.
	 */

	if (p->flags & TERMP_NOBREAK) {
		if ( ! (TERMP_NONOBREAK & p->flags))
			for ( ; vis < maxvis; vis++)
				putchar(' ');
	} else
		putchar('\n');

	p->col = 0;
}


/* 
 * A newline only breaks an existing line; it won't assert vertical
 * space.  All data in the output buffer is flushed prior to the newline
 * assertion.
 */
void
term_newln(struct termp *p)
{

	p->flags |= TERMP_NOSPACE;
	if (0 == p->col) {
		p->flags &= ~TERMP_NOLPAD;
		return;
	}
	term_flushln(p);
	p->flags &= ~TERMP_NOLPAD;
}


/*
 * Asserts a vertical space (a full, empty line-break between lines).
 * Note that if used twice, this will cause two blank spaces and so on.
 * All data in the output buffer is flushed prior to the newline
 * assertion.
 */
void
term_vspace(struct termp *p)
{

	term_newln(p);
	putchar('\n');
}


/*
 * Break apart a word into "pwords" (partial-words, usually from
 * breaking up a phrase into individual words) and, eventually, put them
 * into the output buffer.  If we're a literal word, then don't break up
 * the word and put it verbatim into the output buffer.
 */
void
term_word(struct termp *p, const char *word)
{
	int 		 i, j, len;

	len = (int)strlen(word);

	if (p->flags & TERMP_LITERAL) {
		term_pword(p, word, len);
		return;
	}

	/* LINTED */
	for (j = i = 0; i < len; i++) {
		if (' ' != word[i]) {
			j++;
			continue;
		} 
		
		/* Escaped spaces don't delimit... */
		if (i && ' ' == word[i] && '\\' == word[i - 1]) {
			j++;
			continue;
		}

		if (0 == j)
			continue;
		assert(i >= j);
		term_pword(p, &word[i - j], j);
		j = 0;
	}
	if (j > 0) {
		assert(i >= j);
		term_pword(p, &word[i - j], j);
	}
}


/*
 * Determine the symbol indicated by an escape sequences, that is, one
 * starting with a backslash.  Once done, we pass this value into the
 * output buffer by way of the symbol table.
 */
static void
term_nescape(struct termp *p, const char *word, size_t len)
{
	const char	*rhs;
	size_t		 sz;
	int		 i;

	rhs = term_a2ascii(p->symtab, word, len, &sz);
	if (rhs)
		for (i = 0; i < (int)sz; i++) 
			term_encodea(p, rhs[i]);
}


/*
 * Handle an escape sequence: determine its length and pass it to the
 * escape-symbol look table.  Note that we assume mdoc(3) has validated
 * the escape sequence (we assert upon badly-formed escape sequences).
 */
static void
term_pescape(struct termp *p, const char *word, int *i, int len)
{
	int		 j;

	if (++(*i) >= len)
		return;

	if ('(' == word[*i]) {
		(*i)++;
		if (*i + 1 >= len)
			return;

		term_nescape(p, &word[*i], 2);
		(*i)++;
		return;

	} else if ('*' == word[*i]) { 
		(*i)++;
		if (*i >= len)
			return;

		switch (word[*i]) {
		case ('('):
			(*i)++;
			if (*i + 1 >= len)
				return;

			term_nescape(p, &word[*i], 2);
			(*i)++;
			return;
		case ('['):
			break;
		default:
			term_nescape(p, &word[*i], 1);
			return;
		}
	
	} else if ('f' == word[*i]) {
		(*i)++;
		if (*i >= len)
			return;
		switch (word[*i]) {
		case ('B'):
			p->flags |= TERMP_BOLD;
			break;
		case ('I'):
			p->flags |= TERMP_UNDER;
			break;
		case ('P'):
			/* FALLTHROUGH */
		case ('R'):
			p->flags &= ~TERMP_STYLE;
			break;
		default:
			break;
		}
		return;

	} else if ('[' != word[*i]) {
		term_nescape(p, &word[*i], 1);
		return;
	}

	(*i)++;
	for (j = 0; word[*i] && ']' != word[*i]; (*i)++, j++)
		/* Loop... */ ;

	if (0 == word[*i])
		return;

	term_nescape(p, &word[*i - j], (size_t)j);
}


/*
 * Handle pwords, partial words, which may be either a single word or a
 * phrase that cannot be broken down (such as a literal string).  This
 * handles word styling.
 */
static void
term_pword(struct termp *p, const char *word, int len)
{
	int		 i;

	if (term_isclosedelim(word, len))
		if ( ! (TERMP_IGNDELIM & p->flags))
			p->flags |= TERMP_NOSPACE;

	if ( ! (TERMP_NOSPACE & p->flags))
		term_chara(p, ' ');

	if ( ! (p->flags & TERMP_NONOSPACE))
		p->flags &= ~TERMP_NOSPACE;

	/* 
	 * If ANSI (word-length styling), then apply our style now,
	 * before the word.
	 */

	for (i = 0; i < len; i++) 
		if ('\\' == word[i]) 
			term_pescape(p, word, &i, len);
		else
			term_encodea(p, word[i]);

	if (term_isopendelim(word, len))
		p->flags |= TERMP_NOSPACE;
}


/*
 * Insert a single character into the line-buffer.  If the buffer's
 * space is exceeded, then allocate more space by doubling the buffer
 * size.
 */
static void
term_chara(struct termp *p, char c)
{
	size_t		 s;

	if (p->col + 1 >= p->maxcols) {
		if (0 == p->maxcols)
			p->maxcols = 256;
		s = p->maxcols * 2;
		p->buf = realloc(p->buf, s);
		if (NULL == p->buf)
			err(1, "realloc");
		p->maxcols = s;
	}
	p->buf[(int)(p->col)++] = c;
}


static void
term_encodea(struct termp *p, char c)
{

	if (TERMP_STYLE & p->flags) {
		if (TERMP_BOLD & p->flags) {
			term_chara(p, c);
			term_chara(p, 8);
		}
		if (TERMP_UNDER & p->flags) {
			term_chara(p, '_');
			term_chara(p, 8);
		}
	}
	term_chara(p, c);
}
