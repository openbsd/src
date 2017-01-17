/*	$OpenBSD: html.c,v 1.64 2017/01/17 01:47:46 schwarze Exp $ */
/*
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011-2015, 2017 Ingo Schwarze <schwarze@openbsd.org>
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

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "out.h"
#include "html.h"
#include "manconf.h"
#include "main.h"

struct	htmldata {
	const char	 *name;
	int		  flags;
#define	HTML_CLRLINE	 (1 << 0)
#define	HTML_NOSTACK	 (1 << 1)
#define	HTML_AUTOCLOSE	 (1 << 2) /* Tag has auto-closure. */
};

static	const struct htmldata htmltags[TAG_MAX] = {
	{"html",	HTML_CLRLINE}, /* TAG_HTML */
	{"head",	HTML_CLRLINE}, /* TAG_HEAD */
	{"body",	HTML_CLRLINE}, /* TAG_BODY */
	{"meta",	HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_META */
	{"title",	HTML_CLRLINE}, /* TAG_TITLE */
	{"div",		HTML_CLRLINE}, /* TAG_DIV */
	{"h1",		0}, /* TAG_H1 */
	{"h2",		0}, /* TAG_H2 */
	{"span",	0}, /* TAG_SPAN */
	{"link",	HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_LINK */
	{"br",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_BR */
	{"a",		0}, /* TAG_A */
	{"table",	HTML_CLRLINE}, /* TAG_TABLE */
	{"tbody",	HTML_CLRLINE}, /* TAG_TBODY */
	{"col",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_COL */
	{"tr",		HTML_CLRLINE}, /* TAG_TR */
	{"td",		HTML_CLRLINE}, /* TAG_TD */
	{"li",		HTML_CLRLINE}, /* TAG_LI */
	{"ul",		HTML_CLRLINE}, /* TAG_UL */
	{"ol",		HTML_CLRLINE}, /* TAG_OL */
	{"dl",		HTML_CLRLINE}, /* TAG_DL */
	{"dt",		HTML_CLRLINE}, /* TAG_DT */
	{"dd",		HTML_CLRLINE}, /* TAG_DD */
	{"blockquote",	HTML_CLRLINE}, /* TAG_BLOCKQUOTE */
	{"pre",		HTML_CLRLINE }, /* TAG_PRE */
	{"b",		0 }, /* TAG_B */
	{"i",		0 }, /* TAG_I */
	{"code",	0 }, /* TAG_CODE */
	{"small",	0 }, /* TAG_SMALL */
	{"style",	HTML_CLRLINE}, /* TAG_STYLE */
	{"math",	HTML_CLRLINE}, /* TAG_MATH */
	{"mrow",	0}, /* TAG_MROW */
	{"mi",		0}, /* TAG_MI */
	{"mo",		0}, /* TAG_MO */
	{"msup",	0}, /* TAG_MSUP */
	{"msub",	0}, /* TAG_MSUB */
	{"msubsup",	0}, /* TAG_MSUBSUP */
	{"mfrac",	0}, /* TAG_MFRAC */
	{"msqrt",	0}, /* TAG_MSQRT */
	{"mfenced",	0}, /* TAG_MFENCED */
	{"mtable",	0}, /* TAG_MTABLE */
	{"mtr",		0}, /* TAG_MTR */
	{"mtd",		0}, /* TAG_MTD */
	{"munderover",	0}, /* TAG_MUNDEROVER */
	{"munder",	0}, /* TAG_MUNDER*/
	{"mover",	0}, /* TAG_MOVER*/
};

static	const char	*const roffscales[SCALE_MAX] = {
	"cm", /* SCALE_CM */
	"in", /* SCALE_IN */
	"pc", /* SCALE_PC */
	"pt", /* SCALE_PT */
	"em", /* SCALE_EM */
	"em", /* SCALE_MM */
	"ex", /* SCALE_EN */
	"ex", /* SCALE_BU */
	"em", /* SCALE_VS */
	"ex", /* SCALE_FS */
};

static	void	 a2width(const char *, struct roffsu *);
static	void	 bufncat(struct html *, const char *, size_t);
static	void	 print_ctag(struct html *, struct tag *);
static	int	 print_escape(char);
static	int	 print_encode(struct html *, const char *, int);
static	void	 print_metaf(struct html *, enum mandoc_esc);
static	void	 print_attr(struct html *, const char *, const char *);


void *
html_alloc(const struct manoutput *outopts)
{
	struct html	*h;

	h = mandoc_calloc(1, sizeof(struct html));

	h->tags.head = NULL;
	h->style = outopts->style;
	h->base_man = outopts->man;
	h->base_includes = outopts->includes;
	if (outopts->fragment)
		h->oflags |= HTML_FRAGMENT;

	return h;
}

void
html_free(void *p)
{
	struct tag	*tag;
	struct html	*h;

	h = (struct html *)p;

	while ((tag = h->tags.head) != NULL) {
		h->tags.head = tag->next;
		free(tag);
	}

	free(h);
}

void
print_gen_head(struct html *h)
{
	struct tag	*t;

	print_otag(h, TAG_META, "?", "charset", "utf-8");

	/*
	 * Print a default style-sheet.
	 */
	t = print_otag(h, TAG_STYLE, "");
	print_text(h, "table.head, table.foot { width: 100%; }\n"
	      "td.head-rtitle, td.foot-os { text-align: right; }\n"
	      "td.head-vol { text-align: center; }\n"
	      "table.foot td { width: 50%; }\n"
	      "table.head td { width: 33%; }\n"
	      "div.spacer { margin: 1em 0; }\n");
	print_tagq(h, t);

	if (h->style)
		print_otag(h, TAG_LINK, "?h??", "rel", "stylesheet",
		    h->style, "type", "text/css", "media", "all");
}

static void
print_metaf(struct html *h, enum mandoc_esc deco)
{
	enum htmlfont	 font;

	switch (deco) {
	case ESCAPE_FONTPREV:
		font = h->metal;
		break;
	case ESCAPE_FONTITALIC:
		font = HTMLFONT_ITALIC;
		break;
	case ESCAPE_FONTBOLD:
		font = HTMLFONT_BOLD;
		break;
	case ESCAPE_FONTBI:
		font = HTMLFONT_BI;
		break;
	case ESCAPE_FONT:
	case ESCAPE_FONTROMAN:
		font = HTMLFONT_NONE;
		break;
	default:
		abort();
	}

	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}

	h->metal = h->metac;
	h->metac = font;

	switch (font) {
	case HTMLFONT_ITALIC:
		h->metaf = print_otag(h, TAG_I, "");
		break;
	case HTMLFONT_BOLD:
		h->metaf = print_otag(h, TAG_B, "");
		break;
	case HTMLFONT_BI:
		h->metaf = print_otag(h, TAG_B, "");
		print_otag(h, TAG_I, "");
		break;
	default:
		break;
	}
}

int
html_strlen(const char *cp)
{
	size_t		 rsz;
	int		 skip, sz;

	/*
	 * Account for escaped sequences within string length
	 * calculations.  This follows the logic in term_strlen() as we
	 * must calculate the width of produced strings.
	 * Assume that characters are always width of "1".  This is
	 * hacky, but it gets the job done for approximation of widths.
	 */

	sz = 0;
	skip = 0;
	while (1) {
		rsz = strcspn(cp, "\\");
		if (rsz) {
			cp += rsz;
			if (skip) {
				skip = 0;
				rsz--;
			}
			sz += rsz;
		}
		if ('\0' == *cp)
			break;
		cp++;
		switch (mandoc_escape(&cp, NULL, NULL)) {
		case ESCAPE_ERROR:
			return sz;
		case ESCAPE_UNICODE:
		case ESCAPE_NUMBERED:
		case ESCAPE_SPECIAL:
		case ESCAPE_OVERSTRIKE:
			if (skip)
				skip = 0;
			else
				sz++;
			break;
		case ESCAPE_SKIPCHAR:
			skip = 1;
			break;
		default:
			break;
		}
	}
	return sz;
}

static int
print_escape(char c)
{

	switch (c) {
	case '<':
		printf("&lt;");
		break;
	case '>':
		printf("&gt;");
		break;
	case '&':
		printf("&amp;");
		break;
	case '"':
		printf("&quot;");
		break;
	case ASCII_NBRSP:
		printf("&nbsp;");
		break;
	case ASCII_HYPH:
		putchar('-');
		break;
	case ASCII_BREAK:
		break;
	default:
		return 0;
	}
	return 1;
}

static int
print_encode(struct html *h, const char *p, int norecurse)
{
	size_t		 sz;
	int		 c, len, nospace;
	const char	*seq;
	enum mandoc_esc	 esc;
	static const char rejs[9] = { '\\', '<', '>', '&', '"',
		ASCII_NBRSP, ASCII_HYPH, ASCII_BREAK, '\0' };

	nospace = 0;

	while ('\0' != *p) {
		if (HTML_SKIPCHAR & h->flags && '\\' != *p) {
			h->flags &= ~HTML_SKIPCHAR;
			p++;
			continue;
		}

		sz = strcspn(p, rejs);

		fwrite(p, 1, sz, stdout);
		p += (int)sz;

		if ('\0' == *p)
			break;

		if (print_escape(*p++))
			continue;

		esc = mandoc_escape(&p, &seq, &len);
		if (ESCAPE_ERROR == esc)
			break;

		switch (esc) {
		case ESCAPE_FONT:
		case ESCAPE_FONTPREV:
		case ESCAPE_FONTBOLD:
		case ESCAPE_FONTITALIC:
		case ESCAPE_FONTBI:
		case ESCAPE_FONTROMAN:
			if (0 == norecurse)
				print_metaf(h, esc);
			continue;
		case ESCAPE_SKIPCHAR:
			h->flags |= HTML_SKIPCHAR;
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
		if (c > 0x7E)
			printf("&#%d;", c);
		else if ( ! print_escape(c))
			putchar(c);
	}

	return nospace;
}

static void
print_attr(struct html *h, const char *key, const char *val)
{
	printf(" %s=\"", key);
	(void)print_encode(h, val, 1);
	putchar('\"');
}

struct tag *
print_otag(struct html *h, enum htmltag tag, const char *fmt, ...)
{
	va_list		 ap;
	struct roffsu	 mysu, *su;
	struct tag	*t;
	char		*s;
	int		 i, have_style;

	/* Push this tags onto the stack of open scopes. */

	if ( ! (HTML_NOSTACK & htmltags[tag].flags)) {
		t = mandoc_malloc(sizeof(struct tag));
		t->tag = tag;
		t->next = h->tags.head;
		h->tags.head = t;
	} else
		t = NULL;

	if ( ! (HTML_NOSPACE & h->flags))
		if ( ! (HTML_CLRLINE & htmltags[tag].flags)) {
			/* Manage keeps! */
			if ( ! (HTML_KEEP & h->flags)) {
				if (HTML_PREKEEP & h->flags)
					h->flags |= HTML_KEEP;
				putchar(' ');
			} else
				printf("&#160;");
		}

	if ( ! (h->flags & HTML_NONOSPACE))
		h->flags &= ~HTML_NOSPACE;
	else
		h->flags |= HTML_NOSPACE;

	/* Print out the tag name and attributes. */

	printf("<%s", htmltags[tag].name);

	va_start(ap, fmt);

	have_style = 0;
	while (*fmt != '\0') {
		if (*fmt == 's') {
			printf(" style=\"");
			have_style = 1;
			fmt++;
			break;
		}
		s = va_arg(ap, char *);
		switch (*fmt++) {
		case 'c':
			print_attr(h, "class", s);
			break;
		case 'h':
			print_attr(h, "href", s);
			break;
		case 'i':
			print_attr(h, "id", s);
			break;
		case '?':
			print_attr(h, s, va_arg(ap, char *));
			break;
		default:
			abort();
		}
	}

	/* Print out styles. */

	s = NULL;
	su = &mysu;
	while (*fmt != '\0') {

		/* First letter: input argument type. */

		switch (*fmt++) {
		case 'h':
			i = va_arg(ap, int);
			SCALE_HS_INIT(su, i);
			break;
		case 's':
			s = va_arg(ap, char *);
			break;
		case 'u':
			su = va_arg(ap, struct roffsu *);
			break;
		case 'v':
			i = va_arg(ap, int);
			SCALE_VS_INIT(su, i);
			break;
		case 'w':
			s = va_arg(ap, char *);
			a2width(s, su);
			break;
		default:
			abort();
		}

		/* Second letter: style name. */

		bufinit(h);
		switch (*fmt++) {
		case 'b':
			bufcat_su(h, "margin-bottom", su);
			break;
		case 'h':
			bufcat_su(h, "height", su);
			break;
		case 'i':
			bufcat_su(h, "text-indent", su);
			break;
		case 'l':
			bufcat_su(h, "margin-left", su);
			break;
		case 't':
			bufcat_su(h, "margin-top", su);
			break;
		case 'w':
			bufcat_su(h, "width", su);
			break;
		case 'W':
			bufcat_su(h, "min-width", su);
			break;
		case '?':
			bufcat_style(h, s, va_arg(ap, char *));
			break;
		default:
			abort();
		}
		printf("%s", h->buf);
	}
	if (have_style)
		putchar('"');

	va_end(ap);

	/* Accommodate for "well-formed" singleton escaping. */

	if (HTML_AUTOCLOSE & htmltags[tag].flags)
		putchar('/');

	putchar('>');

	h->flags |= HTML_NOSPACE;

	if ((HTML_AUTOCLOSE | HTML_CLRLINE) & htmltags[tag].flags)
		putchar('\n');

	return t;
}

static void
print_ctag(struct html *h, struct tag *tag)
{

	/*
	 * Remember to close out and nullify the current
	 * meta-font and table, if applicable.
	 */
	if (tag == h->metaf)
		h->metaf = NULL;
	if (tag == h->tblt)
		h->tblt = NULL;

	printf("</%s>", htmltags[tag->tag].name);
	if (HTML_CLRLINE & htmltags[tag->tag].flags) {
		h->flags |= HTML_NOSPACE;
		putchar('\n');
	}

	h->tags.head = tag->next;
	free(tag);
}

void
print_gen_decls(struct html *h)
{

	puts("<!DOCTYPE html>");
}

void
print_text(struct html *h, const char *word)
{

	if ( ! (HTML_NOSPACE & h->flags)) {
		/* Manage keeps! */
		if ( ! (HTML_KEEP & h->flags)) {
			if (HTML_PREKEEP & h->flags)
				h->flags |= HTML_KEEP;
			putchar(' ');
		} else
			printf("&#160;");
	}

	assert(NULL == h->metaf);
	switch (h->metac) {
	case HTMLFONT_ITALIC:
		h->metaf = print_otag(h, TAG_I, "");
		break;
	case HTMLFONT_BOLD:
		h->metaf = print_otag(h, TAG_B, "");
		break;
	case HTMLFONT_BI:
		h->metaf = print_otag(h, TAG_B, "");
		print_otag(h, TAG_I, "");
		break;
	default:
		break;
	}

	assert(word);
	if ( ! print_encode(h, word, 0)) {
		if ( ! (h->flags & HTML_NONOSPACE))
			h->flags &= ~HTML_NOSPACE;
		h->flags &= ~HTML_NONEWLINE;
	} else
		h->flags |= HTML_NOSPACE | HTML_NONEWLINE;

	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}

	h->flags &= ~HTML_IGNDELIM;
}

void
print_tagq(struct html *h, const struct tag *until)
{
	struct tag	*tag;

	while ((tag = h->tags.head) != NULL) {
		print_ctag(h, tag);
		if (until && tag == until)
			return;
	}
}

void
print_stagq(struct html *h, const struct tag *suntil)
{
	struct tag	*tag;

	while ((tag = h->tags.head) != NULL) {
		if (suntil && tag == suntil)
			return;
		print_ctag(h, tag);
	}
}

void
print_paragraph(struct html *h)
{
	struct tag	*t;

	t = print_otag(h, TAG_DIV, "c", "spacer");
	print_tagq(h, t);
}


/*
 * Calculate the scaling unit passed in a `-width' argument.  This uses
 * either a native scaling unit (e.g., 1i, 2m) or the string length of
 * the value.
 */
static void
a2width(const char *p, struct roffsu *su)
{
	if (a2roffsu(p, su, SCALE_MAX) < 2) {
		su->unit = SCALE_EN;
		su->scale = html_strlen(p);
	} else if (su->scale < 0.0)
		su->scale = 0.0;
}

void
bufinit(struct html *h)
{

	h->buf[0] = '\0';
	h->buflen = 0;
}

void
bufcat_style(struct html *h, const char *key, const char *val)
{

	bufcat(h, key);
	bufcat(h, ":");
	bufcat(h, val);
	bufcat(h, ";");
}

void
bufcat(struct html *h, const char *p)
{

	/*
	 * XXX This is broken and not easy to fix.
	 * When using the -Oincludes option, buffmt_includes()
	 * may pass in strings overrunning BUFSIZ, causing a crash.
	 */

	h->buflen = strlcat(h->buf, p, BUFSIZ);
	assert(h->buflen < BUFSIZ);
}

void
bufcat_fmt(struct html *h, const char *fmt, ...)
{
	va_list		 ap;

	va_start(ap, fmt);
	(void)vsnprintf(h->buf + (int)h->buflen,
	    BUFSIZ - h->buflen - 1, fmt, ap);
	va_end(ap);
	h->buflen = strlen(h->buf);
}

static void
bufncat(struct html *h, const char *p, size_t sz)
{

	assert(h->buflen + sz + 1 < BUFSIZ);
	strncat(h->buf, p, sz);
	h->buflen += sz;
}

void
buffmt_includes(struct html *h, const char *name)
{
	const char	*p, *pp;

	pp = h->base_includes;

	bufinit(h);
	while (NULL != (p = strchr(pp, '%'))) {
		bufncat(h, pp, (size_t)(p - pp));
		switch (*(p + 1)) {
		case 'I':
			bufcat(h, name);
			break;
		default:
			bufncat(h, p, 2);
			break;
		}
		pp = p + 2;
	}
	if (pp)
		bufcat(h, pp);
}

void
buffmt_man(struct html *h, const char *name, const char *sec)
{
	const char	*p, *pp;

	pp = h->base_man;

	bufinit(h);
	while (NULL != (p = strchr(pp, '%'))) {
		bufncat(h, pp, (size_t)(p - pp));
		switch (*(p + 1)) {
		case 'S':
			bufcat(h, sec ? sec : "1");
			break;
		case 'N':
			bufcat_fmt(h, "%s", name);
			break;
		default:
			bufncat(h, p, 2);
			break;
		}
		pp = p + 2;
	}
	if (pp)
		bufcat(h, pp);
}

void
bufcat_su(struct html *h, const char *p, const struct roffsu *su)
{
	double		 v;

	v = su->scale;
	if (SCALE_MM == su->unit && 0.0 == (v /= 100.0))
		v = 1.0;
	else if (SCALE_BU == su->unit)
		v /= 24.0;

	bufcat_fmt(h, "%s: %.2f%s;", p, v, roffscales[su->unit]);
}

void
bufcat_id(struct html *h, const char *src)
{

	/* Cf. <http://www.w3.org/TR/html5/dom.html#the-id-attribute>. */

	for (; '\0' != *src; src++)
		bufncat(h, *src == ' ' ? "_" : src, 1);
}
