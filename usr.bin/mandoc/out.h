/*	$Id: out.h,v 1.6 2010/07/25 18:05:54 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@bsd.lv>
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

#define	DATESIZ		24

__BEGIN_DECLS

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
	DECO_SPECIAL, /* special character */
	DECO_SSPECIAL, /* single-char special */
	DECO_RESERVED, /* reserved word */
	DECO_BOLD,
	DECO_ITALIC,
	DECO_ROMAN,
	DECO_PREVIOUS,
	DECO_NOSPACE,
	DECO_FONT, /* font */
	DECO_FFONT, /* font family */
	DECO_MAX
};

struct	roffsu {
	enum roffscale	  unit;
	double		  scale;
};

#define	SCALE_INVERT(p) \
	do { (p)->scale = -(p)->scale; } \
	while (/* CONSTCOND */ 0)

#define	SCALE_VS_INIT(p, v) \
	do { (p)->unit = SCALE_VS; \
	     (p)->scale = (v); } \
	while (/* CONSTCOND */ 0)

#define	SCALE_HS_INIT(p, v) \
	do { (p)->unit = SCALE_BU; \
	     (p)->scale = (v); } \
	while (/* CONSTCOND */ 0)

int		  a2roffsu(const char *, 
			struct roffsu *, enum roffscale);
int		  a2roffdeco(enum roffdeco *, const char **, size_t *);
void		  time2a(time_t, char *, size_t);

__END_DECLS

#endif /*!HTML_H*/
