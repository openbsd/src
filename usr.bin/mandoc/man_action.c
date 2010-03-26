/*	$Id: man_action.c,v 1.14 2010/03/26 01:22:05 schwarze Exp $ */
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
#include <stdlib.h>
#include <string.h>

#include "libman.h"
#include "libmandoc.h"

struct	actions {
	int	(*post)(struct man *);
};

static	int	  post_TH(struct man *);
static	int	  post_de(struct man *);
static	int	  post_fi(struct man *);
static	int	  post_nf(struct man *);

const	struct actions man_actions[MAN_MAX] = {
	{ NULL }, /* br */
	{ post_TH }, /* TH */
	{ NULL }, /* SH */
	{ NULL }, /* SS */
	{ NULL }, /* TP */
	{ NULL }, /* LP */
	{ NULL }, /* PP */
	{ NULL }, /* P */
	{ NULL }, /* IP */
	{ NULL }, /* HP */
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
	{ NULL }, /* na */
	{ NULL }, /* i */
	{ NULL }, /* sp */
	{ post_nf }, /* nf */
	{ post_fi }, /* fi */
	{ NULL }, /* r */
	{ NULL }, /* RE */
	{ NULL }, /* RS */
	{ NULL }, /* DT */
	{ NULL }, /* UC */
	{ NULL }, /* PD */
	{ NULL }, /* Sp */
	{ post_nf }, /* Vb */
	{ post_fi }, /* Ve */
	{ post_de }, /* de */
	{ post_de }, /* dei */
	{ post_de }, /* am */
	{ post_de }, /* ami */
	{ post_de }, /* ig */
	{ NULL }, /* . */
};


int
man_action_post(struct man *m)
{

	if (MAN_ACTED & m->last->flags)
		return(1);
	m->last->flags |= MAN_ACTED;

	switch (m->last->type) {
	case (MAN_TEXT):
		/* FALLTHROUGH */
	case (MAN_ROOT):
		return(1);
	default:
		break;
	}

	if (NULL == man_actions[m->last->tok].post)
		return(1);
	return((*man_actions[m->last->tok].post)(m));
}


static int
post_fi(struct man *m)
{

	if ( ! (MAN_LITERAL & m->flags))
		if ( ! man_nwarn(m, m->last, WNLITERAL))
			return(0);
	m->flags &= ~MAN_LITERAL;
	return(1);
}


static int
post_de(struct man *m)
{

	/*
	 * XXX: for the time being, we indiscriminately remove roff
	 * instructions from the parse stream.
	 */
	if (MAN_BLOCK == m->last->type)
		man_node_delete(m, m->last);

	return(1);
}


static int
post_nf(struct man *m)
{

	if (MAN_LITERAL & m->flags)
		if ( ! man_nwarn(m, m->last, WOLITERAL))
			return(0);
	m->flags |= MAN_LITERAL;
	return(1);
}


static int
post_TH(struct man *m)
{
	struct man_node	*n;
	char		*ep;
	long		 lval;

	if (m->meta.title)
		free(m->meta.title);
	if (m->meta.vol)
		free(m->meta.vol);
	if (m->meta.source)
		free(m->meta.source);

	m->meta.title = m->meta.vol = m->meta.source = NULL;
	m->meta.msec = 0;
	m->meta.date = 0;

	/* ->TITLE<- MSEC DATE SOURCE VOL */

	n = m->last->child;
	assert(n);
	m->meta.title = mandoc_strdup(n->string);

	/* TITLE ->MSEC<- DATE SOURCE VOL */

	n = n->next;
	assert(n);

	lval = strtol(n->string, &ep, 10);
	if (n->string[0] != '\0' && *ep == '\0')
		m->meta.msec = (int)lval;
	else if ( ! man_nwarn(m, n, WMSEC))
		return(0);

	/* TITLE MSEC ->DATE<- SOURCE VOL */

	n = n->next;
	if (n) {
		m->meta.date = mandoc_a2time
			(MTIME_ISO_8601, n->string);

		if (0 == m->meta.date) {
			if ( ! man_nwarn(m, n, WDATE))
				return(0);
			m->meta.date = time(NULL);
		}
	} else
		m->meta.date = time(NULL);

	/* TITLE MSEC DATE ->SOURCE<- VOL */

	if (n && (n = n->next))
		m->meta.source = mandoc_strdup(n->string);

	/* TITLE MSEC DATE SOURCE ->VOL<- */

	if (n && (n = n->next))
		m->meta.vol = mandoc_strdup(n->string);

	/*
	 * Remove the `TH' node after we've processed it for our
	 * meta-data.
	 */
	man_node_delete(m, m->last);
	return(1);
}
