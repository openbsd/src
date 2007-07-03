/*	$OpenBSD: history.c,v 1.32 2007/07/03 13:22:43 joris Exp $	*/
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
	CVS_OP_HISTORY, 0, "history",
	{ "hi", "his" },			/* omghi2you */
	"Display the history of actions done in the base repository",
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
	NULL
};

#define HISTORY_ALL_USERS		0x01
#define HISTORY_DISPLAY_ARCHIVED	0x02

void
cvs_history_add(int type, struct cvs_file *cf, const char *argument)
{
	FILE *fp;
	char *cwd;
	char revbuf[CVS_REV_BUFSZ], repo[MAXPATHLEN], fpath[MAXPATHLEN];

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
		cvs_get_repository_name(".", repo, sizeof(repo));
	} else {
		strlcpy(repo, argument, sizeof(repo));
	}

	/* construct revision field */
	revbuf[0] = '\0';
	if (cvs_cmdop != CVS_OP_CHECKOUT && cvs_cmdop != CVS_OP_EXPORT) {
		switch (type) {
		case CVS_HISTORY_TAG:
			strlcpy(revbuf, argument, sizeof(revbuf));
			break;
		case CVS_HISTORY_CHECKOUT:
		case CVS_HISTORY_EXPORT:
			/* copy TAG or DATE to revbuf */
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
	}

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s",
	    current_cvsroot->cr_dir, CVS_PATH_HISTORY);

	if ((fp = fopen(fpath, "a")) != NULL) {
		fprintf(fp, "%c%x|%s|%s|%s|%s|%s\n",
		    historytab[type], time(NULL), getlogin(), cwd, repo,
		    revbuf, (cf != NULL) ? cf->file_name : argument);

		(void)fclose(fp);
	} else {
		cvs_log(LP_ERR, "failed to add entry to history file");
	}

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
