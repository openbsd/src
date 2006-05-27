/*	$OpenBSD: status.c,v 1.57 2006/05/27 03:30:31 joris Exp $	*/
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
#include "proto.h"

int	cvs_status(int, char **);
void	cvs_status_local(struct cvs_file *);

struct cvs_cmd cvs_cmd_status = {
	CVS_OP_STATUS, CVS_REQ_STATUS, "status",
	{ "st", "stat" },
	"Display status information on checked out files",
	"[-lRv]",
	"lRv:",
	NULL,
	cvs_status
};

#define CVS_STATUS_SEP	\
	"==================================================================="

const char *status_tab[] = {
	"Unknown",
	"Locally Added",
	"Locally Removed",
	"Locally Modified",
	"Up-to-date",
	"Needs Checkout",
	"Needs Checkout",
	"Needs Merge",
	"Needs Patch",
	"Entry Invalid",
	"Unresolved Conflict",
	"Classifying error",
};

int
cvs_status(int argc, char **argv)
{
	int ch;
	char *arg = ".";
	struct cvs_recursion cr;

	while ((ch = getopt(argc, argv, cvs_cmd_status.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			break;
		case 'R':
			break;
		case 'v':
			break;
		default:
			fatal("%s", cvs_cmd_status.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.local = cvs_status_local;
	cr.remote = NULL;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	return (0);
}

void
cvs_status_local(struct cvs_file *cf)
{
	int l;
	size_t len;
	const char *status;
	char buf[128], timebuf[32], revbuf[32];

	cvs_log(LP_TRACE, "cvs_status_local(%s)", cf->file_path);

	cvs_file_classify(cf);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Examining %s", cf->file_path);
		return;
	}

	cvs_printf("%s\n", CVS_STATUS_SEP);

	status = status_tab[cf->file_status];
	if (cf->file_status == FILE_MODIFIED &&
	    cf->file_ent->ce_conflict != NULL)
		status = "File had conflicts on merge";

	cvs_printf("File: %-17s\tStatus: %s\n\n", cf->file_name, status);

	if (cf->file_ent == NULL) {
		l = snprintf(buf, sizeof(buf),
		    "No entry for %s", cf->file_name);
		if (l == -1 || l >= (int)sizeof(buf))
			fatal("cvs_status_local: overflow");
	} else if (cf->file_status == FILE_ADDED) {
		len = strlcpy(buf, "New file!", sizeof(buf));
		if (len >= sizeof(buf))
			fatal("cvs_status_local: truncation");
	} else {
		rcsnum_tostr(cf->file_ent->ce_rev, revbuf, sizeof(revbuf));

		if (cf->file_ent->ce_conflict == NULL) {
			ctime_r(&(cf->file_ent->ce_mtime), timebuf);
			if (timebuf[strlen(timebuf) - 1] == '\n')
				timebuf[strlen(timebuf) - 1] = '\0';
		} else {
			len = strlcpy(timebuf, cf->file_ent->ce_conflict,
			    sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_status_local: truncation");
		}

		l = snprintf(buf, sizeof(buf), "%s\t%s", revbuf, timebuf);
		if (l == -1 || l >= (int)sizeof(buf))
			fatal("cvs_status_local: overflow");
	}

	cvs_printf("   Working revision:\t%s\n", buf);

	buf[0] = '\0';
	if (cf->file_rcs == NULL) {
		len = strlcat(buf, "No revision control file", sizeof(buf));
		if (len >= sizeof(buf))
			fatal("cvs_status_local: truncation");
	} else {
		rcsnum_tostr(cf->file_rcs->rf_head, revbuf, sizeof(revbuf));
		l = snprintf(buf, sizeof(buf), "%s\t%s", revbuf,
		    cf->file_rpath);
		if (l == -1 || l >= (int)sizeof(buf))
			fatal("cvs_status_local: overflow");
	}

	cvs_printf("   Repository revision:\t%s\n", buf);

	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_tag != NULL)
			cvs_printf("   Sticky Tag:\t%s\n",
			    cf->file_ent->ce_tag);
		if (cf->file_ent->ce_opts != NULL)
			cvs_printf("   Sticky Options:\t%s\n",
			    cf->file_ent->ce_opts);
	}

	cvs_printf("\n");
}
