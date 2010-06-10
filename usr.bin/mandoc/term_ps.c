/*	$Id: term_ps.c,v 1.1 2010/06/10 22:50:10 schwarze Exp $ */
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

#include <sys/param.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "out.h"
#include "main.h"
#include "term.h"

#define	PS_CHAR_WIDTH	  6
#define	PS_CHAR_HEIGHT	  12
#define	PS_CHAR_TOPMARG	 (792 - 24)
#define	PS_CHAR_TOP	 (PS_CHAR_TOPMARG - 36)
#define	PS_CHAR_LEFT	  36
#define	PS_CHAR_BOTMARG	  24
#define	PS_CHAR_BOT	 (PS_CHAR_BOTMARG + 36)

#define	PS_BUFSLOP	  128
#define	PS_GROWBUF(p, sz) \
	do if ((p)->engine.ps.psmargcur + (sz) > \
			(p)->engine.ps.psmargsz) { \
		(p)->engine.ps.psmargsz += /* CONSTCOND */ \
			MAX(PS_BUFSLOP, (sz)); \
		(p)->engine.ps.psmarg = realloc \
			((p)->engine.ps.psmarg,  \
			 (p)->engine.ps.psmargsz); \
		if (NULL == (p)->engine.ps.psmarg) { \
			perror(NULL); \
			exit(EXIT_FAILURE); \
		} \
	} while (/* CONSTCOND */ 0)

static	void		  ps_letter(struct termp *, char);
static	void		  ps_begin(struct termp *);
static	void		  ps_end(struct termp *);
static	void		  ps_advance(struct termp *, size_t);
static	void		  ps_endline(struct termp *);
static	void		  ps_printf(struct termp *, const char *, ...);
static	void		  ps_putchar(struct termp *, char);


void *
ps_alloc(void)
{
	struct termp	*p;

	if (NULL == (p = term_alloc(TERMENC_ASCII)))
		return(NULL);

	p->type = TERMTYPE_PS;
	p->letter = ps_letter;
	p->begin = ps_begin;
	p->end = ps_end;
	p->advance = ps_advance;
	p->endline = ps_endline;
	return(p);
}


void
ps_free(void *arg)
{
	struct termp	*p;

	p = (struct termp *)arg;

	if (p->engine.ps.psmarg)
		free(p->engine.ps.psmarg);

	term_free(p);
}


static void
ps_printf(struct termp *p, const char *fmt, ...)
{
	va_list		 ap;
	int		 pos;

	va_start(ap, fmt);

	/*
	 * If we're running in regular mode, then pipe directly into
	 * vprintf().  If we're processing margins, then push the data
	 * into our growable margin buffer.
	 */

	if ( ! (PS_MARGINS & p->engine.ps.psstate)) {
		vprintf(fmt, ap);
		va_end(ap);
		return;
	}

	/* 
	 * XXX: I assume that the in-margin print won't exceed
	 * PS_BUFSLOP (128 bytes), which is reasonable but still an
	 * assumption that will cause pukeage if it's not the case.
	 */

	PS_GROWBUF(p, PS_BUFSLOP);

	pos = (int)p->engine.ps.psmargcur;
	vsnprintf(&p->engine.ps.psmarg[pos], PS_BUFSLOP, fmt, ap);
	p->engine.ps.psmargcur = strlen(p->engine.ps.psmarg);

	va_end(ap);
}


static void
ps_putchar(struct termp *p, char c)
{
	int		 pos;

	/* See ps_printf(). */

	if ( ! (PS_MARGINS & p->engine.ps.psstate)) {
		putchar(c);
		return;
	}

	PS_GROWBUF(p, 2);

	pos = (int)p->engine.ps.psmargcur++;
	p->engine.ps.psmarg[pos++] = c;
	p->engine.ps.psmarg[pos] = '\0';
}


/* ARGSUSED */
static void
ps_end(struct termp *p)
{

	/*
	 * At the end of the file, do one last showpage.  This is the
	 * same behaviour as groff(1) and works for multiple pages as
	 * well as just one.
	 */

	assert(p->engine.ps.psmarg && p->engine.ps.psmarg[0]);
	printf("%s", p->engine.ps.psmarg);
	printf("showpage\n");
	printf("%s\n", "%%EOF");
}


static void
ps_begin(struct termp *p)
{

	/*
	 * Emit the standard PostScript prologue, set our initial page
	 * position, then run pageopen() on the initial page.
	 */

	printf("%s\n", "%!PS");
	printf("%s\n", "/Courier");
	printf("%s\n", "10 selectfont");

	p->engine.ps.psstate = 0;

	if (p->engine.ps.psmarg) {
		assert(p->engine.ps.psmargsz);
		p->engine.ps.psmarg[0] = '\0';
	}

	p->engine.ps.psmargcur = 0;

	/*
	 * Now dump the margins into our margin buffer.  If we don't do
	 * this, we'd break any current state to run the header and
	 * footer with each and evern new page.
	 */

	p->engine.ps.pscol = PS_CHAR_LEFT;
	p->engine.ps.psrow = PS_CHAR_TOPMARG;

	p->engine.ps.psstate |= PS_MARGINS;

	(*p->headf)(p, p->argf);
	(*p->endline)(p);

	p->engine.ps.psstate &= ~PS_MARGINS;
	assert(0 == p->engine.ps.psstate);

	p->engine.ps.pscol = PS_CHAR_LEFT;
	p->engine.ps.psrow = PS_CHAR_BOTMARG;
	p->engine.ps.psstate |= PS_MARGINS;

	(*p->footf)(p, p->argf);
	(*p->endline)(p);

	p->engine.ps.psstate &= ~PS_MARGINS;
	assert(0 == p->engine.ps.psstate);

	p->engine.ps.pscol = PS_CHAR_LEFT;
	p->engine.ps.psrow = PS_CHAR_TOP;

	assert(p->engine.ps.psmarg);
	assert('\0' != p->engine.ps.psmarg[0]);
}


static void
ps_letter(struct termp *p, char c)
{
	
	if ( ! (PS_INLINE & p->engine.ps.psstate)) {
		/*
		 * If we're not in a PostScript "word" context, then
		 * open one now at the current cursor.
		 */
		ps_printf(p, "%zu %zu moveto\n(", 
				p->engine.ps.pscol, 
				p->engine.ps.psrow);
		p->engine.ps.psstate |= PS_INLINE;
	}

	/*
	 * We need to escape these characters as per the PostScript
	 * specification.  We would also escape non-graphable characters
	 * (like tabs), but none of them would get to this point and
	 * it's superfluous to abort() on them.
	 */

	switch (c) {
	case ('('):
		/* FALLTHROUGH */
	case (')'):
		/* FALLTHROUGH */
	case ('\\'):
		ps_putchar(p, '\\');
		break;
	default:
		break;
	}

	/* Write the character and adjust where we are on the page. */
	ps_putchar(p, c);
	p->engine.ps.pscol += PS_CHAR_WIDTH;
}


static void
ps_advance(struct termp *p, size_t len)
{

	if (PS_INLINE & p->engine.ps.psstate) {
		/* Dump out any existing line scope. */
		ps_printf(p, ") show\n");
		p->engine.ps.psstate &= ~PS_INLINE;
	}

	p->engine.ps.pscol += len ? len * PS_CHAR_WIDTH : 0;
}


static void
ps_endline(struct termp *p)
{

	if (PS_INLINE & p->engine.ps.psstate) {
		ps_printf(p, ") show\n");
		p->engine.ps.psstate &= ~PS_INLINE;
	} 

	if (PS_MARGINS & p->engine.ps.psstate)
		return;

	p->engine.ps.pscol = PS_CHAR_LEFT;
	if (p->engine.ps.psrow >= PS_CHAR_HEIGHT + PS_CHAR_BOT) {
		p->engine.ps.psrow -= PS_CHAR_HEIGHT;
		return;
	}

	assert(p->engine.ps.psmarg && p->engine.ps.psmarg[0]);
	printf("%s", p->engine.ps.psmarg);
	printf("showpage\n");
	p->engine.ps.psrow = PS_CHAR_TOP;
}
