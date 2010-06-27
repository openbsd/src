/*	$Id: mdoc_term.c,v 1.92 2010/06/27 21:54:42 schwarze Exp $ */
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
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "term.h"
#include "regs.h"
#include "mdoc.h"
#include "chars.h"
#include "main.h"

#define	INDENT		  5
#define	HALFINDENT	  3

struct	termpair {
	struct termpair	 *ppair;
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

static	size_t	  a2width(const struct termp *, const char *);
static	size_t	  a2height(const struct termp *, const char *);
static	size_t	  a2offs(const struct termp *, const char *);

static	int	  arg_hasattr(int, const struct mdoc_node *);
static	int	  arg_getattr(int, const struct mdoc_node *);
static	void	  print_bvspace(struct termp *,
			const struct mdoc_node *,
			const struct mdoc_node *);
static	void  	  print_mdoc_node(DECL_ARGS);
static	void	  print_mdoc_nodelist(DECL_ARGS);
static	void	  print_mdoc_head(struct termp *, const void *);
static	void	  print_mdoc_foot(struct termp *, const void *);
static	void	  synopsis_pre(struct termp *, 
			const struct mdoc_node *);

static	void	  termp____post(DECL_ARGS);
static	void	  termp_an_post(DECL_ARGS);
static	void	  termp_aq_post(DECL_ARGS);
static	void	  termp_bd_post(DECL_ARGS);
static	void	  termp_bk_post(DECL_ARGS);
static	void	  termp_bl_post(DECL_ARGS);
static	void	  termp_bq_post(DECL_ARGS);
static	void	  termp_brq_post(DECL_ARGS);
static	void	  termp_bx_post(DECL_ARGS);
static	void	  termp_d1_post(DECL_ARGS);
static	void	  termp_dq_post(DECL_ARGS);
static	int	  termp_fd_pre(DECL_ARGS);
static	void	  termp_fo_post(DECL_ARGS);
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

static	int	  termp_an_pre(DECL_ARGS);
static	int	  termp_ap_pre(DECL_ARGS);
static	int	  termp_aq_pre(DECL_ARGS);
static	int	  termp_bd_pre(DECL_ARGS);
static	int	  termp_bf_pre(DECL_ARGS);
static	int	  termp_bk_pre(DECL_ARGS);
static	int	  termp_bl_pre(DECL_ARGS);
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
static	int	  termp_vt_pre(DECL_ARGS);
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
	{ termp_bl_pre, termp_bl_post }, /* Bl */
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
	{ termp_fd_pre, NULL }, /* Fd */ 
	{ termp_fl_pre, NULL }, /* Fl */
	{ termp_fn_pre, NULL }, /* Fn */ 
	{ termp_ft_pre, NULL }, /* Ft */ 
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
	{ termp_vt_pre, NULL }, /* Vt */
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
	{ termp_under_pre, termp____post }, /* %T */
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
	{ NULL, NULL }, /* Ec */ /* FIXME: no space */
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
	{ termp_bk_pre, termp_bk_post }, /* Bk */
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
	{ NULL, NULL }, /* Ta */ 
};


void
terminal_mdoc(void *arg, const struct mdoc *mdoc)
{
	const struct mdoc_node	*n;
	const struct mdoc_meta	*m;
	struct termp		*p;

	p = (struct termp *)arg;

	p->overstep = 0;
	p->maxrmargin = p->defrmargin;
	p->tabwidth = term_len(p, 5);

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

	term_begin(p, print_mdoc_head, print_mdoc_foot, m);

	if (n->child)
		print_mdoc_nodelist(p, NULL, m, n->child);

	term_end(p);
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

	if (MDOC_EOS & n->flags)
		p->flags |= TERMP_SENTENCE;

	p->offset = offset;
	p->rmargin = rmargin;
}


static void
print_mdoc_foot(struct termp *p, const void *arg)
{
	char		buf[DATESIZ], os[BUFSIZ];
	const struct mdoc_meta *m;

	m = (const struct mdoc_meta *)arg;

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
	p->rmargin = (p->maxrmargin - 
			term_strlen(p, buf) + term_len(p, 1)) / 2;
	p->flags |= TERMP_NOSPACE | TERMP_NOBREAK;

	term_word(p, os);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin - term_strlen(p, os);
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


static void
print_mdoc_head(struct termp *p, const void *arg)
{
	char		buf[BUFSIZ], title[BUFSIZ];
	const struct mdoc_meta *m;

	m = (const struct mdoc_meta *)arg;

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

	snprintf(title, BUFSIZ, "%s(%s)", m->title, m->msec);

	p->offset = 0;
	p->rmargin = (p->maxrmargin - 
			term_strlen(p, buf) + term_len(p, 1)) / 2;
	p->flags |= TERMP_NOBREAK | TERMP_NOSPACE;

	term_word(p, title);
	term_flushln(p);

	p->offset = p->rmargin;
	p->rmargin = p->maxrmargin - term_strlen(p, title);
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
a2height(const struct termp *p, const char *v)
{
	struct roffsu	 su;

	assert(v);
	if ( ! a2roffsu(v, &su, SCALE_VS))
		SCALE_VS_INIT(&su, term_len(p, 1));

	return(term_vspan(p, &su));
}


static size_t
a2width(const struct termp *p, const char *v)
{
	struct roffsu	 su;

	assert(v);
	if ( ! a2roffsu(v, &su, SCALE_MAX))
		SCALE_HS_INIT(&su, term_strlen(p, v));

	return(term_hspan(p, &su));
}


static size_t
a2offs(const struct termp *p, const char *v)
{
	struct roffsu	 su;

	if ('\0' == *v)
		return(0);
	else if (0 == strcmp(v, "left"))
		return(0);
	else if (0 == strcmp(v, "indent"))
		return(term_len(p, INDENT + 1));
	else if (0 == strcmp(v, "indent-two"))
		return(term_len(p, (INDENT + 1) * 2));
	else if ( ! a2roffsu(v, &su, SCALE_MAX))
		SCALE_HS_INIT(&su, term_strlen(p, v));

	return(term_hspan(p, &su));
}


/*
 * Return 1 if an argument has a particular argument value or 0 if it
 * does not.  See arg_getattr().
 */
static int
arg_hasattr(int arg, const struct mdoc_node *n)
{

	return(-1 != arg_getattr(arg, n));
}


/*
 * Get the index of an argument in a node's argument list or -1 if it
 * does not exist.
 */
static int
arg_getattr(int v, const struct mdoc_node *n)
{
	int		 i;

	if (NULL == n->args)
		return(0);

	for (i = 0; i < (int)n->args->argc; i++) 
		if (n->args->argv[i].arg == v)
			return(i);

	return(-1);
}


/*
 * Determine how much space to print out before block elements of `It'
 * (and thus `Bl') and `Bd'.  And then go ahead and print that space,
 * too.
 */
static void
print_bvspace(struct termp *p, 
		const struct mdoc_node *bl, 
		const struct mdoc_node *n)
{
	const struct mdoc_node	*nn;

	term_newln(p);

	if (MDOC_Bd == bl->tok && bl->data.Bd.comp)
		return;
	if (MDOC_Bl == bl->tok && bl->data.Bl.comp)
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

	if (MDOC_Bl == bl->tok && LIST_column == bl->data.Bl.type)
		if (n->prev && MDOC_It == n->prev->tok)
			return;

	/* A `-diag' without body does not vspace. */

	if (MDOC_Bl == bl->tok && LIST_diag == bl->data.Bl.type)
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
	int		        i, col;
	size_t		        width, offset, ncols, dcol;
	enum mdoc_list		type;

	if (MDOC_BLOCK == n->type) {
		print_bvspace(p, n->parent->parent, n);
		return(1);
	}

	bl = n->parent->parent->parent;
	type = bl->data.Bl.type;

	/* 
	 * First calculate width and offset.  This is pretty easy unless
	 * we're a -column list, in which case all prior columns must
	 * be accounted for.
	 */

	width = offset = 0;

	if (bl->data.Bl.offs)
		offset = a2offs(p, bl->data.Bl.offs);

	switch (type) {
	case (LIST_column):
		if (MDOC_HEAD == n->type)
			break;

		col = arg_getattr(MDOC_Column, bl);

		/*
		 * Imitate groff's column handling:
		 * - For each earlier column, add its width.
		 * - For less than 5 columns, add four more blanks per
		 *   column.
		 * - For exactly 5 columns, add three more blank per
		 *   column.
		 * - For more than 5 columns, add only one column.
		 */
		ncols = bl->args->argv[col].sz;
		/* LINTED */
		dcol = ncols < 5 ? term_len(p, 4) : 
			ncols == 5 ? term_len(p, 3) : term_len(p, 1);

		/*
		 * Calculate the offset by applying all prior MDOC_BODY,
		 * so we stop at the MDOC_HEAD (NULL == nn->prev).
		 */

		for (i = 0, nn = n->prev; 
				nn->prev && i < (int)ncols; 
				nn = nn->prev, i++)
			offset += dcol + a2width
				(p, bl->args->argv[col].value[i]);

		/*
		 * When exceeding the declared number of columns, leave
		 * the remaining widths at 0.  This will later be
		 * adjusted to the default width of 10, or, for the last
		 * column, stretched to the right margin.
		 */
		if (i >= (int)ncols)
			break;

		/*
		 * Use the declared column widths, extended as explained
		 * in the preceding paragraph.
		 */
		width = a2width(p, bl->args->argv[col].value[i]) + dcol;
		break;
	default:
		if (NULL == bl->data.Bl.width)
			break;

		/* 
		 * Note: buffer the width by 2, which is groff's magic
		 * number for buffering single arguments.  See the above
		 * handling for column for how this changes.
		 */
		assert(bl->data.Bl.width);
		width = a2width(p, bl->data.Bl.width) + term_len(p, 2);
		break;
	}

	/* 
	 * List-type can override the width in the case of fixed-head
	 * values (bullet, dash/hyphen, enum).  Tags need a non-zero
	 * offset.
	 */

	switch (type) {
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		if (width < term_len(p, 4))
			width = term_len(p, 4);
		break;
	case (LIST_enum):
		if (width < term_len(p, 5))
			width = term_len(p, 5);
		break;
	case (LIST_hang):
		if (0 == width)
			width = term_len(p, 8);
		break;
	case (LIST_column):
		/* FALLTHROUGH */
	case (LIST_tag):
		if (0 == width)
			width = term_len(p, 10);
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
	case (LIST_diag):
		if (MDOC_BODY == n->type)
			term_word(p, "\\ \\ ");
		break;
	case (LIST_inset):
		if (MDOC_BODY == n->type) 
			term_word(p, "\\ ");
		break;
	default:
		break;
	}

	p->flags |= TERMP_NOSPACE;

	switch (type) {
	case (LIST_diag):
		if (MDOC_HEAD == n->type)
			term_fontpush(p, TERMFONT_BOLD);
		break;
	default:
		break;
	}

	/*
	 * Pad and break control.  This is the tricky part.  These flags
	 * are documented in term_flushln() in term.c.  Note that we're
	 * going to unset all of these flags in termp_it_post() when we
	 * exit.
	 */

	switch (type) {
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_enum):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		if (MDOC_HEAD == n->type)
			p->flags |= TERMP_NOBREAK;
		else
			p->flags |= TERMP_NOLPAD;
		break;
	case (LIST_hang):
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
	case (LIST_tag):
		if (MDOC_HEAD == n->type)
			p->flags |= TERMP_NOBREAK | TERMP_TWOSPACE;
		else
			p->flags |= TERMP_NOLPAD;

		if (MDOC_HEAD != n->type)
			break;
		if (NULL == n->next || NULL == n->next->child)
			p->flags |= TERMP_DANGLE;
		break;
	case (LIST_column):
		if (MDOC_HEAD == n->type)
			break;

		if (NULL == n->next)
			p->flags &= ~TERMP_NOBREAK;
		else
			p->flags |= TERMP_NOBREAK;

		assert(n->prev);
		if (MDOC_BODY == n->prev->type) 
			p->flags |= TERMP_NOLPAD;

		break;
	case (LIST_diag):
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
	case (LIST_hang):
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
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_enum):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		/* FALLTHROUGH */
	case (LIST_tag):
		assert(width);
		if (MDOC_HEAD == n->type)
			p->rmargin = p->offset + width;
		else 
			p->offset += width;
		break;
	case (LIST_column):
		assert(width);
		p->rmargin = p->offset + width;
		/* 
		 * XXX - this behaviour is not documented: the
		 * right-most column is filled to the right margin.
		 */
		if (MDOC_HEAD == n->type)
			break;
		if (NULL == n->next && p->rmargin < p->maxrmargin)
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
		case (LIST_bullet):
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "\\[bu]");
			term_fontpop(p);
			break;
		case (LIST_dash):
			/* FALLTHROUGH */
		case (LIST_hyphen):
			term_fontpush(p, TERMFONT_BOLD);
			term_word(p, "\\(hy");
			term_fontpop(p);
			break;
		case (LIST_enum):
			(pair->ppair->ppair->count)++;
			snprintf(buf, sizeof(buf), "%d.", 
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
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_item):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		/* FALLTHROUGH */
	case (LIST_enum):
		if (MDOC_HEAD == n->type)
			return(0);
		break;
	case (LIST_column):
		if (MDOC_HEAD == n->type)
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
	enum mdoc_list	   type;

	if (MDOC_BLOCK == n->type)
		return;

	type = n->parent->parent->parent->data.Bl.type;

	switch (type) {
	case (LIST_item):
		/* FALLTHROUGH */
	case (LIST_diag):
		/* FALLTHROUGH */
	case (LIST_inset):
		if (MDOC_BODY == n->type)
			term_newln(p);
		break;
	case (LIST_column):
		if (MDOC_BODY == n->type)
			term_flushln(p);
		break;
	default:
		term_newln(p);
		break;
	}

	/* 
	 * Now that our output is flushed, we can reset our tags.  Since
	 * only `It' sets these flags, we're free to assume that nobody
	 * has munged them in the meanwhile.
	 */

	p->flags &= ~TERMP_DANGLE;
	p->flags &= ~TERMP_NOBREAK;
	p->flags &= ~TERMP_TWOSPACE;
	p->flags &= ~TERMP_NOLPAD;
	p->flags &= ~TERMP_HANG;
}


/* ARGSUSED */
static int
termp_nm_pre(DECL_ARGS)
{

	if (NULL == n->child && NULL == m->name)
		return(1);

	synopsis_pre(p, n);

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

	if (n->child)
		p->flags |= TERMP_NOSPACE;
	else if (n->next && n->next->line == n->line)
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

	if (arg_hasattr(MDOC_Split, n)) {
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

	if (n->child && n->child->next)
		term_word(p, "functions return");
	else
		term_word(p, "function returns");

       	term_word(p, "the value 0 if successful; otherwise the value "
			"-1 is returned and the global variable");

	term_fontpush(p, TERMFONT_UNDER);
	term_word(p, "errno");
	term_fontpop(p);

       	term_word(p, "is set to indicate the error.");
	p->flags |= TERMP_SENTENCE;

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

	if (n->child && n->child->next)
		term_word(p, "utilities exit");
	else
		term_word(p, "utility exits");

       	term_word(p, "0 on success, and >0 if an error occurs.");
	p->flags |= TERMP_SENTENCE;

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
static int
termp_bl_pre(DECL_ARGS)
{

	return(MDOC_HEAD != n->type);
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

	if (NULL == n->child)
		return(0);

	assert(MDOC_TEXT == n->child->type);
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


/*
 * This decides how to assert whitespace before any of the SYNOPSIS set
 * of macros (which, as in the case of Ft/Fo and Ft/Fn, may contain
 * macro combos).
 */
static void
synopsis_pre(struct termp *p, const struct mdoc_node *n)
{
	/* 
	 * Obviously, if we're not in a SYNOPSIS or no prior macros
	 * exist, do nothing.
	 */
	if (NULL == n->prev || ! (MDOC_SYNPRETTY & n->flags))
		return;

	/*
	 * If we're the second in a pair of like elements, emit our
	 * newline and return.  UNLESS we're `Fo', `Fn', `Fn', in which
	 * case we soldier on.
	 */
	if (n->prev->tok == n->tok && 
			MDOC_Ft != n->tok && 
			MDOC_Fo != n->tok && 
			MDOC_Fn != n->tok) {
		term_newln(p);
		return;
	}

	/*
	 * If we're one of the SYNOPSIS set and non-like pair-wise after
	 * another (or Fn/Fo, which we've let slip through) then assert
	 * vertical space, else only newline and move on.
	 */
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
		term_vspace(p);
		break;
	case (MDOC_Ft):
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
			term_vspace(p);
			break;
		}
		/* FALLTHROUGH */
	default:
		term_newln(p);
		break;
	}
}


static int
termp_vt_pre(DECL_ARGS)
{

	if (MDOC_ELEM == n->type) {
		synopsis_pre(p, n);
		return(termp_under_pre(p, pair, m, n));
	} else if (MDOC_BLOCK == n->type) {
		synopsis_pre(p, n);
		return(1);
	} else if (MDOC_HEAD == n->type)
		return(0);

	return(termp_under_pre(p, pair, m, n));
}


/* ARGSUSED */
static int
termp_bold_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_BOLD);
	return(1);
}


/* ARGSUSED */
static int
termp_fd_pre(DECL_ARGS)
{

	synopsis_pre(p, n);
	return(termp_bold_pre(p, pair, m, n));
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
		p->offset = term_len(p, INDENT);
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
	p->flags |= TERMP_SENTENCE;
	return(0);
}


/* ARGSUSED */
static void
termp_lb_post(DECL_ARGS)
{

	if (SEC_LIBRARY == n->sec && MDOC_LINE & n->flags)
		term_newln(p);
}


/* ARGSUSED */
static int
termp_ud_pre(DECL_ARGS)
{

	term_word(p, "currently under development.");
	p->flags |= TERMP_SENTENCE;
	return(0);
}


/* ARGSUSED */
static int
termp_d1_pre(DECL_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);
	term_newln(p);
	p->offset += term_len(p, (INDENT + 1));
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

	/* NB: MDOC_LINE does not effect this! */
	synopsis_pre(p, n);
	term_fontpush(p, TERMFONT_UNDER);
	return(1);
}


/* ARGSUSED */
static int
termp_fn_pre(DECL_ARGS)
{
	const struct mdoc_node	*nn;

	synopsis_pre(p, n);

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

	if (MDOC_SYNPRETTY & n->flags)
		term_word(p, ";");

	return(0);
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
	size_t			 tabwidth;
	size_t			 rm, rmax;
	const struct mdoc_node	*nn;

	if (MDOC_BLOCK == n->type) {
		print_bvspace(p, n, n);
		return(1);
	} else if (MDOC_HEAD == n->type)
		return(0);

	if (n->data.Bd.offs)
		p->offset += a2offs(p, n->data.Bd.offs);

	/*
	 * If -ragged or -filled are specified, the block does nothing
	 * but change the indentation.  If -unfilled or -literal are
	 * specified, text is printed exactly as entered in the display:
	 * for macro lines, a newline is appended to the line.  Blank
	 * lines are allowed.
	 */
	
	if (DISP_literal != n->data.Bd.type && 
			DISP_unfilled != n->data.Bd.type)
		return(1);

	tabwidth = p->tabwidth;
	p->tabwidth = term_len(p, 8);
	rm = p->rmargin;
	rmax = p->maxrmargin;
	p->rmargin = p->maxrmargin = TERM_MAXMARGIN;

	for (nn = n->child; nn; nn = nn->next) {
		p->flags |= TERMP_NOSPACE;
		print_mdoc_node(p, pair, m, nn);
		if (NULL == nn->prev ||
		    nn->prev->line < nn->line ||
		    NULL == nn->next)
			term_flushln(p);
	}

	p->tabwidth = tabwidth;
	p->rmargin = rm;
	p->maxrmargin = rmax;
	return(0);
}


/* ARGSUSED */
static void
termp_bd_post(DECL_ARGS)
{
	size_t		 rm, rmax;

	if (MDOC_BODY != n->type) 
		return;

	rm = p->rmargin;
	rmax = p->maxrmargin;

	if (DISP_literal == n->data.Bd.type || 
			DISP_unfilled == n->data.Bd.type)
		p->rmargin = p->maxrmargin = TERM_MAXMARGIN;

	p->flags |= TERMP_NOSPACE;
	term_newln(p);

	p->rmargin = rm;
	p->maxrmargin = rmax;
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
		p->offset = term_len(p, HALFINDENT);
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

	synopsis_pre(p, n);
	term_fontpush(p, TERMFONT_BOLD);
	return(1);
}


/* ARGSUSED */
static int
termp_in_pre(DECL_ARGS)
{

	synopsis_pre(p, n);

	if (MDOC_SYNPRETTY & n->flags && MDOC_LINE & n->flags) {
		term_fontpush(p, TERMFONT_BOLD);
		term_word(p, "#include");
		term_word(p, "<");
	} else {
		term_word(p, "<");
		term_fontpush(p, TERMFONT_UNDER);
	}

	p->flags |= TERMP_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
termp_in_post(DECL_ARGS)
{

	if (MDOC_SYNPRETTY & n->flags)
		term_fontpush(p, TERMFONT_BOLD);

	p->flags |= TERMP_NOSPACE;
	term_word(p, ">");

	if (MDOC_SYNPRETTY & n->flags)
		term_fontpop(p);
}


/* ARGSUSED */
static int
termp_sp_pre(DECL_ARGS)
{
	size_t		 i, len;

	switch (n->tok) {
	case (MDOC_sp):
		len = n->child ? a2height(p, n->child->string) : 1;
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

	if (MDOC_BLOCK == n->type) {
		synopsis_pre(p, n);
		return(1);
	} else if (MDOC_BODY == n->type) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, "(");
		p->flags |= TERMP_NOSPACE;
		return(1);
	} 

	/* XXX: we drop non-initial arguments as per groff. */

	assert(n->child);
	assert(n->child->string);
	term_fontpush(p, TERMFONT_BOLD);
	term_word(p, n->child->string);
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

	if (MDOC_SYNPRETTY & n->flags) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, ";");
	}
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
	if (0 == strcmp("on", n->child->string))
		p->flags &= ~TERMP_NONOSPACE;
	else
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
termp_bk_pre(DECL_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		return(1);
	case (MDOC_HEAD):
		return(0);
	case (MDOC_BODY):
		p->flags |= TERMP_PREKEEP;
		return(1);
	default:
		abort();
	}
}


/* ARGSUSED */
static void
termp_bk_post(DECL_ARGS)
{

	if (MDOC_BODY == n->type)
		p->flags &= ~(TERMP_KEEP | TERMP_PREKEEP);
}

/* ARGSUSED */
static int
termp_under_pre(DECL_ARGS)
{

	term_fontpush(p, TERMFONT_UNDER);
	return(1);
}
