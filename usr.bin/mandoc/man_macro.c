/*	$Id: man_macro.c,v 1.5 2009/08/22 15:15:37 schwarze Exp $ */
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
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "libman.h"

#define	FL_NLINE	(1 << 0)
#define	FL_TLINE	(1 << 1)

static	int		 man_args(struct man *, int, 
				int *, char *, char **);

static	int man_flags[MAN_MAX] = {
	0, /* br */
	0, /* TH */
	0, /* SH */
	0, /* SS */
	FL_TLINE, /* TP */
	0, /* LP */
	0, /* PP */
	0, /* P */
	0, /* IP */
	0, /* HP */
	FL_NLINE, /* SM */
	FL_NLINE, /* SB */
	FL_NLINE, /* BI */
	FL_NLINE, /* IB */
	FL_NLINE, /* BR */
	FL_NLINE, /* RB */
	FL_NLINE, /* R */
	FL_NLINE, /* B */
	FL_NLINE, /* I */
	FL_NLINE, /* IR */
	FL_NLINE, /* RI */
	0, /* na */
	FL_NLINE, /* i */
	0, /* sp */
};

int
man_macro(struct man *man, int tok, int line, 
		int ppos, int *pos, char *buf)
{
	int		 w, la;
	char		*p;
	struct man_node	*n;

	if ( ! man_elem_alloc(man, line, ppos, tok))
		return(0);
	n = man->last;
	man->next = MAN_NEXT_CHILD;

	for (;;) {
		la = *pos;
		w = man_args(man, line, pos, buf, &p);

		if (-1 == w)
			return(0);
		if (0 == w)
			break;

		if ( ! man_word_alloc(man, line, la, p))
			return(0);
		man->next = MAN_NEXT_SIBLING;
	}

	if (n == man->last && (FL_NLINE & man_flags[tok])) {
		if (MAN_NLINE & man->flags) 
			return(man_perr(man, line, ppos, WLNSCOPE));
		man->flags |= MAN_NLINE;
		return(1);
	}

	if (FL_TLINE & man_flags[tok]) {
		if (MAN_NLINE & man->flags) 
			return(man_perr(man, line, ppos, WLNSCOPE));
		man->flags |= MAN_NLINE;
		return(1);
	}

	/*
	 * Note that when TH is pruned, we'll be back at the root, so
	 * make sure that we don't clobber as its sibling.
	 */

	for ( ; man->last; man->last = man->last->parent) {
		if (man->last == n)
			break;
		if (man->last->type == MAN_ROOT)
			break;
		if ( ! man_valid_post(man))
			return(0);
		if ( ! man_action_post(man))
			return(0);
	}

	assert(man->last);

	/*
	 * Same here regarding whether we're back at the root. 
	 */

	if (man->last->type != MAN_ROOT && ! man_valid_post(man))
		return(0);
	if (man->last->type != MAN_ROOT && ! man_action_post(man))
		return(0);
	if (man->last->type != MAN_ROOT)
		man->next = MAN_NEXT_SIBLING;

	return(1);
}


int
man_macroend(struct man *m)
{

	for ( ; m->last && m->last != m->first; 
			m->last = m->last->parent) {
		if ( ! man_valid_post(m))
			return(0);
		if ( ! man_action_post(m))
			return(0);
	}
	assert(m->last == m->first);

	if ( ! man_valid_post(m))
		return(0);
	if ( ! man_action_post(m))
		return(0);

	return(1);
}


/* ARGSUSED */
static int
man_args(struct man *m, int line, 
		int *pos, char *buf, char **v)
{

	if (0 == buf[*pos])
		return(0);

	/* First parse non-quoted strings. */

	if ('\"' != buf[*pos]) {
		*v = &buf[*pos];

		while (buf[*pos]) {
			if (' ' == buf[*pos])
				if ('\\' != buf[*pos - 1])
					break;
			(*pos)++;
		}

		if (0 == buf[*pos])
			return(1);

		buf[(*pos)++] = 0;

		if (0 == buf[*pos])
			return(1);

		while (buf[*pos] && ' ' == buf[*pos])
			(*pos)++;

		if (buf[*pos])
			return(1);

		if ( ! man_pwarn(m, line, *pos, WTSPACE))
			return(-1);

		return(1);
	}

	/*
	 * If we're a quoted string (and quoted strings are allowed),
	 * then parse ahead to the next quote.  If none's found, it's an
	 * error.  After, parse to the next word.  
	 */

	*v = &buf[++(*pos)];

	while (buf[*pos] && '\"' != buf[*pos])
		(*pos)++;

	if (0 == buf[*pos]) {
		if ( ! man_pwarn(m, line, *pos, WTQUOTE))
			return(-1);
		return(1);
	}

	buf[(*pos)++] = 0;
	if (0 == buf[*pos])
		return(1);

	while (buf[*pos] && ' ' == buf[*pos])
		(*pos)++;

	if (buf[*pos])
		return(1);

	if ( ! man_pwarn(m, line, *pos, WTSPACE))
		return(-1);
	return(1);
}
