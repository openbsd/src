/*	$OpenBSD: libroff.h,v 1.21 2018/12/13 02:05:57 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014, 2017 Ingo Schwarze <schwarze@openbsd.org>
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

struct	eqn_node {
	struct mparse	 *parse;  /* main parser, for error reporting */
	struct roff_node *node;   /* syntax tree of this equation */
	struct eqn_def	 *defs;   /* array of definitions */
	char		 *data;   /* source code of this equation */
	char		 *start;  /* first byte of the current token */
	char		 *end;	  /* first byte of the next token */
	size_t		  defsz;  /* number of definitions */
	size_t		  sz;     /* length of the source code */
	size_t		  toksz;  /* length of the current token */
	int		  gsize;  /* default point size */
	int		  delim;  /* in-line delimiters enabled */
	char		  odelim; /* in-line opening delimiter */
	char		  cdelim; /* in-line closing delimiter */
};

struct	eqn_def {
	char		 *key;
	size_t		  keysz;
	char		 *val;
	size_t		  valsz;
};


struct eqn_node	*eqn_alloc(struct mparse *);
void		 eqn_box_free(struct eqn_box *);
void		 eqn_free(struct eqn_node *);
void		 eqn_parse(struct eqn_node *);
void		 eqn_read(struct eqn_node *, const char *);
void		 eqn_reset(struct eqn_node *);
