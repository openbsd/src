/*	$OpenBSD: rcsutil.c,v 1.2 2006/04/24 04:51:57 ray Exp $	*/
/*
 * Copyright (c) 2005, 2006 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
 * Copyright (c) 2006 Ray Lai <ray@openbsd.org>
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

#include "rcsprog.h"

/*
 * rcs_get_mtime()
 *
 * Get <filename> last modified time.
 * Returns last modified time on success, or -1 on failure.
 */
time_t
rcs_get_mtime(const char *filename)
{
	struct stat st;
	time_t mtime;

	if (stat(filename, &st) == -1) {
		warn("%s", filename);
		return (-1);
	}
	mtime = (time_t)st.st_mtimespec.tv_sec;

	return (mtime);
}

/*
 * rcs_set_mtime()
 *
 * Set <filename> last modified time to <mtime> if it's not set to -1.
 */
void
rcs_set_mtime(const char *filename, time_t mtime)
{
	static struct timeval tv[2];

	if (mtime == -1)
		return;

	tv[0].tv_sec = mtime;
	tv[1].tv_sec = tv[0].tv_sec;

	if (utimes(filename, tv) == -1)
		fatal("error setting utimes: %s", strerror(errno));
}

int
rcs_getopt(int argc, char **argv, const char *optstr)
{
	char *a;
	const char *c;
	static int i = 1;
	int opt, hasargument, ret;

	hasargument = 0;
	rcs_optarg = NULL;

	if (i >= argc)
		return (-1);

	a = argv[i++];
	if (*a++ != '-')
		return (-1);

	ret = 0;
	opt = *a;
	for (c = optstr; *c != '\0'; c++) {
		if (*c == opt) {
			a++;
			ret = opt;

			if (*(c + 1) == ':') {
				if (*(c + 2) == ':') {
					if (*a != '\0')
						hasargument = 1;
				} else {
					if (*a != '\0') {
						hasargument = 1;
					} else {
						ret = 1;
						break;
					}
				}
			}

			if (hasargument == 1)
				rcs_optarg = a;

			if (ret == opt)
				rcs_optind++;
			break;
		}
	}

	if (ret == 0)
		warnx("unknown option -%c", opt);
	else if (ret == 1)
		warnx("missing argument for option -%c", opt);

	return (ret);
}

/*
 * rcs_choosefile()
 *
 * Given a relative filename, decide where the corresponding RCS file
 * should be.  Tries each extension until a file is found.  If no file
 * was found, returns a path with the first extension.
 *
 * Returns pointer to a char array on success, NULL on failure.
 */
char *
rcs_choosefile(const char *filename)
{
	struct stat sb;
	char *p, *ext, name[MAXPATHLEN], *next, *ptr, rcsdir[MAXPATHLEN],
	    *ret, *suffixes, rcspath[MAXPATHLEN];

	/* If -x flag was not given, use default. */
	if (rcs_suffixes == NULL)
		rcs_suffixes = RCS_DEFAULT_SUFFIX;

	/*
	 * If `filename' contains a directory, `rcspath' contains that
	 * directory, including a trailing slash.  Otherwise `rcspath'
	 * contains an empty string.
	 */
	if (strlcpy(rcspath, filename, sizeof(rcspath)) >= sizeof(rcspath))
		return (NULL);
	/* If `/' is found, end string after `/'. */
	if ((ptr = strrchr(rcspath, '/')) != NULL)
		*(++ptr) = '\0';
	else
		rcspath[0] = '\0';

	/* Append RCS/ to `rcspath' if it exists. */
	if (strlcpy(rcsdir, rcspath, sizeof(rcsdir)) >= sizeof(rcsdir) ||
	    strlcat(rcsdir, RCSDIR, sizeof(rcsdir)) >= sizeof(rcsdir))
		return (NULL);
	if (stat(rcsdir, &sb) == 0 && (sb.st_mode & S_IFDIR))
		if (strlcpy(rcspath, rcsdir, sizeof(rcspath)) >= sizeof(rcspath) ||
		    strlcat(rcspath, "/", sizeof(rcspath)) >= sizeof(rcspath))
			return (NULL);

	/* Name of file without path. */
	if ((ptr = strrchr(filename, '/')) == NULL) {
		if (strlcpy(name, filename, sizeof(name)) >= sizeof(name))
			return (NULL);
	} else {
		/* Skip `/'. */
		if (strlcpy(name, ptr + 1, sizeof(name)) >= sizeof(name))
			return (NULL);
	}

	/* Name of RCS file without an extension. */
	if (strlcat(rcspath, name, sizeof(rcspath)) >= sizeof(rcspath))
		return (NULL);

	/*
	 * If only the empty suffix was given, use existing rcspath.
	 * This ensures that there is at least one suffix for strsep().
	 */
	if (strcmp(rcs_suffixes, "") == 0) {
		ret = xstrdup(rcspath);
		return (ret);
	}

	/*
	 * Cycle through slash-separated `rcs_suffixes', appending each
	 * extension to `rcspath' and testing if the file exists.  If it
	 * does, return that string.  Otherwise return path with first
	 * extension.
	 */
	suffixes = xstrdup(rcs_suffixes);
	for (ret = NULL, next = suffixes; (ext = strsep(&next, "/")) != NULL;) {
		char fpath[MAXPATHLEN];

		if ((p = strrchr(rcspath, ',')) != NULL) {
			if (!strcmp(p, ext)) {
				if (stat(rcspath, &sb) == 0) {
					ret = xstrdup(rcspath);
					goto out;
				}
			}

			continue;
		}

		/* Construct RCS file path. */
		if (strlcpy(fpath, rcspath, sizeof(fpath)) >= sizeof(fpath) ||
		    strlcat(fpath, ext, sizeof(fpath)) >= sizeof(fpath))
			goto out;

		/* Don't use `filename' as RCS file. */
		if (strcmp(fpath, filename) == 0)
			continue;

		if (stat(fpath, &sb) == 0) {
			ret = xstrdup(fpath);
			goto out;
		}
	}

	/*
	 * `ret' is still NULL.  No RCS file with any extension exists
	 * so we use the first extension.
	 *
	 * `suffixes' should now be NUL separated, so the first
	 * extension can be read just by reading `suffixes'.
	 */
	if (strlcat(rcspath, suffixes, sizeof(rcspath)) >=
	    sizeof(rcspath))
		goto out;
	ret = xstrdup(rcspath);

out:
	/* `ret' may be NULL, which indicates an error. */
	xfree(suffixes);
	return (ret);
}

/*
 * Find the name of an RCS file, given a file name `fname'.  If an RCS
 * file is found, the name is copied to the `len' sized buffer `out'.
 * Returns 0 if RCS file was found, -1 otherwise.
 */
int
rcs_statfile(char *fname, char *out, size_t len, int flags)
{
	struct stat st;
	char *rcspath;

	if ((rcspath = rcs_choosefile(fname)) == NULL)
		fatal("rcs_statfile: path truncation");

	/* Error out if file not found and we are not creating one. */
	if (stat(rcspath, &st) == -1 && !(flags & RCS_CREATE)) {
		if (strcmp(__progname, "rcsclean") != 0 &&
		    strcmp(__progname, "ci") != 0)
			warn("%s", rcspath);
		xfree(rcspath);
		return (-1);
	}

	if (strlcpy(out, rcspath, len) >= len)
		fatal("rcs_statfile: path truncation");

	xfree(rcspath);

	return (0);
}

/*
 * Allocate an RCSNUM and store in <rev>.
 */
void
rcs_set_rev(const char *str, RCSNUM **rev)
{
	if (str == NULL || (*rev = rcsnum_parse(str)) == NULL)
		fatal("bad revision number '%s'", str);
}

/*
 * Set <str> to <new_str>.  Print warning if <str> is redefined.
 */
void
rcs_setrevstr(char **str, char *new_str)
{
	if (new_str == NULL)
		return;
	if (*str != NULL)
		warnx("redefinition of revision number");
	*str = new_str;
}

/*
 * Set <str1> or <str2> to <new_str>, depending on which is not set.
 * If both are set, error out.
 */
void
rcs_setrevstr2(char **str1, char **str2, char *new_str)
{
	if (new_str == NULL)
		return;
	if (*str1 == NULL)
		*str1 = new_str;
	else if (*str2 == NULL)
		*str2 = new_str;
	else
		fatal("too many revision numbers");
}

/*
 * Get revision from file.  The revision can be specified as a symbol or
 * a revision number.
 */
RCSNUM *
rcs_getrevnum(const char *rev_str, RCSFILE *file)
{
	RCSNUM *rev;

	/* Search for symbol. */
	rev = rcs_sym_getrev(file, rev_str);

	/* Search for revision number. */
	if (rev == NULL)
		rev = rcsnum_parse(rev_str);

	return (rev);
}

/*
 * Prompt for and store user's input in an allocated string.
 *
 * Returns the string's pointer.
 */
char *
rcs_prompt(const char *prompt)
{
	BUF *bp;
	size_t len;
	char *buf;

	bp = cvs_buf_alloc(0, BUF_AUTOEXT);
	if (isatty(STDIN_FILENO))
		(void)fprintf(stderr, "%s", prompt);
	if (isatty(STDIN_FILENO))
		(void)fprintf(stderr, ">> ");
	while ((buf = fgetln(stdin, &len)) != NULL) {
		/* The last line may not be EOL terminated. */
		if (buf[0] == '.' && (len == 1 || buf[1] == '\n'))
			break;
		else
			cvs_buf_append(bp, buf, len);

		if (isatty(STDIN_FILENO))
			(void)fprintf(stderr, ">> ");
	}
	cvs_buf_putc(bp, '\0');

	return (cvs_buf_release(bp));
}

u_int
rcs_rev_select(RCSFILE *file, char *range)
{
	int i;
	u_int nrev;
	char *ep;
	char *lstr, *rstr;
	struct rcs_delta *rdp;
	struct cvs_argvector *revargv, *revrange;
	RCSNUM lnum, rnum;

	nrev = 0;
	(void)memset(&lnum, 0, sizeof(lnum));
	(void)memset(&rnum, 0, sizeof(rnum));

	if (range == NULL) {
		TAILQ_FOREACH(rdp, &file->rf_delta, rd_list)
			if (rcsnum_cmp(rdp->rd_num, file->rf_head, 0) == 0) {
				rdp->rd_flags |= RCS_RD_SELECT;
				return (1);
			}
		return (0);
	}

	revargv = cvs_strsplit(range, ",");
	for (i = 0; revargv->argv[i] != NULL; i++) {
		revrange = cvs_strsplit(revargv->argv[i], ":");
		if (revrange->argv[0] == NULL)
			/* should not happen */
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
		if (rcsnum_aton(lstr, &ep, &lnum) == 0 || (*ep != '\0'))
			fatal("invalid revision: %s", lstr);

		if (rstr != NULL) {
			if (rcsnum_aton(rstr, &ep, &rnum) == 0 || (*ep != '\0'))
				fatal("invalid revision: %s", rstr);
		} else
			rcsnum_cpy(file->rf_head, &rnum, 0);

		cvs_argv_destroy(revrange);

		TAILQ_FOREACH(rdp, &file->rf_delta, rd_list)
			if (rcsnum_cmp(rdp->rd_num, &lnum, 0) <= 0 &&
			    rcsnum_cmp(rdp->rd_num, &rnum, 0) >= 0 &&
			    !(rdp->rd_flags & RCS_RD_SELECT)) {
				rdp->rd_flags |= RCS_RD_SELECT;
				nrev++;
			}
	}
	cvs_argv_destroy(revargv);

	if (lnum.rn_id != NULL)
		xfree(lnum.rn_id);
	if (rnum.rn_id != NULL)
		xfree(rnum.rn_id);

	return (nrev);
}

/*
 * Load description from <in> to <file>.
 * If <in> starts with a `-', <in> is taken as the description.
 * Otherwise <in> is the name of the file containing the description.
 * If <in> is NULL, the description is read from stdin.
 */
void
rcs_set_description(RCSFILE *file, const char *in)
{
	BUF *bp;
	char *content;
	const char *prompt =
	    "enter description, terminated with single '.' or end of file:\n"
	    "NOTE: This is NOT the log message!\n";

	/* Description is in file <in>. */
	if (in != NULL && *in != '-') {
		bp = cvs_buf_load(in, BUF_AUTOEXT);
		cvs_buf_putc(bp, '\0');
		content = cvs_buf_release(bp);
	/* Description is in <in>. */
	} else if (in != NULL)
		/* Skip leading `-'. */
		content = xstrdup(in + 1);
	/* Get description from stdin. */
	else
		content = rcs_prompt(prompt);

	rcs_desc_set(file, content);
	xfree(content);
}
