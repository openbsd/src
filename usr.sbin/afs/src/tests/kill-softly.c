/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <err.h>
#include <roken.h>

#ifdef RCSID
RCSID("$KTH: kill-softly.c,v 1.6 2000/10/03 00:34:20 lha Exp $");
#endif

struct entry {
    char *name;
    int status;
};

static void
kill_one (struct entry *ents, int ind, int curents);

static void
do_dir (const char *dirname);

static void
kill_dir (const char *dirname);

static void
kill_one (struct entry *ents, int ind, int curents)
{
    int ret;
    int i;

    ret = unlink (ents[ind].name);
    if (ret < 0) {
	if (errno == EISDIR || errno == EPERM)
	    do_dir (ents[ind].name);
	else
	    err (1, "unlink %s", ents[ind].name);
    }
    ents[ind].status = 0;
    for (i = 0; i <= ind; ++i) {
	struct stat sb;

	ret = lstat (ents[i].name, &sb);
	if (ret == 0 || errno != ENOENT)
	    err (1, "%s still exists?", ents[i].name);
    }

    for (i = ind + 1; i < curents; ++i) {
	struct stat sb;

	ret = lstat (ents[i].name, &sb);
	if (ret < 0)
	    err (1, "stat %s", ents[i].name);
    }
}

static void
do_dir (const char *dirname)
{
    int ret;

    ret = chdir (dirname);
    if (ret < 0)
	err (1, "chdir %s", dirname);
    kill_dir (dirname);
    ret = chdir ("..");
    if (ret < 0)
	err (1, "chdir ..");
    ret = rmdir (dirname);
    if (ret < 0)
	err (1, "rmdir %s", dirname);
}

static void
kill_dir (const char *dirname)
{
    struct entry *ents;
    int maxents;
    int curents = 0;
    DIR *dir;
    struct dirent *dp;
    int i;

    dir = opendir (".");
    if (dir == NULL)
	err (1, "opendir %s", dirname);
    maxents = 10;
    ents = malloc (sizeof (*ents) * maxents);
    if (ents == NULL)
	err (1, "malloc");
    while ((dp = readdir (dir)) != NULL) {
	if (strcmp (dp->d_name, ".") == 0
	    || strcmp (dp->d_name, "..") == 0)
	    continue;

	if (curents >= maxents) {
	    maxents *= 2;
	    ents = realloc (ents, sizeof(*ents) * maxents);
	    if (ents == NULL)
		err (1, "realloc");
	}
	ents[curents].name   = strdup (dp->d_name);
	ents[curents].status = 1;
	++curents;
    }
    closedir (dir);
    for (i = 0; i < curents; ++i)
	kill_one (ents, i, curents);
    free (ents);
}

int
main(int argc, char **argv)
{
    set_progname (argv[0]);

    if (argc != 2)
	errx (1, "usage: %s directory", argv[0]);
    do_dir (argv[1]);
    return 0;
}
