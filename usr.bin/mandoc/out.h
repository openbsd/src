/*	$Id: out.h,v 1.9 2011/03/07 01:35:33 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef OUT_H
#define OUT_H

__BEGIN_DECLS

struct	roffcol {
	size_t		 width; /* width of cell */
	size_t		 decimal; /* decimal position in cell */
};

typedef	size_t (*tbl_strlen)(const char *, void *);
typedef	size_t (*tbl_len)(size_t, void *);

struct	rofftbl {
	tbl_strlen	 slen; /* calculate string length */
	tbl_len		 len; /* produce width of empty space */
	struct roffcol	*cols; /* master column specifiers */
	void		*arg; /* passed to slen and len */
};

enum	roffscale {
	SCALE_CM,
	SCALE_IN,
	SCALE_PC,
	SCALE_PT,
	SCALE_EM,
	SCALE_MM,
	SCALE_EN,
	SCALE_BU,
	SCALE_VS,
	SCALE_FS,
	SCALE_MAX
};

enum	roffdeco {
	DECO_NONE,
	DECO_NUMBERED, /* numbered character */
	DECO_SPECIAL, /* special character */
	DECO_SSPECIAL, /* single-char special */
	DECO_RESERVED, /* reserved word */
	DECO_BOLD, /* bold font */
	DECO_ITALIC, /* italic font */
	DECO_ROMAN, /* "normal" undecorated font */
	DECO_PREVIOUS, /* revert to previous font */
	DECO_NOSPACE, /* suppress spacing */
	DECO_FONT, /* font */
	DECO_FFONT, /* font family */
	DECO_MAX
};

struct	roffsu {
	enum roffscale	  unit;
	double		  scale;
};

#define	SCALE_VS_INIT(p, v) \
	do { (p)->unit = SCALE_VS; \
	     (p)->scale = (v); } \
	while (/* CONSTCOND */ 0)

#define	SCALE_HS_INIT(p, v) \
	do { (p)->unit = SCALE_BU; \
	     (p)->scale = (v); } \
	while (/* CONSTCOND */ 0)

int	  a2roffsu(const char *, struct roffsu *, enum roffscale);
int	  a2roffdeco(enum roffdeco *, const char **, size_t *);
void	  time2a(time_t, char *, size_t);
void	  tblcalc(struct rofftbl *tbl, const struct tbl_span *);

__END_DECLS

#endif /*!OUT_H*/
