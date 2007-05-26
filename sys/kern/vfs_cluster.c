/*	$OpenBSD: vfs_cluster.c,v 1.37 2007/05/26 20:26:51 pedro Exp $	*/
/*	$NetBSD: vfs_cluster.c,v 1.12 1996/04/22 01:39:05 christos Exp $	*/

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
 *	@(#)vfs_cluster.c	8.8 (Berkeley) 7/28/94
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>

#include <uvm/uvm_extern.h>

void cluster_wbuild(struct vnode *, struct buf *, long, daddr64_t, int,
    daddr64_t);
struct cluster_save *cluster_collectbufs(struct vnode *, struct cluster_info *,
    struct buf *);

/*
 * Do clustered write for FFS.
 *
 * Three cases:
 *	1. Write is not sequential (write asynchronously)
 *	Write is sequential:
 *	2.	beginning of cluster - begin cluster
 *	3.	middle of a cluster - add to cluster
 *	4.	end of a cluster - asynchronously write cluster
 */
void
cluster_write(struct buf *bp, struct cluster_info *ci, u_quad_t filesize)
{
	struct vnode *vp;
	daddr64_t lbn;
	int maxclen, cursize;

	vp = bp->b_vp;
	lbn = bp->b_lblkno;

	/* Initialize vnode to beginning of file. */
	if (lbn == 0)
		ci->ci_lasta = ci->ci_clen = ci->ci_cstart = ci->ci_lastw = 0;

	if (ci->ci_clen == 0 || lbn != ci->ci_lastw + 1 ||
	    (bp->b_blkno != ci->ci_lasta + btodb(bp->b_bcount))) {
		maxclen = MAXBSIZE / vp->v_mount->mnt_stat.f_iosize - 1;
		if (ci->ci_clen != 0) {
			/*
			 * Next block is not sequential.
			 *
			 * If we are not writing at end of file, the process
			 * seeked to another point in the file since its
			 * last write, or we have reached our maximum
			 * cluster size, then push the previous cluster.
			 * Otherwise try reallocating to make it sequential.
			 */
			cursize = ci->ci_lastw - ci->ci_cstart + 1;
			if (((u_quad_t)(lbn + 1)) * bp->b_bcount != filesize ||
			    lbn != ci->ci_lastw + 1 || ci->ci_clen <= cursize) {
				cluster_wbuild(vp, NULL, bp->b_bcount,
				    ci->ci_cstart, cursize, lbn);
			} else {
				struct buf **bpp, **endbp;
				struct cluster_save *buflist;

				buflist = cluster_collectbufs(vp, ci, bp);
				endbp = &buflist->bs_children
				    [buflist->bs_nchildren - 1];
				if (VOP_REALLOCBLKS(vp, buflist)) {
					/*
					 * Failed, push the previous cluster.
					 */
					for (bpp = buflist->bs_children;
					    bpp < endbp; bpp++)
						brelse(*bpp);
					free(buflist, M_VCLUSTER);
					cluster_wbuild(vp, NULL, bp->b_bcount,
					    ci->ci_cstart, cursize, lbn);
				} else {
					/*
					 * Succeeded, keep building cluster.
					 */
					for (bpp = buflist->bs_children;
					    bpp <= endbp; bpp++)
						bdwrite(*bpp);
					free(buflist, M_VCLUSTER);
					ci->ci_lastw = lbn;
					ci->ci_lasta = bp->b_blkno;
					return;
				}
			}
		}
		/*
		 * Consider beginning a cluster.
		 * If at end of file, make cluster as large as possible,
		 * otherwise find size of existing cluster.
		 */
		if ((u_quad_t)(lbn + 1) * (u_quad_t)bp->b_bcount != filesize &&
		    (VOP_BMAP(vp, lbn, NULL, &bp->b_blkno, &maxclen) ||
		    bp->b_blkno == -1)) {
			bawrite(bp);
			ci->ci_clen = 0;
			ci->ci_lasta = bp->b_blkno;
			ci->ci_cstart = lbn + 1;
			ci->ci_lastw = lbn;
			return;
		}
		ci->ci_clen = maxclen;
		if (maxclen == 0) {		/* I/O not contiguous */
			ci->ci_cstart = lbn + 1;
			bawrite(bp);
		} else {			/* Wait for rest of cluster */
			ci->ci_cstart = lbn;
			bdwrite(bp);
		}
	} else if (lbn == ci->ci_cstart + ci->ci_clen) {
		/*
		 * At end of cluster, write it out.
		 */
		cluster_wbuild(vp, bp, bp->b_bcount, ci->ci_cstart,
		    ci->ci_clen + 1, lbn);
		ci->ci_clen = 0;
		ci->ci_cstart = lbn + 1;
	} else
		/*
		 * In the middle of a cluster, so just delay the
		 * I/O for now.
		 */
		bdwrite(bp);
	ci->ci_lastw = lbn;
	ci->ci_lasta = bp->b_blkno;
}

/*
 * The last lbn argument is the current block on which I/O is being
 * performed.  Check to see that it doesn't fall in the middle of
 * the current block (if last_bp == NULL).
 */
void
cluster_wbuild(struct vnode *vp, struct buf *last_bp, long size,
    daddr64_t start_lbn, int len, daddr64_t lbn)
{
	struct buf *bp;

#ifdef DIAGNOSTIC
	if (size != vp->v_mount->mnt_stat.f_iosize)
		panic("cluster_wbuild: size %ld != filesize %ld",
			size, vp->v_mount->mnt_stat.f_iosize);
#endif
redo:
	while ((!incore(vp, start_lbn) || start_lbn == lbn) && len) {
		++start_lbn;
		--len;
	}

	/* Get more memory for current buffer */
	if (len <= 1) {
		if (last_bp) {
			bawrite(last_bp);
		} else if (len) {
			bp = getblk(vp, start_lbn, size, 0, 0);
			/*
			 * The buffer could have already been flushed out of
			 * the cache. If that has happened, we'll get a new
			 * buffer here with random data, just drop it.
			 */
			if ((bp->b_flags & B_DELWRI) == 0)
				brelse(bp);
			else
				bawrite(bp);
		}
		return;
	}

	bp = getblk(vp, start_lbn, size, 0, 0);
	if (!(bp->b_flags & B_DELWRI)) {
		++start_lbn;
		--len;
		brelse(bp);
		goto redo;
	}

	++start_lbn;
	--len;
	bawrite(bp);
	goto redo;
}

/*
 * Collect together all the buffers in a cluster.
 * Plus add one additional buffer.
 */
struct cluster_save *
cluster_collectbufs(struct vnode *vp, struct cluster_info *ci,
    struct buf *last_bp)
{
	struct cluster_save *buflist;
	daddr64_t lbn;
	int i, len;

	len = ci->ci_lastw - ci->ci_cstart + 1;
	buflist = malloc(sizeof(struct buf *) * (len + 1) + sizeof(*buflist),
	    M_VCLUSTER, M_WAITOK);
	buflist->bs_nchildren = 0;
	buflist->bs_children = (struct buf **)(buflist + 1);
	for (lbn = ci->ci_cstart, i = 0; i < len; lbn++, i++)
		(void)bread(vp, lbn, last_bp->b_bcount, NOCRED,
		    &buflist->bs_children[i]);
	buflist->bs_children[i] = last_bp;
	buflist->bs_nchildren = i + 1;
	return (buflist);
}
