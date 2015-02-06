/*      $OpenBSD: regular.c,v 1.12 2015/02/06 23:21:59 millert Exp $      */
/*      $NetBSD: regular.c,v 1.2 1995/09/08 03:22:59 tls Exp $      */

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "extern.h"

#define	MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

void
c_regular(int fd1, char *file1, off_t skip1, off_t len1,
    int fd2, char *file2, off_t skip2, off_t len2)
{
	u_char ch, *p1, *p2;
	off_t byte, length, line;
	int dfound;

	if (sflag && len1 != len2)
		exit(1);

	if (skip1 > len1)
		eofmsg(file1);
	len1 -= skip1;
	if (skip2 > len2)
		eofmsg(file2);
	len2 -= skip2;

	length = MINIMUM(len1, len2);
	if (length > SIZE_MAX) {
	mmap_failed:
		c_special(fd1, file1, skip1, fd2, file2, skip2);
		return;
	}

	if ((p1 = mmap(NULL, (size_t)length, PROT_READ,
	    MAP_PRIVATE, fd1, skip1)) == MAP_FAILED)
		goto mmap_failed;
	if ((p2 = mmap(NULL, (size_t)length, PROT_READ,
	    MAP_PRIVATE, fd2, skip2)) == MAP_FAILED) {
		munmap(p1, (size_t)length);
		goto mmap_failed;
	}
	if (length) {
		madvise(p1, length, MADV_SEQUENTIAL);
		madvise(p2, length, MADV_SEQUENTIAL);
	}

	dfound = 0;
	for (byte = line = 1; length--; ++p1, ++p2, ++byte) {
		if ((ch = *p1) != *p2) {
			if (lflag) {
				dfound = 1;
				(void)printf("%6lld %3o %3o\n", (long long)byte,
				    ch, *p2);
			} else
				diffmsg(file1, file2, byte, line);
				/* NOTREACHED */
		}
		if (ch == '\n')
			++line;
	}

	if (len1 != len2)
		eofmsg (len1 > len2 ? file2 : file1);
	if (dfound)
		exit(DIFF_EXIT);
}
