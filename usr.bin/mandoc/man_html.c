/*	$Id: man_html.c,v 1.29 2011/01/04 22:28:17 schwarze Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "html.h"
#include "man.h"
#include "main.h"

/* TODO: preserve ident widths. */
/* FIXME: have PD set the default vspace width. */

#define	INDENT		  5
#define	HALFINDENT	  3

#define	MAN_ARGS	  const struct man_meta *m, \
			  const struct man_node *n, \
			  struct mhtml *mh, \
			  struct html *h

struct	mhtml {
	int		  fl;
#define	MANH_LITERAL	 (1 << 0) /* literal context */
};

struct	htmlman {
	int		(*pre)(MAN_ARGS);
	int		(*post)(MAN_ARGS);
};

static	void		  print_man(MAN_ARGS);
static	void		  print_man_head(MAN_ARGS);
static	void		  print_man_nodelist(MAN_ARGS);
static	void		  print_man_node(MAN_ARGS);

static	int		  a2width(const struct man_node *,
				struct roffsu *);

static	int		  man_alt_pre(MAN_ARGS);
static	int		  man_br_pre(MAN_ARGS);
static	int		  man_ign_pre(MAN_ARGS);
static	int		  man_in_pre(MAN_ARGS);
static	int		  man_literal_pre(MAN_ARGS);
static	void		  man_root_post(MAN_ARGS);
static	int		  man_root_pre(MAN_ARGS);
static	int		  man_B_pre(MAN_ARGS);
static	int		  man_HP_pre(MAN_ARGS);
static	int		  man_I_pre(MAN_ARGS);
static	int		  man_IP_pre(MAN_ARGS);
static	int		  man_PP_pre(MAN_ARGS);
static	int		  man_RS_pre(MAN_ARGS);
static	int		  man_SH_pre(MAN_ARGS);
static	int		  man_SM_pre(MAN_ARGS);
static	int		  man_SS_pre(MAN_ARGS);

static	const struct htmlman mans[MAN_MAX] = {
	{ man_br_pre, NULL }, /* br */
	{ NULL, NULL }, /* TH */
	{ man_SH_pre, NULL }, /* SH */
	{ man_SS_pre, NULL }, /* SS */
	{ man_IP_pre, NULL }, /* TP */
	{ man_PP_pre, NULL }, /* LP */
	{ man_PP_pre, NULL }, /* PP */
	{ man_PP_pre, NULL }, /* P */
	{ man_IP_pre, NULL }, /* IP */
	{ man_HP_pre, NULL }, /* HP */ 
	{ man_SM_pre, NULL }, /* SM */
	{ man_SM_pre, NULL }, /* SB */
	{ man_alt_pre, NULL }, /* BI */
	{ man_alt_pre, NULL }, /* IB */
	{ man_alt_pre, NULL }, /* BR */
	{ man_alt_pre, NULL }, /* RB */
	{ NULL, NULL }, /* R */
	{ man_B_pre, NULL }, /* B */
	{ man_I_pre, NULL }, /* I */
	{ man_alt_pre, NULL }, /* IR */
	{ man_alt_pre, NULL }, /* RI */
	{ NULL, NULL }, /* na */
	{ man_br_pre, NULL }, /* sp */
	{ man_literal_pre, NULL }, /* nf */
	{ man_literal_pre, NULL }, /* fi */
	{ NULL, NULL }, /* RE */
	{ man_RS_pre, NULL }, /* RS */
	{ man_ign_pre, NULL }, /* DT */
	{ man_ign_pre, NULL }, /* UC */
	{ man_ign_pre, NULL }, /* PD */
	{ man_ign_pre, NULL }, /* AT */
	{ man_in_pre, NULL }, /* in */
	{ man_ign_pre, NULL }, /* ft */
};


void
html_man(void *arg, const struct man *m)
{
	struct html	*h;
	struct tag	*t;
	struct mhtml	 mh;

	h = (struct html *)arg;

	print_gen_decls(h);

	memset(&mh, 0, sizeof(struct mhtml));

	t = print_otag(h, TAG_HTML, 0, NULL);
	print_man(man_meta(m), man_node(m), &mh, h);
	print_tagq(h, t);

	printf("\n");
}


static void
print_man(MAN_ARGS) 
{
	struct tag	*t;

	t = print_otag(h, TAG_HEAD, 0, NULL);
	print_man_head(m, n, mh, h);
	print_tagq(h, t);

	t = print_otag(h, TAG_BODY, 0, NULL);
	print_man_nodelist(m, n, mh, h);
	print_tagq(h, t);
}


/* ARGSUSED */
static void
print_man_head(MAN_ARGS)
{

	print_gen_head(h);
	bufinit(h);
	buffmt(h, "%s(%s)", m->title, m->msec);

	print_otag(h, TAG_TITLE, 0, NULL);
	print_text(h, h->buf);
}


static void
print_man_nodelist(MAN_ARGS)
{

	print_man_node(m, n, mh, h);
	if (n->next)
		print_man_nodelist(m, n->next, mh, h);
}


static void
print_man_node(MAN_ARGS)
{
	int		 child;
	struct tag	*t;

	child = 1;
	t = h->tags.head;

	bufinit(h);

	/*
	 * FIXME: embedded elements within next-line scopes (e.g., `br'
	 * within an empty `B') will cause formatting to be forgotten
	 * due to scope closing out.
	 */

	switch (n->type) {
	case (MAN_ROOT):
		child = man_root_pre(m, n, mh, h);
		break;
	case (MAN_TEXT):
		print_text(h, n->string);
		if (MANH_LITERAL & mh->fl)
			print_otag(h, TAG_BR, 0, NULL);
		return;
	case (MAN_TBL):
		print_tbl(h, n->span);
		break;
	default:
		/* 
		 * Close out scope of font prior to opening a macro
		 * scope.  Assert that the metafont is on the top of the
		 * stack (it's never nested).
		 */
		if (HTMLFONT_NONE != h->metac) {
			h->metal = h->metac;
			h->metac = HTMLFONT_NONE;
		}
		if (mans[n->tok].pre)
			child = (*mans[n->tok].pre)(m, n, mh, h);
		break;
	}

	if (child && n->child)
		print_man_nodelist(m, n->child, mh, h);

	/* This will automatically close out any font scope. */
	print_stagq(h, t);

	bufinit(h);

	switch (n->type) {
	case (MAN_ROOT):
		man_root_post(m, n, mh, h);
		break;
	case (MAN_TBL):
		break;
	default:
		if (mans[n->tok].post)
			(*mans[n->tok].post)(m, n, mh, h);
		break;
	}
}


static int
a2width(const struct man_node *n, struct roffsu *su)
{

	if (MAN_TEXT != n->type)
		return(0);
	if (a2roffsu(n->string, su, SCALE_BU))
		return(1);

	return(0);
}


/* ARGSUSED */
static int
man_root_pre(MAN_ARGS)
{
	struct htmlpair	 tag[3];
	struct tag	*t, *tt;
	char		 b[BUFSIZ], title[BUFSIZ];

	b[0] = 0;
	if (m->vol)
		(void)strlcat(b, m->vol, BUFSIZ);

	snprintf(title, BUFSIZ - 1, "%s(%s)", m->title, m->msec);

	PAIR_SUMMARY_INIT(&tag[0], "Document Header");
	PAIR_CLASS_INIT(&tag[1], "head");
	if (NULL == h->style) {
		PAIR_INIT(&tag[2], ATTR_WIDTH, "100%");
		t = print_otag(h, TAG_TABLE, 3, tag);
		PAIR_INIT(&tag[0], ATTR_WIDTH, "30%");
		print_otag(h, TAG_COL, 1, tag);
		print_otag(h, TAG_COL, 1, tag);
		print_otag(h, TAG_COL, 1, tag);
	} else
		t = print_otag(h, TAG_TABLE, 2, tag);

	print_otag(h, TAG_TBODY, 0, NULL);

	tt = print_otag(h, TAG_TR, 0, NULL);

	PAIR_CLASS_INIT(&tag[0], "head-ltitle");
	print_otag(h, TAG_TD, 1, tag);

	print_text(h, title);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag[0], "head-vol");
	if (NULL == h->style) {
		PAIR_INIT(&tag[1], ATTR_ALIGN, "center");
		print_otag(h, TAG_TD, 2, tag);
	} else 
		print_otag(h, TAG_TD, 1, tag);

	print_text(h, b);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag[0], "head-rtitle");
	if (NULL == h->style) {
		PAIR_INIT(&tag[1], ATTR_ALIGN, "right");
		print_otag(h, TAG_TD, 2, tag);
	} else 
		print_otag(h, TAG_TD, 1, tag);

	print_text(h, title);
	print_tagq(h, t);
	return(1);
}


/* ARGSUSED */
static void
man_root_post(MAN_ARGS)
{
	struct htmlpair	 tag[3];
	struct tag	*t, *tt;
	char		 b[DATESIZ];

	if (m->rawdate)
		strlcpy(b, m->rawdate, DATESIZ);
	else
		time2a(m->date, b, DATESIZ);

	PAIR_SUMMARY_INIT(&tag[0], "Document Footer");
	PAIR_CLASS_INIT(&tag[1], "foot");
	if (NULL == h->style) {
		PAIR_INIT(&tag[2], ATTR_WIDTH, "100%");
		t = print_otag(h, TAG_TABLE, 3, tag);
		PAIR_INIT(&tag[0], ATTR_WIDTH, "50%");
		print_otag(h, TAG_COL, 1, tag);
		print_otag(h, TAG_COL, 1, tag);
	} else
		t = print_otag(h, TAG_TABLE, 2, tag);

	tt = print_otag(h, TAG_TR, 0, NULL);

	PAIR_CLASS_INIT(&tag[0], "foot-date");
	print_otag(h, TAG_TD, 1, tag);

	print_text(h, b);
	print_stagq(h, tt);

	PAIR_CLASS_INIT(&tag[0], "foot-os");
	if (NULL == h->style) {
		PAIR_INIT(&tag[1], ATTR_ALIGN, "right");
		print_otag(h, TAG_TD, 2, tag);
	} else 
		print_otag(h, TAG_TD, 1, tag);

	if (m->source)
		print_text(h, m->source);
	print_tagq(h, t);
}



/* ARGSUSED */
static int
man_br_pre(MAN_ARGS)
{
	struct roffsu	 su;
	struct htmlpair	 tag;

	SCALE_VS_INIT(&su, 1);

	if (MAN_sp == n->tok) {
		if (n->child)
			a2roffsu(n->child->string, &su, SCALE_VS);
	} else
		su.scale = 0;

	bufcat_su(h, "height", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);

	/* So the div isn't empty: */
	print_text(h, "\\~");

	return(0);
}


/* ARGSUSED */
static int
man_SH_pre(MAN_ARGS)
{
	struct htmlpair	 tag;

	if (MAN_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag, "section");
		print_otag(h, TAG_DIV, 1, &tag);
		return(1);
	} else if (MAN_BODY == n->type)
		return(1);

	print_otag(h, TAG_H1, 0, NULL);
	return(1);
}


/* ARGSUSED */
static int
man_alt_pre(MAN_ARGS)
{
	const struct man_node	*nn;
	int		 i;
	enum htmltag	 fp;
	struct tag	*t;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		t = NULL;
		switch (n->tok) {
		case (MAN_BI):
			fp = i % 2 ? TAG_I : TAG_B;
			break;
		case (MAN_IB):
			fp = i % 2 ? TAG_B : TAG_I;
			break;
		case (MAN_RI):
			fp = i % 2 ? TAG_I : TAG_MAX;
			break;
		case (MAN_IR):
			fp = i % 2 ? TAG_MAX : TAG_I;
			break;
		case (MAN_BR):
			fp = i % 2 ? TAG_MAX : TAG_B;
			break;
		case (MAN_RB):
			fp = i % 2 ? TAG_B : TAG_MAX;
			break;
		default:
			abort();
			/* NOTREACHED */
		}

		if (i)
			h->flags |= HTML_NOSPACE;

		if (TAG_MAX != fp)
			t = print_otag(h, fp, 0, NULL);

		print_man_node(m, nn, mh, h);

		if (t)
			print_tagq(h, t);
	}

	return(0);
}


/* ARGSUSED */
static int
man_SM_pre(MAN_ARGS)
{
	
	print_otag(h, TAG_SMALL, 0, NULL);
	if (MAN_SB == n->tok)
		print_otag(h, TAG_B, 0, NULL);
	return(1);
}


/* ARGSUSED */
static int
man_SS_pre(MAN_ARGS)
{
	struct htmlpair	 tag;

	if (MAN_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag, "subsection");
		print_otag(h, TAG_DIV, 1, &tag);
		return(1);
	} else if (MAN_BODY == n->type)
		return(1);

	print_otag(h, TAG_H2, 0, NULL);
	return(1);
}


/* ARGSUSED */
static int
man_PP_pre(MAN_ARGS)
{

	if (MAN_HEAD == n->type)
		return(0);
	else if (MAN_BODY == n->type && n->prev)
		print_otag(h, TAG_P, 0, NULL);

	return(1);
}


/* ARGSUSED */
static int
man_IP_pre(MAN_ARGS)
{
	struct roffsu		 su;
	struct htmlpair	 	 tag;
	const struct man_node	*nn;
	int			 width;

	/*
	 * This scattering of 1-BU margins and pads is to make sure that
	 * when text overruns its box, the subsequent text isn't flush
	 * up against it.  However, the rest of the right-hand box must
	 * also be adjusted in consideration of this 1-BU space.
	 */

	if (MAN_BODY == n->type) { 
		print_otag(h, TAG_TD, 0, NULL);
		return(1);
	}

	nn = MAN_BLOCK == n->type ? 
		n->head->child : n->parent->head->child;

	SCALE_HS_INIT(&su, INDENT);
	width = 0;

	/* Width is the second token. */

	if (MAN_IP == n->tok && NULL != nn)
		if (NULL != (nn = nn->next))
			width = a2width(nn, &su);

	/* Width is the first token. */

	if (MAN_TP == n->tok && NULL != nn) {
		/* Skip past non-text children. */
		while (nn && MAN_TEXT != nn->type)
			nn = nn->next;
		if (nn)
			width = a2width(nn, &su);
	}

	if (MAN_BLOCK == n->type) {
		print_otag(h, TAG_P, 0, NULL);
		print_otag(h, TAG_TABLE, 0, NULL);
		bufcat_su(h, "width", &su);
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_COL, 1, &tag);
		print_otag(h, TAG_COL, 0, NULL);
		print_otag(h, TAG_TBODY, 0, NULL);
		print_otag(h, TAG_TR, 0, NULL);
		return(1);
	} 

	print_otag(h, TAG_TD, 0, NULL);

	/* For IP, only print the first header element. */

	if (MAN_IP == n->tok && n->child)
		print_man_node(m, n->child, mh, h);

	/* For TP, only print next-line header elements. */

	if (MAN_TP == n->tok)
		for (nn = n->child; nn; nn = nn->next)
			if (nn->line > n->line)
				print_man_node(m, nn, mh, h);

	return(0);
}


/* ARGSUSED */
static int
man_HP_pre(MAN_ARGS)
{
	struct htmlpair	 tag;
	struct roffsu	 su;
	const struct man_node *np;

	np = MAN_BLOCK == n->type ? 
		n->head->child : 
		n->parent->head->child;

	if (NULL == np || ! a2width(np, &su))
		SCALE_HS_INIT(&su, INDENT);

	if (MAN_HEAD == n->type) {
		print_otag(h, TAG_TD, 0, NULL);
		return(0);
	} else if (MAN_BLOCK == n->type) {
		print_otag(h, TAG_P, 0, NULL);
		print_otag(h, TAG_TABLE, 0, NULL);
		bufcat_su(h, "width", &su);
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_COL, 1, &tag);
		print_otag(h, TAG_COL, 0, NULL);
		print_otag(h, TAG_TBODY, 0, NULL);
		print_otag(h, TAG_TR, 0, NULL);
		return(1);
	}

	su.scale = -su.scale;
	bufcat_su(h, "text-indent", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_TD, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
man_B_pre(MAN_ARGS)
{

	print_otag(h, TAG_B, 0, NULL);
	return(1);
}


/* ARGSUSED */
static int
man_I_pre(MAN_ARGS)
{
	
	print_otag(h, TAG_I, 0, NULL);
	return(1);
}


/* ARGSUSED */
static int
man_literal_pre(MAN_ARGS)
{

	if (MAN_nf == n->tok) {
		print_otag(h, TAG_BR, 0, NULL);
		mh->fl |= MANH_LITERAL;
	} else
		mh->fl &= ~MANH_LITERAL;

	return(1);
}


/* ARGSUSED */
static int
man_in_pre(MAN_ARGS)
{

	print_otag(h, TAG_BR, 0, NULL);
	return(0);
}


/* ARGSUSED */
static int
man_ign_pre(MAN_ARGS)
{

	return(0);
}


/* ARGSUSED */
static int
man_RS_pre(MAN_ARGS)
{
	struct htmlpair	 tag;
	struct roffsu	 su;

	if (MAN_HEAD == n->type)
		return(0);
	else if (MAN_BODY == n->type)
		return(1);

	SCALE_HS_INIT(&su, INDENT);
	if (n->head->child)
		a2width(n->head->child, &su);

	bufcat_su(h, "margin-left", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);
	return(1);
}
