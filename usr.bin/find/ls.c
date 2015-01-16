/*	$OpenBSD: ls.c,v 1.16 2015/01/16 06:40:07 deraadt Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <limits.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>
#include <fts.h>
#include "find.h"
#include "extern.h"

/* Derived from the print routines in the ls(1) source code. */

static void printlink(char *);
static void printtime(time_t);

#define NAME_WIDTH	8
#define	DATELEN		64
#define	SIXMONTHS	((DAYSPERNYEAR / 2) * SECSPERDAY)

void
printlong(char *name, char *accpath, struct stat *sb)
{
	char modep[15];

	(void)printf("%6llu %4lld ", (unsigned long long)sb->st_ino,
	    (long long)sb->st_blocks);
	(void)strmode(sb->st_mode, modep);
	(void)printf("%s %3u %-*.*s %-*.*s ", modep, sb->st_nlink, 
	    NAME_WIDTH, UT_NAMESIZE, user_from_uid(sb->st_uid, 0), 
	    NAME_WIDTH, UT_NAMESIZE, group_from_gid(sb->st_gid, 0));

	if (S_ISCHR(sb->st_mode) || S_ISBLK(sb->st_mode))
		(void)printf("%3d, %3d ", major(sb->st_rdev),
		    minor(sb->st_rdev));
	else
		(void)printf("%8lld ", (long long)sb->st_size);
	printtime(sb->st_mtime);
	(void)printf("%s", name);
	if (S_ISLNK(sb->st_mode))
		printlink(accpath);
	(void)putchar('\n');
}

static void
printtime(time_t ftime)
{
	char f_date[DATELEN];
	static time_t now;
	static int now_set = 0;

	if (! now_set) {
		now = time(NULL);
		now_set = 1;
	}

	/*
	 * convert time to string, and print
	 */
	if (strftime(f_date, sizeof(f_date),
	    (ftime + SIXMONTHS <= now || ftime > now) ? "%b %e  %Y" :
	    "%b %e %H:%M", localtime(&ftime)) == 0)
		f_date[0] = '\0';

	printf("%s ", f_date);
}

static void
printlink(char *name)
{
	int lnklen;
	char path[PATH_MAX];

	if ((lnklen = readlink(name, path, sizeof(path) - 1)) == -1) {
		warn("%s", name);
		return;
	}
	path[lnklen] = '\0';
	(void)printf(" -> %s", path);
}
