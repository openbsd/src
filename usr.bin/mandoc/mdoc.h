/*	$Id: mdoc.h,v 1.26 2010/05/24 00:00:10 schwarze Exp $ */
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
#ifndef MDOC_H
#define MDOC_H

/*
 * This library implements a validating scanner/parser for ``mdoc'' roff
 * macro documents, a.k.a. BSD manual page documents.  The mdoc.c file
 * drives the parser, while macro.c describes the macro ontologies.
 * validate.c pre- and post-validates parsed macros, and action.c
 * performs actions on parsed and validated macros.
 */

/* What follows is a list of ALL possible macros. */

enum	mdoct {
	MDOC_Ap = 0,
	MDOC_Dd,
	MDOC_Dt,
	MDOC_Os,
	MDOC_Sh,
	MDOC_Ss,
	MDOC_Pp,
	MDOC_D1,
	MDOC_Dl,
	MDOC_Bd,
	MDOC_Ed,
	MDOC_Bl,
	MDOC_El,
	MDOC_It,
	MDOC_Ad,
	MDOC_An,
	MDOC_Ar,
	MDOC_Cd,
	MDOC_Cm,
	MDOC_Dv,
	MDOC_Er,
	MDOC_Ev,
	MDOC_Ex,
	MDOC_Fa,
	MDOC_Fd,
	MDOC_Fl,
	MDOC_Fn,
	MDOC_Ft,
	MDOC_Ic,
	MDOC_In,
	MDOC_Li,
	MDOC_Nd,
	MDOC_Nm,
	MDOC_Op,
	MDOC_Ot,
	MDOC_Pa,
	MDOC_Rv,
	MDOC_St,
	MDOC_Va,
	MDOC_Vt,
	MDOC_Xr,
	MDOC__A,
	MDOC__B,
	MDOC__D,
	MDOC__I,
	MDOC__J,
	MDOC__N,
	MDOC__O,
	MDOC__P,
	MDOC__R,
	MDOC__T,
	MDOC__V,
	MDOC_Ac,
	MDOC_Ao,
	MDOC_Aq,
	MDOC_At,
	MDOC_Bc,
	MDOC_Bf,
	MDOC_Bo,
	MDOC_Bq,
	MDOC_Bsx,
	MDOC_Bx,
	MDOC_Db,
	MDOC_Dc,
	MDOC_Do,
	MDOC_Dq,
	MDOC_Ec,
	MDOC_Ef,
	MDOC_Em,
	MDOC_Eo,
	MDOC_Fx,
	MDOC_Ms,
	MDOC_No,
	MDOC_Ns,
	MDOC_Nx,
	MDOC_Ox,
	MDOC_Pc,
	MDOC_Pf,
	MDOC_Po,
	MDOC_Pq,
	MDOC_Qc,
	MDOC_Ql,
	MDOC_Qo,
	MDOC_Qq,
	MDOC_Re,
	MDOC_Rs,
	MDOC_Sc,
	MDOC_So,
	MDOC_Sq,
	MDOC_Sm,
	MDOC_Sx,
	MDOC_Sy,
	MDOC_Tn,
	MDOC_Ux,
	MDOC_Xc,
	MDOC_Xo,
	MDOC_Fo,
	MDOC_Fc,
	MDOC_Oo,
	MDOC_Oc,
	MDOC_Bk,
	MDOC_Ek,
	MDOC_Bt,
	MDOC_Hf,
	MDOC_Fr,
	MDOC_Ud,
	MDOC_Lb,
	MDOC_Lp,
	MDOC_Lk,
	MDOC_Mt,
	MDOC_Brq,
	MDOC_Bro,
	MDOC_Brc,
	MDOC__C,
	MDOC_Es,
	MDOC_En,
	MDOC_Dx,
	MDOC__Q,
	MDOC_br,
	MDOC_sp,
	MDOC__U,
	MDOC_MAX
};

/* What follows is a list of ALL possible macro arguments. */

#define	MDOC_Split	 0
#define	MDOC_Nosplit	 1
#define	MDOC_Ragged	 2
#define	MDOC_Unfilled	 3
#define	MDOC_Literal	 4
#define	MDOC_File	 5
#define	MDOC_Offset	 6
#define	MDOC_Bullet	 7
#define	MDOC_Dash	 8
#define	MDOC_Hyphen	 9
#define	MDOC_Item	 10
#define	MDOC_Enum	 11
#define	MDOC_Tag	 12
#define	MDOC_Diag	 13
#define	MDOC_Hang	 14
#define	MDOC_Ohang	 15
#define	MDOC_Inset	 16
#define	MDOC_Column	 17
#define	MDOC_Width	 18
#define	MDOC_Compact	 19
#define	MDOC_Std	 20
#define	MDOC_Filled	 21
#define	MDOC_Words	 22
#define	MDOC_Emphasis	 23
#define	MDOC_Symbolic	 24
#define	MDOC_Nested	 25
#define	MDOC_Centred	 26
#define	MDOC_ARG_MAX	 27

/* Type of a syntax node. */
enum	mdoc_type {
	MDOC_TEXT,
	MDOC_ELEM,
	MDOC_HEAD,
	MDOC_TAIL,
	MDOC_BODY,
	MDOC_BLOCK,
	MDOC_ROOT
};

/* Section (named/unnamed) of `Sh'. */
enum	mdoc_sec {
	SEC_NONE,		/* No section, yet. */
	SEC_NAME,
	SEC_LIBRARY,
	SEC_SYNOPSIS,
	SEC_DESCRIPTION,
	SEC_IMPLEMENTATION,
	SEC_RETURN_VALUES,
	SEC_ENVIRONMENT, 
	SEC_FILES,
	SEC_EXIT_STATUS,
	SEC_EXAMPLES,
	SEC_DIAGNOSTICS,
	SEC_COMPATIBILITY,
	SEC_ERRORS,
	SEC_SEE_ALSO,
	SEC_STANDARDS,
	SEC_HISTORY,
	SEC_AUTHORS,
	SEC_CAVEATS,
	SEC_BUGS,
	SEC_SECURITY,
	SEC_CUSTOM,		/* User-defined. */
	SEC__MAX
};

/* Information from prologue. */
struct	mdoc_meta {
	char		 *msec;
	char		 *vol;
	char		 *arch;
	time_t		  date;
	char		 *title;
	char		 *os;
	char		 *name;
};

/* An argument to a macro (multiple values = `It -column'). */
struct	mdoc_argv {
	int	  	  arg;
	int		  line;
	int		  pos;
	size_t		  sz;
	char		**value;
};

struct 	mdoc_arg {
	size_t		  argc;
	struct mdoc_argv *argv;
	unsigned int	  refcnt;
};

enum	mdoc_list {
	LIST__NONE = 0,
	LIST_bullet,
	LIST_column,
	LIST_dash,
	LIST_diag,
	LIST_enum,
	LIST_hang,
	LIST_hyphen,
	LIST_inset,
	LIST_item,
	LIST_ohang,
	LIST_tag
};

/* Node in AST. */
struct	mdoc_node {
	struct mdoc_node *parent; /* parent AST node */
	struct mdoc_node *child; /* first child AST node */
	struct mdoc_node *next; /* sibling AST node */
	struct mdoc_node *prev; /* prior sibling AST node */
	int		  nchild; /* number children */
	int		  line; /* parse line */
	int		  pos; /* parse column */
	enum mdoct	  tok; /* tok or MDOC__MAX if none */
	int		  flags;
#define	MDOC_VALID	 (1 << 0) /* has been validated */
#define	MDOC_ACTED	 (1 << 1) /* has been acted upon */
#define	MDOC_EOS	 (1 << 2) /* at sentence boundary */
#define	MDOC_LINE	 (1 << 3) /* first macro/text on line */
	enum mdoc_type	  type; /* AST node type */
	enum mdoc_sec	  sec; /* current named section */
	struct mdoc_arg	 *args; 	/* BLOCK/ELEM */
	struct mdoc_node *pending;	/* BLOCK */
	struct mdoc_node *head;		/* BLOCK */
	struct mdoc_node *body;		/* BLOCK */
	struct mdoc_node *tail;		/* BLOCK */
	char		 *string;	/* TEXT */

	union {
		enum mdoc_list list; /* for `Bl' nodes */
	} data;
};

#define	MDOC_IGN_SCOPE	 (1 << 0) /* Ignore scope violations. */
#define	MDOC_IGN_ESCAPE	 (1 << 1) /* Ignore bad escape sequences. */
#define	MDOC_IGN_MACRO	 (1 << 2) /* Ignore unknown macros. */

/* See mdoc.3 for documentation. */

extern	const char *const *mdoc_macronames;
extern	const char *const *mdoc_argnames;

__BEGIN_DECLS

struct	mdoc;

/* See mdoc.3 for documentation. */

void	 	  mdoc_free(struct mdoc *);
struct	mdoc	 *mdoc_alloc(void *, int, mandocmsg);
void		  mdoc_reset(struct mdoc *);
int	 	  mdoc_parseln(struct mdoc *, int, char *, int);
const struct mdoc_node *mdoc_node(const struct mdoc *);
const struct mdoc_meta *mdoc_meta(const struct mdoc *);
int		  mdoc_endparse(struct mdoc *);

__END_DECLS

#endif /*!MDOC_H*/
