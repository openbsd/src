/*	$OpenBSD: file.c,v 1.4 2004/07/25 03:18:52 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fnmatch.h>

#include "cvs.h"
#include "log.h"


#define CVS_IGN_STATIC    0x01     /* pattern is static, no need to glob */



#define CVS_CHAR_ISMETA(c)  ((c == '*') || (c == '?') || (c == '['))



/* ignore pattern */
struct cvs_ignpat {
	char  ip_pat[MAXNAMLEN];
	int   ip_flags;
	TAILQ_ENTRY (cvs_ignpat) ip_list;
};


/*
 * Standard patterns to ignore.
 */

static const char *cvs_ign_std[] = {
	".",
	"..",
	"*.o",
	"*.so",
	"*.bak",
	"*.orig",
	"*.rej",
	"*.exe",
	"*.depend",
	"CVS",
	"core",
#ifdef OLD_SMELLY_CRUFT
	"RCSLOG",
	"tags",
	"TAGS",
	"RCS",
	"SCCS",
	"#*",
	".#*",
	",*",
#endif
};


static RCSNUM *cvs_addedrev;


TAILQ_HEAD(, cvs_ignpat)  cvs_ign_pats;


static struct cvs_dir*  cvs_file_getdir  (struct cvs_file *, int);
static void             cvs_file_freedir (struct cvs_dir *);
static int              cvs_file_sort    (struct cvs_flist *);
static int              cvs_file_cmp     (const void *, const void *);



/*
 * cvs_file_init()
 *
 */

int
cvs_file_init(void)
{
	int i;
	size_t len;
	char path[MAXPATHLEN], buf[MAXNAMLEN];
	FILE *ifp;
	struct passwd *pwd;

	TAILQ_INIT(&cvs_ign_pats);

	cvs_addedrev = rcsnum_alloc();
	rcsnum_aton("0", NULL, cvs_addedrev);

	/* standard patterns to ignore */
	for (i = 0; i < sizeof(cvs_ign_std)/sizeof(char *); i++)
		cvs_file_ignore(cvs_ign_std[i]); 

	/* read the cvsignore file in the user's home directory, if any */
	pwd = getpwuid(getuid());
	if (pwd != NULL) {
		snprintf(path, sizeof(path), "%s/.cvsignore", pwd->pw_dir);
		ifp = fopen(path, "r");
		if (ifp == NULL) {
			if (errno != ENOENT)
				cvs_log(LP_ERRNO, "failed to open `%s'", path);
		}
		else {
			while (fgets(buf, sizeof(buf), ifp) != NULL) {
				len = strlen(buf);
				if (len == 0)
					continue;
				if (buf[len - 1] != '\n') {
					cvs_log(LP_ERR, "line too long in `%s'",
					    path);
				}
				buf[--len] = '\0';
				cvs_file_ignore(buf);
			}
			(void)fclose(ifp);
		}
	}

	return (0);
}


/*
 * cvs_file_ignore()
 *
 * Add the pattern <pat> to the list of patterns for files to ignore.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_file_ignore(const char *pat)
{
	char *cp;
	struct cvs_ignpat *ip;

	ip = (struct cvs_ignpat *)malloc(sizeof(*ip));
	if (ip == NULL) {
		cvs_log(LP_ERR, "failed to allocate space for ignore pattern");
		return (-1);
	}

	strlcpy(ip->ip_pat, pat, sizeof(ip->ip_pat));

	/* check if we will need globbing for that pattern */
	ip->ip_flags = CVS_IGN_STATIC;
	for (cp = ip->ip_pat; *cp != '\0'; cp++) {
		if (CVS_CHAR_ISMETA(*cp)) {
			ip->ip_flags &= ~CVS_IGN_STATIC;
			break;
		}
	}

	TAILQ_INSERT_TAIL(&cvs_ign_pats, ip, ip_list);

	return (0);
}


/*
 * cvs_file_isignored()
 *
 * Returns 1 if the filename <file> is matched by one of the ignore
 * patterns, or 0 otherwise.
 */

int
cvs_file_isignored(const char *file)
{
	struct cvs_ignpat *ip;

	TAILQ_FOREACH(ip, &cvs_ign_pats, ip_list) {
		if (ip->ip_flags & CVS_IGN_STATIC) {
			if (strcmp(file, ip->ip_pat) == 0)
				return (1);
		}
		else if (fnmatch(ip->ip_pat, file, FNM_PERIOD) == 0)
			return (1);
	}

	return (0);
}


/*
 * cvs_file_getv()
 *
 * Get a vector of all the files found in the directory <dir> and not
 * matching any of the ignore patterns.  The number of files found is
 * returned in <nfiles>.
 * Returns a pointer to a dynamically-allocated string vector on success,
 * or NULL on failure.  The returned vector should be freed with
 * cvs_freeargv().
 */

char**
cvs_file_getv(const char *dir, int *nfiles, int recurse)
{
	int nf, ret, fd;
	long base;
	void *dp, *ep, *tmp;
	char fbuf[1024], **fvec;
	struct dirent *ent;

	*nfiles = 0;
	fvec = NULL;

	fd = open(dir, O_RDONLY);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open `%s'", dir);
		return (NULL);
	}
	ret = getdirentries(fd, fbuf, sizeof(fbuf), &base);
	if (ret == -1) {
		cvs_log(LP_ERRNO, "failed to get directory entries");
		(void)close(fd);
		return (NULL);
	}

	dp = fbuf;
	ep = fbuf + (size_t)ret;
	while (dp < ep) {
		ent = (struct dirent *)dp;
		dp += ent->d_reclen;

		if (cvs_file_isignored(ent->d_name))
			continue;

		tmp = realloc(fvec, (*nfiles + 1) * sizeof(char *));
		if (tmp == NULL) {
			cvs_log(LP_ERRNO, "failed to reallocate file vector");
			(void)close(fd);
			free(fvec);
			return (NULL);
		}
		fvec[++(*nfiles)] = strdup(ent->d_name);

		*nfiles++;
	}

	(void)close(fd);

	return (fvec);
}


/*
 * cvs_file_get()
 *
 * Load a cvs_file structure with all the information pertaining to the file
 * <path>.
 * The <flags> parameter specifies various flags that alter the behaviour of
 * the function.  The CF_STAT flag is used to keep stat information of the
 * file in the structure after it is used (it is lost otherwise).  The
 * CF_RECURSE flag causes the function to recursively load subdirectories
 * when <path> is a directory.  The CF_SORT flag causes the files to be
 * sorted in alphabetical order upon loading.
 * The special case of "." as a path specification generates recursion for
 * a single level and is equivalent to calling cvs_file_get() on all files
 * of that directory.
 * Returns a pointer to the cvs file structure, which must later be freed
 * with cvs_file_free().
 */

struct cvs_file*
cvs_file_get(const char *path, int flags)
{
	int cwd;
	size_t dlen;
	struct stat st;
	struct cvs_file *cfp;
	struct cvs_ent *ent;

	printf("cvs_file_get(%s)\n", path);

	if (strcmp(path, ".") == 0)
		cwd = 1;
	else
		cwd = 0;

	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat %s", path);
		return (NULL);
	}

	cfp = (struct cvs_file *)malloc(sizeof(*cfp));
	if (cfp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS file data");
		return (NULL);
	}
	memset(cfp, 0, sizeof(*cfp));

	ent = cvs_ent_getent(path);
	if (ent == NULL)
		cfp->cf_cvstat = (cwd == 1) ?
		    CVS_FST_UPTODATE : CVS_FST_UNKNOWN;
	else {
		if (rcsnum_cmp(ent->ce_rev, cvs_addedrev, 2) == 0)
			cfp->cf_cvstat = CVS_FST_ADDED;
		else
			cfp->cf_cvstat = CVS_FST_UPTODATE;

		cvs_ent_free(ent);
	}

	cfp->cf_path = strdup(path);
	if (cfp->cf_path == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate file path");
		free(cfp);
		return (NULL);
	}

	cfp->cf_name = strrchr(cfp->cf_path, '/');
	if (cfp->cf_name == NULL)
		cfp->cf_name = cfp->cf_path;
	else
		cfp->cf_name++;

	/* convert from stat mode to dirent values */
	cfp->cf_type = IFTODT(st.st_mode);
	if ((cfp->cf_type == DT_DIR) && ((flags & CF_RECURSE) || cwd)) {
		if ((flags & CF_KNOWN) && (cfp->cf_cvstat == CVS_FST_UNKNOWN))
			cfp->cf_ddat = NULL;
		else {
			cfp->cf_ddat = cvs_file_getdir(cfp, flags);
			if (cfp->cf_ddat == NULL) {
				cvs_file_free(cfp);
				return (NULL);
			}
		}
	}

	if (flags & CF_STAT) {
		cfp->cf_stat = (struct stat *)malloc(sizeof(struct stat));
		if (cfp->cf_stat == NULL) {
			cvs_log(LP_ERRNO, "failed to allocate stat structure");
			cvs_file_free(cfp);
			return (NULL);
		}

		memcpy(cfp->cf_stat, &st, sizeof(struct stat));
	}

	return (cfp);
}


/*
 * cvs_file_getdir()
 *
 * Get a cvs directory structure for the directory whose path is <dir>.
 */

static struct cvs_dir*
cvs_file_getdir(struct cvs_file *cf, int flags)
{
	int nf, ret, fd;
	long base;
	void *dp, *ep, *tmp;
	char fbuf[1024], pbuf[MAXPATHLEN];
	struct dirent *ent;
	struct cvs_file *cfp;
	struct cvs_dir *cdp;

	cdp = (struct cvs_dir *)malloc(sizeof(*cdp));
	if (cdp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate dir");
		return (NULL);
	}
	memset(cdp, 0, sizeof(*cdp));
	LIST_INIT(&(cdp->cd_files));

	if (cvs_readrepo(cf->cf_path, pbuf, sizeof(pbuf)) == 0) {
		cdp->cd_repo = strdup(pbuf);
		if (cdp->cd_repo == NULL) {
			free(cdp);
			return (NULL);
		}
	}

	cdp->cd_root = cvsroot_get(cf->cf_path);
	if (cdp->cd_root == NULL) {
		cvs_file_freedir(cdp);
		return (NULL);
	}

	fd = open(cf->cf_path, O_RDONLY);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open `%s'", cf->cf_path);
		cvs_file_freedir(cdp);
		return (NULL);
	}
	ret = getdirentries(fd, fbuf, sizeof(fbuf), &base);
	if (ret == -1) {
		cvs_log(LP_ERRNO, "failed to get directory entries");
		(void)close(fd);
		cvs_file_freedir(cdp);
		return (NULL);
	}

	dp = fbuf;
	ep = fbuf + (size_t)ret;
	while (dp < ep) {
		ent = (struct dirent *)dp;
		dp += ent->d_reclen;

		if ((flags & CF_IGNORE) && cvs_file_isignored(ent->d_name))
			continue;

		snprintf(pbuf, sizeof(pbuf), "%s/%s", cf->cf_path, ent->d_name);
		cfp = cvs_file_get(pbuf, flags);
		if (cfp != NULL) {
			cfp->cf_parent = cf;
			LIST_INSERT_HEAD(&(cdp->cd_files), cfp, cf_list);
		}
	}

	if (flags & CF_SORT)
		cvs_file_sort(&(cdp->cd_files));

	(void)close(fd);

	return (cdp);
}


/*
 * cvs_file_free()
 *
 * Free a cvs_file structure and its contents.
 */

void
cvs_file_free(struct cvs_file *cf)
{
	struct cvs_file *cfp;
	struct cvs_dir *cd;

	if (cf->cf_path != NULL)
		free(cf->cf_path);
	if (cf->cf_stat != NULL)
		free(cf->cf_stat);
	if (cf->cf_ddat != NULL)
		cvs_file_freedir(cf->cf_ddat);
	free(cf);
}


/*
 * cvs_file_freedir()
 *
 * Free a cvs_dir structure and its contents.
 */

static void
cvs_file_freedir(struct cvs_dir *cd)
{
	struct cvs_file *cfp;

	if (cd->cd_root != NULL)
		cvsroot_free(cd->cd_root);
	if (cd->cd_repo != NULL)
		free(cd->cd_repo);

	while (!LIST_EMPTY(&(cd->cd_files))) {
		cfp = LIST_FIRST(&(cd->cd_files));
		LIST_REMOVE(cfp, cf_list);
		cvs_file_free(cfp);
	}
}


/*
 * cvs_file_sort()
 *
 * Sort a list of cvs file structures according to their filename.
 */

static int
cvs_file_sort(struct cvs_flist *flp)
{
	int i;
	size_t nb;
	struct cvs_file *cf, *cfvec[256];

	i = 0;
	LIST_FOREACH(cf, flp, cf_list) {
		cfvec[i++] = cf;
		if (i == sizeof(cfvec)/sizeof(struct cvs_file *)) {
			cvs_log(LP_WARN, "too many files to sort");
			return (-1);
		}

		/* now unlink it from the list,
		 * we'll put it back in order later
		 */
		LIST_REMOVE(cf, cf_list);
	}

	/* clear the list just in case */
	LIST_INIT(flp);
	nb = (size_t)i;

	heapsort(cfvec, nb, sizeof(cf), cvs_file_cmp);

	/* rebuild the list from the bottom up */
	for (i = (int)nb - 1; i >= 0; i--)
		LIST_INSERT_HEAD(flp, cfvec[i], cf_list);

	return (0);
}


static int
cvs_file_cmp(const void *f1, const void *f2)
{
	struct cvs_file *cf1, *cf2;
	cf1 = *(struct cvs_file **)f1;
	cf2 = *(struct cvs_file **)f2;
	return strcmp(cf1->cf_name, cf2->cf_name);
}
