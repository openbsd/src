/*	$Id: mdoc.c,v 1.13 2009/07/12 19:05:52 schwarze Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libmdoc.h"

enum	merr {
	ENOCALL,
	EBODYPROL,
	EPROLBODY,
	ESPACE,
	ETEXTPROL,
	ENOBLANK,
	EMALLOC
};

const	char *const __mdoc_macronames[MDOC_MAX] = {		 
	"Ap",		"Dd",		"Dt",		"Os",
	"Sh",		"Ss",		"Pp",		"D1",
	"Dl",		"Bd",		"Ed",		"Bl",
	"El",		"It",		"Ad",		"An",
	"Ar",		"Cd",		"Cm",		"Dv",
	"Er",		"Ev",		"Ex",		"Fa",
	"Fd",		"Fl",		"Fn",		"Ft",
	"Ic",		"In",		"Li",		"Nd",
	"Nm",		"Op",		"Ot",		"Pa",
	"Rv",		"St",		"Va",		"Vt",
	/* LINTED */
	"Xr",		"\%A",		"\%B",		"\%D",
	/* LINTED */
	"\%I",		"\%J",		"\%N",		"\%O",
	/* LINTED */
	"\%P",		"\%R",		"\%T",		"\%V",
	"Ac",		"Ao",		"Aq",		"At",
	"Bc",		"Bf",		"Bo",		"Bq",
	"Bsx",		"Bx",		"Db",		"Dc",
	"Do",		"Dq",		"Ec",		"Ef",
	"Em",		"Eo",		"Fx",		"Ms",
	"No",		"Ns",		"Nx",		"Ox",
	"Pc",		"Pf",		"Po",		"Pq",
	"Qc",		"Ql",		"Qo",		"Qq",
	"Re",		"Rs",		"Sc",		"So",
	"Sq",		"Sm",		"Sx",		"Sy",
	"Tn",		"Ux",		"Xc",		"Xo",
	"Fo",		"Fc",		"Oo",		"Oc",
	"Bk",		"Ek",		"Bt",		"Hf",
	"Fr",		"Ud",		"Lb",		"Lp",
	"Lk",		"Mt",		"Brq",		"Bro",
	/* LINTED */
	"Brc",		"\%C",		"Es",		"En",
	/* LINTED */
	"Dx",		"\%Q"
	};

const	char *const __mdoc_argnames[MDOC_ARG_MAX] = {		 
	"split",		"nosplit",		"ragged",
	"unfilled",		"literal",		"file",		 
	"offset",		"bullet",		"dash",		 
	"hyphen",		"item",			"enum",		 
	"tag",			"diag",			"hang",		 
	"ohang",		"inset",		"column",	 
	"width",		"compact",		"std",	 
	"filled",		"words",		"emphasis",
	"symbolic",		"nested"
	};

const	char * const *mdoc_macronames = __mdoc_macronames;
const	char * const *mdoc_argnames = __mdoc_argnames;

static	void		  mdoc_free1(struct mdoc *);
static	int		  mdoc_alloc1(struct mdoc *);
static	struct mdoc_node *node_alloc(struct mdoc *, int, int, 
				int, enum mdoc_type);
static	int		  node_append(struct mdoc *, 
				struct mdoc_node *);
static	int		  parsetext(struct mdoc *, int, char *);
static	int		  parsemacro(struct mdoc *, int, char *);
static	int		  macrowarn(struct mdoc *, int, const char *);
static	int		  perr(struct mdoc *, int, int, enum merr);

const struct mdoc_node *
mdoc_node(const struct mdoc *m)
{

	return(MDOC_HALT & m->flags ? NULL : m->first);
}


const struct mdoc_meta *
mdoc_meta(const struct mdoc *m)
{

	return(MDOC_HALT & m->flags ? NULL : &m->meta);
}


/*
 * Frees volatile resources (parse tree, meta-data, fields).
 */
static void
mdoc_free1(struct mdoc *mdoc)
{

	if (mdoc->first)
		mdoc_node_freelist(mdoc->first);
	if (mdoc->meta.title)
		free(mdoc->meta.title);
	if (mdoc->meta.os)
		free(mdoc->meta.os);
	if (mdoc->meta.name)
		free(mdoc->meta.name);
	if (mdoc->meta.arch)
		free(mdoc->meta.arch);
	if (mdoc->meta.vol)
		free(mdoc->meta.vol);
}


/*
 * Allocate all volatile resources (parse tree, meta-data, fields).
 */
static int
mdoc_alloc1(struct mdoc *mdoc)
{

	bzero(&mdoc->meta, sizeof(struct mdoc_meta));
	mdoc->flags = 0;
	mdoc->lastnamed = mdoc->lastsec = SEC_NONE;
	mdoc->last = calloc(1, sizeof(struct mdoc_node));
	if (NULL == mdoc->last)
		return(0);

	mdoc->first = mdoc->last;
	mdoc->last->type = MDOC_ROOT;
	mdoc->next = MDOC_NEXT_CHILD;
	return(1);
}


/*
 * Free up volatile resources (see mdoc_free1()) then re-initialises the
 * data with mdoc_alloc1().  After invocation, parse data has been reset
 * and the parser is ready for re-invocation on a new tree; however,
 * cross-parse non-volatile data is kept intact.
 */
int
mdoc_reset(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	return(mdoc_alloc1(mdoc));
}


/*
 * Completely free up all volatile and non-volatile parse resources.
 * After invocation, the pointer is no longer usable.
 */
void
mdoc_free(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	if (mdoc->htab)
		mdoc_hash_free(mdoc->htab);
	free(mdoc);
}


/*
 * Allocate volatile and non-volatile parse resources.  
 */
struct mdoc *
mdoc_alloc(void *data, int pflags, const struct mdoc_cb *cb)
{
	struct mdoc	*p;

	if (NULL == (p = calloc(1, sizeof(struct mdoc))))
		return(NULL);
	if (cb)
		(void)memcpy(&p->cb, cb, sizeof(struct mdoc_cb));

	p->data = data;
	p->pflags = pflags;

	if (NULL == (p->htab = mdoc_hash_alloc())) {
		free(p);
		return(NULL);
	} else if (mdoc_alloc1(p))
		return(p);

	free(p);
	return(NULL);
}


/*
 * Climb back up the parse tree, validating open scopes.  Mostly calls
 * through to macro_end() in macro.c.
 */
int
mdoc_endparse(struct mdoc *m)
{

	if (MDOC_HALT & m->flags)
		return(0);
	else if (mdoc_macroend(m))
		return(1);
	m->flags |= MDOC_HALT;
	return(0);
}


/*
 * Main parse routine.  Parses a single line -- really just hands off to
 * the macro (parsemacro()) or text parser (parsetext()).
 */
int
mdoc_parseln(struct mdoc *m, int ln, char *buf)
{

	if (MDOC_HALT & m->flags)
		return(0);

	return('.' == *buf ? parsemacro(m, ln, buf) :
			parsetext(m, ln, buf));
}


int
mdoc_verr(struct mdoc *mdoc, int ln, int pos, 
		const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == mdoc->cb.mdoc_err)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);

	return((*mdoc->cb.mdoc_err)(mdoc->data, ln, pos, buf));
}


int
mdoc_vwarn(struct mdoc *mdoc, int ln, int pos, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == mdoc->cb.mdoc_warn)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*mdoc->cb.mdoc_warn)(mdoc->data, ln, pos, buf));
}


int
mdoc_nerr(struct mdoc *mdoc, const struct mdoc_node *node, 
		const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == mdoc->cb.mdoc_err)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*mdoc->cb.mdoc_err)(mdoc->data, 
				node->line, node->pos, buf));
}


int
mdoc_warn(struct mdoc *mdoc, enum mdoc_warn type, 
		const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == mdoc->cb.mdoc_warn)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*mdoc->cb.mdoc_warn)(mdoc->data, mdoc->last->line,
				mdoc->last->pos, buf));
}


int
mdoc_err(struct mdoc *mdoc, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == mdoc->cb.mdoc_err)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*mdoc->cb.mdoc_err)(mdoc->data, mdoc->last->line,
				mdoc->last->pos, buf));
}


int
mdoc_pwarn(struct mdoc *mdoc, int line, int pos, enum mdoc_warn type,
		const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == mdoc->cb.mdoc_warn)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*mdoc->cb.mdoc_warn)(mdoc->data, line, pos, buf));
}

int
mdoc_perr(struct mdoc *mdoc, int line, int pos, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	if (NULL == mdoc->cb.mdoc_err)
		return(0);

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return((*mdoc->cb.mdoc_err)(mdoc->data, line, pos, buf));
}


int
mdoc_macro(struct mdoc *m, int tok, 
		int ln, int pp, int *pos, char *buf)
{

	if (MDOC_PROLOGUE & mdoc_macros[tok].flags && 
			MDOC_PBODY & m->flags)
		return(perr(m, ln, pp, EPROLBODY));
	if ( ! (MDOC_PROLOGUE & mdoc_macros[tok].flags) && 
			! (MDOC_PBODY & m->flags))
		return(perr(m, ln, pp, EBODYPROL));

	if (1 != pp && ! (MDOC_CALLABLE & mdoc_macros[tok].flags))
		return(perr(m, ln, pp, ENOCALL));

	return((*mdoc_macros[tok].fp)(m, tok, ln, pp, pos, buf));
}


static int
perr(struct mdoc *m, int line, int pos, enum merr type)
{
	char		*p;

	p = NULL;
	switch (type) {
	case (ENOCALL):
		p = "not callable";
		break;
	case (EPROLBODY):
		p = "macro disallowed in document body";
		break;
	case (EBODYPROL):
		p = "macro disallowed in document prologue";
		break;
	case (EMALLOC):
		p = "memory exhausted";
		break;
	case (ETEXTPROL):
		p = "text disallowed in document prologue";
		break;
	case (ENOBLANK):
		p = "blank lines disallowed in non-literal contexts";
		break;
	case (ESPACE):
		p = "whitespace disallowed after delimiter";
		break;
	}
	assert(p);
	return(mdoc_perr(m, line, pos, p));
}


static int
node_append(struct mdoc *mdoc, struct mdoc_node *p)
{

	assert(mdoc->last);
	assert(mdoc->first);
	assert(MDOC_ROOT != p->type);

	switch (mdoc->next) {
	case (MDOC_NEXT_SIBLING):
		mdoc->last->next = p;
		p->prev = mdoc->last;
		p->parent = mdoc->last->parent;
		break;
	case (MDOC_NEXT_CHILD):
		mdoc->last->child = p;
		p->parent = mdoc->last;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	p->parent->nchild++;

	if ( ! mdoc_valid_pre(mdoc, p))
		return(0);
	if ( ! mdoc_action_pre(mdoc, p))
		return(0);

	switch (p->type) {
	case (MDOC_HEAD):
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->head = p;
		break;
	case (MDOC_TAIL):
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->tail = p;
		break;
	case (MDOC_BODY):
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->body = p;
		break;
	default:
		break;
	}

	mdoc->last = p;

	switch (p->type) {
	case (MDOC_TEXT):
		if ( ! mdoc_valid_post(mdoc))
			return(0);
		if ( ! mdoc_action_post(mdoc))
			return(0);
		break;
	default:
		break;
	}

	return(1);
}


static struct mdoc_node *
node_alloc(struct mdoc *mdoc, int line, 
		int pos, int tok, enum mdoc_type type)
{
	struct mdoc_node *p;

	if (NULL == (p = calloc(1, sizeof(struct mdoc_node)))) {
		(void)perr(mdoc, (mdoc)->last->line, 
				(mdoc)->last->pos, EMALLOC);
		return(NULL);
	}

	p->sec = mdoc->lastsec;
	p->line = line;
	p->pos = pos;
	p->tok = tok;
	if (MDOC_TEXT != (p->type = type))
		assert(p->tok >= 0);

	return(p);
}


int
mdoc_tail_alloc(struct mdoc *mdoc, int line, int pos, int tok)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_TAIL);
	if (NULL == p)
		return(0);
	return(node_append(mdoc, p));
}


int
mdoc_head_alloc(struct mdoc *mdoc, int line, int pos, int tok)
{
	struct mdoc_node *p;

	assert(mdoc->first);
	assert(mdoc->last);

	p = node_alloc(mdoc, line, pos, tok, MDOC_HEAD);
	if (NULL == p)
		return(0);
	return(node_append(mdoc, p));
}


int
mdoc_body_alloc(struct mdoc *mdoc, int line, int pos, int tok)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_BODY);
	if (NULL == p)
		return(0);
	return(node_append(mdoc, p));
}


int
mdoc_block_alloc(struct mdoc *mdoc, int line, int pos, 
		int tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_BLOCK);
	if (NULL == p)
		return(0);
	p->args = args;
	if (p->args)
		(args->refcnt)++;
	return(node_append(mdoc, p));
}


int
mdoc_elem_alloc(struct mdoc *mdoc, int line, int pos, 
		int tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, tok, MDOC_ELEM);
	if (NULL == p)
		return(0);
	p->args = args;
	if (p->args)
		(args->refcnt)++;
	return(node_append(mdoc, p));
}


int
mdoc_word_alloc(struct mdoc *mdoc, 
		int line, int pos, const char *word)
{
	struct mdoc_node *p;

	p = node_alloc(mdoc, line, pos, -1, MDOC_TEXT);
	if (NULL == p)
		return(0);
	if (NULL == (p->string = strdup(word))) {
		(void)perr(mdoc, (mdoc)->last->line, 
				(mdoc)->last->pos, EMALLOC);
		return(0);
	}
	return(node_append(mdoc, p));
}


void
mdoc_node_free(struct mdoc_node *p)
{

	if (p->parent)
		p->parent->nchild--;
	if (p->string)
		free(p->string);
	if (p->args)
		mdoc_argv_free(p->args);
	free(p);
}


void
mdoc_node_freelist(struct mdoc_node *p)
{

	if (p->child)
		mdoc_node_freelist(p->child);
	if (p->next)
		mdoc_node_freelist(p->next);

	assert(0 == p->nchild);
	mdoc_node_free(p);
}


/*
 * Parse free-form text, that is, a line that does not begin with the
 * control character.
 */
static int
parsetext(struct mdoc *m, int line, char *buf)
{

	if (SEC_NONE == m->lastnamed)
		return(perr(m, line, 0, ETEXTPROL));

	if (0 == buf[0] && ! (MDOC_LITERAL & m->flags))
		return(perr(m, line, 0, ENOBLANK));

	if ( ! mdoc_word_alloc(m, line, 0, buf))
		return(0);

	m->next = MDOC_NEXT_SIBLING;
	return(1);
}


static int
macrowarn(struct mdoc *m, int ln, const char *buf)
{
	if ( ! (MDOC_IGN_MACRO & m->pflags))
		return(mdoc_verr(m, ln, 1, 
				"unknown macro: %s%s", 
				buf, strlen(buf) > 3 ? "..." : ""));
	return(mdoc_vwarn(m, ln, 1, "unknown macro: %s%s",
				buf, strlen(buf) > 3 ? "..." : ""));
}


/*
 * Parse a macro line, that is, a line beginning with the control
 * character.
 */
int
parsemacro(struct mdoc *m, int ln, char *buf)
{
	int		  i, c;
	char		  mac[5];

	/* Empty lines are ignored. */

	if (0 == buf[1])
		return(1);

	if (' ' == buf[1]) {
		i = 2;
		while (buf[i] && ' ' == buf[i])
			i++;
		if (0 == buf[i])
			return(1);
		return(perr(m, ln, 1, ESPACE));
	}

	/* Copy the first word into a nil-terminated buffer. */

	for (i = 1; i < 5; i++) {
		if (0 == (mac[i - 1] = buf[i]))
			break;
		else if (' ' == buf[i])
			break;
	}

	mac[i - 1] = 0;

	if (i == 5 || i <= 2) {
		if ( ! macrowarn(m, ln, mac))
			goto err;
		return(1);
	} 
	
	if (MDOC_MAX == (c = mdoc_hash_find(m->htab, mac))) {
		if ( ! macrowarn(m, ln, mac))
			goto err;
		return(1);
	}

	/* The macro is sane.  Jump to the next word. */

	while (buf[i] && ' ' == buf[i])
		i++;

	/* Begin recursive parse sequence. */

	if ( ! mdoc_macro(m, c, ln, 1, &i, buf)) 
		goto err;

	return(1);

err:	/* Error out. */

	m->flags |= MDOC_HALT;
	return(0);
}
