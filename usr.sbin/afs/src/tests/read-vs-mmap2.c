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
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <roken.h>

#ifdef RCSID
RCSID("$KTH: read-vs-mmap2.c,v 1.8 2000/10/03 00:35:24 lha Exp $");
#endif

static void
generate_random_file (const char *filename, size_t sz)
{
    int fd;
    char *buf;
    int i;

    buf = malloc (sz);
    if (buf == NULL)
	err (1, "malloc %u", sz);

    fd = open (filename, O_WRONLY | O_CREAT, 0666);
    if (fd < 0)
	err (1, "open %s", filename);

    for (i = 0; i < sz; ++i)
	buf[i] = rand();

    if (write (fd, buf, sz) != sz)
	err (1, "write");
    if (close (fd))
	err (1, "close");
    free (buf);
}

static char *
read_file (int fd, size_t sz)
{
    char *buf;

    buf = malloc (sz);
    if (buf == NULL)
	err (1, "malloc %u", sz);
    if (read (fd, buf, sz) != sz)
	err (1, "read");
    return buf;
}

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

static void *
mmap_file (int fd, size_t sz)
{
    void *ret;

    ret = mmap (0, sz, PROT_READ, MAP_SHARED, fd, 0);
    if (ret == (void *)MAP_FAILED)
	err (1, "mmap");
    return ret;
}

int
main (int argc, char **argv)
{
    const char *file = "foo";
    const size_t sz  = 16384;
    char *malloc_buf;
    void *mmap_buf;
    int fd;

    set_progname (argv[0]);

    srand (time(NULL));

    generate_random_file (file, sz);

    fd = open (file, O_RDONLY, 0);
    if (fd < 0)
	err (1, "open %s", file);

    malloc_buf = read_file (fd, sz);
    mmap_buf   = mmap_file (fd, sz);
    close (fd);
    unlink (file);
    if (memcmp (malloc_buf, mmap_buf, sz) != 0)
	return 1;
    return 0;
}
