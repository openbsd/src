/*	$OpenBSD: remove.c,v 1.51 2006/05/30 22:05:53 joris Exp $	*/
/*
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
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
#include "proto.h"

extern char *__progname;

int		cvs_remove(int, char **);
void		cvs_remove_local(struct cvs_file *);

static int	force_remove = 0;
static int	removed = 0;
static int	existing = 0;

struct cvs_cmd cvs_cmd_remove = {
	CVS_OP_REMOVE, CVS_REQ_REMOVE, "remove",
	{ "rm", "delete" },
	"Remove an entry from the repository",
	"[-flR] [file ...]",
	"flR",
	NULL,
	cvs_remove
};

int
cvs_remove(int argc, char **argv)
{
	int ch;
	int flags;
	char *arg = ".";
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;
	while ((ch = getopt(argc, argv, cvs_cmd_remove.cmd_opts)) != -1) {
		switch (ch) {
		case 'f':
			force_remove = 1;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		default:
			fatal("%s", cvs_cmd_remove.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.local = cvs_remove_local;
	cr.remote = NULL;
	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (existing != 0) {
		cvs_log(LP_ERR, "%d file(s) exist, remove them first",
		    existing);
	}

	if (removed != 0) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "use '%s commit' to remove %s "
			    "permanently", __progname, (removed > 1) ?
			    "these files" : "this file");
	}

	return (0);
}

void
cvs_remove_local(struct cvs_file *cf)
{
	int l;
	CVSENTRIES *entlist;
	char *entry, buf[MAXPATHLEN], tbuf[32], rbuf[16];

	cvs_log(LP_TRACE, "cvs_remove_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Removing %s", cf->file_path);
		return;
	}

	cvs_file_classify(cf, NULL, 0);

	if (cf->file_status == FILE_UNKNOWN) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "nothing known about '%s'",
			    cf->file_path);
		return;
	}

	if (force_remove == 1) {
		if (unlink(cf->file_path) == -1)
			fatal("cvs_remove_local: %s", strerror(errno));
		(void)close(cf->fd);
		cf->fd = -1;
	}

	if (cf->fd != -1) {
		if (verbosity > 1)
			cvs_log(LP_ERR, "file `%s' still in working directory",
			    cf->file_name);
		existing++;
	} else {
		switch(cf->file_status) {
		case FILE_UNKNOWN:
			if (verbosity > 1) {
				cvs_log(LP_ERR, "nothing known about `%s'",
				    cf->file_name);
			}
			return;
		case FILE_ADDED:
			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_remove(entlist, cf->file_name);
			cvs_ent_close(entlist, ENT_SYNC);

			l = snprintf(buf, sizeof(buf), "%s/%s/%s%s",
			    cf->file_path, CVS_PATH_CVSDIR, cf->file_name,
			    CVS_DESCR_FILE_EXT);
			if (l == -1 || l >= (int)sizeof(buf))
				fatal("cvs_remove_local: overflow");

			(void)unlink(buf);

			if (verbosity > 1) {
				cvs_log(LP_NOTICE, "removed `%s'",
				    cf->file_name);
			}
			return;
		case FILE_REMOVED:
			if (verbosity > 1 ) {
				cvs_log(LP_ERR,
				    "file `%s' already scheduled for removal",
				    cf->file_name);
			}
			return;
		default:
			rcsnum_tostr(cf->file_ent->ce_rev, rbuf,
			     sizeof(rbuf));

			ctime_r(&cf->file_ent->ce_mtime, tbuf);
			if (tbuf[strlen(tbuf) - 1] == '\n')
				tbuf[strlen(tbuf) - 1] = '\0';

			entry = xmalloc(CVS_ENT_MAXLINELEN);
			l = snprintf(entry, CVS_ENT_MAXLINELEN,
			     "/%s/-%s/%s//", cf->file_name, rbuf, tbuf);
			if (l == -1 || l >= CVS_ENT_MAXLINELEN)
				fatal("cvs_remove_local: overflow");

			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_add(entlist, entry);
			cvs_ent_close(entlist, ENT_SYNC);

			xfree(entry);

			if (verbosity > 1) {
				cvs_log(LP_NOTICE,
				    "scheduling file `%s' for removal",
				    cf->file_name);
			}

			cf->file_status = FILE_REMOVED;
			removed++;
			break;
		}
	}
}
