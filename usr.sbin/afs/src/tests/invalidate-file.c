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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>

#include <atypes.h>

#include <kafs.h>

#include <fs.h>
#include <arlalib.h>

#include <err.h>
#include <roken.h>

RCSID("$KTH: invalidate-file.c,v 1.3 2000/10/16 22:01:08 assar Exp $");

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

static void
create_write_file (char *filename)
{
    int ret;
    int fd;

    fs_invalidate (filename);

    fd = open(filename, O_RDWR|O_CREAT, 0666);
    if (fd < 0)
	err (1, "open(rw): %s", filename);

    ret = write (fd, "foo", 3);
    if (ret < 0)
	err (1, "write");
    
    fs_invalidate (filename);
    
    ret = write (fd, "foo", 3);
    if (ret < 0)
	err (1, "write2");
    
    ret = close (fd);
    if (ret < 0)
	err (1, "close");
}

static void
read_file (char *filename)
{
    int ret;
    int fd;
    char buf[3];

    fs_invalidate (filename);

    fd = open(filename, O_RDONLY, 0666);
    if (fd < 0)
	err (1, "open(ro)");

    ret = read (fd, buf, sizeof(buf));
    if (ret < 0)
	err (1, "read");
    
    fs_invalidate (filename);
    
    ret = read (fd, buf, sizeof(buf));
    if (ret < 0)
	err (1, "read");
    
    ret = close (fd);
    if (ret < 0)
	err (1, "close");
}

static void
mmap_read_file (char *filename)
{
    int fd;
    void *v;
    char buf[6];

    fs_invalidate (filename);

    fd = open(filename, O_RDONLY, 0666);
    if (fd < 0)
	err (1, "open(ro-mmap)");

    v = mmap (NULL, 6, PROT_READ, MAP_SHARED, fd, 0);
    if (v == (void *)MAP_FAILED)
	err (1, "mmap(ro) %s", filename);

    memcpy (buf, v, 3);
    fs_invalidate (filename);
    memcpy (buf, v, 3);

    munmap (v, 6);
}

static void
mmap_write_file (char *filename)
{
    int fd;
    void *v;

    fs_invalidate (filename);

    fd = open (filename, O_RDWR, 0666);
    if (fd < 0)
	err (1, "open(rw-mmap)");

    v = mmap (NULL, 6, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (v == (void *)MAP_FAILED)
	err (1, "mmap(rw) %s", filename);

    memcpy (v, "foo", 3);
    fs_invalidate (filename);
    memcpy (v, "foo", 3);

    munmap (v, 6);
    close (fd);
}

int
main(int argc, char **argv)
{
    char *filename = "foo";
    
    set_progname (argv[0]);

    if (!k_hasafs())
	exit (1);
    
    create_write_file (filename);
    read_file (filename);
    read_file (filename);
    read_file (filename);
    read_file (filename);
    mmap_read_file (filename);
    mmap_read_file (filename);
    mmap_read_file (filename);
    mmap_read_file (filename);
    mmap_write_file (filename);
    mmap_write_file (filename);
    mmap_write_file (filename);
    mmap_write_file (filename);
    
    return 0;
}
