/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
RCSID("$KTH: create-remove.c,v 1.2 2000/10/03 00:33:28 lha Exp $");
#endif

static int
creat_dir (const char *name)
{
    int ret = mkdir (name, 0777);
    if (ret < 0) err (1, "mkdir %s", name);
    return 0;
}

static int
remove_dir (const char *name)
{
    int ret = rmdir (name);
    if (ret < 0) err (1, "rmdir %s", name);
    return 0;
}

static int
creat_file (const char *name)
{
    int ret = open (name, O_CREAT|O_RDWR, 0777);
    if (ret < 0) err (1, "mkdir %s", name);
    close (ret);
    return 0;
}

static int
unlink_file (const char *name)
{
    int ret = unlink (name);
    if (ret < 0) err (1, "unlink %s", name);
    return 0;
}


static void
usage (int ret)
{
    fprintf (stderr, "%s [file|dir] number-of-dirs\n", __progname);
    exit (ret);
}

static int
creat_many (int num,
	    int (*c) (const char *name),
	    int (*d) (const char *name))
{
    char name[MAXPATHLEN];

    if (num < 0)
	errx (1, "not be negative");

    snprintf (name, sizeof(name), "foo-%d-%d", num, getpid());

    while (num-- > 0) {
	(c) (name);
	(d) (name);
    }
    return 0;
}


int
main(int argc, char **argv)
{
    char *ptr;
    int count;

    set_progname (argv[0]);

    if (argc != 3)
	usage (1);

    count = strtol (argv[2], &ptr, 0);
    if (count == 0 && ptr == argv[2])
	errx (1, "'%s' not a number", argv[2]);

    if (strcmp ("file", argv[1]) == 0) 
	return creat_many (count, creat_file, unlink_file);
    else if (strcmp("dir", argv[1]) == 0)
	return creat_many (count, creat_dir, remove_dir);
    else
	errx (1, "unknown type: %s", argv[1]);
    return 0;
}
