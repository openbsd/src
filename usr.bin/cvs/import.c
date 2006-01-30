/*	$OpenBSD: import.c,v 1.38 2006/01/30 17:58:47 xsa Exp $	*/
/*
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "proto.h"


#define CVS_IMPORT_DEFBRANCH	"1.1.1"


static int	cvs_import_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_import_pre_exec(struct cvsroot *);
static int	cvs_import_pre_exec(struct cvsroot *);
static int	cvs_import_post_exec(struct cvsroot *);
static int	cvs_import_remote(CVSFILE *, void *);
static int	cvs_import_local(CVSFILE *, void *);
static int	cvs_import_cleanup(void);

static int dflag = 0;
static int conflicts = 0;
static RCSNUM *imp_brnum;

static char *module, *vendor, *release;

struct cvs_cmd cvs_cmd_import = {
	CVS_OP_IMPORT, CVS_REQ_IMPORT, "import",
	{ "im", "imp" },
	"Import sources into CVS, using vendor branches",
	"[-d] [-b branch] [-I ign] [-k mode] [-m msg] [-W spec] module "
	"vendortag releasetag ...",
	"b:dI:k:m:W:",
	NULL,
	CF_RECURSE | CF_IGNORE | CF_NOSYMS,
	cvs_import_init,
	cvs_import_pre_exec,
	cvs_import_remote,
	cvs_import_local,
	cvs_import_post_exec,
	cvs_import_cleanup,
	CVS_CMD_SENDDIR
};

static int
cvs_import_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'b':
			if ((imp_brnum = rcsnum_parse(optarg)) == NULL) {
				cvs_log(LP_ERR, "%s is not a numeric branch",
				    optarg);
				return (CVS_EX_USAGE);
			}
			break;
		case 'd':
			dflag = 1;
			break;
		case 'I':
			if (cvs_file_ignore(optarg) < 0) {
				cvs_log(LP_ERR, "failed to add `%s' to list "
				    "of ignore patterns", optarg);
				return (CVS_EX_USAGE);
			}
			break;
		case 'k':
			break;
		case 'm':
			cvs_msg = xstrdup(optarg);
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 3)
		return (CVS_EX_USAGE);

	if ((imp_brnum == NULL) &&
	    ((imp_brnum = rcsnum_parse(CVS_IMPORT_DEFBRANCH)) == NULL))
		fatal("cvs_import_init: rcsnum_parse failed");

	module = argv[0];
	vendor = argv[1];
	release = argv[2];

	*arg = optind + 3;

	if (cvs_msg == NULL)
		cvs_msg = cvs_logmsg_get(NULL, NULL, NULL, NULL);

	return (0);
}

static int
cvs_import_pre_exec(struct cvsroot *root)
{
	char numbuf[64], repodir[MAXPATHLEN];

	if (root->cr_method == CVS_METHOD_LOCAL) {
		if (cvs_path_cat(root->cr_dir, module, repodir,
		    sizeof(repodir)) >= sizeof(repodir))
			fatal("cvs_import_pre_exec: cvs_path_cat overflow");

		if (mkdir(repodir, 0700) == -1)
			fatal("cvs_import_pre_exec: mkdir `%s': %s",
			    repodir, strerror(errno));
	} else {
		rcsnum_tostr(imp_brnum, numbuf, sizeof(numbuf));

		cvs_sendarg(root, "-b", 0);
		cvs_sendarg(root, numbuf, 0);
		cvs_logmsg_send(root, cvs_msg);
		cvs_sendarg(root, module, 0);
		cvs_sendarg(root, vendor, 0);
		cvs_sendarg(root, release, 0);
	}

	return (0);
}

static int
cvs_import_post_exec(struct cvsroot *root)
{
	char buf[8];

	if (root->cr_method == CVS_METHOD_LOCAL) {
		if (conflicts > 0)
			snprintf(buf, sizeof(buf), "%d", conflicts);

		if (verbosity > 0)
			cvs_printf("\n%s conflicts created by this import\n\n",
			    conflicts == 0 ? "No" : buf);
	}

	return (CVS_EX_OK);
}

/*
 * cvs_import_remote()
 *
 * Perform the import of a single file or directory.
 */
static int
cvs_import_remote(CVSFILE *cf, void *arg)
{
	size_t sz;
	struct cvsroot *root;
	char fpath[MAXPATHLEN], repodir[MAXPATHLEN];
	char repo[MAXPATHLEN], date[32];

	root = CVS_DIR_ROOT(cf);

	if (cvs_path_cat(root->cr_dir, module, repo, sizeof(repo)) >=
	    sizeof(repo))
		fatal("cvs_import_remove: cvs_path_cat overflow");

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_type == DT_DIR) {
		if (!strcmp(cf->cf_name, "."))
			strlcpy(repodir, repo, sizeof(repodir));
		else {
			if(cvs_path_cat(repo, fpath, repodir,
			    sizeof(repodir)) >= sizeof(repodir))
				fatal("cvs_import_remove: cvs_path_cat overflow");
		}

		cvs_sendreq(root, CVS_REQ_DIRECTORY, fpath);
		cvs_sendln(root, repodir);
		return (0);
	}

	if (dflag == 1) {
		ctime_r(&(cf->cf_mtime), date);
		sz = strlen(date);
		if ((sz > 0) && (date[sz - 1] == '\n'))
			date[--sz] = '\0';
		cvs_sendreq(root, CVS_REQ_CHECKINTIME, date);
	}

	cvs_sendreq(root, CVS_REQ_MODIFIED, cf->cf_name);
	cvs_sendfile(root, fpath);

	return (0);
}

static int
cvs_import_local(CVSFILE *cf, void *arg)
{
	time_t stamp;
	char *fcont;
	char fpath[MAXPATHLEN], rpath[MAXPATHLEN], repo[MAXPATHLEN];
	const char *comment;
	struct stat fst;
	struct timeval ts[2];
	struct cvsroot *root;
	RCSFILE *rf;
	RCSNUM *rev;
	BUF *bp;

	root = CVS_DIR_ROOT(cf);

	if (cvs_path_cat(root->cr_dir, module, repo, sizeof(repo)) >=
	    sizeof(repo))
		fatal("cvs_import_local: cvs_path_cat overflow");

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_type == DT_DIR) {
		if (!strcmp(cf->cf_name, "."))
			strlcpy(rpath, repo, sizeof(rpath));
		else {
			if (cvs_path_cat(repo, fpath, rpath,
			    sizeof(rpath)) >= sizeof(rpath))
				fatal("cvs_import_local: cvs_path_cat overflow");

			cvs_printf("Importing %s\n", rpath);
			if (mkdir(rpath, 0700) == -1) {
				cvs_log(LP_ERRNO, "failed to create %s",
				    rpath);
			}
		}

		return (0);
	}

	/*
	 * If -d was given, use the file's last modification time as the
	 * timestamps for the initial revisions.
	 */
	if (dflag == 1) {
		if (stat(fpath, &fst) == -1)
			fatal("cvs_import_local: stat failed on `%s': %s",
			    fpath, strerror(errno));

		stamp = (time_t)fst.st_mtime;

		ts[0].tv_sec = stamp;
		ts[0].tv_usec = 0;
		ts[1].tv_sec = stamp;
		ts[1].tv_usec = 0;
	} else
		stamp = -1;

	if (strlcpy(rpath, repo, sizeof(rpath)) >= sizeof(rpath) ||
	    strlcat(rpath, "/", sizeof(rpath)) >= sizeof(rpath) ||
	    strlcat(rpath, fpath, sizeof(rpath)) >= sizeof(rpath) ||
	    strlcat(rpath, RCS_FILE_EXT, sizeof(rpath)) >= sizeof(rpath))
		fatal("cvs_import_local: path truncation");

	cvs_printf("N %s\n", fpath);

	if ((rf = rcs_open(rpath, RCS_RDWR|RCS_CREATE)) == NULL);
		fatal("cvs_import_local: rcs_open: `%s': %s", rpath,
		    rcs_errstr(rcs_errno));

	comment = rcs_comment_lookup(cf->cf_name);
	if ((comment != NULL) && (rcs_comment_set(rf, comment) < 0)) {
		cvs_log(LP_WARN, "failed to set RCS comment leader: %s",
		    rcs_errstr(rcs_errno));
		/* don't error out, no big deal */
	}

	rev = rcsnum_brtorev(imp_brnum);
	if (rcs_rev_add(rf, rev, cvs_msg, stamp, NULL) < 0) {
		(void)unlink(rpath);
		fatal("cvs_import_local: rcs_rev_add failed: %s",
		    rcs_errstr(rcs_errno));
	}

	if (rcs_sym_add(rf, release, rev) < 0) {
		(void)unlink(rpath);
		fatal("cvs_import_local: rcs_sym_add failed: %s",
		    rcs_errstr(rcs_errno));
	}

	rcsnum_cpy(imp_brnum, rev, 2);
	if (rcs_rev_add(rf, rev, cvs_msg, stamp, NULL) < 0) {
		(void)unlink(rpath);
		fatal("cvs_import_local: rcs_rev_add failed: %s",
		    rcs_errstr(rcs_errno));
	}

	if (rcs_head_set(rf, rev) < 0) {
		(void)unlink(rpath);
		fatal("cvs_import_local: rcs_head_set failed: %s",
		    rcs_errstr(rcs_errno));
	}

	if (rcs_branch_set(rf, imp_brnum) < 0) {
		(void)unlink(rpath);
		fatal("cvs_import_local: rcs_branch_set failed: %s",
		    rcs_errstr(rcs_errno));
	}

	if (rcs_sym_add(rf, vendor, imp_brnum) < 0) {
		(void)unlink(rpath);
		fatal("cvs_import_local: rcs_sym_add failed: %s",
		    rcs_errstr(rcs_errno));
	}

	if ((bp = cvs_buf_load(fpath, BUF_AUTOEXT)) == NULL) {
		(void)unlink(rpath);
		fatal("cvs_import_local: cvs_buf_load failed");
	}

	cvs_buf_putc(bp, '\0');

	fcont = cvs_buf_release(bp);

	if (rcs_deltatext_set(rf, rev, fcont) < 0) {
		(void)unlink(rpath);
		fatal("cvs_import_local: rcs_deltatext_set failed");
	}

	/* add the vendor tag and release tag as symbols */
	rcs_close(rf);

	if ((dflag ==1) && (utimes(rpath, ts) == -1))
		cvs_log(LP_ERRNO, "failed to timestamp RCS file");

	return (0);
}

static int
cvs_import_cleanup(void)
{
	if (imp_brnum != NULL)
		rcsnum_free(imp_brnum);
	return (0);
}
