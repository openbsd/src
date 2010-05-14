/*	$Id: mdoc_validate.c,v 1.51 2010/05/14 14:47:44 schwarze Exp $ */
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
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "libmdoc.h"
#include "libmandoc.h"

/* FIXME: .Bl -diag can't have non-text children in HEAD. */
/* TODO: ignoring Pp (it's superfluous in some invocations). */

#define	PRE_ARGS  struct mdoc *mdoc, const struct mdoc_node *n
#define	POST_ARGS struct mdoc *mdoc

typedef	int	(*v_pre)(PRE_ARGS);
typedef	int	(*v_post)(POST_ARGS);

struct	valids {
	v_pre	*pre;
	v_post	*post;
};

static	int	 check_parent(PRE_ARGS, enum mdoct, enum mdoc_type);
static	int	 check_msec(PRE_ARGS, ...);
static	int	 check_sec(PRE_ARGS, ...);
static	int	 check_stdarg(PRE_ARGS);
static	int	 check_text(struct mdoc *, int, int, const char *);
static	int	 check_argv(struct mdoc *, 
			const struct mdoc_node *,
			const struct mdoc_argv *);
static	int	 check_args(struct mdoc *, 
			const struct mdoc_node *);
static	int	 err_child_lt(struct mdoc *, const char *, int);
static	int	 warn_child_lt(struct mdoc *, const char *, int);
static	int	 err_child_gt(struct mdoc *, const char *, int);
static	int	 warn_child_gt(struct mdoc *, const char *, int);
static	int	 err_child_eq(struct mdoc *, const char *, int);
static	int	 warn_child_eq(struct mdoc *, const char *, int);
static	int	 warn_count(struct mdoc *, const char *, 
			int, const char *, int);
static	int	 err_count(struct mdoc *, const char *, 
			int, const char *, int);

static	int	 berr_ge1(POST_ARGS);
static	int	 bwarn_ge1(POST_ARGS);
static	int	 ebool(POST_ARGS);
static	int	 eerr_eq0(POST_ARGS);
static	int	 eerr_eq1(POST_ARGS);
static	int	 eerr_ge1(POST_ARGS);
static	int	 eerr_le1(POST_ARGS);
static	int	 ewarn_ge1(POST_ARGS);
static	int	 herr_eq0(POST_ARGS);
static	int	 herr_ge1(POST_ARGS);
static	int	 hwarn_eq1(POST_ARGS);
static	int	 hwarn_eq0(POST_ARGS);
static	int	 hwarn_le1(POST_ARGS);

static	int	 post_an(POST_ARGS);
static	int	 post_at(POST_ARGS);
static	int	 post_bf(POST_ARGS);
static	int	 post_bl(POST_ARGS);
static	int	 post_bl_head(POST_ARGS);
static	int	 post_it(POST_ARGS);
static	int	 post_lb(POST_ARGS);
static	int	 post_nm(POST_ARGS);
static	int	 post_root(POST_ARGS);
static	int	 post_rs(POST_ARGS);
static	int	 post_sh(POST_ARGS);
static	int	 post_sh_body(POST_ARGS);
static	int	 post_sh_head(POST_ARGS);
static	int	 post_st(POST_ARGS);
static	int	 post_vt(POST_ARGS);
static	int	 pre_an(PRE_ARGS);
static	int	 pre_bd(PRE_ARGS);
static	int	 pre_bl(PRE_ARGS);
static	int	 pre_dd(PRE_ARGS);
static	int	 pre_display(PRE_ARGS);
static	int	 pre_dt(PRE_ARGS);
static	int	 pre_ex(PRE_ARGS);
static	int	 pre_fd(PRE_ARGS);
static	int	 pre_it(PRE_ARGS);
static	int	 pre_lb(PRE_ARGS);
static	int	 pre_os(PRE_ARGS);
static	int	 pre_rv(PRE_ARGS);
static	int	 pre_sh(PRE_ARGS);
static	int	 pre_ss(PRE_ARGS);

static	v_post	 posts_an[] = { post_an, NULL };
static	v_post	 posts_at[] = { post_at, NULL };
static	v_post	 posts_bd[] = { hwarn_eq0, bwarn_ge1, NULL };
static	v_post	 posts_bf[] = { hwarn_le1, post_bf, NULL };
static	v_post	 posts_bl[] = { bwarn_ge1, post_bl, NULL };
static	v_post	 posts_bool[] = { eerr_eq1, ebool, NULL };
static	v_post	 posts_fo[] = { hwarn_eq1, bwarn_ge1, NULL };
static	v_post	 posts_it[] = { post_it, NULL };
static	v_post	 posts_lb[] = { eerr_eq1, post_lb, NULL };
static	v_post	 posts_nd[] = { berr_ge1, NULL };
static	v_post	 posts_nm[] = { post_nm, NULL };
static	v_post	 posts_notext[] = { eerr_eq0, NULL };
static	v_post	 posts_rs[] = { berr_ge1, herr_eq0, post_rs, NULL };
static	v_post	 posts_sh[] = { herr_ge1, bwarn_ge1, post_sh, NULL };
static	v_post	 posts_sp[] = { eerr_le1, NULL };
static	v_post	 posts_ss[] = { herr_ge1, NULL };
static	v_post	 posts_st[] = { eerr_eq1, post_st, NULL };
static	v_post	 posts_text[] = { eerr_ge1, NULL };
static	v_post	 posts_text1[] = { eerr_eq1, NULL };
static	v_post	 posts_vt[] = { post_vt, NULL };
static	v_post	 posts_wline[] = { bwarn_ge1, herr_eq0, NULL };
static	v_post	 posts_wtext[] = { ewarn_ge1, NULL };
static	v_post	 posts_xr[] = { ewarn_ge1, NULL };
static	v_pre	 pres_an[] = { pre_an, NULL };
static	v_pre	 pres_bd[] = { pre_display, pre_bd, NULL };
static	v_pre	 pres_bl[] = { pre_bl, NULL };
static	v_pre	 pres_d1[] = { pre_display, NULL };
static	v_pre	 pres_dd[] = { pre_dd, NULL };
static	v_pre	 pres_dt[] = { pre_dt, NULL };
static	v_pre	 pres_er[] = { NULL, NULL };
static	v_pre	 pres_ex[] = { pre_ex, NULL };
static	v_pre	 pres_fd[] = { pre_fd, NULL };
static	v_pre	 pres_it[] = { pre_it, NULL };
static	v_pre	 pres_lb[] = { pre_lb, NULL };
static	v_pre	 pres_os[] = { pre_os, NULL };
static	v_pre	 pres_rv[] = { pre_rv, NULL };
static	v_pre	 pres_sh[] = { pre_sh, NULL };
static	v_pre	 pres_ss[] = { pre_ss, NULL };

const	struct valids mdoc_valids[MDOC_MAX] = {
	{ NULL, NULL },				/* Ap */
	{ pres_dd, posts_text },		/* Dd */
	{ pres_dt, NULL },			/* Dt */
	{ pres_os, NULL },			/* Os */
	{ pres_sh, posts_sh },			/* Sh */ 
	{ pres_ss, posts_ss },			/* Ss */ 
	{ NULL, posts_notext },			/* Pp */ 
	{ pres_d1, posts_wline },		/* D1 */
	{ pres_d1, posts_wline },		/* Dl */
	{ pres_bd, posts_bd },			/* Bd */
	{ NULL, NULL },				/* Ed */
	{ pres_bl, posts_bl },			/* Bl */ 
	{ NULL, NULL },				/* El */
	{ pres_it, posts_it },			/* It */
	{ NULL, posts_text },			/* Ad */ 
	{ pres_an, posts_an },			/* An */ 
	{ NULL, NULL },				/* Ar */
	{ NULL, posts_text },			/* Cd */ 
	{ NULL, NULL },				/* Cm */
	{ NULL, NULL },				/* Dv */ 
	{ pres_er, posts_text },		/* Er */ 
	{ NULL, NULL },				/* Ev */ 
	{ pres_ex, NULL },			/* Ex */ 
	{ NULL, NULL },				/* Fa */ 
	{ pres_fd, posts_wtext },		/* Fd */
	{ NULL, NULL },				/* Fl */
	{ NULL, posts_text },			/* Fn */ 
	{ NULL, posts_wtext },			/* Ft */ 
	{ NULL, posts_text },			/* Ic */ 
	{ NULL, posts_text1 },			/* In */ 
	{ NULL, NULL },				/* Li */
	{ NULL, posts_nd },			/* Nd */
	{ NULL, posts_nm },			/* Nm */
	{ NULL, posts_wline },			/* Op */
	{ NULL, NULL },				/* Ot */
	{ NULL, NULL },				/* Pa */
	{ pres_rv, NULL },			/* Rv */
	{ NULL, posts_st },			/* St */ 
	{ NULL, NULL },				/* Va */
	{ NULL, posts_vt },			/* Vt */ 
	{ NULL, posts_xr },			/* Xr */ 
	{ NULL, posts_text },			/* %A */
	{ NULL, posts_text },			/* %B */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, posts_text },			/* %D */ /* FIXME: check date with mandoc_a2time(). */
	{ NULL, posts_text },			/* %I */
	{ NULL, posts_text },			/* %J */
	{ NULL, posts_text },			/* %N */
	{ NULL, posts_text },			/* %O */
	{ NULL, posts_text },			/* %P */
	{ NULL, posts_text },			/* %R */
	{ NULL, posts_text },			/* %T */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, posts_text },			/* %V */
	{ NULL, NULL },				/* Ac */
	{ NULL, NULL },				/* Ao */
	{ NULL, posts_wline },			/* Aq */
	{ NULL, posts_at },			/* At */ 
	{ NULL, NULL },				/* Bc */
	{ NULL, posts_bf },			/* Bf */
	{ NULL, NULL },				/* Bo */
	{ NULL, posts_wline },			/* Bq */
	{ NULL, NULL },				/* Bsx */
	{ NULL, NULL },				/* Bx */
	{ NULL, posts_bool },			/* Db */
	{ NULL, NULL },				/* Dc */
	{ NULL, NULL },				/* Do */
	{ NULL, posts_wline },			/* Dq */
	{ NULL, NULL },				/* Ec */
	{ NULL, NULL },				/* Ef */ 
	{ NULL, NULL },				/* Em */ 
	{ NULL, NULL },				/* Eo */
	{ NULL, NULL },				/* Fx */
	{ NULL, posts_text },			/* Ms */ 
	{ NULL, posts_notext },			/* No */
	{ NULL, posts_notext },			/* Ns */
	{ NULL, NULL },				/* Nx */
	{ NULL, NULL },				/* Ox */
	{ NULL, NULL },				/* Pc */
	{ NULL, posts_text1 },			/* Pf */
	{ NULL, NULL },				/* Po */
	{ NULL, posts_wline },			/* Pq */
	{ NULL, NULL },				/* Qc */
	{ NULL, posts_wline },			/* Ql */
	{ NULL, NULL },				/* Qo */
	{ NULL, posts_wline },			/* Qq */
	{ NULL, NULL },				/* Re */
	{ NULL, posts_rs },			/* Rs */
	{ NULL, NULL },				/* Sc */
	{ NULL, NULL },				/* So */
	{ NULL, posts_wline },			/* Sq */
	{ NULL, posts_bool },			/* Sm */ 
	{ NULL, posts_text },			/* Sx */
	{ NULL, posts_text },			/* Sy */
	{ NULL, posts_text },			/* Tn */
	{ NULL, NULL },				/* Ux */
	{ NULL, NULL },				/* Xc */
	{ NULL, NULL },				/* Xo */
	{ NULL, posts_fo },			/* Fo */ 
	{ NULL, NULL },				/* Fc */ 
	{ NULL, NULL },				/* Oo */
	{ NULL, NULL },				/* Oc */
	{ NULL, posts_wline },			/* Bk */
	{ NULL, NULL },				/* Ek */
	{ NULL, posts_notext },			/* Bt */
	{ NULL, NULL },				/* Hf */
	{ NULL, NULL },				/* Fr */
	{ NULL, posts_notext },			/* Ud */
	{ pres_lb, posts_lb },			/* Lb */
	{ NULL, posts_notext },			/* Lp */ 
	{ NULL, posts_text },			/* Lk */ 
	{ NULL, posts_text },			/* Mt */ 
	{ NULL, posts_wline },			/* Brq */ 
	{ NULL, NULL },				/* Bro */ 
	{ NULL, NULL },				/* Brc */ 
	{ NULL, posts_text },			/* %C */
	{ NULL, NULL },				/* Es */
	{ NULL, NULL },				/* En */
	{ NULL, NULL },				/* Dx */
	{ NULL, posts_text },			/* %Q */
	{ NULL, posts_notext },			/* br */
	{ NULL, posts_sp },			/* sp */
	{ NULL, posts_text1 },			/* %U */
	{ NULL, NULL },				/* eos */
};


int
mdoc_valid_pre(struct mdoc *mdoc, const struct mdoc_node *n)
{
	v_pre		*p;
	int		 line, pos;
	const char	*tp;

	if (MDOC_TEXT == n->type) {
		tp = n->string;
		line = n->line;
		pos = n->pos;
		return(check_text(mdoc, line, pos, tp));
	}

	if ( ! check_args(mdoc, n))
		return(0);
	if (NULL == mdoc_valids[n->tok].pre)
		return(1);
	for (p = mdoc_valids[n->tok].pre; *p; p++)
		if ( ! (*p)(mdoc, n)) 
			return(0);
	return(1);
}


int
mdoc_valid_post(struct mdoc *mdoc)
{
	v_post		*p;

	if (MDOC_VALID & mdoc->last->flags)
		return(1);
	mdoc->last->flags |= MDOC_VALID;

	if (MDOC_TEXT == mdoc->last->type)
		return(1);
	if (MDOC_ROOT == mdoc->last->type)
		return(post_root(mdoc));

	if (NULL == mdoc_valids[mdoc->last->tok].post)
		return(1);
	for (p = mdoc_valids[mdoc->last->tok].post; *p; p++)
		if ( ! (*p)(mdoc)) 
			return(0);

	return(1);
}


static inline int
warn_count(struct mdoc *m, const char *k, 
		int want, const char *v, int has)
{

	return(mdoc_vwarn(m, m->last->line, m->last->pos, 
		"suggests %s %s %d (has %d)", v, k, want, has));
}


static inline int
err_count(struct mdoc *m, const char *k,
		int want, const char *v, int has)
{

	return(mdoc_verr(m, m->last->line, m->last->pos,
		"requires %s %s %d (has %d)", v, k, want, has));
}


/*
 * Build these up with macros because they're basically the same check
 * for different inequalities.  Yes, this could be done with functions,
 * but this is reasonable for now.
 */

#define CHECK_CHILD_DEFN(lvl, name, ineq) 			\
static int 							\
lvl##_child_##name(struct mdoc *mdoc, const char *p, int sz) 	\
{ 								\
	if (mdoc->last->nchild ineq sz)				\
		return(1); 					\
	return(lvl##_count(mdoc, #ineq, sz, p, mdoc->last->nchild)); \
}

#define CHECK_BODY_DEFN(name, lvl, func, num) 			\
static int 							\
b##lvl##_##name(POST_ARGS) 					\
{ 								\
	if (MDOC_BODY != mdoc->last->type) 			\
		return(1); 					\
	return(func(mdoc, "multi-line arguments", (num))); 	\
}

#define CHECK_ELEM_DEFN(name, lvl, func, num) 			\
static int							\
e##lvl##_##name(POST_ARGS) 					\
{ 								\
	assert(MDOC_ELEM == mdoc->last->type); 			\
	return(func(mdoc, "line arguments", (num))); 		\
}

#define CHECK_HEAD_DEFN(name, lvl, func, num)			\
static int 							\
h##lvl##_##name(POST_ARGS) 					\
{ 								\
	if (MDOC_HEAD != mdoc->last->type) 			\
		return(1); 					\
	return(func(mdoc, "line arguments", (num)));	 	\
}


CHECK_CHILD_DEFN(warn, gt, >)			/* warn_child_gt() */
CHECK_CHILD_DEFN(err, gt, >)			/* err_child_gt() */
CHECK_CHILD_DEFN(warn, eq, ==)			/* warn_child_eq() */
CHECK_CHILD_DEFN(err, eq, ==)			/* err_child_eq() */
CHECK_CHILD_DEFN(err, lt, <)			/* err_child_lt() */
CHECK_CHILD_DEFN(warn, lt, <)			/* warn_child_lt() */
CHECK_BODY_DEFN(ge1, warn, warn_child_gt, 0)	/* bwarn_ge1() */
CHECK_BODY_DEFN(ge1, err, err_child_gt, 0)	/* berr_ge1() */
CHECK_ELEM_DEFN(ge1, warn, warn_child_gt, 0)	/* ewarn_gt1() */
CHECK_ELEM_DEFN(eq1, err, err_child_eq, 1)	/* eerr_eq1() */
CHECK_ELEM_DEFN(le1, err, err_child_lt, 2)	/* eerr_le1() */
CHECK_ELEM_DEFN(eq0, err, err_child_eq, 0)	/* eerr_eq0() */
CHECK_ELEM_DEFN(ge1, err, err_child_gt, 0)	/* eerr_ge1() */
CHECK_HEAD_DEFN(eq0, err, err_child_eq, 0)	/* herr_eq0() */
CHECK_HEAD_DEFN(le1, warn, warn_child_lt, 2)	/* hwarn_le1() */
CHECK_HEAD_DEFN(ge1, err, err_child_gt, 0)	/* herr_ge1() */
CHECK_HEAD_DEFN(eq1, warn, warn_child_eq, 1)	/* hwarn_eq1() */
CHECK_HEAD_DEFN(eq0, warn, warn_child_eq, 0)	/* hwarn_eq0() */


static int
check_stdarg(PRE_ARGS)
{

	if (n->args && 1 == n->args->argc)
		if (MDOC_Std == n->args->argv[0].arg)
			return(1);
	return(mdoc_nwarn(mdoc, n, EARGVAL));
}


static int
check_sec(PRE_ARGS, ...)
{
	enum mdoc_sec	 sec;
	va_list		 ap;

	va_start(ap, n);

	for (;;) {
		/* LINTED */
		sec = (enum mdoc_sec)va_arg(ap, int);
		if (SEC_CUSTOM == sec)
			break;
		if (sec != mdoc->lastsec)
			continue;
		va_end(ap);
		return(1);
	}

	va_end(ap);
	return(mdoc_nwarn(mdoc, n, EBADSEC));
}


static int
check_msec(PRE_ARGS, ...)
{
	va_list		 ap;
	int		 msec;

	va_start(ap, n);
	for (;;) {
		/* LINTED */
		if (0 == (msec = va_arg(ap, int)))
			break;
		if (msec != mdoc->meta.msec)
			continue;
		va_end(ap);
		return(1);
	}

	va_end(ap);
	return(mdoc_nwarn(mdoc, n, EBADMSEC));
}


static int
check_args(struct mdoc *m, const struct mdoc_node *n)
{
	int		 i;

	if (NULL == n->args)
		return(1);

	assert(n->args->argc);
	for (i = 0; i < (int)n->args->argc; i++)
		if ( ! check_argv(m, n, &n->args->argv[i]))
			return(0);

	return(1);
}


static int
check_argv(struct mdoc *m, const struct mdoc_node *n, 
		const struct mdoc_argv *v)
{
	int		 i;

	for (i = 0; i < (int)v->sz; i++)
		if ( ! check_text(m, v->line, v->pos, v->value[i]))
			return(0);

	if (MDOC_Std == v->arg) {
		/* `Nm' name must be set. */
		if (v->sz || m->meta.name)
			return(1);
		return(mdoc_nerr(m, n, ENAME));
	}

	return(1);
}


static int
check_text(struct mdoc *mdoc, int line, int pos, const char *p)
{
	int		 c;

	for ( ; *p; p++, pos++) {
		if ('\t' == *p) {
			if ( ! (MDOC_LITERAL & mdoc->flags))
				if ( ! mdoc_pwarn(mdoc, line, pos, EPRINT))
					return(0);
		} else if ( ! isprint((u_char)*p))
			if ( ! mdoc_pwarn(mdoc, line, pos, EPRINT))
				return(0);

		if ('\\' != *p)
			continue;

		c = mandoc_special(p);
		if (c) {
			p += c - 1;
			pos += c - 1;
			continue;
		}
		if ( ! (MDOC_IGN_ESCAPE & mdoc->pflags))
			return(mdoc_perr(mdoc, line, pos, EESCAPE));
		if ( ! mdoc_pwarn(mdoc, line, pos, EESCAPE))
			return(0);
	}

	return(1);
}




static int
check_parent(PRE_ARGS, enum mdoct tok, enum mdoc_type t)
{

	assert(n->parent);
	if ((MDOC_ROOT == t || tok == n->parent->tok) &&
			(t == n->parent->type))
		return(1);

	return(mdoc_verr(mdoc, n->line, n->pos, "require parent %s",
		MDOC_ROOT == t ? "<root>" : mdoc_macronames[tok]));
}



static int
pre_display(PRE_ARGS)
{
	struct mdoc_node *node;

	/* Display elements (`Bd', `D1'...) cannot be nested. */

	if (MDOC_BLOCK != n->type)
		return(1);

	/* LINTED */
	for (node = mdoc->last->parent; node; node = node->parent) 
		if (MDOC_BLOCK == node->type)
			if (MDOC_Bd == node->tok)
				break;
	if (NULL == node)
		return(1);

	return(mdoc_nerr(mdoc, n, ENESTDISP));
}


static int
pre_bl(PRE_ARGS)
{
	int		 pos, type, width, offset;

	if (MDOC_BLOCK != n->type)
		return(1);
	if (NULL == n->args)
		return(mdoc_nerr(mdoc, n, ELISTTYPE));

	/* Make sure that only one type of list is specified.  */

	type = offset = width = -1;

	/* LINTED */
	for (pos = 0; pos < (int)n->args->argc; pos++)
		switch (n->args->argv[pos].arg) {
		case (MDOC_Bullet):
			/* FALLTHROUGH */
		case (MDOC_Dash):
			/* FALLTHROUGH */
		case (MDOC_Enum):
			/* FALLTHROUGH */
		case (MDOC_Hyphen):
			/* FALLTHROUGH */
		case (MDOC_Item):
			/* FALLTHROUGH */
		case (MDOC_Tag):
			/* FALLTHROUGH */
		case (MDOC_Diag):
			/* FALLTHROUGH */
		case (MDOC_Hang):
			/* FALLTHROUGH */
		case (MDOC_Ohang):
			/* FALLTHROUGH */
		case (MDOC_Inset):
			/* FALLTHROUGH */
		case (MDOC_Column):
			/*
			 * Note that if a duplicate is detected, we
			 * remove the duplicate instead of passing it
			 * over.  If we don't do this, mdoc_action will
			 * become confused when it scans over multiple
			 * types whilst setting its bitmasks.
			 *
			 * FIXME: this should occur in mdoc_action.c.
			 */
			if (type >= 0) {
				if ( ! mdoc_nwarn(mdoc, n, EMULTILIST))
					return(0);
				mdoc_argn_free(n->args, pos);
				break;
			}
			type = n->args->argv[pos].arg;
			break;
		case (MDOC_Compact):
			if (type < 0 && ! mdoc_nwarn(mdoc, n, ENOTYPE))
				return(0);
			break;
		case (MDOC_Width):
			if (width >= 0)
				return(mdoc_nerr(mdoc, n, EARGREP));
			if (type < 0 && ! mdoc_nwarn(mdoc, n, ENOTYPE))
				return(0);
			width = n->args->argv[pos].arg;
			break;
		case (MDOC_Offset):
			if (offset >= 0)
				return(mdoc_nerr(mdoc, n, EARGREP));
			if (type < 0 && ! mdoc_nwarn(mdoc, n, ENOTYPE))
				return(0);
			offset = n->args->argv[pos].arg;
			break;
		default:
			break;
		}

	if (type < 0)
		return(mdoc_nerr(mdoc, n, ELISTTYPE));

	/* 
	 * Validate the width field.  Some list types don't need width
	 * types and should be warned about them.  Others should have it
	 * and must also be warned.
	 */

	switch (type) {
	case (MDOC_Tag):
		if (width < 0 && ! mdoc_nwarn(mdoc, n, EMISSWIDTH))
			return(0);
		break;
	case (MDOC_Column):
		/* FALLTHROUGH */
	case (MDOC_Diag):
		/* FALLTHROUGH */
	case (MDOC_Ohang):
		/* FALLTHROUGH */
	case (MDOC_Inset):
		/* FALLTHROUGH */
	case (MDOC_Item):
		if (width >= 0 && ! mdoc_nwarn(mdoc, n, ENOWIDTH))
			return(0);
		break;
	default:
		break;
	}

	return(1);
}


static int
pre_bd(PRE_ARGS)
{
	int		 i, type, err;

	if (MDOC_BLOCK != n->type)
		return(1);
	if (NULL == n->args) 
		return(mdoc_nerr(mdoc, n, EDISPTYPE));

	/* Make sure that only one type of display is specified.  */

	/* LINTED */
	for (i = 0, err = type = 0; ! err && 
			i < (int)n->args->argc; i++)
		switch (n->args->argv[i].arg) {
		case (MDOC_Centred):
			/* FALLTHROUGH */
		case (MDOC_Ragged):
			/* FALLTHROUGH */
		case (MDOC_Unfilled):
			/* FALLTHROUGH */
		case (MDOC_Filled):
			/* FALLTHROUGH */
		case (MDOC_Literal):
			if (0 == type++) 
				break;
			return(mdoc_nerr(mdoc, n, EMULTIDISP));
		default:
			break;
		}

	if (type)
		return(1);
	return(mdoc_nerr(mdoc, n, EDISPTYPE));
}


static int
pre_ss(PRE_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);
	return(check_parent(mdoc, n, MDOC_Sh, MDOC_BODY));
}


static int
pre_sh(PRE_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);
	return(check_parent(mdoc, n, MDOC_MAX, MDOC_ROOT));
}


static int
pre_it(PRE_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);
	return(check_parent(mdoc, n, MDOC_Bl, MDOC_BODY));
}


static int
pre_an(PRE_ARGS)
{

	if (NULL == n->args || 1 == n->args->argc)
		return(1);
	return(mdoc_verr(mdoc, n->line, n->pos, 
				"only one argument allowed"));
}


static int
pre_lb(PRE_ARGS)
{

	return(check_sec(mdoc, n, SEC_LIBRARY, SEC_CUSTOM));
}


static int
pre_rv(PRE_ARGS)
{

	return(check_stdarg(mdoc, n));
}


static int
pre_ex(PRE_ARGS)
{

	if ( ! check_msec(mdoc, n, 1, 6, 8, 0))
		return(0);
	return(check_stdarg(mdoc, n));
}


static int
pre_dt(PRE_ARGS)
{

	/* FIXME: make sure is capitalised. */

	if (0 == mdoc->meta.date || mdoc->meta.os)
		if ( ! mdoc_nwarn(mdoc, n, EPROLOOO))
			return(0);
	if (mdoc->meta.title)
		if ( ! mdoc_nwarn(mdoc, n, EPROLREP))
			return(0);
	return(1);
}


static int
pre_os(PRE_ARGS)
{

	if (NULL == mdoc->meta.title || 0 == mdoc->meta.date)
		if ( ! mdoc_nwarn(mdoc, n, EPROLOOO))
			return(0);
	if (mdoc->meta.os)
		if ( ! mdoc_nwarn(mdoc, n, EPROLREP))
			return(0);
	return(1);
}


static int
pre_dd(PRE_ARGS)
{

	if (mdoc->meta.title || mdoc->meta.os)
		if ( ! mdoc_nwarn(mdoc, n, EPROLOOO))
			return(0);
	if (mdoc->meta.date)
		if ( ! mdoc_nwarn(mdoc, n, EPROLREP))
			return(0);
	return(1);
}


static int
post_bf(POST_ARGS)
{
	char		 *p;
	struct mdoc_node *head;

	if (MDOC_BLOCK != mdoc->last->type)
		return(1);

	head = mdoc->last->head;

	if (mdoc->last->args && head->child)
		return(mdoc_nerr(mdoc, mdoc->last, ELINE));
	else if (mdoc->last->args)
		return(1);

	if (NULL == head->child || MDOC_TEXT != head->child->type)
		return(mdoc_nerr(mdoc, mdoc->last, ELINE));

	p = head->child->string;

	if (0 == strcmp(p, "Em"))
		return(1);
	else if (0 == strcmp(p, "Li"))
		return(1);
	else if (0 == strcmp(p, "Sy"))
		return(1);

	return(mdoc_nerr(mdoc, head, EFONT));
}


static int
post_lb(POST_ARGS)
{

	if (mdoc_a2lib(mdoc->last->child->string))
		return(1);
	return(mdoc_nwarn(mdoc, mdoc->last, ELIB));
}


static int
post_vt(POST_ARGS)
{
	const struct mdoc_node *n;

	/*
	 * The Vt macro comes in both ELEM and BLOCK form, both of which
	 * have different syntaxes (yet more context-sensitive
	 * behaviour).  ELEM types must have a child; BLOCK types,
	 * specifically the BODY, should only have TEXT children.
	 */

	if (MDOC_ELEM == mdoc->last->type)
		return(eerr_ge1(mdoc));
	if (MDOC_BODY != mdoc->last->type)
		return(1);
	
	for (n = mdoc->last->child; n; n = n->next)
		if (MDOC_TEXT != n->type &&
		    (MDOC_ELEM != n->type || MDOC_eos != n->tok)) 
			if ( ! mdoc_nwarn(mdoc, n, EBADCHILD))
				return(0);

	return(1);
}


static int
post_nm(POST_ARGS)
{

	if (mdoc->last->child)
		return(1);
	if (mdoc->meta.name)
		return(1);
	return(mdoc_nerr(mdoc, mdoc->last, ENAME));
}


static int
post_at(POST_ARGS)
{

	if (NULL == mdoc->last->child)
		return(1);
	if (MDOC_TEXT != mdoc->last->child->type)
		return(mdoc_nerr(mdoc, mdoc->last, EATT));
	if (mdoc_a2att(mdoc->last->child->string))
		return(1);
	return(mdoc_nerr(mdoc, mdoc->last, EATT));
}


static int
post_an(POST_ARGS)
{

	if (mdoc->last->args) {
		if (NULL == mdoc->last->child)
			return(1);
		return(mdoc_nerr(mdoc, mdoc->last, ENOLINE));
	}

	if (mdoc->last->child)
		return(1);
	return(mdoc_nerr(mdoc, mdoc->last, ELINE));
}


static int
post_it(POST_ARGS)
{
	int		  type, i, cols;
	struct mdoc_node *n, *c;

	if (MDOC_BLOCK != mdoc->last->type)
		return(1);

	n = mdoc->last->parent->parent;
	if (NULL == n->args)
		return(mdoc_nerr(mdoc, mdoc->last, ELISTTYPE));

	/* Some types require block-head, some not. */

	/* LINTED */
	for (cols = type = -1, i = 0; -1 == type && 
			i < (int)n->args->argc; i++)
		switch (n->args->argv[i].arg) {
		case (MDOC_Tag):
			/* FALLTHROUGH */
		case (MDOC_Diag):
			/* FALLTHROUGH */
		case (MDOC_Hang):
			/* FALLTHROUGH */
		case (MDOC_Ohang):
			/* FALLTHROUGH */
		case (MDOC_Inset):
			/* FALLTHROUGH */
		case (MDOC_Bullet):
			/* FALLTHROUGH */
		case (MDOC_Dash):
			/* FALLTHROUGH */
		case (MDOC_Enum):
			/* FALLTHROUGH */
		case (MDOC_Hyphen):
			/* FALLTHROUGH */
		case (MDOC_Item):
			type = n->args->argv[i].arg;
			break;
		case (MDOC_Column):
			type = n->args->argv[i].arg;
			cols = (int)n->args->argv[i].sz;
			break;
		default:
			break;
		}

	if (-1 == type)
		return(mdoc_nerr(mdoc, mdoc->last, ELISTTYPE));

	switch (type) {
	case (MDOC_Tag):
		if (NULL == mdoc->last->head->child)
			if ( ! mdoc_nwarn(mdoc, mdoc->last, ELINE))
				return(0);
		break;
	case (MDOC_Hang):
		/* FALLTHROUGH */
	case (MDOC_Ohang):
		/* FALLTHROUGH */
	case (MDOC_Inset):
		/* FALLTHROUGH */
	case (MDOC_Diag):
		if (NULL == mdoc->last->head->child)
			if ( ! mdoc_nwarn(mdoc, mdoc->last, ELINE))
				return(0);
		if (NULL == mdoc->last->body->child)
			if ( ! mdoc_nwarn(mdoc, mdoc->last, EMULTILINE))
				return(0);
		break;
	case (MDOC_Bullet):
		/* FALLTHROUGH */
	case (MDOC_Dash):
		/* FALLTHROUGH */
	case (MDOC_Enum):
		/* FALLTHROUGH */
	case (MDOC_Hyphen):
		/* FALLTHROUGH */
	case (MDOC_Item):
		if (mdoc->last->head->child)
			if ( ! mdoc_nwarn(mdoc, mdoc->last, ENOLINE))
				return(0);
		if (NULL == mdoc->last->body->child)
			if ( ! mdoc_nwarn(mdoc, mdoc->last, EMULTILINE))
				return(0);
		break;
	case (MDOC_Column):
		if (NULL == mdoc->last->head->child)
			if ( ! mdoc_nwarn(mdoc, mdoc->last, ELINE))
				return(0);
		if (mdoc->last->body->child)
			if ( ! mdoc_nwarn(mdoc, mdoc->last, ENOMULTILINE))
				return(0);
		c = mdoc->last->child;
		for (i = 0; c && MDOC_HEAD == c->type; c = c->next)
			i++;

		if (i < cols || i == (cols + 1)) {
			if ( ! mdoc_vwarn(mdoc, mdoc->last->line, 
					mdoc->last->pos, "column "
					"mismatch: have %d, want %d", 
					i, cols))
				return(0);
			break;
		} else if (i == cols)
			break;

		return(mdoc_verr(mdoc, mdoc->last->line, 
				mdoc->last->pos, "column mismatch: "
				"have %d, want %d", i, cols));
	default:
		break;
	}

	return(1);
}


static int
post_bl_head(POST_ARGS) 
{
	int			i;
	const struct mdoc_node *n;
	const struct mdoc_argv *a;

	n = mdoc->last->parent;
	assert(n->args);

	for (i = 0; i < (int)n->args->argc; i++) {
		a = &n->args->argv[i];
		if (a->arg == MDOC_Column) {
			if (a->sz && mdoc->last->nchild)
				return(mdoc_nerr(mdoc, n, ECOLMIS));
			return(1);
		}
	}

	if (0 == (i = mdoc->last->nchild))
		return(1);
	return(warn_count(mdoc, "==", 0, "line arguments", i));
}


static int
post_bl(POST_ARGS)
{
	struct mdoc_node	*n;

	if (MDOC_HEAD == mdoc->last->type) 
		return(post_bl_head(mdoc));
	if (MDOC_BODY != mdoc->last->type)
		return(1);
	if (NULL == mdoc->last->child)
		return(1);

	/*
	 * We only allow certain children of `Bl'.  This is usually on
	 * `It', but apparently `Sm' occurs here and there, so we let
	 * that one through, too.
	 */

	/* LINTED */
	for (n = mdoc->last->child; n; n = n->next) {
		if (MDOC_BLOCK == n->type && MDOC_It == n->tok)
			continue;
		if (MDOC_Sm == n->tok)
			continue;
		return(mdoc_nerr(mdoc, n, EBADCHILD));
	}

	return(1);
}


static int
ebool(struct mdoc *mdoc)
{
	struct mdoc_node *n;

	/* LINTED */
	for (n = mdoc->last->child; n; n = n->next) {
		if (MDOC_TEXT != n->type)
			break;
		if (0 == strcmp(n->string, "on"))
			continue;
		if (0 == strcmp(n->string, "off"))
			continue;
		break;
	}

	if (NULL == n)
		return(1);
	return(mdoc_nerr(mdoc, n, EBOOL));
}


static int
post_root(POST_ARGS)
{

	if (NULL == mdoc->first->child)
		return(mdoc_nerr(mdoc, mdoc->first, ENODAT));
	if ( ! (MDOC_PBODY & mdoc->flags))
		return(mdoc_nerr(mdoc, mdoc->first, ENOPROLOGUE));

	if (MDOC_BLOCK != mdoc->first->child->type)
		return(mdoc_nerr(mdoc, mdoc->first, ENODAT));
	if (MDOC_Sh != mdoc->first->child->tok)
		return(mdoc_nerr(mdoc, mdoc->first, ENODAT));

	return(1);
}


static int
post_st(POST_ARGS)
{

	if (mdoc_a2st(mdoc->last->child->string))
		return(1);
	return(mdoc_nerr(mdoc, mdoc->last, EBADSTAND));
}


static int
post_rs(POST_ARGS)
{
	struct mdoc_node	*nn;

	if (MDOC_BODY != mdoc->last->type)
		return(1);

	for (nn = mdoc->last->child; nn; nn = nn->next)
		switch (nn->tok) {
		case(MDOC__U):
			/* FALLTHROUGH */
		case(MDOC__Q):
			/* FALLTHROUGH */
		case(MDOC__C):
			/* FALLTHROUGH */
		case(MDOC__A):
			/* FALLTHROUGH */
		case(MDOC__B):
			/* FALLTHROUGH */
		case(MDOC__D):
			/* FALLTHROUGH */
		case(MDOC__I):
			/* FALLTHROUGH */
		case(MDOC__J):
			/* FALLTHROUGH */
		case(MDOC__N):
			/* FALLTHROUGH */
		case(MDOC__O):
			/* FALLTHROUGH */
		case(MDOC__P):
			/* FALLTHROUGH */
		case(MDOC__R):
			/* FALLTHROUGH */
		case(MDOC__T):
			/* FALLTHROUGH */
		case(MDOC__V):
			break;
		default:
			return(mdoc_nerr(mdoc, nn, EBADCHILD));
		}

	return(1);
}


static int
post_sh(POST_ARGS)
{

	if (MDOC_HEAD == mdoc->last->type)
		return(post_sh_head(mdoc));
	if (MDOC_BODY == mdoc->last->type)
		return(post_sh_body(mdoc));

	return(1);
}


static int
post_sh_body(POST_ARGS)
{
	struct mdoc_node *n;

	if (SEC_NAME != mdoc->lastsec)
		return(1);

	/*
	 * Warn if the NAME section doesn't contain the `Nm' and `Nd'
	 * macros (can have multiple `Nm' and one `Nd').  Note that the
	 * children of the BODY declaration can also be "text".
	 */

	if (NULL == (n = mdoc->last->child))
		return(mdoc_nwarn(mdoc, mdoc->last, ENAMESECINC));

	for ( ; n && n->next; n = n->next) {
		if (MDOC_ELEM == n->type && MDOC_Nm == n->tok)
			continue;
		if (MDOC_TEXT == n->type)
			continue;
		if ( ! mdoc_nwarn(mdoc, mdoc->last, ENAMESECINC))
			return(0);
	}

	assert(n);
	if (MDOC_BLOCK == n->type && MDOC_Nd == n->tok)
		return(1);
	return(mdoc_nwarn(mdoc, mdoc->last, ENAMESECINC));
}


static int
post_sh_head(POST_ARGS)
{
	char		        buf[64];
	enum mdoc_sec	        sec;
	const struct mdoc_node *n;

	/*
	 * Process a new section.  Sections are either "named" or
	 * "custom"; custom sections are user-defined, while named ones
	 * usually follow a conventional order and may only appear in
	 * certain manual sections.
	 */

	buf[0] = 0;

	for (n = mdoc->last->child; n; n = n->next) {
		/* XXX - copied from compact(). */
		assert(MDOC_TEXT == n->type);

		if (strlcat(buf, n->string, 64) >= 64)
			return(mdoc_nerr(mdoc, n, ETOOLONG));
		if (NULL == n->next)
			continue;
		if (strlcat(buf, " ", 64) >= 64)
			return(mdoc_nerr(mdoc, n, ETOOLONG));
	}

	sec = mdoc_str2sec(buf);

	/* 
	 * Check: NAME should always be first, CUSTOM has no roles,
	 * non-CUSTOM has a conventional order to be followed.
	 */

	if (SEC_NAME != sec && SEC_NONE == mdoc->lastnamed)
		if ( ! mdoc_nwarn(mdoc, mdoc->last, ESECNAME))
			return(0);

	if (SEC_CUSTOM == sec)
		return(1);

	if (sec == mdoc->lastnamed)
		if ( ! mdoc_nwarn(mdoc, mdoc->last, ESECREP))
			return(0);

	if (sec < mdoc->lastnamed)
		if ( ! mdoc_nwarn(mdoc, mdoc->last, ESECOOO))
			return(0);

	/* 
	 * Check particular section/manual conventions.  LIBRARY can
	 * only occur in msec 2, 3 (TODO: are there more of these?).
	 */

	switch (sec) {
	case (SEC_LIBRARY):
		switch (mdoc->meta.msec) {
		case (2):
			/* FALLTHROUGH */
		case (3):
			break;
		default:
			return(mdoc_nwarn(mdoc, mdoc->last, EWRONGMSEC));
		}
		break;
	default:
		break;
	}

	return(1);
}


static int
pre_fd(PRE_ARGS)
{

	return(check_sec(mdoc, n, SEC_SYNOPSIS, SEC_CUSTOM));
}
