/*	$OpenBSD: ffs_inode.c,v 1.41 2005/12/17 13:56:01 pedro Exp $	*/
/*	$NetBSD: ffs_inode.c,v 1.10 1996/05/11 18:27:19 mycroft Exp $	*/

/*
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
 *	@(#)ffs_inode.c	8.8 (Berkeley) 10/19/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>

#include <uvm/uvm_extern.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static int ffs_indirtrunc(struct inode *, daddr_t, daddr_t, daddr_t, int,
			       long *);

/*
 * Update the access, modified, and inode change times as specified by the
 * IN_ACCESS, IN_UPDATE, and IN_CHANGE flags respectively. The IN_MODIFIED
 * flag is used to specify that the inode needs to be updated but that the
 * times have already been set. The access and modified times are taken from
 * the second and third parameters; the inode change time is always taken
 * from the current time. If waitfor is set, then wait for the disk write
 * of the inode to complete.
 */
int
ffs_update(struct inode *ip, struct timespec *atime, 
    struct timespec *mtime, int waitfor)
{
	struct vnode *vp;
	struct fs *fs;
	struct buf *bp;
	int error;
	struct timespec ts;

	vp = ITOV(ip);
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		ip->i_flag &=
		    ~(IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE);
		return (0);
	}

	if ((vp->v_mount->mnt_flag & MNT_NOATIME) &&
	    !(ip->i_flag & (IN_CHANGE | IN_UPDATE))) {
		ip->i_flag &= ~IN_ACCESS;
	}

	if ((ip->i_flag &
	    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
	    waitfor != MNT_WAIT)
		return (0);

	getnanotime(&ts);

	if (ip->i_flag & IN_ACCESS) {
		ip->i_ffs_atime = atime ? atime->tv_sec : ts.tv_sec;
		ip->i_ffs_atimensec = atime ? atime->tv_nsec : ts.tv_nsec;
	}

	if (ip->i_flag & IN_UPDATE) {
		ip->i_ffs_mtime = mtime ? mtime->tv_sec : ts.tv_sec;
		ip->i_ffs_mtimensec = mtime ? mtime->tv_nsec : ts.tv_nsec;
		ip->i_modrev++;
	}

	if (ip->i_flag & IN_CHANGE) {
		ip->i_ffs_ctime = ts.tv_sec;
		ip->i_ffs_ctimensec = ts.tv_nsec;
	}

	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE);
	fs = ip->i_fs;

	/*
	 * Ensure that uid and gid are correct. This is a temporary
	 * fix until fsck has been changed to do the update.
	 */
	if (fs->fs_inodefmt < FS_44INODEFMT) {
		ip->i_din1->di_ouid = ip->i_ffs_uid;
		ip->i_din1->di_ogid = ip->i_ffs_gid;
	}

	error = bread(ip->i_devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		(int)fs->fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}

	if (DOINGSOFTDEP(vp))
		softdep_update_inodeblock(ip, bp, waitfor);
	else if (ip->i_effnlink != ip->i_ffs_nlink) 
		panic("ffs_update: bad link cnt");

	*((struct ufs1_dinode *)bp->b_data +
	    ino_to_fsbo(fs, ip->i_number)) = *ip->i_din1;
	if (waitfor && !DOINGASYNC(vp)) {
		return (bwrite(bp));
	} else {
		bdwrite(bp);
		return (0);
	}
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */

/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
int
ffs_truncate(struct inode *oip, off_t length, int flags, struct ucred *cred)
{
	struct vnode *ovp;
	daddr_t lastblock;
	daddr_t bn, lbn, lastiblock[NIADDR], indir_lbn[NIADDR];
	daddr_t oldblks[NDADDR + NIADDR], newblks[NDADDR + NIADDR];
	struct fs *fs;
	struct buf *bp;
	int offset, size, level;
	long count, nblocks, vflags, blocksreleased = 0;
	int i, aflags, error, allerror;
	off_t osize;

	if (length < 0)
		return (EINVAL);
	ovp = ITOV(oip);

	if (ovp->v_type != VREG &&
	    ovp->v_type != VDIR &&
	    ovp->v_type != VLNK)
		return (0);

	if (oip->i_ffs_size == length)
		return (0);
	if (ovp->v_type == VLNK &&
	    (oip->i_ffs_size < ovp->v_mount->mnt_maxsymlinklen ||
	     (ovp->v_mount->mnt_maxsymlinklen == 0 &&
	      oip->i_din1->di_blocks == 0))) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("ffs_truncate: partial truncate of symlink");
#endif
		bzero((char *)&oip->i_ffs_shortlink, (u_int)oip->i_ffs_size);
		oip->i_ffs_size = 0;
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (UFS_UPDATE(oip, MNT_WAIT));
	}

	if ((error = getinoquota(oip)) != 0)
		return (error);

	uvm_vnp_setsize(ovp, length);
	oip->i_ci.ci_lasta = oip->i_ci.ci_clen 
	    = oip->i_ci.ci_cstart = oip->i_ci.ci_lastw = 0;

	if (DOINGSOFTDEP(ovp)) {
		if (length > 0 || softdep_slowdown(ovp)) {
			/*
			 * If a file is only partially truncated, then
			 * we have to clean up the data structures
			 * describing the allocation past the truncation
			 * point. Finding and deallocating those structures
			 * is a lot of work. Since partial truncation occurs
			 * rarely, we solve the problem by syncing the file
			 * so that it will have no data structures left.
			 */
			if ((error = VOP_FSYNC(ovp, cred, MNT_WAIT,
					       curproc)) != 0)
				return (error);
		} else {
			(void)ufs_quota_free_blocks(oip, oip->i_ffs_blocks, 
			    NOCRED);
			softdep_setup_freeblocks(oip, length);
			(void) vinvalbuf(ovp, 0, cred, curproc, 0, 0);
			oip->i_flag |= IN_CHANGE | IN_UPDATE;
			return (UFS_UPDATE(oip, 0));
		}
	}

	fs = oip->i_fs;
	osize = oip->i_ffs_size; 
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
		if (length > fs->fs_maxfilesize)
			return (EFBIG);
		aflags = B_CLRBUF;
		if (flags & IO_SYNC)
			aflags |= B_SYNC;
		error = UFS_BUF_ALLOC(oip, length - 1, 1, 
				   cred, aflags, &bp);
		if (error)
			return (error);
		oip->i_ffs_size = length;
		uvm_vnp_setsize(ovp, length);
		(void) uvm_vnp_uncache(ovp);
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (UFS_UPDATE(oip, MNT_WAIT));
	}
	uvm_vnp_setsize(ovp, length);

	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundary, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever becomes accessible again because
	 * of subsequent file growth. Directories however are not
	 * zero'ed as they should grow back initialized to empty.
	 */
	offset = blkoff(fs, length);
	if (offset == 0) {
		oip->i_ffs_size = length;
	} else {
		lbn = lblkno(fs, length);
		aflags = B_CLRBUF;
		if (flags & IO_SYNC)
			aflags |= B_SYNC;
		error = UFS_BUF_ALLOC(oip, length - 1, 1,
				   cred, aflags, &bp);
		if (error)
			return (error);
		/*
		 * When we are doing soft updates and the UFS_BALLOC
		 * above fills in a direct block hole with a full sized
		 * block that will be truncated down to a fragment below,
		 * we must flush out the block dependency with an FSYNC
		 * so that we do not get a soft updates inconsistency
		 * when we create the fragment below.
		 */
		if (DOINGSOFTDEP(ovp) && lbn < NDADDR &&
		    fragroundup(fs, blkoff(fs, length)) < fs->fs_bsize &&
		    (error = VOP_FSYNC(ovp, cred, MNT_WAIT, curproc)) != 0)
			return (error);
		oip->i_ffs_size = length;
		size = blksize(fs, oip, lbn);
		(void) uvm_vnp_uncache(ovp);
		if (ovp->v_type != VDIR)
			bzero((char *)bp->b_data + offset,
			      (u_int)(size - offset));
		allocbuf(bp, size);
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
	}
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ffs_indirtrunc below.
	 */
	bcopy((caddr_t)&oip->i_ffs_db[0], (caddr_t)oldblks, sizeof oldblks);
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			oip->i_ffs_ib[level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		oip->i_ffs_db[i] = 0;
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	if ((error = UFS_UPDATE(oip, MNT_WAIT)) != 0)
		allerror = error;
	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	bcopy((caddr_t)&oip->i_ffs_db[0], (caddr_t)newblks, sizeof newblks);
	bcopy((caddr_t)oldblks, (caddr_t)&oip->i_ffs_db[0], sizeof oldblks);
	oip->i_ffs_size = osize;
	vflags = ((length > 0) ? V_SAVE : 0) | V_SAVEMETA;
	allerror = vinvalbuf(ovp, vflags, cred, curproc, 0, 0);

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = oip->i_ffs_ib[level];
		if (bn != 0) {
			error = ffs_indirtrunc(oip, indir_lbn[level],
			    fsbtodb(fs, bn), lastiblock[level], level, &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				oip->i_ffs_ib[level] = 0;
				ffs_blkfree(oip, bn, fs->fs_bsize);
				blocksreleased += nblocks;
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		long bsize;

		bn = oip->i_ffs_db[i];
		if (bn == 0)
			continue;
		oip->i_ffs_db[i] = 0;
		bsize = blksize(fs, oip, i);
		ffs_blkfree(oip, bn, bsize);
		blocksreleased += btodb(bsize);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = oip->i_ffs_db[lastblock];
	if (bn != 0) {
		long oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = blksize(fs, oip, lastblock);
		oip->i_ffs_size = length;
		newspace = blksize(fs, oip, lastblock);
		if (newspace == 0)
			panic("ffs_truncate: newspace");
		if (oldspace - newspace > 0) {
			/*
			 * Block number of space to be free'd is
			 * the old block # plus the number of frags
			 * required for the storage we're keeping.
			 */
			bn += numfrags(fs, newspace);
			ffs_blkfree(oip, bn, oldspace - newspace);
			blocksreleased += btodb(oldspace - newspace);
		}
	}
done:
#ifdef DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if (newblks[NDADDR + level] != oip->i_ffs_ib[level])
			panic("ffs_truncate1");
	for (i = 0; i < NDADDR; i++)
		if (newblks[i] != oip->i_ffs_db[i])
			panic("ffs_truncate2");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_ffs_size = length;
	oip->i_ffs_blocks -= blocksreleased;
	if (oip->i_ffs_blocks < 0)			/* sanity */
		oip->i_ffs_blocks = 0;
	oip->i_flag |= IN_CHANGE;
	(void)ufs_quota_free_blocks(oip, blocksreleased, NOCRED);
	return (allerror);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 *
 * NB: triple indirect blocks are untested.
 */
static int
ffs_indirtrunc(struct inode *ip, daddr_t lbn, daddr_t dbn, daddr_t lastbn,
    int level, long *countp)
{
	int i;
	struct buf *bp;
	struct fs *fs = ip->i_fs;
	daddr_t *bap;
	struct vnode *vp;
	daddr_t *copy = NULL, nb, nlbn, last;
	long blkcount, factor;
	int nblocks, blocksreleased = 0;
	int error = 0, allerror = 0;

	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	bp = getblk(vp, lbn, (int)fs->fs_bsize, 0, 0);
	if (!(bp->b_flags & (B_DONE | B_DELWRI))) {
		curproc->p_stats->p_ru.ru_inblock++;	/* pay for read */
		bp->b_flags |= B_READ;
		if (bp->b_bcount > bp->b_bufsize)
			panic("ffs_indirtrunc: bad buffer size");
		bp->b_blkno = dbn;
		VOP_STRATEGY(bp);
		error = biowait(bp);
	}
	if (error) {
		brelse(bp);
		*countp = 0;
		return (error);
	}

	bap = (daddr_t *)bp->b_data;
	if (lastbn != -1) {
		MALLOC(copy, daddr_t *, fs->fs_bsize, M_TEMP, M_WAITOK);
		bcopy((caddr_t)bap, (caddr_t)copy, (u_int)fs->fs_bsize);
		bzero((caddr_t)&bap[last + 1],
		    (u_int)(NINDIR(fs) - (last + 1)) * sizeof (daddr_t));
		if (!DOINGASYNC(vp)) {
			error = bwrite(bp);
			if (error)
				allerror = error;
		} else {
			bawrite(bp);
		}
		bap = copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = bap[i];
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = ffs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
					       (daddr_t)-1, level - 1,
					       &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
		ffs_blkfree(ip, nb, fs->fs_bsize);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = bap[i];
		if (nb != 0) {
			error = ffs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
					       last, level - 1, &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
	}
	if (copy != NULL) {
		FREE(copy, M_TEMP);
	} else {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
		
	*countp = blocksreleased;
	return (allerror);
}
