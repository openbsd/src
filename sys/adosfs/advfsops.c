/*	$NetBSD: advfsops.c,v 1.14.2.1 1995/11/10 16:05:16 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
#include <miscfs/specfs/specdev.h> /* XXX */
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/buf.h>
#include <adosfs/adosfs.h>

int
adosfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
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

	if (error = copyin(data, (caddr_t)&args, sizeof(struct adosfs_args)))
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
	if (error = namei(ndp))
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
		VOP_LOCK(devvp);
		if (error = VOP_ACCESS(devvp, accessmode, p->p_ucred, p)) {
			vput(devvp);
			return (error);
		}
		VOP_UNLOCK(devvp);
	}
/* MNT_UPDATE? */
	if (error = adosfs_mountfs(devvp, mp, p)) {
		vrele(devvp);
		return (error);
	}
	amp = VFSTOADOSFS(mp);
	amp->uid = args.uid;
	amp->gid = args.gid;
	amp->mask = args.mask;
	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
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
	if (error = vfs_mountedon(devvp))
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	if (error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0))
		return (error);

	/* 
	 * open blkdev and read root block
	 */
	if (error = VOP_OPEN(devvp, FREAD, NOCRED, p))
		return (error);
	if (error = VOP_IOCTL(devvp, DIOCGDINFO,(caddr_t)&dl, FREAD, NOCRED, p))
		goto fail;

	parp = &dl.d_partitions[part];
	amp = malloc(sizeof(struct adosfsmount), M_ADOSFSMNT, M_WAITOK);
	bzero((char *)amp, (u_long)sizeof(struct adosfsmount));
	amp->mp = mp;
	amp->startb = parp->p_offset;
	amp->endb = parp->p_offset + parp->p_size;
	amp->bsize = dl.d_secsize;
	amp->nwords = amp->bsize >> 2;
	amp->devvp = devvp;
/*	amp->rootb = (parp->p_size - 1 + 2) >> 1;*/
	amp->rootb = (parp->p_size - 1 + parp->p_cpg) >> 1;
	
	mp->mnt_data = (qaddr_t)amp;
        mp->mnt_stat.f_fsid.val[0] = (long)devvp->v_rdev;
        mp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_ADOSFS);
	mp->mnt_flag |= MNT_LOCAL;
	devvp->v_specflags |= SI_MOUNTEDON;

	/*
	 * init anode table.
	 */
	for (i = 0; i < ANODEHASHSZ; i++) 
		LIST_INIT(&amp->anodetab[i]);

	/*
	 * get the root anode, if not a valid fs this will fail.
	 */
	if (error = VFS_ROOT(mp, &rvp))
		goto fail;
	vput(rvp);

	return(0);

fail:
	(void) VOP_CLOSE(devvp, FREAD, NOCRED, p);
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
	if (error = vflush(mp, NULLVP, flags))
		return (error);
	amp = VFSTOADOSFS(mp);
	amp->devvp->v_specflags &= ~SI_MOUNTEDON;
	error = VOP_CLOSE(amp->devvp, FREAD, NOCRED, p);
	vrele(amp->devvp);
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

	if (error = VFS_VGET(mp, (ino_t)VFSTOADOSFS(mp)->rootb, &nvp))
		return (error);
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
	sbp->f_type = 0;
	sbp->f_bsize = amp->bsize;
	sbp->f_iosize = amp->bsize;
	sbp->f_blocks = 2;		/* XXX */
	sbp->f_bfree = 0;		/* none */
	sbp->f_bavail = 0;		/* none */
	sbp->f_files = 0;		/* who knows */
	sbp->f_ffree = 0;		/* " " */
	if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_op->vfs_name, MFSNAMELEN);
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
	int namlen, error, tmplen;

	error = 0;
	amp = VFSTOADOSFS(mp);
	bp = NULL;

	/* 
	 * check hash table. we are done if found
	 */
	if (*vpp = adosfs_ahashget(mp, an))
		return (0);

	if (error = getnewvnode(VT_ADOSFS, mp, adosfs_vnodeop_p, &vp))
		return (error);

	/*
	 * setup, insert in hash, and lock before io.
	 */
	vp->v_data = ap = malloc(sizeof(struct anode), M_ANODE, M_WAITOK);
	bzero(ap, sizeof(struct anode));
	ap->vp = vp;
	ap->amp = amp;
	ap->block = an;
	ap->nwords = amp->nwords;
	adosfs_ainshash(amp, ap);

	if (error = bread(amp->devvp, an, amp->bsize, NOCRED, &bp)) {
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
		vp->v_type = VDIR;
		break;
	case ADIR:
		vp->v_type = VDIR;
		break;
	case ALFILE:
		vp->v_type = VREG;
		ap->fsize = adoswordn(bp, ap->nwords - 47);
		break;
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
		nam = bp->b_data + (6 * sizeof(long));
		tmplen = namlen = *(u_char *)nam++;
		tmp = nam;
		while (tmplen-- && *tmp != ':')
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
		break;
	default:
		brelse(bp);
		vput(vp);
		return (EINVAL);
	}
	/* 
	 * if dir alloc hash table and copy it in 
	 */
	if (vp->v_type == VDIR) {
		int i;

		ap->tab = malloc(ANODETABSZ(ap) * 2, M_ANODE, M_WAITOK);
		ap->ntabent = ANODETABENT(ap);
		ap->tabi = (int *)&ap->tab[ap->ntabent];
		bzero(ap->tabi, ANODETABSZ(ap));
		for (i = 0; i < ap->ntabent; i++)
			ap->tab[i] = adoswordn(bp, i + 6);
	}

	/*
	 * misc.
	 */
	ap->pblock = adoswordn(bp, ap->nwords - 3);
	ap->hashf = adoswordn(bp, ap->nwords - 4);
	ap->linknext = adoswordn(bp, ap->nwords - 10);
	ap->linkto = adoswordn(bp, ap->nwords - 11);

	/*
	 * setup last indirect block cache.
	 */
	ap->lastlindblk = 0;
	if (ap->type == AFILE) 
		ap->lastindblk = ap->block;
	else if (ap->type == ALFILE)
		ap->lastindblk = ap->linkto;

	if (ap->type == AROOT) {
		ap->adprot = 0;
		ap->uid = amp->uid;
		ap->gid = amp->gid;
	} else {
		ap->adprot = adoswordn(bp, ap->nwords - 48);
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

	/*
	 * copy in name
	 */
	nam = bp->b_data + (ap->nwords - 20) * sizeof(long);
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

	*vpp = vp;		/* return vp */
	brelse(bp);		/* release buffer */
	return (0);
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

struct ifid {
	ushort	ifid_len;
	ushort	ifid_pad;
	int	ifid_ino;
	long	ifid_start;
};

int
adosfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	struct mount *mp;
	struct fid *fhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	struct ifid *ifhp = (struct ifid *)fhp;
	struct adosfsmount *amp = VFSTOADOSFS(mp);
	struct anode *ap;
	struct netcred *np;
	struct vnode *nvp;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	printf("adfhtovp(%x, %x, %x)\n", mp, fhp, vpp);
#endif
	
	/*
	 * Get the export permission structure for this <mp, client> tuple.
	 */
	np = vfs_export_lookup(mp, &amp->export, nam);
	if (np == NULL)
		return (EACCES);

	if (error = VFS_VGET(mp, ifhp->ifid_ino, &nvp)) {
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
	
	ifhp->ifid_ino = ap->block;
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
adosfs_sync(mp, waitfor)
	struct mount *mp;
	int waitfor;
{
#ifdef ADOSFS_DIAGNOSTIC
	printf("ad_sync(%x, %x)\n", mp, waitfor);
#endif
	return(0);
}

int
adosfs_init()
{
	return(0);
}

/*
 * vfs generic function call table
 */
struct vfsops adosfs_vfsops = {
	MOUNT_ADOSFS,
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
