/*	$OpenBSD: roff.h,v 1.12 2015/04/02 23:47:43 schwarze Exp $	*/
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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

struct	mdoc_arg;
union	mdoc_data;

enum	roff_sec {
	SEC_NONE = 0,
	SEC_NAME,
	SEC_LIBRARY,
	SEC_SYNOPSIS,
	SEC_DESCRIPTION,
	SEC_CONTEXT,
	SEC_IMPLEMENTATION,	/* IMPLEMENTATION NOTES */
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
	SEC_CUSTOM,
	SEC__MAX
};

enum	roff_type {
	ROFFT_ROOT,
	ROFFT_BLOCK,
	ROFFT_HEAD,
	ROFFT_BODY,
	ROFFT_TAIL,
	ROFFT_ELEM,
	ROFFT_TEXT,
	ROFFT_TBL,
	ROFFT_EQN
};

/*
 * Indicates that a BODY's formatting has ended, but
 * the scope is still open.  Used for badly nested blocks.
 */
enum	mdoc_endbody {
	ENDBODY_NOT = 0,
	ENDBODY_SPACE,	/* Is broken: append a space. */
	ENDBODY_NOSPACE	/* Is broken: don't append a space. */
};

struct	roff_node {
	struct roff_node *parent;  /* Parent AST node. */
	struct roff_node *child;   /* First child AST node. */
	struct roff_node *last;    /* Last child AST node. */
	struct roff_node *next;    /* Sibling AST node. */
	struct roff_node *prev;    /* Prior sibling AST node. */
	struct roff_node *head;    /* BLOCK */
	struct roff_node *body;    /* BLOCK/ENDBODY */
	struct roff_node *tail;    /* BLOCK */
	struct mdoc_arg	 *args;    /* BLOCK/ELEM */
	union mdoc_data	 *norm;    /* Normalized arguments. */
	char		 *string;  /* TEXT */
	const struct tbl_span *span; /* TBL */
	const struct eqn *eqn;	   /* EQN */
	int		  nchild;  /* Number of child nodes. */
	int		  line;    /* Input file line number. */
	int		  pos;     /* Input file column number. */
	int		  tok;     /* Request or macro ID. */
	int		  flags;
#define	MDOC_VALID	 (1 << 0)  /* Has been validated. */
#define	MDOC_ENDED	 (1 << 1)  /* Gone past body end mark. */
#define MDOC_EOS	 (1 << 2)  /* At sentence boundary. */
#define	MDOC_LINE	 (1 << 3)  /* First macro/text on line. */
#define MDOC_SYNPRETTY	 (1 << 4)  /* SYNOPSIS-style formatting. */
#define MDOC_BROKEN	 (1 << 5)  /* Must validate parent when ending. */
#define	MDOC_DELIMO	 (1 << 6)
#define	MDOC_DELIMC	 (1 << 7)
#define	MAN_VALID	  MDOC_VALID
#define	MAN_EOS		  MDOC_EOS
#define	MAN_LINE	  MDOC_LINE
	int		  prev_font; /* Before entering this node. */
	int		  aux;     /* Decoded node data, type-dependent. */
	enum roff_type	  type;    /* AST node type. */
	enum roff_sec	  sec;     /* Current named section. */
	enum mdoc_endbody end;     /* BODY */
};

struct	roff_meta {
	char		 *msec;    /* Manual section, usually a digit. */
	char		 *vol;     /* Manual volume title. */
	char		 *os;      /* Operating system. */
	char		 *arch;    /* Machine architecture. */
	char		 *title;   /* Manual title, usually CAPS. */
	char		 *name;    /* Leading manual name. */
	char		 *date;    /* Normalized date. */
	int		  hasbody; /* Document is not empty. */
};
