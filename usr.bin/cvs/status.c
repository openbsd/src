/*	$OpenBSD: status.c,v 1.11 2005/04/03 17:32:50 xsa Exp $	*/
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
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


const char *cvs_statstr[] = {
	"Unknown",
	"Up to date",
	"Locally Modified",
	"Added",
	"Removed",
	"Conflict",
	"Patched",
};


int cvs_status_file(CVSFILE *, void *);
int cvs_status_options(char *, int, char **, int *);
int cvs_status_sendflags(struct cvsroot *);

struct cvs_cmd_info cvs_status = {
	cvs_status_options,
	cvs_status_sendflags,
	cvs_status_file,
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
			return (EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

int
cvs_status_sendflags(struct cvsroot *root)
{
	if (verbose && (cvs_sendarg(root, "-v", 0) < 0))
		return (-1);
	return (0);
}

/*
 * cvs_status_file()
 *
 * Get the status of a single file.
 */
int
cvs_status_file(CVSFILE *cfp, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvs_ent *entp;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cfp);
	repo = CVS_DIR_REPO(cfp);

	if (cfp->cf_type == DT_DIR) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cfp->cf_cvstat == CVS_FST_UNKNOWN)
				ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
				    CVS_FILE_NAME(cfp));
			else
				ret = cvs_senddir(root, cfp);
		}

		return (ret);
	}

	cvs_file_getpath(cfp, fpath, sizeof(fpath));
	entp = cvs_ent_getent(fpath);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if ((entp != NULL) && (cvs_sendentry(root, entp) < 0)) {
			cvs_ent_free(entp);
			return (-1);
		}

		switch (cfp->cf_cvstat) {
		case CVS_FST_UNKNOWN:
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cfp));
			break;
		case CVS_FST_UPTODATE:
			ret = cvs_sendreq(root, CVS_REQ_UNCHANGED,
			    CVS_FILE_NAME(cfp));
			break;
		case CVS_FST_MODIFIED:
			ret = cvs_sendreq(root, CVS_REQ_MODIFIED,
			    CVS_FILE_NAME(cfp));
			if (ret == 0)
				ret = cvs_sendfile(root, fpath);
		default:
			break;
		}
	} else {
		if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
			cvs_log(LP_WARN, "I know nothing about %s", fpath);
			return (0);
		}

		snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
		    root->cr_dir, repo, CVS_FILE_NAME(cfp), RCS_FILE_EXT);

		rf = rcs_open(rcspath, RCS_READ);
		if (rf == NULL) {
			if (entp != NULL)
				cvs_ent_free(entp);
			return (-1);
		}

		rcs_close(rf);
	}

	if (entp != NULL)
		cvs_ent_free(entp);
	return (ret);
}
