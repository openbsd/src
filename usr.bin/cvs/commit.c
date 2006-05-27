/*	$OpenBSD: commit.c,v 1.55 2006/05/27 03:30:30 joris Exp $	*/
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

int	cvs_commit(int, char **);
void	cvs_commit_local(struct cvs_file *);
void	cvs_commit_check_conflicts(struct cvs_file *);

static char *commit_diff_file(struct cvs_file *);

struct	cvs_flisthead files_affected;
int	conflicts_found;
char	*logmsg;

struct cvs_cmd cvs_cmd_commit = {
	CVS_OP_COMMIT, CVS_REQ_CI, "commit",
	{ "ci", "com" },
	"Check files into the repository",
	"[-flR] [-F logfile | -m msg] [-r rev] ...",
	"F:flm:Rr:",
	NULL,
	cvs_commit
};

int
cvs_commit(int argc, char **argv)
{
	int ch;
	char *arg = ".";
	struct cvs_recursion cr;

	while ((ch = getopt(argc, argv, cvs_cmd_commit.cmd_opts)) != -1) {
		switch (ch) {
		case 'f':
			break;
		case 'F':
			break;
		case 'l':
			break;
		case 'm':
			logmsg = xstrdup(optarg);
			break;
		case 'r':
			break;
		case 'R':
			break;
		default:
			fatal("%s", cvs_cmd_commit.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (logmsg == NULL)
		fatal("please use -m to specify a log message for now");

	TAILQ_INIT(&files_affected);
	conflicts_found = 0;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.local = cvs_commit_check_conflicts;
	cr.remote = NULL;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (conflicts_found != 0)
		fatal("%d conflicts found, please correct these first",
		    conflicts_found);

	cr.local = cvs_commit_local;
	cvs_file_walklist(&files_affected, &cr);
	cvs_file_freelist(&files_affected);

	return (0);
}

void
cvs_commit_check_conflicts(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "cvs_commit_check_conflicts(%s)", cf->file_path);

	/*
	 * cvs_file_classify makes the noise for us
	 * XXX - we want that?
	 */
	cvs_file_classify(cf);

	if (cf->file_status == FILE_CONFLICT ||
	    cf->file_status == FILE_LOST ||
	    cf->file_status == FILE_UNLINK)
		conflicts_found++;

	if (cf->file_status == FILE_ADDED ||
	    cf->file_status == FILE_REMOVED ||
	    cf->file_status == FILE_MODIFIED)
		cvs_file_get(cf->file_path, &files_affected);
}

void
cvs_commit_local(struct cvs_file *cf)
{
	BUF *b;
	char *d, *f, rbuf[16];

	cvs_log(LP_TRACE, "cvs_commit_local(%s)", cf->file_path);
	cvs_file_classify(cf);

	rcsnum_tostr(cf->file_rcs->rf_head, rbuf, sizeof(rbuf));

	cvs_printf("Checking in %s:\n", cf->file_path);
	cvs_printf("%s <- %s\n", cf->file_rpath, cf->file_path);
	cvs_printf("old revision: %s; ", rbuf);

	d = commit_diff_file(cf);

	if ((b = cvs_buf_load(cf->file_path, BUF_AUTOEXT)) == NULL)
		fatal("cvs_commit_local: failed to load file");

	cvs_buf_putc(b, '\0');
	f = cvs_buf_release(b);

	if (rcs_deltatext_set(cf->file_rcs, cf->file_rcs->rf_head, d) == -1)
		fatal("cvs_commit_local: failed to set delta");

	if (rcs_rev_add(cf->file_rcs, RCS_HEAD_REV, logmsg, -1, NULL) == -1)
		fatal("cvs_commit_local: failed to add new revision");

	if (rcs_deltatext_set(cf->file_rcs, cf->file_rcs->rf_head, f) == -1)
		fatal("cvs_commit_local: failed to set new HEAD delta");

	xfree(f);
	xfree(d);

	rcs_write(cf->file_rcs);

	rcsnum_tostr(cf->file_rcs->rf_head, rbuf, sizeof(rbuf));
	cvs_printf("new revision: %s\n", rbuf);

	(void)unlink(cf->file_path);
	(void)close(cf->fd);
	cf->fd = -1;
	cvs_checkout_file(cf, cf->file_rcs->rf_head, 0);

	cvs_printf("done\n");

}

static char *
commit_diff_file(struct cvs_file *cf)
{
	char*delta,  *p1, *p2;
	BUF *b1, *b2, *b3;

	if ((b1 = cvs_buf_load(cf->file_path, BUF_AUTOEXT)) == NULL)
		fatal("commit_diff_file: failed to load '%s'", cf->file_path);

	if ((b2 = rcs_getrev(cf->file_rcs, cf->file_rcs->rf_head)) == NULL)
		fatal("commit_diff_file: failed to load HEAD for '%s'",
		    cf->file_path);

	if ((b3 = cvs_buf_alloc(128, BUF_AUTOEXT)) == NULL)
		fatal("commit_diff_file: failed to create diff buf");

	(void)xasprintf(&p1, "%s/diff1.XXXXXXXXXX", cvs_tmpdir);
	cvs_buf_write_stmp(b1, p1, 0600, NULL);
	cvs_buf_free(b1);

	(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);
	cvs_buf_write_stmp(b2, p2, 0600, NULL);
	cvs_buf_free(b2);

	diff_format = D_RCSDIFF;
	if (cvs_diffreg(p1, p2, b3) == D_ERROR)
		fatal("commit_diff_file: failed to get RCS patch");

	cvs_buf_putc(b3, '\0');
	delta = cvs_buf_release(b3);
	return (delta);
}
