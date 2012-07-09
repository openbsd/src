/*	$Id: mdoc_man.c,v 1.26 2012/07/09 22:36:04 schwarze Exp $ */
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
static  void	  font_push(char);
static	void	  font_pop(void);
static	void	  post_bd(DECL_ARGS);
static	void	  post_bf(DECL_ARGS);
static	void	  post_bk(DECL_ARGS);
static	void	  post_dl(DECL_ARGS);
static	void	  post_enc(DECL_ARGS);
static	void	  post_eo(DECL_ARGS);
static	void	  post_fa(DECL_ARGS);
static	void	  post_fl(DECL_ARGS);
static	void	  post_fn(DECL_ARGS);
static	void	  post_fo(DECL_ARGS);
static	void	  post_font(DECL_ARGS);
static	void	  post_in(DECL_ARGS);
static	void	  post_lb(DECL_ARGS);
static	void	  post_nm(DECL_ARGS);
static	void	  post_percent(DECL_ARGS);
static	void	  post_pf(DECL_ARGS);
static	void	  post_sect(DECL_ARGS);
static	void	  post_sp(DECL_ARGS);
static	void	  post_vt(DECL_ARGS);
static	int	  pre_an(DECL_ARGS);
static	int	  pre_ap(DECL_ARGS);
static	int	  pre_bd(DECL_ARGS);
static	int	  pre_bf(DECL_ARGS);
static	int	  pre_bk(DECL_ARGS);
static	int	  pre_br(DECL_ARGS);
static	int	  pre_bx(DECL_ARGS);
static	int	  pre_dl(DECL_ARGS);
static	int	  pre_enc(DECL_ARGS);
static	int	  pre_em(DECL_ARGS);
static	int	  pre_fa(DECL_ARGS);
static	int	  pre_fl(DECL_ARGS);
static	int	  pre_fn(DECL_ARGS);
static	int	  pre_fo(DECL_ARGS);
static	int	  pre_ft(DECL_ARGS);
static	int	  pre_in(DECL_ARGS);
static	int	  pre_it(DECL_ARGS);
static	int	  pre_lk(DECL_ARGS);
static	int	  pre_li(DECL_ARGS);
static	int	  pre_nm(DECL_ARGS);
static	int	  pre_no(DECL_ARGS);
static	int	  pre_ns(DECL_ARGS);
static	int	  pre_pp(DECL_ARGS);
static	int	  pre_sm(DECL_ARGS);
static	int	  pre_sp(DECL_ARGS);
static	int	  pre_sect(DECL_ARGS);
static	int	  pre_sy(DECL_ARGS);
static	void	  pre_syn(const struct mdoc_node *);
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
	{ NULL, pre_em, post_font, NULL, NULL }, /* Ad */
	{ NULL, pre_an, NULL, NULL, NULL }, /* An */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Ar */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Cd */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Cm */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Dv */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Er */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Ev */
	{ NULL, pre_enc, post_enc, "The \\fB",
	    "\\fP\nutility exits 0 on success, and >0 if an error occurs."
	    }, /* Ex */
	{ NULL, pre_fa, post_fa, NULL, NULL }, /* Fa */
	{ NULL, NULL, NULL, NULL, NULL }, /* _Fd */
	{ NULL, pre_fl, post_fl, NULL, NULL }, /* Fl */
	{ NULL, pre_fn, post_fn, NULL, NULL }, /* Fn */
	{ NULL, pre_ft, post_font, NULL, NULL }, /* Ft */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Ic */
	{ NULL, pre_in, post_in, NULL, NULL }, /* In */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Li */
	{ cond_head, pre_enc, NULL, "\\- ", NULL }, /* Nd */
	{ NULL, pre_nm, post_nm, NULL, NULL }, /* Nm */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Op */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ot */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Pa */
	{ NULL, pre_enc, post_enc, "The \\fB",
		"\\fP\nfunction returns the value 0 if successful;\n"
		"otherwise the value -1 is returned and the global\n"
		"variable \\fIerrno\\fP is set to indicate the error."
		}, /* Rv */
	{ NULL, NULL, NULL, NULL, NULL }, /* St */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Va */
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
	{ NULL, pre_bf, post_bf, NULL, NULL }, /* Bf */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bo */
	{ cond_body, pre_enc, post_enc, "[", "]" }, /* Bq */
	{ NULL, pre_ux, NULL, "BSD/OS", NULL }, /* Bsx */
	{ NULL, pre_bx, NULL, NULL, NULL }, /* Bx */
	{ NULL, NULL, NULL, NULL, NULL }, /* Db */
	{ NULL, NULL, NULL, NULL, NULL }, /* Dc */
	{ cond_body, pre_enc, post_enc, "``", "''" }, /* Do */
	{ cond_body, pre_enc, post_enc, "``", "''" }, /* Dq */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ec */
	{ NULL, NULL, NULL, NULL, NULL }, /* Ef */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Em */
	{ NULL, NULL, post_eo, NULL, NULL }, /* Eo */
	{ NULL, pre_ux, NULL, "FreeBSD", NULL }, /* Fx */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Ms */
	{ NULL, pre_no, NULL, NULL, NULL }, /* No */
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
	{ NULL, pre_em, post_font, NULL, NULL }, /* Sx */
	{ NULL, pre_sy, post_font, NULL, NULL }, /* Sy */
	{ NULL, pre_li, post_font, NULL, NULL }, /* Tn */
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
	{ NULL, pre_lk, NULL, NULL, NULL }, /* Lk */
	{ NULL, pre_em, post_font, NULL, NULL }, /* Mt */
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
#define	MMAN_spc_force	(1 << 1)
#define	MMAN_nl		(1 << 2)
#define	MMAN_br		(1 << 3)
#define	MMAN_sp		(1 << 4)
#define	MMAN_Sm		(1 << 5)
#define	MMAN_Bk		(1 << 6)
#define	MMAN_An_split	(1 << 7)
#define	MMAN_An_nosplit	(1 << 8)

static	struct {
	char	*head;
	char	*tail;
	size_t	 size;
}	fontqueue;

static void
font_push(char newfont)
{

	if (fontqueue.head + fontqueue.size <= ++fontqueue.tail) {
		fontqueue.size += 8;
		fontqueue.head = mandoc_realloc(fontqueue.head,
				fontqueue.size);
	}
	*fontqueue.tail = newfont;
	print_word("\\f");
	putchar(newfont);
	outflags &= ~MMAN_spc;
}

static void
font_pop(void)
{

	if (fontqueue.tail > fontqueue.head)
		fontqueue.tail--;
	outflags &= ~MMAN_spc;
	print_word("\\f");
	putchar(*fontqueue.tail);
}

static void
print_word(const char *s)
{

	if ((MMAN_sp | MMAN_br | MMAN_nl) & outflags) {
		/* 
		 * If we need a newline, print it now and start afresh.
		 */
		if (MMAN_sp & outflags)
			printf("\n.sp\n");
		else if (MMAN_br & outflags)
			printf("\n.br\n");
		else if (MMAN_nl & outflags)
			putchar('\n');
		outflags &= ~(MMAN_sp|MMAN_br|MMAN_nl|MMAN_spc);
	} else if (MMAN_spc & outflags && '\0' != s[0])
		/*
		 * If we need a space, only print it if
		 * (1) it is forced by `No' or
		 * (2) what follows is not terminating punctuation or
		 * (3) what follows is longer than one character.
		 */
		if (MMAN_spc_force & outflags ||
		    NULL == strchr(".,:;)]?!", s[0]) || '\0' != s[1]) {
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
	outflags &= ~MMAN_spc_force;

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
	if (0 == fontqueue.size) {
		fontqueue.size = 8;
		fontqueue.head = fontqueue.tail = mandoc_malloc(8);
		*fontqueue.tail = 'R';
	}
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
	if (MMAN_spc & outflags && prev && prev->line < n->line)
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

static void
post_enc(DECL_ARGS)
{
	const char *suffix;

	suffix = manacts[n->tok].suffix;
	if (NULL == suffix)
		return;
	outflags &= ~MMAN_spc;
	print_word(suffix);
}

static void
post_font(DECL_ARGS)
{

	font_pop();
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
	if (MDOC_Sh == n->tok && SEC_AUTHORS == n->sec)
		outflags &= ~(MMAN_An_split | MMAN_An_nosplit);
}

/* See mdoc_term.c, synopsis_pre() for comments. */
static void
pre_syn(const struct mdoc_node *n)
{

	if (NULL == n->prev || ! (MDOC_SYNPRETTY & n->flags))
		return;

	if (n->prev->tok == n->tok &&
			MDOC_Ft != n->tok &&
			MDOC_Fo != n->tok &&
			MDOC_Fn != n->tok) {
		outflags |= MMAN_br;
		return;
	}

	switch (n->prev->tok) {
	case (MDOC_Fd):
		/* FALLTHROUGH */
	case (MDOC_Fn):
		/* FALLTHROUGH */
	case (MDOC_Fo):
		/* FALLTHROUGH */
	case (MDOC_In):
		/* FALLTHROUGH */
	case (MDOC_Vt):
		outflags |= MMAN_sp;
		break;
	case (MDOC_Ft):
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
			outflags |= MMAN_sp;
			break;
		}
		/* FALLTHROUGH */
	default:
		outflags |= MMAN_br;
		break;
	}
}

static int
pre_an(DECL_ARGS)
{

	switch (n->norm->An.auth) {
	case (AUTH_split):
		outflags &= ~MMAN_An_nosplit;
		outflags |= MMAN_An_split;
		return(0);
	case (AUTH_nosplit):
		outflags &= ~MMAN_An_split;
		outflags |= MMAN_An_nosplit;
		return(0);
	default:
		if (MMAN_An_split & outflags)
			outflags |= MMAN_br;
		else if (SEC_AUTHORS == n->sec &&
		    ! (MMAN_An_nosplit & outflags))
			outflags |= MMAN_An_split;
		return(1);
	}
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

	if (0 == n->norm->Bd.comp)
		outflags |= MMAN_sp;
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
pre_bf(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		return(1);
	case (MDOC_BODY):
		break;
	default:
		return(0);
	}
	switch (n->norm->Bf.font) {
	case (FONT_Em):
		font_push('I');
		break;
	case (FONT_Sy):
		font_push('B');
		break;
	default:
		font_push('R');
		break;
	}
	return(1);
}

static void
post_bf(DECL_ARGS)
{

	if (MDOC_BODY == n->type)
		font_pop();
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

	outflags |= MMAN_br;
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
pre_em(DECL_ARGS)
{

	font_push('I');
	return(1);
}

static void
post_eo(DECL_ARGS)
{

	if (MDOC_HEAD == n->type || MDOC_BODY == n->type)
		outflags &= ~MMAN_spc;
}

static int
pre_fa(DECL_ARGS)
{

	if (MDOC_Fa == n->tok)
		n = n->child;

	while (NULL != n) {
		font_push('I');
		print_node(m, n);
		font_pop();
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
pre_fl(DECL_ARGS)
{

	font_push('B');
	print_word("-");
	outflags &= ~MMAN_spc;
	return(1);
}

static void
post_fl(DECL_ARGS)
{

	font_pop();
	if (0 == n->nchild && NULL != n->next &&
			n->next->line == n->line)
		outflags &= ~MMAN_spc;
}

static int
pre_fn(DECL_ARGS)
{

	pre_syn(n);

	n = n->child;
	if (NULL == n)
		return(0);

	font_push('B');
	print_node(m, n);
	font_pop();
	outflags &= ~MMAN_spc;
	print_word("(");
	outflags &= ~MMAN_spc;
	return(pre_fa(m, n->next));
}

static void
post_fn(DECL_ARGS)
{

	print_word(")");
	if (MDOC_SYNPRETTY & n->flags) {
		print_word(";");
		outflags |= MMAN_br;
	}
}

static int
pre_fo(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		pre_syn(n);
		break;
	case (MDOC_HEAD):
		font_push('B');
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
		font_pop();
		break;
	case (MDOC_BODY):
		post_fn(m, n);
		break;
	default:
		break;
	}
}

static int
pre_ft(DECL_ARGS)
{

	pre_syn(n);
	font_push('I');
	return(1);
}

static int
pre_in(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		pre_syn(n);
		font_push('B');
		print_word("#include <");
		outflags &= ~MMAN_spc;
	} else {
		print_word("<");
		outflags &= ~MMAN_spc;
		font_push('I');
	}
	return(1);
}

static void
post_in(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		outflags &= ~MMAN_spc;
		print_word(">");
		font_pop();
		outflags |= MMAN_br;
	} else {
		font_pop();
		outflags &= ~MMAN_spc;
		print_word(">");
	}
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

	if (SEC_LIBRARY == n->sec)
		outflags |= MMAN_br;
}

static int
pre_lk(DECL_ARGS)
{
	const struct mdoc_node *link, *descr;

	if (NULL == (link = n->child))
		return(0);

	if (NULL != (descr = link->next)) {
		font_push('I');
		while (NULL != descr) {
			print_word(descr->string);
			descr = descr->next;
		}
		print_word(":");
		font_pop();
	}

	font_push('B');
	print_word(link->string);
	font_pop();
	return(0);
}

static int
pre_li(DECL_ARGS)
{

	font_push('R');
	return(1);
}

static int
pre_nm(DECL_ARGS)
{

	if (MDOC_BLOCK == n->type)
		pre_syn(n);
	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return(1);
	if (NULL == n->child && NULL == m->name)
		return(0);
	font_push('B');
	if (NULL == n->child)
		print_word(m->name);
	return(1);
}

static void
post_nm(DECL_ARGS)
{

	if (MDOC_ELEM != n->type && MDOC_HEAD != n->type)
		return;
	font_pop();
}

static int
pre_no(DECL_ARGS)
{

	outflags |= MMAN_spc_force;
	return(1);
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
		outflags |= MMAN_Sm | MMAN_spc;
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
pre_sy(DECL_ARGS)
{

	font_push('B');
	return(1);
}

static int
pre_vt(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags) {
		switch (n->type) {
		case (MDOC_BLOCK):
			pre_syn(n);
			return(1);
		case (MDOC_BODY):
			break;
		default:
			return(0);
		}
	}
	font_push('I');
	return(1);
}

static void
post_vt(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags && MDOC_BODY != n->type)
		return;
	font_pop();
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
