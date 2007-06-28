/*	$OpenBSD: status.c,v 1.75 2007/06/28 21:38:09 xsa Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

void	cvs_status_local(struct cvs_file *);

static int show_sym = 0;

struct cvs_cmd cvs_cmd_status = {
	CVS_OP_STATUS, 0, "status",
	{ "st", "stat" },
	"Display status information on checked out files",
	"[-lRv]",
	"lRv",
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
	int ch, flags;
	char *arg = ".";
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_status.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		case 'v':
			show_sym = 1;
			break;
		default:
			fatal("%s", cvs_cmd_status.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		flags |= CR_REPO;
		cr.fileproc = cvs_status_local;
	} else {
		cvs_client_connect_to_server();
		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
		if (show_sym)
			cvs_client_send_request("Argument -v");
		cr.fileproc = cvs_client_sendfile;
	}

	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("status");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_status_local(struct cvs_file *cf)
{
	size_t len;
	RCSNUM *head;
	const char *status;
	char buf[128], timebuf[CVS_TIME_BUFSZ], revbuf[CVS_REV_BUFSZ];
	struct rcs_sym *sym;

	cvs_log(LP_TRACE, "cvs_status_local(%s)", cf->file_path);

	cvs_file_classify(cf, NULL);

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

	if (cf->file_status == FILE_LOST ||
	    cf->file_status == FILE_UNKNOWN ||
	    (cf->file_rcs != NULL && cf->in_attic == 1)) {
		(void)xsnprintf(buf, sizeof(buf), "no file %s\t",
		    cf->file_name);
	} else
		if (strlcpy(buf, cf->file_name, sizeof(buf)) >= sizeof(buf))
			fatal("cvs_status_local: overflow");

	cvs_printf("File: %-17s\tStatus: %s\n\n", buf, status);

	if (cf->file_ent == NULL) {
		(void)xsnprintf(buf, sizeof(buf),
		    "No entry for %s", cf->file_name);
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

		(void)xsnprintf(buf, sizeof(buf), "%s\t%s", revbuf, timebuf);
	}

	cvs_printf("   Working revision:\t%s\n", buf);

	buf[0] = '\0';
	if (cf->file_rcs == NULL) {
		len = strlcat(buf, "No revision control file", sizeof(buf));
		if (len >= sizeof(buf))
			fatal("cvs_status_local: truncation");
	} else {
		head = rcs_head_get(cf->file_rcs);
		rcsnum_tostr(head, revbuf, sizeof(revbuf));
		rcsnum_free(head);
		(void)xsnprintf(buf, sizeof(buf), "%s\t%s", revbuf,
		    cf->file_rpath);
	}

	cvs_printf("   Repository revision:\t%s\n", buf);

	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_tag != NULL)
			cvs_printf("   Sticky Tag:\t\t%s\n",
			    cf->file_ent->ce_tag);
		else if (verbosity > 0)
			cvs_printf("   Sticky Tag:\t\t(none)\n");

		if (cf->file_ent->ce_opts != NULL)
			cvs_printf("   Sticky Options:\t%s\n",
			    cf->file_ent->ce_opts);
		else if (verbosity > 0)
			cvs_printf("   Sticky Options:\t(none)\n");
	}

	if (show_sym == 1) {
		cvs_printf("\n");
		cvs_printf("   Existing Tags:\n");

		if (!TAILQ_EMPTY(&(cf->file_rcs->rf_symbols))) {
			TAILQ_FOREACH(sym,
			    &(cf->file_rcs->rf_symbols), rs_list) {
				(void)rcsnum_tostr(sym->rs_num, revbuf,
				    sizeof(revbuf));

				cvs_printf("\t%-25s\t(%s: %s)\n", sym->rs_name,
				    RCSNUM_ISBRANCH(sym->rs_num) ? "branch" :
				    "revision", revbuf);
			 }
		} else
			cvs_printf("\tNo Tags Exist\n");
	}

	cvs_printf("\n");
}
