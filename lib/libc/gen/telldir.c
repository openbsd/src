/*	$OpenBSD: telldir.c,v 1.13 2008/05/01 19:49:18 otto Exp $ */
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
#include <sys/queue.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread_private.h"
#include "telldir.h"

int _readdir_unlocked(DIR *, struct dirent **, int);

/*
 * return a pointer into a directory
 */
long
_telldir_unlocked(DIR *dirp)
{
	long i;
	struct ddloc *lp;

	i = dirp->dd_td->td_last;
	lp = &dirp->dd_td->td_locs[i];

	/* return previous telldir, if there */
	for (; i < dirp->dd_td->td_loccnt; i++, lp++) {
		if (lp->loc_seek == dirp->dd_seek && 
		    lp->loc_loc == dirp->dd_loc) {
			dirp->dd_td->td_last = i;
			return (i);
		}
	}

	if (dirp->dd_td->td_loccnt == dirp->dd_td->td_sz) {
		size_t newsz = dirp->dd_td->td_sz * 2 + 1;
		struct ddloc *p;
		p = realloc(dirp->dd_td->td_locs, newsz * sizeof(*p));
		if (p == NULL)
			return (-1);
		dirp->dd_td->td_sz = newsz;
		dirp->dd_td->td_locs = p;
		lp = &dirp->dd_td->td_locs[i];
	}
	dirp->dd_td->td_loccnt++;
	lp->loc_seek = dirp->dd_seek;
	lp->loc_loc = dirp->dd_loc;
	dirp->dd_td->td_last = i;
	return (i);
}

long
telldir(DIR *dirp)
{
	long i;

	_MUTEX_LOCK(&dirp->dd_lock);
	i = _telldir_unlocked(dirp);
	_MUTEX_UNLOCK(&dirp->dd_lock);

	return (i);
}

/*
 * seek to an entry in a directory.
 * Only values returned by "telldir" should be passed to seekdir.
 */
void
__seekdir(DIR *dirp, long loc)
{
	struct ddloc *lp;
	struct dirent *dp;

	if (loc < 0 || loc >= dirp->dd_td->td_loccnt)
		return;
	lp = &dirp->dd_td->td_locs[loc];
	dirp->dd_td->td_last = loc;
	if (lp->loc_loc == dirp->dd_loc && lp->loc_seek == dirp->dd_seek)
		return;
	(void) lseek(dirp->dd_fd, (off_t)lp->loc_seek, SEEK_SET);
	dirp->dd_seek = lp->loc_seek;
	dirp->dd_loc = 0;
	while (dirp->dd_loc < lp->loc_loc) {
		_readdir_unlocked(dirp, &dp, 0);
		if (dp == NULL)
			break;
	}
}
