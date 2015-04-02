/*	$OpenBSD: libman.h,v 1.44 2015/04/02 22:06:17 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

enum	man_next {
	MAN_NEXT_SIBLING = 0,
	MAN_NEXT_CHILD
};

struct	man {
	struct mparse	*parse; /* parse pointer */
	const char	*defos; /* default OS argument for .TH */
	int		 quick; /* abort parse early */
	int		 flags; /* parse flags */
#define	MAN_ELINE	(1 << 1) /* Next-line element scope. */
#define	MAN_BLINE	(1 << 2) /* Next-line block scope. */
#define	MAN_LITERAL	(1 << 4) /* Literal input. */
#define	MAN_NEWLINE	(1 << 6) /* first macro/text in a line */
	enum man_next	 next; /* where to put the next node */
	struct roff_node *last; /* the last parsed node */
	struct roff_node *first; /* the first parsed node */
	struct man_meta	 meta; /* document meta-data */
	struct roff	*roff;
};

#define	MACRO_PROT_ARGS	  struct man *man, \
			  int tok, \
			  int line, \
			  int ppos, \
			  int *pos, \
			  char *buf

struct	man_macro {
	void		(*fp)(MACRO_PROT_ARGS);
	int		  flags;
#define	MAN_SCOPED	 (1 << 0)  /* Optional next-line scope. */
#define	MAN_NSCOPED	 (1 << 1)  /* Allowed in next-line element scope. */
#define	MAN_BSCOPE	 (1 << 2)  /* Break next-line block scope. */
#define	MAN_JOIN	 (1 << 3)  /* Join arguments together. */
};

extern	const struct man_macro *const man_macros;

__BEGIN_DECLS

void		  man_word_alloc(struct man *, int, int, const char *);
void		  man_word_append(struct man *, const char *);
void		  man_block_alloc(struct man *, int, int, int);
void		  man_head_alloc(struct man *, int, int, int);
void		  man_body_alloc(struct man *, int, int, int);
void		  man_elem_alloc(struct man *, int, int, int);
void		  man_node_delete(struct man *, struct roff_node *);
void		  man_hash_init(void);
int		  man_hash_find(const char *);
void		  man_macroend(struct man *);
void		  man_valid_post(struct man *);
void		  man_unscope(struct man *, const struct roff_node *);

__END_DECLS
