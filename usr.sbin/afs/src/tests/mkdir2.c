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
RCSID("$KTH: mkdir2.c,v 1.3 2000/10/03 00:34:47 lha Exp $");
#endif

int
main(int argc, char *argv[])
{
    int ret;
    struct stat dot_sb, sb;

    set_progname (argv[0]);

    ret = mkdir ("foo", 0777);
    if (ret < 0)
	err (1, "mkdir foo");
    ret = lstat (".", &dot_sb);
    if (ret < 0)
	err (1, "lstat .");
    ret = lstat ("foo", &sb);
    if (ret < 0)
	err (1, "lstat foo");
    if (sb.st_nlink != 2)
	errx (1, "sb.st_link != 2");
    ret = lstat ("foo/.", &sb);
    if (ret < 0)
	err (1, "lstat foo/.");
    if (sb.st_nlink != 2)
	errx (1, "sb.st_link != 2");
    ret = lstat ("foo/..", &sb);
    if (ret < 0)
	err (1, "lstat foo");
    if (sb.st_nlink != dot_sb.st_nlink)
	errx (1, "sb.st_link != dot_sb.st_nlink");
    ret = rmdir ("foo");
    if (ret < 0)
	err (1, "rmdir foo");
    return 0;
}
