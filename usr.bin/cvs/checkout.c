/*	$OpenBSD: checkout.c,v 1.71 2007/01/12 23:56:11 joris Exp $	*/
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
#include "log.h"
#include "diff.h"
#include "remote.h"

int	cvs_checkout(int, char **);
int	cvs_export(int, char **);
static void checkout_check_repository(int, char **);
static void checkout_repository(const char *, const char *);

extern int prune_dirs;
extern int build_dirs;

struct cvs_cmd cvs_cmd_checkout = {
	CVS_OP_CHECKOUT, 0, "checkout",
	{ "co", "get" },
	"Checkout a working copy of a repository",
	"[-AcflNnPpRs] [-D date | -r tag] [-d dir] [-j rev] [-k mode] "
	"[-t id] module ...",
	"AcD:d:fj:k:lNnPRr:st:",
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
		case 'P':
			prune_dirs = 1;
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
	int ch, flags;

	prune_dirs = 1;
	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_export.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
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

	for (i = 0; i < argc; i++) {
		cvs_mkpath(argv[i]);

		if (cvs_path_cat(current_cvsroot->cr_dir, argv[i], repo,
		    sizeof(repo)) >= sizeof(repo))
			fatal("checkout_check_repository: truncation");

		if (stat(repo, &st) == -1) {
			cvs_log(LP_ERR, "cannot find repository %s - ignored",
			    argv[i]);
			continue;
		}

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

	build_dirs = 1;
	cr.enterdir = cvs_update_enterdir;
	cr.leavedir = cvs_update_leavedir;
	cr.fileproc = cvs_update_local;
	cr.flags = CR_REPO | CR_RECURSE_DIRS;

	cvs_repository_lock(repobase);
	cvs_repository_getdir(repobase, wdbase, &fl, &dl, 1);

	cvs_file_walklist(&fl, &cr);
	cvs_file_freelist(&fl);

	cvs_repository_unlock(repobase);

	cvs_file_walklist(&dl, &cr);
	cvs_file_freelist(&dl);
}

void
cvs_checkout_file(struct cvs_file *cf, RCSNUM *rnum, BUF *bp, int flags)
{
	int l, oflags, exists;
	time_t rcstime;
	CVSENTRIES *ent;
	struct timeval tv[2];
	char *p, *entry, rev[16], timebuf[64], tbuf[32], stickytag[32];

	rcsnum_tostr(rnum, rev, sizeof(rev));

	cvs_log(LP_TRACE, "cvs_checkout_file(%s, %s, %d) -> %s",
	    cf->file_path, rev, flags,
	    (cvs_server_active) ? "to client" : "to disk");

	if (flags & CO_DUMP) {
		if (cvs_server_active) {
			cvs_printf("dump file %s to client\n", cf->file_path);
		} else {
			rcs_rev_write_fd(cf->file_rcs, rnum,
			    STDOUT_FILENO, 1);
		}

		return;
	}

	if (cvs_server_active == 0) {
		oflags = O_WRONLY | O_TRUNC;
		if (cf->fd != -1) {
			exists = 1;
			(void)close(cf->fd);
		} else  {
			exists = 0;
			oflags |= O_CREAT;
		}

		cf->fd = open(cf->file_path, oflags);
		if (cf->fd == -1)
			fatal("cvs_checkout_file: open: %s", strerror(errno));

		rcs_rev_write_fd(cf->file_rcs, rnum, cf->fd, 1);

		if (fchmod(cf->fd, 0644) == -1)
			fatal("cvs_checkout_file: fchmod: %s", strerror(errno));

		if (exists == 0) {
			rcstime = rcs_rev_getdate(cf->file_rcs, rnum);
			rcstime = cvs_hack_time(rcstime, 0);
		} else {
			time(&rcstime);
		}

		tv[0].tv_sec = rcstime;
		tv[0].tv_usec = 0;
		tv[1] = tv[0];
		if (futimes(cf->fd, tv) == -1)
			fatal("cvs_checkout_file: futimes: %s",
			    strerror(errno));
	} else {
		time(&rcstime);
	}

	rcstime = cvs_hack_time(rcstime, 1);

	ctime_r(&rcstime, tbuf);
	if (tbuf[strlen(tbuf) - 1] == '\n')
		tbuf[strlen(tbuf) - 1] = '\0';

	if (flags & CO_MERGE) {
		l = snprintf(timebuf, sizeof(timebuf), "Result of merge+%s",
		    tbuf);
		if (l == -1 || l >= (int)sizeof(timebuf))
			fatal("cvs_checkout_file: overflow");
	} else {
		strlcpy(timebuf, tbuf, sizeof(timebuf));
	}

	if (flags & CO_SETSTICKY) {
		l = snprintf(stickytag, sizeof(stickytag), "T%s", rev);
		if (l == -1 || l >= (int)sizeof(stickytag))
			fatal("cvs_checkout_file: overflow");
	} else {
		stickytag[0] = '\0';
	}

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	l = snprintf(entry, CVS_ENT_MAXLINELEN, "/%s/%s/%s//%s", cf->file_name,
	    rev, timebuf, stickytag);

	if (cvs_server_active == 0) {
		ent = cvs_ent_open(cf->file_wd);
		cvs_ent_add(ent, entry);
		cvs_ent_close(ent, ENT_SYNC);
	} else {
		if ((p = strrchr(cf->file_rpath, ',')) != NULL)
			*p = '\0';

		if (flags & CO_COMMIT)
			cvs_server_update_entry("Checked-in", cf);
		else
			cvs_server_update_entry("Updated", cf);

		cvs_remote_output(entry);

		if (!(flags & CO_COMMIT)) {
#if 0
			cvs_remote_output("u=rw,g=rw,o=rw");

			/* XXX */
			printf("%ld\n", cvs_buf_len(nbp));

			if (cvs_buf_write_fd(nbp, STDOUT_FILENO) == -1)
				fatal("cvs_checkout_file: failed to send file");
			cvs_buf_free(nbp);
#endif
		}

		if (p != NULL)
			*p = ',';
	}

	xfree(entry);
}
