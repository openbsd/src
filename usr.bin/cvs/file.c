/*	$OpenBSD: file.c,v 1.31 2004/08/27 14:00:29 jfb Exp $	*/
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
#include "file.h"


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
	".#*",
#ifdef OLD_SMELLY_CRUFT
	"RCSLOG",
	"tags",
	"TAGS",
	"RCS",
	"SCCS",
	"#*",
	",*",
#endif
};


/*
 * Entries in the CVS/Entries file with a revision of '0' have only been
 * added.  Compare against this revision to see if this is the case
 */
static RCSNUM *cvs_addedrev;


TAILQ_HEAD(, cvs_ignpat)  cvs_ign_pats;


static int        cvs_file_getdir  (CVSFILE *, int);
static void       cvs_file_freedir (struct cvs_dir *);
static int        cvs_file_sort    (struct cvs_flist *, u_int);
static int        cvs_file_cmp     (const void *, const void *);
static int        cvs_file_cmpname (const char *, const char *);
static CVSFILE*   cvs_file_alloc   (const char *, u_int);
static CVSFILE*   cvs_file_lget    (const char *, int, CVSFILE *);



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
	for (i = 0; i < (int)(sizeof(cvs_ign_std)/sizeof(char *)); i++)
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
 * cvs_file_chkign()
 *
 * Returns 1 if the filename <file> is matched by one of the ignore
 * patterns, or 0 otherwise.
 */

int
cvs_file_chkign(const char *file)
{
	int flags;
	struct cvs_ignpat *ip;

	flags = FNM_PERIOD;
	if (cvs_nocase)
		flags |= FNM_CASEFOLD;

	TAILQ_FOREACH(ip, &cvs_ign_pats, ip_list) {
		if (ip->ip_flags & CVS_IGN_STATIC) {
			if (cvs_file_cmpname(file, ip->ip_pat) == 0)
				return (1);
		}
		else if (fnmatch(ip->ip_pat, file, flags) == 0)
			return (1);
	}

	return (0);
}


/*
 * cvs_file_create()
 *
 * Create a new file whose path is specified in <path> and of type <type>.
 * If the type is DT_DIR, the CVS administrative repository and files will be
 * created.
 * Returns the created file on success, or NULL on failure.
 */

CVSFILE*
cvs_file_create(const char *path, u_int type, mode_t mode)
{
	int fd;
	CVSFILE *cfp;

	cfp = cvs_file_alloc(path, type);
	if (cfp == NULL)
		return (NULL);

	cfp->cf_type = type;
	cfp->cf_mode = mode;
	cfp->cf_ddat->cd_root = cvsroot_get(path);
	cfp->cf_ddat->cd_repo = strdup(cfp->cf_path);

	if (type == DT_DIR) {
		if ((mkdir(path, mode) == -1) || (cvs_mkadmin(cfp, mode) < 0)) {
			cvs_file_free(cfp);
			return (NULL);
		}

		cfp->cf_ddat->cd_ent = cvs_ent_open(path, O_RDWR);
	}
	else {
		fd = open(path, O_WRONLY|O_CREAT|O_EXCL, mode);
		if (fd == -1) {
			cvs_file_free(cfp);
			return (NULL);
		}
		(void)close(fd);
	}

	return (cfp);
}


/*
 * cvs_file_get()
 *
 * Load a cvs_file structure with all the information pertaining to the file
 * <path>.
 * The <flags> parameter specifies various flags that alter the behaviour of
 * the function.  The CF_RECURSE flag causes the function to recursively load
 * subdirectories when <path> is a directory.
 * The CF_SORT flag causes the files to be sorted in alphabetical order upon
 * loading.  The special case of "." as a path specification generates
 * recursion for a single level and is equivalent to calling cvs_file_get() on
 * all files of that directory.
 * Returns a pointer to the cvs file structure, which must later be freed
 * with cvs_file_free().
 */

CVSFILE*
cvs_file_get(const char *path, int flags)
{
	return cvs_file_lget(path, flags, NULL);
}


/*
 * cvs_file_getspec()
 *
 * Load a specific set of files whose paths are given in the vector <fspec>,
 * whose size is given in <fsn>.
 * Returns a pointer to the lowest common subdirectory to all specified
 * files.
 */

CVSFILE*
cvs_file_getspec(char **fspec, int fsn, int flags)
{
	int i;
	char *sp, *np, pcopy[MAXPATHLEN];
	CVSFILE *base, *cf, *nf;

	base = cvs_file_get(".", 0);
	if (base == NULL)
		return (NULL);

	for (i = 0; i < fsn; i++) {
		strlcpy(pcopy, fspec[i], sizeof(pcopy));
		cf = base;
		sp = pcopy;

		do {
			np = strchr(sp, '/');
			if (np != NULL)
				*np = '\0';
			nf = cvs_file_find(cf, sp);
			if (nf == NULL) {
				nf = cvs_file_lget(pcopy, 0, cf);
				if (nf == NULL) {
					cvs_file_free(base);
					return (NULL);
				}

				cvs_file_attach(cf, nf);
			}

			if (np != NULL) {
				*np = '/';
				sp = np + 1;
			}

			cf = nf;
		} while (np != NULL);
	}

	return (base);
}


/*
 * cvs_file_find()
 *
 * Find the pointer to a CVS file entry within the file hierarchy <hier>.
 * The file's pathname <path> must be relative to the base of <hier>.
 * Returns the entry on success, or NULL on failure.
 */

CVSFILE*
cvs_file_find(CVSFILE *hier, const char *path)
{
	char *pp, *sp, pbuf[MAXPATHLEN];
	CVSFILE *sf, *cf;

	strlcpy(pbuf, path, sizeof(pbuf));

	cf = hier;
	pp = pbuf;
	do {
		sp = strchr(pp, '/');
		if (sp != NULL)
			*(sp++) = '\0';

		/* special case */
		if (*pp == '.') {
			if ((*(pp + 1) == '.') && (*(pp + 2) == '\0')) {
				/* request to go back to parent */
				if (cf->cf_parent == NULL) {
					cvs_log(LP_NOTICE,
					    "path %s goes back too far", path);
					return (NULL);
				}
				cf = cf->cf_parent;
				continue;
			}
			else if (*(pp + 1) == '\0')
				continue;
		}

		TAILQ_FOREACH(sf, &(cf->cf_ddat->cd_files), cf_list)
			if (cvs_file_cmpname(pp, sf->cf_name) == 0)
				break;
		if (sf == NULL)
			return (NULL);

		cf = sf;
		pp = sp;
	} while (sp != NULL);

	return (cf);
}


/*
 * cvs_file_attach()
 *
 * Attach the file <file> as one of the children of parent <parent>, which
 * has to be a file of type DT_DIR.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_file_attach(CVSFILE *parent, CVSFILE *file)
{
	struct cvs_dir *dp;

	if (parent->cf_type != DT_DIR)
		return (-1);

	dp = parent->cf_ddat;

	TAILQ_INSERT_TAIL(&(dp->cd_files), file, cf_list);
	dp->cd_nfiles++;
	file->cf_parent = parent;

	return (0);
}


/*
 * cvs_file_getdir()
 *
 * Get a cvs directory structure for the directory whose path is <dir>.
 * This function should not free the directory information on error, as this
 * is performed by cvs_file_free().
 */

static int
cvs_file_getdir(CVSFILE *cf, int flags)
{
	int ret, fd;
	u_int ndirs;
	long base;
	void *dp, *ep;
	char fbuf[2048], pbuf[MAXPATHLEN];
	struct dirent *ent;
	CVSFILE *cfp;
	struct stat st;
	struct cvs_dir *cdp;
	struct cvs_flist dirs;

	ndirs = 0;
	TAILQ_INIT(&dirs);
	cdp = cf->cf_ddat;

	if (cf->cf_cvstat != CVS_FST_UNKNOWN) {
		cdp->cd_root = cvsroot_get(cf->cf_path);
		if (cdp->cd_root == NULL)
			return (-1);

		if (flags & CF_MKADMIN)
			cvs_mkadmin(cf, 0755);

		/* if the CVS administrative directory exists, load the info */
		snprintf(pbuf, sizeof(pbuf), "%s/" CVS_PATH_CVSDIR,
		    cf->cf_path);
		if ((stat(pbuf, &st) == 0) && S_ISDIR(st.st_mode)) {
			if (cvs_readrepo(cf->cf_path, pbuf,
			    sizeof(pbuf)) == 0) {
				cdp->cd_repo = strdup(pbuf);
				if (cdp->cd_repo == NULL) {
					cvs_log(LP_ERRNO,
					    "failed to dup repository string");
					return (-1);
				}
			}

			cdp->cd_ent = cvs_ent_open(cf->cf_path, O_RDWR);
		}
	}

	if (!(flags & CF_RECURSE) || (cf->cf_cvstat == CVS_FST_UNKNOWN))
		return (0);

	fd = open(cf->cf_path, O_RDONLY);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open `%s'", cf->cf_path);
		return (-1);
	}

	do {
		ret = getdirentries(fd, fbuf, sizeof(fbuf), &base);
		if (ret == -1) {
			cvs_log(LP_ERRNO, "failed to get directory entries");
			(void)close(fd);
			return (-1);
		}

		dp = fbuf;
		ep = fbuf + (size_t)ret;
		while (dp < ep) {
			ent = (struct dirent *)dp;
			if (ent->d_fileno == 0)
				continue;
			dp += ent->d_reclen;

			if ((flags & CF_IGNORE) && cvs_file_chkign(ent->d_name))
				continue;

			if ((flags & CF_NOSYMS) && (ent->d_type == DT_LNK))
				continue;

			snprintf(pbuf, sizeof(pbuf), "%s/%s",
			    cf->cf_path, ent->d_name);
			cfp = cvs_file_lget(pbuf, flags, cf);
			if (cfp != NULL) {
				if (cfp->cf_type == DT_DIR) {
					TAILQ_INSERT_TAIL(&dirs, cfp, cf_list);
					ndirs++;
				}
				else {
					TAILQ_INSERT_TAIL(&(cdp->cd_files), cfp,
					    cf_list);
					cdp->cd_nfiles++;
				}
			}
		}
	} while (ret > 0);

	if (flags & CF_SORT) {
		cvs_file_sort(&(cdp->cd_files), cdp->cd_nfiles);
		cvs_file_sort(&dirs, ndirs);
	}

	while (!TAILQ_EMPTY(&dirs)) {
		cfp = TAILQ_FIRST(&dirs);
		TAILQ_REMOVE(&dirs, cfp, cf_list);
		TAILQ_INSERT_TAIL(&(cdp->cd_files), cfp, cf_list);
	}
	cdp->cd_nfiles += ndirs;

	(void)close(fd);

	return (0);
}


/*
 * cvs_file_free()
 *
 * Free a cvs_file structure and its contents.
 */

void
cvs_file_free(CVSFILE *cf)
{
	if (cf->cf_path != NULL)
		free(cf->cf_path);
	if (cf->cf_ddat != NULL)
		cvs_file_freedir(cf->cf_ddat);
	free(cf);
}


/*
 * cvs_file_examine()
 *
 * Examine the contents of the CVS file structure <cf> with the function
 * <exam>.  The function is called for all subdirectories and files of the
 * root file.
 */

int
cvs_file_examine(CVSFILE *cf, int (*exam)(CVSFILE *, void *), void *arg)
{
	int ret;
	CVSFILE *fp;

	if (cf->cf_type == DT_DIR) {
		ret = (*exam)(cf, arg);
		TAILQ_FOREACH(fp, &(cf->cf_ddat->cd_files), cf_list) {
			ret = cvs_file_examine(fp, exam, arg);
			if (ret == -1)
				break;
		}
	}
	else
		ret = (*exam)(cf, arg);

	return (ret);
}


/*
 * cvs_file_freedir()
 *
 * Free a cvs_dir structure and its contents.
 */

static void
cvs_file_freedir(struct cvs_dir *cd)
{
	CVSFILE *cfp;

	if (cd->cd_root != NULL)
		cvsroot_free(cd->cd_root);
	if (cd->cd_repo != NULL)
		free(cd->cd_repo);

	if (cd->cd_ent != NULL)
		cvs_ent_close(cd->cd_ent);

	while (!TAILQ_EMPTY(&(cd->cd_files))) {
		cfp = TAILQ_FIRST(&(cd->cd_files));
		TAILQ_REMOVE(&(cd->cd_files), cfp, cf_list);
		cvs_file_free(cfp);
	}
}


/*
 * cvs_file_sort()
 *
 * Sort a list of cvs file structures according to their filename.  The list
 * <flp> is modified according to the sorting algorithm.  The number of files
 * in the list must be given by <nfiles>.
 * Returns 0 on success, or -1 on failure.
 */

static int
cvs_file_sort(struct cvs_flist *flp, u_int nfiles)
{
	int i;
	size_t nb;
	CVSFILE *cf, **cfvec;

	cfvec = (CVSFILE **)calloc(nfiles, sizeof(CVSFILE *));
	if (cfvec == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate sorting vector");
		return (-1);
	}

	i = 0;
	TAILQ_FOREACH(cf, flp, cf_list) {
		if (i == (int)nfiles) {
			cvs_log(LP_WARN, "too many files to sort");
			/* rebuild the list and abort sorting */
			while (--i >= 0)
				TAILQ_INSERT_HEAD(flp, cfvec[i], cf_list);
			free(cfvec);
			return (-1);
		}
		cfvec[i++] = cf;

		/* now unlink it from the list,
		 * we'll put it back in order later
		 */
		TAILQ_REMOVE(flp, cf, cf_list);
	}

	/* clear the list just in case */
	TAILQ_INIT(flp);
	nb = (size_t)i;

	heapsort(cfvec, nb, sizeof(cf), cvs_file_cmp);

	/* rebuild the list from the bottom up */
	for (i = (int)nb - 1; i >= 0; i--)
		TAILQ_INSERT_HEAD(flp, cfvec[i], cf_list);

	free(cfvec);
	return (0);
}


static int
cvs_file_cmp(const void *f1, const void *f2)
{
	CVSFILE *cf1, *cf2;
	cf1 = *(CVSFILE **)f1;
	cf2 = *(CVSFILE **)f2;
	return cvs_file_cmpname(cf1->cf_name, cf2->cf_name);
}


CVSFILE*
cvs_file_alloc(const char *path, u_int type)
{
	size_t len;
	char pbuf[MAXPATHLEN];
	CVSFILE *cfp;
	struct cvs_dir *ddat;

	cfp = (CVSFILE *)malloc(sizeof(*cfp));
	if (cfp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS file data");
		return (NULL);
	}
	memset(cfp, 0, sizeof(*cfp));

	/* ditch trailing slashes */
	strlcpy(pbuf, path, sizeof(pbuf));
	len = strlen(pbuf);
	while (pbuf[len - 1] == '/')
		pbuf[--len] = '\0';

	cfp->cf_path = strdup(pbuf);
	if (cfp->cf_path == NULL) {
		free(cfp);
		return (NULL);
	}

	cfp->cf_name = strrchr(cfp->cf_path, '/');
	if (cfp->cf_name == NULL)
		cfp->cf_name = cfp->cf_path;
	else
		cfp->cf_name++;

	cfp->cf_type = type;
	cfp->cf_cvstat = CVS_FST_UNKNOWN;

	if (type == DT_DIR) {
		ddat = (struct cvs_dir *)malloc(sizeof(*ddat));
		if (ddat == NULL) {
			cvs_file_free(cfp);
			return (NULL);
		}
		memset(ddat, 0, sizeof(*ddat));
		TAILQ_INIT(&(ddat->cd_files));
		cfp->cf_ddat = ddat;
	}
	return (cfp);
}


/*
 * cvs_file_lget()
 *
 * Get the file and link it with the parent right away.
 * Returns a pointer to the created file structure on success, or NULL on
 * failure.
 */

static CVSFILE*
cvs_file_lget(const char *path, int flags, CVSFILE *parent)
{
	int cwd;
	struct stat st;
	CVSFILE *cfp;
	struct cvs_ent *ent;

	ent = NULL;

	if (strcmp(path, ".") == 0)
		cwd = 1;
	else
		cwd = 0;

	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat %s", path);
		return (NULL);
	}

	cfp = cvs_file_alloc(path, IFTODT(st.st_mode));
	if (cfp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS file data");
		return (NULL);
	}
	cfp->cf_parent = parent;
	cfp->cf_mode = st.st_mode & ACCESSPERMS;
	cfp->cf_mtime = st.st_mtime;

	if ((parent != NULL) && (CVS_DIR_ENTRIES(parent) != NULL)) {
		ent = cvs_ent_get(CVS_DIR_ENTRIES(parent), cfp->cf_name);
	}

	if (ent == NULL) {
		cfp->cf_cvstat = (cwd == 1) ?
		    CVS_FST_UPTODATE : CVS_FST_UNKNOWN;
	}
	else {
		/* always show directories as up-to-date */
		if (ent->ce_type == CVS_ENT_DIR)
			cfp->cf_cvstat = CVS_FST_UPTODATE;
		else if (rcsnum_cmp(ent->ce_rev, cvs_addedrev, 2) == 0)
			cfp->cf_cvstat = CVS_FST_ADDED;
		else {
			/* check last modified time */
			if (ent->ce_mtime == st.st_mtime)
				cfp->cf_cvstat = CVS_FST_UPTODATE;
			else
				cfp->cf_cvstat = CVS_FST_MODIFIED;
		}
	}

	if ((cfp->cf_type == DT_DIR) && (cvs_file_getdir(cfp, flags) < 0)) {
		cvs_file_free(cfp);
		return (NULL);
	}

	return (cfp);
}


static int
cvs_file_cmpname(const char *name1, const char *name2)
{
	return (cvs_nocase == 0) ? (strcmp(name1, name2)) :
	    (strcasecmp(name1, name2));
}
