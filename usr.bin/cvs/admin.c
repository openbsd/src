/*	$OpenBSD: admin.c,v 1.3 2005/03/07 19:41:07 joris Exp $	*/
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


int cvs_admin_file (CVSFILE *, void *);

#define LOCK_SET	0x01
#define LOCK_REMOVE	0x02

#define FLAG_BRANCH		0x01
#define FLAG_DELUSER		0x02
#define FLAG_INTERACTIVE	0x04
#define FLAG_QUIET		0x08

/*
 * cvs_admin()
 *
 * Handler for the `cvs admin' command.
 * Returns 0 on success, or one of the known exit codes on error.
 */
int
cvs_admin(int argc, char **argv)
{
	int i, ch, flags;
	int runflags, kflag, lockrev, strictlock;
	char *q;
	char *comment, *replace_msg;
	char *alist, *subst, *lockrev_arg, *unlockrev_arg;
	char *userfile, *branch_arg, *elist;
	struct cvsroot *root;
	RCSNUM *rcs;

	runflags = strictlock = lockrev = 0;
	comment = replace_msg = NULL;
	alist = subst = elist = lockrev_arg = NULL;
	userfile = branch_arg = unlockrev_arg = NULL;
	flags = CF_SORT|CF_IGNORE|CF_RECURSE;

	/* option-o-rama ! */
	while ((ch = getopt(argc, argv, "a:A:b::c:e::Ik:l::Lm:n:N:o:qs:t:u::U"))
	    != -1) {
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
				return (EX_USAGE);
			}
			break;
		case 'l':
			lockrev |= LOCK_SET;
			if (optarg)
				lockrev_arg = optarg;
			break;
		case 'L':
			strictlock |= LOCK_SET;
			break;
		case 'm':
			replace_msg = optarg;
			break;
		case 'n':
			break;
		case 'N':
			break;
		case 'o':
			break;
		case 'q':
			runflags |= FLAG_QUIET;
			break;
		case 's':
			break;
		case 't':
			break;
		case 'u':
			lockrev |= LOCK_REMOVE;
			if (optarg)
				unlockrev_arg = optarg;
			break;
		case 'U':
			strictlock |= LOCK_REMOVE;
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	/* do some sanity checking on the arguments */
	if ((strictlock & LOCK_SET) && (strictlock & LOCK_REMOVE)) {
		cvs_log(LP_ERR, "-L and -U are incompatible");
		return (EX_PROTOCOL);
	}

	if (lockrev_arg != NULL) {
		if ((rcs = rcsnum_parse(lockrev_arg)) == NULL) {
			cvs_log(LP_ERR, "%s is not a numeric branch",
			    lockrev_arg);
			return (EX_USAGE);
		}
		rcsnum_free(rcs);
	}

	if (unlockrev_arg != NULL) {
		if ((rcs = rcsnum_parse(unlockrev_arg)) == NULL) {
			cvs_log(LP_ERR, "%s is not a numeric branch",
			    unlockrev_arg);
			return (EX_PROTOCOL);
		}
		rcsnum_free(rcs);
	}

	if (replace_msg != NULL) {
		if ((q = strchr(replace_msg, ':')) == NULL) {
			cvs_log(LP_ERR, "invalid option for -m");
			return (EX_USAGE);
		}
		*q = '\0';
		if ((rcs = rcsnum_parse(replace_msg)) == NULL) {
			cvs_log(LP_ERR, "%s is not a numeric revision",
			    replace_msg);
			return (EX_PROTOCOL);
		}
		rcsnum_free(rcs);
		*q = ':';
	}

	if (argc == 0) {
		cvs_files = cvs_file_get(".", flags);
	} else {
		cvs_files = cvs_file_getspec(argv, argc, 0);
	}
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

		if ((alist != NULL) && ((cvs_sendarg(root, "-a", 0) < 0) || 
		    (cvs_sendarg(root, alist, 0) < 0)))
			return (EX_PROTOCOL);

		if ((userfile != NULL) && ((cvs_sendarg(root, "-A", 0) < 0) ||
		    (cvs_sendarg(root, userfile, 0) < 0)))
			return (EX_PROTOCOL);

		if (runflags & FLAG_BRANCH) {
			if (cvs_sendarg(root, "-b", 0) < 0)
				return (EX_PROTOCOL);
			if ((branch_arg != NULL) &&
			    (cvs_sendarg(root, branch_arg, 0) < 0))
				return (EX_PROTOCOL);
		}

		if ((comment != NULL) && (cvs_sendarg(root, comment, 0) < 0))
			return (EX_PROTOCOL);

		if (runflags & FLAG_DELUSER)  {
			if (cvs_sendarg(root, "-e", 0) < 0)
				return (EX_PROTOCOL);
			if ((elist != NULL) &&
			    (cvs_sendarg(root, elist, 0) < 0))
				return (EX_PROTOCOL);
		}

		if (runflags & FLAG_INTERACTIVE) {
			if (cvs_sendarg(root, "-I", 0) < 0)
				return (EX_PROTOCOL);
		}

		if ((subst != NULL) && ((cvs_sendarg(root, "-k", 0) < 0) ||
		    (cvs_sendarg(root, subst, 0) < 0)))
			return (EX_PROTOCOL);

		if (lockrev & LOCK_SET) {
			if (cvs_sendarg(root, "-l", 0) < 0)
				return (EX_PROTOCOL);
			if ((lockrev_arg != NULL) &&
			    (cvs_sendarg(root, lockrev_arg, 0) < 0))
				return (0);
		}

		if ((strictlock & LOCK_SET) &&
		    (cvs_sendarg(root, "-L", 0) < 0))
			return (EX_PROTOCOL);

		if ((replace_msg != NULL) && ((cvs_sendarg(root, "-m", 0) < 0)
		    || (cvs_sendarg(root, replace_msg, 0) < 0)))
			return (EX_PROTOCOL);

		if (lockrev & LOCK_REMOVE) {
			if (cvs_sendarg(root, "-u", 0) < 0)
				return (EX_PROTOCOL);
			if ((unlockrev_arg != NULL) &&
			    (cvs_sendarg(root, unlockrev_arg, 0) < 0))
				return (EX_PROTOCOL);
		}

		if ((strictlock & LOCK_REMOVE) &&
		    (cvs_sendarg(root, "-U", 0) < 0))
			return (EX_PROTOCOL);
	}

	cvs_file_examine(cvs_files, cvs_admin_file, NULL);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_senddir(root, cvs_files) < 0)
			return (EX_PROTOCOL);
		for (i = 0; i < argc; i++)
			if (cvs_sendarg(root, argv[i], 0) < 0)
				return (EX_PROTOCOL);
		if (cvs_sendreq(root, CVS_REQ_ADMIN, NULL) < 0)
			return (EX_PROTOCOL);
	}

	return (0);
}


/*
 * cvs_admin_file()
 *
 * Perform admin commands on each file.
 */
int
cvs_admin_file(CVSFILE *cfp, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvs_ent *entp;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cfp);
	repo = CVS_DIR_REPO(cfp);

	if (cfp->cf_type == DT_DIR) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cfp->cf_cvstat == CVS_FST_UNKNOWN)
				ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
				    CVS_FILE_NAME(cfp));
			else
				ret = cvs_senddir(root, cfp);
		}

		return (ret);
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
			ret = cvs_sendreq(root, CVS_REQ_MODIFIED,
			    CVS_FILE_NAME(cfp));
			if (ret == 0)
				ret = cvs_sendfile(root, fpath);
		default:
			break;
		}
	} else {
		if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
			cvs_log(LP_WARN, "I know nothing about %s", fpath);
			return (0);
		}

		snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
		    root->cr_dir, repo, CVS_FILE_NAME(cfp), RCS_FILE_EXT);

		rf = rcs_open(rcspath, RCS_READ);
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
