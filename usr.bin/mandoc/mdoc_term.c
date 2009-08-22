/*	$Id: mdoc_term.c,v 1.51 2009/08/22 18:10:02 schwarze Exp $ */
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
#include "mdoc.h"

#define	INDENT		  5
#define	HALFINDENT	  3

/* FIXME: macro arguments can be escaped. */

#define	TTYPE_PROG	  0
#define	TTYPE_CMD_FLAG	  1
#define	TTYPE_CMD_ARG	  2
#define	TTYPE_SECTION	  3
#define	TTYPE_FUNC_DECL	  4
#define	TTYPE_VAR_DECL	  5
#define	TTYPE_FUNC_TYPE	  6
#define	TTYPE_FUNC_NAME	  7
#define	TTYPE_FUNC_ARG	  8
#define	TTYPE_LINK	  9
#define	TTYPE_SSECTION	  10
#define	TTYPE_FILE	  11
#define	TTYPE_EMPH	  12
#define	TTYPE_CONFIG	  13
#define	TTYPE_CMD	  14
#define	TTYPE_INCLUDE	  15
#define	TTYPE_SYMB	  16
#define	TTYPE_SYMBOL	  17
#define	TTYPE_DIAG	  18
#define	TTYPE_LINK_ANCHOR 19
#define	TTYPE_LINK_TEXT	  20
#define	TTYPE_REF_JOURNAL 21
#define	TTYPE_LIST	  22
#define	TTYPE_NMAX	  23

const	int ttypes[TTYPE_NMAX] = {
	TERMP_BOLD, 		/* TTYPE_PROG */
	TERMP_BOLD,		/* TTYPE_CMD_FLAG */
	TERMP_UNDER, 		/* TTYPE_CMD_ARG */
	TERMP_BOLD, 		/* TTYPE_SECTION */
	TERMP_BOLD,		/* TTYPE_FUNC_DECL */
	TERMP_UNDER,		/* TTYPE_VAR_DECL */
	TERMP_UNDER,		/* TTYPE_FUNC_TYPE */
	TERMP_BOLD, 		/* TTYPE_FUNC_NAME */
	TERMP_UNDER, 		/* TTYPE_FUNC_ARG */
	TERMP_UNDER, 		/* TTYPE_LINK */
	TERMP_BOLD,	 	/* TTYPE_SSECTION */
	TERMP_UNDER,		/* TTYPE_FILE */
	TERMP_UNDER, 		/* TTYPE_EMPH */
	TERMP_BOLD,	 	/* TTYPE_CONFIG */
	TERMP_BOLD,	 	/* TTYPE_CMD */
	TERMP_BOLD,	 	/* TTYPE_INCLUDE */
	TERMP_BOLD,	 	/* TTYPE_SYMB */
	TERMP_BOLD,	 	/* TTYPE_SYMBOL */
	TERMP_BOLD,	 	/* TTYPE_DIAG */
	TERMP_UNDER, 		/* TTYPE_LINK_ANCHOR */
	TERMP_BOLD,	 	/* TTYPE_LINK_TEXT */
	TERMP_UNDER,	 	/* TTYPE_REF_JOURNAL */
	TERMP_BOLD		/* TTYPE_LIST */
};

struct	termpair {
	struct termpair	 *ppair;
	int	  	  flag;	
	int		  count;
};

#define	DECL_ARGS struct termp *p, \
		  struct termpair *pair, \
	  	  const struct mdoc_meta *meta, \
		  const struct mdoc_node *node

struct	termact {
	int	(*pre)(DECL_ARGS);
	void	(*post)(DECL_ARGS);
};

static	void	  termp____post(DECL_ARGS);
static	void	  termp__t_post(DECL_ARGS);
static	void	  termp_an_post(DECL_ARGS);
static	void	  termp_aq_post(DECL_ARGS);
static	void	  termp_bd_post(DECL_ARGS);
static	void	  termp_bl_post(DECL_ARGS);
static	void	  termp_bq_post(DECL_ARGS);
static	void	  termp_brq_post(DECL_ARGS);
static	void	  termp_bx_post(DECL_ARGS);
static	void	  termp_d1_post(DECL_ARGS);
static	void	  termp_dq_post(DECL_ARGS);
static	void	  termp_fd_post(DECL_ARGS);
static	void	  termp_fn_post(DECL_ARGS);
static	void	  termp_fo_post(DECL_ARGS);
static	void	  termp_ft_post(DECL_ARGS);
static	void	  termp_in_post(DECL_ARGS);
static	void	  termp_it_post(DECL_ARGS);
static	void	  termp_lb_post(DECL_ARGS);
static	void	  termp_op_post(DECL_ARGS);
static	void	  termp_pf_post(DECL_ARGS);
static	void	  termp_pq_post(DECL_ARGS);
static	void	  termp_qq_post(DECL_ARGS);
static	void	  termp_sh_post(DECL_ARGS);
static	void	  termp_sq_post(DECL_ARGS);
static	void	  termp_ss_post(DECL_ARGS);
static	void	  termp_vt_post(DECL_ARGS);

static	int	  termp__j_pre(DECL_ARGS);
static	int	  termp__t_pre(DECL_ARGS);
static	int	  termp_an_pre(DECL_ARGS);
static	int	  termp_ap_pre(DECL_ARGS);
static	int	  termp_aq_pre(DECL_ARGS);
static	int	  termp_ar_pre(DECL_ARGS);
static	int	  termp_bd_pre(DECL_ARGS);
static	int	  termp_bf_pre(DECL_ARGS);
static	int	  termp_bq_pre(DECL_ARGS);
static	int	  termp_br_pre(DECL_ARGS);
static	int	  termp_brq_pre(DECL_ARGS);
static	int	  termp_bt_pre(DECL_ARGS);
static	int	  termp_cd_pre(DECL_ARGS);
static	int	  termp_cm_pre(DECL_ARGS);
static	int	  termp_d1_pre(DECL_ARGS);
static	int	  termp_dq_pre(DECL_ARGS);
static	int	  termp_em_pre(DECL_ARGS);
static	int	  termp_ex_pre(DECL_ARGS);
static	int	  termp_fa_pre(DECL_ARGS);
static	int	  termp_fd_pre(DECL_ARGS);
static	int	  termp_fl_pre(DECL_ARGS);
static	int	  termp_fn_pre(DECL_ARGS);
static	int	  termp_fo_pre(DECL_ARGS);
static	int	  termp_ft_pre(DECL_ARGS);
static	int	  termp_ic_pre(DECL_ARGS);
static	int	  termp_in_pre(DECL_ARGS);
static	int	  termp_it_pre(DECL_ARGS);
static	int	  termp_lk_pre(DECL_ARGS);
static	int	  termp_ms_pre(DECL_ARGS);
static	int	  termp_mt_pre(DECL_ARGS);
static	int	  termp_nd_pre(DECL_ARGS);
static	int	  termp_nm_pre(DECL_ARGS);
static	int	  termp_ns_pre(DECL_ARGS);
static	int	  termp_op_pre(DECL_ARGS);
static	int	  termp_pa_pre(DECL_ARGS);
static	int	  termp_pf_pre(DECL_ARGS);
static	int	  termp_pp_pre(DECL_ARGS);
static	int	  termp_pq_pre(DECL_ARGS);
static	int	  termp_qq_pre(DECL_ARGS);
static	int	  termp_rs_pre(DECL_ARGS);
static	int	  termp_rv_pre(DECL_ARGS);
static	int	  termp_sh_pre(DECL_ARGS);
static	int	  termp_sm_pre(DECL_ARGS);
static	int	  termp_sp_pre(DECL_ARGS);
static	int	  termp_sq_pre(DECL_ARGS);
static	int	  termp_ss_pre(DECL_ARGS);
static	int	  termp_sx_pre(DECL_ARGS);
static	int	  termp_sy_pre(DECL_ARGS);
static	int	  termp_ud_pre(DECL_ARGS);
static	int	  termp_va_pre(DECL_ARGS);
static	int	  termp_vt_pre(DECL_ARGS);
static	int	  termp_xr_pre(DECL_ARGS);
static	int	  termp_xx_pre(DECL_ARGS);

static const struct termact termacts[MDOC_MAX] = {
	{ termp_ap_pre, NULL }, /* Ap */
	{ NULL, NULL }, /* Dd */
	{ NULL, NULL }, /* Dt */
	{ NULL, NULL }, /* Os */
	{ termp_sh_pre, termp_sh_post }, /* Sh */
	{ termp_ss_pre, termp_ss_post }, /* Ss */ 
	{ termp_pp_pre, NULL }, /* Pp */ 
	{ termp_d1_pre, termp_d1_post }, /* D1 */
	{ termp_d1_pre, termp_d1_post }, /* Dl */
	{ termp_bd_pre, termp_bd_post }, /* Bd */
	{ NULL, NULL }, /* Ed */
	{ NULL, termp_bl_post }, /* Bl */
	{ NULL, NULL }, /* El */
	{ termp_it_pre, termp_it_post }, /* It */
	{ NULL, NULL }, /* Ad */ 
	{ termp_an_pre, termp_an_post }, /* An */
	{ termp_ar_pre, NULL }, /* Ar */
	{ termp_cd_pre, NULL }, /* Cd */
	{ termp_cm_pre, NULL }, /* Cm */
	{ NULL, NULL }, /* Dv */ 
	{ NULL, NULL }, /* Er */ 
	{ NULL, NULL }, /* Ev */ 
	{ termp_ex_pre, NULL }, /* Ex */
	{ termp_fa_pre, NULL }, /* Fa */ 
	{ termp_fd_pre, termp_fd_post }, /* Fd */ 
	{ termp_fl_pre, NULL }, /* Fl */
	{ termp_fn_pre, termp_fn_post }, /* Fn */ 
	{ termp_ft_pre, termp_ft_post }, /* Ft */ 
	{ termp_ic_pre, NULL }, /* Ic */ 
	{ termp_in_pre, termp_in_post }, /* In */ 
	{ NULL, NULL }, /* Li */
	{ termp_nd_pre, NULL }, /* Nd */ 
	{ termp_nm_pre, NULL }, /* Nm */ 
	{ termp_op_pre, termp_op_post }, /* Op */
	{ NULL, NULL }, /* Ot */
	{ termp_pa_pre, NULL }, /* Pa */
	{ termp_rv_pre, NULL }, /* Rv */
	{ NULL, NULL }, /* St */ 
	{ termp_va_pre, NULL }, /* Va */
	{ termp_vt_pre, termp_vt_post }, /* Vt */ 
	{ termp_xr_pre, NULL }, /* Xr */
	{ NULL, termp____post }, /* %A */
	{ NULL, termp____post }, /* %B */
	{ NULL, termp____post }, /* %D */
	{ NULL, termp____post }, /* %I */
	{ termp__j_pre, termp____post }, /* %J */
	{ NULL, termp____post }, /* %N */
	{ NULL, termp____post }, /* %O */
	{ NULL, termp____post }, /* %P */
	{ NULL, termp____post }, /* %R */
	{ termp__t_pre, termp__t_post }, /* %T */
	{ NULL, termp____post }, /* %V */
	{ NULL, NULL }, /* Ac */
	{ termp_aq_pre, termp_aq_post }, /* Ao */
	{ termp_aq_pre, termp_aq_post }, /* Aq */
	{ NULL, NULL }, /* At */
	{ NULL, NULL }, /* Bc */
	{ termp_bf_pre, NULL }, /* Bf */ 
	{ termp_bq_pre, termp_bq_post }, /* Bo */
	{ termp_bq_pre, termp_bq_post }, /* Bq */
	{ termp_xx_pre, NULL }, /* Bsx */
	{ NULL, termp_bx_post }, /* Bx */
	{ NULL, NULL }, /* Db */
	{ NULL, NULL }, /* Dc */
	{ termp_dq_pre, termp_dq_post }, /* Do */
	{ termp_dq_pre, termp_dq_post }, /* Dq */
	{ NULL, NULL }, /* Ec */
	{ NULL, NULL }, /* Ef */
	{ termp_em_pre, NULL }, /* Em */ 
	{ NULL, NULL }, /* Eo */
	{ termp_xx_pre, NULL }, /* Fx */
	{ termp_ms_pre, NULL }, /* Ms */
	{ NULL, NULL }, /* No */
	{ termp_ns_pre, NULL }, /* Ns */
	{ termp_xx_pre, NULL }, /* Nx */
	{ termp_xx_pre, NULL }, /* Ox */
	{ NULL, NULL }, /* Pc */
	{ termp_pf_pre, termp_pf_post }, /* Pf */
	{ termp_pq_pre, termp_pq_post }, /* Po */
	{ termp_pq_pre, termp_pq_post }, /* Pq */
	{ NULL, NULL }, /* Qc */
	{ termp_sq_pre, termp_sq_post }, /* Ql */
	{ termp_qq_pre, termp_qq_post }, /* Qo */
	{ termp_qq_pre, termp_qq_post }, /* Qq */
	{ NULL, NULL }, /* Re */
	{ termp_rs_pre, NULL }, /* Rs */
	{ NULL, NULL }, /* Sc */
	{ termp_sq_pre, termp_sq_post }, /* So */
	{ termp_sq_pre, termp_sq_post }, /* Sq */
	{ termp_sm_pre, NULL }, /* Sm */
	{ termp_sx_pre, NULL }, /* Sx */
	{ termp_sy_pre, NULL }, /* Sy */
	{ NULL, NULL }, /* Tn */
	{ termp_xx_pre, NULL }, /* Ux */
	{ NULL, NULL }, /* Xc */
	{ NULL, NULL }, /* Xo */
	{ termp_fo_pre, termp_fo_post }, /* Fo */ 
	{ NULL, NULL }, /* Fc */ 
	{ termp_op_pre, termp_op_post }, /* Oo */
	{ NULL, NULL }, /* Oc */
	{ NULL, NULL }, /* Bk */
	{ NULL, NULL }, /* Ek */
	{ termp_bt_pre, NULL }, /* Bt */
	{ NULL, NULL }, /* Hf */
	{ NULL, NULL }, /* Fr */
	{ termp_ud_pre, NULL }, /* Ud */
	{ NULL, termp_lb_post }, /* Lb */
	{ termp_pp_pre, NULL }, /* Lp */ 
	{ termp_lk_pre, NULL }, /* Lk */ 
	{ termp_mt_pre, NULL }, /* Mt */ 
	{ termp_brq_pre, termp_brq_post }, /* Brq */ 
	{ termp_brq_pre, termp_brq_post }, /* Bro */ 
	{ NULL, NULL }, /* Brc */ 
	{ NULL, NULL }, /* %C */ 
	{ NULL, NULL }, /* Es */ 
	{ NULL, NULL }, /* En */ 
	{ termp_xx_pre, NULL }, /* Dx */ 
	{ NULL, NULL }, /* %Q */ 
	{ termp_br_pre, NULL }, /* br */
	{ termp_sp_pre, NULL }, /* sp */ 
};

static	int	  arg_hasattr(int, const struct mdoc_node *);
static	int	  arg_getattrs(const int *, int *, size_t,
			const struct mdoc_node *);
static	int	  arg_getattr(int, const struct mdoc_node *);
static	size_t	  arg_offset(const struct mdoc_argv *);
static	size_t	  arg_width(const struct mdoc_argv *, int);
static	int	  arg_listtype(const struct mdoc_node *);
static	void	  fmt_block_vspace(struct termp *,
			const struct mdoc_node *,
			const struct mdoc_node *);
static	void  	  print_node(DECL_ARGS);
static	void	  print_head(struct termp *, 
			const struct mdoc_meta *);
static	void	  print_body(DECL_ARGS);
static	void	  print_foot(struct termp *, 
			const struct mdoc_meta *);


int
mdoc_run(struct termp *p, const struct mdoc *m)
{
	/*
	 * Main output function.  When this is called, assume that the
	 * tree is properly formed.
	 */
	print_head(p, mdoc_meta(m));
	assert(mdoc_node(m));
	assert(MDOC_ROOT == mdoc_node(m)->type);
	if (mdoc_node(m)->child)
		print_body(p, NULL, mdoc_meta(m), mdoc_node(m)->child);
	print_foot(p, mdoc_meta(m));
	return(1);
}


static void
print_body(DECL_ARGS)
{

	print_node(p, pair, meta, node);
	if ( ! node->next)
		return;
	print_body(p, pair, meta, node->next);
}


static void
print_node(DECL_ARGS)
{
	int		 dochild;
	struct termpair	 npair;
	size_t		 offset, rmargin;

	dochild = 1;
	offset = p->offset;
	rmargin = p->rmargin;

	npair.ppair = pair;
	npair.flag = 0;
	npair.count = 0;

	/*
	 * Note on termpair.  This allows a pre function to set a termp
	 * flag that is automatically unset after the body, but before
	 * the post function.  Thus, if a pre uses a termpair flag, it
	 * must be reapplied in the post for use.
	 */

	if (MDOC_TEXT != node->type) {
		if (termacts[node->tok].pre)
			if ( ! (*termacts[node->tok].pre)(p, &npair, meta, node))
				dochild = 0;
	} else /* MDOC_TEXT == node->type */
		term_word(p, node->string);

	/* Children. */

	p->flags |= npair.flag;

	if (dochild && node->child)
		print_body(p, &npair, meta, node->child);

	p->flags &= ~npair.flag;

	/* Post-processing. */

	if (MDOC_TEXT != node->type)
		if (termacts[node->tok].post)
			(*termacts[node->tok].post)(p, &npair, meta, node);

	p->offset = offset;
	p->rmargin = rmargin;
}


static void
print_foot(struct termp *p, const struct mdoc_meta *meta)
{
	struct tm	*tm;
	char		*buf, *os;

	/* 
	 * Output the footer in new-groff style, that is, three columns
	 * with the middle being the manual date and flanking columns
	 * being the operating system:
	 *
	 * SYSTEM                  DATE                    SYSTEM
	 */

	if (NULL == (buf = malloc(p->rmargin)))
		err(1, "malloc");
	if (NULL == (os = malloc(p->rmargin)))
		err(1, "malloc");

	tm = localtime(&meta->date);

	if (0 == strftime(buf, p->rmargin, "%B %e, %Y", tm))
		err(1, "strftime");

	(void)strlcpy(os, meta->os, p->rmargin);

	term_vspace(p);

	p->offset = 0;
	p->rmargin = (p->maxrmargin - strlen(buf) + 1) / 2;
	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;

	term_word(p, os);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin - strlen(os);
	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;

	term_word(p, buf);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin;
	p->flags &= ~TERMP_NOBREAK;
	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;

	term_word(p, os);
	term_flushln(p);

	p->offset = 0;
	p->rmargin = p->maxrmargin;
	p->flags = 0;

	free(buf);
	free(os);
}


static void
print_head(struct termp *p, const struct mdoc_meta *meta)
{
	char		*buf, *title;

	p->rmargin = p->maxrmargin;
	p->offset = 0;

	if (NULL == (buf = malloc(p->rmargin)))
		err(1, "malloc");
	if (NULL == (title = malloc(p->rmargin)))
		err(1, "malloc");

	/*
	 * The header is strange.  It has three components, which are
	 * really two with the first duplicated.  It goes like this:
	 *
	 * IDENTIFIER              TITLE                   IDENTIFIER
	 *
	 * The IDENTIFIER is NAME(SECTION), which is the command-name
	 * (if given, or "unknown" if not) followed by the manual page
	 * section.  These are given in `Dt'.  The TITLE is a free-form
	 * string depending on the manual volume.  If not specified, it
	 * switches on the manual section.
	 */

	assert(meta->vol);
	(void)strlcpy(buf, meta->vol, p->rmargin);

	if (meta->arch) {
		(void)strlcat(buf, " (", p->rmargin);
		(void)strlcat(buf, meta->arch, p->rmargin);
		(void)strlcat(buf, ")", p->rmargin);
	}

	(void)snprintf(title, p->rmargin, "%s(%d)", 
			meta->title, meta->msec);

	p->offset = 0;
	p->rmargin = (p->maxrmargin - strlen(buf) + 1) / 2;
	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin - strlen(title);
	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;

	term_word(p, buf);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin;
	p->flags &= ~TERMP_NOBREAK;
	p->flags |= TERMP_NOLPAD | TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	p->offset = 0;
	p->rmargin = p->maxrmargin;
	p->flags &= ~TERMP_NOSPACE;

	free(title);
	free(buf);
}


static size_t
arg_width(const struct mdoc_argv *arg, int pos)
{
	int		 i, len;
	const char	*p;

	assert(pos < (int)arg->sz && pos >= 0);
	assert(arg->value[pos]);

	p = arg->value[pos];

	if (0 == (len = (int)strlen(p)))
		return(0);

	for (i = 0; i < len - 1; i++) 
		if ( ! isdigit((u_char)p[i]))
			break;

	if (i == len - 1) 
		if ('n' == p[len - 1] || 'm' == p[len - 1])
			return((size_t)atoi(p) + 2);

	return((size_t)len + 2);
}


static int
arg_listtype(const struct mdoc_node *n)
{
	int		 i, len;

	assert(MDOC_BLOCK == n->type);

	len = (int)(n->args ? n->args->argc : 0);

	for (i = 0; i < len; i++) 
		switch (n->args->argv[i].arg) {
		case (MDOC_Bullet):
			/* FALLTHROUGH */
		case (MDOC_Dash):
			/* FALLTHROUGH */
		case (MDOC_Enum):
			/* FALLTHROUGH */
		case (MDOC_Hyphen):
			/* FALLTHROUGH */
		case (MDOC_Tag):
			/* FALLTHROUGH */
		case (MDOC_Inset):
			/* FALLTHROUGH */
		case (MDOC_Diag):
			/* FALLTHROUGH */
		case (MDOC_Item):
			/* FALLTHROUGH */
		case (MDOC_Column):
			/* FALLTHROUGH */
		case (MDOC_Hang):
			/* FALLTHROUGH */
		case (MDOC_Ohang):
			return(n->args->argv[i].arg);
		default:
			break;
		}

	return(-1);
}


static size_t
arg_offset(const struct mdoc_argv *arg)
{
	int		 len, i;
	const char	*p;

	assert(*arg->value);
	p = *arg->value;

	if (0 == strcmp(p, "left"))
		return(0);
	if (0 == strcmp(p, "indent"))
		return(INDENT + 1);
	if (0 == strcmp(p, "indent-two"))
		return((INDENT + 1) * 2);

	if (0 == (len = (int)strlen(p)))
		return(0);

	for (i = 0; i < len - 1; i++) 
		if ( ! isdigit((u_char)p[i]))
			break;

	if (i == len - 1) 
		if ('n' == p[len - 1] || 'm' == p[len - 1])
			return((size_t)atoi(p));

	return((size_t)len);
}


static int
arg_hasattr(int arg, const struct mdoc_node *n)
{

	return(-1 != arg_getattr(arg, n));
}


static int
arg_getattr(int v, const struct mdoc_node *n)
{
	int		 val;

	return(arg_getattrs(&v, &val, 1, n) ? val : -1);
}


static int
arg_getattrs(const int *keys, int *vals, 
		size_t sz, const struct mdoc_node *n)
{
	int		 i, j, k;

	if (NULL == n->args)
		return(0);

	for (k = i = 0; i < (int)n->args->argc; i++) 
		for (j = 0; j < (int)sz; j++)
			if (n->args->argv[i].arg == keys[j]) {
				vals[j] = i;
				k++;
			}
	return(k);
}


/* ARGSUSED */
static void
fmt_block_vspace(struct termp *p, 
		const struct mdoc_node *bl, 
		const struct mdoc_node *node)
{
	const struct mdoc_node *n;

	term_newln(p);

	if (MDOC_Bl == bl->tok && arg_hasattr(MDOC_Compact, bl))
		return;
	assert(node);

	/*
	 * Search through our prior nodes.  If we follow a `Ss' or `Sh',
	 * then don't vspace.
	 */

	for (n = node; n; n = n->parent) {
		if (MDOC_BLOCK != n->type)
			continue;
		if (MDOC_Ss == n->tok)
			return;
		if (MDOC_Sh == n->tok)
			return;
		if (NULL == n->prev)
			continue;
		break;
	}

	/* 
	 * XXX - not documented: a `-column' does not ever assert vspace
	 * within the list.
	 */

	if (MDOC_Bl == bl->tok && arg_hasattr(MDOC_Column, bl))
		if (node->prev && MDOC_It == node->prev->tok)
			return;

	/*
	 * XXX - not documented: a `-diag' without a body does not
	 * assert a vspace prior to the next element. 
	 */
	if (MDOC_Bl == bl->tok && arg_hasattr(MDOC_Diag, bl)) 
		if (node->prev && MDOC_It == node->prev->tok) {
			assert(node->prev->body);
			if (NULL == node->prev->body->child)
				return;
		}

	term_vspace(p);
}


/* ARGSUSED */
static int
termp_dq_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);

	term_word(p, "\\(lq");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_dq_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;

	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rq");
}


/* ARGSUSED */
static int
termp_it_pre(DECL_ARGS)
{
	const struct mdoc_node *bl, *n;
	char		        buf[7];
	int		        i, type, keys[3], vals[3];
	size_t		        width, offset;

	if (MDOC_BLOCK == node->type) {
		fmt_block_vspace(p, node->parent->parent, node);
		return(1);
	}

	bl = node->parent->parent->parent;

	/* Save parent attributes. */

	pair->flag = p->flags;

	/* Get list width and offset. */

	keys[0] = MDOC_Width;
	keys[1] = MDOC_Offset;
	keys[2] = MDOC_Column;

	vals[0] = vals[1] = vals[2] = -1;

	width = offset = 0;

	(void)arg_getattrs(keys, vals, 3, bl);

	type = arg_listtype(bl);
	assert(-1 != type);

	/* Calculate real width and offset. */

	switch (type) {
	case (MDOC_Column):
		if (MDOC_BODY == node->type)
			break;
		/* 
		 * Work around groff's column handling.  The offset is
		 * equal to the sum of all widths leading to the current
		 * column (plus the -offset value).  If this column
		 * exceeds the stated number of columns, the width is
		 * set as 0, else it's the stated column width (later
		 * the 0 will be adjusted to default 10 or, if in the
		 * last column case, set to stretch to the margin).
		 */
		for (i = 0, n = node->prev; n && n && 
				i < (int)bl->args[vals[2]].argv->sz; 
				n = n->prev, i++)
			offset += arg_width 
				(&bl->args->argv[vals[2]], i);

		/* Whether exceeds maximum column. */
		if (i < (int)bl->args[vals[2]].argv->sz)
			width = arg_width(&bl->args->argv[vals[2]], i);
		else
			width = 0;

		if (vals[1] >= 0) 
			offset += arg_offset(&bl->args->argv[vals[1]]);
		break;
	default:
		if (vals[0] >= 0) 
			width = arg_width(&bl->args->argv[vals[0]], 0);
		if (vals[1] >= 0) 
			offset += arg_offset(&bl->args->argv[vals[1]]);
		break;
	}

	/* 
	 * List-type can override the width in the case of fixed-head
	 * values (bullet, dash/hyphen, enum).  Tags need a non-zero
	 * offset.  FIXME: double-check that correct.
	 */

	switch (type) {
	case (MDOC_Bullet):
		/* FALLTHROUGH */
	case (MDOC_Dash):
		/* FALLTHROUGH */
	case (MDOC_Hyphen):
		if (width < 4)
			width = 4;
		break;
	case (MDOC_Enum):
		if (width < 5)
			width = 5;
		break;
	case (MDOC_Hang):
		if (0 == width)
			width = 8;
		break;
	case (MDOC_Column):
		/* FALLTHROUGH */
	case (MDOC_Tag):
		if (0 == width)
			width = 10;
		break;
	default:
		break;
	}

	/* 
	 * Whitespace control.  Inset bodies need an initial space,
	 * while diagonal bodies need two.
	 */

	p->flags |= TERMP_NOSPACE;

	switch (type) {
	case (MDOC_Diag):
		if (MDOC_BODY == node->type)
			term_word(p, "\\ \\ ");
		break;
	case (MDOC_Inset):
		if (MDOC_BODY == node->type) 
			term_word(p, "\\ ");
		break;
	default:
		break;
	}

	p->flags |= TERMP_NOSPACE;

	/*
	 * Style flags.  Diagnostic heads need TTYPE_DIAG.
	 */

	switch (type) {
	case (MDOC_Diag):
		if (MDOC_HEAD == node->type)
			p->flags |= ttypes[TTYPE_DIAG];
		break;
	default:
		break;
	}

	/*
	 * Pad and break control.  This is the tricker part.  Lists with
	 * set right-margins for the head get TERMP_NOBREAK because, if
	 * they overrun the margin, they wrap to the new margin.
	 * Correspondingly, the body for these types don't left-pad, as
	 * the head will pad out to to the right.
	 */

	switch (type) {
	case (MDOC_Bullet):
		/* FALLTHROUGH */
	case (MDOC_Dash):
		/* FALLTHROUGH */
	case (MDOC_Enum):
		/* FALLTHROUGH */
	case (MDOC_Hyphen):
		if (MDOC_HEAD == node->type)
			p->flags |= TERMP_NOBREAK;
		else
			p->flags |= TERMP_NOLPAD;
		break;
	case (MDOC_Hang):
		if (MDOC_HEAD == node->type)
			p->flags |= TERMP_NOBREAK;
		else
			p->flags |= TERMP_NOLPAD;

		if (MDOC_HEAD != node->type)
			break;

		/*
		 * This is ugly.  If `-hang' is specified and the body
		 * is a `Bl' or `Bd', then we want basically to nullify
		 * the "overstep" effect in term_flushln() and treat
		 * this as a `-ohang' list instead.
		 */
		if (node->next->child && 
				(MDOC_Bl == node->next->child->tok ||
				 MDOC_Bd == node->next->child->tok)) {
			p->flags &= ~TERMP_NOBREAK;
			p->flags &= ~TERMP_NOLPAD;
		} else
			p->flags |= TERMP_HANG;
		break;
	case (MDOC_Tag):
		if (MDOC_HEAD == node->type)
			p->flags |= TERMP_NOBREAK | TERMP_TWOSPACE;
		else
			p->flags |= TERMP_NOLPAD;

		if (MDOC_HEAD != node->type)
			break;
		if (NULL == node->next || NULL == node->next->child)
			p->flags |= TERMP_DANGLE;
		break;
	case (MDOC_Column):
		if (MDOC_HEAD == node->type) {
			assert(node->next);
			if (MDOC_BODY == node->next->type)
				p->flags &= ~TERMP_NOBREAK;
			else
				p->flags |= TERMP_NOBREAK;
			if (node->prev) 
				p->flags |= TERMP_NOLPAD;
		}
		break;
	case (MDOC_Diag):
		if (MDOC_HEAD == node->type)
			p->flags |= TERMP_NOBREAK;
		break;
	default:
		break;
	}

	/* 
	 * Margin control.  Set-head-width lists have their right
	 * margins shortened.  The body for these lists has the offset
	 * necessarily lengthened.  Everybody gets the offset.
	 */

	p->offset += offset;

	switch (type) {
	case (MDOC_Hang):
		/*
		 * Same stipulation as above, regarding `-hang'.  We
		 * don't want to recalculate rmargin and offsets when
		 * using `Bd' or `Bl' within `-hang' overstep lists.
		 */
		if (MDOC_HEAD == node->type && node->next->child &&
				(MDOC_Bl == node->next->child->tok || 
				 MDOC_Bd == node->next->child->tok))
			break;
		/* FALLTHROUGH */
	case (MDOC_Bullet):
		/* FALLTHROUGH */
	case (MDOC_Dash):
		/* FALLTHROUGH */
	case (MDOC_Enum):
		/* FALLTHROUGH */
	case (MDOC_Hyphen):
		/* FALLTHROUGH */
	case (MDOC_Tag):
		assert(width);
		if (MDOC_HEAD == node->type)
			p->rmargin = p->offset + width;
		else 
			p->offset += width;
		break;
	case (MDOC_Column):
		assert(width);
		p->rmargin = p->offset + width;
		/* 
		 * XXX - this behaviour is not documented: the
		 * right-most column is filled to the right margin.
		 */
		if (MDOC_HEAD == node->type &&
				MDOC_BODY == node->next->type)
			p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}

	/* 
	 * The dash, hyphen, bullet and enum lists all have a special
	 * HEAD character (temporarily bold, in some cases).  
	 */

	if (MDOC_HEAD == node->type)
		switch (type) {
		case (MDOC_Bullet):
			p->flags |= TERMP_BOLD;
			term_word(p, "\\[bu]");
			p->flags &= ~TERMP_BOLD;
			break;
		case (MDOC_Dash):
			/* FALLTHROUGH */
		case (MDOC_Hyphen):
			p->flags |= TERMP_BOLD;
			term_word(p, "\\(hy");
			p->flags &= ~TERMP_BOLD;
			break;
		case (MDOC_Enum):
			(pair->ppair->ppair->count)++;
			(void)snprintf(buf, sizeof(buf), "%d.", 
					pair->ppair->ppair->count);
			term_word(p, buf);
			break;
		default:
			break;
		}

	/* 
	 * If we're not going to process our children, indicate so here.
	 */

	switch (type) {
	case (MDOC_Bullet):
		/* FALLTHROUGH */
	case (MDOC_Item):
		/* FALLTHROUGH */
	case (MDOC_Dash):
		/* FALLTHROUGH */
	case (MDOC_Hyphen):
		/* FALLTHROUGH */
	case (MDOC_Enum):
		if (MDOC_HEAD == node->type)
			return(0);
		break;
	case (MDOC_Column):
		if (MDOC_BODY == node->type)
			return(0);
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
termp_it_post(DECL_ARGS)
{
	int		   type;

	if (MDOC_BODY != node->type && MDOC_HEAD != node->type)
		return;

	type = arg_listtype(node->parent->parent->parent);
	assert(-1 != type);

	switch (type) {
	case (MDOC_Item):
		/* FALLTHROUGH */
	case (MDOC_Diag):
		/* FALLTHROUGH */
	case (MDOC_Inset):
		if (MDOC_BODY == node->type)
			term_flushln(p);
		break;
	case (MDOC_Column):
		if (MDOC_HEAD == node->type)
			term_flushln(p);
		break;
	default:
		term_flushln(p);
		break;
	}

	p->flags = pair->flag;
}


/* ARGSUSED */
static int
termp_nm_pre(DECL_ARGS)
{

	if (SEC_SYNOPSIS == node->sec)
		term_newln(p);

	pair->flag |= ttypes[TTYPE_PROG];
	p->flags |= ttypes[TTYPE_PROG];

	if (NULL == node->child)
		term_word(p, meta->name);

	return(1);
}


/* ARGSUSED */
static int
termp_fl_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_CMD_FLAG];
	p->flags |= ttypes[TTYPE_CMD_FLAG];
	term_word(p, "\\-");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
termp_an_pre(DECL_ARGS)
{

	if (NULL == node->child)
		return(1);

	/*
	 * XXX: this is poorly documented.  If not in the AUTHORS
	 * section, `An -split' will cause newlines to occur before the
	 * author name.  If in the AUTHORS section, by default, the
	 * first `An' invocation is nosplit, then all subsequent ones,
	 * regardless of whether interspersed with other macros/text,
	 * are split.  -split, in this case, will override the condition
	 * of the implied first -nosplit.
	 */
	
	if (node->sec == SEC_AUTHORS) {
		if ( ! (TERMP_ANPREC & p->flags)) {
			if (TERMP_SPLIT & p->flags)
				term_newln(p);
			return(1);
		}
		if (TERMP_NOSPLIT & p->flags)
			return(1);
		term_newln(p);
		return(1);
	}

	if (TERMP_SPLIT & p->flags)
		term_newln(p);

	return(1);
}


/* ARGSUSED */
static void
termp_an_post(DECL_ARGS)
{

	if (node->child) {
		if (SEC_AUTHORS == node->sec)
			p->flags |= TERMP_ANPREC;
		return;
	}

	if (arg_getattr(MDOC_Split, node) > -1) {
		p->flags &= ~TERMP_NOSPLIT;
		p->flags |= TERMP_SPLIT;
	} else {
		p->flags &= ~TERMP_SPLIT;
		p->flags |= TERMP_NOSPLIT;
	}

}


/* ARGSUSED */
static int
termp_ar_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_CMD_ARG];
	return(1);
}


/* ARGSUSED */
static int
termp_ns_pre(DECL_ARGS)
{

	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
termp_pp_pre(DECL_ARGS)
{

	term_vspace(p);
	return(1);
}


/* ARGSUSED */
static int
termp_rs_pre(DECL_ARGS)
{

	if (MDOC_BLOCK == node->type && node->prev)
		term_vspace(p);
	return(1);
}


/* ARGSUSED */
static int
termp_rv_pre(DECL_ARGS)
{
	int		 i;

	i = arg_getattr(MDOC_Std, node);
	assert(-1 != i);
	assert(node->args->argv[i].sz);

	term_newln(p);
	term_word(p, "The");

	p->flags |= ttypes[TTYPE_FUNC_NAME];
	term_word(p, *node->args->argv[i].value);
	p->flags &= ~ttypes[TTYPE_FUNC_NAME];
	p->flags |= TERMP_NOSPACE;

       	term_word(p, "() function returns the value 0 if successful;");
       	term_word(p, "otherwise the value -1 is returned and the");
       	term_word(p, "global variable");

	p->flags |= ttypes[TTYPE_VAR_DECL];
	term_word(p, "errno");
	p->flags &= ~ttypes[TTYPE_VAR_DECL];

       	term_word(p, "is set to indicate the error.");

	return(1);
}


/* ARGSUSED */
static int
termp_ex_pre(DECL_ARGS)
{
	int		 i;

	i = arg_getattr(MDOC_Std, node);
	assert(-1 != i);
	assert(node->args->argv[i].sz);

	term_word(p, "The");
	p->flags |= ttypes[TTYPE_PROG];
	term_word(p, *node->args->argv[i].value);
	p->flags &= ~ttypes[TTYPE_PROG];
       	term_word(p, "utility exits 0 on success, and >0 if an error occurs.");

	return(1);
}


/* ARGSUSED */
static int
termp_nd_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);

#if defined(__OpenBSD__) || defined(__linux__)
	term_word(p, "\\(en");
#else
	term_word(p, "\\(em");
#endif
	return(1);
}


/* ARGSUSED */
static void
termp_bl_post(DECL_ARGS)
{

	if (MDOC_BLOCK == node->type)
		term_newln(p);
}


/* ARGSUSED */
static void
termp_op_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type) 
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rB");
}


/* ARGSUSED */
static int
termp_xr_pre(DECL_ARGS)
{
	const struct mdoc_node *n;

	assert(node->child && MDOC_TEXT == node->child->type);
	n = node->child;

	term_word(p, n->string);
	if (NULL == (n = n->next)) 
		return(0);
	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");
	p->flags |= TERMP_NOSPACE;
	term_word(p, n->string);
	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");
	return(0);
}


/* ARGSUSED */
static int
termp_vt_pre(DECL_ARGS)
{

	/* FIXME: this can be "type name". */
	pair->flag |= ttypes[TTYPE_VAR_DECL];
	return(1);
}


/* ARGSUSED */
static void
termp_vt_post(DECL_ARGS)
{

	if (node->sec != SEC_SYNOPSIS)
		return;
	if (node->next && MDOC_Vt == node->next->tok)
		term_newln(p);
	else if (node->next)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_fd_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_FUNC_DECL];
	return(1);
}


/* ARGSUSED */
static void
termp_fd_post(DECL_ARGS)
{

	if (node->sec != SEC_SYNOPSIS)
		return;

	term_newln(p);
	if (node->next && MDOC_Fd != node->next->tok)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_sh_pre(DECL_ARGS)
{

	switch (node->type) {
	case (MDOC_HEAD):
		term_vspace(p);
		pair->flag |= ttypes[TTYPE_SECTION];
		break;
	case (MDOC_BODY):
		p->offset = INDENT;
		break;
	default:
		break;
	}
	return(1);
}


/* ARGSUSED */
static void
termp_sh_post(DECL_ARGS)
{

	switch (node->type) {
	case (MDOC_HEAD):
		term_newln(p);
		break;
	case (MDOC_BODY):
		term_newln(p);
		p->offset = 0;
		break;
	default:
		break;
	}
}


/* ARGSUSED */
static int
termp_op_pre(DECL_ARGS)
{

	switch (node->type) {
	case (MDOC_BODY):
		term_word(p, "\\(lB");
		p->flags |= TERMP_NOSPACE;
		break;
	default:
		break;
	}
	return(1);
}


/* ARGSUSED */
static int
termp_bt_pre(DECL_ARGS)
{

	term_word(p, "is currently in beta test.");
	return(1);
}


/* ARGSUSED */
static void
termp_lb_post(DECL_ARGS)
{

	term_newln(p);
}


/* ARGSUSED */
static int
termp_ud_pre(DECL_ARGS)
{

	term_word(p, "currently under development.");
	return(1);
}


/* ARGSUSED */
static int
termp_d1_pre(DECL_ARGS)
{

	if (MDOC_BLOCK != node->type)
		return(1);
	term_newln(p);
	p->offset += (INDENT + 1);
	return(1);
}


/* ARGSUSED */
static void
termp_d1_post(DECL_ARGS)
{

	if (MDOC_BLOCK != node->type) 
		return;
	term_newln(p);
}


/* ARGSUSED */
static int
termp_aq_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);
	term_word(p, "\\(la");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_aq_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(ra");
}


/* ARGSUSED */
static int
termp_ft_pre(DECL_ARGS)
{

	if (SEC_SYNOPSIS == node->sec)
		if (node->prev && MDOC_Fo == node->prev->tok)
			term_vspace(p);
	pair->flag |= ttypes[TTYPE_FUNC_TYPE];
	return(1);
}


/* ARGSUSED */
static void
termp_ft_post(DECL_ARGS)
{

	if (SEC_SYNOPSIS == node->sec)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_fn_pre(DECL_ARGS)
{
	const struct mdoc_node *n;

	assert(node->child && MDOC_TEXT == node->child->type);

	/* FIXME: can be "type funcname" "type varname"... */

	p->flags |= ttypes[TTYPE_FUNC_NAME];
	term_word(p, node->child->string);
	p->flags &= ~ttypes[TTYPE_FUNC_NAME];

	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");

	for (n = node->child->next; n; n = n->next) {
		p->flags |= ttypes[TTYPE_FUNC_ARG];
		term_word(p, n->string);
		p->flags &= ~ttypes[TTYPE_FUNC_ARG];
		if (n->next)
			term_word(p, ",");
	}

	term_word(p, ")");

	if (SEC_SYNOPSIS == node->sec)
		term_word(p, ";");

	return(0);
}


/* ARGSUSED */
static void
termp_fn_post(DECL_ARGS)
{

	if (node->sec == SEC_SYNOPSIS && node->next)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_sx_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_LINK];
	return(1);
}


/* ARGSUSED */
static int
termp_fa_pre(DECL_ARGS)
{
	struct mdoc_node *n;

	if (node->parent->tok != MDOC_Fo) {
		pair->flag |= ttypes[TTYPE_FUNC_ARG];
		return(1);
	}

	for (n = node->child; n; n = n->next) {
		p->flags |= ttypes[TTYPE_FUNC_ARG];
		term_word(p, n->string);
		p->flags &= ~ttypes[TTYPE_FUNC_ARG];
		if (n->next)
			term_word(p, ",");
	}

	if (node->child && node->next && node->next->tok == MDOC_Fa)
		term_word(p, ",");

	return(0);
}


/* ARGSUSED */
static int
termp_va_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_VAR_DECL];
	return(1);
}


/* ARGSUSED */
static int
termp_bd_pre(DECL_ARGS)
{
	int	         i, type;

	/*
	 * This is fairly tricky due primarily to crappy documentation.
	 * If -ragged or -filled are specified, the block does nothing
	 * but change the indentation.
	 *
	 * If, on the other hand, -unfilled or -literal are specified,
	 * then the game changes.  Text is printed exactly as entered in
	 * the display: if a macro line, a newline is appended to the
	 * line.  Blank lines are allowed.
	 */

	if (MDOC_BLOCK == node->type) {
		fmt_block_vspace(p, node, node);
		return(1);
	} else if (MDOC_BODY != node->type)
		return(1);

	assert(node->parent->args);

	for (type = -1, i = 0; -1 == type && 
			i < (int)node->parent->args->argc; i++) {
		switch (node->parent->args->argv[i].arg) {
		case (MDOC_Ragged):
			/* FALLTHROUGH */
		case (MDOC_Filled):
			/* FALLTHROUGH */
		case (MDOC_Unfilled):
			/* FALLTHROUGH */
		case (MDOC_Literal):
			type = node->parent->args->argv[i].arg;
			break;
		default:
			break;
		}
	}
	
	assert(type > -1);

	i = arg_getattr(MDOC_Offset, node->parent);
	if (-1 != i)
		p->offset += arg_offset(&node->parent->args->argv[i]);

	switch (type) {
	case (MDOC_Literal):
		/* FALLTHROUGH */
	case (MDOC_Unfilled):
		break;
	default:
		return(1);
	}

	for (node = node->child; node; node = node->next) {
		p->flags |= TERMP_NOSPACE;
		print_node(p, pair, meta, node);
		if (node->next)
			term_flushln(p);
	}

	return(0);
}


/* ARGSUSED */
static void
termp_bd_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type) 
		return;
	p->flags |= TERMP_NOSPACE;
	term_flushln(p);
}


/* ARGSUSED */
static int
termp_qq_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);
	term_word(p, "\"");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_qq_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\"");
}


/* ARGSUSED */
static void
termp_bx_post(DECL_ARGS)
{

	if (node->child)
		p->flags |= TERMP_NOSPACE;
	term_word(p, "BSD");
}


/* ARGSUSED */
static int
termp_xx_pre(DECL_ARGS)
{
	const char	*pp;

	pp = NULL;
	switch (node->tok) {
	case (MDOC_Bsx):
		pp = "BSDI BSD/OS";
		break;
	case (MDOC_Dx):
		pp = "DragonFlyBSD";
		break;
	case (MDOC_Fx):
		pp = "FreeBSD";
		break;
	case (MDOC_Nx):
		pp = "NetBSD";
		break;
	case (MDOC_Ox):
		pp = "OpenBSD";
		break;
	case (MDOC_Ux):
		pp = "UNIX";
		break;
	default:
		break;
	}

	assert(pp);
	term_word(p, pp);
	return(1);
}


/* ARGSUSED */
static int
termp_sq_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);
	term_word(p, "\\(oq");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_sq_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(aq");
}


/* ARGSUSED */
static int
termp_pa_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_FILE];
	return(1);
}


/* ARGSUSED */
static int
termp_pf_pre(DECL_ARGS)
{

	p->flags |= TERMP_IGNDELIM;
	return(1);
}


/* ARGSUSED */
static void
termp_pf_post(DECL_ARGS)
{

	p->flags &= ~TERMP_IGNDELIM;
	p->flags |= TERMP_NOSPACE;
}


/* ARGSUSED */
static int
termp_ss_pre(DECL_ARGS)
{

	switch (node->type) {
	case (MDOC_BLOCK):
		term_newln(p);
		if (node->prev)
			term_vspace(p);
		break;
	case (MDOC_HEAD):
		pair->flag |= ttypes[TTYPE_SSECTION];
		p->offset = HALFINDENT;
		break;
	default:
		break;
	}

	return(1);
}


/* ARGSUSED */
static void
termp_ss_post(DECL_ARGS)
{

	if (MDOC_HEAD == node->type)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_em_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_EMPH];
	return(1);
}


/* ARGSUSED */
static int
termp_cd_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_CONFIG];
	term_newln(p);
	return(1);
}


/* ARGSUSED */
static int
termp_cm_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_CMD_FLAG];
	return(1);
}


/* ARGSUSED */
static int
termp_ic_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_CMD];
	return(1);
}


/* ARGSUSED */
static int
termp_in_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_INCLUDE];
	p->flags |= ttypes[TTYPE_INCLUDE];

	if (SEC_SYNOPSIS == node->sec)
		term_word(p, "#include");

	term_word(p, "<");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_in_post(DECL_ARGS)
{

	p->flags |= TERMP_NOSPACE | ttypes[TTYPE_INCLUDE];
	term_word(p, ">");
	p->flags &= ~ttypes[TTYPE_INCLUDE];

	if (SEC_SYNOPSIS != node->sec)
		return;

	term_newln(p);
	/* 
	 * XXX Not entirely correct.  If `.In foo bar' is specified in
	 * the SYNOPSIS section, then it produces a single break after
	 * the <foo>; mandoc asserts a vertical space.  Since this
	 * construction is rarely used, I think it's fine.
	 */
	if (node->next && MDOC_In != node->next->tok)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_sp_pre(DECL_ARGS)
{
	int		 i, len;

	if (NULL == node->child) {
		term_vspace(p);
		return(0);
	}

	len = atoi(node->child->string);
	if (0 == len)
		term_newln(p);
	for (i = 0; i < len; i++)
		term_vspace(p);

	return(0);
}


/* ARGSUSED */
static int
termp_br_pre(DECL_ARGS)
{

	term_newln(p);
	return(1);
}


/* ARGSUSED */
static int
termp_brq_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);
	term_word(p, "\\(lC");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_brq_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rC");
}


/* ARGSUSED */
static int
termp_bq_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);
	term_word(p, "\\(lB");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_bq_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rB");
}


/* ARGSUSED */
static int
termp_pq_pre(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return(1);
	term_word(p, "\\&(");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_pq_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;
	term_word(p, ")");
}


/* ARGSUSED */
static int
termp_fo_pre(DECL_ARGS)
{
	const struct mdoc_node *n;

	if (MDOC_BODY == node->type) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, "(");
		p->flags |= TERMP_NOSPACE;
		return(1);
	} else if (MDOC_HEAD != node->type) 
		return(1);

	p->flags |= ttypes[TTYPE_FUNC_NAME];
	for (n = node->child; n; n = n->next) {
		assert(MDOC_TEXT == n->type);
		term_word(p, n->string);
	}
	p->flags &= ~ttypes[TTYPE_FUNC_NAME];

	return(0);
}


/* ARGSUSED */
static void
termp_fo_post(DECL_ARGS)
{

	if (MDOC_BODY != node->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");
	p->flags |= TERMP_NOSPACE;
	term_word(p, ";");
	term_newln(p);
}


/* ARGSUSED */
static int
termp_bf_pre(DECL_ARGS)
{
	const struct mdoc_node	*n;

	if (MDOC_HEAD == node->type)
		return(0);
	else if (MDOC_BLOCK != node->type)
		return(1);

	if (NULL == (n = node->head->child)) {
		if (arg_hasattr(MDOC_Emphasis, node))
			pair->flag |= ttypes[TTYPE_EMPH];
		else if (arg_hasattr(MDOC_Symbolic, node))
			pair->flag |= ttypes[TTYPE_SYMB];

		return(1);
	} 

	assert(MDOC_TEXT == n->type);
	if (0 == strcmp("Em", n->string))
		pair->flag |= ttypes[TTYPE_EMPH];
	else if (0 == strcmp("Sy", n->string))
		pair->flag |= ttypes[TTYPE_SYMB];

	return(1);
}


/* ARGSUSED */
static int
termp_sy_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_SYMB];
	return(1);
}


/* ARGSUSED */
static int
termp_ms_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_SYMBOL];
	return(1);
}



/* ARGSUSED */
static int
termp_sm_pre(DECL_ARGS)
{

	assert(node->child && MDOC_TEXT == node->child->type);
	if (0 == strcmp("on", node->child->string)) {
		p->flags &= ~TERMP_NONOSPACE;
		p->flags &= ~TERMP_NOSPACE;
	} else
		p->flags |= TERMP_NONOSPACE;

	return(0);
}


/* ARGSUSED */
static int
termp_ap_pre(DECL_ARGS)
{

	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(aq");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
termp__j_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_REF_JOURNAL];
	return(1);
}


/* ARGSUSED */
static int
termp__t_pre(DECL_ARGS)
{

	term_word(p, "\"");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp__t_post(DECL_ARGS)
{

	p->flags |= TERMP_NOSPACE;
	term_word(p, "\"");
	termp____post(p, pair, meta, node);
}


/* ARGSUSED */
static void
termp____post(DECL_ARGS)
{

	p->flags |= TERMP_NOSPACE;
	term_word(p, node->next ? "," : ".");
}


/* ARGSUSED */
static int
termp_lk_pre(DECL_ARGS)
{
	const struct mdoc_node *n;

	assert(node->child);
	n = node->child;

	if (NULL == n->next) {
		pair->flag |= ttypes[TTYPE_LINK_ANCHOR];
		return(1);
	}

	p->flags |= ttypes[TTYPE_LINK_ANCHOR];
	term_word(p, n->string);
	p->flags |= TERMP_NOSPACE;
	term_word(p, ":");
	p->flags &= ~ttypes[TTYPE_LINK_ANCHOR];

	p->flags |= ttypes[TTYPE_LINK_TEXT];
	for (n = n->next; n; n = n->next) 
		term_word(p, n->string);

	p->flags &= ~ttypes[TTYPE_LINK_TEXT];
	return(0);
}


/* ARGSUSED */
static int
termp_mt_pre(DECL_ARGS)
{

	pair->flag |= ttypes[TTYPE_LINK_ANCHOR];
	return(1);
}


