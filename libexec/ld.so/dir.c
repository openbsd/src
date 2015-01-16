/*	$OpenBSD: dir.c,v 1.23 2015/01/16 16:18:07 deraadt Exp $	*/
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

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "syscall.h"
#include "archdep.h"
#include "util.h"
#include "dir.h"

struct _dl_dirdesc {
	long	dd_loc;		/* offset in current buffer */
	long	dd_size;	/* amount of data returned by getdents() */
	char	*dd_buf;	/* data buffer */
	int	dd_len;		/* size of data buffer */
	int	dd_fd;		/* file descriptor associated with directory */
};

/*
 * Open a directory.
 */
_dl_DIR *
_dl_opendir(const char *name)
{
	_dl_DIR *dirp;
	int fd;
	struct stat sb;

	if ((fd = _dl_open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC)) < 0)
		return (NULL);
	if (_dl_fstat(fd, &sb) || (dirp = _dl_malloc(sizeof(*dirp))) == NULL) {
		_dl_close(fd);
		return (NULL);
	}

	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	dirp->dd_size = 0;
	dirp->dd_len = _dl_round_page(sb.st_blksize);
	dirp->dd_buf = _dl_malloc(dirp->dd_len);
	if (dirp->dd_buf == NULL) {
		_dl_free(dirp);
		_dl_close (fd);
		return (NULL);
	}

	return (dirp);
}


/*
 * close a directory.
 */
int
_dl_closedir(_dl_DIR *dirp)
{
	int ret;

	ret = _dl_close(dirp->dd_fd);
	_dl_free(dirp->dd_buf);
	_dl_free(dirp);
	return ret;
}


/*
 * get next entry in a directory.
 */
struct dirent *
_dl_readdir(_dl_DIR *dirp)
{
	struct dirent *dp;

	for (;;) {
		if (dirp->dd_loc >= dirp->dd_size) {
			dirp->dd_loc = 0;
		}
		if (dirp->dd_loc == 0) {
			dirp->dd_size = _dl_getdents(dirp->dd_fd,
			    dirp->dd_buf, dirp->dd_len);
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
