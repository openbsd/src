/*	$Id: mdoc_man.c,v 1.18 2012/07/08 15:00:43 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Ingo Schwarze <schwarze@openbsd.org>
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
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "man.h"
#include "mdoc.h"
#include "main.h"

#define	DECL_ARGS const struct mdoc_meta *m, \
		  const struct mdoc_node *n

struct	manact {
	int		(*cond)(DECL_ARGS); /* DON'T run actions */
	int		(*pre)(DECL_ARGS); /* pre-node action */
	void		(*post)(DECL_ARGS); /* post-node action */
	const char	 *prefix; /* pre-node string constant */
	const char	 *suffix; /* post-node string constant */
};

static	int	  cond_body(DECL_ARGS);
static	int	  cond_head(DECL_ARGS);
static	void	  post_bd(DECL_ARGS);
static	void	  post_bk(DECL_ARGS);
static	void	  post_dl(DECL_ARGS);
static	void	  post_enc(DECL_ARGS);
static	void	  post_fa(DECL_ARGS);
static	void	  post_fn(DECL_ARGS);
static	void	  post_fo(DECL_ARGS);
static	void	  post_in(DECL_ARGS);
static	void	  post_lb(DECL_ARGS);
static	void	  post_nm(DECL_ARGS);
static	void	  post_percent(DECL_ARGS);
static	void	  post_pf(DECL_ARGS);
static	void	  post_sect(DECL_ARGS);
static	void	  post_sp(DECL_ARGS);
static	void	  post_vt(DECL_ARGS);
static	int	  pre_ap(DECL_ARGS);
static	int	  pre_bd(DECL_ARGS);
static	int	  pre_bk(DECL_ARGS);
static	int	  pre_br(DECL_ARGS);
static	int	  pre_bx(DECL_ARGS);
static	int	  pre_dl(DECL_ARGS);
static	int	  pre_enc(DECL_ARGS);
static	int	  pre_fa(DECL_ARGS);
static	int	  pre_fn(DECL_ARGS);
static	int	  pre_fo(DECL_ARGS);
static	int	  pre_in(DECL_ARGS);
static	int	  pre_it(DECL_ARGS);
static	int	  pre_nm(DECL_ARGS);
static	int	  pre_ns(DECL_ARGS);
static	int	  pre_pp(DECL_ARGS);
static	int	  pre_sm(DECL_ARGS);
static	int	  pre_sp(DECL_ARGS);
static	int	  pre_sect(DECL_ARGS);
static	int	  pre_vt(DECL_ARGS);
static	int	  pre_ux(DECL_ARGS);
static	int	  pre_xr(DECL_ARGS);
static	void	  print_word(const char *);
static	void	  print_offs(const char *);
static	void	  print_node(DECL_ARGS);

static	const struct manact manacts[MDOC_MAX + 1] = {
	{ NULL, pre_ap, NULL, NULL, NULL }, /* Ap */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dd */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dt */
	{ NULL, NULL, NULL, NULL, NULL }, /* Os */
	{ NULL, pre_sect, post_sect, ".SH", NULL }, /* Sh */
	{ NULL, pre_sect, post_sect, ".SS", NULL }, /* Ss */
	{ NULL, pre_pp, NULL, NULL, NULL }, /* Pp */
	{ cond_body, pre_dl, post_dl, NULL, NULL }, /* D1 */
	{ cond_body, pre_dl, post_dl, NULL, NULL }, /* Dl */
	{ cond_body, pre_bd, post_bd, NULL, NULL }, /* Bd */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ed */
	{ NULL, NULL, NULL, NULL, NULL }, /* Bl */
	{ NULL, NULL, NULL, NULL, NULL }, /* El */
	{ NULL, pre_it, NULL, NULL, NULL }, /* _It */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Ad */
	{ NULL, NULL, NULL, NULL, NULL }, /* _An */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Ar */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Cd */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Cm */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Dv */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Er */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Ev */
	{ NULL, pre_enc, post_enc, "The \\fB",
	    "\\fP\nutility exits 0 on success, and >0 if an error occurs."
	    }, /* Ex */
	{ NULL, pre_fa, post_fa, NULL, NULL }, /* Fa */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fd */
	{ NULL, pre_enc, post_enc, "\\fB-", "\\fP" }, /* Fl */
	{ NULL, pre_fn, post_fn, NULL, NULL }, /* Fn */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Ft */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Ic */
	{ NULL, pre_in, post_in, NULL, NULL }, /* In */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Li */
	{ cond_head, pre_enc, NULL, "\\- ", NULL }, /* Nd */
	{ NULL, pre_nm, post_nm, NULL, NULL }, /* Nm */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Op */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ot */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Pa */
	{ NULL, pre_enc, post_enc, "The \\fB",
		"\\fP\nfunction returns the value 0 if successful;\n"
		"otherwise the value -1 is returned and the global\n"
		"variable \\fIerrno\\fP is set to indicate the error."
		}, /* Rv */
	{ NULL, NULL, NULL, NULL, NULL }, /* St */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Va */
	{ NULL, pre_vt, post_vt, NULL, NULL }, /* Vt */
	{ NULL, pre_xr, NULL, NULL, NULL }, /* Xr */
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
	{ NULL, NULL, NULL, NULL, NULL }, /* Ac */
	{ cond_body, pre_enc, post_enc, "<", ">" }, /* Ao */
	{ cond_body, pre_enc, post_enc, "<", ">" }, /* Aq */
	{ NULL, NULL, NULL, NULL, NULL }, /* At */
	{ NULL, NULL, NULL, NULL, NULL }, /* Bc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Bf */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bo */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bq */
	{ NULL, pre_ux, NULL, "BSD/OS", NULL }, /* Bsx */
	{ NULL, pre_bx, NULL, NULL, NULL }, /* Bx */
	{ NULL, NULL, NULL, NULL, NULL }, /* Db */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dc */
	{ cond_body, pre_enc, post_enc, "``", "''" }, /* Do */
	{ cond_body, pre_enc, post_enc, "``", "''" }, /* Dq */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ec */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ef */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Em */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Eo */
	{ NULL, pre_ux, NULL, "FreeBSD", NULL }, /* Fx */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Ms */
	{ NULL, NULL, NULL, NULL, NULL }, /* No */
	{ NULL, pre_ns, NULL, NULL, NULL }, /* Ns */
	{ NULL, pre_ux, NULL, "NetBSD", NULL }, /* Nx */
	{ NULL, pre_ux, NULL, "OpenBSD", NULL }, /* Ox */
	{ NULL, NULL, NULL, NULL, NULL }, /* Pc */
	{ NULL, NULL, post_pf, NULL, NULL }, /* Pf */
	{ cond_body, pre_enc, post_enc, "(", ")" }, /* Po */
	{ cond_body, pre_enc, post_enc, "(", ")" }, /* Pq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Qc */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* Ql */
	{ cond_body, pre_enc, post_enc, "\"", "\"" }, /* Qo */
	{ cond_body, pre_enc, post_enc, "\"", "\"" }, /* Qq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Re */
	{ cond_body, pre_pp, NULL, NULL, NULL }, /* Rs */
	{ NULL, NULL, NULL, NULL, NULL }, /* Sc */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* So */
	{ cond_body, pre_enc, post_enc, "`", "'" }, /* Sq */
	{ NULL, pre_sm, NULL, NULL, NULL }, /* Sm */
	{ NULL, pre_enc, post_enc, "\\fI", "\\fP" }, /* Sx */
	{ NULL, pre_enc, post_enc, "\\fB", "\\fP" }, /* Sy */
	{ NULL, pre_enc, post_enc, "\\fR", "\\fP" }, /* Tn */
	{ NULL, pre_ux, NULL, "UNIX", NULL }, /* Ux */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Xc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Xo */
	{ NULL, pre_fo, post_fo, NULL, NULL }, /* Fo */
	{ NULL, NULL, NULL, NULL, NULL }, /* Fc */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Oo */
	{ NULL, NULL, NULL, NULL, NULL }, /* Oc */
	{ NULL, pre_bk, post_bk, NULL, NULL }, /* Bk */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ek */
	{ NULL, pre_ux, NULL, "is currently in beta test.", NULL }, /* Bt */
	{ NULL, NULL, NULL, NULL, NULL }, /* Hf */
	{ NULL, NULL, NULL, NULL, NULL }, /* Fr */
	{ NULL, pre_ux, NULL, "currently under development.", NULL }, /* Ud */
	{ NULL, NULL, post_lb, NULL, NULL }, /* Lb */
	{ NULL, pre_pp, NULL, NULL, NULL }, /* Lp */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Lk */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Mt */
	{ cond_body, pre_enc, post_enc, "{", "}" }, /* Brq */
	{ cond_body, pre_enc, post_enc, "{", "}" }, /* Bro */
	{ NULL, NULL, NULL, NULL, NULL }, /* Brc */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%C */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Es */
	{ NULL, NULL, NULL, NULL, NULL }, /* _En */
	{ NULL, pre_ux, NULL, "DragonFly", NULL }, /* Dx */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%Q */
	{ NULL, pre_br, NULL, NULL, NULL }, /* br */
	{ NULL, pre_sp, post_sp, NULL, NULL }, /* sp */
	{ NULL, NULL, NULL, NULL, NULL }, /* _%U */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Ta */
	{ NULL, NULL, NULL, NULL, NULL }, /* ROOT */
};

static	int		outflags;
#define	MMAN_spc	(1 << 0)
#define	MMAN_nl		(1 << 1)
#define	MMAN_Sm		(1 << 2)
#define	MMAN_Bk		(1 << 3)

static void
print_word(const char *s)
{

	if (MMAN_nl & outflags) {
		/* 
		 * If we need a newline, print it now and start afresh.
		 */
		putchar('\n');
		outflags &= ~(MMAN_nl|MMAN_spc);
	} else if (MMAN_spc & outflags && '\0' != s[0])
		/*
		 * If we need a space, only print it before
		 * (1) a nonzero length word;
		 * (2) a word that is non-punctuation; and
		 * (3) if punctuation, non-terminating puncutation.
		 */
		if (NULL == strchr(".,:;)]?!", s[0]) || '\0' != s[1]) {
			if (MMAN_Bk & outflags) {
				putchar('\\');
				putchar('~');
			} else 
				putchar(' ');
		}

	/*
	 * Reassign needing space if we're not following opening
	 * punctuation.
	 */
	if (MMAN_Sm & outflags &&
	    (('(' != s[0] && '[' != s[0]) || '\0' != s[1]))
		outflags |= MMAN_spc;
	else
		outflags &= ~MMAN_spc;

	for ( ; *s; s++) {
		switch (*s) {
		case (ASCII_NBRSP):
			printf("\\~");
			break;
		case (ASCII_HYPH):
			putchar('-');
			break;
		default:
			putchar((unsigned char)*s);
			break;
		}
	}
}

static void
print_offs(const char *v)
{
	char		  buf[24];
	struct roffsu	  su;
	size_t		  sz;

	if (NULL == v || '\0' == *v || 0 == strcmp(v, "left"))
		sz = 0;
	else if (0 == strcmp(v, "indent"))
		sz = 6;
	else if (0 == strcmp(v, "indent-two"))
		sz = 12;
	else if (a2roffsu(v, &su, SCALE_MAX)) {
		print_word(v);
		return;
	} else
		sz = strlen(v);

	snprintf(buf, sizeof(buf), "%ldn", sz);
	print_word(buf);
}

void
man_man(void *arg, const struct man *man)
{

	/*
	 * Dump the keep buffer.
	 * We're guaranteed by now that this exists (is non-NULL).
	 * Flush stdout afterward, just in case.
	 */
	fputs(mparse_getkeep(man_mparse(man)), stdout);
	fflush(stdout);
}

void
man_mdoc(void *arg, const struct mdoc *mdoc)
{
	const struct mdoc_meta *m;
	const struct mdoc_node *n;

	m = mdoc_meta(mdoc);
	n = mdoc_node(mdoc);

	printf(".TH \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
			m->title, m->msec, m->date, m->os, m->vol);

	outflags = MMAN_nl | MMAN_Sm;
	print_node(m, n);
	putchar('\n');
}

static void
print_node(DECL_ARGS)
{
	const struct mdoc_node	*prev, *sub;
	const struct manact	*act;
	int			 cond, do_sub;
	
	/*
	 * Break the line if we were parsed subsequent the current node.
	 * This makes the page structure be more consistent.
	 */
	prev = n->prev ? n->prev : n->parent;
	if (prev && prev->line < n->line &&
	    MDOC_Fo != prev->tok && MDOC_Ns != prev->tok)
		outflags |= MMAN_nl;

	act = NULL;
	cond = 0;
	do_sub = 1;

	if (MDOC_TEXT == n->type) {
		/*
		 * Make sure that we don't happen to start with a
		 * control character at the start of a line.
		 */
		if (MMAN_nl & outflags && ('.' == *n->string || 
					'\'' == *n->string)) {
			print_word("\\&");
			outflags &= ~MMAN_spc;
		}
		print_word(n->string);
	} else {
		/*
		 * Conditionally run the pre-node action handler for a
		 * node.
		 */
		act = manacts + n->tok;
		cond = NULL == act->cond || (*act->cond)(m, n);
		if (cond && act->pre)
			do_sub = (*act->pre)(m, n);
	}

	/* 
	 * Conditionally run all child nodes.
	 * Note that this iterates over children instead of using
	 * recursion.  This prevents unnecessary depth in the stack.
	 */
	if (do_sub)
		for (sub = n->child; sub; sub = sub->next)
			print_node(m, sub);

	/*
	 * Lastly, conditionally run the post-node handler.
	 */
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

/*
 * Output a font encoding before a node, e.g., \fR.
 * This obviously has no trailing space.
 */
static int
pre_enc(DECL_ARGS)
{
	const char	*prefix;

	prefix = manacts[n->tok].prefix;
	if (NULL == prefix)
		return(1);
	print_word(prefix);
	outflags &= ~MMAN_spc;
	return(1);
}

/*
 * Output a font encoding subsequent a node, e.g., \fP.
 */
static void
post_enc(DECL_ARGS)
{
	const char *suffix;

	suffix = manacts[n->tok].suffix;
	if (NULL == suffix)
		return;
	outflags &= ~MMAN_spc;
	print_word(suffix);
	if (MDOC_Fl == n->tok && 0 == n->nchild)
		outflags &= ~MMAN_spc;
}

/*
 * Used in listings (percent = %A, e.g.).
 * FIXME: this is incomplete. 
 * It doesn't print a nice ", and" for lists.
 */
static void
post_percent(DECL_ARGS)
{

	post_enc(m, n);
	if (n->next)
		print_word(",");
	else {
		print_word(".");
		outflags |= MMAN_nl;
	}
}

/*
 * Print before a section header.
 */
static int
pre_sect(DECL_ARGS)
{

	if (MDOC_HEAD != n->type)
		return(1);
	outflags |= MMAN_nl;
	print_word(manacts[n->tok].prefix);
	print_word("\"");
	outflags &= ~MMAN_spc;
	return(1);
}

/*
 * Print subsequent a section header.
 */
static void
post_sect(DECL_ARGS)
{

	if (MDOC_HEAD != n->type)
		return;
	outflags &= ~MMAN_spc;
	print_word("\"");
	outflags |= MMAN_nl;
}

static int
pre_ap(DECL_ARGS)
{

	outflags &= ~MMAN_spc;
	print_word("'");
	outflags &= ~MMAN_spc;
	return(0);
}

static int
pre_bd(DECL_ARGS)
{

	if (0 == n->norm->Bd.comp) {
		outflags |= MMAN_nl;
		print_word(".sp");
	}
	if (DISP_unfilled == n->norm->Bd.type ||
	    DISP_literal  == n->norm->Bd.type) {
		outflags |= MMAN_nl;
		print_word(".nf");
	}
	outflags |= MMAN_nl;
	print_word(".RS");
	print_offs(n->norm->Bd.offs);
	outflags |= MMAN_nl;
	return(1);
}

static void
post_bd(DECL_ARGS)
{

	outflags |= MMAN_nl;
	print_word(".RE");
	if (DISP_unfilled == n->norm->Bd.type ||
	    DISP_literal  == n->norm->Bd.type) {
		outflags |= MMAN_nl;
		print_word(".fi");
	}
	outflags |= MMAN_nl;
}

static int
pre_bk(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		return(1);
	case (MDOC_BODY):
		outflags |= MMAN_Bk;
		return(1);
	default:
		return(0);
	}
}

static void
post_bk(DECL_ARGS)
{

	if (MDOC_BODY == n->type)
		outflags &= ~MMAN_Bk;
}

static int
pre_br(DECL_ARGS)
{

	outflags |= MMAN_nl;
	print_word(".br");
	outflags |= MMAN_nl;
	return(0);
}

static int
pre_bx(DECL_ARGS)
{

	n = n->child;
	if (n) {
		print_word(n->string);
		outflags &= ~MMAN_spc;
		n = n->next;
	}
	print_word("BSD");
	if (NULL == n)
		return(0);
	outflags &= ~MMAN_spc;
	print_word("-");
	outflags &= ~MMAN_spc;
	print_word(n->string);
	return(0);
}

static int
pre_dl(DECL_ARGS)
{

	outflags |= MMAN_nl;
	print_word(".RS 6n");
	outflags |= MMAN_nl;
	return(1);
}

static void
post_dl(DECL_ARGS)
{

	outflags |= MMAN_nl;
	print_word(".RE");
	outflags |= MMAN_nl;
}

static int
pre_fa(DECL_ARGS)
{

	if (MDOC_Fa == n->tok)
		n = n->child;

	while (NULL != n) {
		print_word("\\fI");
		outflags &= ~MMAN_spc;
		print_node(m, n);
		outflags &= ~MMAN_spc;
		print_word("\\fP");
		if (NULL != (n = n->next))
			print_word(",");
	}
	return(0);
}

static void
post_fa(DECL_ARGS)
{

	if (NULL != n->next && MDOC_Fa == n->next->tok)
		print_word(",");
}

static int
pre_fn(DECL_ARGS)
{

	n = n->child;
	if (NULL == n)
		return(0);

	if (MDOC_SYNPRETTY & n->flags) {
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
	}
	print_word("\\fB");
	outflags &= ~MMAN_spc;
	print_node(m, n);
	outflags &= ~MMAN_spc;
	print_word("\\fP(");
	outflags &= ~MMAN_spc;
	return(pre_fa(m, n->next));
}

static void
post_fn(DECL_ARGS)
{

	print_word(")");
	if (MDOC_SYNPRETTY & n->flags) {
		print_word(";");
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
	}
}

static int
pre_fo(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_HEAD):
		if (MDOC_SYNPRETTY & n->flags) {
			outflags |= MMAN_nl;
			print_word(".br");
			outflags |= MMAN_nl;
		}
		print_word("\\fB");
		outflags &= ~MMAN_spc;
		break;
	case (MDOC_BODY):
		outflags &= ~MMAN_spc;
		print_word("(");
		outflags &= ~MMAN_spc;
		break;
	default:
		break;
	}
	return(1);
}
		
static void
post_fo(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_HEAD):
		outflags &= ~MMAN_spc;
		print_word("\\fP");
		break;
	case (MDOC_BODY):
		post_fn(m, n);
		break;
	default:
		break;
	}
}

static int
pre_in(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
		print_word("\\fB#include <");
	} else
		print_word("<\\fI");
	outflags &= ~MMAN_spc;
	return(1);
}

static void
post_in(DECL_ARGS)
{

	outflags &= ~MMAN_spc;
	if (MDOC_SYNPRETTY & n->flags) {
		print_word(">\\fP");
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
	} else
		print_word("\\fP>");
}

static int
pre_it(DECL_ARGS)
{
	const struct mdoc_node *bln;

	if (MDOC_HEAD == n->type) {
		outflags |= MMAN_nl;
		print_word(".TP");
		bln = n->parent->parent->prev;
		switch (bln->norm->Bl.type) {
		case (LIST_bullet):
			print_word("4n");
			outflags |= MMAN_nl;
			print_word("\\fBo\\fP");
			break;
		default:
			if (bln->norm->Bl.width)
				print_word(bln->norm->Bl.width);
			break;
		}
		outflags |= MMAN_nl;
	}
	return(1);
}

static void
post_lb(DECL_ARGS)
{

	if (SEC_LIBRARY == n->sec) {
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
	}
}

static int
pre_nm(DECL_ARGS)
{

	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return(1);
	if (MDOC_SYNPRETTY & n->flags) {
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
	}
	print_word("\\fB");
	outflags &= ~MMAN_spc;
	if (NULL == n->child)
		print_word(m->name);
	return(1);
}

static void
post_nm(DECL_ARGS)
{

	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return;
	outflags &= ~MMAN_spc;
	print_word("\\fP");
}

static int
pre_ns(DECL_ARGS)
{

	outflags &= ~MMAN_spc;
	return(0);
}

static void
post_pf(DECL_ARGS)
{

	outflags &= ~MMAN_spc;
}

static int
pre_pp(DECL_ARGS)
{

	outflags |= MMAN_nl;
	if (MDOC_It == n->parent->tok)
		print_word(".sp");
	else
		print_word(".PP");
	outflags |= MMAN_nl;
	return(MDOC_Rs == n->tok);
}

static int
pre_sm(DECL_ARGS)
{

	assert(n->child && MDOC_TEXT == n->child->type);
	if (0 == strcmp("on", n->child->string))
		outflags |= MMAN_Sm;
	else
		outflags &= ~MMAN_Sm;
	return(0);
}

static int
pre_sp(DECL_ARGS)
{

	outflags |= MMAN_nl;
	print_word(".sp");
	return(1);
}

static void
post_sp(DECL_ARGS)
{

	outflags |= MMAN_nl;
}

static int
pre_vt(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		switch (n->type) {
		case (MDOC_BLOCK):
			return(1);
		case (MDOC_BODY):
			break;
		default:
			return(0);
		}
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
	}
	print_word("\\fI");
	outflags &= ~MMAN_spc;
	return(1);
}

static void
post_vt(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags && MDOC_BODY != n->type)
		return;

	outflags &= ~MMAN_spc;
	print_word("\\fP");
	if (MDOC_SYNPRETTY & n->flags) {
		outflags |= MMAN_nl;
		print_word(".br");
		outflags |= MMAN_nl;
	}
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
	outflags &= ~MMAN_spc;
	print_word("(");
	print_node(m, n);
	print_word(")");
	return(0);
}

static int
pre_ux(DECL_ARGS)
{

	print_word(manacts[n->tok].prefix);
	if (NULL == n->child)
		return(0);
	outflags &= ~MMAN_spc;
	print_word("\\~");
	outflags &= ~MMAN_spc;
	return(1);
}
