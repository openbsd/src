/*	$Id: man_macro.c,v 1.18 2010/05/23 20:57:16 schwarze Exp $ */
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
#include <string.h>

#include "libman.h"

enum	rew {
	REW_REWIND,
	REW_NOHALT,
	REW_HALT
};

static	int		 blk_close(MACRO_PROT_ARGS);
static	int		 blk_exp(MACRO_PROT_ARGS);
static	int		 blk_imp(MACRO_PROT_ARGS);
static	int		 in_line_eoln(MACRO_PROT_ARGS);

static	int		 rew_scope(enum man_type, 
				struct man *, enum mant);
static	enum rew	 rew_dohalt(enum mant, enum man_type, 
				const struct man_node *);
static	enum rew	 rew_block(enum mant, enum man_type, 
				const struct man_node *);
static	int		 rew_warn(struct man *, 
				struct man_node *, enum merr);

const	struct man_macro __man_macros[MAN_MAX] = {
	{ in_line_eoln, MAN_NSCOPED }, /* br */
	{ in_line_eoln, 0 }, /* TH */
	{ blk_imp, MAN_SCOPED }, /* SH */
	{ blk_imp, MAN_SCOPED }, /* SS */
	{ blk_imp, MAN_SCOPED | MAN_FSCOPED }, /* TP */
	{ blk_imp, 0 }, /* LP */
	{ blk_imp, 0 }, /* PP */
	{ blk_imp, 0 }, /* P */
	{ blk_imp, 0 }, /* IP */
	{ blk_imp, 0 }, /* HP */
	{ in_line_eoln, MAN_SCOPED }, /* SM */
	{ in_line_eoln, MAN_SCOPED }, /* SB */
	{ in_line_eoln, 0 }, /* BI */
	{ in_line_eoln, 0 }, /* IB */
	{ in_line_eoln, 0 }, /* BR */
	{ in_line_eoln, 0 }, /* RB */
	{ in_line_eoln, MAN_SCOPED }, /* R */
	{ in_line_eoln, MAN_SCOPED }, /* B */
	{ in_line_eoln, MAN_SCOPED }, /* I */
	{ in_line_eoln, 0 }, /* IR */
	{ in_line_eoln, 0 }, /* RI */
	{ in_line_eoln, MAN_NSCOPED }, /* na */
	{ in_line_eoln, 0 }, /* i */
	{ in_line_eoln, MAN_NSCOPED }, /* sp */
	{ in_line_eoln, 0 }, /* nf */
	{ in_line_eoln, 0 }, /* fi */
	{ in_line_eoln, 0 }, /* r */
	{ blk_close, 0 }, /* RE */
	{ blk_exp, MAN_EXPLICIT }, /* RS */
	{ in_line_eoln, 0 }, /* DT */
	{ in_line_eoln, 0 }, /* UC */
	{ in_line_eoln, 0 }, /* PD */
	{ in_line_eoln, MAN_NSCOPED }, /* Sp */
	{ in_line_eoln, 0 }, /* Vb */
	{ in_line_eoln, 0 }, /* Ve */
	{ in_line_eoln, 0 }, /* AT */
};

const	struct man_macro * const man_macros = __man_macros;


/*
 * Warn when "n" is an explicit non-roff macro.
 */
static int
rew_warn(struct man *m, struct man_node *n, enum merr er)
{

	if (er == WERRMAX || MAN_BLOCK != n->type)
		return(1);
	if (MAN_VALID & n->flags)
		return(1);
	if ( ! (MAN_EXPLICIT & man_macros[n->tok].flags))
		return(1);
	return(man_nwarn(m, n, er));
}


/*
 * Rewind scope.  If a code "er" != WERRMAX has been provided, it will
 * be used if an explicit block scope is being closed out.
 */
int
man_unscope(struct man *m, const struct man_node *n, enum merr er)
{

	assert(n);

	/* LINTED */
	while (m->last != n) {
		if ( ! rew_warn(m, m->last, er))
			return(0);
		if ( ! man_valid_post(m))
			return(0);
		if ( ! man_action_post(m))
			return(0);
		m->last = m->last->parent;
		assert(m->last);
	}

	if ( ! rew_warn(m, m->last, er))
		return(0);
	if ( ! man_valid_post(m))
		return(0);
	if ( ! man_action_post(m))
		return(0);

	m->next = MAN_ROOT == m->last->type ? 
		MAN_NEXT_CHILD : MAN_NEXT_SIBLING;

	return(1);
}


static enum rew
rew_block(enum mant ntok, enum man_type type, const struct man_node *n)
{

	if (MAN_BLOCK == type && ntok == n->parent->tok && 
			MAN_BODY == n->parent->type)
		return(REW_REWIND);
	return(ntok == n->tok ? REW_HALT : REW_NOHALT);
}


/*
 * There are three scope levels: scoped to the root (all), scoped to the
 * section (all less sections), and scoped to subsections (all less
 * sections and subsections).
 */
static enum rew 
rew_dohalt(enum mant tok, enum man_type type, const struct man_node *n)
{
	enum rew	 c;

	/* We cannot progress beyond the root ever. */
	if (MAN_ROOT == n->type)
		return(REW_HALT);

	assert(n->parent);

	/* Normal nodes shouldn't go to the level of the root. */
	if (MAN_ROOT == n->parent->type)
		return(REW_REWIND);

	/* Already-validated nodes should be closed out. */
	if (MAN_VALID & n->flags)
		return(REW_NOHALT);

	/* First: rewind to ourselves. */
	if (type == n->type && tok == n->tok)
		return(REW_REWIND);

	/* 
	 * Next follow the implicit scope-smashings as defined by man.7:
	 * section, sub-section, etc.
	 */

	switch (tok) {
	case (MAN_SH):
		break;
	case (MAN_SS):
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	case (MAN_RS):
		/* Rewind to a subsection, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SS, type, n)))
			return(c);
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	default:
		/* Rewind to an offsetter, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_RS, type, n)))
			return(c);
		/* Rewind to a subsection, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SS, type, n)))
			return(c);
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	}

	return(REW_NOHALT);
}


/*
 * Rewinding entails ascending the parse tree until a coherent point,
 * for example, the `SH' macro will close out any intervening `SS'
 * scopes.  When a scope is closed, it must be validated and actioned.
 */
static int
rew_scope(enum man_type type, struct man *m, enum mant tok)
{
	struct man_node	*n;
	enum rew	 c;

	/* LINTED */
	for (n = m->last; n; n = n->parent) {
		/* 
		 * Whether we should stop immediately (REW_HALT), stop
		 * and rewind until this point (REW_REWIND), or keep
		 * rewinding (REW_NOHALT).
		 */
		c = rew_dohalt(tok, type, n);
		if (REW_HALT == c)
			return(1);
		if (REW_REWIND == c)
			break;
	}

	/* 
	 * Rewind until the current point.  Warn if we're a roff
	 * instruction that's mowing over explicit scopes.
	 */
	assert(n);

	return(man_unscope(m, n, WERRMAX));
}


/*
 * Close out a generic explicit macro.
 */
/* ARGSUSED */
int
blk_close(MACRO_PROT_ARGS)
{
	enum mant	 	 ntok;
	const struct man_node	*nn;

	switch (tok) {
	case (MAN_RE):
		ntok = MAN_RS;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	for (nn = m->last->parent; nn; nn = nn->parent)
		if (ntok == nn->tok)
			break;

	if (NULL == nn)
		if ( ! man_pwarn(m, line, ppos, WNOSCOPE))
			return(0);

	if ( ! rew_scope(MAN_BODY, m, ntok))
		return(0);
	if ( ! rew_scope(MAN_BLOCK, m, ntok))
		return(0);

	return(1);
}


int
blk_exp(MACRO_PROT_ARGS)
{
	int		 w, la;
	char		*p;

	/* 
	 * Close out prior scopes.  "Regular" explicit macros cannot be
	 * nested, but we allow roff macros to be placed just about
	 * anywhere.
	 */

	if ( ! rew_scope(MAN_BODY, m, tok))
		return(0);
	if ( ! rew_scope(MAN_BLOCK, m, tok))
		return(0);

	if ( ! man_block_alloc(m, line, ppos, tok))
		return(0);
	if ( ! man_head_alloc(m, line, ppos, tok))
		return(0);

	for (;;) {
		la = *pos;
		w = man_args(m, line, pos, buf, &p);

		if (-1 == w)
			return(0);
		if (0 == w)
			break;

		if ( ! man_word_alloc(m, line, la, p))
			return(0);
	}

	assert(m);
	assert(tok != MAN_MAX);

	if ( ! rew_scope(MAN_HEAD, m, tok))
		return(0);
	return(man_body_alloc(m, line, ppos, tok));
}



/*
 * Parse an implicit-block macro.  These contain a MAN_HEAD and a
 * MAN_BODY contained within a MAN_BLOCK.  Rules for closing out other
 * scopes, such as `SH' closing out an `SS', are defined in the rew
 * routines.
 */
int
blk_imp(MACRO_PROT_ARGS)
{
	int		 w, la;
	char		*p;
	struct man_node	*n;

	/* Close out prior scopes. */

	if ( ! rew_scope(MAN_BODY, m, tok))
		return(0);
	if ( ! rew_scope(MAN_BLOCK, m, tok))
		return(0);

	/* Allocate new block & head scope. */

	if ( ! man_block_alloc(m, line, ppos, tok))
		return(0);
	if ( ! man_head_alloc(m, line, ppos, tok))
		return(0);

	n = m->last;

	/* Add line arguments. */

	for (;;) {
		la = *pos;
		w = man_args(m, line, pos, buf, &p);

		if (-1 == w)
			return(0);
		if (0 == w)
			break;

		if ( ! man_word_alloc(m, line, la, p))
			return(0);
	}

	/* Close out head and open body (unless MAN_SCOPE). */

	if (MAN_SCOPED & man_macros[tok].flags) {
		/* If we're forcing scope (`TP'), keep it open. */
		if (MAN_FSCOPED & man_macros[tok].flags) {
			m->flags |= MAN_BLINE;
			return(1);
		} else if (n == m->last) {
			m->flags |= MAN_BLINE;
			return(1);
		}
	}

	if ( ! rew_scope(MAN_HEAD, m, tok))
		return(0);
	return(man_body_alloc(m, line, ppos, tok));
}


int
in_line_eoln(MACRO_PROT_ARGS)
{
	int		 w, la;
	char		*p;
	struct man_node	*n;

	if ( ! man_elem_alloc(m, line, ppos, tok))
		return(0);

	n = m->last;

	for (;;) {
		la = *pos;
		w = man_args(m, line, pos, buf, &p);

		if (-1 == w)
			return(0);
		if (0 == w)
			break;
		if ( ! man_word_alloc(m, line, la, p))
			return(0);
	}

	/*
	 * If no arguments are specified and this is MAN_SCOPED (i.e.,
	 * next-line scoped), then set our mode to indicate that we're
	 * waiting for terms to load into our context.
	 */

	if (n == m->last && MAN_SCOPED & man_macros[tok].flags) {
		assert( ! (MAN_NSCOPED & man_macros[tok].flags));
		m->flags |= MAN_ELINE;
		return(1);
	} 

	/* Set ignorable context, if applicable. */

	if (MAN_NSCOPED & man_macros[tok].flags) {
		assert( ! (MAN_SCOPED & man_macros[tok].flags));
		m->flags |= MAN_ILINE;
	}
	
	/*
	 * Rewind our element scope.  Note that when TH is pruned, we'll
	 * be back at the root, so make sure that we don't clobber as
	 * its sibling.
	 */

	for ( ; m->last; m->last = m->last->parent) {
		if (m->last == n)
			break;
		if (m->last->type == MAN_ROOT)
			break;
		if ( ! man_valid_post(m))
			return(0);
		if ( ! man_action_post(m))
			return(0);
	}

	assert(m->last);

	/*
	 * Same here regarding whether we're back at the root. 
	 */

	if (m->last->type != MAN_ROOT && ! man_valid_post(m))
		return(0);
	if (m->last->type != MAN_ROOT && ! man_action_post(m))
		return(0);

	m->next = MAN_ROOT == m->last->type ?
		MAN_NEXT_CHILD : MAN_NEXT_SIBLING;

	return(1);
}


int
man_macroend(struct man *m)
{

	return(man_unscope(m, m->first, WEXITSCOPE));
}

