/*	$OpenBSD: vfs_conf.c,v 1.40 2010/12/21 20:14:43 thib Exp $	*/
/*	$NetBSD: vfs_conf.c,v 1.21.4.1 1995/11/01 00:06:26 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *
 *	@(#)vfs_conf.c	8.8 (Berkeley) 3/31/94
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/timeout.h>

#ifdef FFS
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ffs/ffs_extern.h>
#endif

#ifdef EXT2FS
#include <ufs/ext2fs/ext2fs_extern.h>
#endif

#ifdef CD9660
#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_extern.h>
#endif

#ifdef MFS
#include <ufs/mfs/mfs_extern.h>
#endif

#ifdef NFSCLIENT
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#endif

/*
 * This defines the root filesystem.
 */
struct vnode *rootvnode;

/*
 * Set up the filesystem operations for vnodes.
 * The types are defined in mount.h.
 */


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

#ifdef PORTAL
extern	const struct vfsops portal_vfsops;
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

/*
 * Set up the filesystem operations for vnodes.
 */
static struct vfsconf vfsconflist[] = {

        /* Fast Filesystem */
#ifdef FFS
        { &ffs_vfsops, MOUNT_FFS, 1, 0, MNT_LOCAL, NULL },
#endif

        /* Memory-based Filesystem */
#ifdef MFS
        { &mfs_vfsops, MOUNT_MFS, 3, 0, MNT_LOCAL, NULL },
#endif

#ifdef EXT2FS
	{ &ext2fs_vfsops, MOUNT_EXT2FS, 17, 0, MNT_LOCAL, NULL },
#endif
        /* ISO9660 (aka CDROM) Filesystem */
#ifdef CD9660
        { &cd9660_vfsops, MOUNT_CD9660, 14, 0, MNT_LOCAL, NULL },
#endif

        /* MSDOS Filesystem */
#ifdef MSDOSFS
        { &msdosfs_vfsops, MOUNT_MSDOS, 4, 0, MNT_LOCAL, NULL },
#endif

        /* Sun-compatible Network Filesystem */
#ifdef NFSCLIENT
        { &nfs_vfsops, MOUNT_NFS, 2, 0, 0, NULL },
#endif

	/* NNPFS */
#ifdef NNPFS
	{ &nnpfs_vfsops, MOUNT_NNPFS, 21, 0, 0, NULL },
#endif
	
        /* /proc Filesystem */
#ifdef PROCFS
        { &procfs_vfsops, MOUNT_PROCFS, 12, 0, 0, NULL },
#endif

        /* Portal Filesystem */
#ifdef PORTAL
        { &portal_vfsops, MOUNT_PORTAL, 8, 0, 0, NULL },
#endif

	/* NTFS Filesystem */
#ifdef NTFS
	{ &ntfs_vfsops, MOUNT_NTFS, 6, 0, MNT_LOCAL, NULL },
#endif

	/* UDF Filesystem */
#ifdef UDF
	{ &udf_vfsops, MOUNT_UDF, 13, 0, MNT_LOCAL, NULL },
#endif

};


/*
 * Initially the size of the list, vfs_init will set maxvfsconf
 * to the highest defined type number.
 */
int maxvfsconf = sizeof(vfsconflist) / sizeof(struct vfsconf);
struct vfsconf *vfsconf = vfsconflist;
