/*	$OpenBSD: opendir.c,v 1.20 2010/01/19 17:12:43 millert Exp $ */
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

#include "telldir.h"

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
	int pageoffset;

	if ((fd = open(name, O_RDONLY | O_NONBLOCK)) == -1)
		return (NULL);
	if (fstat(fd, &sb)) {
		close(fd);
		return (NULL);
	}
	if (!S_ISDIR(sb.st_mode)) {
		close(fd);
		errno = ENOTDIR;
		return (NULL);
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 ||
	    (dirp = malloc(sizeof(DIR) + sizeof(struct _telldir))) == NULL) {
		close(fd);
		return (NULL);
	}

	dirp->dd_td = (struct _telldir *)((char *)dirp + sizeof(DIR));
	dirp->dd_td->td_locs = NULL;
	dirp->dd_td->td_sz = 0;
	dirp->dd_td->td_loccnt = 0;
	dirp->dd_td->td_last = 0;

	/*
	 * Use a buffer that is page aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdirentries()
	 */
	pageoffset = getpagesize() - 1;
	dirp->dd_len = ((int)sb.st_blksize + pageoffset) & ~pageoffset;
	dirp->dd_buf = malloc((size_t)dirp->dd_len);
	if (dirp->dd_buf == NULL) {
		free(dirp);
		close(fd);
		return (NULL);
	}

	dirp->dd_seek = 0;
	dirp->dd_loc = 0;
	dirp->dd_fd = fd;
	dirp->dd_flags = flags;
	dirp->dd_lock = NULL;

	/*
	 * Set up seek point for rewinddir.
	 */
	dirp->dd_rewind = telldir(dirp);

	return (dirp);
}
