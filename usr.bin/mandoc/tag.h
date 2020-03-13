/* $OpenBSD: tag.h,v 1.11 2020/03/13 00:31:05 schwarze Exp $ */
/*
 * Copyright (c) 2015, 2018, 2019, 2020 Ingo Schwarze <schwarze@openbsd.org>
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
 *
 * Internal interfaces to tag syntax tree nodes.
 * For use by mandoc(1) validation modules only.
 */

/*
 * Tagging priorities.
 * Lower numbers indicate higher importance.
 */
#define	TAG_MANUAL	1		/* Set with a .Tg macro. */
#define	TAG_STRONG	2		/* Good automatic tagging. */
#define	TAG_WEAK	(INT_MAX - 2)	/* Dubious automatic tagging. */
#define	TAG_FALLBACK	(INT_MAX - 1)	/* Tag only used if unique. */
#define	TAG_DELETE	(INT_MAX)	/* Tag not used at all. */

/*
 * Return values of tag_check().
 */
enum tag_result {
	TAG_OK,		/* Argument exists as a tag. */
	TAG_MISS,	/* Argument not found. */
	TAG_EMPTY	/* No tag exists at all. */
};


void		 tag_alloc(void);
void		 tag_put(const char *, int, struct roff_node *);
enum tag_result	 tag_check(const char *);
void		 tag_free(void);
