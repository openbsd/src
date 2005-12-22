/*	$OpenBSD: commit.c,v 1.49 2005/12/22 14:59:54 xsa Exp $	*/
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


static int	cvs_commit_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_commit_prepare(CVSFILE *, void *);
static int	cvs_commit_remote(CVSFILE *, void *);
static int	cvs_commit_local(CVSFILE *, void *);
static int	cvs_commit_pre_exec(struct cvsroot *);

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
	cvs_commit_remote,
	cvs_commit_local,
	NULL,
	NULL,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};

static char *mfile = NULL;
static char *rev = NULL;
static char **commit_files = NULL;
static int commit_fcount = 0;
static int wantedstatus = 0;

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
			cvs_msg = xstrdup(optarg);
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

	if (mfile != NULL)
		cvs_msg = cvs_logmsg_open(mfile);

	*arg = optind;

	commit_files = (argv + optind);
	commit_fcount = (argc - optind);

	return (0);
}

int
cvs_commit_pre_exec(struct cvsroot *root)
{
	CVSFILE *cfp;
	CVSFILE *tmp;
	int ret, i, flags = CF_RECURSE | CF_IGNORE | CF_SORT;
	struct cvs_flist added, modified, removed, *cl[3];
	int stattype[] = { CVS_FST_ADDED, CVS_FST_MODIFIED, CVS_FST_REMOVED };

	SIMPLEQ_INIT(&added);
	SIMPLEQ_INIT(&modified);
	SIMPLEQ_INIT(&removed);

	cl[0] = &added;
	cl[1] = &modified;
	cl[2] = &removed;

	if ((tmp = cvs_file_loadinfo(".", CF_NOFILES, NULL, NULL, 1)) == NULL)
		return (CVS_EX_DATA);

	/*
	 * Obtain the file lists for the logmessage.
	 */
	for (i = 0; i < 3; i++) {
		wantedstatus = stattype[i];
		if (commit_fcount != 0) {
			ret = cvs_file_getspec(commit_files, commit_fcount,
			    flags, cvs_commit_prepare, cl[i], NULL);
		} else {
			ret = cvs_file_get(".", flags, cvs_commit_prepare,
			    cl[i], NULL);
		}

		if (ret != CVS_EX_OK) {
			cvs_file_free(tmp);
			return (CVS_EX_DATA);
		}
	}

	/*
	 * If we didn't catch any file, don't call the editor.
	 */
	if (SIMPLEQ_EMPTY(&added) && SIMPLEQ_EMPTY(&modified) &&
	    SIMPLEQ_EMPTY(&removed)) {
		cvs_file_free(tmp);
		return (0);
	}

	/*
	 * Fetch the log message for real, with all the files.
	 */
	if (cvs_msg == NULL)
		cvs_msg = cvs_logmsg_get(tmp->cf_name, &added, &modified,
		    &removed);

	cvs_file_free(tmp);

	/* free the file lists */
	for (i = 0; i < 3; i++) {
		while (!SIMPLEQ_EMPTY(cl[i])) {
			cfp = SIMPLEQ_FIRST(cl[i]);
			SIMPLEQ_REMOVE_HEAD(cl[i], cf_list);
			cvs_file_free(cfp);
		}
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

	if ((cf->cf_type == DT_REG) && (cf->cf_cvstat == wantedstatus)) {
		copy = cvs_file_copy(cf);
		if (copy == NULL)
			return (CVS_EX_DATA);

		SIMPLEQ_INSERT_TAIL(clp, copy, cf_list);
	}

	return (0);
}


/*
 * cvs_commit_remote()
 *
 * Commit a single file.
 */
int
cvs_commit_remote(CVSFILE *cf, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	repo = NULL;
	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat != CVS_FST_UNKNOWN) {
			if (cvs_senddir(root, cf) < 0)
				return (CVS_EX_PROTO);
		}
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_parent != NULL)
		repo = cf->cf_parent->cf_repo;

	if ((cf->cf_cvstat == CVS_FST_ADDED) ||
	    (cf->cf_cvstat == CVS_FST_MODIFIED) ||
	    (cf->cf_cvstat == CVS_FST_REMOVED)) {
		if (cvs_sendentry(root, cf) < 0) {
			return (CVS_EX_PROTO);
		}

		/* if it's removed, don't bother sending a
		 * Modified request together with the file its
		 * contents.
		 */
		if (cf->cf_cvstat == CVS_FST_REMOVED)
			return (0);

		if (cvs_sendreq(root, CVS_REQ_MODIFIED, cf->cf_name) < 0)
			return (CVS_EX_PROTO);

		if (cvs_sendfile(root, fpath) < 0) {
			return (CVS_EX_PROTO);
		}
	}

	return (0);
}

static int
cvs_commit_local(CVSFILE *cf, void *arg)
{
	char fpath[MAXPATHLEN], rcspath[MAXPATHLEN];

	if (cf->cf_type == DT_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Examining %s", cf->cf_name);
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));
	cvs_rcs_getpath(cf, rcspath, sizeof(rcspath));

	return (0);
}
