/*	$OpenBSD: ufs_readwrite.c,v 1.27 2005/11/11 16:27:52 pedro Exp $	*/
/*	$NetBSD: ufs_readwrite.c,v 1.9 1996/05/11 18:27:57 mycroft Exp $	*/

/*
 * Copyright (c) 1993
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
 *	@(#)ufs_readwrite.c	8.11 (Berkeley) 5/8/95
 */

#ifdef LFS_READWRITE
#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct lfs
#define	I_FS			i_lfs
#define	READ			lfs_read
#define	READ_S			"lfs_read"
#define	WRITE			lfs_write
#define	WRITE_S			"lfs_write"
#define	fs_bsize		lfs_bsize
#define MAXFILESIZE		fs->lfs_maxfilesize
#else
#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct fs
#define	I_FS			i_fs
#define	READ			ffs_read
#define	READ_S			"ffs_read"
#define	WRITE			ffs_write
#define	WRITE_S			"ffs_write"
#define MAXFILESIZE		fs->fs_maxfilesize
#endif

#include <sys/event.h>

#define VN_KNOTE(vp, b) \
	KNOTE((struct klist *)&vp->v_selectinfo.vsi_selinfo.si_note, (b))

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
int
READ(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp;
	struct inode *ip;
	struct uio *uio;
	FS *fs;
	struct buf *bp;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	mode_t mode;
	int error;

	vp = ap->a_vp;
	ip = VTOI(vp);
	mode = ip->i_ffs_mode;
	uio = ap->a_uio;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", READ_S);

	if (vp->v_type == VLNK) {
		if ((int)ip->i_ffs_size < vp->v_mount->mnt_maxsymlinklen ||
		    (vp->v_mount->mnt_maxsymlinklen == 0 &&
		     ip->i_ffs_blocks == 0))
			panic("%s: short symlink", READ_S);
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", READ_S, vp->v_type);
#endif
	fs = ip->I_FS;
	if ((u_int64_t)uio->uio_offset > MAXFILESIZE)
		return (EFBIG);

	if (uio->uio_resid == 0)
		return (0);

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_ffs_size - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = BLKSIZE(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

#ifdef LFS_READWRITE
		(void)lfs_check(vp, lbn);
		error = cluster_read(vp, &ip->i_ci, ip->i_ffs_size, lbn, 
		    size, NOCRED, &bp);
#else
		if (lblktosize(fs, nextlbn) >= ip->i_ffs_size)
			error = bread(vp, lbn, size, NOCRED, &bp);
		else if (doclusterread)
			error = cluster_read(vp, &ip->i_ci,
			    ip->i_ffs_size, lbn, size, NOCRED, &bp);
		else if (lbn - 1 == ip->i_ci.ci_lastr) {
			int nextsize = BLKSIZE(fs, ip, nextlbn);
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else
			error = bread(vp, lbn, size, NOCRED, &bp);
#endif
		if (error)
			break;
		ip->i_ci.ci_lastr = lbn;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= bp->b_resid;
		if (size < xfersize) {
			if (size == 0)
				break;
			xfersize = size;
		}
		error = uiomove((char *)bp->b_data + blkoffset, (int)xfersize,
				uio);
		if (error)
			break;
		brelse(bp);
	}
	if (bp != NULL)
		brelse(bp);
	ip->i_flag |= IN_ACCESS;
	return (error);
}

/*
 * Vnode op for writing.
 */
int
WRITE(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp;
	struct uio *uio;
	struct inode *ip;
	FS *fs;
	struct buf *bp;
	struct proc *p;
	daddr_t lbn;
	off_t osize;
	int blkoffset, error, extended, flags, ioflag, resid, size, xfersize;

	extended = 0;
	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", WRITE_S);
#endif

	/*
	 * If writing 0 bytes, succeed and do not change
	 * update time or file offset (standards compliance)
	 */
	if (uio->uio_resid == 0)
		return (0);

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_ffs_size;
		if ((ip->i_ffs_flags & APPEND) && uio->uio_offset != ip->i_ffs_size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("%s: nonsync dir write", WRITE_S);
		break;
	default:
		panic("%s: type", WRITE_S);
	}

	fs = ip->I_FS;
	if (uio->uio_offset < 0 ||
	    (u_int64_t)uio->uio_offset + uio->uio_resid > MAXFILESIZE)
		return (EFBIG);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	p = uio->uio_procp;
	if (vp->v_type == VREG && p &&
	    uio->uio_offset + uio->uio_resid >
	    p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}

	resid = uio->uio_resid;
	osize = ip->i_ffs_size;
	flags = ioflag & IO_SYNC ? B_SYNC : 0;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (fs->fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;

		if ((error = UFS_BUF_ALLOC(ip, uio->uio_offset, xfersize,
			 ap->a_cred, flags, &bp)) != 0)
			break;
		if (uio->uio_offset + xfersize > ip->i_ffs_size) {
			ip->i_ffs_size = uio->uio_offset + xfersize;
			uvm_vnp_setsize(vp, ip->i_ffs_size);
			extended = 1;
		}
		(void)uvm_vnp_uncache(vp);

		size = BLKSIZE(fs, ip, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, xfersize, uio);

		if (error != 0)
			bzero((char *)bp->b_data + blkoffset, xfersize);

#ifdef LFS_READWRITE
		(void)VOP_BWRITE(bp);
#else
		if (ioflag & IO_SYNC)
			(void)bwrite(bp);
		else if (xfersize + blkoffset == fs->fs_bsize) {
			if (doclusterwrite)
				cluster_write(bp, &ip->i_ci, ip->i_ffs_size);
			else
				bawrite(bp);
		} else
			bdwrite(bp);
#endif
		if (error || xfersize == 0)
			break;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if (resid > uio->uio_resid && ap->a_cred && ap->a_cred->cr_uid != 0)
		ip->i_ffs_mode &= ~(ISUID | ISGID);
	if (resid > uio->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)UFS_TRUNCATE(ip, osize,
			    ioflag & IO_SYNC, ap->a_cred);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC)) {
		error = UFS_UPDATE(ip, MNT_WAIT);
	}
	return (error);
}
