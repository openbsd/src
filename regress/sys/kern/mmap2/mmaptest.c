/*	$OpenBSD: mmaptest.c,v 1.2 2002/01/03 15:07:05 art Exp $	*/
/*
 * Copyright (c) 2001 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <string.h>

#include <sys/types.h>
#include <sys/mman.h>

/*
 * Map the same object in two places in memory.
 * Should cause a cache alias on sparc.
 */

#define MAGIC "The voices in my head are trying to ignore me."

int
main()
{
	char fname[25] = "/tmp/mmaptestXXXXXXXXXX";
	int page_size;
	int fd;
	char *v1, *v2;

	if ((fd = mkstemp(fname)) < 0)
		err(1, "mkstemp");

	if (remove(fname) < 0)
		err(1, "remove");

	if ((page_size = sysconf(_SC_PAGESIZE)) < 0)
		err(1, "sysconf");

	if (ftruncate(fd, 2 * page_size) < 0)
		err(1, "ftruncate");

	/* map two pages, then map the first page over the second */

	v1 = mmap(NULL, 2 * page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (v1 == MAP_FAILED)
		err(1, "mmap 1");

	/* No need to unmap, mmap is supposed to do that for us if MAP_FIXED */

	v2 = mmap(v1 + page_size, page_size, PROT_READ|PROT_WRITE,
	    MAP_SHARED|MAP_FIXED, fd, 0);
	if (v2 == MAP_FAILED)
		err(1, "mmap 2");

	memcpy(v1, MAGIC, sizeof(MAGIC));

	if (memcmp(v2, MAGIC, sizeof(MAGIC)) != 0)
		err(1, "comparsion 1 failed");

	if (memcmp(v1, v2, sizeof(MAGIC)) != 0)
		err(1, "comparsion 2 failed");

	if (munmap(v1, 2 * page_size) < 0)
		err(1, "munmap");

	close(fd);

	return 0;
}

