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
RCSID("$KTH: create-files.c,v 1.6 2000/10/03 00:33:23 lha Exp $");
#endif

static int
creat_files (int count, long startsize)
{
    int i;
    long size = 0;
    
    for (i = 0; i < count; ++i) {
	char num[17];
	int fd;

	snprintf (num, sizeof(num), "%d", i);
	
	fd = open (num, O_WRONLY | O_CREAT | O_EXCL, 0777);
	if (fd < 0)
	    err (1, "open %s", num);
	size = startsize;
	while (size > 0) {
	    char buf[8192];
	    size_t len;
	    ssize_t ret;

	    len = min(sizeof(buf), size);

	    ret = write (fd, buf, len);
	    if (ret < 0)
		err (1, "write to %s", num);
	    if (ret != len)
		errx (1, "short write to %s", num);
	    size -= ret;
	}
	if (close (fd) < 0)
	    err (1, "close %s", num);
    }
    return 0;
}

static void
usage (int ret)
{
    fprintf (stderr, "%s number-of-files size-of-files\n", __progname);
    exit (ret);
}

int
main(int argc, char **argv)
{
    char *ptr;
    int count;
    long size;

    set_progname (argv[0]);

    if (argc != 3)
	usage (1);

    count = strtol (argv[1], &ptr, 0);
    if (count == 0 && ptr == argv[1])
	errx (1, "'%s' not a number", argv[1]);

    size = strtol (argv[2], &ptr, 0);
    if (size == 0 && ptr == argv[2])
	errx (1, "`%s' not a number", argv[2]);

    return creat_files (count, size);
}
