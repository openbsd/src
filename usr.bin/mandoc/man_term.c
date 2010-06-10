/*	$Id: man_term.c,v 1.41 2010/06/10 22:50:10 schwarze Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "man.h"
#include "term.h"
#include "chars.h"
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

static	int		  a2width(const struct man_node *);
static	int		  a2height(const struct man_node *);

static	void		  print_man_nodelist(DECL_ARGS);
static	void		  print_man_node(DECL_ARGS);
static	void		  print_man_head(struct termp *, const void *);
static	void		  print_man_foot(struct termp *, const void *);
static	void		  print_bvspace(struct termp *, 
				const struct man_node *);

static	int		  pre_B(DECL_ARGS);
static	int		  pre_BI(DECL_ARGS);
static	int		  pre_HP(DECL_ARGS);
static	int		  pre_I(DECL_ARGS);
static	int		  pre_IP(DECL_ARGS);
static	int		  pre_PP(DECL_ARGS);
static	int		  pre_RB(DECL_ARGS);
static	int		  pre_RI(DECL_ARGS);
static	int		  pre_RS(DECL_ARGS);
static	int		  pre_SH(DECL_ARGS);
static	int		  pre_SS(DECL_ARGS);
static	int		  pre_TP(DECL_ARGS);
static	int		  pre_br(DECL_ARGS);
static	int		  pre_fi(DECL_ARGS);
static	int		  pre_ign(DECL_ARGS);
static	int		  pre_nf(DECL_ARGS);
static	int		  pre_sp(DECL_ARGS);

static	void		  post_IP(DECL_ARGS);
static	void		  post_HP(DECL_ARGS);
static	void		  post_RS(DECL_ARGS);
static	void		  post_SH(DECL_ARGS);
static	void		  post_SS(DECL_ARGS);
static	void		  post_TP(DECL_ARGS);

static	const struct termact termacts[MAN_MAX] = {
	{ pre_br, NULL, MAN_NOTEXT }, /* br */
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
	{ pre_BI, NULL, 0 }, /* BI */
	{ pre_BI, NULL, 0 }, /* IB */
	{ pre_RB, NULL, 0 }, /* BR */
	{ pre_RB, NULL, 0 }, /* RB */
	{ NULL, NULL, 0 }, /* R */
	{ pre_B, NULL, 0 }, /* B */
	{ pre_I, NULL, 0 }, /* I */
	{ pre_RI, NULL, 0 }, /* IR */
	{ pre_RI, NULL, 0 }, /* RI */
	{ NULL, NULL, MAN_NOTEXT }, /* na */
	{ pre_I, NULL, 0 }, /* i */
	{ pre_sp, NULL, MAN_NOTEXT }, /* sp */
	{ pre_nf, NULL, 0 }, /* nf */
	{ pre_fi, NULL, 0 }, /* fi */
	{ NULL, NULL, 0 }, /* r */
	{ NULL, NULL, 0 }, /* RE */
	{ pre_RS, post_RS, 0 }, /* RS */
	{ pre_ign, NULL, 0 }, /* DT */
	{ pre_ign, NULL, 0 }, /* UC */
	{ pre_ign, NULL, 0 }, /* PD */
 	{ pre_sp, NULL, MAN_NOTEXT }, /* Sp */
 	{ pre_nf, NULL, 0 }, /* Vb */
 	{ pre_fi, NULL, 0 }, /* Ve */
	{ pre_ign, NULL, 0 }, /* AT */
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
	p->tabwidth = 5;

	if (NULL == p->symtab)
		switch (p->enc) {
		case (TERMENC_ASCII):
			p->symtab = chars_init(CHARS_ASCII);
			break;
		default:
			abort();
			/* NOTREACHED */
		}

	n = man_node(man);
	m = man_meta(man);

	term_begin(p, print_man_head, print_man_foot, m);
	p->flags |= TERMP_NOSPACE;

	mt.fl = 0;
	mt.lmargin = INDENT;
	mt.offset = INDENT;

	if (n->child)
		print_man_nodelist(p, &mt, n->child, m);

	term_end(p);
}


static int
a2height(const struct man_node *n)
{
	struct roffsu	 su;

	assert(MAN_TEXT == n->type);
	assert(n->string);
	if ( ! a2roffsu(n->string, &su, SCALE_VS))
		SCALE_VS_INIT(&su, strlen(n->string));

	return((int)term_vspan(&su));
}


static int
a2width(const struct man_node *n)
{
	struct roffsu	 su;

	assert(MAN_TEXT == n->type);
	assert(n->string);
	if ( ! a2roffsu(n->string, &su, SCALE_BU))
		return(-1);

	return((int)term_hspan(&su));
}


static void
print_bvspace(struct termp *p, const struct man_node *n)
{
	term_newln(p);

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
pre_fi(DECL_ARGS)
{

	mt->fl &= ~MANT_LITERAL;
	return(1);
}


/* ARGSUSED */
static int
pre_nf(DECL_ARGS)
{

	mt->fl |= MANT_LITERAL;
	return(MAN_Vb != n->tok);
}


/* ARGSUSED */
static int
pre_RB(DECL_ARGS)
{
	const struct man_node *nn;
	int		 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		if (i % 2 && MAN_RB == n->tok)
			term_fontrepl(p, TERMFONT_BOLD);
		else if ( ! (i % 2) && MAN_RB != n->tok)
			term_fontrepl(p, TERMFONT_BOLD);
		else
			term_fontrepl(p, TERMFONT_NONE);

		if (i > 0)
			p->flags |= TERMP_NOSPACE;

		print_man_node(p, mt, nn, m);
	}
	return(0);
}


/* ARGSUSED */
static int
pre_RI(DECL_ARGS)
{
	const struct man_node *nn;
	int		 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		if (i % 2 && MAN_RI == n->tok)
			term_fontrepl(p, TERMFONT_UNDER);
		else if ( ! (i % 2) && MAN_RI != n->tok)
			term_fontrepl(p, TERMFONT_UNDER);
		else
			term_fontrepl(p, TERMFONT_NONE);

		if (i > 0)
			p->flags |= TERMP_NOSPACE;

		print_man_node(p, mt, nn, m);
	}
	return(0);
}


/* ARGSUSED */
static int
pre_BI(DECL_ARGS)
{
	const struct man_node	*nn;
	int			 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		if (i % 2 && MAN_BI == n->tok)
			term_fontrepl(p, TERMFONT_UNDER);
		else if (i % 2)
			term_fontrepl(p, TERMFONT_BOLD);
		else if (MAN_BI == n->tok)
			term_fontrepl(p, TERMFONT_BOLD);
		else
			term_fontrepl(p, TERMFONT_UNDER);

		if (i)
			p->flags |= TERMP_NOSPACE;

		print_man_node(p, mt, nn, m);
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
pre_sp(DECL_ARGS)
{
	int		 i, len;

	len = n->child ? a2height(n->child) : 1;

	if (0 == len)
		term_newln(p);
	for (i = 0; i <= len; i++)
		term_vspace(p);

	return(0);
}


/* ARGSUSED */
static int
pre_br(DECL_ARGS)
{

	term_newln(p);
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
		if ((ival = a2width(nn)) >= 0)
			len = (size_t)ival;

	if (0 == len)
		len = 1;

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
		mt->lmargin = INDENT;
		print_bvspace(p, n);
		break;
	default:
		p->offset = mt->offset;
		break;
	}

	return(1);
}


/* ARGSUSED */
static int
pre_IP(DECL_ARGS)
{
	const struct man_node	*nn;
	size_t			 len;
	int			 ival;

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

	/* Calculate offset. */

	if (NULL != (nn = n->parent->head->child))
		if (NULL != (nn = nn->next)) {
			for ( ; nn->next; nn = nn->next)
				/* Do nothing. */ ;
			if ((ival = a2width(nn)) >= 0)
				len = (size_t)ival;
		}

	switch (n->type) {
	case (MAN_HEAD):
		/* Handle zero-width lengths. */
		if (0 == len)
			len = 1;

		p->offset = mt->offset;
		p->rmargin = mt->offset + len;
		if (ival < 0)
			break;

		/* Set the saved left-margin. */
		mt->lmargin = (size_t)ival;

		/* Don't print the length value. */
		for (nn = n->child; nn->next; nn = nn->next)
			print_man_node(p, mt, nn, m);
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
		term_flushln(p);
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
	int			 ival;

	switch (n->type) {
	case (MAN_HEAD):
		p->flags |= TERMP_NOBREAK;
		p->flags |= TERMP_TWOSPACE;
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
			if ((ival = a2width(nn)) >= 0)
				len = (size_t)ival;
	}

	switch (n->type) {
	case (MAN_HEAD):
		/* Handle zero-length properly. */
		if (0 == len)
			len = 1;

		p->offset = mt->offset;
		p->rmargin = mt->offset + len;

		/* Don't print same-line elements. */
		for (nn = n->child; nn; nn = nn->next) 
			if (nn->line > n->line)
				print_man_node(p, mt, nn, m);

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
		term_flushln(p);
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
		mt->lmargin = INDENT;
		mt->offset = INDENT;
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
		p->offset = HALFINDENT;
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
		mt->lmargin = INDENT;
		mt->offset = INDENT;
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
		mt->offset = mt->lmargin + INDENT;
		p->offset = mt->offset;
		return(1);
	}

	if ((ival = a2width(nn)) < 0)
		return(1);

	mt->offset = INDENT + (size_t)ival;
	p->offset = mt->offset;

	return(1);
}


/* ARGSUSED */
static void
post_RS(DECL_ARGS)
{

	switch (n->type) {
	case (MAN_BLOCK):
		mt->offset = mt->lmargin = INDENT;
		break;
	case (MAN_HEAD):
		break;
	default:
		term_newln(p);
		p->offset = INDENT;
		break;
	}
}


static void
print_man_node(DECL_ARGS)
{
	size_t		 rm, rmax;
	int		 c;

	c = 1;

	switch (n->type) {
	case(MAN_TEXT):
		if (0 == *n->string) {
			term_vspace(p);
			break;
		}

		term_word(p, n->string);

		/* FIXME: this means that macro lines are munged!  */

		if (MANT_LITERAL & mt->fl) {
			rm = p->rmargin;
			rmax = p->maxrmargin;
			p->rmargin = p->maxrmargin = TERM_MAXMARGIN;
			p->flags |= TERMP_NOSPACE;
			term_flushln(p);
			p->rmargin = rm;
			p->maxrmargin = rmax;
		}
		break;
	default:
		if ( ! (MAN_NOTEXT & termacts[n->tok].flags))
			term_fontrepl(p, TERMFONT_NONE);
		if (termacts[n->tok].pre)
			c = (*termacts[n->tok].pre)(p, mt, n, m);
		break;
	}

	if (c && n->child)
		print_man_nodelist(p, mt, n->child, m);

	if (MAN_TEXT != n->type) {
		if (termacts[n->tok].post)
			(*termacts[n->tok].post)(p, mt, n, m);
		if ( ! (MAN_NOTEXT & termacts[n->tok].flags))
			term_fontrepl(p, TERMFONT_NONE);
	}

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
	char		buf[DATESIZ];
	const struct man_meta *meta;

	meta = (const struct man_meta *)arg;

	term_fontrepl(p, TERMFONT_NONE);

	if (meta->rawdate)
		strlcpy(buf, meta->rawdate, DATESIZ);
	else
		time2a(meta->date, buf, DATESIZ);

	term_vspace(p);
	term_vspace(p);
	term_vspace(p);

	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;
	p->rmargin = p->maxrmargin - strlen(buf);
	p->offset = 0;

	if (meta->source)
		term_word(p, meta->source);
	if (meta->source)
		term_word(p, "");
	term_flushln(p);

	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;
	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin;
	p->flags &= ~TERMP_NOBREAK;

	term_word(p, buf);
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
	buflen = strlen(buf);

	snprintf(title, BUFSIZ, "%s(%s)", m->title, m->msec);
	titlen = strlen(title);

	p->offset = 0;
	p->rmargin = 2 * (titlen+1) + buflen < p->maxrmargin ?
	    (p->maxrmargin - strlen(buf) + 1) / 2 :
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
