/*	$Id: mdoc_term.c,v 1.66 2010/01/01 21:37:52 schwarze Exp $ */
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
#include "term.h"
#include "mdoc.h"
#include "chars.h"
#include "main.h"

#define	INDENT		  5
#define	HALFINDENT	  3

struct	termpair {
	struct termpair	 *ppair;
	int	  	  flag;	
	int		  count;
};

#define	DECL_ARGS struct termp *p, \
		  struct termpair *pair, \
	  	  const struct mdoc_meta *m, \
		  const struct mdoc_node *n

struct	termact {
	int	(*pre)(DECL_ARGS);
	void	(*post)(DECL_ARGS);
};

static	size_t	  a2width(const struct mdoc_argv *, int);
static	size_t	  a2height(const struct mdoc_node *);
static	size_t	  a2offs(const struct mdoc_argv *);

static	int	  arg_hasattr(int, const struct mdoc_node *);
static	int	  arg_getattrs(const int *, int *, size_t,
			const struct mdoc_node *);
static	int	  arg_getattr(int, const struct mdoc_node *);
static	int	  arg_listtype(const struct mdoc_node *);
static	void	  print_bvspace(struct termp *,
			const struct mdoc_node *,
			const struct mdoc_node *);
static	void  	  print_mdoc_node(DECL_ARGS);
static	void	  print_mdoc_head(DECL_ARGS);
static	void	  print_mdoc_nodelist(DECL_ARGS);
static	void	  print_foot(DECL_ARGS);

static	void	  termp____post(DECL_ARGS);
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

static	int	  termp__t_pre(DECL_ARGS);
static	int	  termp_an_pre(DECL_ARGS);
static	int	  termp_ap_pre(DECL_ARGS);
static	int	  termp_aq_pre(DECL_ARGS);
static	int	  termp_bd_pre(DECL_ARGS);
static	int	  termp_bf_pre(DECL_ARGS);
static	int	  termp_bold_pre(DECL_ARGS);
static	int	  termp_bq_pre(DECL_ARGS);
static	int	  termp_brq_pre(DECL_ARGS);
static	int	  termp_bt_pre(DECL_ARGS);
static	int	  termp_cd_pre(DECL_ARGS);
static	int	  termp_d1_pre(DECL_ARGS);
static	int	  termp_dq_pre(DECL_ARGS);
static	int	  termp_ex_pre(DECL_ARGS);
static	int	  termp_fa_pre(DECL_ARGS);
static	int	  termp_fl_pre(DECL_ARGS);
static	int	  termp_fn_pre(DECL_ARGS);
static	int	  termp_fo_pre(DECL_ARGS);
static	int	  termp_ft_pre(DECL_ARGS);
static	int	  termp_in_pre(DECL_ARGS);
static	int	  termp_it_pre(DECL_ARGS);
static	int	  termp_li_pre(DECL_ARGS);
static	int	  termp_lk_pre(DECL_ARGS);
static	int	  termp_nd_pre(DECL_ARGS);
static	int	  termp_nm_pre(DECL_ARGS);
static	int	  termp_ns_pre(DECL_ARGS);
static	int	  termp_op_pre(DECL_ARGS);
static	int	  termp_pf_pre(DECL_ARGS);
static	int	  termp_pq_pre(DECL_ARGS);
static	int	  termp_qq_pre(DECL_ARGS);
static	int	  termp_rs_pre(DECL_ARGS);
static	int	  termp_rv_pre(DECL_ARGS);
static	int	  termp_sh_pre(DECL_ARGS);
static	int	  termp_sm_pre(DECL_ARGS);
static	int	  termp_sp_pre(DECL_ARGS);
static	int	  termp_sq_pre(DECL_ARGS);
static	int	  termp_ss_pre(DECL_ARGS);
static	int	  termp_under_pre(DECL_ARGS);
static	int	  termp_ud_pre(DECL_ARGS);
static	int	  termp_xr_pre(DECL_ARGS);
static	int	  termp_xx_pre(DECL_ARGS);

static	const struct termact termacts[MDOC_MAX] = {
	{ termp_ap_pre, NULL }, /* Ap */
	{ NULL, NULL }, /* Dd */
	{ NULL, NULL }, /* Dt */
	{ NULL, NULL }, /* Os */
	{ termp_sh_pre, termp_sh_post }, /* Sh */
	{ termp_ss_pre, termp_ss_post }, /* Ss */ 
	{ termp_sp_pre, NULL }, /* Pp */ 
	{ termp_d1_pre, termp_d1_post }, /* D1 */
	{ termp_d1_pre, termp_d1_post }, /* Dl */
	{ termp_bd_pre, termp_bd_post }, /* Bd */
	{ NULL, NULL }, /* Ed */
	{ NULL, termp_bl_post }, /* Bl */
	{ NULL, NULL }, /* El */
	{ termp_it_pre, termp_it_post }, /* It */
	{ NULL, NULL }, /* Ad */ 
	{ termp_an_pre, termp_an_post }, /* An */
	{ termp_under_pre, NULL }, /* Ar */
	{ termp_cd_pre, NULL }, /* Cd */
	{ termp_bold_pre, NULL }, /* Cm */
	{ NULL, NULL }, /* Dv */ 
	{ NULL, NULL }, /* Er */ 
	{ NULL, NULL }, /* Ev */ 
	{ termp_ex_pre, NULL }, /* Ex */
	{ termp_fa_pre, NULL }, /* Fa */ 
	{ termp_bold_pre, termp_fd_post }, /* Fd */ 
	{ termp_fl_pre, NULL }, /* Fl */
	{ termp_fn_pre, termp_fn_post }, /* Fn */ 
	{ termp_ft_pre, termp_ft_post }, /* Ft */ 
	{ termp_bold_pre, NULL }, /* Ic */ 
	{ termp_in_pre, termp_in_post }, /* In */ 
	{ termp_li_pre, NULL }, /* Li */
	{ termp_nd_pre, NULL }, /* Nd */ 
	{ termp_nm_pre, NULL }, /* Nm */ 
	{ termp_op_pre, termp_op_post }, /* Op */
	{ NULL, NULL }, /* Ot */
	{ termp_under_pre, NULL }, /* Pa */
	{ termp_rv_pre, NULL }, /* Rv */
	{ NULL, NULL }, /* St */ 
	{ termp_under_pre, NULL }, /* Va */
	{ termp_under_pre, termp_vt_post }, /* Vt */
	{ termp_xr_pre, NULL }, /* Xr */
	{ NULL, termp____post }, /* %A */
	{ termp_under_pre, termp____post }, /* %B */
	{ NULL, termp____post }, /* %D */
	{ termp_under_pre, termp____post }, /* %I */
	{ termp_under_pre, termp____post }, /* %J */
	{ NULL, termp____post }, /* %N */
	{ NULL, termp____post }, /* %O */
	{ NULL, termp____post }, /* %P */
	{ NULL, termp____post }, /* %R */
	{ termp__t_pre, termp____post }, /* %T */
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
	{ termp_under_pre, NULL }, /* Em */ 
	{ NULL, NULL }, /* Eo */
	{ termp_xx_pre, NULL }, /* Fx */
	{ termp_bold_pre, NULL }, /* Ms */ /* FIXME: convert to symbol? */
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
	{ termp_under_pre, NULL }, /* Sx */
	{ termp_bold_pre, NULL }, /* Sy */
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
	{ termp_sp_pre, NULL }, /* Lp */ 
	{ termp_lk_pre, NULL }, /* Lk */ 
	{ termp_under_pre, NULL }, /* Mt */ 
	{ termp_brq_pre, termp_brq_post }, /* Brq */ 
	{ termp_brq_pre, termp_brq_post }, /* Bro */ 
	{ NULL, NULL }, /* Brc */ 
	{ NULL, termp____post }, /* %C */ 
	{ NULL, NULL }, /* Es */ /* TODO */
	{ NULL, NULL }, /* En */ /* TODO */
	{ termp_xx_pre, NULL }, /* Dx */ 
	{ NULL, termp____post }, /* %Q */ 
	{ termp_sp_pre, NULL }, /* br */
	{ termp_sp_pre, NULL }, /* sp */ 
	{ termp_under_pre, termp____post }, /* %U */ 
};


void
terminal_mdoc(void *arg, const struct mdoc *mdoc)
{
	const struct mdoc_node	*n;
	const struct mdoc_meta	*m;
	struct termp		*p;

	p = (struct termp *)arg;

	if (NULL == p->symtab)
		switch (p->enc) {
		case (TERMENC_ASCII):
			p->symtab = chars_init(CHARS_ASCII);
			break;
		default:
			abort();
			/* NOTREACHED */
		}

	n = mdoc_node(mdoc);
	m = mdoc_meta(mdoc);

	print_mdoc_head(p, NULL, m, n);
	if (n->child)
		print_mdoc_nodelist(p, NULL, m, n->child);
	print_foot(p, NULL, m, n);
}


static void
print_mdoc_nodelist(DECL_ARGS)
{

	print_mdoc_node(p, pair, m, n);
	if (n->next)
		print_mdoc_nodelist(p, pair, m, n->next);
}


/* ARGSUSED */
static void
print_mdoc_node(DECL_ARGS)
{
	int		 chld;
	const void	*font;
	struct termpair	 npair;
	size_t		 offset, rmargin;

	chld = 1;
	offset = p->offset;
	rmargin = p->rmargin;
	font = term_fontq(p);

	memset(&npair, 0, sizeof(struct termpair));
	npair.ppair = pair;

	if (MDOC_TEXT != n->type) {
		if (termacts[n->tok].pre)
			chld = (*termacts[n->tok].pre)(p, &npair, m, n);
	} else 
		term_word(p, n->string); 

	if (chld && n->child)
		print_mdoc_nodelist(p, &npair, m, n->child);

	term_fontpopq(p, font);

	if (MDOC_TEXT != n->type)
		if (termacts[n->tok].post)
			(*termacts[n->tok].post)(p, &npair, m, n);

	p->offset = offset;
	p->rmargin = rmargin;
}


/* ARGSUSED */
static void
print_foot(DECL_ARGS)
{
	char		buf[DATESIZ], os[BUFSIZ];

	term_fontrepl(p, TERMFONT_NONE);

	/* 
	 * Output the footer in new-groff style, that is, three columns
	 * with the middle being the manual date and flanking columns
	 * being the operating system:
	 *
	 * SYSTEM                  DATE                    SYSTEM
	 */

	time2a(m->date, buf, DATESIZ);
	strlcpy(os, m->os, BUFSIZ);

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
}


/* ARGSUSED */
static void
print_mdoc_head(DECL_ARGS)
{
	char		buf[BUFSIZ], title[BUFSIZ];

	p->rmargin = p->maxrmargin;
	p->offset = 0;

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

	assert(m->vol);
	strlcpy(buf, m->vol, BUFSIZ);

	if (m->arch) {
		strlcat(buf, " (", BUFSIZ);
		strlcat(buf, m->arch, BUFSIZ);
		strlcat(buf, ")", BUFSIZ);
	}

	snprintf(title, BUFSIZ, "%s(%d)", m->title, m->msec);

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
}


static size_t
a2height(const struct mdoc_node *n)
{
	struct roffsu	 su;

	assert(MDOC_TEXT == n->type);
	assert(n->string);
	if ( ! a2roffsu(n->string, &su, SCALE_VS))
		SCALE_VS_INIT(&su, strlen(n->string));

	return(term_vspan(&su));
}


static size_t
a2width(const struct mdoc_argv *arg, int pos)
{
	struct roffsu	 su;

	assert(arg->value[pos]);
	if ( ! a2roffsu(arg->value[pos], &su, SCALE_MAX))
		SCALE_HS_INIT(&su, strlen(arg->value[pos]));

	/*
	 * This is a bit if a magic number on groff's part.  Be careful
	 * in changing it, as the MDOC_Column handler will subtract one
	 * from this for >5 columns (don't go below zero!).
	 */
	return(term_hspan(&su) + 2);
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
a2offs(const struct mdoc_argv *arg)
{
	struct roffsu	 su;

	if ('\0' == arg->value[0][0])
		return(0);
	else if (0 == strcmp(arg->value[0], "left"))
		return(0);
	else if (0 == strcmp(arg->value[0], "indent"))
		return(INDENT + 1);
	else if (0 == strcmp(arg->value[0], "indent-two"))
		return((INDENT + 1) * 2);
	else if ( ! a2roffsu(arg->value[0], &su, SCALE_MAX))
		SCALE_HS_INIT(&su, strlen(arg->value[0]));

	return(term_hspan(&su));
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


static void
print_bvspace(struct termp *p, 
		const struct mdoc_node *bl, 
		const struct mdoc_node *n)
{
	const struct mdoc_node	*nn;

	term_newln(p);
	if (arg_hasattr(MDOC_Compact, bl))
		return;

	/* Do not vspace directly after Ss/Sh. */

	for (nn = n; nn; nn = nn->parent) {
		if (MDOC_BLOCK != nn->type)
			continue;
		if (MDOC_Ss == nn->tok)
			return;
		if (MDOC_Sh == nn->tok)
			return;
		if (NULL == nn->prev)
			continue;
		break;
	}

	/* A `-column' does not assert vspace within the list. */

	if (MDOC_Bl == bl->tok && arg_hasattr(MDOC_Column, bl))
		if (n->prev && MDOC_It == n->prev->tok)
			return;

	/* A `-diag' without body does not vspace. */

	if (MDOC_Bl == bl->tok && arg_hasattr(MDOC_Diag, bl)) 
		if (n->prev && MDOC_It == n->prev->tok) {
			assert(n->prev->body);
			if (NULL == n->prev->body->child)
				return;
		}

	term_vspace(p);
}


/* ARGSUSED */
static int
termp_dq_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);

	term_word(p, "\\(lq");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_dq_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return;

	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rq");
}


/* ARGSUSED */
static int
termp_it_pre(DECL_ARGS)
{
	const struct mdoc_node *bl, *nn;
	char		        buf[7];
	int		        i, type, keys[3], vals[3];
	size_t		        width, offset, ncols;
	int			dcol;

	if (MDOC_BLOCK == n->type) {
		print_bvspace(p, n->parent->parent, n);
		return(1);
	}

	bl = n->parent->parent->parent;

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

	if (vals[1] >= 0) 
		offset = a2offs(&bl->args->argv[vals[1]]);

	/* Calculate real width and offset. */

	switch (type) {
	case (MDOC_Column):
		if (MDOC_BODY == n->type)
			break;

		/*
		 * Imitate groff's column handling.
		 * For each earlier column, add its width.
		 * For less than 5 columns, add two more blanks per column.
		 * For exactly 5 columns, add only one more blank per column.
		 * For more than 5 columns, SUBTRACT one column.  We can
		 * do this because a2width() pads exactly 2 spaces.
		 */
		ncols = bl->args->argv[vals[2]].sz;
		dcol = ncols < 5 ? 2 : ncols == 5 ? 1 : -1;
		for (i=0, nn=n->prev; nn && i < (int)ncols; nn=nn->prev, i++)
			offset += a2width(&bl->args->argv[vals[2]], i) + 
				(size_t)dcol;

		/*
		 * Use the declared column widths,
		 * extended as explained in the preceding paragraph.
		 */
		if (i < (int)ncols)
			width = a2width(&bl->args->argv[vals[2]], i) + 
				(size_t)dcol;

		/*
		 * When exceeding the declared number of columns,
		 * leave the remaining widths at 0.
		 * This will later be adjusted to the default width of 10,
		 * or, for the last column, stretched to the right margin.
		 */
		break;
	default:
		if (vals[0] >= 0) 
			width = a2width(&bl->args->argv[vals[0]], 0);
		break;
	}

	/* 
	 * List-type can override the width in the case of fixed-head
	 * values (bullet, dash/hyphen, enum).  Tags need a non-zero
	 * offset.
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
		if (MDOC_BODY == n->type)
			term_word(p, "\\ \\ ");
		break;
	case (MDOC_Inset):
		if (MDOC_BODY == n->type) 
			term_word(p, "\\ ");
		break;
	default:
		break;
	}

	p->flags |= TERMP_NOSPACE;

	switch (type) {
	case (MDOC_Diag):
		if (MDOC_HEAD == n->type)
			term_fontpush(p, TERMFONT_BOLD);
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
		if (MDOC_HEAD == n->type)
			p->flags |= TERMP_NOBREAK;
		else
			p->flags |= TERMP_NOLPAD;
		break;
	case (MDOC_Hang):
		if (MDOC_HEAD == n->type)
			p->flags |= TERMP_NOBREAK;
		else
			p->flags |= TERMP_NOLPAD;

		if (MDOC_HEAD != n->type)
			break;

		/*
		 * This is ugly.  If `-hang' is specified and the body
		 * is a `Bl' or `Bd', then we want basically to nullify
		 * the "overstep" effect in term_flushln() and treat
		 * this as a `-ohang' list instead.
		 */
		if (n->next->child && 
				(MDOC_Bl == n->next->child->tok ||
				 MDOC_Bd == n->next->child->tok)) {
			p->flags &= ~TERMP_NOBREAK;
			p->flags &= ~TERMP_NOLPAD;
		} else
			p->flags |= TERMP_HANG;
		break;
	case (MDOC_Tag):
		if (MDOC_HEAD == n->type)
			p->flags |= TERMP_NOBREAK | TERMP_TWOSPACE;
		else
			p->flags |= TERMP_NOLPAD;

		if (MDOC_HEAD != n->type)
			break;
		if (NULL == n->next || NULL == n->next->child)
			p->flags |= TERMP_DANGLE;
		break;
	case (MDOC_Column):
		if (MDOC_HEAD == n->type) {
			assert(n->next);
			if (MDOC_BODY == n->next->type)
				p->flags &= ~TERMP_NOBREAK;
			else
				p->flags |= TERMP_NOBREAK;
			if (n->prev) 
				p->flags |= TERMP_NOLPAD;
		}
		break;
	case (MDOC_Diag):
		if (MDOC_HEAD == n->type)
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
		if (MDOC_HEAD == n->type && n->next->child &&
				(MDOC_Bl == n->next->child->tok || 
				 MDOC_Bd == n->next->child->tok))
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
		if (MDOC_HEAD == n->type)
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
		if (MDOC_HEAD == n->type &&
				MDOC_BODY == n->next->type &&
				p->rmargin < p->maxrmargin)
			p->rmargin = p->maxrmargin;
		break;
	default:
		break;
	}

	/* 
	 * The dash, hyphen, bullet and enum lists all have a special
	 * HEAD character (temporarily bold, in some cases).  
	 */

	if (MDOC_HEAD == n->type)
		switch (type) {
		case (MDOC_Bullet):
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "\\[bu]");
			term_fontpop(p);
			break;
		case (MDOC_Dash):
			/* FALLTHROUGH */
		case (MDOC_Hyphen):
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "\\(hy");
			term_fontpop(p);
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
		if (MDOC_HEAD == n->type)
			return(0);
		break;
	case (MDOC_Column):
		if (MDOC_BODY == n->type)
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

	if (MDOC_BODY != n->type && MDOC_HEAD != n->type)
		return;

	type = arg_listtype(n->parent->parent->parent);
	assert(-1 != type);

	switch (type) {
	case (MDOC_Item):
		/* FALLTHROUGH */
	case (MDOC_Diag):
		/* FALLTHROUGH */
	case (MDOC_Inset):
		if (MDOC_BODY == n->type)
			term_flushln(p);
		break;
	case (MDOC_Column):
		if (MDOC_HEAD == n->type)
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

	if (SEC_SYNOPSIS == n->sec)
		term_newln(p);

	term_fontpush(p, TERMFONT_BOLD);

	if (NULL == n->child)
		term_word(p, m->name);
	return(1);
}


/* ARGSUSED */
static int
termp_fl_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, "\\-");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
termp_an_pre(DECL_ARGS)
{

	if (NULL == n->child)
		return(1);

	/*
	 * If not in the AUTHORS section, `An -split' will cause
	 * newlines to occur before the author name.  If in the AUTHORS
	 * section, by default, the first `An' invocation is nosplit,
	 * then all subsequent ones, regardless of whether interspersed
	 * with other macros/text, are split.  -split, in this case,
	 * will override the condition of the implied first -nosplit.
	 */
	
	if (n->sec == SEC_AUTHORS) {
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

	if (n->child) {
		if (SEC_AUTHORS == n->sec)
			p->flags |= TERMP_ANPREC;
		return;
	}

	if (arg_getattr(MDOC_Split, n) > -1) {
		p->flags &= ~TERMP_NOSPLIT;
		p->flags |= TERMP_SPLIT;
	} else {
		p->flags &= ~TERMP_SPLIT;
		p->flags |= TERMP_NOSPLIT;
	}

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
termp_rs_pre(DECL_ARGS)
{

	if (SEC_SEE_ALSO != n->sec)
		return(1);
	if (MDOC_BLOCK == n->type && n->prev)
		term_vspace(p);
	return(1);
}


/* ARGSUSED */
static int
termp_rv_pre(DECL_ARGS)
{
	const struct mdoc_node	*nn;

	term_newln(p);
	term_word(p, "The");

	for (nn = n->child; nn; nn = nn->next) {
		term_fontpush(p, TERMFONT_BOLD);
		term_word(p, nn->string);
		term_fontpop(p);
		p->flags |= TERMP_NOSPACE;
		if (nn->next && NULL == nn->next->next)
			term_word(p, "(), and");
		else if (nn->next)
			term_word(p, "(),");
		else
			term_word(p, "()");
	}

	if (n->child->next)
		term_word(p, "functions return");
	else
		term_word(p, "function returns");

       	term_word(p, "the value 0 if successful; otherwise the value "
			"-1 is returned and the global variable");

	term_fontpush(p, TERMFONT_UNDER);
	term_word(p, "errno");
	term_fontpop(p);

       	term_word(p, "is set to indicate the error.");

	return(0);
}


/* ARGSUSED */
static int
termp_ex_pre(DECL_ARGS)
{
	const struct mdoc_node	*nn;

	term_word(p, "The");

	for (nn = n->child; nn; nn = nn->next) {
		term_fontpush(p, TERMFONT_BOLD);
		term_word(p, nn->string);
		term_fontpop(p);
		p->flags |= TERMP_NOSPACE;
		if (nn->next && NULL == nn->next->next)
			term_word(p, ", and");
		else if (nn->next)
			term_word(p, ",");
		else
			p->flags &= ~TERMP_NOSPACE;
	}

	if (n->child->next)
		term_word(p, "utilities exit");
	else
		term_word(p, "utility exits");

       	term_word(p, "0 on success, and >0 if an error occurs.");

	return(0);
}


/* ARGSUSED */
static int
termp_nd_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
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

	if (MDOC_BLOCK == n->type)
		term_newln(p);
}


/* ARGSUSED */
static void
termp_op_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type) 
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rB");
}


/* ARGSUSED */
static int
termp_xr_pre(DECL_ARGS)
{
	const struct mdoc_node *nn;

	assert(n->child && MDOC_TEXT == n->child->type);
	nn = n->child;

	term_word(p, nn->string);
	if (NULL == (nn = nn->next)) 
		return(0);
	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");
	p->flags |= TERMP_NOSPACE;
	term_word(p, nn->string);
	p->flags |= TERMP_NOSPACE;
	term_word(p, ")");

	return(0);
}


/* ARGSUSED */
static void
termp_vt_post(DECL_ARGS)
{

	if (n->sec != SEC_SYNOPSIS)
		return;
	if (n->next && MDOC_Vt == n->next->tok)
		term_newln(p);
	else if (n->next)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_bold_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	return(1);
}


/* ARGSUSED */
static void
termp_fd_post(DECL_ARGS)
{

	if (n->sec != SEC_SYNOPSIS)
		return;

	term_newln(p);
	if (n->next && MDOC_Fd != n->next->tok)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_sh_pre(DECL_ARGS)
{

	/* No vspace between consecutive `Sh' calls. */

	switch (n->type) {
	case (MDOC_BLOCK):
		if (n->prev && MDOC_Sh == n->prev->tok)
			if (NULL == n->prev->body->child)
				break;
		term_vspace(p);
		break;
	case (MDOC_HEAD):
		term_fontpush(p, TERMFONT_BOLD);
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

	switch (n->type) {
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

	switch (n->type) {
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
	return(0);
}


/* ARGSUSED */
static void
termp_lb_post(DECL_ARGS)
{

	if (SEC_LIBRARY == n->sec)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_ud_pre(DECL_ARGS)
{

	term_word(p, "currently under development.");
	return(0);
}


/* ARGSUSED */
static int
termp_d1_pre(DECL_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);
	term_newln(p);
	p->offset += (INDENT + 1);
	return(1);
}


/* ARGSUSED */
static void
termp_d1_post(DECL_ARGS)
{

	if (MDOC_BLOCK != n->type) 
		return;
	term_newln(p);
}


/* ARGSUSED */
static int
termp_aq_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);
	term_word(p, "\\(la");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_aq_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(ra");
}


/* ARGSUSED */
static int
termp_ft_pre(DECL_ARGS)
{

	if (SEC_SYNOPSIS == n->sec)
		if (n->prev && MDOC_Fo == n->prev->tok)
			term_vspace(p);

	term_fontpush(p, TERMFONT_UNDER);
	return(1);
}


/* ARGSUSED */
static void
termp_ft_post(DECL_ARGS)
{

	if (SEC_SYNOPSIS == n->sec)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_fn_pre(DECL_ARGS)
{
	const struct mdoc_node	*nn;

	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, n->child->string);
	term_fontpop(p);

	p->flags |= TERMP_NOSPACE;
	term_word(p, "(");

	for (nn = n->child->next; nn; nn = nn->next) {
		term_fontpush(p, TERMFONT_UNDER);
		term_word(p, nn->string);
		term_fontpop(p);

		if (nn->next)
			term_word(p, ",");
	}

	term_word(p, ")");

	if (SEC_SYNOPSIS == n->sec)
		term_word(p, ";");

	return(0);
}


/* ARGSUSED */
static void
termp_fn_post(DECL_ARGS)
{

	if (n->sec == SEC_SYNOPSIS && n->next)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_fa_pre(DECL_ARGS)
{
	const struct mdoc_node	*nn;

	if (n->parent->tok != MDOC_Fo) {
		term_fontpush(p, TERMFONT_UNDER);
		return(1);
	}

	for (nn = n->child; nn; nn = nn->next) {
		term_fontpush(p, TERMFONT_UNDER);
		term_word(p, nn->string);
		term_fontpop(p);

		if (nn->next)
			term_word(p, ",");
	}

	if (n->child && n->next && n->next->tok == MDOC_Fa)
		term_word(p, ",");

	return(0);
}


/* ARGSUSED */
static int
termp_bd_pre(DECL_ARGS)
{
	int	         	 i, type;
	const struct mdoc_node	*nn;

	if (MDOC_BLOCK == n->type) {
		print_bvspace(p, n, n);
		return(1);
	} else if (MDOC_BODY != n->type)
		return(1);

	nn = n->parent;

	for (type = -1, i = 0; i < (int)nn->args->argc; i++) {
		switch (nn->args->argv[i].arg) {
		case (MDOC_Centred):
			/* FALLTHROUGH */
		case (MDOC_Ragged):
			/* FALLTHROUGH */
		case (MDOC_Filled):
			/* FALLTHROUGH */
		case (MDOC_Unfilled):
			/* FALLTHROUGH */
		case (MDOC_Literal):
			type = nn->args->argv[i].arg;
			break;
		case (MDOC_Offset):
			p->offset += a2offs(&nn->args->argv[i]);
			break;
		default:
			break;
		}
	}

	/*
	 * If -ragged or -filled are specified, the block does nothing
	 * but change the indentation.  If -unfilled or -literal are
	 * specified, text is printed exactly as entered in the display:
	 * for macro lines, a newline is appended to the line.  Blank
	 * lines are allowed.
	 */
	
	assert(type > -1);
	if (MDOC_Literal != type && MDOC_Unfilled != type)
		return(1);

	for (nn = n->child; nn; nn = nn->next) {
		p->flags |= TERMP_NOSPACE;
		print_mdoc_node(p, pair, m, nn);
		if (NULL == nn->next)
			continue;
		if (nn->prev && nn->prev->line < nn->line)
			term_flushln(p);
		else if (NULL == nn->prev)
			term_flushln(p);
	}

	return(0);
}


/* ARGSUSED */
static void
termp_bd_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type) 
		return;
	p->flags |= TERMP_NOSPACE;
	term_flushln(p);
}


/* ARGSUSED */
static int
termp_qq_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);
	term_word(p, "\"");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_qq_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\"");
}


/* ARGSUSED */
static void
termp_bx_post(DECL_ARGS)
{

	if (n->child)
		p->flags |= TERMP_NOSPACE;
	term_word(p, "BSD");
}


/* ARGSUSED */
static int
termp_xx_pre(DECL_ARGS)
{
	const char	*pp;

	pp = NULL;
	switch (n->tok) {
	case (MDOC_Bsx):
		pp = "BSDI BSD/OS";
		break;
	case (MDOC_Dx):
		pp = "DragonFly";
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

	if (MDOC_BODY != n->type)
		return(1);
	term_word(p, "\\(oq");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_sq_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(aq");
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

	switch (n->type) {
	case (MDOC_BLOCK):
		term_newln(p);
		if (n->prev)
			term_vspace(p);
		break;
	case (MDOC_HEAD):
		term_fontpush(p, TERMFONT_BOLD);
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

	if (MDOC_HEAD == n->type)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_cd_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	term_newln(p);
	return(1);
}


/* ARGSUSED */
static int
termp_in_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	if (SEC_SYNOPSIS == n->sec)
		term_word(p, "#include");

	term_word(p, "<");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_in_post(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	p->flags |= TERMP_NOSPACE;
	term_word(p, ">");
	term_fontpop(p);

	if (SEC_SYNOPSIS != n->sec)
		return;

	term_newln(p);
	/* 
	 * XXX Not entirely correct.  If `.In foo bar' is specified in
	 * the SYNOPSIS section, then it produces a single break after
	 * the <foo>; mandoc asserts a vertical space.  Since this
	 * construction is rarely used, I think it's fine.
	 */
	if (n->next && MDOC_In != n->next->tok)
		term_vspace(p);
}


/* ARGSUSED */
static int
termp_sp_pre(DECL_ARGS)
{
	size_t		 i, len;

	switch (n->tok) {
	case (MDOC_sp):
		len = n->child ? a2height(n->child) : 1;
		break;
	case (MDOC_br):
		len = 0;
		break;
	default:
		len = 1;
		break;
	}

	if (0 == len)
		term_newln(p);
	for (i = 0; i < len; i++)
		term_vspace(p);

	return(0);
}


/* ARGSUSED */
static int
termp_brq_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);
	term_word(p, "\\(lC");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_brq_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rC");
}


/* ARGSUSED */
static int
termp_bq_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);
	term_word(p, "\\(lB");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_bq_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	p->flags |= TERMP_NOSPACE;
	term_word(p, "\\(rB");
}


/* ARGSUSED */
static int
termp_pq_pre(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);
	term_word(p, "\\&(");
	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_pq_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	term_word(p, ")");
}


/* ARGSUSED */
static int
termp_fo_pre(DECL_ARGS)
{
	const struct mdoc_node *nn;

	if (MDOC_BODY == n->type) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, "(");
		p->flags |= TERMP_NOSPACE;
		return(1);
	} else if (MDOC_HEAD != n->type) 
		return(1);

	term_fontpush(p, TERMFONT_BOLD);
	for (nn = n->child; nn; nn = nn->next) {
		assert(MDOC_TEXT == nn->type);
		term_word(p, nn->string);
	}
	term_fontpop(p);

	return(0);
}


/* ARGSUSED */
static void
termp_fo_post(DECL_ARGS)
{

	if (MDOC_BODY != n->type)
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
	const struct mdoc_node	*nn;

	if (MDOC_HEAD == n->type)
		return(0);
	else if (MDOC_BLOCK != n->type)
		return(1);

	if (NULL == (nn = n->head->child)) {
		if (arg_hasattr(MDOC_Emphasis, n))
			term_fontpush(p, TERMFONT_UNDER);
		else if (arg_hasattr(MDOC_Symbolic, n))
			term_fontpush(p, TERMFONT_BOLD);
		else
			term_fontpush(p, TERMFONT_NONE);

		return(1);
	} 

	assert(MDOC_TEXT == nn->type);
	if (0 == strcmp("Em", nn->string))
		term_fontpush(p, TERMFONT_UNDER);
	else if (0 == strcmp("Sy", nn->string))
		term_fontpush(p, TERMFONT_BOLD);
	else
		term_fontpush(p, TERMFONT_NONE);

	return(1);
}


/* ARGSUSED */
static int
termp_sm_pre(DECL_ARGS)
{

	assert(n->child && MDOC_TEXT == n->child->type);
	if (0 == strcmp("on", n->child->string)) {
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
static void
termp____post(DECL_ARGS)
{

	/* TODO: %U. */

	p->flags |= TERMP_NOSPACE;
	switch (n->tok) {
	case (MDOC__T):
		term_word(p, "\\(rq");
		p->flags |= TERMP_NOSPACE;
		break;
	default:
		break;
	}
	term_word(p, n->next ? "," : ".");
}


/* ARGSUSED */
static int
termp_li_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_NONE);
	return(1);
}


/* ARGSUSED */
static int
termp_lk_pre(DECL_ARGS)
{
	const struct mdoc_node *nn;

	term_fontpush(p, TERMFONT_UNDER);
	nn = n->child;

	if (NULL == nn->next)
		return(1);

	term_word(p, nn->string);
	term_fontpop(p);

	p->flags |= TERMP_NOSPACE;
	term_word(p, ":");

	term_fontpush(p, TERMFONT_BOLD);
	for (nn = nn->next; nn; nn = nn->next) 
		term_word(p, nn->string);
	term_fontpop(p);

	return(0);
}


/* ARGSUSED */
static int
termp_under_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_UNDER);
	return(1);
}


/* ARGSUSED */
static int
termp__t_pre(DECL_ARGS)
{

	term_word(p, "\\(lq");
	p->flags |= TERMP_NOSPACE;
	return(1);
}
