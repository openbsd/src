/*	$OpenBSD: commit.c,v 1.8 2004/12/06 21:03:12 deraadt Exp $	*/
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
#include <sys/queue.h>
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
#include "buf.h"
#include "proto.h"




int    cvs_commit_prepare  (CVSFILE *, void *);
int    cvs_commit_file     (CVSFILE *, void *);


/*
 * cvs_commit()
 *
 * Handler for the `cvs commit' command.
 */

int
cvs_commit(int argc, char **argv)
{
	int i, ch, recurse, flags;
	char *msg, *mfile;
	struct cvs_flist cl;
	struct cvsroot *root;

	flags = CF_RECURSE|CF_IGNORE|CF_SORT;
	recurse = 1;
	mfile = NULL;
	msg = NULL;
	TAILQ_INIT(&cl);

	while ((ch = getopt(argc, argv, "F:flm:R")) != -1) {
		switch (ch) {
		case 'F':
			mfile = optarg;
			break;
		case 'f':
			recurse = 0;
			break;
		case 'l':
			recurse = 0;
			break;
		case 'm':
			msg = optarg;
			break;
		case 'R':
			recurse = 1;
			break;
		default:
			return (EX_USAGE);
		}
	}

	if ((msg != NULL) && (mfile != NULL)) {
		cvs_log(LP_ERR, "the -F and -m flags are mutually exclusive");
		return (EX_USAGE);
	}

	if ((mfile != NULL) && (msg = cvs_logmsg_open(mfile)) == NULL)
		return (EX_DATAERR);

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		cvs_files = cvs_file_get(".", flags);
	} else {
		cvs_files = cvs_file_getspec(argv, argc, flags);
	}
	if (cvs_files == NULL)
		return (EX_DATAERR);

	cvs_file_examine(cvs_files, cvs_commit_prepare, &cl);

	if (msg == NULL) {
		msg = cvs_logmsg_get(CVS_FILE_NAME(cvs_files), &cl);
		if (msg == NULL)
			return (1);
	}

	root = CVS_DIR_ROOT(cvs_files);
	cvs_connect(root);
	cvs_logmsg_send(root, msg);

	cvs_file_examine(cvs_files, cvs_commit_file, &cl);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		cvs_senddir(root, cvs_files);
		if (argc > 0) {
			for (i = 0; i < argc; i++)
				cvs_sendarg(root, argv[i], 0);
		}
		cvs_sendreq(root, CVS_REQ_CI, NULL);
	}

	return (0);
}


/*
 * cvs_commit_prepare()
 *
 * Examine the file <cf> to see if it will be part of the commit, in which
 * case it gets added to the list passed as second argument.
 */

int
cvs_commit_prepare(CVSFILE *cf, void *arg)
{
	CVSFILE *copy;
	struct cvs_flist *clp = (struct cvs_flist *)arg;

	if ((cf->cf_type == DT_REG) && (cf->cf_cvstat == CVS_FST_MODIFIED)) {
		copy = cvs_file_copy(cf);
		if (copy == NULL)
			return (-1);

		TAILQ_INSERT_TAIL(clp, copy, cf_list);
	}

	return (0);
}


/*
 * cvs_commit_file()
 *
 * Commit a single file.
 */

int
cvs_commit_file(CVSFILE *cf, void *arg)
{
	char *repo, rcspath[MAXPATHLEN], fpath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;
	struct cvs_ent *entp;

	rf = NULL;
	repo = NULL;

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat != CVS_FST_UNKNOWN) {
			root = CVS_DIR_ROOT(cf);
			if ((cf->cf_parent != NULL) &&
			    (root != cf->cf_parent->cf_ddat->cd_root)) {
				cvs_connect(root);
			}

			cvs_senddir(root, cf);
		}

		return (0);
	}


	root = CVS_DIR_ROOT(cf);
	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_parent != NULL)
		repo = cf->cf_parent->cf_ddat->cd_repo;

	entp = cvs_ent_getent(fpath);
	if (entp == NULL)
		return (-1);

	if ((cf->cf_cvstat == CVS_FST_ADDED) ||
	    (cf->cf_cvstat == CVS_FST_MODIFIED)) {
		if ((root->cr_method != CVS_METHOD_LOCAL) &&
		    (cvs_sendentry(root, entp) < 0)) {
			cvs_ent_free(entp);
			return (-1);
		}

		cvs_sendreq(root, CVS_REQ_MODIFIED, CVS_FILE_NAME(cf));
		cvs_sendfile(root, fpath);
	}

	snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, fpath, RCS_FILE_EXT);

	cvs_ent_free(entp);

	return (0);
}
