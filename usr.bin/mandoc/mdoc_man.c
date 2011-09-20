/*	$Id: mdoc_man.c,v 1.2 2011/09/20 13:47:59 schwarze Exp $ */
/*
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "mdoc.h"
#include "main.h"

static	int	  need_space = 0;
static	int	  need_nl = 0;

#define	DECL_ARGS const struct mdoc_meta *m, \
		  const struct mdoc_node *n

struct	manact {
	int		(*cond)(DECL_ARGS);
	int		(*pre)(DECL_ARGS);
	void		(*post)(DECL_ARGS);
	const char	 *prefix;
	const char	 *suffix;
};

static	void	  print_word(const char *);
static	void	  print_node(DECL_ARGS);

static	int	  cond_head(DECL_ARGS);
static	int	  cond_body(DECL_ARGS);
static	int	  pre_enc(DECL_ARGS);
static	void	  post_enc(DECL_ARGS);
static	void	  post_percent(DECL_ARGS);

static	int	  pre_dl(DECL_ARGS);
static	void	  post_dl(DECL_ARGS);
static	int	  pre_it(DECL_ARGS);
static	int	  pre_nm(DECL_ARGS);
static	void	  post_nm(DECL_ARGS);
static	int	  pre_ns(DECL_ARGS);
static	int	  pre_pp(DECL_ARGS);
static	int	  pre_sh(DECL_ARGS);
static	void	  post_sh(DECL_ARGS);
static	int	  pre_xr(DECL_ARGS);


static	const struct manact manacts[MDOC_MAX] = {
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ap */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Dd */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Dt */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Os */
	{ NULL, pre_sh, post_sh, NULL, NULL }, /* Sh */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ss */
	{ NULL, pre_pp, NULL, NULL, NULL }, /* Pp */
	{ NULL, NULL, NULL, NULL, NULL }, /* _D1 */
	{ cond_body, pre_dl, post_dl, NULL, NULL }, /* Dl */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bd */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ed */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bl */
	{ NULL, NULL, NULL, NULL, NULL }, /* _El */
	{ NULL, pre_it, NULL, NULL, NULL }, /* _It */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ad */
	{ NULL, NULL, NULL, NULL, NULL }, /* _An */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Ar */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Cd */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Cm */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Dv */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Er */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ev */
	{ NULL, pre_enc, post_enc, "The \\fB",
	    "\\fP\nutility exits 0 on success, and >0 if an error occurs."
	    }, /* Ex */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fa */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fd */
	{ NULL, pre_enc, post_enc, "\\fB-", "\\fP" }, /* Fl */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fn */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ft */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ic */
	{ NULL, NULL, NULL, NULL, NULL }, /* _In */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Li */
	{ cond_head, pre_enc, NULL, "\\- ", NULL }, /* Nd */
	{ NULL, pre_nm, post_nm, NULL, NULL }, /* Nm */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Op */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ot */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* _Pa */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Rv */
	{ NULL, NULL, NULL, NULL, NULL }, /* _St */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Va */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Vt */
	{ NULL, pre_xr, NULL, NULL, NULL }, /* _Xr */
	{ NULL, NULL, post_percent, NULL, NULL }, /* _%A */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%B */
	{ NULL, NULL, post_percent, NULL, NULL }, /* _%D */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%I */
	{ NULL, pre_enc, post_percent, "\\fI", "\\fP" }, /* %J */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%N */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%O */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%P */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%R */
	{ NULL, pre_enc, post_percent, "\"", "\"" }, /* %T */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%V */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ac */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ao */
	{ cond_body, pre_enc, post_enc, "<", ">" }, /* Aq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _At */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bf */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bsx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Db */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Dc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Do */
	{ cond_body, pre_enc, post_enc, "``", "''" }, /* Dq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ec */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ef */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* _Em */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Eo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ms */
	{ NULL, NULL, NULL, NULL, NULL }, /* _No */
	{ NULL, pre_ns, NULL, NULL, NULL }, /* Ns */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Nx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ox */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Pc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Pf */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Po */
	{ cond_body, pre_enc, post_enc, "(", ")" }, /* _Pq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Qc */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* Ql */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Qo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Qq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Re */
	{ cond_body, pre_pp, NULL, NULL, NULL }, /* Rs */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Sc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _So */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* Sq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Sm */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Sx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Sy */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Tn */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ux */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Xc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Xo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Oo */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Oc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bk */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ek */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bt */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Hf */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fr */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ud */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Lb */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Lp */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Lk */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Mt */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Brq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bro */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Brc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%C */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Es */
	{ NULL, NULL, NULL, NULL, NULL }, /* _En */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Dx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%Q */
	{ NULL, NULL, NULL, NULL, NULL }, /* _br */
	{ NULL, NULL, NULL, NULL, NULL }, /* _sp */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%U */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ta */
};


static void
print_word(const char *s)
{
	if (need_nl) {
		putchar('\n');
		need_space = 0;
		need_nl = 0;
	} else if (need_space &&
	    (NULL == strchr(".,:;)]?!", s[0]) || '\0' != s[1]))
		putchar(' ');
	need_space = ('(' != s[0] && '[' != s[0]) || '\0' != s[1];
	for ( ; *s; s++) {
		switch (*s) {
		case (ASCII_NBRSP):
			printf("\\~");
			break;
		case (ASCII_HYPH):
			putchar('-');
			break;
		default:
			putchar(*s);
			break;
		}
	}
}

void
man_mdoc(void *arg, const struct mdoc *mdoc)
{
	const struct mdoc_meta *m;
	const struct mdoc_node *n;

	m = mdoc_meta(mdoc);
	n = mdoc_node(mdoc);

	printf(".TH \"%s\" \"%s\" \"%s\"", m->title, m->msec, m->date);
	need_nl = 1;
	need_space = 0;

	print_node(m, n);
}

static void
print_node(DECL_ARGS)
{
	const struct mdoc_node	*prev, *sub;
	const struct manact	*act = NULL;
	int			 cond, do_sub;

	prev = n->prev ? n->prev : n->parent;
	if (prev && prev->line < n->line)
		need_nl = 1;

	cond = 0;
	do_sub = 1;
	if (MDOC_TEXT == n->type) {
		print_word(n->string);
	} else {
		act = manacts + n->tok;
		cond = NULL == act->cond || (*act->cond)(m, n);
		if (cond && act->pre)
			do_sub = (*act->pre)(m, n);
	}

	if (do_sub)
		for (sub = n->child; sub; sub = sub->next)
			print_node(m, sub);

	if (cond && act->post)
		(*act->post)(m, n);
}

static int
cond_head(DECL_ARGS)
{
	return(MDOC_HEAD == n->type);
}

static int
cond_body(DECL_ARGS)
{
	return(MDOC_BODY == n->type);
}

static int
pre_enc(DECL_ARGS)
{
	const char *prefix;

	prefix = manacts[n->tok].prefix;
	if (NULL == prefix)
		return(1);
	print_word(prefix);
	need_space = 0;
	return(1);
}

static void
post_enc(DECL_ARGS)
{
	const char *suffix;

	suffix = manacts[n->tok].suffix;
	if (NULL == suffix)
		return;
	need_space = 0;
	print_word(suffix);
}

static void
post_percent(DECL_ARGS)
{

	post_enc(m, n);
	if (n->next)
		print_word(",");
	else {
		print_word(".");
		need_nl = 1;
	}
}

static int
pre_dl(DECL_ARGS)
{

	need_nl = 1;
	print_word(".RS 6n");
	need_nl = 1;
	return(1);
}

static void
post_dl(DECL_ARGS)
{

	need_nl = 1;
	print_word(".RE");
	need_nl = 1;
}

static int
pre_it(DECL_ARGS)
{
	const struct mdoc_node *bln;

	if (MDOC_HEAD == n->type) {
		need_nl = 1;
		print_word(".TP");
		bln = n->parent->parent->prev;
		print_word(bln->norm->Bl.width);
		need_nl = 1;
	}
	return(1);
}

static int
pre_nm(DECL_ARGS)
{

	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return(1);
	print_word("\\fB");
	need_space = 0;
	if (NULL == n->child)
		print_word(m->name);
	return(1);
}

static void
post_nm(DECL_ARGS)
{

	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return;
	need_space = 0;
	print_word("\\fP");
}

static int
pre_ns(DECL_ARGS)
{

	need_space = 0;
	return(0);
}

static int
pre_pp(DECL_ARGS)
{

	need_nl = 1;
	if (MDOC_It == n->parent->tok)
		print_word(".sp");
	else
		print_word(".PP");
	need_nl = 1;
	return(1);
}

static int
pre_sh(DECL_ARGS)
{

	if (MDOC_HEAD != n->type)
		return(1);
	need_nl = 1;
	print_word(".SH \"");
	need_space = 0;
	return(1);
}

static void
post_sh(DECL_ARGS)
{

	if (MDOC_HEAD != n->type)
		return;
	need_space = 0;
	print_word("\"");
	need_nl = 1;
}

static int
pre_xr(DECL_ARGS)
{

	n = n->child;
	if (NULL == n)
		return(0);
	print_node(m, n);
	n = n->next;
	if (NULL == n)
		return(0);
	need_space = 0;
	print_word("(");
	print_node(m, n);
	print_word(")");
	return(0);
}
