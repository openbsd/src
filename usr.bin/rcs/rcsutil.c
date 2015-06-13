/*	$OpenBSD: rcsutil.c,v 1.44 2015/06/13 20:15:21 nicm Exp $	*/
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

#include <sys/stat.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rcsprog.h"

/*
 * rcs_get_mtime()
 *
 * Get <filename> last modified time.
 * Returns last modified time on success, or -1 on failure.
 */
time_t
rcs_get_mtime(RCSFILE *file)
{
	struct stat st;
	time_t mtime;

	if (file->rf_file == NULL)
		return (-1);

	if (fstat(fileno(file->rf_file), &st) == -1) {
		warn("%s", file->rf_path);
		return (-1);
	}

	mtime = st.st_mtimespec.tv_sec;

	return (mtime);
}

/*
 * rcs_set_mtime()
 *
 * Set <filename> last modified time to <mtime> if it's not set to -1.
 */
void
rcs_set_mtime(RCSFILE *file, time_t mtime)
{
	static struct timeval tv[2];

	if (file->rf_file == NULL || mtime == -1)
		return;

	tv[0].tv_sec = mtime;
	tv[1].tv_sec = tv[0].tv_sec;

	if (futimes(fileno(file->rf_file), tv) == -1)
		err(1, "utimes");
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
 * Opens and returns file descriptor to RCS file.
 */
int
rcs_choosefile(const char *filename, char *out, size_t len)
{
	int fd;
	struct stat sb;
	char *p, *ext, name[PATH_MAX], *next, *ptr, rcsdir[PATH_MAX],
	    *suffixes, rcspath[PATH_MAX];

	/*
	 * If `filename' contains a directory, `rcspath' contains that
	 * directory, including a trailing slash.  Otherwise `rcspath'
	 * contains an empty string.
	 */
	if (strlcpy(rcspath, filename, sizeof(rcspath)) >= sizeof(rcspath))
		errx(1, "rcs_choosefile: truncation");

	/* If `/' is found, end string after `/'. */
	if ((ptr = strrchr(rcspath, '/')) != NULL)
		*(++ptr) = '\0';
	else
		rcspath[0] = '\0';

	/* Append RCS/ to `rcspath' if it exists. */
	if (strlcpy(rcsdir, rcspath, sizeof(rcsdir)) >= sizeof(rcsdir) ||
	    strlcat(rcsdir, RCSDIR, sizeof(rcsdir)) >= sizeof(rcsdir))
		errx(1, "rcs_choosefile: truncation");

	if (stat(rcsdir, &sb) == 0 && S_ISDIR(sb.st_mode))
		if (strlcpy(rcspath, rcsdir, sizeof(rcspath))
		    >= sizeof(rcspath) ||
		    strlcat(rcspath, "/", sizeof(rcspath)) >= sizeof(rcspath))
			errx(1, "rcs_choosefile: truncation");

	/* Name of file without path. */
	if ((ptr = strrchr(filename, '/')) == NULL) {
		if (strlcpy(name, filename, sizeof(name)) >= sizeof(name))
			errx(1, "rcs_choosefile: truncation");
	} else {
		/* Skip `/'. */
		if (strlcpy(name, ptr + 1, sizeof(name)) >= sizeof(name))
			errx(1, "rcs_choosefile: truncation");
	}

	/* Name of RCS file without an extension. */
	if (strlcat(rcspath, name, sizeof(rcspath)) >= sizeof(rcspath))
		errx(1, "rcs_choosefile: truncation");

	/*
	 * If only the empty suffix was given, use existing rcspath.
	 * This ensures that there is at least one suffix for strsep().
	 */
	if (strcmp(rcs_suffixes, "") == 0) {
		if (strlcpy(out, rcspath, len) >= len)
			errx(1, "rcs_choosefile: truncation");
		fd = open(rcspath, O_RDONLY);
		return (fd);
	}

	/*
	 * Cycle through slash-separated `rcs_suffixes', appending each
	 * extension to `rcspath' and testing if the file exists.  If it
	 * does, return that string.  Otherwise return path with first
	 * extension.
	 */
	suffixes = xstrdup(rcs_suffixes);
	for (next = suffixes; (ext = strsep(&next, "/")) != NULL;) {
		char fpath[PATH_MAX];

		if ((p = strrchr(rcspath, ',')) != NULL) {
			if (!strcmp(p, ext)) {
				if ((fd = open(rcspath, O_RDONLY)) == -1)
					continue;

				if (fstat(fd, &sb) == -1)
					err(1, "%s", rcspath);

				if (strlcpy(out, rcspath, len) >= len)
					errx(1, "rcs_choosefile; truncation");

				free(suffixes);
				return (fd);
			}

			continue;
		}

		/* Construct RCS file path. */
		if (strlcpy(fpath, rcspath, sizeof(fpath)) >= sizeof(fpath) ||
		    strlcat(fpath, ext, sizeof(fpath)) >= sizeof(fpath))
			errx(1, "rcs_choosefile: truncation");

		/* Don't use `filename' as RCS file. */
		if (strcmp(fpath, filename) == 0)
			continue;

		if ((fd = open(fpath, O_RDONLY)) == -1)
			continue;

		if (fstat(fd, &sb) == -1)
			err(1, "%s", fpath);

		if (strlcpy(out, fpath, len) >= len)
			errx(1, "rcs_choosefile: truncation");

		free(suffixes);
		return (fd);
	}

	/*
	 * `suffixes' should now be NUL separated, so the first
	 * extension can be read just by reading `suffixes'.
	 */
	if (strlcat(rcspath, suffixes, sizeof(rcspath)) >= sizeof(rcspath))
		errx(1, "rcs_choosefile: truncation");

	free(suffixes);

	if (strlcpy(out, rcspath, len) >= len)
		errx(1, "rcs_choosefile: truncation");

	fd = open(rcspath, O_RDONLY);

	return (fd);
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
		errx(1, "too many revision numbers");
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

	bp = buf_alloc(0);
	if (isatty(STDIN_FILENO))
		(void)fprintf(stderr, "%s", prompt);
	if (isatty(STDIN_FILENO))
		(void)fprintf(stderr, ">> ");
	clearerr(stdin);
	while ((buf = fgetln(stdin, &len)) != NULL) {
		/* The last line may not be EOL terminated. */
		if (buf[0] == '.' && (len == 1 || buf[1] == '\n'))
			break;
		else
			buf_append(bp, buf, len);

		if (isatty(STDIN_FILENO))
			(void)fprintf(stderr, ">> ");
	}
	buf_putc(bp, '\0');

	return (buf_release(bp));
}

u_int
rcs_rev_select(RCSFILE *file, const char *range)
{
	int i;
	u_int nrev;
	char *ep;
	char *lstr, *rstr;
	struct rcs_delta *rdp;
	struct rcs_argvector *revargv, *revrange;
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

	revargv = rcs_strsplit(range, ",");
	for (i = 0; revargv->argv[i] != NULL; i++) {
		revrange = rcs_strsplit(revargv->argv[i], ":");
		if (revrange->argv[0] == NULL)
			/* should not happen */
			errx(1, "invalid revision range: %s", revargv->argv[i]);
		else if (revrange->argv[1] == NULL)
			lstr = rstr = revrange->argv[0];
		else {
			if (revrange->argv[2] != NULL)
				errx(1, "invalid revision range: %s",
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
			errx(1, "invalid revision: %s", lstr);

		if (rstr != NULL) {
			if (rcsnum_aton(rstr, &ep, &rnum) == 0 || (*ep != '\0'))
				errx(1, "invalid revision: %s", rstr);
		} else
			rcsnum_cpy(file->rf_head, &rnum, 0);

		rcs_argv_destroy(revrange);

		TAILQ_FOREACH(rdp, &file->rf_delta, rd_list)
			if (rcsnum_cmp(rdp->rd_num, &lnum, 0) <= 0 &&
			    rcsnum_cmp(rdp->rd_num, &rnum, 0) >= 0 &&
			    !(rdp->rd_flags & RCS_RD_SELECT)) {
				rdp->rd_flags |= RCS_RD_SELECT;
				nrev++;
			}
	}
	rcs_argv_destroy(revargv);

	free(lnum.rn_id);
	free(rnum.rn_id);

	return (nrev);
}

/*
 * Load description from <in> to <file>.
 * If <in> starts with a `-', <in> is taken as the description.
 * Otherwise <in> is the name of the file containing the description.
 * If <in> is NULL, the description is read from stdin.
 * Returns 0 on success, -1 on failure, setting errno.
 */
int
rcs_set_description(RCSFILE *file, const char *in)
{
	BUF *bp;
	char *content;
	const char *prompt =
	    "enter description, terminated with single '.' or end of file:\n"
	    "NOTE: This is NOT the log message!\n";

	/* Description is in file <in>. */
	if (in != NULL && *in != '-') {
		if ((bp = buf_load(in)) == NULL)
			return (-1);
		buf_putc(bp, '\0');
		content = buf_release(bp);
	/* Description is in <in>. */
	} else if (in != NULL)
		/* Skip leading `-'. */
		content = xstrdup(in + 1);
	/* Get description from stdin. */
	else
		content = rcs_prompt(prompt);

	rcs_desc_set(file, content);
	free(content);
	return (0);
}

/*
 * Split the contents of a file into a list of lines.
 */
struct rcs_lines *
rcs_splitlines(u_char *data, size_t len)
{
	u_char *c, *p;
	struct rcs_lines *lines;
	struct rcs_line *lp;
	size_t i, tlen;

	lines = xcalloc(1, sizeof(*lines));
	TAILQ_INIT(&(lines->l_lines));

	lp = xcalloc(1, sizeof(*lp));
	TAILQ_INSERT_TAIL(&(lines->l_lines), lp, l_list);


	p = c = data;
	for (i = 0; i < len; i++) {
		if (*p == '\n' || (i == len - 1)) {
			tlen = p - c + 1;
			lp = xmalloc(sizeof(*lp));
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
rcs_freelines(struct rcs_lines *lines)
{
	struct rcs_line *lp;

	while ((lp = TAILQ_FIRST(&(lines->l_lines))) != NULL) {
		TAILQ_REMOVE(&(lines->l_lines), lp, l_list);
		free(lp);
	}

	free(lines);
}

BUF *
rcs_patchfile(u_char *data, size_t dlen, u_char *patch, size_t plen,
    int (*p)(struct rcs_lines *, struct rcs_lines *))
{
	struct rcs_lines *dlines, *plines;
	struct rcs_line *lp;
	BUF *res;

	dlines = rcs_splitlines(data, dlen);
	plines = rcs_splitlines(patch, plen);

	if (p(dlines, plines) < 0) {
		rcs_freelines(dlines);
		rcs_freelines(plines);
		return (NULL);
	}

	res = buf_alloc(1024);
	TAILQ_FOREACH(lp, &dlines->l_lines, l_list) {
		if (lp->l_line == NULL)
			continue;
		buf_append(res, lp->l_line, lp->l_len);
	}

	rcs_freelines(dlines);
	rcs_freelines(plines);
	return (res);
}

/*
 * rcs_yesno()
 *
 * Read a char from standard input, returns defc if the
 * user enters an equivalent to defc, else whatever char
 * was entered.  Converts input to lower case.
 */
int
rcs_yesno(int defc)
{
	int c, ret;

	fflush(stderr);
	fflush(stdout);

	clearerr(stdin);
	if (isalpha(c = getchar()))
		c = tolower(c);
	if (c == defc || c == '\n' || (c == EOF && feof(stdin)))
		ret = defc;
	else
		ret = c;

	while (c != EOF && c != '\n')
		c = getchar();

	return (ret);
}

/*
 * rcs_strsplit()
 *
 * Split a string <str> of <sep>-separated values and allocate
 * an argument vector for the values found.
 */
struct rcs_argvector *
rcs_strsplit(const char *str, const char *sep)
{
	struct rcs_argvector *av;
	size_t i = 0;
	char *cp, *p;

	cp = xstrdup(str);
	av = xmalloc(sizeof(*av));
	av->str = cp;
	av->argv = xmalloc(sizeof(*(av->argv)));

	while ((p = strsep(&cp, sep)) != NULL) {
		av->argv[i++] = p;
		av->argv = xreallocarray(av->argv,
		    i + 1, sizeof(*(av->argv)));
	}
	av->argv[i] = NULL;

	return (av);
}

/*
 * rcs_argv_destroy()
 *
 * Free an argument vector previously allocated by rcs_strsplit().
 */
void
rcs_argv_destroy(struct rcs_argvector *av)
{
	free(av->str);
	free(av->argv);
	free(av);
}

/*
 * Strip suffix from filename.
 */
void
rcs_strip_suffix(char *filename)
{
	char *p, *suffixes, *next, *ext;

	if ((p = strrchr(filename, ',')) != NULL) {
		suffixes = xstrdup(rcs_suffixes);
		for (next = suffixes; (ext = strsep(&next, "/")) != NULL;) {
			if (!strcmp(p, ext)) {
				*p = '\0';
				break;
			}
		}
		free(suffixes);
	}
}
