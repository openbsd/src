/*	$OpenBSD: status.c,v 1.4 2004/12/07 17:10:56 tedu Exp $	*/
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


int cvs_status_file (CVSFILE *, void *);


/*
 * cvs_status()
 *
 * Handler for the `cvs status' command.
 * Returns 0 on success, or one of the known exit codes on error.
 */
int
cvs_status(int argc, char **argv)
{
	int i, ch, flags;
	struct cvs_file *cf;

	cf = NULL;
	flags = CF_SORT|CF_IGNORE|CF_RECURSE;

	while ((ch = getopt(argc, argv, "F:flm:Rr:")) != -1) {
		switch (ch) {
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		cf = cvs_file_get(".", flags);
		if (cf == NULL) {
			return (EX_DATAERR);
		}

		cvs_file_examine(cf, cvs_status_file, NULL);
	} else {
		for (i = 0; i < argc; i++) {
			cf = cvs_file_get(argv[i], flags);
		}
	}

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
	char *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvs_ent *entp;
	struct cvsroot *root;

	cvs_file_getpath(cfp, fpath, sizeof(fpath));
	cvs_log(LP_DEBUG, "%s: getting status for %s", __func__, fpath);

	if (cfp->cf_type == DT_DIR) {
		root = cfp->cf_ddat->cd_root;
		if ((cfp->cf_parent == NULL) ||
		    (root != cfp->cf_parent->cf_ddat->cd_root)) {
			cvs_connect(root);
		}

		cvs_senddir(root, cfp);
		return (0);
	} else
		root = cfp->cf_parent->cf_ddat->cd_root;

	rf = NULL;
	if (cfp->cf_parent != NULL)
		repo = cfp->cf_parent->cf_ddat->cd_repo;
	else
		repo = NULL;

	if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
		if (root->cr_method == CVS_METHOD_LOCAL)
			cvs_log(LP_WARN, "I know nothing about %s", fpath);
		else
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cfp));
		return (0);
	}

	entp = cvs_ent_getent(fpath);
	if (entp == NULL)
		return (-1);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_sendentry(root, entp) < 0) {
			cvs_ent_free(entp);
			return (-1);
		}
	}

	if (cfp->cf_cvstat == CVS_FST_UPTODATE) {
		if (root->cr_method != CVS_METHOD_LOCAL)
			cvs_sendreq(root, CVS_REQ_UNCHANGED,
			    CVS_FILE_NAME(cfp));
		cvs_ent_free(entp);
		return (0);
	}

	/* at this point, the file is modified */
	if (root->cr_method != CVS_METHOD_LOCAL) {
		cvs_sendreq(root, CVS_REQ_MODIFIED, CVS_FILE_NAME(cfp));
		cvs_sendfile(root, fpath);
	} else {
		snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
		    root->cr_dir, repo, CVS_FILE_NAME(cfp), RCS_FILE_EXT);

		rf = rcs_open(rcspath, RCS_MODE_READ);
		if (rf == NULL) {
			cvs_ent_free(entp);
			return (-1);
		}

		rcs_close(rf);
	}
	cvs_ent_free(entp);
	return (0);
}
