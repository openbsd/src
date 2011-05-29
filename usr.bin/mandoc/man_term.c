/*	$Id: man_term.c,v 1.68 2011/05/29 21:22:18 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2011 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "man.h"
#include "term.h"
#include "main.h"

#define	INDENT		  7
#define	HALFINDENT	  3

/* FIXME: have PD set the default vspace width. */

struct	mtermp {
	int		  fl;
#define	MANT_LITERAL	 (1 << 0)
	/* 
	 * Default amount to indent the left margin after leading text
	 * has been printed (e.g., `HP' left-indent, `TP' and `IP' body
	 * indent).  This needs to be saved because `HP' and so on, if
	 * not having a specified value, must default.
	 *
	 * Note that this is the indentation AFTER the left offset, so
	 * the total offset is usually offset + lmargin.
	 */
	size_t		  lmargin;
	/*
	 * The default offset, i.e., the amount between any text and the
	 * page boundary.
	 */
	size_t		  offset;
};

#define	DECL_ARGS 	  struct termp *p, \
			  struct mtermp *mt, \
			  const struct man_node *n, \
			  const struct man_meta *m

struct	termact {
	int		(*pre)(DECL_ARGS);
	void		(*post)(DECL_ARGS);
	int		  flags;
#define	MAN_NOTEXT	 (1 << 0) /* Never has text children. */
};

static	int		  a2width(const struct termp *, const char *);
static	size_t		  a2height(const struct termp *, const char *);

static	void		  print_man_nodelist(DECL_ARGS);
static	void		  print_man_node(DECL_ARGS);
static	void		  print_man_head(struct termp *, const void *);
static	void		  print_man_foot(struct termp *, const void *);
static	void		  print_bvspace(struct termp *, 
				const struct man_node *);

static	int		  pre_alternate(DECL_ARGS);
static	int		  pre_B(DECL_ARGS);
static	int		  pre_HP(DECL_ARGS);
static	int		  pre_I(DECL_ARGS);
static	int		  pre_IP(DECL_ARGS);
static	int		  pre_PP(DECL_ARGS);
static	int		  pre_RS(DECL_ARGS);
static	int		  pre_SH(DECL_ARGS);
static	int		  pre_SS(DECL_ARGS);
static	int		  pre_TP(DECL_ARGS);
static	int		  pre_ign(DECL_ARGS);
static	int		  pre_in(DECL_ARGS);
static	int		  pre_literal(DECL_ARGS);
static	int		  pre_sp(DECL_ARGS);
static	int		  pre_ft(DECL_ARGS);

static	void		  post_IP(DECL_ARGS);
static	void		  post_HP(DECL_ARGS);
static	void		  post_RS(DECL_ARGS);
static	void		  post_SH(DECL_ARGS);
static	void		  post_SS(DECL_ARGS);
static	void		  post_TP(DECL_ARGS);

static	const struct termact termacts[MAN_MAX] = {
	{ pre_sp, NULL, MAN_NOTEXT }, /* br */
	{ NULL, NULL, 0 }, /* TH */
	{ pre_SH, post_SH, 0 }, /* SH */
	{ pre_SS, post_SS, 0 }, /* SS */
	{ pre_TP, post_TP, 0 }, /* TP */
	{ pre_PP, NULL, 0 }, /* LP */
	{ pre_PP, NULL, 0 }, /* PP */
	{ pre_PP, NULL, 0 }, /* P */
	{ pre_IP, post_IP, 0 }, /* IP */
	{ pre_HP, post_HP, 0 }, /* HP */ 
	{ NULL, NULL, 0 }, /* SM */
	{ pre_B, NULL, 0 }, /* SB */
	{ pre_alternate, NULL, 0 }, /* BI */
	{ pre_alternate, NULL, 0 }, /* IB */
	{ pre_alternate, NULL, 0 }, /* BR */
	{ pre_alternate, NULL, 0 }, /* RB */
	{ NULL, NULL, 0 }, /* R */
	{ pre_B, NULL, 0 }, /* B */
	{ pre_I, NULL, 0 }, /* I */
	{ pre_alternate, NULL, 0 }, /* IR */
	{ pre_alternate, NULL, 0 }, /* RI */
	{ pre_ign, NULL, MAN_NOTEXT }, /* na */
	{ pre_sp, NULL, MAN_NOTEXT }, /* sp */
	{ pre_literal, NULL, 0 }, /* nf */
	{ pre_literal, NULL, 0 }, /* fi */
	{ NULL, NULL, 0 }, /* RE */
	{ pre_RS, post_RS, 0 }, /* RS */
	{ pre_ign, NULL, 0 }, /* DT */
	{ pre_ign, NULL, 0 }, /* UC */
	{ pre_ign, NULL, 0 }, /* PD */
	{ pre_ign, NULL, 0 }, /* AT */
	{ pre_in, NULL, MAN_NOTEXT }, /* in */
	{ pre_ft, NULL, MAN_NOTEXT }, /* ft */
};



void
terminal_man(void *arg, const struct man *man)
{
	struct termp		*p;
	const struct man_node	*n;
	const struct man_meta	*m;
	struct mtermp		 mt;

	p = (struct termp *)arg;

	p->overstep = 0;
	p->maxrmargin = p->defrmargin;
	p->tabwidth = term_len(p, 5);

	if (NULL == p->symtab)
		p->symtab = mchars_alloc();

	n = man_node(man);
	m = man_meta(man);

	term_begin(p, print_man_head, print_man_foot, m);
	p->flags |= TERMP_NOSPACE;

	mt.fl = 0;
	mt.lmargin = term_len(p, INDENT);
	mt.offset = term_len(p, INDENT);

	if (n->child)
		print_man_nodelist(p, &mt, n->child, m);

	term_end(p);
}


static size_t
a2height(const struct termp *p, const char *cp)
{
	struct roffsu	 su;

	if ( ! a2roffsu(cp, &su, SCALE_VS))
		SCALE_VS_INIT(&su, term_strlen(p, cp));

	return(term_vspan(p, &su));
}


static int
a2width(const struct termp *p, const char *cp)
{
	struct roffsu	 su;

	if ( ! a2roffsu(cp, &su, SCALE_BU))
		return(-1);

	return((int)term_hspan(p, &su));
}


static void
print_bvspace(struct termp *p, const struct man_node *n)
{
	term_newln(p);

	if (n->body && n->body->child && MAN_TBL == n->body->child->type)
		return;

	if (NULL == n->prev)
		return;

	if (MAN_SS == n->prev->tok)
		return;
	if (MAN_SH == n->prev->tok)
		return;

	term_vspace(p);
}


/* ARGSUSED */
static int
pre_ign(DECL_ARGS)
{

	return(0);
}


/* ARGSUSED */
static int
pre_I(DECL_ARGS)
{

	term_fontrepl(p, TERMFONT_UNDER);
	return(1);
}


/* ARGSUSED */
static int
pre_literal(DECL_ARGS)
{

	term_newln(p);

	if (MAN_nf == n->tok)
		mt->fl |= MANT_LITERAL;
	else
		mt->fl &= ~MANT_LITERAL;

	return(0);
}

/* ARGSUSED */
static int
pre_alternate(DECL_ARGS)
{
	enum termfont		 font[2];
	const struct man_node	*nn;
	int			 savelit, i;

	switch (n->tok) {
	case (MAN_RB):
		font[0] = TERMFONT_NONE;
		font[1] = TERMFONT_BOLD;
		break;
	case (MAN_RI):
		font[0] = TERMFONT_NONE;
		font[1] = TERMFONT_UNDER;
		break;
	case (MAN_BR):
		font[0] = TERMFONT_BOLD;
		font[1] = TERMFONT_NONE;
		break;
	case (MAN_BI):
		font[0] = TERMFONT_BOLD;
		font[1] = TERMFONT_UNDER;
		break;
	case (MAN_IR):
		font[0] = TERMFONT_UNDER;
		font[1] = TERMFONT_NONE;
		break;
	case (MAN_IB):
		font[0] = TERMFONT_UNDER;
		font[1] = TERMFONT_BOLD;
		break;
	default:
		abort();
	}

	savelit = MANT_LITERAL & mt->fl;
	mt->fl &= ~MANT_LITERAL;

	for (i = 0, nn = n->child; nn; nn = nn->next, i = 1 - i) {
		term_fontrepl(p, font[i]);
		if (savelit && NULL == nn->next)
			mt->fl |= MANT_LITERAL;
		print_man_node(p, mt, nn, m);
		if (nn->next)
			p->flags |= TERMP_NOSPACE;
	}

	return(0);
}

/* ARGSUSED */
static int
pre_B(DECL_ARGS)
{

	term_fontrepl(p, TERMFONT_BOLD);
	return(1);
}

/* ARGSUSED */
static int
pre_ft(DECL_ARGS)
{
	const char	*cp;

	if (NULL == n->child) {
		term_fontlast(p);
		return(0);
	}

	cp = n->child->string;
	switch (*cp) {
	case ('4'):
		/* FALLTHROUGH */
	case ('3'):
		/* FALLTHROUGH */
	case ('B'):
		term_fontrepl(p, TERMFONT_BOLD);
		break;
	case ('2'):
		/* FALLTHROUGH */
	case ('I'):
		term_fontrepl(p, TERMFONT_UNDER);
		break;
	case ('P'):
		term_fontlast(p);
		break;
	case ('1'):
		/* FALLTHROUGH */
	case ('C'):
		/* FALLTHROUGH */
	case ('R'):
		term_fontrepl(p, TERMFONT_NONE);
		break;
	default:
		break;
	}
	return(0);
}

/* ARGSUSED */
static int
pre_in(DECL_ARGS)
{
	int		 len, less;
	size_t		 v;
	const char	*cp;

	term_newln(p);

	if (NULL == n->child) {
		p->offset = mt->offset;
		return(0);
	}

	cp = n->child->string;
	less = 0;

	if ('-' == *cp)
		less = -1;
	else if ('+' == *cp)
		less = 1;
	else
		cp--;

	if ((len = a2width(p, ++cp)) < 0)
		return(0);

	v = (size_t)len;

	if (less < 0)
		p->offset -= p->offset > v ? v : p->offset;
	else if (less > 0)
		p->offset += v;
	else 
		p->offset = v;

	/* Don't let this creep beyond the right margin. */

	if (p->offset > p->rmargin)
		p->offset = p->rmargin;

	return(0);
}


/* ARGSUSED */
static int
pre_sp(DECL_ARGS)
{
	size_t		 i, len;

	switch (n->tok) {
	case (MAN_br):
		len = 0;
		break;
	default:
		len = n->child ? a2height(p, n->child->string) : 1;
		break;
	}

	if (0 == len)
		term_newln(p);
	for (i = 0; i < len; i++)
		term_vspace(p);

	return(0);
}


/* ARGSUSED */
static int
pre_HP(DECL_ARGS)
{
	size_t			 len;
	int			 ival;
	const struct man_node	*nn;

	switch (n->type) {
	case (MAN_BLOCK):
		print_bvspace(p, n);
		return(1);
	case (MAN_BODY):
		p->flags |= TERMP_NOBREAK;
		p->flags |= TERMP_TWOSPACE;
		break;
	default:
		return(0);
	}

	len = mt->lmargin;
	ival = -1;

	/* Calculate offset. */

	if (NULL != (nn = n->parent->head->child))
		if ((ival = a2width(p, nn->string)) >= 0)
			len = (size_t)ival;

	if (0 == len)
		len = term_len(p, 1);

	p->offset = mt->offset;
	p->rmargin = mt->offset + len;

	if (ival >= 0)
		mt->lmargin = (size_t)ival;

	return(1);
}


/* ARGSUSED */
static void
post_HP(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_BLOCK):
		term_flushln(p);
		break;
	case (MAN_BODY):
		term_flushln(p);
		p->flags &= ~TERMP_NOBREAK;
		p->flags &= ~TERMP_TWOSPACE;
		p->offset = mt->offset;
		p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}
}


/* ARGSUSED */
static int
pre_PP(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_BLOCK):
		mt->lmargin = term_len(p, INDENT);
		print_bvspace(p, n);
		break;
	default:
		p->offset = mt->offset;
		break;
	}

	return(MAN_HEAD != n->type);
}


/* ARGSUSED */
static int
pre_IP(DECL_ARGS)
{
	const struct man_node	*nn;
	size_t			 len;
	int			 savelit, ival;

	switch (n->type) {
	case (MAN_BODY):
		p->flags |= TERMP_NOLPAD;
		p->flags |= TERMP_NOSPACE;
		break;
	case (MAN_HEAD):
		p->flags |= TERMP_NOBREAK;
		break;
	case (MAN_BLOCK):
		print_bvspace(p, n);
		/* FALLTHROUGH */
	default:
		return(1);
	}

	len = mt->lmargin;
	ival = -1;

	/* Calculate the offset from the optional second argument. */
	if (NULL != (nn = n->parent->head->child))
		if (NULL != (nn = nn->next))
			if ((ival = a2width(p, nn->string)) >= 0)
				len = (size_t)ival;

	switch (n->type) {
	case (MAN_HEAD):
		/* Handle zero-width lengths. */
		if (0 == len)
			len = term_len(p, 1);

		p->offset = mt->offset;
		p->rmargin = mt->offset + len;
		if (ival < 0)
			break;

		/* Set the saved left-margin. */
		mt->lmargin = (size_t)ival;

		savelit = MANT_LITERAL & mt->fl;
		mt->fl &= ~MANT_LITERAL;

		if (n->child)
			print_man_node(p, mt, n->child, m);

		if (savelit)
			mt->fl |= MANT_LITERAL;

		return(0);
	case (MAN_BODY):
		p->offset = mt->offset + len;
		p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
post_IP(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_HEAD):
		term_flushln(p);
		p->flags &= ~TERMP_NOBREAK;
		p->rmargin = p->maxrmargin;
		break;
	case (MAN_BODY):
		term_newln(p);
		p->flags &= ~TERMP_NOLPAD;
		break;
	default:
		break;
	}
}


/* ARGSUSED */
static int
pre_TP(DECL_ARGS)
{
	const struct man_node	*nn;
	size_t			 len;
	int			 savelit, ival;

	switch (n->type) {
	case (MAN_HEAD):
		p->flags |= TERMP_NOBREAK;
		break;
	case (MAN_BODY):
		p->flags |= TERMP_NOLPAD;
		p->flags |= TERMP_NOSPACE;
		break;
	case (MAN_BLOCK):
		print_bvspace(p, n);
		/* FALLTHROUGH */
	default:
		return(1);
	}

	len = (size_t)mt->lmargin;
	ival = -1;

	/* Calculate offset. */

	if (NULL != (nn = n->parent->head->child)) {
		while (nn && MAN_TEXT != nn->type)
			nn = nn->next;
		if (nn && nn->next)
			if ((ival = a2width(p, nn->string)) >= 0)
				len = (size_t)ival;
	}

	switch (n->type) {
	case (MAN_HEAD):
		/* Handle zero-length properly. */
		if (0 == len)
			len = term_len(p, 1);

		p->offset = mt->offset;
		p->rmargin = mt->offset + len;

		savelit = MANT_LITERAL & mt->fl;
		mt->fl &= ~MANT_LITERAL;

		/* Don't print same-line elements. */
		for (nn = n->child; nn; nn = nn->next)
			if (nn->line > n->line)
				print_man_node(p, mt, nn, m);

		if (savelit)
			mt->fl |= MANT_LITERAL;

		if (ival >= 0)
			mt->lmargin = (size_t)ival;

		return(0);
	case (MAN_BODY):
		p->offset = mt->offset + len;
		p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
post_TP(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_HEAD):
		term_flushln(p);
		p->flags &= ~TERMP_NOBREAK;
		p->flags &= ~TERMP_TWOSPACE;
		p->rmargin = p->maxrmargin;
		break;
	case (MAN_BODY):
		term_newln(p);
		p->flags &= ~TERMP_NOLPAD;
		break;
	default:
		break;
	}
}


/* ARGSUSED */
static int
pre_SS(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_BLOCK):
		mt->lmargin = term_len(p, INDENT);
		mt->offset = term_len(p, INDENT);
		/* If following a prior empty `SS', no vspace. */
		if (n->prev && MAN_SS == n->prev->tok)
			if (NULL == n->prev->body->child)
				break;
		if (NULL == n->prev)
			break;
		term_vspace(p);
		break;
	case (MAN_HEAD):
		term_fontrepl(p, TERMFONT_BOLD);
		p->offset = term_len(p, HALFINDENT);
		break;
	case (MAN_BODY):
		p->offset = mt->offset;
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
post_SS(DECL_ARGS)
{
	
	switch (n->type) {
	case (MAN_HEAD):
		term_newln(p);
		break;
	case (MAN_BODY):
		term_newln(p);
		break;
	default:
		break;
	}
}


/* ARGSUSED */
static int
pre_SH(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_BLOCK):
		mt->lmargin = term_len(p, INDENT);
		mt->offset = term_len(p, INDENT);
		/* If following a prior empty `SH', no vspace. */
		if (n->prev && MAN_SH == n->prev->tok)
			if (NULL == n->prev->body->child)
				break;
		/* If the first macro, no vspae. */
		if (NULL == n->prev)
			break;
		term_vspace(p);
		break;
	case (MAN_HEAD):
		term_fontrepl(p, TERMFONT_BOLD);
		p->offset = 0;
		break;
	case (MAN_BODY):
		p->offset = mt->offset;
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
post_SH(DECL_ARGS)
{
	
	switch (n->type) {
	case (MAN_HEAD):
		term_newln(p);
		break;
	case (MAN_BODY):
		term_newln(p);
		break;
	default:
		break;
	}
}


/* ARGSUSED */
static int
pre_RS(DECL_ARGS)
{
	const struct man_node	*nn;
	int			 ival;

	switch (n->type) {
	case (MAN_BLOCK):
		term_newln(p);
		return(1);
	case (MAN_HEAD):
		return(0);
	default:
		break;
	}

	if (NULL == (nn = n->parent->head->child)) {
		mt->offset = mt->lmargin + term_len(p, INDENT);
		p->offset = mt->offset;
		return(1);
	}

	if ((ival = a2width(p, nn->string)) < 0)
		return(1);

	mt->offset = term_len(p, INDENT) + (size_t)ival;
	p->offset = mt->offset;

	return(1);
}


/* ARGSUSED */
static void
post_RS(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_BLOCK):
		mt->offset = mt->lmargin = term_len(p, INDENT);
		break;
	case (MAN_HEAD):
		break;
	default:
		term_newln(p);
		p->offset = term_len(p, INDENT);
		break;
	}
}


static void
print_man_node(DECL_ARGS)
{
	size_t		 rm, rmax;
	int		 c;

	switch (n->type) {
	case(MAN_TEXT):
		/*
		 * If we have a blank line, output a vertical space.
		 * If we have a space as the first character, break
		 * before printing the line's data.
		 */
		if ('\0' == *n->string) {
			term_vspace(p);
			return;
		} else if (' ' == *n->string && MAN_LINE & n->flags)
			term_newln(p);

		term_word(p, n->string);

		/*
		 * If we're in a literal context, make sure that words
		 * togehter on the same line stay together.  This is a
		 * POST-printing call, so we check the NEXT word.  Since
		 * -man doesn't have nested macros, we don't need to be
		 * more specific than this.
		 */
		if (MANT_LITERAL & mt->fl && 
				(NULL == n->next || 
				 n->next->line > n->line)) {
			rm = p->rmargin;
			rmax = p->maxrmargin;
			p->rmargin = p->maxrmargin = TERM_MAXMARGIN;
			p->flags |= TERMP_NOSPACE;
			term_flushln(p);
			p->flags &= ~TERMP_NOLPAD;
			p->rmargin = rm;
			p->maxrmargin = rmax;
		}

		if (MAN_EOS & n->flags)
			p->flags |= TERMP_SENTENCE;
		return;
	case (MAN_EQN):
		term_word(p, n->eqn->data);
		return;
	case (MAN_TBL):
		/*
		 * Tables are preceded by a newline.  Then process a
		 * table line, which will cause line termination,
		 */
		if (TBL_SPAN_FIRST & n->span->flags) 
			term_newln(p);
		term_tbl(p, n->span);
		return;
	default:
		break;
	}

	if ( ! (MAN_NOTEXT & termacts[n->tok].flags))
		term_fontrepl(p, TERMFONT_NONE);

	c = 1;
	if (termacts[n->tok].pre)
		c = (*termacts[n->tok].pre)(p, mt, n, m);

	if (c && n->child)
		print_man_nodelist(p, mt, n->child, m);

	if (termacts[n->tok].post)
		(*termacts[n->tok].post)(p, mt, n, m);
	if ( ! (MAN_NOTEXT & termacts[n->tok].flags))
		term_fontrepl(p, TERMFONT_NONE);

	if (MAN_EOS & n->flags)
		p->flags |= TERMP_SENTENCE;
}


static void
print_man_nodelist(DECL_ARGS)
{

	print_man_node(p, mt, n, m);
	if ( ! n->next)
		return;
	print_man_nodelist(p, mt, n->next, m);
}


static void
print_man_foot(struct termp *p, const void *arg)
{
	const struct man_meta *meta;

	meta = (const struct man_meta *)arg;

	term_fontrepl(p, TERMFONT_NONE);

	term_vspace(p);
	term_vspace(p);
	term_vspace(p);

	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;
	p->rmargin = p->maxrmargin - term_strlen(p, meta->date);
	p->offset = 0;

	/* term_strlen() can return zero. */
	if (p->rmargin == p->maxrmargin)
		p->rmargin--;

	if (meta->source)
		term_word(p, meta->source);
	if (meta->source)
		term_word(p, "");
	term_flushln(p);

	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;
	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin;
	p->flags &= ~TERMP_NOBREAK;

	term_word(p, meta->date);
	term_flushln(p);
}


static void
print_man_head(struct termp *p, const void *arg)
{
	char		buf[BUFSIZ], title[BUFSIZ];
	size_t		buflen, titlen;
	const struct man_meta *m;

	m = (const struct man_meta *)arg;

	/*
	 * Note that old groff would spit out some spaces before the
	 * header.  We discontinue this strange behaviour, but at one
	 * point we did so here.
	 */

	p->rmargin = p->maxrmargin;

	p->offset = 0;
	buf[0] = title[0] = '\0';

	if (m->vol)
		strlcpy(buf, m->vol, BUFSIZ);
	buflen = term_strlen(p, buf);

	snprintf(title, BUFSIZ, "%s(%s)", m->title, m->msec);
	titlen = term_strlen(p, title);

	p->offset = 0;
	p->rmargin = 2 * (titlen+1) + buflen < p->maxrmargin ?
	    (p->maxrmargin - 
	     term_strlen(p, buf) + term_len(p, 1)) / 2 :
	    p->maxrmargin - buflen;
	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;
	p->offset = p->rmargin;
	p->rmargin = p->offset + buflen + titlen < p->maxrmargin ?
	    p->maxrmargin - titlen : p->maxrmargin;

	term_word(p, buf);
	term_flushln(p);

	p->flags &= ~TERMP_NOBREAK;
	if (p->rmargin + titlen <= p->maxrmargin) {
		p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;
		p->offset = p->rmargin;
		p->rmargin = p->maxrmargin;
		term_word(p, title);
		term_flushln(p);
	}

	p->rmargin = p->maxrmargin;
	p->offset = 0;
	p->flags &= ~TERMP_NOSPACE;

	/* 
	 * Groff likes to have some leading spaces before content.  Well
	 * that's fine by me.
	 */

	term_vspace(p);
	term_vspace(p);
	term_vspace(p);
}
