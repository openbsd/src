/*	$Id: man.c,v 1.31 2010/05/20 00:58:02 schwarze Exp $ */
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
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libman.h"
#include "libmandoc.h"

const	char *const __man_merrnames[WERRMAX] = {		 
	"invalid character", /* WNPRINT */
	"invalid date format", /* WDATE */
	"scope of prior line violated", /* WLNSCOPE */
	"over-zealous prior line scope violation", /* WLNSCOPE2 */
	"trailing whitespace", /* WTSPACE */
	"unterminated quoted parameter", /* WTQUOTE */
	"document has no body", /* WNODATA */
	"document has no title/section", /* WNOTITLE */
	"invalid escape sequence", /* WESCAPE */
	"invalid number format", /* WNUMFMT */
	"expected block head arguments", /* WHEADARGS */
	"expected block body arguments", /* WBODYARGS */
	"expected empty block head", /* WNHEADARGS */
	"ill-formed macro", /* WMACROFORM */
	"scope open on exit", /* WEXITSCOPE */
	"no scope context", /* WNOSCOPE */
	"literal context already open", /* WOLITERAL */
	"no literal context open", /* WNLITERAL */
	"document title should be uppercase", /* WTITLECASE */
	"deprecated comment style", /* WBADCOMMENT */
};

const	char *const __man_macronames[MAN_MAX] = {		 
	"br",		"TH",		"SH",		"SS",
	"TP", 		"LP",		"PP",		"P",
	"IP",		"HP",		"SM",		"SB",
	"BI",		"IB",		"BR",		"RB",
	"R",		"B",		"I",		"IR",
	"RI",		"na",		"i",		"sp",
	"nf",		"fi",		"r",		"RE",
	"RS",		"DT",		"UC",		"PD",
	"Sp",		"Vb",		"Ve",
	};

const	char * const *man_macronames = __man_macronames;

static	struct man_node	*man_node_alloc(int, int, 
				enum man_type, enum mant);
static	int		 man_node_append(struct man *, 
				struct man_node *);
static	void		 man_node_free(struct man_node *);
static	void		 man_node_unlink(struct man *, 
				struct man_node *);
static	int		 man_ptext(struct man *, int, char *, int);
static	int		 man_pmacro(struct man *, int, char *, int);
static	void		 man_free1(struct man *);
static	void		 man_alloc1(struct man *);
static	int		 macrowarn(struct man *, int, const char *, int);


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


void
man_reset(struct man *man)
{

	man_free1(man);
	man_alloc1(man);
}


void
man_free(struct man *man)
{

	man_free1(man);
	free(man);
}


struct man *
man_alloc(void *data, int pflags, const struct man_cb *cb)
{
	struct man	*p;

	p = mandoc_calloc(1, sizeof(struct man));

	if (cb)
		memcpy(&p->cb, cb, sizeof(struct man_cb));

	man_hash_init();
	p->data = data;
	p->pflags = pflags;

	man_alloc1(p);
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
man_parseln(struct man *m, int ln, char *buf, int offs)
{

	if (MAN_HALT & m->flags)
		return(0);

	return(('.' == buf[offs] || '\'' == buf[offs]) ? 
			man_pmacro(m, ln, buf, offs) : 
			man_ptext(m, ln, buf, offs));
}


static void
man_free1(struct man *man)
{

	if (man->first)
		man_node_delete(man, man->first);
	if (man->meta.title)
		free(man->meta.title);
	if (man->meta.source)
		free(man->meta.source);
	if (man->meta.vol)
		free(man->meta.vol);
	if (man->meta.msec)
		free(man->meta.msec);
}


static void
man_alloc1(struct man *m)
{

	memset(&m->meta, 0, sizeof(struct man_meta));
	m->flags = 0;
	m->last = mandoc_calloc(1, sizeof(struct man_node));
	m->first = m->last;
	m->last->type = MAN_ROOT;
	m->last->tok = MAN_MAX;
	m->next = MAN_NEXT_CHILD;
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
	
	assert(p->parent);
	p->parent->nchild++;

	if ( ! man_valid_pre(man, p))
		return(0);

	switch (p->type) {
	case (MAN_HEAD):
		assert(MAN_BLOCK == p->parent->type);
		p->parent->head = p;
		break;
	case (MAN_BODY):
		assert(MAN_BLOCK == p->parent->type);
		p->parent->body = p;
		break;
	default:
		break;
	}

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
man_node_alloc(int line, int pos, enum man_type type, enum mant tok)
{
	struct man_node *p;

	p = mandoc_calloc(1, sizeof(struct man_node));
	p->line = line;
	p->pos = pos;
	p->type = type;
	p->tok = tok;
	return(p);
}


int
man_elem_alloc(struct man *m, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(line, pos, MAN_ELEM, tok);
	if ( ! man_node_append(m, p))
		return(0);
	m->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_head_alloc(struct man *m, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(line, pos, MAN_HEAD, tok);
	if ( ! man_node_append(m, p))
		return(0);
	m->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_body_alloc(struct man *m, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(line, pos, MAN_BODY, tok);
	if ( ! man_node_append(m, p))
		return(0);
	m->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_block_alloc(struct man *m, int line, int pos, enum mant tok)
{
	struct man_node *p;

	p = man_node_alloc(line, pos, MAN_BLOCK, tok);
	if ( ! man_node_append(m, p))
		return(0);
	m->next = MAN_NEXT_CHILD;
	return(1);
}


int
man_word_alloc(struct man *m, int line, int pos, const char *word)
{
	struct man_node	*n;
	size_t		 sv, len;

	len = strlen(word);

	n = man_node_alloc(line, pos, MAN_TEXT, MAN_MAX);
	n->string = mandoc_malloc(len + 1);
	sv = strlcpy(n->string, word, len + 1);

	/* Prohibit truncation. */
	assert(sv < len + 1);

	if ( ! man_node_append(m, n))
		return(0);

	m->next = MAN_NEXT_SIBLING;
	return(1);
}


/*
 * Free all of the resources held by a node.  This does NOT unlink a
 * node from its context; for that, see man_node_unlink().
 */
static void
man_node_free(struct man_node *p)
{

	if (p->string)
		free(p->string);
	free(p);
}


void
man_node_delete(struct man *m, struct man_node *p)
{

	while (p->child)
		man_node_delete(m, p->child);

	man_node_unlink(m, p);
	man_node_free(p);
}


static int
man_ptext(struct man *m, int line, char *buf, int offs)
{
	int		 i;

	/* Ignore bogus comments. */

	if ('\\' == buf[offs] && 
			'.' == buf[offs + 1] && 
			'"' == buf[offs + 2])
		return(man_pwarn(m, line, offs, WBADCOMMENT));

	/* Literal free-form text whitespace is preserved. */

	if (MAN_LITERAL & m->flags) {
		if ( ! man_word_alloc(m, line, offs, buf + offs))
			return(0);
		goto descope;
	}

	/* Pump blank lines directly into the backend. */

	for (i = offs; ' ' == buf[i]; i++)
		/* Skip leading whitespace. */ ;

	if ('\0' == buf[i]) {
		/* Allocate a blank entry. */
		if ( ! man_word_alloc(m, line, offs, ""))
			return(0);
		goto descope;
	}

	/* 
	 * Warn if the last un-escaped character is whitespace. Then
	 * strip away the remaining spaces (tabs stay!).   
	 */

	i = (int)strlen(buf);
	assert(i);

	if (' ' == buf[i - 1] || '\t' == buf[i - 1]) {
		if (i > 1 && '\\' != buf[i - 2])
			if ( ! man_pwarn(m, line, i - 1, WTSPACE))
				return(0);

		for (--i; i && ' ' == buf[i]; i--)
			/* Spin back to non-space. */ ;

		/* Jump ahead of escaped whitespace. */
		i += '\\' == buf[i] ? 2 : 1;

		buf[i] = '\0';
	}

	if ( ! man_word_alloc(m, line, offs, buf + offs))
		return(0);

	/*
	 * End-of-sentence check.  If the last character is an unescaped
	 * EOS character, then flag the node as being the end of a
	 * sentence.  The front-end will know how to interpret this.
	 */

	assert(i);
	if (mandoc_eos(buf, (size_t)i))
		m->last->flags |= MAN_EOS;

descope:
	/*
	 * Co-ordinate what happens with having a next-line scope open:
	 * first close out the element scope (if applicable), then close
	 * out the block scope (also if applicable).
	 */

	if (MAN_ELINE & m->flags) {
		m->flags &= ~MAN_ELINE;
		if ( ! man_unscope(m, m->last->parent, WERRMAX))
			return(0);
	}

	if ( ! (MAN_BLINE & m->flags))
		return(1);
	m->flags &= ~MAN_BLINE;

	if ( ! man_unscope(m, m->last->parent, WERRMAX))
		return(0);
	return(man_body_alloc(m, line, offs, m->last->tok));
}


static int
macrowarn(struct man *m, int ln, const char *buf, int offs)
{
	if ( ! (MAN_IGN_MACRO & m->pflags))
		return(man_verr(m, ln, offs, "unknown macro: %s%s", 
				buf, strlen(buf) > 3 ? "..." : ""));
	return(man_vwarn(m, ln, offs, "unknown macro: %s%s",
				buf, strlen(buf) > 3 ? "..." : ""));
}


int
man_pmacro(struct man *m, int ln, char *buf, int offs)
{
	int		 i, j, ppos;
	enum mant	 tok;
	char		 mac[5];
	struct man_node	*n;

	/* Comments and empties are quickly ignored. */

	offs++;

	if ('\0' == buf[offs])
		return(1);

	i = offs;

	/*
	 * Skip whitespace between the control character and initial
	 * text.  "Whitespace" is both spaces and tabs.
	 */

	if (' ' == buf[i] || '\t' == buf[i]) {
		i++;
		while (buf[i] && (' ' == buf[i] || '\t' == buf[i]))
			i++;
		if ('\0' == buf[i])
			goto out;
	}

	ppos = i;

	/* Copy the first word into a nil-terminated buffer. */

	for (j = 0; j < 4; j++, i++) {
		if ('\0' == (mac[j] = buf[i]))
			break;
		else if (' ' == buf[i])
			break;

		/* Check for invalid characters. */

		if (isgraph((u_char)buf[i]))
			continue;
		return(man_perr(m, ln, i, WNPRINT));
	}

	mac[j] = '\0';

	if (j == 4 || j < 1) {
		if ( ! (MAN_IGN_MACRO & m->pflags)) {
			(void)man_perr(m, ln, ppos, WMACROFORM);
			goto err;
		} 
		if ( ! man_pwarn(m, ln, ppos, WMACROFORM))
			goto err;
		return(1);
	}
	
	if (MAN_MAX == (tok = man_hash_find(mac))) {
		if ( ! macrowarn(m, ln, mac, ppos))
			goto err;
		return(1);
	}

	/* The macro is sane.  Jump to the next word. */

	while (buf[i] && ' ' == buf[i])
		i++;

	/* 
	 * Trailing whitespace.  Note that tabs are allowed to be passed
	 * into the parser as "text", so we only warn about spaces here.
	 */

	if ('\0' == buf[i] && ' ' == buf[i - 1])
		if ( ! man_pwarn(m, ln, i - 1, WTSPACE))
			goto err;

	/* 
	 * Remove prior ELINE macro, as it's being clobbering by a new
	 * macro.  Note that NSCOPED macros do not close out ELINE
	 * macros---they don't print text---so we let those slip by.
	 */

	if ( ! (MAN_NSCOPED & man_macros[tok].flags) &&
			m->flags & MAN_ELINE) {
		assert(MAN_TEXT != m->last->type);

		/*
		 * This occurs in the following construction:
		 *   .B
		 *   .br
		 *   .B
		 *   .br
		 *   I hate man macros.
		 * Flat-out disallow this madness.
		 */
		if (MAN_NSCOPED & man_macros[m->last->tok].flags)
			return(man_perr(m, ln, ppos, WLNSCOPE));

		n = m->last;

		assert(n);
		assert(NULL == n->child);
		assert(0 == n->nchild);

		if ( ! man_nwarn(m, n, WLNSCOPE))
			return(0);

		man_node_delete(m, n);
		m->flags &= ~MAN_ELINE;
	}

	/*
	 * Save the fact that we're in the next-line for a block.  In
	 * this way, embedded roff instructions can "remember" state
	 * when they exit.
	 */

	if (MAN_BLINE & m->flags)
		m->flags |= MAN_BPLINE;

	/* Call to handler... */

	assert(man_macros[tok].fp);
	if ( ! (*man_macros[tok].fp)(m, tok, ln, ppos, &i, buf))
		goto err;

out:
	/* 
	 * We weren't in a block-line scope when entering the
	 * above-parsed macro, so return.
	 */

	if ( ! (MAN_BPLINE & m->flags)) {
		m->flags &= ~MAN_ILINE; 
		return(1);
	}
	m->flags &= ~MAN_BPLINE;

	/*
	 * If we're in a block scope, then allow this macro to slip by
	 * without closing scope around it.
	 */

	if (MAN_ILINE & m->flags) {
		m->flags &= ~MAN_ILINE;
		return(1);
	}

	/* 
	 * If we've opened a new next-line element scope, then return
	 * now, as the next line will close out the block scope.
	 */

	if (MAN_ELINE & m->flags)
		return(1);

	/* Close out the block scope opened in the prior line.  */

	assert(MAN_BLINE & m->flags);
	m->flags &= ~MAN_BLINE;

	if ( ! man_unscope(m, m->last->parent, WERRMAX))
		return(0);
	return(man_body_alloc(m, ln, offs, m->last->tok));

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


/*
 * Unlink a node from its context.  If "m" is provided, the last parse
 * point will also be adjusted accordingly.
 */
static void
man_node_unlink(struct man *m, struct man_node *n)
{

	/* Adjust siblings. */

	if (n->prev)
		n->prev->next = n->next;
	if (n->next)
		n->next->prev = n->prev;

	/* Adjust parent. */

	if (n->parent) {
		n->parent->nchild--;
		if (n->parent->child == n)
			n->parent->child = n->prev ? n->prev : n->next;
	}

	/* Adjust parse point, if applicable. */

	if (m && m->last == n) {
		/*XXX: this can occur when bailing from validation. */
		/*assert(NULL == n->next);*/
		if (n->prev) {
			m->last = n->prev;
			m->next = MAN_NEXT_SIBLING;
		} else {
			m->last = n->parent;
			m->next = MAN_NEXT_CHILD;
		}
	}

	if (m && m->first == n)
		m->first = NULL;
}
