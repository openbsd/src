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
#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <atypes.h>
#include <kafs.h>

#include <roken.h>

#include <err.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

RCSID("$KTH: write-closed2.c,v 1.5 2000/10/03 00:36:42 lha Exp $");

static int
set_acl (char *dir)
{
    struct ViceIoctl a_params;
    char *foo = "1\n0\nsystem:anyuser 0\n";

    a_params.in_size  = strlen(foo);
    a_params.out_size = 0;
    a_params.in       = foo;
    a_params.out      = NULL;

    return k_pioctl (dir, VIOCSETAL, &a_params, 1);
}

static void
doit (const char *filename)
{
    int fd;
    int ret;
    void *buf;

    ret = mkdir ("bad", 0777);
    if (ret < 0)
	err (1, "mkdir bad");

    ret = chdir ("bad");
    if (ret < 0)
	err (1, "chdir bad");

    fd = open (filename, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
	err (1, "open %s", filename);
    ret = ftruncate (fd, 1);
    if (ret < 0)
	err (1, "ftruncate %s", filename);
    buf = mmap (NULL, 1, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == (void *) MAP_FAILED)
	err (1, "mmap");
    ret = set_acl (".");
    if (ret < 0)
	err (1, "setacl failed");

    ret = close (fd);
    if (ret < 0)
	err (1, "close %s", filename);
    *((char *)buf) = 0x17;
    ret = munmap (buf, 1);
    if (ret < 0)
	err (1, "munmap");
}

int
main(int argc, char **argv)
{
    const char *file = "foo";

    set_progname (argv[0]);

    if (!k_hasafs())
	errx (1, "!k_hasafs");

    if (argc != 1 && argc != 2)
	errx (1, "usage: %s [file]", argv[0]);
    if (argc == 2)
	file = argv[1];
    doit (file);
    return 0;
}
