/*	$Id: term_ascii.c,v 1.6 2011/05/29 21:22:18 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>

#include "mandoc.h"
#include "out.h"
#include "term.h"
#include "main.h"

static	struct termp	 *ascii_init(enum termenc, char *);
static	double		  ascii_hspan(const struct termp *,
				const struct roffsu *);
static	size_t		  ascii_width(const struct termp *, int);
static	void		  ascii_advance(struct termp *, size_t);
static	void		  ascii_begin(struct termp *);
static	void		  ascii_end(struct termp *);
static	void		  ascii_endline(struct termp *);
static	void		  ascii_letter(struct termp *, int);

static	void		  locale_advance(struct termp *, size_t);
static	void		  locale_endline(struct termp *);
static	void		  locale_letter(struct termp *, int);
static	size_t		  locale_width(const struct termp *, int);

static struct termp *
ascii_init(enum termenc enc, char *outopts)
{
	const char	*toks[2];
	char		*v;
	struct termp	*p;

	p = mandoc_calloc(1, sizeof(struct termp));
	p->enc = enc;

	p->tabwidth = 5;
	p->defrmargin = 78;

	p->begin = ascii_begin;
	p->end = ascii_end;
	p->hspan = ascii_hspan;
	p->type = TERMTYPE_CHAR;

	p->enc = TERMENC_ASCII;
	p->advance = ascii_advance;
	p->endline = ascii_endline;
	p->letter = ascii_letter;
	p->width = ascii_width;

	if (TERMENC_ASCII != enc) {
		v = TERMENC_LOCALE == enc ?
			setlocale(LC_ALL, "") :
			setlocale(LC_CTYPE, "UTF-8");
		if (NULL != v && MB_CUR_MAX > 1) {
			p->enc = enc;
			p->advance = locale_advance;
			p->endline = locale_endline;
			p->letter = locale_letter;
			p->width = locale_width;
		}
	}

	toks[0] = "width";
	toks[1] = NULL;

	while (outopts && *outopts)
		switch (getsubopt(&outopts, UNCONST(toks), &v)) {
		case (0):
			p->defrmargin = (size_t)atoi(v);
			break;
		default:
			break;
		}

	/* Enforce a lower boundary. */
	if (p->defrmargin < 58)
		p->defrmargin = 58;

	return(p);
}

void *
ascii_alloc(char *outopts)
{

	return(ascii_init(TERMENC_ASCII, outopts));
}

void *
utf8_alloc(char *outopts)
{

	return(ascii_init(TERMENC_UTF8, outopts));
}


void *
locale_alloc(char *outopts)
{

	return(ascii_init(TERMENC_LOCALE, outopts));
}

/* ARGSUSED */
static size_t
ascii_width(const struct termp *p, int c)
{

	return(1);
}

void
ascii_free(void *arg)
{

	term_free((struct termp *)arg);
}

/* ARGSUSED */
static void
ascii_letter(struct termp *p, int c)
{
	
	putchar(c);
}

static void
ascii_begin(struct termp *p)
{

	(*p->headf)(p, p->argf);
}

static void
ascii_end(struct termp *p)
{

	(*p->footf)(p, p->argf);
}

/* ARGSUSED */
static void
ascii_endline(struct termp *p)
{

	putchar('\n');
}

/* ARGSUSED */
static void
ascii_advance(struct termp *p, size_t len)
{
	size_t	 	i;

	for (i = 0; i < len; i++)
		putchar(' ');
}

/* ARGSUSED */
static double
ascii_hspan(const struct termp *p, const struct roffsu *su)
{
	double		 r;

	/*
	 * Approximate based on character width.  These are generated
	 * entirely by eyeballing the screen, but appear to be correct.
	 */

	switch (su->unit) {
	case (SCALE_CM):
		r = 4 * su->scale;
		break;
	case (SCALE_IN):
		r = 10 * su->scale;
		break;
	case (SCALE_PC):
		r = (10 * su->scale) / 6;
		break;
	case (SCALE_PT):
		r = (10 * su->scale) / 72;
		break;
	case (SCALE_MM):
		r = su->scale / 1000;
		break;
	case (SCALE_VS):
		r = su->scale * 2 - 1;
		break;
	default:
		r = su->scale;
		break;
	}

	return(r);
}

/* ARGSUSED */
static size_t
locale_width(const struct termp *p, int c)
{
	int		rc;

	return((rc = wcwidth(c)) < 0 ? 0 : rc);
}

/* ARGSUSED */
static void
locale_advance(struct termp *p, size_t len)
{
	size_t	 	i;

	for (i = 0; i < len; i++)
		putwchar(L' ');
}

/* ARGSUSED */
static void
locale_endline(struct termp *p)
{

	putwchar(L'\n');
}

/* ARGSUSED */
static void
locale_letter(struct termp *p, int c)
{
	
	putwchar(c);
}
