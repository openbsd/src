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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <roken.h>

RCSID("$KTH: write-ucc.c,v 1.3 2000/10/03 00:36:53 lha Exp $");

static void
doit (const char *filename)
{
    int fd;
    int ret;
    struct timeval tv[2];
    struct stat sb;

    fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
	err (1, "open %s", filename);
    ret = write (fd, "hej\n", 4);
    if(ret < 0)
	err (1, "write %s", filename);
    if (ret != 4)
	errx (1, "short write to %s", filename);
    gettimeofday (&tv[0], NULL);
    tv[1] = tv[0];
    ret = utimes (filename, tv);
    if(ret < 0)
	err (1, "utimes %s", filename);
    ret = chmod (filename, 0644);
    if (ret < 0)
	err (1, "chmod %s", filename);
    ret = chown (filename, 0, 0);
    ret = fstat (fd, &sb);
    if (ret < 0)
	err (1, "fstat %s", filename);
    if (sb.st_size != 4)
	errx (1, "stat 1: size = %lu != 4", (unsigned long)sb.st_size);
    ret = close (fd);
    if (ret < 0)
	err (1, "close %s", filename);
    ret = stat (filename, &sb);
    if (ret < 0)
	err (1, "stat %s", filename);
    if (sb.st_size != 4)
	errx (1, "stat 1: size = %lu != 4", (unsigned long)sb.st_size);
}

int
main(int argc, char **argv)
{
    const char *file = "blaha";

    set_progname (argv[0]);
    if (argc != 1 && argc != 2)
	errx (1, "usage: %s [file]", argv[0]);
    if (argc == 2)
	file = argv[1];
    doit (file);
    return 0;
}
