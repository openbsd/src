/*	$NetBSD: msdosfs_lookup.c,v 1.17 1995/09/09 19:38:06 ws Exp $	*/

/*-
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 * 
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 * 
 * This software is provided "as is".
 * 
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 * 
 * October 1992
 */

#include <sys/param.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <msdosfs/bpb.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/fat.h>

/*
 * When we search a directory the blocks containing directory entries are
 * read and examined.  The directory entries contain information that would
 * normally be in the inode of a unix filesystem.  This means that some of
 * a directory's contents may also be in memory resident denodes (sort of
 * an inode).  This can cause problems if we are searching while some other
 * process is modifying a directory.  To prevent one process from accessing
 * incompletely modified directory information we depend upon being the
 * soul owner of a directory block.  bread/brelse provide this service.
 * This being the case, when a process modifies a directory it must first
 * acquire the disk block that contains the directory entry to be modified.
 * Then update the disk block and the denode, and then write the disk block
 * out to disk.  This way disk blocks containing directory entries and in
 * memory denode's will be in synch.
 */
int
msdosfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vdp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	daddr_t bn;
	int error;
	int lockparent;
	int wantparent;
	int slotstatus;

#define	NONE	0
#define	FOUND	1
	int slotoffset;
	int slotcluster;
	int frcn;
	u_long cluster;
	int rootreloff;
	int diroff;
	int blsize;
	int isadir;		/* ~0 if found direntry is a directory	 */
	u_long scn;		/* starting cluster number		 */
	struct vnode *pdp;
	struct denode *dp;
	struct denode *tdp;
	struct msdosfsmount *pmp;
	struct buf *bp = 0;
	struct direntry *dep;
	u_char dosfilename[12];
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): looking for %s\n", cnp->cn_nameptr);
#endif
	dp = VTODE(vdp);
	pmp = dp->de_pmp;
	*vpp = NULL;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT | WANTPARENT);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): vdp %08x, dp %08x, Attr %02x\n",
	       vdp, dp, dp->de_Attributes);
#endif

	/*
	 * Check accessiblity of directory.
	 */
	if ((dp->de_Attributes & ATTR_DIRECTORY) == 0)
		return (ENOTDIR);
	if (error = VOP_ACCESS(vdp, VEXEC, cnp->cn_cred, cnp->cn_proc))
		return (error);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if (error = cache_lookup(vdp, vpp, cnp)) {
		int vpid;

		if (error == ENOENT)
			return (error);
		/*
		 * Get the next vnode in the path.
		 * See comment below starting `Step through' for
		 * an explaination of the locking protocol.
		 */
		pdp = vdp;
		dp = VTODE(*vpp);
		vdp = *vpp;
		vpid = vdp->v_id;
		if (pdp == vdp) {   /* lookup on "." */
			VREF(vdp);
			error = 0;
		} else if (flags & ISDOTDOT) {
			VOP_UNLOCK(pdp);
			error = vget(vdp, 1);
			if (!error && lockparent && (flags & ISLASTCN))
				error = VOP_LOCK(pdp);
		} else {
			error = vget(vdp, 1);
			if (!lockparent || error || !(flags & ISLASTCN))
				VOP_UNLOCK(pdp);
		}
		/*
		 * Check that the capability number did not change
		 * while we were waiting for the lock.
		 */
		if (!error) {
			if (vpid == vdp->v_id) {
#ifdef MSDOSFS_DEBUG
				printf("msdosfs_lookup(): cache hit, vnode %08x, file %s\n",
				       vdp, dp->de_Name);
#endif
				return (0);
			}
			vput(vdp);
			if (lockparent && pdp != vdp && (flags & ISLASTCN))
				VOP_UNLOCK(pdp);
		}
		if (error = VOP_LOCK(pdp))
			return (error);
		vdp = pdp;
		dp = VTODE(vdp);
		*vpp = NULL;
	}

	/*
	 * If they are going after the . or .. entry in the root directory,
	 * they won't find it.  DOS filesystems don't have them in the root
	 * directory.  So, we fake it. deget() is in on this scam too.
	 */
	if ((vdp->v_flag & VROOT) && cnp->cn_nameptr[0] == '.' &&
	    (cnp->cn_namelen == 1 ||
		(cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.'))) {
		isadir = ATTR_DIRECTORY;
		scn = MSDOSFSROOT;
#ifdef MSDOSFS_DEBUG
		printf("msdosfs_lookup(): looking for . or .. in root directory\n");
#endif
		cluster = MSDOSFSROOT;
		diroff = MSDOSFSROOT_OFS;
		goto foundroot;
	}

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	slotstatus = FOUND;
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN)) {
		slotstatus = NONE;
		slotoffset = -1;
	}

	unix2dosfn((u_char *)cnp->cn_nameptr, dosfilename, cnp->cn_namelen);
	dosfilename[11] = 0;
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): dos version of filename %s, length %d\n",
	       dosfilename, cnp->cn_namelen);
#endif
	/*
	 * Search the directory pointed at by vdp for the name pointed at
	 * by cnp->cn_nameptr.
	 */
	tdp = NULL;
	/*
	 * The outer loop ranges over the clusters that make up the
	 * directory.  Note that the root directory is different from all
	 * other directories.  It has a fixed number of blocks that are not
	 * part of the pool of allocatable clusters.  So, we treat it a
	 * little differently. The root directory starts at "cluster" 0.
	 */
	rootreloff = 0;
	for (frcn = 0;; frcn++) {
		if (error = pcbmap(dp, frcn, &bn, &cluster, &blsize)) {
			if (error == E2BIG)
				break;
			return (error);
		}
		if (error = bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp))
			return (error);
		for (diroff = 0; diroff < blsize;
		     diroff += sizeof(struct direntry),
		     rootreloff += sizeof(struct direntry)) {
			dep = (struct direntry *)(bp->b_data + diroff);
			/*
			 * If the slot is empty and we are still looking
			 * for an empty then remember this one.  If the
			 * slot is not empty then check to see if it
			 * matches what we are looking for.  If the slot
			 * has never been filled with anything, then the
			 * remainder of the directory has never been used,
			 * so there is no point in searching it.
			 */
			if (dep->deName[0] == SLOT_EMPTY ||
			    dep->deName[0] == SLOT_DELETED) {
				if (slotstatus != FOUND) {
					slotstatus = FOUND;
					if (cluster == MSDOSFSROOT)
						slotoffset = rootreloff;
					else
						slotoffset = diroff;
					slotcluster = cluster;
				}
				if (dep->deName[0] == SLOT_EMPTY) {
					brelse(bp);
					goto notfound;
				}
			} else {
				/*
				 * Ignore volume labels (anywhere, not just
				 * the root directory).
				 */
				if (dep->deAttributes & ATTR_VOLUME)
					continue;
				/*
				 * Check for a name match.
				 */
				if (bcmp(dosfilename, dep->deName, 11))
					continue;
#ifdef MSDOSFS_DEBUG
				printf("msdosfs_lookup(): match diroff %d, rootreloff %d\n",
				       diroff, rootreloff);
#endif
				/*
				 * Remember where this directory
				 * entry came from for whoever did
				 * this lookup. If this is the root
				 * directory we are interested in
				 * the offset relative to the
				 * beginning of the directory (not
				 * the beginning of the cluster).
				 */
				if (cluster == MSDOSFSROOT)
					diroff = rootreloff;
				dp->de_fndoffset = diroff;
				dp->de_fndclust = cluster;
				goto found;
			}
		}	/* for (diroff = 0; .... */
		/*
		 * Release the buffer holding the directory cluster just
		 * searched.
		 */
		brelse(bp);
	}	/* for (frcn = 0; ; frcn++) */

notfound:;
	/*
	 * We hold no disk buffers at this point.
	 */

	/*
	 * If we get here we didn't find the entry we were looking for. But
	 * that's ok if we are creating or renaming and are at the end of
	 * the pathname and the directory hasn't been removed.
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): op %d, refcnt %d, slotstatus %d\n",
	       nameiop, dp->de_refcnt, slotstatus);
	printf("               slotoffset %d, slotcluster %d\n",
	       slotoffset, slotcluster);
#endif
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN) && dp->de_refcnt != 0) {
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		if (error = VOP_ACCESS(vdp, VWRITE, cnp->cn_cred, cnp->cn_proc))
			return (error);
		/*
		 * Return an indication of where the new directory
		 * entry should be put.  If we didn't find a slot,
		 * then set dp->de_fndoffset to -1 indicating
		 * that the new slot belongs at the end of the
		 * directory. If we found a slot, then the new entry
		 * can be put at dp->de_fndoffset.
		 */
		if (slotstatus == NONE) {
			dp->de_fndoffset = (u_long)-1;
			dp->de_fndclust = (u_long)-1;
		} else {
#ifdef MSDOSFS_DEBUG
			printf("msdosfs_lookup(): saving empty slot location\n");
#endif
			dp->de_fndoffset = slotoffset;
			dp->de_fndclust = slotcluster;
		}

		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to do a direnter().
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory inode in ndp->ni_dvp.
		 * The pathname buffer is saved so that the name
		 * can be obtained later.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(vdp);
		return (EJUSTRETURN);
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if ((cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
		cache_enter(vdp, *vpp, cnp);
	return (ENOENT);

found:;
	/*
	 * NOTE:  We still have the buffer with matched directory entry at
	 * this point.
	 */
	isadir = dep->deAttributes & ATTR_DIRECTORY;
	scn = getushort(dep->deStartCluster);

foundroot:;
	/*
	 * If we entered at foundroot, then we are looking for the . or ..
	 * entry of the filesystems root directory.  isadir and scn were
	 * setup before jumping here.  And, bp is null.  There is no buf
	 * header.
	 */

	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * If the wantparent flag isn't set, we return only
	 * the directory (in ndp->ni_dvp), otherwise we go
	 * on and lock the inode, being careful with ".".
	 */
	if (nameiop == DELETE && (flags & ISLASTCN)) {
		/*
		 * Write access to directory required to delete files.
		 */
		if (error = VOP_ACCESS(vdp, VWRITE, cnp->cn_cred, cnp->cn_proc)) {
			if (bp)
				brelse(bp);
			return (error);
		}
		/*
		 * Return pointer to current entry in dp->i_offset.
		 * Save directory inode pointer in ndp->ni_dvp for dirremove().
		 */
		if (dp->de_StartCluster == scn && isadir) {	/* "." */
			VREF(vdp);
			*vpp = vdp;
			if (bp)
				brelse(bp);
			return (0);
		}
		if (error = deget(pmp, cluster, diroff, dep, &tdp)) {
			if (bp)
				brelse(bp);
			return (error);
		}
		*vpp = DETOV(tdp);
		if (!lockparent)
			VOP_UNLOCK(vdp);
		if (bp)
			brelse(bp);
		return (0);
	}

	/*
	 * If rewriting (RENAME), return the inode and the
	 * information required to rewrite the present directory
	 * Must get inode of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (nameiop == RENAME && wantparent &&
	    (flags & ISLASTCN)) {
		if (error = VOP_ACCESS(vdp, VWRITE, cnp->cn_cred, cnp->cn_proc)) {
			if (bp)
				brelse(bp);
			return (error);
		}
		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
		if (dp->de_StartCluster == scn && isadir) {
			if (bp)
				brelse(bp);
			return (EISDIR);
		}
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			if (bp)
				brelse(bp);
			return (error);
		}
		*vpp = DETOV(tdp);
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(vdp);
		if (bp)
			brelse(bp);
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not `vput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the VFS_VGET for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	pdp = vdp;
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(pdp);	/* race to get the inode */
		if (error = deget(pmp, cluster, diroff, dep, &tdp)) {
			VOP_LOCK(pdp);
			if (bp)
				brelse(bp);
			return (error);
		}
		if (lockparent && (flags & ISLASTCN) &&
		    (error = VOP_LOCK(pdp))) {
			vput(DETOV(tdp));
			if (bp)
				brelse(bp);
			return (error);
		}
		*vpp = DETOV(tdp);
	} else if (dp->de_StartCluster == scn && isadir) {
		VREF(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		if (error = deget(pmp, cluster, diroff, dep, &tdp)) {
			if (bp)
				brelse(bp);
			return (error);
		}
		if (!lockparent || !(flags & ISLASTCN))
			VOP_UNLOCK(pdp);
		*vpp = DETOV(tdp);
	}
	if (bp)
		brelse(bp);

	/*
	 * Insert name into cache if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
	return (0);
}

/*
 * dep  - directory entry to copy into the directory
 * ddep - directory to add to
 * depp - return the address of the denode for the created directory entry
 *	  if depp != 0
 */
int
createde(dep, ddep, depp)
	struct denode *dep;
	struct denode *ddep;
	struct denode **depp;
{
	int error;
	u_long dirclust, diroffset;
	struct direntry *ndep;
	struct msdosfsmount *pmp = ddep->de_pmp;
	struct buf *bp;

#ifdef MSDOSFS_DEBUG
	printf("createde(dep %08x, ddep %08x, depp %08x)\n", dep, ddep, depp);
#endif

	/*
	 * If no space left in the directory then allocate another cluster
	 * and chain it onto the end of the file.  There is one exception
	 * to this.  That is, if the root directory has no more space it
	 * can NOT be expanded.  extendfile() checks for and fails attempts
	 * to extend the root directory.  We just return an error in that
	 * case.
	 */
	if (ddep->de_fndclust == (u_long)-1) {
		if (error = extendfile(ddep, 1, &bp, &dirclust, DE_CLEAR))
			return (error);
		ndep = (struct direntry *)bp->b_data;
		/*
		 * Let caller know where we put the directory entry.
		 */
		ddep->de_fndclust = dirclust;
		ddep->de_fndoffset = diroffset = 0;
		/*
		 * Update the size of the directory
		 */
		ddep->de_FileSize += pmp->pm_bpcluster;
	} else {
		/*
		 * There is space in the existing directory.  So, we just
		 * read in the cluster with space.  Copy the new directory
		 * entry in.  Then write it to disk. NOTE:  DOS directories
		 * do not get smaller as clusters are emptied.
		 */
		dirclust = ddep->de_fndclust;
		diroffset = ddep->de_fndoffset;
		if (error = readep(pmp, dirclust, diroffset, &bp, &ndep))
			return (error);
	}
	DE_EXTERNALIZE(ndep, dep);

	/*
	 * If they want us to return with the denode gotten.
	 */
	if (depp) {
		if (error = deget(pmp, dirclust, diroffset, ndep, depp))
			return (error);
	}
	if (error = bwrite(bp)) {
		vput(DETOV(*depp));	/* free the vnode we got on error */
		return (error);
	}
	return (0);
}

/*
 * Read in a directory entry and mark it as being deleted.
 */
int
markdeleted(pmp, dirclust, diroffset)
	struct msdosfsmount *pmp;
	u_long dirclust;
	u_long diroffset;
{
	int error;
	struct direntry *ep;
	struct buf *bp;

	if (error = readep(pmp, dirclust, diroffset, &bp, &ep))
		return (error);
	ep->deName[0] = SLOT_DELETED;
	return (bwrite(bp));
}

/*
 * Remove a directory entry. At this point the file represented by the
 * directory entry to be removed is still full length until no one has it
 * open.  When the file no longer being used msdosfs_inactive() is called
 * and will truncate the file to 0 length.  When the vnode containing the
 * denode is needed for some other purpose by VFS it will call
 * msdosfs_reclaim() which will remove the denode from the denode cache.
 */
int
removede(pdep, dep)
	struct denode *pdep;	/* directory where the entry is removed */
	struct denode *dep;	/* file to be removed */
{
	int error;

#ifdef MSDOSFS_DEBUG
	printf("removede(): filename %s\n", dep->de_Name);
	printf("removede(): dep %08x, ndpcluster %d, ndpoffset %d\n",
	       dep, pdep->de_fndclust, pdep->de_fndoffset);
#endif

	/*
	 * Read the directory block containing the directory entry we are
	 * to make free.  The nameidata structure holds the cluster number
	 * and directory entry index number of the entry to free.
	 */
	if (error = markdeleted(pdep->de_pmp, pdep->de_fndclust, pdep->de_fndoffset))
		return (error);
	dep->de_refcnt--;
	return (0);
}

/*
 * Be sure a directory is empty except for "." and "..". Return 1 if empty,
 * return 0 if not empty or error.
 */
int
dosdirempty(dep)
	struct denode *dep;
{
	int blsize;
	int error;
	u_long cn;
	daddr_t bn;
	struct buf *bp;
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;

	/*
	 * Since the filesize field in directory entries for a directory is
	 * zero, we just have to feel our way through the directory until
	 * we hit end of file.
	 */
	for (cn = 0;; cn++) {
		error = pcbmap(dep, cn, &bn, 0, &blsize);
		if (error == E2BIG)
			return (1);	/* it's empty */
		if (error = bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp))
			return (error);
		for (dentp = (struct direntry *)bp->b_data;
		     (char *)dentp < bp->b_data + blsize;
		     dentp++) {
			if (dentp->deName[0] != SLOT_DELETED &&
			    (dentp->deAttributes & ATTR_VOLUME) == 0) {
				/*
				 * In dos directories an entry whose name
				 * starts with SLOT_EMPTY (0) starts the
				 * beginning of the unused part of the
				 * directory, so we can just return that it
				 * is empty.
				 */
				if (dentp->deName[0] == SLOT_EMPTY) {
					brelse(bp);
					return (1);
				}
				/*
				 * Any names other than "." and ".." in a
				 * directory mean it is not empty.
				 */
				if (bcmp(dentp->deName, ".          ", 11) &&
				    bcmp(dentp->deName, "..         ", 11)) {
					brelse(bp);
#ifdef MSDOSFS_DEBUG
					printf("dosdirempty(): entry found %02x, %02x\n",
					       dentp->deName[0], dentp->deName[1]);
#endif
					return (0);	/* not empty */
				}
			}
		}
		brelse(bp);
	}
	/* NOTREACHED */
}

/*
 * Check to see if the directory described by target is in some
 * subdirectory of source.  This prevents something like the following from
 * succeeding and leaving a bunch or files and directories orphaned. mv
 * /a/b/c /a/b/c/d/e/f Where c and f are directories.
 *
 * source - the inode for /a/b/c
 * target - the inode for /a/b/c/d/e/f
 *
 * Returns 0 if target is NOT a subdirectory of source.
 * Otherwise returns a non-zero error number.
 * The target inode is always unlocked on return.
 */
int
doscheckpath(source, target)
	struct denode *source;
	struct denode *target;
{
	daddr_t scn;
	struct msdosfsmount *pmp;
	struct direntry *ep;
	struct denode *dep;
	struct buf *bp = NULL;
	int error = 0;

	dep = target;
	if ((target->de_Attributes & ATTR_DIRECTORY) == 0 ||
	    (source->de_Attributes & ATTR_DIRECTORY) == 0) {
		error = ENOTDIR;
		goto out;
	}
	if (dep->de_StartCluster == source->de_StartCluster) {
		error = EEXIST;
		goto out;
	}
	if (dep->de_StartCluster == MSDOSFSROOT)
		goto out;
	for (;;) {
		if ((dep->de_Attributes & ATTR_DIRECTORY) == 0) {
			error = ENOTDIR;
			goto out;
		}
		pmp = dep->de_pmp;
		scn = dep->de_StartCluster;
		if (error = bread(pmp->pm_devvp, cntobn(pmp, scn),
				  pmp->pm_bpcluster, NOCRED, &bp))
			break;
		ep = (struct direntry *) bp->b_data + 1;
		if ((ep->deAttributes & ATTR_DIRECTORY) == 0 ||
		    bcmp(ep->deName, "..         ", 11) != 0) {
			error = ENOTDIR;
			break;
		}
		scn = getushort(ep->deStartCluster);
		if (scn == source->de_StartCluster) {
			error = EINVAL;
			break;
		}
		if (scn == MSDOSFSROOT)
			break;
		vput(DETOV(dep));
		/* NOTE: deget() clears dep on error */
		error = deget(pmp, scn, 0, ep, &dep);
		brelse(bp);
		bp = NULL;
		if (error)
			break;
	}
out:;
	if (bp)
		brelse(bp);
	if (error == ENOTDIR)
		printf("doscheckpath(): .. not a directory?\n");
	if (dep != NULL)
		vput(DETOV(dep));
	return (error);
}

/*
 * Read in the disk block containing the directory entry (dirclu, dirofs)
 * and return the address of the buf header, and the address of the
 * directory entry within the block.
 */
int
readep(pmp, dirclust, diroffset, bpp, epp)
	struct msdosfsmount *pmp;
	u_long dirclust, diroffset;
	struct buf **bpp;
	struct direntry **epp;
{
	int error;
	daddr_t bn;
	int blsize;
	u_long boff;

	boff = diroffset & ~pmp->pm_crbomask;
	blsize = pmp->pm_bpcluster;
	if (dirclust == MSDOSFSROOT
	    && boff + blsize > (pmp->pm_rootdirsize << pmp->pm_bnshift))
		blsize = (pmp->pm_rootdirsize << pmp->pm_bnshift) - boff;
	bn = detobn(pmp, dirclust, diroffset);
	if (error = bread(pmp->pm_devvp, bn, blsize, NOCRED, bpp)) {
		*bpp = NULL;
		return (error);
	}
	if (epp)
		*epp = bptoep(pmp, *bpp, diroffset);
	return (0);
}

/*
 * Read in the disk block containing the directory entry dep came from and
 * return the address of the buf header, and the address of the directory
 * entry within the block.
 */
int
readde(dep, bpp, epp)
	struct denode *dep;
	struct buf **bpp;
	struct direntry **epp;
{

	return (readep(dep->de_pmp, dep->de_dirclust, dep->de_diroffset,
	    bpp, epp));
}
