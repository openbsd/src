/*	$OpenBSD: commit.c,v 1.18 2005/03/30 17:43:04 joris Exp $	*/
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


static int cvs_commit_prepare(CVSFILE *, void *);
int cvs_commit_file(CVSFILE *, void *);
int cvs_commit_options(char *, int, char **, int *);
int cvs_commit_helper(void);

struct cvs_cmd_info cvs_commit = {
	cvs_commit_options,
	NULL,
	cvs_commit_file,
	NULL,
	cvs_commit_helper,
	CF_RECURSE | CF_IGNORE | CF_SORT,
	CVS_REQ_CI,
	CVS_CMD_NEEDLOG | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};

static char *mfile = NULL;

int
cvs_commit_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'F':
			mfile = optarg;
			break;
		case 'f':
			/* XXX half-implemented */
			cvs_commit.file_flags &= ~CF_RECURSE;
			break;
		case 'l':
			cvs_commit.file_flags &= ~CF_RECURSE;
			break;
		case 'm':
			cvs_msg = strdup(optarg);
			if (cvs_msg == NULL) {
				cvs_log(LP_ERRNO, "failed to copy message");
				return (EX_DATAERR);
			}
			break;
		case 'R':
			cvs_commit.file_flags |= CF_RECURSE;
			break;
		default:
			return (EX_USAGE);
		}
	}

	if ((cvs_msg != NULL) && (mfile != NULL)) {
		cvs_log(LP_ERR, "the -F and -m flags are mutually exclusive");
		return (EX_USAGE);
	}

	if ((mfile != NULL) && (cvs_msg = cvs_logmsg_open(mfile)) == NULL)
		return (EX_DATAERR);

	*arg = optind;
	return (0);
}

int
cvs_commit_helper(void)
{
	struct cvs_flist cl;
	CVSFILE *cfp;

	TAILQ_INIT(&cl);
	cvs_file_examine(cvs_files, cvs_commit_prepare, &cl);
	if (TAILQ_EMPTY(&cl))
		return (0);

	if (cvs_msg == NULL)
		cvs_msg = cvs_logmsg_get(CVS_FILE_NAME(cvs_files),
		    NULL, &cl, NULL);

	while (!TAILQ_EMPTY(&cl)) {
		cfp = TAILQ_FIRST(&cl);
		TAILQ_REMOVE(&cl, cfp, cf_list);
		cvs_file_free(cfp);
	}

	if (cvs_msg == NULL)
		return (1);

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
	int ret;
	char *repo, rcspath[MAXPATHLEN], fpath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;
	struct cvs_ent *entp;

	ret = 0;
	rf = NULL;
	repo = NULL;
	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cf->cf_cvstat != CVS_FST_UNKNOWN)
				ret = cvs_senddir(root, cf);
		}

		return (ret);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_parent != NULL)
		repo = cf->cf_parent->cf_ddat->cd_repo;

	entp = cvs_ent_getent(fpath);
	if (entp == NULL)
		return (-1);

	if ((cf->cf_cvstat == CVS_FST_ADDED) ||
	    (cf->cf_cvstat == CVS_FST_MODIFIED)) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cvs_sendentry(root, entp) < 0) {
				cvs_ent_free(entp);
				return (-1);
			}

			if (cvs_sendreq(root, CVS_REQ_MODIFIED,
			    CVS_FILE_NAME(cf)) < 0) {
				cvs_ent_free(entp);
				return (-1);
			}

			if (cvs_sendfile(root, fpath) < 0) {
				cvs_ent_free(entp);
				return (-1);
			}
		}
	}

	snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, fpath, RCS_FILE_EXT);

	cvs_ent_free(entp);

	return (0);
}
