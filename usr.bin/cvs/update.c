/*	$OpenBSD: update.c,v 1.54 2006/01/27 15:26:38 xsa Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "proto.h"
#include "diff.h"

static int	cvs_update_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_update_pre_exec(struct cvsroot *);
static int	cvs_update_remote(CVSFILE *, void *);
static int	cvs_update_local(CVSFILE *, void *);

struct cvs_cmd cvs_cmd_update = {
	CVS_OP_UPDATE, CVS_REQ_UPDATE, "update",
	{ "up", "upd" },
	"Bring work tree in sync with repository",
	"[-ACdflPpR] [-D date | -r rev] [-I ign] [-j rev] [-k mode] "
	"[-t id] ...",
	"ACD:dfI:j:k:lPpQqRr:t:",
	NULL,
	CF_SORT | CF_RECURSE | CF_IGNORE | CF_NOSYMS,
	cvs_update_init,
	cvs_update_pre_exec,
	cvs_update_remote,
	cvs_update_local,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2 | CVS_CMD_SENDDIR
};

static char *date, *rev, *koptstr;
static int dflag, Aflag;
static int kflag = RCS_KWEXP_DEFAULT;

static int
cvs_update_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	dflag = Aflag = 0;
	date = NULL;
	rev = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'C':
		case 'D':
			date = optarg;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			break;
		case 'I':
			break;
		case 'j':
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				rcs_kflag_usage();
				return (CVS_EX_USAGE);
			}
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'P':
			cmd->cmd_flags |= CVS_CMD_PRUNEDIRS;
			break;
		case 'p':
			cvs_noexec = 1;	/* no locks will be created */
			break;
		case 'Q':
		case 'q':
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

	*arg = optind;
	return (0);
}

static int
cvs_update_pre_exec(struct cvsroot *root)
{
	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_cmd_update.cmd_flags & CVS_CMD_PRUNEDIRS)
			cvs_sendarg(root, "-P", 0);

		if (Aflag == 1)
			cvs_sendarg(root, "-A", 0);

		if (dflag == 1)
			cvs_sendarg(root, "-d", 0);

		if (rev != NULL) {
			cvs_sendarg(root, "-r", 0);
			cvs_sendarg(root, rev, 0);
		}

		if (date != NULL) {
			cvs_sendarg(root, "-D", 0);
			cvs_sendarg(root, date, 0);
		}
	}

	return (0);
}

/*
 * cvs_update_remote()
 *
 * Update a single file.  In the case where we act as client, send any
 * pertinent information about that file to the server.
 */
static int
cvs_update_remote(CVSFILE *cf, void *arg)
{
	char fpath[MAXPATHLEN];
	struct cvsroot *root;

	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		else
			cvs_senddir(root, cf);
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	cvs_sendentry(root, cf);

	if (!(cf->cf_flags & CVS_FILE_ONDISK))
		return (0);

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		cvs_sendreq(root, CVS_REQ_MODIFIED, cf->cf_name);
		cvs_sendfile(root, fpath);
		break;
	default:
		break;
	}

	return (0);
}

/*
 * cvs_update_local()
 */
static int
cvs_update_local(CVSFILE *cf, void *arg)
{
	int ret, islocal, revdiff;
	char fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	char *repo;
	RCSFILE *rf;
	RCSNUM *frev;
	BUF *fbuf;
	struct cvsroot *root;

	revdiff = ret = 0;
	rf = NULL;
	frev = NULL;
	islocal = (cvs_cmdop != CVS_OP_SERVER);

	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);
	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		if (verbosity > 1)
			cvs_printf("? %s\n", fpath);
		return (CVS_EX_OK);
	}

	if (cf->cf_type == DT_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Updating %s", fpath);
		return (CVS_EX_OK);
	}

	cvs_rcs_getpath(cf, rcspath, sizeof(rcspath));

	/*
	 * Only open the RCS file for files that have not been added.
	 */
	if (cf->cf_cvstat != CVS_FST_ADDED) {
		rf = rcs_open(rcspath, RCS_READ);

		/*
		 * If there is no RCS file available in the repository
		 * directory that matches this file, it's gone.
		 * XXX: so what about the Attic?
		 */
		if (rf == NULL) {
			cvs_log(LP_WARN, "%s is no longer in the repository",
			    fpath);
			if (cvs_checkout_rev(NULL, NULL, cf, fpath,
			    islocal, CHECKOUT_REV_REMOVED) < 0)
				fatal("cvs_update_local: cvs_checkout_rev failed");
			return (CVS_EX_OK);
		}
	} else {
		/* There's no need to update a newly added file */
		cvs_printf("A %s\n", fpath);
		return (CVS_EX_OK);
	}

	/* set keyword expansion */
	/* XXX look at cf->cf_opts as well for this */
	if (rcs_kwexp_set(rf, kflag) < 0)
		fatal("cvs_update_local: rcs_kwexp_set failed");

	/* fill in the correct revision */
	if (rev != NULL) {
		if ((frev = rcsnum_parse(rev)) == NULL)
			fatal("cvs_update_local: rcsnum_parse failed");
	} else
		frev = rf->rf_head;

	/*
	 * Compare the headrevision with the revision we currently have.
	 */
	if (cf->cf_lrev != NULL)
		revdiff = rcsnum_cmp(cf->cf_lrev, frev, 0);

	switch (cf->cf_cvstat) {
	case CVS_FST_MODIFIED:
		/*
		 * If the file has been modified but there is a newer version
		 * available, we try to merge it into the existing changes.
		 */
		if (revdiff == 1) {
			fbuf = cvs_diff3(rf, fpath, cf->cf_lrev, frev);
			if (fbuf == NULL) {
				cvs_log(LP_ERR, "merge failed");
				break;
			}

			/*
			 * Please note fbuf will be free'd in cvs_checkout_rev
			 */
			if (cvs_checkout_rev(rf, frev, cf, fpath, islocal,
			    CHECKOUT_REV_MERGED, fbuf) != -1) {
				cvs_printf("%c %s\n",
				    (diff3_conflicts > 0) ? 'C' : 'M',
				    fpath);
				if (diff3_conflicts > 0)
					cf->cf_cvstat = CVS_FST_CONFLICT;
			}
		} else {
			cvs_printf("M %s\n", fpath);
		}
		break;
	case CVS_FST_REMOVED:
		cvs_printf("R %s\n", fpath);
		break;
	case CVS_FST_CONFLICT:
		cvs_printf("C %s\n", fpath);
		break;
	case CVS_FST_LOST:
		if (cvs_checkout_rev(rf, frev, cf, fpath, islocal,
		    CHECKOUT_REV_UPDATED) != -1) {
			cf->cf_cvstat = CVS_FST_UPTODATE;
			cvs_printf("U %s\n", fpath);
		}
		break;
	case CVS_FST_UPTODATE:
		if (revdiff == 1) {
			if (cvs_checkout_rev(rf, frev, cf, fpath, islocal,
			    CHECKOUT_REV_UPDATED) != -1)
				cvs_printf("P %s\n", fpath);
		}
		break;
	default:
		break;
	}

	if ((frev != NULL) && (frev != rf->rf_head))
		rcsnum_free(frev);
	rcs_close(rf);

	return (CVS_EX_OK);
}
