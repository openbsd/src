/*	$OpenBSD: util.c,v 1.16 2004/12/06 21:13:49 jfb Exp $	*/
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

#include <md5.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "file.h"

static const char *cvs_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


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



pid_t cvs_exec_pid;


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
	size_t dlen;
	FILE *fp;
	char repo_path[MAXPATHLEN];

	snprintf(repo_path, sizeof(repo_path), "%s/CVS/Repository", dir);
	fp = fopen(repo_path, "r");
	if (fp == NULL) {
		return (-1);
	}

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
 * cvs_datesec()
 *
 * Take a date string and transform it into the number of seconds since the
 * Epoch.  The <type> argument specifies whether the timestamp is in ctime(3)
 * format or RFC 822 format (as CVS uses in its protocol).  If the <adj>
 * parameter is not 0, the returned time will be adjusted according to the
 * machine's local timezone.
 */

time_t
cvs_datesec(const char *date, int type, int adj)
{
	int i;
	long off;
	char sign, mon[8], gmt[8], hr[4], min[4], *ep;
	struct tm cvs_tm;

	memset(&cvs_tm, 0, sizeof(cvs_tm));
	cvs_tm.tm_isdst = -1;

	if (type == CVS_DATE_RFC822) {
		if (sscanf(date, "%d %3s %d %2d:%2d:%2d %5s", &cvs_tm.tm_mday,
		    mon, &cvs_tm.tm_year, &cvs_tm.tm_hour, &cvs_tm.tm_min,
		    &cvs_tm.tm_sec, gmt) < 7)
			return (-1);
		cvs_tm.tm_year -= 1900;

		if (*gmt == '-') {
			sscanf(gmt, "%c%2s%2s", &sign, hr, min);
			cvs_tm.tm_gmtoff = strtol(hr, &ep, 10);
			if ((cvs_tm.tm_gmtoff == LONG_MIN) ||
			    (cvs_tm.tm_gmtoff == LONG_MAX) ||
			    (*ep != '\0')) {
				cvs_log(LP_ERR,
				    "parse error in GMT hours specification `%s'", hr);
				cvs_tm.tm_gmtoff = 0;
			} else {
				/* get seconds */
				cvs_tm.tm_gmtoff *= 3600;

				/* add the minutes */
				off = strtol(min, &ep, 10);
				if ((cvs_tm.tm_gmtoff == LONG_MIN) ||
				    (cvs_tm.tm_gmtoff == LONG_MAX) ||
				    (*ep != '\0')) {
					cvs_log(LP_ERR,
					    "parse error in GMT minutes "
					    "specification `%s'", min);
				} else
					cvs_tm.tm_gmtoff += off * 60;
			}
		}
		if (sign == '-')
			cvs_tm.tm_gmtoff = -cvs_tm.tm_gmtoff;
	} else if (type == CVS_DATE_CTIME) {
		/* gmt is used for the weekday */
		sscanf(date, "%3s %3s %d %2d:%2d:%2d %d", gmt, mon,
		    &cvs_tm.tm_mday, &cvs_tm.tm_hour, &cvs_tm.tm_min,
		    &cvs_tm.tm_sec, &cvs_tm.tm_year);
		cvs_tm.tm_year -= 1900;
		cvs_tm.tm_gmtoff = 0; 
	}

	for (i = 0; i < (int)(sizeof(cvs_months)/sizeof(cvs_months[0])); i++) {
		if (strcmp(cvs_months[i], mon) == 0) {
			cvs_tm.tm_mon = i;
			break;
		}
	}

	return mktime(&cvs_tm);
}


/*
 * cvs_strtomode()
 *
 * Read the contents of the string <str> and generate a permission mode from
 * the contents of <str>, which is assumed to have the mode format of CVS.
 * The CVS protocol specification states that any modes or mode types that are
 * not recognized should be silently ignored.  This function does not return
 * an error in such cases, but will issue warnings.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_strtomode(const char *str, mode_t *mode)
{
	char type;
	mode_t m;
	char buf[32], ms[4], *sp, *ep;

	m = 0;
	strlcpy(buf, str, sizeof(buf));
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

	return (0);
}


/*
 * cvs_modetostr()
 *
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_modetostr(mode_t mode, char *buf, size_t len)
{
	size_t l;
	char tmp[16], *bp;
	mode_t um, gm, om;

	um = (mode & S_IRWXU) >> 6;
	gm = (mode & S_IRWXG) >> 3;
	om = mode & S_IRWXO;

	bp = buf;
	*bp = '\0';
	l = 0;

	if (um) {
		snprintf(tmp, sizeof(tmp), "u=%s", cvs_modestr[um]);
		l = strlcat(buf, tmp, len);
	}
	if (gm) {
		if (um)
			strlcat(buf, ",", len);
		snprintf(tmp, sizeof(tmp), "g=%s", cvs_modestr[gm]);
		strlcat(buf, tmp, len);
	}
	if (om) {
		if (um || gm)
			strlcat(buf, ",", len);
		snprintf(tmp, sizeof(tmp), "o=%s", cvs_modestr[gm]);
		strlcat(buf, tmp, len);
	}

	return (0);
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
		cvs_log(LP_ERRNO, "failed to generate file checksum");
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

int
cvs_splitpath(const char *path, char *base, size_t blen, char **file)
{
	size_t rlen;
	char *sp;

	if ((rlen = strlcpy(base, path, blen)) >= blen)
		return (-1);

	while ((rlen > 0) && (base[rlen - 1] == '/'))
		base[--rlen] = '\0';

	sp = strrchr(base, '/');
	if (sp == NULL) {
		strlcpy(base, "./", blen);
		strlcat(base, path, blen);
		sp = base + 1;
	}

	*sp = '\0';
	if (file != NULL)
		*file = sp + 1;

	return (0);
}


/*
 * cvs_getargv()
 *
 * Parse a line contained in <line> and generate an argument vector by
 * splitting the line on spaces and tabs.  The resulting vector is stored in
 * <argv>, which can accept up to <argvlen> entries.
 * Returns the number of arguments in the vector, or -1 if an error occured.
 */

int
cvs_getargv(const char *line, char **argv, int argvlen)
{
	u_int i;
	int argc, err;
	char linebuf[256], qbuf[128], *lp, *cp, *arg;

	strlcpy(linebuf, line, sizeof(linebuf));
	memset(argv, 0, sizeof(argv));
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

		argv[argc] = strdup(arg);
		if (argv[argc] == NULL) {
			cvs_log(LP_ERRNO, "failed to copy argument");
			err++;
			break;
		}
		argc++;
	}

	if (err) {
		/* ditch the argument vector */
		for (i = 0; i < (u_int)argc; i++)
			free(argv[i]);
		argc = -1;
	}

	return (argc);
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
			free(argv[i]);
}


/*
 * cvs_mkadmin()
 *
 * Create the CVS administrative files within the directory <cdir>.  If the
 * files already exist, they are kept as is.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_mkadmin(CVSFILE *cdir, mode_t mode)
{
	char dpath[MAXPATHLEN], path[MAXPATHLEN];
	FILE *fp;
	CVSENTRIES *ef;
	struct stat st;
	struct cvsroot *root;

	cvs_file_getpath(cdir, dpath, sizeof(dpath));

	snprintf(path, sizeof(path), "%s/" CVS_PATH_CVSDIR, dpath);
	if ((mkdir(path, mode) == -1) && (errno != EEXIST)) {
		cvs_log(LP_ERRNO, "failed to create directory %s", path);
		return (-1);
	}

	/* just create an empty Entries file */
	ef = cvs_ent_open(dpath, O_WRONLY);
	(void)cvs_ent_close(ef);

	root = cdir->cf_ddat->cd_root;
	snprintf(path, sizeof(path), "%s/" CVS_PATH_ROOTSPEC, dpath);
	if ((root != NULL) && (stat(path, &st) != 0) && (errno == ENOENT)) {
		fp = fopen(path, "w");
		if (fp == NULL) {
			cvs_log(LP_ERRNO, "failed to open %s", path);
			return (-1);
		}
		if (root->cr_user != NULL) {
			fprintf(fp, "%s", root->cr_user);
			if (root->cr_pass != NULL)
				fprintf(fp, ":%s", root->cr_pass);
			if (root->cr_host != NULL)
				putc('@', fp);
		}

		if (root->cr_host != NULL) {
			fprintf(fp, "%s", root->cr_host);
			if (root->cr_dir != NULL)
				putc(':', fp);
		}
		if (root->cr_dir)
			fprintf(fp, "%s", root->cr_dir);
		putc('\n', fp);
		(void)fclose(fp);
	}

	snprintf(path, sizeof(path), "%s/" CVS_PATH_REPOSITORY, dpath);
	if ((stat(path, &st) != 0) && (errno == ENOENT) &&
	    (cdir->cf_ddat->cd_repo != NULL)) {
		fp = fopen(path, "w");
		if (fp == NULL) {
			cvs_log(LP_ERRNO, "failed to open %s", path);
			return (-1);
		}
		fprintf(fp, "%s\n", cdir->cf_ddat->cd_repo);
		(void)fclose(fp);
	}

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
