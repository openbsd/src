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

#ifdef RCSID
RCSID("$KTH: utime-file.c,v 1.2 2000/12/01 15:01:21 lha Exp $");
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <roken.h>
#include <err.h>

int
main (int argc, char ** argv)
{
    int len;
    int ret;
    int fd;
    char *filename = "foo";
    char *buf;
    struct stat sb;
    struct utimbuf t;

    switch (argc) {
    case 1:
	len = 8 * 1024; break;
    case 2:
	len = atoi(argv[1]);
	if (len == 0)
	    errx (1, "invalid len");
    default:
	errx (1, "argv != [12]");
    }

    buf = malloc (len);
    memset (buf, 'a', len);

    fd = open (filename, O_RDWR|O_CREAT|O_EXCL, 0744);
    if (fd < 0)
	errx (1, "open");
    ret = fstat (fd, &sb);
    if (ret < 0)
	errx (1, "open");

    ret = ftruncate (fd, len);
    fstat (fd, &sb);
    lseek (fd, 0, SEEK_SET);
    write (fd, buf, len);
    fstat (fd, &sb);

    t.modtime = t.actime = time (NULL); 
    utime (filename, &t);

    close (fd);
    free (buf);

    return 0;
}
