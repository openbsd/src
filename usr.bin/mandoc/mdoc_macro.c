/*	$Id: mdoc_macro.c,v 1.42 2010/05/15 15:37:53 schwarze Exp $ */
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "libmdoc.h"
#include "libmandoc.h"

enum	rew {
	REWIND_REWIND,
	REWIND_NOHALT,
	REWIND_HALT
};

static	int	  	blk_full(MACRO_PROT_ARGS);
static	int	  	blk_exp_close(MACRO_PROT_ARGS);
static	int	  	blk_part_exp(MACRO_PROT_ARGS);
static	int	  	blk_part_imp(MACRO_PROT_ARGS);
static	int	  	ctx_synopsis(MACRO_PROT_ARGS);
static	int	  	in_line_eoln(MACRO_PROT_ARGS);
static	int	  	in_line_argn(MACRO_PROT_ARGS);
static	int	  	in_line(MACRO_PROT_ARGS);
static	int	  	obsolete(MACRO_PROT_ARGS);

static	int	  	append_delims(struct mdoc *, 
				int, int *, char *);
static	enum mdoct	lookup(enum mdoct, const char *);
static	enum mdoct	lookup_raw(const char *);
static	int	  	phrase(struct mdoc *, int, int, 
				char *, enum margserr);
static	enum mdoct 	rew_alt(enum mdoct);
static	int	  	rew_dobreak(enum mdoct, 
				const struct mdoc_node *);
static	enum rew  	rew_dohalt(enum mdoct, enum mdoc_type, 
				const struct mdoc_node *);
static	int	  	rew_elem(struct mdoc *, enum mdoct);
static	int	  	rew_last(struct mdoc *, 
				const struct mdoc_node *);
static	int	  	rew_sub(enum mdoc_type, struct mdoc *, 
				enum mdoct, int, int);
static	int	  	swarn(struct mdoc *, enum mdoc_type, int, 
				int, const struct mdoc_node *);

const	struct mdoc_macro __mdoc_macros[MDOC_MAX] = {
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ap */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dd */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dt */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Os */
	{ blk_full, 0 }, /* Sh */
	{ blk_full, 0 }, /* Ss */ 
	{ in_line_eoln, 0 }, /* Pp */ 
	{ blk_part_imp, MDOC_PARSED }, /* D1 */
	{ blk_part_imp, MDOC_PARSED }, /* Dl */
	{ blk_full, MDOC_EXPLICIT }, /* Bd */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ed */
	{ blk_full, MDOC_EXPLICIT }, /* Bl */
	{ blk_exp_close, MDOC_EXPLICIT }, /* El */
	{ blk_full, MDOC_PARSED }, /* It */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ad */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* An */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ar */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Cd */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Cm */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Dv */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Er */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ev */ 
	{ in_line_eoln, 0 }, /* Ex */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fa */ 
	{ in_line_eoln, 0 }, /* Fd */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fl */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fn */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ft */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ic */ 
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* In */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Li */
	{ blk_full, 0 }, /* Nd */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Nm */ 
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Op */
	{ obsolete, 0 }, /* Ot */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Pa */
	{ in_line_eoln, 0 }, /* Rv */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* St */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Va */
	{ ctx_synopsis, MDOC_CALLABLE | MDOC_PARSED }, /* Vt */ 
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Xr */
	{ in_line_eoln, 0 }, /* %A */
	{ in_line_eoln, 0 }, /* %B */
	{ in_line_eoln, 0 }, /* %D */
	{ in_line_eoln, 0 }, /* %I */
	{ in_line_eoln, 0 }, /* %J */
	{ in_line_eoln, 0 }, /* %N */
	{ in_line_eoln, 0 }, /* %O */
	{ in_line_eoln, 0 }, /* %P */
	{ in_line_eoln, 0 }, /* %R */
	{ in_line_eoln, 0 }, /* %T */
	{ in_line_eoln, 0 }, /* %V */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Ac */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Ao */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Aq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* At */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Bc */
	{ blk_full, MDOC_EXPLICIT }, /* Bf */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Bo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Bq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bsx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bx */
	{ in_line_eoln, 0 }, /* Db */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Dc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Do */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Dq */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Ec */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ef */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Em */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Eo */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Fx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ms */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* No */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ns */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Nx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ox */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Pc */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED | MDOC_IGNDELIM }, /* Pf */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Po */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Pq */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Qc */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Ql */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Qo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Qq */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Re */
	{ blk_full, MDOC_EXPLICIT }, /* Rs */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Sc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* So */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Sq */
	{ in_line_eoln, 0 }, /* Sm */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Sx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Sy */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Tn */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ux */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Xc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Xo */
	{ blk_full, MDOC_EXPLICIT | MDOC_CALLABLE }, /* Fo */ 
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Fc */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Oo */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Oc */
	{ blk_full, MDOC_EXPLICIT }, /* Bk */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ek */
	{ in_line_eoln, 0 }, /* Bt */
	{ in_line_eoln, 0 }, /* Hf */
	{ obsolete, 0 }, /* Fr */
	{ in_line_eoln, 0 }, /* Ud */
	{ in_line_eoln, 0 }, /* Lb */
	{ in_line_eoln, 0 }, /* Lp */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Lk */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Mt */ 
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Brq */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Bro */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Brc */
	{ in_line_eoln, 0 }, /* %C */
	{ obsolete, 0 }, /* Es */
	{ obsolete, 0 }, /* En */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Dx */
	{ in_line_eoln, 0 }, /* %Q */
	{ in_line_eoln, 0 }, /* br */
	{ in_line_eoln, 0 }, /* sp */
	{ in_line_eoln, 0 }, /* %U */
};

const	struct mdoc_macro * const mdoc_macros = __mdoc_macros;


static int
swarn(struct mdoc *mdoc, enum mdoc_type type, 
		int line, int pos, const struct mdoc_node *p)
{
	const char	*n, *t, *tt;

	n = t = "<root>";
	tt = "block";

	switch (type) {
	case (MDOC_BODY):
		tt = "multi-line";
		break;
	case (MDOC_HEAD):
		tt = "line";
		break;
	default:
		break;
	}

	switch (p->type) {
	case (MDOC_BLOCK):
		n = mdoc_macronames[p->tok];
		t = "block";
		break;
	case (MDOC_BODY):
		n = mdoc_macronames[p->tok];
		t = "multi-line";
		break;
	case (MDOC_HEAD):
		n = mdoc_macronames[p->tok];
		t = "line";
		break;
	default:
		break;
	}

	if ( ! (MDOC_IGN_SCOPE & mdoc->pflags))
		return(mdoc_verr(mdoc, line, pos, 
				"%s scope breaks %s scope of %s", 
				tt, t, n));
	return(mdoc_vwarn(mdoc, line, pos, 
				"%s scope breaks %s scope of %s", 
				tt, t, n));
}


/*
 * This is called at the end of parsing.  It must traverse up the tree,
 * closing out open [implicit] scopes.  Obviously, open explicit scopes
 * are errors.
 */
int
mdoc_macroend(struct mdoc *m)
{
	struct mdoc_node *n;

	/* Scan for open explicit scopes. */

	n = MDOC_VALID & m->last->flags ?  m->last->parent : m->last;

	for ( ; n; n = n->parent) {
		if (MDOC_BLOCK != n->type)
			continue;
		if ( ! (MDOC_EXPLICIT & mdoc_macros[n->tok].flags))
			continue;
		return(mdoc_nerr(m, n, EOPEN));
	}

	/* Rewind to the first. */

	return(rew_last(m, m->first));
}


/*
 * Look up a macro from within a subsequent context.
 */
static enum mdoct
lookup(enum mdoct from, const char *p)
{
	/* FIXME: make -diag lists be un-PARSED. */

	if ( ! (MDOC_PARSED & mdoc_macros[from].flags))
		return(MDOC_MAX);
	return(lookup_raw(p));
}


/*
 * Lookup a macro following the initial line macro.
 */
static enum mdoct
lookup_raw(const char *p)
{
	enum mdoct	 res;

	if (MDOC_MAX == (res = mdoc_hash_find(p)))
		return(MDOC_MAX);
	if (MDOC_CALLABLE & mdoc_macros[res].flags)
		return(res);
	return(MDOC_MAX);
}


static int
rew_last(struct mdoc *mdoc, const struct mdoc_node *to)
{

	assert(to);
	mdoc->next = MDOC_NEXT_SIBLING;

	/* LINTED */
	while (mdoc->last != to) {
		if ( ! mdoc_valid_post(mdoc))
			return(0);
		if ( ! mdoc_action_post(mdoc))
			return(0);
		mdoc->last = mdoc->last->parent;
		assert(mdoc->last);
	}

	if ( ! mdoc_valid_post(mdoc))
		return(0);
	return(mdoc_action_post(mdoc));
}


/*
 * Return the opening macro of a closing one, e.g., `Ec' has `Eo' as its
 * matching pair.
 */
static enum mdoct
rew_alt(enum mdoct tok)
{
	switch (tok) {
	case (MDOC_Ac):
		return(MDOC_Ao);
	case (MDOC_Bc):
		return(MDOC_Bo);
	case (MDOC_Brc):
		return(MDOC_Bro);
	case (MDOC_Dc):
		return(MDOC_Do);
	case (MDOC_Ec):
		return(MDOC_Eo);
	case (MDOC_Ed):
		return(MDOC_Bd);
	case (MDOC_Ef):
		return(MDOC_Bf);
	case (MDOC_Ek):
		return(MDOC_Bk);
	case (MDOC_El):
		return(MDOC_Bl);
	case (MDOC_Fc):
		return(MDOC_Fo);
	case (MDOC_Oc):
		return(MDOC_Oo);
	case (MDOC_Pc):
		return(MDOC_Po);
	case (MDOC_Qc):
		return(MDOC_Qo);
	case (MDOC_Re):
		return(MDOC_Rs);
	case (MDOC_Sc):
		return(MDOC_So);
	case (MDOC_Xc):
		return(MDOC_Xo);
	default:
		break;
	}
	abort();
	/* NOTREACHED */
}


/* 
 * Rewind rules.  This indicates whether to stop rewinding
 * (REWIND_HALT) without touching our current scope, stop rewinding and
 * close our current scope (REWIND_REWIND), or continue (REWIND_NOHALT).
 * The scope-closing and so on occurs in the various rew_* routines.
 */
static enum rew
rew_dohalt(enum mdoct tok, enum mdoc_type type, 
		const struct mdoc_node *p)
{

	if (MDOC_ROOT == p->type)
		return(REWIND_HALT);
	if (MDOC_VALID & p->flags)
		return(REWIND_NOHALT);

	switch (tok) {
	case (MDOC_Aq):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		/* FALLTHROUGH */
	case (MDOC_D1):
		/* FALLTHROUGH */
	case (MDOC_Dl):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		/* FALLTHROUGH */
	case (MDOC_Op):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		/* FALLTHROUGH */
	case (MDOC_Ql):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		/* FALLTHROUGH */
	case (MDOC_Vt):
		assert(MDOC_TAIL != type);
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		break;
	case (MDOC_It):
		assert(MDOC_TAIL != type);
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		if (MDOC_BODY == p->type && MDOC_Bl == p->tok)
			return(REWIND_HALT);
		break;
	case (MDOC_Sh):
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		break;
	case (MDOC_Nd):
		/* FALLTHROUGH */
	case (MDOC_Ss):
		assert(MDOC_TAIL != type);
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		if (MDOC_BODY == p->type && MDOC_Sh == p->tok)
			return(REWIND_HALT);
		break;
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Bd):
		/* FALLTHROUGH */
	case (MDOC_Bf):
		/* FALLTHROUGH */
	case (MDOC_Bk):
		/* FALLTHROUGH */
	case (MDOC_Bl):
		/* FALLTHROUGH */
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Eo):
		/* FALLTHROUGH */
	case (MDOC_Fo):
		/* FALLTHROUGH */
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Rs):
		/* FALLTHROUGH */
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Xo):
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		break;
	/* Multi-line explicit scope close. */
	case (MDOC_Ac):
		/* FALLTHROUGH */
	case (MDOC_Bc):
		/* FALLTHROUGH */
	case (MDOC_Brc):
		/* FALLTHROUGH */
	case (MDOC_Dc):
		/* FALLTHROUGH */
	case (MDOC_Ec):
		/* FALLTHROUGH */
	case (MDOC_Ed):
		/* FALLTHROUGH */
	case (MDOC_Ek):
		/* FALLTHROUGH */
	case (MDOC_El):
		/* FALLTHROUGH */
	case (MDOC_Fc):
		/* FALLTHROUGH */
	case (MDOC_Ef):
		/* FALLTHROUGH */
	case (MDOC_Oc):
		/* FALLTHROUGH */
	case (MDOC_Pc):
		/* FALLTHROUGH */
	case (MDOC_Qc):
		/* FALLTHROUGH */
	case (MDOC_Re):
		/* FALLTHROUGH */
	case (MDOC_Sc):
		/* FALLTHROUGH */
	case (MDOC_Xc):
		if (type == p->type && rew_alt(tok) == p->tok)
			return(REWIND_REWIND);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(REWIND_NOHALT);
}


/*
 * See if we can break an encountered scope (the rew_dohalt has returned
 * REWIND_NOHALT). 
 */
static int
rew_dobreak(enum mdoct tok, const struct mdoc_node *p)
{

	assert(MDOC_ROOT != p->type);
	if (MDOC_ELEM == p->type)
		return(1);
	if (MDOC_TEXT == p->type)
		return(1);
	if (MDOC_VALID & p->flags)
		return(1);

	switch (tok) {
	case (MDOC_It):
		return(MDOC_It == p->tok);
	case (MDOC_Nd):
		return(MDOC_Nd == p->tok);
	case (MDOC_Ss):
		return(MDOC_Ss == p->tok);
	case (MDOC_Sh):
		if (MDOC_Nd == p->tok)
			return(1);
		if (MDOC_Ss == p->tok)
			return(1);
		return(MDOC_Sh == p->tok);
	case (MDOC_El):
		if (MDOC_It == p->tok)
			return(1);
		break;
	case (MDOC_Oc):
		if (MDOC_Op == p->tok)
			return(1);
		break;
	default:
		break;
	}

	if (MDOC_EXPLICIT & mdoc_macros[tok].flags) 
		return(p->tok == rew_alt(tok));
	else if (MDOC_BLOCK == p->type)
		return(1);

	return(tok == p->tok);
}


static int
rew_elem(struct mdoc *mdoc, enum mdoct tok)
{
	struct mdoc_node *n;

	n = mdoc->last;
	if (MDOC_ELEM != n->type)
		n = n->parent;
	assert(MDOC_ELEM == n->type);
	assert(tok == n->tok);

	return(rew_last(mdoc, n));
}


static int
rew_sub(enum mdoc_type t, struct mdoc *m, 
		enum mdoct tok, int line, int ppos)
{
	struct mdoc_node *n;
	enum rew	  c;

	/* LINTED */
	for (n = m->last; n; n = n->parent) {
		c = rew_dohalt(tok, t, n);
		if (REWIND_HALT == c) {
			if (MDOC_BLOCK != t)
				return(1);
			if ( ! (MDOC_EXPLICIT & mdoc_macros[tok].flags))
				return(1);
			return(mdoc_perr(m, line, ppos, ENOCTX));
		}
		if (REWIND_REWIND == c)
			break;
		else if (rew_dobreak(tok, n))
			continue;
		if ( ! swarn(m, t, line, ppos, n))
			return(0);
	}

	assert(n);
	if ( ! rew_last(m, n))
		return(0);

	/*
	 * The current block extends an enclosing block beyond a line
	 * break.  Now that the current block ends, close the enclosing
	 * block, too.
	 */
	if (NULL != (n = n->pending)) {
		assert(MDOC_HEAD == n->type);
		if ( ! rew_last(m, n))
			return(0);
		if ( ! mdoc_body_alloc(m, n->line, n->pos, n->tok))
			return(0);
	}
	return(1);
}


static int
append_delims(struct mdoc *m, int line, int *pos, char *buf)
{
	int		 la;
	enum margserr	 ac;
	char		*p;

	if ('\0' == buf[*pos])
		return(1);

	for (;;) {
		la = *pos;
		ac = mdoc_zargs(m, line, pos, buf, ARGS_NOWARN, &p);

		if (ARGS_ERROR == ac)
			return(0);
		else if (ARGS_EOLN == ac)
			break;

		assert(DELIM_NONE != mdoc_isdelim(p));
		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);
		/*
		 * If we encounter end-of-sentence symbols, then trigger
		 * the double-space.
		 *
		 * XXX: it's easy to allow this to propogate outward to
		 * the last symbol, such that `. )' will cause the
		 * correct double-spacing.  However, (1) groff isn't
		 * smart enough to do this and (2) it would require
		 * knowing which symbols break this behaviour, for
		 * example, `.  ;' shouldn't propogate the double-space.
		 */
		if (mandoc_eos(p, strlen(p)))
			m->last->flags |= MDOC_EOS;
	}

	return(1);
}


/*
 * Close out block partial/full explicit.  
 */
static int
blk_exp_close(MACRO_PROT_ARGS)
{
	int	 	 j, lastarg, maxargs, flushed, nl;
	enum margserr	 ac;
	enum mdoct	 ntok;
	char		*p;

	nl = MDOC_NEWLINE & m->flags;

	switch (tok) {
	case (MDOC_Ec):
		maxargs = 1;
		break;
	default:
		maxargs = 0;
		break;
	}

	if ( ! (MDOC_CALLABLE & mdoc_macros[tok].flags)) {
		if (buf[*pos]) 
			if ( ! mdoc_pwarn(m, line, ppos, ENOLINE))
				return(0);

		if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
			return(0);
		return(rew_sub(MDOC_BLOCK, m, tok, line, ppos));
	}

	if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
		return(0);

	if (maxargs > 0) 
		if ( ! mdoc_tail_alloc(m, line, ppos, rew_alt(tok)))
			return(0);

	for (flushed = j = 0; ; j++) {
		lastarg = *pos;

		if (j == maxargs && ! flushed) {
			if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
				return(0);
			flushed = 1;
		}

		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_PUNCT == ac)
			break;
		if (ARGS_EOLN == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! mdoc_word_alloc(m, line, lastarg, p))
				return(0);
			continue;
		}

		if ( ! flushed) {
			if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
				return(0);
			flushed = 1;
		}
		if ( ! mdoc_macro(m, ntok, line, lastarg, pos, buf))
			return(0);
		break;
	}

	if ( ! flushed && ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
		return(0);

	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
in_line(MACRO_PROT_ARGS)
{
	int		 la, lastpunct, cnt, nc, nl;
	enum margverr	 av;
	enum mdoct	 ntok;
	enum margserr	 ac;
	enum mdelim	 d;
	struct mdoc_arg	*arg;
	char		*p;

	nl = MDOC_NEWLINE & m->flags;

	/*
	 * Whether we allow ignored elements (those without content,
	 * usually because of reserved words) to squeak by.
	 */

	switch (tok) {
	case (MDOC_An):
		/* FALLTHROUGH */
	case (MDOC_Ar):
		/* FALLTHROUGH */
	case (MDOC_Fl):
		/* FALLTHROUGH */
	case (MDOC_Lk):
		/* FALLTHROUGH */
	case (MDOC_Nm):
		/* FALLTHROUGH */
	case (MDOC_Pa):
		nc = 1;
		break;
	default:
		nc = 0;
		break;
	}

	for (arg = NULL;; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		} 
		if (ARGV_EOLN == av)
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	for (cnt = 0, lastpunct = 1;; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;
		if (ARGS_PUNCT == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		/* 
		 * In this case, we've located a submacro and must
		 * execute it.  Close out scope, if open.  If no
		 * elements have been generated, either create one (nc)
		 * or raise a warning.
		 */

		if (MDOC_MAX != ntok) {
			if (0 == lastpunct && ! rew_elem(m, tok))
				return(0);
			if (nc && 0 == cnt) {
				if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
					return(0);
				if ( ! rew_last(m, m->last))
					return(0);
			} else if ( ! nc && 0 == cnt) {
				mdoc_argv_free(arg);
				if ( ! mdoc_pwarn(m, line, ppos, EIGNE))
					return(0);
			}
			if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
				return(0);
			if ( ! nl)
				return(1);
			return(append_delims(m, line, pos, buf));
		} 

		/* 
		 * Non-quote-enclosed punctuation.  Set up our scope, if
		 * a word; rewind the scope, if a delimiter; then append
		 * the word. 
		 */

		d = ARGS_QWORD == ac ? DELIM_NONE : mdoc_isdelim(p);

		if (ARGS_QWORD != ac && DELIM_NONE != d) {
			if (0 == lastpunct && ! rew_elem(m, tok))
				return(0);
			lastpunct = 1;
		} else if (lastpunct) {
			if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
				return(0);
			lastpunct = 0;
		}

		if (DELIM_NONE == d)
			cnt++;
		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);

		/*
		 * `Fl' macros have their scope re-opened with each new
		 * word so that the `-' can be added to each one without
		 * having to parse out spaces.
		 */
		if (0 == lastpunct && MDOC_Fl == tok) {
			if ( ! rew_elem(m, tok))
				return(0);
			lastpunct = 1;
		}
	}

	if (0 == lastpunct && ! rew_elem(m, tok))
		return(0);

	/*
	 * If no elements have been collected and we're allowed to have
	 * empties (nc), open a scope and close it out.  Otherwise,
	 * raise a warning.
	 */

	if (nc && 0 == cnt) {
		if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
			return(0);
		if ( ! rew_last(m, m->last))
			return(0);
	} else if ( ! nc && 0 == cnt) {
		mdoc_argv_free(arg);
		if ( ! mdoc_pwarn(m, line, ppos, EIGNE))
			return(0);
	}

	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
blk_full(MACRO_PROT_ARGS)
{
	int		  la;
	struct mdoc_arg	 *arg;
	struct mdoc_node *head; /* save of head macro */
	struct mdoc_node *body; /* save of body macro */
	struct mdoc_node *n;
	enum mdoct	  ntok;
	enum margserr	  ac, lac;
	enum margverr	  av;
	char		 *p;

	/* Close out prior implicit scope. */

	if ( ! (MDOC_EXPLICIT & mdoc_macros[tok].flags)) {
		if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
			return(0);
		if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
			return(0);
	}

	/*
	 * This routine accomodates implicitly- and explicitly-scoped
	 * macro openings.  Implicit ones first close out prior scope
	 * (seen above).  Delay opening the head until necessary to
	 * allow leading punctuation to print.  Special consideration
	 * for `It -column', which has phrase-part syntax instead of
	 * regular child nodes.
	 */

	for (arg = NULL;; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		} 

		if (ARGV_EOLN == av)
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	if ( ! mdoc_block_alloc(m, line, ppos, tok, arg))
		return(0);

	head = body = NULL;

	/*
	 * The `Nd' macro has all arguments in its body: it's a hybrid
	 * of block partial-explicit and full-implicit.  Stupid.
	 */

	if (MDOC_Nd == tok) {
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);
		head = m->last;
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
		body = m->last;
	} 

	ac = ARGS_ERROR;

	for ( ; ; ) {
		la = *pos;
		lac = ac;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;

		if (ARGS_PEND == ac) {
			if (ARGS_PPHRASE == lac)
				ac = ARGS_PPHRASE;
			else
				ac = ARGS_PHRASE;
		}

		/* Don't emit leading punct. for phrases. */

		if (NULL == head && 
				ARGS_PHRASE != ac &&
				ARGS_PPHRASE != ac &&
				ARGS_QWORD != ac &&
				DELIM_OPEN == mdoc_isdelim(p)) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		}

		/* Always re-open head for phrases. */

		if (NULL == head || 
				ARGS_PHRASE == ac || 
				ARGS_PPHRASE == ac) {
			if ( ! mdoc_head_alloc(m, line, ppos, tok))
				return(0);
			head = m->last;
		}

		if (ARGS_PHRASE == ac || ARGS_PPHRASE == ac) {
			if (ARGS_PPHRASE == ac)
				m->flags |= MDOC_PPHRASE;
			if ( ! phrase(m, line, la, buf, ac))
				return(0);
			m->flags &= ~MDOC_PPHRASE;
			if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
				return(0);
			continue;
		}

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
			return(0);
		break;
	}

	if (NULL == head) {
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);
		head = m->last;
	}
	
	if (1 == ppos && ! append_delims(m, line, pos, buf))
		return(0);

	/* If we've already opened our body, exit now. */

	if (NULL != body)
		return(1);

	/*
	 * If there is an open (i.e., unvalidated) sub-block requiring
	 * explicit close-out, postpone switching the current block from
	 * head to body until the rew_sub() call closing out that
	 * sub-block.
	 */
	for (n = m->last; n && n != head; n = n->parent) {
		if (MDOC_BLOCK == n->type && 
				MDOC_EXPLICIT & mdoc_macros[n->tok].flags &&
				! (MDOC_VALID & n->flags)) {
			assert( ! (MDOC_ACTED & n->flags));
			n->pending = head;
			return(1);
		}
	}
	/* Close out scopes to remain in a consistent state. */

	if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
		return(0);
	if ( ! mdoc_body_alloc(m, line, ppos, tok))
		return(0);

	return(1);
}


static int
blk_part_imp(MACRO_PROT_ARGS)
{
	int		  la;
	enum mdoct	  ntok;
	enum margserr	  ac;
	char		 *p;
	struct mdoc_node *blk; /* saved block context */
	struct mdoc_node *body; /* saved body context */
	struct mdoc_node *n;

	/*
	 * A macro that spans to the end of the line.  This is generally
	 * (but not necessarily) called as the first macro.  The block
	 * has a head as the immediate child, which is always empty,
	 * followed by zero or more opening punctuation nodes, then the
	 * body (which may be empty, depending on the macro), then zero
	 * or more closing punctuation nodes.
	 */

	if ( ! mdoc_block_alloc(m, line, ppos, tok, NULL))
		return(0);

	blk = m->last;

	if ( ! mdoc_head_alloc(m, line, ppos, tok))
		return(0);
	if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
		return(0);

	/*
	 * Open the body scope "on-demand", that is, after we've
	 * processed all our the leading delimiters (open parenthesis,
	 * etc.).
	 */

	for (body = NULL; ; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;
		if (ARGS_PUNCT == ac)
			break;

		if (NULL == body && ARGS_QWORD != ac &&
		    DELIM_OPEN == mdoc_isdelim(p)) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		} 

		if (NULL == body) {
		       if ( ! mdoc_body_alloc(m, line, ppos, tok))
			       return(0);
			body = m->last;
		}

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
			return(0);
		break;
	}

	/* Clean-ups to leave in a consistent state. */

	if (NULL == body) {
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
		body = m->last;
	}

	for (n = body->child; n && n->next; n = n->next)
		/* Do nothing. */ ;
	
	/* 
	 * End of sentence spacing: if the last node is a text node and
	 * has a trailing period, then mark it as being end-of-sentence.
	 */

	if (n && MDOC_TEXT == n->type && n->string)
		if (mandoc_eos(n->string, strlen(n->string)))
			n->flags |= MDOC_EOS;

	/* Up-propogate the end-of-space flag. */

	if (n && (MDOC_EOS & n->flags)) {
		body->flags |= MDOC_EOS;
		body->parent->flags |= MDOC_EOS;
	}

	/* 
	 * If we can't rewind to our body, then our scope has already
	 * been closed by another macro (like `Oc' closing `Op').  This
	 * is ugly behaviour nodding its head to OpenBSD's overwhelming
	 * crufty use of `Op' breakage.
	 */
	for (n = m->last; n; n = n->parent)
		if (body == n)
			break;

	if (NULL == n && ! mdoc_nwarn(m, body, EIMPBRK))
		return(0);

	if (n && ! rew_last(m, body))
		return(0);

	/* Standard appending of delimiters. */

	if (1 == ppos && ! append_delims(m, line, pos, buf))
		return(0);

	/* Rewind scope, if applicable. */

	if (n && ! rew_last(m, blk))
		return(0);

	return(1);
}


static int
blk_part_exp(MACRO_PROT_ARGS)
{
	int		  la, nl;
	enum margserr	  ac;
	struct mdoc_node *head; /* keep track of head */
	struct mdoc_node *body; /* keep track of body */
	char		 *p;
	enum mdoct	  ntok;

	nl = MDOC_NEWLINE & m->flags;

	/*
	 * The opening of an explicit macro having zero or more leading
	 * punctuation nodes; a head with optional single element (the
	 * case of `Eo'); and a body that may be empty.
	 */

	if ( ! mdoc_block_alloc(m, line, ppos, tok, NULL))
		return(0); 

	for (head = body = NULL; ; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_PUNCT == ac)
			break;
		if (ARGS_EOLN == ac)
			break;

		/* Flush out leading punctuation. */

		if (NULL == head && ARGS_QWORD != ac &&
		    DELIM_OPEN == mdoc_isdelim(p)) {
			assert(NULL == body);
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		} 

		if (NULL == head) {
			assert(NULL == body);
			if ( ! mdoc_head_alloc(m, line, ppos, tok))
				return(0);
			head = m->last;
		}

		/*
		 * `Eo' gobbles any data into the head, but most other
		 * macros just immediately close out and begin the body.
		 */

		if (NULL == body) {
			assert(head);
			/* No check whether it's a macro! */
			if (MDOC_Eo == tok)
				if ( ! mdoc_word_alloc(m, line, la, p))
					return(0);

			if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
				return(0);
			if ( ! mdoc_body_alloc(m, line, ppos, tok))
				return(0);
			body = m->last;

			if (MDOC_Eo == tok)
				continue;
		}

		assert(NULL != head && NULL != body);

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
			return(0);
		break;
	}

	/* Clean-up to leave in a consistent state. */

	if (NULL == head) {
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);
		head = m->last;
	}

	if (NULL == body) {
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
		body = m->last;
	}

	/* Standard appending of delimiters. */

	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


/* ARGSUSED */
static int
in_line_argn(MACRO_PROT_ARGS)
{
	int		 la, flushed, j, maxargs, nl;
	enum margserr	 ac;
	enum margverr	 av;
	struct mdoc_arg	*arg;
	char		*p;
	enum mdoct	 ntok;

	nl = MDOC_NEWLINE & m->flags;

	/*
	 * A line macro that has a fixed number of arguments (maxargs).
	 * Only open the scope once the first non-leading-punctuation is
	 * found (unless MDOC_IGNDELIM is noted, like in `Pf'), then
	 * keep it open until the maximum number of arguments are
	 * exhausted.
	 */

	switch (tok) {
	case (MDOC_Ap):
		/* FALLTHROUGH */
	case (MDOC_No):
		/* FALLTHROUGH */
	case (MDOC_Ns):
		/* FALLTHROUGH */
	case (MDOC_Ux):
		maxargs = 0;
		break;
	case (MDOC_Xr):
		maxargs = 2;
		break;
	default:
		maxargs = 1;
		break;
	}

	for (arg = NULL; ; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		} 

		if (ARGV_EOLN == av)
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	for (flushed = j = 0; ; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_PUNCT == ac)
			break;
		if (ARGS_EOLN == ac)
			break;

		if ( ! (MDOC_IGNDELIM & mdoc_macros[tok].flags) && 
				ARGS_QWORD != ac &&
				0 == j && DELIM_OPEN == mdoc_isdelim(p)) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		} else if (0 == j)
		       if ( ! mdoc_elem_alloc(m, line, la, tok, arg))
			       return(0);

		if (j == maxargs && ! flushed) {
			if ( ! rew_elem(m, tok))
				return(0);
			flushed = 1;
		}

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX != ntok) {
			if ( ! flushed && ! rew_elem(m, tok))
				return(0);
			flushed = 1;
			if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
				return(0);
			j++;
			break;
		}

		if ( ! (MDOC_IGNDELIM & mdoc_macros[tok].flags) &&
				ARGS_QWORD != ac &&
				! flushed &&
				DELIM_NONE != mdoc_isdelim(p)) {
			if ( ! rew_elem(m, tok))
				return(0);
			flushed = 1;
		}

		/* 
		 * XXX: this is a hack to work around groff's ugliness
		 * as regards `Xr' and extraneous arguments.  It should
		 * ideally be deprecated behaviour, but because this is
		 * code is no here, it's unlikely to be removed.
		 */
		if (MDOC_Xr == tok && j == maxargs) {
			if ( ! mdoc_elem_alloc(m, line, la, MDOC_Ns, NULL))
				return(0);
			if ( ! rew_elem(m, MDOC_Ns))
				return(0);
		}

		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);
		j++;
	}

	if (0 == j && ! mdoc_elem_alloc(m, line, la, tok, arg))
	       return(0);

	/* Close out in a consistent state. */

	if ( ! flushed && ! rew_elem(m, tok))
		return(0);
	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
in_line_eoln(MACRO_PROT_ARGS)
{
	int		 la;
	enum margserr	 ac;
	enum margverr	 av;
	struct mdoc_arg	*arg;
	char		*p;
	enum mdoct	 ntok;

	assert( ! (MDOC_PARSED & mdoc_macros[tok].flags));

	/* Parse macro arguments. */

	for (arg = NULL; ; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		}
		if (ARGV_EOLN == av) 
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	/* Open element scope. */

	if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
		return(0);

	/* Parse argument terms. */

	for (;;) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		}

		if ( ! rew_elem(m, tok))
			return(0);
		return(mdoc_macro(m, ntok, line, la, pos, buf));
	}

	/* Close out (no delimiters). */

	return(rew_elem(m, tok));
}


/* ARGSUSED */
static int
ctx_synopsis(MACRO_PROT_ARGS)
{
	int		 nl;

	nl = MDOC_NEWLINE & m->flags;

	/* If we're not in the SYNOPSIS, go straight to in-line. */
	if (SEC_SYNOPSIS != m->lastsec)
		return(in_line(m, tok, line, ppos, pos, buf));

	/* If we're a nested call, same place. */
	if ( ! nl)
		return(in_line(m, tok, line, ppos, pos, buf));

	/*
	 * XXX: this will open a block scope; however, if later we end
	 * up formatting the block scope, then child nodes will inherit
	 * the formatting.  Be careful.
	 */

	return(blk_part_imp(m, tok, line, ppos, pos, buf));
}


/* ARGSUSED */
static int
obsolete(MACRO_PROT_ARGS)
{

	return(mdoc_pwarn(m, line, ppos, EOBS));
}


/*
 * Phrases occur within `Bl -column' entries, separated by `Ta' or tabs.
 * They're unusual because they're basically free-form text until a
 * macro is encountered.
 */
static int
phrase(struct mdoc *m, int line, int ppos, char *buf, enum margserr ac)
{
	int		 la, pos;
	enum margserr	 aac;
	enum mdoct	 ntok;
	char		*p;

	assert(ARGS_PHRASE == ac || ARGS_PPHRASE == ac);

	for (pos = ppos; ; ) {
		la = pos;

		aac = mdoc_zargs(m, line, &pos, buf, 0, &p);

		if (ARGS_ERROR == aac)
			return(0);
		if (ARGS_EOLN == aac)
			break;

		ntok = ARGS_QWORD == aac ? MDOC_MAX : lookup_raw(p);

		if (MDOC_MAX == ntok) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, &pos, buf))
			return(0);
		return(append_delims(m, line, &pos, buf));
	}

	return(1);
}


