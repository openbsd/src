/*	$OpenBSD: diffdir.c,v 1.39 2010/11/08 15:49:13 millert Exp $	*/

/*
 * Copyright (c) 2003, 2010 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diff.h"
#include "xmalloc.h"

static int selectfile(struct dirent *);
static struct dirent **slurpdir(char *, int);
static void diffit(struct dirent *, char *, size_t, char *, size_t, int);

#define d_status	d_type		/* we need to store status for -l */

/*
 * Diff directory traversal. Will be called recursively if -r was specified.
 */
void
diffdir(char *p1, char *p2, int flags)
{
	struct dirent *dent1, **dp1, **dirp1 = NULL;
	struct dirent *dent2, **dp2, **dirp2 = NULL;
	size_t dirlen1, dirlen2;
	char path1[MAXPATHLEN], path2[MAXPATHLEN];
	int pos;

	dirlen1 = strlcpy(path1, *p1 ? p1 : ".", sizeof(path1));
	if (dirlen1 >= sizeof(path1) - 1) {
		warnx("%s: %s", p1, strerror(ENAMETOOLONG));
		status = 2;
		return;
	}
	if (path1[dirlen1 - 1] != '/') {
		path1[dirlen1++] = '/';
		path1[dirlen1] = '\0';
	}
	dirlen2 = strlcpy(path2, *p2 ? p2 : ".", sizeof(path2));
	if (dirlen2 >= sizeof(path2) - 1) {
		warnx("%s: %s", p2, strerror(ENAMETOOLONG));
		status = 2;
		return;
	}
	if (path2[dirlen2 - 1] != '/') {
		path2[dirlen2++] = '/';
		path2[dirlen2] = '\0';
	}

	/* get a list of the entries in each directory */
	dp1 = dirp1 = slurpdir(path1, Nflag + Pflag);
	dp2 = dirp2 = slurpdir(path2, Nflag);
	if (dirp1 == NULL || dirp2 == NULL)
		goto closem;

	/*
	 * If we were given a starting point, find it.
	 */
	if (start != NULL) {
		while (*dp1 != NULL && strcmp((*dp1)->d_name, start) < 0)
			dp1++;
		while (*dp2 != NULL && strcmp((*dp2)->d_name, start) < 0)
			dp2++;
	}

	/*
	 * Iterate through the two directory lists, diffing as we go.
	 */
	while (*dp1 != NULL || *dp2 != NULL) {
		dent1 = *dp1;
		dent2 = *dp2;

		pos = dent1 == NULL ? 1 : dent2 == NULL ? -1 :
		    strcmp(dent1->d_name, dent2->d_name);
		if (pos == 0) {
			/* file exists in both dirs, diff it */
			diffit(dent1, path1, dirlen1, path2, dirlen2, flags);
			dp1++;
			dp2++;
		} else if (pos < 0) {
			/* file only in first dir, only diff if -N */
			if (Nflag)
				diffit(dent1, path1, dirlen1, path2, dirlen2,
				    flags);
			else if (lflag)
				dent1->d_status |= D_ONLY;
			else
				print_only(path1, dirlen1, dent1->d_name);
			dp1++;
		} else {
			/* file only in second dir, only diff if -N or -P */
			if (Nflag || Pflag)
				diffit(dent2, path1, dirlen1, path2, dirlen2,
				    flags);
			else if (lflag)
				dent2->d_status |= D_ONLY;
			else
				print_only(path2, dirlen2, dent2->d_name);
			dp2++;
		}
	}
	if (lflag) {
		path1[dirlen1] = '\0';
		path2[dirlen2] = '\0';
		for (dp1 = dirp1; (dent1 = *dp1) != NULL; dp1++) {
			print_status(dent1->d_status, path1, path2,
			    dent1->d_name);
		}
		for (dp2 = dirp2; (dent2 = *dp2) != NULL; dp2++) {
			if (dent2->d_status == D_ONLY)
				print_status(dent2->d_status, path2, NULL,
				    dent2->d_name);
		}
	}

closem:
	if (dirp1 != NULL) {
		for (dp1 = dirp1; (dent1 = *dp1) != NULL; dp1++)
			xfree(dent1);
		xfree(dirp1);
	}
	if (dirp2 != NULL) {
		for (dp2 = dirp2; (dent2 = *dp2) != NULL; dp2++)
			xfree(dent2);
		xfree(dirp2);
	}
}

/*
 * Read in a whole directory, culling out the "excluded" files.
 * Returns an array of struct dirent *'s in alphabetic order.
 * Caller is responsible for free()ing each array element and the array itself.
 */
static struct dirent **
slurpdir(char *dirname, int enoentok)
{
	struct dirent **namelist = NULL;
	int rval;

	rval = scandir(dirname, &namelist, selectfile, alphasort);
	if (rval == -1) {
		if (enoentok && errno == ENOENT) {
			namelist = xmalloc(sizeof(struct dirent *));
			namelist[0] = NULL;
		} else {
			warn("%s", dirname);
		}
	}

	return (namelist);
}

/*
 * Do the actual diff by calling either diffreg() or diffdir().
 */
static void
diffit(struct dirent *dp, char *path1, size_t plen1, char *path2, size_t plen2,
    int flags)
{
	flags |= D_HEADER;
	strlcpy(path1 + plen1, dp->d_name, MAXPATHLEN - plen1);
	if (stat(path1, &stb1) != 0) {
		if (!(Nflag || Pflag) || errno != ENOENT) {
			warn("%s", path1);
			return;
		}
		flags |= D_EMPTY1;
		memset(&stb1, 0, sizeof(stb1));
	}

	strlcpy(path2 + plen2, dp->d_name, MAXPATHLEN - plen2);
	if (stat(path2, &stb2) != 0) {
		if (!Nflag || errno != ENOENT) {
			warn("%s", path2);
			return;
		}
		flags |= D_EMPTY2;
		memset(&stb2, 0, sizeof(stb2));
		stb2.st_mode = stb1.st_mode;
	}
	if (stb1.st_mode == 0)
		stb1.st_mode = stb2.st_mode;

	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
		if (rflag)
			diffdir(path1, path2, flags);
		else if (lflag)
			dp->d_status |= D_COMMON;
		else
			printf("Common subdirectories: %s and %s\n",
			    path1, path2);
		return;
	}
	if (!S_ISREG(stb1.st_mode) && !S_ISDIR(stb1.st_mode))
		dp->d_status = D_SKIPPED1;
	else if (!S_ISREG(stb2.st_mode) && !S_ISDIR(stb2.st_mode))
		dp->d_status = D_SKIPPED2;
	else
		dp->d_status = diffreg(path1, path2, flags);
	if (!lflag)
		print_status(dp->d_status, path1, path2, "");
}

/*
 * Returns 1 if the directory entry should be included in the
 * diff, else 0.  Checks the excludes list.
 */
static int
selectfile(struct dirent *dp)
{
	struct excludes *excl;

	if (dp->d_fileno == 0)
		return (0);

	/* always skip "." and ".." */
	if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
	    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
		return (0);

	/* check excludes list */
	for (excl = excludes_list; excl != NULL; excl = excl->next)
		if (fnmatch(excl->pattern, dp->d_name, FNM_PATHNAME) == 0)
			return (0);

	return (1);
}
