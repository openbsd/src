/*	$OpenBSD: html.c,v 1.125 2019/04/30 15:52:42 schwarze Exp $ */
/*
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011-2015, 2017-2019 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc_aux.h"
#include "mandoc_ohash.h"
#include "mandoc.h"
#include "roff.h"
#include "out.h"
#include "html.h"
#include "manconf.h"
#include "main.h"

struct	htmldata {
	const char	 *name;
	int		  flags;
#define	HTML_NOSTACK	 (1 << 0)
#define	HTML_AUTOCLOSE	 (1 << 1)
#define	HTML_NLBEFORE	 (1 << 2)
#define	HTML_NLBEGIN	 (1 << 3)
#define	HTML_NLEND	 (1 << 4)
#define	HTML_NLAFTER	 (1 << 5)
#define	HTML_NLAROUND	 (HTML_NLBEFORE | HTML_NLAFTER)
#define	HTML_NLINSIDE	 (HTML_NLBEGIN | HTML_NLEND)
#define	HTML_NLALL	 (HTML_NLAROUND | HTML_NLINSIDE)
#define	HTML_INDENT	 (1 << 6)
#define	HTML_NOINDENT	 (1 << 7)
};

static	const struct htmldata htmltags[TAG_MAX] = {
	{"html",	HTML_NLALL},
	{"head",	HTML_NLALL | HTML_INDENT},
	{"body",	HTML_NLALL},
	{"meta",	HTML_NOSTACK | HTML_AUTOCLOSE | HTML_NLALL},
	{"title",	HTML_NLAROUND},
	{"div",		HTML_NLAROUND},
	{"div",		0},
	{"section",	HTML_NLALL},
	{"h1",		HTML_NLAROUND},
	{"h2",		HTML_NLAROUND},
	{"span",	0},
	{"link",	HTML_NOSTACK | HTML_AUTOCLOSE | HTML_NLALL},
	{"br",		HTML_NOSTACK | HTML_AUTOCLOSE | HTML_NLALL},
	{"a",		0},
	{"table",	HTML_NLALL | HTML_INDENT},
	{"tr",		HTML_NLALL | HTML_INDENT},
	{"td",		HTML_NLAROUND},
	{"li",		HTML_NLAROUND | HTML_INDENT},
	{"ul",		HTML_NLALL | HTML_INDENT},
	{"ol",		HTML_NLALL | HTML_INDENT},
	{"dl",		HTML_NLALL | HTML_INDENT},
	{"dt",		HTML_NLAROUND},
	{"dd",		HTML_NLAROUND | HTML_INDENT},
	{"p",		HTML_NLAROUND | HTML_INDENT},
	{"pre",		HTML_NLALL | HTML_NOINDENT},
	{"var",		0},
	{"cite",	0},
	{"b",		0},
	{"i",		0},
	{"code",	0},
	{"small",	0},
	{"style",	HTML_NLALL | HTML_INDENT},
	{"math",	HTML_NLALL | HTML_INDENT},
	{"mrow",	0},
	{"mi",		0},
	{"mn",		0},
	{"mo",		0},
	{"msup",	0},
	{"msub",	0},
	{"msubsup",	0},
	{"mfrac",	0},
	{"msqrt",	0},
	{"mfenced",	0},
	{"mtable",	0},
	{"mtr",		0},
	{"mtd",		0},
	{"munderover",	0},
	{"munder",	0},
	{"mover",	0},
};

/* Avoid duplicate HTML id= attributes. */
static	struct ohash	 id_unique;

static	void	 html_reset_internal(struct html *);
static	void	 print_byte(struct html *, char);
static	void	 print_endword(struct html *);
static	void	 print_indent(struct html *);
static	void	 print_word(struct html *, const char *);

static	void	 print_ctag(struct html *, struct tag *);
static	int	 print_escape(struct html *, char);
static	int	 print_encode(struct html *, const char *, const char *, int);
static	void	 print_href(struct html *, const char *, const char *, int);
static	void	 print_metaf(struct html *);


void *
html_alloc(const struct manoutput *outopts)
{
	struct html	*h;

	h = mandoc_calloc(1, sizeof(struct html));

	h->tag = NULL;
	h->style = outopts->style;
	if ((h->base_man1 = outopts->man) == NULL)
		h->base_man2 = NULL;
	else if ((h->base_man2 = strchr(h->base_man1, ';')) != NULL)
		*h->base_man2++ = '\0';
	h->base_includes = outopts->includes;
	if (outopts->fragment)
		h->oflags |= HTML_FRAGMENT;
	if (outopts->toc)
		h->oflags |= HTML_TOC;

	mandoc_ohash_init(&id_unique, 4, 0);

	return h;
}

static void
html_reset_internal(struct html *h)
{
	struct tag	*tag;
	char		*cp;
	unsigned int	 slot;

	while ((tag = h->tag) != NULL) {
		h->tag = tag->next;
		free(tag);
	}
	cp = ohash_first(&id_unique, &slot);
	while (cp != NULL) {
		free(cp);
		cp = ohash_next(&id_unique, &slot);
	}
	ohash_delete(&id_unique);
}

void
html_reset(void *p)
{
	html_reset_internal(p);
	mandoc_ohash_init(&id_unique, 4, 0);
}

void
html_free(void *p)
{
	html_reset_internal(p);
	free(p);
}

void
print_gen_head(struct html *h)
{
	struct tag	*t;

	print_otag(h, TAG_META, "?", "charset", "utf-8");
	if (h->style != NULL) {
		print_otag(h, TAG_LINK, "?h??", "rel", "stylesheet",
		    h->style, "type", "text/css", "media", "all");
		return;
	}

	/*
	 * Print a minimal embedded style sheet.
	 */

	t = print_otag(h, TAG_STYLE, "");
	print_text(h, "table.head, table.foot { width: 100%; }");
	print_endline(h);
	print_text(h, "td.head-rtitle, td.foot-os { text-align: right; }");
	print_endline(h);
	print_text(h, "td.head-vol { text-align: center; }");
	print_endline(h);
	print_text(h, "div.Pp { margin: 1ex 0ex; }");
	print_endline(h);
	print_text(h, "div.Nd, div.Bf, div.Op { display: inline; }");
	print_endline(h);
	print_text(h, "span.Pa, span.Ad { font-style: italic; }");
	print_endline(h);
	print_text(h, "span.Ms { font-weight: bold; }");
	print_endline(h);
	print_text(h, "dl.Bl-diag ");
	print_byte(h, '>');
	print_text(h, " dt { font-weight: bold; }");
	print_endline(h);
	print_text(h, "code.Nm, code.Fl, code.Cm, code.Ic, "
	    "code.In, code.Fd, code.Fn,");
	print_endline(h);
	print_text(h, "code.Cd { font-weight: bold; "
	    "font-family: inherit; }");
	print_tagq(h, t);
}

int
html_setfont(struct html *h, enum mandoc_esc font)
{
	switch (font) {
	case ESCAPE_FONTPREV:
		font = h->metal;
		break;
	case ESCAPE_FONTITALIC:
	case ESCAPE_FONTBOLD:
	case ESCAPE_FONTBI:
	case ESCAPE_FONTCW:
	case ESCAPE_FONTROMAN:
		break;
	case ESCAPE_FONT:
		font = ESCAPE_FONTROMAN;
		break;
	default:
		return 0;
	}
	h->metal = h->metac;
	h->metac = font;
	return 1;
}

static void
print_metaf(struct html *h)
{
	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}
	switch (h->metac) {
	case ESCAPE_FONTITALIC:
		h->metaf = print_otag(h, TAG_I, "");
		break;
	case ESCAPE_FONTBOLD:
		h->metaf = print_otag(h, TAG_B, "");
		break;
	case ESCAPE_FONTBI:
		h->metaf = print_otag(h, TAG_B, "");
		print_otag(h, TAG_I, "");
		break;
	case ESCAPE_FONTCW:
		h->metaf = print_otag(h, TAG_SPAN, "c", "Li");
		break;
	default:
		break;
	}
}

void
html_close_paragraph(struct html *h)
{
	struct tag	*t;

	for (t = h->tag; t != NULL && t->closed == 0; t = t->next) {
		switch(t->tag) {
		case TAG_P:
		case TAG_PRE:
			print_tagq(h, t);
			break;
		case TAG_A:
			print_tagq(h, t);
			continue;
		default:
			continue;
		}
		break;
	}
}

/*
 * ROFF_nf switches to no-fill mode, ROFF_fi to fill mode.
 * TOKEN_NONE does not switch.  The old mode is returned.
 */
enum roff_tok
html_fillmode(struct html *h, enum roff_tok want)
{
	struct tag	*t;
	enum roff_tok	 had;

	for (t = h->tag; t != NULL; t = t->next)
		if (t->tag == TAG_PRE)
			break;

	had = t == NULL ? ROFF_fi : ROFF_nf;

	if (want != had) {
		switch (want) {
		case ROFF_fi:
			print_tagq(h, t);
			break;
		case ROFF_nf:
			html_close_paragraph(h);
			print_otag(h, TAG_PRE, "");
			break;
		case TOKEN_NONE:
			break;
		default:
			abort();
		}
	}
	return had;
}

char *
html_make_id(const struct roff_node *n, int unique)
{
	const struct roff_node	*nch;
	char			*buf, *bufs, *cp;
	unsigned int		 slot;
	int			 suffix;

	for (nch = n->child; nch != NULL; nch = nch->next)
		if (nch->type != ROFFT_TEXT)
			return NULL;

	buf = NULL;
	deroff(&buf, n);
	if (buf == NULL)
		return NULL;

	/*
	 * In ID attributes, only use ASCII characters that are
	 * permitted in URL-fragment strings according to the
	 * explicit list at:
	 * https://url.spec.whatwg.org/#url-fragment-string
	 */

	for (cp = buf; *cp != '\0'; cp++)
		if (isalnum((unsigned char)*cp) == 0 &&
		    strchr("!$&'()*+,-./:;=?@_~", *cp) == NULL)
			*cp = '_';

	if (unique == 0)
		return buf;

	/* Avoid duplicate HTML id= attributes. */

	bufs = NULL;
	suffix = 1;
	slot = ohash_qlookup(&id_unique, buf);
	cp = ohash_find(&id_unique, slot);
	if (cp != NULL) {
		while (cp != NULL) {
			free(bufs);
			if (++suffix > 127) {
				free(buf);
				return NULL;
			}
			mandoc_asprintf(&bufs, "%s_%d", buf, suffix);
			slot = ohash_qlookup(&id_unique, bufs);
			cp = ohash_find(&id_unique, slot);
		}
		free(buf);
		buf = bufs;
	}
	ohash_insert(&id_unique, slot, buf);
	return buf;
}

static int
print_escape(struct html *h, char c)
{

	switch (c) {
	case '<':
		print_word(h, "&lt;");
		break;
	case '>':
		print_word(h, "&gt;");
		break;
	case '&':
		print_word(h, "&amp;");
		break;
	case '"':
		print_word(h, "&quot;");
		break;
	case ASCII_NBRSP:
		print_word(h, "&nbsp;");
		break;
	case ASCII_HYPH:
		print_byte(h, '-');
		break;
	case ASCII_BREAK:
		break;
	default:
		return 0;
	}
	return 1;
}

static int
print_encode(struct html *h, const char *p, const char *pend, int norecurse)
{
	char		 numbuf[16];
	const char	*seq;
	size_t		 sz;
	int		 c, len, breakline, nospace;
	enum mandoc_esc	 esc;
	static const char rejs[10] = { ' ', '\\', '<', '>', '&', '"',
		ASCII_NBRSP, ASCII_HYPH, ASCII_BREAK, '\0' };

	if (pend == NULL)
		pend = strchr(p, '\0');

	breakline = 0;
	nospace = 0;

	while (p < pend) {
		if (HTML_SKIPCHAR & h->flags && '\\' != *p) {
			h->flags &= ~HTML_SKIPCHAR;
			p++;
			continue;
		}

		for (sz = strcspn(p, rejs); sz-- && p < pend; p++)
			print_byte(h, *p);

		if (breakline &&
		    (p >= pend || *p == ' ' || *p == ASCII_NBRSP)) {
			print_otag(h, TAG_BR, "");
			breakline = 0;
			while (p < pend && (*p == ' ' || *p == ASCII_NBRSP))
				p++;
			continue;
		}

		if (p >= pend)
			break;

		if (*p == ' ') {
			print_endword(h);
			p++;
			continue;
		}

		if (print_escape(h, *p++))
			continue;

		esc = mandoc_escape(&p, &seq, &len);
		switch (esc) {
		case ESCAPE_FONT:
		case ESCAPE_FONTPREV:
		case ESCAPE_FONTBOLD:
		case ESCAPE_FONTITALIC:
		case ESCAPE_FONTBI:
		case ESCAPE_FONTCW:
		case ESCAPE_FONTROMAN:
			if (0 == norecurse) {
				h->flags |= HTML_NOSPACE;
				if (html_setfont(h, esc))
					print_metaf(h);
				h->flags &= ~HTML_NOSPACE;
			}
			continue;
		case ESCAPE_SKIPCHAR:
			h->flags |= HTML_SKIPCHAR;
			continue;
		case ESCAPE_ERROR:
			continue;
		default:
			break;
		}

		if (h->flags & HTML_SKIPCHAR) {
			h->flags &= ~HTML_SKIPCHAR;
			continue;
		}

		switch (esc) {
		case ESCAPE_UNICODE:
			/* Skip past "u" header. */
			c = mchars_num2uc(seq + 1, len - 1);
			break;
		case ESCAPE_NUMBERED:
			c = mchars_num2char(seq, len);
			if (c < 0)
				continue;
			break;
		case ESCAPE_SPECIAL:
			c = mchars_spec2cp(seq, len);
			if (c <= 0)
				continue;
			break;
		case ESCAPE_UNDEF:
			c = *seq;
			break;
		case ESCAPE_DEVICE:
			print_word(h, "html");
			continue;
		case ESCAPE_BREAK:
			breakline = 1;
			continue;
		case ESCAPE_NOSPACE:
			if ('\0' == *p)
				nospace = 1;
			continue;
		case ESCAPE_OVERSTRIKE:
			if (len == 0)
				continue;
			c = seq[len - 1];
			break;
		default:
			continue;
		}
		if ((c < 0x20 && c != 0x09) ||
		    (c > 0x7E && c < 0xA0))
			c = 0xFFFD;
		if (c > 0x7E) {
			(void)snprintf(numbuf, sizeof(numbuf), "&#x%.4X;", c);
			print_word(h, numbuf);
		} else if (print_escape(h, c) == 0)
			print_byte(h, c);
	}

	return nospace;
}

static void
print_href(struct html *h, const char *name, const char *sec, int man)
{
	struct stat	 sb;
	const char	*p, *pp;
	char		*filename;

	if (man) {
		pp = h->base_man1;
		if (h->base_man2 != NULL) {
			mandoc_asprintf(&filename, "%s.%s", name, sec);
			if (stat(filename, &sb) == -1)
				pp = h->base_man2;
			free(filename);
		}
	} else
		pp = h->base_includes;

	while ((p = strchr(pp, '%')) != NULL) {
		print_encode(h, pp, p, 1);
		if (man && p[1] == 'S') {
			if (sec == NULL)
				print_byte(h, '1');
			else
				print_encode(h, sec, NULL, 1);
		} else if ((man && p[1] == 'N') ||
		    (man == 0 && p[1] == 'I'))
			print_encode(h, name, NULL, 1);
		else
			print_encode(h, p, p + 2, 1);
		pp = p + 2;
	}
	if (*pp != '\0')
		print_encode(h, pp, NULL, 1);
}

struct tag *
print_otag(struct html *h, enum htmltag tag, const char *fmt, ...)
{
	va_list		 ap;
	struct tag	*t;
	const char	*attr;
	char		*arg1, *arg2;
	int		 style_written, tflags;

	tflags = htmltags[tag].flags;

	/* Push this tag onto the stack of open scopes. */

	if ((tflags & HTML_NOSTACK) == 0) {
		t = mandoc_malloc(sizeof(struct tag));
		t->tag = tag;
		t->next = h->tag;
		t->refcnt = 0;
		t->closed = 0;
		h->tag = t;
	} else
		t = NULL;

	if (tflags & HTML_NLBEFORE)
		print_endline(h);
	if (h->col == 0)
		print_indent(h);
	else if ((h->flags & HTML_NOSPACE) == 0) {
		if (h->flags & HTML_KEEP)
			print_word(h, "&#x00A0;");
		else {
			if (h->flags & HTML_PREKEEP)
				h->flags |= HTML_KEEP;
			print_endword(h);
		}
	}

	if ( ! (h->flags & HTML_NONOSPACE))
		h->flags &= ~HTML_NOSPACE;
	else
		h->flags |= HTML_NOSPACE;

	/* Print out the tag name and attributes. */

	print_byte(h, '<');
	print_word(h, htmltags[tag].name);

	va_start(ap, fmt);

	while (*fmt != '\0' && *fmt != 's') {

		/* Parse attributes and arguments. */

		arg1 = va_arg(ap, char *);
		arg2 = NULL;
		switch (*fmt++) {
		case 'c':
			attr = "class";
			break;
		case 'h':
			attr = "href";
			break;
		case 'i':
			attr = "id";
			break;
		case '?':
			attr = arg1;
			arg1 = va_arg(ap, char *);
			break;
		default:
			abort();
		}
		if (*fmt == 'M')
			arg2 = va_arg(ap, char *);
		if (arg1 == NULL)
			continue;

		/* Print the attributes. */

		print_byte(h, ' ');
		print_word(h, attr);
		print_byte(h, '=');
		print_byte(h, '"');
		switch (*fmt) {
		case 'I':
			print_href(h, arg1, NULL, 0);
			fmt++;
			break;
		case 'M':
			print_href(h, arg1, arg2, 1);
			fmt++;
			break;
		case 'R':
			print_byte(h, '#');
			print_encode(h, arg1, NULL, 1);
			fmt++;
			break;
		default:
			print_encode(h, arg1, NULL, 1);
			break;
		}
		print_byte(h, '"');
	}

	style_written = 0;
	while (*fmt++ == 's') {
		arg1 = va_arg(ap, char *);
		arg2 = va_arg(ap, char *);
		if (arg2 == NULL)
			continue;
		print_byte(h, ' ');
		if (style_written == 0) {
			print_word(h, "style=\"");
			style_written = 1;
		}
		print_word(h, arg1);
		print_byte(h, ':');
		print_byte(h, ' ');
		print_word(h, arg2);
		print_byte(h, ';');
	}
	if (style_written)
		print_byte(h, '"');

	va_end(ap);

	/* Accommodate for "well-formed" singleton escaping. */

	if (HTML_AUTOCLOSE & htmltags[tag].flags)
		print_byte(h, '/');

	print_byte(h, '>');

	if (tflags & HTML_NLBEGIN)
		print_endline(h);
	else
		h->flags |= HTML_NOSPACE;

	if (tflags & HTML_INDENT)
		h->indent++;
	if (tflags & HTML_NOINDENT)
		h->noindent++;

	return t;
}

static void
print_ctag(struct html *h, struct tag *tag)
{
	int	 tflags;

	if (tag->closed == 0) {
		tag->closed = 1;
		if (tag == h->metaf)
			h->metaf = NULL;
		if (tag == h->tblt)
			h->tblt = NULL;

		tflags = htmltags[tag->tag].flags;
		if (tflags & HTML_INDENT)
			h->indent--;
		if (tflags & HTML_NOINDENT)
			h->noindent--;
		if (tflags & HTML_NLEND)
			print_endline(h);
		print_indent(h);
		print_byte(h, '<');
		print_byte(h, '/');
		print_word(h, htmltags[tag->tag].name);
		print_byte(h, '>');
		if (tflags & HTML_NLAFTER)
			print_endline(h);
	}
	if (tag->refcnt == 0) {
		h->tag = tag->next;
		free(tag);
	}
}

void
print_gen_decls(struct html *h)
{
	print_word(h, "<!DOCTYPE html>");
	print_endline(h);
}

void
print_gen_comment(struct html *h, struct roff_node *n)
{
	int	 wantblank;

	print_word(h, "<!-- This is an automatically generated file."
	    "  Do not edit.");
	h->indent = 1;
	wantblank = 0;
	while (n != NULL && n->type == ROFFT_COMMENT) {
		if (strstr(n->string, "-->") == NULL &&
		    (wantblank || *n->string != '\0')) {
			print_endline(h);
			print_indent(h);
			print_word(h, n->string);
			wantblank = *n->string != '\0';
		}
		n = n->next;
	}
	if (wantblank)
		print_endline(h);
	print_word(h, " -->");
	print_endline(h);
	h->indent = 0;
}

void
print_text(struct html *h, const char *word)
{
	if (h->col && (h->flags & HTML_NOSPACE) == 0) {
		if ( ! (HTML_KEEP & h->flags)) {
			if (HTML_PREKEEP & h->flags)
				h->flags |= HTML_KEEP;
			print_endword(h);
		} else
			print_word(h, "&#x00A0;");
	}

	assert(h->metaf == NULL);
	print_metaf(h);
	print_indent(h);
	if ( ! print_encode(h, word, NULL, 0)) {
		if ( ! (h->flags & HTML_NONOSPACE))
			h->flags &= ~HTML_NOSPACE;
		h->flags &= ~HTML_NONEWLINE;
	} else
		h->flags |= HTML_NOSPACE | HTML_NONEWLINE;

	if (h->metaf != NULL) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}

	h->flags &= ~HTML_IGNDELIM;
}

void
print_tagq(struct html *h, const struct tag *until)
{
	struct tag	*this, *next;

	for (this = h->tag; this != NULL; this = next) {
		next = this == until ? NULL : this->next;
		print_ctag(h, this);
	}
}

/*
 * Close out all open elements up to but excluding suntil.
 * Note that a paragraph just inside stays open together with it
 * because paragraphs include subsequent phrasing content.
 */
void
print_stagq(struct html *h, const struct tag *suntil)
{
	struct tag	*this, *next;

	for (this = h->tag; this != NULL; this = next) {
		next = this->next;
		if (this == suntil || (next == suntil &&
		    (this->tag == TAG_P || this->tag == TAG_PRE)))
			break;
		print_ctag(h, this);
	}
}


/***********************************************************************
 * Low level output functions.
 * They implement line breaking using a short static buffer.
 ***********************************************************************/

/*
 * Buffer one HTML output byte.
 * If the buffer is full, flush and deactivate it and start a new line.
 * If the buffer is inactive, print directly.
 */
static void
print_byte(struct html *h, char c)
{
	if ((h->flags & HTML_BUFFER) == 0) {
		putchar(c);
		h->col++;
		return;
	}

	if (h->col + h->bufcol < sizeof(h->buf)) {
		h->buf[h->bufcol++] = c;
		return;
	}

	putchar('\n');
	h->col = 0;
	print_indent(h);
	putchar(' ');
	putchar(' ');
	fwrite(h->buf, h->bufcol, 1, stdout);
	putchar(c);
	h->col = (h->indent + 1) * 2 + h->bufcol + 1;
	h->bufcol = 0;
	h->flags &= ~HTML_BUFFER;
}

/*
 * If something was printed on the current output line, end it.
 * Not to be called right after print_indent().
 */
void
print_endline(struct html *h)
{
	if (h->col == 0)
		return;

	if (h->bufcol) {
		putchar(' ');
		fwrite(h->buf, h->bufcol, 1, stdout);
		h->bufcol = 0;
	}
	putchar('\n');
	h->col = 0;
	h->flags |= HTML_NOSPACE;
	h->flags &= ~HTML_BUFFER;
}

/*
 * Flush the HTML output buffer.
 * If it is inactive, activate it.
 */
static void
print_endword(struct html *h)
{
	if (h->noindent) {
		print_byte(h, ' ');
		return;
	}

	if ((h->flags & HTML_BUFFER) == 0) {
		h->col++;
		h->flags |= HTML_BUFFER;
	} else if (h->bufcol) {
		putchar(' ');
		fwrite(h->buf, h->bufcol, 1, stdout);
		h->col += h->bufcol + 1;
	}
	h->bufcol = 0;
}

/*
 * If at the beginning of a new output line,
 * perform indentation and mark the line as containing output.
 * Make sure to really produce some output right afterwards,
 * but do not use print_otag() for producing it.
 */
static void
print_indent(struct html *h)
{
	size_t	 i;

	if (h->col)
		return;

	if (h->noindent == 0) {
		h->col = h->indent * 2;
		for (i = 0; i < h->col; i++)
			putchar(' ');
	}
	h->flags &= ~HTML_NOSPACE;
}

/*
 * Print or buffer some characters
 * depending on the current HTML output buffer state.
 */
static void
print_word(struct html *h, const char *cp)
{
	while (*cp != '\0')
		print_byte(h, *cp++);
}
