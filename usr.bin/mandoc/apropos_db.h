/*	$Id: apropos_db.h,v 1.6 2011/11/18 01:10:03 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef APROPOS_H
#define APROPOS_H

struct	res {
	char		*file; /* file in file-system */
	char		*cat; /* category (3p, 3, etc.) */
	char		*title; /* title (FOO, etc.) */
	char		*arch; /* arch (or empty string) */
	char		*desc; /* description (from Nd) */
	unsigned int	 rec; /* record in index */
};

struct	opts {
	const char	*arch; /* restrict to architecture */
	const char	*cat; /* restrict to manual section */
};

__BEGIN_DECLS

struct	expr;

int	 	 apropos_search(int, char **, const struct opts *, 
			const struct expr *, size_t, void *, 
			void (*)(struct res *, size_t, void *));
struct	expr	*exprcomp(int, char *[], size_t *);
void		 exprfree(struct expr *);

__END_DECLS

#endif /*!APROPOS_H*/
