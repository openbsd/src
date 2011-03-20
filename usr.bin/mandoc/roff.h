/*	$Id: roff.h,v 1.8 2011/03/20 23:36:42 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef ROFF_H
#define ROFF_H

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

__BEGIN_DECLS

struct	roff;

void	 	  roff_free(struct roff *);
struct	roff	 *roff_alloc(struct regset *, void *, mandocmsg);
void		  roff_reset(struct roff *);
enum	rofferr	  roff_parseln(struct roff *, int, 
			char **, size_t *, int, int *);
void		  roff_endparse(struct roff *);
const struct tbl_span *roff_span(const struct roff *);
const struct eqn *roff_eqn(const struct roff *);

__END_DECLS

#endif /*!ROFF_H*/
