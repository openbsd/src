/*	$OpenBSD: rcsparse.c,v 1.7 2011/07/14 16:38:39 sobrado Exp $	*/
/*
 * Copyright (c) 2010 Tobias Stoeckmann <tobias@openbsd.org>
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

#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rcs.h"
#include "rcsparse.h"
#include "xmalloc.h"

#define RCS_BUFSIZE	16384
#define RCS_BUFEXTSIZE	8192

/* RCS token types */
#define RCS_TOK_HEAD		(1 << 0)
#define RCS_TOK_BRANCH		(1 << 1)
#define RCS_TOK_ACCESS		(1 << 2)
#define RCS_TOK_SYMBOLS		(1 << 3)
#define RCS_TOK_LOCKS		(1 << 4)
#define RCS_TOK_STRICT		(1 << 5)
#define RCS_TOK_COMMENT		(1 << 6)
#define RCS_TOK_COMMITID	(1 << 7)
#define RCS_TOK_EXPAND		(1 << 8)
#define RCS_TOK_DESC		(1 << 9)
#define RCS_TOK_DATE		(1 << 10)
#define RCS_TOK_AUTHOR		(1 << 11)
#define RCS_TOK_STATE		(1 << 12)
#define RCS_TOK_BRANCHES	(1 << 13)
#define RCS_TOK_NEXT		(1 << 14)
#define RCS_TOK_LOG		(1 << 15)
#define RCS_TOK_TEXT		(1 << 16)
#define RCS_TOK_COLON		(1 << 17)
#define RCS_TOK_COMMA		(1 << 18)
#define RCS_TOK_SCOLON		(1 << 19)

#define RCS_TYPE_STRING		(1 << 20)
#define RCS_TYPE_NUMBER		(1 << 21)
#define RCS_TYPE_BRANCH		(1 << 22)
#define RCS_TYPE_REVISION	(1 << 23)
#define RCS_TYPE_LOGIN		(1 << 24)
#define RCS_TYPE_STATE		(1 << 25)
#define RCS_TYPE_SYMBOL		(1 << 26)
#define RCS_TYPE_DATE		(1 << 27)
#define RCS_TYPE_KEYWORD	(1 << 28)
#define RCS_TYPE_COMMITID	(1 << 29)

#define MANDATORY	0
#define OPTIONAL	1

/* opaque parse data */
struct rcs_pdata {
	char			*rp_buf;
	size_t			 rp_blen;
	char			*rp_bufend;
	size_t			 rp_tlen;

	struct rcs_delta	*rp_delta;
	int			 rp_lineno;
	int			 rp_msglineno;
	int			 rp_token;

	union {
		RCSNUM		*rev;
		char		*str;
		struct tm	 date;
	} rp_value;
};

struct rcs_keyword {
	const char	*k_name;
	int		 k_val;
};

struct rcs_section {
	int	token;
	int	(*parse)(RCSFILE *, struct rcs_pdata *);
	int	opt;
};

/* this has to be sorted always */
static const struct rcs_keyword keywords[] = {
	{ "access",		RCS_TOK_ACCESS},
	{ "author",		RCS_TOK_AUTHOR},
	{ "branch",		RCS_TOK_BRANCH},
	{ "branches",		RCS_TOK_BRANCHES},
	{ "comment",		RCS_TOK_COMMENT},
	{ "date",		RCS_TOK_DATE},
	{ "desc",		RCS_TOK_DESC},
	{ "expand",		RCS_TOK_EXPAND},
	{ "head",		RCS_TOK_HEAD},
	{ "locks",		RCS_TOK_LOCKS},
	{ "log",		RCS_TOK_LOG},
	{ "next",		RCS_TOK_NEXT},
	{ "state",		RCS_TOK_STATE},
	{ "strict",		RCS_TOK_STRICT},
	{ "symbols",		RCS_TOK_SYMBOLS},
	{ "text",		RCS_TOK_TEXT}
};

/* parser functions specified in rcs_section structs */
static int	rcsparse_head(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_branch(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_access(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_symbols(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_locks(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_strict(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_comment(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_commitid(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_expand(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_deltarevision(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_date(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_author(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_state(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_branches(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_next(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_textrevision(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_log(RCSFILE *, struct rcs_pdata *);
static int	rcsparse_text(RCSFILE *, struct rcs_pdata *);

static int	rcsparse_delta(RCSFILE *);
static int	rcsparse_deltatext(RCSFILE *);
static int	rcsparse_desc(RCSFILE *);

static int	kw_cmp(const void *, const void *);
static int	rcsparse(RCSFILE *, struct rcs_section *);
static void	rcsparse_growbuf(RCSFILE *);
static int	rcsparse_string(RCSFILE *, int);
static int	rcsparse_token(RCSFILE *, int);
static void	rcsparse_warnx(RCSFILE *, char *, ...);
static int	valid_login(char *);

/*
 * head [REVISION];
 * [branch BRANCH];
 * access [LOGIN ...];
 * symbols [SYMBOL:REVISION ...];
 * locks [LOGIN:REVISION ...];
 * [strict;]
 * [comment [@[...]@];]
 * [expand [@[...]@];]
 */
static struct rcs_section sec_admin[] = {
	{ RCS_TOK_HEAD, rcsparse_head, MANDATORY },
	{ RCS_TOK_BRANCH, rcsparse_branch, OPTIONAL },
	{ RCS_TOK_ACCESS, rcsparse_access, MANDATORY },
	{ RCS_TOK_SYMBOLS, rcsparse_symbols, MANDATORY },
	{ RCS_TOK_LOCKS, rcsparse_locks, MANDATORY },
	{ RCS_TOK_STRICT, rcsparse_strict, OPTIONAL },
	{ RCS_TOK_COMMENT, rcsparse_comment, OPTIONAL },
	{ RCS_TOK_EXPAND, rcsparse_expand, OPTIONAL },
	{ 0, NULL, 0 }
};

/*
 * REVISION
 * date [YY]YY.MM.DD.HH.MM.SS;
 * author LOGIN;
 * state STATE;
 * branches [REVISION ...];
 * next [REVISION];
 * [commitid ID;]
 */
static struct rcs_section sec_delta[] = {
	{ RCS_TYPE_REVISION, rcsparse_deltarevision, MANDATORY },
	{ RCS_TOK_DATE, rcsparse_date, MANDATORY },
	{ RCS_TOK_AUTHOR, rcsparse_author, MANDATORY },
	{ RCS_TOK_STATE, rcsparse_state, MANDATORY },
	{ RCS_TOK_BRANCHES, rcsparse_branches, MANDATORY },
	{ RCS_TOK_NEXT, rcsparse_next, MANDATORY },
	{ RCS_TOK_COMMITID, rcsparse_commitid, OPTIONAL },
	{ 0, NULL, 0 }
};

/*
 * REVISION
 * log @[...]@
 * text @[...]@
 */
static struct rcs_section sec_deltatext[] = {
	{ RCS_TYPE_REVISION, rcsparse_textrevision, MANDATORY },
	{ RCS_TOK_LOG, rcsparse_log, MANDATORY },
	{ RCS_TOK_TEXT, rcsparse_text, MANDATORY },
	{ 0, NULL, 0 }
};

/*
 * rcsparse_init()
 *
 * Initializes the parsing data structure and parses the admin section of
 * RCS file <rfp>.
 *
 * Returns 0 on success or 1 on failure.
 */
int
rcsparse_init(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & RCS_PARSED)
		return (0);

	pdp = xmalloc(sizeof(*pdp));
	pdp->rp_buf = xmalloc(RCS_BUFSIZE);
	pdp->rp_blen = RCS_BUFSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;
	pdp->rp_token = -1;
	pdp->rp_lineno = 1;
	pdp->rp_msglineno = 1;

	/* ditch the strict lock */
	rfp->rf_flags &= ~RCS_SLOCK;
	rfp->rf_pdata = pdp;

	if (rcsparse(rfp, sec_admin)) {
		rcsparse_free(rfp);
		return (1);
	}

	if ((rfp->rf_flags & RCS_PARSE_FULLY) &&
	    rcsparse_deltatexts(rfp, NULL)) {
		rcsparse_free(rfp);
		return (1);
	}

	rfp->rf_flags |= RCS_SYNCED;
	return (0);
}

/*
 * rcsparse_deltas()
 *
 * Parse deltas. If <rev> is not NULL, parse only as far as that
 * revision. If <rev> is NULL, parse all deltas.
 *
 * Returns 0 on success or 1 on error.
 */
int
rcsparse_deltas(RCSFILE *rfp, RCSNUM *rev)
{
	int ret;
	struct rcs_delta *enddelta;

	if ((rfp->rf_flags & PARSED_DELTAS) || (rfp->rf_flags & RCS_CREATE))
		return (0);

	for (;;) {
		ret = rcsparse_delta(rfp);
		if (rev != NULL) {
			enddelta = TAILQ_LAST(&(rfp->rf_delta), rcs_dlist);
			if (enddelta == NULL)
				return (1);

			if (rcsnum_cmp(enddelta->rd_num, rev, 0) == 0)
				break;
		}

		if (ret == 0) {
			rfp->rf_flags |= PARSED_DELTAS;
			break;
		}
		else if (ret == -1)
			return (1);
	}

	return (0);
}

/*
 * rcsparse_deltatexts()
 *
 * Parse deltatexts. If <rev> is not NULL, parse only as far as that
 * revision. If <rev> is NULL, parse everything.
 *
 * Returns 0 on success or 1 on error.
 */
int
rcsparse_deltatexts(RCSFILE *rfp, RCSNUM *rev)
{
	int ret;
	struct rcs_delta *rdp;

	if ((rfp->rf_flags & PARSED_DELTATEXTS) ||
	    (rfp->rf_flags & RCS_CREATE))
		return (0);

	if (!(rfp->rf_flags & PARSED_DESC))
		if (rcsparse_desc(rfp))
			return (1);

	rdp = (rev != NULL) ? rcs_findrev(rfp, rev) : NULL;

	for (;;) {
		if (rdp != NULL && rdp->rd_text != NULL)
			break;
		ret = rcsparse_deltatext(rfp);
		if (ret == 0) {
			rfp->rf_flags |= PARSED_DELTATEXTS;
			break;
		}
		else if (ret == -1)
			return (1);
	}

	return (0);
}

/*
 * rcsparse_free()
 *
 * Free the contents of the <rfp>'s parser data structure.
 */
void
rcsparse_free(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	pdp = rfp->rf_pdata;

	if (pdp->rp_buf != NULL)
		xfree(pdp->rp_buf);
	if (pdp->rp_token == RCS_TYPE_REVISION)
		rcsnum_free(pdp->rp_value.rev);
	xfree(pdp);
}

/*
 * rcsparse_desc()
 *
 * Parse desc of the RCS file <rfp>.  By calling rcsparse_desc, all deltas
 * will be parsed in order to proceed the reading cursor to the desc keyword.
 *
 * desc @[...]@;
 *
 * Returns 0 on success or 1 on error.
 */
static int
rcsparse_desc(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & PARSED_DESC)
		return (0);

	if (!(rfp->rf_flags & PARSED_DELTAS) && rcsparse_deltas(rfp, NULL))
		return (1);

	pdp = (struct rcs_pdata *)rfp->rf_pdata;

	if (rcsparse_token(rfp, RCS_TOK_DESC) != RCS_TOK_DESC ||
	    rcsparse_token(rfp, RCS_TYPE_STRING) != RCS_TYPE_STRING)
		return (1);

	rfp->rf_desc = pdp->rp_value.str;
	rfp->rf_flags |= PARSED_DESC;

	return (0);
}

/*
 * rcsparse_deltarevision()
 *
 * Called upon reaching a new REVISION entry in the delta section.
 * A new rcs_delta structure will be prepared in pdp->rp_delta for further
 * parsing.
 *
 * REVISION
 *
 * Always returns 0.
 */
static int
rcsparse_deltarevision(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_delta *rdp;

	rdp = xcalloc(1, sizeof(*rdp));
	TAILQ_INIT(&rdp->rd_branches);
	rdp->rd_num = pdp->rp_value.rev;
	pdp->rp_delta = rdp;

	return (0);
}

/*
 * rcsparse_date()
 *
 * Parses the specified date of current delta pdp->rp_delta.
 *
 * date YYYY.MM.DD.HH.MM.SS;
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_date(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_DATE) != RCS_TYPE_DATE)
		return (1);

	pdp->rp_delta->rd_date = pdp->rp_value.date;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_author()
 *
 * Parses the specified author of current delta pdp->rp_delta.
 *
 * author LOGIN;
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_author(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_LOGIN) != RCS_TYPE_LOGIN)
		return (1);

	pdp->rp_delta->rd_author = pdp->rp_value.str;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_state()
 *
 * Parses the specified state of current delta pdp->rp_delta.
 *
 * state STATE;
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_state(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_STATE) != RCS_TYPE_STATE)
		return (1);

	pdp->rp_delta->rd_state = pdp->rp_value.str;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_branches()
 *
 * Parses the specified branches of current delta pdp->rp_delta.
 *
 * branches [REVISION ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_branches(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_branch *rb;
	int type;

	while ((type = rcsparse_token(rfp, RCS_TOK_SCOLON|RCS_TYPE_REVISION))
	    == RCS_TYPE_REVISION) {
		rb = xmalloc(sizeof(*rb));
		rb->rb_num = pdp->rp_value.rev;
		TAILQ_INSERT_TAIL(&(pdp->rp_delta->rd_branches), rb, rb_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_next()
 *
 * Parses the specified next revision of current delta pdp->rp_delta.
 *
 * next [REVISION];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_next(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_REVISION|RCS_TOK_SCOLON);
	if (type == RCS_TYPE_REVISION) {
		pdp->rp_delta->rd_next = pdp->rp_value.rev;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	} else
		pdp->rp_delta->rd_next = rcsnum_alloc();

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_commitid()
 *
 * Parses the specified commit id of current delta pdp->rp_delta. The
 * commitid keyword is optional and can be omitted.
 *
 * [commitid ID;]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_commitid(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_COMMITID) != RCS_TYPE_COMMITID)
		return (1);

	/* XXX - do something with commitid */

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_textrevision()
 *
 * Called upon reaching a new REVISION entry in the delta text section.
 * pdp->rp_delta will be set to REVISION's delta (created in delta section)
 * for further parsing.
 *
 * REVISION
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_textrevision(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_delta *rdp;

	TAILQ_FOREACH(rdp, &rfp->rf_delta, rd_list) {
		if (rcsnum_cmp(rdp->rd_num, pdp->rp_value.rev, 0) == 0)
			break;
	}
	if (rdp == NULL) {
		rcsparse_warnx(rfp, "delta for revision \"%s\" not found",
		    pdp->rp_buf);
		rcsnum_free(pdp->rp_value.rev);
		return (1);
	}
	pdp->rp_delta = rdp;

	rcsnum_free(pdp->rp_value.rev);
	return (0);
}

/*
 * rcsparse_log()
 *
 * Parses the specified log of current deltatext pdp->rp_delta.
 *
 * log @[...]@
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_log(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_STRING) != RCS_TYPE_STRING)
		return (1);

	pdp->rp_delta->rd_log = pdp->rp_value.str;

	return (0);
}

/*
 * rcsparse_text()
 *
 * Parses the specified text of current deltatext pdp->rp_delta.
 *
 * text @[...]@
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_text(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_STRING) != RCS_TYPE_STRING)
		return (1);

	pdp->rp_delta->rd_tlen = pdp->rp_tlen - 1;
	if (pdp->rp_delta->rd_tlen == 0) {
		pdp->rp_delta->rd_text = xstrdup("");
	} else {
		pdp->rp_delta->rd_text = xmalloc(pdp->rp_delta->rd_tlen);
		memcpy(pdp->rp_delta->rd_text, pdp->rp_buf,
		    pdp->rp_delta->rd_tlen);
	}
	xfree(pdp->rp_value.str);

	return (0);
}

/*
 * rcsparse_head()
 *
 * Parses the head revision of RCS file <rfp>.
 *
 * head [REVISION];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_head(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_REVISION|RCS_TOK_SCOLON);
	if (type == RCS_TYPE_REVISION) {
		rfp->rf_head = pdp->rp_value.rev;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_branch()
 *
 * Parses the default branch of RCS file <rfp>. The branch keyword is
 * optional and can be omitted.
 *
 * [branch BRANCH;]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_branch(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_BRANCH|RCS_TOK_SCOLON);
	if (type == RCS_TYPE_BRANCH) {
		rfp->rf_branch = pdp->rp_value.rev;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_access()
 *
 * Parses the access list of RCS file <rfp>.
 *
 * access [LOGIN ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_access(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_access *ap;
	int type;

	while ((type = rcsparse_token(rfp, RCS_TOK_SCOLON|RCS_TYPE_LOGIN))
	    == RCS_TYPE_LOGIN) {
		ap = xmalloc(sizeof(*ap));
		ap->ra_name = pdp->rp_value.str;
		TAILQ_INSERT_TAIL(&(rfp->rf_access), ap, ra_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_symbols()
 *
 * Parses the symbol list of RCS file <rfp>.
 *
 * symbols [SYMBOL:REVISION ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_symbols(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_sym *symp;
	char *name;
	int type;

	while ((type = rcsparse_token(rfp, RCS_TOK_SCOLON|RCS_TYPE_SYMBOL)) ==
	    RCS_TYPE_SYMBOL) {
		name = pdp->rp_value.str;
		if (rcsparse_token(rfp, RCS_TOK_COLON) != RCS_TOK_COLON ||
		    rcsparse_token(rfp, RCS_TYPE_NUMBER) != RCS_TYPE_NUMBER) {
			xfree(name);
			return (1);
		}
		symp = xmalloc(sizeof(*symp));
		symp->rs_name = name;
		symp->rs_num = pdp->rp_value.rev;
		TAILQ_INSERT_TAIL(&(rfp->rf_symbols), symp, rs_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_locks()
 *
 * Parses the lock list of RCS file <rfp>.
 *
 * locks [SYMBOL:REVISION ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_locks(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_lock *lkp;
	char *name;
	int type;

	while ((type = rcsparse_token(rfp, RCS_TOK_SCOLON|RCS_TYPE_LOGIN)) ==
	    RCS_TYPE_LOGIN) {
		name = pdp->rp_value.str;
		if (rcsparse_token(rfp, RCS_TOK_COLON) != RCS_TOK_COLON ||
		    rcsparse_token(rfp, RCS_TYPE_REVISION) !=
		    RCS_TYPE_REVISION) {
			xfree(name);
			return (1);
		}
		lkp = xmalloc(sizeof(*lkp));
		lkp->rl_name = name;
		lkp->rl_num = pdp->rp_value.rev;
		TAILQ_INSERT_TAIL(&(rfp->rf_locks), lkp, rl_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_locks()
 *
 * Parses the strict keyword of RCS file <rfp>. The strict keyword is
 * optional and can be omitted.
 *
 * [strict;]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_strict(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	rfp->rf_flags |= RCS_SLOCK;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_comment()
 *
 * Parses the comment of RCS file <rfp>.  The comment keyword is optional
 * and can be omitted.
 *
 * [comment [@[...]@];]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_comment(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_STRING|RCS_TOK_SCOLON);
	if (type == RCS_TYPE_STRING) {
		rfp->rf_comment = pdp->rp_value.str;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_expand()
 *
 * Parses expand of RCS file <rfp>.  The expand keyword is optional and
 * can be omitted.
 *
 * [expand [@[...]@];]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_expand(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_STRING|RCS_TOK_SCOLON);
	if (type == RCS_TYPE_STRING) {
		rfp->rf_expand = pdp->rp_value.str;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

#define RBUF_PUTC(ch) \
do { \
	if (bp == pdp->rp_bufend - 1) { \
		len = bp - pdp->rp_buf; \
		rcsparse_growbuf(rfp); \
		bp = pdp->rp_buf + len; \
	} \
	*(bp++) = (ch); \
	pdp->rp_tlen++; \
} while (0);

static int
rcsparse_string(RCSFILE *rfp, int allowed)
{
	struct rcs_pdata *pdp;
	int c;
	size_t len;
	char *bp;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;

	bp = pdp->rp_buf;
	pdp->rp_tlen = 0;
	*bp = '\0';

	for (;;) {
		c = getc(rfp->rf_file);
		if (c == '@') {
			c = getc(rfp->rf_file);
			if (c == EOF) {
				return (EOF);
			} else if (c != '@') {
				ungetc(c, rfp->rf_file);
				break;
			}
		}

		if (c == EOF) {
			return (EOF);
		} else if (c == '\n')
			pdp->rp_lineno++;

		RBUF_PUTC(c);
	}

	bp = pdp->rp_buf + pdp->rp_tlen;
	RBUF_PUTC('\0');

	if (!(allowed & RCS_TYPE_STRING)) {
		rcsparse_warnx(rfp, "unexpected RCS string");
		return (0);
	}

	pdp->rp_value.str = xstrdup(pdp->rp_buf);

	return (RCS_TYPE_STRING);
}

static int
rcsparse_token(RCSFILE *rfp, int allowed)
{
	const struct rcs_keyword *p;
	struct rcs_pdata *pdp;
	int c, pre, ret, type;
	char *bp;
	size_t len;
	RCSNUM *datenum;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;

	if (pdp->rp_token != -1) {
		/* no need to check for allowed here */
		type = pdp->rp_token;
		pdp->rp_token = -1;
		return (type);
	}

	/* skip whitespaces */
	c = EOF;
	do {
		pre = c;
		c = getc(rfp->rf_file);
		if (c == EOF) {
			if (ferror(rfp->rf_file)) {
				rcsparse_warnx(rfp, "error during parsing");
				return (0);
			}
			if (pre != '\n')
				rcsparse_warnx(rfp,
				    "no newline at end of file");
			return (EOF);
		} else if (c == '\n')
			pdp->rp_lineno++;
	} while (isspace(c));

	pdp->rp_msglineno = pdp->rp_lineno;
	type = 0;
	switch (c) {
	case '@':
		ret = rcsparse_string(rfp, allowed);
		if (ret == EOF && ferror(rfp->rf_file)) {
			rcsparse_warnx(rfp, "error during parsing");
			return (0);
		}
		return (ret);
		/* NOTREACHED */
	case ':':
		type = RCS_TOK_COLON;
		if (type & allowed)
			return (type);
		rcsparse_warnx(rfp, "unexpected token \"%c\"", c);
		return (0);
		/* NOTREACHED */
	case ';':
		type = RCS_TOK_SCOLON;
		if (type & allowed)
			return (type);
		rcsparse_warnx(rfp, "unexpected token \"%c\"", c);
		return (0);
		/* NOTREACHED */
	case ',':
		type = RCS_TOK_COMMA;
		if (type & allowed)
			return (type);
		rcsparse_warnx(rfp, "unexpected token \"%c\"", c);
		return (0);
		/* NOTREACHED */
	default:
		if (!isgraph(c)) {
			rcsparse_warnx(rfp, "unexpected character 0x%.2X", c);
			return (0);
		}
		break;
	}
	allowed &= ~(RCS_TOK_COLON|RCS_TOK_SCOLON|RCS_TOK_COMMA);

	bp = pdp->rp_buf;
	pdp->rp_tlen = 0;
	*bp = '\0';

	for (;;) {
		if (c == EOF) {
			if (ferror(rfp->rf_file))
				rcsparse_warnx(rfp, "error during parsing");
			else
				rcsparse_warnx(rfp, "unexpected end of file");
			return (0);
		} else if (c == '\n')
			pdp->rp_lineno++;

		RBUF_PUTC(c);

		c = getc(rfp->rf_file);

		if (isspace(c)) {
			if (c == '\n')
				pdp->rp_lineno++;
			RBUF_PUTC('\0');
			break;
		} else if (c == ';' || c == ':' || c == ',') {
			ungetc(c, rfp->rf_file);
			RBUF_PUTC('\0');
			break;
		} else if (!isgraph(c)) {
			rcsparse_warnx(rfp, "unexpected character 0x%.2X", c);
			return (0);
		}
	}

	switch (allowed) {
	case RCS_TYPE_COMMITID:
		/* XXX validate commitid */
		break;
	case RCS_TYPE_LOGIN:
		if (!valid_login(pdp->rp_buf)) {
			rcsparse_warnx(rfp, "invalid login \"%s\"",
			    pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.str = xstrdup(pdp->rp_buf);
		break;
	case RCS_TYPE_SYMBOL:
		if (!rcs_sym_check(pdp->rp_buf)) {
			rcsparse_warnx(rfp, "invalid symbol \"%s\"",
			    pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.str = xstrdup(pdp->rp_buf);
		break;
		/* FALLTHROUGH */
	case RCS_TYPE_STATE:
		if (rcs_state_check(pdp->rp_buf)) {
			rcsparse_warnx(rfp, "invalid state \"%s\"",
			    pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.str = xstrdup(pdp->rp_buf);
		break;
	case RCS_TYPE_DATE:
		if ((datenum = rcsnum_parse(pdp->rp_buf)) == NULL) {
			rcsparse_warnx(rfp, "invalid date \"%s\"", pdp->rp_buf);
			return (0);
		}
		if (datenum->rn_len != 6) {
			rcsnum_free(datenum);
			rcsparse_warnx(rfp, "invalid date \"%s\"", pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.date.tm_year = datenum->rn_id[0];
		if (pdp->rp_value.date.tm_year >= 1900)
			pdp->rp_value.date.tm_year -= 1900;
		pdp->rp_value.date.tm_mon = datenum->rn_id[1] - 1;
		pdp->rp_value.date.tm_mday = datenum->rn_id[2];
		pdp->rp_value.date.tm_hour = datenum->rn_id[3];
		pdp->rp_value.date.tm_min = datenum->rn_id[4];
		pdp->rp_value.date.tm_sec = datenum->rn_id[5];
		rcsnum_free(datenum);
		break;
	case RCS_TYPE_NUMBER:
		pdp->rp_value.rev = rcsnum_parse(pdp->rp_buf);
		if (pdp->rp_value.rev == NULL) {
			rcsparse_warnx(rfp, "invalid number \"%s\"",
			    pdp->rp_buf);
			return (0);
		}
		break;
	case RCS_TYPE_BRANCH:
		pdp->rp_value.rev = rcsnum_parse(pdp->rp_buf);
		if (pdp->rp_value.rev == NULL) {
			rcsparse_warnx(rfp, "invalid branch \"%s\"",
			    pdp->rp_buf);
			return (0);
		}
		if (!RCSNUM_ISBRANCH(pdp->rp_value.rev)) {
			rcsnum_free(pdp->rp_value.rev);
			rcsparse_warnx(rfp, "expected branch, got \"%s\"",
			    pdp->rp_buf);
			return (0);
		}
		break;
	case RCS_TYPE_KEYWORD:
		if (islower(*pdp->rp_buf)) {
			p = bsearch(pdp->rp_buf, keywords,
			    sizeof(keywords) / sizeof(keywords[0]),
			    sizeof(keywords[0]), kw_cmp);
			if (p != NULL)
				return (p->k_val);
		}
		allowed = RCS_TYPE_REVISION;
		/* FALLTHROUGH */
	case RCS_TYPE_REVISION:
		pdp->rp_value.rev = rcsnum_parse(pdp->rp_buf);
		if (pdp->rp_value.rev != NULL) {
			if (RCSNUM_ISBRANCH(pdp->rp_value.rev)) {
				rcsnum_free(pdp->rp_value.rev);
				rcsparse_warnx(rfp,
				    "expected revision, got \"%s\"",
				    pdp->rp_buf);
				return (0);
			}
			break;
		}
		/* FALLTHROUGH */
	default:
		RBUF_PUTC('\0');
		rcsparse_warnx(rfp, "unexpected token \"%s\"", pdp->rp_buf);
		return (0);
		/* NOTREACHED */
	}

	return (allowed);
}

static int
rcsparse(RCSFILE *rfp, struct rcs_section *sec)
{
	struct rcs_pdata *pdp;
	int i, token;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;
	i = 0;

	token = 0;
	for (i = 0; sec[i].token != 0; i++) {
		token = rcsparse_token(rfp, RCS_TYPE_KEYWORD);
		if (token == 0)
			return (1);

		while (token != sec[i].token) {
			if (sec[i].parse == NULL)
				goto end;
			if (sec[i].opt) {
				i++;
				continue;
			}
			if (token == EOF || (!(rfp->rf_flags & PARSED_DELTAS) &&
			    token == RCS_TOK_DESC))
				goto end;
			rcsparse_warnx(rfp, "unexpected token \"%s\"",
			    pdp->rp_buf);
			return (1);
		}

		if (sec[i].parse(rfp, pdp))
			return (1);
	}
end:
	if (token == RCS_TYPE_REVISION)
		pdp->rp_token = token;
	else if (token == RCS_TOK_DESC)
		pdp->rp_token = RCS_TOK_DESC;
	else if (token == EOF)
		rfp->rf_flags |= RCS_PARSED;

	return (0);
}

static int
rcsparse_deltatext(RCSFILE *rfp)
{
	int ret;

	if (rfp->rf_flags & PARSED_DELTATEXTS)
		return (0);

	if (!(rfp->rf_flags & PARSED_DESC))
		if ((ret = rcsparse_desc(rfp)))
			return (ret);
		
	if (rcsparse(rfp, sec_deltatext))
		return (-1);

	if (rfp->rf_flags & RCS_PARSED)
		rfp->rf_flags |= PARSED_DELTATEXTS;

	return (1);
}

static int
rcsparse_delta(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & PARSED_DELTAS)
		return (0);

	pdp = (struct rcs_pdata *)rfp->rf_pdata;
	if (pdp->rp_token == RCS_TOK_DESC) {
		rfp->rf_flags |= PARSED_DELTAS;
		return (0);
	}

	if (rcsparse(rfp, sec_delta))
		return (-1);

	if (pdp->rp_delta != NULL) {
		TAILQ_INSERT_TAIL(&rfp->rf_delta, pdp->rp_delta, rd_list);
		pdp->rp_delta = NULL;
		rfp->rf_ndelta++;
		return (1);
	}

	return (0);
}

/*
 * rcsparse_growbuf()
 *
 * Attempt to grow the internal parse buffer for the RCS file <rf> by
 * RCS_BUFEXTSIZE.
 * In case of failure, the original buffer is left unmodified.
 */
static void
rcsparse_growbuf(RCSFILE *rfp)
{
	struct rcs_pdata *pdp = (struct rcs_pdata *)rfp->rf_pdata;
	
	pdp->rp_buf = xrealloc(pdp->rp_buf, 1,
		pdp->rp_blen + RCS_BUFEXTSIZE);
	pdp->rp_blen += RCS_BUFEXTSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;
}

/*
 * Borrowed from src/usr.sbin/user/user.c:
 * return 1 if `login' is a valid login name
 */
static int
valid_login(char *login_name)
{
	unsigned char *cp;

	/* The first character cannot be a hyphen */
	if (*login_name == '-')
		return 0;

	for (cp = login_name ; *cp ; cp++) {
		/* We allow '$' as the last character for samba */
		if (!isalnum(*cp) && *cp != '.' && *cp != '_' && *cp != '-' &&
		    !(*cp == '$' && *(cp + 1) == '\0')) {
			return 0;
		}
	}
	if ((char *)cp - login_name > _PW_NAME_LEN)
		return 0;
	return 1;
}

static int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct rcs_keyword *)e)->k_name));
}

static void
rcsparse_warnx(RCSFILE *rfp, char *fmt, ...)
{
	struct rcs_pdata *pdp;
	va_list ap;
	char *nfmt;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;
	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", rfp->rf_path, pdp->rp_msglineno, fmt)
	    == -1)
		nfmt = fmt;
	vwarnx(nfmt, ap);
	va_end(ap);
	if (nfmt != fmt)
		free(nfmt);
}
