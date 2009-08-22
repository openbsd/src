/*	$Id: man.c,v 1.8 2009/08/22 15:15:37 schwarze Exp $ */
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libman.h"

const	char *const __man_merrnames[WERRMAX] = {		 
	"invalid character", /* WNPRINT */
	"system: malloc error", /* WNMEM */
	"invalid manual section", /* WMSEC */
	"invalid date format", /* WDATE */
	"scope of prior line violated", /* WLNSCOPE */
	"trailing whitespace", /* WTSPACE */
	"unterminated quoted parameter", /* WTQUOTE */
	"document has no body", /* WNODATA */
	"document has no title/section", /* WNOTITLE */
	"invalid escape sequence", /* WESCAPE */
	"invalid number format", /* WNUMFMT */
};

const	char *const __man_macronames[MAN_MAX] = {		 
	"br",		"TH",		"SH",		"SS",
	"TP", 		"LP",		"PP",		"P",
	"IP",		"HP",		"SM",		"SB",
	"BI",		"IB",		"BR",		"RB",
	"R",		"B",		"I",		"IR",
	"RI",		"na",		"i",		"sp"
	};

const	char * const *man_macronames = __man_macronames;

static	struct man_node	*man_node_alloc(int, int, 
				enum man_type, int);
static	int		 man_node_append(struct man *, 
				struct man_node *);
static	int		 man_ptext(struct man *, int, char *);
static	int		 man_pmacro(struct man *, int, char *);
static	void		 man_free1(struct man *);
static	int		 man_alloc1(struct man *);


const struct man_node *
man_node(const struct man *m)
{

	return(MAN_HALT & m->flags ? NULL : m->first);
}


const struct man_meta *
man_meta(const struct man *m)
{

	return(MAN_HALT & m->flags ? NULL : &m->meta);
}


int
man_reset(struct man *man)
{

	man_free1(man);
	return(man_alloc1(man));
}


void
man_free(struct man *man)
{

	man_free1(man);

	if (man->htab)
		man_hash_free(man->htab);
	free(man);
}


struct man *
man_alloc(void *data, int pflags, const struct man_cb *cb)
{
	struct man	*p;

	if (NULL == (p = calloc(1, sizeof(struct man))))
		return(NULL);

	if ( ! man_alloc1(p)) {
		free(p);
		return(NULL);
	}

	p->data = data;
	p->pflags = pflags;
	(void)memcpy(&p->cb, cb, sizeof(struct man_cb));

	if (NULL == (p->htab = man_hash_alloc())) {
		free(p);
		return(NULL);
	}
	return(p);
}


int
man_endparse(struct man *m)
{

	if (MAN_HALT & m->flags)
		return(0);
	else if (man_macroend(m))
		return(1);
	m->flags |= MAN_HALT;
	return(0);
}


int
man_parseln(struct man *m, int ln, char *buf)
{

	return('.' == *buf ? 
			man_pmacro(m, ln, buf) : 
			man_ptext(m, ln, buf));
}


static void
man_free1(struct man *man)
{

	if (man->first)
		man_node_freelist(man->first);
	if (man->meta.title)
		free(man->meta.title);
	if (man->meta.source)
		free(man->meta.source);
	if (man->meta.vol)
		free(man->meta.vol);
}


static int
man_alloc1(struct man *m)
{

	bzero(&m->meta, sizeof(struct man_meta));
	m->flags = 0;
	m->last = calloc(1, sizeof(struct man_node));
	if (NULL == m->last)
		return(0);
	m->first = m->last;
	m->last->type = MAN_ROOT;
	m->next = MAN_NEXT_CHILD;
	return(1);
}


static int
man_node_append(struct man *man, struct man_node *p)
{

	assert(man->last);
	assert(man->first);
	assert(MAN_ROOT != p->type);

	switch (man->next) {
	case (MAN_NEXT_SIBLING):
		man->last->next = p;
		p->prev = man->last;
		p->parent = man->last->parent;
		break;
	case (MAN_NEXT_CHILD):
		man->last->child = p;
		p->parent = man->last;
		break;
	default:
		abort();
		/* NOTREACHED */
	}
	
	p->parent->nchild++;

	man->last = p;

	switch (p->type) {
	case (MAN_TEXT):
		if ( ! man_valid_post(man))
			return(0);
		if ( ! man_action_post(man))
			return(0);
		break;
	default:
		break;
	}

	return(1);
}


static struct man_node *
man_node_alloc(int line, int pos, enum man_type type, int tok)
{
	struct man_node *p;

	p = calloc(1, sizeof(struct man_node));
	if (NULL == p)
		return(NULL);

	p->line = line;
	p->pos = pos;
	p->type = type;
	p->tok = tok;
	return(p);
}


int
man_elem_alloc(struct man *man, int line, int pos, int tok)
{
	struct man_node *p;

	p = man_node_alloc(line, pos, MAN_ELEM, tok);
	if (NULL == p)
		return(0);
	return(man_node_append(man, p));
}


int
man_word_alloc(struct man *man, 
		int line, int pos, const char *word)
{
	struct man_node	*p;

	p = man_node_alloc(line, pos, MAN_TEXT, -1);
	if (NULL == p)
		return(0);
	if (NULL == (p->string = strdup(word)))
		return(0);
	return(man_node_append(man, p));
}


void
man_node_free(struct man_node *p)
{

	if (p->string)
		free(p->string);
	if (p->parent)
		p->parent->nchild--;
	free(p);
}


void
man_node_freelist(struct man_node *p)
{

	if (p->child)
		man_node_freelist(p->child);
	if (p->next)
		man_node_freelist(p->next);

	assert(0 == p->nchild);
	man_node_free(p);
}


static int
man_ptext(struct man *m, int line, char *buf)
{

	if ( ! man_word_alloc(m, line, 0, buf))
		return(0);
	m->next = MAN_NEXT_SIBLING;

	/*
	 * If this is one of the zany NLINE macros that consumes the
	 * next line of input as being influenced, then close out the
	 * existing macro "scope" and continue processing.
	 */

	if ( ! (MAN_NLINE & m->flags))
		return(1);

	m->flags &= ~MAN_NLINE;
	m->last = m->last->parent;

	assert(MAN_ROOT != m->last->type);
	if ( ! man_valid_post(m))
		return(0);
	if ( ! man_action_post(m))
		return(0);

	return(1);
}


int
man_pmacro(struct man *m, int ln, char *buf)
{
	int		  i, j, c, ppos, fl;
	char		  mac[5];
	struct man_node	 *n;

	/* Comments and empties are quickly ignored. */

	n = m->last;
	fl = MAN_NLINE & m->flags;

	if (0 == buf[1])
		goto out;

	i = 1;

	if (' ' == buf[i]) {
		i++;
		while (buf[i] && ' ' == buf[i])
			i++;
		if (0 == buf[i])
			goto out;
	}

	ppos = i;

	/* Copy the first word into a nil-terminated buffer. */

	for (j = 0; j < 4; j++, i++) {
		if (0 == (mac[j] = buf[i]))
			break;
		else if (' ' == buf[i])
			break;
	}

	mac[j] = 0;

	if (j == 4 || j < 1) {
		if ( ! (MAN_IGN_MACRO & m->pflags)) {
			(void)man_verr(m, ln, ppos, 
				"ill-formed macro: %s", mac);
			goto err;
		} 
		if ( ! man_vwarn(m, ln, ppos, 
				"ill-formed macro: %s", mac))
			goto err;
		return(1);
	}
	
	if (MAN_MAX == (c = man_hash_find(m->htab, mac))) {
		if ( ! (MAN_IGN_MACRO & m->pflags)) {
			(void)man_verr(m, ln, ppos, 
				"unknown macro: %s", mac);
			goto err;
		} 
		if ( ! man_vwarn(m, ln, ppos, 
				"unknown macro: %s", mac))
			goto err;
		return(1);
	}

	/* The macro is sane.  Jump to the next word. */

	while (buf[i] && ' ' == buf[i])
		i++;

	/* Begin recursive parse sequence. */

	if ( ! man_macro(m, c, ln, ppos, &i, buf))
		goto err;

out:
	if (fl) {
		/*
		 * A NLINE macro has been immediately followed with
		 * another.  Close out the preceding macro's scope, and
		 * continue.
		 */
		assert(MAN_ROOT != m->last->type);
		assert(m->last->parent);
		assert(MAN_ROOT != m->last->parent->type);

		if (n != m->last)
			m->last = m->last->parent;

		if ( ! man_valid_post(m))
			return(0);
		if ( ! man_action_post(m))
			return(0);
		m->next = MAN_NEXT_SIBLING;
		m->flags &= ~MAN_NLINE;
	} 

	return(1);

err:	/* Error out. */

	m->flags |= MAN_HALT;
	return(0);
}


int
man_verr(struct man *man, int ln, int pos, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == man->cb.man_err)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*man->cb.man_err)(man->data, ln, pos, buf));
}


int
man_vwarn(struct man *man, int ln, int pos, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == man->cb.man_warn)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*man->cb.man_warn)(man->data, ln, pos, buf));
}


int
man_err(struct man *m, int line, int pos, int iserr, enum merr type)
{
	const char	 *p;
	
	p = __man_merrnames[(int)type];
	assert(p);

	if (iserr)
		return(man_verr(m, line, pos, p));

	return(man_vwarn(m, line, pos, p));
}
