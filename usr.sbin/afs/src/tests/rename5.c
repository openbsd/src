/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <roken.h>

#include <err.h>

RCSID("$KTH: rename5.c,v 1.3 2000/10/03 00:35:44 lha Exp $");

static void
emkdir (const char *path, mode_t mode)
{
    int ret;

    ret = mkdir (path, mode);
    if (ret < 0)
	err (1, "mkdir %s", path);
}

static void
elstat (const char *path, struct stat *sb)
{
    int ret;

    ret = lstat (path, sb);
    if (ret < 0)
	err (1, "lstat %s", path);
}

static void
check_inum (const struct stat *sb1, const struct stat *sb2)
{
    if (sb1->st_ino != sb2->st_ino)
	errx (1, "wrong inode-number %u != %u",
	      (unsigned)sb1->st_ino, (unsigned)sb2->st_ino);
}

int
main(int argc, char **argv)
{
    int ret;
    struct stat old_sb, new_sb, dot_sb;

    set_progname (argv[0]);
    emkdir ("old_parent", 0777);
    emkdir ("new_parent", 0777);
    emkdir ("old_parent/victim", 0777);

    elstat ("old_parent", &old_sb);
    elstat ("new_parent", &new_sb);
    elstat ("old_parent/victim/..", &dot_sb);
    check_inum (&old_sb, &dot_sb);

    ret = rename("old_parent/victim", "new_parent/victim");
    if (ret < 0)
	err (1, "rename old_parent/victim new_parent/victim");

    elstat ("new_parent/victim/..", &dot_sb);

    check_inum (&new_sb, &dot_sb);

    return 0;
}
