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
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <roken.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#ifdef RCSID
RCSID("$KTH: mmap-shared-write.c,v 1.3 2000/10/03 00:34:57 lha Exp $");
#endif

static int
doit (const char *filename)
{
    int fd;
    size_t sz = getpagesize ();
    void *v;

    fd = open (filename, O_RDWR | O_CREAT, 0600);
    if (fd < 0)
	err (1, "open %s", filename);
    if (ftruncate (fd, sz) < 0)
	err (1, "ftruncate %s", filename);
    v = mmap (NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (v == (void *)MAP_FAILED)
	err (1, "mmap %s", filename);

    memset (v, 'z', sz);

    msync (v, sz, MS_SYNC);

    if (close (fd) < 0)
	err (1, "close %s", filename);
    return 0;
}

static void
usage(void)
{
    errx (1, "usage: [filename]");
}

int
main (int argc, char **argv)
{
    const char *filename = "foo";

    set_progname(argv[0]);

    if (argc != 1 && argc != 2)
	usage ();

    if (argc == 2)
	filename = argv[1];

    return doit (filename);
}
