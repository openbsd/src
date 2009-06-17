/*	$Id: man_term.c,v 1.3 2009/06/17 22:27:34 schwarze Exp $ */
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
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term.h"
#include "man.h"

#define	DECL_ARGS 	  struct termp *p, \
			  const struct man_node *n, \
			  const struct man_meta *m

struct	termact {
	int		(*pre)(DECL_ARGS);
	void		(*post)(DECL_ARGS);
};

static	int		  pre_B(DECL_ARGS);
static	int		  pre_BI(DECL_ARGS);
static	int		  pre_BR(DECL_ARGS);
static	int		  pre_I(DECL_ARGS);
static	int		  pre_IB(DECL_ARGS);
static	int		  pre_IP(DECL_ARGS);
static	int		  pre_IR(DECL_ARGS);
static	int		  pre_PP(DECL_ARGS);
static	int		  pre_RB(DECL_ARGS);
static	int		  pre_RI(DECL_ARGS);
static	int		  pre_SH(DECL_ARGS);
static	int		  pre_SS(DECL_ARGS);
static	int		  pre_TP(DECL_ARGS);

static	void		  post_B(DECL_ARGS);
static	void		  post_I(DECL_ARGS);
static	void		  post_SH(DECL_ARGS);
static	void		  post_SS(DECL_ARGS);

static const struct termact termacts[MAN_MAX] = {
	{ NULL, NULL }, /* __ */
	{ NULL, NULL }, /* TH */
	{ pre_SH, post_SH }, /* SH */
	{ pre_SS, post_SS }, /* SS */
	{ pre_TP, NULL }, /* TP */
	{ pre_PP, NULL }, /* LP */
	{ pre_PP, NULL }, /* PP */
	{ pre_PP, NULL }, /* P */
	{ pre_IP, NULL }, /* IP */
	{ pre_PP, NULL }, /* HP */ /* FIXME */
	{ NULL, NULL }, /* SM */
	{ pre_B, post_B }, /* SB */
	{ pre_BI, NULL }, /* BI */
	{ pre_IB, NULL }, /* IB */
	{ pre_BR, NULL }, /* BR */
	{ pre_RB, NULL }, /* RB */
	{ NULL, NULL }, /* R */
	{ pre_B, post_B }, /* B */
	{ pre_I, post_I }, /* I */
	{ pre_IR, NULL }, /* IR */
	{ pre_RI, NULL }, /* RI */
	{ pre_PP, NULL }, /* br */
	{ NULL, NULL }, /* na */
	{ pre_I, post_I }, /* i */
};

static	void		  print_head(struct termp *, 
				const struct man_meta *);
static	void		  print_body(DECL_ARGS);
static	void		  print_node(DECL_ARGS);
static	void		  print_foot(struct termp *, 
				const struct man_meta *);


int
man_run(struct termp *p, const struct man *m)
{

	print_head(p, man_meta(m));
	p->flags |= TERMP_NOSPACE;
	print_body(p, man_node(m), man_meta(m));
	print_foot(p, man_meta(m));

	return(1);
}


/* ARGSUSED */
static int
pre_I(DECL_ARGS)
{

	p->flags |= TERMP_UNDER;
	return(1);
}


/* ARGSUSED */
static void
post_I(DECL_ARGS)
{

	p->flags &= ~TERMP_UNDER;
}


/* ARGSUSED */
static int
pre_IR(DECL_ARGS)
{
	const struct man_node *nn;
	int		 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		if ( ! (i % 2))
			p->flags |= TERMP_UNDER;
		if (i > 0)
			p->flags |= TERMP_NOSPACE;
		print_node(p, nn, m);
		if ( ! (i % 2))
			p->flags &= ~TERMP_UNDER;
	}
	return(0);
}


/* ARGSUSED */
static int
pre_IB(DECL_ARGS)
{
	const struct man_node *nn;
	int		 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		p->flags |= i % 2 ? TERMP_BOLD : TERMP_UNDER;
		if (i > 0)
			p->flags |= TERMP_NOSPACE;
		print_node(p, nn, m);
		p->flags &= i % 2 ? ~TERMP_BOLD : ~TERMP_UNDER;
	}
	return(0);
}


/* ARGSUSED */
static int
pre_RB(DECL_ARGS)
{
	const struct man_node *nn;
	int		 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		if (i % 2)
			p->flags |= TERMP_BOLD;
		if (i > 0)
			p->flags |= TERMP_NOSPACE;
		print_node(p, nn, m);
		if (i % 2)
			p->flags &= ~TERMP_BOLD;
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
		if ( ! (i % 2))
			p->flags |= TERMP_UNDER;
		if (i > 0)
			p->flags |= TERMP_NOSPACE;
		print_node(p, nn, m);
		if ( ! (i % 2))
			p->flags &= ~TERMP_UNDER;
	}
	return(0);
}


/* ARGSUSED */
static int
pre_BR(DECL_ARGS)
{
	const struct man_node *nn;
	int		 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		if ( ! (i % 2))
			p->flags |= TERMP_BOLD;
		if (i > 0)
			p->flags |= TERMP_NOSPACE;
		print_node(p, nn, m);
		if ( ! (i % 2))
			p->flags &= ~TERMP_BOLD;
	}
	return(0);
}


/* ARGSUSED */
static int
pre_BI(DECL_ARGS)
{
	const struct man_node *nn;
	int		 i;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		p->flags |= i % 2 ? TERMP_UNDER : TERMP_BOLD;
		if (i > 0)
			p->flags |= TERMP_NOSPACE;
		print_node(p, nn, m);
		p->flags &= i % 2 ? ~TERMP_UNDER : ~TERMP_BOLD;
	}
	return(0);
}


/* ARGSUSED */
static int
pre_B(DECL_ARGS)
{

	p->flags |= TERMP_BOLD;
	return(1);
}


/* ARGSUSED */
static void
post_B(DECL_ARGS)
{

	p->flags &= ~TERMP_BOLD;
}


/* ARGSUSED */
static int
pre_PP(DECL_ARGS)
{

	term_vspace(p);
	p->offset = INDENT;
	return(0);
}


/* ARGSUSED */
static int
pre_IP(DECL_ARGS)
{
#if 0
	const struct man_node *nn;
	size_t		 offs;
#endif

	term_vspace(p);
	p->offset = INDENT;

#if 0
	if (NULL == (nn = n->child))
		return(1);
	if (MAN_TEXT != nn->type)
		errx(1, "expected text line argument");

	if (nn->next) {
		if (MAN_TEXT != nn->next->type)
			errx(1, "expected text line argument");
		offs = (size_t)atoi(nn->next->string);
	} else
		offs = strlen(nn->string);

	p->offset += offs;
#endif
	p->flags |= TERMP_NOSPACE;
	return(0);
}


/* ARGSUSED */
static int
pre_TP(DECL_ARGS)
{
	const struct man_node *nn;
	size_t		 offs;

	term_vspace(p);
	p->offset = INDENT;

	if (NULL == (nn = n->child))
		return(1);

	if (nn->line == n->line) {
		if (MAN_TEXT != nn->type)
			errx(1, "expected text line argument");
		offs = (size_t)atoi(nn->string);
		nn = nn->next;
	} else
		offs = INDENT;

	for ( ; nn; nn = nn->next)
		print_node(p, nn, m);

	term_flushln(p);
	p->flags |= TERMP_NOSPACE;
	p->offset += offs;
	return(0);
}


/* ARGSUSED */
static int
pre_SS(DECL_ARGS)
{

	term_vspace(p);
	p->flags |= TERMP_BOLD;
	return(1);
}


/* ARGSUSED */
static void
post_SS(DECL_ARGS)
{
	
	term_flushln(p);
	p->flags &= ~TERMP_BOLD;
	p->flags |= TERMP_NOSPACE;
}


/* ARGSUSED */
static int
pre_SH(DECL_ARGS)
{

	term_vspace(p);
	p->offset = 0;
	p->flags |= TERMP_BOLD;
	return(1);
}


/* ARGSUSED */
static void
post_SH(DECL_ARGS)
{
	
	term_flushln(p);
	p->offset = INDENT;
	p->flags &= ~TERMP_BOLD;
	p->flags |= TERMP_NOSPACE;
}


static void
print_node(DECL_ARGS)
{
	int		 c, sz;

	c = 1;

	switch (n->type) {
	case(MAN_ELEM):
		if (termacts[n->tok].pre)
			c = (*termacts[n->tok].pre)(p, n, m);
		break;
	case(MAN_TEXT):
		if (0 == *n->string) {
			term_vspace(p);
			break;
		}
		/*
		 * Note!  This is hacky.  Here, we recognise the `\c'
		 * escape embedded in so many -man pages.  It's supposed
		 * to remove the subsequent space, so we mark NOSPACE if
		 * it's encountered in the string.
		 */
		sz = (int)strlen(n->string);
		term_word(p, n->string);
		if (sz >= 2 && n->string[sz - 1] == 'c' &&
				n->string[sz - 2] == '\\')
			p->flags |= TERMP_NOSPACE;
		break;
	default:
		break;
	}

	if (c && n->child)
		print_body(p, n->child, m);

	switch (n->type) {
	case (MAN_ELEM):
		if (termacts[n->tok].post)
			(*termacts[n->tok].post)(p, n, m);
		break;
	default:
		break;
	}
}


static void
print_body(DECL_ARGS)
{
	print_node(p, n, m);
	if ( ! n->next)
		return;
	print_body(p, n->next, m);
}


static void
print_foot(struct termp *p, const struct man_meta *meta)
{
	struct tm	*tm;
	char		*buf;

	if (NULL == (buf = malloc(p->rmargin)))
		err(1, "malloc");

	tm = localtime(&meta->date);

	if (0 == strftime(buf, p->rmargin, "%B %d, %Y", tm))
		err(1, "strftime");

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

	free(buf);
}


static void
print_head(struct termp *p, const struct man_meta *meta)
{
	char		*buf, *title;

	p->rmargin = p->maxrmargin;
	p->offset = 0;

	if (NULL == (buf = malloc(p->rmargin)))
		err(1, "malloc");
	if (NULL == (title = malloc(p->rmargin)))
		err(1, "malloc");

	if (meta->vol)
		(void)strlcpy(buf, meta->vol, p->rmargin);
	else
		*buf = 0;

	(void)snprintf(title, p->rmargin, "%s(%d)", 
			meta->title, meta->msec);

	p->offset = 0;
	p->rmargin = (p->maxrmargin - strlen(buf)) / 2;
	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;
	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin - strlen(title);

	term_word(p, buf);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin;
	p->flags &= ~TERMP_NOBREAK;
	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	p->rmargin = p->maxrmargin;
	p->offset = 0;
	p->flags &= ~TERMP_NOSPACE;

	free(title);
	free(buf);
}

