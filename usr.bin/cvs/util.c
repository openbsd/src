/*	$OpenBSD: util.c,v 1.65 2006/01/01 05:05:58 joris Exp $	*/
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
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"

#if !defined(RCSPROG)

/* letter -> mode type map */
static const int cvs_modetypes[26] = {
	-1, -1, -1, -1, -1, -1,  1, -1, -1, -1, -1, -1, -1,
	-1,  2, -1, -1, -1, -1, -1,  0, -1, -1, -1, -1, -1,
};

/* letter -> mode map */
static const mode_t cvs_modes[3][26] = {
	{
		0,  0,       0,       0,       0,  0,  0,    /* a - g */
		0,  0,       0,       0,       0,  0,  0,    /* h - m */
		0,  0,       0,       S_IRUSR, 0,  0,  0,    /* n - u */
		0,  S_IWUSR, S_IXUSR, 0,       0             /* v - z */
	},
	{
		0,  0,       0,       0,       0,  0,  0,    /* a - g */
		0,  0,       0,       0,       0,  0,  0,    /* h - m */
		0,  0,       0,       S_IRGRP, 0,  0,  0,    /* n - u */
		0,  S_IWGRP, S_IXGRP, 0,       0             /* v - z */
	},
	{
		0,  0,       0,       0,       0,  0,  0,    /* a - g */
		0,  0,       0,       0,       0,  0,  0,    /* h - m */
		0,  0,       0,       S_IROTH, 0,  0,  0,    /* n - u */
		0,  S_IWOTH, S_IXOTH, 0,       0             /* v - z */
	}
};


/* octal -> string */
static const char *cvs_modestr[8] = {
	"", "x", "w", "wx", "r", "rx", "rw", "rwx"
};



/*
 * cvs_readrepo()
 *
 * Read the path stored in the `Repository' CVS file for a given directory
 * <dir>, and store that path into the buffer pointed to by <dst>, whose size
 * is <len>.
 */
int
cvs_readrepo(const char *dir, char *dst, size_t len)
{
	size_t dlen, l;
	FILE *fp;
	char repo_path[MAXPATHLEN];

	l = cvs_path_cat(dir, "CVS/Repository", repo_path, sizeof(repo_path));
	if (l >= sizeof(repo_path))
		return (-1);

	fp = fopen(repo_path, "r");
	if (fp == NULL)
		return (-1);

	if (fgets(dst, (int)len, fp) == NULL) {
		if (ferror(fp)) {
			cvs_log(LP_ERRNO, "failed to read from `%s'",
			    repo_path);
		}
		(void)fclose(fp);
		return (-1);
	}
	dlen = strlen(dst);
	if ((dlen > 0) && (dst[dlen - 1] == '\n'))
		dst[--dlen] = '\0';

	(void)fclose(fp);
	return (0);
}


/*
 * cvs_strtomode()
 *
 * Read the contents of the string <str> and generate a permission mode from
 * the contents of <str>, which is assumed to have the mode format of CVS.
 * The CVS protocol specification states that any modes or mode types that are
 * not recognized should be silently ignored.  This function does not return
 * an error in such cases, but will issue warnings.
 */
void
cvs_strtomode(const char *str, mode_t *mode)
{
	char type;
	size_t l;
	mode_t m;
	char buf[32], ms[4], *sp, *ep;

	m = 0;
	l = strlcpy(buf, str, sizeof(buf));
	if (l >= sizeof(buf))
		fatal("cvs_strtomode: string truncation");

	sp = buf;
	ep = sp;

	for (sp = buf; ep != NULL; sp = ep + 1) {
		ep = strchr(sp, ',');
		if (ep != NULL)
			*ep = '\0';

		memset(ms, 0, sizeof ms);
		if (sscanf(sp, "%c=%3s", &type, ms) != 2 &&
			sscanf(sp, "%c=", &type) != 1) {
			cvs_log(LP_WARN, "failed to scan mode string `%s'", sp);
			continue;
		}

		if ((type <= 'a') || (type >= 'z') ||
		    (cvs_modetypes[type - 'a'] == -1)) {
			cvs_log(LP_WARN,
			    "invalid mode type `%c'"
			    " (`u', `g' or `o' expected), ignoring", type);
			continue;
		}

		/* make type contain the actual mode index */
		type = cvs_modetypes[type - 'a'];

		for (sp = ms; *sp != '\0'; sp++) {
			if ((*sp <= 'a') || (*sp >= 'z') ||
			    (cvs_modes[(int)type][*sp - 'a'] == 0)) {
				cvs_log(LP_WARN,
				    "invalid permission bit `%c'", *sp);
			} else
				m |= cvs_modes[(int)type][*sp - 'a'];
		}
	}

	*mode = m;
}


/*
 * cvs_modetostr()
 *
 * Generate a CVS-format string to represent the permissions mask on a file
 * from the mode <mode> and store the result in <buf>, which can accept up to
 * <len> bytes (including the terminating NUL byte).  The result is guaranteed
 * to be NUL-terminated.
 */
void
cvs_modetostr(mode_t mode, char *buf, size_t len)
{
	int l;
	size_t l1;
	char tmp[16], *bp;
	mode_t um, gm, om;

	um = (mode & S_IRWXU) >> 6;
	gm = (mode & S_IRWXG) >> 3;
	om = mode & S_IRWXO;

	bp = buf;
	*bp = '\0';

	if (um) {
		l = snprintf(tmp, sizeof(tmp), "u=%s", cvs_modestr[um]);
		if (l == -1 || l >= (int)sizeof(tmp))
			fatal("cvs_modetostr: overflow for user mode");

		l1 = strlcat(buf, tmp, len);
		if (l1 >= len)
			fatal("cvs_modetostr: string truncation");
	}

	if (gm) {
		if (um) {
			l1 = strlcat(buf, ",", len);
			if (l1 >= len)
				fatal("cvs_modetostr: string truncation");
		}

		l = snprintf(tmp, sizeof(tmp), "g=%s", cvs_modestr[gm]);
		if (l == -1 || l >= (int)sizeof(tmp))
			fatal("cvs_modetostr: overflow for group mode");

		l1 = strlcat(buf, tmp, len);
		if (l1 >= len)
			fatal("cvs_modetostr: string truncation");
	}

	if (om) {
		if (um || gm) {
			l1 = strlcat(buf, ",", len);
			if (l1 >= len)
				fatal("cvs_modetostr: string truncation");
		}

		l = snprintf(tmp, sizeof(tmp), "o=%s", cvs_modestr[gm]);
		if (l == -1 || l >= (int)sizeof(tmp))
			fatal("cvs_modetostr: overflow for others mode");

		l1 = strlcat(buf, tmp, len);
		if (l1 >= len)
			fatal("cvs_modetostr: string truncation");
	}
}

/*
 * cvs_cksum()
 *
 * Calculate the MD5 checksum of the file whose path is <file> and generate
 * a CVS-format 32 hex-digit string, which is stored in <dst>, whose size is
 * given in <len> and must be at least 33.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_cksum(const char *file, char *dst, size_t len)
{
	if (len < CVS_CKSUM_LEN) {
		cvs_log(LP_WARN, "buffer too small for checksum");
		return (-1);
	}
	if (MD5File(file, dst) == NULL) {
		cvs_log(LP_ERRNO, "failed to generate checksum for %s", file);
		return (-1);
	}

	return (0);
}

/*
 * cvs_splitpath()
 *
 * Split a path <path> into the base portion and the filename portion.
 * The path is copied in <base> and the last delimiter is replaced by a NUL
 * byte.  The <file> pointer is set to point to the first character after
 * that delimiter.
 * Returns 0 on success, or -1 on failure.
 */
void
cvs_splitpath(const char *path, char *base, size_t blen, char **file)
{
	size_t rlen;
	char *sp;

	if ((rlen = strlcpy(base, path, blen)) >= blen)
		fatal("cvs_splitpath: path truncation");

	while ((rlen > 0) && (base[rlen - 1] == '/'))
		base[--rlen] = '\0';

	sp = strrchr(base, '/');
	if (sp == NULL) {
		rlen = strlcpy(base, "./", blen);
		if (rlen >= blen)
			fatal("cvs_splitpath: path truncation");

		rlen = strlcat(base, path, blen);
		if (rlen >= blen)
			fatal("cvs_splitpath: path truncation");

		sp = base + 1;
	}

	*sp = '\0';
	if (file != NULL)
		*file = sp + 1;
}

/*
 * cvs_getargv()
 *
 * Parse a line contained in <line> and generate an argument vector by
 * splitting the line on spaces and tabs.  The resulting vector is stored in
 * <argv>, which can accept up to <argvlen> entries.
 * Returns the number of arguments in the vector, or -1 if an error occurred.
 */
int
cvs_getargv(const char *line, char **argv, int argvlen)
{
	size_t l;
	u_int i;
	int argc, err;
	char linebuf[256], qbuf[128], *lp, *cp, *arg;

	l = strlcpy(linebuf, line, sizeof(linebuf));
	if (l >= sizeof(linebuf))
		fatal("cvs_getargv: string truncation");

	memset(argv, 0, argvlen * sizeof(char *));
	argc = 0;

	/* build the argument vector */
	err = 0;
	for (lp = linebuf; lp != NULL;) {
		if (*lp == '"') {
			/* double-quoted string */
			lp++;
			i = 0;
			memset(qbuf, 0, sizeof(qbuf));
			while (*lp != '"') {
				if (*lp == '\\')
					lp++;
				if (*lp == '\0') {
					cvs_log(LP_ERR, "no terminating quote");
					err++;
					break;
				}

				qbuf[i++] = *lp++;
				if (i == sizeof(qbuf)) {
					err++;
					break;
				}
			}

			arg = qbuf;
		} else {
			cp = strsep(&lp, " \t");
			if (cp == NULL)
				break;
			else if (*cp == '\0')
				continue;

			arg = cp;
		}

		if (argc == argvlen) {
			err++;
			break;
		}

		argv[argc] = xstrdup(arg);
		argc++;
	}

	if (err != 0) {
		/* ditch the argument vector */
		for (i = 0; i < (u_int)argc; i++)
			xfree(argv[i]);
		argc = -1;
	}

	return (argc);
}


/*
 * cvs_makeargv()
 *
 * Allocate an argument vector large enough to accommodate for all the
 * arguments found in <line> and return it.
 */
char **
cvs_makeargv(const char *line, int *argc)
{
	int i, ret;
	char *argv[1024], **copy;
	size_t size;

	ret = cvs_getargv(line, argv, 1024);
	if (ret == -1)
		return (NULL);

	size = (ret + 1) * sizeof(char *);
	copy = (char **)xmalloc(size);
	memset(copy, 0, size);

	for (i = 0; i < ret; i++)
		copy[i] = argv[i];
	copy[ret] = NULL;

	*argc = ret;
	return (copy);
}


/*
 * cvs_freeargv()
 *
 * Free an argument vector previously generated by cvs_getargv().
 */
void
cvs_freeargv(char **argv, int argc)
{
	int i;

	for (i = 0; i < argc; i++)
		if (argv[i] != NULL)
			xfree(argv[i]);
}


/*
 * cvs_mkadmin()
 *
 * Create the CVS administrative files within the directory <cdir>.  If the
 * files already exist, they are kept as is.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_mkadmin(const char *dpath, const char *rootpath, const char *repopath,
    char *tag, char *date, int nb)
{
	size_t l;
	char path[MAXPATHLEN];
	FILE *fp;
	CVSENTRIES *ef;
	struct stat st;

	cvs_log(LP_TRACE, "cvs_mkadmin(%s, %s, %s, %s, %s, %d)",
	    dpath, rootpath, repopath, tag ? tag : "", date ? date : "", nb);

	l = cvs_path_cat(dpath, CVS_PATH_CVSDIR, path, sizeof(path));
	if (l >= sizeof(path))
		fatal("cvs_mkadmin: path truncation");

	if ((mkdir(path, 0755) == -1) && (errno != EEXIST))
		fatal("cvs_mkadmin: mkdir: `%s': %s", path, strerror(errno));

	/* just create an empty Entries file */
	ef = cvs_ent_open(dpath, O_WRONLY);
	if (ef != NULL)
		cvs_ent_close(ef);

	l = cvs_path_cat(dpath, CVS_PATH_ROOTSPEC, path, sizeof(path));
	if (l >= sizeof(path))
		fatal("cvs_mkadmin: path truncation");

	if ((stat(path, &st) == -1) && (errno == ENOENT)) {
		if ((fp = fopen(path, "w")) == NULL)
			fatal("cvs_mkadmin: fopen: `%s': %s",
			    path, strerror(errno));

		if (rootpath != NULL)
			fprintf(fp, "%s\n", rootpath);
		(void)fclose(fp);
	}

	l = cvs_path_cat(dpath, CVS_PATH_REPOSITORY, path, sizeof(path));
	if (l >= sizeof(path))
		fatal("cvs_mkadmin: path truncation");

	if ((stat(path, &st) == -1) && (errno == ENOENT)) {
		if ((fp = fopen(path, "w")) == NULL)
			fatal("cvs_mkadmin: fopen: `%s': %s",
			    path, strerror(errno));

		if (repopath != NULL)
			fprintf(fp, "%s\n", repopath);
		(void)fclose(fp);
	}

	/* create CVS/Tag file (if needed) */
	/* XXX correct? */
	if (tag != NULL || date != NULL)
		(void)cvs_write_tagfile(tag, date, nb);

	return (0);
}


/*
 * cvs_exec()
 */
int
cvs_exec(int argc, char **argv, int fds[3])
{
	int ret;
	pid_t pid;

	if ((pid = fork()) == -1) {
		cvs_log(LP_ERRNO, "failed to fork");
		return (-1);
	} else if (pid == 0) {
		execvp(argv[0], argv);
		cvs_log(LP_ERRNO, "failed to exec %s", argv[0]);
		exit(1);
	}

	if (waitpid(pid, &ret, 0) == -1)
		cvs_log(LP_ERRNO, "failed to waitpid");

	return (ret);
}

/*
 * cvs_chdir()
 *
 * Change to directory <path>.
 * If <rm> is equal to `1', <path> is removed if chdir() fails so we
 * do not have temporary directories leftovers.
 * Returns 0 on success.
 */
int
cvs_chdir(const char *path, int rm)
{
	if (chdir(path) == -1) {
		if (rm == 1)
			cvs_unlink(path);
		fatal("cvs_chdir: `%s': %s", path, strerror(errno));
	}

	return (0);
}

/*
 * cvs_rename()
 * Change the name of a file.
 * rename() wrapper with an error message.
 * Returns 0 on success.
 */
int
cvs_rename(const char *from, const char *to)
{
	cvs_log(LP_TRACE, "cvs_rename(%s,%s)", from, to);

	if (cvs_noexec == 1)
		return (0);

	if (rename(from, to) == -1)
		fatal("cvs_rename: `%s'->`%s': %s", from, to, strerror(errno));

	return (0);
}

/*
 * cvs_unlink()
 *
 * Removes the link named by <path>.
 * unlink() wrapper with an error message.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_unlink(const char *path)
{
	cvs_log(LP_TRACE, "cvs_unlink(%s)", path);

	if (cvs_noexec == 1)
		return (0);

	if ((unlink(path) == -1) && (errno != ENOENT)) {
		cvs_log(LP_ERRNO, "cannot remove `%s'", path);
		return (-1);
	}

	return (0);
}

/*
 * cvs_rmdir()
 *
 * Remove a directory tree from disk.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_rmdir(const char *path)
{
	int ret = -1;
	size_t len;
	DIR *dirp;
	struct dirent *ent;
	char fpath[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_rmdir(%s)", path);

	if (cvs_noexec == 1)
		return (0);

	if ((dirp = opendir(path)) == NULL) {
		cvs_log(LP_ERRNO, "failed to open '%s'", path);
		return (-1);
	}

	while ((ent = readdir(dirp)) != NULL) {
		if (!strcmp(ent->d_name, ".") ||
		    !strcmp(ent->d_name, ".."))
			continue;

		len = cvs_path_cat(path, ent->d_name, fpath, sizeof(fpath));
		if (len >= sizeof(fpath))
			fatal("cvs_rmdir: path truncation");

		if (ent->d_type == DT_DIR) {
			if (cvs_rmdir(fpath) == -1)
				goto done;
		} else if ((cvs_unlink(fpath) == -1) && (errno != ENOENT))
			goto done;
	}


	if ((rmdir(path) == -1) && (errno != ENOENT)) {
		cvs_log(LP_ERRNO, "failed to remove '%s'", path);
		goto done;
	}

	ret = 0;
done:
	closedir(dirp);
	return (ret);
}

/*
 * Create a directory, and the parent directories if needed.
 * based upon mkpath() from mkdir.c
 */
int
cvs_create_dir(const char *path, int create_adm, char *root, char *repo)
{
	size_t l;
	int len, ret;
	char *d, *s;
	struct stat sb;
	char rpath[MAXPATHLEN], entry[MAXPATHLEN];
	CVSENTRIES *entf;
	struct cvs_ent *ent;

	if ((create_adm == 1) && (root == NULL))
		fatal("cvs_create_dir failed");

	s = xstrdup(path);
	rpath[0] = '\0';
	if (repo != NULL) {
		if (strlcpy(rpath, repo, sizeof(rpath)) >= sizeof(rpath))
			fatal("cvs_create_dir: path truncation");

		if (strlcat(rpath, "/", sizeof(rpath)) >= sizeof(rpath))
			fatal("cvs_create_dir: path truncation");
	}

	ret = -1;
	entf = NULL;
	d = strtok(s, "/");
	while (d != NULL) {
		if (stat(d, &sb)) {
			/* try to create the directory */
			if ((errno != ENOENT) || (mkdir(d, 0755) &&
			    errno != EEXIST)) {
				cvs_log(LP_ERRNO, "failed to create `%s'", d);
				goto done;
			}
		} else if (!S_ISDIR(sb.st_mode)) {
			cvs_log(LP_ERR, "`%s' not a directory", d);
			goto done;
		}

		/*
		 * Create administrative files if requested.
		 */
		if (create_adm == 1) {
			l = strlcat(rpath, d, sizeof(rpath));
			if (l >= sizeof(rpath))
				fatal("cvs_create_dir: path truncation");

			l = strlcat(rpath, "/", sizeof(rpath));
			if (l >= sizeof(rpath))
				fatal("cvs_create_dir: path truncation");

			if (cvs_mkadmin(d, root, rpath, NULL, NULL, 0) < 0) {
				cvs_log(LP_ERR, "failed to create adm files");
				goto done;
			}
		}

		/*
		 * Add it to the parent directory entry file.
		 * (if any).
		 */
		entf = cvs_ent_open(".", O_RDWR);
		if (entf != NULL && strcmp(d, ".")) {
			len = snprintf(entry, sizeof(entry), "D/%s////", d);
			if (len == -1 || len >= (int)sizeof(entry))
				fatal("cvs_create_dir: overflow in entry buf");

			if ((ent = cvs_ent_parse(entry)) == NULL) {
				cvs_log(LP_ERR, "failed to parse entry");
				goto done;
			}

			cvs_ent_remove(entf, d, 0);

			if (cvs_ent_add(entf, ent) < 0) {
				cvs_log(LP_ERR, "failed to add entry");
				goto done;
			}
		}

		if (entf != NULL) {
			cvs_ent_close(entf);
			entf = NULL;
		}

		/* All went ok, switch to the newly created directory. */
		cvs_chdir(d, 0);

		d = strtok(NULL, "/");
	}

	ret = 0;
done:
	if (entf != NULL)
		cvs_ent_close(entf);
	xfree(s);
	return (ret);
}

/*
 * cvs_path_cat()
 *
 * Concatenate the two paths <base> and <end> and store the generated path
 * into the buffer <dst>, which can accept up to <dlen> bytes, including the
 * NUL byte.  The result is guaranteed to be NUL-terminated.
 * Returns the number of bytes necessary to store the full resulting path,
 * not including the NUL byte (a value equal to or larger than <dlen>
 * indicates truncation).
 */
size_t
cvs_path_cat(const char *base, const char *end, char *dst, size_t dlen)
{
	size_t len;

	len = strlcpy(dst, base, dlen);
	if (len >= dlen - 1) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", dst);
	} else {
		dst[len] = '/';
		dst[len + 1] = '\0';
		len = strlcat(dst, end, dlen);
		if (len >= dlen) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", dst);
		}
	}

	return (len);
}

/*
 * cvs_rcs_getpath()
 *
 * Get the RCS path of the file <file> and store it in <buf>, which is
 * of size <len>. For portability, it is recommended that <buf> always be
 * at least MAXPATHLEN bytes long.
 * Returns a pointer to the start of the path on success, or NULL on failure.
 */
char *
cvs_rcs_getpath(CVSFILE *file, char *buf, size_t len)
{
	int l;
	char *repo;
	struct cvsroot *root;

	root = CVS_DIR_ROOT(file);
	repo = CVS_DIR_REPO(file);

	l = snprintf(buf, len, "%s/%s/%s%s",
	    root->cr_dir, repo, file->cf_name, RCS_FILE_EXT);
	if (l == -1 || l >= (int)len)
		fatal("cvs_rcs_getpath: path truncation");

	return (buf);
}

/*
 * cvs_write_tagfile()
 *
 * Write the CVS/Tag file for current directory.
 */
void
cvs_write_tagfile(char *tag, char *date, int nb)
{
	FILE *fp;
	char tagpath[MAXPATHLEN];

	if (cvs_noexec == 1)
		return;

	if (strlcpy(tagpath, CVS_PATH_TAG, sizeof(tagpath)) >= sizeof(tagpath))
		return;

	if ((tag != NULL) || (date != NULL)) {
		fp = fopen(tagpath, "w+");
		if (fp == NULL) {
			if (errno != ENOENT)
				cvs_log(LP_NOTICE,
				    "failed to open `%s' : %s", tagpath,
				    strerror(errno));
			return;
		}
		if (tag != NULL) {
			if (nb != 0)
				fprintf(fp, "N%s\n", tag);
			else
				fprintf(fp, "T%s\n", tag);
		} else {
			fprintf(fp, "D%s\n", date);
		}
		(void)fclose(fp);
	} else {
		cvs_unlink(tagpath);
		return;
	}
}

/*
 * cvs_parse_tagfile()
 *
 * Parse the CVS/Tag file for current directory.
 *
 * If it contains a branch tag, sets <tagp>.
 * If it contains a date, sets <datep>.
 * If it contains a non-branch tag, sets <nbp>.
 *
 * Returns nothing but an error message, and sets <tagp>, <datep> to NULL
 * and <nbp> to 0.
 */
void
cvs_parse_tagfile(char **tagp, char **datep, int *nbp)
{
	FILE *fp;
	int linenum;
	size_t len;
	char linebuf[128], tagpath[MAXPATHLEN];

	if (tagp != NULL)
		*tagp = (char *)NULL;

	if (datep != NULL)
		*datep = (char *)NULL;

	if (nbp != NULL)
		*nbp = 0;

	if (strlcpy(tagpath, CVS_PATH_TAG, sizeof(tagpath)) >= sizeof(tagpath))
		return;

	fp = fopen(tagpath, "r");
	if (fp == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_NOTICE, "failed to open `%s' : %s", tagpath,
			    strerror(errno));
		return;
	}

	linenum = 0;

	while (fgets(linebuf, (int)sizeof(linebuf), fp) != NULL) {
		linenum++;
		if ((len = strlen(linebuf)) == 0)
			continue;
		if (linebuf[len -1] != '\n') {
			cvs_log(LP_WARN, "line too long in `%s:%d'", tagpath,
			    linenum);
			break;
		}
		linebuf[--len] = '\0';

		switch (*linebuf) {
		case 'T':
			if (tagp != NULL)
				*tagp = xstrdup(linebuf);
			break;
		case 'D':
			if (datep != NULL)
				*datep = xstrdup(linebuf);
			break;
		case 'N':
			if (tagp != NULL)
				*tagp = xstrdup(linebuf);
			if (nbp != NULL)
				*nbp = 1;
			break;
		default:
			break;
		}
	}
	if (ferror(fp))
		cvs_log(LP_NOTICE, "failed to read line from `%s'", tagpath);

	(void)fclose(fp);
}

#endif	/* !RCSPROG */

/*
 * Split the contents of a file into a list of lines.
 */
struct cvs_lines *
cvs_splitlines(const char *fcont)
{
	char *dcp;
	struct cvs_lines *lines;
	struct cvs_line *lp;

	lines = (struct cvs_lines *)xmalloc(sizeof(*lines));
	TAILQ_INIT(&(lines->l_lines));
	lines->l_nblines = 0;
	lines->l_data = xstrdup(fcont);

	lp = (struct cvs_line *)xmalloc(sizeof(*lp));
	lp->l_line = NULL;
	lp->l_lineno = 0;
	TAILQ_INSERT_TAIL(&(lines->l_lines), lp, l_list);

	for (dcp = lines->l_data; *dcp != '\0';) {
		lp = (struct cvs_line *)xmalloc(sizeof(*lp));
		lp->l_line = dcp;
		lp->l_lineno = ++(lines->l_nblines);
		TAILQ_INSERT_TAIL(&(lines->l_lines), lp, l_list);

		dcp = strchr(dcp, '\n');
		if (dcp == NULL)
			break;
		*(dcp++) = '\0';
	}

	return (lines);
}

void
cvs_freelines(struct cvs_lines *lines)
{
	struct cvs_line *lp;

	while ((lp = TAILQ_FIRST(&(lines->l_lines))) != NULL) {
		TAILQ_REMOVE(&(lines->l_lines), lp, l_list);
		xfree(lp);
	}

	xfree(lines->l_data);
	xfree(lines);
}

BUF *
cvs_patchfile(const char *data, const char *patch,
    int (*p)(struct cvs_lines *, struct cvs_lines *))
{
	struct cvs_lines *dlines, *plines;
	struct cvs_line *lp;
	size_t len;
	int lineno;
	BUF *res;

	len = strlen(data);

	if ((dlines = cvs_splitlines(data)) == NULL)
		return (NULL);

	if ((plines = cvs_splitlines(patch)) == NULL)
		return (NULL);

	if (p(dlines, plines) < 0) {
		cvs_freelines(dlines);
		cvs_freelines(plines);
		return (NULL);
	}

	lineno = 0;
	res = cvs_buf_alloc(len, BUF_AUTOEXT);
	TAILQ_FOREACH(lp, &dlines->l_lines, l_list) {
		if (lineno != 0)
			cvs_buf_fappend(res, "%s\n", lp->l_line);
		lineno++;
	}

	cvs_freelines(dlines);
	cvs_freelines(plines);
	return (res);
}

/*
 * a hack to mimic and thus match gnu cvs behaviour.
 */
time_t
cvs_hack_time(time_t oldtime, int togmt)
{
	int l;
	struct tm *t;
	char tbuf[32];

	if (togmt == 1) {
		t = gmtime(&oldtime);
		if (t == NULL)
			return (0);

		return (mktime(t));
	}

	t = localtime(&oldtime);

	l = snprintf(tbuf, sizeof(tbuf), "%d/%d/%d GMT %d:%d:%d",
	    t->tm_mon + 1, t->tm_mday, t->tm_year + 1900, t->tm_hour,
	    t->tm_min, t->tm_sec);
	if (l == -1 || l >= (int)sizeof(tbuf))
		return (0);

	return (cvs_date_parse(tbuf));
}
