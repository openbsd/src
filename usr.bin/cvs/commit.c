/*	$OpenBSD: commit.c,v 1.37 2005/05/31 08:58:47 xsa Exp $	*/
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "cvs.h"
#include "log.h"
#include "proto.h"


static int cvs_commit_init    (struct cvs_cmd *, int, char **, int *);
static int cvs_commit_prepare (CVSFILE *, void *);
static int cvs_commit_file    (CVSFILE *, void *);
static int cvs_commit_pre_exec(struct cvsroot *);

struct cvs_cmd cvs_cmd_commit = {
	CVS_OP_COMMIT, CVS_REQ_CI, "commit",
	{ "ci",  "com" },
	"Check files into the repository",
	"[-flR] [-F logfile | -m msg] [-r rev] ...",
	"F:flm:Rr:",
	NULL,
	CF_RECURSE | CF_IGNORE | CF_SORT,
	cvs_commit_init,
	cvs_commit_pre_exec,
	cvs_commit_file,
	NULL,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};

static char *mfile = NULL;
static char *rev = NULL;
static char **commit_files = NULL;
static int commit_fcount = 0;

static int
cvs_commit_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'F':
			mfile = optarg;
			break;
		case 'f':
			/* XXX half-implemented */
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'm':
			cvs_msg = strdup(optarg);
			if (cvs_msg == NULL) {
				cvs_log(LP_ERRNO, "failed to copy message");
				return (CVS_EX_USAGE);
			}
			break;
		case 'R':
			cmd->file_flags |= CF_RECURSE;
			break;
		case 'r':
			rev = optarg;
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

	commit_files = (argv + optind);
	commit_fcount = (argc - optind);

	return (0);
}

int
cvs_commit_pre_exec(struct cvsroot *root)
{
	struct cvs_flist cl;
	CVSFILE *cfp;
	CVSFILE *tmp;
	int flags = CF_RECURSE | CF_IGNORE | CF_SORT;
 
	SIMPLEQ_INIT(&cl);

	if (commit_fcount != 0) {
		tmp = cvs_file_getspec(commit_files, commit_fcount,
		    flags, cvs_commit_prepare, &cl);
	} else {
		tmp = cvs_file_get(".", flags, cvs_commit_prepare, &cl);
	}

	if (tmp == NULL)
		return (CVS_EX_DATA);

	if (SIMPLEQ_EMPTY(&cl)) {
		cvs_file_free(tmp);
		return (0);
	}

	if (cvs_msg == NULL)
		cvs_msg = cvs_logmsg_get(CVS_FILE_NAME(tmp),
		    NULL, &cl, NULL);

	cvs_file_free(tmp);

	while (!SIMPLEQ_EMPTY(&cl)) {
		cfp = SIMPLEQ_FIRST(&cl);
		SIMPLEQ_REMOVE_HEAD(&cl, cf_list);
		cvs_file_free(cfp);
	}

	if (cvs_msg == NULL)
		return (CVS_EX_DATA);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_logmsg_send(root, cvs_msg) < 0)
			return (CVS_EX_PROTO);

		if (rev != NULL) {
			if ((cvs_sendarg(root, "-r", 0) < 0) ||
			    (cvs_sendarg(root, rev, 0) < 0))
				return (CVS_EX_PROTO);
		}
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
			if (cf->cf_cvstat != CVS_FST_UNKNOWN) {
				if (cvs_senddir(root, cf) < 0)
					return (CVS_EX_PROTO);
			}
		}

		return (0);
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
		return (CVS_EX_DATA);
	}

	return (0);
}
