/*	$Id: mdoc_argv.c,v 1.38 2011/05/29 21:22:18 schwarze Exp $ */
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

#include "mdoc.h"
#include "mandoc.h"
#include "libmdoc.h"
#include "libmandoc.h"

#define	MULTI_STEP	 5 /* pre-allocate argument values */
#define	DELIMSZ	  	 6 /* max possible size of a delimiter */

enum	argsflag {
	ARGSFL_NONE = 0,
	ARGSFL_DELIM, /* handle delimiters of [[::delim::][ ]+]+ */
	ARGSFL_TABSEP /* handle tab/`Ta' separated phrases */
};

enum	argvflag {
	ARGV_NONE, /* no args to flag (e.g., -split) */
	ARGV_SINGLE, /* one arg to flag (e.g., -file xxx)  */
	ARGV_MULTI, /* multiple args (e.g., -column xxx yyy) */
	ARGV_OPT_SINGLE /* optional arg (e.g., -offset [xxx]) */
};

static	enum mdocargt	 argv_a2arg(enum mdoct, const char *);
static	enum margserr	 args(struct mdoc *, int, int *, 
				char *, enum argsflag, char **);
static	int		 args_checkpunct(const char *, int);
static	int		 argv(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	int		 argv_single(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	int		 argv_opt_single(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	int		 argv_multi(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	void		 argn_free(struct mdoc_arg *, int);

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

static	const enum argsflag argflags[MDOC_MAX] = {
	ARGSFL_NONE, /* Ap */
	ARGSFL_NONE, /* Dd */
	ARGSFL_NONE, /* Dt */
	ARGSFL_NONE, /* Os */
	ARGSFL_NONE, /* Sh */
	ARGSFL_NONE, /* Ss */ 
	ARGSFL_NONE, /* Pp */ 
	ARGSFL_DELIM, /* D1 */
	ARGSFL_DELIM, /* Dl */
	ARGSFL_NONE, /* Bd */
	ARGSFL_NONE, /* Ed */
	ARGSFL_NONE, /* Bl */
	ARGSFL_NONE, /* El */
	ARGSFL_NONE, /* It */
	ARGSFL_DELIM, /* Ad */ 
	ARGSFL_DELIM, /* An */
	ARGSFL_DELIM, /* Ar */
	ARGSFL_NONE, /* Cd */
	ARGSFL_DELIM, /* Cm */
	ARGSFL_DELIM, /* Dv */ 
	ARGSFL_DELIM, /* Er */ 
	ARGSFL_DELIM, /* Ev */ 
	ARGSFL_NONE, /* Ex */
	ARGSFL_DELIM, /* Fa */ 
	ARGSFL_NONE, /* Fd */ 
	ARGSFL_DELIM, /* Fl */
	ARGSFL_DELIM, /* Fn */ 
	ARGSFL_DELIM, /* Ft */ 
	ARGSFL_DELIM, /* Ic */ 
	ARGSFL_NONE, /* In */ 
	ARGSFL_DELIM, /* Li */
	ARGSFL_NONE, /* Nd */ 
	ARGSFL_DELIM, /* Nm */ 
	ARGSFL_DELIM, /* Op */
	ARGSFL_NONE, /* Ot */
	ARGSFL_DELIM, /* Pa */
	ARGSFL_NONE, /* Rv */
	ARGSFL_DELIM, /* St */ 
	ARGSFL_DELIM, /* Va */
	ARGSFL_DELIM, /* Vt */ 
	ARGSFL_DELIM, /* Xr */
	ARGSFL_NONE, /* %A */
	ARGSFL_NONE, /* %B */
	ARGSFL_NONE, /* %D */
	ARGSFL_NONE, /* %I */
	ARGSFL_NONE, /* %J */
	ARGSFL_NONE, /* %N */
	ARGSFL_NONE, /* %O */
	ARGSFL_NONE, /* %P */
	ARGSFL_NONE, /* %R */
	ARGSFL_NONE, /* %T */
	ARGSFL_NONE, /* %V */
	ARGSFL_DELIM, /* Ac */
	ARGSFL_NONE, /* Ao */
	ARGSFL_DELIM, /* Aq */
	ARGSFL_DELIM, /* At */
	ARGSFL_DELIM, /* Bc */
	ARGSFL_NONE, /* Bf */ 
	ARGSFL_NONE, /* Bo */
	ARGSFL_DELIM, /* Bq */
	ARGSFL_DELIM, /* Bsx */
	ARGSFL_DELIM, /* Bx */
	ARGSFL_NONE, /* Db */
	ARGSFL_DELIM, /* Dc */
	ARGSFL_NONE, /* Do */
	ARGSFL_DELIM, /* Dq */
	ARGSFL_DELIM, /* Ec */
	ARGSFL_NONE, /* Ef */
	ARGSFL_DELIM, /* Em */ 
	ARGSFL_NONE, /* Eo */
	ARGSFL_DELIM, /* Fx */
	ARGSFL_DELIM, /* Ms */
	ARGSFL_DELIM, /* No */
	ARGSFL_DELIM, /* Ns */
	ARGSFL_DELIM, /* Nx */
	ARGSFL_DELIM, /* Ox */
	ARGSFL_DELIM, /* Pc */
	ARGSFL_DELIM, /* Pf */
	ARGSFL_NONE, /* Po */
	ARGSFL_DELIM, /* Pq */
	ARGSFL_DELIM, /* Qc */
	ARGSFL_DELIM, /* Ql */
	ARGSFL_NONE, /* Qo */
	ARGSFL_DELIM, /* Qq */
	ARGSFL_NONE, /* Re */
	ARGSFL_NONE, /* Rs */
	ARGSFL_DELIM, /* Sc */
	ARGSFL_NONE, /* So */
	ARGSFL_DELIM, /* Sq */
	ARGSFL_NONE, /* Sm */
	ARGSFL_DELIM, /* Sx */
	ARGSFL_DELIM, /* Sy */
	ARGSFL_DELIM, /* Tn */
	ARGSFL_DELIM, /* Ux */
	ARGSFL_DELIM, /* Xc */
	ARGSFL_NONE, /* Xo */
	ARGSFL_NONE, /* Fo */ 
	ARGSFL_NONE, /* Fc */ 
	ARGSFL_NONE, /* Oo */
	ARGSFL_DELIM, /* Oc */
	ARGSFL_NONE, /* Bk */
	ARGSFL_NONE, /* Ek */
	ARGSFL_NONE, /* Bt */
	ARGSFL_NONE, /* Hf */
	ARGSFL_NONE, /* Fr */
	ARGSFL_NONE, /* Ud */
	ARGSFL_NONE, /* Lb */
	ARGSFL_NONE, /* Lp */
	ARGSFL_DELIM, /* Lk */
	ARGSFL_DELIM, /* Mt */
	ARGSFL_DELIM, /* Brq */
	ARGSFL_NONE, /* Bro */
	ARGSFL_DELIM, /* Brc */
	ARGSFL_NONE, /* %C */
	ARGSFL_NONE, /* Es */
	ARGSFL_NONE, /* En */
	ARGSFL_NONE, /* Dx */
	ARGSFL_NONE, /* %Q */
	ARGSFL_NONE, /* br */
	ARGSFL_NONE, /* sp */
	ARGSFL_NONE, /* %U */
	ARGSFL_NONE, /* Ta */
};

static	const enum mdocargt args_Ex[] = {
	MDOC_Std,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_An[] = {
	MDOC_Split,
	MDOC_Nosplit,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bd[] = {
	MDOC_Ragged,
	MDOC_Unfilled,
	MDOC_Filled,
	MDOC_Literal,
	MDOC_File,
	MDOC_Offset,
	MDOC_Compact,
	MDOC_Centred,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bf[] = {
	MDOC_Emphasis,
	MDOC_Literal,
	MDOC_Symbolic,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bk[] = {
	MDOC_Words,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bl[] = {
	MDOC_Bullet,
	MDOC_Dash,
	MDOC_Hyphen,
	MDOC_Item,
	MDOC_Enum,
	MDOC_Tag,
	MDOC_Diag,
	MDOC_Hang,
	MDOC_Ohang,
	MDOC_Inset,
	MDOC_Column,
	MDOC_Width,
	MDOC_Offset,
	MDOC_Compact,
	MDOC_Nested,
	MDOC_ARG_MAX
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
		argn_free(p, i);

	free(p->argv);
	free(p);
}

static void
argn_free(struct mdoc_arg *p, int iarg)
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
mdoc_zargs(struct mdoc *m, int line, int *pos, char *buf, char **v)
{

	return(args(m, line, pos, buf, ARGSFL_NONE, v));
}

enum margserr
mdoc_args(struct mdoc *m, int line, int *pos, 
		char *buf, enum mdoct tok, char **v)
{
	enum argsflag	  fl;
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
			if (LIST_column == n->norm->Bl.type) {
				fl = ARGSFL_TABSEP;
				break;
			}

	return(args(m, line, pos, buf, fl, v));
}

static enum margserr
args(struct mdoc *m, int line, int *pos, 
		char *buf, enum argsflag fl, char **v)
{
	char		*p, *pp;
	enum margserr	 rc;

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

	if (ARGSFL_DELIM == fl)
		if (args_checkpunct(buf, *pos))
			return(ARGS_PUNCT);

	/*
	 * First handle TABSEP items, restricted to `Bl -column'.  This
	 * ignores conventional token parsing and instead uses tabs or
	 * `Ta' macros to separate phrases.  Phrases are parsed again
	 * for arguments at a later phase.
	 */

	if (ARGSFL_TABSEP == fl) {
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
		if ('\0' == *p && ' ' == *(p - 1))
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
			if (MDOC_PPHRASE & m->flags)
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

		if ('\0' == buf[*pos])
			mdoc_pmsg(m, line, *pos, MANDOCERR_EOLNSPACE);

		return(ARGS_QWORD);
	}

	p = &buf[*pos];
	*v = mandoc_getarg(m->parse, &p, line, pos);

	return(ARGS_WORD);
}

/* 
 * Check if the string consists only of space-separated closing
 * delimiters.  This is a bit of a dance: the first must be a close
 * delimiter, but it may be followed by middle delimiters.  Arbitrary
 * whitespace may separate these tokens.
 */
static int
args_checkpunct(const char *buf, int i)
{
	int		 j;
	char		 dbuf[DELIMSZ];
	enum mdelim	 d;

	/* First token must be a close-delimiter. */

	for (j = 0; buf[i] && ' ' != buf[i] && j < DELIMSZ; j++, i++)
		dbuf[j] = buf[i];

	if (DELIMSZ == j)
		return(0);

	dbuf[j] = '\0';
	if (DELIM_CLOSE != mdoc_isdelim(dbuf))
		return(0);

	while (' ' == buf[i])
		i++;

	/* Remaining must NOT be open/none. */
	
	while (buf[i]) {
		j = 0;
		while (buf[i] && ' ' != buf[i] && j < DELIMSZ)
			dbuf[j++] = buf[i++];

		if (DELIMSZ == j)
			return(0);

		dbuf[j] = '\0';
		d = mdoc_isdelim(dbuf);
		if (DELIM_NONE == d || DELIM_OPEN == d)
			return(0);

		while (' ' == buf[i])
			i++;
	}

	return('\0' == buf[i]);
}

/*
 * Match up an argument string (e.g., `-foo bar' having "foo") with the
 * correct identifier.  It must apply to the given macro.  If none was
 * found (including bad matches), return MDOC_ARG_MAX.
 */
static enum mdocargt
argv_a2arg(enum mdoct tok, const char *p)
{
	const enum mdocargt *argsp;

	argsp = NULL;

	switch (tok) {
	case (MDOC_An):
		argsp = args_An;
		break;
	case (MDOC_Bd):
		argsp = args_Bd;
		break;
	case (MDOC_Bf):
		argsp = args_Bf;
		break;
	case (MDOC_Bk):
		argsp = args_Bk;
		break;
	case (MDOC_Bl):
		argsp = args_Bl;
		break;
	case (MDOC_Rv):
		/* FALLTHROUGH */
	case (MDOC_Ex):
		argsp = args_Ex;
		break;
	default:
		return(MDOC_ARG_MAX);
	}

	assert(argsp);

	for ( ; MDOC_ARG_MAX != *argsp ; argsp++)
		if (0 == strcmp(p, mdoc_argnames[*argsp]))
			return(*argsp);

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
		ac = args(m, line, pos, buf, ARGSFL_NONE, &p);
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

	ac = args(m, line, pos, buf, ARGSFL_NONE, &p);
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

	ac = args(m, line, pos, buf, ARGSFL_NONE, &p);
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
