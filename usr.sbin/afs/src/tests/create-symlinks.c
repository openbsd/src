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
RCSID("$KTH: create-symlinks.c,v 1.3 2000/10/12 00:26:03 lha Exp $");
#endif

#define CONTENT_STRING "kaka"

static FILE *verbose_fp = NULL;

static int
creat_symlinks (int count)
{
    int ret;
    int i;
    
    fprintf (verbose_fp, "creating:");

    for (i = 0; i < count; ++i) {
	char num[17];

	fprintf (verbose_fp, " c%d", i);
	fflush (verbose_fp);

	snprintf (num, sizeof(num), "%d", i);
	
	ret = symlink (CONTENT_STRING, num);
	if (ret < 0)
	    err (1, "symlink %s", num);
    }
    fprintf (verbose_fp, "\n");
    return 0;
}

static int
verify_contents (int count)
{
    int ret, i;
    char file[MAXPATHLEN];
    char content[MAXPATHLEN];
    
    fprintf (verbose_fp, "reading:");
    for (i = 0; i < count; i++) {
	fprintf (verbose_fp, " r%d", i); 
	fflush (verbose_fp);

	snprintf (file, sizeof(file), "%d", i);
	ret = readlink (file, content, sizeof(content));
	if (ret < 0)
	    err (1, "readlink: %d", i);
	if (strcmp (CONTENT_STRING, content) != 0)
	    errx (1, "%s != %s", content, CONTENT_STRING);
    }
    fprintf (verbose_fp, "\n");
    return 0;
}

static void
usage (int ret)
{
    fprintf (stderr, "%s number-of-symlinks\n", __progname);
    exit (ret);
}

int
main(int argc, char **argv)
{
    char *ptr;
    int count;

    set_progname (argv[0]);

    if (argc != 2)
	usage (1);

    verbose_fp = fdopen (4, "w");
    if (verbose_fp == NULL) {
	verbose_fp = fopen ("/dev/null", "w");
	if (verbose_fp == NULL)
	    err (1, "fopen");
    }

    count = strtol (argv[1], &ptr, 0);
    if (count == 0 && ptr == argv[1])
	errx (1, "'%s' not a number", argv[1]);

    return creat_symlinks (count) ||
	verify_contents(count);
}
