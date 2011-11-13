/*	$Id: apropos_db.h,v 1.1 2011/11/13 10:28:38 schwarze Exp $ */
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

#define TYPE_NAME	  0x01
#define TYPE_FUNCTION	  0x02
#define TYPE_UTILITY	  0x04
#define TYPE_INCLUDES	  0x08
#define TYPE_VARIABLE	  0x10
#define TYPE_STANDARD	  0x20
#define TYPE_AUTHOR	  0x40
#define TYPE_CONFIG	  0x80
#define TYPE_DESC	  0x100
#define TYPE_XREF	  0x200
#define TYPE_PATH	  0x400
#define TYPE_ENV	  0x800
#define TYPE_ERR	  0x1000

struct	rec {
	char		*file; /* file in file-system */
	char		*cat; /* category (3p, 3, etc.) */
	char		*title; /* title (FOO, etc.) */
	char		*arch; /* arch (or empty string) */
	char		*desc; /* description (from Nd) */
	unsigned int	 rec; /* record in index */
	/*
	 * By the time the apropos_search() callback is called, these
	 * are superfluous.
	 * Maintain a binary tree for checking the uniqueness of `rec'
	 * when adding elements to the results array.
	 * Since the results array is dynamic, use offset in the array
	 * instead of a pointer to the structure.
	 */
	int		 lhs;
	int		 rhs;
};

struct	opts {
	const char	*arch; /* restrict to architecture */
	const char	*cat; /* restrict to manual section */
};

__BEGIN_DECLS

struct	expr;

void	 	 apropos_search(const struct opts *, 
			const struct expr *, void *, 
			void (*)(struct rec *, size_t, void *));

struct	expr	*exprcomp(int, char *[], int);
void		 exprfree(struct expr *);

__END_DECLS

#endif /*!APROPOS_H*/
