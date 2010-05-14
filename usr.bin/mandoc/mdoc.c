/*	$Id: mdoc.c,v 1.46 2010/05/14 01:54:37 schwarze Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmdoc.h"
#include "libmandoc.h"

const	char *const __mdoc_merrnames[MERRMAX] = {		 
	"trailing whitespace", /* ETAILWS */
	"unexpected quoted parameter", /* EQUOTPARM */
	"unterminated quoted parameter", /* EQUOTTERM */
	"argument parameter suggested", /* EARGVAL */
	"macro disallowed in prologue", /* EBODYPROL */
	"macro disallowed in body", /* EPROLBODY */
	"text disallowed in prologue", /* ETEXTPROL */
	"blank line disallowed", /* ENOBLANK */
	"text parameter too long", /* ETOOLONG */
	"invalid escape sequence", /* EESCAPE */
	"invalid character", /* EPRINT */
	"document has no body", /* ENODAT */
	"document has no prologue", /* ENOPROLOGUE */
	"expected line arguments", /* ELINE */
	"invalid AT&T argument", /* EATT */
	"default name not yet set", /* ENAME */
	"missing list type", /* ELISTTYPE */
	"missing display type", /* EDISPTYPE */
	"too many display types", /* EMULTIDISP */
	"too many list types", /* EMULTILIST */
	"NAME section must be first", /* ESECNAME */
	"badly-formed NAME section", /* ENAMESECINC */
	"argument repeated", /* EARGREP */
	"expected boolean parameter", /* EBOOL */
	"inconsistent column syntax", /* ECOLMIS */
	"nested display invalid", /* ENESTDISP */
	"width argument missing", /* EMISSWIDTH */
	"invalid section for this manual section", /* EWRONGMSEC */
	"section out of conventional order", /* ESECOOO */
	"section repeated", /* ESECREP */
	"invalid standard argument", /* EBADSTAND */
	"multi-line arguments discouraged", /* ENOMULTILINE */
	"multi-line arguments suggested", /* EMULTILINE */
	"line arguments discouraged", /* ENOLINE */
	"prologue macro out of conventional order", /* EPROLOOO */
	"prologue macro repeated", /* EPROLREP */
	"invalid manual section", /* EBADMSEC */
	"invalid section", /* EBADSEC */
	"invalid font mode", /* EFONT */
	"invalid date syntax", /* EBADDATE */
	"invalid number format", /* ENUMFMT */
	"superfluous width argument", /* ENOWIDTH */
	"system: utsname error", /* EUTSNAME */
	"obsolete macro", /* EOBS */
	"end-of-line scope violation", /* EIMPBRK */
	"empty macro ignored", /* EIGNE */
	"unclosed explicit scope", /* EOPEN */
	"unterminated quoted phrase", /* EQUOTPHR */
	"closure macro without prior context", /* ENOCTX */
	"no description found for library", /* ELIB */
	"bad child for parent context", /* EBADCHILD */
	"list arguments preceding type", /* ENOTYPE */
	"deprecated comment style", /* EBADCOMMENT */
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
	"Xr",		"%A",		"%B",		"%D",
	/* LINTED */
	"%I",		"%J",		"%N",		"%O",
	/* LINTED */
	"%P",		"%R",		"%T",		"%V",
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
	"Brc",		"%C",		"Es",		"En",
	/* LINTED */
	"Dx",		"%Q",		"br",		"sp",
	/* LINTED */
	"%U",		"eos"
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
	"symbolic",		"nested",		"centered"
	};

const	char * const *mdoc_macronames = __mdoc_macronames;
const	char * const *mdoc_argnames = __mdoc_argnames;

static	void		  mdoc_node_free(struct mdoc_node *);
static	void		  mdoc_node_unlink(struct mdoc *, 
				struct mdoc_node *);
static	void		  mdoc_free1(struct mdoc *);
static	void		  mdoc_alloc1(struct mdoc *);
static	struct mdoc_node *node_alloc(struct mdoc *, int, int, 
				enum mdoct, enum mdoc_type);
static	int		  node_append(struct mdoc *, 
				struct mdoc_node *);
static	int		  mdoc_ptext(struct mdoc *, int, char *);
static	int		  mdoc_pmacro(struct mdoc *, int, char *);
static	int		  macrowarn(struct mdoc *, int, const char *);


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
		mdoc_node_delete(mdoc, mdoc->first);
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
static void
mdoc_alloc1(struct mdoc *mdoc)
{

	memset(&mdoc->meta, 0, sizeof(struct mdoc_meta));
	mdoc->flags = 0;
	mdoc->lastnamed = mdoc->lastsec = SEC_NONE;
	mdoc->last = mandoc_calloc(1, sizeof(struct mdoc_node));
	mdoc->first = mdoc->last;
	mdoc->last->type = MDOC_ROOT;
	mdoc->next = MDOC_NEXT_CHILD;
}


/*
 * Free up volatile resources (see mdoc_free1()) then re-initialises the
 * data with mdoc_alloc1().  After invocation, parse data has been reset
 * and the parser is ready for re-invocation on a new tree; however,
 * cross-parse non-volatile data is kept intact.
 */
void
mdoc_reset(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	mdoc_alloc1(mdoc);
}


/*
 * Completely free up all volatile and non-volatile parse resources.
 * After invocation, the pointer is no longer usable.
 */
void
mdoc_free(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	free(mdoc);
}


/*
 * Allocate volatile and non-volatile parse resources.  
 */
struct mdoc *
mdoc_alloc(void *data, int pflags, const struct mdoc_cb *cb)
{
	struct mdoc	*p;

	p = mandoc_calloc(1, sizeof(struct mdoc));

	if (cb)
		memcpy(&p->cb, cb, sizeof(struct mdoc_cb));

	p->data = data;
	p->pflags = pflags;

	mdoc_hash_init();
	mdoc_alloc1(p);
	return(p);
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
 * the macro (mdoc_pmacro()) or text parser (mdoc_ptext()).
 */
int
mdoc_parseln(struct mdoc *m, int ln, char *buf)
{

	if (MDOC_HALT & m->flags)
		return(0);

	return('.' == *buf ? mdoc_pmacro(m, ln, buf) :
			mdoc_ptext(m, ln, buf));
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
mdoc_err(struct mdoc *m, int line, int pos, int iserr, enum merr type)
{
	const char	*p;

	p = __mdoc_merrnames[(int)type];
	assert(p);

	if (iserr)
		return(mdoc_verr(m, line, pos, p));

	return(mdoc_vwarn(m, line, pos, p));
}


int
mdoc_macro(struct mdoc *m, enum mdoct tok, 
		int ln, int pp, int *pos, char *buf)
{
	assert(tok < MDOC_MAX);

	/* If we're in the body, deny prologue calls. */

	if (MDOC_PROLOGUE & mdoc_macros[tok].flags && 
			MDOC_PBODY & m->flags)
		return(mdoc_perr(m, ln, pp, EPROLBODY));

	/* If we're in the prologue, deny "body" macros.  */

	if ( ! (MDOC_PROLOGUE & mdoc_macros[tok].flags) && 
			! (MDOC_PBODY & m->flags)) {
		if ( ! mdoc_pwarn(m, ln, pp, EBODYPROL))
			return(0);
		if (NULL == m->meta.title)
			m->meta.title = mandoc_strdup("unknown");
		if (NULL == m->meta.vol)
			m->meta.vol = mandoc_strdup("local");
		if (NULL == m->meta.os)
			m->meta.os = mandoc_strdup("local");
		if (0 == m->meta.date)
			m->meta.date = time(NULL);
		m->flags |= MDOC_PBODY;
	}

	return((*mdoc_macros[tok].fp)(m, tok, ln, pp, pos, buf));
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
node_alloc(struct mdoc *m, int line, int pos, 
		enum mdoct tok, enum mdoc_type type)
{
	struct mdoc_node *p;

	p = mandoc_calloc(1, sizeof(struct mdoc_node));
	p->sec = m->lastsec;
	p->line = line;
	p->pos = pos;
	p->tok = tok;
	p->type = type;

	return(p);
}


int
mdoc_tail_alloc(struct mdoc *m, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_TAIL);
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_head_alloc(struct mdoc *m, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	assert(m->first);
	assert(m->last);

	p = node_alloc(m, line, pos, tok, MDOC_HEAD);
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_body_alloc(struct mdoc *m, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_BODY);
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_block_alloc(struct mdoc *m, int line, int pos, 
		enum mdoct tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_BLOCK);
	p->args = args;
	if (p->args)
		(args->refcnt)++;
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_elem_alloc(struct mdoc *m, int line, int pos, 
		enum mdoct tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_ELEM);
	p->args = args;
	if (p->args)
		(args->refcnt)++;
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_word_alloc(struct mdoc *m, int line, int pos, const char *p)
{
	struct mdoc_node *n;
	size_t		  sv, len;

	len = strlen(p);

	n = node_alloc(m, line, pos, MDOC_MAX, MDOC_TEXT);
	n->string = mandoc_malloc(len + 1);
	sv = strlcpy(n->string, p, len + 1);

	/* Prohibit truncation. */
	assert(sv < len + 1);

	if ( ! node_append(m, n))
		return(0);

	m->next = MDOC_NEXT_SIBLING;
	return(1);
}


void
mdoc_node_free(struct mdoc_node *p)
{

	if (p->string)
		free(p->string);
	if (p->args)
		mdoc_argv_free(p->args);
	free(p);
}


static void
mdoc_node_unlink(struct mdoc *m, struct mdoc_node *n)
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
		if (n->prev) {
			m->last = n->prev;
			m->next = MDOC_NEXT_SIBLING;
		} else {
			m->last = n->parent;
			m->next = MDOC_NEXT_CHILD;
		}
	}

	if (m && m->first == n)
		m->first = NULL;
}


void
mdoc_node_delete(struct mdoc *m, struct mdoc_node *p)
{

	while (p->child) {
		assert(p->nchild);
		mdoc_node_delete(m, p->child);
	}
	assert(0 == p->nchild);

	mdoc_node_unlink(m, p);
	mdoc_node_free(p);
}


/*
 * Parse free-form text, that is, a line that does not begin with the
 * control character.
 */
static int
mdoc_ptext(struct mdoc *m, int line, char *buf)
{
	int		 i;

	/* Ignore bogus comments. */

	if ('\\' == buf[0] && '.' == buf[1] && '\"' == buf[2])
		return(mdoc_pwarn(m, line, 0, EBADCOMMENT));

	/* No text before an initial macro. */

	if (SEC_NONE == m->lastnamed)
		return(mdoc_perr(m, line, 0, ETEXTPROL));

	/* Literal just gets pulled in as-is. */
	
	if (MDOC_LITERAL & m->flags)
		return(mdoc_word_alloc(m, line, 0, buf));

	/* Check for a blank line, which may also consist of spaces. */

	for (i = 0; ' ' == buf[i]; i++)
		/* Skip to first non-space. */ ;

	if ('\0' == buf[i]) {
		if ( ! mdoc_pwarn(m, line, 0, ENOBLANK))
			return(0);

		/*
		 * Insert a `Pp' in the case of a blank line.  Technically,
		 * blank lines aren't allowed, but enough manuals assume this
		 * behaviour that we want to work around it.
		 */
		if ( ! mdoc_elem_alloc(m, line, 0, MDOC_Pp, NULL))
			return(0);

		m->next = MDOC_NEXT_SIBLING;
		return(1);
	}

	/* 
	 * Warn if the last un-escaped character is whitespace. Then
	 * strip away the remaining spaces (tabs stay!).   
	 */

	i = (int)strlen(buf);
	assert(i);

	if (' ' == buf[i - 1] || '\t' == buf[i - 1]) {
		if (i > 1 && '\\' != buf[i - 2])
			if ( ! mdoc_pwarn(m, line, i - 1, ETAILWS))
				return(0);

		for (--i; i && ' ' == buf[i]; i--)
			/* Spin back to non-space. */ ;

		/* Jump ahead of escaped whitespace. */
		i += '\\' == buf[i] ? 2 : 1;

		buf[i] = '\0';
	}

	/* Allocate the whole word. */
	if ( ! mdoc_word_alloc(m, line, 0, buf))
		return(0);

	/*
	 * Mark the end of a sentence.  Only works when you respect
	 * Jason's rule: "new sentence, new line".
	 */
	if ('.' == buf[i-1] || '!' == buf[i-1] || '?' == buf[i-1]) {
		m->next = MDOC_NEXT_SIBLING;
		if ( ! mdoc_elem_alloc(m, line, i, MDOC_eos, NULL))
			return(0);
	}

	m->next = MDOC_NEXT_SIBLING;
	return(1);
}


static int
macrowarn(struct mdoc *m, int ln, const char *buf)
{
	if ( ! (MDOC_IGN_MACRO & m->pflags))
		return(mdoc_verr(m, ln, 0, "unknown macro: %s%s", 
				buf, strlen(buf) > 3 ? "..." : ""));
	return(mdoc_vwarn(m, ln, 0, "unknown macro: %s%s",
				buf, strlen(buf) > 3 ? "..." : ""));
}


/*
 * Parse a macro line, that is, a line beginning with the control
 * character.
 */
int
mdoc_pmacro(struct mdoc *m, int ln, char *buf)
{
	enum mdoct	tok;
	int		i, j;
	char		mac[5];
	struct mdoc_node *n;
	char		 *t;

	/* Empty lines are ignored. */

	if ('\0' == buf[1])
		return(1);

	i = 1;

	/* Accept whitespace after the initial control char. */

	if (' ' == buf[i]) {
		i++;
		while (buf[i] && ' ' == buf[i])
			i++;
		if ('\0' == buf[i])
			return(1);
	}

	/* Copy the first word into a nil-terminated buffer. */

	for (j = 0; j < 4; j++, i++) {
		if ('\0' == (mac[j] = buf[i]))
			break;
		else if (' ' == buf[i])
			break;

		/* Check for invalid characters. */

		if (isgraph((u_char)buf[i]))
			continue;
		return(mdoc_perr(m, ln, i, EPRINT));
	}

	mac[j] = 0;

	if (j == 4 || j < 2) {
		if ( ! macrowarn(m, ln, mac))
			goto err;
		return(1);
	} 
	
	if (MDOC_MAX == (tok = mdoc_hash_find(mac))) {
		if ( ! macrowarn(m, ln, mac))
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
		if ( ! mdoc_pwarn(m, ln, i - 1, ETAILWS))
			goto err;

	/* 
	 * Begin recursive parse sequence.  Since we're at the start of
	 * the line, we don't need to do callable/parseable checks.
	 */
	if ( ! mdoc_macro(m, tok, ln, 1, &i, buf)) 
		goto err;

	/*
	 * Mark the end of a sentence, but be careful not to insert
	 * markers into reference blocks and after ellipses in
	 * function definitions.
	 */
	n = m->last;
	if (n->child)
		n = n->child;
	while (n->next)
		n = n->next;
	if (MDOC_TEXT == n->type &&
	    MDOC_Fn != n->parent->tok &&
	    MDOC_Rs != m->last->parent->tok) {
		t = n->string;
		while (t[0] && t[1])
			t++;
		if ('.' == *t || '!' == *t || '?' == *t) {
			if ( ! mdoc_elem_alloc(m, ln, i, MDOC_eos, NULL))
				return(0);
			m->next = MDOC_NEXT_SIBLING;
		}
	}

	return(1);

err:	/* Error out. */

	m->flags |= MDOC_HALT;
	return(0);
}


