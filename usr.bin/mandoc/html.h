/*	$Id: html.h,v 1.8 2010/07/25 18:05:54 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
	ATTR_SUMMARY,
	ATTR_MAX
};

enum	htmlfont {
	HTMLFONT_NONE = 0,
	HTMLFONT_BOLD,
	HTMLFONT_ITALIC,
	HTMLFONT_MAX
};

struct	tag {
	struct tag	 *next;
	enum htmltag	  tag;
};

struct	ord {
	struct ord	 *next;
	const void	 *cookie;
	int		  pos;
};

struct tagq {
	struct tag	 *head;
};
struct ordq {
	struct ord	 *head;
};

struct	htmlpair {
	enum htmlattr	  key;
	const char	 *val;
};

#define	PAIR_INIT(p, t, v) \
	do { \
		(p)->key = (t); \
		(p)->val = (v); \
	} while (/* CONSTCOND */ 0)

#define	PAIR_ID_INIT(p, v)	PAIR_INIT(p, ATTR_ID, v)
#define	PAIR_CLASS_INIT(p, v)	PAIR_INIT(p, ATTR_CLASS, v)
#define	PAIR_HREF_INIT(p, v)	PAIR_INIT(p, ATTR_HREF, v)
#define	PAIR_STYLE_INIT(p, h)	PAIR_INIT(p, ATTR_STYLE, (h)->buf)
#define	PAIR_SUMMARY_INIT(p, v)	PAIR_INIT(p, ATTR_SUMMARY, v)

enum	htmltype {
	HTML_HTML_4_01_STRICT,
	HTML_XHTML_1_0_STRICT
};

struct	html {
	int		  flags;
#define	HTML_NOSPACE	 (1 << 0)
#define	HTML_IGNDELIM	 (1 << 1)
#define	HTML_KEEP	 (1 << 2)
#define	HTML_PREKEEP	 (1 << 3)
#define	HTML_NONOSPACE	 (1 << 4)
	struct tagq	  tags;
	struct ordq	  ords;
	void		 *symtab;
	char		 *base;
	char		 *base_man;
	char		 *base_includes;
	char		 *style;
	char		  buf[BUFSIZ];
	size_t		  buflen;
	struct tag	 *metaf;
	enum htmlfont	  metal;
	enum htmlfont	  metac;
	enum htmltype	  type;
};

struct	roffsu;

void		  print_gen_decls(struct html *);
void		  print_gen_head(struct html *);
struct tag	 *print_ofont(struct html *, enum htmlfont);
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

void		  html_idcat(char *, const char *, int);

__END_DECLS

#endif /*!HTML_H*/
