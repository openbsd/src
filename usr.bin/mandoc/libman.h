/*	$Id: libman.h,v 1.15 2010/04/02 11:37:07 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@kth.se>
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
	int		 pflags; /* parse flags (see man.h) */
	int		 svflags; /* flags saved during roff blocks */
	int		 flags; /* parse flags */
#define	MAN_HALT	(1 << 0) /* badness happened: die */
#define	MAN_ELINE	(1 << 1) /* Next-line element scope. */
#define	MAN_BLINE	(1 << 2) /* Next-line block scope. */
#define	MAN_ILINE	(1 << 3) /* Ignored in next-line scope. */
#define	MAN_LITERAL	(1 << 4) /* Literal input. */
#define	MAN_BPLINE	(1 << 5)
	enum man_next	 next;
	enum man_next	 svnext;
	struct man_node	*last;
	struct man_node	*first;
	struct man_meta	 meta;
};

enum	merr {
	WNPRINT = 0,
	WMSEC,
	WDATE,
	WLNSCOPE,
	WLNSCOPE2,
	WTSPACE,
	WTQUOTE,
	WNODATA,
	WNOTITLE,
	WESCAPE,
	WNUMFMT,
	WHEADARGS,
	WBODYARGS,
	WNHEADARGS,
	WMACROFORM,
	WEXITSCOPE,
	WNOSCOPE,
	WOLITERAL,
	WNLITERAL,
	WROFFNEST,
	WROFFSCOPE,
	WTITLECASE,
	WERRMAX
};

#define	MACRO_PROT_ARGS	  struct man *m, enum mant tok, int line, \
			  int ppos, int *pos, char *buf

struct	man_macro {
	int		(*fp)(MACRO_PROT_ARGS);
	int		  flags;
#define	MAN_SCOPED	 (1 << 0)
#define	MAN_EXPLICIT	 (1 << 1)	/* See blk_imp(). */
#define	MAN_FSCOPED	 (1 << 2)	/* See blk_imp(). */
#define	MAN_NSCOPED	 (1 << 3)	/* See in_line_eoln(). */
#define	MAN_NOCLOSE	 (1 << 4)	/* See blk_exp(). */
};

extern	const struct man_macro *const man_macros;

__BEGIN_DECLS

#define		  man_perr(m, l, p, t) \
		  man_err((m), (l), (p), 1, (t))
#define		  man_pwarn(m, l, p, t) \
		  man_err((m), (l), (p), 0, (t))
#define		  man_nerr(m, n, t) \
		  man_err((m), (n)->line, (n)->pos, 1, (t))
#define		  man_nwarn(m, n, t) \
		  man_err((m), (n)->line, (n)->pos, 0, (t))

int		  man_word_alloc(struct man *, int, int, const char *);
int		  man_block_alloc(struct man *, int, int, enum mant);
int		  man_head_alloc(struct man *, int, int, enum mant);
int		  man_body_alloc(struct man *, int, int, enum mant);
int		  man_elem_alloc(struct man *, int, int, enum mant);
void		  man_node_delete(struct man *, struct man_node *);
void		  man_hash_init(void);
enum	mant	  man_hash_find(const char *);
int		  man_macroend(struct man *);
int		  man_args(struct man *, int, int *, char *, char **);
#define	ARGS_ERROR	(-1)
#define	ARGS_EOLN	(0)
#define	ARGS_WORD	(1)
#define	ARGS_QWORD	(1)
int		  man_err(struct man *, int, int, int, enum merr);
int		  man_vwarn(struct man *, int, int, const char *, ...);
int		  man_verr(struct man *, int, int, const char *, ...);
int		  man_valid_post(struct man *);
int		  man_valid_pre(struct man *, const struct man_node *);
int		  man_action_post(struct man *);
int		  man_action_pre(struct man *, struct man_node *);
int		  man_unscope(struct man *, 
			const struct man_node *, enum merr);

__END_DECLS

#endif /*!LIBMAN_H*/
