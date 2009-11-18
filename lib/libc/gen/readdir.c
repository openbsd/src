/*	$OpenBSD: readdir.c,v 1.15 2009/11/18 07:43:22 guenther Exp $ */
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

#include <dirent.h>
#include "thread_private.h"

/*
 * get next entry in a directory.
 */
int
_readdir_unlocked(DIR *dirp, struct dirent **result, int skipdeleted)
{
	struct dirent *dp;

	*result = NULL;
	for (;;) {
		if (dirp->dd_loc >= dirp->dd_size)
			dirp->dd_loc = 0;
		if (dirp->dd_loc == 0) {
			dirp->dd_size = getdirentries(dirp->dd_fd,
			    dirp->dd_buf, dirp->dd_len, &dirp->dd_seek);
			if (dirp->dd_size == 0)
				return (0);
			if (dirp->dd_size < 0)
				return (-1);
		}
		dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);
		if ((long)dp & 03)	/* bogus pointer check */
			return (-1);
		if (dp->d_reclen <= 0 ||
		    dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc)
			return (-1);
		dirp->dd_loc += dp->d_reclen;
		/*
		 * When called from seekdir(), we let it decide on
		 * the end condition to avoid overshooting: the next
		 * readdir call should produce the next non-deleted entry,
		 * and we already advanced dd_loc.
		 */
		if (dp->d_ino == 0 && skipdeleted)
			continue;
		*result = dp;
		return (0);
	}
}

struct dirent *
readdir(DIR *dirp)
{
	struct dirent *dp;

	_MUTEX_LOCK(&dirp->dd_lock);
	_readdir_unlocked(dirp, &dp, 1);
	_MUTEX_UNLOCK(&dirp->dd_lock);

	return (dp);
}
