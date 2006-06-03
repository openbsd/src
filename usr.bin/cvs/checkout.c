/*	$OpenBSD: checkout.c,v 1.60 2006/06/03 19:07:13 joris Exp $	*/
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
#include "proto.h"

int	cvs_checkout(int, char **);
static void checkout_repository(const char *, const char *);

extern int prune_dirs;
extern int build_dirs;

struct cvs_cmd cvs_cmd_checkout = {
	CVS_OP_CHECKOUT, CVS_REQ_CO, "checkout",
	{ "co", "get" },
	"Checkout a working copy of a repository",
	"[-AcflNnPpRs] [-D date | -r tag] [-d dir] [-j rev] [-k mode] "
	"[-t id] module ...",
	"AcD:d:fj:k:lNnPRr:st:",
	NULL,
	cvs_checkout
};

struct cvs_cmd cvs_cmd_export = {
	CVS_OP_EXPORT, CVS_REQ_EXPORT, "export",
	{ "exp", "ex" },
	"Export sources from CVS, similar to checkout",
	"module ...",
	"",
	NULL,
	cvs_checkout
};

int
cvs_checkout(int argc, char **argv)
{
	int i, ch, l;
	struct stat st;
	char repo[MAXPATHLEN];

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

	for (i = 0; i < argc; i++) {
		cvs_mkpath(argv[i]);

		l = snprintf(repo, sizeof(repo), "%s/%s",
		    current_cvsroot->cr_dir, argv[i]);
		if (l == -1 || l >= (int)sizeof(repo))
			fatal("cvs_checkout: overflow");

		if (stat(repo, &st) == -1) {
			cvs_log(LP_ERR, "cannot find repository %s - ignored",
			    argv[i]);
			continue;
		}

		checkout_repository(repo, argv[i]);
	}

	return (0);
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
	cr.local = cvs_update_local;
	cr.remote = NULL;
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
	BUF *nbp;
	int l, oflags, exists;
	time_t rcstime;
	CVSENTRIES *ent;
	struct timeval tv[2];
	char *entry, rev[16], timebuf[64], tbuf[32], stickytag[32];

	rcsnum_tostr(rnum, rev, sizeof(rev));

	cvs_log(LP_TRACE, "cvs_checkout_file(%s, %s, %d)",
	    cf->file_path, rev, flags);

	nbp = rcs_kwexp_buf(bp, cf->file_rcs, rnum);

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

	if (cvs_buf_write_fd(nbp, cf->fd) == -1)
		fatal("cvs_checkout_file: %s", strerror(errno));

	cvs_buf_free(nbp);

	if (fchmod(cf->fd, 0644) == -1)
		fatal("cvs_checkout_file: fchmod: %s", strerror(errno));

	if (exists == 0) {
		rcstime = rcs_rev_getdate(cf->file_rcs, rnum);
		if ((rcstime = cvs_hack_time(rcstime, 0)) == 0)
			fatal("cvs_checkout_file: time conversion failed");
	} else {
		time(&rcstime);
	}

	tv[0].tv_sec = rcstime;
	tv[0].tv_usec = 0;
	tv[1] = tv[0];
	if (futimes(cf->fd, tv) == -1)
		fatal("cvs_checkout_file: futimes: %s", strerror(errno));

	if ((rcstime = cvs_hack_time(rcstime, 1)) == 0)
		fatal("cvs_checkout_file: to gmt failed");

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

	ent = cvs_ent_open(cf->file_wd);
	cvs_ent_add(ent, entry);
	cvs_ent_close(ent, ENT_SYNC);

	xfree(entry);
}
