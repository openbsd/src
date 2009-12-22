/*	$Id: man_action.c,v 1.10 2009/12/22 23:58:00 schwarze Exp $ */
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
#include <sys/utsname.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libman.h"
#include "libmandoc.h"

struct	actions {
	int	(*post)(struct man *);
};

static	int	  post_TH(struct man *);
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
};

static	time_t	  man_atotime(const char *);


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

	if (NULL == (n = n->next))
		m->meta.date = time(NULL);
	else if (0 == (m->meta.date = man_atotime(n->string))) {
		if ( ! man_nwarn(m, n, WDATE))
			return(0);
		m->meta.date = time(NULL);
	}

	/* TITLE MSEC DATE ->SOURCE<- VOL */

	if (n && (n = n->next))
		m->meta.source = mandoc_strdup(n->string);

	/* TITLE MSEC DATE SOURCE ->VOL<- */

	if (n && (n = n->next))
		m->meta.vol = mandoc_strdup(n->string);

	/* 
	 * The end document shouldn't have the prologue macros as part
	 * of the syntax tree (they encompass only meta-data).  
	 */

	if (m->last->parent->child == m->last) {
		m->last->parent->child = NULL;
		n = m->last;
		m->last = m->last->parent;
		m->next = MAN_NEXT_CHILD;
	} else {
		assert(m->last->prev);
		m->last->prev->next = NULL;
		n = m->last;
		m->last = m->last->prev;
		m->next = MAN_NEXT_SIBLING;
	}

	man_node_freelist(n);
	return(1);
}


static time_t
man_atotime(const char *p)
{
	struct tm	 tm;
	char		*pp;

	memset(&tm, 0, sizeof(struct tm));

	if ((pp = strptime(p, "%b %d %Y", &tm)) && 0 == *pp)
		return(mktime(&tm));
	if ((pp = strptime(p, "%d %b %Y", &tm)) && 0 == *pp)
		return(mktime(&tm));
	if ((pp = strptime(p, "%b %d, %Y", &tm)) && 0 == *pp)
		return(mktime(&tm));
	if ((pp = strptime(p, "%b %Y", &tm)) && 0 == *pp)
		return(mktime(&tm));

	return(0);
}
