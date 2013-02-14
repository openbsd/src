/*	$OpenBSD: expand.c,v 1.21 2013/02/14 12:30:49 gilles Exp $	*/

/*
 * Copyright (c) 2009 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

static const char *expandnode_info(struct expandnode *);

struct expandnode *
expand_lookup(struct expand *expand, struct expandnode *key)
{
	return RB_FIND(expandtree, &expand->tree, key);
}

void
expand_insert(struct expand *expand, struct expandnode *node)
{
	struct expandnode *xn;

	log_trace(TRACE_EXPAND, "expand: %p: expand_insert() called for %s",
	    expand, expandnode_info(node));
	if (node->type == EXPAND_USERNAME &&
	    expand->parent &&
	    expand->parent->type == EXPAND_USERNAME &&
	    !strcmp(expand->parent->u.user, node->u.user)) {
		log_trace(TRACE_EXPAND, "expand: %p: setting sameuser = 1",
		    expand);
		node->sameuser = 1;
	}

	if (expand_lookup(expand, node)) {
		log_trace(TRACE_EXPAND, "expand: %p: node found, discarding",
			expand);
		return;
	}

	log_trace(TRACE_EXPAND, "expand: %p: inserting node", expand);
	xn = xmemdup(node, sizeof *xn, "expand_insert");
	xn->rule = expand->rule;
	xn->parent = expand->parent;
	xn->alias = expand->alias;
	if (xn->parent)
		xn->depth = xn->parent->depth + 1;
	else
		xn->depth = 0;
	RB_INSERT(expandtree, &expand->tree, xn);
	if (expand->queue)
		TAILQ_INSERT_TAIL(expand->queue, xn, tq_entry);
	expand->nb_nodes++;
}

void
expand_clear(struct expand *expand)
{
	struct expandnode *xn;

	log_trace(TRACE_EXPAND, "expand: %p: clearing expand tree", expand);
	if (expand->queue)
		while ((xn = TAILQ_FIRST(expand->queue)))
			TAILQ_REMOVE(expand->queue, xn, tq_entry);

	while ((xn = RB_ROOT(&expand->tree)) != NULL) {
		RB_REMOVE(expandtree, &expand->tree, xn);
		free(xn);
	}
}

void
expand_free(struct expand *expand)
{
	expand_clear(expand);

	log_trace(TRACE_EXPAND, "expand: %p: freeing expand tree", expand);
	free(expand);
}

int
expand_cmp(struct expandnode *e1, struct expandnode *e2)
{
	if (e1->type < e2->type)
		return -1;
	if (e1->type > e2->type)
		return 1;
	if (e1->sameuser < e2->sameuser)
		return -1;
	if (e1->sameuser > e2->sameuser)
		return 1;
	if (e1->mapping < e2->mapping)
		return -1;
	if (e1->mapping > e2->mapping)
		return 1;
	if (e1->userbase < e2->userbase)
		return -1;
	if (e1->userbase > e2->userbase)
		return 1;

	return memcmp(&e1->u, &e2->u, sizeof(e1->u));
}

static int
expand_line_split(char **line, char **ret)
{
	static char	buffer[MAX_LINE_SIZE];
	int		esc, i, dq, sq;
	char	       *s;

	bzero(buffer, sizeof buffer);
	esc = dq = sq = i = 0;
	for (s = *line; (*s) && (i < (int)sizeof(buffer)); ++s) {
		if (esc) {
			buffer[i++] = *s;
			esc = 0;
			continue;
		}
		if (*s == '\\') {
			esc = 1;
			continue;
		}
		if (*s == ',' && !dq && !sq) {
			*ret = buffer;
			*line = s+1;
			return (1);
		}

		buffer[i++] = *s;
		esc = 0;

		if (*s == '"' && !sq)
			dq ^= 1;
		if (*s == '\'' && !dq)
			sq ^= 1;
	}

	if (esc || dq || sq || i == sizeof(buffer))
		return (-1);

	*ret = buffer;
	*line = s;
	return (i ? 1 : 0);
}

int
expand_line(struct expand *expand, const char *s, int do_includes)
{
	struct expandnode	xn;
	char			buffer[MAX_LINE_SIZE];
	char		       *p, *subrcpt;
	int			ret;

	bzero(buffer, sizeof buffer);
	if (strlcpy(buffer, s, sizeof buffer) >= sizeof buffer)
		return 0;

	p = buffer;
	while ((ret = expand_line_split(&p, &subrcpt)) > 0) {
		subrcpt = strip(subrcpt);
		if (subrcpt[0] == '\0')
			continue;
		if (! text_to_expandnode(&xn, subrcpt))
			return 0;
		if (! do_includes)
			if (xn.type == EXPAND_INCLUDE)
				continue;
		expand_insert(expand, &xn);
	}

	if (ret >= 0)
		return 1;

	/* expand_line_split() returned < 0 */
	return 0;
}

static const char *
expandnode_info(struct expandnode *e)
{
	static char	buffer[1024];
	const char     *type = NULL;
	const char     *value = NULL;

	switch (e->type) {
	case EXPAND_FILTER:
		type = "filter";
		break;
	case EXPAND_FILENAME:
		type = "filename";
		break;
	case EXPAND_INCLUDE:
		type = "include";
		break;
	case EXPAND_USERNAME:
		type = "username";
		break;
	case EXPAND_ADDRESS:
		type = "address";
		break;
	case EXPAND_INVALID:
	default:
		return NULL;
	}

	if ((value = expandnode_to_text(e)) == NULL)
		return NULL;

	strlcpy(buffer, type, sizeof buffer);
	strlcat(buffer, ":", sizeof buffer);
	if (strlcat(buffer, value, sizeof buffer) >= sizeof buffer)
		return NULL;
	if (e->mapping || e->userbase) {
		strlcat(buffer, " [", sizeof buffer);
		if (e->mapping) {
			strlcat(buffer, "mapping=", sizeof buffer);
			strlcat(buffer, e->mapping->t_name, sizeof buffer);
			if (e->userbase)
				strlcat(buffer, ", ", sizeof buffer);

		}
		if (e->userbase) {
			strlcat(buffer, "userbase=", sizeof buffer);
			strlcat(buffer, e->userbase->t_name, sizeof buffer);
		}
		if (strlcat(buffer, "]", sizeof buffer) >= sizeof buffer)
			return NULL;
	}
	return buffer;
}

RB_GENERATE(expandtree, expandnode, entry, expand_cmp);
