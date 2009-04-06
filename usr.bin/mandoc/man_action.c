/* $Id: man_action.c,v 1.1 2009/04/06 20:30:40 kristaps Exp $ */
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
#include <sys/utsname.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libman.h"

struct	actions {
	int	(*post)(struct man *);
};


static	int	  post_TH(struct man *);
static	time_t	  man_atotime(const char *);

const	struct actions man_actions[MAN_MAX] = {
	{ NULL }, /* __ */
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
	{ NULL }, /* br */
	{ NULL }, /* na */
	{ NULL }, /* i */
};


int
man_action_post(struct man *m)
{

	if (MAN_ACTED & m->last->flags)
		return(1);
	m->last->flags |= MAN_ACTED;

	switch (m->last->type) {
	case (MAN_TEXT):
		break;
	case (MAN_ROOT):
		break;
	default:
		if (NULL == man_actions[m->last->tok].post)
			break;
		return((*man_actions[m->last->tok].post)(m));
	}
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

	if (NULL == (m->meta.title = strdup(n->string)))
		return(man_verr(m, n->line, n->pos, 
					"memory exhausted"));

	/* TITLE ->MSEC<- DATE SOURCE VOL */

	n = n->next;
	assert(n);

	errno = 0;
	lval = strtol(n->string, &ep, 10);
	if (n->string[0] != '\0' && *ep == '\0')
		m->meta.msec = (int)lval;
	else if ( ! man_vwarn(m, n->line, n->pos, "invalid section"))
		return(0);

	/* TITLE MSEC ->DATE<- SOURCE VOL */

	if (NULL == (n = n->next))
		m->meta.date = time(NULL);
	else if (0 == (m->meta.date = man_atotime(n->string))) {
		if ( ! man_vwarn(m, n->line, n->pos, "invalid date"))
			return(0);
		m->meta.date = time(NULL);
	}

	/* TITLE MSEC DATE ->SOURCE<- VOL */

	if (n && (n = n->next))
		if (NULL == (m->meta.source = strdup(n->string)))
			return(man_verr(m, n->line, n->pos, 
						"memory exhausted"));

	/* TITLE MSEC DATE SOURCE ->VOL<- */

	if (n && (n = n->next))
		if (NULL == (m->meta.vol = strdup(n->string)))
			return(man_verr(m, n->line, n->pos, 
						"memory exhausted"));

	/* 
	 * The end document shouldn't have the prologue macros as part
	 * of the syntax tree (they encompass only meta-data).  
	 */

	if (m->last->parent->child == m->last) {
		assert(MAN_ROOT == m->last->parent->type);
		m->last->parent->child = NULL;
		n = m->last;
		m->last = m->last->parent;
		m->next = MAN_NEXT_CHILD;
		assert(m->last == m->first);
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

	(void)memset(&tm, 0, sizeof(struct tm));

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
