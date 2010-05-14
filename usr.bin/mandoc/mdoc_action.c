/*	$Id: mdoc_action.c,v 1.33 2010/05/14 19:52:43 schwarze Exp $ */
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
#ifndef	OSNAME
#include <sys/utsname.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmdoc.h"
#include "libmandoc.h"

#define	POST_ARGS struct mdoc *m, struct mdoc_node *n
#define	PRE_ARGS  struct mdoc *m, const struct mdoc_node *n

#define	NUMSIZ	  32
#define	DATESIZ	  32

struct	actions {
	int	(*pre)(PRE_ARGS);
	int	(*post)(POST_ARGS);
};

static	int	  concat(struct mdoc *, char *,
			const struct mdoc_node *, size_t);
static	inline int order_rs(enum mdoct);

static	int	  post_ar(POST_ARGS);
static	int	  post_at(POST_ARGS);
static	int	  post_bl(POST_ARGS);
static	int	  post_bl_head(POST_ARGS);
static	int	  post_bl_tagwidth(POST_ARGS);
static	int	  post_bl_width(POST_ARGS);
static	int	  post_dd(POST_ARGS);
static	int	  post_display(POST_ARGS);
static	int	  post_dt(POST_ARGS);
static	int	  post_lb(POST_ARGS);
static	int	  post_nm(POST_ARGS);
static	int	  post_os(POST_ARGS);
static	int	  post_pa(POST_ARGS);
static	int	  post_prol(POST_ARGS);
static	int	  post_rs(POST_ARGS);
static	int	  post_sh(POST_ARGS);
static	int	  post_st(POST_ARGS);
static	int	  post_std(POST_ARGS);

static	int	  pre_bd(PRE_ARGS);
static	int	  pre_bl(PRE_ARGS);
static	int	  pre_dl(PRE_ARGS);
static	int	  pre_offset(PRE_ARGS);

static	const struct actions mdoc_actions[MDOC_MAX] = {
	{ NULL, NULL }, /* Ap */
	{ NULL, post_dd }, /* Dd */ 
	{ NULL, post_dt }, /* Dt */ 
	{ NULL, post_os }, /* Os */ 
	{ NULL, post_sh }, /* Sh */ 
	{ NULL, NULL }, /* Ss */ 
	{ NULL, NULL }, /* Pp */ 
	{ NULL, NULL }, /* D1 */
	{ pre_dl, post_display }, /* Dl */
	{ pre_bd, post_display }, /* Bd */ 
	{ NULL, NULL }, /* Ed */
	{ pre_bl, post_bl }, /* Bl */ 
	{ NULL, NULL }, /* El */
	{ NULL, NULL }, /* It */
	{ NULL, NULL }, /* Ad */ 
	{ NULL, NULL }, /* An */
	{ NULL, post_ar }, /* Ar */
	{ NULL, NULL }, /* Cd */
	{ NULL, NULL }, /* Cm */
	{ NULL, NULL }, /* Dv */ 
	{ NULL, NULL }, /* Er */ 
	{ NULL, NULL }, /* Ev */ 
	{ NULL, post_std }, /* Ex */
	{ NULL, NULL }, /* Fa */ 
	{ NULL, NULL }, /* Fd */ 
	{ NULL, NULL }, /* Fl */
	{ NULL, NULL }, /* Fn */ 
	{ NULL, NULL }, /* Ft */ 
	{ NULL, NULL }, /* Ic */ 
	{ NULL, NULL }, /* In */ 
	{ NULL, NULL }, /* Li */
	{ NULL, NULL }, /* Nd */ 
	{ NULL, post_nm }, /* Nm */ 
	{ NULL, NULL }, /* Op */
	{ NULL, NULL }, /* Ot */
	{ NULL, post_pa }, /* Pa */
	{ NULL, post_std }, /* Rv */
	{ NULL, post_st }, /* St */
	{ NULL, NULL }, /* Va */
	{ NULL, NULL }, /* Vt */ 
	{ NULL, NULL }, /* Xr */
	{ NULL, NULL }, /* %A */
	{ NULL, NULL }, /* %B */
	{ NULL, NULL }, /* %D */
	{ NULL, NULL }, /* %I */
	{ NULL, NULL }, /* %J */
	{ NULL, NULL }, /* %N */
	{ NULL, NULL }, /* %O */
	{ NULL, NULL }, /* %P */
	{ NULL, NULL }, /* %R */
	{ NULL, NULL }, /* %T */
	{ NULL, NULL }, /* %V */
	{ NULL, NULL }, /* Ac */
	{ NULL, NULL }, /* Ao */
	{ NULL, NULL }, /* Aq */
	{ NULL, post_at }, /* At */ 
	{ NULL, NULL }, /* Bc */
	{ NULL, NULL }, /* Bf */ 
	{ NULL, NULL }, /* Bo */
	{ NULL, NULL }, /* Bq */
	{ NULL, NULL }, /* Bsx */
	{ NULL, NULL }, /* Bx */
	{ NULL, NULL }, /* Db */
	{ NULL, NULL }, /* Dc */
	{ NULL, NULL }, /* Do */
	{ NULL, NULL }, /* Dq */
	{ NULL, NULL }, /* Ec */
	{ NULL, NULL }, /* Ef */
	{ NULL, NULL }, /* Em */ 
	{ NULL, NULL }, /* Eo */
	{ NULL, NULL }, /* Fx */
	{ NULL, NULL }, /* Ms */
	{ NULL, NULL }, /* No */
	{ NULL, NULL }, /* Ns */
	{ NULL, NULL }, /* Nx */
	{ NULL, NULL }, /* Ox */
	{ NULL, NULL }, /* Pc */
	{ NULL, NULL }, /* Pf */
	{ NULL, NULL }, /* Po */
	{ NULL, NULL }, /* Pq */
	{ NULL, NULL }, /* Qc */
	{ NULL, NULL }, /* Ql */
	{ NULL, NULL }, /* Qo */
	{ NULL, NULL }, /* Qq */
	{ NULL, NULL }, /* Re */
	{ NULL, post_rs }, /* Rs */
	{ NULL, NULL }, /* Sc */
	{ NULL, NULL }, /* So */
	{ NULL, NULL }, /* Sq */
	{ NULL, NULL }, /* Sm */
	{ NULL, NULL }, /* Sx */
	{ NULL, NULL }, /* Sy */
	{ NULL, NULL }, /* Tn */
	{ NULL, NULL }, /* Ux */
	{ NULL, NULL }, /* Xc */
	{ NULL, NULL }, /* Xo */
	{ NULL, NULL }, /* Fo */ 
	{ NULL, NULL }, /* Fc */ 
	{ NULL, NULL }, /* Oo */
	{ NULL, NULL }, /* Oc */
	{ NULL, NULL }, /* Bk */
	{ NULL, NULL }, /* Ek */
	{ NULL, NULL }, /* Bt */
	{ NULL, NULL }, /* Hf */
	{ NULL, NULL }, /* Fr */
	{ NULL, NULL }, /* Ud */
	{ NULL, post_lb }, /* Lb */
	{ NULL, NULL }, /* Lp */
	{ NULL, NULL }, /* Lk */
	{ NULL, NULL }, /* Mt */
	{ NULL, NULL }, /* Brq */
	{ NULL, NULL }, /* Bro */
	{ NULL, NULL }, /* Brc */
	{ NULL, NULL }, /* %C */
	{ NULL, NULL }, /* Es */
	{ NULL, NULL }, /* En */
	{ NULL, NULL }, /* Dx */
	{ NULL, NULL }, /* %Q */
	{ NULL, NULL }, /* br */
	{ NULL, NULL }, /* sp */
	{ NULL, NULL }, /* %U */
};

#define	RSORD_MAX 14

static	const enum mdoct rsord[RSORD_MAX] = {
	MDOC__A,
	MDOC__T,
	MDOC__B,
	MDOC__I,
	MDOC__J,
	MDOC__R,
	MDOC__N,
	MDOC__V,
	MDOC__P,
	MDOC__Q,
	MDOC__D,
	MDOC__O,
	MDOC__C,
	MDOC__U
};


int
mdoc_action_pre(struct mdoc *m, const struct mdoc_node *n)
{

	switch (n->type) {
	case (MDOC_ROOT):
		/* FALLTHROUGH */
	case (MDOC_TEXT):
		return(1);
	default:
		break;
	}

	if (NULL == mdoc_actions[n->tok].pre)
		return(1);
	return((*mdoc_actions[n->tok].pre)(m, n));
}


int
mdoc_action_post(struct mdoc *m)
{

	if (MDOC_ACTED & m->last->flags)
		return(1);
	m->last->flags |= MDOC_ACTED;

	switch (m->last->type) {
	case (MDOC_TEXT):
		/* FALLTHROUGH */
	case (MDOC_ROOT):
		return(1);
	default:
		break;
	}

	if (NULL == mdoc_actions[m->last->tok].post)
		return(1);
	return((*mdoc_actions[m->last->tok].post)(m, m->last));
}


/*
 * Concatenate sibling nodes together.  All siblings must be of type
 * MDOC_TEXT or an assertion is raised.  Concatenation is separated by a
 * single whitespace.
 */
static int
concat(struct mdoc *m, char *p, const struct mdoc_node *n, size_t sz)
{

	assert(sz);
	p[0] = '\0';
	for ( ; n; n = n->next) {
		assert(MDOC_TEXT == n->type);
		if (strlcat(p, n->string, sz) >= sz)
			return(mdoc_nerr(m, n, ETOOLONG));
		if (NULL == n->next)
			continue;
		if (strlcat(p, " ", sz) >= sz)
			return(mdoc_nerr(m, n, ETOOLONG));
	}

	return(1);
}


/*
 * Macros accepting `-std' as an argument have the name of the current
 * document (`Nm') filled in as the argument if it's not provided.
 */
static int
post_std(POST_ARGS)
{
	struct mdoc_node	*nn;

	if (n->child)
		return(1);
	
	nn = n;
	m->next = MDOC_NEXT_CHILD;
	assert(m->meta.name);
	if ( ! mdoc_word_alloc(m, n->line, n->pos, m->meta.name))
		return(0);
	m->last = nn;
	return(1);
}


/*
 * The `Nm' macro's first use sets the name of the document.  See also
 * post_std(), etc.
 */
static int
post_nm(POST_ARGS)
{
	char		 buf[BUFSIZ];

	if (m->meta.name)
		return(1);
	if ( ! concat(m, buf, n->child, BUFSIZ))
		return(0);
	m->meta.name = mandoc_strdup(buf);
	return(1);
}


/*
 * Look up the value of `Lb' for matching predefined strings.  If it has
 * one, then substitute the current value for the formatted value.  Note
 * that the lookup may fail (we can provide arbitrary strings).
 */
/* ARGSUSED */
static int
post_lb(POST_ARGS)
{
	const char	*p;
	char		*buf;
	size_t		 sz;

	assert(MDOC_TEXT == n->child->type);
	p = mdoc_a2lib(n->child->string);

	if (p) {
		free(n->child->string);
		n->child->string = mandoc_strdup(p);
		return(1);
	}

	sz = strlen(n->child->string) +
		2 + strlen("\\(lqlibrary\\(rq");
	buf = mandoc_malloc(sz);
	snprintf(buf, sz, "library \\(lq%s\\(rq", n->child->string);
	free(n->child->string);
	n->child->string = buf;
	return(1);
}


/*
 * Substitute the value of `St' for the corresponding formatted string.
 * We're guaranteed that this exists (it's been verified during the
 * validation phase).
 */
/* ARGSUSED */
static int
post_st(POST_ARGS)
{
	const char	*p;

	assert(MDOC_TEXT == n->child->type);
	p = mdoc_a2st(n->child->string);
	assert(p);
	free(n->child->string);
	n->child->string = mandoc_strdup(p);
	return(1);
}


/*
 * Look up the standard string in a table.  We know that it exists from
 * the validation phase, so assert on failure.  If a standard key wasn't
 * supplied, supply the default ``AT&T UNIX''.
 */
static int
post_at(POST_ARGS)
{
	struct mdoc_node	*nn;
	const char		*p;

	if (n->child) {
		assert(MDOC_TEXT == n->child->type);
		p = mdoc_a2att(n->child->string);
		assert(p);
		free(n->child->string);
		n->child->string = mandoc_strdup(p);
		return(1);
	}

	nn = n;
	m->next = MDOC_NEXT_CHILD;
	if ( ! mdoc_word_alloc(m, nn->line, nn->pos, "AT&T UNIX"))
		return(0);
	m->last = nn;
	return(1);
}


/*
 * Mark the current section.  The ``named'' section (lastnamed) is set
 * whenever the current section isn't a custom section--we use this to
 * keep track of section ordering.  Also check that the section is
 * allowed within the document's manual section.
 */
static int
post_sh(POST_ARGS)
{
	enum mdoc_sec	 sec;
	char		 buf[BUFSIZ];

	if (MDOC_HEAD != n->type)
		return(1);

	if ( ! concat(m, buf, n->child, BUFSIZ))
		return(0);
	sec = mdoc_str2sec(buf);
	/*
	 * The first section should always make us move into a non-new
	 * state.
	 */
	if (SEC_NONE == m->lastnamed || SEC_CUSTOM != sec)
		m->lastnamed = sec;

	/* Some sections only live in certain manual sections. */

	switch ((m->lastsec = sec)) {
	case (SEC_RETURN_VALUES):
		/* FALLTHROUGH */
	case (SEC_ERRORS):
		switch (m->meta.msec) {
		case (2):
			/* FALLTHROUGH */
		case (3):
			/* FALLTHROUGH */
		case (9):
			break;
		default:
			return(mdoc_nwarn(m, n, EBADSEC));
		}
		break;
	default:
		break;
	}
	return(1);
}


/*
 * Parse out the contents of `Dt'.  See in-line documentation for how we
 * handle the various fields of this macro.
 */
static int
post_dt(POST_ARGS)
{
	struct mdoc_node *nn;
	const char	 *cp;
	char		 *ep;
	long		  lval;

	if (m->meta.title)
		free(m->meta.title);
	if (m->meta.vol)
		free(m->meta.vol);
	if (m->meta.arch)
		free(m->meta.arch);

	m->meta.title = m->meta.vol = m->meta.arch = NULL;
	m->meta.msec = 0;

	/* Handles: `.Dt' 
	 *   --> title = unknown, volume = local, msec = 0, arch = NULL
	 */

	if (NULL == (nn = n->child)) {
		/* XXX: make these macro values. */
		m->meta.title = mandoc_strdup("unknown");
		m->meta.vol = mandoc_strdup("local");
		return(post_prol(m, n));
	}

	/* Handles: `.Dt TITLE' 
	 *   --> title = TITLE, volume = local, msec = 0, arch = NULL
	 */

	m->meta.title = mandoc_strdup(nn->string);

	if (NULL == (nn = nn->next)) {
		/* XXX: make this a macro value. */
		m->meta.vol = mandoc_strdup("local");
		return(post_prol(m, n));
	}

	/* Handles: `.Dt TITLE SEC'
	 *   --> title = TITLE, volume = SEC is msec ? 
	 *           format(msec) : SEC,
	 *       msec = SEC is msec ? atoi(msec) : 0,
	 *       arch = NULL
	 */

	cp = mdoc_a2msec(nn->string);
	if (cp) {
		/* FIXME: where is strtonum!? */
		m->meta.vol = mandoc_strdup(cp);
		lval = strtol(nn->string, &ep, 10);
		if (nn->string[0] != '\0' && *ep == '\0')
			m->meta.msec = (int)lval;
	} else 
		m->meta.vol = mandoc_strdup(nn->string);

	if (NULL == (nn = nn->next))
		return(post_prol(m, n));

	/* Handles: `.Dt TITLE SEC VOL'
	 *   --> title = TITLE, volume = VOL is vol ?
	 *       format(VOL) : 
	 *           VOL is arch ? format(arch) : 
	 *               VOL
	 */

	cp = mdoc_a2vol(nn->string);
	if (cp) {
		free(m->meta.vol);
		m->meta.vol = mandoc_strdup(cp);
	} else {
		cp = mdoc_a2arch(nn->string);
		if (NULL == cp) {
			free(m->meta.vol);
			m->meta.vol = mandoc_strdup(nn->string);
		} else 
			m->meta.arch = mandoc_strdup(cp);
	}	

	/* Ignore any subsequent parameters... */
	/* FIXME: warn about subsequent parameters. */

	return(post_prol(m, n));
}


/*
 * Set the operating system by way of the `Os' macro.  Note that if an
 * argument isn't provided and -DOSNAME="\"foo\"" is provided during
 * compilation, this value will be used instead of filling in "sysname
 * release" from uname().
 */
static int
post_os(POST_ARGS)
{
	char		  buf[BUFSIZ];
#ifndef OSNAME
	struct utsname	  utsname;
#endif

	if (m->meta.os)
		free(m->meta.os);

	if ( ! concat(m, buf, n->child, BUFSIZ))
		return(0);

	if ('\0' == buf[0]) {
#ifdef OSNAME
		if (strlcat(buf, OSNAME, BUFSIZ) >= BUFSIZ)
			return(mdoc_nerr(m, n, EUTSNAME));
#else /*!OSNAME */
		if (-1 == uname(&utsname))
			return(mdoc_nerr(m, n, EUTSNAME));
		if (strlcat(buf, utsname.sysname, BUFSIZ) >= BUFSIZ)
			return(mdoc_nerr(m, n, ETOOLONG));
		if (strlcat(buf, " ", 64) >= BUFSIZ)
			return(mdoc_nerr(m, n, ETOOLONG));
		if (strlcat(buf, utsname.release, BUFSIZ) >= BUFSIZ)
			return(mdoc_nerr(m, n, ETOOLONG));
#endif /*!OSNAME*/
	}

	m->meta.os = mandoc_strdup(buf);
	return(post_prol(m, n));
}


/*
 * Calculate the -width for a `Bl -tag' list if it hasn't been provided.
 * Uses the first head macro.  NOTE AGAIN: this is ONLY if the -width
 * argument has NOT been provided.  See post_bl_width() for converting
 * the -width string.
 */
static int
post_bl_tagwidth(POST_ARGS)
{
	struct mdoc_node *nn;
	size_t		  sz;
	int		  i;
	char		  buf[NUMSIZ];

	/* Defaults to ten ens. */

	sz = 10; /* XXX: make this a macro value. */
	nn = n->body->child;

	if (nn) {
		assert(MDOC_BLOCK == nn->type);
		assert(MDOC_It == nn->tok);
		nn = nn->head->child;
		if (MDOC_TEXT != nn->type) {
			sz = mdoc_macro2len(nn->tok);
			if (sz == 0) {
				if ( ! mdoc_nwarn(m, n, ENOWIDTH))
					return(0);
				sz = 10;
			}
		} else
			sz = strlen(nn->string) + 1;
	} 

	snprintf(buf, NUMSIZ, "%zun", sz);

	/*
	 * We have to dynamically add this to the macro's argument list.
	 * We're guaranteed that a MDOC_Width doesn't already exist.
	 */

	nn = n;
	assert(nn->args);
	i = (int)(nn->args->argc)++;

	nn->args->argv = mandoc_realloc(nn->args->argv, 
			nn->args->argc * sizeof(struct mdoc_argv));

	nn->args->argv[i].arg = MDOC_Width;
	nn->args->argv[i].line = n->line;
	nn->args->argv[i].pos = n->pos;
	nn->args->argv[i].sz = 1;
	nn->args->argv[i].value = mandoc_malloc(sizeof(char *));
	nn->args->argv[i].value[0] = mandoc_strdup(buf);
	return(1);
}


/*
 * Calculate the real width of a list from the -width string, which may
 * contain a macro (with a known default width), a literal string, or a
 * scaling width.
 */
static int
post_bl_width(POST_ARGS)
{
	size_t		  width;
	int		  i;
	enum mdoct	  tok;
	char		  buf[NUMSIZ];
	char		 *p;

	if (NULL == n->args)
		return(1);

	for (i = 0; i < (int)n->args->argc; i++)
		if (MDOC_Width == n->args->argv[i].arg)
			break;

	if (i == (int)n->args->argc)
		return(1);
	p = n->args->argv[i].value[0];

	/*
	 * If the value to -width is a macro, then we re-write it to be
	 * the macro's width as set in share/tmac/mdoc/doc-common.
	 */

	if (0 == strcmp(p, "Ds"))
		/* XXX: make into a macro. */
		width = 6;
	else if (MDOC_MAX == (tok = mdoc_hash_find(p)))
		return(1);
	else if (0 == (width = mdoc_macro2len(tok))) 
		return(mdoc_nwarn(m, n, ENOWIDTH));

	/* The value already exists: free and reallocate it. */

	snprintf(buf, NUMSIZ, "%zun", width);
	free(n->args->argv[i].value[0]);
	n->args->argv[i].value[0] = mandoc_strdup(buf);
	return(1);
}


/*
 * Do processing for -column lists, which can have two distinct styles
 * of invocation.  Merge this two styles into a consistent form.
 */
/* ARGSUSED */
static int
post_bl_head(POST_ARGS)
{
	int			 i, c;
	struct mdoc_node	*np, *nn, *nnp;

	if (NULL == n->child)
		return(1);

	np = n->parent;
	assert(np->args);

	for (c = 0; c < (int)np->args->argc; c++) 
		if (MDOC_Column == np->args->argv[c].arg)
			break;

	if (c == (int)np->args->argc)
		return(1);
	assert(0 == np->args->argv[c].sz);

	/*
	 * Accomodate for new-style groff column syntax.  Shuffle the
	 * child nodes, all of which must be TEXT, as arguments for the
	 * column field.  Then, delete the head children.
	 */

	np->args->argv[c].sz = (size_t)n->nchild;
	np->args->argv[c].value = mandoc_malloc
		((size_t)n->nchild * sizeof(char *));

	for (i = 0, nn = n->child; nn; i++) {
		np->args->argv[c].value[i] = nn->string;
		nn->string = NULL;
		nnp = nn;
		nn = nn->next;
		mdoc_node_delete(NULL, nnp);
	}

	n->nchild = 0;
	n->child = NULL;
	return(1);
}


static int
post_bl(POST_ARGS)
{
	int		  i, r, len;

	if (MDOC_HEAD == n->type)
		return(post_bl_head(m, n));
	if (MDOC_BLOCK != n->type)
		return(1);

	/*
	 * These are fairly complicated, so we've broken them into two
	 * functions.  post_bl_tagwidth() is called when a -tag is
	 * specified, but no -width (it must be guessed).  The second
	 * when a -width is specified (macro indicators must be
	 * rewritten into real lengths).
	 */

	len = (int)(n->args ? n->args->argc : 0);

	for (r = i = 0; i < len; i++) {
		if (MDOC_Tag == n->args->argv[i].arg)
			r |= 1 << 0;
		if (MDOC_Width == n->args->argv[i].arg)
			r |= 1 << 1;
	}

	if (r & (1 << 0) && ! (r & (1 << 1))) {
		if ( ! post_bl_tagwidth(m, n))
			return(0);
	} else if (r & (1 << 1))
		if ( ! post_bl_width(m, n))
			return(0);

	return(1);
}


/*
 * The `Pa' macro defaults to a tilde if no value is provided as an
 * argument.
 */
static int
post_pa(POST_ARGS)
{
	struct mdoc_node *np;

	if (n->child)
		return(1);
	
	np = n;
	m->next = MDOC_NEXT_CHILD;
	/* XXX: make into macro value. */
	if ( ! mdoc_word_alloc(m, n->line, n->pos, "~"))
		return(0);
	m->last = np;
	return(1);
}


/*
 * The `Ar' macro defaults to two strings "file ..." if no value is
 * provided as an argument.
 */
static int
post_ar(POST_ARGS)
{
	struct mdoc_node *np;

	if (n->child)
		return(1);
	
	np = n;
	m->next = MDOC_NEXT_CHILD;
	/* XXX: make into macro values. */
	if ( ! mdoc_word_alloc(m, n->line, n->pos, "file"))
		return(0);
	if ( ! mdoc_word_alloc(m, n->line, n->pos, "..."))
		return(0);
	m->last = np;
	return(1);
}


/*
 * Parse the date field in `Dd'.
 */
static int
post_dd(POST_ARGS)
{
	char		buf[DATESIZ];

	if ( ! concat(m, buf, n->child, DATESIZ))
		return(0);

	m->meta.date = mandoc_a2time
		(MTIME_MDOCDATE | MTIME_CANONICAL, buf);

	if (0 == m->meta.date) {
		if ( ! mdoc_nwarn(m, n, EBADDATE))
			return(0);
		m->meta.date = time(NULL);
	}

	return(post_prol(m, n));
}


/*
 * Remove prologue macros from the document after they're processed.
 * The final document uses mdoc_meta for these values and discards the
 * originals.
 */
static int
post_prol(POST_ARGS)
{

	mdoc_node_delete(m, n);
	if (m->meta.title && m->meta.date && m->meta.os)
		m->flags |= MDOC_PBODY;
	return(1);
}


/*
 * Trigger a literal context.
 */
static int
pre_dl(PRE_ARGS)
{

	if (MDOC_BODY == n->type)
		m->flags |= MDOC_LITERAL;
	return(1);
}


/* ARGSUSED */
static int
pre_offset(PRE_ARGS)
{
	int		 i;

	/* 
	 * Make sure that an empty offset produces an 8n length space as
	 * stipulated by mdoc.samples. 
	 */

	assert(n->args);
	for (i = 0; i < (int)n->args->argc; i++) {
		if (MDOC_Offset != n->args->argv[i].arg) 
			continue;
		if (n->args->argv[i].sz)
			break;
		assert(1 == n->args->refcnt);
		/* If no value set, length of <string>. */
		n->args->argv[i].sz++;
		n->args->argv[i].value = mandoc_malloc(sizeof(char *));
		n->args->argv[i].value[0] = mandoc_strdup("8n");
		break;
	}

	return(1);
}


static int
pre_bl(PRE_ARGS)
{

	return(MDOC_BLOCK == n->type ? pre_offset(m, n) : 1);
}


static int
pre_bd(PRE_ARGS)
{
	int		 i;

	if (MDOC_BLOCK == n->type)
		return(pre_offset(m, n));
	if (MDOC_BODY != n->type)
		return(1);

	/* Enter literal context if `Bd -literal' or `-unfilled'. */

	for (n = n->parent, i = 0; i < (int)n->args->argc; i++)
		if (MDOC_Literal == n->args->argv[i].arg)
			m->flags |= MDOC_LITERAL;
		else if (MDOC_Unfilled == n->args->argv[i].arg)
			m->flags |= MDOC_LITERAL;

	return(1);
}


static int
post_display(POST_ARGS)
{

	if (MDOC_BODY == n->type)
		m->flags &= ~MDOC_LITERAL;
	return(1);
}


static inline int
order_rs(enum mdoct t)
{
	int		i;

	for (i = 0; i < (int)RSORD_MAX; i++)
		if (rsord[i] == t)
			return(i);

	abort();
	/* NOTREACHED */
}


/* ARGSUSED */
static int
post_rs(POST_ARGS)
{
	struct mdoc_node	*nn, *next, *prev;
	int			 o;

	if (MDOC_BLOCK != n->type)
		return(1);

	assert(n->body->child);
	for (next = NULL, nn = n->body->child->next; nn; nn = next) {
		o = order_rs(nn->tok);

		/* Remove `nn' from the chain. */
		next = nn->next;
		if (next)
			next->prev = nn->prev;

		prev = nn->prev;
		if (prev)
			prev->next = nn->next;

		nn->prev = nn->next = NULL;

		/* 
		 * Scan back until we reach a node that's ordered before
		 * us, then set ourselves as being the next. 
		 */
		for ( ; prev; prev = prev->prev)
			if (order_rs(prev->tok) <= o)
				break;

		nn->prev = prev;
		if (prev) {
			if (prev->next)
				prev->next->prev = nn;
			nn->next = prev->next;
			prev->next = nn;
			continue;
		} 

		n->body->child->prev = nn;
		nn->next = n->body->child;
		n->body->child = nn;
	}
	return(1);
}
