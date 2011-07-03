/*	$OpenBSD: ffs_subr.c,v 1.25 2011/07/03 18:23:10 tedu Exp $	*/
/*	$NetBSD: ffs_subr.c,v 1.6 1996/03/17 02:16:23 christos Exp $	*/

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
 *	@(#)ffs_subr.c	8.2 (Berkeley) 9/21/93
 */

#include <sys/param.h>
#include <ufs/ffs/fs.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/ffs_extern.h>

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
ffs_bufatoff(struct inode *ip, off_t offset, char **res, struct buf **bpp)
{
	struct fs *fs;
	struct vnode *vp;
	struct buf *bp;
	daddr64_t lbn;
	int bsize, error;

	vp = ITOV(ip);
	fs = ip->i_fs;
	lbn = lblkno(fs, offset);
	bsize = blksize(fs, ip, lbn);

	*bpp = NULL;
	if ((error = bread(vp, lbn, fs->fs_bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return (error);
	}
	bp->b_bcount = bsize;
	if (res)
		*res = (char *)bp->b_data + blkoff(fs, offset);
	*bpp = bp;
	return (0);
}
#else
/* Prototypes for userland */
void	ffs_fragacct(struct fs *, int, int32_t[], int);
int	ffs_isfreeblock(struct fs *, u_char *, daddr64_t);
int	ffs_isblock(struct fs *, u_char *, daddr64_t);
void	ffs_clrblock(struct fs *, u_char *, daddr64_t);
void	ffs_setblock(struct fs *, u_char *, daddr64_t);
__dead void panic(const char *, ...);
#endif

/*
 * Update the frsum fields to reflect addition or deletion
 * of some frags.
 */
void
ffs_fragacct(struct fs *fs, int fragmap, int32_t fraglist[], int cnt)
{
	int inblk;
	int field, subfield;
	int siz, pos;

	inblk = (int)(fragtbl[fs->fs_frag][fragmap]) << 1;
	fragmap <<= 1;
	for (siz = 1; siz < fs->fs_frag; siz++) {
		if ((inblk & (1 << (siz + (fs->fs_frag % NBBY)))) == 0)
			continue;
		field = around[siz];
		subfield = inside[siz];
		for (pos = siz; pos <= fs->fs_frag; pos++) {
			if ((fragmap & field) == subfield) {
				fraglist[siz] += cnt;
				pos += siz;
				field <<= siz;
				subfield <<= siz;
			}
			field <<= 1;
			subfield <<= 1;
		}
	}
}

#if defined(_KERNEL) && defined(DIAGNOSTIC)
void
ffs_checkoverlap(struct buf *bp, struct inode *ip)
{
	daddr64_t start, last;
	struct vnode *vp;
	struct buf *ep;

	start = bp->b_blkno;
	last = start + btodb(bp->b_bcount) - 1;
	LIST_FOREACH(ep, &bufhead, b_list) {
		if (ep == bp || (ep->b_flags & B_INVAL) ||
		    ep->b_vp == NULLVP)
			continue;
		if (VOP_BMAP(ep->b_vp, (daddr64_t)0, &vp, NULL, NULL))
			continue;
		if (vp != ip->i_devvp)
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
		    ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		vprint("Disk overlap", vp);
		(void)printf("\tstart %lld, end %lld overlap start %llu, end %llu\n",
			start, last, ep->b_blkno,
			ep->b_blkno + btodb(ep->b_bcount) - 1);
		panic("Disk buffer overlap");
	}
}
#endif /* DIAGNOSTIC */

/*
 * block operations
 *
 * check if a block is available
 */
int
ffs_isblock(struct fs *fs, u_char *cp, daddr64_t h)
{
	u_char mask;

	switch (fs->fs_frag) {
	default:
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	}
}

/*
 * take a block out of the map
 */
void
ffs_clrblock(struct fs *fs, u_char *cp, daddr64_t h)
{

	switch (fs->fs_frag) {
	default:
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	}
}

/*
 * put a block into the map
 */
void
ffs_setblock(struct fs *fs, u_char *cp, daddr64_t h)
{

	switch (fs->fs_frag) {
	default:
	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	}
}

/*
 * check if a block is free
 */
int
ffs_isfreeblock(struct fs *fs, u_char *cp, daddr64_t h)
{

	switch (fs->fs_frag) {
	default:
	case 8:
		return (cp[h] == 0);
	case 4:
		return ((cp[h >> 1] & (0x0f << ((h & 0x1) << 2))) == 0);
	case 2:
		return ((cp[h >> 2] & (0x03 << ((h & 0x3) << 1))) == 0);
	case 1:
		return ((cp[h >> 3] & (0x01 << (h & 0x7))) == 0);
	}
}
