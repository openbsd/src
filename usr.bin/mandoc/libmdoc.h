/*	$Id: libmdoc.h,v 1.41 2010/07/13 01:09:12 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef LIBMDOC_H
#define LIBMDOC_H

#include "mdoc.h"

enum	mdoc_next {
	MDOC_NEXT_SIBLING = 0,
	MDOC_NEXT_CHILD
};

struct	mdoc {
	void		 *data; /* private application data */
	mandocmsg	  msg; /* message callback */
	int		  flags;
#define	MDOC_HALT	 (1 << 0) /* error in parse: halt */
#define	MDOC_LITERAL	 (1 << 1) /* in a literal scope */
#define	MDOC_PBODY	 (1 << 2) /* in the document body */
#define	MDOC_NEWLINE	 (1 << 3) /* first macro/text in a line */
#define	MDOC_PHRASELIT	 (1 << 4) /* literal within a partila phrase */
#define	MDOC_PPHRASE	 (1 << 5) /* within a partial phrase */
#define	MDOC_FREECOL	 (1 << 6) /* `It' invocation should close */
#define	MDOC_SYNOPSIS	 (1 << 7) /* SYNOPSIS-style formatting */
	int		  pflags;
	enum mdoc_next	  next; /* where to put the next node */
	struct mdoc_node *last; /* the last node parsed */
	struct mdoc_node *first; /* the first node parsed */
	struct mdoc_meta  meta; /* document meta-data */
	enum mdoc_sec	  lastnamed;
	enum mdoc_sec	  lastsec;
	struct regset	 *regs; /* registers */
};

#define	MACRO_PROT_ARGS	struct mdoc *m, \
			enum mdoct tok, \
			int line, \
			int ppos, \
			int *pos, \
			char *buf

struct	mdoc_macro {
	int		(*fp)(MACRO_PROT_ARGS);
	int		  flags;
#define	MDOC_CALLABLE	 (1 << 0)
#define	MDOC_PARSED	 (1 << 1)
#define	MDOC_EXPLICIT	 (1 << 2)
#define	MDOC_PROLOGUE	 (1 << 3)
#define	MDOC_IGNDELIM	 (1 << 4) 
	/* Reserved words in arguments treated as text. */
};

enum	margserr {
	ARGS_ERROR,
	ARGS_EOLN,
	ARGS_WORD,
	ARGS_PUNCT,
	ARGS_QWORD,
	ARGS_PHRASE,
	ARGS_PPHRASE,
	ARGS_PEND
};

enum	margverr {
	ARGV_ERROR,
	ARGV_EOLN,
	ARGV_ARG,
	ARGV_WORD
};

enum	mdelim {
	DELIM_NONE = 0,
	DELIM_OPEN,
	DELIM_MIDDLE,
	DELIM_CLOSE
};

extern	const struct mdoc_macro *const mdoc_macros;

__BEGIN_DECLS

#define		  mdoc_pmsg(m, l, p, t) \
		  (*(m)->msg)((t), (m)->data, (l), (p), NULL)
#define		  mdoc_nmsg(m, n, t) \
		  (*(m)->msg)((t), (m)->data, (n)->line, (n)->pos, NULL)
int		  mdoc_vmsg(struct mdoc *, enum mandocerr, 
			int, int, const char *, ...);
int		  mdoc_macro(MACRO_PROT_ARGS);
int		  mdoc_word_alloc(struct mdoc *, 
			int, int, const char *);
int		  mdoc_elem_alloc(struct mdoc *, int, int, 
			enum mdoct, struct mdoc_arg *);
int		  mdoc_block_alloc(struct mdoc *, int, int, 
			enum mdoct, struct mdoc_arg *);
int		  mdoc_head_alloc(struct mdoc *, int, int, enum mdoct);
int		  mdoc_tail_alloc(struct mdoc *, int, int, enum mdoct);
int		  mdoc_body_alloc(struct mdoc *, int, int, enum mdoct);
int		  mdoc_endbody_alloc(struct mdoc *m, int line, int pos,
			enum mdoct tok, struct mdoc_node *body,
			enum mdoc_endbody end);
void		  mdoc_node_delete(struct mdoc *, struct mdoc_node *);
void		  mdoc_hash_init(void);
enum mdoct	  mdoc_hash_find(const char *);
enum mdelim	  mdoc_iscdelim(char);
enum mdelim	  mdoc_isdelim(const char *);
size_t		  mdoc_isescape(const char *);
enum	mdoc_sec  mdoc_str2sec(const char *);
time_t		  mdoc_atotime(const char *);
size_t		  mdoc_macro2len(enum mdoct);
const char	 *mdoc_a2att(const char *);
const char	 *mdoc_a2lib(const char *);
const char	 *mdoc_a2st(const char *);
const char	 *mdoc_a2arch(const char *);
const char	 *mdoc_a2vol(const char *);
const char	 *mdoc_a2msec(const char *);
int		  mdoc_valid_pre(struct mdoc *, struct mdoc_node *);
int		  mdoc_valid_post(struct mdoc *);
int		  mdoc_action_pre(struct mdoc *, 
			struct mdoc_node *);
int		  mdoc_action_post(struct mdoc *);
enum margverr	  mdoc_argv(struct mdoc *, int, enum mdoct,
			struct mdoc_arg **, int *, char *);
void		  mdoc_argv_free(struct mdoc_arg *);
void		  mdoc_argn_free(struct mdoc_arg *, int);
enum margserr	  mdoc_args(struct mdoc *, int,
			int *, char *, enum mdoct, char **);
enum margserr	  mdoc_zargs(struct mdoc *, int, 
			int *, char *, int, char **);
#define	ARGS_DELIM	(1 << 1)
#define	ARGS_TABSEP	(1 << 2)
#define	ARGS_NOWARN	(1 << 3)

int		  mdoc_macroend(struct mdoc *);

__END_DECLS

#endif /*!LIBMDOC_H*/
