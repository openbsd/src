/*	$OpenBSD: repo.c,v 1.1 2005/02/16 15:41:15 jfb Exp $	*/
/*
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <libgen.h>

#include "log.h"
#include "repo.h"
#include "cvsd.h"



static CVSRPENT*  cvs_repo_loadrec (CVSREPO *, const char *);


/*
 * cvs_repo_load()
 *
 * Load the information for a specific CVS repository whose base directory
 * is specified in <base>.
 */

CVSREPO*
cvs_repo_load(const char *base, int flags)
{
	struct stat st;
	CVSREPO *repo;

	cvs_log(LP_DEBUG, "loading repository %s", base);

	if (stat(base, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat %s", base);
		return (NULL);
	}

	if (!S_ISDIR(st.st_mode)) {
		cvs_log(LP_ERR, "%s: repository path is not a directory", base);
		return (NULL);
	}

	repo = (struct cvs_repo *)malloc(sizeof(*repo));
	if (repo == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate repository data");
		return (NULL);
	}
	memset(repo, 0, sizeof(*repo));

	TAILQ_INIT(&(repo->cr_modules));

	repo->cr_path = strdup(base);
	if (repo->cr_path == NULL) {
		cvs_log(LP_ERRNO, "failed to copy repository path");
		free(repo);
		return (NULL);
	}

	repo->cr_tree = cvs_repo_loadrec(repo, repo->cr_path);
	if (repo->cr_tree == NULL) {
		cvs_repo_free(repo);
		return (NULL);
	}

	return (repo);
}


/*
 * cvs_repo_free()
 *
 * Free the data associated to a repository.
 */

void
cvs_repo_free(CVSREPO *repo)
{
	CVSMODULE *mod;

	if (repo != NULL) {
		if (repo->cr_path != NULL)
			free(repo->cr_path);

		while ((mod = TAILQ_FIRST(&(repo->cr_modules))) != NULL) {
			TAILQ_REMOVE(&(repo->cr_modules), mod, cm_link);
			cvs_repo_modfree(mod);
		}

		if (repo->cr_tree != NULL)
			cvs_repo_entfree(repo->cr_tree);

		free(repo);
	}
}


/*
 * cvs_repo_lockdir()
 *
 * Obtain a lock on the directory <dir> which is relative to the root of
 * the repository <repo>.  The owner of the lock becomes <pid>.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_repo_lockdir(CVSREPO *repo, const char *dir, int type, pid_t owner)
{
	CVSRPENT *ent;

	if ((ent = cvs_repo_find(repo, dir)) == NULL) {
		return (-1);
	}

	return cvs_repo_lockent(ent, type, owner);
}


/*
 * cvs_repo_unlockdir()
 *
 * Attempt to unlock the directory <dir> in the repository <repo>.  The <owner>
 * argument is used to make sure that the caller really owns the lock it is
 * trying to release.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_repo_unlockdir(CVSREPO *repo, const char *dir, pid_t owner)
{
	CVSRPENT *ent;

	if ((ent = cvs_repo_find(repo, dir)) == NULL) {
		return (-1);
	}

	return cvs_repo_unlockent(ent, owner);
}


/*
 * cvs_repo_lockent()
 *
 * Obtain a lock on the entry <ent>.  The owner of the lock becomes <pid>.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_repo_lockent(CVSRPENT *ent, int type, pid_t owner)
{
	struct cvs_lock *lk;
	struct cvs_lklist *list;

	if ((type != CVS_LOCK_READ) && (type != CVS_LOCK_WRITE)) {
		cvs_log(LP_ERR, "invalid lock type (%d) requested");
		return (-1);
	}

	lk = (struct cvs_lock *)malloc(sizeof(*lk));
	if (lk == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate repository lock");
		return (-1);
	}
	lk->lk_owner = owner;
	lk->lk_type = type;
	lk->lk_ent = ent;

	if ((ent->cr_wlock != NULL) && (ent->cr_wlock->lk_owner != 0)) {
		/*
		 * Another process has already locked the entry with a write
		 * lock, so regardless of the type of lock we are requesting,
		 * we'll have to wait in the pending requests queue.
		 */ 
		if (ent->cr_wlock->lk_owner == owner) {
			cvs_log(LP_WARN, "double-lock attempt");
			free(lk);
		} else
			TAILQ_INSERT_TAIL(&(ent->cr_lkreq), lk, lk_link);
	} else {
		if (type == CVS_LOCK_READ) {
			/*
			 * If there are any pending write lock requests,
			 * add the read lock request at the tail of the queue
			 * instead of assigning it right away.  Otherwise,
			 * we could end up with a write lock request never
			 * being obtained if other processes make overlapping
			 * read lock requests.
			 */
			if (TAILQ_EMPTY(&(ent->cr_lkreq)))
				list = &(ent->cr_rlocks);
			else
				list = &(ent->cr_lkreq);
			TAILQ_INSERT_TAIL(list, lk, lk_link);
		} else if (type == CVS_LOCK_WRITE) {
			if (TAILQ_EMPTY(&(ent->cr_rlocks)))
				ent->cr_wlock = lk;
			else
				TAILQ_INSERT_TAIL(&(ent->cr_lkreq), lk, lk_link);
		}
	}

	return (0);
}


/*
 * cvs_repo_unlockent()
 *
 * Attempt to unlock the entry <ent>.  The <owner> argument is used to make
 * sure that the caller really owns the lock it is trying to release.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_repo_unlockent(CVSRPENT *ent, pid_t owner)
{
	struct cvs_lock *lk;

	if ((ent->cr_wlock != NULL) && (ent->cr_wlock->lk_owner != 0)) {
		if (ent->cr_wlock->lk_owner != owner) {
			cvs_log(LP_ERR, "child %d attempted to unlock write "
			    "lock owned by %d", ent->cr_wlock->lk_owner);
			return (-1);
		}

		free(ent->cr_wlock);
		ent->cr_wlock = NULL;
	} else {
		TAILQ_FOREACH(lk, &(ent->cr_rlocks), lk_link) {
			if (lk->lk_owner == owner) {
				TAILQ_REMOVE(&(ent->cr_rlocks), lk, lk_link);
				free(lk);
				break;
			}
		}
	}

#ifdef notyet
	/* assign lock to any process with a pending request */
	while ((lk = TAILQ_FIRST(&(ent->cr_lkreq))) != NULL) {
		TAILQ_REMOVE(&(ent->cr_lkreq), lk, lk_link);
		/* XXX send message to process */
		child = cvsd_child_find(lk->lk_owner);
		if (child == NULL)
			continue;

		break;
	}
#endif

	return (0);
}


/*
 * cvs_repo_alias()
 *
 * Add a new module entry with name <alias> in the repository <repo>, which
 * points to the path <path> within the repository. 
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_repo_alias(CVSREPO *repo, const char *path, const char *alias)
{
	CVSMODULE *mod;

	mod = (CVSMODULE *)malloc(sizeof(*mod));
	if (mod == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate module alias");
		return (-1);
	}
	memset(mod, 0, sizeof(*mod));

	mod->cm_name = strdup(alias);
	if (mod->cm_name == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate module alias");
		free(mod);
		return (-1);
	}
	mod->cm_flags |= CVS_MODULE_ISALIAS;

	mod->cm_path = strdup(path);
	if (mod->cm_path == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate module alias");
		free(mod->cm_name);
		free(mod);
		return (-1);
	}

	TAILQ_INSERT_TAIL(&(repo->cr_modules), mod, cm_link);

	return (0);
}


/*
 * cvs_repo_unalias()
 *
 * Remove the module alias <alias> from the repository <repo>.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_repo_unalias(CVSREPO *repo, const char *alias)
{
	CVSMODULE *mod;

	TAILQ_FOREACH(mod, &(repo->cr_modules), cm_link) {
		if (strcmp(mod->cm_name, alias) == 0) {
			if (!(mod->cm_flags & CVS_MODULE_ISALIAS)) {
				cvs_log(LP_ERR,
				    "attempt to remove non-aliased module `%s'",
				    mod->cm_name);
				return (-1);
			}

			break;
		}
	}
	if (mod == NULL)
		return (-1);

	TAILQ_REMOVE(&(repo->cr_modules), mod, cm_link);
	return (0);
}


/*
 * cvs_repo_find()
 *
 * Find the pointer to a CVS file entry within the file hierarchy <hier>.
 * The file's pathname <path> must be relative to the base of <hier>.
 * Returns the entry on success, or NULL on failure.
 */
CVSRPENT*
cvs_repo_find(CVSREPO *repo, const char *path)
{
	size_t len;
	char *pp, *sp, pbuf[MAXPATHLEN];
	CVSRPENT *sf, *cf;

	if ((len = strlcpy(pbuf, path, sizeof(pbuf))) >= sizeof(pbuf)) {
		cvs_log(LP_ERR, "path %s too long", path);
		return (NULL);
	}

	/* remove any trailing slashes */
	while ((len > 0) && (pbuf[len - 1] == '/'))
		pbuf[--len] = '\0';

	cf = repo->cr_tree;
	pp = pbuf;
	do {
		if (cf->cr_type != CVS_RPENT_DIR) {
			cvs_log(LP_ERR,
			    "part of the path %s is not a directory", path);
			return (NULL);
		}
		sp = strchr(pp, '/');
		if (sp != NULL)
			*(sp++) = '\0';

		/* special case */
		if (*pp == '.') {
			if ((*(pp + 1) == '.') && (*(pp + 2) == '\0')) {
				/* request to go back to parent */
				if (cf->cr_parent == NULL) {
					cvs_log(LP_NOTICE,
					    "path %s goes back too far", path);
					return (NULL);
				}
				cf = cf->cr_parent;
				continue;
			} else if (*(pp + 1) == '\0')
				continue;
		}

		TAILQ_FOREACH(sf, &(cf->cr_files), cr_link) {
			if (strcmp(pp, sf->cr_name) == 0)
				break;
		}
		if (sf == NULL)
			return (NULL);

		cf = sf;
		pp = sp;
	} while (sp != NULL);

	return (cf);
}


#if 0
/*
 * cvs_repo_getpath()
 *
 * Get the full path of the file <file> and store it in <buf>, which is of
 * size <len>.  For portability, it is recommended that <buf> always be
 * at least MAXPATHLEN bytes long.
 * Returns a pointer to the start of the path on success, or NULL on failure.
 */
char*
cvs_repo_getpath(CVSRPENT *file, char *buf, size_t len)
{
	u_int i;
	char *fp, *namevec[CVS_FILE_MAXDEPTH];
	CVSRPENT *top;

	buf[0] = '\0';
	i = CVS_FILE_MAXDEPTH;
	memset(namevec, 0, sizeof(namevec));

	/* find the top node */
	for (top = file; (top != NULL) && (i > 0); top = top->cr_parent) {
		fp = top->cr_name;

		/* skip self-references */
		if ((fp[0] == '.') && (fp[1] == '\0'))
			continue;
		namevec[--i] = fp;
	}

	if (i == 0)
		return (NULL);
	else if (i == CVS_FILE_MAXDEPTH) {
		strlcpy(buf, ".", len);
		return (buf);
	}

	while (i < CVS_FILE_MAXDEPTH - 1) {
		strlcat(buf, namevec[i++], len);
		strlcat(buf, "/", len);
	}
	strlcat(buf, namevec[i], len);

	return (buf);
}
#endif


/*
 * cvs_repo_loadrec()
 *
 * Recursively load the repository structure
 */
static CVSRPENT*
cvs_repo_loadrec(CVSREPO *repo, const char *path)
{
	int ret, fd;
	long base;
	u_char *dp, *ep;
	mode_t fmode;
	char fbuf[2048], pbuf[MAXPATHLEN];
	struct dirent *ent;
	CVSRPENT *cfp, *cr_ent;
	struct stat st;

	cvs_log(LP_NOTICE, "loading %s", path);
	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat %s", path);
		return (NULL);
	}

	cfp = (CVSRPENT *)malloc(sizeof(*cfp));
	if (cfp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate repository entry");
		return (NULL);
	}
	memset(cfp, 0, sizeof(*cfp));
	TAILQ_INIT(&(cfp->cr_rlocks));
	TAILQ_INIT(&(cfp->cr_lkreq));

	cfp->cr_name = strdup(basename(path));
	if (cfp->cr_name == NULL) {
		cvs_log(LP_ERRNO, "failed to copy entry name");
		free(cfp);
		return (NULL);
	}

	if (repo->cr_flags & CVS_REPO_CHKPERM) {
		if (S_ISDIR(st.st_mode))
			fmode = CVSD_DPERM;
		else
			fmode = CVSD_FPERM;
		/* perform permission checks on the file */
		if (st.st_uid != cvsd_uid) {
			cvs_log(LP_WARN, "owner of `%s' is not %s",
			    path, CVSD_USER);
		}

		if (st.st_gid != cvsd_gid) {
			cvs_log(LP_WARN, "group of `%s' is not %s",
			    path, CVSD_GROUP);
		}

		if (st.st_mode & S_IWGRP) {
			cvs_log(LP_WARN, "file `%s' is group-writable",
			    path, fmode);
		}

		if (st.st_mode & S_IWOTH) {
			cvs_log(LP_WARN, "file `%s' is world-writable",
			    path, fmode);
		}
	}

	if (S_ISREG(st.st_mode))
		cfp->cr_type = CVS_RPENT_RCSFILE;
	else if (S_ISDIR(st.st_mode)) {
		cfp->cr_type = CVS_RPENT_DIR;

		TAILQ_INIT(&(cfp->cr_files));

		if ((fd = open(path, O_RDONLY)) == -1) {
			cvs_log(LP_ERRNO, "failed to open `%s'", path);
			cvs_repo_entfree(cfp);
			return (NULL);
		}

		do {
			ret = getdirentries(fd, fbuf, sizeof(fbuf), &base);
			if (ret == -1) {
				cvs_log(LP_ERRNO,
				    "failed to get directory entries");
				cvs_repo_entfree(cfp);
				(void)close(fd);
				return (NULL);
			}

			dp = fbuf;
			ep = fbuf + (size_t)ret;
			while (dp < ep) {
				ent = (struct dirent *)dp;
				dp += ent->d_reclen;
				if (ent->d_fileno == 0)
					continue;

				if (((ent->d_namlen == 1) &&
				    (ent->d_name[0] == '.')) ||
				    ((ent->d_namlen == 2) &&
				    (ent->d_name[0] == '.') &&
				    (ent->d_name[1] == '.')))
					continue;

				snprintf(pbuf, sizeof(pbuf), "%s/%s", path,
				    ent->d_name);

				if ((ent->d_type != DT_DIR) &&
				    (ent->d_type != DT_REG)) {
					cvs_log(LP_NOTICE, "skipping non-"
					    "regular file `%s'", pbuf);
					continue;
				}

				cr_ent = cvs_repo_loadrec(repo, pbuf);
				if (cr_ent == NULL) {
					cvs_repo_entfree(cfp);
					(void)close(fd);
					return (NULL);
				}

				cr_ent->cr_parent = cfp;
				TAILQ_INSERT_TAIL(&(cfp->cr_files), cr_ent, cr_link);
			}
		} while (ret > 0);

		(void)close(fd);
	}

	return (cfp);
}


/*
 * cvs_repo_entfree()
 *
 * Free a repository entry structure and all underlying data.  In the case of
 * directories, any child entries are also freed recursively.
 */
void
cvs_repo_entfree(CVSRPENT *ent)
{
	CVSRPENT *ch_ent;

	if (ent->cr_type == CVS_RPENT_DIR) {
		while ((ch_ent = TAILQ_FIRST(&(ent->cr_files))) != NULL) {
			TAILQ_REMOVE(&(ent->cr_files), ch_ent, cr_link);
			cvs_repo_entfree(ch_ent);
		}

	}

	if (ent->cr_name != NULL)
		free(ent->cr_name);
	free(ent);
}


/*
 * cvs_repo_modfree()
 *
 * Free a CVS module structure.
 */
void
cvs_repo_modfree(CVSMODULE *mod)
{
	if (mod->cm_name != NULL)
		free(mod->cm_name);
	if (mod->cm_path != NULL)
		free(mod->cm_path);
	free(mod);
}
