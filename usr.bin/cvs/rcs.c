/*	$OpenBSD: rcs.c,v 1.16 2004/12/16 17:16:18 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "rcs.h"
#include "log.h"

#define RCS_BUFSIZE   8192


/* RCS token types */
#define RCS_TOK_ERR     -1
#define RCS_TOK_EOF      0
#define RCS_TOK_NUM      1
#define RCS_TOK_ID       2
#define RCS_TOK_STRING   3
#define RCS_TOK_SCOLON   4
#define RCS_TOK_COLON    5


#define RCS_TOK_HEAD     8
#define RCS_TOK_BRANCH   9
#define RCS_TOK_ACCESS   10
#define RCS_TOK_SYMBOLS  11
#define RCS_TOK_LOCKS    12
#define RCS_TOK_COMMENT  13
#define RCS_TOK_EXPAND   14
#define RCS_TOK_DATE     15
#define RCS_TOK_AUTHOR   16
#define RCS_TOK_STATE    17
#define RCS_TOK_NEXT     18
#define RCS_TOK_BRANCHES 19
#define RCS_TOK_DESC     20
#define RCS_TOK_LOG      21
#define RCS_TOK_TEXT     22
#define RCS_TOK_STRICT   23

#define RCS_ISKEY(t)    (((t) >= RCS_TOK_HEAD) && ((t) <= RCS_TOK_BRANCHES))


#define RCS_NOSCOL   0x01   /* no terminating semi-colon */
#define RCS_VOPT     0x02   /* value is optional */


/* opaque parse data */
struct rcs_pdata {
	u_int  rp_line;

	char  *rp_buf;
	size_t rp_blen;

	/* pushback token buffer */
	char   rp_ptok[128];
	int    rp_pttype;       /* token type, RCS_TOK_ERR if no token */

	FILE  *rp_file;
};


struct rcs_line {
	char *rl_line;
	int   rl_lineno;
	TAILQ_ENTRY(rcs_line) rl_list;
};
TAILQ_HEAD(rcs_tqh, rcs_line);

struct rcs_foo {
	int       rl_nblines;
	char     *rl_data;
	struct rcs_tqh rl_lines;
};

static int  rcs_parse_admin     (RCSFILE *);
static int  rcs_parse_delta     (RCSFILE *);
static int  rcs_parse_deltatext (RCSFILE *);

static int      rcs_parse_access    (RCSFILE *);
static int      rcs_parse_symbols   (RCSFILE *);
static int      rcs_parse_locks     (RCSFILE *);
static int      rcs_parse_branches  (RCSFILE *, struct rcs_delta *);
static void     rcs_freedelta       (struct rcs_delta *);
static void     rcs_freepdata       (struct rcs_pdata *);
static int      rcs_gettok          (RCSFILE *);
static int      rcs_pushtok         (RCSFILE *, const char *, int);
static int      rcs_patch_lines     (struct rcs_foo *, struct rcs_foo *);

static struct rcs_delta*  rcs_findrev    (RCSFILE *, RCSNUM *);
static struct rcs_foo*    rcs_splitlines (const char *);
static void               rcs_freefoo    (struct rcs_foo *);

#define RCS_TOKSTR(rfp)   ((struct rcs_pdata *)rfp->rf_pdata)->rp_buf
#define RCS_TOKLEN(rfp)   ((struct rcs_pdata *)rfp->rf_pdata)->rp_blen


static struct rcs_key {
	char  rk_str[16];
	int   rk_id;
	int   rk_val;
	int   rk_flags;
} rcs_keys[] = {
	{ "access",   RCS_TOK_ACCESS,   RCS_TOK_ID,     RCS_VOPT     },
	{ "author",   RCS_TOK_AUTHOR,   RCS_TOK_STRING, 0            },
	{ "branch",   RCS_TOK_BRANCH,   RCS_TOK_NUM,    RCS_VOPT     },
	{ "branches", RCS_TOK_BRANCHES, RCS_TOK_NUM,    RCS_VOPT     },
	{ "comment",  RCS_TOK_COMMENT,  RCS_TOK_STRING, RCS_VOPT     },
	{ "date",     RCS_TOK_DATE,     RCS_TOK_NUM,    0            },
	{ "desc",     RCS_TOK_DESC,     RCS_TOK_STRING, RCS_NOSCOL   },
	{ "expand",   RCS_TOK_EXPAND,   RCS_TOK_STRING, RCS_VOPT     },
	{ "head",     RCS_TOK_HEAD,     RCS_TOK_NUM,    RCS_VOPT     },
	{ "locks",    RCS_TOK_LOCKS,    RCS_TOK_ID,     0            },
	{ "log",      RCS_TOK_LOG,      RCS_TOK_STRING, RCS_NOSCOL   },
	{ "next",     RCS_TOK_NEXT,     RCS_TOK_NUM,    RCS_VOPT     },
	{ "state",    RCS_TOK_STATE,    RCS_TOK_STRING, RCS_VOPT     },
	{ "strict",   RCS_TOK_STRICT,   0,              0,           },
	{ "symbols",  RCS_TOK_SYMBOLS,  0,              0            },
	{ "text",     RCS_TOK_TEXT,     RCS_TOK_STRING, RCS_NOSCOL   },
};



/*
 * rcs_open()
 *
 * Open a file containing RCS-formatted information.  The file's path is
 * given in <path>, and the opening mode is given in <mode>, which is either
 * RCS_MODE_READ, RCS_MODE_WRITE, or RCS_MODE_RDWR.  If the mode requests write
 * access and the file does not exist, it will be created.
 * The file isn't actually parsed by rcs_open(); parsing is delayed until the
 * first operation that requires information from the file.
 * Returns a handle to the opened file on success, or NULL on failure.
 */
RCSFILE*
rcs_open(const char *path, u_int mode)
{
	RCSFILE *rfp;
	struct stat st;

	if ((stat(path, &st) == -1) && (errno == ENOENT) &&
	   !(mode & RCS_MODE_WRITE)) {
		cvs_log(LP_ERRNO, "cannot open RCS file `%s'", path);
		return (NULL);
	}

	rfp = (RCSFILE *)malloc(sizeof(*rfp));
	if (rfp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate RCS file structure");
		return (NULL);
	}
	memset(rfp, 0, sizeof(*rfp));

	rfp->rf_head = rcsnum_alloc();
	if (rfp->rf_head == NULL) {
		free(rfp);
		return (NULL);
	}

	rfp->rf_branch = rcsnum_alloc();
	if (rfp->rf_branch == NULL) {
		rcs_close(rfp);
		return (NULL);
	}

	rfp->rf_path = strdup(path);
	if (rfp->rf_path == NULL) {
		cvs_log(LP_ERRNO, "failed to duplicate RCS file path");
		rcs_close(rfp);
		return (NULL);
	}

	rcsnum_aton(RCS_HEAD_INIT, NULL, rfp->rf_head);

	rfp->rf_ref = 1;
	rfp->rf_flags |= RCS_RF_SLOCK;
	rfp->rf_mode = mode;

	TAILQ_INIT(&(rfp->rf_delta));
	TAILQ_INIT(&(rfp->rf_symbols));
	TAILQ_INIT(&(rfp->rf_locks));

	if (rcs_parse(rfp) < 0) {
		rcs_close(rfp);
		return (NULL);
	}

	return (rfp);
}


/*
 * rcs_close()
 *
 * Close an RCS file handle.
 */
void
rcs_close(RCSFILE *rfp)
{
	struct rcs_delta *rdp;
	struct rcs_lock *rlp;
	struct rcs_sym *rsp;

	if (rfp->rf_ref > 1) {
		rfp->rf_ref--;
		return;
	}

	while (!TAILQ_EMPTY(&(rfp->rf_delta))) {
		rdp = TAILQ_FIRST(&(rfp->rf_delta));
		TAILQ_REMOVE(&(rfp->rf_delta), rdp, rd_list);
		rcs_freedelta(rdp);
	}

	while (!TAILQ_EMPTY(&(rfp->rf_symbols))) {
		rsp = TAILQ_FIRST(&(rfp->rf_symbols));
		TAILQ_REMOVE(&(rfp->rf_symbols), rsp, rs_list);
		rcsnum_free(rsp->rs_num);
		free(rsp->rs_name);
		free(rsp);
	}

	while (!TAILQ_EMPTY(&(rfp->rf_locks))) {
		rlp = TAILQ_FIRST(&(rfp->rf_locks));
		TAILQ_REMOVE(&(rfp->rf_locks), rlp, rl_list);
		rcsnum_free(rlp->rl_num);
		free(rlp);
	}

	if (rfp->rf_head != NULL)
		rcsnum_free(rfp->rf_head);
	if (rfp->rf_branch != NULL)
		rcsnum_free(rfp->rf_branch);

	if (rfp->rf_path != NULL)
		free(rfp->rf_path);
	if (rfp->rf_comment != NULL)
		free(rfp->rf_comment);
	if (rfp->rf_expand != NULL)
		free(rfp->rf_expand);
	if (rfp->rf_desc != NULL)
		free(rfp->rf_desc);
	free(rfp);
}


/*
 * rcs_write()
 *
 * Write the contents of the RCS file handle <rfp> to disk in the file whose
 * path is in <rf_path>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_write(RCSFILE *rfp)
{
	FILE *fp;
	char buf[1024], numbuf[64], *cp;
	size_t rlen, len;
	struct rcs_sym *symp;
	struct rcs_delta *rdp;

	if (rfp->rf_flags & RCS_RF_SYNCED)
		return (0);

	fp = fopen(rfp->rf_path, "w");
	if (fp == NULL) {
		cvs_log(LP_ERRNO, "failed to open RCS output file `%s'",
		    rfp->rf_path);
		return (-1);
	}

	rcsnum_tostr(rfp->rf_head, numbuf, sizeof(numbuf));
	fprintf(fp, "head\t%s;\n", numbuf);
	fprintf(fp, "access;\n");

	fprintf(fp, "symbols\n");
	TAILQ_FOREACH(symp, &(rfp->rf_symbols), rs_list) {
		rcsnum_tostr(symp->rs_num, numbuf, sizeof(numbuf));
		snprintf(buf, sizeof(buf), "%s:%s", symp->rs_name, numbuf);
		fprintf(fp, "\t%s", buf);
		if (symp != TAILQ_LAST(&(rfp->rf_symbols), rcs_slist))
			fputc('\n', fp);
	}
	fprintf(fp, ";\n");

	fprintf(fp, "locks;");

	if (rfp->rf_flags & RCS_RF_SLOCK)
		fprintf(fp, " strict;");
	fputc('\n', fp);

	if (rfp->rf_comment != NULL)
		fprintf(fp, "comment\t@%s@;\n", rfp->rf_comment);

	if (rfp->rf_expand != NULL)
		fprintf(fp, "expand @ %s @;\n", rfp->rf_expand);

	fprintf(fp, "\n\n");

	TAILQ_FOREACH(rdp, &(rfp->rf_delta), rd_list) {
		fprintf(fp, "%s\n", rcsnum_tostr(rdp->rd_num, numbuf,
		    sizeof(numbuf)));
		fprintf(fp, "date\t%d.%02d.%02d.%02d.%02d.%02d;",
		    rdp->rd_date.tm_year, rdp->rd_date.tm_mon + 1,
		    rdp->rd_date.tm_mday, rdp->rd_date.tm_hour,
		    rdp->rd_date.tm_min, rdp->rd_date.tm_sec);
		fprintf(fp, "\tauthor %s;\tstate %s;\n",
		    rdp->rd_author, rdp->rd_state);
		fprintf(fp, "branches;\n");
		fprintf(fp, "next\t%s;\n\n", rcsnum_tostr(rdp->rd_next,
		    numbuf, sizeof(numbuf)));
	}

	fprintf(fp, "\ndesc\n@%s@\n\n", rfp->rf_desc);

	/* deltatexts */
	TAILQ_FOREACH(rdp, &(rfp->rf_delta), rd_list) {
		fprintf(fp, "\n%s\n", rcsnum_tostr(rdp->rd_num, numbuf,
		    sizeof(numbuf)));
		fprintf(fp, "log\n@%s@\ntext\n@", rdp->rd_log);

		cp = rdp->rd_text;
		do {
			len = sizeof(buf);
			rlen = rcs_stresc(1, cp, buf, &len);
			fprintf(fp, "%s", buf);
			cp += rlen;
		} while (len != 0);
		fprintf(fp, "@\n\n");
	}
	fclose(fp);

	rfp->rf_flags |= RCS_RF_SYNCED;

	return (0);
}


/*
 * rcs_addsym()
 *
 * Add a symbol to the list of symbols for the RCS file <rfp>.  The new symbol
 * is named <sym> and is bound to the RCS revision <snum>.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_addsym(RCSFILE *rfp, const char *sym, RCSNUM *snum)
{
	struct rcs_sym *symp;

	/* first look for duplication */
	TAILQ_FOREACH(symp, &(rfp->rf_symbols), rs_list) {
		if (strcmp(symp->rs_name, sym) == 0) {
			return (-1);
		}
	}

	symp = (struct rcs_sym *)malloc(sizeof(*symp));
	if (symp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate RCS symbol");
		return (-1);
	}

	symp->rs_name = strdup(sym);
	if (symp->rs_name == NULL) {
		cvs_log(LP_ERRNO, "failed to duplicate symbol");
		free(symp);
		return (-1);
	}

	symp->rs_num = rcsnum_alloc();
	if (symp->rs_num == NULL) {
		free(symp);
		return (-1);
	}
	rcsnum_cpy(snum, symp->rs_num, 0);

	TAILQ_INSERT_HEAD(&(rfp->rf_symbols), symp, rs_list);

	/* not synced anymore */
	rfp->rf_flags &= ~RCS_RF_SYNCED;

	return (0);
}


/*
 * rcs_patch()
 *
 * Apply an RCS-format patch pointed to by <patch> to the file contents
 * found in <data>.
 * Returns 0 on success, or -1 on failure.
 */

BUF*
rcs_patch(const char *data, const char *patch)
{
	struct rcs_foo *dlines, *plines;
	struct rcs_line *lp;
	size_t len;
	int lineno;
	BUF *res;

	len = strlen(data);
	res = cvs_buf_alloc(len, BUF_AUTOEXT);
	if (res == NULL)
		return (NULL);

	dlines = rcs_splitlines(data);
	if (dlines == NULL)
		return (NULL);

	plines = rcs_splitlines(patch);
	if (plines == NULL) {
		rcs_freefoo(dlines);
		return (NULL);
	}

	if (rcs_patch_lines(dlines, plines) < 0) {
		rcs_freefoo(plines);
		rcs_freefoo(dlines);
		return (NULL);
	}

	lineno = 0;
	TAILQ_FOREACH(lp, &dlines->rl_lines, rl_list) {
		if (lineno != 0)
			cvs_buf_fappend(res, "%s\n", lp->rl_line);
		lineno++;
	}

	rcs_freefoo(dlines);
	rcs_freefoo(plines);
	return (res);
}

static int
rcs_patch_lines(struct rcs_foo *dlines, struct rcs_foo *plines)
{
	char op, *ep;
	struct rcs_line *lp, *dlp, *ndlp;
	int i, lineno, nbln;

	dlp = TAILQ_FIRST(&(dlines->rl_lines));
	lp = TAILQ_FIRST(&(plines->rl_lines));

	/* skip first bogus line */
	for (lp = TAILQ_NEXT(lp, rl_list); lp != NULL;
	    lp = TAILQ_NEXT(lp, rl_list)) {
		op = *(lp->rl_line);
		lineno = (int)strtol((lp->rl_line + 1), &ep, 10);
		if ((lineno > dlines->rl_nblines) || (lineno <= 0) ||
		    (*ep != ' ')) {
			cvs_log(LP_ERR,
			    "invalid line specification in RCS patch");
			return (NULL);
		}
		ep++;
		nbln = (int)strtol(ep, &ep, 10);
		if ((nbln <= 0) || (*ep != '\0')) {
			cvs_log(LP_ERR,
			    "invalid line number specification in RCS patch");
			return (NULL);
		}

		/* find the appropriate line */
		for (;;) {
			if (dlp == NULL)
				break;
			if (dlp->rl_lineno == lineno)
				break;
			if (dlp->rl_lineno > lineno) {
				dlp = TAILQ_PREV(dlp, rcs_tqh, rl_list);
			} else if (dlp->rl_lineno < lineno) {
				ndlp = TAILQ_NEXT(dlp, rl_list);
				if (ndlp->rl_lineno > lineno)
					break;
				dlp = ndlp;
			}
		}
		if (dlp == NULL) {
			cvs_log(LP_ERR,
			    "can't find referenced line in RCS patch");
			return (NULL);
		}

		if (op == 'd') {
			for (i = 0; (i < nbln) && (dlp != NULL); i++) {
				ndlp = TAILQ_NEXT(dlp, rl_list);
				TAILQ_REMOVE(&(dlines->rl_lines), dlp, rl_list);
				dlp = ndlp;
			}
		} else if (op == 'a') {
			for (i = 0; i < nbln; i++) {
				ndlp = lp;
				lp = TAILQ_NEXT(lp, rl_list);
				if (lp == NULL) {
					cvs_log(LP_ERR, "truncated RCS patch");
					return (-1);
				}
				TAILQ_REMOVE(&(plines->rl_lines), lp, rl_list);
				TAILQ_INSERT_AFTER(&(dlines->rl_lines), dlp,
				    lp, rl_list);
				dlp = lp;

				/* we don't want lookup to block on those */
				lp->rl_lineno = lineno;

				lp = ndlp;
			}
		} else {
			cvs_log(LP_ERR, "unknown RCS patch operation `%c'", op);
			return (-1);
		}

		/* last line of the patch, done */
		if (lp->rl_lineno == plines->rl_nblines)
			break;
	}

	/* once we're done patching, rebuild the line numbers */
	lineno = 0;
	TAILQ_FOREACH(lp, &(dlines->rl_lines), rl_list)
		lp->rl_lineno = lineno++;
	dlines->rl_nblines = lineno - 1;

	return (0);
}


/*
 * rcs_getrev()
 *
 * Get the whole contents of revision <rev> from the RCSFILE <rfp>.  The
 * returned buffer is dynamically allocated and should be released using
 * cvs_buf_free() once the caller is done using it.
 */

BUF*
rcs_getrev(RCSFILE *rfp, RCSNUM *rev)
{
	int res;
	size_t len;
	void *bp;
	RCSNUM *crev;
	BUF *rbuf;
	struct rcs_delta *rdp = NULL;

	res = rcsnum_cmp(rfp->rf_head, rev, 0);
	if (res == 1) {
		cvs_log(LP_ERR, "sorry, can't travel in the future yet");
		return (NULL);
	} else {
		rdp = rcs_findrev(rfp, rfp->rf_head);
		if (rdp == NULL) {
			cvs_log(LP_ERR, "failed to get RCS HEAD revision");
			return (NULL);
		}

		len = strlen(rdp->rd_text);
		rbuf = cvs_buf_alloc(len, BUF_AUTOEXT);
		if (rbuf == NULL)
			return (NULL);
		cvs_buf_append(rbuf, rdp->rd_text, len);

		if (res != 0) {
			/* Apply patches backwards to get the right version.
			 * This will need some rework to support sub branches.
			 */
			crev = rcsnum_alloc();
			if (crev == NULL)
				return (NULL);
			rcsnum_cpy(rfp->rf_head, crev, 0);
			do {
				crev->rn_id[crev->rn_len - 1]--;
				rdp = rcs_findrev(rfp, crev);
				if (rdp == NULL)
					return (NULL);

				cvs_buf_putc(rbuf, '\0');
				bp = cvs_buf_release(rbuf);
				rbuf = rcs_patch((char *)bp, rdp->rd_text);
				if (rbuf == NULL)
					break;
			} while (rcsnum_cmp(crev, rev, 0) != 0);

			rcsnum_free(crev);
		}
	}


	return (rbuf);
}


/*
 * rcs_gethead()
 *
 * Get the head revision for the RCS file <rf>.
 */
BUF*
rcs_gethead(RCSFILE *rf)
{
	return rcs_getrev(rf, rf->rf_head);
}


/*
 * rcs_getrevbydate()
 *
 * Get an RCS revision by a specific date.
 */

RCSNUM*
rcs_getrevbydate(RCSFILE *rfp, struct tm *date)
{
	return (NULL);
}


/*
 * rcs_findrev()
 *
 * Find a specific revision's delta entry in the tree of the RCS file <rfp>.
 * The revision number is given in <rev>.
 * Returns a pointer to the delta on success, or NULL on failure.
 */

static struct rcs_delta*
rcs_findrev(RCSFILE *rfp, RCSNUM *rev)
{
	u_int cmplen;
	struct rcs_delta *rdp;
	struct rcs_dlist *hp;
	int found;
	
	cmplen = 2;
	hp = &(rfp->rf_delta);

	do {
		found = 0;
		TAILQ_FOREACH(rdp, hp, rd_list) {
			if (rcsnum_cmp(rdp->rd_num, rev, cmplen) == 0) {
				if (cmplen == rev->rn_len)
					return (rdp);

				hp = &(rdp->rd_snodes);
				cmplen += 2;
				found = 1;
				break;
			}
		}
	} while (found && cmplen < rev->rn_len);

	return (NULL);
}


/*
 * rcs_parse()
 *
 * Parse the contents of file <path>, which are in the RCS format.
 * Returns 0 on success, or -1 on failure.
 */

int
rcs_parse(RCSFILE *rfp)
{
	int ret;
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & RCS_RF_PARSED)
		return (0);

	pdp = (struct rcs_pdata *)malloc(sizeof(*pdp));
	if (pdp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate RCS parser data");
		return (-1);
	}
	memset(pdp, 0, sizeof(*pdp));

	pdp->rp_line = 1;
	pdp->rp_pttype = RCS_TOK_ERR;

	pdp->rp_file = fopen(rfp->rf_path, "r");
	if (pdp->rp_file == NULL) {
		cvs_log(LP_ERRNO, "failed to open RCS file `%s'", rfp->rf_path);
		rcs_freepdata(pdp);
		return (-1);
	}

	pdp->rp_buf = (char *)malloc(RCS_BUFSIZE);
	if (pdp->rp_buf == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate RCS parser buffer");
		rcs_freepdata(pdp);
		return (-1);
	}
	pdp->rp_blen = RCS_BUFSIZE;

	/* ditch the strict lock */
	rfp->rf_flags &= ~RCS_RF_SLOCK;
	rfp->rf_pdata = pdp;

	if (rcs_parse_admin(rfp) < 0) {
		rcs_freepdata(pdp);
		return (-1);
	}

	for (;;) {
		ret = rcs_parse_delta(rfp);
		if (ret == 0)
			break;
		else if (ret == -1) {
			rcs_freepdata(pdp);
			return (-1);
		}
	}

	ret = rcs_gettok(rfp);
	if (ret != RCS_TOK_DESC) {
		cvs_log(LP_ERR, "token `%s' found where RCS desc expected",
		    RCS_TOKSTR(rfp));
		rcs_freepdata(pdp);
		return (-1);
	}

	ret = rcs_gettok(rfp);
	if (ret != RCS_TOK_STRING) {
		cvs_log(LP_ERR, "token `%s' found where RCS desc expected",
		    RCS_TOKSTR(rfp));
		rcs_freepdata(pdp);
		return (-1);
	}

	rfp->rf_desc = strdup(RCS_TOKSTR(rfp));
	if (rfp->rf_desc == NULL) {
		cvs_log(LP_ERRNO, "failed to duplicate rcs token");
		rcs_freepdata(pdp);
		return (-1);
	}

	for (;;) {
		ret = rcs_parse_deltatext(rfp);
		if (ret == 0)
			break;
		else if (ret == -1) {
			rcs_freepdata(pdp);
			return (-1);
		}
	}

	cvs_log(LP_DEBUG, "RCS file `%s' parsed OK (%u lines)", rfp->rf_path,
	    pdp->rp_line);

	rcs_freepdata(pdp);

	rfp->rf_pdata = NULL;
	rfp->rf_flags |= RCS_RF_PARSED|RCS_RF_SYNCED;

	return (0);
}


/*
 * rcs_parse_admin()
 *
 * Parse the administrative portion of an RCS file.
 * Returns 0 on success, or -1 on failure.
 */

static int
rcs_parse_admin(RCSFILE *rfp)
{
	u_int i;
	int tok, ntok, hmask;
	struct rcs_key *rk;

	/* hmask is a mask of the headers already encountered */
	hmask = 0;
	for (;;) {
		tok = rcs_gettok(rfp);
		if (tok == RCS_TOK_ERR) {
			cvs_log(LP_ERR, "parse error in RCS admin section");
			return (-1);
		} else if (tok == RCS_TOK_NUM) {
			/* assume this is the start of the first delta */
			rcs_pushtok(rfp, RCS_TOKSTR(rfp), tok);
			return (0);
		}

		rk = NULL;
		for (i = 0; i < sizeof(rcs_keys)/sizeof(rcs_keys[0]); i++)
			if (rcs_keys[i].rk_id == tok)
				rk = &(rcs_keys[i]);

		if (hmask & (1 << tok)) {
			cvs_log(LP_ERR, "duplicate RCS key");
			return (-1);
		}
		hmask |= (1 << tok);

		switch (tok) {
		case RCS_TOK_HEAD:
		case RCS_TOK_BRANCH:
		case RCS_TOK_COMMENT:
		case RCS_TOK_EXPAND:
			ntok = rcs_gettok(rfp);
			if (ntok == RCS_TOK_SCOLON)
				break;
			if (ntok != rk->rk_val) {
				cvs_log(LP_ERR,
				    "invalid value type for RCS key `%s'",
				    rk->rk_str);
			}

			if (tok == RCS_TOK_HEAD) {
				rcsnum_aton(RCS_TOKSTR(rfp), NULL,
				    rfp->rf_head);
			} else if (tok == RCS_TOK_BRANCH) {
				rcsnum_aton(RCS_TOKSTR(rfp), NULL,
				    rfp->rf_branch);
			} else if (tok == RCS_TOK_COMMENT) {
				rfp->rf_comment = strdup(RCS_TOKSTR(rfp));
				if (rfp->rf_comment == NULL) {
					cvs_log(LP_ERRNO,
					    "failed to duplicate rcs token");
					return (-1);
				}
			} else if (tok == RCS_TOK_EXPAND) {
				rfp->rf_expand = strdup(RCS_TOKSTR(rfp));
				if (rfp->rf_expand == NULL) {
					cvs_log(LP_ERRNO,
					    "failed to duplicate rcs token");
					return (-1);
				}
			}

			/* now get the expected semi-colon */
			ntok = rcs_gettok(rfp);
			if (ntok != RCS_TOK_SCOLON) {
				cvs_log(LP_ERR,
				    "missing semi-colon after RCS `%s' key",
				    rk->rk_str);	
				return (-1);
			}
			break;
		case RCS_TOK_ACCESS:
			rcs_parse_access(rfp);
			break;
		case RCS_TOK_SYMBOLS:
			rcs_parse_symbols(rfp);
			break;
		case RCS_TOK_LOCKS:
			rcs_parse_locks(rfp);
			break;
		default:
			cvs_log(LP_ERR,
			    "unexpected token `%s' in RCS admin section",
			    RCS_TOKSTR(rfp));
			return (-1);
		}
	}

	return (0);
}


/*
 * rcs_parse_delta()
 *
 * Parse an RCS delta section and allocate the structure to store that delta's
 * information in the <rfp> delta list.
 * Returns 1 if the section was parsed OK, 0 if it is the last delta, and
 * -1 on error.
 */

static int
rcs_parse_delta(RCSFILE *rfp)
{
	int ret, tok, ntok, hmask;
	u_int i;
	char *tokstr;
	RCSNUM *datenum;
	struct rcs_delta *rdp;
	struct rcs_key *rk;

	rdp = (struct rcs_delta *)malloc(sizeof(*rdp));
	if (rdp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate RCS delta structure");
		return (-1);
	}
	memset(rdp, 0, sizeof(*rdp));

	rdp->rd_num = rcsnum_alloc();
	if (rdp->rd_num == NULL) {
		rcs_freedelta(rdp);
		return (-1);
	}
	rdp->rd_next = rcsnum_alloc();
	if (rdp->rd_next == NULL) {
		rcs_freedelta(rdp);
		return (-1);
	}

	TAILQ_INIT(&(rdp->rd_branches));

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_NUM) {
		cvs_log(LP_ERR, "unexpected token `%s' at start of delta",
		    RCS_TOKSTR(rfp));
		rcs_freedelta(rdp);
		return (-1);
	}
	rcsnum_aton(RCS_TOKSTR(rfp), NULL, rdp->rd_num);

	hmask = 0;
	ret = 0;
	tokstr = NULL;

	for (;;) {
		tok = rcs_gettok(rfp);
		if (tok == RCS_TOK_ERR) {
			cvs_log(LP_ERR, "parse error in RCS delta section");
			rcs_freedelta(rdp);
			return (-1);
		} else if (tok == RCS_TOK_NUM || tok == RCS_TOK_DESC) {
			rcs_pushtok(rfp, RCS_TOKSTR(rfp), tok);
			ret = (tok == RCS_TOK_NUM ? 1 : 0);
			break;
		}

		rk = NULL;
		for (i = 0; i < sizeof(rcs_keys)/sizeof(rcs_keys[0]); i++)
			if (rcs_keys[i].rk_id == tok)
				rk = &(rcs_keys[i]);

		if (hmask & (1 << tok)) {
			cvs_log(LP_ERR, "duplicate RCS key");
			rcs_freedelta(rdp);
			return (-1);
		}
		hmask |= (1 << tok);

		switch (tok) {
		case RCS_TOK_DATE:
		case RCS_TOK_AUTHOR:
		case RCS_TOK_STATE:
		case RCS_TOK_NEXT:
			ntok = rcs_gettok(rfp);
			if (ntok == RCS_TOK_SCOLON) {
				if (rk->rk_flags & RCS_VOPT)
					break;
				else {
					cvs_log(LP_ERR, "missing mandatory "
					    "value to RCS key `%s'",
					    rk->rk_str);
					rcs_freedelta(rdp);
					return (-1);
				}
			}

			if (ntok != rk->rk_val) {
				cvs_log(LP_ERR,
				    "invalid value type for RCS key `%s'",
				    rk->rk_str);
				rcs_freedelta(rdp);
				return (-1);
			}

			if (tokstr != NULL)
				free(tokstr);
			tokstr = strdup(RCS_TOKSTR(rfp));
			if (tokstr == NULL) {
				cvs_log(LP_ERRNO,
				    "failed to duplicate rcs token");
				rcs_freedelta(rdp);
				return (-1);
			}

			/* now get the expected semi-colon */
			ntok = rcs_gettok(rfp);
			if (ntok != RCS_TOK_SCOLON) {
				cvs_log(LP_ERR,
				    "missing semi-colon after RCS `%s' key",
				    rk->rk_str);	
				rcs_freedelta(rdp);
				return (-1);
			}

			if (tok == RCS_TOK_DATE) {
				datenum = rcsnum_alloc();
				if (datenum == NULL) {
					rcs_freedelta(rdp);
					return (-1);
				}
				rcsnum_aton(tokstr, NULL, datenum);
				if (datenum->rn_len != 6) {
					cvs_log(LP_ERR,
					    "RCS date specification has %s "
					    "fields",
					    (datenum->rn_len > 6) ? "too many" :
					    "missing");
					rcs_freedelta(rdp);
				}
				rdp->rd_date.tm_year = datenum->rn_id[0];
				rdp->rd_date.tm_mon = datenum->rn_id[1] - 1;
				rdp->rd_date.tm_mday = datenum->rn_id[2];
				rdp->rd_date.tm_hour = datenum->rn_id[3];
				rdp->rd_date.tm_min = datenum->rn_id[4];
				rdp->rd_date.tm_sec = datenum->rn_id[5];
				rcsnum_free(datenum);
			} else if (tok == RCS_TOK_AUTHOR) {
				rdp->rd_author = tokstr;
				tokstr = NULL;
			} else if (tok == RCS_TOK_STATE) {
				rdp->rd_state = tokstr;
				tokstr = NULL;
			} else if (tok == RCS_TOK_NEXT) {
				rcsnum_aton(tokstr, NULL, rdp->rd_next);
			}
			break;
		case RCS_TOK_BRANCHES:
			rcs_parse_branches(rfp, rdp);
			break;
		default:
			cvs_log(LP_ERR,
			    "unexpected token `%s' in RCS delta",
			    RCS_TOKSTR(rfp));
			rcs_freedelta(rdp);
			return (-1);
		}
	}

	if (tokstr != NULL)
		free(tokstr);

	TAILQ_INSERT_TAIL(&(rfp->rf_delta), rdp, rd_list);

	return (ret);
}


/*
 * rcs_parse_deltatext()
 *
 * Parse an RCS delta text section and fill in the log and text field of the
 * appropriate delta section.
 * Returns 1 if the section was parsed OK, 0 if it is the last delta, and
 * -1 on error.
 */

static int
rcs_parse_deltatext(RCSFILE *rfp)
{
	int tok;
	RCSNUM *tnum;
	struct rcs_delta *rdp;

	tok = rcs_gettok(rfp);
	if (tok == RCS_TOK_EOF)
		return (0);

	if (tok != RCS_TOK_NUM) {
		cvs_log(LP_ERR,
		    "unexpected token `%s' at start of RCS delta text",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tnum = rcsnum_alloc();
	if (tnum == NULL)
		return (-1);
	rcsnum_aton(RCS_TOKSTR(rfp), NULL, tnum);

	TAILQ_FOREACH(rdp, &(rfp->rf_delta), rd_list) {
		if (rcsnum_cmp(tnum, rdp->rd_num, 0) == 0)
			break;
	}
	rcsnum_free(tnum);

	if (rdp == NULL) {
		cvs_log(LP_ERR, "RCS delta text `%s' has no matching delta",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_LOG) {
		cvs_log(LP_ERR, "unexpected token `%s' where RCS log expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_STRING) {
		cvs_log(LP_ERR, "unexpected token `%s' where RCS log expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}
	rdp->rd_log = strdup(RCS_TOKSTR(rfp));
	if (rdp->rd_log == NULL) {
		cvs_log(LP_ERRNO, "failed to copy RCS deltatext log");
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_TEXT) {
		cvs_log(LP_ERR, "unexpected token `%s' where RCS text expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	tok = rcs_gettok(rfp);
	if (tok != RCS_TOK_STRING) {
		cvs_log(LP_ERR, "unexpected token `%s' where RCS text expected",
		    RCS_TOKSTR(rfp));
		return (-1);
	}

	rdp->rd_text = strdup(RCS_TOKSTR(rfp));
	if (rdp->rd_text == NULL) {
		cvs_log(LP_ERRNO, "failed to copy RCS delta text");
		return (-1);
	}

	return (1);
}


/*
 * rcs_parse_access()
 *
 * Parse the access list given as value to the `access' keyword.
 * Returns 0 on success, or -1 on failure.
 */

static int
rcs_parse_access(RCSFILE *rfp)
{
	int type;

	while ((type = rcs_gettok(rfp)) != RCS_TOK_SCOLON) {
		if (type != RCS_TOK_ID) {
			cvs_log(LP_ERR, "unexpected token `%s' in access list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}
	}

	return (0);
}


/*
 * rcs_parse_symbols()
 *
 * Parse the symbol list given as value to the `symbols' keyword.
 * Returns 0 on success, or -1 on failure.
 */

static int
rcs_parse_symbols(RCSFILE *rfp)
{
	int type;
	struct rcs_sym *symp;

	for (;;) {
		type = rcs_gettok(rfp);
		if (type == RCS_TOK_SCOLON)
			break;

		if (type != RCS_TOK_STRING) {
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		symp = (struct rcs_sym *)malloc(sizeof(*symp));
		if (symp == NULL) {
			cvs_log(LP_ERRNO, "failed to allocate RCS symbol");
			return (-1);
		}
		symp->rs_name = strdup(RCS_TOKSTR(rfp));
		if (symp->rs_name == NULL) {
			cvs_log(LP_ERRNO, "failed to duplicate rcs token");
			free(symp);
			return (-1);
		}

		symp->rs_num = rcsnum_alloc();
		if (symp->rs_num == NULL) {
			cvs_log(LP_ERRNO, "failed to allocate rcsnum info");
			free(symp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_COLON) {
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			free(symp->rs_name);
			free(symp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_NUM) {
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			free(symp->rs_name);
			free(symp);
			return (-1);
		}

		if (rcsnum_aton(RCS_TOKSTR(rfp), NULL, symp->rs_num) < 0) {
			cvs_log(LP_ERR, "failed to parse RCS NUM `%s'",
			    RCS_TOKSTR(rfp));
			rcsnum_free(symp->rs_num);
			free(symp->rs_name);
			free(symp);
			return (-1);
		}

		TAILQ_INSERT_HEAD(&(rfp->rf_symbols), symp, rs_list);
	}

	return (0);
}


/*
 * rcs_parse_locks()
 *
 * Parse the lock list given as value to the `locks' keyword.
 * Returns 0 on success, or -1 on failure.
 */

static int
rcs_parse_locks(RCSFILE *rfp)
{
	int type;
	struct rcs_lock *lkp;

	for (;;) {
		type = rcs_gettok(rfp);
		if (type == RCS_TOK_SCOLON)
			break;

		if (type != RCS_TOK_ID) {
			cvs_log(LP_ERR, "unexpected token `%s' in lock list",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		lkp = (struct rcs_lock *)malloc(sizeof(*lkp));
		if (lkp == NULL) {
			cvs_log(LP_ERRNO, "failed to allocate RCS lock");
			return (-1);
		}
		lkp->rl_num = rcsnum_alloc();
		if (lkp->rl_num == NULL) {
			free(lkp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_COLON) {
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			free(lkp);
			return (-1);
		}

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_NUM) {
			cvs_log(LP_ERR, "unexpected token `%s' in symbol list",
			    RCS_TOKSTR(rfp));
			free(lkp);
			return (-1);
		}

		if (rcsnum_aton(RCS_TOKSTR(rfp), NULL, lkp->rl_num) < 0) {
			cvs_log(LP_ERR, "failed to parse RCS NUM `%s'",
			    RCS_TOKSTR(rfp));
			free(lkp);
			return (-1);
		}

		TAILQ_INSERT_HEAD(&(rfp->rf_locks), lkp, rl_list);
	}

	/* check if we have a `strict' */
	type = rcs_gettok(rfp);
	if (type != RCS_TOK_STRICT) {
		rcs_pushtok(rfp, RCS_TOKSTR(rfp), type);
	} else {
		rfp->rf_flags |= RCS_RF_SLOCK;

		type = rcs_gettok(rfp);
		if (type != RCS_TOK_SCOLON) {
			cvs_log(LP_ERR,
			    "missing semi-colon after `strict' keyword");
			return (-1);
		}
	}

	return (0);
}

/*
 * rcs_parse_branches()
 *
 * Parse the list of branches following a `branches' keyword in a delta.
 * Returns 0 on success, or -1 on failure.
 */

static int
rcs_parse_branches(RCSFILE *rfp, struct rcs_delta *rdp)
{
	int type;
	struct rcs_branch *brp;

	for (;;) {
		type = rcs_gettok(rfp);
		if (type == RCS_TOK_SCOLON)
			break;

		if (type != RCS_TOK_NUM) {
			cvs_log(LP_ERR,
			    "unexpected token `%s' in list of branches",
			    RCS_TOKSTR(rfp));
			return (-1);
		}

		brp = (struct rcs_branch *)malloc(sizeof(*brp));
		if (brp == NULL) {
			cvs_log(LP_ERRNO, "failed to allocate RCS branch");
			return (-1);
		}
		brp->rb_num = rcsnum_alloc();
		if (brp->rb_num == NULL) {
			free(brp);
			return (-1);
		}

		rcsnum_aton(RCS_TOKSTR(rfp), NULL, brp->rb_num);

		TAILQ_INSERT_TAIL(&(rdp->rd_branches), brp, rb_list);
	}

	return (0);
}


/*
 * rcs_freedelta()
 *
 * Free the contents of a delta structure.
 */

void
rcs_freedelta(struct rcs_delta *rdp)
{
	struct rcs_branch *rb;
	struct rcs_delta *crdp;

	if (rdp->rd_num != NULL)
		rcsnum_free(rdp->rd_num);
	if (rdp->rd_next != NULL)
		rcsnum_free(rdp->rd_next);

	if (rdp->rd_author != NULL)
		free(rdp->rd_author);
	if (rdp->rd_state != NULL)
		free(rdp->rd_state);
	if (rdp->rd_log != NULL)
		free(rdp->rd_log);
	if (rdp->rd_text != NULL)
		free(rdp->rd_text);

	while ((rb = TAILQ_FIRST(&(rdp->rd_branches))) != NULL) {
		TAILQ_REMOVE(&(rdp->rd_branches), rb, rb_list);
		rcsnum_free(rb->rb_num);
		free(rb);
	}

	while ((crdp = TAILQ_FIRST(&(rdp->rd_snodes))) != NULL) {
		TAILQ_REMOVE(&(rdp->rd_snodes), crdp, rd_list);
		rcs_freedelta(crdp);
	}

	free(rdp);
}


/*
 * rcs_freepdata()
 *
 * Free the contents of the parser data structure.
 */

static void
rcs_freepdata(struct rcs_pdata *pd)
{
	if (pd->rp_file != NULL)
		(void)fclose(pd->rp_file);
	if (pd->rp_buf != NULL)
		free(pd->rp_buf);
	free(pd);
}


/*
 * rcs_gettok()
 *
 * Get the next RCS token from the string <str>.
 */

static int
rcs_gettok(RCSFILE *rfp)
{
	u_int i;
	int ch, last, type;
	char *bp, *bep;
	struct rcs_pdata *pdp = (struct rcs_pdata *)rfp->rf_pdata;

	type = RCS_TOK_ERR;
	bp = pdp->rp_buf;
	bep = pdp->rp_buf + pdp->rp_blen - 1;
	*bp = '\0';

	if (pdp->rp_pttype != RCS_TOK_ERR) {
		type = pdp->rp_pttype;
		strlcpy(pdp->rp_buf, pdp->rp_ptok, pdp->rp_blen);
		pdp->rp_pttype = RCS_TOK_ERR;
		return (type);
	}

	/* skip leading whitespace */
	/* XXX we must skip backspace too for compatibility, should we? */
	do {
		ch = getc(pdp->rp_file);
		if (ch == '\n')
			pdp->rp_line++;
	} while (isspace(ch));

	if (ch == EOF) {
		type = RCS_TOK_EOF;
	} else if (ch == ';') {
		type = RCS_TOK_SCOLON;
	} else if (ch == ':') {
		type = RCS_TOK_COLON;
	} else if (isalpha(ch)) {
		*(bp++) = ch;
		while (bp <= bep - 1) {
			ch = getc(pdp->rp_file);
			if (!isalnum(ch) && ch != '_' && ch != '-') {
				ungetc(ch, pdp->rp_file);
				break;
			}
			*(bp++) = ch;
		}
		*bp = '\0';

		for (i = 0; i < sizeof(rcs_keys)/sizeof(rcs_keys[0]); i++) {
			if (strcmp(rcs_keys[i].rk_str, pdp->rp_buf) == 0) {
				type = rcs_keys[i].rk_id;
				break;
			}
		}

		/* not a keyword, assume it's just a string */
		if (type == RCS_TOK_ERR)
			type = RCS_TOK_STRING;

	} else if (ch == '@') {
		/* we have a string */
		for (;;) {
			ch = getc(pdp->rp_file);
			if (ch == '@') {
				ch = getc(pdp->rp_file);
				if (ch != '@') {
					ungetc(ch, pdp->rp_file);
					break;
				}
			} else if (ch == '\n')
				pdp->rp_line++;

			*(bp++) = ch;
			if (bp == bep)
				break;
		}

		*bp = '\0';
		type = RCS_TOK_STRING;
	} else if (isdigit(ch)) {
		*(bp++) = ch;
		last = ch;
		type = RCS_TOK_NUM;

		for (;;) {
			ch = getc(pdp->rp_file);
			if (bp == bep)
				break;
			if (!isdigit(ch) && ch != '.') {
				ungetc(ch, pdp->rp_file);
				break;
			}

			if (last == '.' && ch == '.') {
				type = RCS_TOK_ERR;
				break;
			}
			last = ch;
			*(bp++) = ch;
		}
		*(bp) = '\0';
	}

	return (type);
}


/*
 * rcs_pushtok()
 *
 * Push a token back in the parser's token buffer.
 */

static int
rcs_pushtok(RCSFILE *rfp, const char *tok, int type)
{
	struct rcs_pdata *pdp = (struct rcs_pdata *)rfp->rf_pdata;

	if (pdp->rp_pttype != RCS_TOK_ERR)
		return (-1);

	pdp->rp_pttype = type;
	strlcpy(pdp->rp_ptok, tok, sizeof(pdp->rp_ptok));
	return (0);
}


/*
 * rcs_stresc()
 *
 * Performs either escaping or unescaping of the string stored in <str>.
 * The operation is to escape special RCS characters if the <esc> argument
 * is 1, or unescape otherwise.  The result is stored in the <buf> destination
 * buffer, and <blen> must originally point to the size of <buf>.
 * Returns the number of bytes which have been read from the source <str> and
 * operated on.  The <blen> parameter will contain the number of bytes
 * actually copied in <buf>.
 */

size_t
rcs_stresc(int esc, const char *str, char *buf, size_t *blen)
{
	size_t rlen;
	const char *sp;
	char *bp, *bep;

	rlen = 0;
	bp = buf;
	bep = buf + *blen - 1;

	for (sp = str; (*sp != '\0') && (bp <= (bep - 1)); sp++) {
		if (*sp == '@') {
			if (esc) {
				if (bp > (bep - 2))
					break;
				*(bp++) = '@';
			} else {
				sp++;
				if (*sp != '@') {
					cvs_log(LP_WARN,
					    "unknown escape character `%c' in "
					    "RCS file", *sp);
					if (*sp == '\0')
						break;
				}
			}
		}

		*(bp++) = *sp;
	}

	*bp = '\0';
	*blen = (bp - buf);
	return (sp - str);
}


/*
 * rcs_splitlines()
 *
 * Split the contents of a file into a list of lines.
 */

static struct rcs_foo*
rcs_splitlines(const char *fcont)
{
	char *dcp;
	struct rcs_foo *foo;
	struct rcs_line *lp;

	foo = (struct rcs_foo *)malloc(sizeof(*foo));
	if (foo == NULL) {
		cvs_log(LP_ERR, "failed to allocate line structure");
		return (NULL);
	}
	TAILQ_INIT(&(foo->rl_lines));
	foo->rl_nblines = 0;
	foo->rl_data = strdup(fcont);
	if (foo->rl_data == NULL) {
		cvs_log(LP_ERRNO, "failed to copy file contents");
		free(foo);
		return (NULL);
	}

	/*
	 * Add a first bogus line with line number 0.  This is used so we
	 * can position the line pointer before 1 when changing the first line
	 * in rcs_patch().
	 */
	lp = (struct rcs_line *)malloc(sizeof(*lp));
	if (lp == NULL)
		return (NULL);

	lp->rl_line = NULL;
	lp->rl_lineno = 0;
	TAILQ_INSERT_TAIL(&(foo->rl_lines), lp, rl_list);


	for (dcp = foo->rl_data; *dcp != '\0';) {
		lp = (struct rcs_line *)malloc(sizeof(*lp));
		if (lp == NULL) {
			cvs_log(LP_ERR, "failed to allocate line entry");
			return (NULL);
		}

		lp->rl_line = dcp;
		lp->rl_lineno = ++(foo->rl_nblines);
		TAILQ_INSERT_TAIL(&(foo->rl_lines), lp, rl_list);

		dcp = strchr(dcp, '\n');
		if (dcp == NULL) {
			break;
		}
		*(dcp++) = '\0';
	}

	return (foo);
}

static void
rcs_freefoo(struct rcs_foo *fp)
{
	struct rcs_line *lp;

	while ((lp = TAILQ_FIRST(&fp->rl_lines)) != NULL) {
		TAILQ_REMOVE(&fp->rl_lines, lp, rl_list);
		free(lp);
	}
	free(fp->rl_data);
	free(fp);
}
