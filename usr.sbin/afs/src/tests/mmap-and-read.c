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
RCSID("$KTH: mmap-and-read.c,v 1.12 2000/12/18 04:03:51 assar Exp $");
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

static char *
generate_random_file (const char *filename,
		      unsigned npages,
		      unsigned pagesize,
		      int writep)
{
    int fd;
    char *buf, *fbuf;
    int i;
    int prot;
    int flags;
    size_t sz = npages * pagesize;

    buf = malloc (sz);
    if (buf == NULL)
	err (1, "malloc %u", sz);

    for (i = 0; i < npages; ++i)
	memset (buf + pagesize * i, '0' + i, pagesize);

    fd = open (filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
	err (1, "open %s", filename);

    if (ftruncate (fd, sz) < 0)
	err (1, "ftruncate");

    prot = PROT_READ | PROT_WRITE;
    flags = MAP_SHARED;

    fbuf = mmap (0, sz, prot, flags, fd, 0);
    if (fbuf == (void *)MAP_FAILED)
	err (1, "mmap");

    if (writep) {
	if(write(fd, "hej\n", 4) != 4)
	    err(1, "write");
    }

    memcpy (fbuf, buf, sz);

#if 0
    if (msync (fbuf, sz, MS_SYNC))
	err(1, "msync");
#endif

    if (munmap (fbuf, sz) != 0)
	err (1, "munmap");

    if (close (fd))
	err (1, "close");
    return buf;
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

static int
test (const char *file, int writep)
{
    const size_t sz  = 4 * getpagesize();
    char *buf;
    char *malloc_buf;
    int fd;
    int ret;

    buf = generate_random_file (file, 4, getpagesize(), writep);

    fd = open (file, O_RDONLY, 0);
    if (fd < 0)
	err (1, "open %s", file);

    malloc_buf = read_file (fd, sz);
    close (fd);
    ret = memcmp (buf, malloc_buf, sz);
    free (buf);
    
    return ret;
}


int
main (int argc, char **argv)
{

    set_progname (argv[0]);

    srand (time(NULL));

    if (test ("foo", 1) != 0)
	errx (1, "test(1)");
    if (test ("bar", 0) != 0)
	errx (1, "test(2)");

    return 0;
}
