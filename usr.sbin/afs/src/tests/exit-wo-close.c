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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <err.h>
#include <roken.h>

#ifdef RCSID
RCSID("$KTH: exit-wo-close.c,v 1.4 2000/10/03 00:33:44 lha Exp $");
#endif

static int 
child (const char *filename)
{
    int fd;
    int ret;

    fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
	err (1, "open %s", filename);
    ret = write (fd, "hej", 3);
    if (ret != 3)
	err (1, "write %s", filename);
    return 0;
}

static int 
parent (const char *filename, pid_t child_pid)
{
    int stat;
    int ret;
    int fd;
    struct stat sb;
    char buf[3];

    ret = waitpid (child_pid, &stat, 0);
    if (ret < 0)
	err (1, "waitpid %u", (unsigned)child_pid);
    if (!WIFEXITED(stat) || WEXITSTATUS(stat) != 0)
	errx (1, "weird child %u", (unsigned)child_pid);
    fd = open (filename, O_RDONLY, 0);
    if (fd < 0)
	err (1, "open %s", filename);
    ret = fstat (fd, &sb);
    if (ret < 0)
	err (1, "fstat %s", filename);
    if (sb.st_size != 3)
	errx (1, "size of %s = %u != 3", filename, (unsigned)sb.st_size);
    ret = read (fd, buf, sizeof(buf));
    if (ret < 0)
	err (1, "read %s", filename);
    if (ret != 3)
	errx (1, "short read from %s", filename);
    if (memcmp (buf, "hej", 3) != 0)
	errx (1, "bad contents of %s = `%.3s'", filename, buf);
    close (fd);
    return 0;
}

static int
doit (const char *filename)
{
    pid_t pid;

    pid = fork ();
    if (pid < 0)
	err (1, "fork");

    if (pid == 0)
	return child (filename);
    else
	return parent (filename, pid);
}

int
main(int argc, char **argv)
{
    const char *file = "foo";

    set_progname (argv[0]);

    if (argc != 2 && argc != 1)
	errx (1, "usage: %s [file]", argv[0]);
    if (argc == 2)
	file = argv[1];
    return doit (file);
}
