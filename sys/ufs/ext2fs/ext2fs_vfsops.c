/*	$NetBSD: ffs_vfsops.c,v 1.21 1996/10/12 21:58:47 christos Exp $	*/

/* Modified for EXT2FS on NetBSD by Manuel Bouyer, April 1997 */

/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	may be used to endorse or promote products derived from this software
 *	without specific prior written permission.
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
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

int ext2fs_sbupdate __P((struct ufsmount *, int));
int ext2fs_check_export __P((struct mount *, struct ufid *, struct mbuf *,
	struct vnode **, int *, struct ucred **));

struct vfsops ext2fs_vfsops = {
	MOUNT_EXT2FS,
	ext2fs_mount,
	ufs_start,
	ext2fs_unmount,
	ufs_root,
	ufs_quotactl,
	ext2fs_statfs,
	ext2fs_sync,
	ext2fs_vget,
	ext2fs_fhtovp,
	ext2fs_vptofh,
	ext2fs_init,
	ext2fs_mountroot,
};

extern u_long ext2gennumber;

/*
 * This is the generic part of fhtovp called after the underlying
 * filesystem has validated the file handle.
 *
 * Verify that a host should have access to a filesystem, and if so
 * return a vnode for the presented file handle.
 */
int
ext2fs_check_export(mp, ufhp, nam, vpp, exflagsp, credanonp)
	register struct mount *mp;
	struct ufid *ufhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	register struct inode *ip;
	register struct netcred *np;
	register struct ufsmount *ump = VFSTOUFS(mp);
	struct vnode *nvp;
	int error;

	/*
	 * Get the export permission structure for this <mp, client> tuple.
	 */
	np = vfs_export_lookup(mp, &ump->um_export, nam);
	if (np == NULL)
		return (EACCES);

	if ((error = VFS_VGET(mp, ufhp->ufid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	ip = VTOI(nvp);
	if (ip->i_e2fs_mode == 0 || ip->i_e2fs_dtime != 0 || 
		ip->i_e2fs_gen != ufhp->ufid_gen) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}   


/*
 * Called by main() when ext2fs is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */
#define ROOTNAME	"root_device"

int
ext2fs_mountroot()
{
	extern struct vnode *rootvp;
	register struct m_ext2fs *fs;
	register struct mount *mp;
	struct proc *p = curproc;	/* XXX */
	struct ufsmount *ump;
	size_t size;
	int error;

	if (root_device->dv_class != DV_DISK)
		return (ENODEV);
	
	/*
	 * Get vnodes for swapdev and rootdev.
	 */
	if (bdevvp(swapdev, &swapdev_vp) || bdevvp(rootdev, &rootvp))
		panic("ext2fs_mountroot: can't setup bdevvp's");

	mp = malloc(sizeof(struct mount), M_MOUNT, M_WAITOK);
	bzero((char *)mp, sizeof(struct mount));
	mp->mnt_op = &ext2fs_vfsops;
	mp->mnt_flag = MNT_RDONLY;
	if ((error = ext2fs_mountfs(rootvp, mp, p)) != 0) {
		free(mp, M_MOUNT);
		return (error);
	}
	if ((error = vfs_lock(mp)) != 0) {
		(void)ext2fs_unmount(mp, 0, p);
		free(mp, M_MOUNT);
		return (error);
	}
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mp->mnt_vnodecovered = NULLVP;
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	bzero(fs->e2fs_fsmnt, sizeof(fs->e2fs_fsmnt));
	fs->e2fs_fsmnt[0] = '/';
	bcopy(fs->e2fs_fsmnt, mp->mnt_stat.f_mntonname, MNAMELEN);
	(void) copystr(ROOTNAME, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
		&size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)ext2fs_statfs(mp, &mp->mnt_stat, p);
	vfs_unlock(mp);
	inittodr(fs->e2fs.e2fs_wtime);
	return (0);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
int
ext2fs_mount(mp, path, data, ndp, p)
	register struct mount *mp;
	const char *path;
	void * data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;
	struct ufs_args args;
	struct ufsmount *ump = NULL;
	register struct m_ext2fs *fs;
	size_t size;
	int error, flags;
	mode_t accessmode;

	error = copyin(data, (caddr_t)&args, sizeof (struct ufs_args));
	if (error)
		return (error);
	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_e2fs;
		if (fs->e2fs_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			if (vfs_busy(mp))
				return (EBUSY);
			error = ext2fs_flushfiles(mp, flags, p);
			if (error == 0 &&
				ext2fs_cgupdate(ump, MNT_WAIT) == 0 &&
				(fs->e2fs.e2fs_state & E2FS_ERRORS) == 0) {
				fs->e2fs.e2fs_state = E2FS_ISCLEAN;
				(void) ext2fs_sbupdate(ump, MNT_WAIT);
			}
			vfs_unbusy(mp);
			if (error)
				return (error);
			fs->e2fs_ronly = 1;
		}
		if (mp->mnt_flag & MNT_RELOAD) {
			error = ext2fs_reload(mp, ndp->ni_cnd.cn_cred, p);
			if (error)
				return (error);
		}
		if (fs->e2fs_ronly && (mp->mnt_flag & MNT_WANTRDWR)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			if (p->p_ucred->cr_uid != 0) {
				devvp = ump->um_devvp;
				VOP_LOCK(devvp);
				error = VOP_ACCESS(devvp, VREAD | VWRITE,
						   p->p_ucred, p);
				if (error) {
					VOP_UNLOCK(devvp);
					return (error);
				}
				VOP_UNLOCK(devvp);
			}
			fs->e2fs_ronly = 0;
			if (fs->e2fs.e2fs_state == E2FS_ISCLEAN)
				fs->e2fs.e2fs_state = 0;
			else
				fs->e2fs.e2fs_state = E2FS_ERRORS;
			fs->e2fs_fmod = 1;
		}
		if (args.fspec == 0) {
			/*
			 * Process export requests.
			 */
			return (vfs_export(mp, &ump->um_export, &args.export));
		}
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
		VOP_LOCK(devvp);
		error = VOP_ACCESS(devvp, accessmode, p->p_ucred, p);
		if (error) {
			vput(devvp);
			return (error);
		}
		VOP_UNLOCK(devvp);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0)
		error = ext2fs_mountfs(devvp, mp, p);
	else {
		if (devvp != ump->um_devvp)
			error = EINVAL;	/* needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	(void) copyinstr(path, fs->e2fs_fsmnt, sizeof(fs->e2fs_fsmnt) - 1, &size);
	bzero(fs->e2fs_fsmnt + size, sizeof(fs->e2fs_fsmnt) - size);
	bcopy(fs->e2fs_fsmnt, mp->mnt_stat.f_mntonname, MNAMELEN);
	(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, 
		&size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	if (fs->e2fs_fmod != 0) {	/* XXX */
		fs->e2fs_fmod = 0;
		if (fs->e2fs.e2fs_state == 0)
			fs->e2fs.e2fs_wtime = time.tv_sec;
		else
			printf("%s: file system not clean; please fsck(8)\n",
				mp->mnt_stat.f_mntfromname);
		(void) ext2fs_cgupdate(ump, MNT_WAIT);
	}
	return (0);
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) re-read summary information from disk.
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read inode data for all active vnodes.
 */
int
ext2fs_reload(mountp, cred, p)
	register struct mount *mountp;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp, *nvp, *devvp;
	struct inode *ip;
	struct buf *bp;
	struct m_ext2fs *fs;
	struct ext2fs *newfs;
	struct partinfo dpart;
	int i, size, error;

	if ((mountp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOUFS(mountp)->um_devvp;
	if (vinvalbuf(devvp, 0, cred, p, 0, 0))
		panic("ext2fs_reload: dirty1");
	/*
	 * Step 2: re-read superblock from disk.
	 */
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, NOCRED, p) != 0)
		size = DEV_BSIZE;
	else
		size = dpart.disklab->d_secsize;
	error = bread(devvp, (daddr_t)(SBOFF / size), SBSIZE, NOCRED, &bp);
	if (error)
		return (error);
	newfs = (struct ext2fs *)bp->b_data;
	if (newfs->e2fs_magic != E2FS_MAGIC || newfs->e2fs_rev != E2FS_REV) {
#ifdef DIAGNOSTIC
		printf("Wrong magic number: %x (expected %x for ext2 fs)",
					newfs->e2fs_magic, E2FS_MAGIC);
		printf("or wrong revision number: %x (expected %x for ext2 fs)",
					newfs->e2fs_rev, E2FS_REV);
#endif
		brelse(bp);
		return (EIO);		/* XXX needs translation */
	}
	if (newfs->e2fs_log_bsize > 2) { /* block size = 1024|2048|4096 */
#ifdef DIAGNOSTIC
		printf("wrong block size: %d (expected <=2 for ext2 fs)\n",
			newfs->e2fs_log_bsize);
#endif
		brelse(bp);
		return (EIO);	   /* XXX needs translation */
	}


	fs = VFSTOUFS(mountp)->um_e2fs;
	/* 
	 * copy in new superblock, and compute in-memory values
	 */
	bcopy(newfs, &fs->e2fs, SBSIZE);
	fs->e2fs_ncg = howmany(fs->e2fs.e2fs_bcount - fs->e2fs.e2fs_first_dblock,
		fs->e2fs.e2fs_bpg);
	/* XXX assume hw bsize = 512 */
	fs->e2fs_fsbtodb = fs->e2fs.e2fs_log_bsize + 1;
	fs->e2fs_bsize = 1024 << fs->e2fs.e2fs_log_bsize;
	fs->e2fs_bshift = LOG_MINBSIZE + fs->e2fs.e2fs_log_bsize;
	fs->e2fs_qbmask = fs->e2fs_bsize - 1;
	fs->e2fs_bmask = ~fs->e2fs_qbmask;
	fs->e2fs_ngdb = howmany(fs->e2fs_ncg,
			fs->e2fs_bsize / sizeof(struct ext2_gd));
	fs->e2fs_ipb = fs->e2fs_bsize / sizeof(struct ext2fs_dinode);
	fs->e2fs_itpg = fs->e2fs.e2fs_ipg/fs->e2fs_ipb;

	/*
	 * Step 3: re-read summary information from disk.
	 */

	for (i=0; i < fs->e2fs_ngdb; i++) {
		error = bread(devvp , fsbtodb(fs, ((fs->e2fs_bsize>1024)?0:1)+i+1),
				fs->e2fs_bsize, NOCRED, &bp);
		if (error)
			return (error);
		bcopy(bp->b_data,
			&fs->e2fs_gd[i* fs->e2fs_bsize / sizeof(struct ext2_gd)],
			fs->e2fs_bsize);
		brelse(bp);
	}
	
loop:
	for (vp = mountp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		nvp = vp->v_mntvnodes.le_next;
		/*
		 * Step 4: invalidate all inactive vnodes.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			continue;
		}
		/*
		 * Step 5: invalidate all cached file data.
		 */
		if (vget(vp, 1))
			goto loop;
		if (vinvalbuf(vp, 0, cred, p, 0, 0))
			panic("ext2fs_reload: dirty2");
		/*
		 * Step 6: re-read inode data for all active vnodes.
		 */
		ip = VTOI(vp);
		error = bread(devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
				  (int)fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			vput(vp);
			return (error);
		}
		bcopy((struct ext2fs_dinode *)bp->b_data +
			ino_to_fsbo(fs, ip->i_number),
			&ip->i_din.e2fs_din, sizeof(struct ext2fs_dinode));
		brelse(bp);
		vput(vp);
		if (vp->v_mount != mountp)
			goto loop;
	}
	return (0);
}

/*
 * Common code for mount and mountroot
 */
int
ext2fs_mountfs(devvp, mp, p)
	register struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
{
	register struct ufsmount *ump;
	struct buf *bp;
	register struct ext2fs *fs;
	register struct m_ext2fs *m_fs;
	dev_t dev;
	struct partinfo dpart;
	int error, i, size, ronly;
	struct ucred *cred;
	extern struct vnode *rootvp;

	dev = devvp->v_rdev;
	cred = p ? p->p_ucred : NOCRED;
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
	if ((error = vinvalbuf(devvp, V_SAVE, cred, p, 0, 0)) != 0)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	if (error)
		return (error);
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, cred, p) != 0)
		size = DEV_BSIZE;
	else
		size = dpart.disklab->d_secsize;

	bp = NULL;
	ump = NULL;

#ifdef DEBUG_EXT2
	printf("sb size: %d ino size %d\n", sizeof(struct ext2fs),
			sizeof(struct ext2fs_dinode));
#endif
	error = bread(devvp, (SBOFF / DEV_BSIZE), SBSIZE, cred, &bp);
	if (error)
		goto out;
	fs = (struct ext2fs *)bp->b_data;
	if (fs->e2fs_magic != E2FS_MAGIC || fs->e2fs_rev != E2FS_REV) {
#ifdef DIAGNOSTIC
		printf("Wrong magic number: %x (expected %x for ext2 fs)",
			fs->e2fs_magic, E2FS_MAGIC);
		printf(" or wrong revision number: %x (expected %x for ext2 fs)\n",
			fs->e2fs_rev, E2FS_REV);
#endif
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}

	if (fs->e2fs_log_bsize > 2) { /* block size = 1024|2048|4096 */
#ifdef DIAGNOSTIC
		printf("wrong block size: %d (expected <2 for ext2 fs)\n",
			fs->e2fs_log_bsize);
#endif
		error = EINVAL;	 /* XXX needs translation */
		goto out;
	}

	ump = malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	bzero((caddr_t)ump, sizeof *ump);
	ump->um_e2fs = malloc(sizeof(struct m_ext2fs), M_UFSMNT, M_WAITOK);
	bcopy(bp->b_data, ump->um_e2fs, SBSIZE);
	brelse(bp);
	bp = NULL;
	m_fs = ump->um_e2fs;
	m_fs->e2fs_ronly = ronly;
	if (ronly == 0) {
		if (m_fs->e2fs.e2fs_state == E2FS_ISCLEAN)
			m_fs->e2fs.e2fs_state = 0;
		else
			m_fs->e2fs.e2fs_state = E2FS_ERRORS;
		m_fs->e2fs_fmod = 1;
	}

	/* compute dynamic sb infos */
	m_fs->e2fs_ncg =
		howmany(m_fs->e2fs.e2fs_bcount - m_fs->e2fs.e2fs_first_dblock,
		m_fs->e2fs.e2fs_bpg);
	/* XXX assume hw bsize = 512 */
	m_fs->e2fs_fsbtodb = m_fs->e2fs.e2fs_log_bsize + 1;
	m_fs->e2fs_bsize = 1024 << m_fs->e2fs.e2fs_log_bsize;
	m_fs->e2fs_bshift = LOG_MINBSIZE + m_fs->e2fs.e2fs_log_bsize;
	m_fs->e2fs_qbmask = m_fs->e2fs_bsize - 1;
	m_fs->e2fs_bmask = ~m_fs->e2fs_qbmask;
	m_fs->e2fs_ngdb = howmany(m_fs->e2fs_ncg,
		m_fs->e2fs_bsize / sizeof(struct ext2_gd));
	m_fs->e2fs_ipb = m_fs->e2fs_bsize / sizeof(struct ext2fs_dinode);
	m_fs->e2fs_itpg = m_fs->e2fs.e2fs_ipg/m_fs->e2fs_ipb;

	m_fs->e2fs_gd = malloc(m_fs->e2fs_ngdb * m_fs->e2fs_bsize,
		M_UFSMNT, M_WAITOK);
	for (i=0; i < m_fs->e2fs_ngdb; i++) {
		error = bread(devvp , fsbtodb(m_fs, ((m_fs->e2fs_bsize>1024)?0:1)+i+1),
			m_fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			free(m_fs->e2fs_gd, M_UFSMNT);
			goto out;
		}
		bcopy(bp->b_data,
			&m_fs->e2fs_gd[i* m_fs->e2fs_bsize / sizeof(struct ext2_gd)],
			m_fs->e2fs_bsize);
		brelse(bp);
		bp = NULL;
	}

	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_EXT2FS);
	mp->mnt_maxsymlinklen = EXT2_MAXSYMLINKLEN;
	mp->mnt_flag |= MNT_LOCAL;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_nindir = NINDIR(m_fs);
	ump->um_bptrtodb = m_fs->e2fs_fsbtodb;
	ump->um_seqinc = 1; /* no frags */
	devvp->v_specflags |= SI_MOUNTEDON;
	return (0);
out:
	if (bp)
		brelse(bp);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, cred, p);
	if (ump) {
		free(ump->um_e2fs, M_UFSMNT);
		free(ump, M_UFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * unmount system call
 */
int
ext2fs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	register struct ufsmount *ump;
	register struct m_ext2fs *fs;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = ext2fs_flushfiles(mp, flags, p)) != 0)
		return (error);
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	if (fs->e2fs_ronly == 0 &&
		ext2fs_cgupdate(ump, MNT_WAIT) == 0 &&
		(fs->e2fs.e2fs_state & E2FS_ERRORS) == 0) {
		fs->e2fs.e2fs_state = E2FS_ISCLEAN;
		(void) ext2fs_sbupdate(ump, MNT_WAIT);
	}
	ump->um_devvp->v_specflags &= ~SI_MOUNTEDON;
	error = VOP_CLOSE(ump->um_devvp, fs->e2fs_ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);
	vrele(ump->um_devvp);
	free(fs->e2fs_gd, M_UFSMNT);
	free(fs, M_UFSMNT);
	free(ump, M_UFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
int
ext2fs_flushfiles(mp, flags, p)
	register struct mount *mp;
	int flags;
	struct proc *p;
{
	extern int doforce;
	register struct ufsmount *ump;
	int error;

	if (!doforce)
		flags &= ~FORCECLOSE;
	ump = VFSTOUFS(mp);
	error = vflush(mp, NULLVP, flags);
	return (error);
}

/*
 * Get file system statistics.
 */
int
ext2fs_statfs(mp, sbp, p)
	struct mount *mp;
	register struct statfs *sbp;
	struct proc *p;
{
	register struct ufsmount *ump;
	register struct m_ext2fs *fs;
	u_int32_t overhead, overhead_per_group;

	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	if (fs->e2fs.e2fs_magic != E2FS_MAGIC)
		panic("ext2fs_statfs");
#ifdef COMPAT_09
	sbp->f_type = 1;
#else
	sbp->f_type = 0;
#endif

	/*
	 * Compute the overhead (FS structures)
	 */
	overhead_per_group = 1 /* super block */ +
						 fs->e2fs_ngdb +
						 1 /* block bitmap */ +
						 1 /* inode bitmap */ +
						 fs->e2fs_itpg;
	overhead = fs->e2fs.e2fs_first_dblock +
		   fs->e2fs_ncg * overhead_per_group;


	sbp->f_bsize = fs->e2fs_bsize;
	sbp->f_iosize = fs->e2fs_bsize;
	sbp->f_blocks = fs->e2fs.e2fs_bcount - overhead;
	sbp->f_bfree = fs->e2fs.e2fs_fbcount;
	sbp->f_bavail = sbp->f_bfree - fs->e2fs.e2fs_rbcount;
	sbp->f_files =  fs->e2fs.e2fs_icount;
	sbp->f_ffree = fs->e2fs.e2fs_ficount;
	if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_op->vfs_name, MFSNAMELEN);
	return (0);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
int
ext2fs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp;
	register struct inode *ip;
	register struct ufsmount *ump = VFSTOUFS(mp);
	register struct m_ext2fs *fs;
	int error, allerror = 0;

	fs = ump->um_e2fs;
	/*
	 * Write back modified superblock.
	 * Consistency check that the superblock
	 * is still in the buffer cache.
	 */
	if (fs->e2fs_fmod != 0) {
		if (fs->e2fs_ronly != 0) {		/* XXX */
			printf("fs = %s\n", fs->e2fs_fsmnt);
			panic("update: rofs mod");
		}
		fs->e2fs_fmod = 0;
		fs->e2fs.e2fs_wtime = time.tv_sec;
		allerror = ext2fs_cgupdate(ump, waitfor);
	}
	/*
	 * Write back each (modified) inode.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first;
		 vp != NULL;
		 vp = vp->v_mntvnodes.le_next) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		if (VOP_ISLOCKED(vp))
			continue;
		ip = VTOI(vp);
		if ((ip->i_flag &
			(IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
			vp->v_dirtyblkhd.lh_first == NULL)
			continue;
		if (vget(vp, 1))
			goto loop;
		if ((error = VOP_FSYNC(vp, cred, waitfor, p)) != 0)
			allerror = error;
		vput(vp);
	}
	/*
	 * Force stale file system control information to be flushed.
	 */
	if ((error = VOP_FSYNC(ump->um_devvp, cred, waitfor, p)) != 0)
		allerror = error;
	return (allerror);
}

/*
 * Look up a EXT2FS dinode number to find its incore vnode, otherwise read it
 * in from disk.  If it is in core, wait for the lock bit to clear, then
 * return the inode locked.  Detection and handling of mount points must be
 * done by the calling routine.
 */
int
ext2fs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	register struct m_ext2fs *fs;
	register struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	dev_t dev;
	int error;

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
	if ((*vpp = ufs_ihashget(dev, ino)) != NULL)
		return (0);

	/* Allocate a new vnode/inode. */
	if ((error = getnewvnode(VT_EXT2FS, mp, ext2fs_vnodeop_p, &vp)) != 0) {
		*vpp = NULL;
		return (error);
	}
	MALLOC(ip, struct inode *, sizeof(struct inode), M_EXT2FSNODE, M_WAITOK);
	bzero((caddr_t)ip, sizeof(struct inode));
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_e2fs = fs = ump->um_e2fs;
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_e2fs_last_lblk = 0;
	ip->i_e2fs_last_blk = 0;

	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ufs_ihashins(ip);

	/* Read in the disk contents for the inode, copy into the inode. */
	error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
			  (int)fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain. With mode
		 * still zero, it will be unlinked and returned to the free
		 * list by vput().
		 */
		vput(vp);
		brelse(bp);
		*vpp = NULL;
		return (error);
	}
	bcopy(((struct ext2fs_dinode*)bp->b_data + ino_to_fsbo(fs, ino)),
				&ip->i_din, sizeof(struct ext2fs_dinode));

	brelse(bp);

	/*
	 * Initialize the vnode from the inode, check for aliases.
	 * Note that the underlying vnode may have changed.
	 */
	error = ufs_vinit(mp, ext2fs_specop_p, EXT2FS_FIFOOPS, &vp);
	if (error) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_devvp = ump->um_devvp;
	VREF(ip->i_devvp);
	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_e2fs_gen == 0) {
		if (++ext2gennumber < (u_long)time.tv_sec)
			ext2gennumber = time.tv_sec;
		ip->i_e2fs_gen = ext2gennumber;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IN_MODIFIED;
	}

	*vpp = vp;
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call ext2fs_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
int
ext2fs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	register struct mount *mp;
	struct fid *fhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	register struct ufid *ufhp;
	struct m_ext2fs *fs;

	ufhp = (struct ufid *)fhp;
	fs = VFSTOUFS(mp)->um_e2fs;
	if ((ufhp->ufid_ino < EXT2_FIRSTINO && ufhp->ufid_ino != EXT2_ROOTINO) ||
		ufhp->ufid_ino >= fs->e2fs_ncg * fs->e2fs.e2fs_ipg)
		return (ESTALE);
	return (ext2fs_check_export(mp, ufhp, nam, vpp, exflagsp, credanonp));
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
ext2fs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	register struct inode *ip;
	register struct ufid *ufhp;

	ip = VTOI(vp);
	ufhp = (struct ufid *)fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_e2fs_gen;
	return (0);
}

/*
 * Write a superblock and associated information back to disk.
 */
int
ext2fs_sbupdate(mp, waitfor)
	struct ufsmount *mp;
	int waitfor;
{
	register struct m_ext2fs *fs = mp->um_e2fs;
	register struct buf *bp;
	int error = 0;

	bp = getblk(mp->um_devvp, SBLOCK, SBSIZE, 0, 0);
	bcopy((caddr_t)(&fs->e2fs), bp->b_data, SBSIZE);
	if (waitfor == MNT_WAIT)
		error = bwrite(bp);
	else
		bawrite(bp);
	return (error);
}

int
ext2fs_cgupdate(mp, waitfor)
	struct ufsmount *mp;
	int waitfor;
{
	register struct m_ext2fs *fs = mp->um_e2fs;
	register struct buf *bp;
	int i, error = 0, allerror = 0;

	allerror = ext2fs_sbupdate(mp, waitfor);
	for (i = 0; i < fs->e2fs_ngdb; i++) {
		bp = getblk(mp->um_devvp, fsbtodb(fs, ((fs->e2fs_bsize>1024)?0:1)+i+1),
			fs->e2fs_bsize, 0, 0);
		bcopy(&fs->e2fs_gd[i* fs->e2fs_bsize / sizeof(struct ext2_gd)],
		 bp->b_data, fs->e2fs_bsize);
		if (waitfor == MNT_WAIT)
			error = bwrite(bp);
		else
			bawrite(bp);
	}

	if (!allerror && error)
		allerror = error;
	return (allerror);
}
