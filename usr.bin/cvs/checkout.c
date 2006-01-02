/*	$OpenBSD: checkout.c,v 1.44 2006/01/02 08:11:56 xsa Exp $	*/
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


#define CVS_LISTMOD	1
#define CVS_STATMOD	2

static int	cvs_checkout_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_checkout_pre_exec(struct cvsroot *);
static int	cvs_checkout_local(CVSFILE *cf, void *);

struct cvs_cmd cvs_cmd_checkout = {
	CVS_OP_CHECKOUT, CVS_REQ_CO, "checkout",
	{ "co", "get" },
	"Checkout sources for editing",
	"[-AcflNnPpRs] [-D date | -r tag] [-d dir] [-j rev] [-k mode] "
	"[-t id] module ...",
	"AcD:d:fj:k:lNnPRr:st:",
	NULL,
	0,
	cvs_checkout_init,
	cvs_checkout_pre_exec,
	NULL,
	NULL,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};

struct cvs_cmd cvs_cmd_export = {
	CVS_OP_EXPORT, CVS_REQ_EXPORT, "export",
	{ "ex", "exp" },
	"Extract copy of a module without management directories",
	"[-flNnR] [-d dir] [-k mode] -D date | -r tag module ...",
	"D:d:fk:lNnRr:",
	NULL,
	0,
	cvs_checkout_init,
	cvs_checkout_pre_exec,
	NULL,
	NULL,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};

static char *currepo = NULL;
static DIR *dirp = NULL;
static int cwdfd = -1;
static char *date, *tag, *koptstr, *tgtdir, *rcsid;
static int statmod = 0;
static int shorten = 0;
static int usehead = 0;
static int kflag = RCS_KWEXP_DEFAULT;

/* modules */
static char **co_mods;
static int    co_nmod;

/* XXX checkout has issues in remote mode, -N gets seen as module */

static int
cvs_checkout_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;
	RCSNUM *rcs;

	date = tag = koptstr = tgtdir = rcsid = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			break;
		case 'c':
			statmod = CVS_LISTMOD;
			break;
		case 'D':
			date = optarg;
			cmd->cmd_flags |= CVS_CMD_PRUNEDIRS;
			break;
		case 'd':
			tgtdir = optarg;
			shorten = 1;
			break;
		case 'f':
			usehead = 1;
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
		case 'P':
			cmd->cmd_flags |= CVS_CMD_PRUNEDIRS;
			break;
		case 'N':
			shorten = 0;
			break;
		case 'p':
			cvs_noexec = 1;	/* no locks will be created */
			break;
		case 'r':
			tag = optarg;
			cmd->cmd_flags |= CVS_CMD_PRUNEDIRS;
			break;
		case 's':
			statmod = CVS_STATMOD;
			break;
		case 't':
			rcsid = optarg;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	co_mods = argv;
	co_nmod = argc;

	if ((statmod == 0) && (argc == 0)) {
		cvs_log(LP_ABORT,
		    "must specify at least one module or directory");
		return (-1);
	}

	if (statmod && (argc > 0)) {
		cvs_log(LP_ABORT,  "-c and -s must not get any arguments");
		return (-1);
	}

	/* `export' command exceptions */
	if (cvs_cmdop == CVS_OP_EXPORT) {
		if (!tag && !date) {
			cvs_log(LP_ABORT, "must specify a tag or date");
			return (-1);
		}

		/* we don't want numerical revisions here */
		if (tag && (rcs = rcsnum_parse(tag)) != NULL) {
			cvs_log(LP_ABORT, "tag `%s' must be a symbolic tag",
			    tag);
			rcsnum_free(rcs);
			return (-1);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_checkout_pre_exec(struct cvsroot *root)
{
	int i, ret;
	char *sp, repo[MAXPATHLEN];

	if ((dirp = opendir(".")) == NULL) {
		cvs_log(LP_ERRNO, "failed to save cwd");
		return (CVS_EX_DATA);
	}

	cwdfd = dirfd(dirp);

	for (i = 0; i < co_nmod; i++) {
		if ((sp = strchr(co_mods[i], '/')) != NULL)
			*sp = '\0';

		if ((mkdir(co_mods[i], 0755) == -1) && (errno != EEXIST)) {
			cvs_log(LP_ERRNO, "can't create base directory '%s'",
			    co_mods[i]);
			return (CVS_EX_DATA);
		}

		if (cvs_mkadmin(co_mods[i], root->cr_str, co_mods[i],
		    NULL, NULL, 0) < 0) {
			cvs_log(LP_ERR, "can't create base directory '%s'",
			    co_mods[i]);
			return (CVS_EX_DATA);
		}

		if (sp != NULL)
			*sp = '/';
	}

	if (root->cr_method == CVS_METHOD_LOCAL) {
		if ((dirp = opendir(".")) == NULL)
			return (CVS_EX_DATA);
		cwdfd = dirfd(dirp);

		for (i = 0; i < co_nmod; i++) {
			snprintf(repo, sizeof(repo), "%s/%s", root->cr_dir,
			    co_mods[i]);
			currepo = co_mods[i];
			ret = cvs_file_get(repo, CF_RECURSE | CF_REPO |
			    CF_IGNORE, cvs_checkout_local, NULL, NULL);
			if (ret != CVS_EX_OK) {
				closedir(dirp);
				return (ret);
			}
		}

		closedir(dirp);
	} else {
		/*
		 * These arguments are for the expand-modules
		 * command that we send to the server before requesting
		 * a checkout.
		 */
		for (i = 0; i < co_nmod; i++)
			cvs_sendarg(root, co_mods[i], 0);

		cvs_sendreq(root, CVS_REQ_DIRECTORY, ".");
		cvs_sendln(root, root->cr_dir);
		cvs_sendreq(root, CVS_REQ_XPANDMOD, NULL);

		if (usehead == 1)
			cvs_sendarg(root, "-f", 0);

		if (tgtdir != NULL) {
			cvs_sendarg(root, "-d", 0);
			cvs_sendarg(root, tgtdir, 0);
		}

		if (shorten == 0)
			cvs_sendarg(root, "-N", 0);

		if (cvs_cmd_checkout.cmd_flags & CVS_CMD_PRUNEDIRS);
			cvs_sendarg(root, "-P", 0);

		for (i = 0; i < co_nmod; i++)
			cvs_sendarg(root, co_mods[i], 0);

		if (statmod == CVS_LISTMOD)
			cvs_sendarg(root, "-c", 0);
		else if (statmod == CVS_STATMOD)
			cvs_sendarg(root, "-s", 0);

		if (tag != NULL) {
			cvs_sendarg(root, "-r", 0);
			cvs_sendarg(root, tag, 0);
		}

		if (date != NULL) {
			cvs_sendarg(root, "-D", 0);
			cvs_sendarg(root, date, 0);
		}
	}

	return (0);
}

static int
cvs_checkout_local(CVSFILE *cf, void *arg)
{
	char rcspath[MAXPATHLEN], fpath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;
	static int inattic = 0;

	/* we don't want these */
	if ((cf->cf_type == DT_DIR) && !strcmp(cf->cf_name, "Attic")) {
		inattic = 1;
		return (CVS_EX_OK);
	}

	root = CVS_DIR_ROOT(cf);
	cvs_file_getpath(cf, fpath, sizeof(fpath));

	snprintf(rcspath, sizeof(rcspath), "%s/%s%s", root->cr_dir,
	    fpath, RCS_FILE_EXT);

	if (cf->cf_type == DT_DIR) {
		inattic = 0;
		if (verbosity > 1)
			cvs_log(LP_INFO, "Updating %s", fpath);

		if (cvs_cmdop != CVS_OP_SERVER) {
			/*
			 * We pass an empty repository name to
			 * cvs_create_dir(), because it will correctly
			 * create the repository directory for us.
			 */
			if (cvs_create_dir(fpath, 1, root->cr_dir, NULL) < 0)
				return (CVS_EX_FILE);
			if (fchdir(cwdfd) < 0) {
				cvs_log(LP_ERRNO, "fchdir failed");
				return (CVS_EX_FILE);
			}
		} else {
			/*
			 * TODO: send responses to client so it'll
			 * create it's directories.
			 */
		}

		return (CVS_EX_OK);
	}

	if (inattic == 1)
		return (CVS_EX_OK);

	if ((rf = rcs_open(rcspath, RCS_READ)) == NULL) {
		cvs_log(LP_ERR, "cvs_checkout_local: rcs_open failed");
		return (CVS_EX_DATA);
	}

	if (cvs_checkout_rev(rf, rf->rf_head, cf, fpath,
	    (cvs_cmdop != CVS_OP_SERVER) ? 1 : 0,
	    CHECKOUT_REV_CREATED) < 0) {
		rcs_close(rf);
		return (CVS_EX_DATA);
	}

	rcs_close(rf);

	cvs_printf("U %s\n", fpath);
	return (CVS_EX_OK);
}
