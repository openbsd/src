/*	$Id: tree.c,v 1.10 2010/07/13 01:09:13 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <time.h>

#include "mandoc.h"
#include "mdoc.h"
#include "man.h"
#include "main.h"

static	void	print_mdoc(const struct mdoc_node *, int);
static	void	print_man(const struct man_node *, int);


/* ARGSUSED */
void
tree_mdoc(void *arg, const struct mdoc *mdoc)
{

	print_mdoc(mdoc_node(mdoc), 0);
}


/* ARGSUSED */
void
tree_man(void *arg, const struct man *man)
{

	print_man(man_node(man), 0);
}


static void
print_mdoc(const struct mdoc_node *n, int indent)
{
	const char	 *p, *t;
	int		  i, j;
	size_t		  argc, sz;
	char		**params;
	struct mdoc_argv *argv;

	argv = NULL;
	argc = sz = 0;
	params = NULL;

	switch (n->type) {
	case (MDOC_ROOT):
		t = "root";
		break;
	case (MDOC_BLOCK):
		t = "block";
		break;
	case (MDOC_HEAD):
		t = "block-head";
		break;
	case (MDOC_BODY):
		if (n->end)
			t = "body-end";
		else
			t = "block-body";
		break;
	case (MDOC_TAIL):
		t = "block-tail";
		break;
	case (MDOC_ELEM):
		t = "elem";
		break;
	case (MDOC_TEXT):
		t = "text";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	switch (n->type) {
	case (MDOC_TEXT):
		p = n->string;
		break;
	case (MDOC_BODY):
		p = mdoc_macronames[n->tok];
		break;
	case (MDOC_HEAD):
		p = mdoc_macronames[n->tok];
		break;
	case (MDOC_TAIL):
		p = mdoc_macronames[n->tok];
		break;
	case (MDOC_ELEM):
		p = mdoc_macronames[n->tok];
		if (n->args) {
			argv = n->args->argv;
			argc = n->args->argc;
		}
		break;
	case (MDOC_BLOCK):
		p = mdoc_macronames[n->tok];
		if (n->args) {
			argv = n->args->argv;
			argc = n->args->argc;
		}
		break;
	case (MDOC_ROOT):
		p = "root";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	for (i = 0; i < indent; i++)
		(void)printf("    ");
	(void)printf("%s (%s)", p, t);

	for (i = 0; i < (int)argc; i++) {
		(void)printf(" -%s", mdoc_argnames[argv[i].arg]);
		if (argv[i].sz > 0)
			(void)printf(" [");
		for (j = 0; j < (int)argv[i].sz; j++)
			(void)printf(" [%s]", argv[i].value[j]);
		if (argv[i].sz > 0)
			(void)printf(" ]");
	}

	for (i = 0; i < (int)sz; i++)
		(void)printf(" [%s]", params[i]);

	(void)printf(" %d:%d\n", n->line, n->pos);

	if (n->child)
		print_mdoc(n->child, indent + 1);
	if (n->next)
		print_mdoc(n->next, indent);
}


static void
print_man(const struct man_node *n, int indent)
{
	const char	 *p, *t;
	int		  i;

	switch (n->type) {
	case (MAN_ROOT):
		t = "root";
		break;
	case (MAN_ELEM):
		t = "elem";
		break;
	case (MAN_TEXT):
		t = "text";
		break;
	case (MAN_BLOCK):
		t = "block";
		break;
	case (MAN_HEAD):
		t = "block-head";
		break;
	case (MAN_BODY):
		t = "block-body";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	switch (n->type) {
	case (MAN_TEXT):
		p = n->string;
		break;
	case (MAN_ELEM):
		/* FALLTHROUGH */
	case (MAN_BLOCK):
		/* FALLTHROUGH */
	case (MAN_HEAD):
		/* FALLTHROUGH */
	case (MAN_BODY):
		p = man_macronames[n->tok];
		break;
	case (MAN_ROOT):
		p = "root";
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	for (i = 0; i < indent; i++)
		(void)printf("    ");
	(void)printf("%s (%s) %d:%d\n", p, t, n->line, n->pos);

	if (n->child)
		print_man(n->child, indent + 1);
	if (n->next)
		print_man(n->next, indent);
}
