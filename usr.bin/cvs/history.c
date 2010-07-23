/*	$OpenBSD: history.c,v 1.40 2010/07/23 21:46:05 ray Exp $	*/
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
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

void	cvs_history_local(struct cvs_file *);

static void	history_compress(char *, const char *);

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
	RCSNUM *hrev;
	size_t len;
	int fd;
	char *cwd, *p, *rev;
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

	/* construct repository field */
	if (cvs_cmdop != CVS_OP_CHECKOUT && cvs_cmdop != CVS_OP_EXPORT) {
		cvs_get_repository_name((cf != NULL) ? cf->file_wd : ".",
		    repo, sizeof(repo));
	} else {
		cvs_get_repository_name(argument, repo, sizeof(repo));
	}

	if (cvs_server_active == 1) {
		cwd = "<remote>";
	} else {
		if (getcwd(fpath, sizeof(fpath)) == NULL)
			fatal("cvs_history_add: getcwd: %s", strerror(errno));
		p = fpath;
		if (cvs_cmdop == CVS_OP_CHECKOUT ||
		    cvs_cmdop == CVS_OP_EXPORT) {
			if (strlcat(fpath, "/", sizeof(fpath)) >=
			    sizeof(fpath) || strlcat(fpath, argument,
			    sizeof(fpath)) >= sizeof(fpath))
				fatal("cvs_history_add: string truncation");
		}
		if (cvs_homedir != NULL && cvs_homedir[0] != '\0') {
			len = strlen(cvs_homedir);
			if (strncmp(cvs_homedir, fpath, len) == 0 &&
			    fpath[len] == '/') {
				p += len - 1;
				*p = '~';
			}
		}

		history_compress(p, repo);
		cwd = xstrdup(p);
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
		 * buf_alloc uses xcalloc(), so we are safe even
		 * if neither cvs_specified_tag nor cvs_specified_date
		 * have been supplied.
		 */
		buf = buf_alloc(128);
		if (cvs_specified_tag != NULL) {
			buf_puts(buf, cvs_specified_tag);
			if (cvs_specified_date != -1)
				buf_putc(buf, ':');
		}
		if (cvs_specified_date != -1) {
			gmtime_r(&cvs_specified_date, &datetm);
			strftime(timebuf, sizeof(timebuf),
			    "%Y.%m.%d.%H.%M.%S", &datetm);
			buf_puts(buf, timebuf);
		}
		rev = buf_release(buf);
		break;
	case CVS_HISTORY_UPDATE_MERGED:
	case CVS_HISTORY_UPDATE_MERGED_ERR:
	case CVS_HISTORY_COMMIT_MODIFIED:
	case CVS_HISTORY_COMMIT_ADDED:
	case CVS_HISTORY_COMMIT_REMOVED:
	case CVS_HISTORY_UPDATE_CO:
		if ((hrev = rcs_head_get(cf->file_rcs)) == NULL)
			fatal("cvs_history_add: rcs_head_get failed");
		rcsnum_tostr(hrev, revbuf, sizeof(revbuf));
		rcsnum_free(hrev);
		break;
	}

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s",
	    current_cvsroot->cr_dir, CVS_PATH_HISTORY);

	if ((fd = open(fpath, O_WRONLY|O_APPEND)) == -1) {
		if (errno != ENOENT)
			cvs_log(LP_ERR, "failed to open history file");
	} else {
		if ((fp = fdopen(fd, "a")) != NULL) {
			fprintf(fp, "%c%x|%s|%s|%s|%s|%s\n",
			    historytab[type], time(NULL), getlogin(), cwd,
			    repo, rev, (cf != NULL) ? cf->file_name :
			    argument);
			(void)fclose(fp);
		} else {
			cvs_log(LP_ERR, "failed to add entry to history file");
			(void)close(fd);
		}
	}

	if (rev != revbuf)
		xfree(rev);
	if (cvs_server_active != 1)
		xfree(cwd);
}

static void
history_compress(char *wdir, const char *repo)
{
	char *p;
	const char *q;
	size_t repo_len, wdir_len;

	repo_len = strlen(repo);
	wdir_len = strlen(wdir);

	p = wdir + wdir_len;
	q = repo + repo_len;

	while (p >= wdir && q >= repo) {
		if (*p != *q)
			break;
		p--;
		q--;
	}
	p++;
	q++;

	/* if it's not worth the effort, skip compression */
	if (repo + repo_len - q < 3)
		return;

	(void)xsnprintf(p, strlen(p) + 1, "*%zx", q - repo);
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
