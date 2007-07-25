/*	$OpenBSD: checkout.c,v 1.99 2007/07/25 08:45:24 xsa Exp $	*/
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

#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

static void checkout_check_repository(int, char **);
static void checkout_repository(const char *, const char *);

extern int prune_dirs;
extern int build_dirs;

static int flags = CR_REPO | CR_RECURSE_DIRS;

struct cvs_cmd cvs_cmd_checkout = {
	CVS_OP_CHECKOUT, 0, "checkout",
	{ "co", "get" },
	"Checkout a working copy of a repository",
	"[-AcflNnPpRs] [-D date | -r tag] [-d dir] [-j rev] [-k mode] "
	"[-t id] module ...",
	"AcD:d:fj:k:lNnPpRr:st:",
	NULL,
	cvs_checkout
};

struct cvs_cmd cvs_cmd_export = {
	CVS_OP_EXPORT, 0, "export",
	{ "exp", "ex" },
	"Export sources from CVS, similar to checkout",
	"[-flNnR] [-d dir] [-k mode] -D date | -r rev module ...",
	"D:d:k:flNnRr:",
	NULL,
	cvs_export
};

int
cvs_checkout(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, cvs_cmd_checkout.cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			reset_stickies = 1;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'P':
			prune_dirs = 1;
			break;
		case 'R':
			break;
		case 'r':
			cvs_specified_tag = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_checkout.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_checkout.cmd_synopsis);

	checkout_check_repository(argc, argv);

	return (0);
}

int
cvs_export(int argc, char **argv)
{
	int ch;

	prune_dirs = 1;

	while ((ch = getopt(argc, argv, cvs_cmd_export.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		case 'r':
			cvs_specified_tag = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_export.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_export.cmd_synopsis);

	checkout_check_repository(argc, argv);

	return (0);
}

static void
checkout_check_repository(int argc, char **argv)
{
	int i;
	char repo[MAXPATHLEN];
	struct stat st;
	struct cvs_recursion cr;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();

		if (cvs_specified_tag != NULL)
			cvs_client_send_request("Argument -r%s",
			    cvs_specified_tag);
		if (reset_stickies == 1)
			cvs_client_send_request("Argument -A");

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (cvs_cmdop == CVS_OP_CHECKOUT && prune_dirs == 1)
			cvs_client_send_request("Argument -P");

		cr.enterdir = NULL;
		cr.leavedir = NULL;
		cr.fileproc = cvs_client_sendfile;
		cr.flags = flags;

		cvs_file_run(argc, argv, &cr);

		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");

		cvs_client_send_request("%s",
		    (cvs_cmdop == CVS_OP_CHECKOUT) ? "co" : "export");

		cvs_client_get_responses();

		return;
	}

	for (i = 0; i < argc; i++) {
		(void)xsnprintf(repo, sizeof(repo), "%s/%s",
		    current_cvsroot->cr_dir, argv[i]);

		if (stat(repo, &st) == -1) {
			cvs_log(LP_ERR, "cannot find module `%s' - ignored",
			    argv[i]);
			continue;
		}

		cvs_mkpath(argv[i], cvs_specified_tag);
		checkout_repository(repo, argv[i]);
	}
}

static void
checkout_repository(const char *repobase, const char *wdbase)
{
	struct cvs_flisthead fl, dl;
	struct cvs_recursion cr;

	TAILQ_INIT(&fl);
	TAILQ_INIT(&dl);

	cvs_history_add((cvs_cmdop == CVS_OP_CHECKOUT) ?
	    CVS_HISTORY_CHECKOUT : CVS_HISTORY_EXPORT, NULL, wdbase);

	build_dirs = 1;
	cr.enterdir = cvs_update_enterdir;
	cr.leavedir = cvs_update_leavedir;
	cr.fileproc = cvs_update_local;
	cr.flags = flags;

	cvs_repository_lock(repobase);
	cvs_repository_getdir(repobase, wdbase, &fl, &dl, 1);

	cvs_file_walklist(&fl, &cr);
	cvs_file_freelist(&fl);

	cvs_repository_unlock(repobase);

	cvs_file_walklist(&dl, &cr);
	cvs_file_freelist(&dl);
}

void
cvs_checkout_file(struct cvs_file *cf, RCSNUM *rnum, int co_flags)
{
	int kflag, oflags, exists;
	time_t rcstime;
	CVSENTRIES *ent;
	struct timeval tv[2];
	char *tosend;
	char template[MAXPATHLEN], entry[CVS_ENT_MAXLINELEN];
	char kbuf[8], stickytag[32], rev[CVS_REV_BUFSZ];
	char timebuf[CVS_TIME_BUFSZ], tbuf[CVS_TIME_BUFSZ];

	exists = 0;
	tosend = NULL;
	rcsnum_tostr(rnum, rev, sizeof(rev));

	cvs_log(LP_TRACE, "cvs_checkout_file(%s, %s, %d) -> %s",
	    cf->file_path, rev, co_flags,
	    (cvs_server_active) ? "to client" : "to disk");

	if (co_flags & CO_DUMP) {
		if (cvs_server_active) {
			cvs_printf("dump file %s to client\n", cf->file_path);
		} else {
			rcs_rev_write_fd(cf->file_rcs, rnum,
			    STDOUT_FILENO, 1);
		}

		return;
	}

	if (cvs_server_active == 0) {
		if (!(co_flags & CO_MERGE)) {
			oflags = O_WRONLY | O_TRUNC;
			if (cf->fd != -1) {
				exists = 1;
				(void)close(cf->fd);
			} else  {
				oflags |= O_CREAT;
			}

			cf->fd = open(cf->file_path, oflags);
			if (cf->fd == -1)
				fatal("cvs_checkout_file: open: %s",
				    strerror(errno));

			rcs_rev_write_fd(cf->file_rcs, rnum, cf->fd, 1);
		} else {
			cvs_merge_file(cf, 1);
		}

		if (fchmod(cf->fd, 0644) == -1)
			fatal("cvs_checkout_file: fchmod: %s", strerror(errno));

		if ((exists == 0) && (cf->file_ent == NULL) &&
		    !(co_flags & CO_MERGE))
			rcstime = rcs_rev_getdate(cf->file_rcs, rnum);
		else
			time(&rcstime);

		tv[0].tv_sec = rcstime;
		tv[0].tv_usec = 0;
		tv[1] = tv[0];
		if (futimes(cf->fd, tv) == -1)
			fatal("cvs_checkout_file: futimes: %s",
			    strerror(errno));
	} else {
		time(&rcstime);
	}

	asctime_r(gmtime(&rcstime), tbuf);
	if (tbuf[strlen(tbuf) - 1] == '\n')
		tbuf[strlen(tbuf) - 1] = '\0';

	if (co_flags & CO_MERGE) {
		(void)xsnprintf(timebuf, sizeof(timebuf), "Result of merge+%s",
		    tbuf);
	} else {
		strlcpy(timebuf, tbuf, sizeof(timebuf));
	}

	if (co_flags & CO_SETSTICKY)
		if (cvs_specified_tag != NULL)
			(void)xsnprintf(stickytag, sizeof(stickytag), "T%s",
			    cvs_specified_tag);
		else
			(void)xsnprintf(stickytag, sizeof(stickytag), "T%s",
			    rev);
	else
		stickytag[0] = '\0';

	kbuf[0] = '\0';
	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_opts != NULL)
			strlcpy(kbuf, cf->file_ent->ce_opts, sizeof(kbuf));
	} else if (cf->file_rcs->rf_expand != NULL) {
		kflag = rcs_kflag_get(cf->file_rcs->rf_expand);
		if (!(kflag & RCS_KWEXP_DEFAULT))
			(void)xsnprintf(kbuf, sizeof(kbuf),
			    "-k%s", cf->file_rcs->rf_expand);
	}

	(void)xsnprintf(entry, CVS_ENT_MAXLINELEN, "/%s/%s/%s/%s/%s",
	    cf->file_name, rev, timebuf, kbuf, stickytag);

	if (cvs_server_active == 0) {
		if (!(co_flags & CO_REMOVE)) {
			ent = cvs_ent_open(cf->file_wd);
			cvs_ent_add(ent, entry);
			cvs_ent_close(ent, ENT_SYNC);
		}
	} else {
		if (co_flags & CO_MERGE) {
			cvs_merge_file(cf, 1);
			tosend = cf->file_path;
		}

		if (co_flags & CO_COMMIT)
			cvs_server_update_entry("Checked-in", cf);
		else if (co_flags & CO_MERGE)
			cvs_server_update_entry("Merged", cf);
		else if (co_flags & CO_REMOVE)
			cvs_server_update_entry("Removed", cf);
		else
			cvs_server_update_entry("Updated", cf);

		if (!(co_flags & CO_REMOVE))
			cvs_remote_output(entry);

		if (!(co_flags & CO_COMMIT) && !(co_flags & CO_REMOVE)) {
			if (!(co_flags & CO_MERGE)) {
				(void)xsnprintf(template, MAXPATHLEN,
				    "%s/checkout.XXXXXXXXXX", cvs_tmpdir);

				rcs_rev_write_stmp(cf->file_rcs, rnum,
				    template, 0);
				tosend = template;
			}

			cvs_remote_send_file(tosend);

			if (!(co_flags & CO_MERGE)) {
				(void)unlink(template);
				cvs_worklist_run(&temp_files,
				    cvs_worklist_unlink);
			}
		}
	}
}
