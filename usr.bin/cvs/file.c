/*	$OpenBSD: file.c,v 1.262 2010/10/28 15:02:41 millert Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "cvs.h"
#include "remote.h"

#define CVS_IGN_STATIC	0x01	/* pattern is static, no need to glob */

#define CVS_CHAR_ISMETA(c)	((c == '*') || (c == '?') || (c == '['))

extern int print_stdout;
extern int build_dirs;

/*
 * Standard patterns to ignore.
 */
static const char *cvs_ign_std[] = {
	".",
	"..",
	"*.o",
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
};

char *cvs_directory_tag = NULL;
struct ignore_head cvs_ign_pats;
struct ignore_head dir_ign_pats;
struct ignore_head checkout_ign_pats;

RB_GENERATE(cvs_flisthead, cvs_filelist, flist, cvs_filelist_cmp);

void
cvs_file_init(void)
{
	int i;
	FILE *ifp;
	char path[MAXPATHLEN], buf[MAXNAMLEN];

	TAILQ_INIT(&cvs_ign_pats);
	TAILQ_INIT(&dir_ign_pats);
	TAILQ_INIT(&checkout_ign_pats);

	/* standard patterns to ignore */
	for (i = 0; i < (int)(sizeof(cvs_ign_std)/sizeof(char *)); i++)
		cvs_file_ignore(cvs_ign_std[i], &cvs_ign_pats);

	if (cvs_homedir == NULL)
		return;

	/* read the cvsignore file in the user's home directory, if any */
	(void)xsnprintf(path, MAXPATHLEN, "%s/.cvsignore", cvs_homedir);

	ifp = fopen(path, "r");
	if (ifp == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_ERRNO,
			    "failed to open user's cvsignore file `%s'", path);
	} else {
		while (fgets(buf, MAXNAMLEN, ifp) != NULL) {
			buf[strcspn(buf, "\n")] = '\0';
			if (buf[0] == '\0')
				continue;

			cvs_file_ignore(buf, &cvs_ign_pats);
		}

		(void)fclose(ifp);
	}
}

void
cvs_file_ignore(const char *pat, struct ignore_head *list)
{
	char *cp;
	size_t len;
	struct cvs_ignpat *ip;

	ip = xmalloc(sizeof(*ip));
	len = strlcpy(ip->ip_pat, pat, sizeof(ip->ip_pat));
	if (len >= sizeof(ip->ip_pat))
		fatal("cvs_file_ignore: truncation of pattern '%s'", pat);

	/* check if we will need globbing for that pattern */
	ip->ip_flags = CVS_IGN_STATIC;
	for (cp = ip->ip_pat; *cp != '\0'; cp++) {
		if (CVS_CHAR_ISMETA(*cp)) {
			ip->ip_flags &= ~CVS_IGN_STATIC;
			break;
		}
	}

	TAILQ_INSERT_TAIL(list, ip, ip_list);
}

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

	TAILQ_FOREACH(ip, &dir_ign_pats, ip_list) {
		if (ip->ip_flags & CVS_IGN_STATIC) {
			if (cvs_file_cmpname(file, ip->ip_pat) == 0)
				return (1);
		} else if (fnmatch(ip->ip_pat, file, flags) == 0)
			return (1);
	}

	TAILQ_FOREACH(ip, &checkout_ign_pats, ip_list) {
		if (ip->ip_flags & CVS_IGN_STATIC) {
			if (cvs_file_cmpname(file, ip->ip_pat) == 0)
				return (1);
		} else if (fnmatch(ip->ip_pat, file, flags) == 0)
			return (1);
	}

	return (0);
}

void
cvs_file_run(int argc, char **argv, struct cvs_recursion *cr)
{
	int i;
	struct cvs_flisthead fl;

	RB_INIT(&fl);

	for (i = 0; i < argc; i++) {
		STRIP_SLASH(argv[i]);
		cvs_file_get(argv[i], FILE_USER_SUPPLIED, &fl, 0);
	}

	cvs_file_walklist(&fl, cr);
	cvs_file_freelist(&fl);
}

struct cvs_filelist *
cvs_file_get(char *name, int flags, struct cvs_flisthead *fl, int type)
{
	char *p;
	struct cvs_filelist *l, find;

	for (p = name; p[0] == '.' && p[1] == '/';)
		p += 2;

	find.file_path = p;
	l = RB_FIND(cvs_flisthead, fl, &find);
	if (l != NULL)
		return (l);

	l = (struct cvs_filelist *)xmalloc(sizeof(*l));
	l->file_path = xstrdup(p);
	l->flags = flags;
	l->type = type;

	RB_INSERT(cvs_flisthead, fl, l);
	return (l);
}

struct cvs_file *
cvs_file_get_cf(const char *d, const char *f, const char *fpath, int fd,
	int type, int flags)
{
	const char *p;
	struct cvs_file *cf;

	for (p = fpath; p[0] == '.' && p[1] == '/';)
		p += 2;

	cf = (struct cvs_file *)xcalloc(1, sizeof(*cf));

	cf->file_name = xstrdup(f);
	cf->file_wd = xstrdup(d);
	cf->file_path = xstrdup(p);
	cf->fd = fd;
	cf->repo_fd = -1;
	cf->file_type = type;
	cf->file_status = 0;
	cf->file_flags = flags;
	cf->in_attic = 0;
	cf->file_ent = NULL;

	if (cf->fd != -1)
		cf->file_flags |= FILE_ON_DISK;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL ||
	    cvs_server_active == 1)
		cvs_validate_directory(cf->file_path);

	return (cf);
}

void
cvs_file_walklist(struct cvs_flisthead *fl, struct cvs_recursion *cr)
{
	int fd, type;
	struct stat st;
	struct cvs_file *cf;
	struct cvs_filelist *l, *nxt;
	char *d, *f, repo[MAXPATHLEN], fpath[MAXPATHLEN];

	for (l = RB_MIN(cvs_flisthead, fl); l != NULL; l = nxt) {
		if (cvs_quit)
			fatal("received signal %d", sig_received);

		cvs_log(LP_TRACE, "cvs_file_walklist: element '%s'",
		    l->file_path);

		if ((f = basename(l->file_path)) == NULL)
			fatal("cvs_file_walklist: basename failed");
		if ((d = dirname(l->file_path)) == NULL)
			fatal("cvs_file_walklist: dirname failed");

		type = l->type;
		if ((fd = open(l->file_path, O_RDONLY)) != -1) {
			if (type == 0) {
				if (fstat(fd, &st) == -1) {
					cvs_log(LP_ERRNO, "%s", l->file_path);
					(void)close(fd);
					goto next;
				}

				if (S_ISDIR(st.st_mode))
					type = CVS_DIR;
				else if (S_ISREG(st.st_mode))
					type = CVS_FILE;
				else {
					cvs_log(LP_ERR,
					    "ignoring bad file type for %s",
					    l->file_path);
					(void)close(fd);
					goto next;
				}
			}
		} else if (current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
			/*
			 * During checkout -p, do not use any locally
			 * available directories.
			 */
			if ((cmdp->cmd_flags & CVS_USE_WDIR) &&
			    (cvs_cmdop != CVS_OP_CHECKOUT || !print_stdout))
				if (stat(d, &st) == -1) {
					cvs_log(LP_ERRNO, "%s", d);
					goto next;
				}

			cvs_get_repository_path(d, repo, MAXPATHLEN);
			(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s",
			    repo, f);

			if ((fd = open(fpath, O_RDONLY)) == -1) {
				strlcat(fpath, RCS_FILE_EXT, MAXPATHLEN);
				fd = open(fpath, O_RDONLY);
			}

			if (fd != -1 && type == 0) {
				if (fstat(fd, &st) == -1)
					fatal("cvs_file_walklist: %s: %s",
					     fpath, strerror(errno));

				if (S_ISDIR(st.st_mode))
					type = CVS_DIR;
				else if (S_ISREG(st.st_mode))
					type = CVS_FILE;
				else {
					cvs_log(LP_ERR,
					    "ignoring bad file type for %s",
					    l->file_path);
					(void)close(fd);
					goto next;
				}

				/* this file is not in our working copy yet */
				(void)close(fd);
				fd = -1;
			} else if (fd != -1) {
				close(fd);
				fd = -1;
			}
		}

		cf = cvs_file_get_cf(d, f, l->file_path, fd, type, l->flags);
		if (cf->file_type == CVS_DIR) {
			cvs_file_walkdir(cf, cr);
		} else {
			if (l->flags & FILE_USER_SUPPLIED) {
				cvs_parse_tagfile(cf->file_wd,
				    &cvs_directory_tag, NULL, NULL);

				if (cvs_directory_tag == NULL &&
				    cvs_specified_tag != NULL)
					cvs_directory_tag =
					    xstrdup(cvs_specified_tag);

				if (current_cvsroot->cr_method ==
				    CVS_METHOD_LOCAL) {
					cvs_get_repository_path(cf->file_wd,
					    repo, MAXPATHLEN);
					cvs_repository_lock(repo,
					    (cmdp->cmd_flags & CVS_LOCK_REPO));
				}
			}

			if (cr->fileproc != NULL)
				cr->fileproc(cf);

			if (l->flags & FILE_USER_SUPPLIED) {
				if (cmdp->cmd_flags & CVS_LOCK_REPO)
					cvs_repository_unlock(repo);
				if (cvs_directory_tag != NULL) {
					xfree(cvs_directory_tag);
					cvs_directory_tag = NULL;
				}
			}
		}

		cvs_file_free(cf);

next:
		nxt = RB_NEXT(cvs_flisthead, fl, l);
	}
}

void
cvs_file_walkdir(struct cvs_file *cf, struct cvs_recursion *cr)
{
	int l, type;
	FILE *fp;
	int nbytes;
	off_t base;
	size_t bufsize;
	struct stat st;
	struct dirent *dp;
	struct cvs_ent *ent;
	struct cvs_ignpat *ip;
	struct cvs_ent_line *line;
	struct cvs_flisthead fl, dl;
	CVSENTRIES *entlist;
	char *buf, *ebuf, *cp, repo[MAXPATHLEN], fpath[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_file_walkdir(%s)", cf->file_path);

	if (cr->enterdir != NULL)
		cr->enterdir(cf);

	if (cr->fileproc != NULL)
		cr->fileproc(cf);

	if (cf->file_status == FILE_SKIP)
		return;

	/*
	 * If this is a repository-only command, do not touch any
	 * locally available directories or try to create them.
	 */
	if (!(cmdp->cmd_flags & CVS_USE_WDIR)) {
		RB_INIT(&fl);
		RB_INIT(&dl);
		goto walkrepo;
	}

	/*
	 * If we do not have an admin directory inside here, dont bother,
	 * unless we are running export or import.
	 */
	(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s", cf->file_path,
	    CVS_PATH_CVSDIR);

	l = stat(fpath, &st);
	if (cvs_cmdop != CVS_OP_EXPORT && cvs_cmdop != CVS_OP_IMPORT &&
	    (l == -1 || (l == 0 && !S_ISDIR(st.st_mode)))) {
		return;
	}

	cvs_parse_tagfile(cf->file_path, &cvs_directory_tag, NULL, NULL);

	/*
	 * check for a local .cvsignore file
	 */
	(void)xsnprintf(fpath, MAXPATHLEN, "%s/.cvsignore", cf->file_path);

	if ((fp = fopen(fpath, "r")) != NULL) {
		while (fgets(fpath, MAXPATHLEN, fp) != NULL) {
			fpath[strcspn(fpath, "\n")] = '\0';
			if (fpath[0] == '\0')
				continue;

			cvs_file_ignore(fpath, &dir_ign_pats);
		}

		(void)fclose(fp);
	}

	if (fstat(cf->fd, &st) == -1)
		fatal("cvs_file_walkdir: %s %s", cf->file_path,
		    strerror(errno));

	if (st.st_size > SIZE_MAX)
		fatal("cvs_file_walkdir: %s: file size too big", cf->file_name);

	bufsize = st.st_size;
	if (bufsize < st.st_blksize)
		bufsize = st.st_blksize;

	buf = xmalloc(bufsize);
	RB_INIT(&fl);
	RB_INIT(&dl);

	while ((nbytes = getdirentries(cf->fd, buf, bufsize, &base)) > 0) {
		ebuf = buf + nbytes;
		cp = buf;

		while (cp < ebuf) {
			dp = (struct dirent *)cp;
			if (!strcmp(dp->d_name, ".") ||
			    !strcmp(dp->d_name, "..") ||
			    !strcmp(dp->d_name, CVS_PATH_CVSDIR) ||
			    dp->d_fileno == 0) {
				cp += dp->d_reclen;
				continue;
			}

			if (cvs_file_chkign(dp->d_name) &&
			    cvs_cmdop != CVS_OP_RLOG &&
			    cvs_cmdop != CVS_OP_RTAG) {
				cp += dp->d_reclen;
				continue;
			}

			(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s",
			    cf->file_path, dp->d_name);

			/*
			 * nfs and afs will show d_type as DT_UNKNOWN
			 * for files and/or directories so when we encounter
			 * this we call lstat() on the path to be sure.
			 */
			if (dp->d_type == DT_UNKNOWN) {
				if (lstat(fpath, &st) == -1)
					fatal("'%s': %s", fpath,
					    strerror(errno));

				switch (st.st_mode & S_IFMT) {
				case S_IFDIR:
					type = CVS_DIR;
					break;
				case S_IFREG:
					type = CVS_FILE;
					break;
				default:
					type = FILE_SKIP;
					break;
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
					type = FILE_SKIP;
					break;
				}
			}

			if (type == FILE_SKIP) {
				if (verbosity > 1) {
					cvs_log(LP_NOTICE, "ignoring `%s'",
					    dp->d_name);
				}
				cp += dp->d_reclen;
				continue;
			}

			switch (type) {
			case CVS_DIR:
				if (cr->flags & CR_RECURSE_DIRS)
					cvs_file_get(fpath, 0, &dl, CVS_DIR);
				break;
			case CVS_FILE:
				cvs_file_get(fpath, 0, &fl, CVS_FILE);
				break;
			default:
				fatal("type %d unknown, shouldn't happen",
				    type);
			}

			cp += dp->d_reclen;
		}
	}

	if (nbytes == -1)
		fatal("cvs_file_walkdir: %s %s", cf->file_path,
		    strerror(errno));

	xfree(buf);

	while ((ip = TAILQ_FIRST(&dir_ign_pats)) != NULL) {
		TAILQ_REMOVE(&dir_ign_pats, ip, ip_list);
		xfree(ip);
	}

	entlist = cvs_ent_open(cf->file_path);
	TAILQ_FOREACH(line, &(entlist->cef_ent), entries_list) {
		ent = cvs_ent_parse(line->buf);

		(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s", cf->file_path,
		    ent->ce_name);

		if (!(cr->flags & CR_RECURSE_DIRS) &&
		    ent->ce_type == CVS_ENT_DIR)
			continue;
		if (ent->ce_type == CVS_ENT_DIR)
			cvs_file_get(fpath, 0, &dl, CVS_DIR);
		else if (ent->ce_type == CVS_ENT_FILE)
			cvs_file_get(fpath, 0, &fl, CVS_FILE);

		cvs_ent_free(ent);
	}

walkrepo:
	if (current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		cvs_get_repository_path(cf->file_path, repo, MAXPATHLEN);
		cvs_repository_lock(repo, (cmdp->cmd_flags & CVS_LOCK_REPO));
	}

	if (cr->flags & CR_REPO) {
		xsnprintf(fpath, sizeof(fpath), "%s/%s", cf->file_path,
		    CVS_PATH_STATICENTRIES);

		if (!(cmdp->cmd_flags & CVS_USE_WDIR) ||
		    stat(fpath, &st) == -1 || build_dirs == 1)
			cvs_repository_getdir(repo, cf->file_path, &fl, &dl,
			    (cr->flags & CR_RECURSE_DIRS) ?
			    REPOSITORY_DODIRS : 0);
	}

	cvs_file_walklist(&fl, cr);
	cvs_file_freelist(&fl);

	if (current_cvsroot->cr_method == CVS_METHOD_LOCAL &&
	    (cmdp->cmd_flags & CVS_LOCK_REPO))
		cvs_repository_unlock(repo);

	if (cvs_directory_tag != NULL && cmdp->cmd_flags & CVS_USE_WDIR) {
		cvs_write_tagfile(cf->file_path, cvs_directory_tag, NULL);
		xfree(cvs_directory_tag);
		cvs_directory_tag = NULL;
	}

	cvs_file_walklist(&dl, cr);
	cvs_file_freelist(&dl);

	if (cr->leavedir != NULL)
		cr->leavedir(cf);
}

void
cvs_file_freelist(struct cvs_flisthead *fl)
{
	struct cvs_filelist *f, *nxt;

	for (f = RB_MIN(cvs_flisthead, fl); f != NULL; f = nxt) {
		nxt = RB_NEXT(cvs_flisthead, fl, f);
		RB_REMOVE(cvs_flisthead, fl, f);
		xfree(f->file_path);
		xfree(f);
	}
}

void
cvs_file_classify(struct cvs_file *cf, const char *tag)
{
	size_t len;
	struct stat st;
	BUF *b1, *b2;
	int server_has_file, notag;
	int rflags, ismodified, rcsdead;
	CVSENTRIES *entlist = NULL;
	const char *state;
	char repo[MAXPATHLEN], rcsfile[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_file_classify(%s, %s)", cf->file_path,
	    (tag != NULL) ? tag : "none");

	if (!strcmp(cf->file_path, ".")) {
		cf->file_status = FILE_UPTODATE;
		return;
	}

	cvs_get_repository_path(cf->file_wd, repo, MAXPATHLEN);
	(void)xsnprintf(rcsfile, MAXPATHLEN, "%s/%s",
	    repo, cf->file_name);

	if (cf->file_type == CVS_FILE) {
		len = strlcat(rcsfile, RCS_FILE_EXT, MAXPATHLEN);
		if (len >= MAXPATHLEN)
			fatal("cvs_file_classify: truncation");
	}

	cf->file_rpath = xstrdup(rcsfile);

	if (cmdp->cmd_flags & CVS_USE_WDIR) {
		entlist = cvs_ent_open(cf->file_wd);
		cf->file_ent = cvs_ent_get(entlist, cf->file_name);
	} else
		cf->file_ent = NULL;

	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_tag != NULL && cvs_specified_tag == NULL)
			tag = cf->file_ent->ce_tag;

		if (cf->file_flags & FILE_ON_DISK &&
		    cf->file_ent->ce_type == CVS_ENT_FILE &&
		    cf->file_type == CVS_DIR && tag != NULL) {
			cf->file_status = FILE_SKIP;
			return;
		}

		if (cf->file_flags & FILE_ON_DISK &&
		    cf->file_ent->ce_type == CVS_ENT_DIR &&
		    cf->file_type == CVS_FILE && tag != NULL) {
			cf->file_status = FILE_SKIP;
			return;
		}

		if (cf->file_flags & FILE_INSIDE_ATTIC &&
		    cf->file_ent->ce_type == CVS_ENT_DIR &&
		    cf->file_type != CVS_DIR) {
			cf->file_status = FILE_SKIP;
			return;
		}

		if (cf->file_flags & FILE_ON_DISK &&
		    cf->file_ent->ce_type == CVS_ENT_DIR &&
		    cf->file_type != CVS_DIR)
			fatal("%s is supposed to be a directory, but it is not",
			    cf->file_path);
		if (cf->file_flags & FILE_ON_DISK &&
		    cf->file_ent->ce_type == CVS_ENT_FILE &&
		    cf->file_type != CVS_FILE)
			fatal("%s is supposed to be a file, but it is not",
			    cf->file_path);
	}

	if (cf->file_type == CVS_DIR) {
		if (!(cmdp->cmd_flags & CVS_USE_WDIR))
			cf->file_status = FILE_UPTODATE;
		else if (cf->fd == -1 && stat(rcsfile, &st) != -1)
			cf->file_status = DIR_CREATE;
		else if (cf->file_ent != NULL || cvs_cmdop == CVS_OP_RLOG ||
		    cvs_cmdop == CVS_OP_RTAG)
			cf->file_status = FILE_UPTODATE;
		else
			cf->file_status = FILE_UNKNOWN;

		return;
	}

	rflags = RCS_READ;
	switch (cvs_cmdop) {
	case CVS_OP_COMMIT:
	case CVS_OP_TAG:
	case CVS_OP_RTAG:
		rflags = RCS_WRITE;
		break;
	case CVS_OP_ADMIN:
	case CVS_OP_IMPORT:
	case CVS_OP_LOG:
	case CVS_OP_RLOG:
		rflags |= RCS_PARSE_FULLY;
		break;
	default:
		break;
	}

	cf->repo_fd = open(cf->file_rpath, O_RDONLY);
	if (cf->repo_fd != -1) {
		cf->file_rcs = rcs_open(cf->file_rpath, cf->repo_fd, rflags);
		if (cf->file_rcs == NULL)
			fatal("cvs_file_classify: failed to parse RCS");
	} else {
		(void)xsnprintf(rcsfile, MAXPATHLEN, "%s/%s/%s%s",
		     repo, CVS_PATH_ATTIC, cf->file_name, RCS_FILE_EXT);

		cf->repo_fd = open(rcsfile, O_RDONLY);
		if (cf->repo_fd != -1) {
			xfree(cf->file_rpath);
			cf->file_rpath = xstrdup(rcsfile);
			cf->file_rcs = rcs_open(cf->file_rpath,
			    cf->repo_fd, rflags);
			if (cf->file_rcs == NULL)
				fatal("cvs_file_classify: failed to parse RCS");
			cf->in_attic = 1;
		} else {
			cf->file_rcs = NULL;
		}
	}

	notag = 0;
	cf->file_flags |= FILE_HAS_TAG;
	if (tag != NULL && cf->file_rcs != NULL) {
		if ((cf->file_rcsrev = rcs_translate_tag(tag, cf->file_rcs))
		    == NULL) {
			cf->file_rcsrev = rcs_translate_tag(NULL, cf->file_rcs);
			if (cf->file_rcsrev != NULL) {
				notag = 1;
				cf->file_flags &= ~FILE_HAS_TAG;
			}
		}
	} else if (cf->file_ent != NULL && cf->file_ent->ce_tag != NULL) {
		cf->file_rcsrev = rcsnum_alloc();
		rcsnum_cpy(cf->file_ent->ce_rev, cf->file_rcsrev, 0);
	} else if (cf->file_rcs != NULL) {
		cf->file_rcsrev = rcs_translate_tag(NULL, cf->file_rcs);
	} else {
		cf->file_rcsrev = NULL;
	}

	ismodified = rcsdead = 0;
	if ((cf->file_flags & FILE_ON_DISK) && cf->file_ent != NULL) {
		if (fstat(cf->fd, &st) == -1)
			fatal("cvs_file_classify: %s", strerror(errno));

		if (st.st_mtime != cf->file_ent->ce_mtime)
			ismodified = 1;
	}

	server_has_file = 0;
	if (cvs_server_active == 1 && cf->file_ent != NULL &&
	    cf->file_ent->ce_mtime == CVS_SERVER_UPTODATE) {
		server_has_file = 1;
		ismodified = 0;
	}

	if ((server_has_file == 1) || (cf->fd != -1))
		cf->file_flags |= FILE_ON_DISK;

	if (ismodified == 1 &&
	    (cf->file_flags & FILE_ON_DISK) && cf->file_rcs != NULL &&
	    cf->file_ent != NULL && !RCSNUM_ISBRANCH(cf->file_ent->ce_rev) &&
	    cf->file_ent->ce_status != CVS_ENT_ADDED) {
		b1 = rcs_rev_getbuf(cf->file_rcs, cf->file_ent->ce_rev, 0);
		b2 = buf_load_fd(cf->fd);

		if (buf_differ(b1, b2))
			ismodified = 1;
		else
			ismodified = 0;
		buf_free(b1);
		buf_free(b2);
	}

	if (cf->file_rcs != NULL && cf->file_rcsrev != NULL &&
	    !RCSNUM_ISBRANCH(cf->file_rcsrev)) {
		state = rcs_state_get(cf->file_rcs, cf->file_rcsrev);
		if (state == NULL)
			fatal("failed to get state for HEAD for %s",
			    cf->file_path);
		if (!strcmp(state, RCS_STATE_DEAD))
			rcsdead = 1;

		if (cvs_specified_date == -1 && cvs_directory_date == -1 &&
		    tag == NULL && cf->in_attic &&
		    !RCSNUM_ISBRANCHREV(cf->file_rcsrev))
			rcsdead = 1;

		cf->file_rcs->rf_dead = rcsdead;
	}

	/*
	 * 10 Sin
	 * 20 Goto hell
	 * (I welcome you if-else hell)
	 */
	if (cf->file_ent == NULL) {
		if (cf->file_rcs == NULL) {
			if (!(cf->file_flags & FILE_ON_DISK)) {
				cvs_log(LP_NOTICE,
				    "nothing known about '%s'",
				    cf->file_path);
			}

			cf->file_status = FILE_UNKNOWN;
		} else if (rcsdead == 1 || !(cf->file_flags & FILE_HAS_TAG)) {
			if (!(cf->file_flags & FILE_ON_DISK)) {
				cf->file_status = FILE_UPTODATE;
			} else if (cvs_cmdop != CVS_OP_ADD) {
				cf->file_status = FILE_UNKNOWN;
			}
		} else if (notag == 0 && cf->file_rcsrev != NULL) {
			cf->file_status = FILE_CHECKOUT;
		} else {
			cf->file_status = FILE_UPTODATE;
		}

		return;
	}

	switch (cf->file_ent->ce_status) {
	case CVS_ENT_ADDED:
		if (!(cf->file_flags & FILE_ON_DISK)) {
			if (cvs_cmdop != CVS_OP_REMOVE) {
				cvs_log(LP_NOTICE,
				    "warning: new-born %s has disappeared",
				    cf->file_path);
			}
			cf->file_status = FILE_REMOVE_ENTRY;
		} else if (cf->file_rcs == NULL || rcsdead == 1 ||
		    !(cf->file_flags & FILE_HAS_TAG)) {
			cf->file_status = FILE_ADDED;
		} else {
			cvs_log(LP_NOTICE,
			    "conflict: %s already created by others",
			    cf->file_path);
			cf->file_status = FILE_CONFLICT;
		}
		break;
	case CVS_ENT_REMOVED:
		if (cf->file_flags & FILE_ON_DISK) {
			cvs_log(LP_NOTICE,
			    "%s should be removed but is still there",
			    cf->file_path);
			cf->file_status = FILE_REMOVED;
		} else if (cf->file_rcs == NULL || rcsdead == 1) {
			cf->file_status = FILE_REMOVE_ENTRY;
		} else {
			if (rcsnum_differ(cf->file_ent->ce_rev,
			    cf->file_rcsrev) && cvs_cmdop != CVS_OP_ADD) {
				cvs_log(LP_NOTICE,
				    "conflict: removed %s was modified"
				    " by a second party",
				    cf->file_path);
				cf->file_status = FILE_CONFLICT;
			} else {
				cf->file_status = FILE_REMOVED;
			}
		}
		break;
	case CVS_ENT_REG:
		if (cf->file_rcs == NULL || cf->file_rcsrev == NULL ||
		    rcsdead == 1 || (reset_tag == 1 && cf->in_attic == 1) ||
		    (notag == 1 && tag != NULL)) {
			if (!(cf->file_flags & FILE_ON_DISK)) {
				cvs_log(LP_NOTICE,
				    "warning: %s's entry exists but"
				    " is no longer in the repository,"
				    " removing entry",
				     cf->file_path);
				cf->file_status = FILE_REMOVE_ENTRY;
			} else {
				if (ismodified) {
					cvs_log(LP_NOTICE,
					    "conflict: %s is no longer "
					    "in the repository but is "
					    "locally modified",
					    cf->file_path);
					if (cvs_cmdop == CVS_OP_COMMIT)
						cf->file_status = FILE_UNLINK;
					else
						cf->file_status = FILE_CONFLICT;
				} else if (cvs_cmdop != CVS_OP_IMPORT) {
					cvs_log(LP_NOTICE,
					    "%s is no longer in the "
					    "repository",
					    cf->file_path);

					cf->file_status = FILE_UNLINK;
				}
			}
		} else if (cf->file_rcsrev == NULL) {
			cf->file_status = FILE_UNLINK;
		} else {
			if (!(cf->file_flags & FILE_ON_DISK)) {
				if (cvs_cmdop != CVS_OP_REMOVE) {
					cvs_log(LP_NOTICE,
					    "warning: %s was lost",
					    cf->file_path);
				}
				cf->file_status = FILE_LOST;
			} else {
				if (ismodified == 1)
					cf->file_status = FILE_MODIFIED;
				else
					cf->file_status = FILE_UPTODATE;
				if (rcsnum_differ(cf->file_ent->ce_rev,
				    cf->file_rcsrev)) {
					if (cf->file_status == FILE_MODIFIED)
						cf->file_status = FILE_MERGE;
					else
						cf->file_status = FILE_PATCH;
				}
			}
		}
		break;
	case CVS_ENT_UNKNOWN:
		if (cvs_server_active != 1)
			fatal("server-side questionable in local mode?");
		cf->file_status = FILE_UNKNOWN;
		break;
	default:
		break;
	}
}

void
cvs_file_free(struct cvs_file *cf)
{
	xfree(cf->file_name);
	xfree(cf->file_wd);
	xfree(cf->file_path);

	if (cf->file_rcsrev != NULL)
		rcsnum_free(cf->file_rcsrev);
	if (cf->file_rpath != NULL)
		xfree(cf->file_rpath);
	if (cf->file_ent != NULL)
		cvs_ent_free(cf->file_ent);
	if (cf->file_rcs != NULL)
		rcs_close(cf->file_rcs);
	if (cf->fd != -1)
		(void)close(cf->fd);
	if (cf->repo_fd != -1)
		(void)close(cf->repo_fd);
	xfree(cf);
}

int
cvs_file_cmpname(const char *name1, const char *name2)
{
	return (cvs_nocase == 0) ? (strcmp(name1, name2)) :
	    (strcasecmp(name1, name2));
}

int
cvs_file_cmp(const char *file1, const char *file2)
{
	struct stat stb1, stb2;
	int fd1, fd2, ret;

	ret = 0;

	if ((fd1 = open(file1, O_RDONLY|O_NOFOLLOW, 0)) == -1)
		fatal("cvs_file_cmp: open: `%s': %s", file1, strerror(errno));
	if ((fd2 = open(file2, O_RDONLY|O_NOFOLLOW, 0)) == -1)
		fatal("cvs_file_cmp: open: `%s': %s", file2, strerror(errno));

	if (fstat(fd1, &stb1) == -1)
		fatal("cvs_file_cmp: `%s': %s", file1, strerror(errno));
	if (fstat(fd2, &stb2) == -1)
		fatal("cvs_file_cmp: `%s': %s", file2, strerror(errno));

	if (stb1.st_size != stb2.st_size ||
	    (stb1.st_mode & S_IFMT) != (stb2.st_mode & S_IFMT)) {
		ret = 1;
		goto out;
	}

	if (S_ISBLK(stb1.st_mode) || S_ISCHR(stb1.st_mode)) {
		if (stb1.st_rdev != stb2.st_rdev)
			ret = 1;
		goto out;
	}

	if (S_ISREG(stb1.st_mode)) {
		void *p1, *p2;

		if (stb1.st_size > SIZE_MAX) {
			ret = 1;
			goto out;
		}

		if ((p1 = mmap(NULL, stb1.st_size, PROT_READ,
		    MAP_FILE, fd1, (off_t)0)) == MAP_FAILED)
			fatal("cvs_file_cmp: mmap failed");

		if ((p2 = mmap(NULL, stb1.st_size, PROT_READ,
		    MAP_FILE, fd2, (off_t)0)) == MAP_FAILED)
			fatal("cvs_file_cmp: mmap failed");

		madvise(p1, stb1.st_size, MADV_SEQUENTIAL);
		madvise(p2, stb1.st_size, MADV_SEQUENTIAL);

		ret = memcmp(p1, p2, stb1.st_size);

		(void)munmap(p1, stb1.st_size);
		(void)munmap(p2, stb1.st_size);
	}

out:
	(void)close(fd1);
	(void)close(fd2);

	return (ret);
}

int
cvs_file_copy(const char *from, const char *to)
{
	struct stat st;
	struct timeval tv[2];
	time_t atime, mtime;
	int src, dst, ret;

	ret = 0;

	cvs_log(LP_TRACE, "cvs_file_copy(%s,%s)", from, to);

	if (cvs_noexec == 1)
		return (0);

	if ((src = open(from, O_RDONLY, 0)) == -1)
		fatal("cvs_file_copy: open: `%s': %s", from, strerror(errno));

	if (fstat(src, &st) == -1)
		fatal("cvs_file_copy: `%s': %s", from, strerror(errno));

	atime = st.st_atimespec.tv_sec;
	mtime = st.st_mtimespec.tv_sec;

	if (S_ISREG(st.st_mode)) {
		char *p;
		int saved_errno;

		if (st.st_size > SIZE_MAX) {
			ret = -1;
			goto out;
		}

		if ((dst = open(to, O_CREAT|O_TRUNC|O_WRONLY,
		    st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO))) == -1)
			fatal("cvs_file_copy: open `%s': %s",
			    to, strerror(errno));

		if ((p = mmap(NULL, st.st_size, PROT_READ,
		    MAP_FILE, src, (off_t)0)) == MAP_FAILED) {
			saved_errno = errno;
			(void)unlink(to);
			fatal("cvs_file_copy: mmap: %s", strerror(saved_errno));
		}

		madvise(p, st.st_size, MADV_SEQUENTIAL);

		if (atomicio(vwrite, dst, p, st.st_size) != st.st_size) {
			saved_errno = errno;
			(void)unlink(to);
			fatal("cvs_file_copy: `%s': %s", from,
			    strerror(saved_errno));
		}

		(void)munmap(p, st.st_size);

		tv[0].tv_sec = atime;
		tv[1].tv_sec = mtime;

		if (futimes(dst, tv) == -1) {
			saved_errno = errno;
			(void)unlink(to);
			fatal("cvs_file_copy: futimes: %s",
			    strerror(saved_errno));
		}
		(void)close(dst);
	}
out:
	(void)close(src);

	return (ret);
}

int
cvs_filelist_cmp(struct cvs_filelist *f1, struct cvs_filelist *f2)
{
	return (strcmp(f1->file_path, f2->file_path));
}
