/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <roken.h>

#include <err.h>

RCSID("$KTH: still-there-p.c,v 1.1.2.1 2001/04/28 22:09:39 lha Exp $");

#define TEST_BUFFER_SZ		(1024*8)

int
main(int argc, char **argv)
{
    const char *file = "foo";
    char otherbuf[TEST_BUFFER_SZ];
    char buf[TEST_BUFFER_SZ];
    int fd;
	
    set_progname (argv[0]);

    fd = open (file, O_RDWR|O_TRUNC|O_CREAT, 0644);
    if (fd < 0)
	err(1, "open(%s)", file);

    if (write (fd, buf, sizeof(buf)) != sizeof(buf))
	errx(1, "write");

    while (1) {
	if (lseek(fd, 0, SEEK_SET) < 0)
	    err(1, "lseek");

	if (read(fd, otherbuf, sizeof(otherbuf)) != sizeof(otherbuf)) {
	    struct stat sb;

	    if (fstat(fd, &sb) < 0)
		err(1, "fstat");
	    printf("size: %d\n", (int)sb.st_size);
	    printf ("lseek(SEEK_CUR): %d\n", (int)lseek(fd, 0, SEEK_CUR));
	    errx(1, "read");
	}

	if (memcmp(buf, otherbuf, sizeof(buf)) != 0)
	    errx (1, "buf != otherbuf");
    }
    close(fd);

    return 0;
}
