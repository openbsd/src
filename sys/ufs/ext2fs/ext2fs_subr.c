/*	$OpenBSD: ext2fs_subr.c,v 1.35 2016/08/10 07:53:02 natano Exp $	*/
/*	$NetBSD: ext2fs_subr.c,v 1.1 1997/06/11 09:34:03 bouyer Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_subr.c	8.2 (Berkeley) 9/21/93
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>
#include <ufs/ext2fs/ext2fs_extents.h>

#ifdef _KERNEL

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
ext2fs_bufatoff(struct inode *ip, off_t offset, char **res, struct buf **bpp)
{
	struct vnode *vp;
	struct m_ext2fs *fs;
	struct buf *bp;
	daddr_t lbn, pos;
	int error;

	vp = ITOV(ip);
	fs = ip->i_e2fs;
	lbn = lblkno(fs, offset);

	if (ip->i_e2din->e2di_flags & EXT4_EXTENTS) {
		struct ext4_extent_path path;
		struct ext4_extent *ep;

		memset(&path, 0, sizeof path);
		if (ext4_ext_find_extent(fs, ip, lbn, &path) == NULL ||
		    (ep = path.ep_ext) == NULL)
			goto normal;

		if (path.ep_bp != NULL) {
			brelse(path.ep_bp);
			path.ep_bp = NULL;
		}
		pos = lbn - ep->e_blk + (((daddr_t)ep->e_start_hi << 32) | ep->e_start_lo);
		error = bread(ip->i_devvp, fsbtodb(fs, pos), fs->e2fs_bsize, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}

		if (res)
			*res = (char *)bp->b_data + blkoff(fs, offset);

		*bpp = bp;

		return (0);
	}

 normal:
	*bpp = NULL;
	if ((error = bread(vp, lbn, fs->e2fs_bsize, &bp)) != 0) {
		brelse(bp);
		return (error);
	}
	if (res)
		*res = (char *)bp->b_data + blkoff(fs, offset);
	*bpp = bp;
	return (0);
}
#endif

#if defined(_KERNEL) && defined(DIAGNOSTIC)
void
ext2fs_checkoverlap(struct buf *bp, struct inode *ip)
{
	struct buf *ep;
	struct vnode *vp;
	daddr_t start, last;

	start = bp->b_blkno;
	last = start + btodb(bp->b_bcount) - 1;
	LIST_FOREACH(ep, &bufhead, b_list) {
		if (ep == bp || (ep->b_flags & B_INVAL) ||
			ep->b_vp == NULLVP)
			continue;
		if (VOP_BMAP(ep->b_vp, 0, &vp, NULL, NULL))
			continue;
		if (vp != ip->i_devvp)
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
			ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		vprint("Disk overlap", vp);
		printf("\tstart %lld, end %lld overlap start %lld, end %lld\n",
			start, last, (long long)ep->b_blkno,
			(long long)(ep->b_blkno + btodb(ep->b_bcount) - 1));
		panic("Disk buffer overlap");
	}
}
#endif /* DIAGNOSTIC */

/*
 * Initialize the vnode associated with a new inode, handle aliased vnodes.
 */
int
ext2fs_vinit(struct mount *mp, struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *vp, *nvp;
	struct timeval tv;

	vp = *vpp;
	ip = VTOI(vp);
	vp->v_type = IFTOVT(ip->i_e2fs_mode);

	switch(vp->v_type) {
	case VCHR:
	case VBLK:
		vp->v_op = &ext2fs_specvops;

		nvp = checkalias(vp, letoh32(ip->i_e2din->e2di_rdev), mp);
		if (nvp != NULL) {
			/*
			 * Discard unneeded vnode, but save its inode. Note
			 * that the lock is carried over in the inode to the
			 * replacement vnode.
			 */
			nvp->v_data = vp->v_data;
			vp->v_data = NULL;
			vp->v_op = &spec_vops;
#ifdef VFSLCKDEBUG
			vp->v_flag &= ~VLOCKSWORK;
#endif
			vrele(vp);
			vgone(vp);
			/* Reinitialize aliased vnode. */
			vp = nvp;
			ip->i_vnode = vp;
		}

		break;

	case VFIFO:
#ifdef FIFO
		vp->v_op = &ext2fs_fifovops;
		break;
#else
		return (EOPNOTSUPP);
#endif /* FIFO */

	default:

		break;
	}

	if (ip->i_number == EXT2_ROOTINO)
		vp->v_flag |= VROOT;

	/* Initialize modrev times */
	getmicrouptime(&tv);
	ip->i_modrev = (u_quad_t)tv.tv_sec << 32;
	ip->i_modrev |= (u_quad_t)tv.tv_usec * 4294;

	*vpp = vp;

	return (0);
}
