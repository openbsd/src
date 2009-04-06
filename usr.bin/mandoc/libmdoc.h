/* $Id: libmdoc.h,v 1.1 2009/04/06 20:30:40 kristaps Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@openbsd.org>
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
#ifndef LIBMDOC_H
#define LIBMDOC_H

#include "mdoc.h"

enum	mdoc_next {
	MDOC_NEXT_SIBLING = 0,
	MDOC_NEXT_CHILD
};

struct	mdoc {
	void		 *data;
	struct mdoc_cb	  cb;
	void		 *htab;
	int		  flags;
#define	MDOC_HALT	 (1 << 0)
#define	MDOC_LITERAL	 (1 << 1)
	int		  pflags;
	enum mdoc_next	  next;
	struct mdoc_node *last;
	struct mdoc_node *first;
	struct mdoc_meta  meta;
	enum mdoc_sec	  lastnamed;
	enum mdoc_sec	  lastsec;
};


#define	MACRO_PROT_ARGS	struct mdoc *mdoc, int tok, int line, \
			int ppos, int *pos, char *buf

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

#define	mdoc_nwarn(mdoc, node, type, fmt, ...) \
		  mdoc_vwarn((mdoc), (node)->line, \
		  (node)->pos, (type), (fmt), ##__VA_ARGS__)

#define	mdoc_nerr(mdoc, node, fmt, ...) \
		  mdoc_verr((mdoc), (node)->line, \
		  (node)->pos, (fmt), ##__VA_ARGS__)

#define	mdoc_warn(mdoc, type, fmt, ...) \
		  mdoc_vwarn((mdoc), (mdoc)->last->line, \
		  (mdoc)->last->pos, (type), (fmt), ##__VA_ARGS__)

#define	mdoc_err(mdoc, fmt, ...) \
		  mdoc_verr((mdoc), (mdoc)->last->line, \
		  (mdoc)->last->pos, (fmt), ##__VA_ARGS__)

#define	mdoc_msg(mdoc, fmt, ...) \
		  mdoc_vmsg((mdoc), (mdoc)->last->line, \
		  (mdoc)->last->pos, (fmt), ##__VA_ARGS__)

#define	mdoc_pmsg(mdoc, line, pos, fmt, ...) \
		  mdoc_vmsg((mdoc), (line), \
	  	  (pos), (fmt), ##__VA_ARGS__)

#define	mdoc_pwarn(mdoc, line, pos, type, fmt, ...) \
		  mdoc_vwarn((mdoc), (line), \
	  	  (pos), (type), (fmt), ##__VA_ARGS__)

#define	mdoc_perr(mdoc, line, pos, fmt, ...) \
		  mdoc_verr((mdoc), (line), \
	  	  (pos), (fmt), ##__VA_ARGS__)

extern	const struct mdoc_macro *const mdoc_macros;

__BEGIN_DECLS

int		  mdoc_vwarn(struct mdoc *, int, int, 
			enum mdoc_warn, const char *, ...);
void		  mdoc_vmsg(struct mdoc *, int, int, 
			const char *, ...);
int		  mdoc_verr(struct mdoc *, int, int, 
			const char *, ...);
int		  mdoc_macro(MACRO_PROT_ARGS);
int		  mdoc_word_alloc(struct mdoc *, 
			int, int, const char *);
int		  mdoc_elem_alloc(struct mdoc *, int, int, 
			int, struct mdoc_arg *);
int		  mdoc_block_alloc(struct mdoc *, int, int, 
			int, struct mdoc_arg *);
int		  mdoc_head_alloc(struct mdoc *, int, int, int);
int		  mdoc_tail_alloc(struct mdoc *, int, int, int);
int		  mdoc_body_alloc(struct mdoc *, int, int, int);
void		  mdoc_node_free(struct mdoc_node *);
void		  mdoc_node_freelist(struct mdoc_node *);
void		 *mdoc_hash_alloc(void);
int		  mdoc_hash_find(const void *, const char *);
void		  mdoc_hash_free(void *);
int		  mdoc_iscdelim(char);
int		  mdoc_isdelim(const char *);
size_t		  mdoc_isescape(const char *);
enum	mdoc_sec  mdoc_atosec(const char *);
time_t		  mdoc_atotime(const char *);

size_t		  mdoc_macro2len(int);
const char	 *mdoc_a2arch(const char *);
const char	 *mdoc_a2vol(const char *);
const char	 *mdoc_a2msec(const char *);
int		  mdoc_valid_pre(struct mdoc *, 
			const struct mdoc_node *);
int		  mdoc_valid_post(struct mdoc *);
int		  mdoc_action_pre(struct mdoc *, 
			const struct mdoc_node *);
int		  mdoc_action_post(struct mdoc *);
int		  mdoc_argv(struct mdoc *, int, int,
			struct mdoc_arg **, int *, char *);
#define	ARGV_ERROR	(-1)
#define	ARGV_EOLN	(0)
#define	ARGV_ARG	(1)
#define	ARGV_WORD	(2)
void		  mdoc_argv_free(struct mdoc_arg *);
int		  mdoc_args(struct mdoc *, int,
			int *, char *, int, char **);
#define	ARGS_ERROR	(-1)
#define	ARGS_EOLN	(0)
#define	ARGS_WORD	(1)
#define	ARGS_PUNCT	(2)
#define	ARGS_QWORD	(3)
#define	ARGS_PHRASE	(4)

/* FIXME: get rid of these. */
int		  xstrlcpys(char *, const struct mdoc_node *, size_t);
void	 	 *xrealloc(void *, size_t);
char	 	 *xstrdup(const char *);
int		  mdoc_macroend(struct mdoc *);

__END_DECLS

#endif /*!LIBMDOC_H*/
