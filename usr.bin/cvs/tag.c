/*	$OpenBSD: tag.c,v 1.2 2004/12/14 22:30:48 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2004 Joris Vink <amni@pandora.be>
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


int cvs_tag_file (CVSFILE *, void *);


/*
 * cvs_tag()
 *
 * Handler for the `cvs tag' command.
 * Returns 0 on success, or one of the known exit codes on error.
 */
int
cvs_tag(int argc, char **argv)
{
	int ch, flags;
	struct cvsroot *root;
	char *tag, *old_tag;
	int branch, delete;

	old_tag = NULL;
	branch = delete = 0;
	flags = CF_SORT|CF_IGNORE|CF_RECURSE;

	while ((ch = getopt(argc, argv, "bdlr:")) != -1) {
		switch (ch) {
		case 'b':
			branch = 1;
			break;
		case 'd':
			delete = 1;
			break;
		case 'l':
			flags &= ~CF_RECURSE;
			break;
		case 'r':
			old_tag = optarg;
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		return (EX_USAGE);
	} else {
		tag = argv[0];
		argc--;
		argv++;
	}

	if (branch && delete) {
		cvs_log(LP_WARN, "ignoring -b with -d options");
		branch = 0;
	}

	if (delete && old_tag)
		old_tag = NULL;

	if (argc == 0)
		cvs_files = cvs_file_get(".", flags);
	else
		cvs_files = cvs_file_getspec(argv, argc, 0);
	if (cvs_files == NULL)
		return (EX_DATAERR);

	root = CVS_DIR_ROOT(cvs_files);
	if (root == NULL) {
		cvs_log(LP_ERR,
		    "No CVSROOT specified!  Please use the `-d' option");
		cvs_log(LP_ERR,
		    "or set the CVSROOT environment variable.");
		return (EX_USAGE);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_connect(root) < 0)
			return (EX_PROTOCOL);
		if (branch && (cvs_sendarg(root, "-b", 0) < 0))
			return (EX_PROTOCOL);
		if (delete && (cvs_sendarg(root, "-d", 0) < 0))
			return (EX_PROTOCOL);
		if (old_tag) {
			cvs_sendarg(root, "-r", 0);
			cvs_sendarg(root, old_tag, 0);
		}
		if (cvs_sendarg(root, tag, 0) < 0)
			return (EX_PROTOCOL);
	}

	cvs_file_examine(cvs_files, cvs_tag_file, NULL);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_senddir(root, cvs_files) < 0)
			return (EX_PROTOCOL);
		if (cvs_sendreq(root, CVS_REQ_TAG, NULL) < 0)
			return (EX_PROTOCOL);
	}

	return (0);
}


/*
 * cvs_tag_file()
 *
 * Get the status of a single file.
 */
int
cvs_tag_file(CVSFILE *cfp, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvs_ent *entp;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cfp);

	if ((root->cr_method != CVS_METHOD_LOCAL) && (cfp->cf_type == DT_DIR)) {
		if (cvs_senddir(root, cfp) < 0)
			return (-1);
		return (0);
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
			ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED, 
			    CVS_FILE_NAME(cfp));
		default:
			break;
		}
	} else {
		if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
			cvs_log(LP_WARN, "I know nothing about %s", fpath);
			return (0);
		}

		if (cfp->cf_parent != NULL)
			repo = cfp->cf_parent->cf_ddat->cd_repo;
		else
			repo = NULL;

		snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
		    root->cr_dir, repo, CVS_FILE_NAME(cfp), RCS_FILE_EXT);

		rf = rcs_open(rcspath, RCS_MODE_READ);
		if (rf == NULL) {
			cvs_ent_free(entp);
			return (-1);
		}

		rcs_close(rf);
	}

	if (entp != NULL)
		cvs_ent_free(entp);

	return (ret);
}
