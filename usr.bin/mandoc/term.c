/*	$Id: term.c,v 1.19 2009/12/22 23:58:00 schwarze Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chars.h"
#include "out.h"
#include "term.h"
#include "man.h"
#include "mdoc.h"
#include "main.h"

/* FIXME: accomodate non-breaking, non-collapsing white-space. */
/* FIXME: accomodate non-breaking, collapsing white-space. */

static	struct termp	 *term_alloc(enum termenc);
static	void		  term_free(struct termp *);

static	void		  do_escaped(struct termp *, const char **);
static	void		  do_special(struct termp *,
				const char *, size_t);
static	void		  do_reserved(struct termp *,
				const char *, size_t);
static	void		  buffer(struct termp *, char);
static	void		  encode(struct termp *, char);


void *
ascii_alloc(void)
{

	return(term_alloc(TERMENC_ASCII));
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
	if (p->symtab)
		chars_free(p->symtab);

	free(p);
}


static struct termp *
term_alloc(enum termenc enc)
{
	struct termp *p;

	p = calloc(1, sizeof(struct termp));
	if (NULL == p) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	p->maxrmargin = 78;
	p->enc = enc;
	return(p);
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
 * The usage of termp:flags is as follows:
 *
 *  - TERMP_NOLPAD: when beginning to write the line, don't left-pad the
 *    offset value.  This is useful when doing columnar lists where the
 *    prior column has right-padded.
 *
 *  - TERMP_NOBREAK: this is the most important and is used when making
 *    columns.  In short: don't print a newline and instead pad to the
 *    right margin.  Used in conjunction with TERMP_NOLPAD.
 *
 *  - TERMP_TWOSPACE: when padding, make sure there are at least two
 *    space characters of padding.  Otherwise, rather break the line.
 *
 *  - TERMP_DANGLE: don't newline when TERMP_NOBREAK is specified and
 *    the line is overrun, and don't pad-right if it's underrun.
 *
 *  - TERMP_HANG: like TERMP_DANGLE, but doesn't newline when
 *    overruning, instead save the position and continue at that point
 *    when the next invocation.
 *
 *  In-line line breaking:
 *
 *  If TERMP_NOBREAK is specified and the line overruns the right
 *  margin, it will break and pad-right to the right margin after
 *  writing.  If maxrmargin is violated, it will break and continue
 *  writing from the right-margin, which will lead to the above scenario
 *  upon exit.  Otherwise, the line will break at the right margin.
 */
void
term_flushln(struct termp *p)
{
	int		 i;     /* current input position in p->buf */
	size_t		 vis;   /* current visual position on output */
	size_t		 vbl;   /* number of blanks to prepend to output */
	size_t		 vsz;   /* visual characters to write to output */
	size_t		 bp;    /* visual right border position */
	int		 j;     /* temporary loop index */
	size_t		 maxvis, mmax;
	static int	 overstep = 0;

	/*
	 * First, establish the maximum columns of "visible" content.
	 * This is usually the difference between the right-margin and
	 * an indentation, but can be, for tagged lists or columns, a
	 * small set of values. 
	 */

	assert(p->offset < p->rmargin);

	maxvis = (int)(p->rmargin - p->offset) - overstep < 0 ?
		/* LINTED */ 
		0 : p->rmargin - p->offset - overstep;
	mmax = (int)(p->maxrmargin - p->offset) - overstep < 0 ?
		/* LINTED */
		0 : p->maxrmargin - p->offset - overstep;

	bp = TERMP_NOBREAK & p->flags ? mmax : maxvis;

	/* 
	 * FIXME: if bp is zero, we still output the first word before
	 * breaking the line.
	 */

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
			if (j && ' ' == p->buf[j]) 
				break;
			else if (8 == p->buf[j])
				vsz--;
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
			/* Remove the overstep width. */
			bp += (int)/* LINTED */
				overstep;
			overstep = 0;
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

	p->col = 0;
	overstep = 0;

	if ( ! (TERMP_NOBREAK & p->flags)) {
		putchar('\n');
		return;
	}

	if (TERMP_HANG & p->flags) {
		/* We need one blank after the tag. */
		overstep = /* LINTED */
			vis - maxvis + 1;

		/*
		 * Behave exactly the same way as groff:
		 * If we have overstepped the margin, temporarily move
		 * it to the right and flag the rest of the line to be
		 * shorter.
		 * If we landed right at the margin, be happy.
		 * If we are one step before the margin, temporarily
		 * move it one step LEFT and flag the rest of the line
		 * to be longer.
		 */
		if (overstep >= -1) {
			assert((int)maxvis + overstep >= 0);
			/* LINTED */
			maxvis += overstep;
		} else
			overstep = 0;

	} else if (TERMP_DANGLE & p->flags)
		return;

	/* Right-pad. */
	if (maxvis > vis + /* LINTED */
			((TERMP_TWOSPACE & p->flags) ? 1 : 0))  
		for ( ; vis < maxvis; vis++)
			putchar(' ');
	else {	/* ...or newline break. */
		putchar('\n');
		for (i = 0; i < (int)p->rmargin; i++)
			putchar(' ');
	}
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


static void
do_special(struct termp *p, const char *word, size_t len)
{
	const char	*rhs;
	size_t		 sz;
	int		 i;

	rhs = chars_a2ascii(p->symtab, word, len, &sz);

	if (NULL == rhs) {
#if 0
		fputs("Unknown special character: ", stderr);
		for (i = 0; i < (int)len; i++)
			fputc(word[i], stderr);
		fputc('\n', stderr);
#endif
		return;
	}
	for (i = 0; i < (int)sz; i++) 
		encode(p, rhs[i]);
}


static void
do_reserved(struct termp *p, const char *word, size_t len)
{
	const char	*rhs;
	size_t		 sz;
	int		 i;

	rhs = chars_a2res(p->symtab, word, len, &sz);

	if (NULL == rhs) {
#if 0
		fputs("Unknown reserved word: ", stderr);
		for (i = 0; i < (int)len; i++)
			fputc(word[i], stderr);
		fputc('\n', stderr);
#endif
		return;
	}
	for (i = 0; i < (int)sz; i++) 
		encode(p, rhs[i]);
}


/*
 * Handle an escape sequence: determine its length and pass it to the
 * escape-symbol look table.  Note that we assume mdoc(3) has validated
 * the escape sequence (we assert upon badly-formed escape sequences).
 */
static void
do_escaped(struct termp *p, const char **word)
{
	int		 j, type;
	const char	*wp;

	wp = *word;
	type = 1;

	if (0 == *(++wp)) {
		*word = wp;
		return;
	}

	if ('(' == *wp) {
		wp++;
		if (0 == *wp || 0 == *(wp + 1)) {
			*word = 0 == *wp ? wp : wp + 1;
			return;
		}

		do_special(p, wp, 2);
		*word = ++wp;
		return;

	} else if ('*' == *wp) {
		if (0 == *(++wp)) {
			*word = wp;
			return;
		}

		switch (*wp) {
		case ('('):
			wp++;
			if (0 == *wp || 0 == *(wp + 1)) {
				*word = 0 == *wp ? wp : wp + 1;
				return;
			}

			do_reserved(p, wp, 2);
			*word = ++wp;
			return;
		case ('['):
			type = 0;
			break;
		default:
			do_reserved(p, wp, 1);
			*word = wp;
			return;
		}
	
	} else if ('f' == *wp) {
		if (0 == *(++wp)) {
			*word = wp;
			return;
		}

		switch (*wp) {
		case ('B'):
			p->bold++;
			break;
		case ('I'):
			p->under++;
			break;
		case ('P'):
			/* FALLTHROUGH */
		case ('R'):
			p->bold = p->under = 0;
			break;
		default:
			break;
		}

		*word = wp;
		return;

	} else if ('[' != *wp) {
		do_special(p, wp, 1);
		*word = wp;
		return;
	}

	wp++;
	for (j = 0; *wp && ']' != *wp; wp++, j++)
		/* Loop... */ ;

	if (0 == *wp) {
		*word = wp;
		return;
	}

	if (type)
		do_special(p, wp - j, (size_t)j);
	else
		do_reserved(p, wp - j, (size_t)j);
	*word = wp;
}


/*
 * Handle pwords, partial words, which may be either a single word or a
 * phrase that cannot be broken down (such as a literal string).  This
 * handles word styling.
 */
void
term_word(struct termp *p, const char *word)
{
	const char	 *sv;

	sv = word;

	if (word[0] && 0 == word[1])
		switch (word[0]) {
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
			if ( ! (TERMP_IGNDELIM & p->flags))
				p->flags |= TERMP_NOSPACE;
			break;
		default:
			break;
		}

	if ( ! (TERMP_NOSPACE & p->flags))
		buffer(p, ' ');

	if ( ! (p->flags & TERMP_NONOSPACE))
		p->flags &= ~TERMP_NOSPACE;

	for ( ; *word; word++)
		if ('\\' != *word)
			encode(p, *word);
		else
			do_escaped(p, &word);

	if (sv[0] && 0 == sv[1])
		switch (sv[0]) {
		case('('):
			/* FALLTHROUGH */
		case('['):
			/* FALLTHROUGH */
		case('{'):
			p->flags |= TERMP_NOSPACE;
			break;
		default:
			break;
		}
}


/*
 * Insert a single character into the line-buffer.  If the buffer's
 * space is exceeded, then allocate more space by doubling the buffer
 * size.
 */
static void
buffer(struct termp *p, char c)
{
	size_t		 s;

	if (p->col + 1 >= p->maxcols) {
		if (0 == p->maxcols)
			p->maxcols = 256;
		s = p->maxcols * 2;
		p->buf = realloc(p->buf, s);
		if (NULL == p->buf) {
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		p->maxcols = s;
	}
	p->buf[(int)(p->col)++] = c;
}


static void
encode(struct termp *p, char c)
{
	
	if (' ' != c) {
		if (p->under) {
			buffer(p, '_');
			buffer(p, 8);
		}
		if (p->bold) {
			buffer(p, c);
			buffer(p, 8);
		}
	}
	buffer(p, c);
}


size_t
term_vspan(const struct roffsu *su)
{
	double		 r;

	switch (su->unit) {
	case (SCALE_CM):
		r = su->scale * 2;
		break;
	case (SCALE_IN):
		r = su->scale * 6;
		break;
	case (SCALE_PC):
		r = su->scale;
		break;
	case (SCALE_PT):
		r = su->scale / 8;
		break;
	case (SCALE_MM):
		r = su->scale / 1000;
		break;
	case (SCALE_VS):
		r = su->scale;
		break;
	default:
		r = su->scale - 1;
		break;
	}

	if (r < 0.0)
		r = 0.0;
	return(/* LINTED */(size_t)
			r);
}


size_t
term_hspan(const struct roffsu *su)
{
	double		 r;

	/* XXX: CM, IN, and PT are approximations. */

	switch (su->unit) {
	case (SCALE_CM):
		r = 4 * su->scale;
		break;
	case (SCALE_IN):
		/* XXX: this is an approximation. */
		r = 10 * su->scale;
		break;
	case (SCALE_PC):
		r = (10 * su->scale) / 6;
		break;
	case (SCALE_PT):
		r = (10 * su->scale) / 72;
		break;
	case (SCALE_MM):
		r = su->scale / 1000; /* FIXME: double-check. */
		break;
	case (SCALE_VS):
		r = su->scale * 2 - 1; /* FIXME: double-check. */
		break;
	default:
		r = su->scale;
		break;
	}

	if (r < 0.0)
		r = 0.0;
	return((size_t)/* LINTED */
			r);
}


