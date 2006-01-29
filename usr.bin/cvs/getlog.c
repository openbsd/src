/*	$OpenBSD: getlog.c,v 1.53 2006/01/29 11:09:45 xsa Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "proto.h"


#define CVS_GLOG_RFONLY		0x01
#define CVS_GLOG_HDONLY		0x02


#define CVS_GETLOG_REVSEP	"----------------------------"
#define CVS_GETLOG_REVEND \
 "============================================================================="

static int	cvs_getlog_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_getlog_remote(CVSFILE *, void *);
static int	cvs_getlog_local(CVSFILE *, void *);
static int	cvs_getlog_pre_exec(struct cvsroot *);

struct cvs_cmd cvs_cmd_log = {
	CVS_OP_LOG, CVS_REQ_LOG, "log",
	{ "lo" },
	"Print out history information for files",
	"[-bhlNRt] [-d dates] [-r revisions] [-s states] [-w logins]",
	"bd:hlNRr:s:tw:",
	NULL,
	CF_NOSYMS | CF_IGNORE | CF_SORT | CF_RECURSE,
	cvs_getlog_init,
	cvs_getlog_pre_exec,
	cvs_getlog_remote,
	cvs_getlog_local,
	NULL,
	NULL,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};


struct cvs_cmd cvs_cmd_rlog = {
	CVS_OP_LOG, CVS_REQ_LOG, "log",
	{ "lo" },
	"Print out history information for files",
	"[-bhlNRt] [-d dates] [-r revisions] [-s states] [-w logins]",
	"d:hlRr:",
	NULL,
	CF_NOSYMS | CF_IGNORE | CF_SORT | CF_RECURSE,
	cvs_getlog_init,
	cvs_getlog_pre_exec,
	cvs_getlog_remote,
	cvs_getlog_local,
	NULL,
	NULL,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};

static int log_rfonly = 0;
static int log_honly = 0;
static int log_lhonly = 0;
static int log_notags = 0;

static int
cvs_getlog_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'b':
			break;
		case 'd':
			break;
		case 'h':
			log_honly = 1;
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'N':
			log_notags = 1;
			break;
		case 'R':
			log_rfonly = 1;
			break;
		case 'r':
			break;
		case 's':
			break;
		case 't':
			log_lhonly = 1;
			break;
		case 'w':
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_getlog_pre_exec(struct cvsroot *root)
{
	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (log_honly == 1)
			cvs_sendarg(root, "-h", 0);
		if (log_notags == 1)
			cvs_sendarg(root, "-N", 0);
		if (log_rfonly == 1)
			cvs_sendarg(root, "-R", 0);
		if (log_lhonly == 1)
			cvs_sendarg(root, "-t", 0);
	}
	return (0);
}

/*
 * cvs_getlog_remote()
 *
 */
static int
cvs_getlog_remote(CVSFILE *cf, void *arg)
{
	char fpath[MAXPATHLEN];
	struct cvsroot *root;

	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		else
			cvs_senddir(root, cf);
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	cvs_sendentry(root, cf);

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		cvs_sendreq(root, CVS_REQ_ISMODIFIED, cf->cf_name);
		break;
	default:
		break;
	}

	return (0);
}



static int
cvs_getlog_local(CVSFILE *cf, void *arg)
{
	int nrev;
	char rcspath[MAXPATHLEN], numbuf[64];
	RCSFILE *rf;
	struct rcs_sym *sym;
	struct rcs_delta *rdp;
	struct rcs_access *acp;

	nrev = 0;

	if (cf->cf_cvstat == CVS_FST_ADDED) {
		if (verbosity > 0)
			cvs_log(LP_WARN, "%s has been added, but not committed",
			    cf->cf_name);
		return (0);
	}

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		if (verbosity > 0)
			cvs_log(LP_WARN, "nothing known about %s", cf->cf_name);
		return (0);
	}

	if (cf->cf_type == DT_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Logging %s", cf->cf_name);
		return (0);
	}

	cvs_rcs_getpath(cf, rcspath, sizeof(rcspath));

	if (log_rfonly == 1) {
		cvs_printf("%s\n", rcspath);
		return (0);
	}

	if ((rf = rcs_open(rcspath, RCS_READ|RCS_PARSE_FULLY)) == NULL)
		fatal("cvs_getlog_local: rcs_open `%s': %s", rcspath,
		    strerror(rcs_errno));

	cvs_printf("\nRCS file: %s", rcspath);
	cvs_printf("\nWorking file: %s", cf->cf_name);
	cvs_printf("\nhead:");
	if (rcs_head_get(rf) != NULL) {
		cvs_printf(" %s",
		    rcsnum_tostr(rcs_head_get(rf), numbuf, sizeof(numbuf)));
	}
	cvs_printf("\nbranch:");
	if (rcs_branch_get(rf) != NULL) {
		cvs_printf(" %s",
		    rcsnum_tostr(rcs_branch_get(rf), numbuf, sizeof(numbuf)));
	}
	cvs_printf("\nlocks:%s", (rf->rf_flags & RCS_SLOCK) ? " strict" : "");

	cvs_printf("\naccess list:\n");
	TAILQ_FOREACH(acp, &(rf->rf_access), ra_list)
		cvs_printf("\t%s\n", acp->ra_name);

	if (log_notags == 0) {
		cvs_printf("symbolic names:\n");
		TAILQ_FOREACH(sym, &(rf->rf_symbols), rs_list)
			cvs_printf("\t%s: %s\n", sym->rs_name,
			    rcsnum_tostr(sym->rs_num, numbuf, sizeof(numbuf)));
	}

	cvs_printf("keyword substitution: %s\n",
	    rf->rf_expand == NULL ? "kv" : rf->rf_expand);

	cvs_printf("total revisions: %u", rf->rf_ndelta);

	if ((log_honly == 0) && (log_lhonly == 0))
		cvs_printf(";\tselected revisions: %u", nrev);

	cvs_printf("\n");

	if ((log_honly == 0) || (log_lhonly == 1))
		cvs_printf("description:\n%s", rf->rf_desc);

	if ((log_honly == 0) && (log_lhonly == 0)) {
		TAILQ_FOREACH(rdp, &(rf->rf_delta), rd_list) {
			rcsnum_tostr(rdp->rd_num, numbuf, sizeof(numbuf));
			cvs_printf(CVS_GETLOG_REVSEP "\nrevision %s\n", numbuf);
			cvs_printf("date: %d/%02d/%02d %02d:%02d:%02d;"
			    "  author: %s;  state: %s;\n",
			    rdp->rd_date.tm_year + 1900,
			    rdp->rd_date.tm_mon + 1,
			    rdp->rd_date.tm_mday, rdp->rd_date.tm_hour,
			    rdp->rd_date.tm_min, rdp->rd_date.tm_sec,
			    rdp->rd_author, rdp->rd_state);
			cvs_printf("%s", rdp->rd_log);
		}
	}

	cvs_printf(CVS_GETLOG_REVEND "\n");

	rcs_close(rf);

	return (0);
}
