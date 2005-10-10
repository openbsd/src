/*	$OpenBSD: opendir.c,v 1.15 2005/10/10 17:37:43 espie Exp $ */
/*
 * Copyright (c) 1983, 1993
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

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Open a directory.
 */
DIR *
opendir(const char *name)
{

	return (__opendir2(name, DTF_NODUP));
}

DIR *
__opendir2(const char *name, int flags)
{
	DIR *dirp;
	int fd;
	struct stat sb;
	int pagesz;
	int incr;

	if ((fd = open(name, O_RDONLY | O_NONBLOCK)) == -1)
		return (NULL);
	if (fstat(fd, &sb) || !S_ISDIR(sb.st_mode)) {
		close(fd);
		errno = ENOTDIR;
		return (NULL);
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 ||
	    (dirp = (DIR *)malloc(sizeof(DIR))) == NULL) {
		close(fd);
		return (NULL);
	}

	/*
	 * If the machine's page size is an exact multiple of DIRBLKSIZ,
	 * use a buffer that is cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdirentries()
	 */
	if (((pagesz = getpagesize()) % DIRBLKSIZ) == 0)
		incr = pagesz;
	else
		incr = DIRBLKSIZ;

	dirp->dd_len = incr;
	dirp->dd_buf = malloc(dirp->dd_len);
	if (dirp->dd_buf == NULL) {
		free(dirp);
		close (fd);
		return (NULL);
	}

	dirp->dd_seek = 0;
	dirp->dd_loc = 0;
	dirp->dd_fd = fd;
	dirp->dd_flags = flags;

	/*
	 * Set up seek point for rewinddir.
	 */
	dirp->dd_rewind = telldir(dirp);

	return (dirp);
}
