/*	$OpenBSD: dir.c,v 1.11 2006/03/24 20:00:35 otto Exp $	*/
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = ": opendir.c,v 1.6 1998/08/15 08:10:14 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

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
	int incr;

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

	/*
	 * If the machine's page size is an exact multiple of DIRBLKSIZ,
	 * use a buffer that is cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdirentries()
	 * - not done in ld.so.
	 */
	incr = DIRBLKSIZ;

	/* UNION mount support removed */

	dirp->dd_len = incr;
	dirp->dd_buf = _dl_malloc(dirp->dd_len);
	if (dirp->dd_buf == NULL) {
		_dl_free(dirp);
		_dl_close (fd);
		return (NULL);
	}
	dirp->dd_seek = 0;
	dirp->dd_loc = 0;
	dirp->dd_fd = fd;
	dirp->dd_flags = DTF_NODUP;

	return (dirp);
}


#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$ closedir.c,v 1.3 1998/11/20 11:18:37 d Exp $";
#endif /* LIBC_SCCS and not lint */
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


#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$ readdir.c,v 1.5 2001/05/17 20:20:36 rees Exp $";
#endif /* LIBC_SCCS and not lint */
/*
 * get next entry in a directory.
 */
struct dirent *
_dl_readdir(DIR *dirp)
{
	struct dirent *dp;

	for (;;) {
		if (dirp->dd_loc >= dirp->dd_size) {
			if (dirp->dd_flags & __DTF_READALL)
				return (NULL);
			dirp->dd_loc = 0;
		}
		if (dirp->dd_loc == 0 && !(dirp->dd_flags & __DTF_READALL)) {
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
