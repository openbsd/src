/*	$Id: man_term.c,v 1.13 2009/08/22 23:17:40 schwarze Exp $ */
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
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term.h"
#include "man.h"

#define	INDENT		  7
#define	HALFINDENT	  3

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
};

static	int		  pre_B(DECL_ARGS);
static	int		  pre_BI(DECL_ARGS);
static	int		  pre_BR(DECL_ARGS);
static	int		  pre_HP(DECL_ARGS);
static	int		  pre_I(DECL_ARGS);
static	int		  pre_IB(DECL_ARGS);
static	int		  pre_IP(DECL_ARGS);
static	int		  pre_IR(DECL_ARGS);
static	int		  pre_PP(DECL_ARGS);
static	int		  pre_RB(DECL_ARGS);
static	int		  pre_RI(DECL_ARGS);
static	int		  pre_RS(DECL_ARGS);
static	int		  pre_SH(DECL_ARGS);
static	int		  pre_SS(DECL_ARGS);
static	int		  pre_TP(DECL_ARGS);
static	int		  pre_br(DECL_ARGS);
static	int		  pre_fi(DECL_ARGS);
static	int		  pre_nf(DECL_ARGS);
static	int		  pre_r(DECL_ARGS);
static	int		  pre_sp(DECL_ARGS);

static	void		  post_B(DECL_ARGS);
static	void		  post_I(DECL_ARGS);
static	void		  post_IP(DECL_ARGS);
static	void		  post_HP(DECL_ARGS);
static	void		  post_RS(DECL_ARGS);
static	void		  post_SH(DECL_ARGS);
static	void		  post_SS(DECL_ARGS);
static	void		  post_TP(DECL_ARGS);
static	void		  post_i(DECL_ARGS);

static const struct termact termacts[MAN_MAX] = {
	{ pre_br, NULL }, /* br */
	{ NULL, NULL }, /* TH */
	{ pre_SH, post_SH }, /* SH */
	{ pre_SS, post_SS }, /* SS */
	{ pre_TP, post_TP }, /* TP */
	{ pre_PP, NULL }, /* LP */
	{ pre_PP, NULL }, /* PP */
	{ pre_PP, NULL }, /* P */
	{ pre_IP, post_IP }, /* IP */
	{ pre_HP, post_HP }, /* HP */ 
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
	{ NULL, NULL }, /* na */
	{ pre_I, post_i }, /* i */
	{ pre_sp, NULL }, /* sp */
	{ pre_nf, NULL }, /* nf */
	{ pre_fi, NULL }, /* fi */
	{ pre_r, NULL }, /* r */
	{ NULL, NULL }, /* RE */
	{ pre_RS, post_RS }, /* RS */
	{ NULL, NULL }, /* DT */
};

static	void		  print_head(struct termp *, 
				const struct man_meta *);
static	void		  print_body(DECL_ARGS);
static	void		  print_node(DECL_ARGS);
static	void		  print_foot(struct termp *, 
				const struct man_meta *);
static	void		  fmt_block_vspace(struct termp *, 
				const struct man_node *);
static	int		  arg_width(const struct man_node *);


int
man_run(struct termp *p, const struct man *m)
{
	struct mtermp	 mt;

	print_head(p, man_meta(m));
	p->flags |= TERMP_NOSPACE;
	assert(man_node(m));
	assert(MAN_ROOT == man_node(m)->type);

	mt.fl = 0;
	mt.lmargin = INDENT;
	mt.offset = INDENT;

	if (man_node(m)->child)
		print_body(p, &mt, man_node(m)->child, man_meta(m));
	print_foot(p, man_meta(m));

	return(1);
}


static void
fmt_block_vspace(struct termp *p, const struct man_node *n)
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


static int
arg_width(const struct man_node *n)
{
	int		 i, len;
	const char	*p;

	assert(MAN_TEXT == n->type);
	assert(n->string);

	p = n->string;

	if (0 == (len = (int)strlen(p)))
		return(-1);

	for (i = 0; i < len; i++) 
		if ( ! isdigit((u_char)p[i]))
			break;

	if (i == len - 1)  {
		if ('n' == p[len - 1] || 'm' == p[len - 1])
			return(atoi(p));
	} else if (i == len)
		return(atoi(p));

	return(-1);
}


/* ARGSUSED */
static int
pre_I(DECL_ARGS)
{

	p->flags |= TERMP_UNDER;
	return(1);
}


/* ARGSUSED */
static int
pre_r(DECL_ARGS)
{

	p->flags &= ~TERMP_UNDER;
	p->flags &= ~TERMP_BOLD;
	return(1);
}


/* ARGSUSED */
static void
post_i(DECL_ARGS)
{

	if (n->nchild)
		p->flags &= ~TERMP_UNDER;
}


/* ARGSUSED */
static void
post_I(DECL_ARGS)
{

	p->flags &= ~TERMP_UNDER;
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

	term_newln(p);
	mt->fl |= MANT_LITERAL;
	return(1);
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
		print_node(p, mt, nn, m);
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
		print_node(p, mt, nn, m);
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
		print_node(p, mt, nn, m);
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
		print_node(p, mt, nn, m);
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
		print_node(p, mt, nn, m);
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
		print_node(p, mt, nn, m);
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
pre_sp(DECL_ARGS)
{
	int		 i, len;

	if (NULL == n->child) {
		term_vspace(p);
		return(0);
	}

	len = atoi(n->child->string);
	if (0 == len)
		term_newln(p);
	for (i = 0; i < len; i++)
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
		fmt_block_vspace(p, n);
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
		if ((ival = arg_width(nn)) >= 0)
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
		fmt_block_vspace(p, n);
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
		p->flags |= TERMP_TWOSPACE;
		break;
	case (MAN_BLOCK):
		fmt_block_vspace(p, n);
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
			if ((ival = arg_width(nn)) >= 0)
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
			print_node(p, mt, nn, m);
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
		fmt_block_vspace(p, n);
		/* FALLTHROUGH */
	default:
		return(1);
	}

	len = (size_t)mt->lmargin;
	ival = -1;

	/* Calculate offset. */

	if (NULL != (nn = n->parent->head->child))
		if (NULL != nn->next)
			if ((ival = arg_width(nn)) >= 0)
				len = (size_t)ival;

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
				print_node(p, mt, nn, m);

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
		p->flags |= TERMP_BOLD;
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
		p->flags &= ~TERMP_BOLD;
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
		term_vspace(p);
		break;
	case (MAN_HEAD):
		p->flags |= TERMP_BOLD;
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
		p->flags &= ~TERMP_BOLD;
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

	if ((ival = arg_width(nn)) < 0)
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
	default:
		term_newln(p);
		p->offset = INDENT;
		break;
	}
}


static void
print_node(DECL_ARGS)
{
	int		 c, sz;

	c = 1;

	switch (n->type) {
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
		/* FIXME: this means that macro lines are munged!  */
		if (MANT_LITERAL & mt->fl) {
			p->flags |= TERMP_NOSPACE;
			term_flushln(p);
		}
		break;
	default:
		if (termacts[n->tok].pre)
			c = (*termacts[n->tok].pre)(p, mt, n, m);
		break;
	}

	if (c && n->child)
		print_body(p, mt, n->child, m);

	if (MAN_TEXT != n->type)
		if (termacts[n->tok].post)
			(*termacts[n->tok].post)(p, mt, n, m);
}


static void
print_body(DECL_ARGS)
{

	print_node(p, mt, n, m);
	if ( ! n->next)
		return;
	print_body(p, mt, n->next, m);
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
	p->rmargin = (p->maxrmargin - strlen(buf) + 1) / 2;
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

