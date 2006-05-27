/*	$OpenBSD: update.c,v 1.59 2006/05/27 03:30:31 joris Exp $	*/
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

int	cvs_update(int, char **);

struct cvs_cmd cvs_cmd_update = {
	CVS_OP_UPDATE, CVS_REQ_UPDATE, "update",
	{ "up", "upd" },
	"Bring work tree in sync with repository",
	"[-ACdflPpR] [-D date | -r rev] [-I ign] [-j rev] [-k mode] "
	"[-t id] ...",
	"ACD:dfI:j:k:lPpQqRr:t:",
	NULL,
	cvs_update
};

int
cvs_update(int argc, char **argv)
{
	int ch;
	char *arg = ".";
	struct cvs_recursion cr;

	while ((ch = getopt(argc, argv, cvs_cmd_update.cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			break;
		case 'C':
		case 'D':
			break;
		case 'd':
			break;
		case 'f':
			break;
		case 'I':
			break;
		case 'j':
			break;
		case 'k':
			break;
		case 'l':
			break;
		case 'P':
			break;
		case 'p':
			break;
		case 'Q':
		case 'q':
			break;
		case 'R':
			break;
		case 'r':
			break;
		default:
			fatal("%s", cvs_cmd_update.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = cvs_update_enterdir;
	cr.leavedir = NULL;
	cr.local = cvs_update_local;
	cr.remote = NULL;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	return (0);
}

void
cvs_update_enterdir(struct cvs_file *cf)
{
	int l;
	char *entry;
	CVSENTRIES *entlist;

	cvs_log(LP_TRACE, "cvs_update_enterdir(%s)", cf->file_path);

	cvs_file_classify(cf);

	if (cf->file_status == DIR_CREATE) {
		cvs_mkpath(cf->file_path);
		if ((cf->fd = open(cf->file_path, O_RDONLY)) == -1)
			fatal("cvs_update_enterdir: %s", strerror(errno));

		entry = xmalloc(CVS_ENT_MAXLINELEN);
		l = snprintf(entry, CVS_ENT_MAXLINELEN, "D/%s////",
		    cf->file_name);
		if (l == -1 || l >= CVS_ENT_MAXLINELEN)
			fatal("cvs_update_enterdir: overflow");

		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_add(entlist, entry);
		cvs_ent_close(entlist, ENT_SYNC);
		xfree(entry);
	}
}

void
cvs_update_local(struct cvs_file *cf)
{
	CVSENTRIES *entlist;

	cvs_log(LP_TRACE, "cvs_update_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (cf->file_status != FILE_UNKNOWN &&
		    verbosity > 1)
			cvs_log(LP_NOTICE, "Updating %s", cf->file_path);
		return;
	}

	cvs_file_classify(cf);

	switch (cf->file_status) {
	case FILE_UNKNOWN:
		cvs_printf("? %s\n", cf->file_path);
		break;
	case FILE_MODIFIED:
		if (cf->file_ent->ce_conflict != NULL)
			cvs_printf("C %s\n", cf->file_path);
		else
			cvs_printf("M %s\n", cf->file_path);
		break;
	case FILE_ADDED:
		cvs_printf("A %s\n", cf->file_path);
		break;
	case FILE_REMOVED:
		cvs_printf("R %s\n", cf->file_path);
		break;
	case FILE_CONFLICT:
		cvs_printf("C %s\n", cf->file_path);
		break;
	case FILE_LOST:
	case FILE_CHECKOUT:
	case FILE_PATCH:
		if (cvs_checkout_file(cf, cf->file_rcs->rf_head, 0))
			cvs_printf("U %s\n", cf->file_path);
		break;
	case FILE_MERGE:
		cvs_printf("needs merge: %s\n", cf->file_path);
		break;
	case FILE_UNLINK:
		(void)unlink(cf->file_path);
	case FILE_REMOVE_ENTRY:
		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_remove(entlist, cf->file_name);
		cvs_ent_close(entlist, ENT_SYNC);
		break;
	default:
		break;
	}
}
