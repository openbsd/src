/*
 * This file is substantially duplicated from src/sys/ufs/lfs/lfs_vfsops.c:lfs_statfs().
 * That file's copyright is applied here.
 */
/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *      @(#)lfs_vfsops.c        8.20 (Berkeley) 6/10/95
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <fstab.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/dinode.h>
#include <ufs/lfs/lfs.h>

int		lfs_df __P((int, char *, struct statfs *));

extern int	bread __P((int, off_t, void *, int));
extern char	*getmntpt __P((char *));

union {
	struct lfs il_fs;
	char dummy[LFS_SBPAD];
} sb;
#define sblock sb.il_fs

int
lfs_df(rfd, file, sfsp)
	int rfd;
	char *file;
	struct statfs *sfsp;
{
	char *mntpt;

	if (bread(rfd, (off_t)LFS_LABELPAD, &sblock, LFS_SBPAD) == 0) {
		return (-1);
	}
	if (sblock.lfs_magic != LFS_MAGIC) {
		return (-1);
	}
	sfsp->f_type = 0;	/* Unused field, set to 0 */
	sfsp->f_flags = 0;	/* The fs is not mapped, so no flags */
	sfsp->f_bsize = sblock.lfs_bsize;
	sfsp->f_iosize = sblock.lfs_bsize;
	sfsp->f_blocks = dbtofrags(&sblock, sblock.lfs_dsize);
	sfsp->f_bfree = dbtofrags(&sblock, sblock.lfs_bfree);
	sfsp->f_bavail = sblock.lfs_dsize * (100 - sblock.lfs_minfree) / 100;
	sfsp->f_bavail = (sfsp->f_bavail > sblock.lfs_bfree) ?
					sblock.lfs_bfree : sfsp->f_bavail;
	sfsp->f_bavail = dbtofrags(&sblock, sfsp->f_bavail);
	if (sfsp->f_bavail < 0)
		sfsp->f_bavail = 0;

	sfsp->f_files = sblock.lfs_nfiles;
	sfsp->f_ffree = sblock.lfs_bfree * sblock.lfs_inopb;
	sfsp->f_fsid.val[0] = 0;
	sfsp->f_fsid.val[1] = 0;
	if ((mntpt = getmntpt(file)) == 0)
		mntpt = "";
	memmove(&sfsp->f_mntonname[0], mntpt, MNAMELEN);
	memmove(&sfsp->f_mntfromname[0], file, MNAMELEN);
	strncpy(sfsp->f_fstypename, MOUNT_LFS, MFSNAMELEN-1);
	sfsp->f_fstypename[MFSNAMELEN-1] = '\0';
	return (0);
}
