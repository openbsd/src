/*	$Id: mdoc_argv.c,v 1.36 2011/04/21 22:59:54 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "libmdoc.h"
#include "libmandoc.h"

#define	MULTI_STEP	 5 /* pre-allocate argument values */

static	enum mdocargt	 argv_a2arg(enum mdoct, const char *);
static	enum margserr	 args(struct mdoc *, int, int *, 
				char *, int, char **);
static	int		 args_checkpunct(const char *);
static	int		 argv(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	int		 argv_single(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	int		 argv_opt_single(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	int		 argv_multi(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);

enum	argvflag {
	ARGV_NONE, /* no args to flag (e.g., -split) */
	ARGV_SINGLE, /* one arg to flag (e.g., -file xxx)  */
	ARGV_MULTI, /* multiple args (e.g., -column xxx yyy) */
	ARGV_OPT_SINGLE /* optional arg (e.g., -offset [xxx]) */
};

static	const enum argvflag argvflags[MDOC_ARG_MAX] = {
	ARGV_NONE,	/* MDOC_Split */
	ARGV_NONE,	/* MDOC_Nosplit */
	ARGV_NONE,	/* MDOC_Ragged */
	ARGV_NONE,	/* MDOC_Unfilled */
	ARGV_NONE,	/* MDOC_Literal */
	ARGV_SINGLE,	/* MDOC_File */
	ARGV_OPT_SINGLE, /* MDOC_Offset */
	ARGV_NONE,	/* MDOC_Bullet */
	ARGV_NONE,	/* MDOC_Dash */
	ARGV_NONE,	/* MDOC_Hyphen */
	ARGV_NONE,	/* MDOC_Item */
	ARGV_NONE,	/* MDOC_Enum */
	ARGV_NONE,	/* MDOC_Tag */
	ARGV_NONE,	/* MDOC_Diag */
	ARGV_NONE,	/* MDOC_Hang */
	ARGV_NONE,	/* MDOC_Ohang */
	ARGV_NONE,	/* MDOC_Inset */
	ARGV_MULTI,	/* MDOC_Column */
	ARGV_SINGLE,	/* MDOC_Width */
	ARGV_NONE,	/* MDOC_Compact */
	ARGV_NONE,	/* MDOC_Std */
	ARGV_NONE,	/* MDOC_Filled */
	ARGV_NONE,	/* MDOC_Words */
	ARGV_NONE,	/* MDOC_Emphasis */
	ARGV_NONE,	/* MDOC_Symbolic */
	ARGV_NONE	/* MDOC_Symbolic */
};

static	const int argflags[MDOC_MAX] = {
	0, /* Ap */
	0, /* Dd */
	0, /* Dt */
	0, /* Os */
	0, /* Sh */
	0, /* Ss */ 
	0, /* Pp */ 
	ARGS_DELIM, /* D1 */
	ARGS_DELIM, /* Dl */
	0, /* Bd */
	0, /* Ed */
	0, /* Bl */
	0, /* El */
	0, /* It */
	ARGS_DELIM, /* Ad */ 
	ARGS_DELIM, /* An */
	ARGS_DELIM, /* Ar */
	0, /* Cd */
	ARGS_DELIM, /* Cm */
	ARGS_DELIM, /* Dv */ 
	ARGS_DELIM, /* Er */ 
	ARGS_DELIM, /* Ev */ 
	0, /* Ex */
	ARGS_DELIM, /* Fa */ 
	0, /* Fd */ 
	ARGS_DELIM, /* Fl */
	ARGS_DELIM, /* Fn */ 
	ARGS_DELIM, /* Ft */ 
	ARGS_DELIM, /* Ic */ 
	0, /* In */ 
	ARGS_DELIM, /* Li */
	0, /* Nd */ 
	ARGS_DELIM, /* Nm */ 
	ARGS_DELIM, /* Op */
	0, /* Ot */
	ARGS_DELIM, /* Pa */
	0, /* Rv */
	ARGS_DELIM, /* St */ 
	ARGS_DELIM, /* Va */
	ARGS_DELIM, /* Vt */ 
	ARGS_DELIM, /* Xr */
	0, /* %A */
	0, /* %B */
	0, /* %D */
	0, /* %I */
	0, /* %J */
	0, /* %N */
	0, /* %O */
	0, /* %P */
	0, /* %R */
	0, /* %T */
	0, /* %V */
	ARGS_DELIM, /* Ac */
	0, /* Ao */
	ARGS_DELIM, /* Aq */
	ARGS_DELIM, /* At */
	ARGS_DELIM, /* Bc */
	0, /* Bf */ 
	0, /* Bo */
	ARGS_DELIM, /* Bq */
	ARGS_DELIM, /* Bsx */
	ARGS_DELIM, /* Bx */
	0, /* Db */
	ARGS_DELIM, /* Dc */
	0, /* Do */
	ARGS_DELIM, /* Dq */
	ARGS_DELIM, /* Ec */
	0, /* Ef */
	ARGS_DELIM, /* Em */ 
	0, /* Eo */
	ARGS_DELIM, /* Fx */
	ARGS_DELIM, /* Ms */
	ARGS_DELIM, /* No */
	ARGS_DELIM, /* Ns */
	ARGS_DELIM, /* Nx */
	ARGS_DELIM, /* Ox */
	ARGS_DELIM, /* Pc */
	ARGS_DELIM, /* Pf */
	0, /* Po */
	ARGS_DELIM, /* Pq */
	ARGS_DELIM, /* Qc */
	ARGS_DELIM, /* Ql */
	0, /* Qo */
	ARGS_DELIM, /* Qq */
	0, /* Re */
	0, /* Rs */
	ARGS_DELIM, /* Sc */
	0, /* So */
	ARGS_DELIM, /* Sq */
	0, /* Sm */
	ARGS_DELIM, /* Sx */
	ARGS_DELIM, /* Sy */
	ARGS_DELIM, /* Tn */
	ARGS_DELIM, /* Ux */
	ARGS_DELIM, /* Xc */
	0, /* Xo */
	0, /* Fo */ 
	0, /* Fc */ 
	0, /* Oo */
	ARGS_DELIM, /* Oc */
	0, /* Bk */
	0, /* Ek */
	0, /* Bt */
	0, /* Hf */
	0, /* Fr */
	0, /* Ud */
	0, /* Lb */
	0, /* Lp */
	ARGS_DELIM, /* Lk */
	ARGS_DELIM, /* Mt */
	ARGS_DELIM, /* Brq */
	0, /* Bro */
	ARGS_DELIM, /* Brc */
	0, /* %C */
	0, /* Es */
	0, /* En */
	0, /* Dx */
	0, /* %Q */
	0, /* br */
	0, /* sp */
	0, /* %U */
	0, /* Ta */
};

/*
 * Parse an argument from line text.  This comes in the form of -key
 * [value0...], which may either have a single mandatory value, at least
 * one mandatory value, an optional single value, or no value.
 */
enum margverr
mdoc_argv(struct mdoc *m, int line, enum mdoct tok,
		struct mdoc_arg **v, int *pos, char *buf)
{
	char		 *p, sv;
	struct mdoc_argv tmp;
	struct mdoc_arg	 *arg;

	if ('\0' == buf[*pos])
		return(ARGV_EOLN);

	assert(' ' != buf[*pos]);

	/* Parse through to the first unescaped space. */

	p = &buf[++(*pos)];

	assert(*pos > 0);

	/* LINTED */
	while (buf[*pos]) {
		if (' ' == buf[*pos])
			if ('\\' != buf[*pos - 1])
				break;
		(*pos)++;
	}

	/* XXX - save zeroed byte, if not an argument. */

	sv = '\0';
	if (buf[*pos]) {
		sv = buf[*pos];
		buf[(*pos)++] = '\0';
	}

	memset(&tmp, 0, sizeof(struct mdoc_argv));
	tmp.line = line;
	tmp.pos = *pos;

	/* See if our token accepts the argument. */

	if (MDOC_ARG_MAX == (tmp.arg = argv_a2arg(tok, p))) {
		/* XXX - restore saved zeroed byte. */
		if (sv)
			buf[*pos - 1] = sv;
		return(ARGV_WORD);
	}

	while (buf[*pos] && ' ' == buf[*pos])
		(*pos)++;

	if ( ! argv(m, line, &tmp, pos, buf))
		return(ARGV_ERROR);

	if (NULL == (arg = *v))
		arg = *v = mandoc_calloc(1, sizeof(struct mdoc_arg));

	arg->argc++;
	arg->argv = mandoc_realloc
		(arg->argv, arg->argc * sizeof(struct mdoc_argv));

	memcpy(&arg->argv[(int)arg->argc - 1], 
			&tmp, sizeof(struct mdoc_argv));

	return(ARGV_ARG);
}

void
mdoc_argv_free(struct mdoc_arg *p)
{
	int		 i;

	if (NULL == p)
		return;

	if (p->refcnt) {
		--(p->refcnt);
		if (p->refcnt)
			return;
	}
	assert(p->argc);

	for (i = (int)p->argc - 1; i >= 0; i--)
		mdoc_argn_free(p, i);

	free(p->argv);
	free(p);
}

void
mdoc_argn_free(struct mdoc_arg *p, int iarg)
{
	struct mdoc_argv *arg;
	int		  j;

	arg = &p->argv[iarg];

	if (arg->sz && arg->value) {
		for (j = (int)arg->sz - 1; j >= 0; j--) 
			free(arg->value[j]);
		free(arg->value);
	}

	for (--p->argc; iarg < (int)p->argc; iarg++)
		p->argv[iarg] = p->argv[iarg+1];
}

enum margserr
mdoc_zargs(struct mdoc *m, int line, int *pos, 
		char *buf, int flags, char **v)
{

	return(args(m, line, pos, buf, flags, v));
}

enum margserr
mdoc_args(struct mdoc *m, int line, int *pos, 
		char *buf, enum mdoct tok, char **v)
{
	int		  fl;
	struct mdoc_node *n;

	fl = argflags[tok];

	if (MDOC_It != tok)
		return(args(m, line, pos, buf, fl, v));

	/*
	 * We know that we're in an `It', so it's reasonable to expect
	 * us to be sitting in a `Bl'.  Someday this may not be the case
	 * (if we allow random `It's sitting out there), so provide a
	 * safe fall-back into the default behaviour.
	 */

	for (n = m->last; n; n = n->parent)
		if (MDOC_Bl == n->tok)
			break;

	if (n && LIST_column == n->norm->Bl.type) {
		fl |= ARGS_TABSEP;
		fl &= ~ARGS_DELIM;
	}

	return(args(m, line, pos, buf, fl, v));
}

static enum margserr
args(struct mdoc *m, int line, int *pos, 
		char *buf, int fl, char **v)
{
	int		 i;
	char		*p, *pp;
	enum margserr	 rc;

	/*
	 * Parse out the terms (like `val' in `.Xx -arg val' or simply
	 * `.Xx val'), which can have all sorts of properties:
	 *
	 *   ARGS_DELIM: use special handling if encountering trailing
	 *   delimiters in the form of [[::delim::][ ]+]+.
	 *
	 *   ARGS_NOWARN: don't post warnings.  This is only used when
	 *   re-parsing delimiters, as the warnings have already been
	 *   posted.
	 *
	 *   ARGS_TABSEP: use special handling for tab/`Ta' separated
	 *   phrases like in `Bl -column'.
	 */

	assert(' ' != buf[*pos]);

	if ('\0' == buf[*pos]) {
		if (MDOC_PPHRASE & m->flags)
			return(ARGS_EOLN);
		/*
		 * If we're not in a partial phrase and the flag for
		 * being a phrase literal is still set, the punctuation
		 * is unterminated.
		 */
		if (MDOC_PHRASELIT & m->flags)
			mdoc_pmsg(m, line, *pos, MANDOCERR_BADQUOTE);

		m->flags &= ~MDOC_PHRASELIT;
		return(ARGS_EOLN);
	}

	*v = &buf[*pos];

	if (ARGS_DELIM & fl && args_checkpunct(&buf[*pos])) {
		i = strlen(&buf[*pos]) + *pos;
		if (i && ' ' != buf[i - 1])
			return(ARGS_PUNCT);
		if (ARGS_NOWARN & fl)
			return(ARGS_PUNCT);
		mdoc_pmsg(m, line, *pos, MANDOCERR_EOLNSPACE);
		return(ARGS_PUNCT);
	}

	/*
	 * First handle TABSEP items, restricted to `Bl -column'.  This
	 * ignores conventional token parsing and instead uses tabs or
	 * `Ta' macros to separate phrases.  Phrases are parsed again
	 * for arguments at a later phase.
	 */

	if (ARGS_TABSEP & fl) {
		/* Scan ahead to tab (can't be escaped). */
		p = strchr(*v, '\t');
		pp = NULL;

		/* Scan ahead to unescaped `Ta'. */
		if ( ! (MDOC_PHRASELIT & m->flags)) 
			for (pp = *v; ; pp++) {
				if (NULL == (pp = strstr(pp, "Ta")))
					break;
				if (pp > *v && ' ' != *(pp - 1))
					continue;
				if (' ' == *(pp + 2) || '\0' == *(pp + 2))
					break;
			}

		/* By default, assume a phrase. */
		rc = ARGS_PHRASE;

		/* 
		 * Adjust new-buffer position to be beyond delimiter
		 * mark (e.g., Ta -> end + 2).
		 */
		if (p && pp) {
			*pos += pp < p ? 2 : 1;
			rc = pp < p ? ARGS_PHRASE : ARGS_PPHRASE;
			p = pp < p ? pp : p;
		} else if (p && ! pp) {
			rc = ARGS_PPHRASE;
			*pos += 1;
		} else if (pp && ! p) {
			p = pp;
			*pos += 2;
		} else {
			rc = ARGS_PEND;
			p = strchr(*v, 0);
		}

		/* Whitespace check for eoln case... */
		if ('\0' == *p && ' ' == *(p - 1) && ! (ARGS_NOWARN & fl))
			mdoc_pmsg(m, line, *pos, MANDOCERR_EOLNSPACE);

		*pos += (int)(p - *v);

		/* Strip delimiter's preceding whitespace. */
		pp = p - 1;
		while (pp > *v && ' ' == *pp) {
			if (pp > *v && '\\' == *(pp - 1))
				break;
			pp--;
		}
		*(pp + 1) = 0;

		/* Strip delimiter's proceeding whitespace. */
		for (pp = &buf[*pos]; ' ' == *pp; pp++, (*pos)++)
			/* Skip ahead. */ ;

		return(rc);
	} 

	/* 
	 * Process a quoted literal.  A quote begins with a double-quote
	 * and ends with a double-quote NOT preceded by a double-quote.
	 * Whitespace is NOT involved in literal termination.
	 */

	if (MDOC_PHRASELIT & m->flags || '\"' == buf[*pos]) {
		if ( ! (MDOC_PHRASELIT & m->flags))
			*v = &buf[++(*pos)];

		if (MDOC_PPHRASE & m->flags)
			m->flags |= MDOC_PHRASELIT;

		for ( ; buf[*pos]; (*pos)++) {
			if ('\"' != buf[*pos])
				continue;
			if ('\"' != buf[*pos + 1])
				break;
			(*pos)++;
		}

		if ('\0' == buf[*pos]) {
			if (ARGS_NOWARN & fl || MDOC_PPHRASE & m->flags)
				return(ARGS_QWORD);
			mdoc_pmsg(m, line, *pos, MANDOCERR_BADQUOTE);
			return(ARGS_QWORD);
		}

		m->flags &= ~MDOC_PHRASELIT;
		buf[(*pos)++] = '\0';

		if ('\0' == buf[*pos])
			return(ARGS_QWORD);

		while (' ' == buf[*pos])
			(*pos)++;

		if (0 == buf[*pos] && ! (ARGS_NOWARN & fl))
			mdoc_pmsg(m, line, *pos, MANDOCERR_EOLNSPACE);

		return(ARGS_QWORD);
	}

	/* 
	 * A non-quoted term progresses until either the end of line or
	 * a non-escaped whitespace.
	 */

	for ( ; buf[*pos]; (*pos)++)
		if (*pos && ' ' == buf[*pos] && '\\' != buf[*pos - 1])
			break;

	if ('\0' == buf[*pos])
		return(ARGS_WORD);

	buf[(*pos)++] = '\0';

	while (' ' == buf[*pos])
		(*pos)++;

	if ('\0' == buf[*pos] && ! (ARGS_NOWARN & fl))
		mdoc_pmsg(m, line, *pos, MANDOCERR_EOLNSPACE);

	return(ARGS_WORD);
}

/* 
 * Check if the string consists only of space-separated closing
 * delimiters.  This is a bit of a dance: the first must be a close
 * delimiter, but it may be followed by middle delimiters.  Arbitrary
 * whitespace may separate these tokens.
 */
static int
args_checkpunct(const char *p)
{
	int		 i, j;
	char		 buf[DELIMSZ];
	enum mdelim	 d;

	i = 0;

	/* First token must be a close-delimiter. */

	for (j = 0; p[i] && ' ' != p[i] && j < DELIMSZ; j++, i++)
		buf[j] = p[i];

	if (DELIMSZ == j)
		return(0);

	buf[j] = '\0';
	if (DELIM_CLOSE != mandoc_isdelim(buf))
		return(0);

	while (' ' == p[i])
		i++;

	/* Remaining must NOT be open/none. */
	
	while (p[i]) {
		j = 0;
		while (p[i] && ' ' != p[i] && j < DELIMSZ)
			buf[j++] = p[i++];

		if (DELIMSZ == j)
			return(0);

		buf[j] = '\0';
		d = mandoc_isdelim(buf);
		if (DELIM_NONE == d || DELIM_OPEN == d)
			return(0);

		while (' ' == p[i])
			i++;
	}

	return('\0' == p[i]);
}

/*
 * Match up an argument string (e.g., `-foo bar' having "foo") with the
 * correrct identifier.  It must apply to the given macro.  If none was
 * found (including bad matches), return MDOC_ARG_MAX.
 */
static enum mdocargt
argv_a2arg(enum mdoct tok, const char *p)
{
	enum mdocargt	 args[MDOC_ARG_MAX];
	int		 i, len;

	len = 0;

	switch (tok) {
	case (MDOC_An):
		args[len++] = MDOC_Split;
		args[len++] = MDOC_Nosplit;
		break;
	case (MDOC_Bd):
		args[len++] = MDOC_Ragged;
		args[len++] = MDOC_Unfilled;
		args[len++] = MDOC_Filled;
		args[len++] = MDOC_Literal;
		args[len++] = MDOC_File;
		args[len++] = MDOC_Offset;
		args[len++] = MDOC_Compact;
		args[len++] = MDOC_Centred;
		break;
	case (MDOC_Bf):
		args[len++] = MDOC_Emphasis;
		args[len++] = MDOC_Literal;
		args[len++] = MDOC_Symbolic;
		break;
	case (MDOC_Bk):
		args[len++] = MDOC_Words;
		break;
	case (MDOC_Bl):
		args[len++] = MDOC_Bullet;
		args[len++] = MDOC_Dash;
		args[len++] = MDOC_Hyphen;
		args[len++] = MDOC_Item;
		args[len++] = MDOC_Enum;
		args[len++] = MDOC_Tag;
		args[len++] = MDOC_Diag;
		args[len++] = MDOC_Hang;
		args[len++] = MDOC_Ohang;
		args[len++] = MDOC_Inset;
		args[len++] = MDOC_Column;
		args[len++] = MDOC_Width;
		args[len++] = MDOC_Offset;
		args[len++] = MDOC_Compact;
		args[len++] = MDOC_Nested;
		break;
	case (MDOC_Rv):
		/* FALLTHROUGH */
	case (MDOC_Ex):
		args[len++] = MDOC_Std;
		break;
	default:
		break;
	}

	for (i = 0; i < len; i++)
		if (0 == strcmp(p, mdoc_argnames[args[i]]))
			return(args[i]);

	return(MDOC_ARG_MAX);
}

static int
argv_multi(struct mdoc *m, int line, 
		struct mdoc_argv *v, int *pos, char *buf)
{
	enum margserr	 ac;
	char		*p;

	for (v->sz = 0; ; v->sz++) {
		if ('-' == buf[*pos])
			break;
		ac = args(m, line, pos, buf, 0, &p);
		if (ARGS_ERROR == ac)
			return(0);
		else if (ARGS_EOLN == ac)
			break;

		if (0 == v->sz % MULTI_STEP)
			v->value = mandoc_realloc(v->value, 
				(v->sz + MULTI_STEP) * sizeof(char *));

		v->value[(int)v->sz] = mandoc_strdup(p);
	}

	return(1);
}

static int
argv_opt_single(struct mdoc *m, int line, 
		struct mdoc_argv *v, int *pos, char *buf)
{
	enum margserr	 ac;
	char		*p;

	if ('-' == buf[*pos])
		return(1);

	ac = args(m, line, pos, buf, 0, &p);
	if (ARGS_ERROR == ac)
		return(0);
	if (ARGS_EOLN == ac)
		return(1);

	v->sz = 1;
	v->value = mandoc_malloc(sizeof(char *));
	v->value[0] = mandoc_strdup(p);

	return(1);
}

/*
 * Parse a single, mandatory value from the stream.
 */
static int
argv_single(struct mdoc *m, int line, 
		struct mdoc_argv *v, int *pos, char *buf)
{
	int		 ppos;
	enum margserr	 ac;
	char		*p;

	ppos = *pos;

	ac = args(m, line, pos, buf, 0, &p);
	if (ARGS_EOLN == ac) {
		mdoc_pmsg(m, line, ppos, MANDOCERR_SYNTARGVCOUNT);
		return(0);
	} else if (ARGS_ERROR == ac)
		return(0);

	v->sz = 1;
	v->value = mandoc_malloc(sizeof(char *));
	v->value[0] = mandoc_strdup(p);

	return(1);
}

/*
 * Determine rules for parsing arguments.  Arguments can either accept
 * no parameters, an optional single parameter, one parameter, or
 * multiple parameters.
 */
static int
argv(struct mdoc *mdoc, int line, 
		struct mdoc_argv *v, int *pos, char *buf)
{

	v->sz = 0;
	v->value = NULL;

	switch (argvflags[v->arg]) {
	case (ARGV_SINGLE):
		return(argv_single(mdoc, line, v, pos, buf));
	case (ARGV_MULTI):
		return(argv_multi(mdoc, line, v, pos, buf));
	case (ARGV_OPT_SINGLE):
		return(argv_opt_single(mdoc, line, v, pos, buf));
	case (ARGV_NONE):
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(1);
}
