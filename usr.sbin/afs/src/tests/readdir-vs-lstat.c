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
#include <errno.h>

#include <err.h>
#include <roken.h>

#ifdef RCSID
RCSID("$KTH: readdir-vs-lstat.c,v 1.11 2000/10/03 00:35:34 lha Exp $");
#endif

static int
verify_inodes (const char *dirname)
{
    DIR *d;
    struct dirent *dp;
    
    if (chdir (dirname) < 0)
	err (1, "chdir %s", dirname);

    d = opendir (".");
    if (d == NULL)
	err (1, "opendir %s", dirname);
    while ((dp = readdir (d)) != NULL) {
	struct stat sb;

	if (lstat (dp->d_name, &sb) < 0) {
	    if (errno == EACCES)
		continue;
	    err (1, "lstat %s", dp->d_name);
	}
	if (dp->d_ino != sb.st_ino)
	    errx (1, "%s: inode %u != %u", dp->d_name,
		  (unsigned)dp->d_ino, (unsigned)sb.st_ino);
    }
    closedir (d);
    return 0;
}

static void
usage (int ret)
{
    fprintf (stderr, "%s [directory]\n", __progname);
    exit (ret);
}

int
main(int argc, char **argv)
{
    char *name = ".";

    set_progname (argv[0]);

    if (argc > 2)
	usage (1);

    if (argc > 1)
	name = argv[1];

    return verify_inodes (name);
}
