/*	$OpenBSD: ffs_softdep_stub.c,v 1.1 1998/03/15 03:53:51 millert Exp $	*/

/*
 * Copyright 1997 Marshall Kirk McKusick. All Rights Reserved.
 *
 * This code is derived from work done by Greg Ganger and Yale Patt at the
 * University of Michigan.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. None of the names of McKusick, Ganger, Patt, or the University of 
 *    Michigan may be used to endorse or promote products derived from 
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ffs_softdep.stub.c	9.1 (McKusick) 7/9/97
 */

#ifndef FFS_SOFTUPDATES

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_extern.h>

int
softdep_flushfiles(oldmnt, flags, p)
	struct mount *oldmnt;
	int flags;
	struct proc *p;
{

	panic("softdep_flushfiles called");
}

int
softdep_mount(devvp, mp, fs, cred)
	struct vnode *devvp;
	struct mount *mp;
	struct fs *fs;
	struct ucred *cred;
{

	return (0);
}

void 
softdep_initialize()
{

	return;
}

void
softdep_setup_inomapdep(bp, ip, newinum)
	struct buf *bp;
	struct inode *ip;
	ino_t newinum;
{

	panic("softdep_setup_inomapdep called");
}

void
softdep_setup_blkmapdep(bp, fs, newblkno)
	struct buf *bp;
	struct fs *fs;
	ufs_daddr_t newblkno;
{

	panic("softdep_setup_blkmapdep called");
}

void 
softdep_setup_allocdirect(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;
	ufs_lbn_t lbn;
	ufs_daddr_t newblkno;
	ufs_daddr_t oldblkno;
	long newsize;
	long oldsize;
	struct buf *bp;
{
	
	panic("softdep_setup_allocdirect called");
}

void
softdep_setup_allocindir_page(ip, lbn, bp, ptrno, newblkno, oldblkno, nbp)
	struct inode *ip;
	ufs_lbn_t lbn;
	struct buf *bp;
	int ptrno;
	ufs_daddr_t newblkno;
	ufs_daddr_t oldblkno;
	struct buf *nbp;
{

	panic("softdep_setup_allocindir_page called");
}

void
softdep_setup_allocindir_meta(nbp, ip, bp, ptrno, newblkno)
	struct buf *nbp;
	struct inode *ip;
	struct buf *bp;
	int ptrno;
	ufs_daddr_t newblkno;
{

	panic("softdep_setup_allocindir_meta called");
}

void
softdep_setup_freeblocks(ip, length)
	struct inode *ip;
	off_t length;
{
	
	panic("softdep_setup_freeblocks called");
}

void
softdep_freefile(ap)
	struct vop_vfree_args /* {
		struct vnode *a_pvp;
		ino_t a_ino;
		int a_mode;
	} */ *ap;
{

	panic("softdep_freefile called");
}

void 
softdep_setup_directory_add(bp, dp, diroffset, newinum, newdirbp)
	struct buf *bp;
	struct inode *dp;
	off_t diroffset;
	long newinum;
	struct buf *newdirbp;
{

	panic("softdep_setup_directory_add called");
}

void 
softdep_change_directoryentry_offset(dp, base, oldloc, newloc, entrysize)
	struct inode *dp;
	caddr_t base;
	caddr_t oldloc;
	caddr_t newloc;
	int entrysize;
{

	panic("softdep_change_directoryentry_offset called");
}

void 
softdep_setup_remove(bp, dp, ip, isrmdir)
	struct buf *bp;
	struct inode *dp;
	struct inode *ip;
	int isrmdir;
{
	
	panic("softdep_setup_remove called");
}

void 
softdep_setup_directory_change(bp, dp, ip, newinum, isrmdir)
	struct buf *bp;
	struct inode *dp;
	struct inode *ip;
	long newinum;
	int isrmdir;
{

	panic("softdep_setup_directory_change called");
}

void
softdep_increase_linkcnt(ip)
	struct inode *ip;
{

	panic("softdep_increase_linkcnt called");
}

void 
softdep_load_inodeblock(ip)
	struct inode *ip;
{

	panic("softdep_load_inodeblock called");
}

void 
softdep_update_inodeblock(ip, bp, waitfor)
	struct inode *ip;
	struct buf *bp;
	int waitfor;
{

	panic("softdep_update_inodeblock called");
}

int
softdep_fsync(vp)
	struct vnode *vp;
{

	panic("softdep_fsync called");
	return (EIO);
}

int
softdep_sync_metadata(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{

	return (0);
}

#endif /* !FFS_SOFTUPDATES */
