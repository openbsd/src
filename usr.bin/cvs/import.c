/*	$OpenBSD: import.c,v 1.77 2007/09/22 16:01:22 joris Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

void	cvs_import_local(struct cvs_file *);

static void import_new(struct cvs_file *);
static void import_update(struct cvs_file *);
static void import_tag(struct cvs_file *, RCSNUM *, RCSNUM *);
static BUF *import_get_rcsdiff(struct cvs_file *, RCSNUM *);

#define IMPORT_DEFAULT_BRANCH	"1.1.1"

static char *import_branch = IMPORT_DEFAULT_BRANCH;
static char *logmsg = NULL;
static char *vendor_tag = NULL;
static char *release_tag = NULL;
static char *koptstr;
static int kflag = RCS_KWEXP_DEFAULT;
static int dflag = 0;

char *import_repository = NULL;
int import_conflicts = 0;

struct cvs_cmd cvs_cmd_import = {
	CVS_OP_IMPORT, 0, "import",
	{ "im", "imp" },
	"Import sources into CVS, using vendor branches",
	"[-b branch] [-d] [-k mode] [-m message] "
	"repository vendor-tag release-tags",
	"b:dk:m:",
	NULL,
	cvs_import
};

int
cvs_import(int argc, char **argv)
{
	int ch;
	char repo[MAXPATHLEN], *arg = ".";
	struct cvs_recursion cr;

	while ((ch = getopt(argc, argv, cvs_cmd_import.cmd_opts)) != -1) {
		switch (ch) {
		case 'b':
			import_branch = optarg;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expension mode");
				fatal("%s", cvs_cmd_import.cmd_synopsis);
			}
			break;
		case 'm':
			logmsg = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_import.cmd_synopsis);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 3)
		fatal("%s", cvs_cmd_import.cmd_synopsis);

	if (logmsg == NULL)
		logmsg = cvs_logmsg_create(NULL, NULL, NULL);

	if (logmsg == NULL)
		fatal("This shouldnt happen, honestly!");

	import_repository = argv[0];
	vendor_tag = argv[1];
	release_tag = argv[2];

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();

		cvs_client_send_request("Argument -b%s", IMPORT_DEFAULT_BRANCH);

		if (kflag != RCS_KWEXP_DEFAULT)
			cvs_client_send_request("Argument -k%s", koptstr);

		cvs_client_send_logmsg(logmsg);
		cvs_client_send_request("Argument %s", import_repository);
		cvs_client_send_request("Argument %s", vendor_tag);
		cvs_client_send_request("Argument %s", release_tag);

		cr.enterdir = NULL;
		cr.leavedir = NULL;
		cr.fileproc = cvs_client_sendfile;
		cr.flags = CR_RECURSE_DIRS;

		cvs_file_run(1, &arg, &cr);
		cvs_client_senddir(".");
		cvs_client_send_request("import");

		cvs_client_get_responses();
		return (0);
	}

	(void)xsnprintf(repo, sizeof(repo), "%s/%s",
	    current_cvsroot->cr_dir, import_repository);

	if (cvs_noexec != 1) {
		if (mkdir(repo, 0755) == -1 && errno != EEXIST)
			fatal("cvs_import: %s: %s", repo, strerror(errno));
	}

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.fileproc = cvs_import_local;
	cr.flags = CR_RECURSE_DIRS;
	cvs_file_run(1, &arg, &cr);

	if (import_conflicts != 0) {
		cvs_printf("\n%d conflicts created by this import.\n\n",
		    import_conflicts);
		cvs_printf("Use the following command to help the merge:\n");
		cvs_printf("\topencvs checkout ");
		cvs_printf("-j%s:yesterday -j%s %s\n\n", vendor_tag,
		    vendor_tag, import_repository);
	} else {
		cvs_printf("\nNo conflicts created by this import.\n\n");
	}

	return (0);
}

void
cvs_import_local(struct cvs_file *cf)
{
	int isnew;
	struct stat st;
	char repo[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_import_local(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_type == CVS_DIR) {
		if (!strcmp(cf->file_path, "."))
			return;

		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Importing %s", cf->file_path);

		if (cvs_noexec == 1)
			return;

		if (mkdir(cf->file_rpath, 0755) == -1 && errno != EEXIST)
			fatal("cvs_import_local: %s: %s", cf->file_rpath,
			    strerror(errno));

		return;
	}

	isnew = 1;
	(void)xsnprintf(repo, sizeof(repo), "%s/%s/%s/%s%s",
	    current_cvsroot->cr_dir, cf->file_wd, CVS_PATH_ATTIC,
	    cf->file_name, RCS_FILE_EXT);

	if (cf->file_rcs != NULL || stat(repo, &st) != -1)
		isnew = 0;

	if (isnew == 1)
		import_new(cf);
	else
		import_update(cf);
}

static void
import_new(struct cvs_file *cf)
{
	BUF *bp;
	time_t tstamp;
	struct stat st;
	struct rcs_branch *brp;
	struct rcs_delta *rdp;
	RCSNUM *branch, *brev;

	tstamp = -1;

	cvs_log(LP_TRACE, "import_new(%s)", cf->file_name);

	if (cvs_noexec == 1) {
		cvs_printf("N %s/%s\n", import_repository, cf->file_path);
		return;
	}

	if (dflag == 1) {
		if (fstat(cf->fd, &st) == -1)
			fatal("import_new: %s", strerror(errno));

		tstamp = st.st_mtime;
	}

	if ((branch = rcsnum_parse(import_branch)) == NULL)
		fatal("import_new: failed to parse branch");

	if ((bp = cvs_buf_load_fd(cf->fd, BUF_AUTOEXT)) == NULL)
		fatal("import_new: failed to load %s", cf->file_path);

	if ((brev = rcsnum_brtorev(branch)) == NULL)
		fatal("import_new: failed to get first branch revision");

	cf->repo_fd = open(cf->file_rpath, O_CREAT|O_TRUNC|O_WRONLY);
	if (cf->repo_fd < 0)
		fatal("import_new: %s: %s", cf->file_rpath, strerror(errno));

	cf->file_rcs = rcs_open(cf->file_rpath, cf->repo_fd, RCS_CREATE, 0444);
	if (cf->file_rcs == NULL)
		fatal("import_new: failed to create RCS file for %s",
		    cf->file_path);

	rcs_branch_set(cf->file_rcs, branch);

	if (rcs_sym_add(cf->file_rcs, vendor_tag, branch) == -1)
		fatal("import_new: failed to add release tag");

	if (rcs_sym_add(cf->file_rcs, release_tag, brev) == -1)
		fatal("import_new: failed to add vendor tag");

	if (rcs_rev_add(cf->file_rcs, brev, logmsg, tstamp, NULL) == -1)
		fatal("import_new: failed to create first branch revision");

	if (rcs_rev_add(cf->file_rcs, RCS_HEAD_REV, "Initial revision",
	    tstamp, NULL) == -1)
		fatal("import_new: failed to create first revision");

	if ((rdp = rcs_findrev(cf->file_rcs, cf->file_rcs->rf_head)) == NULL)
		fatal("import_new: cannot find newly added revision");

	brp = xmalloc(sizeof(*brp));
	brp->rb_num = rcsnum_alloc();
	rcsnum_cpy(brev, brp->rb_num, 0);
	TAILQ_INSERT_TAIL(&(rdp->rd_branches), brp, rb_list);

	if (rcs_deltatext_set(cf->file_rcs,
	    cf->file_rcs->rf_head, bp) == -1)
		fatal("import_new: failed to set deltatext");

	if (kflag != RCS_KWEXP_DEFAULT)
		rcs_kwexp_set(cf->file_rcs, kflag);

	rcs_write(cf->file_rcs);
	cvs_printf("N %s/%s\n", import_repository, cf->file_path);

	rcsnum_free(branch);
	rcsnum_free(brev);
}

static void
import_update(struct cvs_file *cf)
{
	int ret;
	BUF *b1, *b2, *d;
	char branch[CVS_REV_BUFSZ];
	RCSNUM *newrev, *rev, *brev, *hrev;

	cvs_log(LP_TRACE, "import_update(%s)", cf->file_path);

	if ((rev = rcs_translate_tag(import_branch, cf->file_rcs)) == NULL)
		fatal("import_update: could not translate tag `%s'", import_branch);

	if ((brev = rcsnum_parse(import_branch)) == NULL)
		fatal("import_update: rcsnum_parse failed");

	if (rev != NULL) {
		if ((b1 = rcs_rev_getbuf(cf->file_rcs, rev, 0)) == NULL)
			fatal("import_update: failed to grab revision");

		if ((b2 = cvs_buf_load_fd(cf->fd, BUF_AUTOEXT)) == NULL)
			fatal("import_update: failed to load %s",
			    cf->file_path);

		ret = cvs_buf_differ(b1, b2);
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		if (ret == 0) {
			import_tag(cf, brev, rev);
			rcsnum_free(brev);
			rcs_write(cf->file_rcs);
			return;
		}
	}

	if (cf->file_rcs->rf_branch != NULL)
		rcsnum_tostr(cf->file_rcs->rf_branch, branch, sizeof(branch));

	if (rev != NULL) {
		d = import_get_rcsdiff(cf, rev);
		newrev = rcsnum_inc(rev);
	} else {
		hrev = rcs_head_get(cf->file_rcs);
		d = import_get_rcsdiff(cf, hrev);
		rcsnum_free(hrev);
		newrev = rcsnum_brtorev(brev);
	}

	if (rcs_rev_add(cf->file_rcs, newrev, logmsg, -1, NULL) == -1)
		fatal("import_update: failed to add new revision");

	if (rcs_deltatext_set(cf->file_rcs, newrev, d) == -1)
		fatal("import_update: failed to set deltatext");

	import_tag(cf, brev, newrev);

	if (cf->file_rcs->rf_branch == NULL || cf->in_attic == 1 ||
	    strcmp(branch, import_branch)) {
		import_conflicts++;
		cvs_printf("C %s/%s\n", import_repository, cf->file_path);
	} else {
		cvs_printf("U %s/%s\n", import_repository, cf->file_path);
	}

	if (kflag != RCS_KWEXP_DEFAULT)
		rcs_kwexp_set(cf->file_rcs, kflag);

	rcsnum_free(brev);
	rcs_write(cf->file_rcs);
}

static void
import_tag(struct cvs_file *cf, RCSNUM *branch, RCSNUM *newrev)
{
	char b[CVS_REV_BUFSZ];

	if (cvs_noexec != 1) {
		rcsnum_tostr(branch, b, sizeof(b));
		rcs_sym_add(cf->file_rcs, vendor_tag, branch);

		rcsnum_tostr(newrev, b, sizeof(b));
		rcs_sym_add(cf->file_rcs, release_tag, newrev);
	}
}

static BUF *
import_get_rcsdiff(struct cvs_file *cf, RCSNUM *rev)
{
	char *p1, *p2;
	BUF *b1, *b2;

	b2 = cvs_buf_alloc(128, BUF_AUTOEXT);

	if (cvs_noexec != 1) {
		if ((b1 = cvs_buf_load_fd(cf->fd, BUF_AUTOEXT)) == NULL)
			fatal("import_get_rcsdiff: failed loading %s",
			    cf->file_path);

		(void)xasprintf(&p1, "%s/diff1.XXXXXXXXXX", cvs_tmpdir);
		cvs_buf_write_stmp(b1, p1, NULL);
		cvs_buf_free(b1);

		(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);
		rcs_rev_write_stmp(cf->file_rcs, rev, p2, 0);

		diff_format = D_RCSDIFF;
		if (cvs_diffreg(p2, p1, b2) == D_ERROR)
			fatal("import_get_rcsdiff: failed to get RCS patch");

		(void)unlink(p1);
		(void)unlink(p2);

		xfree(p1);
		xfree(p2);
	}

	return (b2);
}
