/*	$Id: mdoc_macro.c,v 1.20 2009/08/22 15:36:58 schwarze Exp $ */
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

#include "libmdoc.h"

#define	REWIND_REWIND	(1 << 0)
#define	REWIND_NOHALT	(1 << 1)
#define	REWIND_HALT	(1 << 2)

static	int	  obsolete(MACRO_PROT_ARGS);
static	int	  blk_part_exp(MACRO_PROT_ARGS);
static	int	  in_line_eoln(MACRO_PROT_ARGS);
static	int	  in_line_argn(MACRO_PROT_ARGS);
static	int	  in_line(MACRO_PROT_ARGS);
static	int	  blk_full(MACRO_PROT_ARGS);
static	int	  blk_exp_close(MACRO_PROT_ARGS);
static	int	  blk_part_imp(MACRO_PROT_ARGS);

static	int	  phrase(struct mdoc *, int, int, char *);
static	int	  rew_dohalt(int, enum mdoc_type, 
			const struct mdoc_node *);
static	int	  rew_alt(int);
static	int	  rew_dobreak(int, const struct mdoc_node *);
static	int	  rew_elem(struct mdoc *, int);
static	int	  rew_impblock(struct mdoc *, int, int, int);
static	int	  rew_expblock(struct mdoc *, int, int, int);
static	int	  rew_subblock(enum mdoc_type, 
			struct mdoc *, int, int, int);
static	int	  rew_last(struct mdoc *, struct mdoc_node *);
static	int	  append_delims(struct mdoc *, int, int *, char *);
static	int	  lookup(struct mdoc *, int, int, int, const char *);
static	int	  swarn(struct mdoc *, enum mdoc_type, int, int, 
			const struct mdoc_node *);

/* Central table of library: who gets parsed how. */

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
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Vt */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Xr */
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
	{ in_line_argn, MDOC_PARSED | MDOC_IGNDELIM }, /* Pf */
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
mdoc_macroend(struct mdoc *mdoc)
{
	struct mdoc_node *n;

	/* Scan for open explicit scopes. */

	n = MDOC_VALID & mdoc->last->flags ?
		mdoc->last->parent : mdoc->last;

	for ( ; n; n = n->parent) {
		if (MDOC_BLOCK != n->type)
			continue;
		if ( ! (MDOC_EXPLICIT & mdoc_macros[n->tok].flags))
			continue;
		return(mdoc_nerr(mdoc, n, EOPEN));
	}

	return(rew_last(mdoc, mdoc->first));
}

static int
lookup(struct mdoc *mdoc, int line, int pos, int from, const char *p)
{
	int		 res;

	res = mdoc_hash_find(mdoc->htab, p);
	if (MDOC_PARSED & mdoc_macros[from].flags)
		return(res);
	if (MDOC_MAX == res)
		return(res);
	if ( ! mdoc_pwarn(mdoc, line, pos, EMACPARM))
		return(-1);
	return(MDOC_MAX);
}


static int
rew_last(struct mdoc *mdoc, struct mdoc_node *to)
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


static int
rew_alt(int tok)
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
static int 
rew_dohalt(int tok, enum mdoc_type type, const struct mdoc_node *p)
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
		assert(MDOC_HEAD != type);
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
rew_dobreak(int tok, const struct mdoc_node *p)
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
		/* XXX - experimental! */
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
rew_elem(struct mdoc *mdoc, int tok)
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
rew_subblock(enum mdoc_type type, struct mdoc *mdoc, 
		int tok, int line, int ppos)
{
	struct mdoc_node *n;
	int		  c;

	/* LINTED */
	for (n = mdoc->last; n; n = n->parent) {
		c = rew_dohalt(tok, type, n);
		if (REWIND_HALT == c)
			return(1);
		if (REWIND_REWIND == c)
			break;
		else if (rew_dobreak(tok, n))
			continue;
		if ( ! swarn(mdoc, type, line, ppos, n))
			return(0);
	}

	assert(n);
	return(rew_last(mdoc, n));
}


static int
rew_expblock(struct mdoc *mdoc, int tok, int line, int ppos)
{
	struct mdoc_node *n;
	int		  c;

	/* LINTED */
	for (n = mdoc->last; n; n = n->parent) {
		c = rew_dohalt(tok, MDOC_BLOCK, n);
		if (REWIND_HALT == c)
			return(mdoc_perr(mdoc, line, ppos, ENOCTX));
		if (REWIND_REWIND == c)
			break;
		else if (rew_dobreak(tok, n))
			continue;
		if ( ! swarn(mdoc, MDOC_BLOCK, line, ppos, n))
			return(0);
	}

	assert(n);
	return(rew_last(mdoc, n));
}


static int
rew_impblock(struct mdoc *mdoc, int tok, int line, int ppos)
{
	struct mdoc_node *n;
	int		  c;

	/* LINTED */
	for (n = mdoc->last; n; n = n->parent) {
		c = rew_dohalt(tok, MDOC_BLOCK, n);
		if (REWIND_HALT == c)
			return(1);
		else if (REWIND_REWIND == c)
			break;
		else if (rew_dobreak(tok, n))
			continue;
		if ( ! swarn(mdoc, MDOC_BLOCK, line, ppos, n))
			return(0);
	}

	assert(n);
	return(rew_last(mdoc, n));
}


static int
append_delims(struct mdoc *mdoc, int line, int *pos, char *buf)
{
	int		 c, lastarg;
	char		*p;

	if (0 == buf[*pos])
		return(1);

	for (;;) {
		lastarg = *pos;
		c = mdoc_args(mdoc, line, pos, buf, 0, &p);
		assert(ARGS_PHRASE != c);

		if (ARGS_ERROR == c)
			return(0);
		else if (ARGS_EOLN == c)
			break;
		assert(mdoc_isdelim(p));
		if ( ! mdoc_word_alloc(mdoc, line, lastarg, p))
			return(0);
		mdoc->next = MDOC_NEXT_SIBLING;
	}

	return(1);
}


/*
 * Close out block partial/full explicit.  
 */
static int
blk_exp_close(MACRO_PROT_ARGS)
{
	int	 	 j, c, lastarg, maxargs, flushed;
	char		*p;

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
			if ( ! mdoc_pwarn(mdoc, line, ppos, ENOLINE))
				return(0);

		if ( ! rew_subblock(MDOC_BODY, mdoc, tok, line, ppos))
			return(0);
		return(rew_expblock(mdoc, tok, line, ppos));
	}

	if ( ! rew_subblock(MDOC_BODY, mdoc, tok, line, ppos))
		return(0);

	if (maxargs > 0) {
		if ( ! mdoc_tail_alloc(mdoc, line, 
					ppos, rew_alt(tok)))
			return(0);
		mdoc->next = MDOC_NEXT_CHILD;
	}

	for (flushed = j = 0; ; j++) {
		lastarg = *pos;

		if (j == maxargs && ! flushed) {
			if ( ! rew_expblock(mdoc, tok, line, ppos))
				return(0);
			flushed = 1;
		}

		c = mdoc_args(mdoc, line, pos, buf, tok, &p);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (-1 == (c = lookup(mdoc, line, lastarg, tok, p)))
			return(0);
		else if (MDOC_MAX != c) {
			if ( ! flushed) {
				if ( ! rew_expblock(mdoc, tok, 
							line, ppos))
					return(0);
				flushed = 1;
			}
			if ( ! mdoc_macro(mdoc, c, line, lastarg, pos, buf))
				return(0);
			break;
		} 

		if ( ! mdoc_word_alloc(mdoc, line, lastarg, p))
			return(0);
		mdoc->next = MDOC_NEXT_SIBLING;
	}

	if ( ! flushed && ! rew_expblock(mdoc, tok, line, ppos))
		return(0);

	if (ppos > 1)
		return(1);
	return(append_delims(mdoc, line, pos, buf));
}


/*
 * In-line macros where reserved words cause scope close-reopen.
 */
static int
in_line(MACRO_PROT_ARGS)
{
	int		  la, lastpunct, c, w, cnt, d, nc;
	struct mdoc_arg	 *arg;
	char		 *p;

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
		c = mdoc_argv(mdoc, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = la;
			break;
		} 
		if (ARGV_EOLN == c)
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	for (cnt = 0, lastpunct = 1;; ) {
		la = *pos;
		w = mdoc_args(mdoc, line, pos, buf, tok, &p);

		if (ARGS_ERROR == w)
			return(0);
		if (ARGS_EOLN == w)
			break;
		if (ARGS_PUNCT == w)
			break;

		/* Quoted words shouldn't be looked-up. */

		c = ARGS_QWORD == w ? MDOC_MAX :
			lookup(mdoc, line, la, tok, p);

		/* 
		 * In this case, we've located a submacro and must
		 * execute it.  Close out scope, if open.  If no
		 * elements have been generated, either create one (nc)
		 * or raise a warning.
		 */

		if (MDOC_MAX != c && -1 != c) {
			if (0 == lastpunct && ! rew_elem(mdoc, tok))
				return(0);
			if (nc && 0 == cnt) {
				if ( ! mdoc_elem_alloc(mdoc, line, ppos, 
							tok, arg))
					return(0);
				if ( ! rew_last(mdoc, mdoc->last))
					return(0);
			} else if ( ! nc && 0 == cnt) {
				mdoc_argv_free(arg);
				if ( ! mdoc_pwarn(mdoc, line, ppos, EIGNE))
					return(0);
			}
			c = mdoc_macro(mdoc, c, line, la, pos, buf);
			if (0 == c)
				return(0);
			if (ppos > 1)
				return(1);
			return(append_delims(mdoc, line, pos, buf));
		} else if (-1 == c)
			return(0);

		/* 
		 * Non-quote-enclosed punctuation.  Set up our scope, if
		 * a word; rewind the scope, if a delimiter; then append
		 * the word. 
		 */

		d = mdoc_isdelim(p);

		if (ARGS_QWORD != w && d) {
			if (0 == lastpunct && ! rew_elem(mdoc, tok))
				return(0);
			lastpunct = 1;
		} else if (lastpunct) {
			c = mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
			if (0 == c)
				return(0);
			mdoc->next = MDOC_NEXT_CHILD;
			lastpunct = 0;
		}

		if ( ! d)
			cnt++;
		if ( ! mdoc_word_alloc(mdoc, line, la, p))
			return(0);
		mdoc->next = MDOC_NEXT_SIBLING;
	}

	if (0 == lastpunct && ! rew_elem(mdoc, tok))
		return(0);

	/*
	 * If no elements have been collected and we're allowed to have
	 * empties (nc), open a scope and close it out.  Otherwise,
	 * raise a warning.
	 *
	 */
	if (nc && 0 == cnt) {
		c = mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
		if (0 == c)
			return(0);
		if ( ! rew_last(mdoc, mdoc->last))
			return(0);
	} else if ( ! nc && 0 == cnt) {
		mdoc_argv_free(arg);
		if ( ! mdoc_pwarn(mdoc, line, ppos, EIGNE))
			return(0);
	}

	if (ppos > 1)
		return(1);
	return(append_delims(mdoc, line, pos, buf));
}


/*
 * Block full-explicit and full-implicit.
 */
static int
blk_full(MACRO_PROT_ARGS)
{
	int		  c, lastarg, reopen, dohead;
	struct mdoc_arg	 *arg;
	char		 *p;

	/* 
	 * Whether to process a block-head section.  If this is
	 * non-zero, then a head will be opened for all line arguments.
	 * If not, then the head will always be empty and only a body
	 * will be opened, which will stay open at the eoln.
	 */

	switch (tok) {
	case (MDOC_Nd):
		dohead = 0;
		break;
	default:
		dohead = 1;
		break;
	}

	if ( ! (MDOC_EXPLICIT & mdoc_macros[tok].flags)) {
		if ( ! rew_subblock(MDOC_BODY, mdoc, 
					tok, line, ppos))
			return(0);
		if ( ! rew_impblock(mdoc, tok, line, ppos))
			return(0);
	}

	for (arg = NULL;; ) {
		lastarg = *pos;
		c = mdoc_argv(mdoc, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = lastarg;
			break;
		} 

		if (ARGV_EOLN == c)
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	if ( ! mdoc_block_alloc(mdoc, line, ppos, tok, arg))
		return(0);
	mdoc->next = MDOC_NEXT_CHILD;

	if (0 == buf[*pos]) {
		if ( ! mdoc_head_alloc(mdoc, line, ppos, tok))
			return(0);
		if ( ! rew_subblock(MDOC_HEAD, mdoc, 
					tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
			return(0);
		mdoc->next = MDOC_NEXT_CHILD;
		return(1);
	}

	if ( ! mdoc_head_alloc(mdoc, line, ppos, tok))
		return(0);

	/* Immediately close out head and enter body, if applicable. */

	if (0 == dohead) {
		if ( ! rew_subblock(MDOC_HEAD, mdoc, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
			return(0);
	} 

	mdoc->next = MDOC_NEXT_CHILD;

	for (reopen = 0;; ) {
		lastarg = *pos;
		c = mdoc_args(mdoc, line, pos, buf, tok, &p);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_EOLN == c)
			break;
		if (ARGS_PHRASE == c) {
			assert(dohead);
			if (reopen && ! mdoc_head_alloc
					(mdoc, line, ppos, tok))
				return(0);
			mdoc->next = MDOC_NEXT_CHILD;
			/*
			 * Phrases are self-contained macro phrases used
			 * in the columnar output of a macro. They need
			 * special handling.
			 */
			if ( ! phrase(mdoc, line, lastarg, buf))
				return(0);
			if ( ! rew_subblock(MDOC_HEAD, mdoc, 
						tok, line, ppos))
				return(0);

			reopen = 1;
			continue;
		}

		if (-1 == (c = lookup(mdoc, line, lastarg, tok, p)))
			return(0);

		if (MDOC_MAX == c) {
			if ( ! mdoc_word_alloc(mdoc, line, lastarg, p))
				return(0);
			mdoc->next = MDOC_NEXT_SIBLING;
			continue;
		} 

		if ( ! mdoc_macro(mdoc, c, line, lastarg, pos, buf))
			return(0);
		break;
	}
	
	if (1 == ppos && ! append_delims(mdoc, line, pos, buf))
		return(0);

	/* If the body's already open, then just return. */
	if (0 == dohead) 
		return(1);

	if ( ! rew_subblock(MDOC_HEAD, mdoc, tok, line, ppos))
		return(0);
	if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
		return(0);
	mdoc->next = MDOC_NEXT_CHILD;

	return(1);
}


/*
 * Block partial-imnplicit scope.
 */
static int
blk_part_imp(MACRO_PROT_ARGS)
{
	int		  lastarg, c;
	char		 *p;
	struct mdoc_node *blk, *body, *n;

	if ( ! mdoc_block_alloc(mdoc, line, ppos, tok, NULL))
		return(0);
	mdoc->next = MDOC_NEXT_CHILD;
	blk = mdoc->last;

	if ( ! mdoc_head_alloc(mdoc, line, ppos, tok))
		return(0);
	mdoc->next = MDOC_NEXT_SIBLING;

	if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
		return(0);
	mdoc->next = MDOC_NEXT_CHILD;
	body = mdoc->last;

	/* XXX - no known argument macros. */

	for (;;) {
		lastarg = *pos;
		c = mdoc_args(mdoc, line, pos, buf, tok, &p);
		assert(ARGS_PHRASE != c);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (-1 == (c = lookup(mdoc, line, lastarg, tok, p)))
			return(0);
		else if (MDOC_MAX == c) {
			if ( ! mdoc_word_alloc(mdoc, line, lastarg, p))
				return(0);
			mdoc->next = MDOC_NEXT_SIBLING;
			continue;
		} 

		if ( ! mdoc_macro(mdoc, c, line, lastarg, pos, buf))
			return(0);
		break;
	}

	/*
	 * Since we know what our context is, we can rewind directly to
	 * it.  This allows us to accomodate for our scope being
	 * violated by another token.
	 */

	for (n = mdoc->last; n; n = n->parent)
		if (body == n)
			break;

	if (NULL == n && ! mdoc_nwarn(mdoc, body, EIMPBRK))
			return(0);

	if (n && ! rew_last(mdoc, body))
		return(0);

	if (1 == ppos && ! append_delims(mdoc, line, pos, buf))
		return(0);

	if (n && ! rew_last(mdoc, blk))
		return(0);

	return(1);
}


/*
 * Block partial-explicit macros.
 */
static int
blk_part_exp(MACRO_PROT_ARGS)
{
	int		  lastarg, flushed, j, c, maxargs;
	char		 *p;

	flushed = 0;

	/*
	 * Number of arguments (head arguments).  Only `Eo' has these,
	 */

	switch (tok) {
	case (MDOC_Eo):
		maxargs = 1;
		break;
	default:
		maxargs = 0;
		break;
	}

	if ( ! mdoc_block_alloc(mdoc, line, ppos, tok, NULL))
		return(0); 
	mdoc->next = MDOC_NEXT_CHILD;

	if (0 == maxargs) {
		if ( ! mdoc_head_alloc(mdoc, line, ppos, tok))
			return(0);
		if ( ! rew_subblock(MDOC_HEAD, mdoc, 
					tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
			return(0);
		flushed = 1;
	} else if ( ! mdoc_head_alloc(mdoc, line, ppos, tok))
		return(0);

	mdoc->next = MDOC_NEXT_CHILD;

	for (j = 0; ; j++) {
		lastarg = *pos;
		if (j == maxargs && ! flushed) {
			if ( ! rew_subblock(MDOC_HEAD, mdoc, 
						tok, line, ppos))
				return(0);
			flushed = 1;
			if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
				return(0);
			mdoc->next = MDOC_NEXT_CHILD;
		}

		c = mdoc_args(mdoc, line, pos, buf, tok, &p);
		assert(ARGS_PHRASE != c);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (-1 == (c = lookup(mdoc, line, lastarg, tok, p)))
			return(0);
		else if (MDOC_MAX != c) {
			if ( ! flushed) {
				if ( ! rew_subblock(MDOC_HEAD, mdoc, 
							tok, line, ppos))
					return(0);
				flushed = 1;
				if ( ! mdoc_body_alloc(mdoc, line, 
							ppos, tok))
					return(0);
				mdoc->next = MDOC_NEXT_CHILD;
			}
			if ( ! mdoc_macro(mdoc, c, line, lastarg, 
						pos, buf))
				return(0);
			break;
		}

		if ( ! flushed && mdoc_isdelim(p)) {
			if ( ! rew_subblock(MDOC_HEAD, mdoc, 
						tok, line, ppos))
				return(0);
			flushed = 1;
			if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
				return(0);
			mdoc->next = MDOC_NEXT_CHILD;
		}
	
		if ( ! mdoc_word_alloc(mdoc, line, lastarg, p))
			return(0);
		mdoc->next = MDOC_NEXT_SIBLING;
	}

	if ( ! flushed) {
		if ( ! rew_subblock(MDOC_HEAD, mdoc, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(mdoc, line, ppos, tok))
			return(0);
		mdoc->next = MDOC_NEXT_CHILD;
	}

	if (ppos > 1)
		return(1);
	return(append_delims(mdoc, line, pos, buf));
}


/*
 * In-line macros where reserved words signal closure of the macro.
 * Macros also have a fixed number of arguments.
 */
static int
in_line_argn(MACRO_PROT_ARGS)
{
	int		  lastarg, flushed, j, c, maxargs;
	struct mdoc_arg	 *arg;
	char		 *p;

	
	/* 
	 * Fixed maximum arguments per macro.  Some of these have none
	 * and close as soon as the invocation is parsed.
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
	default:
		maxargs = 1;
		break;
	}

	for (arg = NULL;; ) {
		lastarg = *pos;
		c = mdoc_argv(mdoc, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = lastarg;
			break;
		} 

		if (ARGV_EOLN == c)
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	if ( ! mdoc_elem_alloc(mdoc, line, ppos, tok, arg))
		return(0);
	mdoc->next = MDOC_NEXT_CHILD;

	for (flushed = j = 0; ; j++) {
		lastarg = *pos;

		if (j == maxargs && ! flushed) {
			if ( ! rew_elem(mdoc, tok))
				return(0);
			flushed = 1;
		}

		c = mdoc_args(mdoc, line, pos, buf, tok, &p);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (-1 == (c = lookup(mdoc, line, lastarg, tok, p)))
			return(0);
		else if (MDOC_MAX != c) {
			if ( ! flushed && ! rew_elem(mdoc, tok))
				return(0);
			flushed = 1;
			if ( ! mdoc_macro(mdoc, c, line, lastarg, pos, buf))
				return(0);
			break;
		}

		if ( ! (MDOC_IGNDELIM & mdoc_macros[tok].flags) &&
				! flushed && mdoc_isdelim(p)) {
			if ( ! rew_elem(mdoc, tok))
				return(0);
			flushed = 1;
		}
	
		if ( ! mdoc_word_alloc(mdoc, line, lastarg, p))
			return(0);
		mdoc->next = MDOC_NEXT_SIBLING;
	}

	if ( ! flushed && ! rew_elem(mdoc, tok))
		return(0);

	if (ppos > 1)
		return(1);
	return(append_delims(mdoc, line, pos, buf));
}


/*
 * In-line macro that spans an entire line.  May be callable, but has no
 * subsequent parsed arguments.
 */
static int
in_line_eoln(MACRO_PROT_ARGS)
{
	int		  c, w, la;
	struct mdoc_arg	 *arg;
	char		 *p;

	assert( ! (MDOC_PARSED & mdoc_macros[tok].flags));

	arg = NULL;

	for (;;) {
		la = *pos;
		c = mdoc_argv(mdoc, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = la;
			break;
		}
		if (ARGV_EOLN == c) 
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	if ( ! mdoc_elem_alloc(mdoc, line, ppos, tok, arg))
		return(0);

	mdoc->next = MDOC_NEXT_CHILD;

	for (;;) {
		la = *pos;
		w = mdoc_args(mdoc, line, pos, buf, tok, &p);

		if (ARGS_ERROR == w)
			return(0);
		if (ARGS_EOLN == w)
			break;

		c = ARGS_QWORD == w ? MDOC_MAX :
			lookup(mdoc, line, la, tok, p);

		if (MDOC_MAX != c && -1 != c) {
			if ( ! rew_elem(mdoc, tok))
				return(0);
			return(mdoc_macro(mdoc, c, line, la, pos, buf));
		} else if (-1 == c)
			return(0);

		if ( ! mdoc_word_alloc(mdoc, line, la, p))
			return(0);
		mdoc->next = MDOC_NEXT_SIBLING;
	}

	return(rew_elem(mdoc, tok));
}


/* ARGSUSED */
static int
obsolete(MACRO_PROT_ARGS)
{

	return(mdoc_pwarn(mdoc, line, ppos, EOBS));
}


/*
 * Phrases occur within `Bl -column' entries, separated by `Ta' or tabs.
 * They're unusual because they're basically free-form text until a
 * macro is encountered.
 */
static int
phrase(struct mdoc *mdoc, int line, int ppos, char *buf)
{
	int		  c, w, la, pos;
	char		 *p;

	for (pos = ppos; ; ) {
		la = pos;

		/* Note: no calling context! */
		w = mdoc_zargs(mdoc, line, &pos, buf, &p);

		if (ARGS_ERROR == w)
			return(0);
		if (ARGS_EOLN == w)
			break;

		c = ARGS_QWORD == w ? MDOC_MAX :
			mdoc_hash_find(mdoc->htab, p);

		if (MDOC_MAX != c && -1 != c) {
			if ( ! mdoc_macro(mdoc, c, line, la, &pos, buf))
				return(0);
			return(append_delims(mdoc, line, &pos, buf));
		} else if (-1 == c)
			return(0);

		if ( ! mdoc_word_alloc(mdoc, line, la, p))
			return(0);
		mdoc->next = MDOC_NEXT_SIBLING;
	}

	return(1);
}


