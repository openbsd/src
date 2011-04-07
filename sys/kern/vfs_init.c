/*	$OpenBSD: vfs_init.c,v 1.29 2011/04/07 13:42:53 thib Exp $	*/
/*	$NetBSD: vfs_init.c,v 1.6 1996/02/09 19:00:58 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
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
 *
 *	@(#)vfs_init.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/pool.h>

struct pool namei_pool;

/* This defines the root filesystem. */
struct vnode *rootvnode;

/* Set up the filesystem operations for vnodes. */
#ifdef FFS
extern	const struct vfsops ffs_vfsops;
#endif

#ifdef MFS
extern	const struct vfsops mfs_vfsops;
#endif

#ifdef MSDOSFS
extern	const struct vfsops msdosfs_vfsops;
#endif

#ifdef NFSCLIENT
extern	const struct vfsops nfs_vfsops;
#endif

#ifdef PROCFS
extern	const struct vfsops procfs_vfsops;
#endif

#ifdef CD9660
extern	const struct vfsops cd9660_vfsops;
#endif

#ifdef EXT2FS
extern	const struct vfsops ext2fs_vfsops;
#endif

#ifdef NNPFS
extern  const struct vfsops nnpfs_vfsops;
#endif

#ifdef NTFS
extern  const struct vfsops ntfs_vfsops;
#endif

#ifdef UDF
extern  const struct vfsops udf_vfsops;
#endif

/* Set up the filesystem operations for vnodes. */
static struct vfsconf vfsconflist[] = {
#ifdef FFS
        { &ffs_vfsops, MOUNT_FFS, 1, 0, MNT_LOCAL, NULL },
#endif

#ifdef MFS
        { &mfs_vfsops, MOUNT_MFS, 3, 0, MNT_LOCAL, NULL },
#endif

#ifdef EXT2FS
	{ &ext2fs_vfsops, MOUNT_EXT2FS, 17, 0, MNT_LOCAL, NULL },
#endif

#ifdef CD9660
        { &cd9660_vfsops, MOUNT_CD9660, 14, 0, MNT_LOCAL, NULL },
#endif

#ifdef MSDOSFS
        { &msdosfs_vfsops, MOUNT_MSDOS, 4, 0, MNT_LOCAL, NULL },
#endif

#ifdef NFSCLIENT
        { &nfs_vfsops, MOUNT_NFS, 2, 0, 0, NULL },
#endif

#ifdef NNPFS
	{ &nnpfs_vfsops, MOUNT_NNPFS, 21, 0, 0, NULL },
#endif

#ifdef PROCFS
        { &procfs_vfsops, MOUNT_PROCFS, 12, 0, 0, NULL },
#endif

#ifdef NTFS
	{ &ntfs_vfsops, MOUNT_NTFS, 6, 0, MNT_LOCAL, NULL },
#endif

#ifdef UDF
	{ &udf_vfsops, MOUNT_UDF, 13, 0, MNT_LOCAL, NULL },
#endif
};


/*
 * Initially the size of the list, vfsinit will set maxvfsconf
 * to the highest defined type number.
 */
int maxvfsconf = sizeof(vfsconflist) / sizeof(struct vfsconf);
struct vfsconf *vfsconf = vfsconflist;

/* Initialize the vnode structures and initialize each file system type. */
void
vfsinit(void)
{
	int i;
	struct vfsconf *vfsconflist;
	int vfsconflistlen;

	pool_init(&namei_pool, MAXPATHLEN, 0, 0, 0, "namei",
	    &pool_allocator_nointr);

	/* Initialize the vnode table. */
	vntblinit();

	/* Initialize the vnode name cache. */
	nchinit();

	/*
	 * Stop using vfsconf and maxvfsconf as a temporary storage,
	 * set them to their correct values now.
	 */
	vfsconflist = vfsconf;
	vfsconflistlen = maxvfsconf;
	vfsconf = NULL;
	maxvfsconf = 0;

	for (i = 0; i < vfsconflistlen; i++)
		vfs_register(&vfsconflist[i]);
}
