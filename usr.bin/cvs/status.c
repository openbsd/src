/*	$OpenBSD: status.c,v 1.20 2005/04/27 04:54:46 jfb Exp $	*/
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


#define CVS_STATUS_SEP \
 "==================================================================="


const char *cvs_statstr[] = {
	"Unknown",
	"Up-to-date",
	"Locally Modified",
	"Added",
	"Removed",
	"Conflict",
	"Patched",
	"Needs Checkout",
};


int cvs_status_remote    (CVSFILE *, void *);
int cvs_status_local     (CVSFILE *, void *);
int cvs_status_options   (char *, int, char **, int *);
int cvs_status_sendflags (struct cvsroot *);

struct cvs_cmd_info cvs_status = {
	cvs_status_options,
	cvs_status_sendflags,
	cvs_status_remote,
	NULL, NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	CVS_REQ_STATUS,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};

static int verbose = 0;

int
cvs_status_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'l':
			cvs_status.file_flags &= ~CF_RECURSE;
			break;
		case 'R':
			cvs_status.file_flags |= CF_RECURSE;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

int
cvs_status_sendflags(struct cvsroot *root)
{
	if (verbose && (cvs_sendarg(root, "-v", 0) < 0))
		return (CVS_EX_PROTO);
	return (0);
}

/*
 * cvs_status_remote()
 *
 * Get the status of a single file.
 */
int
cvs_status_remote(CVSFILE *cfp, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cfp);
	repo = CVS_DIR_REPO(cfp);

	if (cfp->cf_type == DT_DIR) {
		if (cfp->cf_cvstat == CVS_FST_UNKNOWN)
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cfp));
		else
			ret = cvs_senddir(root, cfp);
		return (ret);
	}

	cvs_file_getpath(cfp, fpath, sizeof(fpath));

	if (cvs_sendentry(root, cfp) < 0) {
		return (-1);
	}

	switch (cfp->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cfp->cf_name);
		break;
	case CVS_FST_UPTODATE:
		ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, cfp->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		ret = cvs_sendreq(root, CVS_REQ_MODIFIED, cfp->cf_name);
		if (ret == 0)
			ret = cvs_sendfile(root, fpath);
	default:
		break;
	}

	return (ret);
}

int
cvs_status_local(CVSFILE *cfp, void *arg)
{
	int l;
	char *repo, buf[MAXNAMLEN], fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;

	if (cfp->cf_type == DT_DIR)
		return (0);

	root = CVS_DIR_ROOT(cfp);
	repo = CVS_DIR_REPO(cfp);

	cvs_file_getpath(cfp, fpath, sizeof(fpath));

	if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
		cvs_log(LP_WARN, "I know nothing about %s", fpath);
		return (0);
	}

	l = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, CVS_FILE_NAME(cfp), RCS_FILE_EXT);
	if (l == -1 || l >= (int)sizeof(rcspath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", rcspath);
		return (-1);
	}

	rf = rcs_open(rcspath, RCS_READ);
	if (rf == NULL) {
		return (-1);
	}

	buf[0] = '\0';
	if (cfp->cf_cvstat == CVS_FST_LOST)
		strlcpy(buf, "No file ", sizeof(buf));
	strlcat(buf, CVS_FILE_NAME(cfp), sizeof(buf));

	cvs_printf(CVS_STATUS_SEP "\nFile: %-18sStatus: %s\n\n",
	    buf, cvs_statstr[cfp->cf_cvstat]);

	if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
		snprintf(buf, sizeof(buf), "No entry for %s",
		    CVS_FILE_NAME(cfp));
	} else {
		snprintf(buf, sizeof(buf), "%s %s",
		    rcsnum_tostr(cfp->cf_lrev, buf, sizeof(buf)),
		    "date here");
	}

	cvs_printf("   Working revision:    %s\n", buf);
	rcsnum_tostr(rf->rf_head, buf, sizeof(buf));
	cvs_printf("   Repository revision: %s %s\n", buf, rcspath);
	cvs_printf("   Sticky Tag:          %s\n", "(none)");
	cvs_printf("   Sticky Date:         %s\n", "(none)");
	cvs_printf("   Sticky Options:      %s\n", "(none)");

	rcs_close(rf);

	return (0);
}
