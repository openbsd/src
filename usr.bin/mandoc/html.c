/*	$Id: html.c,v 1.25 2011/04/24 16:22:02 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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

#include "mandoc.h"
#include "out.h"
#include "html.h"
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
	{"p",		HTML_CLRLINE | HTML_NOSTACK | HTML_AUTOCLOSE}, /* TAG_P */
	{"pre",		HTML_CLRLINE }, /* TAG_PRE */
	{"b",		0 }, /* TAG_B */
	{"i",		0 }, /* TAG_I */
	{"code",	0 }, /* TAG_CODE */
	{"small",	0 }, /* TAG_SMALL */
};

static	const char	*const htmlattrs[ATTR_MAX] = {
	"http-equiv", /* ATTR_HTTPEQUIV */
	"content", /* ATTR_CONTENT */
	"name", /* ATTR_NAME */
	"rel", /* ATTR_REL */
	"href", /* ATTR_HREF */
	"type", /* ATTR_TYPE */
	"media", /* ATTR_MEDIA */
	"class", /* ATTR_CLASS */
	"style", /* ATTR_STYLE */
	"width", /* ATTR_WIDTH */
	"id", /* ATTR_ID */
	"summary", /* ATTR_SUMMARY */
	"align", /* ATTR_ALIGN */
	"colspan", /* ATTR_COLSPAN */
};

static	void		  print_num(struct html *, const char *, size_t);
static	void		  print_spec(struct html *, enum roffdeco,
				const char *, size_t);
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

	h = mandoc_calloc(1, sizeof(struct html));

	h->type = type;
	h->tags.head = NULL;
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
	struct html	*h;

	h = (struct html *)p;

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

/* ARGSUSED */
static void
print_num(struct html *h, const char *p, size_t len)
{
	const char	*rhs;

	rhs = chars_num2char(p, len);
	if (rhs)
		putchar((int)*rhs);
}

static void
print_spec(struct html *h, enum roffdeco d, const char *p, size_t len)
{
	int		 cp;
	const char	*rhs;
	size_t		 sz;

	if ((cp = chars_spec2cp(h->symtab, p, len)) > 0) {
		printf("&#%d;", cp);
		return;
	} else if (-1 == cp && DECO_SSPECIAL == d) {
		fwrite(p, 1, len, stdout);
		return;
	} else if (-1 == cp)
		return;

	if (NULL != (rhs = chars_spec2str(h->symtab, p, len, &sz)))
		fwrite(rhs, 1, sz, stdout);
}


static void
print_res(struct html *h, const char *p, size_t len)
{
	int		 cp;
	const char	*rhs;
	size_t		 sz;

	if ((cp = chars_res2cp(h->symtab, p, len)) > 0) {
		printf("&#%d;", cp);
		return;
	} else if (-1 == cp)
		return;

	if (NULL != (rhs = chars_res2str(h->symtab, p, len, &sz)))
		fwrite(rhs, 1, sz, stdout);
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

	if (h->metaf) {
		print_tagq(h, h->metaf);
		h->metaf = NULL;
	}

	h->metal = h->metac;
	h->metac = font;

	if (HTMLFONT_NONE != font)
		h->metaf = HTMLFONT_BOLD == font ?
			print_otag(h, TAG_B, 0, NULL) :
			print_otag(h, TAG_I, 0, NULL);
}


static int
print_encode(struct html *h, const char *p, int norecurse)
{
	size_t		 sz;
	int		 len, nospace;
	const char	*seq;
	enum roffdeco	 deco;
	static const char rejs[6] = { '\\', '<', '>', '&', ASCII_HYPH, '\0' };

	nospace = 0;

	for (; *p; p++) {
		sz = strcspn(p, rejs);

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
		} else if (ASCII_HYPH == *p) {
			/*
			 * Note: "soft hyphens" aren't graphically
			 * displayed when not breaking the text; we want
			 * them to be displayed.
			 */
			/*printf("&#173;");*/
			putchar('-');
			continue;
		} else if ('\0' == *p)
			break;

		seq = ++p;
		len = a2roffdeco(&deco, &seq, &sz);

		switch (deco) {
		case (DECO_NUMBERED):
			print_num(h, seq, sz);
			break;
		case (DECO_RESERVED):
			print_res(h, seq, sz);
			break;
		case (DECO_SSPECIAL):
			/* FALLTHROUGH */
		case (DECO_SPECIAL):
			print_spec(h, deco, seq, sz);
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

	if ((HTML_AUTOCLOSE | HTML_CLRLINE) & htmltags[tag].flags)
		putchar('\n');

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

	if (HTML_XHTML_1_0_STRICT == h->type)
		puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
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
	if (HTMLFONT_NONE != h->metac)
		h->metaf = HTMLFONT_BOLD == h->metac ?
			print_otag(h, TAG_B, 0, NULL) :
			print_otag(h, TAG_I, 0, NULL);

	assert(word);
	if ( ! print_encode(h, word, 0))
		if ( ! (h->flags & HTML_NONOSPACE))
			h->flags &= ~HTML_NOSPACE;

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
		/* 
		 * Remember to close out and nullify the current
		 * meta-font and table, if applicable.
		 */
		if (tag == h->metaf)
			h->metaf = NULL;
		if (tag == h->tblt)
			h->tblt = NULL;
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
		/* 
		 * Remember to close out and nullify the current
		 * meta-font and table, if applicable.
		 */
		if (tag == h->metaf)
			h->metaf = NULL;
		if (tag == h->tblt)
			h->tblt = NULL;
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

	/* 
	 * XXX: the CSS spec isn't clear as to which types accept
	 * integer or real numbers, so we just make them all decimals.
	 */
	buffmt(h, "%s: %.2f%s;", p, v, u);
}


void
html_idcat(char *dst, const char *src, int sz)
{
	int		 ssz;

	assert(sz > 2);

	/* Cf. <http://www.w3.org/TR/html4/types.html#h-6.2>. */

	/* We can't start with a number (bah). */

	if ('#' == *dst) {
		dst++;
		sz--;
	}
	if ('\0' == *dst) {
		*dst++ = 'x';
		*dst = '\0';
		sz--;
	}

	for ( ; *dst != '\0' && sz; dst++, sz--)
		/* Jump to end. */ ;

	for ( ; *src != '\0' && sz > 1; src++) {
		ssz = snprintf(dst, (size_t)sz, "%.2x", *src);
		sz -= ssz;
		dst += ssz;
	}
}
