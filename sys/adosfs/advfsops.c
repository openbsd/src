/*	$OpenBSD: advfsops.c,v 1.16 2000/02/07 04:57:15 assar Exp $	*/
/*	$NetBSD: advfsops.c,v 1.24 1996/12/22 10:10:12 cgd Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1996 Matthias Scheler
 * All rights reserved.
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/buf.h>

#include <machine/endian.h>

#include <miscfs/specfs/specdev.h> /* XXX */
#include <adosfs/adosfs.h>

int	 adosfs_init __P((struct vfsconf *));
int	 adosfs_mount __P((struct mount *, const char *, caddr_t, struct nameidata *,
    struct proc *));
int	 adosfs_start __P((struct mount *, int, struct proc *));
int	 adosfs_unmount __P((struct mount *, int, struct proc *));
int	 adosfs_root __P((struct mount *, struct vnode **));
int	 adosfs_quotactl __P((struct mount *, int, uid_t, caddr_t,
    struct proc *));
int	 adosfs_statfs __P((struct mount *, struct statfs *, struct proc *));
int	 adosfs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int	 adosfs_vget __P((struct mount *, ino_t, struct vnode **));
int	 adosfs_fhtovp __P((struct mount *, struct fid *, struct vnode **));

int	 adosfs_vptofh __P((struct vnode *, struct fid *));

int	 adosfs_mountfs __P((struct vnode *, struct mount *, struct proc *));
int	 adosfs_loadbitmap __P((struct adosfsmount *));

int
adosfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;
	struct adosfs_args args;
	struct adosfsmount *amp;
	size_t size;
	int error;
	mode_t accessmode;

	error = copyin(data, (caddr_t)&args, sizeof(struct adosfs_args));
	if (error)
		return(error);
	
#if 0
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);
#endif
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EROFS);
	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		amp = VFSTOADOSFS(mp);
		if (args.fspec == 0)
			return (vfs_export(mp, &amp->export, &args.export));
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	if ((error = namei(ndp)) != 0)
		return (error);
	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return (ENXIO);
	}
	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (p->p_ucred->cr_uid != 0) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE  | LK_RETRY, p);
		error = VOP_ACCESS(devvp, accessmode, p->p_ucred, p);
		if (error) {
			vput(devvp);
			return (error);
		}
		VOP_UNLOCK(devvp, 0, p);
	}
/* MNT_UPDATE? */
	if ((error = adosfs_mountfs(devvp, mp, p)) != 0) {
		vrele(devvp);
		return (error);
	}
	amp = VFSTOADOSFS(mp);
	amp->uid = args.uid;
	amp->gid = args.gid;
	amp->mask = args.mask;
	(void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void)copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	bcopy(&args, &mp->mnt_stat.mount_info.adosfs_args, sizeof(args));
	return (0);
}

int
adosfs_mountfs(devvp, mp, p)
	struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
{
	struct disklabel dl;
	struct partition *parp;
	struct adosfsmount *amp;
	struct buf *bp;
	struct vnode *rvp;
	int error, part, i;

	part = DISKPART(devvp->v_rdev);
	amp = NULL;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if ((error = vfs_mountedon(devvp)) != 0)
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	if ((error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0)) != 0)
		return (error);

	/* 
	 * open blkdev and read root block
	 */
	if ((error = VOP_OPEN(devvp, FREAD, NOCRED, p)) != 0)
		return (error);
	error = VOP_IOCTL(devvp, DIOCGDINFO,(caddr_t)&dl, FREAD, NOCRED, p);
	if (error)
		goto fail;

	parp = &dl.d_partitions[part];
	amp = malloc(sizeof(struct adosfsmount), M_ADOSFSMNT, M_WAITOK);
	bzero((char *)amp, sizeof(struct adosfsmount));
	amp->mp = mp;
	if (dl.d_type == DTYPE_FLOPPY) {
		amp->bsize = dl.d_secsize;
		amp->secsperblk = 1;
	}
	else {
		amp->bsize = parp->p_fsize * parp->p_frag;
		amp->secsperblk = parp->p_frag;
	}
	amp->rootb = (parp->p_size / amp->secsperblk - 1 + parp->p_cpg) >> 1;
	amp->numblks = parp->p_size / amp->secsperblk - parp->p_cpg;

	bp = NULL;
	if ((error = bread(devvp, (daddr_t)BBOFF,
			   amp->bsize, NOCRED, &bp)) != 0)
		goto fail;

	amp->dostype = adoswordn(bp, 0);
	brelse(bp);

	if (amp->dostype < 0x444f5300 || amp->dostype > 0x444f5305) {
		error = EINVAL;
		goto fail;
	}

	amp->nwords = amp->bsize >> 2;
	amp->dbsize = amp->bsize - (IS_FFS(amp) ? 0 : OFS_DATA_OFFSET);
	amp->devvp = devvp;
	
	mp->mnt_data = (qaddr_t)amp;
        mp->mnt_stat.f_fsid.val[0] = (int32_t)devvp->v_rdev;
        mp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_ADOSFS);
	mp->mnt_flag |= MNT_LOCAL;
	devvp->v_specmountpoint = mp;

	/*
	 * init anode table.
	 */
	for (i = 0; i < ANODEHASHSZ; i++) 
		LIST_INIT(&amp->anodetab[i]);

	/*
	 * get the root anode, if not a valid fs this will fail.
	 */
	if ((error = VFS_ROOT(mp, &rvp)) != 0)
		goto fail;
	/* allocate and load bitmap, set free space */
	amp->bitmap = malloc(((amp->numblks + 31) / 32) * sizeof(*amp->bitmap),
	    M_ADOSFSBITMAP, M_WAITOK);
	if (amp->bitmap)
		adosfs_loadbitmap(amp);
	if (mp->mnt_flag & MNT_RDONLY && amp->bitmap) {
		/*
		 * Don't need the bitmap any more if it's read-only.
		 */
		free(amp->bitmap, M_ADOSFSBITMAP);
		amp->bitmap = NULL;
	}
	vput(rvp);

	return(0);

fail:
	(void) VOP_CLOSE(devvp, FREAD, NOCRED, p);
	if (amp && amp->bitmap)
		free(amp->bitmap, M_ADOSFSBITMAP);
	if (amp)
		free(amp, M_ADOSFSMNT);
	return (error);
}

int
adosfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
}

int
adosfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct adosfsmount *amp;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return (error);
	amp = VFSTOADOSFS(mp);
	amp->devvp->v_specmountpoint = NULL;
	error = VOP_CLOSE(amp->devvp, FREAD, NOCRED, p);
	vrele(amp->devvp);
	if (amp->bitmap)
		free(amp->bitmap, M_ADOSFSBITMAP);
	free(amp, M_ADOSFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

int
adosfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *nvp;
	int error;

	if ((error = VFS_VGET(mp, ABLKTOINO(VFSTOADOSFS(mp)->rootb), &nvp)) !=
	    0)
		return (error);
	/* XXX verify it's a root block? */
	*vpp = nvp;
	return (0);
}

int
adosfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct adosfsmount *amp;

	amp = VFSTOADOSFS(mp);
	sbp->f_bsize = amp->bsize;
	sbp->f_iosize = amp->dbsize;
	sbp->f_blocks = amp->numblks;
	sbp->f_bfree = amp->freeblks;
	sbp->f_bavail = amp->freeblks;
	sbp->f_files = 0;		/* who knows */
	sbp->f_ffree = 0;		/* " " */
	if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
		bcopy(&mp->mnt_stat.mount_info.adosfs_args,
		    &sbp->mount_info.adosfs_args, sizeof(struct adosfs_args));
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return (0);
}

/* 
 * lookup an anode, check mount's hash table if not found, create
 * return locked and referenced al la vget(vp, 1);
 */
int
adosfs_vget(mp, an, vpp)
	struct mount *mp;
	ino_t an;
	struct vnode **vpp;
{
	struct adosfsmount *amp;
	struct vnode *vp;
	struct anode *ap;
	struct buf *bp;
	char *nam, *tmp;
	int namlen, error;

	error = 0;
	amp = VFSTOADOSFS(mp);
	bp = NULL;

	/* 
	 * check hash table. we are done if found
	 */
 retry:
	if ((*vpp = adosfs_ahashget(mp, an)) != NULL)
		return (0);

	error = getnewvnode(VT_ADOSFS, mp, adosfs_vnodeop_p, &vp);
	if (error)
		return (error);

	/*
	 * setup, insert in hash, and lock before io.
	 */
	vp->v_data = ap = malloc(sizeof(struct anode), M_ANODE, M_WAITOK);
	bzero(ap, sizeof(struct anode));
	lockinit(&ap->a_lock, PINOD, "anode", 0, 0);
	ap->vp = vp;
	ap->amp = amp;
	ap->block = AINOTOBLK(an);
	ap->nwords = amp->nwords;
	error = adosfs_ainshash(amp, ap);

	if (error) {
		vrele (vp);

		if (error == EEXIST)
			goto retry;

		return (error);
	}

	if ((error = bread(amp->devvp, an * amp->secsperblk, amp->bsize,
	    NOCRED, &bp)) != 0) {
		vput(vp);
		return (error);
	}

	/*
	 * get type and fill rest in based on that.
	 */
	switch (ap->type = adosfs_getblktype(amp, bp)) {
	case AROOT:
		vp->v_type = VDIR;
		vp->v_flag |= VROOT;
		ap->mtimev.days = adoswordn(bp, ap->nwords - 10);
		ap->mtimev.mins = adoswordn(bp, ap->nwords - 9);
		ap->mtimev.ticks = adoswordn(bp, ap->nwords - 8);
		ap->created.days = adoswordn(bp, ap->nwords - 7);
		ap->created.mins = adoswordn(bp, ap->nwords - 6);
		ap->created.ticks = adoswordn(bp, ap->nwords - 5);
		break;
	case ALDIR:
	case ADIR:
		vp->v_type = VDIR;
		break;
	case ALFILE:
	case AFILE:
		vp->v_type = VREG;
		ap->fsize = adoswordn(bp, ap->nwords - 47);
		break;
	case ASLINK:		/* XXX soft link */
		vp->v_type = VLNK;
		/*
		 * convert from BCPL string and
		 * from: "part:dir/file" to: "/part/dir/file"
		 */
		nam = bp->b_data + (6 * sizeof(u_int32_t));
		namlen = strlen(nam);
		tmp = nam;
		while (*tmp && *tmp != ':')
			tmp++;
		if (*tmp == 0) {
			ap->slinkto = malloc(namlen + 1, M_ANODE, M_WAITOK);
			bcopy(nam, ap->slinkto, namlen);
		} else if (*nam == ':') {
			ap->slinkto = malloc(namlen + 1, M_ANODE, M_WAITOK);
			bcopy(nam, ap->slinkto, namlen);
			ap->slinkto[0] = '/';
		} else {
			ap->slinkto = malloc(namlen + 2, M_ANODE, M_WAITOK);
			ap->slinkto[0] = '/';
			bcopy(nam, &ap->slinkto[1], namlen);
			ap->slinkto[tmp - nam + 1] = '/';
			namlen++;
		}
		ap->slinkto[namlen] = 0;
		ap->fsize = namlen;
		break;
	default:
		brelse(bp);
		vput(vp);
		return (EINVAL);
	}

	/*
	 * Get appropriate data from this block;  hard link needs
	 * to get other data from the "real" block.
	 */

	/*
	 * copy in name (from original block)
	 */
	nam = bp->b_data + (ap->nwords - 20) * sizeof(u_int32_t);
	namlen = *(u_char *)nam++;
	if (namlen > 30) {
#ifdef DIAGNOSTIC
		printf("adosfs: aget: name length too long blk %d\n", an);
#endif
		brelse(bp);
		vput(vp);
		return (EINVAL);
	}
	bcopy(nam, ap->name, namlen);
	ap->name[namlen] = 0;

	/* 
	 * if dir alloc hash table and copy it in 
	 */
	if (vp->v_type == VDIR) {
		int i;

		ap->ntabent = ANODETABENT(ap);
		ap->tab = malloc(ap->ntabent * (sizeof(daddr_t) + sizeof(int)),
		    M_ANODE, M_WAITOK);
		ap->tabi = (int *)&ap->tab[ap->ntabent];
		bzero(ap->tabi, ANODETABENT(ap) * sizeof(int));
		for (i = 0; i < ap->ntabent; i++)
			ap->tab[i] = (daddr_t)adoswordn(bp, i + 6);
	}

	/*
	 * misc.
	 */
	ap->pblock = (daddr_t)adoswordn(bp, ap->nwords - 3);
	ap->hashf = (daddr_t)adoswordn(bp, ap->nwords - 4);
	ap->linknext = (daddr_t)adoswordn(bp, ap->nwords - 10);
	ap->linkto = (daddr_t)adoswordn(bp, ap->nwords - 11);

	/*
	 * setup last indirect block cache.
	 */
	ap->lastlindblk = 0;
	if (ap->type == AFILE)  {
		ap->lastindblk = ap->block;
		if (adoswordn(bp, ap->nwords - 10))
			ap->linkto = ap->block;
	} else if (ap->type == ALFILE) {
		ap->lastindblk = ap->linkto;
		brelse(bp);
		bp = NULL;
		error = bread(amp->devvp, ap->linkto * amp->secsperblk,
		    amp->bsize, NOCRED, &bp);
		ap->fsize = adoswordn(bp, ap->nwords - 47);
		/*
		 * Should ap->block be set to the real file header block?
		 */
		ap->block = ap->linkto;
	}

	if (ap->type == AROOT) {
		ap->adprot = 15;
		ap->uid = amp->uid;
		ap->gid = amp->gid;
	} else {
		ap->adprot = adoswordn(bp, ap->nwords - 48) ^ 15;
		/*
		 * Get uid/gid from extensions in file header
		 * (really need to know if this is a muFS partition)
		 */
		ap->uid = (adoswordn(bp, ap->nwords - 49) >> 16) & 0xffff;
		ap->gid = adoswordn(bp, ap->nwords - 49) & 0xffff;
		if (ap->uid || ap->gid) {
			if (ap->uid == 0xffff)
				ap->uid = 0;
			if (ap->gid == 0xffff)
				ap->gid = 0;
			ap->adprot |= 0x40000000;	/* Kludge */
		}
		else {
			/*
			 * uid & gid extension don't exist,
			 * so use the mount-point uid/gid
			 */
			ap->uid = amp->uid;
			ap->gid = amp->gid;
		}
	}
	ap->mtime.days = adoswordn(bp, ap->nwords - 23);
	ap->mtime.mins = adoswordn(bp, ap->nwords - 22);
	ap->mtime.ticks = adoswordn(bp, ap->nwords - 21);

	*vpp = vp;		/* return vp */
	brelse(bp);		/* release buffer */
	return (0);
}

/*
 * Load the bitmap into memory, and count the number of available
 * blocks.
 * The bitmap will be released if the filesystem is read-only;  it's
 * only needed to find the free space.
 */
int
adosfs_loadbitmap(amp)
	struct adosfsmount *amp;
{
	struct buf *bp, *mapbp;
	u_int32_t bits;
	daddr_t bn;
	int blkix, endix, mapix, bmsize, n;
	int error;

	bp = mapbp = NULL;
	bn = amp->rootb;
	if ((error = bread(amp->devvp, bn * amp->secsperblk, amp->bsize,
	    NOCRED, &bp)) != 0)
		return (error);
	blkix = amp->nwords - 49;
	endix = amp->nwords - 24;
	mapix = 0;
	bmsize = (amp->numblks + 31) / 32;
	while (mapix < bmsize) {
		if (adoswordn(bp, blkix) == 0)
			break;
		if (mapbp != NULL)
			brelse(mapbp);
		if ((error = bread(amp->devvp,
		    adoswordn(bp, blkix) * amp->secsperblk, amp->bsize,
		     NOCRED, &mapbp)) != 0)
			break;
		if (adoscksum(mapbp, amp->nwords)) {
#ifdef DIAGNOSTIC
			printf("adosfs: loadbitmap - cksum of blk %d failed\n",
			    adoswordn(bp, blkix));
#endif
			/* XXX Force read-only?  Set free space 0? */
			break;
		}
		n = 1;
		while (n < amp->nwords && mapix < bmsize) {
			amp->bitmap[mapix++] = bits = adoswordn(mapbp, n);
			++n;
			if (mapix == bmsize && amp->numblks & 31)
				bits &= ~(0xffffffff << (amp->numblks & 31));
			while (bits) {
				if (bits & 1)
					++amp->freeblks;
				bits >>= 1;
			}
		}
		++blkix;
		if (mapix < bmsize && blkix == endix) {
			bn = (daddr_t)adoswordn(bp, blkix);
			brelse(bp);
			if ((error = bread(amp->devvp, bn * amp->secsperblk,
			    amp->bsize, NOCRED, &bp)) != 0)
				break;
			/*
			 * Why is there no checksum on these blocks?
			 */
			blkix = 0;
			endix = amp->nwords - 1;
		}
	}
	if (bp)
		brelse(bp);
	if (mapbp)
		brelse(mapbp);
	return (error);
}


/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is in range
 * - call iget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the generation number matches
 */

/*
 * This is using the same layout as struct fid from mount.h for the first two
 * fields.
 */
struct ifid {
	u_short	ifid_len;
	u_short	ifid_pad;
	ino_t	ifid_ino;
	daddr_t	ifid_start;
};

int
adosfs_fhtovp(mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	struct ifid *ifhp = (struct ifid *)fhp;
	struct adosfsmount *amp = VFSTOADOSFS(mp);
#if 0
	struct anode *ap;
#endif
	struct netcred *np;
	struct vnode *nvp;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	printf("adfhtovp(%x, %x, %x)\n", mp, fhp, vpp);
#endif
	
	if ((error = VFS_VGET(mp, ifhp->ifid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
#if 0
	ap = VTOA(nvp);
	if (ap->inode.iso_mode == 0) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
#endif
	*vpp = nvp;
	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return(0);
}

int
adosfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	struct anode *ap = VTOA(vp);
	struct ifid *ifhp;

	ifhp = (struct ifid *)fhp;
	ifhp->ifid_len = sizeof(struct ifid);
	
	ifhp->ifid_ino = ABLKTOINO(ap->block);
	ifhp->ifid_start = ap->block;
	
#ifdef ADOSFS_DIAGNOSTIC
	printf("advptofh(%x, %x)\n", vp, fhp);
#endif
	return(0);
}

int
adosfs_quotactl(mp, cmds, uid, arg, p)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return(EOPNOTSUPP);
}

int
adosfs_sync(mp, waitfor, uc, p)
	struct mount *mp;
	int waitfor;
	struct ucred *uc;
	struct proc *p;
{
#ifdef ADOSFS_DIAGNOSTIC
	printf("ad_sync(%x, %x)\n", mp, waitfor);
#endif
	return(0);
}

int
adosfs_init(struct vfsconf *vfsp)
{
	return (0);
}

/*
 * vfs generic function call table
 */
struct vfsops adosfs_vfsops = {
	adosfs_mount,
	adosfs_start,
	adosfs_unmount,
	adosfs_root,
	adosfs_quotactl,                
	adosfs_statfs,                  
	adosfs_sync,                    
	adosfs_vget,
	adosfs_fhtovp,                  
	adosfs_vptofh,                  
	adosfs_init,                    
};           
