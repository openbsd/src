/*	$Id: html.h,v 1.1 2009/10/21 19:13:50 schwarze Exp $ */
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
#ifndef HTML_H
#define HTML_H

__BEGIN_DECLS

enum	htmltag {
	TAG_HTML,
	TAG_HEAD,
	TAG_BODY,
	TAG_META,
	TAG_TITLE,
	TAG_DIV,
	TAG_H1,
	TAG_H2,
	TAG_P,
	TAG_SPAN,
	TAG_LINK,
	TAG_BR,
	TAG_A,
	TAG_TABLE,
	TAG_COL,
	TAG_TR,
	TAG_TD,
	TAG_LI,
	TAG_UL,
	TAG_OL,
	TAG_BASE,
	TAG_MAX
};

enum	htmlattr {
	ATTR_HTTPEQUIV,
	ATTR_CONTENT,
	ATTR_NAME,
	ATTR_REL,
	ATTR_HREF,
	ATTR_TYPE,
	ATTR_MEDIA,
	ATTR_CLASS,
	ATTR_STYLE,
	ATTR_WIDTH,
	ATTR_VALIGN,
	ATTR_TARGET,
	ATTR_ID,
	ATTR_MAX
};

struct	tag {
	enum htmltag	  tag;
	SLIST_ENTRY(tag)  entry;
};

struct	ord {
	int		  pos;
	const void	 *cookie;
	SLIST_ENTRY(ord)  entry;
};

SLIST_HEAD(tagq, tag);
SLIST_HEAD(ordq, ord);

struct	htmlpair {
	enum htmlattr	  key;
	const char	 *val;
};

#define	PAIR_CLASS_INIT(p, v) \
	do { (p)->key = ATTR_CLASS; \
	(p)->val = (v); } while (/* CONSTCOND */ 0)
#define	PAIR_HREF_INIT(p, v) \
	do { (p)->key = ATTR_HREF; \
	(p)->val = (v); } while (/* CONSTCOND */ 0)
#define	PAIR_STYLE_INIT(p, h) \
	do { (p)->key = ATTR_STYLE; \
	(p)->val = (h)->buf; } while (/* CONSTCOND */ 0)

struct	html {
	int		  flags;
#define	HTML_NOSPACE	 (1 << 0)
#define	HTML_NEWLINE	 (1 << 1)
#define	HTML_IGNDELIM	 (1 << 2)
	struct tagq	  tags;
	struct ordq	  ords;
	void		 *symtab;
	char		 *base;
	char		 *base_man;
	char		 *base_includes;
	char		 *style;
	char		  buf[BUFSIZ];
	size_t		  buflen;
};

struct	roffsu;

void		  print_gen_doctype(struct html *);
void		  print_gen_head(struct html *);
struct tag	 *print_otag(struct html *, enum htmltag, 
				int, const struct htmlpair *);
void		  print_tagq(struct html *, const struct tag *);
void		  print_stagq(struct html *, const struct tag *);
void		  print_text(struct html *, const char *);

void		  bufcat_su(struct html *, const char *, 
			const struct roffsu *);
void		  buffmt_man(struct html *, 
			const char *, const char *);
void		  buffmt_includes(struct html *, const char *);
void		  buffmt(struct html *, const char *, ...);
void		  bufcat(struct html *, const char *);
void		  bufcat_style(struct html *, 
			const char *, const char *);
void		  bufncat(struct html *, const char *, size_t);
void		  bufinit(struct html *);

__END_DECLS

#endif /*!HTML_H*/
