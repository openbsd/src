/*	$NetBSD: msdosfs_lookup.c,v 1.35 2016/01/30 09:59:27 mlelstv Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
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

#include <ffs/buf.h>

#include <fs/msdosfs/bpb.h>
#include <fs/msdosfs/direntry.h>
#include <fs/msdosfs/denode.h>
#include <fs/msdosfs/msdosfsmount.h>
#include <fs/msdosfs/fat.h>



/*
 * dep  - directory entry to copy into the directory
 * ddep - directory to add to
 * depp - return the address of the denode for the created directory entry
 *	  if depp != 0
 * cnp  - componentname needed for Win95 long filenames
 */
int
createde(struct denode *dep, struct denode *ddep, struct denode **depp, struct componentname *cnp)
{
	int error, rberror;
	u_long dirclust, clusoffset;
	u_long fndoffset, havecnt = 0, wcnt = 1, i;
	struct direntry *ndep;
	struct msdosfsmount *pmp = ddep->de_pmp;
	struct buf *bp;
	daddr_t bn;
	int blsize;
#define async 0

#ifdef MSDOSFS_DEBUG
	printf("createde(dep %p, ddep %p, depp %p, cnp %p)\n",
	    dep, ddep, depp, cnp);
#endif

	/*
	 * If no space left in the directory then allocate another cluster
	 * and chain it onto the end of the file.  There is one exception
	 * to this.  That is, if the root directory has no more space it
	 * can NOT be expanded.  extendfile() checks for and fails attempts
	 * to extend the root directory.  We just return an error in that
	 * case.
	 */
	if (ddep->de_fndoffset >= ddep->de_FileSize) {
		u_long needlen = ddep->de_fndoffset + sizeof(struct direntry)
		    - ddep->de_FileSize;
		dirclust = de_clcount(pmp, needlen);
		if ((error = extendfile(ddep, dirclust, 0, 0, DE_CLEAR)) != 0) {
			(void)detrunc(ddep, ddep->de_FileSize, 0, NOCRED);
			goto err_norollback;
		}

		/*
		 * Update the size of the directory
		 */
		ddep->de_FileSize += de_cn2off(pmp, dirclust);
	}

	/*
	 * We just read in the cluster with space.  Copy the new directory
	 * entry in.  Then write it to disk. NOTE:  DOS directories
	 * do not get smaller as clusters are emptied.
	 */
	error = pcbmap(ddep, de_cluster(pmp, ddep->de_fndoffset),
		       &bn, &dirclust, &blsize);
	if (error)
		goto err_norollback;
	clusoffset = ddep->de_fndoffset;
	if (dirclust != MSDOSFSROOT)
		clusoffset &= pmp->pm_crbomask;
	if ((error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn), blsize,
	    B_MODIFY, &bp)) != 0) {
		goto err_norollback;
	}
	ndep = bptoep(pmp, bp, clusoffset);

	DE_EXTERNALIZE(ndep, dep);

	/*
	 * Now write the Win95 long name
	 */
	if (ddep->de_fndcnt > 0) {
		u_int8_t chksum = winChksum(ndep->deName);
		const u_char *un = (const u_char *)cnp->cn_nameptr;
		int unlen = cnp->cn_namelen;
		u_long xhavecnt;

		fndoffset = ddep->de_fndoffset;
		xhavecnt = ddep->de_fndcnt + 1;

		for(; wcnt < xhavecnt; wcnt++) {
			if ((fndoffset & pmp->pm_crbomask) == 0) {
				/* we should never get here if ddep is root
				 * directory */

				if (async)
					(void) bdwrite(bp);
				else if ((error = bwrite(bp)) != 0)
					goto rollback;

				fndoffset -= sizeof(struct direntry);
				error = pcbmap(ddep,
					       de_cluster(pmp, fndoffset),
					       &bn, 0, &blsize);
				if (error)
					goto rollback;

				error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn),
				    blsize, B_MODIFY, &bp);
				if (error) {
					goto rollback;
				}
				ndep = bptoep(pmp, bp,
						fndoffset & pmp->pm_crbomask);
			} else {
				ndep--;
				fndoffset -= sizeof(struct direntry);
			}
			if (!unix2winfn(un, unlen, (struct winentry *)ndep,
					wcnt, chksum,
					ddep->de_pmp->pm_flags & MSDOSFSMNT_UTF8))
				break;
		}
	}

	if (async)
		bdwrite(bp);
	else if ((error = bwrite(bp)) != 0)
		goto rollback;

	/*
	 * If they want us to return with the denode gotten.
	 */
	if (depp) {
		u_long diroffset = clusoffset;

		if (dep->de_Attributes & ATTR_DIRECTORY) {
			dirclust = dep->de_StartCluster;
			if (FAT32(pmp) && dirclust == pmp->pm_rootdirblk)
				dirclust = MSDOSFSROOT;
			if (dirclust == MSDOSFSROOT)
				diroffset = MSDOSFSROOT_OFS;
			else
				diroffset = 0;
		}
		error = deget(pmp, dirclust, diroffset, depp);
		return error;
	}

	return 0;

    rollback:
	/*
	 * Mark all slots modified so far as deleted. Note that we
	 * can't just call removede(), since directory is not in
	 * consistent state.
	 */
	fndoffset = ddep->de_fndoffset;
	rberror = pcbmap(ddep, de_cluster(pmp, fndoffset),
	       &bn, NULL, &blsize);
	if (rberror)
		goto err_norollback;
	if ((rberror = bread(pmp->pm_devvp, de_bn2kb(pmp, bn), blsize,
	    B_MODIFY, &bp)) != 0) {
		goto err_norollback;
	}
	ndep = bptoep(pmp, bp, clusoffset);

	havecnt = ddep->de_fndcnt + 1;
	for(i = wcnt; i <= havecnt; i++) {
		/* mark entry as deleted */
		ndep->deName[0] = SLOT_DELETED;

		if ((fndoffset & pmp->pm_crbomask) == 0) {
			/* we should never get here if ddep is root
			 * directory */

			if (async)
				bdwrite(bp);
			else if ((rberror = bwrite(bp)) != 0)
				goto err_norollback;

			fndoffset -= sizeof(struct direntry);
			rberror = pcbmap(ddep,
				       de_cluster(pmp, fndoffset),
				       &bn, 0, &blsize);
			if (rberror)
				goto err_norollback;

			rberror = bread(pmp->pm_devvp, de_bn2kb(pmp, bn),
			    blsize, B_MODIFY, &bp);
			if (rberror) {
				goto err_norollback;
			}
			ndep = bptoep(pmp, bp, fndoffset);
		} else {
			ndep--;
			fndoffset -= sizeof(struct direntry);
		}
	}

	/* ignore any further error */
	if (async)
		(void) bdwrite(bp);
	else
		(void) bwrite(bp);

    err_norollback:
	return error;
}

/*
 * Be sure a directory is empty except for "." and "..". Return 1 if empty,
 * return 0 if not empty or error.
 */
int
dosdirempty(struct denode *dep)
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
		if ((error = pcbmap(dep, cn, &bn, 0, &blsize)) != 0) {
			if (error == E2BIG)
				return (1);	/* it's empty */
			return (0);
		}
		error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn), blsize,
		    0, &bp);
		if (error) {
			return (0);
		}
		for (dentp = (struct direntry *)bp->b_data;
		     (char *)dentp < (char *)bp->b_data + blsize;
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
					brelse(bp, 0);
					return (1);
				}
				/*
				 * Any names other than "." and ".." in a
				 * directory mean it is not empty.
				 */
				if (memcmp(dentp->deName, ".          ", 11) &&
				    memcmp(dentp->deName, "..         ", 11)) {
					brelse(bp, 0);
#ifdef MSDOSFS_DEBUG
					printf("dosdirempty(): found %.11s, %d, %d\n",
					    dentp->deName, dentp->deName[0],
						dentp->deName[1]);
#endif
					return (0);	/* not empty */
				}
			}
		}
		brelse(bp, 0);
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
doscheckpath(struct denode *source, struct denode *target)
{
	u_long scn;
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
	pmp = dep->de_pmp;
#ifdef	DIAGNOSTIC
	if (pmp != source->de_pmp)
		panic("doscheckpath: source and target on different filesystems");
#endif
	if (FAT32(pmp) && dep->de_StartCluster == pmp->pm_rootdirblk)
		goto out;

	for (;;) {
		if ((dep->de_Attributes & ATTR_DIRECTORY) == 0) {
			error = ENOTDIR;
			break;
		}
		scn = dep->de_StartCluster;
		error = bread(pmp->pm_devvp, de_bn2kb(pmp, cntobn(pmp, scn)),
			      pmp->pm_bpcluster, 0, &bp);
		if (error)
			break;

		ep = (struct direntry *) bp->b_data + 1;
		if ((ep->deAttributes & ATTR_DIRECTORY) == 0 ||
		    memcmp(ep->deName, "..         ", 11) != 0) {
			error = ENOTDIR;
			break;
		}
		scn = getushort(ep->deStartCluster);
		if (FAT32(pmp))
			scn |= getushort(ep->deHighClust) << 16;

		if (scn == source->de_StartCluster) {
			error = EINVAL;
			break;
		}
		if (scn == MSDOSFSROOT)
			break;
		if (FAT32(pmp) && scn == pmp->pm_rootdirblk) {
			/*
			 * scn should be 0 in this case,
			 * but we silently ignore the error.
			 */
			break;
		}

		vput(DETOV(dep));
		brelse(bp, 0);
		bp = NULL;
		/* NOTE: deget() clears dep on error */
		if ((error = deget(pmp, scn, 0, &dep)) != 0)
			break;
	}
out:
	if (bp)
		brelse(bp, 0);
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
readep(struct msdosfsmount *pmp, u_long dirclust, u_long diroffset, struct buf **bpp, struct direntry **epp)
{
	int error;
	daddr_t bn;
	int blsize;

	blsize = pmp->pm_bpcluster;
	if (dirclust == MSDOSFSROOT
	    && de_blk(pmp, diroffset + blsize) > pmp->pm_rootdirsize)
		blsize = de_bn2off(pmp, pmp->pm_rootdirsize) & pmp->pm_crbomask;
	bn = detobn(pmp, dirclust, diroffset);
	if ((error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn), blsize,
	    0, bpp)) != 0) {
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
readde(struct denode *dep, struct buf **bpp, struct direntry **epp)
{
	return (readep(dep->de_pmp, dep->de_dirclust, dep->de_diroffset,
			bpp, epp));
}

/*
 * Remove a directory entry. At this point the file represented by the
 * directory entry to be removed is still full length until noone has it
 * open.  When the file no longer being used msdosfs_inactive() is called
 * and will truncate the file to 0 length.  When the vnode containing the
 * denode is needed for some other purpose by VFS it will call
 * msdosfs_reclaim() which will remove the denode from the denode cache.
 */
int
removede(struct denode *pdep, struct denode *dep)
	/* pdep:	 directory where the entry is removed */
	/* dep:	 file to be removed */
{
	int error;
	struct direntry *ep;
	struct buf *bp;
	daddr_t bn;
	int blsize;
	struct msdosfsmount *pmp = pdep->de_pmp;
	u_long offset = pdep->de_fndoffset;
#define async 0

#ifdef MSDOSFS_DEBUG
	printf("removede(): filename %s, dep %p, offset %08lx\n",
	    dep->de_Name, dep, offset);
#endif

	if (--dep->de_refcnt == 0) {
	}
	offset += sizeof(struct direntry);
	do {
		offset -= sizeof(struct direntry);
		error = pcbmap(pdep, de_cluster(pmp, offset), &bn, 0, &blsize);
		if (error)
			return error;
		error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn), blsize,
		    B_MODIFY, &bp);
		if (error) {
			return error;
		}
		ep = bptoep(pmp, bp, offset);
		/*
		 * Check whether, if we came here the second time, i.e.
		 * when underflowing into the previous block, the last
		 * entry in this block is a longfilename entry, too.
		 */
		if (ep->deAttributes != ATTR_WIN95
		    && offset != pdep->de_fndoffset) {
			brelse(bp, 0);
			break;
		}
		offset += sizeof(struct direntry);
		while (1) {
			/*
			 * We are a bit agressive here in that we delete any Win95
			 * entries preceding this entry, not just the ones we "own".
			 * Since these presumably aren't valid anyway,
			 * there should be no harm.
			 */
			offset -= sizeof(struct direntry);
			ep--->deName[0] = SLOT_DELETED;
			if ((pmp->pm_flags & MSDOSFSMNT_NOWIN95)
			    || !(offset & pmp->pm_crbomask)
			    || ep->deAttributes != ATTR_WIN95)
				break;
		}
		if (async)
			bdwrite(bp);
		else if ((error = bwrite(bp)) != 0)
			return error;
	} while (!(pmp->pm_flags & MSDOSFSMNT_NOWIN95)
	    && !(offset & pmp->pm_crbomask)
	    && offset);
	return 0;
}

/*
 * Create a unique DOS name in dvp
 */
int
uniqdosname(struct denode *dep, struct componentname *cnp, u_char *cp)
{
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;
	int gen;
	int blsize;
	u_long cn;
	daddr_t bn;
	struct buf *bp;
	int error;

	for (gen = 1;; gen++) {
		/*
		 * Generate DOS name with generation number
		 */
		if (!unix2dosfn((const u_char *)cnp->cn_nameptr, cp,
		    cnp->cn_namelen, gen))
			return gen == 1 ? EINVAL : EEXIST;

		/*
		 * Now look for a dir entry with this exact name
		 */
		for (cn = error = 0; !error; cn++) {
			if ((error = pcbmap(dep, cn, &bn, 0, &blsize)) != 0) {
				if (error == E2BIG)	/* EOF reached and not found */
					return 0;
				return error;
			}
			error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn), blsize,
			    0, &bp);
			if (error) {
				return error;
			}
			for (dentp = (struct direntry *)bp->b_data;
			     (char *)dentp < (char *)bp->b_data + blsize;
			     dentp++) {
				if (dentp->deName[0] == SLOT_EMPTY) {
					/*
					 * Last used entry and not found
					 */
					brelse(bp, 0);
					return 0;
				}
				/*
				 * Ignore volume labels and Win95 entries
				 */
				if (dentp->deAttributes & ATTR_VOLUME)
					continue;
				if (!memcmp(dentp->deName, cp, 11)) {
					error = EEXIST;
					break;
				}
			}
			brelse(bp, 0);
		}
	}
}

/*
 * Find any Win'95 long filename entry in directory dep
 */
int
findwin95(struct denode *dep)
{
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;
	int blsize, win95;
	u_long cn;
	daddr_t bn;
	struct buf *bp;

	win95 = 1;
	/*
	 * Read through the directory looking for Win'95 entries
	 * XXX Note: Error currently handled just as EOF
	 */
	for (cn = 0;; cn++) {
		if (pcbmap(dep, cn, &bn, 0, &blsize))
			return win95;
		if (bread(pmp->pm_devvp, de_bn2kb(pmp, bn), blsize,
		    0, &bp)) {
			return win95;
		}
		for (dentp = (struct direntry *)bp->b_data;
		     (char *)dentp < (char *)bp->b_data + blsize;
		     dentp++) {
			if (dentp->deName[0] == SLOT_EMPTY) {
				/*
				 * Last used entry and not found
				 */
				brelse(bp, 0);
				return win95;
			}
			if (dentp->deName[0] == SLOT_DELETED) {
				/*
				 * Ignore deleted files
				 * Note: might be an indication of Win'95
				 * anyway	XXX
				 */
				continue;
			}
			if (dentp->deAttributes == ATTR_WIN95) {
				brelse(bp, 0);
				return 1;
			}
			win95 = 0;
		}
		brelse(bp, 0);
	}
}
