/*	$Id: man_macro.c,v 1.15 2010/04/25 16:32:19 schwarze Exp $ */
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
static	int		 blk_dotted(MACRO_PROT_ARGS);
static	int		 blk_exp(MACRO_PROT_ARGS);
static	int		 blk_imp(MACRO_PROT_ARGS);
static	int		 blk_cond(MACRO_PROT_ARGS);
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
	{ blk_exp, MAN_EXPLICIT | MAN_NOCLOSE}, /* de */
	{ blk_exp, MAN_EXPLICIT | MAN_NOCLOSE}, /* dei */
	{ blk_exp, MAN_EXPLICIT | MAN_NOCLOSE}, /* am */
	{ blk_exp, MAN_EXPLICIT | MAN_NOCLOSE}, /* ami */
	{ blk_exp, MAN_EXPLICIT | MAN_NOCLOSE}, /* ig */
	{ blk_dotted, 0 }, /* . */
	{ blk_cond, 0 }, /* if */
	{ blk_cond, 0 }, /* ie */
	{ blk_cond, 0 }, /* el */
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
	if (MAN_NOCLOSE & man_macros[n->tok].flags)
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
	 * If we're a roff macro, then we can close out anything that
	 * stands between us and our parent context.
	 */
	if (MAN_NOCLOSE & man_macros[tok].flags)
		return(REW_NOHALT);

	/* 
	 * Don't clobber roff macros: this is a bit complicated.  If the
	 * current macro is a roff macro, halt immediately and don't
	 * rewind.  If it's not, and the parent is, then close out the
	 * current scope and halt at the parent.
	 */
	if (MAN_NOCLOSE & man_macros[n->tok].flags)
		return(REW_HALT);
	if (MAN_NOCLOSE & man_macros[n->parent->tok].flags)
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
	if (MAN_NOCLOSE & man_macros[tok].flags)
		return(man_unscope(m, n, WROFFSCOPE));

	return(man_unscope(m, n, WERRMAX));
}


/*
 * Closure for brace blocks (if, ie, el).
 */
int
man_brace_close(struct man *m, int line, int ppos)
{
	struct man_node	*nif;

	nif = m->last->parent;
	while (nif &&
	    MAN_if != nif->tok &&
	    MAN_ie != nif->tok &&
	    MAN_el != nif->tok)
		nif = nif->parent;

	if (NULL == nif)
		return(man_pwarn(m, line, ppos, WNOSCOPE));

	if (MAN_ie != nif->tok || MAN_USE & nif->flags)
		m->flags &= ~MAN_EL_USE;
	else
		m->flags |= MAN_EL_USE;

	if (MAN_USE & nif->flags) {
		if (nif->prev) {
			nif->prev->next = nif->child;
			nif->child->prev = nif->prev;
			nif->prev = NULL;
		} else {
			nif->parent->child = nif->child;
		}
		nif->parent->nchild += nif->nchild - 1;
		while (nif->child) {
			nif->child->parent = nif->parent;
			nif->child = nif->child->next;
		}
		nif->nchild = 0;
		nif->parent = NULL;
	}
	man_node_delete(m, nif);
	return(1);
}


/*
 * Closure for dotted macros (de, dei, am, ami, ign).  This must handle
 * any of these as the parent node, so it needs special handling.
 * Beyond this, it's the same as blk_close().
 */
/* ARGSUSED */
int
blk_dotted(MACRO_PROT_ARGS)
{
	enum mant	 ntok;
	struct man_node	*nn;

	/* Check for any of the following parents... */

	for (nn = m->last->parent; nn; nn = nn->parent)
		if (nn->tok == MAN_de || nn->tok == MAN_dei ||
				nn->tok == MAN_am ||
				nn->tok == MAN_ami ||
				nn->tok == MAN_ig) {
			ntok = nn->tok;
			break;
		}

	if (NULL == nn) {
		if ( ! man_pwarn(m, line, ppos, WNOSCOPE))
			return(0);
		return(1);
	}

	if ( ! rew_scope(MAN_BODY, m, ntok))
		return(0);
	if ( ! rew_scope(MAN_BLOCK, m, ntok))
		return(0);

	/*
	 * Restore flags set when we got here and also stipulate that we
	 * don't post-process the line when exiting the macro op
	 * function in man_pmacro().  See blk_exp().
	 */

	m->flags = m->svflags | MAN_ILINE;
	m->next = m->svnext;
	return(1);
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

	if ( ! (MAN_NOCLOSE & man_macros[tok].flags)) {
		if ( ! rew_scope(MAN_BODY, m, tok))
			return(0);
		if ( ! rew_scope(MAN_BLOCK, m, tok))
			return(0);
	} else {
		/*
		 * Save our state and next-scope indicator; we restore
		 * it when exiting from the roff instruction block.  See
		 * blk_dotted().
		 */
		m->svflags = m->flags;
		m->svnext = m->next;
		
		/* Make sure we drop any line modes. */
		m->flags = 0;
	}

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


/*
 * Parse a conditional roff instruction.
 */
int
blk_cond(MACRO_PROT_ARGS)
{ 
	char		*p = buf + *pos;
	int		 use;

	if (MAN_el == tok)
		use = m->flags & MAN_EL_USE;
	else {
		use = 'n' == *p++;
		/* XXX skip the rest of the condition for now */
		while (*p && !isblank(*p))
			p++;
	}
	m->flags &= ~MAN_EL_USE;

	/* advance to the code controlled by the condition */
	while (*p && isblank(*p))
		p++;
	if ('\0' == *p)
		return(1);

	/* single-line body */
	if (strncmp("\\{", p, 2)) {
		if (use && ! man_parseln(m, line, p))
			return(0);
	        if (MAN_ie == tok && !use)
                        m->flags |= MAN_EL_USE;
		return(1);
        }

	/* multi-line body */
	if ( ! man_block_alloc(m, line, ppos, tok))
		return(0);
	if (use)
		m->last->flags |= MAN_USE;
	p += 2;
	return(*p ? man_parseln(m, line, p) : 1);
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

