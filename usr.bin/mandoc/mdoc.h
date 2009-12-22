/*	$Id: mdoc.h,v 1.16 2009/12/22 23:58:00 schwarze Exp $ */
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

#define	MDOC_Ap		 0
#define	MDOC_Dd		 1
#define	MDOC_Dt		 2
#define	MDOC_Os		 3
#define	MDOC_Sh		 4
#define	MDOC_Ss		 5
#define	MDOC_Pp		 6
#define	MDOC_D1		 7
#define	MDOC_Dl		 8
#define	MDOC_Bd		 9
#define	MDOC_Ed		 10
#define	MDOC_Bl		 11
#define	MDOC_El		 12
#define	MDOC_It		 13
#define	MDOC_Ad		 14
#define	MDOC_An		 15
#define	MDOC_Ar		 16
#define	MDOC_Cd		 17
#define	MDOC_Cm		 18
#define	MDOC_Dv		 19
#define	MDOC_Er		 20
#define	MDOC_Ev		 21
#define	MDOC_Ex		 22
#define	MDOC_Fa		 23
#define	MDOC_Fd		 24
#define	MDOC_Fl		 25
#define	MDOC_Fn		 26
#define	MDOC_Ft		 27
#define	MDOC_Ic		 28
#define	MDOC_In		 29
#define	MDOC_Li		 30
#define	MDOC_Nd		 31
#define	MDOC_Nm		 32
#define	MDOC_Op		 33
#define	MDOC_Ot		 34
#define	MDOC_Pa		 35
#define	MDOC_Rv		 36
#define	MDOC_St		 37
#define	MDOC_Va		 38
#define	MDOC_Vt		 39
#define	MDOC_Xr		 40
#define	MDOC__A		 41
#define	MDOC__B		 42
#define	MDOC__D		 43
#define	MDOC__I		 44
#define	MDOC__J		 45
#define	MDOC__N		 46
#define	MDOC__O		 47
#define	MDOC__P		 48
#define	MDOC__R		 49
#define	MDOC__T		 50
#define	MDOC__V		 51
#define MDOC_Ac		 52
#define MDOC_Ao		 53
#define MDOC_Aq		 54
#define MDOC_At		 55
#define MDOC_Bc		 56
#define MDOC_Bf		 57
#define MDOC_Bo		 58
#define MDOC_Bq		 59
#define MDOC_Bsx	 60
#define MDOC_Bx		 61
#define MDOC_Db		 62
#define MDOC_Dc		 63
#define MDOC_Do		 64
#define MDOC_Dq		 65
#define MDOC_Ec		 66
#define MDOC_Ef		 67
#define MDOC_Em		 68
#define MDOC_Eo		 69
#define MDOC_Fx		 70
#define MDOC_Ms		 71
#define MDOC_No		 72
#define MDOC_Ns		 73
#define MDOC_Nx		 74
#define MDOC_Ox		 75
#define MDOC_Pc		 76
#define MDOC_Pf		 77
#define MDOC_Po		 78
#define MDOC_Pq		 79
#define MDOC_Qc		 80
#define MDOC_Ql		 81
#define MDOC_Qo		 82
#define MDOC_Qq		 83
#define MDOC_Re		 84
#define MDOC_Rs		 85
#define MDOC_Sc		 86
#define MDOC_So		 87
#define MDOC_Sq		 88
#define MDOC_Sm		 89
#define MDOC_Sx		 90
#define MDOC_Sy		 91
#define MDOC_Tn		 92
#define MDOC_Ux		 93
#define MDOC_Xc		 94
#define MDOC_Xo		 95
#define	MDOC_Fo		 96
#define	MDOC_Fc		 97
#define	MDOC_Oo		 98
#define	MDOC_Oc		 99
#define	MDOC_Bk		 100
#define	MDOC_Ek		 101
#define	MDOC_Bt		 102
#define	MDOC_Hf		 103
#define	MDOC_Fr		 104
#define	MDOC_Ud		 105
#define	MDOC_Lb		 106
#define	MDOC_Lp		 107
#define	MDOC_Lk		 108
#define	MDOC_Mt		 109
#define	MDOC_Brq	 110
#define	MDOC_Bro	 111
#define	MDOC_Brc	 112
#define	MDOC__C	 	 113
#define	MDOC_Es	 	 114
#define	MDOC_En	 	 115
#define	MDOC_Dx	 	 116
#define	MDOC__Q	 	 117
#define MDOC_br		 118
#define MDOC_sp		 119
#define MDOC__U		 120
#define	MDOC_MAX	 121

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
	SEC_EXIT_STATUS,
	SEC_RETURN_VALUES,
	SEC_ENVIRONMENT, 
	SEC_FILES,
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
	SEC_CUSTOM		/* User-defined. */
};

/* Information from prologue. */
struct	mdoc_meta {
	int		  msec;
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

/* Node in AST. */
struct	mdoc_node {
	struct mdoc_node *parent;
	struct mdoc_node *child;
	struct mdoc_node *next;
	struct mdoc_node *prev;
	int		  nchild;
	int		  line;
	int		  pos;
	int		  tok;
	int		  flags;
#define	MDOC_VALID	 (1 << 0)
#define	MDOC_ACTED	 (1 << 1)
	enum mdoc_type	  type;
	enum mdoc_sec	  sec;

	struct mdoc_arg	 *args; 	/* BLOCK/ELEM */
	struct mdoc_node *head;		/* BLOCK */
	struct mdoc_node *body;		/* BLOCK */
	struct mdoc_node *tail;		/* BLOCK */
	char		 *string;	/* TEXT */
};

#define	MDOC_IGN_SCOPE	 (1 << 0) /* Ignore scope violations. */
#define	MDOC_IGN_ESCAPE	 (1 << 1) /* Ignore bad escape sequences. */
#define	MDOC_IGN_MACRO	 (1 << 2) /* Ignore unknown macros. */
#define	MDOC_IGN_CHARS	 (1 << 3) /* Ignore disallowed chars. */

/* Call-backs for parse messages. */

struct	mdoc_cb {
	int	(*mdoc_err)(void *, int, int, const char *);
	int	(*mdoc_warn)(void *, int, int, const char *);
};

/* See mdoc.3 for documentation. */

extern	const char *const *mdoc_macronames;
extern	const char *const *mdoc_argnames;

__BEGIN_DECLS

struct	mdoc;

/* See mdoc.3 for documentation. */

void	 	  mdoc_free(struct mdoc *);
struct	mdoc	 *mdoc_alloc(void *, int, const struct mdoc_cb *);
void		  mdoc_reset(struct mdoc *);
int	 	  mdoc_parseln(struct mdoc *, int, char *buf);
const struct mdoc_node *mdoc_node(const struct mdoc *);
const struct mdoc_meta *mdoc_meta(const struct mdoc *);
int		  mdoc_endparse(struct mdoc *);

__END_DECLS

#endif /*!MDOC_H*/
