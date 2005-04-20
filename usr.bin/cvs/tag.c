/*	$OpenBSD: tag.c,v 1.15 2005/04/20 23:11:30 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2004 Joris Vink <joris@openbsd.org>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


static int cvs_tag_local     (CVSFILE *, void *);
static int cvs_tag_remote    (CVSFILE *, void *);
static int cvs_tag_options   (char *, int, char **, int *);
static int cvs_tag_sendflags (struct cvsroot *);

static char *tag_name = NULL;
static char *tag_date = NULL;
static char *tag_oldname = NULL;
static int tag_branch = 0;
static int tag_delete = 0;
static int tag_forcehead = 0;

struct cvs_cmd_info cvs_tag = {
	cvs_tag_options,
	cvs_tag_sendflags,
	cvs_tag_remote,	
	NULL, NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	CVS_REQ_TAG,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};

static int
cvs_tag_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	tag_date = tag_oldname = NULL;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'b':
			tag_branch = 1;
			break;
		case 'd':
			tag_delete = 1;
			break;
		case 'f':
			tag_forcehead = 1;
			break;
		case 'D':
			tag_date = optarg;
			break;
		case 'l':
			cvs_tag.file_flags &= ~CF_RECURSE;
			break;
		case 'r':
			tag_oldname = optarg;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		return (CVS_EX_USAGE);
	} else {
		tag_name = argv[0];
		argc--;
		argv++;
		*arg += 1;
	}

	if (!rcs_sym_check(tag_name)) {
		cvs_log(LP_ABORT,
		    "tag `%s' must not contain the characters `%s'",
		    tag_name, RCS_SYM_INVALCHAR);
		return (CVS_EX_BADTAG);
	}

	if (tag_branch && tag_delete) {
		cvs_log(LP_WARN, "ignoring -b with -d options");
		tag_branch = 0;
	}

	if (tag_delete && tag_oldname)
		tag_oldname = NULL;

	if (tag_delete && tag_date)
		tag_date = NULL;

	if (tag_oldname != NULL && tag_date != NULL) {
		cvs_log(LP_ERROR, "-r and -D options are mutually exclusive");
		return (CVS_EX_USAGE);
	}

	return (0);
}

static int
cvs_tag_sendflags(struct cvsroot *root)
{
	if (tag_branch && (cvs_sendarg(root, "-b", 0) < 0))
		return (CVS_EX_PROTO);

	if (tag_delete && (cvs_sendarg(root, "-d", 0) < 0))
		return (CVS_EX_PROTO);

	if (tag_oldname) {
		if ((cvs_sendarg(root, "-r", 0) < 0) ||
		    (cvs_sendarg(root, tag_oldname, 0) < 0))
			return (CVS_EX_PROTO);
	}

	if (tag_date) {
		if ((cvs_sendarg(root, "-D", 0) < 0) ||
		    (cvs_sendarg(root, tag_date, 0) < 0))
			return (CVS_EX_PROTO);
	}

	if (cvs_sendarg(root, tag_name, 0) < 0)
		return (CVS_EX_PROTO);

	return (0);
}


/*
 * cvs_tag_remote()
 *
 * Get the status of a single file.
 */
static int
cvs_tag_remote(CVSFILE *cfp, void *arg)
{
	int ret;
	char fpath[MAXPATHLEN];
	struct cvsroot *root;

	ret = 0;
	root = CVS_DIR_ROOT(cfp);

	if (cfp->cf_type == DT_DIR) {
		ret = cvs_senddir(root, cfp);
		return (ret);
	}

	if (cvs_sendentry(root, cfp) < 0) {
		return (CVS_EX_PROTO);
	}

	cvs_file_getpath(cfp, fpath, sizeof(fpath));

	switch (cfp->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cfp->cf_name);
		break;
	case CVS_FST_UPTODATE:
		ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, cfp->cf_name);
		break;
	case CVS_FST_MODIFIED:
		ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED, cfp->cf_name); 
	default:
		break;
	}

	return (ret);
}


static int
cvs_tag_local(CVSFILE *cf, void *arg)
{
	int len;
	char *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	struct cvsroot *root;
	RCSFILE *rf;
	RCSNUM *tag_rev;

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		cvs_log(LP_WARN, "I know nothing about %s", fpath);
		return (0);
	}

	repo = CVS_DIR_REPO(cf);
	root = CVS_DIR_ROOT(cf);

	len = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, cf->cf_name, RCS_FILE_EXT);
	if (len == -1 || len >= (int)sizeof(rcspath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", rcspath);
		return (-1);
	}

	rf = rcs_open(rcspath, RCS_READ|RCS_WRITE);
	if (rf == NULL) {
		cvs_log(LP_ERR, "failed to open %s: %s", rcspath,
		    rcs_errstr(rcs_errno));
		return (-1);
	}

	if (rcs_sym_add(rf, tag_name, tag_rev) < 0) {
		cvs_log(LP_ERR, "failed to tag %s: %s", rcspath,
		    rcs_errstr(rcs_errno));
	}

	rcs_close(rf);
	return (0);
}
