/*	$Id: html.c,v 1.7 2010/04/07 23:15:05 schwarze Exp $ */
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
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "out.h"
#include "chars.h"
#include "html.h"
#include "main.h"

#define	UNCONST(a)	((void *)(uintptr_t)(const void *)(a))

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
	{"link",	HTML_CLRLINE | HTML_NOSTACK}, /* TAG_LINK */
	{"br",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_BR */
	{"a",		0}, /* TAG_A */
	{"table",	HTML_CLRLINE}, /* TAG_TABLE */
	{"col",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_COL */
	{"tr",		HTML_CLRLINE}, /* TAG_TR */
	{"td",		HTML_CLRLINE}, /* TAG_TD */
	{"li",		HTML_CLRLINE}, /* TAG_LI */
	{"ul",		HTML_CLRLINE}, /* TAG_UL */
	{"ol",		HTML_CLRLINE}, /* TAG_OL */
};

static	const char	*const htmlfonts[HTMLFONT_MAX] = {
	"roman",
	"bold",
	"italic"
};

static	const char	*const htmlattrs[ATTR_MAX] = {
	"http-equiv",
	"content",
	"name",
	"rel",
	"href",
	"type",
	"media",
	"class",
	"style",
	"width",
	"valign",
	"target",
	"id",
	"summary",
};

static	void		  print_spec(struct html *, const char *, size_t);
static	void		  print_res(struct html *, const char *, size_t);
static	void		  print_ctag(struct html *, enum htmltag);
static	void		  print_doctype(struct html *);
static	void		  print_xmltype(struct html *);
static	int		  print_encode(struct html *, const char *, int);
static	void		  print_metaf(struct html *, enum roffdeco);
static	void		  print_attr(struct html *, 
				const char *, const char *);
static	void		 *ml_alloc(char *, enum htmltype);


static void *
ml_alloc(char *outopts, enum htmltype type)
{
	struct html	*h;
	const char	*toks[4];
	char		*v;

	toks[0] = "style";
	toks[1] = "man";
	toks[2] = "includes";
	toks[3] = NULL;

	h = calloc(1, sizeof(struct html));
	if (NULL == h) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	h->type = type;
	h->tags.head = NULL;
	h->ords.head = NULL;
	h->symtab = chars_init(CHARS_HTML);

	while (outopts && *outopts)
		switch (getsubopt(&outopts, UNCONST(toks), &v)) {
		case (0):
			h->style = v;
			break;
		case (1):
			h->base_man = v;
			break;
		case (2):
			h->base_includes = v;
			break;
		default:
			break;
		}

	return(h);
}

void *
html_alloc(char *outopts)
{

	return(ml_alloc(outopts, HTML_HTML_4_01_STRICT));
}


void *
xhtml_alloc(char *outopts)
{

	return(ml_alloc(outopts, HTML_XHTML_1_0_STRICT));
}


void
html_free(void *p)
{
	struct tag	*tag;
	struct ord	*ord;
	struct html	*h;

	h = (struct html *)p;

	while ((ord = h->ords.head) != NULL) { 
		h->ords.head = ord->next;
		free(ord);
	}

	while ((tag = h->tags.head) != NULL) {
		h->tags.head = tag->next;	
		free(tag);
	}
	
	if (h->symtab)
		chars_free(h->symtab);

	free(h);
}


void
print_gen_head(struct html *h)
{
	struct htmlpair	 tag[4];

	tag[0].key = ATTR_HTTPEQUIV;
	tag[0].val = "Content-Type";
	tag[1].key = ATTR_CONTENT;
	tag[1].val = "text/html; charset=utf-8";
	print_otag(h, TAG_META, 2, tag);

	tag[0].key = ATTR_NAME;
	tag[0].val = "resource-type";
	tag[1].key = ATTR_CONTENT;
	tag[1].val = "document";
	print_otag(h, TAG_META, 2, tag);

	if (h->style) {
		tag[0].key = ATTR_REL;
		tag[0].val = "stylesheet";
		tag[1].key = ATTR_HREF;
		tag[1].val = h->style;
		tag[2].key = ATTR_TYPE;
		tag[2].val = "text/css";
		tag[3].key = ATTR_MEDIA;
		tag[3].val = "all";
		print_otag(h, TAG_LINK, 4, tag);
	}
}


static void
print_spec(struct html *h, const char *p, size_t len)
{
	const char	*rhs;
	size_t		 sz;

	rhs = chars_a2ascii(h->symtab, p, len, &sz);

	if (NULL == rhs) 
		return;
	fwrite(rhs, 1, sz, stdout);
}


static void
print_res(struct html *h, const char *p, size_t len)
{
	const char	*rhs;
	size_t		 sz;

	rhs = chars_a2res(h->symtab, p, len, &sz);

	if (NULL == rhs)
		return;
	fwrite(rhs, 1, sz, stdout);
}


struct tag *
print_ofont(struct html *h, enum htmlfont font)
{
	struct htmlpair	 tag;

	h->metal = h->metac;
	h->metac = font;

	/* FIXME: DECO_ROMAN should just close out preexisting. */

	if (h->metaf && h->tags.head == h->metaf)
		print_tagq(h, h->metaf);

	PAIR_CLASS_INIT(&tag, htmlfonts[font]);
	h->metaf = print_otag(h, TAG_SPAN, 1, &tag);
	return(h->metaf);
}


static void
print_metaf(struct html *h, enum roffdeco deco)
{
	enum htmlfont	 font;

	switch (deco) {
	case (DECO_PREVIOUS):
		font = h->metal;
		break;
	case (DECO_ITALIC):
		font = HTMLFONT_ITALIC;
		break;
	case (DECO_BOLD):
		font = HTMLFONT_BOLD;
		break;
	case (DECO_ROMAN):
		font = HTMLFONT_NONE;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	(void)print_ofont(h, font);
}


static int
print_encode(struct html *h, const char *p, int norecurse)
{
	size_t		 sz;
	int		 len, nospace;
	const char	*seq;
	enum roffdeco	 deco;

	nospace = 0;

	for (; *p; p++) {
		sz = strcspn(p, "\\<>&");

		fwrite(p, 1, sz, stdout);
		p += /* LINTED */
			sz;

		if ('<' == *p) {
			printf("&lt;");
			continue;
		} else if ('>' == *p) {
			printf("&gt;");
			continue;
		} else if ('&' == *p) {
			printf("&amp;");
			continue;
		} else if ('\0' == *p)
			break;

		seq = ++p;
		len = a2roffdeco(&deco, &seq, &sz);

		switch (deco) {
		case (DECO_RESERVED):
			print_res(h, seq, sz);
			break;
		case (DECO_SPECIAL):
			print_spec(h, seq, sz);
			break;
		case (DECO_PREVIOUS):
			/* FALLTHROUGH */
		case (DECO_BOLD):
			/* FALLTHROUGH */
		case (DECO_ITALIC):
			/* FALLTHROUGH */
		case (DECO_ROMAN):
			if (norecurse)
				break;
			print_metaf(h, deco);
			break;
		default:
			break;
		}

		p += len - 1;

		if (DECO_NOSPACE == deco && '\0' == *(p + 1))
			nospace = 1;
	}

	return(nospace);
}


static void
print_attr(struct html *h, const char *key, const char *val)
{
	printf(" %s=\"", key);
	(void)print_encode(h, val, 1);
	putchar('\"');
}


struct tag *
print_otag(struct html *h, enum htmltag tag, 
		int sz, const struct htmlpair *p)
{
	int		 i;
	struct tag	*t;

	/* Push this tags onto the stack of open scopes. */

	if ( ! (HTML_NOSTACK & htmltags[tag].flags)) {
		t = malloc(sizeof(struct tag));
		if (NULL == t) {
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		t->tag = tag;
		t->next = h->tags.head;
		h->tags.head = t;
	} else
		t = NULL;

	if ( ! (HTML_NOSPACE & h->flags))
		if ( ! (HTML_CLRLINE & htmltags[tag].flags))
			putchar(' ');

	/* Print out the tag name and attributes. */

	printf("<%s", htmltags[tag].name);
	for (i = 0; i < sz; i++)
		print_attr(h, htmlattrs[p[i].key], p[i].val);

	/* Add non-overridable attributes. */

	if (TAG_HTML == tag && HTML_XHTML_1_0_STRICT == h->type) {
		print_attr(h, "xmlns", "http://www.w3.org/1999/xhtml");
		print_attr(h, "xml:lang", "en");
		print_attr(h, "lang", "en");
	}

	/* Accomodate for XML "well-formed" singleton escaping. */

	if (HTML_AUTOCLOSE & htmltags[tag].flags)
		switch (h->type) {
		case (HTML_XHTML_1_0_STRICT):
			putchar('/');
			break;
		default:
			break;
		}

	putchar('>');

	h->flags |= HTML_NOSPACE;
	return(t);
}


static void
print_ctag(struct html *h, enum htmltag tag)
{
	
	printf("</%s>", htmltags[tag].name);
	if (HTML_CLRLINE & htmltags[tag].flags) {
		h->flags |= HTML_NOSPACE;
		putchar('\n');
	} 
}


void
print_gen_decls(struct html *h)
{

	print_xmltype(h);
	print_doctype(h);
}


static void
print_xmltype(struct html *h)
{
	const char	*decl;

	switch (h->type) {
	case (HTML_XHTML_1_0_STRICT):
		decl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
		break;
	default:
		decl = NULL;
		break;
	}

	if (NULL == decl)
		return;

	printf("%s\n", decl);
}


static void
print_doctype(struct html *h)
{
	const char	*doctype;
	const char	*dtd;
	const char	*name;

	switch (h->type) {
	case (HTML_HTML_4_01_STRICT):
		name = "HTML";
		doctype = "-//W3C//DTD HTML 4.01//EN";
		dtd = "http://www.w3.org/TR/html4/strict.dtd";
		break;
	default:
		name = "html";
		doctype = "-//W3C//DTD XHTML 1.0 Strict//EN";
		dtd = "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd";
		break;
	}

	printf("<!DOCTYPE %s PUBLIC \"%s\" \"%s\">\n", 
			name, doctype, dtd);
}


void
print_text(struct html *h, const char *p)
{

	if (*p && 0 == *(p + 1))
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
			if ( ! (HTML_IGNDELIM & h->flags))
				h->flags |= HTML_NOSPACE;
			break;
		default:
			break;
		}

	if ( ! (h->flags & HTML_NOSPACE))
		putchar(' ');

	assert(p);
	if ( ! print_encode(h, p, 0))
		h->flags &= ~HTML_NOSPACE;

	if (*p && 0 == *(p + 1))
		switch (*p) {
		case('('):
			/* FALLTHROUGH */
		case('['):
			h->flags |= HTML_NOSPACE;
			break;
		default:
			break;
		}
}


void
print_tagq(struct html *h, const struct tag *until)
{
	struct tag	*tag;

	while ((tag = h->tags.head) != NULL) {
		if (tag == h->metaf)
			h->metaf = NULL;
		print_ctag(h, tag->tag);
		h->tags.head = tag->next;
		free(tag);
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
		if (tag == h->metaf)
			h->metaf = NULL;
		print_ctag(h, tag->tag);
		h->tags.head = tag->next;
		free(tag);
	}
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
	bufncat(h, ":", 1);
	bufcat(h, val);
	bufncat(h, ";", 1);
}


void
bufcat(struct html *h, const char *p)
{

	bufncat(h, p, strlen(p));
}


void
buffmt(struct html *h, const char *fmt, ...)
{
	va_list		 ap;

	va_start(ap, fmt);
	(void)vsnprintf(h->buf + (int)h->buflen, 
			BUFSIZ - h->buflen - 1, fmt, ap);
	va_end(ap);
	h->buflen = strlen(h->buf);
}


void
bufncat(struct html *h, const char *p, size_t sz)
{

	if (h->buflen + sz > BUFSIZ - 1)
		sz = BUFSIZ - 1 - h->buflen;

	(void)strncat(h->buf, p, sz);
	h->buflen += sz;
}


void
buffmt_includes(struct html *h, const char *name)
{
	const char	*p, *pp;

	pp = h->base_includes;
	
	while (NULL != (p = strchr(pp, '%'))) {
		bufncat(h, pp, (size_t)(p - pp));
		switch (*(p + 1)) {
		case('I'):
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
buffmt_man(struct html *h, 
		const char *name, const char *sec)
{
	const char	*p, *pp;

	pp = h->base_man;
	
	/* LINTED */
	while (NULL != (p = strchr(pp, '%'))) {
		bufncat(h, pp, (size_t)(p - pp));
		switch (*(p + 1)) {
		case('S'):
			bufcat(h, sec ? sec : "1");
			break;
		case('N'):
			buffmt(h, name);
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
	const char	*u;

	v = su->scale;

	switch (su->unit) {
	case (SCALE_CM):
		u = "cm";
		break;
	case (SCALE_IN):
		u = "in";
		break;
	case (SCALE_PC):
		u = "pc";
		break;
	case (SCALE_PT):
		u = "pt";
		break;
	case (SCALE_EM):
		u = "em";
		break;
	case (SCALE_MM):
		if (0 == (v /= 100))
			v = 1;
		u = "em";
		break;
	case (SCALE_EN):
		u = "ex";
		break;
	case (SCALE_BU):
		u = "ex";
		break;
	case (SCALE_VS):
		u = "em";
		break;
	default:
		u = "ex";
		break;
	}

	if (su->pt)
		buffmt(h, "%s: %f%s;", p, v, u);
	else
		/* LINTED */
		buffmt(h, "%s: %d%s;", p, (int)v, u);
}


void
html_idcat(char *dst, const char *src, int sz)
{
	int		 ssz;

	assert(sz);

	/* Cf. <http://www.w3.org/TR/html4/types.html#h-6.2>. */

	for ( ; *dst != '\0' && sz; dst++, sz--)
		/* Jump to end. */ ;

	assert(sz > 2);

	/* We can't start with a number (bah). */

	*dst++ = 'x';
	*dst = '\0';
	sz--;

	for ( ; *src != '\0' && sz > 1; src++) {
		ssz = snprintf(dst, (size_t)sz, "%.2x", *src);
		sz -= ssz;
		dst += ssz;
	}
}
