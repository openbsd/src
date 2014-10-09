/*	$Id: eqn_html.c,v 1.4 2014/10/09 15:59:08 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "html.h"

static	const enum htmltag fontmap[EQNFONT__MAX] = {
	TAG_SPAN, /* EQNFONT_NONE */
	TAG_SPAN, /* EQNFONT_ROMAN */
	TAG_B, /* EQNFONT_BOLD */
	TAG_B, /* EQNFONT_FAT */
	TAG_I /* EQNFONT_ITALIC */
};

static const struct eqn_box *
	eqn_box(struct html *, const struct eqn_box *, int);


void
print_eqn(struct html *p, const struct eqn *ep)
{
	struct htmlpair	 tag;
	struct tag	*t;

	PAIR_CLASS_INIT(&tag, "eqn");
	t = print_otag(p, TAG_MATH, 1, &tag);

	p->flags |= HTML_NONOSPACE;
	eqn_box(p, ep->root, 1);
	p->flags &= ~HTML_NONOSPACE;

	print_tagq(p, t);
}

/*
 * This function is fairly brittle.
 * This is because the eqn syntax doesn't play so nicely with recusive
 * formats, e.g.,
 *     foo sub bar sub baz
 * ...needs to resolve into
 *     <msub> foo <msub> bar, baz </msub> </msub>
 * In other words, we need to embed some recursive work.
 * FIXME: this does NOT handle right-left associativity or precedence!
 */
static const struct eqn_box *
eqn_box(struct html *p, const struct eqn_box *bp, int next)
{
	struct tag	*post, *pilet, *tmp;
	struct htmlpair	 tag[2];
	int		 skiptwo;

	if (NULL == bp)
		return(NULL);

	post = pilet = NULL;
	skiptwo = 0;

	/*
	 * If we're a "row" under a pile, then open up the piling
	 * context here.
	 * We do this first because the pile surrounds the content of
	 * the contained expression.
	 */
	if (NULL != bp->parent && bp->parent->pile != EQNPILE_NONE) {
		pilet = print_otag(p, TAG_MTR, 0, NULL);
		print_otag(p, TAG_MTD, 0, NULL);
	}
	if (NULL != bp->parent && bp->parent->type == EQN_MATRIX) {
		pilet = print_otag(p, TAG_MTABLE, 0, NULL);
		print_otag(p, TAG_MTR, 0, NULL);
		print_otag(p, TAG_MTD, 0, NULL);
	}

	/*
	 * If we're establishing a pile, start the table mode now.
	 * If we've already in a pile row, then don't override "pilet",
	 * because we'll be closed out anyway.
	 */
	if (bp->pile != EQNPILE_NONE) {
		tmp = print_otag(p, TAG_MTABLE, 0, NULL);
		pilet = (NULL == pilet) ? tmp : pilet;
	}

	/*
	 * Positioning.
	 * This is the most complicated part, and actually doesn't quite
	 * work (FIXME) because it doesn't account for associativity.
	 * Setting "post" will mean that we're only going to process a
	 * single or double following expression.
	 */
	switch (bp->pos) {
	case (EQNPOS_TO):
		post = print_otag(p, TAG_MOVER, 0, NULL);
		break;
	case (EQNPOS_SUP):
		post = print_otag(p, TAG_MSUP, 0, NULL);
		break;
	case (EQNPOS_FROM):
		post = print_otag(p, TAG_MUNDER, 0, NULL);
		break;
	case (EQNPOS_SUB):
		post = print_otag(p, TAG_MSUB, 0, NULL);
		break;
	case (EQNPOS_OVER):
		post = print_otag(p, TAG_MFRAC, 0, NULL);
		break;
	case (EQNPOS_FROMTO):
		post = print_otag(p, TAG_MUNDEROVER, 0, NULL);
		skiptwo = 1;
		break;
	case (EQNPOS_SUBSUP):
		post = print_otag(p, TAG_MSUBSUP, 0, NULL);
		skiptwo = 1;
		break;
	default:
		break;
	}

	/*t = EQNFONT_NONE == bp->font ? NULL :
	    print_otag(p, fontmap[(int)bp->font], 0, NULL);*/

	if (NULL != bp->text) {
		assert(NULL == bp->first);
		/*
		 * We have text.
		 * This can be a number, a function, a variable, or
		 * pretty much anything else.
		 * First, check for some known functions.
		 * If we're going to create a structural node (e.g.,
		 * sqrt), then set the "post" variable only if it's not
		 * already set.
		 */
		if (0 == strcmp(bp->text, "sqrt")) {
			tmp = print_otag(p, TAG_MSQRT, 0, NULL);
			post = (NULL == post) ? tmp : post;
		} else if (0 == strcmp(bp->text, "+") ||
			   0 == strcmp(bp->text, "-") ||
			   0 == strcmp(bp->text, "=") ||
			   0 == strcmp(bp->text, "(") ||
			   0 == strcmp(bp->text, ")") ||
			   0 == strcmp(bp->text, "/")) {
			tmp = print_otag(p, TAG_MO, 0, NULL);
			print_text(p, bp->text);
			print_tagq(p, tmp);
		} else {
			tmp = print_otag(p, TAG_MI, 0, NULL);
			print_text(p, bp->text);
			print_tagq(p, tmp);
		}
	} else if (NULL != bp->first) {
		assert(NULL == bp->text);
		/* 
		 * If we're a "fenced" component (i.e., having
		 * brackets), then process those brackets now.
		 * Otherwise, introduce a dummy row (if we're not
		 * already in a table context).
		 */
		tmp = NULL;
		if (NULL != bp->left || NULL != bp->right) {
			PAIR_INIT(&tag[0], ATTR_OPEN,
				NULL != bp->left ? bp->left : "");
			PAIR_INIT(&tag[1], ATTR_CLOSE,
				NULL != bp->right ? bp->right : "");
			tmp = print_otag(p, TAG_MFENCED, 2, tag);
			print_otag(p, TAG_MROW, 0, NULL);
		} else if (NULL == pilet)
			tmp = print_otag(p, TAG_MROW, 0, NULL);
		eqn_box(p, bp->first, 1);
		if (NULL != tmp)
			print_tagq(p, tmp);
	}

	/*
	 * If a positional context, invoke the "next" context.
	 * This is recursive and will return the end of the recursive
	 * chain of "next" contexts.
	 */
	if (NULL != post) {
		bp = eqn_box(p, bp->next, 0);
		if (skiptwo)
			bp = eqn_box(p, bp->next, 0);
		print_tagq(p, post);
	}

	/* 
	 * If we're being piled (either directly, in the table, or
	 * indirectly in a table row), then close that out.
	 */
	if (NULL != pilet)
		print_tagq(p, pilet);

	/*
	 * If we're normally processing, then grab the next node.
	 * If we're in a recursive context, then don't seek to the next
	 * node; further recursion has already been handled.
	 */
	return(next ? eqn_box(p, bp->next, 1) : bp);
}
