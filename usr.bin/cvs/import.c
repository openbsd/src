/*	$OpenBSD: import.c,v 1.104 2015/01/16 06:40:07 deraadt Exp $	*/
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

static void import_loginfo(char *);
static void import_new(struct cvs_file *);
static void import_printf(const char *, ...)
	__attribute__((format(printf, 1, 2)));
static void import_update(struct cvs_file *);
static void import_tag(struct cvs_file *, RCSNUM *, RCSNUM *);
static BUF *import_get_rcsdiff(struct cvs_file *, RCSNUM *);

#define IMPORT_DEFAULT_BRANCH	"1.1.1"

extern char *loginfo;
extern char *logmsg;

static char *import_branch = IMPORT_DEFAULT_BRANCH;
static char *vendor_tag = NULL;
static char **release_tags;
static char *koptstr;
static int dflag = 0;
static int tagcount = 0;
static BUF *logbuf;

char *import_repository = NULL;
int import_conflicts = 0;

struct cvs_cmd cvs_cmd_import = {
	CVS_OP_IMPORT, CVS_USE_WDIR, "import",
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
	int i, ch;
	char repo[PATH_MAX], *arg = ".";
	struct cvs_recursion cr;
	struct trigger_list *line_list;

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
				    "invalid RCS keyword expansion mode");
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

	import_repository = argv[0];
	vendor_tag = argv[1];
	argc -= 2;
	argv += 2;

	release_tags = argv;
	tagcount = argc;

	if (!rcs_sym_check(vendor_tag))
		fatal("invalid symbol: %s", vendor_tag);

	for (i = 0; i < tagcount; i++) {
		if (!rcs_sym_check(release_tags[i]))
			fatal("invalid symbol: %s", release_tags[i]);
	}

	if (logmsg == NULL) {
		if (cvs_server_active)
			fatal("no log message specified");
		else
			logmsg = cvs_logmsg_create(NULL, NULL, NULL, NULL);
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();

		cvs_client_send_request("Argument -b%s", IMPORT_DEFAULT_BRANCH);

		if (kflag)
			cvs_client_send_request("Argument -k%s", koptstr);

		cvs_client_send_logmsg(logmsg);
		cvs_client_send_request("Argument %s", import_repository);
		cvs_client_send_request("Argument %s", vendor_tag);
		for (i = 0; i < tagcount; i++)
			cvs_client_send_request("Argument %s", release_tags[i]);

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

	if (cvs_logmsg_verify(logmsg))
		return (0);

	(void)xsnprintf(repo, sizeof(repo), "%s/%s",
	    current_cvsroot->cr_dir, import_repository);

	import_loginfo(import_repository);

	if (cvs_noexec != 1)
		cvs_mkdir(repo, 0755);

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.fileproc = cvs_import_local;
	cr.flags = CR_RECURSE_DIRS;
	cvs_file_run(1, &arg, &cr);

	if (import_conflicts != 0) {
		import_printf("\n%d conflicts created by this import.\n\n",
		    import_conflicts);
		import_printf("Use the following command to help the merge:\n");
		import_printf("\topencvs checkout ");
		import_printf("-j%s:yesterday -j%s %s\n\n", vendor_tag,
		    vendor_tag, import_repository);
	} else {
		import_printf("\nNo conflicts created by this import.\n\n");
	}

	loginfo = buf_release(logbuf);
	logbuf = NULL;

	line_list = cvs_trigger_getlines(CVS_PATH_LOGINFO, import_repository);
	if (line_list != NULL) {
		cvs_trigger_handle(CVS_TRIGGER_LOGINFO, import_repository,
		    loginfo, line_list, NULL);
		cvs_trigger_freelist(line_list);
	}

	xfree(loginfo);
	return (0);
}

static void
import_printf(const char *fmt, ...)
{
	char *str;
	va_list vap;

	va_start(vap, fmt);
	if (vasprintf(&str, fmt, vap) == -1)
		fatal("import_printf: could not allocate memory");
	va_end(vap);

	cvs_printf("%s", str);
	buf_puts(logbuf, str);

	xfree(str);
}

void
cvs_import_local(struct cvs_file *cf)
{
	int isnew;
	struct stat st;
	char repo[PATH_MAX];

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
import_loginfo(char *repo)
{
	int i;
	char pwd[PATH_MAX];

	if (getcwd(pwd, sizeof(pwd)) == NULL)
		fatal("Can't get working directory");

	logbuf = buf_alloc(1024);
	cvs_trigger_loginfo_header(logbuf, repo);

	buf_puts(logbuf, "Log Message:\n");
	buf_puts(logbuf, logmsg);
	if (logmsg[0] != '\0' && logmsg[strlen(logmsg) - 1] != '\n')
		buf_putc(logbuf, '\n');
	buf_putc(logbuf, '\n');

	buf_puts(logbuf, "Status:\n\n");

	buf_puts(logbuf, "Vendor Tag:\t");
	buf_puts(logbuf, vendor_tag);
	buf_putc(logbuf, '\n');
	buf_puts(logbuf, "Release Tags:\t");

	for (i = 0; i < tagcount ; i++) {
		buf_puts(logbuf, "\t\t");
		buf_puts(logbuf, release_tags[i]);
		buf_putc(logbuf, '\n');
	}
	buf_putc(logbuf, '\n');
	buf_putc(logbuf, '\n');
}

static void
import_new(struct cvs_file *cf)
{
	int i;
	BUF *bp;
	mode_t mode;
	time_t tstamp;
	struct stat st;
	struct rcs_branch *brp;
	struct rcs_delta *rdp;
	RCSNUM *branch, *brev;

	tstamp = -1;

	cvs_log(LP_TRACE, "import_new(%s)", cf->file_name);

	if (cvs_noexec == 1) {
		import_printf("N %s/%s\n", import_repository, cf->file_path);
		return;
	}

	if (fstat(cf->fd, &st) == -1)
		fatal("import_new: %s", strerror(errno));

	mode = st.st_mode;

	if (dflag == 1)
		tstamp = st.st_mtime;

	if ((branch = rcsnum_parse(import_branch)) == NULL)
		fatal("import_new: failed to parse branch");

	bp = buf_load_fd(cf->fd);

	if ((brev = rcsnum_brtorev(branch)) == NULL)
		fatal("import_new: failed to get first branch revision");

	cf->repo_fd = open(cf->file_rpath, O_CREAT | O_RDONLY);
	if (cf->repo_fd < 0)
		fatal("import_new: %s: %s", cf->file_rpath, strerror(errno));

	cf->file_rcs = rcs_open(cf->file_rpath, cf->repo_fd, RCS_CREATE,
	    (mode & ~(S_IWUSR | S_IWGRP | S_IWOTH)));
	if (cf->file_rcs == NULL)
		fatal("import_new: failed to create RCS file for %s",
		    cf->file_path);

	rcs_branch_set(cf->file_rcs, branch);

	if (rcs_sym_add(cf->file_rcs, vendor_tag, branch) == -1)
		fatal("import_new: failed to add vendor tag");

	for (i = 0; i < tagcount; i++) {
		if (rcs_sym_add(cf->file_rcs, release_tags[i], brev) == -1)
			fatal("import_new: failed to add release tag");
	}

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

	if (kflag)
		rcs_kwexp_set(cf->file_rcs, kflag);

	rcs_write(cf->file_rcs);
	import_printf("N %s/%s\n", import_repository, cf->file_path);

	rcsnum_free(branch);
	rcsnum_free(brev);
}

static void
import_update(struct cvs_file *cf)
{
	int ret;
	BUF *b1, *b2, *d;
	char branch[CVS_REV_BUFSZ];
	RCSNUM *newrev, *rev, *brev;

	cvs_log(LP_TRACE, "import_update(%s)", cf->file_path);

	if (cf->file_rcs->rf_head == NULL)
		fatal("no head revision in RCS file for `%s'", cf->file_path);

	if ((rev = rcs_translate_tag(import_branch, cf->file_rcs)) == NULL)
		fatal("import_update: could not translate tag `%s'",
		    import_branch);

	if ((brev = rcsnum_parse(import_branch)) == NULL)
		fatal("import_update: rcsnum_parse failed");

	b1 = rcs_rev_getbuf(cf->file_rcs, rev, RCS_KWEXP_NONE);
	b2 = buf_load_fd(cf->fd);

	ret = buf_differ(b1, b2);
	buf_free(b1);
	buf_free(b2);
	if (ret == 0) {
		import_tag(cf, brev, rev);
		rcsnum_free(brev);
		if (cvs_noexec != 1)
			rcs_write(cf->file_rcs);
		import_printf("U %s/%s\n", import_repository, cf->file_path);
		return;
	}

	if (cf->file_rcs->rf_branch != NULL)
		rcsnum_tostr(cf->file_rcs->rf_branch, branch, sizeof(branch));

	if (cf->file_rcs->rf_branch == NULL || cf->in_attic == 1 ||
	    strcmp(branch, import_branch)) {
		import_conflicts++;
		import_printf("C %s/%s\n", import_repository, cf->file_path);
	} else {
		import_printf("U %s/%s\n", import_repository, cf->file_path);
	}

	if (cvs_noexec == 1)
		return;

	d = import_get_rcsdiff(cf, rev);
	newrev = rcsnum_inc(rev);

	if (rcs_rev_add(cf->file_rcs, newrev, logmsg, -1, NULL) == -1)
		fatal("import_update: failed to add new revision");

	if (rcs_deltatext_set(cf->file_rcs, newrev, d) == -1)
		fatal("import_update: failed to set deltatext");

	import_tag(cf, brev, newrev);

	if (kflag)
		rcs_kwexp_set(cf->file_rcs, kflag);

	rcsnum_free(brev);
	rcs_write(cf->file_rcs);
}

static void
import_tag(struct cvs_file *cf, RCSNUM *branch, RCSNUM *newrev)
{
	int i;

	if (cvs_noexec != 1) {
		rcs_sym_add(cf->file_rcs, vendor_tag, branch);

		for (i = 0; i < tagcount; i++)
			rcs_sym_add(cf->file_rcs, release_tags[i], newrev);
	}
}

static BUF *
import_get_rcsdiff(struct cvs_file *cf, RCSNUM *rev)
{
	char *p1, *p2;
	BUF *b1, *b2;
	int fd1, fd2;

	b2 = buf_alloc(128);

	b1 = buf_load_fd(cf->fd);

	(void)xasprintf(&p1, "%s/diff1.XXXXXXXXXX", cvs_tmpdir);
	fd1 = buf_write_stmp(b1, p1, NULL);
	buf_free(b1);

	(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);
	fd2 = rcs_rev_write_stmp(cf->file_rcs, rev, p2, RCS_KWEXP_NONE);

	diff_format = D_RCSDIFF;
	if (diffreg(p2, p1, fd2, fd1, b2, D_FORCEASCII) == D_ERROR)
		fatal("import_get_rcsdiff: failed to get RCS patch");

	close(fd1);
	close(fd2);

	(void)unlink(p1);
	(void)unlink(p2);

	xfree(p1);
	xfree(p2);

	return (b2);
}
