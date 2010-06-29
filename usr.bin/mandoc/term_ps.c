/*	$Id: term_ps.c,v 1.5 2010/06/29 15:49:52 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "out.h"
#include "main.h"
#include "term.h"

struct	glyph {
	int		  wx; /* WX in AFM */
};

struct	font {
	const char	 *name; /* FontName in AFM */
#define	MAXCHAR		  95 /* total characters we can handle */
	struct glyph	  gly[MAXCHAR]; /* glyph metrics */
};

/*
 * We define, for the time being, three fonts: bold, oblique/italic, and
 * normal (roman).  The following table hard-codes the font metrics for
 * ASCII, i.e., 32--127.
 */

static	const struct font fonts[TERMFONT__MAX] = {
	{ "Courier", {
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
	} },
	{ "Courier-Bold", {
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
	} },
	{ "Courier-Oblique", {
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
		{ 600 },
	} },
};

/* These work the buffer used by the header and footer. */
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
static	void		  ps_fclose(struct termp *);
static	size_t		  ps_width(const struct termp *, char);
static	void		  ps_pclose(struct termp *);
static	void		  ps_pletter(struct termp *, int);
static	void		  ps_printf(struct termp *, const char *, ...);
static	void		  ps_putchar(struct termp *, char);
static	void		  ps_setfont(struct termp *, enum termfont);


void *
ps_alloc(char *outopts)
{
	struct termp	*p;
	size_t		 pagex, pagey, margin;
	const char	*toks[2];
	char		*v;

	if (NULL == (p = term_alloc(TERMENC_ASCII)))
		return(NULL);

	/* Default is USA letter. */
	pagex = 612;
	pagey = 792;
	margin = 72;

	p->type = TERMTYPE_PS;
	p->letter = ps_letter;
	p->begin = ps_begin;
	p->end = ps_end;
	p->advance = ps_advance;
	p->endline = ps_endline;
	p->width = ps_width;

	toks[0] = "paper";
	toks[1] = NULL;

	while (outopts && *outopts)
		switch (getsubopt(&outopts, UNCONST(toks), &v)) {
		case (0):
			if (0 == strcasecmp(v, "a4")) {
				pagex = 595;
				pagey = 842;
			} else if (0 == strcasecmp(v, "letter")) {
				pagex = 612;
				pagey = 792;
			}
			break;
		default:
			break;
		}

	assert(margin * 2 < pagex);
	assert(margin * 2 < pagey);

	p->engine.ps.width = pagex;
	p->engine.ps.height = pagey;
	p->engine.ps.header = pagey - (margin / 2);
	p->engine.ps.top = pagey - margin;
	p->engine.ps.footer = (margin / 2);
	p->engine.ps.bottom = margin;
	p->engine.ps.left = margin;
	p->engine.ps.lineheight = 12;

	p->defrmargin = pagex - (margin * 2);
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

	assert(0 == p->engine.ps.psstate);
	assert('\0' == p->engine.ps.last);
	assert(p->engine.ps.psmarg && p->engine.ps.psmarg[0]);
	printf("%s", p->engine.ps.psmarg);
	p->engine.ps.pages++;
	printf("showpage\n");

	printf("%%%%Trailer\n");
	printf("%%%%Pages: %zu\n", p->engine.ps.pages);
	printf("%%%%EOF\n");
}


static void
ps_begin(struct termp *p)
{
	time_t		 t;
	int		 i;

	/* 
	 * Print margins into margin buffer.  Nothing gets output to the
	 * screen yet, so we don't need to initialise the primary state.
	 */

	if (p->engine.ps.psmarg) {
		assert(p->engine.ps.psmargsz);
		p->engine.ps.psmarg[0] = '\0';
	}

	p->engine.ps.psmargcur = 0;
	p->engine.ps.psstate = PS_MARGINS;
	p->engine.ps.pscol = p->engine.ps.left;
	p->engine.ps.psrow = p->engine.ps.header;

	ps_setfont(p, TERMFONT_NONE);

	(*p->headf)(p, p->argf);
	(*p->endline)(p);

	p->engine.ps.pscol = p->engine.ps.left;
	p->engine.ps.psrow = p->engine.ps.footer;

	(*p->footf)(p, p->argf);
	(*p->endline)(p);

	p->engine.ps.psstate &= ~PS_MARGINS;

	assert(0 == p->engine.ps.psstate);
	assert(p->engine.ps.psmarg);
	assert('\0' != p->engine.ps.psmarg[0]);

	/* 
	 * Print header and initialise page state.  Following this,
	 * stuff gets printed to the screen, so make sure we're sane.
	 */

	t = time(NULL);

	printf("%%!PS-Adobe-3.0\n");
	printf("%%%%Creator: mandoc-%s\n", VERSION);
	printf("%%%%CreationDate: %s", ctime(&t));
	printf("%%%%DocumentData: Clean7Bit\n");
	printf("%%%%Orientation: Portrait\n");
	printf("%%%%Pages: (atend)\n");
	printf("%%%%PageOrder: Ascend\n");
	printf("%%%%Orientation: Portrait\n");
	printf("%%%%DocumentMedia: Default %zu %zu 0 () ()\n",
			p->engine.ps.width,
			p->engine.ps.height);
	printf("%%%%DocumentNeededResources: font");
	for (i = 0; i < (int)TERMFONT__MAX; i++)
		printf(" %s", fonts[i].name);
	printf("\n%%%%EndComments\n");

	printf("%%%%Page: %zu %zu\n", 
			p->engine.ps.pages + 1, 
			p->engine.ps.pages + 1);

	ps_setfont(p, TERMFONT_NONE);
	p->engine.ps.pscol = p->engine.ps.left;
	p->engine.ps.psrow = p->engine.ps.top;
}


static void
ps_pletter(struct termp *p, int c)
{
	int		 f;
	
	/*
	 * If we're not in a PostScript "word" context, then open one
	 * now at the current cursor.
	 */

	if ( ! (PS_INLINE & p->engine.ps.psstate)) {
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

	f = (int)p->engine.ps.lastf;

	if (c <= 32 || (c - 32 > MAXCHAR)) {
		ps_putchar(p, ' ');
		p->engine.ps.pscol += (fonts[f].gly[0].wx / 100);
		return;
	} 

	ps_putchar(p, c);
	c -= 32;
	p->engine.ps.pscol += (fonts[f].gly[c].wx / 100);
}


static void
ps_pclose(struct termp *p)
{

	/* 
	 * Spit out that we're exiting a word context (this is a
	 * "partial close" because we don't check the last-char buffer
	 * or anything).
	 */

	if ( ! (PS_INLINE & p->engine.ps.psstate))
		return;
	
	ps_printf(p, ") show\n");
	p->engine.ps.psstate &= ~PS_INLINE;
}


static void
ps_fclose(struct termp *p)
{

	/*
	 * Strong closure: if we have a last-char, spit it out after
	 * checking that we're in the right font mode.  This will of
	 * course open a new scope, if applicable.
	 *
	 * Following this, close out any scope that's open.
	 */

	if ('\0' != p->engine.ps.last) {
		if (p->engine.ps.lastf != TERMFONT_NONE) {
			ps_pclose(p);
			ps_setfont(p, TERMFONT_NONE);
		}
		ps_pletter(p, p->engine.ps.last);
		p->engine.ps.last = '\0';
	}

	if ( ! (PS_INLINE & p->engine.ps.psstate))
		return;

	ps_pclose(p);
}


static void
ps_letter(struct termp *p, char c)
{
	char		cc;

	/*
	 * State machine dictates whether to buffer the last character
	 * or not.  Basically, encoded words are detected by checking if
	 * we're an "8" and switching on the buffer.  Then we put "8" in
	 * our buffer, and on the next charater, flush both character
	 * and buffer.  Thus, "regular" words are detected by having a
	 * regular character and a regular buffer character.
	 */

	if ('\0' == p->engine.ps.last) {
		assert(8 != c);
		p->engine.ps.last = c;
		return;
	} else if (8 == p->engine.ps.last) {
		assert(8 != c);
		p->engine.ps.last = '\0';
	} else if (8 == c) {
		assert(8 != p->engine.ps.last);
		if ('_' == p->engine.ps.last) {
			if (p->engine.ps.lastf != TERMFONT_UNDER) {
				ps_pclose(p);
				ps_setfont(p, TERMFONT_UNDER);
			}
		} else if (p->engine.ps.lastf != TERMFONT_BOLD) {
			ps_pclose(p);
			ps_setfont(p, TERMFONT_BOLD);
		}
		p->engine.ps.last = c;
		return;
	} else {
		if (p->engine.ps.lastf != TERMFONT_NONE) {
			ps_pclose(p);
			ps_setfont(p, TERMFONT_NONE);
		}
		cc = p->engine.ps.last;
		p->engine.ps.last = c;
		c = cc;
	}

	ps_pletter(p, c);
}


static void
ps_advance(struct termp *p, size_t len)
{

	/*
	 * Advance some spaces.  This can probably be made smarter,
	 * i.e., to have multiple space-separated words in the same
	 * scope, but this is easier:  just close out the current scope
	 * and readjust our column settings.
	 */

	ps_fclose(p);
	p->engine.ps.pscol += len;
}


static void
ps_endline(struct termp *p)
{

	/* Close out any scopes we have open: we're at eoln. */

	ps_fclose(p);

	/*
	 * If we're in the margin, don't try to recalculate our current
	 * row.  XXX: if the column tries to be fancy with multiple
	 * lines, we'll do nasty stuff. 
	 */

	if (PS_MARGINS & p->engine.ps.psstate)
		return;

	/*
	 * Put us down a line.  If we're at the page bottom, spit out a
	 * showpage and restart our row.
	 */

	p->engine.ps.pscol = p->engine.ps.left;
	if (p->engine.ps.psrow >= p->engine.ps.lineheight + 
			p->engine.ps.bottom) {
		p->engine.ps.psrow -= p->engine.ps.lineheight;
		return;
	}

	assert(p->engine.ps.psmarg && p->engine.ps.psmarg[0]);
	printf("%s", p->engine.ps.psmarg);
	printf("%%%%Page: %zu %zu\n", 
			p->engine.ps.pages + 1, 
			p->engine.ps.pages + 1);
	printf("showpage\n");
	p->engine.ps.pages++;
	p->engine.ps.psrow = p->engine.ps.top;
}


static void
ps_setfont(struct termp *p, enum termfont f)
{

	assert(f < TERMFONT__MAX);
	ps_printf(p, "/%s 10 selectfont\n", fonts[(int)f].name);
	p->engine.ps.lastf = f;
}


/* ARGSUSED */
static size_t
ps_width(const struct termp *p, char c)
{

	if (c <= 32 || c - 32 >= MAXCHAR)
		return(fonts[(int)TERMFONT_NONE].gly[0].wx / 100);

	c -= 32;
	return(fonts[(int)TERMFONT_NONE].gly[(int)c].wx / 100);
}
