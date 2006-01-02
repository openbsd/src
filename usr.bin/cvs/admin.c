/*	$OpenBSD: admin.c,v 1.26 2006/01/02 08:11:56 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "proto.h"


#define LOCK_SET	0x01
#define LOCK_REMOVE	0x02

#define FLAG_BRANCH		0x01
#define FLAG_DELUSER		0x02
#define FLAG_INTERACTIVE	0x04
#define FLAG_QUIET		0x08

static int	cvs_admin_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_admin_pre_exec(struct cvsroot *);
static int	cvs_admin_remote(CVSFILE *, void *);
static int	cvs_admin_local(CVSFILE *, void *);

struct cvs_cmd cvs_cmd_admin = {
	CVS_OP_ADMIN, CVS_REQ_ADMIN, "admin",
	{ "adm", "rcs" },
	"Administrative front-end for RCS",
	"",
	"a:A:b::c:e::Ik:l::Lm:n:N:o:qs:t:u::U",
	NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	cvs_admin_init,
	cvs_admin_pre_exec,
	cvs_admin_remote,
	cvs_admin_local,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};

static char *q, *Ntag, *ntag, *comment, *replace_msg;
static char *alist, *subst, *lockrev_arg, *unlockrev_arg;
static char *state, *userfile, *branch_arg, *elist, *range;
static int runflags, kflag, lockrev, lkmode;

/* flag as invalid */
static int kflag = RCS_KWEXP_ERR;
static int lkmode = RCS_LOCK_INVAL;

static int
cvs_admin_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;
	RCSNUM *rcs;

	runflags = lockrev = 0;
	Ntag = ntag = comment = replace_msg = NULL;
	state = alist = subst = elist = lockrev_arg = NULL;
	range = userfile = branch_arg = unlockrev_arg = NULL;

	/* option-o-rama ! */
	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			alist = optarg;
			break;
		case 'A':
			userfile = optarg;
			break;
		case 'b':
			runflags |= FLAG_BRANCH;
			if (optarg)
				branch_arg = optarg;
			break;
		case 'c':
			comment = optarg;
			break;
		case 'e':
			runflags |= FLAG_DELUSER;
			if (optarg)
				elist = optarg;
			break;
		case 'I':
			runflags |= FLAG_INTERACTIVE;
			break;
		case 'k':
			subst = optarg;
			kflag = rcs_kflag_get(subst);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				rcs_kflag_usage();
				return (CVS_EX_USAGE);
			}
			break;
		case 'l':
			lockrev |= LOCK_SET;
			if (optarg)
				lockrev_arg = optarg;
			break;
		case 'L':
			lkmode = RCS_LOCK_STRICT;
			break;
		case 'm':
			replace_msg = optarg;
			break;
		case 'n':
			ntag = optarg;
			break;
		case 'N':
			Ntag = optarg;
			break;
		case 'o':
			range = optarg;
			break;
		case 'q':
			runflags |= FLAG_QUIET;
			break;
		case 's':
			state = optarg;
			break;
		case 't':
			break;
		case 'u':
			lockrev |= LOCK_REMOVE;
			if (optarg)
				unlockrev_arg = optarg;
			break;
		case 'U':
			if (lkmode != RCS_LOCK_INVAL) {
				cvs_log(LP_ERR, "-L and -U are incompatible");
				return (CVS_EX_USAGE);
			}
			lkmode = RCS_LOCK_LOOSE;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (lockrev_arg != NULL) {
		if ((rcs = rcsnum_parse(lockrev_arg)) == NULL) {
			cvs_log(LP_ERR, "%s is not a numeric branch",
			    lockrev_arg);
			return (CVS_EX_USAGE);
		}
		rcsnum_free(rcs);
	}

	if (unlockrev_arg != NULL) {
		if ((rcs = rcsnum_parse(unlockrev_arg)) == NULL) {
			cvs_log(LP_ERR, "%s is not a numeric branch",
			    unlockrev_arg);
			return (CVS_EX_USAGE);
		}
		rcsnum_free(rcs);
	}

	if (replace_msg != NULL) {
		if ((q = strchr(replace_msg, ':')) == NULL) {
			cvs_log(LP_ERR, "invalid option for -m");
			return (CVS_EX_USAGE);
		}
		*q = '\0';
		if ((rcs = rcsnum_parse(replace_msg)) == NULL) {
			cvs_log(LP_ERR, "%s is not a numeric revision",
			    replace_msg);
			return (CVS_EX_USAGE);
		}
		rcsnum_free(rcs);
		*q = ':';
	}

	*arg = optind;
	return (0);
}

static int
cvs_admin_pre_exec(struct cvsroot *root)
{
	if (root->cr_method == CVS_METHOD_LOCAL)
		return (0);

	if (alist != NULL) {
		cvs_sendarg(root, "-a", 0);
		cvs_sendarg(root, alist, 0);
	}

	if (userfile != NULL) {
		cvs_sendarg(root, "-A", 0);
		cvs_sendarg(root, userfile, 0);
	}

	if (runflags & FLAG_BRANCH) {
		cvs_sendarg(root, "-b", 0);
		if (branch_arg != NULL)
			cvs_sendarg(root, branch_arg, 0);
	}

	if (comment != NULL) {
		cvs_sendarg(root, "-c", 0);
		cvs_sendarg(root, comment, 0);
	}

	if (runflags & FLAG_DELUSER)  {
		cvs_sendarg(root, "-e", 0);
		if (elist != NULL)
			cvs_sendarg(root, elist, 0);
	}

	if (runflags & FLAG_INTERACTIVE)
		cvs_sendarg(root, "-I", 0);

	if (subst != NULL) {
		cvs_sendarg(root, "-k", 0);
		cvs_sendarg(root, subst, 0);
	}

	if (lockrev & LOCK_SET) {
		cvs_sendarg(root, "-l", 0);
		if (lockrev_arg != NULL)
			cvs_sendarg(root, lockrev_arg, 0);
	}

	if (lkmode == RCS_LOCK_STRICT)
		cvs_sendarg(root, "-L", 0);
	else if (lkmode == RCS_LOCK_LOOSE)
		cvs_sendarg(root, "-U", 0);

	if (replace_msg != NULL) {
		cvs_sendarg(root, "-m", 0);
		cvs_sendarg(root, replace_msg, 0);
	}

	if (ntag != NULL) {
		cvs_sendarg(root, "-n", 0);
		cvs_sendarg(root, ntag, 0);
	}

	if (Ntag != NULL) {
		cvs_sendarg(root, "-N", 0);
		cvs_sendarg(root, Ntag, 0);
	}

	if (range != NULL) {
		cvs_sendarg(root, "-o", 0);
		cvs_sendarg(root, range, 0);
	}

	if (state != NULL) {
		cvs_sendarg(root, "-s", 0);
		cvs_sendarg(root, state, 0);
	}

	if (lockrev & LOCK_REMOVE) {
		cvs_sendarg(root, "-u", 0);
		if (unlockrev_arg != NULL)
			cvs_sendarg(root, unlockrev_arg, 0);
	}

	return (0);
}

/*
 * cvs_admin_remote()
 *
 * Perform admin commands on each file.
 */
static int
cvs_admin_remote(CVSFILE *cf, void *arg)
{
	char *repo, fpath[MAXPATHLEN];
	struct cvsroot *root;

	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		else
			cvs_senddir(root, cf);
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));
	cvs_sendentry(root, cf);

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_MODIFIED:
		cvs_sendreq(root, CVS_REQ_MODIFIED, cf->cf_name);
		cvs_sendfile(root, fpath);
	default:
		break;
	}

	return (0);
}

/*
 * cvs_admin_local()
 *
 * Perform administrative operations on a local RCS file.
 */
static int
cvs_admin_local(CVSFILE *cf, void *arg)
{
	int ret;
	char fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;

	if (cf->cf_type == DT_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Administrating %s", cf->cf_name);
		return (0);
	}

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		if (verbosity > 1)
			cvs_log(LP_WARN, "nothing known about %s", cf->cf_name);
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	cvs_rcs_getpath(cf, rcspath, sizeof(rcspath));

	rf = rcs_open(rcspath, RCS_RDWR);
	if (rf == NULL)
		return (CVS_EX_DATA);

	if (!RCS_KWEXP_INVAL(kflag))
		ret = rcs_kwexp_set(rf, kflag);
	if (lkmode != RCS_LOCK_INVAL)
		ret = rcs_lock_setmode(rf, lkmode);

	rcs_close(rf);

	return (0);
}
