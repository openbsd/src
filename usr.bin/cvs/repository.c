/*	$OpenBSD: repository.c,v 1.23 2010/07/23 08:31:19 ray Exp $	*/
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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"

struct wklhead repo_locks;

void
cvs_repository_unlock(const char *repo)
{
	char fpath[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_repository_unlock(%s)", repo);

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s", repo, CVS_LOCK);

	/* XXX - this ok? */
	worklist_run(&repo_locks, worklist_unlink);
}

void
cvs_repository_lock(const char *repo, int wantlock)
{
	int i;
	uid_t myuid;
	struct stat st;
	char fpath[MAXPATHLEN];
	struct passwd *pw;

	if (cvs_noexec == 1 || cvs_readonlyfs == 1)
		return;

	cvs_log(LP_TRACE, "cvs_repository_lock(%s, %d)", repo, wantlock);

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s", repo, CVS_LOCK);

	myuid = getuid();

	for (i = 0; i < CVS_LOCK_TRIES; i++) {
		if (cvs_quit)
			fatal("received signal %d", sig_received);

		if (stat(fpath, &st) == -1)
			break;

		if (st.st_uid == myuid)
			return;

		if ((pw = getpwuid(st.st_uid)) == NULL)
			fatal("cvs_repository_lock: %s", strerror(errno));

		cvs_log(LP_NOTICE, "waiting for %s's lock in '%s'",
		    pw->pw_name, repo);
		sleep(CVS_LOCK_SLEEP);
	}

	if (i == CVS_LOCK_TRIES)
		fatal("maximum wait time for lock inside '%s' reached", repo);

	if (wantlock == 0)
		return;

	if ((i = open(fpath, O_WRONLY|O_CREAT|O_TRUNC, 0755)) < 0) {
		if (errno == EEXIST)
			fatal("cvs_repository_lock: somebody beat us");
		else
			fatal("cvs_repository_lock: %s: %s",
			    fpath, strerror(errno));
	}

	(void)close(i);
	worklist_add(fpath, &repo_locks);
}

void
cvs_repository_getdir(const char *dir, const char *wdir,
	struct cvs_flisthead *fl, struct cvs_flisthead *dl, int flags)
{
	int type;
	DIR *dirp;
	struct stat st;
	struct dirent *dp;
	char *s, fpath[MAXPATHLEN], rpath[MAXPATHLEN];

	if ((dirp = opendir(dir)) == NULL)
		fatal("cvs_repository_getdir: failed to open '%s'", dir);

	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, "..") ||
		    !strcmp(dp->d_name, CVS_LOCK))
			continue;

		(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s", wdir, dp->d_name);
		(void)xsnprintf(rpath, MAXPATHLEN, "%s/%s", dir, dp->d_name);

		if (!TAILQ_EMPTY(&checkout_ign_pats)) {
			if ((s = strrchr(fpath, ',')) != NULL)
				*s = '\0';
			if (cvs_file_chkign(fpath))
				continue;
			if (s != NULL)
				*s = ',';
		}

		/*
		 * nfs and afs will show d_type as DT_UNKNOWN
		 * for files and/or directories so when we encounter
		 * this we call lstat() on the path to be sure.
		 */
		if (dp->d_type == DT_UNKNOWN) {
			if (lstat(rpath, &st) == -1)
				fatal("'%s': %s", rpath, strerror(errno));

			switch (st.st_mode & S_IFMT) {
			case S_IFDIR:
				type = CVS_DIR;
				break;
			case S_IFREG:
				type = CVS_FILE;
				break;
			default:
				fatal("Unknown file type in repository");
			}
		} else {
			switch (dp->d_type) {
			case DT_DIR:
				type = CVS_DIR;
				break;
			case DT_REG:
				type = CVS_FILE;
				break;
			default:
				fatal("Unknown file type in repository");
			}
		}

		if (!(flags & REPOSITORY_DODIRS) && type == CVS_DIR) {
			if (strcmp(dp->d_name, CVS_PATH_ATTIC))
				continue;
		}

		switch (type) {
		case CVS_DIR:
			if (!strcmp(dp->d_name, CVS_PATH_ATTIC)) {
				cvs_repository_getdir(rpath, wdir, fl, dl,
				    REPOSITORY_IS_ATTIC);
			} else {
				cvs_file_get(fpath, 0, dl, CVS_DIR);
			}
			break;
		case CVS_FILE:
			if ((s = strrchr(fpath, ',')) != NULL &&
			    s != fpath && !strcmp(s, RCS_FILE_EXT)) {
				*s = '\0';
				cvs_file_get(fpath, 
				    (flags & REPOSITORY_IS_ATTIC) ?
				    FILE_INSIDE_ATTIC : 0, fl, CVS_FILE);
			}
			break;
		default:
			fatal("type %d unknown, shouldn't happen", type);
		}
	}

	(void)closedir(dirp);
}
