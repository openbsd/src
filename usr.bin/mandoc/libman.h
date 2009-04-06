/* $Id: libman.h,v 1.1 2009/04/06 20:30:40 kristaps Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef LIBMAN_H
#define LIBMAN_H

#include "man.h"

enum	man_next {
	MAN_NEXT_SIBLING = 0,
	MAN_NEXT_CHILD
};

struct	man {
	void		*data;
	struct man_cb	 cb;
	void		*htab;
	int		 pflags;
	int		 flags;
#define	MAN_HALT	(1 << 0)
#define	MAN_NLINE	(1 << 1)
	enum man_next	 next;
	struct man_node	*last;
	struct man_node	*first;
	struct man_meta	 meta;
};

__BEGIN_DECLS

int		  man_word_alloc(struct man *, int, int, const char *);
int		  man_elem_alloc(struct man *, int, int, int);
void		  man_node_free(struct man_node *);
void		  man_node_freelist(struct man_node *);
void		 *man_hash_alloc(void);
int		  man_macro(struct man *, int, 
			int, int, int *, char *);
int		  man_hash_find(const void *, const char *);
void		  man_hash_free(void *);
int		  man_macroend(struct man *);
int		  man_vwarn(struct man *, int, int, const char *, ...);
int		  man_verr(struct man *, int, int, const char *, ...);
int		  man_valid_post(struct man *);
int		  man_action_post(struct man *);

__END_DECLS

#endif /*!LIBMAN_H*/
