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
#include <unistd.h>

#include <roken.h>

#include <err.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

RCSID("$KTH: make-page.c,v 1.2.2.1 2001/04/27 23:42:50 mattiasa Exp $");

int
main (int argc, char **argv)
{
    int mb = 10;
    int ret;
    size_t sz;
    void *mmap_buf;

    srand(getpid() * time(NULL));

    sz = mb * 1024 * 1024;

#ifdef MAP_ANON
    mmap_buf = mmap (NULL, sz, PROT_READ|PROT_WRITE,
		     MAP_ANON|MAP_PRIVATE, -1, 0);
#else
    {
	int fd;

	fd = open ("/dev/zero", O_RDWR);
	if (fd < 0)
	    err (1, "open /dev/zero");
	mmap_buf = mmap (NULL, sz, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE, fd, 0);
	close (fd);
    }
#endif
    if (mmap_buf == (void *)MAP_FAILED)
	err (1, "mmap");    

    while (1) {
	int *foo = (int *) ((unsigned char *) mmap_buf + (((rand() % (sz - 9)) + 7) & ~7));
	struct timeval tv = { 0, 1000} ;
	*foo = 10;
	select (0, NULL, NULL, NULL, &tv);
    }
    
    ret = munmap (mmap_buf, sz);
    if (ret)
	err (1, "munmap");
}
