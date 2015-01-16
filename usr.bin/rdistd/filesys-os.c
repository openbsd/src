/*	$OpenBSD: filesys-os.c,v 1.12 2015/01/16 06:40:11 deraadt Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
#include <sys/mount.h>

#include "defs.h"

/*
 * OS specific file system routines
 */

static struct statfs   *mnt = NULL;
static int 		entries_left;

/*
 * getfsstat() version of get mount info routines.
 */
int
setmountent(void)
{
	long size;

	size = getfsstat(NULL, 0, MNT_WAIT);
	if (size == -1)
		return (0);

	free(mnt);
	size *= sizeof(struct statfs);
	mnt = xmalloc(size);

	entries_left = getfsstat(mnt, size, MNT_WAIT);
	if (entries_left == -1)
		return (0);

	return (1);
}

/*
 * getfsstat() version of getmountent()
 */
mntent_t *
getmountent(void)
{
	static mntent_t mntstruct;
	static char remote_dev[HOST_NAME_MAX+1 + PATH_MAX + 1];

	if (!entries_left)
		return (NULL);

	memset(&mntstruct, 0, sizeof(mntstruct));

	if (mnt->f_flags & MNT_RDONLY)
		mntstruct.me_flags |= MEFLAG_READONLY;

	if (strcmp(mnt->f_fstypename, "nfs") == 0) {
		strlcpy(remote_dev, mnt->f_mntfromname, sizeof(remote_dev));
		mntstruct.me_path = remote_dev;
		mntstruct.me_type = METYPE_NFS;
	} else {
		mntstruct.me_path = mnt->f_mntonname;
		mntstruct.me_type = METYPE_OTHER;
	}

	mnt++;
	entries_left--;

	return (&mntstruct);
}

/*
 * Done with iterations
 */
void
endmountent(void)
{
	free(mnt);
	mnt = NULL;
}

/*
 * Make a new (copy) of a mntent structure.
 */
mntent_t *
newmountent(const mntent_t *old)
{
	mntent_t *new;

	new = xmalloc(sizeof *new);
	new->me_path = xstrdup(old->me_path);
	new->me_type = xstrdup(old->me_type);
	new->me_flags = old->me_flags;

	return (new);
}
