/*	$OpenBSD: roff_term.c,v 1.2 2017/05/05 02:06:17 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2017 Ingo Schwarze <schwarze@openbsd.org>
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
#include <sys/types.h>

#include <assert.h>

#include "roff.h"
#include "out.h"
#include "term.h"

#define	ROFF_TERM_ARGS struct termp *p, const struct roff_node *n

typedef	void	(*roff_term_pre_fp)(ROFF_TERM_ARGS);

static	void	  roff_term_pre_br(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ft(ROFF_TERM_ARGS);

static	const roff_term_pre_fp roff_term_pre_acts[ROFF_MAX] = {
	roff_term_pre_br,  /* br */
	roff_term_pre_ft,  /* ft */
};


void
roff_term_pre(struct termp *p, const struct roff_node *n)
{
	assert(n->tok < ROFF_MAX);
	(*roff_term_pre_acts[n->tok])(p, n);
}

static void
roff_term_pre_br(ROFF_TERM_ARGS)
{
	term_newln(p);
	if (p->flags & TERMP_BRIND) {
		p->offset = p->rmargin;
		p->rmargin = p->maxrmargin;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
	}
}

static void
roff_term_pre_ft(ROFF_TERM_ARGS)
{
	switch (*n->child->string) {
	case '4':
	case '3':
	case 'B':
		term_fontrepl(p, TERMFONT_BOLD);
		break;
	case '2':
	case 'I':
		term_fontrepl(p, TERMFONT_UNDER);
		break;
	case 'P':
		term_fontlast(p);
		break;
	case '1':
	case 'C':
	case 'R':
		term_fontrepl(p, TERMFONT_NONE);
		break;
	default:
		break;
	}
}
