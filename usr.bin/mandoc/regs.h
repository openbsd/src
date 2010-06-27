/*	$Id: regs.h,v 1.1 2010/06/27 21:58:57 schwarze Exp $ */
/*
 * Copyright (c) 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef REGS_H
#define REGS_H

__BEGIN_DECLS

enum	regs {
	REG_nS = 0,	/* nS */
	REG__MAX
};

struct	reg {
	int		 set; /* whether set or not */
	union {
		unsigned u; /* unsigned integer */
	} v;
};

/*
 * Registers are non-scoped state.  These can be manipulated directly in
 * libroff or indirectly in libman or libmdoc by macros.  These should
 * be implemented sparingly (we are NOT roffdoc!) and documented fully
 * in roff.7.
 */
struct	regset {
	struct reg	 regs[REG__MAX];
};

__END_DECLS

#endif /*!REGS_H*/
