/*	$OpenBSD: diffdir.c,v 1.17 2003/07/04 17:50:24 millert Exp $	*/

/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diff.h"
#include "pathnames.h"

#if 0
static const char sccsid[] = "@(#)diffdir.c	4.12 (Berkeley) 4/30/89";
#endif

/*
 * diff - directory comparison
 */
#define	d_flags	d_ino

#define	DIRECT	1		/* Directory */

struct dir {
	u_long d_ino;
	short d_reclen;
	short d_namlen;
	char *d_entry;
};

static int dirstatus;		/* exit status from diffdir */
static char title[2 * BUFSIZ];


static struct dir *setupdir(char *);
static int ascii(int);
static void compare(struct dir *);
static void calldiff(void);
static void setfile(char **fpp, char **epp, char *file);
static int useless(char *);
static void only(struct dir *dp, int which);
static int entcmp(const void *, const void *);

void
diffdir(char **argv)
{
	struct dir *dir1, *dir2;
	struct dir *d1, *d2;
	int i, cmp;

	if (opt == D_IFDEF)
		warnx("can't specify -I with directories");
	if (opt == D_EDIT && sflag)
		warnx("warning: shouldn't give -s with -e");
	strlcpy(title, "diff ", sizeof title);
	for (i = 1; diffargv[i + 2]; i++) {
		if (!strcmp(diffargv[i], "-"))
			continue;	/* was -S, dont look silly */
		strlcat(title, diffargv[i], sizeof title);
		strlcat(title, " ", sizeof title);
	}
	setfile(&file1, &efile1, file1);
	setfile(&file2, &efile2, file2);
	argv[0] = file1;
	argv[1] = file2;
	dir1 = setupdir(file1);
	dir2 = setupdir(file2);
	d1 = dir1;
	d2 = dir2;
	while (d1->d_entry != 0 || d2->d_entry != 0) {
		if (d1->d_entry && useless(d1->d_entry)) {
			d1++;
			continue;
		}
		if (d2->d_entry && useless(d2->d_entry)) {
			d2++;
			continue;
		}
		if (d1->d_entry == 0)
			cmp = 1;
		else if (d2->d_entry == 0)
			cmp = -1;
		else
			cmp = strcmp(d1->d_entry, d2->d_entry);
		if (cmp < 0) {
			if (opt == 0 || opt == 2)
				only(d1, 1);
			d1++;
			dirstatus |= 1;
		} else if (cmp == 0) {
			compare(d1);
			d1++;
			d2++;
		} else {
			if (opt == 0 || opt == 2)
				only(d2, 2);
			d2++;
			dirstatus |= 1;
		}
	}
	if (rflag) {
		for (d1 = dir1; d1->d_entry; d1++) {
			if ((d1->d_flags & DIRECT) == 0)
				continue;
			strlcpy(efile1, d1->d_entry,
			    file1 + MAXPATHLEN - efile1);
			strlcpy(efile2, d1->d_entry,
			    file2 + MAXPATHLEN - efile2);
			calldiff();
		}
	}
	status = dirstatus;
}

void
setfile(char **fpp, char **epp, char *file)
{
	char *cp;
	size_t len;

	if (*file == '\0')
		file = ".";
	*fpp = emalloc(MAXPATHLEN);
	len = strlcpy(*fpp, file, MAXPATHLEN);
	if (len >= MAXPATHLEN - 1)
		errorx("%s: %s", file, strerror(ENAMETOOLONG));
	cp = *fpp + len - 1;
	if (*cp == '/')
		++cp;
	else {
		*++cp = '/';
		*++cp = '\0';
	}
	*epp = cp;
}

void
only(struct dir *dp, int which)
{
	char *file = which == 1 ? file1 : file2;
	char *efile = which == 1 ? efile1 : efile2;

	printf("Only in %.*s: %s\n", (int)(efile - file - 1), file, dp->d_entry);
}

struct dir *
setupdir(char *cp)
{
	struct dir *dp, *ep;
	struct dirent *rp;
	int nitems;
	DIR *dirp;

	dirp = opendir(cp);
	if (dirp == NULL)
		error("%s", cp);
	nitems = 0;
	dp = emalloc(sizeof(struct dir));
	while ((rp = readdir(dirp))) {
		ep = &dp[nitems++];
		ep->d_reclen = rp->d_reclen;
		ep->d_namlen = rp->d_namlen;
		ep->d_entry = 0;
		ep->d_flags = 0;
		if (ep->d_namlen > 0) {
			ep->d_entry = emalloc(ep->d_namlen + 1);
			strlcpy(ep->d_entry, rp->d_name, ep->d_namlen + 1);
		}
		dp = erealloc(dp, (nitems + 1) * sizeof(struct dir));
	}
	dp[nitems].d_entry = 0;	/* delimiter */
	closedir(dirp);
	qsort(dp, nitems, sizeof(struct dir), entcmp);
	return (dp);
}

static int
entcmp(const void *v1, const void *v2)
{
	const struct dir *d1, *d2;

	d1 = v1;
	d2 = v2;
	return (strcmp(d1->d_entry, d2->d_entry));
}

static void
compare(struct dir *dp)
{
	char buf1[BUFSIZ], buf2[BUFSIZ];
	int i, j, f1, f2, fmt1, fmt2;
	struct stat stb1, stb2;

	strlcpy(efile1, dp->d_entry, file1 + MAXPATHLEN - efile1);
	strlcpy(efile2, dp->d_entry, file2 + MAXPATHLEN - efile2);
	f1 = open(file1, 0);
	if (f1 < 0) {
		warn("%s", file1);
		return;
	}
	f2 = open(file2, 0);
	if (f2 < 0) {
		warn("%s", file2);
		close(f1);
		return;
	}
	fstat(f1, &stb1);
	fstat(f2, &stb2);
	fmt1 = stb1.st_mode & S_IFMT;
	fmt2 = stb2.st_mode & S_IFMT;
	if (fmt1 != S_IFREG || fmt2 != S_IFREG) {
		if (fmt1 == fmt2) {
			if (fmt1 != S_IFDIR && stb1.st_rdev == stb2.st_rdev)
				goto same;
			if (fmt1 == S_IFDIR) {
				dp->d_flags = DIRECT;
				if (opt == D_EDIT)
					goto closem;
				printf("Common subdirectories: %s and %s\n",
				    file1, file2);
				goto closem;
			}
		}
		goto notsame;
	}
	if (stb1.st_size != stb2.st_size)
		goto notsame;
	for (;;) {
		i = read(f1, buf1, BUFSIZ);
		j = read(f2, buf2, BUFSIZ);
		if (i < 0 || j < 0 || i != j)
			goto notsame;
		if (i == 0 && j == 0)
			goto same;
		for (j = 0; j < i; j++)
			if (buf1[j] != buf2[j])
				goto notsame;
	}
same:
	if (sflag != 0)
		printf("Files %s and %s are identical\n", file1, file2);
	goto closem;
notsame:
	dirstatus |= 1;
	if (!ascii(f1) || !ascii(f2)) {
		if (opt == D_NORMAL || opt == D_CONTEXT || opt == D_UNIFIED)
			printf("Binary files %s and %s differ\n",
			    file1, file2);
		goto closem;
	}
	close(f1);
	close(f2);
	anychange = 1;
	if (opt == D_EDIT) {
		printf("ed - %s << '-*-END-*-'\n", dp->d_entry);
		calldiff();
	} else {
		printf("%s%s %s\n", title, file1, file2);
		calldiff();
	}
	if (opt == D_EDIT)
		printf("w\nq\n-*-END-*-\n");
	return;
closem:
	close(f1);
	close(f2);
}

static void
calldiff(void)
{
	int lstatus;
	pid_t pid;

	fflush(stdout);
	pid = fork();
	if (pid == -1)
		errorx("No more processes");
	if (pid == 0) {
		execv(_PATH_DIFF, diffargv);
		error("%s", _PATH_DIFF);
	}
	while (wait(&lstatus) != pid)
		continue;
	/*
		if ((lstatus >> 8) >= 2)
			done(0);
	*/
	dirstatus |= lstatus >> 8;
}

int
ascii(int f)
{
	char buf[BUFSIZ], *cp;
	int cnt;

	if (aflag)
		return (1);

	lseek(f, (off_t)0, SEEK_SET);
	cnt = read(f, buf, BUFSIZ);
	cp = buf;
	while (--cnt >= 0)
		if (*cp++ & 0200)
			return (0);
	return (1);
}

/*
 * THIS IS CRUDE.
 */
int
useless(char *cp)
{
	if (cp[0] == '.') {
		if (cp[1] == '\0')
			return (1);	/* directory "." */
		if (cp[1] == '.' && cp[2] == '\0')
			return (1);	/* directory ".." */
	}
	if (start && strcmp(start, cp) > 0)
		return (1);
	return (0);
}
