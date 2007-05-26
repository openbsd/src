/*	$OpenBSD: ext2fs_subr.c,v 1.15 2007/05/26 20:26:51 pedro Exp $	*/
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

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

union _qcvt {
	int64_t qcvt;
	int32_t val[2];
};

#define SETHIGH(q, h) {			\
	union _qcvt tmp;		\
	tmp.qcvt = (q);			\
	tmp.val[_QUAD_HIGHWORD] = (h);	\
	(q) = tmp.qcvt;			\
}

#define SETLOW(q, l) {			\
	union _qcvt tmp;		\
	tmp.qcvt = (q);			\
	tmp.val[_QUAD_LOWWORD] = (l);	\
	(q) = tmp.qcvt;			\
}

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
	ufs1_daddr_t lbn;
	int error;

	vp = ITOV(ip);
	fs = ip->i_e2fs;
	lbn = lblkno(fs, offset);

	*bpp = NULL;
	if ((error = bread(vp, lbn, fs->e2fs_bsize, NOCRED, &bp)) != 0) {
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
ext2fs_checkoverlap(bp, ip)
	struct buf *bp;
	struct inode *ip;
{
	struct buf *ep;
	ufs1_daddr_t start, last;
	struct vnode *vp;

	start = bp->b_blkno;
	last = start + btodb(bp->b_bcount) - 1;
	LIST_FOREACH(ep, &bufhead, b_list) {
		if (ep == bp || (ep->b_flags & B_INVAL) ||
			ep->b_vp == NULLVP)
			continue;
		if (VOP_BMAP(ep->b_vp, (daddr_t)0, &vp, (daddr_t)0, NULL))
			continue;
		if (vp != ip->i_devvp)
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
			ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		vprint("Disk overlap", vp);
		printf("\tstart %d, end %d overlap start %d, end %ld\n",
			start, last, ep->b_blkno,
			ep->b_blkno + btodb(ep->b_bcount) - 1);
		panic("Disk buffer overlap");
	}
}
#endif /* DIAGNOSTIC */

/*
 * Initialize the vnode associated with a new inode, handle aliased vnodes.
 */
int
ext2fs_vinit(struct mount *mp, int (**specops)(void *),
    int (**fifoops)(void *), struct vnode **vpp)
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
		vp->v_op = specops;

		nvp = checkalias(vp, fs2h32(ip->i_e2din->e2di_rdev), mp);
		if (nvp != NULL) {
			/*
			 * Discard unneeded vnode, but save its inode. Note
			 * that the lock is carried over in the inode to the
			 * replacement vnode.
			 */
			nvp->v_data = vp->v_data;
			vp->v_data = NULL;
			vp->v_op = spec_vnodeop_p;
#ifdef VFSDEBUG
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
		vp->v_op = fifoops;
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
	SETHIGH(ip->i_modrev, tv.tv_sec);
	SETLOW(ip->i_modrev, tv.tv_usec * 4294);

	*vpp = vp;

	return (0);
}
