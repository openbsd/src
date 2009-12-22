/*	$Id: libmdoc.h,v 1.24 2009/12/22 23:58:00 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
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
	void		 *data;
	struct mdoc_cb	  cb;
	int		  flags;
#define	MDOC_HALT	 (1 << 0)	/* Error in parse. Halt. */
#define	MDOC_LITERAL	 (1 << 1)	/* In a literal scope. */
#define	MDOC_PBODY	 (1 << 2)	/* In the document body. */
	int		  pflags;
	enum mdoc_next	  next;
	struct mdoc_node *last;
	struct mdoc_node *first;
	struct mdoc_meta  meta;
	enum mdoc_sec	  lastnamed;
	enum mdoc_sec	  lastsec;
};

enum	merr {
	ETAILWS = 0,
	EQUOTPARM,
	EQUOTTERM,
	EARGVAL,	
	EBODYPROL,
	EPROLBODY,
	ETEXTPROL,
	ENOBLANK,
	ETOOLONG,
	EESCAPE,
	EPRINT,
	ENODAT,
	ENOPROLOGUE,
	ELINE,
	EATT,
	ENAME,
	ELISTTYPE,
	EDISPTYPE,
	EMULTIDISP,
	EMULTILIST,
	ESECNAME,
	ENAMESECINC,
	EARGREP,
	EBOOL,
	ECOLMIS,
	ENESTDISP,
	EMISSWIDTH,
	EWRONGMSEC,
	ESECOOO,
	ESECREP,
	EBADSTAND,
	ENOMULTILINE,
	EMULTILINE,
	ENOLINE,
	EPROLOOO,
	EPROLREP,
	EBADMSEC,
	EBADSEC,
	EFONT,
	EBADDATE,
	ENUMFMT,
	ENOWIDTH,
	EUTSNAME,
	EOBS,
	EIMPBRK,
	EIGNE,
	EOPEN,
	EQUOTPHR,
	ENOCTX,
	ELIB,
	EBADCHILD,
	ENOTYPE,
	MERRMAX
};

#define	MACRO_PROT_ARGS	struct mdoc *m, int tok, int line, \
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

extern	const struct mdoc_macro *const mdoc_macros;

__BEGIN_DECLS

#define		  mdoc_perr(m, l, p, t) \
		  mdoc_err((m), (l), (p), 1, (t))
#define		  mdoc_pwarn(m, l, p, t) \
		  mdoc_err((m), (l), (p), 0, (t))
#define		  mdoc_nerr(m, n, t) \
		  mdoc_err((m), (n)->line, (n)->pos, 1, (t))
#define		  mdoc_nwarn(m, n, t) \
		  mdoc_err((m), (n)->line, (n)->pos, 0, (t))

int		  mdoc_err(struct mdoc *, int, int, int, enum merr);
int		  mdoc_verr(struct mdoc *, int, int, const char *, ...);
int		  mdoc_vwarn(struct mdoc *, int, int, const char *, ...);

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
void		  mdoc_hash_init(void);
int		  mdoc_hash_find(const char *);
int		  mdoc_iscdelim(char);
int		  mdoc_isdelim(const char *);
size_t		  mdoc_isescape(const char *);
enum	mdoc_sec  mdoc_atosec(const char *);
time_t		  mdoc_atotime(const char *);

size_t		  mdoc_macro2len(int);
const char	 *mdoc_a2att(const char *);
const char	 *mdoc_a2lib(const char *);
const char	 *mdoc_a2st(const char *);
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
int		  mdoc_zargs(struct mdoc *, int, 
			int *, char *, int, char **);
#define	ARGS_DELIM	(1 << 1)	/* See args(). */
#define	ARGS_TABSEP	(1 << 2)	/* See args(). */
#define	ARGS_NOWARN	(1 << 3)	/* See args(). */
#define	ARGS_ERROR	(-1)
#define	ARGS_EOLN	(0)
#define	ARGS_WORD	(1)
#define	ARGS_PUNCT	(2)
#define	ARGS_QWORD	(3)
#define	ARGS_PHRASE	(4)
int		  mdoc_macroend(struct mdoc *);

__END_DECLS

#endif /*!LIBMDOC_H*/
