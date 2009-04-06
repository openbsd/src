/* $Id: man_validate.c,v 1.1 2009/04/06 20:30:40 kristaps Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

#include "libman.h"

/* FIXME: validate text. */

#define	POSTARGS  struct man *m, const struct man_node *n

typedef	int	(*v_post)(POSTARGS);

struct	man_valid {
	v_post	 *posts;
};

static	int	  count(const struct man_node *);
static	int	  check_eq0(POSTARGS);
static	int	  check_ge1(POSTARGS);
static	int	  check_ge2(POSTARGS);
static	int	  check_le1(POSTARGS);
static	int	  check_le2(POSTARGS);
static	int	  check_le5(POSTARGS);

static	v_post	  posts_le1[] = { check_le1, NULL };
static	v_post	  posts_le2[] = { check_le2, NULL };
static	v_post	  posts_ge1[] = { check_ge1, NULL };
static	v_post	  posts_eq0[] = { check_eq0, NULL };
static	v_post	  posts_ge2_le5[] = { check_ge2, check_le5, NULL };

static	const struct man_valid man_valids[MAN_MAX] = {
	{ NULL }, /* __ */
	{ posts_ge2_le5 }, /* TH */
	{ posts_ge1 }, /* SH */
	{ posts_ge1 }, /* SS */
	{ NULL }, /* TP */
	{ posts_eq0 }, /* LP */
	{ posts_eq0 }, /* PP */
	{ posts_eq0 }, /* P */
	{ posts_le2 }, /* IP */
	{ posts_le1 }, /* HP */
	{ NULL }, /* SM */
	{ NULL }, /* SB */
	{ NULL }, /* BI */
	{ NULL }, /* IB */
	{ NULL }, /* BR */
	{ NULL }, /* RB */
	{ NULL }, /* R */
	{ NULL }, /* B */
	{ NULL }, /* I */
	{ NULL }, /* IR */
	{ NULL }, /* RI */
	{ posts_eq0 }, /* br */
	{ posts_eq0 }, /* na */
	{ NULL }, /* i */
};


int
man_valid_post(struct man *m)
{
	v_post		*cp;

	if (MAN_VALID & m->last->flags)
		return(1);
	m->last->flags |= MAN_VALID;

	switch (m->last->type) {
	case (MAN_TEXT): 
		/* FALLTHROUGH */
	case (MAN_ROOT):
		return(1);
	default:
		break;
	}

	if (NULL == (cp = man_valids[m->last->tok].posts))
		return(1);
	for ( ; *cp; cp++)
		if ( ! (*cp)(m, m->last))
			return(0);

	return(1);
}


static inline int
count(const struct man_node *n)
{ 
	int		 i;

	for (i = 0; n; n = n->next, i++) 
		/* Loop. */ ;
	return(i);
}


#define	INEQ_DEFINE(x, ineq, name) \
static int \
check_##name(POSTARGS) \
{ \
	int		 c; \
	if ((c = count(n->child)) ineq (x)) \
		return(1); \
	return(man_verr(m, n->line, n->pos, \
			"expected line arguments %s %d, have %d", \
			#ineq, (x), c)); \
}

INEQ_DEFINE(0, ==, eq0)
INEQ_DEFINE(1, >=, ge1)
INEQ_DEFINE(2, >=, ge2)
INEQ_DEFINE(1, <=, le1)
INEQ_DEFINE(2, <=, le2)
INEQ_DEFINE(5, <=, le5)

