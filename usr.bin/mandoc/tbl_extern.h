/*	$Id: tbl_extern.h,v 1.3 2010/10/15 22:07:12 schwarze Exp $ */
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
#ifndef TBL_EXTERN_H
#define TBL_EXTERN_H

enum	tbl_err {
	ERR_SYNTAX,
	ERR_OPTION,
	ERR_MAX
};

enum	tbl_tok {
	TBL_TOK_WORD,
	TBL_TOK_OPENPAREN,
	TBL_TOK_CLOSEPAREN,
	TBL_TOK_COMMA,
	TBL_TOK_SEMICOLON,
	TBL_TOK_PERIOD,
	TBL_TOK_SPACE,
	TBL_TOK_TAB,
	TBL_TOK_NIL
};

enum	tbl_part {
	TBL_PART_OPTS,
	TBL_PART_LAYOUT,
	TBL_PART_CLAYOUT,
	TBL_PART_DATA,
	TBL_PART_ERROR
};

struct	tbl;
struct	tbl_head;
struct	tbl_row;
struct	tbl_cell;
struct	tbl_span;
struct	tbl_data;

TAILQ_HEAD(tbl_rowh, tbl_row);
TAILQ_HEAD(tbl_cellh, tbl_cell);
TAILQ_HEAD(tbl_headh, tbl_head);
TAILQ_HEAD(tbl_spanh, tbl_span);
TAILQ_HEAD(tbl_datah, tbl_data);

struct	tbl {
	enum tbl_part	 	 part;
	int		 	 opts;
#define	TBL_OPT_CENTRE		(1 << 0)
#define	TBL_OPT_EXPAND		(1 << 1)
#define	TBL_OPT_BOX		(1 << 2)
#define	TBL_OPT_DBOX		(1 << 3)
#define	TBL_OPT_ALLBOX		(1 << 4)
#define	TBL_OPT_NOKEEP		(1 << 5)
#define	TBL_OPT_NOSPACE		(1 << 6)
	char		 	 tab;
	char		 	 decimal;
	int		 	 linesize;
	char		 	 delims[2];
	struct tbl_spanh	 span;
	struct tbl_headh	 head;
	struct tbl_rowh		 row;
};

enum	tbl_headt {
	TBL_HEAD_DATA,
	TBL_HEAD_VERT,
	TBL_HEAD_DVERT,
	TBL_HEAD_MAX
};

struct	tbl_head {
	struct tbl		*tbl;
	enum tbl_headt	 	 pos;
	int			 width;
	int			 decimal;
	TAILQ_ENTRY(tbl_head)	 entries;
};

struct	tbl_row {
	struct tbl		*tbl;
	struct tbl_cellh	 cell;
	TAILQ_ENTRY(tbl_row)	 entries;
};

enum	tbl_cellt {
	TBL_CELL_CENTRE,	/* c, C */
	TBL_CELL_RIGHT,		/* r, R */
	TBL_CELL_LEFT,		/* l, L */
	TBL_CELL_NUMBER,	/* n, N */
	TBL_CELL_SPAN,		/* s, S */
	TBL_CELL_LONG,		/* a, A */
	TBL_CELL_DOWN,		/* ^ */
	TBL_CELL_HORIZ,		/* _, - */
	TBL_CELL_DHORIZ,	/* = */
	TBL_CELL_VERT,		/* | */
	TBL_CELL_DVERT,		/* || */
	TBL_CELL_MAX
};

struct	tbl_cell {
	struct tbl_row		*row;
	struct tbl_head		*head;
	enum tbl_cellt	 	 pos;
	int		  	 spacing;
	int		 	 flags;
#define	TBL_CELL_TALIGN		(1 << 0)	/* t, T */
#define	TBL_CELL_BALIGN		(1 << 1)	/* d, D */
#define	TBL_CELL_BOLD		(1 << 2)	/* fB, B, b */
#define	TBL_CELL_ITALIC		(1 << 3)	/* fI, I, i */
#define	TBL_CELL_EQUAL		(1 << 4)	/* e, E */
#define	TBL_CELL_UP		(1 << 5)	/* u, U */
#define	TBL_CELL_WIGN		(1 << 6)	/* z, Z */
	TAILQ_ENTRY(tbl_cell)	 entries;
};

struct	tbl_data {
	struct tbl_span		*span;
	struct tbl_cell		*cell;
	int		 	 flags;
#define	TBL_DATA_HORIZ		(1 << 0)
#define	TBL_DATA_DHORIZ		(1 << 1)
#define	TBL_DATA_NHORIZ		(1 << 2)
#define	TBL_DATA_NDHORIZ 	(1 << 3)
	char			*string;
	TAILQ_ENTRY(tbl_data)	 entries;
};

struct	tbl_span {
	struct tbl_row		*row;
	struct tbl		*tbl;
	int		 	 flags;
#define	TBL_SPAN_HORIZ		(1 << 0)
#define	TBL_SPAN_DHORIZ		(1 << 1)
	struct tbl_datah	 data;
	TAILQ_ENTRY(tbl_span)	 entries;
};

__BEGIN_DECLS

int	 	 tbl_option(struct tbl *, 
			const char *, int, const char *);
int	 	 tbl_layout(struct tbl *, 
			const char *, int, const char *);
int	 	 tbl_data(struct tbl *, 
			const char *, int, const char *);
int		 tbl_data_close(struct tbl *,  const char *, int);

enum tbl_tok	 tbl_next(const char *, int *);
const char	*tbl_last(void);
int		 tbl_last_uint(void);
int		 tbl_errx(struct tbl *, enum tbl_err, 
			const char *, int, int);
int		 tbl_warnx(struct tbl *, enum tbl_err, 
			const char *, int, int);
int		 tbl_err(struct tbl *);

struct tbl_row	*tbl_row_alloc(struct tbl *);
struct tbl_cell	*tbl_cell_alloc(struct tbl_row *, enum tbl_cellt);
struct tbl_span	*tbl_span_alloc(struct tbl *);
struct tbl_data	*tbl_data_alloc(struct tbl_span *);

int		 tbl_write_term(struct termp *, const struct tbl *);
int		 tbl_calc_term(struct termp *, struct tbl *);
int		 tbl_write_tree(const struct tbl *);
int		 tbl_calc_tree(struct tbl *);

__END_DECLS

#endif /*TBL_EXTERN_H*/
