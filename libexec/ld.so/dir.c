/*	$OpenBSD: dir.c,v 1.15 2011/07/14 02:16:00 deraadt Exp $	*/
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "syscall.h"
#include "archdep.h"
#include "util.h"
#include "dir.h"

long _dl_telldir(const DIR *dirp);
void _dl_seekdir(DIR *dirp, long loc);

/*
 * Open a directory.
 */
DIR *
_dl_opendir(const char *name)
{
	DIR *dirp;
	int fd;
	struct stat sb;

	if ((fd = _dl_open(name, O_RDONLY | O_NONBLOCK)) < 0)
		return (NULL);
	if (_dl_fstat(fd, &sb) || !S_ISDIR(sb.st_mode)) {
		_dl_close(fd);
		return (NULL);
	}
	if (_dl_fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
	    (dirp = (DIR *)_dl_malloc(sizeof(DIR))) == NULL) {
		_dl_close(fd);
		return (NULL);
	}

	dirp->dd_len = _dl_round_page(sb.st_blksize);
	dirp->dd_buf = _dl_malloc(dirp->dd_len);
	if (dirp->dd_buf == NULL) {
		_dl_free(dirp);
		_dl_close (fd);
		return (NULL);
	}
	dirp->dd_seek = 0;
	dirp->dd_loc = 0;
	dirp->dd_fd = fd;

	return (dirp);
}


/*
 * close a directory.
 */
int
_dl_closedir(DIR *dirp)
{
	int fd;
	int ret;

	fd = dirp->dd_fd;
	dirp->dd_fd = -1;
	dirp->dd_loc = 0;
	_dl_free((void *)dirp->dd_buf);
	_dl_free((void *)dirp);
	ret = _dl_close(fd);
	return (ret);
}


/*
 * get next entry in a directory.
 */
struct dirent *
_dl_readdir(DIR *dirp)
{
	struct dirent *dp;

	for (;;) {
		if (dirp->dd_loc >= dirp->dd_size) {
			dirp->dd_loc = 0;
		}
		if (dirp->dd_loc == 0) {
			dirp->dd_size = _dl_getdirentries(dirp->dd_fd,
			    dirp->dd_buf, dirp->dd_len, &dirp->dd_seek);
			if (dirp->dd_size <= 0)
				return (NULL);
		}
		dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);
		if ((long)dp & 03)	/* bogus pointer check */
			return (NULL);
		if (dp->d_reclen <= 0 ||
		    dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc)
			return (NULL);
		dirp->dd_loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		return (dp);
	}
}
