/*	$OpenBSD: import.c,v 1.46 2006/05/30 21:41:00 joris Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "diff.h"
#include "log.h"
#include "proto.h"

int	cvs_import(int, char **);
void	cvs_import_local(struct cvs_file *);

static void import_new(struct cvs_file *);
static void import_update(struct cvs_file *);

#define IMPORT_DEFAULT_BRANCH	"1.1.1"

static char *import_branch = IMPORT_DEFAULT_BRANCH;
static char *logmsg = NULL;
static char *vendor_tag = NULL;
static char *release_tag = NULL;

char *import_repository = NULL;

struct cvs_cmd cvs_cmd_import = {
	CVS_OP_IMPORT, CVS_REQ_IMPORT, "import",
	{ "im", "imp" },
	"Import sources into CVS, using vendor branches",
	"[-b vendor branch id] [-m message] repository vendor-tag release-tags",
	"b:m:",
	NULL,
	cvs_import
};

int
cvs_import(int argc, char **argv)
{
	int ch, l;
	char repo[MAXPATHLEN], *arg = ".";
	struct cvs_recursion cr;

	while ((ch = getopt(argc, argv, cvs_cmd_import.cmd_opts)) != -1) {
		switch (ch) {
		case 'b':
			import_branch = optarg;
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
		fatal("please specify a logmessage using -m for now");

	import_repository = argv[0];
	vendor_tag = argv[1];
	release_tag = argv[2];

	l = snprintf(repo, sizeof(repo), "%s/%s", current_cvsroot->cr_dir,
	    import_repository);
	if (l == -1 || l >= (int)sizeof(repo))
		fatal("cvs_import: overflow");

	if (mkdir(repo, 0755) == -1 && errno != EEXIST)
		fatal("cvs_import: %s: %s", repo, strerror(errno));

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.remote = NULL;
	cr.local = cvs_import_local;
	cr.flags = CR_RECURSE_DIRS;
	cvs_file_run(1, &arg, &cr);

	return (0);
}

void
cvs_import_local(struct cvs_file *cf)
{
	int l;
	int isnew;
	struct stat st;
	char repo[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_import_local(%s)", cf->file_path);

	cvs_file_classify(cf, NULL, 0);

	if (cf->file_type == CVS_DIR) {
		if (!strcmp(cf->file_path, "."))
			return;

		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Importing %s", cf->file_path);


		if (mkdir(cf->file_rpath, 0755) == -1 && errno != EEXIST)
			fatal("cvs_import_local: %s: %s", cf->file_rpath,
			    strerror(errno));

		return;
	}

	isnew = 1;
	l = snprintf(repo, sizeof(repo), "%s/%s/%s/%s%s",
	    current_cvsroot->cr_dir, cf->file_wd, CVS_PATH_ATTIC,
	    cf->file_name, RCS_FILE_EXT);
	if (l == -1 || l >= (int)sizeof(repo))
		fatal("import_new: overflow");

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
	char *content;
	struct rcs_branch *brp;
	struct rcs_delta *rdp;
	RCSNUM *branch, *brev;

	cvs_log(LP_TRACE, "import_new(%s)", cf->file_name);

	if ((branch = rcsnum_parse(import_branch)) == NULL)
		fatal("import_new: failed to parse branch");

	if ((bp = cvs_buf_load(cf->file_path, BUF_AUTOEXT)) == NULL)
		fatal("import_new: failed to load %s", cf->file_path);

	cvs_buf_putc(bp, '\0');
	content = cvs_buf_release(bp);

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

	if (rcs_sym_add(cf->file_rcs, release_tag, branch) == -1)
		fatal("import_new: failed to add vendor tag");

	if (rcs_rev_add(cf->file_rcs, brev, logmsg, -1, NULL) == -1)
		fatal("import_new: failed to create first branch revision");

	if (rcs_rev_add(cf->file_rcs, RCS_HEAD_REV, logmsg, -1, NULL) == -1)
		fatal("import_new: failed to create first revision");

	if ((rdp = rcs_findrev(cf->file_rcs, cf->file_rcs->rf_head)) == NULL)
		fatal("import_new: cannot find newly added revision");

	brp = xmalloc(sizeof(*brp));
	brp->rb_num = rcsnum_alloc();
	rcsnum_cpy(brev, brp->rb_num, 0);
	TAILQ_INSERT_TAIL(&(rdp->rd_branches), brp, rb_list);

	if (rcs_deltatext_set(cf->file_rcs,
	    cf->file_rcs->rf_head, content) == -1)
		fatal("import_new: failed to set deltatext");

	rcs_write(cf->file_rcs);
	cvs_printf("N %s\n", cf->file_path);

	rcsnum_free(branch);
	rcsnum_free(brev);
}

static void
import_update(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "import_update(%s)", cf->file_path);
}
