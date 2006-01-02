/*	$OpenBSD: file.c,v 1.134 2006/01/02 08:11:56 xsa Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "file.h"
#include "log.h"

#define CVS_IGN_STATIC	0x01	/* pattern is static, no need to glob */

#define CVS_CHAR_ISMETA(c)	((c == '*') || (c == '?') || (c == '['))


/* ignore pattern */
struct cvs_ignpat {
	char				ip_pat[MAXNAMLEN];
	int				ip_flags;
	TAILQ_ENTRY(cvs_ignpat)		ip_list;
};


/*
 * Standard patterns to ignore.
 */
static const char *cvs_ign_std[] = {
	".",
	"..",
	"*.o",
	"*.so",
	"*.a",
	"*.bak",
	"*.orig",
	"*.rej",
	"*.old",
	"*.exe",
	"*.depend",
	"*.obj",
	"*.elc",
	"*.ln",
	"*.olb",
	"CVS",
	"core",
	"cvslog*",
	"*.core",
	".#*",
	"*~",
	"_$*",
	"*$",
#ifdef OLD_SMELLY_CRUFT
	"RCSLOG",
	"tags",
	"TAGS",
	"RCS",
	"SCCS",
	"cvslog.*",	/* to ignore CVS_CLIENT_LOG output */
	"#*",
	",*",
#endif
};


/*
 * Entries in the CVS/Entries file with a revision of '0' have only been
 * added.  Compare against this revision to see if this is the case
 */
static RCSNUM *cvs_addedrev;


TAILQ_HEAD(, cvs_ignpat)	cvs_ign_pats;

static int cvs_file_getdir(CVSFILE *, int, int (*)(CVSFILE *, void *),
    void *, int);

static int	 cvs_load_dirinfo(CVSFILE *, int);
static int	 cvs_file_sort(struct cvs_flist *, u_int);
static int	 cvs_file_cmp(const void *, const void *);
static int	 cvs_file_cmpname(const char *, const char *);
static CVSFILE	*cvs_file_alloc(const char *, u_int);
static CVSFILE	*cvs_file_lget(const char *, int, CVSFILE *, CVSENTRIES *,
		    struct cvs_ent *);


/*
 * cvs_file_init()
 *
 */
int
cvs_file_init(void)
{
	int i, l;
	size_t len;
	char path[MAXPATHLEN], buf[MAXNAMLEN];
	FILE *ifp;

	TAILQ_INIT(&cvs_ign_pats);

	if ((cvs_addedrev = rcsnum_parse("0")) == NULL)
		return (-1);

	/* standard patterns to ignore */
	for (i = 0; i < (int)(sizeof(cvs_ign_std)/sizeof(char *)); i++)
		cvs_file_ignore(cvs_ign_std[i]);

	/* read the cvsignore file in the user's home directory, if any */
	l = snprintf(path, sizeof(path), "%s/.cvsignore", cvs_homedir);
	if (l == -1 || l >= (int)sizeof(path)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", path);
		return (-1);
	}

	ifp = fopen(path, "r");
	if (ifp == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_ERRNO,
			    "failed to open user's cvsignore file `%s'", path);
	} else {
		while (fgets(buf, (int)sizeof(buf), ifp) != NULL) {
			len = strlen(buf);
			if (len == 0)
				continue;
			if (buf[len - 1] != '\n') {
				cvs_log(LP_ERR, "line too long in `%s'", path);
			}
			buf[--len] = '\0';
			cvs_file_ignore(buf);
		}
		(void)fclose(ifp);
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

	ip = (struct cvs_ignpat *)xmalloc(sizeof(*ip));
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
		} else if (fnmatch(ip->ip_pat, file, flags) == 0)
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
CVSFILE *
cvs_file_create(CVSFILE *parent, const char *path, u_int type, mode_t mode)
{
	int fd, l;
	char fp[MAXPATHLEN], repo[MAXPATHLEN];
	CVSFILE *cfp;

	cfp = cvs_file_alloc(path, type);
	if (cfp == NULL)
		return (NULL);

	l = 0;
	cfp->cf_mode = mode;
	cfp->cf_parent = parent;

	if (type == DT_DIR) {
		cfp->cf_root = cvsroot_get(path);
		if (cfp->cf_root == NULL) {
			cvs_file_free(cfp);
			return (NULL);
		}

		if (cvs_repo_base != NULL) {
			cvs_file_getpath(cfp, fp, sizeof(fp));
			l = snprintf(repo, sizeof(repo), "%s/%s", cvs_repo_base,
			    fp);
		} else {
			cvs_file_getpath(cfp, repo, sizeof(repo));
			l = 0;
		}

		if (l == -1 || l >= (int)sizeof(repo)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", repo);
			cvs_file_free(cfp);
			return (NULL);
		}

		cfp->cf_repo = xstrdup(repo);
		if (((mkdir(path, mode) == -1) && (errno != EEXIST)) ||
		    (cvs_mkadmin(path, cfp->cf_root->cr_str, cfp->cf_repo,
		    NULL, NULL, 0) < 0)) {
			cvs_file_free(cfp);
			return (NULL);
		}
	} else {
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
 * cvs_file_copy()
 *
 * Allocate space to create a copy of the file <orig>.  The copy inherits all
 * of the original's attributes, but does not inherit its children if the
 * original file is a directory.  Note that files copied using this mechanism
 * are linked to their parent, but the parent has no link to the file.  This
 * is so cvs_file_getpath() works.
 * Returns the copied file on success, or NULL on failure.  The returned
 * structure should be freed using cvs_file_free().
 */
CVSFILE *
cvs_file_copy(CVSFILE *orig)
{
	char path[MAXPATHLEN];
	CVSFILE *cfp;

	cvs_file_getpath(orig, path, sizeof(path));

	cfp = cvs_file_alloc(path, orig->cf_type);
	if (cfp == NULL)
		return (NULL);

	cfp->cf_parent = orig->cf_parent;
	cfp->cf_mode = orig->cf_mode;
	cfp->cf_cvstat = orig->cf_cvstat;

	if (orig->cf_type == DT_REG) {
		cfp->cf_etime = orig->cf_etime;
		cfp->cf_mtime = orig->cf_mtime;
	} else if (orig->cf_type == DT_DIR) {
		/* XXX copy CVS directory attributes */
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

int
cvs_file_get(const char *path, int flags, int (*cb)(CVSFILE *, void *),
    void *arg, struct cvs_flist *list)
{
	char *files[1];

	*(const char **)files = path;
	return cvs_file_getspec(files, 1, flags, cb, arg, list);
}


/*
 * cvs_file_getspec()
 *
 * Obtain the info about the supplied files or directories.
 */
int
cvs_file_getspec(char **fspec, int fsn, int flags, int (*cb)(CVSFILE *, void *),
    void *arg, struct cvs_flist *list)
{
	int i, freecf;
	char pcopy[MAXPATHLEN];
	CVSFILE *cf;
	extern char *cvs_rootstr;

	freecf = (list == NULL);
	cvs_error = CVS_EX_DATA;

	/* init the list */
	if (list != NULL)
		SIMPLEQ_INIT(list);

	/*
	 * Fetch the needed information about ".", so we can setup a few
	 * things to get ourselfs going.
	 */
	cf = cvs_file_lget(".", 0, NULL, NULL, NULL);
	if (cf == NULL) {
		cvs_log(LP_ERR, "failed to obtain '.' information");
		return (-1);
	}

	/*
	 * save the base repository path so we can use it to create
	 * the correct full repopath later on.
	 */
	if (cf->cf_repo != NULL) {
		if (cvs_repo_base != NULL)
			xfree(cvs_repo_base);
		cvs_repo_base = xstrdup(cf->cf_repo);
	}

	/*
	 * This will go away when we have support for multiple Roots.
	 */
	if (cvs_rootstr == NULL && cf->cf_root != NULL) {
		cvs_rootstr = xstrdup(cf->cf_root->cr_str);
	}

	cvs_error = CVS_EX_OK;

	/*
	 * Since some commands don't require any files to operate
	 * we can stop right here for those.
	 */
	if (cf->cf_root != NULL) {
		if (cf->cf_root->cr_method != CVS_METHOD_LOCAL &&
		    cvs_cmdop == CVS_OP_CHECKOUT) {
			cvs_file_free(cf);
			return (0);
		}
	}

	cvs_file_free(cf);

	if (cvs_cmdop == CVS_OP_VERSION)
		return (0);

	for (i = 0; i < fsn; i++) {
		strlcpy(pcopy, fspec[i], sizeof(pcopy));

		/*
		 * get rid of any trailing slashes.
		 */
		STRIP_SLASH(pcopy);

		/*
		 * Load the information.
		 */
		cf = cvs_file_loadinfo(pcopy, flags, cb, arg, freecf);
		if (cf == NULL) {
			if (cvs_error != CVS_EX_OK)
				return (-1);
			continue;
		}

		/*
		 * If extra actions are needed, do them now.
		 */
		if (cf->cf_type == DT_DIR) {
			/* do possible extra actions .. */
		} else {
			/* do possible extra actions .. */
		}

		/*
		 * Attach it to a list if requested, otherwise
		 * just free it again.
		 */
		if (list != NULL)
			SIMPLEQ_INSERT_TAIL(list, cf, cf_list);
		else
			cvs_file_free(cf);
	}

	return (0);
}

/*
 * Load the neccesary information about file or directory <path>.
 * Returns a pointer to the loaded information on success, or NULL
 * on failure.
 *
 * If cb is not NULL, the requested path will be passed to that callback
 * with <arg> as an argument.
 *
 * the <freecf> argument is passed to cvs_file_getdir, if this is 1
 * CVSFILE * structs will be free'd once we are done with them.
 */
CVSFILE *
cvs_file_loadinfo(char *path, int flags, int (*cb)(CVSFILE *, void *),
    void *arg, int freecf)
{
	CVSFILE *cf, *base;
	CVSENTRIES *entf;
	struct cvs_ent *ent;
	char *p;
	char parent[MAXPATHLEN], item[MAXPATHLEN];
	int type, callit;
	struct stat st;
	struct cvsroot *root;

	type = 0;
	base = cf = NULL;
	entf = NULL;
	ent = NULL;

	/*
	 * We first have to find out what type of item we are
	 * dealing with. A file or a directory.
	 *
	 * We can do this by stat(2)'ing the item, but since it
	 * might be gone we also check the Entries file in the
	 * parent directory.
	 */

	/* get parent directory */
	if ((p = strrchr(path, '/')) != NULL) {
		*p++ = '\0';
		strlcpy(parent, path, sizeof(parent));
		strlcpy(item, p, sizeof(item));
		*--p = '/';
	} else {
		strlcpy(parent, ".", sizeof(parent));
		strlcpy(item, path, sizeof(item));
	}

	/*
	 * There might not be an Entries file, so do not fail if there
	 * is none available to get the info from.
	 */
	entf = cvs_ent_open(parent, O_RDWR);

	/*
	 * Load the Entry if we successfully opened the Entries file.
	 */
	if (entf != NULL)
		ent = cvs_ent_get(entf, item);

	/*
	 * No Entry available? fall back to stat(2)'ing the item, if
	 * that fails, assume a normal file.
	 */
	if (ent == NULL) {
		if (stat(path, &st) == -1)
			type = DT_REG;
		else
			type = IFTODT(st.st_mode);
	} else {
		if (ent->ce_type == CVS_ENT_DIR)
			type = DT_DIR;
		else
			type = DT_REG;
	}

	/*
	 * Get the base, which is <parent> for a normal file or
	 * <path> for a directory.
	 */
	if (type == DT_DIR)
		base = cvs_file_lget(path, flags, NULL, entf, ent);
	else
		base = cvs_file_lget(parent, flags, NULL, entf, NULL);

	if (base == NULL) {
		cvs_log(LP_ERR, "failed to obtain directory info for '%s'",
		    parent);
		cvs_error = CVS_EX_FILE;
		goto fail;
	}

	/*
	 * Sanity.
	 */
	if (base->cf_type != DT_DIR) {
		cvs_log(LP_ERR, "base directory isn't a directory at all");
		goto fail;
	}

	root = CVS_DIR_ROOT(base);
	if (root == NULL) {
		cvs_error = CVS_EX_BADROOT;
		goto fail;
	}

	/*
	 * If we have a normal file, get the info and link it
	 * to the base.
	 */
	if (type != DT_DIR) {
		cf = cvs_file_lget(path, flags, base, entf, ent);
		if (cf == NULL) {
			cvs_error = CVS_EX_DATA;
			goto fail;
		}

		cvs_file_attach(base, cf);
	}

	/*
	 * Always pass the base directory, unless:
	 * - we are running in server or local mode and the path is not "."
	 * - the directory does not exist on disk.
	 * - the callback is NULL.
	*/
	callit = 1;
	if (cb == NULL)
		callit = 0;

	if ((cvs_cmdop == CVS_OP_SERVER) && (type != DT_DIR))
		callit = 0;

	if ((root->cr_method == CVS_METHOD_LOCAL) && (type != DT_DIR))
		callit = 0;

	if (!(base->cf_flags & CVS_FILE_ONDISK))
		callit = 0;

	if (callit != 0) {
		if ((cvs_error = cb(base,arg)) != CVS_EX_OK)
			goto fail;
	}

	/*
	 * If we have a normal file, pass it as well.
	 */
	if (type != DT_DIR) {
		if ((cb != NULL) && ((cvs_error = cb(cf, arg)) != CVS_EX_OK))
			goto fail;
	} else {
		/*
		 * If the directory exists, recurse through it.
		 */
		if ((base->cf_flags & CVS_FILE_ONDISK) &&
		    cvs_file_getdir(base, flags, cb, arg, freecf) < 0) {
			cvs_error = CVS_EX_FILE;
			goto fail;
		}
	}

	if (entf != NULL) {
		cvs_ent_close(entf);
		entf = NULL;
	}

	return (base);

fail:
	if (entf != NULL)
		cvs_ent_close(entf);
	if (base != NULL)
		cvs_file_free(base);
	return (NULL);
}

/*
 * cvs_file_find()
 *
 * Find the pointer to a CVS file entry within the file hierarchy <hier>.
 * The file's pathname <path> must be relative to the base of <hier>.
 * Returns the entry on success, or NULL on failure.
 */
CVSFILE *
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
			} else if (*(pp + 1) == '\0')
				continue;
		}

		SIMPLEQ_FOREACH(sf, &(cf->cf_files), cf_list)
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
 * cvs_file_getpath()
 *
 * Get the full path of the file <file> and store it in <buf>, which is of
 * size <len>.  For portability, it is recommended that <buf> always be
 * at least MAXPATHLEN bytes long.
 * Returns a pointer to the start of the path.
 */
char *
cvs_file_getpath(CVSFILE *file, char *buf, size_t len)
{
	memset(buf, '\0', len);
	if (file->cf_dir != NULL) {
		strlcat(buf, file->cf_dir, len);
		strlcat(buf, "/", len);
	}

	strlcat(buf, file->cf_name, len);
	return (buf);
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
	if (parent->cf_type != DT_DIR)
		return (-1);

	SIMPLEQ_INSERT_TAIL(&(parent->cf_files), file, cf_list);
	file->cf_parent = parent;

	return (0);
}


/*
 * Load directory information
 */
static int
cvs_load_dirinfo(CVSFILE *cf, int flags)
{
	char fpath[MAXPATHLEN];
	char pbuf[MAXPATHLEN];
	struct stat st;
	int l;

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	/*
	 * Try to obtain the Root for this given directory, if we cannot
	 * get it, fail, unless we are dealing with a directory that is
	 * unknown or not on disk.
	 */
	cf->cf_root = cvsroot_get(fpath);
	if (cf->cf_root == NULL) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN ||
		    !(cf->cf_flags & CVS_FILE_ONDISK))
			return (0);
		return (-1);
	}

	/* if the CVS administrative directory exists, load the info */
	l = snprintf(pbuf, sizeof(pbuf), "%s/" CVS_PATH_CVSDIR, fpath);
	if (l == -1 || l >= (int)sizeof(pbuf)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", pbuf);
		return (-1);
	}

	if ((stat(pbuf, &st) == 0) && S_ISDIR(st.st_mode)) {
		if (cvs_readrepo(fpath, pbuf, sizeof(pbuf)) == 0)
			cf->cf_repo = xstrdup(pbuf);
	} else {
		/*
		 * Fill in the repo path ourselfs.
		 */
		if (cvs_repo_base != NULL) {
			l = snprintf(pbuf, sizeof(pbuf), "%s/%s",
			    cvs_repo_base, fpath);
			if (l == -1 || l >= (int)sizeof(pbuf))
				return (-1);

			cf->cf_repo = xstrdup(pbuf);
		} else
			cf->cf_repo = NULL;
	}

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
cvs_file_getdir(CVSFILE *cf, int flags, int (*cb)(CVSFILE *, void *),
    void *arg, int freecf)
{
	int ret;
	size_t len;
	DIR *dp;
	struct dirent *de;
	char fpath[MAXPATHLEN], pbuf[MAXPATHLEN];
	CVSENTRIES *entf;
	CVSFILE *cfp;
	struct cvs_ent *ent;
	struct cvs_flist dirs;
	int nfiles, ndirs;

	if ((flags & CF_KNOWN) && (cf->cf_cvstat == CVS_FST_UNKNOWN))
		return (0);

	/*
	 * if we are working with a repository, fiddle with
	 * the pathname again.
	 */
	if (flags & CF_REPO) {
		ret = snprintf(fpath, sizeof(fpath), "%s%s%s",
		    cf->cf_root->cr_dir,
		    (cf->cf_dir != NULL) ? "/" : "",
		    (cf->cf_dir != NULL) ? cf->cf_dir : "");
		if (ret == -1 || ret >= (int)sizeof(fpath))
			return (-1);

		if (cf->cf_dir != NULL)
			xfree(cf->cf_dir);
		cf->cf_dir = xstrdup(fpath);
	}

	nfiles = ndirs = 0;
	SIMPLEQ_INIT(&dirs);
	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if ((dp = opendir(fpath)) == NULL) {
		cvs_log(LP_ERRNO, "failed to open directory '%s'", fpath);
		return (-1);
	}

	ret = -1;
	entf = cvs_ent_open(fpath, O_RDWR);
	while ((de = readdir(dp)) != NULL) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;

		len = cvs_path_cat(fpath, de->d_name, pbuf, sizeof(pbuf));
		if (len >= sizeof(pbuf))
			goto done;

		if (entf != NULL)
			ent = cvs_ent_get(entf, de->d_name);
		else
			ent = NULL;

		/*
		 * Do some filtering on the current directory item.
		 */
		if ((flags & CF_IGNORE) && cvs_file_chkign(de->d_name))
			continue;

		if (!(flags & CF_RECURSE) && (de->d_type == DT_DIR)) {
			if (ent != NULL)
				ent->processed = 1;
			continue;
		}

		if ((de->d_type != DT_DIR) && (flags & CF_NOFILES))
			continue;

		cfp = cvs_file_lget(pbuf, flags, cf, entf, ent);
		if (cfp == NULL) {
			cvs_log(LP_ERR, "failed to get '%s'", pbuf);
			goto done;
		}

		/*
		 * A file is linked to the parent <cf>, a directory
		 * is added to the dirs SIMPLEQ list for later use.
		 */
		if ((cfp->cf_type != DT_DIR) && !freecf) {
			SIMPLEQ_INSERT_TAIL(&(cf->cf_files), cfp, cf_list);
			nfiles++;
		} else if (cfp->cf_type == DT_DIR) {
			SIMPLEQ_INSERT_TAIL(&dirs, cfp, cf_list);
			ndirs++;
		}

		/*
		 * Now, for a file, pass it to the callback if it was
		 * supplied to us.
		 */
		if (cfp->cf_type != DT_DIR && cb != NULL) {
			if ((cvs_error = cb(cfp, arg)) != CVS_EX_OK)
				goto done;
		}

		/*
		 * Mark the entry as processed.
		 */
		if (ent != NULL)
			ent->processed = 1;

		/*
		 * If we don't want to keep it, free it
		 */
		if ((cfp->cf_type != DT_DIR) && freecf)
			cvs_file_free(cfp);
	}

	closedir(dp);
	dp = NULL;

	/*
	 * Pass over all of the entries now, so we pickup any files
	 * that might have been lost, or are for some reason not on disk.
	 *
	 * (Follows the same procedure as above ... can we merge them?)
	 */
	while ((entf != NULL) && ((ent = cvs_ent_next(entf)) != NULL)) {
		if (ent->processed == 1)
			continue;
		if (!(flags & CF_RECURSE) && (ent->ce_type == CVS_ENT_DIR))
			continue;
		if ((flags & CF_NOFILES) && (ent->ce_type != CVS_ENT_DIR))
			continue;

		len = cvs_path_cat(fpath, ent->ce_name, pbuf, sizeof(pbuf));
		if (len >= sizeof(pbuf))
			goto done;

		cfp = cvs_file_lget(pbuf, flags, cf, entf, ent);
		if (cfp == NULL) {
			cvs_log(LP_ERR, "failed to fetch '%s'", pbuf);
			goto done;
		}

		if ((cfp->cf_type != DT_DIR) && !freecf) {
			SIMPLEQ_INSERT_TAIL(&(cf->cf_files), cfp, cf_list);
			nfiles++;
		} else if (cfp->cf_type == DT_DIR) {
			SIMPLEQ_INSERT_TAIL(&dirs, cfp, cf_list);
			ndirs++;
		}

		if (cfp->cf_type != DT_DIR && cb != NULL) {
			if ((cvs_error = cb(cfp, arg)) != CVS_EX_OK)
				goto done;
		}

		if ((cfp->cf_type != DT_DIR) && freecf)
			cvs_file_free(cfp);
	}

	/*
	 * Sort files and dirs if requested.
	 */
	if (flags & CF_SORT) {
		if (nfiles > 0)
			cvs_file_sort(&(cf->cf_files), nfiles);
		if (ndirs > 0)
			cvs_file_sort(&dirs, ndirs);
	}

	/*
	 * Finally, run over the directories we have encountered.
	 * Before calling cvs_file_getdir() on them, we pass them
	 * to the callback first.
	 */
	while (!SIMPLEQ_EMPTY(&dirs)) {
		cfp = SIMPLEQ_FIRST(&dirs);
		SIMPLEQ_REMOVE_HEAD(&dirs, cf_list);

		if (!freecf)
			SIMPLEQ_INSERT_TAIL(&(cf->cf_files), cfp, cf_list);

		if (cb != NULL) {
			if ((cvs_error = cb(cfp, arg)) != CVS_EX_OK)
				goto done;
		}

		if ((cfp->cf_flags & CVS_FILE_ONDISK) &&
		    (cvs_file_getdir(cfp, flags, cb, arg, freecf) < 0))
			goto done;

		if (freecf)
			cvs_file_free(cfp);
	}

	ret = 0;
	cfp = NULL;
done:
	if ((cfp != NULL) && freecf)
		cvs_file_free(cfp);

	while (!SIMPLEQ_EMPTY(&dirs)) {
		cfp = SIMPLEQ_FIRST(&dirs);
		SIMPLEQ_REMOVE_HEAD(&dirs, cf_list);

		cvs_file_free(cfp);
	}

	if (entf != NULL)
		cvs_ent_close(entf);
	if (dp != NULL)
		closedir(dp);

	return (ret);
}


/*
 * cvs_file_free()
 *
 * Free a cvs_file structure and its contents.
 */
void
cvs_file_free(CVSFILE *cf)
{
	CVSFILE *child;

	if (cf->cf_name != NULL)
		xfree(cf->cf_name);

	if (cf->cf_dir != NULL)
		xfree(cf->cf_dir);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_root != NULL)
			cvsroot_remove(cf->cf_root);
		if (cf->cf_repo != NULL)
			xfree(cf->cf_repo);
		while (!SIMPLEQ_EMPTY(&(cf->cf_files))) {
			child = SIMPLEQ_FIRST(&(cf->cf_files));
			SIMPLEQ_REMOVE_HEAD(&(cf->cf_files), cf_list);
			cvs_file_free(child);
		}
	} else {
		if (cf->cf_tag != NULL)
			xfree(cf->cf_tag);
		if (cf->cf_opts != NULL)
			xfree(cf->cf_opts);
	}

	xfree(cf);
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

	cfvec = (CVSFILE **)calloc((size_t)nfiles, sizeof(CVSFILE *));
	if (cfvec == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate sorting vector");
		return (-1);
	}

	i = 0;
	SIMPLEQ_FOREACH(cf, flp, cf_list) {
		if (i == (int)nfiles) {
			cvs_log(LP_WARN, "too many files to sort");
			/* rebuild the list and abort sorting */
			while (--i >= 0)
				SIMPLEQ_INSERT_HEAD(flp, cfvec[i], cf_list);
			xfree(cfvec);
			return (-1);
		}
		cfvec[i++] = cf;

		/* now unlink it from the list,
		 * we'll put it back in order later
		 */
		SIMPLEQ_REMOVE_HEAD(flp, cf_list);
	}

	/* clear the list just in case */
	SIMPLEQ_INIT(flp);
	nb = (size_t)i;

	heapsort(cfvec, nb, sizeof(cf), cvs_file_cmp);

	/* rebuild the list from the bottom up */
	for (i = (int)nb - 1; i >= 0; i--)
		SIMPLEQ_INSERT_HEAD(flp, cfvec[i], cf_list);

	xfree(cfvec);
	return (0);
}


static int
cvs_file_cmp(const void *f1, const void *f2)
{
	const CVSFILE *cf1, *cf2;
	cf1 = *(CVSFILE * const *)f1;
	cf2 = *(CVSFILE * const *)f2;
	return cvs_file_cmpname(cf1->cf_name, cf2->cf_name);
}


/*
 * cvs_file_alloc()
 *
 * Allocate a CVSFILE structure and initialize its internals.
 */
CVSFILE *
cvs_file_alloc(const char *path, u_int type)
{
	CVSFILE *cfp;
	char *p;

	cfp = (CVSFILE *)xmalloc(sizeof(*cfp));
	memset(cfp, 0, sizeof(*cfp));

	cfp->cf_type = type;
	cfp->cf_cvstat = CVS_FST_UNKNOWN;

	if (type == DT_DIR) {
		SIMPLEQ_INIT(&(cfp->cf_files));
	}

	cfp->cf_name = xstrdup(basename(path));
	if ((p = strrchr(path, '/')) != NULL) {
		*p = '\0';
		if (strcmp(path, "."))
			cfp->cf_dir = xstrdup(path);
		else
			cfp->cf_dir = NULL;
		*p = '/';
	} else
		cfp->cf_dir = NULL;

	return (cfp);
}


/*
 * cvs_file_lget()
 *
 * Get the file and link it with the parent right away.
 * Returns a pointer to the created file structure on success, or NULL on
 * failure.
 */
static CVSFILE *
cvs_file_lget(const char *path, int flags, CVSFILE *parent, CVSENTRIES *pent,
    struct cvs_ent *ent)
{
	char *c;
	int ret;
	u_int type;
	struct stat st;
	CVSFILE *cfp;
	struct cvsroot *root;

	type = DT_UNKNOWN;
	ret = stat(path, &st);
	if (ret == 0)
		type = IFTODT(st.st_mode);

	if ((flags & CF_REPO) && (type != DT_DIR)) {
		if ((c = strrchr(path, ',')) == NULL)
			return (NULL);
		*c = '\0';
	}

	if ((cfp = cvs_file_alloc(path, type)) == NULL)
		return (NULL);
	cfp->cf_parent = parent;
	cfp->cf_entry = pent;

	if ((cfp->cf_type == DT_DIR) && (cfp->cf_parent == NULL))
		cfp->cf_flags |= CVS_DIRF_BASE;

	if (ret == 0) {
		cfp->cf_mode = st.st_mode & ACCESSPERMS;
		if (cfp->cf_type == DT_REG)
			cfp->cf_mtime = st.st_mtime;
		cfp->cf_flags |= CVS_FILE_ONDISK;

		if (ent == NULL)
			if (cfp->cf_flags & CVS_DIRF_BASE)
				cfp->cf_cvstat = CVS_FST_UPTODATE;
			else
				cfp->cf_cvstat = CVS_FST_UNKNOWN;
		else {
			/* always show directories as up-to-date */
			if (ent->ce_type == CVS_ENT_DIR)
				cfp->cf_cvstat = CVS_FST_UPTODATE;
			else if (rcsnum_cmp(ent->ce_rev, cvs_addedrev, 2) == 0)
				cfp->cf_cvstat = CVS_FST_ADDED;
			else {
				/*
				 * correct st.st_mtime first
				 */
				if ((st.st_mtime =
				    cvs_hack_time(st.st_mtime, 1)) == 0) {
					cvs_file_free(cfp);
					return (NULL);
				}

				/* check last modified time */
				if (ent->ce_mtime == (time_t)st.st_mtime) {
					cfp->cf_cvstat = CVS_FST_UPTODATE;
				} else {
					cfp->cf_cvstat = CVS_FST_MODIFIED;
				}
			}

			cfp->cf_etime = ent->ce_mtime;
		}
	} else {
		if (ent == NULL) {
			/* assume it is a file and unknown */
			cfp->cf_cvstat = CVS_FST_UNKNOWN;
			cfp->cf_type = DT_REG;
		} else {
			if (ent->ce_type == CVS_ENT_FILE)
				cfp->cf_type = DT_REG;
			else if (ent->ce_type == CVS_ENT_DIR)
				cfp->cf_type = DT_DIR;
			else
				cvs_log(LP_WARN, "unknown ce_type %d",
				    ent->ce_type);

			if (ent->ce_status == CVS_ENT_REMOVED)
				cfp->cf_cvstat = CVS_FST_REMOVED;
			else if (ent->ce_status == CVS_ENT_UPTODATE)
				cfp->cf_cvstat = CVS_FST_UPTODATE;
			else if (ent->ce_status == CVS_ENT_ADDED)
				cfp->cf_cvstat = CVS_FST_ADDED;
			else
				cfp->cf_cvstat = CVS_FST_LOST;
		}

		/* XXX assume 0644 ? */
		cfp->cf_mode = 0644;
	}

	if (ent != NULL) {
		/* steal the RCSNUM */
		cfp->cf_lrev = ent->ce_rev;

		if (ent->ce_type == CVS_ENT_FILE) {
			if (ent->ce_tag[0] != '\0')
				cfp->cf_tag = xstrdup(ent->ce_tag);

			if (ent->ce_opts[0] != '\0')
				cfp->cf_opts = xstrdup(ent->ce_opts);
		}
	}

	if (cfp->cf_type == DT_DIR) {
		if (cvs_load_dirinfo(cfp, flags) < 0) {
			cvs_file_free(cfp);
			return (NULL);
		}
	}

	if (flags & CF_REPO) {
		root = CVS_DIR_ROOT(cfp);

		cfp->cf_mode = 0644;
		cfp->cf_cvstat = CVS_FST_LOST;

		c = xstrdup(cfp->cf_dir);
		xfree(cfp->cf_dir);

		if (strcmp(c, root->cr_dir)) {
			c += strlen(root->cr_dir) + 1;
			cfp->cf_dir = xstrdup(c);
			c -= strlen(root->cr_dir) + 1;
		} else {
			cfp->cf_dir = NULL;
		}

		xfree(c);
	}

	if ((cfp->cf_repo != NULL) && (cfp->cf_type == DT_DIR) &&
	    !strcmp(cfp->cf_repo, path))
		cfp->cf_cvstat = CVS_FST_UPTODATE;

	/*
	 * In server mode, we do a few extra checks.
	 */
	if (cvs_cmdop == CVS_OP_SERVER) {
		/*
		 * If for some reason a file was added,
		 * but does not exist anymore, start complaining.
		 */
		if (!(cfp->cf_flags & CVS_FILE_ONDISK) &&
		    (cfp->cf_cvstat == CVS_FST_ADDED) &&
		    (cfp->cf_type != DT_DIR))
			cvs_log(LP_WARN, "new-born %s has disappeared", path);

		/*
		 * Any other needed checks?
		 */
	}

	return (cfp);
}


static int
cvs_file_cmpname(const char *name1, const char *name2)
{
	return (cvs_nocase == 0) ? (strcmp(name1, name2)) :
	    (strcasecmp(name1, name2));
}

/*
 * remove any empty directories.
 */
int
cvs_file_prune(char *path)
{
	DIR *dirp;
	int l, pwd, empty;
	struct dirent *dp;
	char fpath[MAXPATHLEN];
	CVSENTRIES *entf;
	CVSFILE *cfp;

	pwd = (!strcmp(path, "."));

	if ((dirp = opendir(path)) == NULL) {
		cvs_log(LP_ERRNO, "failed to open `%s'", fpath);
		return (-1);
	}

	empty = 0;
	entf = cvs_ent_open(path, O_RDWR);

	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, "..") ||
		    !strcmp(dp->d_name, CVS_PATH_CVSDIR))
			continue;

		empty++;
		if (dp->d_type == DT_DIR) {
			l = snprintf(fpath, sizeof(fpath), "%s%s%s",
			    (pwd) ? "" : path, (pwd) ? "" : "/", dp->d_name);
			if (l == -1 || l >= (int)sizeof(fpath)) {
				errno = ENAMETOOLONG;
				cvs_log(LP_ERRNO, "%s", fpath);
				continue;
			}

			cfp = cvs_file_find(cvs_files, fpath);
			if (cfp == NULL)
				continue;

			/* ignore unknown directories */
			if (cfp->cf_cvstat == CVS_FST_UNKNOWN)
				continue;

			if (cvs_file_prune(fpath)) {
				empty--;
				if (entf)
					cvs_ent_remove(entf, fpath, 0);
			} else {
				empty++;
			}
		}
	}

	closedir(dirp);
	if (entf)
		cvs_ent_close(entf);

	empty = (empty == 0);
	if (empty) {
		if (cvs_rmdir(path) < 0) {
			cvs_log(LP_ERR, "failed to prune `%s'", path);
			empty = 0;
		}
	}

	return (empty);
}
