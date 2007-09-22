/*	$OpenBSD: release.c,v 1.35 2007/09/22 16:01:22 joris Exp $	*/
/*-
 * Copyright (c) 2005-2007 Xavier Santolaria <xsa@openbsd.org>
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

#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

extern char *__progname;

void	cvs_release_local(struct cvs_file *);

static void	release_check_files(struct cvs_file *);

static int	dflag = 0;
static int	files_altered = 0;

struct cvs_cmd cvs_cmd_release = {
	CVS_OP_RELEASE, 0, "release",
	{ "re", "rel" },
	"Indicate that a Module is no longer in use",
	"[-d] dir...",
	"d",
	NULL,
	cvs_release
};

int
cvs_release(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_REPO;

	while ((ch = getopt(argc, argv, cvs_cmd_release.cmd_opts)) != -1) {
		switch (ch) {
		case 'd':
			dflag = 1;
			break;
		default:
			fatal("%s", cvs_cmd_release.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_release.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (dflag == 1)
			cvs_client_send_request("Argument -d");
	} else
		cr.fileproc = cvs_release_local;

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("release");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_release_local(struct cvs_file *cf)
{
	struct stat st;
	struct cvs_recursion cr;
	char *wdir, cwd[MAXPATHLEN];
	char *arg = ".";
	int saved_noexec;

	if (cf->file_type == CVS_FILE)
		return;

	cvs_log(LP_TRACE, "cvs_release_local(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cvs_server_active == 1) {
		cvs_history_add(CVS_HISTORY_RELEASE, cf, NULL);
		return;
	}

	if ((wdir = getcwd(cwd, sizeof(cwd))) == NULL)
		fatal("getcwd failed");

	if (cf->file_type == CVS_DIR) {
		if (!strcmp(cf->file_name, "."))
			return;

		/* chdir before updating the directory. */
		cvs_chdir(cf->file_path, 0);

		if (stat(CVS_PATH_CVSDIR, &st) == -1 || !S_ISDIR(st.st_mode)) {
			cvs_log(LP_ERR, "no repository directory: %s",
			    cf->file_path);
			return;
		}
	}

	saved_noexec = cvs_noexec;
	cvs_noexec = 1;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.fileproc = cvs_update_local;
	cr.flags = CR_REPO | CR_RECURSE_DIRS;

	cvs_file_run(1, &arg, &cr);

	cvs_noexec = saved_noexec;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.fileproc = release_check_files;
	cr.flags = CR_RECURSE_DIRS;

	cvs_file_run(1, &arg, &cr);

	(void)printf("You have [%d] altered files in this repository.\n",
	    files_altered);
	(void)printf("Are you sure you want to release %sdirectory `%s': ",
		(dflag == 1) ? "(and delete) " : "", cf->file_path);

	if (cvs_yesno() == -1) {
		(void)fprintf(stderr,
		    "** `%s' aborted by user choice.\n", cvs_command);

		/* change back to original working dir */
		cvs_chdir(wdir, 0);

		return;
	}

	/* change back to original working dir */
	cvs_chdir(wdir, 0);

	if (dflag == 1) {
		if (cvs_rmdir(cf->file_path) != 0)
			fatal("cvs_release_local: cvs_rmdir failed");
	}
}

static void
release_check_files(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "release_check_files(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_status == FILE_MERGE ||
	    cf->file_status == FILE_ADDED ||
	    cf->file_status == FILE_PATCH ||
	    cf->file_status == FILE_CONFLICT)
		files_altered++;
	return;
}
