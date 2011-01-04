/*	$Id: term.c,v 1.55 2011/01/04 22:28:17 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010 Ingo Schwarze <schwarze@openbsd.org>
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
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "chars.h"
#include "out.h"
#include "term.h"
#include "main.h"

static	void		  spec(struct termp *, enum roffdeco,
				const char *, size_t);
static	void		  res(struct termp *, const char *, size_t);
static	void		  bufferc(struct termp *, char);
static	void		  adjbuf(struct termp *p, size_t);
static	void		  encode(struct termp *, const char *, size_t);


void
term_free(struct termp *p)
{

	if (p->buf)
		free(p->buf);
	if (p->symtab)
		chars_free(p->symtab);

	free(p);
}


void
term_begin(struct termp *p, term_margin head, 
		term_margin foot, const void *arg)
{

	p->headf = head;
	p->footf = foot;
	p->argf = arg;
	(*p->begin)(p);
}


void
term_end(struct termp *p)
{

	(*p->end)(p);
}


struct termp *
term_alloc(enum termenc enc)
{
	struct termp	*p;

	p = calloc(1, sizeof(struct termp));
	if (NULL == p) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	p->enc = enc;
	return(p);
}


/*
 * Flush a line of text.  A "line" is loosely defined as being something
 * that should be followed by a newline, regardless of whether it's
 * broken apart by newlines getting there.  A line can also be a
 * fragment of a columnar list (`Bl -tag' or `Bl -column'), which does
 * not have a trailing newline.
 *
 * The following flags may be specified:
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
	size_t		 vend;	/* end of word visual position on output */
	size_t		 bp;    /* visual right border position */
	size_t		 dv;    /* temporary for visual pos calculations */
	int		 j;     /* temporary loop index for p->buf */
	int		 jhy;	/* last hyph before overflow w/r/t j */
	size_t		 maxvis; /* output position of visible boundary */
	size_t		 mmax; /* used in calculating bp */

	/*
	 * First, establish the maximum columns of "visible" content.
	 * This is usually the difference between the right-margin and
	 * an indentation, but can be, for tagged lists or columns, a
	 * small set of values. 
	 */
	assert  (p->rmargin >= p->offset);
	dv     = p->rmargin - p->offset;
	maxvis = (int)dv > p->overstep ? dv - (size_t)p->overstep : 0;
	dv     = p->maxrmargin - p->offset;
	mmax   = (int)dv > p->overstep ? dv - (size_t)p->overstep : 0;

	bp = TERMP_NOBREAK & p->flags ? mmax : maxvis;

	/*
	 * Indent the first line of a paragraph.
	 */
	vbl = p->flags & TERMP_NOLPAD ? (size_t)0 : p->offset;

	vis = vend = 0;
	i = 0;

	while (i < (int)p->col) {
		/*
		 * Handle literal tab characters: collapse all
		 * subsequent tabs into a single huge set of spaces.
		 */
		while (i < (int)p->col && '\t' == p->buf[i]) {
			vend = (vis / p->tabwidth + 1) * p->tabwidth;
			vbl += vend - vis;
			vis = vend;
			i++;
		}

		/*
		 * Count up visible word characters.  Control sequences
		 * (starting with the CSI) aren't counted.  A space
		 * generates a non-printing word, which is valid (the
		 * space is printed according to regular spacing rules).
		 */

		for (j = i, jhy = 0; j < (int)p->col; j++) {
			if ((j && ' ' == p->buf[j]) || '\t' == p->buf[j])
				break;

			/* Back over the the last printed character. */
			if (8 == p->buf[j]) {
				assert(j);
				vend -= (*p->width)(p, p->buf[j - 1]);
				continue;
			}

			/* Regular word. */
			/* Break at the hyphen point if we overrun. */
			if (vend > vis && vend < bp && 
					ASCII_HYPH == p->buf[j])
				jhy = j;

			vend += (*p->width)(p, p->buf[j]);
		}

		/*
		 * Find out whether we would exceed the right margin.
		 * If so, break to the next line.
		 */
		if (vend > bp && 0 == jhy && vis > 0) {
			vend -= vis;
			(*p->endline)(p);
			if (TERMP_NOBREAK & p->flags) {
				p->viscol = p->rmargin;
				(*p->advance)(p, p->rmargin);
				vend += p->rmargin - p->offset;
			} else {
				p->viscol = 0;
				vbl = p->offset;
			}

			/* Remove the p->overstep width. */

			bp += (size_t)p->overstep;
			p->overstep = 0;
		}

		/* Write out the [remaining] word. */
		for ( ; i < (int)p->col; i++) {
			if (vend > bp && jhy > 0 && i > jhy)
				break;
			if ('\t' == p->buf[i])
				break;
			if (' ' == p->buf[i]) {
				j = i;
				while (' ' == p->buf[i])
					i++;
				dv = (size_t)(i - j) * (*p->width)(p, ' ');
				vbl += dv;
				vend += dv;
				break;
			}
			if (ASCII_NBRSP == p->buf[i]) {
				vbl += (*p->width)(p, ' ');
				continue;
			}

			/*
			 * Now we definitely know there will be
			 * printable characters to output,
			 * so write preceding white space now.
			 */
			if (vbl) {
				(*p->advance)(p, vbl);
				p->viscol += vbl;
				vbl = 0;
			}

			if (ASCII_HYPH == p->buf[i]) {
				(*p->letter)(p, '-');
				p->viscol += (*p->width)(p, '-');
			} else {
				(*p->letter)(p, p->buf[i]);
				p->viscol += (*p->width)(p, p->buf[i]);
			}
		}
		vis = vend;
	}

	/*
	 * If there was trailing white space, it was not printed;
	 * so reset the cursor position accordingly.
	 */
	vis -= vbl;

	p->col = 0;
	p->overstep = 0;

	if ( ! (TERMP_NOBREAK & p->flags)) {
		p->viscol = 0;
		(*p->endline)(p);
		return;
	}

	if (TERMP_HANG & p->flags) {
		/* We need one blank after the tag. */
		p->overstep = (int)(vis - maxvis + (*p->width)(p, ' '));

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
		if (p->overstep >= -1) {
			assert((int)maxvis + p->overstep >= 0);
			maxvis += (size_t)p->overstep;
		} else
			p->overstep = 0;

	} else if (TERMP_DANGLE & p->flags)
		return;

	/* Right-pad. */
	if (maxvis > vis +
	    ((TERMP_TWOSPACE & p->flags) ? (*p->width)(p, ' ') : 0)) {
		p->viscol += maxvis - vis;
		(*p->advance)(p, maxvis - vis);
		vis += (maxvis - vis);
	} else {	/* ...or newline break. */
		(*p->endline)(p);
		p->viscol = p->rmargin;
		(*p->advance)(p, p->rmargin);
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
	if (0 == p->col && 0 == p->viscol) {
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
	p->viscol = 0;
	(*p->endline)(p);
}


static void
spec(struct termp *p, enum roffdeco d, const char *word, size_t len)
{
	const char	*rhs;
	size_t		 sz;

	rhs = chars_spec2str(p->symtab, word, len, &sz);
	if (rhs) 
		encode(p, rhs, sz);
	else if (DECO_SSPECIAL == d)
		encode(p, word, len);
}


static void
res(struct termp *p, const char *word, size_t len)
{
	const char	*rhs;
	size_t		 sz;

	rhs = chars_res2str(p->symtab, word, len, &sz);
	if (rhs)
		encode(p, rhs, sz);
}


void
term_fontlast(struct termp *p)
{
	enum termfont	 f;

	f = p->fontl;
	p->fontl = p->fontq[p->fonti];
	p->fontq[p->fonti] = f;
}


void
term_fontrepl(struct termp *p, enum termfont f)
{

	p->fontl = p->fontq[p->fonti];
	p->fontq[p->fonti] = f;
}


void
term_fontpush(struct termp *p, enum termfont f)
{

	assert(p->fonti + 1 < 10);
	p->fontl = p->fontq[p->fonti];
	p->fontq[++p->fonti] = f;
}


const void *
term_fontq(struct termp *p)
{

	return(&p->fontq[p->fonti]);
}


enum termfont
term_fonttop(struct termp *p)
{

	return(p->fontq[p->fonti]);
}


void
term_fontpopq(struct termp *p, const void *key)
{

	while (p->fonti >= 0 && key != &p->fontq[p->fonti])
		p->fonti--;
	assert(p->fonti >= 0);
}


void
term_fontpop(struct termp *p)
{

	assert(p->fonti);
	p->fonti--;
}


/*
 * Handle pwords, partial words, which may be either a single word or a
 * phrase that cannot be broken down (such as a literal string).  This
 * handles word styling.
 */
void
term_word(struct termp *p, const char *word)
{
	const char	*sv, *seq;
	size_t		 ssz;
	enum roffdeco	 deco;

	sv = word;

	if (word[0] && '\0' == word[1])
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
			if ( ! (TERMP_IGNDELIM & p->flags))
				p->flags |= TERMP_NOSPACE;
			break;
		default:
			break;
		}

	if ( ! (TERMP_NOSPACE & p->flags)) {
		if ( ! (TERMP_KEEP & p->flags)) {
			if (TERMP_PREKEEP & p->flags)
				p->flags |= TERMP_KEEP;
			bufferc(p, ' ');
			if (TERMP_SENTENCE & p->flags)
				bufferc(p, ' ');
		} else
			bufferc(p, ASCII_NBRSP);
	}

	if ( ! (p->flags & TERMP_NONOSPACE))
		p->flags &= ~TERMP_NOSPACE;
	else
		p->flags |= TERMP_NOSPACE;

	p->flags &= ~(TERMP_SENTENCE | TERMP_IGNDELIM);

	while (*word) {
		if ((ssz = strcspn(word, "\\")) > 0)
			encode(p, word, ssz);

		word += ssz;
		if ('\\' != *word)
			continue;

		seq = ++word;
		word += a2roffdeco(&deco, &seq, &ssz);

		switch (deco) {
		case (DECO_RESERVED):
			res(p, seq, ssz);
			break;
		case (DECO_SPECIAL):
			/* FALLTHROUGH */
		case (DECO_SSPECIAL):
			spec(p, deco, seq, ssz);
			break;
		case (DECO_BOLD):
			term_fontrepl(p, TERMFONT_BOLD);
			break;
		case (DECO_ITALIC):
			term_fontrepl(p, TERMFONT_UNDER);
			break;
		case (DECO_ROMAN):
			term_fontrepl(p, TERMFONT_NONE);
			break;
		case (DECO_PREVIOUS):
			term_fontlast(p);
			break;
		default:
			break;
		}

		if (DECO_NOSPACE == deco && '\0' == *word)
			p->flags |= TERMP_NOSPACE;
	}

	/* 
	 * Note that we don't process the pipe: the parser sees it as
	 * punctuation, but we don't in terms of typography.
	 */
	if (sv[0] && '\0' == sv[1])
		switch (sv[0]) {
		case('('):
			/* FALLTHROUGH */
		case('['):
			p->flags |= TERMP_NOSPACE;
			break;
		default:
			break;
		}
}


static void
adjbuf(struct termp *p, size_t sz)
{

	if (0 == p->maxcols)
		p->maxcols = 1024;
	while (sz >= p->maxcols)
		p->maxcols <<= 2;

	p->buf = realloc(p->buf, p->maxcols);
	if (NULL == p->buf) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}
}


static void
bufferc(struct termp *p, char c)
{

	if (p->col + 1 >= p->maxcols)
		adjbuf(p, p->col + 1);

	p->buf[(int)p->col++] = c;
}


static void
encode(struct termp *p, const char *word, size_t sz)
{
	enum termfont	  f;
	int		  i;

	/*
	 * Encode and buffer a string of characters.  If the current
	 * font mode is unset, buffer directly, else encode then buffer
	 * character by character.
	 */

	if (TERMFONT_NONE == (f = term_fonttop(p))) {
		if (p->col + sz >= p->maxcols) 
			adjbuf(p, p->col + sz);
		memcpy(&p->buf[(int)p->col], word, sz);
		p->col += sz;
		return;
	}

	/* Pre-buffer, assuming worst-case. */

	if (p->col + 1 + (sz * 3) >= p->maxcols)
		adjbuf(p, p->col + 1 + (sz * 3));

	for (i = 0; i < (int)sz; i++) {
		if ( ! isgraph((u_char)word[i])) {
			p->buf[(int)p->col++] = word[i];
			continue;
		}

		if (TERMFONT_UNDER == f)
			p->buf[(int)p->col++] = '_';
		else
			p->buf[(int)p->col++] = word[i];

		p->buf[(int)p->col++] = 8;
		p->buf[(int)p->col++] = word[i];
	}
}


size_t
term_len(const struct termp *p, size_t sz)
{

	return((*p->width)(p, ' ') * sz);
}


size_t
term_strlen(const struct termp *p, const char *cp)
{
	size_t		 sz, ssz, rsz, i;
	enum roffdeco	 d;
	const char	*seq, *rhs;

	for (sz = 0; '\0' != *cp; )
		/*
		 * Account for escaped sequences within string length
		 * calculations.  This follows the logic in term_word()
		 * as we must calculate the width of produced strings.
		 */
		if ('\\' == *cp) {
			seq = ++cp;
			cp += a2roffdeco(&d, &seq, &ssz);

			switch (d) {
			case (DECO_RESERVED):
				rhs = chars_res2str
					(p->symtab, seq, ssz, &rsz);
				break;
			case (DECO_SPECIAL):
				/* FALLTHROUGH */
			case (DECO_SSPECIAL):
				rhs = chars_spec2str
					(p->symtab, seq, ssz, &rsz);

				/* Allow for one-char escapes. */
				if (DECO_SSPECIAL != d || rhs)
					break;

				rhs = seq;
				rsz = ssz;
				break;
			default:
				rhs = NULL;
				break;
			}

			if (rhs)
				for (i = 0; i < rsz; i++)
					sz += (*p->width)(p, *rhs++);
		} else if (ASCII_NBRSP == *cp) {
			sz += (*p->width)(p, ' ');
			cp++;
		} else if (ASCII_HYPH == *cp) {
			sz += (*p->width)(p, '-');
			cp++;
		} else
			sz += (*p->width)(p, *cp++);

	return(sz);
}


/* ARGSUSED */
size_t
term_vspan(const struct termp *p, const struct roffsu *su)
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
term_hspan(const struct termp *p, const struct roffsu *su)
{
	double		 v;

	v = ((*p->hspan)(p, su));
	if (v < 0.0)
		v = 0.0;
	return((size_t) /* LINTED */
			v);
}
