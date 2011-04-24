/*	$Id: out.h,v 1.10 2011/04/24 16:22:02 schwarze Exp $ */
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

enum	roffscale {
	SCALE_CM, /* centimeters (c) */
	SCALE_IN, /* inches (i) */
	SCALE_PC, /* pica (P) */
	SCALE_PT, /* points (p) */
	SCALE_EM, /* ems (m) */
	SCALE_MM, /* mini-ems (M) */
	SCALE_EN, /* ens (n) */
	SCALE_BU, /* default horizontal (u) */
	SCALE_VS, /* default vertical (v) */
	SCALE_FS, /* syn. for u (f) */
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

enum	chars {
	CHARS_ASCII, /* 7-bit ascii representation */
	CHARS_HTML /* unicode values */
};

struct	roffcol {
	size_t		 width; /* width of cell */
	size_t		 decimal; /* decimal position in cell */
};

struct	roffsu {
	enum roffscale	  unit;
	double		  scale;
};

typedef	size_t	(*tbl_strlen)(const char *, void *);
typedef	size_t	(*tbl_len)(size_t, void *);

struct	rofftbl {
	tbl_strlen	 slen; /* calculate string length */
	tbl_len		 len; /* produce width of empty space */
	struct roffcol	*cols; /* master column specifiers */
	void		*arg; /* passed to slen and len */
};

__BEGIN_DECLS

#define	SCALE_VS_INIT(p, v) \
	do { (p)->unit = SCALE_VS; \
	     (p)->scale = (v); } \
	while (/* CONSTCOND */ 0)

#define	SCALE_HS_INIT(p, v) \
	do { (p)->unit = SCALE_BU; \
	     (p)->scale = (v); } \
	while (/* CONSTCOND */ 0)

int	  	  a2roffsu(const char *, struct roffsu *, enum roffscale);
int	  	  a2roffdeco(enum roffdeco *, const char **, size_t *);
void	  	  time2a(time_t, char *, size_t);
void	  	  tblcalc(struct rofftbl *tbl, const struct tbl_span *);

void		 *chars_init(enum chars);
const char	 *chars_num2char(const char *, size_t);
const char	 *chars_spec2str(void *, const char *, size_t, size_t *);
int		  chars_spec2cp(void *, const char *, size_t);
const char	 *chars_res2str(void *, const char *, size_t, size_t *);
int		  chars_res2cp(void *, const char *, size_t);
void		  chars_free(void *);

__END_DECLS

#endif /*!OUT_H*/
