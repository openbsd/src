/*	$OpenBSD: history.c,v 1.37 2008/06/10 16:32:35 tobias Exp $	*/
/*
 * Copyright (c) 2007 Joris Vink <joris@openbsd.org>
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

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

void	cvs_history_local(struct cvs_file *);

struct cvs_cmd		cvs_cmd_history = {
	CVS_OP_HISTORY, CVS_USE_WDIR, "history",
	{ "hi", "his" },			/* omghi2you */
	"Display history of actions done in the base repository",
	"[-ac]",
	"ac",
	NULL,
	cvs_history
};

/* keep in sync with the defines for history stuff in cvs.h */
const char historytab[] = {
	'T',
	'O',
	'E',
	'F',
	'W',
	'U',
	'G',
	'C',
	'M',
	'A',
	'R',
	'\0'
};

#define HISTORY_ALL_USERS		0x01
#define HISTORY_DISPLAY_ARCHIVED	0x02

void
cvs_history_add(int type, struct cvs_file *cf, const char *argument)
{
	BUF *buf;
	FILE *fp;
	char *cwd, *rev;
	char revbuf[CVS_REV_BUFSZ], repo[MAXPATHLEN], fpath[MAXPATHLEN];
	char timebuf[CVS_TIME_BUFSZ];
	struct tm datetm;

	if (cvs_nolog == 1)
		return;

	if (cvs_cmdop == CVS_OP_CHECKOUT || cvs_cmdop == CVS_OP_EXPORT) {
		if (type != CVS_HISTORY_CHECKOUT &&
		    type != CVS_HISTORY_EXPORT)
			return;
	}

	cvs_log(LP_TRACE, "cvs_history_add(`%c', `%s', `%s')",
	    historytab[type], (cf != NULL) ? cf->file_name : "", argument);

	if (cvs_server_active == 1) {
		cwd = "<remote>";
	} else {
		if ((cwd = getcwd(NULL, MAXPATHLEN)) == NULL)
			fatal("cvs_history_add: getcwd: %s", strerror(errno));
	}

	/* construct repository field */
	if (cvs_cmdop != CVS_OP_CHECKOUT && cvs_cmdop != CVS_OP_EXPORT) {
		cvs_get_repository_name((cf != NULL) ? cf->file_wd : ".",
		    repo, sizeof(repo));
	} else {
		strlcpy(repo, argument, sizeof(repo));
	}

	/* construct revision field */
	revbuf[0] = '\0';
	rev = revbuf;
	switch (type) {
	case CVS_HISTORY_TAG:
		strlcpy(revbuf, argument, sizeof(revbuf));
		break;
	case CVS_HISTORY_CHECKOUT:
	case CVS_HISTORY_EXPORT:
		/*
		 * cvs_buf_alloc uses xcalloc(), so we are safe even
		 * if neither cvs_specified_tag nor cvs_specified_date
		 * have been supplied.
		 */
		buf = cvs_buf_alloc(128);
		if (cvs_specified_tag != NULL) {
			cvs_buf_puts(buf, cvs_specified_tag);
			if (cvs_specified_date != -1)
				cvs_buf_putc(buf, ':');
		}
		if (cvs_specified_date != -1) {
			gmtime_r(&cvs_specified_date, &datetm);
			strftime(timebuf, sizeof(timebuf),
			    "%Y.%m.%d.%H.%M.%S", &datetm);
			cvs_buf_puts(buf, timebuf);
		}
		rev = cvs_buf_release(buf);
		break;
	case CVS_HISTORY_UPDATE_MERGED:
	case CVS_HISTORY_UPDATE_MERGED_ERR:
	case CVS_HISTORY_COMMIT_MODIFIED:
	case CVS_HISTORY_COMMIT_ADDED:
	case CVS_HISTORY_COMMIT_REMOVED:
	case CVS_HISTORY_UPDATE_CO:
		rcsnum_tostr(cf->file_rcs->rf_head,
			revbuf, sizeof(revbuf));
		break;
	}

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s",
	    current_cvsroot->cr_dir, CVS_PATH_HISTORY);

	if ((fp = fopen(fpath, "a")) != NULL) {
		fprintf(fp, "%c%x|%s|%s|%s|%s|%s\n",
		    historytab[type], time(NULL), getlogin(), cwd, repo,
		    rev, (cf != NULL) ? cf->file_name : argument);

		(void)fclose(fp);
	} else {
		cvs_log(LP_ERR, "failed to add entry to history file");
	}

	if (rev != revbuf)
		xfree(rev);
	if (cvs_server_active != 1)
		xfree(cwd);
}

int
cvs_history(int argc, char **argv)
{
	int ch, flags;

	flags = 0;

	while ((ch = getopt(argc, argv, cvs_cmd_history.cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			flags |= HISTORY_ALL_USERS;
			break;
		case 'c':
			flags |= HISTORY_DISPLAY_ARCHIVED;
			break;
		default:
			fatal("%s", cvs_cmd_history.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	return (0);
}
