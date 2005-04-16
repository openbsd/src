/*	$OpenBSD: getlog.c,v 1.22 2005/04/16 20:05:05 xsa Exp $	*/
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <paths.h>

#include "cvs.h"
#include "log.h"
#include "file.h"
#include "proto.h"


#define CVS_GLOG_RFONLY    0x01
#define CVS_GLOG_HDONLY    0x02


#define CVS_GETLOG_REVSEP   "----------------------------"
#define CVS_GETLOG_REVEND \
 "============================================================================="

static int  cvs_getlog_remote  (CVSFILE *, void *);
static int  cvs_getlog_local   (CVSFILE *, void *);

int cvs_getlog_options(char *, int, char **, int *);

struct cvs_cmd_info cvs_getlog = {
	cvs_getlog_options,
	NULL,
	cvs_getlog_remote,
	NULL, NULL,
	CF_RECURSE,
	CVS_REQ_LOG,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};

static int log_rfonly = 0;
static int log_honly = 0;
static int log_notags = 0;

int
cvs_getlog_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'd':
			break;
		case 'h':
			log_honly = 1;
			break;
		case 'l':
			cvs_getlog.file_flags &= ~CF_RECURSE;
			break;
		case 'N':
			log_notags = 1;
			break;
		case 'R':
			log_rfonly = 1;
			break;
		case 'r':
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}


/*
 * cvs_getlog_remote()
 *
 */
static int
cvs_getlog_remote(CVSFILE *cf, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN];
	struct cvsroot *root;
	struct cvs_ent *entp;

	ret = 0;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cf));
		else
			ret = cvs_senddir(root, cf);
		return (ret);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));
	entp = cvs_ent_getent(fpath);

	if ((entp != NULL) && (cvs_sendentry(root, entp) < 0)) {
		cvs_ent_free(entp);
		return (-1);
	}

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED, cf->cf_name);
		break;
	default:
		break;
	}

	if (entp != NULL)
		cvs_ent_free(entp);
	return (ret);
}



static int
cvs_getlog_local(CVSFILE *cf, void *arg)
{
	int nrev, l;
	char rcspath[MAXPATHLEN], numbuf[64];
	char *repo;
	RCSFILE *rf;
	struct rcs_sym *sym;
	struct rcs_delta *rdp;
	struct rcs_access *acp;
	struct cvsroot *root;

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		cvs_log(LP_WARN, "nothing known about %s", cf->cf_name);
		return (0);
	}

	if (cf->cf_type == DT_DIR) {
		cvs_log(LP_INFO, "Logging %s", cf->cf_name);
		return (0);
	}

	nrev = 0;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	l = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, CVS_FILE_NAME(cf), RCS_FILE_EXT);
	if (l == -1 || l >= (int)sizeof(rcspath)) {
		errno = ENAMETOOLONG;
		 cvs_log(LP_ERRNO, "%s", rcspath);
		return (-1);
	}

	if (log_rfonly) {
		cvs_printf("%s\n", rcspath);
		return (0);
	}

	rf = rcs_open(rcspath, RCS_READ);
	if (rf == NULL)
		return (-1);

	cvs_printf("\nRCS file: %s\nWorking file: %s\n", rcspath, cf->cf_name);
	cvs_printf("head: %s\n",
	    rcsnum_tostr(rcs_head_get(rf), numbuf, sizeof(numbuf)));
	cvs_printf("branch: %s\n",
	    rcsnum_tostr(rcs_branch_get(rf), numbuf, sizeof(numbuf)));
	cvs_printf("locks: %s\n", (rf->rf_flags & RCS_SLOCK) ? "strict" : "");

	cvs_printf("access list:\n");
	TAILQ_FOREACH(acp, &(rf->rf_access), ra_list)
		cvs_printf("\t%s\n", acp->ra_name);

	if (!log_notags) {
		cvs_printf("symbolic names:\n");
		TAILQ_FOREACH(sym, &(rf->rf_symbols), rs_list)
			cvs_printf("\t%s: %s\n", sym->rs_name,
			    rcsnum_tostr(sym->rs_num, numbuf, sizeof(numbuf)));
	}

	cvs_printf("keyword substitution: %s\n", "");

	if (log_honly)
		cvs_printf("total revisions: %u;\n", rf->rf_ndelta);
	else {
		cvs_printf("total revisions: %u;\tselected revisions: %u\n",
		    rf->rf_ndelta, nrev);
		cvs_printf("description:\n%s", rf->rf_desc);
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
