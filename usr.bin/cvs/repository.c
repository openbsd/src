/*	$OpenBSD: repository.c,v 1.1 2006/05/27 03:30:31 joris Exp $	*/
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
#include "file.h"
#include "log.h"
#include "repository.h"
#include "worklist.h"

struct cvs_wklhead repo_locks;

void
cvs_repository_unlock(const char *repo)
{
	int l;
	char fpath[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_repository_unlock(%s)", repo);

	l = snprintf(fpath, sizeof(fpath), "%s/%s", repo, CVS_LOCK);
	if (l == -1 || l >= (int)sizeof(fpath))
		fatal("cvs_repository_unlock: overflow");

	/* XXX - this ok? */
	cvs_worklist_run(&repo_locks, cvs_worklist_unlink);
}

void
cvs_repository_lock(const char *repo)
{
	int l, i;
	uid_t myuid;
	struct stat st;
	char fpath[MAXPATHLEN];
	struct passwd *pw;

	cvs_log(LP_TRACE, "cvs_repository_lock(%s)", repo);

	l = snprintf(fpath, sizeof(fpath), "%s/%s", repo, CVS_LOCK);
	if (l == -1 || l >= (int)sizeof(fpath))
		fatal("cvs_repository_lock: overflow");

	myuid = getuid();
	for (i = 0; i < CVS_LOCK_TRIES; i++) {
		if (cvs_quit)
			fatal("received signal %d", sig_received);

		if (stat(fpath, &st) == -1)
			break;

		if ((pw = getpwuid(st.st_uid)) == NULL)
			fatal("cvs_repository_lock: %s", strerror(errno));

		cvs_log(LP_NOTICE, "waiting for %s's lock in '%s'",
		    pw->pw_name, repo);
		sleep(CVS_LOCK_SLEEP);
	}

	if (i == CVS_LOCK_TRIES)
		fatal("maximum wait time for lock inside '%s' reached", repo);

	if ((i = open(fpath, O_WRONLY|O_CREAT|O_TRUNC, 0755)) < 0) {
		if (errno == EEXIST)
			fatal("cvs_repository_lock: somebody beat us");
		else
			fatal("cvs_repostitory_lock: %s: %s",
			    fpath, strerror(errno));
	}

	(void)close(i);
	cvs_worklist_add(fpath, &repo_locks);
}

void
cvs_repository_getdir(const char *dir, const char *wdir,
	struct cvs_flisthead *fl, struct cvs_flisthead *dl)
{
	int l;
	DIR *dirp;
	struct dirent *dp;
	char *s, fpath[MAXPATHLEN];

	if ((dirp = opendir(dir)) == NULL)
		fatal("cvs_repository_getdir: failed to open '%s'", dir);

	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, "..") ||
		    !strcmp(dp->d_name, "Attic") ||
		    !strcmp(dp->d_name, CVS_LOCK))
			continue;

		if (cvs_file_chkign(dp->d_name))
			continue;

		l = snprintf(fpath, sizeof(fpath), "%s/%s", wdir, dp->d_name);
		if (l == -1 || l >= (int)sizeof(fpath))
			fatal("cvs_repository_getdir: overflow");

		/*
		 * Anticipate the file type for sorting, we do not determine
		 * the final file type until we have the fd floating around.
		 */
		if (dp->d_type == DT_DIR) {
			cvs_file_get(fpath, dl);
		} else if (dp->d_type == DT_REG) {
			if ((s = strrchr(fpath, ',')) != NULL)
				*s = '\0';
			cvs_file_get(fpath, fl);
		}
	}

	(void)closedir(dirp);
}
