/*	$OpenBSD: tag.c,v 1.35 2005/12/30 02:03:28 joris Exp $	*/
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


static int	cvs_tag_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_tag_local(CVSFILE *, void *);
static int	cvs_tag_remote(CVSFILE *, void *);
static int	cvs_tag_pre_exec(struct cvsroot *);

static char *tag_name = NULL;
static char *tag_date = NULL;
static char *tag_oldname = NULL;
static int tag_branch = 0;
static int tag_delete = 0;
static int tag_forcehead = 0;
static int tag_forcemove = 0;

struct cvs_cmd cvs_cmd_tag = {
	CVS_OP_TAG, CVS_REQ_TAG, "tag",
	{ "ta", "freeze" },
	"Add a symbolic tag to checked out version of files",
	"[-bcdFflR] [-D date | -r rev] tagname ...",
	"bcD:dFflRr:",
	NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	cvs_tag_init,
	cvs_tag_pre_exec,
	cvs_tag_remote,
	cvs_tag_local,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};


struct cvs_cmd cvs_cmd_rtag = {
	CVS_OP_RTAG, CVS_REQ_TAG, "rtag",
	{ "rt", "rfreeze" },
	"Add a symbolic tag to a module",
	"[-abdFflnR] [-D date | -r rev] symbolic_tag modules ...",
	"abD:fFflnRr:",
	NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	cvs_tag_init,
	cvs_tag_pre_exec,
	cvs_tag_remote,
	cvs_tag_local,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};

static int
cvs_tag_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	tag_date = tag_oldname = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'b':
			tag_branch = 1;
			break;
		case 'd':
			tag_delete = 1;
			break;
		case 'F':
			tag_forcemove = 1;
			break;
		case 'f':
			tag_forcehead = 1;
			break;
		case 'D':
			tag_date = optarg;
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'R':
			cmd->file_flags |= CF_RECURSE;
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

	if ((tag_branch == 1) && (tag_delete == 1)) {
		cvs_log(LP_WARN, "ignoring -b with -d options");
		tag_branch = 0;
	}

	if ((tag_delete == 1) && (tag_oldname != NULL))
		tag_oldname = NULL;

	if ((tag_delete == 1) && (tag_date != NULL))
		tag_date = NULL;

	if ((tag_oldname != NULL) && (tag_date != NULL)) {
		cvs_log(LP_ERR, "the -D and -r options are mutually exclusive");
		return (CVS_EX_USAGE);
	}

	return (0);
}

static int
cvs_tag_pre_exec(struct cvsroot *root)
{
	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (tag_branch == 1)
			cvs_sendarg(root, "-b", 0);

		if (tag_delete == 1)
			cvs_sendarg(root, "-d", 0);

		if (tag_forcemove == 1)
			cvs_sendarg(root, "-F", 0);

		if (tag_forcehead == 1)
			cvs_sendarg(root, "-f", 0);

		if (tag_oldname != NULL) {
			cvs_sendarg(root, "-r", 0);
			cvs_sendarg(root, tag_oldname, 0);
		}

		if (tag_date != NULL) {
			cvs_sendarg(root, "-D", 0);
			cvs_sendarg(root, tag_date, 0);
		}

		cvs_sendarg(root, tag_name, 0);
	}

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
	char fpath[MAXPATHLEN];
	struct cvsroot *root;

	root = CVS_DIR_ROOT(cfp);

	if (cfp->cf_type == DT_DIR) {
		cvs_senddir(root, cfp);
		return (0);
	}

	cvs_sendentry(root, cfp);
	cvs_file_getpath(cfp, fpath, sizeof(fpath));

	switch (cfp->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cfp->cf_name);
		break;
	case CVS_FST_UPTODATE:
		cvs_sendreq(root, CVS_REQ_UNCHANGED, cfp->cf_name);
		break;
	case CVS_FST_MODIFIED:
		cvs_sendreq(root, CVS_REQ_ISMODIFIED, cfp->cf_name);
	default:
		break;
	}

	return (0);
}


static int
cvs_tag_local(CVSFILE *cf, void *arg)
{
	char fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	RCSNUM *tag_rev;

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_type == DT_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "%s %s",
			    tag_delete ? "Untagging" : "Tagging", fpath);
		return (CVS_EX_OK);
	}

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		if (verbosity > 1)
			cvs_log(LP_WARN, "nothing known about %s", fpath);
		return (0);
	} else if (cf->cf_cvstat == CVS_FST_ADDED) {
		if (verbosity > 1)
			cvs_log(LP_WARN,
			    "couldn't tag added but un-commited file `%s'",
			    fpath);
		return (0);
	} else if (cf->cf_cvstat == CVS_FST_REMOVED) {
		if (verbosity > 1)
			cvs_log(LP_WARN,
			    "skipping removed but un-commited file `%s'",
			    fpath);
		return (0);
	}

	tag_rev = cf->cf_lrev;

	cvs_rcs_getpath(cf, rcspath, sizeof(rcspath));

	rf = rcs_open(rcspath, RCS_READ|RCS_WRITE);
	if (rf == NULL) {
		cvs_log(LP_ERR, "failed to open %s: %s", rcspath,
		    rcs_errstr(rcs_errno));
		return (CVS_EX_DATA);
	}

	if (tag_delete == 1) {
		/* XXX */
		if (verbosity > 0)
			cvs_printf("D %s\n", fpath);

		return (0);
	}

	if (cvs_noexec == 0) {
		if (rcs_sym_add(rf, tag_name, tag_rev) < 0) {
			cvs_log(LP_ERR, "failed to tag %s: %s", rcspath,
			    rcs_errstr(rcs_errno));
		}
	}

	if (verbosity > 0)
		cvs_printf("T %s\n", fpath);

	rcs_close(rf);
	return (0);
}
