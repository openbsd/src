/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <roken.h>

#ifdef RCSID
RCSID("$KTH: hardlink1.c,v 1.5 2000/10/03 00:34:04 lha Exp $");
#endif

int
main(int argc, char *argv[])
{
    int fd1, fd2;
    int ret;
    struct stat sb1, sb2;

    set_progname (argv[0]);

    fd1 = open("foo", O_RDWR|O_CREAT, 0666);
    if (fd1 < 0)
	err (1, "open foo");

    ret = fstat (fd1, &sb1);
    if (ret < 0)
	err (1, "stat foo");

    if (sb1.st_nlink != 1)
	errx (1, "foo.st_nlink != 1");

    ret = link ("foo", "bar");
    if (ret < 0)
	err (1, "link foo, bar");

    ret = fstat (fd1, &sb1);
    if (ret < 0)
	err (1, "stat foo");

    ret = lstat ("bar", &sb2);
    if (ret < 0)
	err (1, "stat bar");

    if (sb1.st_nlink != 2)
	errx (1, "foo.st_nlink != 2");

    if (sb2.st_nlink != 2)
	errx (1, "bar.st_nlink != 2");

    if (sb1.st_dev    != sb2.st_dev
	|| sb1.st_ino != sb2.st_ino)
	errx (1, "dev and ino differ");

    fd2 = open("bar", O_RDONLY, 0);
    if (fd2 < 0)
	err (1, "open bar");

    ret = fstat (fd2, &sb2);
    if (ret < 0)
	err (1, "fstat bar");
    
    if (sb2.st_nlink != 2)
	errx (1, "bar.st_nlink != 2");

    if (write (fd1, "hej", 3) != 3)
	errx (1, "write to foo");

    ret = fstat (fd1, &sb1);
    if (ret < 0)
	err (1, "stat foo");

    if (sb1.st_size != 3)
	errx (1, "foo.st_size != 3");

    ret = close (fd1);
    if (ret < 0)
	err (1, "close foo");

    ret = fstat (fd2, &sb2);
    if (ret < 0)
	err (1, "fstat bar");

    if (sb2.st_size != 3)
	errx (1, "bar.st_size != 3");

    if (unlink ("foo") < 0)
	err (1, "unlink foo");

    ret = fstat (fd2, &sb2);
    if (ret < 0)
	err (1, "fstat bar");
    
    if (sb2.st_nlink != 1)
	errx (1, "bar.st_nlink != 1");

    if (unlink ("bar") < 0)
	err (1, "unlink bar");

    ret = fstat (fd2, &sb2);
    if (ret < 0)
	err (1, "fstat bar");
    
    if (sb2.st_nlink != 0)
	errx (1, "bar.st_nlink != 0");

    ret = close (fd2);
    if (ret < 0)
	err (1, "close bar");
    return 0;
}
