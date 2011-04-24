/*	$Id: libmandoc.h,v 1.11 2011/04/24 16:22:02 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef LIBMANDOC_H
#define LIBMANDOC_H

enum	rofferr {
	ROFF_CONT, /* continue processing line */
	ROFF_RERUN, /* re-run roff interpreter with offset */
	ROFF_APPEND, /* re-run main parser, appending next line */
	ROFF_REPARSE, /* re-run main parser on the result */
	ROFF_SO, /* include another file */
	ROFF_IGN, /* ignore current line */
	ROFF_TBL, /* a table row was successfully parsed */
	ROFF_EQN, /* an equation was successfully parsed */
	ROFF_ERR /* badness: puke and stop */
};

enum	regs {
	REG_nS = 0, /* nS register */
	REG__MAX
};

/*
 * A register (struct reg) can consist of many types: this consists of
 * normalised types from the original string form.  For the time being,
 * there's only an unsigned integer type.
 */
union	regval {
	unsigned  u; /* unsigned integer */
};

/*
 * A single register entity.  If "set" is zero, the value of the
 * register should be the default one, which is per-register.  It's
 * assumed that callers know which type in "v" corresponds to which
 * register value.
 */
struct	reg {
	int		  set; /* whether set or not */
	union regval	  v; /* parsed data */
};

/*
 * The primary interface to setting register values is in libroff,
 * although libmdoc and libman from time to time will manipulate
 * registers (such as `.Sh SYNOPSIS' enabling REG_nS).
 */
struct	regset {
	struct reg	  regs[REG__MAX];
};

__BEGIN_DECLS

struct	roff;
struct	mdoc;
struct	man;

void		 mandoc_msg(enum mandocerr, struct mparse *, 
			int, int, const char *);
void		 mandoc_vmsg(enum mandocerr, struct mparse *, 
			int, int, const char *, ...);
int		 mandoc_special(char *);
char		*mandoc_strdup(const char *);
char		*mandoc_getarg(struct mparse *, char **, int, int *);
char		*mandoc_normdate(struct mparse *, char *, int, int);
int		 mandoc_eos(const char *, size_t, int);
int		 mandoc_hyph(const char *, const char *);
int		 mandoc_getcontrol(const char *, int *);

void	 	 mdoc_free(struct mdoc *);
struct	mdoc	*mdoc_alloc(struct regset *, struct mparse *);
void		 mdoc_reset(struct mdoc *);
int	 	 mdoc_parseln(struct mdoc *, int, char *, int);
int		 mdoc_endparse(struct mdoc *);
int		 mdoc_addspan(struct mdoc *, const struct tbl_span *);
int		 mdoc_addeqn(struct mdoc *, const struct eqn *);

void	 	 man_free(struct man *);
struct	man	*man_alloc(struct regset *, struct mparse *);
void		 man_reset(struct man *);
int	 	 man_parseln(struct man *, int, char *, int);
int		 man_endparse(struct man *);
int		 man_addspan(struct man *, const struct tbl_span *);
int		 man_addeqn(struct man *, const struct eqn *);

void	 	 roff_free(struct roff *);
struct roff	*roff_alloc(struct regset *, struct mparse *);
void		 roff_reset(struct roff *);
enum rofferr	 roff_parseln(struct roff *, int, 
			char **, size_t *, int, int *);
void		 roff_endparse(struct roff *);

const struct tbl_span	*roff_span(const struct roff *);
const struct eqn	*roff_eqn(const struct roff *);

__END_DECLS

#endif /*!LIBMANDOC_H*/
