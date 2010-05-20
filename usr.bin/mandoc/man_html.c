/*	$Id: man_html.c,v 1.12 2010/05/20 00:58:02 schwarze Exp $ */
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
			  struct html *h

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
static	void		  man_root_post(MAN_ARGS);
static	int		  man_root_pre(MAN_ARGS);
static	int		  man_B_pre(MAN_ARGS);
static	int		  man_HP_pre(MAN_ARGS);
static	int		  man_I_pre(MAN_ARGS);
static	int		  man_IP_pre(MAN_ARGS);
static	int		  man_PP_pre(MAN_ARGS);
static	int		  man_RS_pre(MAN_ARGS);
static	int		  man_SB_pre(MAN_ARGS);
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
	{ man_SB_pre, NULL }, /* SB */
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
	{ NULL, NULL }, /* i */
	{ man_br_pre, NULL }, /* sp */
	{ NULL, NULL }, /* nf */
	{ NULL, NULL }, /* fi */
	{ NULL, NULL }, /* r */
	{ NULL, NULL }, /* RE */
	{ man_RS_pre, NULL }, /* RS */
	{ man_ign_pre, NULL }, /* DT */
	{ man_ign_pre, NULL }, /* UC */
	{ man_ign_pre, NULL }, /* PD */
	{ man_br_pre, NULL }, /* Sp */
	{ man_ign_pre, NULL }, /* Vb */
	{ NULL, NULL }, /* Ve */
};


void
html_man(void *arg, const struct man *m)
{
	struct html	*h;
	struct tag	*t;

	h = (struct html *)arg;

	print_gen_decls(h);

	t = print_otag(h, TAG_HTML, 0, NULL);
	print_man(man_meta(m), man_node(m), h);
	print_tagq(h, t);

	printf("\n");
}


static void
print_man(MAN_ARGS) 
{
	struct tag	*t;
	struct htmlpair	 tag;

	t = print_otag(h, TAG_HEAD, 0, NULL);

	print_man_head(m, n, h);
	print_tagq(h, t);
	t = print_otag(h, TAG_BODY, 0, NULL);

	tag.key = ATTR_CLASS;
	tag.val = "body";
	print_otag(h, TAG_DIV, 1, &tag);

	print_man_nodelist(m, n, h);

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

	print_man_node(m, n, h);
	if (n->next)
		print_man_nodelist(m, n->next, h);
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
		child = man_root_pre(m, n, h);
		break;
	case (MAN_TEXT):
		print_text(h, n->string);
		return;
	default:
		/* 
		 * Close out scope of font prior to opening a macro
		 * scope.  Assert that the metafont is on the top of the
		 * stack (it's never nested).
		 */
		if (h->metaf) {
			assert(h->metaf == t);
			print_tagq(h, h->metaf);
			assert(NULL == h->metaf);
			t = h->tags.head;
		}
		if (mans[n->tok].pre)
			child = (*mans[n->tok].pre)(m, n, h);
		break;
	}

	if (child && n->child)
		print_man_nodelist(m, n->child, h);

	/* This will automatically close out any font scope. */
	print_stagq(h, t);

	bufinit(h);

	switch (n->type) {
	case (MAN_ROOT):
		man_root_post(m, n, h);
		break;
	case (MAN_TEXT):
		break;
	default:
		if (mans[n->tok].post)
			(*mans[n->tok].post)(m, n, h);
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

	PAIR_CLASS_INIT(&tag[0], "header");
	bufcat_style(h, "width", "100%");
	PAIR_STYLE_INIT(&tag[1], h);
	PAIR_SUMMARY_INIT(&tag[2], "header");

	t = print_otag(h, TAG_TABLE, 3, tag);
	tt = print_otag(h, TAG_TR, 0, NULL);

	bufinit(h);
	bufcat_style(h, "width", "10%");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, title);
	print_stagq(h, tt);

	bufinit(h);
	bufcat_style(h, "width", "80%");
	bufcat_style(h, "white-space", "nowrap");
	bufcat_style(h, "text-align", "center");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, b);
	print_stagq(h, tt);

	bufinit(h);
	bufcat_style(h, "width", "10%");
	bufcat_style(h, "text-align", "right");
	PAIR_STYLE_INIT(&tag[0], h);
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

	time2a(m->date, b, DATESIZ);

	PAIR_CLASS_INIT(&tag[0], "footer");
	bufcat_style(h, "width", "100%");
	PAIR_STYLE_INIT(&tag[1], h);
	PAIR_SUMMARY_INIT(&tag[2], "footer");

	t = print_otag(h, TAG_TABLE, 3, tag);
	tt = print_otag(h, TAG_TR, 0, NULL);

	bufinit(h);
	bufcat_style(h, "width", "50%");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, b);
	print_stagq(h, tt);

	bufinit(h);
	bufcat_style(h, "width", "50%");
	bufcat_style(h, "text-align", "right");
	PAIR_STYLE_INIT(&tag[0], h);
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

	switch (n->tok) {
	case (MAN_Sp):
		SCALE_VS_INIT(&su, 0.5);
		break;
	case (MAN_sp):
		if (n->child)
			a2roffsu(n->child->string, &su, SCALE_VS);
		break;
	default:
		su.scale = 0;
		break;
	}

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
	struct htmlpair	 tag[2];
	struct roffsu	 su;

	if (MAN_BODY == n->type) {
		SCALE_HS_INIT(&su, INDENT);
		bufcat_su(h, "margin-left", &su);
		PAIR_CLASS_INIT(&tag[0], "sec-body");
		PAIR_STYLE_INIT(&tag[1], h);
		print_otag(h, TAG_DIV, 2, tag);
		return(1);
	} else if (MAN_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag[0], "sec-block");
		if (n->prev && MAN_SH == n->prev->tok)
			if (NULL == n->prev->body->child) {
				print_otag(h, TAG_DIV, 1, tag);
				return(1);
			}

		SCALE_VS_INIT(&su, 1);
		bufcat_su(h, "margin-top", &su);
		if (NULL == n->next)
			bufcat_su(h, "margin-bottom", &su);
		PAIR_STYLE_INIT(&tag[1], h);
		print_otag(h, TAG_DIV, 2, tag);
		return(1);
	}

	PAIR_CLASS_INIT(&tag[0], "sec-head");
	print_otag(h, TAG_DIV, 1, tag);
	return(1);
}


/* ARGSUSED */
static int
man_alt_pre(MAN_ARGS)
{
	const struct man_node	*nn;
	struct tag		*t;
	int			 i;
	enum htmlfont		 fp;

	for (i = 0, nn = n->child; nn; nn = nn->next, i++) {
		switch (n->tok) {
		case (MAN_BI):
			fp = i % 2 ? HTMLFONT_ITALIC : HTMLFONT_BOLD;
			break;
		case (MAN_IB):
			fp = i % 2 ? HTMLFONT_BOLD : HTMLFONT_ITALIC;
			break;
		case (MAN_RI):
			fp = i % 2 ? HTMLFONT_ITALIC : HTMLFONT_NONE;
			break;
		case (MAN_IR):
			fp = i % 2 ? HTMLFONT_NONE : HTMLFONT_ITALIC;
			break;
		case (MAN_BR):
			fp = i % 2 ? HTMLFONT_NONE : HTMLFONT_BOLD;
			break;
		case (MAN_RB):
			fp = i % 2 ? HTMLFONT_BOLD : HTMLFONT_NONE;
			break;
		default:
			abort();
			/* NOTREACHED */
		}

		if (i)
			h->flags |= HTML_NOSPACE;

		/* 
		 * Open and close the scope with each argument, so that
		 * internal \f escapes, which are common, are also
		 * closed out with the scope.
		 */
		t = print_ofont(h, fp);
		print_man_node(m, nn, h);
		print_tagq(h, t);
	}

	return(0);
}


/* ARGSUSED */
static int
man_SB_pre(MAN_ARGS)
{
	struct htmlpair	 tag;
	
	/* FIXME: print_ofont(). */
	PAIR_CLASS_INIT(&tag, "small bold");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
man_SM_pre(MAN_ARGS)
{
	struct htmlpair	 tag;
	
	PAIR_CLASS_INIT(&tag, "small");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
man_SS_pre(MAN_ARGS)
{
	struct htmlpair	 tag[3];
	struct roffsu	 su;

	SCALE_VS_INIT(&su, 1);

	if (MAN_BODY == n->type) {
		PAIR_CLASS_INIT(&tag[0], "ssec-body");
		if (n->parent->next && n->child) {
			bufcat_su(h, "margin-bottom", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_DIV, 2, tag);
			return(1);
		}

		print_otag(h, TAG_DIV, 1, tag);
		return(1);
	} else if (MAN_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag[0], "ssec-block");
		if (n->prev && MAN_SS == n->prev->tok) 
			if (n->prev->body->child) {
				bufcat_su(h, "margin-top", &su);
				PAIR_STYLE_INIT(&tag[1], h);
				print_otag(h, TAG_DIV, 2, tag);
				return(1);
			}

		print_otag(h, TAG_DIV, 1, tag);
		return(1);
	}

	SCALE_HS_INIT(&su, INDENT - HALFINDENT);
	bufcat_su(h, "margin-left", &su);
	PAIR_CLASS_INIT(&tag[0], "ssec-head");
	PAIR_STYLE_INIT(&tag[1], h);
	print_otag(h, TAG_DIV, 2, tag);
	return(1);
}


/* ARGSUSED */
static int
man_PP_pre(MAN_ARGS)
{
	struct htmlpair	 tag;
	struct roffsu	 su;
	int		 i;

	if (MAN_BLOCK != n->type)
		return(1);

	i = 0;

	if (MAN_ROOT == n->parent->type) {
		SCALE_HS_INIT(&su, INDENT);
		bufcat_su(h, "margin-left", &su);
		i = 1;
	}
	if (n->prev) {
		SCALE_VS_INIT(&su, 1);
		bufcat_su(h, "margin-top", &su);
		i = 1;
	}

	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, i, &tag);
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
		SCALE_HS_INIT(&su, INDENT);
		bufcat_su(h, "margin-left", &su);
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
		return(1);
	}

	nn = MAN_BLOCK == n->type ? 
		n->head->child : n->parent->head->child;

	SCALE_HS_INIT(&su, INDENT);
	width = 0;

	/* Width is the last token. */

	if (MAN_IP == n->tok && NULL != nn)
		if (NULL != (nn = nn->next)) {
			for ( ; nn->next; nn = nn->next)
				/* Do nothing. */ ;
			width = a2width(nn, &su);
		}

	/* Width is the first token. */

	if (MAN_TP == n->tok && NULL != nn) {
		/* Skip past non-text children. */
		while (nn && MAN_TEXT != nn->type)
			nn = nn->next;
		if (nn)
			width = a2width(nn, &su);
	}

	if (MAN_BLOCK == n->type) {
		bufcat_su(h, "margin-left", &su);
		SCALE_VS_INIT(&su, 1);
		bufcat_su(h, "margin-top", &su);
		bufcat_style(h, "clear", "both");
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
		return(1);
	} 

	bufcat_su(h, "min-width", &su);
	SCALE_INVERT(&su);
	bufcat_su(h, "margin-left", &su);
	SCALE_HS_INIT(&su, 1);
	bufcat_su(h, "margin-right", &su);
	bufcat_style(h, "clear", "left");

	if (n->next && n->next->child)
		bufcat_style(h, "float", "left");

	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);

	/*
	 * Without a length string, we can print all of our children.
	 */

	if ( ! width)
		return(1);

	/*
	 * When a length has been specified, we need to carefully print
	 * our child context:  IP gets all children printed but the last
	 * (the width), while TP gets all children printed but the first
	 * (the width).
	 */

	if (MAN_IP == n->tok)
		for (nn = n->child; nn->next; nn = nn->next)
			print_man_node(m, nn, h);
	if (MAN_TP == n->tok)
		for (nn = n->child->next; nn; nn = nn->next)
			print_man_node(m, nn, h);

	return(0);
}


/* ARGSUSED */
static int
man_HP_pre(MAN_ARGS)
{
	const struct man_node	*nn;
	struct htmlpair	 	 tag;
	struct roffsu		 su;

	if (MAN_HEAD == n->type)
		return(0);

	nn = MAN_BLOCK == n->type ?
		n->head->child : n->parent->head->child;

	SCALE_HS_INIT(&su, INDENT);

	if (NULL != nn)
		(void)a2width(nn, &su);

	if (MAN_BLOCK == n->type) {
		bufcat_su(h, "margin-left", &su);
		SCALE_VS_INIT(&su, 1);
		bufcat_su(h, "margin-top", &su);
		bufcat_style(h, "clear", "both");
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
		return(1);
	}

	bufcat_su(h, "margin-left", &su);
	SCALE_INVERT(&su);
	bufcat_su(h, "text-indent", &su);

	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
man_B_pre(MAN_ARGS)
{

	print_ofont(h, HTMLFONT_BOLD);
	return(1);
}


/* ARGSUSED */
static int
man_I_pre(MAN_ARGS)
{
	
	print_ofont(h, HTMLFONT_ITALIC);
	return(1);
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
	bufcat_su(h, "margin-left", &su);

	if (n->head->child) {
		SCALE_VS_INIT(&su, 1);
		a2width(n->head->child, &su);
		bufcat_su(h, "margin-top", &su);
	}

	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);
	return(1);
}
