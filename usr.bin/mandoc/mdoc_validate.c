/*	$Id: mdoc_validate.c,v 1.10 2009/06/21 18:15:03 schwarze Exp $ */
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "libmdoc.h"

/* FIXME: .Bl -diag can't have non-text children in HEAD. */
/* TODO: ignoring Pp (it's superfluous in some invocations). */

#define	PRE_ARGS	struct mdoc *mdoc, const struct mdoc_node *n
#define	POST_ARGS	struct mdoc *mdoc

enum	merr {
	ETOOLONG,
	EESCAPE,
	EPRINT,
	ENODATA,
	ENOPROLOGUE,
	ELINE,
	EATT,
	ENAME,
	ELISTTYPE,
	EDISPTYPE,
	EMULTIDISP,
	ESECNAME,
	EMULTILIST,
	EARGREP,
	EBOOL,
	ENESTDISP
};

enum	mwarn {
	WPRINT,
	WNOWIDTH,
	WMISSWIDTH,
	WESCAPE,
	WDEPESC,
	WDEPCOL,
	WWRONGMSEC,
	WSECOOO,
	WSECREP,
	WBADSTAND,
	WNAMESECINC,
	WNOMULTILINE,
	WMULTILINE,
	WLINE,
	WNOLINE,
	WPROLOOO,
	WPROLREP,
	WARGVAL,
	WBADSEC,
	WBADMSEC
};

typedef	int	(*v_pre)(PRE_ARGS);
typedef	int	(*v_post)(POST_ARGS);

struct	valids {
	v_pre	*pre;
	v_post	*post;
};

static	int	pwarn(struct mdoc *, int, int, enum mwarn);
static	int	perr(struct mdoc *, int, int, enum merr);
static	int	check_parent(PRE_ARGS, int, enum mdoc_type);
static	int	check_msec(PRE_ARGS, ...);
static	int	check_sec(PRE_ARGS, ...);
static	int	check_stdarg(PRE_ARGS);
static	int	check_text(struct mdoc *, int, int, const char *);
static	int	check_argv(struct mdoc *, 
			const struct mdoc_node *,
			const struct mdoc_argv *);
static	int	check_args(struct mdoc *, 
			const struct mdoc_node *);
static	int	err_child_lt(struct mdoc *, const char *, int);
static	int	warn_child_lt(struct mdoc *, const char *, int);
static	int	err_child_gt(struct mdoc *, const char *, int);
static	int	warn_child_gt(struct mdoc *, const char *, int);
static	int	err_child_eq(struct mdoc *, const char *, int);
static	int	warn_child_eq(struct mdoc *, const char *, int);
static	int	count_child(struct mdoc *);
static	int	warn_print(struct mdoc *, int, int);
static	int	warn_count(struct mdoc *, const char *, 
			int, const char *, int);
static	int	err_count(struct mdoc *, const char *, 
			int, const char *, int);
static	int	pre_an(PRE_ARGS);
static	int	pre_bd(PRE_ARGS);
static	int	pre_bl(PRE_ARGS);
static	int	pre_cd(PRE_ARGS);
static	int	pre_dd(PRE_ARGS);
static	int	pre_display(PRE_ARGS);
static	int	pre_dt(PRE_ARGS);
static	int	pre_er(PRE_ARGS);
static	int	pre_ex(PRE_ARGS);
static	int	pre_fd(PRE_ARGS);
static	int	pre_it(PRE_ARGS);
static	int	pre_lb(PRE_ARGS);
static	int	pre_os(PRE_ARGS);
static	int	pre_rv(PRE_ARGS);
static	int	pre_sh(PRE_ARGS);
static	int	pre_ss(PRE_ARGS);
static	int	herr_ge1(POST_ARGS);
static	int	hwarn_le1(POST_ARGS);
static	int	herr_eq0(POST_ARGS);
static	int	eerr_eq0(POST_ARGS);
static	int	eerr_le2(POST_ARGS);
static	int	eerr_eq1(POST_ARGS);
static	int	eerr_ge1(POST_ARGS);
static	int	ewarn_eq0(POST_ARGS);
static	int	ewarn_eq1(POST_ARGS);
static	int	bwarn_ge1(POST_ARGS);
static	int	hwarn_eq1(POST_ARGS);
static	int	ewarn_ge1(POST_ARGS);
static	int	ebool(POST_ARGS);
static	int	post_an(POST_ARGS);
static	int	post_args(POST_ARGS);
static	int	post_at(POST_ARGS);
static	int	post_bf(POST_ARGS);
static	int	post_bl(POST_ARGS);
static	int	post_it(POST_ARGS);
static	int	post_nm(POST_ARGS);
static	int	post_root(POST_ARGS);
static	int	post_sh(POST_ARGS);
static	int	post_sh_body(POST_ARGS);
static	int	post_sh_head(POST_ARGS);
static	int	post_st(POST_ARGS);

#define	vwarn(m, t) nwarn((m), (m)->last, (t))
#define	verr(m, t) nerr((m), (m)->last, (t))
#define	nwarn(m, n, t) pwarn((m), (n)->line, (n)->pos, (t))
#define	nerr(m, n, t) perr((m), (n)->line, (n)->pos, (t))

static	v_pre	pres_an[] = { pre_an, NULL };
static	v_pre	pres_bd[] = { pre_display, pre_bd, NULL };
static	v_pre	pres_bl[] = { pre_bl, NULL };
static	v_pre	pres_cd[] = { pre_cd, NULL };
static	v_pre	pres_dd[] = { pre_dd, NULL };
static	v_pre	pres_d1[] = { pre_display, NULL };
static	v_pre	pres_dt[] = { pre_dt, NULL };
static	v_pre	pres_er[] = { pre_er, NULL };
static	v_pre	pres_ex[] = { pre_ex, NULL };
static	v_pre	pres_fd[] = { pre_fd, NULL };
static	v_pre	pres_it[] = { pre_it, NULL };
static	v_pre	pres_lb[] = { pre_lb, NULL };
static	v_pre	pres_os[] = { pre_os, NULL };
static	v_pre	pres_rv[] = { pre_rv, NULL };
static	v_pre	pres_sh[] = { pre_sh, NULL };
static	v_pre	pres_ss[] = { pre_ss, NULL };
static	v_post	posts_bool[] = { eerr_eq1, ebool, NULL };
static	v_post	posts_bd[] = { herr_eq0, bwarn_ge1, NULL };
static	v_post	posts_text[] = { eerr_ge1, NULL };
static	v_post	posts_wtext[] = { ewarn_ge1, NULL };
static	v_post	posts_notext[] = { eerr_eq0, NULL };
static	v_post	posts_wline[] = { bwarn_ge1, herr_eq0, NULL };
static	v_post	posts_sh[] = { herr_ge1, bwarn_ge1, post_sh, NULL };
static	v_post	posts_bl[] = { herr_eq0, bwarn_ge1, post_bl, NULL };
static	v_post	posts_it[] = { post_it, NULL };
static	v_post	posts_in[] = { ewarn_eq1, NULL };
static	v_post	posts_ss[] = { herr_ge1, NULL };
static	v_post	posts_pf[] = { eerr_eq1, NULL };
static	v_post	posts_lb[] = { eerr_eq1, NULL };
static	v_post	posts_st[] = { eerr_eq1, post_st, NULL };
static	v_post	posts_pp[] = { ewarn_eq0, NULL };
static	v_post	posts_ex[] = { eerr_eq0, post_args, NULL };
static	v_post	posts_rv[] = { eerr_eq0, post_args, NULL };
static	v_post	posts_an[] = { post_an, NULL };
static	v_post	posts_at[] = { post_at, NULL };
static	v_post	posts_xr[] = { eerr_ge1, eerr_le2, NULL };
static	v_post	posts_nm[] = { post_nm, NULL };
static	v_post	posts_bf[] = { hwarn_le1, post_bf, NULL };
static	v_post	posts_fo[] = { hwarn_eq1, bwarn_ge1, NULL };

const	struct valids mdoc_valids[MDOC_MAX] = {
	{ NULL, NULL },				/* Ap */
	{ pres_dd, posts_text },		/* Dd */
	{ pres_dt, NULL },			/* Dt */
	{ pres_os, NULL },			/* Os */
	{ pres_sh, posts_sh },			/* Sh */ 
	{ pres_ss, posts_ss },			/* Ss */ 
	{ NULL, posts_pp },			/* Pp */ 
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
	{ pres_cd, posts_text },		/* Cd */ 
	{ NULL, NULL },				/* Cm */
	{ NULL, NULL },				/* Dv */ 
	{ pres_er, posts_text },		/* Er */ 
	{ NULL, NULL },				/* Ev */ 
	{ pres_ex, posts_ex },			/* Ex */ 
	{ NULL, NULL },				/* Fa */ 
	{ pres_fd, posts_wtext },		/* Fd */
	{ NULL, NULL },				/* Fl */
	{ NULL, posts_text },			/* Fn */ 
	{ NULL, posts_wtext },			/* Ft */ 
	{ NULL, posts_text },			/* Ic */ 
	{ NULL, posts_in },			/* In */ 
	{ NULL, NULL },				/* Li */
	{ NULL, posts_wtext },			/* Nd */
	{ NULL, posts_nm },			/* Nm */
	{ NULL, posts_wline },			/* Op */
	{ NULL, NULL },				/* Ot */
	{ NULL, NULL },				/* Pa */
	{ pres_rv, posts_rv },			/* Rv */
	{ NULL, posts_st },			/* St */ 
	{ NULL, NULL },				/* Va */
	{ NULL, posts_text },			/* Vt */ 
	{ NULL, posts_xr },			/* Xr */ 
	{ NULL, posts_text },			/* %A */
	{ NULL, posts_text },			/* %B */
	{ NULL, posts_text },			/* %D */
	{ NULL, posts_text },			/* %I */
	{ NULL, posts_text },			/* %J */
	{ NULL, posts_text },			/* %N */
	{ NULL, posts_text },			/* %O */
	{ NULL, posts_text },			/* %P */
	{ NULL, posts_text },			/* %R */
	{ NULL, posts_text },			/* %T */
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
	{ NULL, posts_pf },			/* Pf */
	{ NULL, NULL },				/* Po */
	{ NULL, posts_wline },			/* Pq */
	{ NULL, NULL },				/* Qc */
	{ NULL, posts_wline },			/* Ql */
	{ NULL, NULL },				/* Qo */
	{ NULL, posts_wline },			/* Qq */
	{ NULL, NULL },				/* Re */
	{ NULL, posts_wline },			/* Rs */
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
	{ NULL, posts_pp },			/* Lp */ 
	{ NULL, NULL },				/* Lk */ 
	{ NULL, posts_text },			/* Mt */ 
	{ NULL, posts_wline },			/* Brq */ 
	{ NULL, NULL },				/* Bro */ 
	{ NULL, NULL },				/* Brc */ 
	{ NULL, posts_text },			/* %C */
	{ NULL, NULL },				/* Es */
	{ NULL, NULL },				/* En */
	{ NULL, NULL },				/* Dx */
	{ NULL, posts_text },			/* %Q */
};


int
mdoc_valid_pre(struct mdoc *mdoc, 
		const struct mdoc_node *n)
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

	/*
	 * This check occurs after the macro's children have been filled
	 * in: postfix validation.  Since this happens when we're
	 * rewinding the scope tree, it's possible to have multiple
	 * invocations (as by design, for now), we set bit MDOC_VALID to
	 * indicate that we've validated.
	 */

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


static int
perr(struct mdoc *m, int line, int pos, enum merr type)
{
	char		 *p;
	
	p = NULL;
	switch (type) {
	case (ETOOLONG):
		p = "text argument too long";
		break;
	case (EESCAPE):
		p = "invalid escape sequence";
		break;
	case (EPRINT):
		p = "invalid character";
		break;
	case (ENESTDISP):
		p = "displays may not be nested";
		break;
	case (EBOOL):
		p = "expected boolean value";
		break;
	case (EARGREP):
		p = "argument repeated";
		break;
	case (EMULTIDISP):
		p = "multiple display types specified";
		break;
	case (EMULTILIST):
		p = "multiple list types specified";
		break;
	case (ELISTTYPE):
		p = "missing list type";
		break;
	case (EDISPTYPE):
		p = "missing display type";
		break;
	case (ESECNAME):
		p = "the NAME section must come first";
		break;
	case (ELINE):
		p = "expected line arguments";
		break;
	case (ENOPROLOGUE):
		p = "document has no prologue";
		break;
	case (ENODATA):
		p = "document has no data";
		break;
	case (EATT):
		p = "expected valid AT&T symbol";
		break;
	case (ENAME):
		p = "default name not yet set";
		break;
	}
	assert(p);
	return(mdoc_perr(m, line, pos, p));
}


static int
pwarn(struct mdoc *m, int line, int pos, enum mwarn type)
{
	char		 *p;
	enum mdoc_warn	  c;

	c = WARN_SYNTAX;
	p = NULL;
	switch (type) {
	case (WBADMSEC):
		p = "inappropriate manual section";
		c = WARN_COMPAT;
		break;
	case (WBADSEC):
		p = "inappropriate document section";
		c = WARN_COMPAT;
		break;
	case (WARGVAL):
		p = "argument value suggested";
		c = WARN_COMPAT;
		break;
	case (WPROLREP):
		p = "prologue macros repeated";
		c = WARN_COMPAT;
		break;
	case (WPROLOOO):
		p = "prologue macros out-of-order";
		c = WARN_COMPAT;
		break;
	case (WDEPCOL):
		p = "deprecated column argument syntax";
		c = WARN_COMPAT;
		break;
	case (WNOWIDTH):
		p = "superfluous width argument";
		break;
	case (WMISSWIDTH):
		p = "missing width argument";
		break;
	case (WPRINT):
		p = "invalid character";
		break;
	case (WESCAPE):
		p = "invalid escape sequence";
		break;
	case (WDEPESC):
		p = "deprecated special-character escape";
		break;
	case (WNOLINE):
		p = "suggested no line arguments";
		break;
	case (WLINE):
		p = "suggested line arguments";
		break;
	case (WMULTILINE):
		p = "suggested multi-line arguments";
		break;
	case (WNOMULTILINE):
		p = "suggested no multi-line arguments";
		break;
	case (WWRONGMSEC):
		p = "document section in wrong manual section";
		c = WARN_COMPAT;
		break;
	case (WSECOOO):
		p = "document section out of conventional order";
		break;
	case (WSECREP):
		p = "document section repeated";
		break;
	case (WBADSTAND):
		p = "unknown standard";
		break;
	case (WNAMESECINC):
		p = "NAME section contents incomplete/badly-ordered";
		break;
	}
	assert(p);
	return(mdoc_pwarn(m, line, pos, c, p));
}


static int
warn_print(struct mdoc *m, int ln, int pos)
{
	if (MDOC_IGN_CHARS & m->pflags)
		return(pwarn(m, ln, pos, WPRINT));
	return(perr(m, ln, pos, EPRINT));
}


static inline int
warn_count(struct mdoc *m, const char *k, 
		int want, const char *v, int has)
{

	return(mdoc_warn(m, WARN_SYNTAX, 
		"suggests %s %s %d (has %d)", v, k, want, has));
}


static inline int
err_count(struct mdoc *m, const char *k,
		int want, const char *v, int has)
{

	return(mdoc_err(m, 
		"requires %s %s %d (has %d)", v, k, want, has));
}


static inline int
count_child(struct mdoc *mdoc)
{
	int		  i;
	struct mdoc_node *n;

	for (i = 0, n = mdoc->last->child; n; n = n->next, i++)
		/* Do nothing */ ;

	return(i);
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
	int i; 							\
	if ((i = count_child(mdoc)) ineq sz) 			\
		return(1); 					\
	return(lvl##_count(mdoc, #ineq, sz, p, i)); 		\
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
CHECK_ELEM_DEFN(eq1, warn, warn_child_eq, 1)	/* ewarn_eq1() */
CHECK_ELEM_DEFN(eq0, warn, warn_child_eq, 0)	/* ewarn_eq0() */
CHECK_ELEM_DEFN(ge1, warn, warn_child_gt, 0)	/* ewarn_gt1() */
CHECK_ELEM_DEFN(eq1, err, err_child_eq, 1)	/* eerr_eq1() */
CHECK_ELEM_DEFN(le2, err, err_child_lt, 3)	/* eerr_le2() */
CHECK_ELEM_DEFN(eq0, err, err_child_eq, 0)	/* eerr_eq0() */
CHECK_ELEM_DEFN(ge1, err, err_child_gt, 0)	/* eerr_ge1() */
CHECK_HEAD_DEFN(eq0, err, err_child_eq, 0)	/* herr_eq0() */
CHECK_HEAD_DEFN(le1, warn, warn_child_lt, 2)	/* hwarn_le1() */
CHECK_HEAD_DEFN(ge1, err, err_child_gt, 0)	/* herr_ge1() */
CHECK_HEAD_DEFN(eq1, warn, warn_child_eq, 1)	/* hwarn_eq1() */


static int
check_stdarg(PRE_ARGS)
{

	if (n->args && 1 == n->args->argc)
		if (MDOC_Std == n->args->argv[0].arg)
			return(1);
	return(nwarn(mdoc, n, WARGVAL));
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
	return(nwarn(mdoc, n, WBADSEC));
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
	return(nwarn(mdoc, n, WBADMSEC));
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
		return(nerr(m, n, ENAME));
	}

	return(1);
}


static int
check_text(struct mdoc *mdoc, int line, int pos, const char *p)
{
	size_t		 c;

	for ( ; *p; p++) {
		if ('\t' == *p) {
			if ( ! (MDOC_LITERAL & mdoc->flags))
				if ( ! warn_print(mdoc, line, pos))
					return(0);
		} else if ( ! isprint((u_char)*p))
			if ( ! warn_print(mdoc, line, pos))
				return(0);

		if ('\\' != *p)
			continue;

		c = mdoc_isescape(p);
		if (c) {
			/* See if form is deprecated. */
			if ('*' == p[1]) 
				if ( ! pwarn(mdoc, line, pos, WDEPESC))
					return(0);
			p += (int)c - 1;
			continue;
		}
		if ( ! (MDOC_IGN_ESCAPE & mdoc->pflags))
			return(perr(mdoc, line, pos, EESCAPE));
		if ( ! pwarn(mdoc, line, pos, WESCAPE))
			return(0);
	}

	return(1);
}




static int
check_parent(PRE_ARGS, int tok, enum mdoc_type t)
{

	assert(n->parent);
	if ((MDOC_ROOT == t || tok == n->parent->tok) &&
			(t == n->parent->type))
		return(1);

	return(mdoc_nerr(mdoc, n, "require parent %s",
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

	return(nerr(mdoc, n, ENESTDISP));
}


static int
pre_bl(PRE_ARGS)
{
	int		 pos, col, type, width, offset;

	if (MDOC_BLOCK != n->type)
		return(1);
	if (NULL == n->args)
		return(nerr(mdoc, n, ELISTTYPE));

	/* Make sure that only one type of list is specified.  */

	type = offset = width = col = -1;

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
			if (-1 != type) 
				return(nerr(mdoc, n, EMULTILIST));
			type = n->args->argv[pos].arg;
			col = pos;
			break;
		case (MDOC_Width):
			if (-1 != width)
				return(nerr(mdoc, n, EARGREP));
			width = n->args->argv[pos].arg;
			break;
		case (MDOC_Offset):
			if (-1 != offset)
				return(nerr(mdoc, n, EARGREP));
			offset = n->args->argv[pos].arg;
			break;
		default:
			break;
		}

	if (-1 == type)
		return(nerr(mdoc, n, ELISTTYPE));

	/* 
	 * Validate the width field.  Some list types don't need width
	 * types and should be warned about them.  Others should have it
	 * and must also be warned.
	 */

	switch (type) {
	case (MDOC_Tag):
		if (-1 == width && ! nwarn(mdoc, n, WMISSWIDTH))
			return(0);
		break;
	case (MDOC_Column):
		/* FALLTHROUGH */
	case (MDOC_Diag):
		/* FALLTHROUGH */
	case (MDOC_Inset):
		/* FALLTHROUGH */
	case (MDOC_Item):
		if (-1 != width && ! nwarn(mdoc, n, WNOWIDTH))
			return(0);
		break;
	default:
		break;
	}

	/*
	 * General validation of fields.
	 */

	switch (type) {
	case (MDOC_Column):
		assert(col >= 0);
		if (0 == n->args->argv[col].sz)
			break;
		if ( ! nwarn(mdoc, n, WDEPCOL))
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
		return(nerr(mdoc, n, EDISPTYPE));

	/* Make sure that only one type of display is specified.  */

	/* LINTED */
	for (i = 0, err = type = 0; ! err && 
			i < (int)n->args->argc; i++)
		switch (n->args->argv[i].arg) {
		case (MDOC_Ragged):
			/* FALLTHROUGH */
		case (MDOC_Unfilled):
			/* FALLTHROUGH */
		case (MDOC_Filled):
			/* FALLTHROUGH */
		case (MDOC_Literal):
			/* FALLTHROUGH */
		case (MDOC_File):
			if (0 == type++) 
				break;
			return(nerr(mdoc, n, EMULTIDISP));
		default:
			break;
		}

	if (type)
		return(1);
	return(nerr(mdoc, n, EDISPTYPE));
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
	return(check_parent(mdoc, n, -1, MDOC_ROOT));
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
	return(mdoc_nerr(mdoc, n, "only one argument allowed"));
}


static int
pre_lb(PRE_ARGS)
{

	return(check_sec(mdoc, n, SEC_LIBRARY, SEC_CUSTOM));
}


static int
pre_rv(PRE_ARGS)
{

	if ( ! check_msec(mdoc, n, 2, 3, 0))
		return(0);
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
pre_er(PRE_ARGS)
{

	return(check_msec(mdoc, n, 2, 3, 9, 0));
}


static int
pre_cd(PRE_ARGS)
{

	return(check_msec(mdoc, n, 4, 0));
}


static int
pre_dt(PRE_ARGS)
{

	if (0 == mdoc->meta.date || mdoc->meta.os)
		if ( ! nwarn(mdoc, n, WPROLOOO))
			return(0);
	if (mdoc->meta.title)
		if ( ! nwarn(mdoc, n, WPROLREP))
			return(0);
	return(1);
}


static int
pre_os(PRE_ARGS)
{

	if (NULL == mdoc->meta.title || 0 == mdoc->meta.date)
		if ( ! nwarn(mdoc, n, WPROLOOO))
			return(0);
	if (mdoc->meta.os)
		if ( ! nwarn(mdoc, n, WPROLREP))
			return(0);
	return(1);
}


static int
pre_dd(PRE_ARGS)
{

	if (mdoc->meta.title || mdoc->meta.os)
		if ( ! nwarn(mdoc, n, WPROLOOO))
			return(0);
	if (mdoc->meta.date)
		if ( ! nwarn(mdoc, n, WPROLREP))
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
		return(mdoc_err(mdoc, "one argument expected"));
	else if (mdoc->last->args)
		return(1);

	if (NULL == head->child || MDOC_TEXT != head->child->type)
		return(mdoc_err(mdoc, "text argument expected"));

	p = head->child->string;

	if (0 == strcmp(p, "Em"))
		return(1);
	else if (0 == strcmp(p, "Li"))
		return(1);
	else if (0 == strcmp(p, "Sm"))
		return(1);

	return(mdoc_nerr(mdoc, head->child, "invalid font mode"));
}


static int
post_nm(POST_ARGS)
{

	if (mdoc->last->child)
		return(1);
	if (mdoc->meta.name)
		return(1);
	return(verr(mdoc, ENAME));
}


static int
post_at(POST_ARGS)
{

	if (NULL == mdoc->last->child)
		return(1);
	if (MDOC_TEXT != mdoc->last->child->type)
		return(verr(mdoc, EATT));
	if (mdoc_a2att(mdoc->last->child->string))
		return(1);
	return(verr(mdoc, EATT));
}


static int
post_an(POST_ARGS)
{

	if (mdoc->last->args) {
		if (NULL == mdoc->last->child)
			return(1);
		return(verr(mdoc, ELINE));
	}

	if (mdoc->last->child)
		return(1);
	return(verr(mdoc, ELINE));
}


static int
post_args(POST_ARGS)
{

	if (mdoc->last->args)
		return(1);
	return(verr(mdoc, ELINE));
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
		return(verr(mdoc, ELISTTYPE));

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
		return(verr(mdoc, ELISTTYPE));

	switch (type) {
	case (MDOC_Tag):
		if (NULL == mdoc->last->head->child)
			if ( ! vwarn(mdoc, WLINE))
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
			if ( ! vwarn(mdoc, WLINE))
				return(0);
		if (NULL == mdoc->last->body->child)
			if ( ! vwarn(mdoc, WMULTILINE))
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
			if ( ! vwarn(mdoc, WNOLINE))
				return(0);
		if (NULL == mdoc->last->body->child)
			if ( ! vwarn(mdoc, WMULTILINE))
				return(0);
		break;
	case (MDOC_Column):
		if (NULL == mdoc->last->head->child)
			if ( ! vwarn(mdoc, WLINE))
				return(0);
		if (mdoc->last->body->child)
			if ( ! vwarn(mdoc, WNOMULTILINE))
				return(0);
		c = mdoc->last->child;
		for (i = 0; c && MDOC_HEAD == c->type; c = c->next)
			i++;
		if (i == cols)
			break;
		return(mdoc_err(mdoc, "column mismatch (have "
					"%d, want %d)", i, cols));
	default:
		break;
	}

	return(1);
}


static int
post_bl(POST_ARGS)
{
	struct mdoc_node	*n;

	if (MDOC_BODY != mdoc->last->type)
		return(1);
	if (NULL == mdoc->last->child)
		return(1);

	/* LINTED */
	for (n = mdoc->last->child; n; n = n->next) {
		if (MDOC_BLOCK == n->type) 
			if (MDOC_It == n->tok)
				continue;
		return(mdoc_nerr(mdoc, n, "bad child of parent %s",
				mdoc_macronames[mdoc->last->tok]));
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
	return(nerr(mdoc, n, EBOOL));
}


static int
post_root(POST_ARGS)
{

	if (NULL == mdoc->first->child)
		return(verr(mdoc, ENODATA));
	if ( ! (MDOC_PBODY & mdoc->flags))
		return(verr(mdoc, ENOPROLOGUE));

	if (MDOC_BLOCK != mdoc->first->child->type)
		return(verr(mdoc, ENODATA));
	if (MDOC_Sh != mdoc->first->child->tok)
		return(verr(mdoc, ENODATA));

	return(1);
}


static int
post_st(POST_ARGS)
{

	if (mdoc_a2st(mdoc->last->child->string))
		return(1);
	return(vwarn(mdoc, WBADSTAND));
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

	if (SEC_NAME != mdoc->lastnamed)
		return(1);

	/*
	 * Warn if the NAME section doesn't contain the `Nm' and `Nd'
	 * macros (can have multiple `Nm' and one `Nd').  Note that the
	 * children of the BODY declaration can also be "text".
	 */

	if (NULL == (n = mdoc->last->child))
		return(vwarn(mdoc, WNAMESECINC));

	for ( ; n && n->next; n = n->next) {
		if (MDOC_ELEM == n->type && MDOC_Nm == n->tok)
			continue;
		if (MDOC_TEXT == n->type)
			continue;
		if ( ! vwarn(mdoc, WNAMESECINC))
			return(0);
	}

	if (MDOC_ELEM == n->type && MDOC_Nd == n->tok)
		return(1);
	return(vwarn(mdoc, WNAMESECINC));
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
			return(nerr(mdoc, n, ETOOLONG));
		if (NULL == n->next)
			continue;
		if (strlcat(buf, " ", 64) >= 64)
			return(nerr(mdoc, n, ETOOLONG));
	}

	sec = mdoc_atosec(buf);

	/* 
	 * Check: NAME should always be first, CUSTOM has no roles,
	 * non-CUSTOM has a conventional order to be followed.
	 */

	if (SEC_NAME != sec && SEC_NONE == mdoc->lastnamed)
		return(verr(mdoc, ESECNAME));
	if (SEC_CUSTOM == sec)
		return(1);
	if (sec == mdoc->lastnamed)
		return(vwarn(mdoc, WSECREP));
	if (sec < mdoc->lastnamed)
		return(vwarn(mdoc, WSECOOO));

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
			return(vwarn(mdoc, WWRONGMSEC));
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
