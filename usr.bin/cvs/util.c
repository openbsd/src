/*	$OpenBSD: util.c,v 1.113 2007/07/19 06:34:15 ray Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2005, 2006 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
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

#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <md5.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

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
			fatal("failed to scan mode string `%s'", sp);
		}

		if (type <= 'a' || type >= 'z' ||
		    cvs_modetypes[type - 'a'] == -1) {
			cvs_log(LP_ERR,
			    "invalid mode type `%c'"
			    " (`u', `g' or `o' expected), ignoring", type);
			continue;
		}

		/* make type contain the actual mode index */
		type = cvs_modetypes[type - 'a'];

		for (sp = ms; *sp != '\0'; sp++) {
			if (*sp <= 'a' || *sp >= 'z' ||
			    cvs_modes[(int)type][*sp - 'a'] == 0) {
				fatal("invalid permission bit `%c'", *sp);
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
	char tmp[16], *bp;
	mode_t um, gm, om;

	um = (mode & S_IRWXU) >> 6;
	gm = (mode & S_IRWXG) >> 3;
	om = mode & S_IRWXO;

	bp = buf;
	*bp = '\0';

	if (um) {
		if (strlcpy(tmp, "u=", sizeof(tmp)) >= sizeof(tmp) ||
		    strlcat(tmp, cvs_modestr[um], sizeof(tmp)) >= sizeof(tmp))
			fatal("cvs_modetostr: overflow for user mode");

		if (strlcat(buf, tmp, len) >= len)
			fatal("cvs_modetostr: string truncation");
	}

	if (gm) {
		if (um) {
			if (strlcat(buf, ",", len) >= len)
				fatal("cvs_modetostr: string truncation");
		}

		if (strlcpy(tmp, "g=", sizeof(tmp)) >= sizeof(tmp) ||
		    strlcat(tmp, cvs_modestr[gm], sizeof(tmp)) >= sizeof(tmp))
			fatal("cvs_modetostr: overflow for group mode");

		if (strlcat(buf, tmp, len) >= len)
			fatal("cvs_modetostr: string truncation");
	}

	if (om) {
		if (um || gm) {
			if (strlcat(buf, ",", len) >= len)
				fatal("cvs_modetostr: string truncation");
		}

		if (strlcpy(tmp, "o=", sizeof(tmp)) >= sizeof(tmp) ||
		    strlcat(tmp, cvs_modestr[gm], sizeof(tmp)) >= sizeof(tmp))
			fatal("cvs_modetostr: overflow for others mode");

		if (strlcat(buf, tmp, len) >= len)
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
		cvs_log(LP_ERR, "buffer too small for checksum");
		return (-1);
	}
	if (MD5File(file, dst) == NULL) {
		cvs_log(LP_ERR, "failed to generate checksum for %s", file);
		return (-1);
	}

	return (0);
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
	int argc, error;
	char linebuf[256], qbuf[128], *lp, *cp, *arg;

	l = strlcpy(linebuf, line, sizeof(linebuf));
	if (l >= sizeof(linebuf))
		fatal("cvs_getargv: string truncation");

	memset(argv, 0, argvlen * sizeof(char *));
	argc = 0;

	/* build the argument vector */
	error = 0;
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
					error++;
					break;
				}

				qbuf[i++] = *lp++;
				if (i == sizeof(qbuf) - 1) {
					error++;
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
			error++;
			break;
		}

		argv[argc] = xstrdup(arg);
		argc++;
	}

	if (error != 0) {
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

	ret = cvs_getargv(line, argv, 1024);
	if (ret == -1)
		return (NULL);

	copy = xcalloc(ret + 1, sizeof(char *));

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
	if (cvs_server_active == 0)
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
	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_unlink(%s)", path);

	if (cvs_noexec == 1)
		return (0);

	if (unlink(path) == -1 && errno != ENOENT) {
		cvs_log(LP_ERRNO, "%s", path);
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
	int type, ret = -1;
	DIR *dirp;
	struct dirent *ent;
	struct stat st;
	char fpath[MAXPATHLEN];

	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_rmdir(%s)", path);

	if (cvs_noexec == 1)
		return (0);

	if ((dirp = opendir(path)) == NULL) {
		cvs_log(LP_ERR, "failed to open '%s'", path);
		return (-1);
	}

	while ((ent = readdir(dirp)) != NULL) {
		if (!strcmp(ent->d_name, ".") ||
		    !strcmp(ent->d_name, ".."))
			continue;

		(void)xsnprintf(fpath, sizeof(fpath), "%s/%s",
		    path, ent->d_name);

		if (ent->d_type == DT_UNKNOWN) {
			if (lstat(fpath, &st) == -1)
				fatal("'%s': %s", fpath, strerror(errno));

			switch (st.st_mode & S_IFMT) {
			case S_IFDIR:
				type = CVS_DIR;
				break;
			case S_IFREG:
				type = CVS_FILE;
				break;
			default:
				fatal("'%s': Unknown file type in copy",
				    fpath);
			}
		} else {
			switch (ent->d_type) {
			case DT_DIR:
				type = CVS_DIR;
				break;
			case DT_REG:
				type = CVS_FILE;
				break;
			default:
				fatal("'%s': Unknown file type in copy",
				    fpath);
			}
		}
		switch (type) {
		case CVS_DIR: 
			if (cvs_rmdir(fpath) == -1)
				goto done;
			break;
		case CVS_FILE:
			if (cvs_unlink(fpath) == -1 && errno != ENOENT)
				goto done;
			break;
		default:
			fatal("type %d unknown, shouldn't happen", type);
		}
	}


	if (rmdir(path) == -1 && errno != ENOENT) {
		cvs_log(LP_ERRNO, "%s", path);
		goto done;
	}

	ret = 0;
done:
	closedir(dirp);
	return (ret);
}

void
cvs_get_repository_path(const char *dir, char *dst, size_t len)
{
	char buf[MAXPATHLEN];

	cvs_get_repository_name(dir, buf, sizeof(buf));
	(void)xsnprintf(dst, len, "%s/%s", current_cvsroot->cr_dir, buf);
}

void
cvs_get_repository_name(const char *dir, char *dst, size_t len)
{
	FILE *fp;
	char *s, fpath[MAXPATHLEN];

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s",
	    dir, CVS_PATH_REPOSITORY);

	if ((fp = fopen(fpath, "r")) != NULL) {
		if ((fgets(dst, len, fp)) == NULL)
			fatal("cvs_get_repository_name: bad repository file");

		if ((s = strchr(dst, '\n')) != NULL)
			*s = '\0';

		(void)fclose(fp);
	} else {
		dst[0] = '\0';

		if (cvs_cmdop == CVS_OP_IMPORT) {
			if (strlcpy(dst, import_repository, len) >= len)
				fatal("cvs_get_repository_name: truncation");
			if (strlcat(dst, "/", len) >= len)
				fatal("cvs_get_repository_name: truncation");

			if (strcmp(dir, ".")) {
				if (strlcat(dst, dir, len) >= len) {
					fatal("cvs_get_repository_name: "
					    "truncation");
				}
			}
		} else {
			if (cvs_cmdop != CVS_OP_CHECKOUT) {
				if (strlcat(dst, dir, len) >= len)
					fatal("cvs_get_repository_name: "
					    "truncation");
			}
		}
	}
}

void
cvs_mkadmin(const char *path, const char *root, const char *repo,
    char *tag, char *date, int nb)
{
	FILE *fp;
	struct stat st;
	char buf[MAXPATHLEN];

	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_mkadmin(%s, %s, %s, %s, %s, %d)",
		    path, root, repo, (tag != NULL) ? tag : "",
		    (date != NULL) ? date : "", nb);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", path, CVS_PATH_CVSDIR);

	if (stat(buf, &st) != -1)
		return;

	if (mkdir(buf, 0755) == -1 && errno != EEXIST)
		fatal("cvs_mkadmin: %s: %s", buf, strerror(errno));

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", path, CVS_PATH_ROOTSPEC);

	if ((fp = fopen(buf, "w")) == NULL)
		fatal("cvs_mkadmin: %s: %s", buf, strerror(errno));

	fprintf(fp, "%s\n", root);
	(void)fclose(fp);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", path, CVS_PATH_REPOSITORY);

	if ((fp = fopen(buf, "w")) == NULL)
		fatal("cvs_mkadmin: %s: %s", buf, strerror(errno));

	fprintf(fp, "%s\n", repo);
	(void)fclose(fp);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", path, CVS_PATH_ENTRIES);

	if ((fp = fopen(buf, "w")) == NULL)
		fatal("cvs_mkadmin: %s: %s", buf, strerror(errno));
	(void)fclose(fp);

	cvs_write_tagfile(path, tag, date, nb);
}

void
cvs_mkpath(const char *path, char *tag)
{
	FILE *fp;
	size_t len;
	char *sp, *dp, *dir, rpath[MAXPATHLEN], repo[MAXPATHLEN];

	dir = xstrdup(path);

	STRIP_SLASH(dir);

	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_mkpath(%s)", dir);

	repo[0] = '\0';
	rpath[0] = '\0';

	if (cvs_cmdop == CVS_OP_UPDATE) {
		if ((fp = fopen(CVS_PATH_REPOSITORY, "r")) != NULL) {
			if ((fgets(repo, sizeof(repo), fp)) == NULL)
				fatal("cvs_mkpath: bad repository file");
			if ((len = strlen(repo)) && repo[len - 1] == '\n')
				repo[len - 1] = '\0';
			(void)fclose(fp);
		}
	}

	for (sp = dir; sp != NULL; sp = dp) {
		dp = strchr(sp, '/');
		if (dp != NULL)
			*(dp++) = '\0';

		if (repo[0] != '\0') {
			len = strlcat(repo, "/", sizeof(repo));
			if (len >= (int)sizeof(repo))
				fatal("cvs_mkpath: overflow");
		}

		len = strlcat(repo, sp, sizeof(repo));
		if (len >= (int)sizeof(repo))
			fatal("cvs_mkpath: overflow");

		if (rpath[0] != '\0') {
			len = strlcat(rpath, "/", sizeof(rpath));
			if (len >= (int)sizeof(rpath))
				fatal("cvs_mkpath: overflow");
		}

		len = strlcat(rpath, sp, sizeof(rpath));
		if (len >= (int)sizeof(rpath))
			fatal("cvs_mkpath: overflow");

		if (mkdir(rpath, 0755) == -1 && errno != EEXIST)
			fatal("cvs_mkpath: %s: %s", rpath, strerror(errno));

		cvs_mkadmin(rpath, current_cvsroot->cr_str, repo,
		    tag, NULL, 0);

		if (cvs_server_active == 1 && strcmp(rpath, ".")) {
			if (tag != NULL)
				cvs_server_set_sticky(rpath, tag);
		}
	}

	xfree(dir);
}

/*
 * Split the contents of a file into a list of lines.
 */
struct cvs_lines *
cvs_splitlines(u_char *data, size_t len)
{
	u_char *p, *c;
	size_t i, tlen;
	struct cvs_lines *lines;
	struct cvs_line *lp;

	lines = xmalloc(sizeof(*lines));
	memset(lines, 0, sizeof(*lines));
	TAILQ_INIT(&(lines->l_lines));

	lp = xmalloc(sizeof(*lp));
	memset(lp, 0, sizeof(*lp));
	TAILQ_INSERT_TAIL(&(lines->l_lines), lp, l_list);

	p = c = data;
	for (i = 0; i < len; i++) {
		if (*p == '\n' || (i == len - 1)) {
			tlen = p - c + 1;
			lp = xmalloc(sizeof(*lp));
			memset(lp, 0, sizeof(*lp));
			lp->l_line = c;
			lp->l_len = tlen;
			lp->l_lineno = ++(lines->l_nblines);
			TAILQ_INSERT_TAIL(&(lines->l_lines), lp, l_list);
			c = p + 1;
		}
		p++;
	}

	return (lines);
}

void
cvs_freelines(struct cvs_lines *lines)
{
	struct cvs_line *lp;

	while ((lp = TAILQ_FIRST(&(lines->l_lines))) != NULL) {
		TAILQ_REMOVE(&(lines->l_lines), lp, l_list);
		if (lp->l_needsfree == 1)
			xfree(lp->l_line);
		xfree(lp);
	}

	xfree(lines);
}

BUF *
cvs_patchfile(u_char *data, size_t dlen, u_char *patch, size_t plen,
    int (*p)(struct cvs_lines *, struct cvs_lines *))
{
	struct cvs_lines *dlines, *plines;
	struct cvs_line *lp;
	BUF *res;

	if ((dlines = cvs_splitlines(data, dlen)) == NULL)
		return (NULL);

	if ((plines = cvs_splitlines(patch, plen)) == NULL)
		return (NULL);

	if (p(dlines, plines) < 0) {
		cvs_freelines(dlines);
		cvs_freelines(plines);
		return (NULL);
	}

	res = cvs_buf_alloc(1024, BUF_AUTOEXT);
	TAILQ_FOREACH(lp, &dlines->l_lines, l_list) {
		if (lp->l_line == NULL)
			continue;
		cvs_buf_append(res, lp->l_line, lp->l_len);
	}

	cvs_freelines(dlines);
	cvs_freelines(plines);
	return (res);
}

/*
 * cvs_strsplit()
 *
 * Split a string <str> of <sep>-separated values and allocate
 * an argument vector for the values found.
 */
struct cvs_argvector *
cvs_strsplit(char *str, const char *sep)
{
	struct cvs_argvector *av;
	size_t i = 0;
	char *cp, *p;

	cp = xstrdup(str);
	av = xmalloc(sizeof(*av));
	av->str = cp;
	av->argv = xmalloc(sizeof(*(av->argv)));

	while ((p = strsep(&cp, sep)) != NULL) {
		av->argv[i++] = p;
		av->argv = xrealloc(av->argv,
		    i + 1, sizeof(*(av->argv)));
	}
	av->argv[i] = NULL;

	return (av);
}

/*
 * cvs_argv_destroy()
 *
 * Free an argument vector previously allocated by cvs_strsplit().
 */
void
cvs_argv_destroy(struct cvs_argvector *av)
{
	xfree(av->str);
	xfree(av->argv);
	xfree(av);
}

u_int
cvs_revision_select(RCSFILE *file, char *range)
{
	int i;
	u_int nrev;
	char *lstr, *rstr;
	struct rcs_delta *rdp;
	struct cvs_argvector *revargv, *revrange;
	RCSNUM *lnum, *rnum;

	nrev = 0;
	lnum = rnum = NULL;

	revargv = cvs_strsplit(range, ",");
	for (i = 0; revargv->argv[i] != NULL; i++) {
		revrange = cvs_strsplit(revargv->argv[i], ":");
		if (revrange->argv[0] == NULL)
			fatal("invalid revision range: %s", revargv->argv[i]);
		else if (revrange->argv[1] == NULL)
			lstr = rstr = revrange->argv[0];
		else {
			if (revrange->argv[2] != NULL)
				fatal("invalid revision range: %s",
				    revargv->argv[i]);

			lstr = revrange->argv[0];
			rstr = revrange->argv[1];

			if (strcmp(lstr, "") == 0)
				lstr = NULL;
			if (strcmp(rstr, "") == 0)
				rstr = NULL;
		}

		if (lstr == NULL)
			lstr = RCS_HEAD_INIT;

		if ((lnum = rcs_translate_tag(lstr, file)) == NULL)
			fatal("cvs_revision_select: could not translate tag `%s'", lstr);

		if (rstr != NULL) {
			if ((rnum = rcs_translate_tag(rstr, file)) == NULL)
				fatal("cvs_revision_select: could not translate tag `%s'", rstr);
		} else {
			rnum = rcsnum_alloc();
			rcsnum_cpy(file->rf_head, rnum, 0);
		}

		cvs_argv_destroy(revrange);

		TAILQ_FOREACH(rdp, &(file->rf_delta), rd_list) {
			if (rcsnum_cmp(rdp->rd_num, lnum, 0) <= 0 &&
			    rcsnum_cmp(rdp->rd_num, rnum, 0) >= 0 &&
			    !(rdp->rd_flags & RCS_RD_SELECT)) {
				rdp->rd_flags |= RCS_RD_SELECT;
				nrev++;
			}
		}
	}

	cvs_argv_destroy(revargv);

	if (lnum != NULL)
		rcsnum_free(lnum);
	if (rnum != NULL)
		rcsnum_free(rnum);

	return (nrev);
}

int
cvs_yesno(void)
{
	int c, ret;

	ret = 0;

	fflush(stderr);
	fflush(stdout);

	if ((c = getchar()) != 'y' && c != 'Y')
		ret = -1;
	else
		while (c != EOF && c != '\n')
			c = getchar();

	return (ret);
}
