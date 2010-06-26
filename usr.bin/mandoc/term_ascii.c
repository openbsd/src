/*	$Id: term_ascii.c,v 1.2 2010/06/26 19:08:00 schwarze Exp $ */
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "out.h"
#include "term.h"
#include "main.h"

static	void		  ascii_endline(struct termp *);
static	void		  ascii_letter(struct termp *, char);
static	void		  ascii_begin(struct termp *);
static	void		  ascii_advance(struct termp *, size_t);
static	void		  ascii_end(struct termp *);
static	size_t		  ascii_width(const struct termp *, char);


void *
ascii_alloc(char *outopts)
{
	struct termp	*p;
	const char	*toks[2];
	char		*v;

	if (NULL == (p = term_alloc(TERMENC_ASCII)))
		return(NULL);

	p->tabwidth = 5;
	p->defrmargin = 78;

	p->type = TERMTYPE_CHAR;
	p->letter = ascii_letter;
	p->begin = ascii_begin;
	p->end = ascii_end;
	p->endline = ascii_endline;
	p->advance = ascii_advance;
	p->width = ascii_width;

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


static size_t
ascii_width(const struct termp *p, char c)
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
ascii_letter(struct termp *p, char c)
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

	/* Just print whitespace on the terminal. */
	for (i = 0; i < len; i++)
		putchar(' ');
}
