/*	$OpenBSD: mdoc_html.c,v 1.123 2017/01/17 01:47:46 schwarze Exp $ */
/*
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014, 2015, 2016, 2017 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
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
#include <unistd.h>

#include "mandoc_aux.h"
#include "roff.h"
#include "mdoc.h"
#include "out.h"
#include "html.h"
#include "main.h"

#define	INDENT		 5

#define	MDOC_ARGS	  const struct roff_meta *meta, \
			  struct roff_node *n, \
			  struct html *h

#ifndef MIN
#define	MIN(a,b)	((/*CONSTCOND*/(a)<(b))?(a):(b))
#endif

struct	htmlmdoc {
	int		(*pre)(MDOC_ARGS);
	void		(*post)(MDOC_ARGS);
};

static	void		  print_mdoc_head(MDOC_ARGS);
static	void		  print_mdoc_node(MDOC_ARGS);
static	void		  print_mdoc_nodelist(MDOC_ARGS);
static	void		  synopsis_pre(struct html *,
				const struct roff_node *);

static	void		  mdoc_root_post(MDOC_ARGS);
static	int		  mdoc_root_pre(MDOC_ARGS);

static	void		  mdoc__x_post(MDOC_ARGS);
static	int		  mdoc__x_pre(MDOC_ARGS);
static	int		  mdoc_ad_pre(MDOC_ARGS);
static	int		  mdoc_an_pre(MDOC_ARGS);
static	int		  mdoc_ap_pre(MDOC_ARGS);
static	int		  mdoc_ar_pre(MDOC_ARGS);
static	int		  mdoc_bd_pre(MDOC_ARGS);
static	int		  mdoc_bf_pre(MDOC_ARGS);
static	void		  mdoc_bk_post(MDOC_ARGS);
static	int		  mdoc_bk_pre(MDOC_ARGS);
static	int		  mdoc_bl_pre(MDOC_ARGS);
static	int		  mdoc_cd_pre(MDOC_ARGS);
static	int		  mdoc_d1_pre(MDOC_ARGS);
static	int		  mdoc_dv_pre(MDOC_ARGS);
static	int		  mdoc_fa_pre(MDOC_ARGS);
static	int		  mdoc_fd_pre(MDOC_ARGS);
static	int		  mdoc_fl_pre(MDOC_ARGS);
static	int		  mdoc_fn_pre(MDOC_ARGS);
static	int		  mdoc_ft_pre(MDOC_ARGS);
static	int		  mdoc_em_pre(MDOC_ARGS);
static	void		  mdoc_eo_post(MDOC_ARGS);
static	int		  mdoc_eo_pre(MDOC_ARGS);
static	int		  mdoc_er_pre(MDOC_ARGS);
static	int		  mdoc_ev_pre(MDOC_ARGS);
static	int		  mdoc_ex_pre(MDOC_ARGS);
static	void		  mdoc_fo_post(MDOC_ARGS);
static	int		  mdoc_fo_pre(MDOC_ARGS);
static	int		  mdoc_ic_pre(MDOC_ARGS);
static	int		  mdoc_igndelim_pre(MDOC_ARGS);
static	int		  mdoc_in_pre(MDOC_ARGS);
static	int		  mdoc_it_pre(MDOC_ARGS);
static	int		  mdoc_lb_pre(MDOC_ARGS);
static	int		  mdoc_li_pre(MDOC_ARGS);
static	int		  mdoc_lk_pre(MDOC_ARGS);
static	int		  mdoc_mt_pre(MDOC_ARGS);
static	int		  mdoc_ms_pre(MDOC_ARGS);
static	int		  mdoc_nd_pre(MDOC_ARGS);
static	int		  mdoc_nm_pre(MDOC_ARGS);
static	int		  mdoc_no_pre(MDOC_ARGS);
static	int		  mdoc_ns_pre(MDOC_ARGS);
static	int		  mdoc_pa_pre(MDOC_ARGS);
static	void		  mdoc_pf_post(MDOC_ARGS);
static	int		  mdoc_pp_pre(MDOC_ARGS);
static	void		  mdoc_quote_post(MDOC_ARGS);
static	int		  mdoc_quote_pre(MDOC_ARGS);
static	int		  mdoc_rs_pre(MDOC_ARGS);
static	int		  mdoc_sh_pre(MDOC_ARGS);
static	int		  mdoc_skip_pre(MDOC_ARGS);
static	int		  mdoc_sm_pre(MDOC_ARGS);
static	int		  mdoc_sp_pre(MDOC_ARGS);
static	int		  mdoc_ss_pre(MDOC_ARGS);
static	int		  mdoc_sx_pre(MDOC_ARGS);
static	int		  mdoc_sy_pre(MDOC_ARGS);
static	int		  mdoc_va_pre(MDOC_ARGS);
static	int		  mdoc_vt_pre(MDOC_ARGS);
static	int		  mdoc_xr_pre(MDOC_ARGS);
static	int		  mdoc_xx_pre(MDOC_ARGS);

static	const struct htmlmdoc mdocs[MDOC_MAX] = {
	{mdoc_ap_pre, NULL}, /* Ap */
	{NULL, NULL}, /* Dd */
	{NULL, NULL}, /* Dt */
	{NULL, NULL}, /* Os */
	{mdoc_sh_pre, NULL }, /* Sh */
	{mdoc_ss_pre, NULL }, /* Ss */
	{mdoc_pp_pre, NULL}, /* Pp */
	{mdoc_d1_pre, NULL}, /* D1 */
	{mdoc_d1_pre, NULL}, /* Dl */
	{mdoc_bd_pre, NULL}, /* Bd */
	{NULL, NULL}, /* Ed */
	{mdoc_bl_pre, NULL}, /* Bl */
	{NULL, NULL}, /* El */
	{mdoc_it_pre, NULL}, /* It */
	{mdoc_ad_pre, NULL}, /* Ad */
	{mdoc_an_pre, NULL}, /* An */
	{mdoc_ar_pre, NULL}, /* Ar */
	{mdoc_cd_pre, NULL}, /* Cd */
	{mdoc_fl_pre, NULL}, /* Cm */
	{mdoc_dv_pre, NULL}, /* Dv */
	{mdoc_er_pre, NULL}, /* Er */
	{mdoc_ev_pre, NULL}, /* Ev */
	{mdoc_ex_pre, NULL}, /* Ex */
	{mdoc_fa_pre, NULL}, /* Fa */
	{mdoc_fd_pre, NULL}, /* Fd */
	{mdoc_fl_pre, NULL}, /* Fl */
	{mdoc_fn_pre, NULL}, /* Fn */
	{mdoc_ft_pre, NULL}, /* Ft */
	{mdoc_ic_pre, NULL}, /* Ic */
	{mdoc_in_pre, NULL}, /* In */
	{mdoc_li_pre, NULL}, /* Li */
	{mdoc_nd_pre, NULL}, /* Nd */
	{mdoc_nm_pre, NULL}, /* Nm */
	{mdoc_quote_pre, mdoc_quote_post}, /* Op */
	{mdoc_ft_pre, NULL}, /* Ot */
	{mdoc_pa_pre, NULL}, /* Pa */
	{mdoc_ex_pre, NULL}, /* Rv */
	{NULL, NULL}, /* St */
	{mdoc_va_pre, NULL}, /* Va */
	{mdoc_vt_pre, NULL}, /* Vt */
	{mdoc_xr_pre, NULL}, /* Xr */
	{mdoc__x_pre, mdoc__x_post}, /* %A */
	{mdoc__x_pre, mdoc__x_post}, /* %B */
	{mdoc__x_pre, mdoc__x_post}, /* %D */
	{mdoc__x_pre, mdoc__x_post}, /* %I */
	{mdoc__x_pre, mdoc__x_post}, /* %J */
	{mdoc__x_pre, mdoc__x_post}, /* %N */
	{mdoc__x_pre, mdoc__x_post}, /* %O */
	{mdoc__x_pre, mdoc__x_post}, /* %P */
	{mdoc__x_pre, mdoc__x_post}, /* %R */
	{mdoc__x_pre, mdoc__x_post}, /* %T */
	{mdoc__x_pre, mdoc__x_post}, /* %V */
	{NULL, NULL}, /* Ac */
	{mdoc_quote_pre, mdoc_quote_post}, /* Ao */
	{mdoc_quote_pre, mdoc_quote_post}, /* Aq */
	{NULL, NULL}, /* At */
	{NULL, NULL}, /* Bc */
	{mdoc_bf_pre, NULL}, /* Bf */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bo */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bq */
	{mdoc_xx_pre, NULL}, /* Bsx */
	{mdoc_xx_pre, NULL}, /* Bx */
	{mdoc_skip_pre, NULL}, /* Db */
	{NULL, NULL}, /* Dc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Do */
	{mdoc_quote_pre, mdoc_quote_post}, /* Dq */
	{NULL, NULL}, /* Ec */ /* FIXME: no space */
	{NULL, NULL}, /* Ef */
	{mdoc_em_pre, NULL}, /* Em */
	{mdoc_eo_pre, mdoc_eo_post}, /* Eo */
	{mdoc_xx_pre, NULL}, /* Fx */
	{mdoc_ms_pre, NULL}, /* Ms */
	{mdoc_no_pre, NULL}, /* No */
	{mdoc_ns_pre, NULL}, /* Ns */
	{mdoc_xx_pre, NULL}, /* Nx */
	{mdoc_xx_pre, NULL}, /* Ox */
	{NULL, NULL}, /* Pc */
	{mdoc_igndelim_pre, mdoc_pf_post}, /* Pf */
	{mdoc_quote_pre, mdoc_quote_post}, /* Po */
	{mdoc_quote_pre, mdoc_quote_post}, /* Pq */
	{NULL, NULL}, /* Qc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Ql */
	{mdoc_quote_pre, mdoc_quote_post}, /* Qo */
	{mdoc_quote_pre, mdoc_quote_post}, /* Qq */
	{NULL, NULL}, /* Re */
	{mdoc_rs_pre, NULL}, /* Rs */
	{NULL, NULL}, /* Sc */
	{mdoc_quote_pre, mdoc_quote_post}, /* So */
	{mdoc_quote_pre, mdoc_quote_post}, /* Sq */
	{mdoc_sm_pre, NULL}, /* Sm */
	{mdoc_sx_pre, NULL}, /* Sx */
	{mdoc_sy_pre, NULL}, /* Sy */
	{NULL, NULL}, /* Tn */
	{mdoc_xx_pre, NULL}, /* Ux */
	{NULL, NULL}, /* Xc */
	{NULL, NULL}, /* Xo */
	{mdoc_fo_pre, mdoc_fo_post}, /* Fo */
	{NULL, NULL}, /* Fc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Oo */
	{NULL, NULL}, /* Oc */
	{mdoc_bk_pre, mdoc_bk_post}, /* Bk */
	{NULL, NULL}, /* Ek */
	{NULL, NULL}, /* Bt */
	{NULL, NULL}, /* Hf */
	{mdoc_em_pre, NULL}, /* Fr */
	{NULL, NULL}, /* Ud */
	{mdoc_lb_pre, NULL}, /* Lb */
	{mdoc_pp_pre, NULL}, /* Lp */
	{mdoc_lk_pre, NULL}, /* Lk */
	{mdoc_mt_pre, NULL}, /* Mt */
	{mdoc_quote_pre, mdoc_quote_post}, /* Brq */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bro */
	{NULL, NULL}, /* Brc */
	{mdoc__x_pre, mdoc__x_post}, /* %C */
	{mdoc_skip_pre, NULL}, /* Es */
	{mdoc_quote_pre, mdoc_quote_post}, /* En */
	{mdoc_xx_pre, NULL}, /* Dx */
	{mdoc__x_pre, mdoc__x_post}, /* %Q */
	{mdoc_sp_pre, NULL}, /* br */
	{mdoc_sp_pre, NULL}, /* sp */
	{mdoc__x_pre, mdoc__x_post}, /* %U */
	{NULL, NULL}, /* Ta */
	{mdoc_skip_pre, NULL}, /* ll */
};

static	const char * const lists[LIST_MAX] = {
	NULL,
	"list-bul",
	"list-col",
	"list-dash",
	"list-diag",
	"list-enum",
	"list-hang",
	"list-hyph",
	"list-inset",
	"list-item",
	"list-ohang",
	"list-tag"
};


/*
 * See the same function in mdoc_term.c for documentation.
 */
static void
synopsis_pre(struct html *h, const struct roff_node *n)
{

	if (NULL == n->prev || ! (NODE_SYNPRETTY & n->flags))
		return;

	if (n->prev->tok == n->tok &&
	    MDOC_Fo != n->tok &&
	    MDOC_Ft != n->tok &&
	    MDOC_Fn != n->tok) {
		print_otag(h, TAG_BR, "");
		return;
	}

	switch (n->prev->tok) {
	case MDOC_Fd:
	case MDOC_Fn:
	case MDOC_Fo:
	case MDOC_In:
	case MDOC_Vt:
		print_paragraph(h);
		break;
	case MDOC_Ft:
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
			print_paragraph(h);
			break;
		}
		/* FALLTHROUGH */
	default:
		print_otag(h, TAG_BR, "");
		break;
	}
}

void
html_mdoc(void *arg, const struct roff_man *mdoc)
{
	struct html	*h;
	struct tag	*t, *tt;

	h = (struct html *)arg;

	if ( ! (HTML_FRAGMENT & h->oflags)) {
		print_gen_decls(h);
		t = print_otag(h, TAG_HTML, "");
		tt = print_otag(h, TAG_HEAD, "");
		print_mdoc_head(&mdoc->meta, mdoc->first->child, h);
		print_tagq(h, tt);
		print_otag(h, TAG_BODY, "");
		print_otag(h, TAG_DIV, "c", "mandoc");
	} else
		t = print_otag(h, TAG_DIV, "c", "mandoc");

	mdoc_root_pre(&mdoc->meta, mdoc->first->child, h);
	print_mdoc_nodelist(&mdoc->meta, mdoc->first->child, h);
	mdoc_root_post(&mdoc->meta, mdoc->first->child, h);
	print_tagq(h, t);
	putchar('\n');
}

static void
print_mdoc_head(MDOC_ARGS)
{

	print_gen_head(h);
	bufinit(h);
	bufcat(h, meta->title);
	if (meta->msec)
		bufcat_fmt(h, "(%s)", meta->msec);
	if (meta->arch)
		bufcat_fmt(h, " (%s)", meta->arch);

	print_otag(h, TAG_TITLE, "");
	print_text(h, h->buf);
}

static void
print_mdoc_nodelist(MDOC_ARGS)
{

	while (n != NULL) {
		print_mdoc_node(meta, n, h);
		n = n->next;
	}
}

static void
print_mdoc_node(MDOC_ARGS)
{
	int		 child;
	struct tag	*t;

	if (n->flags & NODE_NOPRT)
		return;

	child = 1;
	t = h->tags.head;
	n->flags &= ~NODE_ENDED;

	switch (n->type) {
	case ROFFT_TEXT:
		/* No tables in this mode... */
		assert(NULL == h->tblt);

		/*
		 * Make sure that if we're in a literal mode already
		 * (i.e., within a <PRE>) don't print the newline.
		 */
		if (' ' == *n->string && NODE_LINE & n->flags)
			if ( ! (HTML_LITERAL & h->flags))
				print_otag(h, TAG_BR, "");
		if (NODE_DELIMC & n->flags)
			h->flags |= HTML_NOSPACE;
		print_text(h, n->string);
		if (NODE_DELIMO & n->flags)
			h->flags |= HTML_NOSPACE;
		return;
	case ROFFT_EQN:
		if (n->flags & NODE_LINE)
			putchar('\n');
		print_eqn(h, n->eqn);
		break;
	case ROFFT_TBL:
		/*
		 * This will take care of initialising all of the table
		 * state data for the first table, then tearing it down
		 * for the last one.
		 */
		print_tbl(h, n->span);
		return;
	default:
		/*
		 * Close out the current table, if it's open, and unset
		 * the "meta" table state.  This will be reopened on the
		 * next table element.
		 */
		if (h->tblt != NULL) {
			print_tblclose(h);
			t = h->tags.head;
		}
		assert(h->tblt == NULL);
		if (mdocs[n->tok].pre && (n->end == ENDBODY_NOT || n->child))
			child = (*mdocs[n->tok].pre)(meta, n, h);
		break;
	}

	if (h->flags & HTML_KEEP && n->flags & NODE_LINE) {
		h->flags &= ~HTML_KEEP;
		h->flags |= HTML_PREKEEP;
	}

	if (child && n->child)
		print_mdoc_nodelist(meta, n->child, h);

	print_stagq(h, t);

	switch (n->type) {
	case ROFFT_EQN:
		break;
	default:
		if ( ! mdocs[n->tok].post || n->flags & NODE_ENDED)
			break;
		(*mdocs[n->tok].post)(meta, n, h);
		if (n->end != ENDBODY_NOT)
			n->body->flags |= NODE_ENDED;
		if (n->end == ENDBODY_NOSPACE)
			h->flags |= HTML_NOSPACE;
		break;
	}
}

static void
mdoc_root_post(MDOC_ARGS)
{
	struct tag	*t, *tt;

	t = print_otag(h, TAG_TABLE, "c", "foot");
	print_otag(h, TAG_TBODY, "");
	tt = print_otag(h, TAG_TR, "");

	print_otag(h, TAG_TD, "c", "foot-date");
	print_text(h, meta->date);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "foot-os");
	print_text(h, meta->os);
	print_tagq(h, t);
}

static int
mdoc_root_pre(MDOC_ARGS)
{
	struct tag	*t, *tt;
	char		*volume, *title;

	if (NULL == meta->arch)
		volume = mandoc_strdup(meta->vol);
	else
		mandoc_asprintf(&volume, "%s (%s)",
		    meta->vol, meta->arch);

	if (NULL == meta->msec)
		title = mandoc_strdup(meta->title);
	else
		mandoc_asprintf(&title, "%s(%s)",
		    meta->title, meta->msec);

	t = print_otag(h, TAG_TABLE, "c", "head");
	print_otag(h, TAG_TBODY, "");
	tt = print_otag(h, TAG_TR, "");

	print_otag(h, TAG_TD, "c", "head-ltitle");
	print_text(h, title);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "head-vol");
	print_text(h, volume);
	print_stagq(h, tt);

	print_otag(h, TAG_TD, "c", "head-rtitle");
	print_text(h, title);
	print_tagq(h, t);

	free(title);
	free(volume);
	return 1;
}

static int
mdoc_sh_pre(MDOC_ARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		print_otag(h, TAG_DIV, "c", "section");
		return 1;
	case ROFFT_BODY:
		if (n->sec == SEC_AUTHORS)
			h->flags &= ~(HTML_SPLIT|HTML_NOSPLIT);
		return 1;
	default:
		break;
	}

	bufinit(h);

	for (n = n->child; n != NULL && n->type == ROFFT_TEXT; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	if (NULL == n)
		print_otag(h, TAG_H1, "i", h->buf);
	else
		print_otag(h, TAG_H1, "");

	return 1;
}

static int
mdoc_ss_pre(MDOC_ARGS)
{
	if (n->type == ROFFT_BLOCK) {
		print_otag(h, TAG_DIV, "c", "subsection");
		return 1;
	} else if (n->type == ROFFT_BODY)
		return 1;

	bufinit(h);

	for (n = n->child; n != NULL && n->type == ROFFT_TEXT; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	if (NULL == n)
		print_otag(h, TAG_H2, "i", h->buf);
	else
		print_otag(h, TAG_H2, "");

	return 1;
}

static int
mdoc_fl_pre(MDOC_ARGS)
{
	print_otag(h, TAG_B, "c", "flag");

	/* `Cm' has no leading hyphen. */

	if (MDOC_Cm == n->tok)
		return 1;

	print_text(h, "\\-");

	if (!(n->child == NULL &&
	    (n->next == NULL ||
	     n->next->type == ROFFT_TEXT ||
	     n->next->flags & NODE_LINE)))
		h->flags |= HTML_NOSPACE;

	return 1;
}

static int
mdoc_nd_pre(MDOC_ARGS)
{
	if (n->type != ROFFT_BODY)
		return 1;

	/* XXX: this tag in theory can contain block elements. */

	print_text(h, "\\(em");
	print_otag(h, TAG_SPAN, "c", "desc");
	return 1;
}

static int
mdoc_nm_pre(MDOC_ARGS)
{
	int		 len;

	switch (n->type) {
	case ROFFT_HEAD:
		print_otag(h, TAG_TD, "");
		/* FALLTHROUGH */
	case ROFFT_ELEM:
		print_otag(h, TAG_B, "c", "name");
		if (n->child == NULL && meta->name != NULL)
			print_text(h, meta->name);
		return 1;
	case ROFFT_BODY:
		print_otag(h, TAG_TD, "");
		return 1;
	default:
		break;
	}

	synopsis_pre(h, n);
	print_otag(h, TAG_TABLE, "c", "synopsis");

	for (len = 0, n = n->head->child; n; n = n->next)
		if (n->type == ROFFT_TEXT)
			len += html_strlen(n->string);

	if (len == 0 && meta->name != NULL)
		len = html_strlen(meta->name);

	print_otag(h, TAG_COL, "shw", len);
	print_otag(h, TAG_COL, "");
	print_otag(h, TAG_TBODY, "");
	print_otag(h, TAG_TR, "");
	return 1;
}

static int
mdoc_xr_pre(MDOC_ARGS)
{
	if (NULL == n->child)
		return 0;

	if (h->base_man) {
		buffmt_man(h, n->child->string,
		    n->child->next ?
		    n->child->next->string : NULL);
		print_otag(h, TAG_A, "ch", "link-man", h->buf);
	} else
		print_otag(h, TAG_A, "c", "link-man");

	n = n->child;
	print_text(h, n->string);

	if (NULL == (n = n->next))
		return 0;

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");
	h->flags |= HTML_NOSPACE;
	print_text(h, n->string);
	h->flags |= HTML_NOSPACE;
	print_text(h, ")");
	return 0;
}

static int
mdoc_ns_pre(MDOC_ARGS)
{

	if ( ! (NODE_LINE & n->flags))
		h->flags |= HTML_NOSPACE;
	return 1;
}

static int
mdoc_ar_pre(MDOC_ARGS)
{
	print_otag(h, TAG_I, "c", "arg");
	return 1;
}

static int
mdoc_xx_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "unix");
	return 1;
}

static int
mdoc_it_pre(MDOC_ARGS)
{
	enum mdoc_list	 type;
	const struct roff_node *bl;

	bl = n->parent;
	while (bl && MDOC_Bl != bl->tok)
		bl = bl->parent;
	type = bl->norm->Bl.type;

	if (n->type == ROFFT_HEAD) {
		switch (type) {
		case LIST_bullet:
		case LIST_dash:
		case LIST_item:
		case LIST_hyphen:
		case LIST_enum:
			return 0;
		case LIST_diag:
		case LIST_hang:
		case LIST_inset:
		case LIST_ohang:
		case LIST_tag:
			print_otag(h, TAG_DT, "csvt", lists[type],
			    !bl->norm->Bl.comp);
			if (LIST_diag != type)
				break;
			print_otag(h, TAG_B, "c", "diag");
			break;
		case LIST_column:
			break;
		default:
			break;
		}
	} else if (n->type == ROFFT_BODY) {
		switch (type) {
		case LIST_bullet:
		case LIST_hyphen:
		case LIST_dash:
		case LIST_enum:
		case LIST_item:
			print_otag(h, TAG_LI, "csvt", lists[type],
			    !bl->norm->Bl.comp);
			break;
		case LIST_diag:
		case LIST_hang:
		case LIST_inset:
		case LIST_ohang:
		case LIST_tag:
			if (NULL == bl->norm->Bl.width) {
				print_otag(h, TAG_DD, "c", lists[type]);
				break;
			}
			print_otag(h, TAG_DD, "cswl", lists[type],
			    bl->norm->Bl.width);
			break;
		case LIST_column:
			print_otag(h, TAG_TD, "csvt", lists[type],
			    !bl->norm->Bl.comp);
			break;
		default:
			break;
		}
	} else {
		switch (type) {
		case LIST_column:
			print_otag(h, TAG_TR, "c", lists[type]);
			break;
		default:
			break;
		}
	}

	return 1;
}

static int
mdoc_bl_pre(MDOC_ARGS)
{
	int		 i;
	char		 buf[BUFSIZ];
	enum htmltag	 elemtype;

	if (n->type == ROFFT_BODY) {
		if (LIST_column == n->norm->Bl.type)
			print_otag(h, TAG_TBODY, "");
		return 1;
	}

	if (n->type == ROFFT_HEAD) {
		if (LIST_column != n->norm->Bl.type)
			return 0;

		/*
		 * For each column, print out the <COL> tag with our
		 * suggested width.  The last column gets min-width, as
		 * in terminal mode it auto-sizes to the width of the
		 * screen and we want to preserve that behaviour.
		 */

		for (i = 0; i < (int)n->norm->Bl.ncols - 1; i++)
			print_otag(h, TAG_COL, "sww", n->norm->Bl.cols[i]);
		print_otag(h, TAG_COL, "swW", n->norm->Bl.cols[i]);

		return 0;
	}

	assert(lists[n->norm->Bl.type]);
	(void)strlcpy(buf, "list ", BUFSIZ);
	(void)strlcat(buf, lists[n->norm->Bl.type], BUFSIZ);

	switch (n->norm->Bl.type) {
	case LIST_bullet:
	case LIST_dash:
	case LIST_hyphen:
	case LIST_item:
		elemtype = TAG_UL;
		break;
	case LIST_enum:
		elemtype = TAG_OL;
		break;
	case LIST_diag:
	case LIST_hang:
	case LIST_inset:
	case LIST_ohang:
	case LIST_tag:
		elemtype = TAG_DL;
		break;
	case LIST_column:
		elemtype = TAG_TABLE;
		break;
	default:
		abort();
	}

	if (n->norm->Bl.offs)
		print_otag(h, elemtype, "csvtvbwl", buf, 0, 0,
		    n->norm->Bl.offs);
	else
		print_otag(h, elemtype, "csvtvb", buf, 0, 0);

	return 1;
}

static int
mdoc_ex_pre(MDOC_ARGS)
{
	if (n->prev)
		print_otag(h, TAG_BR, "");
	return 1;
}

static int
mdoc_em_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "emph");
	return 1;
}

static int
mdoc_d1_pre(MDOC_ARGS)
{
	if (n->type != ROFFT_BLOCK)
		return 1;

	print_otag(h, TAG_BLOCKQUOTE, "svtvb", 0, 0);

	/* BLOCKQUOTE needs a block body. */

	print_otag(h, TAG_DIV, "c", "display");

	if (MDOC_Dl == n->tok)
		print_otag(h, TAG_CODE, "c", "lit");

	return 1;
}

static int
mdoc_sx_pre(MDOC_ARGS)
{
	bufinit(h);
	bufcat(h, "#");

	for (n = n->child; n; ) {
		bufcat_id(h, n->string);
		if (NULL != (n = n->next))
			bufcat_id(h, " ");
	}

	print_otag(h, TAG_I, "c", "link-sec");
	print_otag(h, TAG_A, "ch", "link-sec", h->buf);
	return 1;
}

static int
mdoc_bd_pre(MDOC_ARGS)
{
	int			 comp, offs, sv;
	struct roff_node	*nn;

	if (n->type == ROFFT_HEAD)
		return 0;

	if (n->type == ROFFT_BLOCK) {
		comp = n->norm->Bd.comp;
		for (nn = n; nn && ! comp; nn = nn->parent) {
			if (nn->type != ROFFT_BLOCK)
				continue;
			if (MDOC_Ss == nn->tok || MDOC_Sh == nn->tok)
				comp = 1;
			if (nn->prev)
				break;
		}
		if ( ! comp)
			print_paragraph(h);
		return 1;
	}

	/* Handle the -offset argument. */

	if (n->norm->Bd.offs == NULL ||
	    ! strcmp(n->norm->Bd.offs, "left"))
		offs = 0;
	else if ( ! strcmp(n->norm->Bd.offs, "indent"))
		offs = INDENT;
	else if ( ! strcmp(n->norm->Bd.offs, "indent-two"))
		offs = INDENT * 2;
	else
		offs = -1;

	if (offs == -1)
		print_otag(h, TAG_DIV, "cswl", "display", n->norm->Bd.offs);
	else
		print_otag(h, TAG_DIV, "cshl", "display", offs);

	if (n->norm->Bd.type != DISP_unfilled &&
	    n->norm->Bd.type != DISP_literal)
		return 1;

	print_otag(h, TAG_PRE, "c", "lit");

	/* This can be recursive: save & set our literal state. */

	sv = h->flags & HTML_LITERAL;
	h->flags |= HTML_LITERAL;

	for (nn = n->child; nn; nn = nn->next) {
		print_mdoc_node(meta, nn, h);
		/*
		 * If the printed node flushes its own line, then we
		 * needn't do it here as well.  This is hacky, but the
		 * notion of selective eoln whitespace is pretty dumb
		 * anyway, so don't sweat it.
		 */
		switch (nn->tok) {
		case MDOC_Sm:
		case MDOC_br:
		case MDOC_sp:
		case MDOC_Bl:
		case MDOC_D1:
		case MDOC_Dl:
		case MDOC_Lp:
		case MDOC_Pp:
			continue;
		default:
			break;
		}
		if (h->flags & HTML_NONEWLINE ||
		    (nn->next && ! (nn->next->flags & NODE_LINE)))
			continue;
		else if (nn->next)
			print_text(h, "\n");

		h->flags |= HTML_NOSPACE;
	}

	if (0 == sv)
		h->flags &= ~HTML_LITERAL;

	return 0;
}

static int
mdoc_pa_pre(MDOC_ARGS)
{
	print_otag(h, TAG_I, "c", "file");
	return 1;
}

static int
mdoc_ad_pre(MDOC_ARGS)
{
	print_otag(h, TAG_I, "c", "addr");
	return 1;
}

static int
mdoc_an_pre(MDOC_ARGS)
{
	if (n->norm->An.auth == AUTH_split) {
		h->flags &= ~HTML_NOSPLIT;
		h->flags |= HTML_SPLIT;
		return 0;
	}
	if (n->norm->An.auth == AUTH_nosplit) {
		h->flags &= ~HTML_SPLIT;
		h->flags |= HTML_NOSPLIT;
		return 0;
	}

	if (h->flags & HTML_SPLIT)
		print_otag(h, TAG_BR, "");

	if (n->sec == SEC_AUTHORS && ! (h->flags & HTML_NOSPLIT))
		h->flags |= HTML_SPLIT;

	print_otag(h, TAG_SPAN, "c", "author");
	return 1;
}

static int
mdoc_cd_pre(MDOC_ARGS)
{
	synopsis_pre(h, n);
	print_otag(h, TAG_B, "c", "config");
	return 1;
}

static int
mdoc_dv_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "define");
	return 1;
}

static int
mdoc_ev_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "env");
	return 1;
}

static int
mdoc_er_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "errno");
	return 1;
}

static int
mdoc_fa_pre(MDOC_ARGS)
{
	const struct roff_node	*nn;
	struct tag		*t;

	if (n->parent->tok != MDOC_Fo) {
		print_otag(h, TAG_I, "c", "farg");
		return 1;
	}

	for (nn = n->child; nn; nn = nn->next) {
		t = print_otag(h, TAG_I, "c", "farg");
		print_text(h, nn->string);
		print_tagq(h, t);
		if (nn->next) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}
	}

	if (n->child && n->next && n->next->tok == MDOC_Fa) {
		h->flags |= HTML_NOSPACE;
		print_text(h, ",");
	}

	return 0;
}

static int
mdoc_fd_pre(MDOC_ARGS)
{
	char		 buf[BUFSIZ];
	size_t		 sz;
	struct tag	*t;

	synopsis_pre(h, n);

	if (NULL == (n = n->child))
		return 0;

	assert(n->type == ROFFT_TEXT);

	if (strcmp(n->string, "#include")) {
		print_otag(h, TAG_B, "c", "macro");
		return 1;
	}

	print_otag(h, TAG_B, "c", "includes");
	print_text(h, n->string);

	if (NULL != (n = n->next)) {
		assert(n->type == ROFFT_TEXT);

		/*
		 * XXX This is broken and not easy to fix.
		 * When using -Oincludes, truncation may occur.
		 * Dynamic allocation wouldn't help because
		 * passing long strings to buffmt_includes()
		 * does not work either.
		 */

		strlcpy(buf, '<' == *n->string || '"' == *n->string ?
		    n->string + 1 : n->string, BUFSIZ);

		sz = strlen(buf);
		if (sz && ('>' == buf[sz - 1] || '"' == buf[sz - 1]))
			buf[sz - 1] = '\0';

		if (h->base_includes) {
			buffmt_includes(h, buf);
			t = print_otag(h, TAG_A, "ch", "link-includes",
			    h->buf);
		} else
			t = print_otag(h, TAG_A, "c", "link-includes");

		print_text(h, n->string);
		print_tagq(h, t);

		n = n->next;
	}

	for ( ; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);
		print_text(h, n->string);
	}

	return 0;
}

static int
mdoc_vt_pre(MDOC_ARGS)
{
	if (n->type == ROFFT_BLOCK) {
		synopsis_pre(h, n);
		return 1;
	} else if (n->type == ROFFT_ELEM) {
		synopsis_pre(h, n);
	} else if (n->type == ROFFT_HEAD)
		return 0;

	print_otag(h, TAG_SPAN, "c", "type");
	return 1;
}

static int
mdoc_ft_pre(MDOC_ARGS)
{
	synopsis_pre(h, n);
	print_otag(h, TAG_I, "c", "ftype");
	return 1;
}

static int
mdoc_fn_pre(MDOC_ARGS)
{
	struct tag	*t;
	char		 nbuf[BUFSIZ];
	const char	*sp, *ep;
	int		 sz, pretty;

	pretty = NODE_SYNPRETTY & n->flags;
	synopsis_pre(h, n);

	/* Split apart into type and name. */
	assert(n->child->string);
	sp = n->child->string;

	ep = strchr(sp, ' ');
	if (NULL != ep) {
		t = print_otag(h, TAG_I, "c", "ftype");

		while (ep) {
			sz = MIN((int)(ep - sp), BUFSIZ - 1);
			(void)memcpy(nbuf, sp, (size_t)sz);
			nbuf[sz] = '\0';
			print_text(h, nbuf);
			sp = ++ep;
			ep = strchr(sp, ' ');
		}
		print_tagq(h, t);
	}

	t = print_otag(h, TAG_B, "c", "fname");

	if (sp)
		print_text(h, sp);

	print_tagq(h, t);

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");
	h->flags |= HTML_NOSPACE;

	for (n = n->child->next; n; n = n->next) {
		if (NODE_SYNPRETTY & n->flags)
			t = print_otag(h, TAG_I, "css?", "farg",
			    "white-space", "nowrap");
		else
			t = print_otag(h, TAG_I, "c", "farg");
		print_text(h, n->string);
		print_tagq(h, t);
		if (n->next) {
			h->flags |= HTML_NOSPACE;
			print_text(h, ",");
		}
	}

	h->flags |= HTML_NOSPACE;
	print_text(h, ")");

	if (pretty) {
		h->flags |= HTML_NOSPACE;
		print_text(h, ";");
	}

	return 0;
}

static int
mdoc_sm_pre(MDOC_ARGS)
{

	if (NULL == n->child)
		h->flags ^= HTML_NONOSPACE;
	else if (0 == strcmp("on", n->child->string))
		h->flags &= ~HTML_NONOSPACE;
	else
		h->flags |= HTML_NONOSPACE;

	if ( ! (HTML_NONOSPACE & h->flags))
		h->flags &= ~HTML_NOSPACE;

	return 0;
}

static int
mdoc_skip_pre(MDOC_ARGS)
{

	return 0;
}

static int
mdoc_pp_pre(MDOC_ARGS)
{

	print_paragraph(h);
	return 0;
}

static int
mdoc_sp_pre(MDOC_ARGS)
{
	struct roffsu	 su;

	SCALE_VS_INIT(&su, 1);

	if (MDOC_sp == n->tok) {
		if (NULL != (n = n->child)) {
			if ( ! a2roffsu(n->string, &su, SCALE_VS))
				su.scale = 1.0;
			else if (su.scale < 0.0)
				su.scale = 0.0;
		}
	} else
		su.scale = 0.0;

	print_otag(h, TAG_DIV, "suh", &su);

	/* So the div isn't empty: */
	print_text(h, "\\~");

	return 0;

}

static int
mdoc_lk_pre(MDOC_ARGS)
{
	if (NULL == (n = n->child))
		return 0;

	assert(n->type == ROFFT_TEXT);

	print_otag(h, TAG_A, "ch", "link-ext", n->string);

	if (NULL == n->next)
		print_text(h, n->string);

	for (n = n->next; n; n = n->next)
		print_text(h, n->string);

	return 0;
}

static int
mdoc_mt_pre(MDOC_ARGS)
{
	struct tag	*t;

	for (n = n->child; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);

		bufinit(h);
		bufcat(h, "mailto:");
		bufcat(h, n->string);
		t = print_otag(h, TAG_A, "ch", "link-mail", h->buf);
		print_text(h, n->string);
		print_tagq(h, t);
	}

	return 0;
}

static int
mdoc_fo_pre(MDOC_ARGS)
{
	struct tag	*t;

	if (n->type == ROFFT_BODY) {
		h->flags |= HTML_NOSPACE;
		print_text(h, "(");
		h->flags |= HTML_NOSPACE;
		return 1;
	} else if (n->type == ROFFT_BLOCK) {
		synopsis_pre(h, n);
		return 1;
	}

	if (n->child == NULL)
		return 0;

	assert(n->child->string);
	t = print_otag(h, TAG_B, "c", "fname");
	print_text(h, n->child->string);
	print_tagq(h, t);
	return 0;
}

static void
mdoc_fo_post(MDOC_ARGS)
{

	if (n->type != ROFFT_BODY)
		return;
	h->flags |= HTML_NOSPACE;
	print_text(h, ")");
	h->flags |= HTML_NOSPACE;
	print_text(h, ";");
}

static int
mdoc_in_pre(MDOC_ARGS)
{
	struct tag	*t;

	synopsis_pre(h, n);
	print_otag(h, TAG_B, "c", "includes");

	/*
	 * The first argument of the `In' gets special treatment as
	 * being a linked value.  Subsequent values are printed
	 * afterward.  groff does similarly.  This also handles the case
	 * of no children.
	 */

	if (NODE_SYNPRETTY & n->flags && NODE_LINE & n->flags)
		print_text(h, "#include");

	print_text(h, "<");
	h->flags |= HTML_NOSPACE;

	if (NULL != (n = n->child)) {
		assert(n->type == ROFFT_TEXT);

		if (h->base_includes) {
			buffmt_includes(h, n->string);
			t = print_otag(h, TAG_A, "ch", "link-includes",
			    h->buf);
		} else
			t = print_otag(h, TAG_A, "c", "link-includes");
		print_text(h, n->string);
		print_tagq(h, t);

		n = n->next;
	}

	h->flags |= HTML_NOSPACE;
	print_text(h, ">");

	for ( ; n; n = n->next) {
		assert(n->type == ROFFT_TEXT);
		print_text(h, n->string);
	}

	return 0;
}

static int
mdoc_ic_pre(MDOC_ARGS)
{
	print_otag(h, TAG_B, "c", "cmd");
	return 1;
}

static int
mdoc_va_pre(MDOC_ARGS)
{
	print_otag(h, TAG_B, "c", "var");
	return 1;
}

static int
mdoc_ap_pre(MDOC_ARGS)
{

	h->flags |= HTML_NOSPACE;
	print_text(h, "\\(aq");
	h->flags |= HTML_NOSPACE;
	return 1;
}

static int
mdoc_bf_pre(MDOC_ARGS)
{
	const char	*cattr;

	if (n->type == ROFFT_HEAD)
		return 0;
	else if (n->type != ROFFT_BODY)
		return 1;

	if (FONT_Em == n->norm->Bf.font)
		cattr = "emph";
	else if (FONT_Sy == n->norm->Bf.font)
		cattr = "symb";
	else if (FONT_Li == n->norm->Bf.font)
		cattr = "lit";
	else
		cattr = "none";

	/*
	 * We want this to be inline-formatted, but needs to be div to
	 * accept block children.
	 */

	print_otag(h, TAG_DIV, "css?hl", cattr, "display", "inline", 1);
	return 1;
}

static int
mdoc_ms_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "symb");
	return 1;
}

static int
mdoc_igndelim_pre(MDOC_ARGS)
{

	h->flags |= HTML_IGNDELIM;
	return 1;
}

static void
mdoc_pf_post(MDOC_ARGS)
{

	if ( ! (n->next == NULL || n->next->flags & NODE_LINE))
		h->flags |= HTML_NOSPACE;
}

static int
mdoc_rs_pre(MDOC_ARGS)
{
	if (n->type != ROFFT_BLOCK)
		return 1;

	if (n->prev && SEC_SEE_ALSO == n->sec)
		print_paragraph(h);

	print_otag(h, TAG_SPAN, "c", "ref");
	return 1;
}

static int
mdoc_no_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "none");
	return 1;
}

static int
mdoc_li_pre(MDOC_ARGS)
{
	print_otag(h, TAG_CODE, "c", "lit");
	return 1;
}

static int
mdoc_sy_pre(MDOC_ARGS)
{
	print_otag(h, TAG_SPAN, "c", "symb");
	return 1;
}

static int
mdoc_lb_pre(MDOC_ARGS)
{
	if (SEC_LIBRARY == n->sec && NODE_LINE & n->flags && n->prev)
		print_otag(h, TAG_BR, "");

	print_otag(h, TAG_SPAN, "c", "lib");
	return 1;
}

static int
mdoc__x_pre(MDOC_ARGS)
{
	const char	*cattr;
	enum htmltag	 t;

	t = TAG_SPAN;

	switch (n->tok) {
	case MDOC__A:
		cattr = "ref-auth";
		if (n->prev && MDOC__A == n->prev->tok)
			if (NULL == n->next || MDOC__A != n->next->tok)
				print_text(h, "and");
		break;
	case MDOC__B:
		cattr = "ref-book";
		t = TAG_I;
		break;
	case MDOC__C:
		cattr = "ref-city";
		break;
	case MDOC__D:
		cattr = "ref-date";
		break;
	case MDOC__I:
		cattr = "ref-issue";
		t = TAG_I;
		break;
	case MDOC__J:
		cattr = "ref-jrnl";
		t = TAG_I;
		break;
	case MDOC__N:
		cattr = "ref-num";
		break;
	case MDOC__O:
		cattr = "ref-opt";
		break;
	case MDOC__P:
		cattr = "ref-page";
		break;
	case MDOC__Q:
		cattr = "ref-corp";
		break;
	case MDOC__R:
		cattr = "ref-rep";
		break;
	case MDOC__T:
		cattr = "ref-title";
		break;
	case MDOC__U:
		cattr = "link-ref";
		break;
	case MDOC__V:
		cattr = "ref-vol";
		break;
	default:
		abort();
	}

	if (MDOC__U != n->tok) {
		print_otag(h, t, "c", cattr);
		return 1;
	}

	print_otag(h, TAG_A, "ch", cattr, n->child->string);

	return 1;
}

static void
mdoc__x_post(MDOC_ARGS)
{

	if (MDOC__A == n->tok && n->next && MDOC__A == n->next->tok)
		if (NULL == n->next->next || MDOC__A != n->next->next->tok)
			if (NULL == n->prev || MDOC__A != n->prev->tok)
				return;

	/* TODO: %U */

	if (NULL == n->parent || MDOC_Rs != n->parent->tok)
		return;

	h->flags |= HTML_NOSPACE;
	print_text(h, n->next ? "," : ".");
}

static int
mdoc_bk_pre(MDOC_ARGS)
{

	switch (n->type) {
	case ROFFT_BLOCK:
		break;
	case ROFFT_HEAD:
		return 0;
	case ROFFT_BODY:
		if (n->parent->args != NULL || n->prev->child == NULL)
			h->flags |= HTML_PREKEEP;
		break;
	default:
		abort();
	}

	return 1;
}

static void
mdoc_bk_post(MDOC_ARGS)
{

	if (n->type == ROFFT_BODY)
		h->flags &= ~(HTML_KEEP | HTML_PREKEEP);
}

static int
mdoc_quote_pre(MDOC_ARGS)
{
	if (n->type != ROFFT_BODY)
		return 1;

	switch (n->tok) {
	case MDOC_Ao:
	case MDOC_Aq:
		print_text(h, n->child != NULL && n->child->next == NULL &&
		    n->child->tok == MDOC_Mt ?  "<" : "\\(la");
		break;
	case MDOC_Bro:
	case MDOC_Brq:
		print_text(h, "\\(lC");
		break;
	case MDOC_Bo:
	case MDOC_Bq:
		print_text(h, "\\(lB");
		break;
	case MDOC_Oo:
	case MDOC_Op:
		print_text(h, "\\(lB");
		h->flags |= HTML_NOSPACE;
		print_otag(h, TAG_SPAN, "c", "opt");
		break;
	case MDOC_En:
		if (NULL == n->norm->Es ||
		    NULL == n->norm->Es->child)
			return 1;
		print_text(h, n->norm->Es->child->string);
		break;
	case MDOC_Do:
	case MDOC_Dq:
	case MDOC_Qo:
	case MDOC_Qq:
		print_text(h, "\\(lq");
		break;
	case MDOC_Po:
	case MDOC_Pq:
		print_text(h, "(");
		break;
	case MDOC_Ql:
		print_text(h, "\\(oq");
		h->flags |= HTML_NOSPACE;
		print_otag(h, TAG_CODE, "c", "lit");
		break;
	case MDOC_So:
	case MDOC_Sq:
		print_text(h, "\\(oq");
		break;
	default:
		abort();
	}

	h->flags |= HTML_NOSPACE;
	return 1;
}

static void
mdoc_quote_post(MDOC_ARGS)
{

	if (n->type != ROFFT_BODY && n->type != ROFFT_ELEM)
		return;

	h->flags |= HTML_NOSPACE;

	switch (n->tok) {
	case MDOC_Ao:
	case MDOC_Aq:
		print_text(h, n->child != NULL && n->child->next == NULL &&
		    n->child->tok == MDOC_Mt ?  ">" : "\\(ra");
		break;
	case MDOC_Bro:
	case MDOC_Brq:
		print_text(h, "\\(rC");
		break;
	case MDOC_Oo:
	case MDOC_Op:
	case MDOC_Bo:
	case MDOC_Bq:
		print_text(h, "\\(rB");
		break;
	case MDOC_En:
		if (n->norm->Es == NULL ||
		    n->norm->Es->child == NULL ||
		    n->norm->Es->child->next == NULL)
			h->flags &= ~HTML_NOSPACE;
		else
			print_text(h, n->norm->Es->child->next->string);
		break;
	case MDOC_Qo:
	case MDOC_Qq:
	case MDOC_Do:
	case MDOC_Dq:
		print_text(h, "\\(rq");
		break;
	case MDOC_Po:
	case MDOC_Pq:
		print_text(h, ")");
		break;
	case MDOC_Ql:
	case MDOC_So:
	case MDOC_Sq:
		print_text(h, "\\(cq");
		break;
	default:
		abort();
	}
}

static int
mdoc_eo_pre(MDOC_ARGS)
{

	if (n->type != ROFFT_BODY)
		return 1;

	if (n->end == ENDBODY_NOT &&
	    n->parent->head->child == NULL &&
	    n->child != NULL &&
	    n->child->end != ENDBODY_NOT)
		print_text(h, "\\&");
	else if (n->end != ENDBODY_NOT ? n->child != NULL :
	    n->parent->head->child != NULL && (n->child != NULL ||
	    (n->parent->tail != NULL && n->parent->tail->child != NULL)))
		h->flags |= HTML_NOSPACE;
	return 1;
}

static void
mdoc_eo_post(MDOC_ARGS)
{
	int	 body, tail;

	if (n->type != ROFFT_BODY)
		return;

	if (n->end != ENDBODY_NOT) {
		h->flags &= ~HTML_NOSPACE;
		return;
	}

	body = n->child != NULL || n->parent->head->child != NULL;
	tail = n->parent->tail != NULL && n->parent->tail->child != NULL;

	if (body && tail)
		h->flags |= HTML_NOSPACE;
	else if ( ! tail)
		h->flags &= ~HTML_NOSPACE;
}
