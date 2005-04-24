/*	$OpenBSD: commit.c,v 1.31 2005/04/24 02:06:27 joris Exp $	*/
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

#include "cvs.h"
#include "log.h"
#include "buf.h"
#include "proto.h"


int cvs_commit_prepare(CVSFILE *, void *);
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
	CVS_CMD_ALLOWSPEC | CVS_CMD_NEEDLOG | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
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
				return (CVS_EX_USAGE);
			}
			break;
		case 'R':
			cvs_commit.file_flags |= CF_RECURSE;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	if ((cvs_msg != NULL) && (mfile != NULL)) {
		cvs_log(LP_ERR, "the -F and -m flags are mutually exclusive");
		return (CVS_EX_USAGE);
	}

	if ((mfile != NULL) && (cvs_msg = cvs_logmsg_open(mfile)) == NULL)
		return (CVS_EX_DATA);

	*arg = optind;
	return (0);
}

int
cvs_commit_helper(void)
{
	struct cvs_flist cl;
	CVSFILE *cfp;

	SIMPLEQ_INIT(&cl);
	cvs_file_examine(cvs_files, cvs_commit_prepare, &cl);
	if (SIMPLEQ_EMPTY(&cl))
		return (0);

	if (cvs_msg == NULL)
		cvs_msg = cvs_logmsg_get(CVS_FILE_NAME(cvs_files),
		    NULL, &cl, NULL);

	while (!SIMPLEQ_EMPTY(&cl)) {
		cfp = SIMPLEQ_FIRST(&cl);
		SIMPLEQ_REMOVE_HEAD(&cl, cf_list);
		cvs_file_free(cfp);
	}

	if (cvs_msg == NULL)
		return (CVS_EX_DATA);

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

	if ((cf->cf_type == DT_REG) && ((cf->cf_cvstat == CVS_FST_MODIFIED) ||
	    (cf->cf_cvstat == CVS_FST_ADDED) ||
	    (cf->cf_cvstat == CVS_FST_REMOVED))) {
		copy = cvs_file_copy(cf);
		if (copy == NULL)
			return (CVS_EX_DATA);

		SIMPLEQ_INSERT_TAIL(clp, copy, cf_list);
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
	int ret, l;
	char *repo, rcspath[MAXPATHLEN], fpath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;

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
		repo = cf->cf_parent->cf_repo;

	if ((cf->cf_cvstat == CVS_FST_ADDED) ||
	    (cf->cf_cvstat == CVS_FST_MODIFIED) ||
	    (cf->cf_cvstat == CVS_FST_REMOVED)) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cvs_sendentry(root, cf) < 0) {
				return (CVS_EX_PROTO);
			}

			/* if it's removed, don't bother sending a
			 * Modified request together with the file its
			 * contents.
			 */
			if (cf->cf_cvstat == CVS_FST_REMOVED)
				return (0);

			if (cvs_sendreq(root, CVS_REQ_MODIFIED,
			    CVS_FILE_NAME(cf)) < 0) {
				return (CVS_EX_PROTO);
			}

			if (cvs_sendfile(root, fpath) < 0) {
				return (CVS_EX_PROTO);
			}
		}
	}

	l = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, fpath, RCS_FILE_EXT);
	if (l == -1 || l >= (int)sizeof(rcspath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", rcspath);
		return (-1);
	}

	return (0);
}
