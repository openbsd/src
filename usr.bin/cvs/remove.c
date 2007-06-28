/*	$OpenBSD: remove.c,v 1.65 2007/06/28 21:38:09 xsa Exp $	*/
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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

extern char *__progname;

void		cvs_remove_local(struct cvs_file *);
void		cvs_remove_force(struct cvs_file *);

static int	force_remove = 0;
static int	removed = 0;
static int	existing = 0;

struct cvs_cmd cvs_cmd_remove = {
	CVS_OP_REMOVE, 0, "remove",
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
	cr.flags = flags;

	cr.fileproc = cvs_remove_force;
	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_remove_local;
	}

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("remove");
		cvs_client_get_responses();
	} else {
		if (existing != 0) {
			cvs_log(LP_ERR, (existing == 1) ?
			    "%d file exists; remove it first" :
			    "%d files exist; remove them first", existing);
		}

		if (removed != 0) {
			if (verbosity > 0) {
				cvs_log(LP_NOTICE,
				    "use '%s commit' to remove %s "
				    "permanently", __progname, (removed > 1) ?
				    "these files" : "this file");
			}
		}
	}

	return (0);
}

void
cvs_remove_force(struct cvs_file *cf)
{
	if (cf->file_type != CVS_DIR) {
		if (cf->fd != -1 && force_remove == 1 && cvs_noexec == 0) {
			if (unlink(cf->file_path) == -1)
				fatal("cvs_remove_force: %s", strerror(errno));
			(void)close(cf->fd);
			cf->fd = -1;
		}
	}
}

void
cvs_remove_local(struct cvs_file *cf)
{
	CVSENTRIES *entlist;
	char *entry, buf[MAXPATHLEN], tbuf[CVS_TIME_BUFSZ], rbuf[CVS_REV_BUFSZ];

	cvs_log(LP_TRACE, "cvs_remove_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Removing %s", cf->file_path);
		return;
	}

	cvs_file_classify(cf, NULL);

	if (cf->file_status == FILE_UNKNOWN) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "nothing known about '%s'",
			    cf->file_path);
		return;
	}

	if (cf->fd != -1) {
		if (verbosity > 1)
			cvs_log(LP_ERR, "file `%s' still in working directory",
			    cf->file_name);
		existing++;
	} else {
		switch (cf->file_status) {
		case FILE_REMOVE_ENTRY:
			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_remove(entlist, cf->file_name);
			cvs_ent_close(entlist, ENT_SYNC);

			(void)xsnprintf(buf, sizeof(buf), "%s/%s/%s%s",
			    cf->file_path, CVS_PATH_CVSDIR, cf->file_name,
			    CVS_DESCR_FILE_EXT);

			(void)unlink(buf);

			if (verbosity > 1) {
				cvs_log(LP_NOTICE, "removed `%s'",
				    cf->file_name);
			}
			return;
		case FILE_REMOVED:
			if (verbosity > 0) {
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
			(void)xsnprintf(entry, CVS_ENT_MAXLINELEN,
			     "/%s/-%s/%s//", cf->file_name, rbuf, tbuf);

			if (cvs_server_active == 1) {
				cvs_server_update_entry("Checked-in", cf);
				cvs_remote_output(entry);
			} else {
				entlist = cvs_ent_open(cf->file_wd);
				cvs_ent_add(entlist, entry);
				cvs_ent_close(entlist, ENT_SYNC);
			}

			xfree(entry);

			if (verbosity > 0) {
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
